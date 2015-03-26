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
#include <linux/stm/stx7200.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>



/* Audio subsystem resources ---------------------------------------------- */

/* Audio subsystem glue */

static struct platform_device stx7200_glue = {
	.name          = "snd_stx7200_glue",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd601200, 0xc),
	}
};

/* Internal DACs */

static struct platform_device stx7200_conv_dac_mem_0 = {
	.name          = "snd_conv_dac_mem",
	.id            = 0,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd601400, 0x4),
	},
	.dev.platform_data = &(struct snd_stm_conv_dac_mem_info) {
		/* .ver = see snd_stm_stx7200_init() */
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7200_conv_dac_mem_1 = {
	.name          = "snd_conv_dac_mem",
	.id            = 1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd601500, 0x4),
	},
	.dev.platform_data = &(struct snd_stm_conv_dac_mem_info) {
		/* .ver = see snd_stm_stx7200_init() */
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
	},
};

/* PCM players */

static struct platform_device stx7200_pcm_player_0 = {
	.name          = "snd_pcm_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd101000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(39), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 0,
		.clock_name = "CLKC_FS0_CH1",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 33,
	},
};

static struct stm_pad_config stx7200_pcm_player_1_auddig0_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* CONF_PAD_AUD0: AUDDIG pads configuration:
		 *   0 = two 2 channels audio streams,
		 *   1 = one 10 channels audio stream
		 * CONF_PAD_AUD1: controls AUDDIG0PCMCLKOUT pad direction:
		 *   0 = pad is output (clk_pcmout1)
		 *   1 = pad is input (clk_pcmin1)
		 * CONF_PAD_AUD2: controls AUDDIG1PCMCLKOUT pad direction:
		 *   0 = pad is output (clk_pcmout2 or clk_pcmout3)
		 *   1 = pad is input (clk_pcmin2 or clk_pcmin3) */
		STM_PAD_SYS_CFG(20, 0, 2, 0),
	},
};

static struct stm_pad_config stx7200_pcm_player_1_mii1_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* CONF_PAD_ETH4: MII1RXD.3, MII1TXCLK,
		 *   MII1COL, MII1CRS, MII1MDINT &
		 *   MII1PHYCLK pads function:
		 *   0 = ethernet, 1 = audio,
		 * CONF_PAD_ETH5: MIICRS pad direction when CONF_PAD_ETH4 = 1:
		 *   0 = output (clk_pcmout1), 1 = input (clk_pcmin1) */
		STM_PAD_SYS_CFG(41, 16 + 4, 16 + 5, 1),
	},
};

static struct platform_device stx7200_pcm_player_1 = {
	.name          = "snd_pcm_player",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd102000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(40), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 1,
		.clock_name = "CLKC_FS0_CH2",
		.channels = 6,
		.fdma_initiator = 0,
		.fdma_request_line = 34,
		/* .pad_config set by stx7200_configure_audio() */
	},
};

static struct stm_pad_config stx7200_pcm_player_2_auddig1_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* CONF_PAD_AUD0: AUDDIG pads configuration:
		 *   0 = two 2 channels audio streams,
		 *   1 = one 10 channels audio stream
		 * CONF_PAD_AUD1: controls AUDDIG0PCMCLKOUT pad direction:
		 *   0 = pad is output (clk_pcmout1)
		 *   1 = pad is input (clk_pcmin1)
		 * CONF_PAD_AUD2: controls AUDDIG1PCMCLKOUT pad direction:
		 *   0 = pad is output (clk_pcmout2 or clk_pcmout3)
		 *   1 = pad is input (clk_pcmin2 or clk_pcmin3) */
		STM_PAD_SYS_CFG(20, 0, 2, 0),
	},
};

static struct stm_pad_config stx7200_pcm_player_2_mii0_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* CONF_PAD_ETH0: MII0.RXERR, MII0.TXD.2-3,
		 *   MII0.RXCLK, MII0.RXD.2-3 pads function:
		 *   0 = ethernet, 1 = audio,
		 * CONF_PAD_ETH1: MII0.TXCLK pad direction:
		 *   0 = output (clk_pcmout2), 1 = input (clk_pcmin2) , */
		STM_PAD_SYS_CFG(41, 16 + 0, 16 + 1, 1),
	},
};

static struct platform_device stx7200_pcm_player_2 = {
	.name          = "snd_pcm_player",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd103000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(41), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #2",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 2,
		.clock_name = "CLKC_FS0_CH3",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 35,
		/* .pad_config set by stx7200_configure_audio() */
	},
};

