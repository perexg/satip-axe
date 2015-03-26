/*
 *   STMicrolectronics Freeman 510/520/530/540 (FLI7510/FLI7520/FLI7530/
 *   FLI7540) audio glue driver
 *
 *   Copyright (c) 2010-2011 STMicroelectronics Limited
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
 */

#include <asm/clock.h>
#include <asm/irq-ilc.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <sound/core.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Audio glue driver implementation
 */

#define AUD_CONFIG_REG1(base)		((base) + 0x00)
#define SPDIF_CLK			0
#define SPDIF_CLK__CLK_256FS_FREE_RUN	(0 << SPDIF_CLK)
#define SPDIF_CLK__M_CLK_SPDIF		(1 << SPDIF_CLK)
#define SPDIF_CLK__M_CLK_HDMI		(2 << SPDIF_CLK)
#define SPDIF_CLK__M_CLK_I2S		(3 << SPDIF_CLK)
#define SPDIF_CLK__CLK_256FS_DEC_1	(4 << SPDIF_CLK)
#define SPDIF_CLK__CLK_256FS_DEC_2	(5 << SPDIF_CLK)
#define MAIN_CLK			3
#define MAIN_CLK__CLK_256FS_FREE_RUN	(0 << MAIN_CLK)
#define MAIN_CLK__M_CLK_MAIN		(1 << MAIN_CLK)
#define MAIN_CLK__M_CLK_HDMI		(2 << MAIN_CLK)
#define MAIN_CLK__M_CLK_I2S		(3 << MAIN_CLK)
#define MAIN_CLK__CLK_256FS_DEC_1	(4 << MAIN_CLK)
#define MAIN_CLK__CLK_256FS_DEC_2	(5 << MAIN_CLK)
#define ENC_CLK				6
#define ENC_CLK__CLK_256FS_FREE_RUN	(0 << ENC_CLK)
#define ENC_CLK__M_CLK_ENC		(1 << ENC_CLK)
#define ENC_CLK__M_CLK_HDMI		(2 << ENC_CLK)
#define ENC_CLK__M_CLK_I2S		(3 << ENC_CLK)
#define ENC_CLK__CLK_256FS_DEC_1	(4 << ENC_CLK)
#define ENC_CLK__CLK_256FS_DEC_2	(5 << ENC_CLK)
#define DAC_CLK				9
#define DAC_CLK__CLK_256FS_FREE_RUN	(0 << DAC_CLK)
#define DAC_CLK__M_CLK_DAC		(1 << DAC_CLK)
#define DAC_CLK__M_CLK_HDMI		(2 << DAC_CLK)
#define DAC_CLK__M_CLK_I2S		(3 << DAC_CLK)
#define DAC_CLK__CLK_256FS_DEC_1	(4 << DAC_CLK)
#define DAC_CLK__CLK_256FS_DEC_2	(5 << DAC_CLK)
#define ADC_CLK				12
#define ADC_CLK__CLK_256FS_FREE_RUN	(0 << ADC_CLK)
#define ADC_CLK__M_CLK_ADC		(1 << ADC_CLK)
#define ADC_CLK__M_CLK_HDMI		(2 << ADC_CLK)
#define ADC_CLK__M_CLK_I2S		(3 << ADC_CLK)
#define ADC_CLK__CLK_256FS_DEC_1	(4 << ADC_CLK)
#define ADC_CLK__CLK_256FS_DEC_2	(5 << ADC_CLK)
#define SPDIF_CLK_DIV2_EN		30
#define SPDIF_CLK_DIV2_EN__DISABLED	(0 << SPDIF_CLK_DIV2_EN)
#define SPDIF_CLK_DIV2_EN__ENABLED	(1 << SPDIF_CLK_DIV2_EN)
#define SPDIF_IN_PAD_HYST_EN		31
#define SPDIF_IN_PAD_HYST_EN__DISABLED  (0 << SPDIF_IN_PAD_HYST_EN)
#define SPDIF_IN_PAD_HYST_EN__ENABLED	(1 << SPDIF_IN_PAD_HYST_EN)

#define AUD_CONFIG_REG2(base)		((base) + 0x04)
#define SPDIF				0
#define SPDIF__PLAYER			(0 << SPDIF)
#define SPDIF__READER			(1 << SPDIF)
#define FLI7510_MAIN_I2S		1
#define FLI7510_MAIN_I2S__PCM_PLAYER_0	(0 << FLI7510_MAIN_I2S)
#define FLI7510_MAIN_I2S__PCM_READER_1	(1 << FLI7510_MAIN_I2S)
#define FLI7510_SEC_I2S			2
#define FLI7510_SEC_I2S__PCM_PLAYER_1	(0 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_PLAYER_2	(1 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_PLAYER_3	(2 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_PLAYER_4	(3 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_READER_0	(4 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_READER_3	(5 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_READER_4	(6 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__AATV_I2S0	(7 << FLI7510_SEC_I2S)
#define FLI7520_MAIN_I2S		1
#define FLI7520_MAIN_I2S__PCM_READER_1	(0 << FLI7520_MAIN_I2S)
#define FLI7520_MAIN_I2S__PCM_READER_5	(1 << FLI7520_MAIN_I2S)
#define FLI7520_MAIN_I2S__PCM_PLAYER_0	(2 << FLI7520_MAIN_I2S)
#define FLI7520_MAIN_I2S__AATV_I2S0	(3 << FLI7520_MAIN_I2S)
#define FLI7520_SEC_I2S			3
#define FLI7520_SEC_I2S__PCM_PLAYER_1	(0 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_PLAYER_2	(1 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_PLAYER_4	(3 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_READER_0	(4 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_READER_3	(5 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_READER_4	(6 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__AATV_I2S0	(7 << FLI7520_SEC_I2S)
#define SPDIF_PLAYER_EN			31
#define SPDIF_PLAYER_EN__DISABLED	(0 << SPDIF_PLAYER_EN)
#define SPDIF_PLAYER_EN__ENABLED	(1 << SPDIF_PLAYER_EN)

