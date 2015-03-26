/*
 * (c) 2010-2011 STMicroelectronics Limited
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
#include <linux/stm/fli7510.h>
#include <asm/irq-ilc.h>
#include <sound/stm.h>



/* Audio subsystem resources ---------------------------------------------- */

/* Audio subsystem glue */

static struct platform_device fli7510_glue = {
	.name = "snd_fli7510_glue",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd670000, 0x1c),
	}
};

/* Audio mute */
static struct platform_device fli7510_snd_conv_gpio = {
	.name = "snd_conv_gpio",
	.id = 0,
	.dev.platform_data = &(struct snd_stm_conv_gpio_info) {
		.group = "speakers",

		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__LEFT_JUSTIFIED |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,

		.mute_supported = 1,
		.mute_gpio = stm_gpio(26, 0), /* Audio mute */
		.mute_value = 0,
	},
};


/* PCM players */

static struct snd_stm_pcm_player_info fli7510_pcm_player_0_info = {
	.name = "PCM player LS",
	.ver = 6,
	.card_device = 0,
	.clock_name = "CLKC_FS_DEC_1",
	.channels = 8,
	.fdma_initiator = 0,
	.fdma_request_line = 41,
	/* .pad_config set by fli7510_configure_audio() */
};

static struct stm_pad_config fli7510_pcm_player_0_pad_config = {
	.gpios_num = 7,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(6, 4, 1),	/* I2SA MCLK  */
		STM_PAD_PIO_OUT(6, 5, 1),	/* I2SA LRCLK */
		STM_PAD_PIO_OUT(6, 6, 1),	/* I2SA SCLK  */
		STM_PAD_PIO_OUT(6, 0, 1),	/* I2SA DATA0 */
		STM_PAD_PIO_OUT(6, 1, 1),	/* I2SA DATA1 */
		STM_PAD_PIO_OUT(6, 2, 1),	/* I2SA DATA2 */
		STM_PAD_PIO_OUT(6, 3, 1),	/* I2SA DATA3 */
	},
};

static struct platform_device fli7510_pcm_player_0 = {
	.name = "snd_pcm_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd618000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(10), -1),
	},
	.dev.platform_data = &fli7510_pcm_player_0_info,
};

static struct snd_stm_pcm_player_info fli7510_pcm_player_1_info = {
	.name = "PCM player AUX/Encoder",
	.ver = 6,
	.card_device = 1,
	.clock_name = "CLKC_FS_DEC_2",
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 42,
	/* .pad_config set by fli7510_configure_audio() */
};

static struct stm_pad_config fli7510_pcm_player_1_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(8, 1, 1),	/* I2SC MCLK */
		STM_PAD_PIO_OUT(8, 2, 1),	/* I2SC LRCLK */
		STM_PAD_PIO_OUT(8, 3, 1),	/* I2SC SCLK */
		STM_PAD_PIO_OUT(8, 0, 1),	/* I2SC DATA */
	},
};

static struct platform_device fli7510_pcm_player_1 = {
	.name = "snd_pcm_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd61c000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(11), -1),
	},
	.dev.platform_data = &fli7510_pcm_player_1_info,
};

static struct platform_device fli7510_pcm_player_2 = {
	.name = "snd_pcm_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd620000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(12), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player HP",
		.ver = 6,
		.card_device = 2,
		.clock_name = "CLKC_FS_DEC_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 43,
	},
};

static struct platform_device fli7510_pcm_player_3 = {
	.name = "snd_pcm_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd628000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(13), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player Monitor",
		.ver = 6,
		.card_device = 3,
		.clock_name = "CLKC_FS_DEC_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 44,
	},
};

static struct platform_device fli7520_pcm_player_3 = {
	.name = "snd_pcm_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd630000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(14), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player AV out",
		.ver = 6,
		.card_device = 3,
		.clock_name = "CLKC_FS_DEC_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 44,
	},
};

static struct platform_device fli7510_pcm_player_4 = {
	.name = "snd_pcm_player",
	.id = 4,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd630000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(14), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player AV out",
		.ver = 6,
		.card_device = 4,
		.clock_name = "CLKC_FS_DEC_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 45,
	},
};

/* SPDIF player */

