/*
 *   STMicroelectronics System-on-Chips' internal (memory mapped)
 *   audio DAC driver
 *
 *   Copyright (c) 2005-2011 STMicroelectronics Limited
 *
 *   Author: Pawel Moll <pawel.moll@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Registers definitions
 */

#define ADAC_CTRL(base)		((base) + 0x00)

#define NRST			0
#define NRST__MASK		(1 << NRST)
#define NRST__RESET		(0 << NRST)
#define NRST__NORMAL		(1 << NRST)

#define MODE			1
#define MODE__DEFAULT		(0 << MODE)

#define NSB			3
#define NSB__MASK		(1 << NSB)
#define NSB__POWER_DOWN		(0 << NSB)
#define NSB__NORMAL		(1 << NSB)

#define SOFTMUTE		4
#define SOFTMUTE__MASK		(1 << SOFTMUTE)
#define SOFTMUTE__NORMAL	(0 << SOFTMUTE)
#define SOFTMUTE__MUTE		(1 << SOFTMUTE)

#define PDNANA			5
#define PDNANA__POWER_DOWN	(0 << PDNANA)
#define PDNANA__NORMAL		(1 << PDNANA)

#define PDNBG			6
#define PDNBG__POWER_DOWN	(0 << PDNBG)
#define PDNBG__NORMAL		(1 << PDNBG)



/*
 * Internal DAC instance structure
 */

struct snd_stm_conv_dac_mem {
	/* System informations */
	struct snd_stm_conv_converter *converter;
	const char *dev_name;

	/* Resources */
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_dac_mem_get_format(void *priv)
{
	snd_stm_printd(1, "snd_stm_conv_dac_mem_get_format(priv=%p)\n", priv);

	return SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS;
}

static int snd_stm_conv_dac_mem_get_oversampling(void *priv)
{
	snd_stm_printd(1, "snd_stm_conv_dac_mem_get_oversampling(priv=%p)\n",
			priv);

	return 256;
}

static int snd_stm_conv_dac_mem_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_dac_mem *conv_dac_mem = priv;
	unsigned long value;

	snd_stm_printd(1, "snd_stm_conv_dac_mem_set_enabled(enabled=%d, "
			"priv=%p)\n", enabled, priv);

	BUG_ON(!conv_dac_mem);
	BUG_ON(!snd_stm_magic_valid(conv_dac_mem));

	snd_stm_printd(1, "%sabling DAC %s's digital part.\n",
			enabled ? "En" : "Dis", conv_dac_mem->dev_name);

	value = readl(ADAC_CTRL(conv_dac_mem->base));
	value &= ~(NSB__MASK | NRST__MASK);
	if (enabled)
		value |= NSB__NORMAL | NRST__NORMAL;
	else
		value |= NSB__POWER_DOWN | NRST__RESET;
	writel(value, ADAC_CTRL(conv_dac_mem->base));

	return 0;
}

static int snd_stm_conv_dac_mem_set_muted(int muted, void *priv)
{
	struct snd_stm_conv_dac_mem *conv_dac_mem = priv;
	unsigned long value;

	snd_stm_printd(1, "snd_stm_conv_dac_mem_set_muted(muted=%d, priv=%p)\n",
		       muted, priv);

	BUG_ON(!conv_dac_mem);
	BUG_ON(!snd_stm_magic_valid(conv_dac_mem));

	snd_stm_printd(1, "%suting DAC %s.\n", muted ? "M" : "Unm",
			conv_dac_mem->dev_name);

	value = readl(ADAC_CTRL(conv_dac_mem->base));
	value &= ~SOFTMUTE__MASK;
	if (muted)
		value |= SOFTMUTE__MUTE;
	else
		value |= SOFTMUTE__NORMAL;
	writel(value, ADAC_CTRL(conv_dac_mem->base));

	return 0;
}

static struct snd_stm_conv_ops snd_stm_conv_dac_mem_ops = {
	.get_format = snd_stm_conv_dac_mem_get_format,
	.get_oversampling = snd_stm_conv_dac_mem_get_oversampling,
	.set_enabled = snd_stm_conv_dac_mem_set_enabled,
	.set_muted = snd_stm_conv_dac_mem_set_muted,
};



/*
 * ALSA lowlevel device implementation
 */

static void snd_stm_conv_dac_mem_read_info(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_dac_mem *conv_dac_mem =
		entry->private_data;

	BUG_ON(!conv_dac_mem);
	BUG_ON(!snd_stm_magic_valid(conv_dac_mem));

	snd_iprintf(buffer, "--- %s ---\n", conv_dac_mem->dev_name);
	snd_iprintf(buffer, "base = 0x%p\n", conv_dac_mem->base);

	snd_iprintf(buffer, "ADAC_CTRL (0x%p) = 0x%08x\n",
			ADAC_CTRL(conv_dac_mem->base),
			readl(ADAC_CTRL(conv_dac_mem->base)));

	snd_iprintf(buffer, "\n");
}