struct snd_stm_fli7510_glue {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_fli7510_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_fli7510_glue *fli7510_glue = entry->private_data;

	BUG_ON(!fli7510_glue);
	BUG_ON(!snd_stm_magic_valid(fli7510_glue));

	snd_iprintf(buffer, "--- snd_fli7510_glue ---\n");
	snd_iprintf(buffer, "AUD_CONFIG_REG1 (0x%p) = 0x%08x\n",
			AUD_CONFIG_REG1(fli7510_glue->base),
			readl(AUD_CONFIG_REG1(fli7510_glue->base)));
	snd_iprintf(buffer, "AUD_CONFIG_REG2 (0x%p) = 0x%08x\n",
			AUD_CONFIG_REG2(fli7510_glue->base),
			readl(AUD_CONFIG_REG2(fli7510_glue->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_fli7510_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_fli7510_glue *fli7510_glue;
	unsigned int value;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	fli7510_glue = kzalloc(sizeof(*fli7510_glue), GFP_KERNEL);
	if (!fli7510_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(fli7510_glue);

	result = snd_stm_memory_request(pdev, &fli7510_glue->mem_region,
			&fli7510_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Clocking configuration:
	 *
	 * clk_256fs_free_run --> SPDIF clock
	 *
	 * clk_256fs_dec_1 -----> MAIN (LS) clock
	 *
	 *                    ,-> ENCODER clock
	 * clk_256fs_dec_2 --<
	 *                    `-> DAC (HP, AUX) clock
	 */
	value = SPDIF_CLK__CLK_256FS_FREE_RUN;
	value |= MAIN_CLK__CLK_256FS_DEC_1;
	value |= ENC_CLK__CLK_256FS_DEC_2;
	value |= DAC_CLK__CLK_256FS_DEC_2;
	value |= SPDIF_CLK_DIV2_EN__DISABLED;
	value |= SPDIF_IN_PAD_HYST_EN__DISABLED;
	writel(value, AUD_CONFIG_REG1(fli7510_glue->base));

	value = SPDIF__PLAYER;
	if (cpu_data->type == CPU_FLI7510) {
		value |= FLI7510_MAIN_I2S__PCM_PLAYER_0;
		value |= FLI7510_SEC_I2S__PCM_PLAYER_1;
	} else {
		value |= FLI7520_MAIN_I2S__PCM_PLAYER_0;
		value |= FLI7520_SEC_I2S__PCM_PLAYER_1;
	}
	value |= SPDIF_PLAYER_EN__ENABLED;
	writel(value, AUD_CONFIG_REG2(fli7510_glue->base));

	/* Additional procfs info */
	snd_stm_info_register(&fli7510_glue->proc_entry, "fli7510_glue",
			snd_stm_fli7510_glue_dump_registers, fli7510_glue);

	platform_set_drvdata(pdev, fli7510_glue);

	return result;

error_memory_request:
	snd_stm_magic_clear(fli7510_glue);
	kfree(fli7510_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_fli7510_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_fli7510_glue *fli7510_glue = platform_get_drvdata(pdev);

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!fli7510_glue);
	BUG_ON(!snd_stm_magic_valid(fli7510_glue));

	/* Remove procfs entry */
	snd_stm_info_unregister(fli7510_glue->proc_entry);

	/* Disable audio outputs */

	snd_stm_memory_release(fli7510_glue->mem_region, fli7510_glue->base);

	snd_stm_magic_clear(fli7510_glue);
	kfree(fli7510_glue);

	return 0;
}

static struct platform_driver snd_stm_fli7510_glue_driver = {
	.driver.name = "snd_fli7510_glue",
	.probe = snd_stm_fli7510_glue_probe,
	.remove = snd_stm_fli7510_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_fli7510_init(void)
{
	int result;

	snd_stm_printd(0, "%s()\n", __func__);

	switch (cpu_data->type) {
	case CPU_FLI7510:
	case CPU_FLI7520:
	case CPU_FLI7530:
	case CPU_FLI7540:
		break;
	default:
		snd_stm_printe("Unsupported (non-Freeman) SOC detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	result = platform_driver_register(&snd_stm_fli7510_glue_driver);
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
	platform_driver_unregister(&snd_stm_fli7510_glue_driver);
error_glue_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_fli7510_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	platform_driver_unregister(&snd_stm_fli7510_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics Freeman 510/520/530/540 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_fli7510_init);
module_exit(snd_stm_fli7510_exit);
