/*
 * (c) 2010-2011 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <asm/irq-ilc.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <sound/stm.h>



/* Audio subsystem resources ---------------------------------------------- */

/* Internal DAC */

static struct platform_device stx7108_conv_dac = {
	.name = "snd_conv_dac_sc",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dac_sc_info) {
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
		.nrst = { SYS_CFG_BANK4, 41, 0, 0 },
		.mode = { SYS_CFG_BANK4, 41, 1, 2 },
		.nsb = { SYS_CFG_BANK4, 41, 3, 3 },
		.softmute = { SYS_CFG_BANK4, 41, 4, 4 },
		.pdana = { SYS_CFG_BANK4, 41, 5, 5 },
		.pndbg = { SYS_CFG_BANK4, 41, 6, 6 },
	},
};

/* PCM players  */

static struct platform_device stx7108_pcm_player_0 = {
	.name = "snd_pcm_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd000d00, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(105), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0 (HDMI)",
		.ver = 6,
		.card_device = 0,
		.clock_name = "CLKC_FS0_CH1",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 23,
	},
};

static struct platform_device stx7108_pcm_player_1 = {
	.name = "snd_pcm_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd002000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(70), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1 (analog)",
		.ver = 6,
		.card_device = 1,
		.clock_name = "CLKC_FS0_CH2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 24,
	},
};

static struct snd_stm_pcm_player_info stx7108_pcm_player_2_info = {
	.name = "PCM player #2",
	.ver = 6,
	.card_device = 2,
	.clock_name = "CLKC_FS0_CH4",
	.channels = 8,
	.fdma_initiator = 0,
	.fdma_request_line = 25,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7108_pcm_player_2_pad_config = {
	.gpios_num = 7,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(25, 1, 1),	/* CLKIN_OUT */
		STM_PAD_PIO_OUT(25, 2, 1),	/* LRCLK */
		STM_PAD_PIO_OUT(25, 3, 1),	/* SCLK */
		STM_PAD_PIO_OUT(25, 4, 1),	/* DATA0 */
		STM_PAD_PIO_OUT(25, 5, 1),	/* DATA1 */
		STM_PAD_PIO_OUT(25, 6, 1),	/* DATA2 */
		STM_PAD_PIO_OUT(25, 7, 1),	/* DATA3 */
	},
};


static struct platform_device stx7108_pcm_player_2 = {
	.name = "snd_pcm_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd003000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(71), -1),
	},
	.dev.platform_data = &stx7108_pcm_player_2_info,
};

/* SPDIF player */

static struct snd_stm_spdif_player_info stx7108_spdif_player_info = {
	.name = "SPDIF player",
	.ver = 4,
	.card_device = 3,
	.clock_name = "CLKC_FS0_CH3",
	.fdma_initiator = 0,
	.fdma_request_line = 27,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7108_spdif_player_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(26, 0, 1),
	},
};

static struct platform_device stx7108_spdif_player = {
	.name = "snd_spdif_player",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd000c00, 0x44),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(104), -1),
	},
	.dev.platform_data = &stx7108_spdif_player_info,
};

/* I2S to SPDIF converters */

static struct platform_device stx7108_conv_i2sspdif_0 = {
	.name = "snd_conv_i2sspdif",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd001000, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(103), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7108_conv_i2sspdif_1 = {
	.name = "snd_conv_i2sspdif",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd001400, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(102), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 2,
		.channel_to = 3,
	},
};

static struct platform_device stx7108_conv_i2sspdif_2 = {
	.name = "snd_conv_i2sspdif",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd001800, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(101), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 4,
		.channel_to = 5,
	},
};

static struct platform_device stx7108_conv_i2sspdif_3 = {
	.name = "snd_conv_i2sspdif",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd001c00, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(100), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM reader */

static struct snd_stm_pcm_reader_info stx7108_pcm_reader_info = {
	.name = "PCM Reader",
	.ver = 5,
	.card_device = 4,
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 26,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7108_pcm_reader_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(26, 1, 1),	/* LRCLK */
		STM_PAD_PIO_IN(26, 2, 1),	/* SCLK */
		STM_PAD_PIO_IN(26, 3, 1),	/* DATA */
	},
};

static struct platform_device stx7108_pcm_reader = {
	.name = "snd_pcm_reader",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd004000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(72), -1),
	},
	.dev.platform_data = &stx7108_pcm_reader_info,
};

static struct platform_device *stx7108_audio_devices[] __initdata = {
	&stx7108_conv_dac,
	&stx7108_pcm_player_0,
	&stx7108_pcm_player_1,
	&stx7108_pcm_player_2,
	&stx7108_spdif_player,
	&stx7108_conv_i2sspdif_0,
	&stx7108_conv_i2sspdif_1,
	&stx7108_conv_i2sspdif_2,
	&stx7108_conv_i2sspdif_3,
	&stx7108_pcm_reader,
};

static int __init stx7108_audio_devices_setup(void)
{
	/* Ugly but quick hack to have SPDIF player & I2S to SPDIF
	 * converters enabled without loading STMFB...
	 * TODO: do this in some sane way! */
	{
		void *hdmi_gpout = ioremap(0xfd000020, 4);
		writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
		iounmap(hdmi_gpout);
	}

	return platform_add_devices(stx7108_audio_devices,
			ARRAY_SIZE(stx7108_audio_devices));
}
device_initcall(stx7108_audio_devices_setup);

/* Configuration */

void __init stx7108_configure_audio(struct stx7108_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->pcm_player_2_output >
			stx7108_pcm_player_2_output_disabled) {
		int unused = 4 - config->pcm_player_2_output;

		stx7108_pcm_player_2_info.pad_config =
				&stx7108_pcm_player_2_pad_config;

		stx7108_pcm_player_2_pad_config.gpios_num -= unused;
	}

	if (config->spdif_player_output_enabled)
		stx7108_spdif_player_info.pad_config =
				&stx7108_spdif_player_pad_config;

	if (config->pcm_reader_input_enabled)
		stx7108_pcm_reader_info.pad_config =
				&stx7108_pcm_reader_pad_config;
}