static int snd_stm_conv_dac_mem_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_dac_mem *conv_dac_mem =
			snd_device->device_data;

	BUG_ON(!conv_dac_mem);
	BUG_ON(!snd_stm_magic_valid(conv_dac_mem));

	/* Initialize DAC with digital part down, analog up and muted */
	writel(NRST__RESET | MODE__DEFAULT | NSB__POWER_DOWN |
			SOFTMUTE__MUTE | PDNANA__NORMAL | PDNBG__NORMAL,
			ADAC_CTRL(conv_dac_mem->base));

	/* Additional procfs info */
	snd_stm_info_register(&conv_dac_mem->proc_entry,
			conv_dac_mem->dev_name,
			snd_stm_conv_dac_mem_read_info,
			conv_dac_mem);

	return 0;
}

static int __exit snd_stm_conv_dac_mem_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_dac_mem *conv_dac_mem =
			snd_device->device_data;

	BUG_ON(!conv_dac_mem);
	BUG_ON(!snd_stm_magic_valid(conv_dac_mem));

	/* Remove procfs entry */
	snd_stm_info_unregister(conv_dac_mem->proc_entry);

	/* Global power done & mute mode */
	writel(NRST__RESET | MODE__DEFAULT | NSB__POWER_DOWN |
			SOFTMUTE__MUTE | PDNANA__POWER_DOWN | PDNBG__POWER_DOWN,
			ADAC_CTRL(conv_dac_mem->base));

	return 0;
}

static struct snd_device_ops snd_stm_conv_dac_mem_snd_device_ops = {
	.dev_register = snd_stm_conv_dac_mem_register,
	.dev_disconnect = snd_stm_conv_dac_mem_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_conv_dac_mem_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_dac_mem_info *conv_dac_mem_info =
			pdev->dev.platform_data;
	struct snd_stm_conv_dac_mem *conv_dac_mem;
	struct snd_card *card = snd_stm_card_get();

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);
	BUG_ON(!conv_dac_mem_info);

	conv_dac_mem = kzalloc(sizeof(*conv_dac_mem), GFP_KERNEL);
	if (!conv_dac_mem) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_dac_mem);
	conv_dac_mem->dev_name = dev_name(&pdev->dev);

	/* Get resources */

	result = snd_stm_memory_request(pdev, &conv_dac_mem->mem_region,
			&conv_dac_mem->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Get connections */

	BUG_ON(!conv_dac_mem_info->source_bus_id);
	snd_stm_printd(0, "This DAC is attached to PCM player '%s'.\n",
			conv_dac_mem_info->source_bus_id);
	conv_dac_mem->converter = snd_stm_conv_register_converter(
			"Analog Output",
			&snd_stm_conv_dac_mem_ops, conv_dac_mem,
			&platform_bus_type, conv_dac_mem_info->source_bus_id,
			conv_dac_mem_info->channel_from,
			conv_dac_mem_info->channel_to, NULL);
	if (!conv_dac_mem->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_dac_mem,
			&snd_stm_conv_dac_mem_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, conv_dac_mem);

	return 0;

error_device:
error_attach:
	snd_stm_memory_release(conv_dac_mem->mem_region,
			conv_dac_mem->base);
error_memory_request:
	snd_stm_magic_clear(conv_dac_mem);
	kfree(conv_dac_mem);
error_alloc:
	return result;
}

static int snd_stm_conv_dac_mem_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_dac_mem *conv_dac_mem = platform_get_drvdata(pdev);

	BUG_ON(!conv_dac_mem);
	BUG_ON(!snd_stm_magic_valid(conv_dac_mem));

	snd_stm_conv_unregister_converter(conv_dac_mem->converter);
	snd_stm_memory_release(conv_dac_mem->mem_region,
			conv_dac_mem->base);

	snd_stm_magic_clear(conv_dac_mem);
	kfree(conv_dac_mem);

	return 0;
}

static struct platform_driver snd_stm_conv_dac_mem_driver = {
	.driver.name = "snd_conv_dac_mem",
	.probe = snd_stm_conv_dac_mem_probe,
	.remove = snd_stm_conv_dac_mem_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_dac_mem_init(void)
{
	return platform_driver_register(&snd_stm_conv_dac_mem_driver);
}

static void __exit snd_stm_conv_dac_mem_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_dac_mem_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics memory-mapped audio DAC driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_dac_mem_init);
module_exit(snd_stm_conv_dac_mem_exit);
