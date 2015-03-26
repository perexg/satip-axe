/*
 *   STMicrolectronics STx7141 audio glue driver
 *
 *   Copyright (c) 2005-2011 STMicroelectronics Limited
 *
 *   Author: Stephen Gallimore <stephen.gallimore@st.com>
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
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <sound/core.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Audio glue driver implementation
 */

#define IO_CTRL(base)				((base) + 0x00)
#define CLKREC_SEL				9
#define CLKREC_SEL__PCMPLHDMI			(0 << CLKREC_SEL)
#define CLKREC_SEL__SPDIFHDMI			(1 << CLKREC_SEL)
#define CLKREC_SEL__PCMPL1			(2 << CLKREC_SEL)
#define CLKREC_SEL__PCMPL0			(3 << CLKREC_SEL)
#define PCMR1_SCLK_INV_SEL			12
#define PCMR1_SCLK_INV_SEL__NO_INVERSION	(0 << PCMR1_SCLK_INV_SEL)
#define PCMR1_SCLK_INV_SEL__INVERSION		(1 << PCMR1_SCLK_INV_SEL)
#define PCMR1_LRCLK_RET_SEL			13
#define PCMR1_LRCLK_RET_SEL__NO_RETIMIMG	(0 << PCMR1_LRCLK_RET_SEL)
#define PCMR1_LRCLK_RET_SEL__RETIME_BY_1_CYCLE	(1 << PCMR1_LRCLK_RET_SEL)
#define PCMR1_SCLK_SEL				14
#define PCMR1_SCLK_SEL__FROM_PAD		(0 << PCMR1_SCLK_SEL)
#define PCMR1_SCLK_SEL__FROM_PCM_PLAYER_0	(1 << PCMR1_SCLK_SEL)
#define PCMR1_LRCLK_SEL				15
#define PCMR1_LRCLK_SEL__FROM_PAD		(0 << PCMR1_LRCLK_SEL)
#define PCMR1_LRCLK_SEL__FROM_PCM_PLAYER_0	(1 << PCMR1_LRCLK_SEL)

struct snd_stm_stx7141_glue {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7141_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7141_glue *stx7141_glue = entry->private_data;

	BUG_ON(!stx7141_glue);
	BUG_ON(!snd_stm_magic_valid(stx7141_glue));

	snd_iprintf(buffer, "--- snd_stx7141_glue ---\n");
	snd_iprintf(buffer, "IO_CTRL (0x%p) = 0x%08x\n",
			IO_CTRL(stx7141_glue->base),
			readl(IO_CTRL(stx7141_glue->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7141_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7141_glue *stx7141_glue;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	stx7141_glue = kzalloc(sizeof(*stx7141_glue), GFP_KERNEL);
	if (!stx7141_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7141_glue);

	result = snd_stm_memory_request(pdev, &stx7141_glue->mem_region,
			&stx7141_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Additional procfs info */
	snd_stm_info_register(&stx7141_glue->proc_entry, "stx7141_glue",
			snd_stm_stx7141_glue_dump_registers, stx7141_glue);

	platform_set_drvdata(pdev, stx7141_glue);

	return result;

error_memory_request:
	snd_stm_magic_clear(stx7141_glue);
	kfree(stx7141_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7141_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7141_glue *stx7141_glue = platform_get_drvdata(pdev);

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!stx7141_glue);
	BUG_ON(!snd_stm_magic_valid(stx7141_glue));

	/* Remove procfs entry */
	snd_stm_info_unregister(stx7141_glue->proc_entry);

	snd_stm_memory_release(stx7141_glue->mem_region, stx7141_glue->base);

	snd_stm_magic_clear(stx7141_glue);
	kfree(stx7141_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7141_glue_driver = {
	.driver.name = "snd_stx7141_glue",
	.probe = snd_stm_stx7141_glue_probe,
	.remove = snd_stm_stx7141_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx7141_init(void)
{
	int result;

	snd_stm_printd(0, "%s()\n", __func__);

	if (cpu_data->type != CPU_STX7141) {
		snd_stm_printe("Not supported (other than STx7141) SOC "
				"detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	result = platform_driver_register(&snd_stm_stx7141_glue_driver);
	if (result != 0) {
		snd_stm_printe("Failed to register audio glue driver!\n");
		goto error_glue_driver_register;
	}

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards!\n");
		goto error_card_register;
	}

	return 0;

error_card_register:
	platform_driver_unregister(&snd_stm_stx7141_glue_driver);
error_glue_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7141_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	platform_driver_unregister(&snd_stm_stx7141_glue_driver);
}

MODULE_AUTHOR("Stephen Gallimore <stephen.gallimore@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7141 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7141_init);
module_exit(snd_stm_stx7141_exit);
