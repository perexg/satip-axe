/*
 *   STMicrolectronics STx7200 SoC audio glue driver
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <sound/core.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);


/*
 * ALSA module parameters
 */

/* CUT 2+ ONLY! As PCM Reader #1 shares pins with MII1 it may receive
 * unwanted traffic if MII1 is actually used to networking,
 * or when PCM Player #1 is configured to use these pins. In such
 * case one may disable the reader input using this module parameter. */
static int pcm_reader_1_enabled = 1;

module_param(pcm_reader_1_enabled, int, 0444);
MODULE_PARM_DESC(id, "PCM Reader #1 control (not valid for STx7200 cut 1).");



/*
 * Audio glue driver implementation
 */

#define IOMUX_CTRL(base)	((base) + 0x00)
#define PCM_CLK_EN		0
#define PCM_CLK_EN__INPUT	(0 << PCM_CLK_EN)
#define PCM_CLK_EN__OUTPUT	(1 << PCM_CLK_EN)
#define DATA0_EN		1
#define DATA0_EN__INPUT		(0 << DATA0_EN)
#define DATA0_EN__OUTPUT	(1 << DATA0_EN)
#define DATA1_EN		2
#define DATA1_EN__INPUT		(0 << DATA1_EN)
#define DATA1_EN__OUTPUT	(1 << DATA1_EN)
#define DATA2_EN		3
#define DATA2_EN__INPUT		(0 << DATA2_EN)
#define DATA2_EN__OUTPUT	(1 << DATA2_EN)
#define SPDIF_EN		4
#define SPDIF_EN__DISABLE	(0 << SPDIF_EN)
#define SPDIF_EN__ENABLE	(1 << SPDIF_EN)
#define PCMRDR1_EN		5
#define PCMRDR1_EN__DISABLE	(0 << PCMRDR1_EN)
#define PCMRDR1_EN__ENABLE	(1 << PCMRDR1_EN)
#define HDMI_CTRL(base)		((base) + 0x04)
#define RECOVERY_CTRL(base)	((base) + 0x08)

struct snd_stm_stx7200_glue {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7200_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7200_glue *stx7200_glue = entry->private_data;

	BUG_ON(!stx7200_glue);
	BUG_ON(!snd_stm_magic_valid(stx7200_glue));

	snd_iprintf(buffer, "--- snd_stx7200_glue ---\n");
	snd_iprintf(buffer, "IOMUX_CTRL (0x%p) = 0x%08x\n",
			IOMUX_CTRL(stx7200_glue->base),
			readl(IOMUX_CTRL(stx7200_glue->base)));
	snd_iprintf(buffer, "HDMI_CTRL (0x%p) = 0x%08x\n",
			HDMI_CTRL(stx7200_glue->base),
			readl(HDMI_CTRL(stx7200_glue->base)));
	snd_iprintf(buffer, "RECOVERY_CTRL (0x%p) = 0x%08x\n",
			RECOVERY_CTRL(stx7200_glue->base),
			readl(RECOVERY_CTRL(stx7200_glue->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7200_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7200_glue *stx7200_glue;
	unsigned long value;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	stx7200_glue = kzalloc(sizeof(*stx7200_glue), GFP_KERNEL);
	if (!stx7200_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7200_glue);

	result = snd_stm_memory_request(pdev, &stx7200_glue->mem_region,
			&stx7200_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Enable audio outputs */
	value = SPDIF_EN__ENABLE | DATA2_EN__OUTPUT | DATA1_EN__OUTPUT |
			DATA0_EN__OUTPUT | PCM_CLK_EN__OUTPUT;

	/* Enable PCM Reader #1 (well, in some cases) */
	if (cpu_data->cut_major > 1 && pcm_reader_1_enabled)
		value |= PCMRDR1_EN__ENABLE;

	writel(value, IOMUX_CTRL(stx7200_glue->base));

	/* Additional procfs info */
	snd_stm_info_register(&stx7200_glue->proc_entry, "stx7200_glue",
			snd_stm_stx7200_glue_dump_registers, stx7200_glue);

	platform_set_drvdata(pdev, stx7200_glue);

	return result;

error_memory_request:
	snd_stm_magic_clear(stx7200_glue);
	kfree(stx7200_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7200_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7200_glue *stx7200_glue = platform_get_drvdata(pdev);
	unsigned long value;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!stx7200_glue);
	BUG_ON(!snd_stm_magic_valid(stx7200_glue));

	/* Remove procfs entry */
	snd_stm_info_unregister(stx7200_glue->proc_entry);

	/* Disable audio outputs */
	value = SPDIF_EN__DISABLE | DATA2_EN__INPUT | DATA1_EN__INPUT |
			DATA0_EN__INPUT | PCM_CLK_EN__INPUT;

	/* Disable PCM Reader #1 (well, in some cases) */
	if (cpu_data->cut_major > 1 && pcm_reader_1_enabled)
		value |= PCMRDR1_EN__DISABLE;

	writel(value, IOMUX_CTRL(stx7200_glue->base));

	snd_stm_memory_release(stx7200_glue->mem_region, stx7200_glue->base);

	snd_stm_magic_clear(stx7200_glue);
	kfree(stx7200_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7200_glue_driver = {
	.driver.name = "snd_stx7200_glue",
	.probe = snd_stm_stx7200_glue_probe,
	.remove = snd_stm_stx7200_glue_remove,
};



/*
 * Audio initialization
 */

#define SET_VER(_info_struct_, _device_, _ver_) \
		(((struct _info_struct_ *)_device_.dev.platform_data)->ver = \
		_ver_)

static int __init snd_stm_stx7200_init(void)
{
	int result;

	snd_stm_printd(0, "%s()\n", __func__);

	if (cpu_data->type != CPU_STX7200) {
		snd_stm_printe("Not supported (other than STx7200) SOC "
				"detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	result = platform_driver_register(&snd_stm_stx7200_glue_driver);
	if (result != 0) {
		snd_stm_printe("Failed to register audio glue driver!\n");
		goto error_glue_driver_register;
	}

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards (%d)!\n", result);
		goto error_card_register;
	}

	return 0;

error_card_register:
	platform_driver_unregister(&snd_stm_stx7200_glue_driver);
error_glue_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7200_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	platform_driver_unregister(&snd_stm_stx7200_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7200 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7200_init);
module_exit(snd_stm_stx7200_exit);
