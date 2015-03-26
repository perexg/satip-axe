/*
 *   STMicrolectronics STx7111 audio glue driver
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
#include <linux/irq.h>
#include <linux/io.h>
#include <sound/core.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Audio glue driver implementation
 */

#define IO_CTRL(base)		((base) + 0x00)
#define PCM_CLK_EN		0
#define PCM_CLK_EN__INPUT	(0 << PCM_CLK_EN)
#define PCM_CLK_EN__OUTPUT	(1 << PCM_CLK_EN)
#define SPDIFHDMI_EN		3
#define SPDIFHDMI_EN__INPUT	(0 << SPDIFHDMI_EN)
#define SPDIFHDMI_EN__OUTPUT	(1 << SPDIFHDMI_EN)
#define PCMPLHDMI_EN		5
#define PCMPLHDMI_EN__INPUT	(0 << PCMPLHDMI_EN)
#define PCMPLHDMI_EN__OUTPUT	(1 << PCMPLHDMI_EN)
#define CLKREC_SEL		9
#define CLKREC_SEL__PCMPLHDMI	(0 << CLKREC_SEL)
#define CLKREC_SEL__SPDIFHDMI	(1 << CLKREC_SEL)
#define CLKREC_SEL__PCMPL1	(2 << CLKREC_SEL)

struct snd_stm_stx7111_glue {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7111_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7111_glue *stx7111_glue = entry->private_data;

	BUG_ON(!stx7111_glue);
	BUG_ON(!snd_stm_magic_valid(stx7111_glue));

	snd_iprintf(buffer, "--- snd_stx7111_glue ---\n");
	snd_iprintf(buffer, "IO_CTRL (0x%p) = 0x%08x\n",
			IO_CTRL(stx7111_glue->base),
			readl(IO_CTRL(stx7111_glue->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7111_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7111_glue *stx7111_glue;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	stx7111_glue = kzalloc(sizeof(*stx7111_glue), GFP_KERNEL);
	if (!stx7111_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7111_glue);

	result = snd_stm_memory_request(pdev, &stx7111_glue->mem_region,
			&stx7111_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Enable audio outputs */
	writel(PCMPLHDMI_EN__OUTPUT | SPDIFHDMI_EN__OUTPUT |
			PCM_CLK_EN__OUTPUT, IO_CTRL(stx7111_glue->base));

	/* Additional procfs info */
	snd_stm_info_register(&stx7111_glue->proc_entry, "stx7111_glue",
			snd_stm_stx7111_glue_dump_registers, stx7111_glue);

	platform_set_drvdata(pdev, stx7111_glue);

	return result;

error_memory_request:
	snd_stm_magic_clear(stx7111_glue);
	kfree(stx7111_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7111_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7111_glue *stx7111_glue = platform_get_drvdata(pdev);

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!stx7111_glue);
	BUG_ON(!snd_stm_magic_valid(stx7111_glue));

	/* Remove procfs entry */
	snd_stm_info_unregister(stx7111_glue->proc_entry);

	/* Disable audio outputs */
	writel(PCMPLHDMI_EN__INPUT | SPDIFHDMI_EN__INPUT |
			PCM_CLK_EN__INPUT, IO_CTRL(stx7111_glue->base));

	snd_stm_memory_release(stx7111_glue->mem_region, stx7111_glue->base);

	snd_stm_magic_clear(stx7111_glue);
	kfree(stx7111_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7111_glue_driver = {
	.driver.name = "snd_stx7111_glue",
	.probe = snd_stm_stx7111_glue_probe,
	.remove = snd_stm_stx7111_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx7111_init(void)
{
	int result;

	snd_stm_printd(0, "%s()\n", __func__);

	if (cpu_data->type != CPU_STX7111) {
		snd_stm_printe("Not supported (other than STx7111) SOC "
				"detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	result = platform_driver_register(&snd_stm_stx7111_glue_driver);
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
	platform_driver_unregister(&snd_stm_stx7111_glue_driver);
error_glue_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7111_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	platform_driver_unregister(&snd_stm_stx7111_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7111 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7111_init);
module_exit(snd_stm_stx7111_exit);