static struct stm_pad_config stx7200_pcm_player_3_auddig01_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* CONF_PAD_AUD0: AUDDIG pads configuration:
		 *   0 = two 2 channels audio streams,
		 *   1 = one 10 channels audio stream
		 * CONF_PAD_AUD1: controls AUDDIG0PCMCLKOUT pad direction:
		 *   0 = pad is output (clk_pcmout1)
		 *   1 = pad is input (clk_pcmin1)
		 * CONF_PAD_AUD2: controls AUDDIG1PCMCLKOUT pad direction:
		 *   0 = pad is output (clk_pcmout2 or clk_pcmout3)
		 *   1 = pad is input (clk_pcmin2 or clk_pcmin3) */
		STM_PAD_SYS_CFG(20, 0, 2, 1),
	},
};

static struct platform_device stx7200_pcm_player_3 = {
	.name          = "snd_pcm_player",
	.id            = 3,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd104000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(42), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #3",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 3,
		.clock_name = "CLKC_FS0_CH4",
		.channels = 10,
		.fdma_initiator = 0,
		.fdma_request_line = 36,
		/* .pad_config set by stx7200_configure_audio() */
	},
};

/* SPDIF player */

static struct platform_device stx7200_spdif_player = {
	.name          = "snd_spdif_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		/* memory resource size set in snd_stm_stx7200_init() */
		STM_PLAT_RESOURCE_MEM(0xfd105000, 0),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(37), -1),
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 5,
		.clock_name = "CLKC_FS1_CH4",
		.fdma_initiator = 0,
		.fdma_request_line = 38,
	},
};

/* HDMI output devices
 * Cut 1.0: Please note that "HDTVOutBaseAddress" (0xFD10C000) from page 54
 * of "7200 Programming Manual, Volume 2" is wrong. The correct HDMI players
 * subsystem base address is "HDMIPlayerBaseAddress" (0xFD106000) from
 * page 488 of the manual.
 * Cut 2.0: HDTVout IP is identical to STx7111's one and the base address
 * is 0xFD112000. */

static struct platform_device stx7200_hdmi_pcm_player = {
	.name          = "snd_pcm_player",
	.id            = 4, /* HDMI PCM player is no. 4 */
	.num_resources = 2,
	.resource      = (struct resource []) {
		/* memory resource set in snd_stm_stx7200_init() */
		STM_PLAT_RESOURCE_MEM(0, 0),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(62), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player HDMI",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 4,
		.clock_name = "CLKC_FS1_CH3",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 39,
	},
};

static struct platform_device stx7200_hdmi_spdif_player = {
	.name          = "snd_spdif_player",
	.id            = 1, /* HDMI SPDIF player is no. 1 */
	.num_resources = 2,
	.resource      = (struct resource []) {
		/* memory resource set in snd_stm_stx7200_init() */
		STM_PLAT_RESOURCE_MEM(0, 0),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(63), -1),
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player HDMI",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 6,
		.clock_name = "CLKC_FS1_CH3",
		.fdma_initiator = 0,
		.fdma_request_line = 40,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_conv_i2sspdif_0 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd113000, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(64), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.4",
		.channel_from = 0,
		.channel_to = 1,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_conv_i2sspdif_1 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd113400, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(65), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.4",
		.channel_from = 2,
		.channel_to = 3,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_conv_i2sspdif_2 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd113800, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(66), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.4",
		.channel_from = 4,
		.channel_to = 5,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_conv_i2sspdif_3 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 3,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd113c00, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(67), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.4",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM readers */

static struct platform_device stx7200_pcm_reader_0 = {
	.name          = "snd_pcm_reader",
	.id = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd100000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(38), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader #0",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 7,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 37,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_pcm_reader_1 = {
	.name          = "snd_pcm_reader",
	.id = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd114000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(35), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader #1",
		.ver = 5,
		.card_device = 8,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 51,
	},
};

/* Devices */

static struct platform_device *stx7200_audio_devices[] __initdata = {
	&stx7200_glue,
	&stx7200_conv_dac_mem_0,
	&stx7200_conv_dac_mem_1,
	&stx7200_pcm_player_0,
	&stx7200_pcm_player_1,
	&stx7200_pcm_player_2,
	&stx7200_pcm_player_3,
	&stx7200_spdif_player,
	&stx7200_hdmi_pcm_player,
	&stx7200_hdmi_spdif_player,
	&stx7200_pcm_reader_0,
};

static struct platform_device *stx7200_cut2_audio_devices[] __initdata = {
	&stx7200_conv_i2sspdif_0,
	&stx7200_conv_i2sspdif_1,
	&stx7200_conv_i2sspdif_2,
	&stx7200_conv_i2sspdif_3,
	&stx7200_pcm_reader_1,
};

#define SET_VER(_info_struct_, _device_, _ver_) \
		(((struct _info_struct_ *)_device_.dev.platform_data)->ver = \
				_ver_)