static struct snd_stm_spdif_player_info fli7510_spdif_player_info = {
	.name = "SPDIF player",
	.ver = 4,
	.card_device = 5,
	.clock_name = "CLKC_FS_FREE_RUN",
	.fdma_initiator = 0,
	.fdma_request_line = 40,
	/* .pad_config set by fli7510_configure_audio() */
};

static struct stm_pad_config fli7510_spdif_player_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(6, 7, 1),  /* SPDIF OUT */
	},
};

static struct platform_device fli7510_spdif_player = {
	.name = "snd_spdif_player",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd638000, 0x44),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(4), -1),
	},
	.dev.platform_data = &fli7510_spdif_player_info,
};

/* PCM readers */

static struct platform_device fli7510_pcm_reader_0 = {
	.name = "snd_pcm_reader",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd604000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(5), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader SPDIF",
		.ver = 5,
		.card_device = 6,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 46,
	},
};

static struct platform_device fli7510_pcm_reader_1 = {
	.name = "snd_pcm_reader",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd608000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(6), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader HDMI",
		.ver = 5,
		.card_device = 7,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 47,
	},
};

static struct platform_device fli7510_pcm_reader_2 = {
	.name = "snd_pcm_reader",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd60c000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(7), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader I2S",
		.ver = 5,
		.card_device = 8,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 48,
	},
};

static struct platform_device fli7510_pcm_reader_3 = {
	.name = "snd_pcm_reader",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd610000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(8), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader Demod",
		.ver = 5,
		.card_device = 9,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 49,
	},
};

static struct platform_device fli7510_pcm_reader_4 = {
	.name = "snd_pcm_reader",
	.id = 4,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd614000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(9), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader AV in",
		.ver = 5,
		.card_device = 10,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 50,
	},
};

static struct platform_device fli7520_pcm_reader_5 = {
	.name = "snd_pcm_reader",
	.id = 5,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd628000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(13), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader HDMI #2",
		.ver = 5,
		.card_device = 11,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 45,
	},
};

/* Devices */

static struct platform_device *fli7510_audio_devices[] __initdata = {
	&fli7510_glue,
	&fli7510_snd_conv_gpio,
	&fli7510_pcm_player_0,
	&fli7510_pcm_player_1,
	&fli7510_pcm_player_2,
	&fli7510_pcm_player_3,
	&fli7510_pcm_player_4,
	&fli7510_spdif_player,
	&fli7510_pcm_reader_0,
	&fli7510_pcm_reader_1,
	&fli7510_pcm_reader_2,
	&fli7510_pcm_reader_3,
	&fli7510_pcm_reader_4,
};

static struct platform_device *fli7520_audio_devices[] __initdata = {
	&fli7510_glue,
	&fli7510_snd_conv_gpio,
	&fli7510_pcm_player_0,
	&fli7510_pcm_player_1,
	&fli7510_pcm_player_2,
	&fli7520_pcm_player_3,
	&fli7510_spdif_player,
	&fli7510_pcm_reader_0,
	&fli7510_pcm_reader_1,
	&fli7510_pcm_reader_2,
	&fli7510_pcm_reader_3,
	&fli7510_pcm_reader_4,
	&fli7520_pcm_reader_5,
};

static int __init fli7510_audio_devices_setup(void)
{
	if (cpu_data->type == CPU_FLI7510)
		return platform_add_devices(fli7510_audio_devices,
				ARRAY_SIZE(fli7510_audio_devices));
	else
		return platform_add_devices(fli7520_audio_devices,
				ARRAY_SIZE(fli7520_audio_devices));
}
device_initcall(fli7510_audio_devices_setup);

/* Configuration */

void __init fli7510_configure_audio(struct fli7510_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->pcm_player_0_output_mode >
			fli7510_pcm_player_0_output_disabled) {
		int unused = 4 - config->pcm_player_0_output_mode;

		fli7510_pcm_player_0_info.pad_config =
				&fli7510_pcm_player_0_pad_config;

		fli7510_pcm_player_0_pad_config.gpios_num -= unused;
	}

	if (config->pcm_player_1_output_enabled)
		fli7510_pcm_player_1_info.pad_config =
				&fli7510_pcm_player_1_pad_config;

	if (config->spdif_player_output_enabled)
		fli7510_spdif_player_info.pad_config =
				&fli7510_spdif_player_pad_config;
}
