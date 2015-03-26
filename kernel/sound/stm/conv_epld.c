/*
 *   STMicroelectronics System-on-Chips' EPLD-controlled ADC/DAC driver
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
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Internal DAC instance structure
 */

struct snd_stm_conv_epld {
	/* System informations */
	const char *dev_name;
	struct snd_stm_conv_converter *converter;
	struct snd_stm_conv_epld_info *info;

	struct snd_stm_conv_ops ops;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * EPLD access implementation
 */

/* Defined in arch/sh/boards/st/common/epld.c */
void epld_write(unsigned long value, unsigned long offset);
unsigned long epld_read(unsigned long offset);

static void snd_stm_conv_epld_set(unsigned long offset,
		unsigned long mask, unsigned long value)
{
	unsigned long reg = epld_read(offset);

	reg &= ~mask;
	reg |= value;

	epld_write(reg, offset);
}


/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_epld_get_format(void *priv)
{
	struct snd_stm_conv_epld *conv_epld = priv;

	snd_stm_printd(1, "snd_stm_conv_epld_get_format(priv=%p)\n", priv);

	BUG_ON(!conv_epld);
	BUG_ON(!snd_stm_magic_valid(conv_epld));

	return conv_epld->info->format;
}

static int snd_stm_conv_epld_get_oversampling(void *priv)
{
	struct snd_stm_conv_epld *conv_epld = priv;

	snd_stm_printd(1, "snd_stm_conv_epld_get_oversampling(priv=%p)\n",
			priv);

	BUG_ON(!conv_epld);
	BUG_ON(!snd_stm_magic_valid(conv_epld));

	return conv_epld->info->oversampling;
}

static int snd_stm_conv_epld_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_epld *conv_epld = priv;

	snd_stm_printd(1, "snd_stm_conv_epld_enable(enabled=%d, priv=%p)\n",
			enabled, priv);

	BUG_ON(!conv_epld);
	BUG_ON(!snd_stm_magic_valid(conv_epld));
	BUG_ON(!conv_epld->info->enable_supported);

	snd_stm_printd(1, "%sabling DAC %s's.\n", enabled ? "En" : "Dis",
			conv_epld->dev_name);

	snd_stm_conv_epld_set(conv_epld->info->enable_offset,
			conv_epld->info->enable_mask,
			enabled ? conv_epld->info->enable_value :
			conv_epld->info->disable_value);

	return 0;
}

static int snd_stm_conv_epld_set_muted(int muted, void *priv)
{
	struct snd_stm_conv_epld *conv_epld = priv;

	snd_stm_printd(1, "snd_stm_conv_epld_set_muted(muted=%d, priv=%p)\n",
			muted, priv);

	BUG_ON(!conv_epld);
	BUG_ON(!snd_stm_magic_valid(conv_epld));
	BUG_ON(!conv_epld->info->mute_supported);

	snd_stm_printd(1, "%suting DAC %s.\n", muted ? "M" : "Unm",
			conv_epld->dev_name);

	snd_stm_conv_epld_set(conv_epld->info->mute_offset,
			conv_epld->info->mute_mask,
			muted ? conv_epld->info->mute_value :
			conv_epld->info->unmute_value);

	return 0;
}



/*
 * Procfs status callback
 */

#define DUMP_EPLD(offset) snd_iprintf(buffer, "EPLD[0x%08x] = 0x%08lx\n", \
		offset, epld_read(offset));

static void snd_stm_conv_epld_read_info(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_epld *conv_epld = entry->private_data;

	BUG_ON(!conv_epld);
	BUG_ON(!snd_stm_magic_valid(conv_epld));

	snd_iprintf(buffer, "--- %s ---\n", conv_epld->dev_name);

	if (conv_epld->info->enable_supported)
		DUMP_EPLD(conv_epld->info->enable_offset);
	if (conv_epld->info->mute_supported)
		DUMP_EPLD(conv_epld->info->mute_offset);

	snd_iprintf(buffer, "\n");
}



/*
 * Platform driver routines
 */

static int snd_stm_conv_epld_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_epld *conv_epld;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!pdev->dev.platform_data);

	conv_epld = kzalloc(sizeof(*conv_epld), GFP_KERNEL);
	if (!conv_epld) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_epld);
	conv_epld->dev_name = dev_name(&pdev->dev);
	conv_epld->info = pdev->dev.platform_data;

	conv_epld->ops.get_format = snd_stm_conv_epld_get_format;
	conv_epld->ops.get_oversampling = snd_stm_conv_epld_get_oversampling;
	if (conv_epld->info->enable_supported)
		conv_epld->ops.set_enabled = snd_stm_conv_epld_set_enabled;
	if (conv_epld->info->mute_supported)
		conv_epld->ops.set_muted = snd_stm_conv_epld_set_muted;

	/* Get connections */

	BUG_ON(!conv_epld->info->source_bus_id);
	snd_stm_printd(0, "This DAC is attached to PCM player '%s'.\n",
			conv_epld->info->source_bus_id);
	conv_epld->converter = snd_stm_conv_register_converter(
			conv_epld->info->group, &conv_epld->ops, conv_epld,
			&platform_bus_type, conv_epld->info->source_bus_id,
			conv_epld->info->channel_from,
			conv_epld->info->channel_to, NULL);
	if (!conv_epld->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		result = -EINVAL;
		goto error_attach;
	}

	/* Initialize converter as muted & disabled */

	if (conv_epld->info->enable_supported)
		snd_stm_conv_epld_set(conv_epld->info->enable_offset,
				conv_epld->info->enable_mask,
				conv_epld->info->disable_value);

	if (conv_epld->info->mute_supported)
		snd_stm_conv_epld_set(conv_epld->info->mute_offset,
				conv_epld->info->mute_mask,
				conv_epld->info->mute_value);

	/* Additional procfs info */

	snd_stm_info_register(&conv_epld->proc_entry,
			conv_epld->dev_name,
			snd_stm_conv_epld_read_info,
			conv_epld);

	/* Done now */

	platform_set_drvdata(pdev, conv_epld);

	return 0;

error_attach:
	snd_stm_magic_clear(conv_epld);
	kfree(conv_epld);
error_alloc:
	return result;
}

static int snd_stm_conv_epld_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_epld *conv_epld = platform_get_drvdata(pdev);

	BUG_ON(!conv_epld);
	BUG_ON(!snd_stm_magic_valid(conv_epld));

	snd_stm_conv_unregister_converter(conv_epld->converter);

	/* Remove procfs entry */

	snd_stm_info_unregister(conv_epld->proc_entry);

	/* Muting and disabling - just to be sure ;-) */

	if (conv_epld->info->enable_supported)
		snd_stm_conv_epld_set(conv_epld->info->enable_offset,
				conv_epld->info->enable_mask,
				conv_epld->info->disable_value);

	if (conv_epld->info->mute_supported)
		snd_stm_conv_epld_set(conv_epld->info->mute_offset,
				conv_epld->info->mute_mask,
				conv_epld->info->mute_value);

	snd_stm_magic_clear(conv_epld);
	kfree(conv_epld);

	return 0;
}

static struct platform_driver snd_stm_conv_epld_driver = {
	.driver.name = "snd_conv_epld",
	.probe = snd_stm_conv_epld_probe,
	.remove = snd_stm_conv_epld_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_epld_init(void)
{
	return platform_driver_register(&snd_stm_conv_epld_driver);
}

static void __exit snd_stm_conv_epld_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_epld_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics EPLD-controlled audio converter driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_epld_init);
module_exit(snd_stm_conv_epld_exit);
