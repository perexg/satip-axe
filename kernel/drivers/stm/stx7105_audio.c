/*
 * Copyright (c) 2010-2011 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <sound/stm.h>



/* Audio subsystem resources ---------------------------------------------- */

/* Audio subsystem glue */

static struct platform_device stx7105_glue = {
	.name          = "snd_stx7105_glue",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe210200, 0xc),
	}
};

/* Internal DAC */

static struct platform_device stx7105_conv_dac_mem = {
	.name          = "snd_conv_dac_mem",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe210100, 0x4),
	},
	.dev.platform_data = &(struct snd_stm_conv_dac_mem_info) {
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
	},
};

/* PCM players  */

static struct snd_stm_pcm_player_info stx7105_pcm_player_0_info = {
	.name = "PCM player #0 (HDMI)",
	.ver = 6,
	.card_device = 0,
	.clock_name = "CLKC_FS0_CH1",
	.channels = 8,
	.fdma_initiator = 0,
	.fdma_request_line = 39,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7105_pcm_player_0_pad_config = {
	.gpios_num = 6,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(10, 3, 1),	/* MCLK */
		STM_PAD_PIO_OUT(10, 4, 1),	/* LRCLK */
		STM_PAD_PIO_OUT(10, 5, 1),	/* SCLK */
		STM_PAD_PIO_OUT(10, 0, 1),	/* DATA0 */
		STM_PAD_PIO_OUT(10, 1, 1),	/* DATA1 */
		STM_PAD_PIO_OUT(10, 2, 1),	/* DATA2 */
	},
};

static struct platform_device stx7105_pcm_player_0 = {
	.name          = "snd_pcm_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd104d00, 0x28),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1400), -1),
	},
	.dev.platform_data = &stx7105_pcm_player_0_info,
};

static struct snd_stm_pcm_player_info stx7105_pcm_player_1_info = {
	.name = "PCM player #1",
	.ver = 6,
	.card_device = 1,
	.clock_name = "CLKC_FS0_CH2",
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 34,
	/* .pad_config set by stx7105_configure_audio() */
};

/* Notice that PCM Player 1 has no MCLK line output,
 * what makes it almost useless... */

static struct stm_pad_config stx7105_pcm_player_1_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(10, 7, 1),	/* DATA */
		STM_PAD_PIO_OUT(11, 0, 1),	/* LRCLK */
		STM_PAD_PIO_OUT(11, 1, 1),	/* SCLK */
	},
};

static struct stm_pad_config stx7106_pcm_player_1_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(11, 0, 1),	/* LRCLK */
		STM_PAD_PIO_OUT(11, 1, 1),	/* SCLK */
		STM_PAD_PIO_OUT(11, 2, 1),	/* DATA */
	},
};

static struct platform_device stx7105_pcm_player_1 = {
	.name          = "snd_pcm_player",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd101800, 0x28),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1420), -1),
	},
	.dev.platform_data = &stx7105_pcm_player_1_info,
};

/* SPDIF player */

static struct snd_stm_spdif_player_info stx7105_spdif_player_info = {
	.name = "SPDIF player (HDMI)",
	.ver = 4,
	.card_device = 2,
	.clock_name = "CLKC_FS0_CH3",
	.fdma_initiator = 0,
	.fdma_request_line = 40,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7105_spdif_player_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(10, 6, 1),
	},
};

static struct platform_device stx7105_spdif_player = {
	.name          = "snd_spdif_player",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd104c00, 0x44),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1460), -1),
	},
	.dev.platform_data = &stx7105_spdif_player_info,
};

/* I2S to SPDIF converters */

static struct platform_device stx7105_conv_i2sspdif_0 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105000, 0x224),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x0a00), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7105_conv_i2sspdif_1 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105400, 0x224),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x0a20), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 2,
		.channel_to = 3,
	},
};

static struct platform_device stx7105_conv_i2sspdif_2 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105800, 0x224),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x0a40), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 4,
		.channel_to = 5,
	},
};

static struct platform_device stx7105_conv_i2sspdif_3 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 3,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105c00, 0x224),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x0a60), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM reader */

static struct snd_stm_pcm_reader_info stx7105_pcm_reader_info = {
	.name = "PCM Reader",
	.ver = 5,
	.card_device = 3,
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 37,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7105_pcm_reader_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(10, 7, -1),	/* DATA */
		STM_PAD_PIO_IN(11, 0, -1),	/* SCLK */
		STM_PAD_PIO_IN(11, 1, -1),	/* LRCLK */
	},
};

static struct stm_pad_config stx7106_pcm_reader_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(11, 0, -1),	/* SCLK */
		STM_PAD_PIO_IN(11, 1, -1),	/* LRCLK */
		STM_PAD_PIO_IN(11, 2, -1),	/* DATA */
	},
};

static struct platform_device stx7105_pcm_reader = {
	.name          = "snd_pcm_reader",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd102000, 0x28),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1440), -1),
	},
	.dev.platform_data = &stx7105_pcm_reader_info,
};

/* Devices */

static struct platform_device *stx7105_audio_devices[] __initdata = {
	&stx7105_glue,
	&stx7105_conv_dac_mem,
	&stx7105_pcm_player_0,
	&stx7105_pcm_player_1,
	&stx7105_spdif_player,
	&stx7105_conv_i2sspdif_0,
	&stx7105_conv_i2sspdif_1,
	&stx7105_conv_i2sspdif_2,
	&stx7105_conv_i2sspdif_3,
	&stx7105_pcm_reader,
};

static int __init stx7105_audio_devices_setup(void)
{
	/* Ugly but quick hack to have SPDIF player & I2S to SPDIF
	 * converters enabled without loading STMFB...
	 * TODO: do this in some sane way! */
	{
		void *hdmi_gpout = ioremap(0xfd104020, 4);
		writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
		iounmap(hdmi_gpout);
	}

	return platform_add_devices(stx7105_audio_devices,
			ARRAY_SIZE(stx7105_audio_devices));
}
device_initcall(stx7105_audio_devices_setup);

/* Configuration */

void __init stx7105_configure_audio(struct stx7105_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->pcm_player_0_output >
			stx7105_pcm_player_0_output_disabled) {
		int unused = 3 - config->pcm_player_0_output;

		stx7105_pcm_player_0_info.pad_config =
				&stx7105_pcm_player_0_pad_config;

		stx7105_pcm_player_0_pad_config.gpios_num -= unused;
	}

	if (config->spdif_player_output_enabled)
		stx7105_spdif_player_info.pad_config =
				&stx7105_spdif_player_pad_config;

	switch (cpu_data->type) {
	case CPU_STX7105:
		if (config->pcm_player_1_enabled)
			stx7105_pcm_player_1_info.pad_config =
					&stx7105_pcm_player_1_pad_config;
		if (config->pcm_reader_input_enabled)
			stx7105_pcm_reader_info.pad_config =
					&stx7105_pcm_reader_pad_config;
		break;
	case CPU_STX7106:
		if (config->pcm_player_1_enabled)
			stx7105_pcm_player_1_info.pad_config =
					&stx7106_pcm_player_1_pad_config;
		if (config->pcm_reader_input_enabled)
			stx7105_pcm_reader_info.pad_config =
					&stx7106_pcm_reader_pad_config;
		break;
	default:
		BUG();
		break;
	}
}
