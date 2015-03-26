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
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <sound/stm.h>



/* Audio subsystem resources ---------------------------------------------- */

/* Audio subsystem glue */

static struct platform_device stx7100_glue = {
	.name          = "snd_stx7100_glue",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x19210200, 0x4),
	},
};

/* Internal DAC */

static struct snd_stm_conv_dac_mem_info stx7100_conv_dac_mem_info = {
	/* .ver = see stx7100_configure_audio() */
	.source_bus_id = "snd_pcm_player.1",
	.channel_from = 0,
	.channel_to = 1,
};

static struct platform_device stx7100_conv_dac_mem = {
	.name          = "snd_conv_dac_mem",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x19210100, 0x4),
	},
	.dev.platform_data = &stx7100_conv_dac_mem_info,
};

/* PCM players */

struct snd_stm_pcm_player_info stx7100_pcm_player_0_info = {
	.name = "PCM player #0 (HDMI)",
	/* .ver = see stx7100_configure_audio() */
	.card_device = 0,
	.clock_name = "CLKC_FS0_CH1",
	.channels = 10,
	.fdma_initiator = 1,
	/* .fdma_request_line = see stx7100_configure_audio() */
};

static struct platform_device stx7100_pcm_player_0 = {
	.name          = "snd_pcm_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18101000, 0x28),
		STM_PLAT_RESOURCE_IRQ(144, -1),
	},
	.dev.platform_data = &stx7100_pcm_player_0_info,
};

struct snd_stm_pcm_player_info stx7100_pcm_player_1_info = {
	.name = "PCM player #1",
	/* .ver = see stx7100_configure_audio() */
	.card_device = 1,
	.clock_name = "CLKC_FS0_CH2",
	.channels = 2,
	.fdma_initiator = 1,
	/* .fdma_request_line = see stx7100_configure_audio() */
};

static struct platform_device stx7100_pcm_player_1 = {
	.name          = "snd_pcm_player",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18101800, 0x28),
		STM_PLAT_RESOURCE_IRQ(145, -1),
	},
	.dev.platform_data = &stx7100_pcm_player_1_info,
};

/* SPDIF player */

struct snd_stm_spdif_player_info stx7100_spdif_player_info = {
	.name = "SPDIF player (HDMI)",
	/* .ver = see stx7100_configure_audio() */
	.card_device = 2,
	.clock_name = "CLKC_FS0_CH3",
	.fdma_initiator = 1,
	/* .fdma_request_line = see stx7100_configure_audio() */
};

static struct platform_device stx7100_spdif_player = {
	.name          = "snd_spdif_player",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18103000, 0x40),
		STM_PLAT_RESOURCE_IRQ(147, -1),
	},
	.dev.platform_data = &stx7100_spdif_player_info,
};

/* HDMI-connected I2S to SPDIF converter */

static struct snd_stm_conv_i2sspdif_info stx7100_conv_i2sspdif_info = {
	/* .ver = see stx7100_configure_audio() */
	.source_bus_id = "snd_pcm_player.0",
	.channel_from = 0,
	.channel_to = 1,
};

static struct platform_device stx7100_conv_i2sspdif = {
	.name          = "snd_conv_i2sspdif",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18103800, 0x224),
		STM_PLAT_RESOURCE_IRQ(142, -1),
	},
	.dev.platform_data = &stx7100_conv_i2sspdif_info,
};

/* PCM reader */

struct snd_stm_pcm_reader_info stx7100_pcm_reader_info = {
	.name = "PCM Reader",
	/* .ver = see stx7100_configure_audio() */
	.card_device = 3,
	.channels = 2,
	.fdma_initiator = 1,
	/* .fdma_request_line = see stx7100_configure_audio() */
};

static struct platform_device stx7100_pcm_reader = {
	.name          = "snd_pcm_reader",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18102000, 0x28),
		STM_PLAT_RESOURCE_IRQ(146, -1),
	},
	.dev.platform_data = &stx7100_pcm_reader_info,
};

/* Devices */

static struct platform_device *stx7100_audio_devices[] __initdata = {
	&stx7100_glue,
	&stx7100_pcm_player_0,
	&stx7100_pcm_player_1,
	&stx7100_conv_dac_mem,
	&stx7100_spdif_player,
	&stx7100_conv_i2sspdif,
	&stx7100_pcm_reader,
};

static int __init stx7100_audio_devices_setup(void)
{
	switch (cpu_data->type) {
	case CPU_STX7100:
		/* FDMA request line configuration */
		stx7100_pcm_player_0_info.fdma_request_line = 26;
		stx7100_pcm_player_1_info.fdma_request_line = 27;
		stx7100_spdif_player_info.fdma_request_line = 29;
		stx7100_pcm_reader_info.fdma_request_line = 28;

		/* IP versions */
		stx7100_pcm_reader_info.ver = 1;
		if (cpu_data->cut_major < 3) {
			/* STx7100 cut < 3.0 */
			stx7100_pcm_player_0_info.ver = 1;
			stx7100_pcm_player_1_info.ver = 1;
		} else {
			/* STx7100 cut >= 3.0 */
			stx7100_pcm_player_0_info.ver = 2;
			stx7100_pcm_player_1_info.ver = 2;
		}
		stx7100_spdif_player_info.ver = 1;
		stx7100_conv_i2sspdif_info.ver = 1;

		break;

	case CPU_STX7109:
		/* FDMA request line configuration */
		stx7100_pcm_player_0_info.fdma_request_line = 24;
		stx7100_pcm_player_1_info.fdma_request_line = 25;
		stx7100_spdif_player_info.fdma_request_line = 27;
		stx7100_pcm_reader_info.fdma_request_line = 26;

		/* IP versions */
		stx7100_pcm_reader_info.ver = 2;
		if (cpu_data->cut_major < 3) {
			/* STx7109 cut < 3.0 */
			stx7100_pcm_player_0_info.ver = 3;
			stx7100_pcm_player_1_info.ver = 3;
		} else {
			/* STx7109 cut >= 3.0 */
			stx7100_pcm_player_0_info.ver = 4;
			stx7100_pcm_player_1_info.ver = 4;
		}
		stx7100_spdif_player_info.ver = 2;
		stx7100_conv_i2sspdif_info.ver = 2;

		break;

	default:
		BUG();
		return -ENODEV;
	}

	return platform_add_devices(stx7100_audio_devices,
			ARRAY_SIZE(stx7100_audio_devices));
}
device_initcall(stx7100_audio_devices_setup);