static int __init stx7200_audio_devices_setup(void)
{
	int result;
	int ver;

	if (cpu_data->type != CPU_STX7200) {
		BUG();
		return -ENODEV;
	}

	/* We assume farther that MEM resource is first, let's check it... */
	BUG_ON(stx7200_spdif_player.resource[0].flags != IORESOURCE_MEM);
	BUG_ON(stx7200_hdmi_pcm_player.resource[0].flags != IORESOURCE_MEM);
	BUG_ON(stx7200_hdmi_spdif_player.resource[0].flags != IORESOURCE_MEM);

	switch (cpu_data->cut_major) {
	case 1:
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_0, 5);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_1, 5);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_2, 5);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_3, 5);

		SET_VER(snd_stm_spdif_player_info, stx7200_spdif_player, 3);
		stx7200_spdif_player.resource[0].end += 0x40;

		SET_VER(snd_stm_pcm_player_info, stx7200_hdmi_pcm_player, 5);
		stx7200_hdmi_pcm_player.resource[0].start = 0xfd106d00;
		stx7200_hdmi_pcm_player.resource[0].end = 0xfd106d27;

		SET_VER(snd_stm_spdif_player_info,
				stx7200_hdmi_spdif_player, 3);
		stx7200_hdmi_spdif_player.resource[0].start = 0xfd106c00;
		stx7200_hdmi_spdif_player.resource[0].end = 0xfd106c3f;

		SET_VER(snd_stm_pcm_reader_info, stx7200_pcm_reader_0, 3);

		break;

	case 2 ... 3:
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_0, 6);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_1, 6);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_2, 6);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_3, 6);

		SET_VER(snd_stm_spdif_player_info, stx7200_spdif_player, 4);
		stx7200_spdif_player.resource[0].end += 0x44;

		SET_VER(snd_stm_pcm_player_info, stx7200_hdmi_pcm_player, 6);
		stx7200_hdmi_pcm_player.resource[0].start = 0xfd112d00;
		stx7200_hdmi_pcm_player.resource[0].end = 0xfd112d27;

		SET_VER(snd_stm_spdif_player_info,
				stx7200_hdmi_spdif_player, 4);
		stx7200_hdmi_spdif_player.resource[0].start = 0xfd112c00;
		stx7200_hdmi_spdif_player.resource[0].end = 0xfd112c43;

		ver = (cpu_data->cut_major == 2 ? 5 : 6);
		SET_VER(snd_stm_pcm_reader_info, stx7200_pcm_reader_0, ver);
		SET_VER(snd_stm_pcm_reader_info, stx7200_pcm_reader_1, ver);

		break;

	default:
		printk(KERN_ERR "%s(): Not supported STx7200 cut %d detected!"
				"\n", __func__, cpu_data->cut_major);
		return -ENODEV;
	}

	/* Ugly but quick hack to have HDMI SPDIF player & I2S to SPDIF
	 * converters enabled without loading STMFB...
	 * TODO: do this in some sane way! */
	{
		void *hdmi_gpout;

		if (cpu_data->cut_major == 1)
			hdmi_gpout = ioremap(0xfd106020, 4);
		else
			hdmi_gpout = ioremap(0xfd112020, 4);

		writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
		iounmap(hdmi_gpout);
	}

	result = platform_add_devices(stx7200_audio_devices,
			ARRAY_SIZE(stx7200_audio_devices));

	if (result == 0 && cpu_data->cut_major > 1)
		result = platform_add_devices(stx7200_cut2_audio_devices,
				ARRAY_SIZE(stx7200_cut2_audio_devices));

	return result;
}
device_initcall(stx7200_audio_devices_setup);

/* Configuration */

void __init stx7200_configure_audio(struct stx7200_audio_config *config)
{
	static int configured;
	struct snd_stm_pcm_player_info *info;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		return;

	info = stx7200_pcm_player_1.dev.platform_data;

	switch (config->pcm_player_1_routing) {
	case stx7200_pcm_player_1_disabled:
		/* Nothing to do... */
		break;
	case stx7200_pcm_player_1_auddig0:
		info->pad_config = &stx7200_pcm_player_1_auddig0_pad_config;
		break;
	case stx7200_pcm_player_1_mii1:
		info->pad_config = &stx7200_pcm_player_1_mii1_pad_config;
		break;
	default:
		BUG();
		break;
	}

	info = stx7200_pcm_player_2.dev.platform_data;

	switch (config->pcm_player_2_routing) {
	case stx7200_pcm_player_2_disabled:
		/* Nothing to do... */
		break;
	case stx7200_pcm_player_2_auddig1:
		info->pad_config = &stx7200_pcm_player_2_auddig1_pad_config;
		break;
	case stx7200_pcm_player_2_mii0:
		info->pad_config = &stx7200_pcm_player_2_mii0_pad_config;
		break;
	default:
		BUG();
		break;
	}

	info = stx7200_pcm_player_3.dev.platform_data;

	switch (config->pcm_player_3_routing) {
	case stx7200_pcm_player_3_disabled:
		/* Nothing to do... */
		break;
	case stx7200_pcm_player_3_aiddig0_auddig1:
		info->pad_config = &stx7200_pcm_player_3_auddig01_pad_config;
		break;
	default:
		BUG();
		break;
	}
}
