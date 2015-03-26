/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/phy.h>
#include <linux/stm/miphy.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/device.h>
#include <linux/delay.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

/* ... and yes, MDIOs are supposed to be configured as OUT, not BIDIR... */

static struct stm_pad_config *stx7105_ethernet_pad_configs[] = {
	[0] = (struct stm_pad_config []) {
		[stx7105_ethernet_mode_mii] = {
			.gpios_num = 19,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(7, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(7, 5, -1),	/* RXERR */
				STM_PAD_PIO_OUT(7, 6, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(7, 7, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(8, 0, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(8, 1, 1),	/* TXD.3 */
				STM_PAD_PIO_OUT(8, 2, 1),	/* TXEN */
				STM_PAD_PIO_OUT(8, 3, 1),	/* MDIO */
				STM_PAD_PIO_OUT(8, 4, 1),	/* MDC */
				STM_PAD_PIO_IN(8, 5, -1),	/* RXCLK */
				STM_PAD_PIO_IN(8, 6, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(8, 7, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(9, 0, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(9, 1, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(9, 2, -1),	/* TXCLK */
				STM_PAD_PIO_IN(9, 3, -1),	/* COL */
				STM_PAD_PIO_IN(9, 4, -1),	/* CRS */
				STM_PAD_PIO_OUT_NAMED(9, 5, 1, "PHYCLK"),
				STM_PAD_PIO_IN_NAMED(9, 6, -1, "MDINT"),
			},
			.sysconfs_num = 5,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* ethernet_interface_on */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* enMII: 0 = reverse MII mode, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
				/* mii_mdio_select:
				 * 0 = from GMAC,
				 * 1 = miim_dio from external input */
				STM_PAD_SYS_CFG(7, 17, 17, 0),
				/* rmii_mode:
				 * 0 = MII, 1 = RMII interface activated */
				/* CUT 1: This register wasn't connected,
				 * so only MII available!!! */
				STM_PAD_SYS_CFG(7, 18, 18, 0),
				/* phy_intf_select:
				 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
				STM_PAD_SYS_CFG(7, 25, 26, 0),
			},
		},
		[stx7105_ethernet_mode_gmii] = { /* 7106 only! */
			.gpios_num = 27,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(7, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(7, 5, -1),	/* RXERR */
				STM_PAD_PIO_OUT(7, 6, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(7, 7, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(8, 0, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(8, 1, 1),	/* TXD.3 */
				STM_PAD_PIO_OUT(8, 2, 1),	/* TXEN */
				STM_PAD_PIO_OUT(8, 3, 1),	/* MDIO */
				STM_PAD_PIO_OUT(8, 4, 1),	/* MDC */
				STM_PAD_PIO_IN(8, 5, -1),	/* RXCLK */
				STM_PAD_PIO_IN(8, 6, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(8, 7, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(9, 0, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(9, 1, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(9, 2, -1),	/* TXCLK */
				STM_PAD_PIO_IN(9, 3, -1),	/* COL */
				STM_PAD_PIO_IN(9, 4, -1),	/* CRS */
				STM_PAD_PIO_OUT_NAMED(9, 5, 1, "PHYCLK"),
				STM_PAD_PIO_IN_NAMED(9, 6, -1, "MDINT"),
#if 0
/* Note: TXER line is not configured (at this moment) as:
   1. It is generally useless and not used at all by a lot of PHYs.
   2. PIO9.7 is muxed with HDMI hot plug detect, which is likely to be used.
   3. Apparently the GMAC (or the SOC) is broken anyway and it doesn't drive
      it correctly ;-) */
				STM_PAD_PIO_OUT(9, 7, 1),	/* TXER */
#endif
				STM_PAD_PIO_OUT(11, 0, 4),	/* TXD.6 */
				STM_PAD_PIO_OUT(11, 1, 4),	/* TXD.7 */
				STM_PAD_PIO_IN(15, 0, -1),	/* RXD.4 */
				STM_PAD_PIO_IN(15, 1, -1),	/* RXD.5 */
				STM_PAD_PIO_IN(15, 2, -1),	/* RXD.6 */
				STM_PAD_PIO_IN(15, 3, -1),	/* RXD.7 */
				STM_PAD_PIO_OUT(15, 4, 4),	/* TXD.4 */
				STM_PAD_PIO_OUT(15, 5, 4),	/* TXD.5 */
			},
			.sysconfs_num = 5,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* ethernet_interface_on */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* enMII: 0 = reverse MII mode, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
				/* mii_mdio_select:
				 * 0 = from GMAC,
				 * 1 = miim_dio from external input */
				STM_PAD_SYS_CFG(7, 17, 17, 0),
				/* rmii_mode: 0 = MII,
				 * 1 = RMII interface activated */
				/* CUT 1: This register wasn't connected,
				 * so only MII available!!! */
				STM_PAD_SYS_CFG(7, 18, 18, 0),
				/* phy_intf_select:
				 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
				STM_PAD_SYS_CFG(7, 25, 26, 0),
			},
		},
		[stx7105_ethernet_mode_rgmii] = { /* TODO */ },
		[stx7105_ethernet_mode_sgmii] = { /* TODO */ },
		[stx7105_ethernet_mode_rmii] = {
			.gpios_num = 11,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(7, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(7, 5, -1),	/* RXERR */
				STM_PAD_PIO_OUT(7, 6, 2),	/* TXD.0 */
				STM_PAD_PIO_OUT(7, 7, 2),	/* TXD.1 */
				STM_PAD_PIO_OUT(8, 2, 2),	/* TXEN */
				STM_PAD_PIO_OUT(8, 3, 2),	/* MDIO */
				STM_PAD_PIO_OUT(8, 4, 2),	/* MDC */
				STM_PAD_PIO_IN(8, 6, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(8, 7, -1),	/* RXD.1 */
				STM_PAD_PIO_OUT_NAMED(9, 5, 1, "PHYCLK"),
				STM_PAD_PIO_IN_NAMED(9, 6, -1, "MDINT"),
			},
			.sysconfs_num = 5,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* Ethernet ON */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* enMII: 0 = reverse MII mode, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
				/* mii_mdio_select:
				 * 1 = miim_dio from external input,
				 * 0 = from GMAC */
				STM_PAD_SYS_CFG(7, 17, 17, 0),
				/* rmii_mode: 0 = MII,
				 * 1 = RMII interface activated */
				/* CUT 1: This register wasn't connected,
				 * so only MII available!!! */
				STM_PAD_SYS_CFG(7, 18, 18, 1),
				/* phy_intf_select:
				 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
				STM_PAD_SYS_CFG(7, 25, 26, 0),
			},
		},
		[stx7105_ethernet_mode_reverse_mii] = {
			.gpios_num = 19,
			.gpios = (struct stm_pad_gpio []) {
				/* TODO: check what about EXCRS output */
				STM_PAD_PIO_IN(7, 4, -1),	/* RXDV */
				/* TODO: check what about EXCOL output */
				STM_PAD_PIO_IN(7, 5, -1),	/* RXERR */
				STM_PAD_PIO_OUT(7, 6, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(7, 7, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(8, 0, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(8, 1, 1),	/* TXD.3 */
				STM_PAD_PIO_OUT(8, 2, 1),	/* TXEN */
				STM_PAD_PIO_OUT(8, 3, 1),	/* MDIO */
				STM_PAD_PIO_OUT(8, 4, 1),	/* MDC */
				STM_PAD_PIO_IN(8, 5, -1),	/* RXCLK */
				STM_PAD_PIO_IN(8, 6, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(8, 7, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(9, 0, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(9, 1, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(9, 2, -1),	/* TXCLK */
				STM_PAD_PIO_IN(9, 3, -1),	/* COL */
				STM_PAD_PIO_IN(9, 4, -1),	/* CRS */
				STM_PAD_PIO_OUT_NAMED(9, 5, 1, "PHYCLK"),
				STM_PAD_PIO_IN_NAMED(9, 6, -1, "MDINT"),
			},
			.sysconfs_num = 5,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* Ethernet ON */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* enMII: 0 = reverse MII mode, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 0),
				/* mii_mdio_select:
				 * 1 = miim_dio from external input,
				 * 0 = from GMAC */
				STM_PAD_SYS_CFG(7, 17, 17, 0),
				/* rmii_mode: 0 = MII,
				 * 1 = RMII interface activated */
				/* CUT 1: This register wasn't connected,
				 * so only MII available!!! */
				STM_PAD_SYS_CFG(7, 18, 18, 0),
				/* phy_intf_select:
				 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
				STM_PAD_SYS_CFG(7, 25, 26, 0),
			},
		},
	},
	[1] = (struct stm_pad_config []) { /* 7106 only! */
		[stx7105_ethernet_mode_mii] = {
			.gpios_num = 19,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(3, 6, -1),	/* MDINT */
				STM_PAD_PIO_OUT(11, 2, 1),	/* TXEN */
				STM_PAD_PIO_OUT_NAMED(11, 3, 2, "PHYCLK"),
				STM_PAD_PIO_OUT(11, 4, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(11, 5, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(11, 6, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(11, 7, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(15, 6, -1),	/* COL */
				STM_PAD_PIO_IN(15, 7, -1),	/* CRS */
				STM_PAD_PIO_IN(16, 0, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(16, 1, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(16, 2, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(16, 3, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(16, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(16, 5, -1),	/* RXERR */
				STM_PAD_PIO_IN(16, 6, -1),	/* RXCLK */
				STM_PAD_PIO_IN(16, 7, -1),	/* TXCLK */
				STM_PAD_PIO_STUB_NAMED(-1, -1, "MDIO"),
				STM_PAD_PIO_STUB_NAMED(-1, -1, "MDC"),
			},
			.sysconfs_num = 7,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* eth1_mdiin_src_sel:
				 * 1 = mdi in is from PIO11(0)
				 * 0 = mdc in is from PIO3(4) */
				STM_PAD_SYS_CFG(16, 4, 4, -1), /* set below */
				/* eth1_mdcin_src_sel:
				 * 1 = mdc in is from PIO11(1)
				 * 0 = mdc in is from PIO3(5) */
				STM_PAD_SYS_CFG(16, 5, 5, -1), /* set below */
				/* ethernet1_interface_on */
				STM_PAD_SYS_CFG(7, 14, 14, 1),
				/* enMII_eth1:
				 * 0 = reverse MII mode, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 30, 30, 1),
				/* mii_mdio_select_eth1:
				 * 0 = from GMAC,
				 * 1 = miim_dio from external input */
				STM_PAD_SYS_CFG(7, 15, 15, 0),
				/* rmii_mode_eth1:
				 * 0 = MII, 1 = RMII interface activated */
				/* CUT 1: This register wasn't connected,
				 * so only MII available!!! */
				STM_PAD_SYS_CFG(7, 19, 19, 0),
				/* phy_intf_select_eth1:
				 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
				STM_PAD_SYS_CFG(7, 28, 29, 0),
			},
		},
		[stx7105_ethernet_mode_rmii] = {
			.gpios_num = 11,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(3, 6, -1),	/* MDINT */
				STM_PAD_PIO_OUT(11, 2, 3),	/* TXEN */
				STM_PAD_PIO_STUB_NAMED(11, 3, "PHYCLK"),
				STM_PAD_PIO_OUT(11, 4, 3),	/* TXD.0 */
				STM_PAD_PIO_OUT(11, 5, 3),	/* TXD.1 */
				STM_PAD_PIO_IN(16, 0, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(16, 1, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(16, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(16, 5, -1),	/* RXERR */
				STM_PAD_PIO_STUB_NAMED(-1, -1, "MDIO"),
				STM_PAD_PIO_STUB_NAMED(-1, -1, "MDC"),
			},
			.sysconfs_num = 7,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* eth1_mdiin_src_sel:
				 * 1 = mdi in is from PIO11(0)
				 * 0 = mdc in is from PIO3(4) */
				STM_PAD_SYS_CFG(16, 4, 4, -1), /* set below */
				/* eth1_mdcin_src_sel:
				 * 1 = mdc in is from PIO11(1)
				 * 0 = mdc in is from PIO3(5) */
				STM_PAD_SYS_CFG(16, 5, 5, -1), /* set below */
				/* ethernet1_interface_on */
				STM_PAD_SYS_CFG(7, 14, 14, 1),
				/* enMII_eth1:
				 * 0 = reverse MII mode, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 30, 30, 1),
				/* mii_mdio_select_eth1:
				 * 1 = miim_dio from external input,
				 * 0 = from GMAC */
				STM_PAD_SYS_CFG(7, 15, 15, 0),
				/* rmii_mode_eth1: 0 = MII,
				 * 1 = RMII interface activated */
				/* CUT 1: This register wasn't connected,
				 * so only MII available!!! */
				STM_PAD_SYS_CFG(7, 19, 19, 1),
				/* phy_intf_select:
				 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
				STM_PAD_SYS_CFG(7, 28, 29, 0),
			},
		},
	}
};

static void stx7105_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	if (mac_speed_sel)
		sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx7105_ethernet_platform_data[] = {
	{
		.pbl = 32,
		.has_gmac = 1,
		.enh_desc = 1,
		.tx_coe = 1,
		.bugged_jumbo =1,
		.pmt = 1,
		.fix_mac_speed = stx7105_ethernet_fix_mac_speed,
		.init = &stmmac_claim_resource,
	}, {
		.pbl = 32,
		.has_gmac = 1,
		.enh_desc = 1,
		.tx_coe = 1,
		.bugged_jumbo =1,
		.pmt = 1,
		.fix_mac_speed = stx7105_ethernet_fix_mac_speed,
		.init = &stmmac_claim_resource,
	}
};

static struct platform_device stx7105_ethernet_devices[] = {
	{
		.name = "stmmaceth",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd110000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq",
					evt2irq(0x12c0), -1),
		},
		.dev.platform_data = &stx7105_ethernet_platform_data[0],
	}, {
		.name = "stmmaceth",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd118000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq",
					ILC_EXT_IRQ(39), -1),
		},
		.dev.platform_data = &stx7105_ethernet_platform_data[1],
	}
};

void __init stx7105_configure_ethernet(int port,
		struct stx7105_ethernet_config *config)
{
	static int configured[ARRAY_SIZE(stx7105_ethernet_devices)];
	struct stx7105_ethernet_config default_config;
	struct stm_pad_config *pad_config;
	int interface;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7105_ethernet_devices));

	if (cpu_data->type == CPU_STX7105 && port != 0) {
		pr_warning("Ethernet port 1 is not available in this SOC!\n");
		return;
	}

	BUG_ON(configured[port]++);

	if (!config)
		config = &default_config;

	pad_config = &stx7105_ethernet_pad_configs[port][config->mode];

	switch (config->mode) {
	case stx7105_ethernet_mode_gmii:
		BUG_ON(cpu_data->type != CPU_STX7106); /* 7106 only! */
		BUG_ON(port == 1); /* Mode not available on MII1 */
		interface = PHY_INTERFACE_MODE_GMII;
		break;
	case stx7105_ethernet_mode_reverse_mii:
		BUG_ON(port == 1); /* Mode not available on MII1 */
		/* Fall through */
	case stx7105_ethernet_mode_mii:
		if (config->ext_clk)
			stm_pad_set_pio_ignored(pad_config, "PHYCLK");
		interface = PHY_INTERFACE_MODE_MII;
		break;
	case stx7105_ethernet_mode_rmii:
		if (config->ext_clk)
			stm_pad_set_pio_in(pad_config, "PHYCLK", -1);
		interface = PHY_INTERFACE_MODE_RMII;
		break;
	default:
		/* TODO: RGMII and SGMII configurations */
		BUG();
		return;
	}

	switch (port) {
	case 0:
		/* mac_speed */
		stx7105_ethernet_platform_data[0].bsp_priv =
				sysconf_claim(SYS_CFG, 7, 20, 20, "stmmac");

		/* This is a workaround for a problem seen on some boards,
		 * such as the HMP7105, which use the SMSC LAN8700 with no
		 * board level logic to work around conflicts on mode pin
		 * usage with the STx7105.
		 *
		 * Background
		 * The 8700 uses the MII RXD[3] pin as a mode selection at
		 * reset for whether nINT/TXERR/TXD4 is used as nINT or TXERR.
		 * However the 7105 uses the same pin as MODE(9), to determine
		 * which processor boots first.
		 * Assuming the pull up/down resistors are configured so that
		 * the ST40 boots first, this is causes the 8700 to treat
		 * nINT/TXERR as TXERR, which is not what we want.
		 *
		 * Workaround
		 * Force MII_MDINT to be an output, driven low, to indicate
		 * there is no error. */
		if (config->mdint_workaround)
			stm_pad_set_pio_out(pad_config, "MDINT", 0);
		break;
	case 1:
		/* mac_speed */
		stx7105_ethernet_platform_data[0].bsp_priv =
				sysconf_claim(SYS_CFG, 7, 21, 21, "stmmac");

		switch (config->routing.mii1.mdio) {
		case stx7105_ethernet_mii1_mdio_pio3_4:
			stm_pad_set_pio(pad_config, "MDIO", 3, 4);
			stm_pad_set_pio_out(pad_config, "MDIO", 4);
			/* eth1_mdiin_src_sel = 0 */
			pad_config->sysconfs[0].value = 0;
			break;
		case stx7105_ethernet_mii1_mdio_pio11_0:
			stm_pad_set_pio(pad_config, "MDIO", 11, 0);
			stm_pad_set_pio_out(pad_config, "MDIO", 3);
			/* eth1_mdiin_src_sel = 1 */
			pad_config->sysconfs[0].value = 1;
			break;
		default:
			BUG();
			break;
		}

		switch (config->routing.mii1.mdc) {
		case stx7105_ethernet_mii1_mdc_pio3_5:
			stm_pad_set_pio(pad_config, "MDC", 3, 5);
			stm_pad_set_pio_out(pad_config, "MDC", 4);
			/* eth1_mdcin_src_sel = 0 */
			pad_config->sysconfs[1].value = 0;
			break;
		case stx7105_ethernet_mii1_mdc_pio11_1:
			stm_pad_set_pio(pad_config, "MDC", 11, 1);
			stm_pad_set_pio_out(pad_config, "MDC", 3);
			/* eth1_mdcin_src_sel = 1 */
			pad_config->sysconfs[1].value = 1;
			break;
		default:
			BUG();
			break;
		}
		break;
	default:
		BUG();
		break;
	}

	stx7105_ethernet_platform_data[port].custom_cfg = (void *) pad_config;
	stx7105_ethernet_platform_data[port].interface = interface;
	stx7105_ethernet_platform_data[port].bus_id = config->phy_bus;
	stx7105_ethernet_platform_data[port].phy_addr = config->phy_addr;
	stx7105_ethernet_platform_data[port].mdio_bus_data =
		config->mdio_bus_data;

	platform_device_register(&stx7105_ethernet_devices[port]);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7105_usb_dma_mask = DMA_BIT_MASK(32);

#define USB_HOST_PWR "USB_HOST_PWR"
#define USB_PHY_PWR "USB_PHY_PWR"
#define USB_HOST_ACK "USB_HOST_ACK"

static void stx7105_usb_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, USB_HOST_PWR, value);
	stm_device_sysconf_write(device_state, USB_PHY_PWR, value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, USB_HOST_ACK)
				 == value)
			break;
		mdelay(10);
	}
}

static struct stm_plat_usb_data stx7105_usb_platform_data[] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		.device_config = &(struct stm_device_config){
			/* .pad_config created in stx7105_configure_usb() */
			.sysconfs_num = 3,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG(32, 4, 4, USB_HOST_PWR),
				STM_DEVICE_SYS_CFG(32, 6, 6, USB_PHY_PWR),
				STM_DEVICE_SYS_STA(15, 4, 4, USB_HOST_ACK),
			},
			.power = stx7105_usb_power,
		}
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		.device_config = &(struct stm_device_config){
			/* .pad_config created in stx7105_configure_usb() */
			.sysconfs_num = 3,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG(32, 5, 5, USB_HOST_PWR),
				STM_DEVICE_SYS_CFG(32, 7, 7, USB_PHY_PWR),
				STM_DEVICE_SYS_STA(15, 5, 5, USB_HOST_ACK),
			},
			.power = stx7105_usb_power,
		}
	},
};

static struct platform_device stx7105_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &stx7105_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7105_usb_platform_data[0],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfe1ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", evt2irq(0x1720),
						    -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfe1ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", evt2irq(0x1700),
						    -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfe100000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfe1fff00,
						    0x100),
		},
	},
	[1] = {
		.name = "stm-usb",
		.id = 1,
		.dev = {
			.dma_mask = &stx7105_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7105_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfeaffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", evt2irq(0x13e0),
						    -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfeaffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", evt2irq(0x13c0),
						    -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfea00000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfeafff00,
						    0x100),
		},
	},
};

void __init stx7105_configure_usb(int port, struct stx7105_usb_config *config)
{
	static int configured[ARRAY_SIZE(stx7105_usb_devices)];
	struct stm_pad_config *pad_config;
	struct sysconf_field *sc;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7105_usb_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	pad_config = stm_pad_config_alloc(2, 3);
	BUG_ON(!pad_config);

	/* set the pad_config in the device_config */
	stx7105_usb_platform_data[port].device_config->pad_config =
		pad_config;

	if (config->ovrcur_mode == stx7105_usb_ovrcur_disabled) {
		/* cfg_usbX_ovrcurr_enable */
		stm_pad_config_add_sys_cfg(pad_config,
				4, 11 + port, 11 + port, 0);
	} else {
		/* cfg_usbX_ovrcurr_enable */
		stm_pad_config_add_sys_cfg(pad_config,
				4, 11 + port, 11 + port, 1);

		if (config->ovrcur_mode == stx7105_usb_ovrcur_active_high)
			/* usbX_prt_ovrcurr_pol */
			stm_pad_config_add_sys_cfg(pad_config,
					4, 3 + port, 3 + port, 1);
		else if (config->ovrcur_mode == stx7105_usb_ovrcur_active_low)
			/* usbX_prt_ovrcurr_pol */
			stm_pad_config_add_sys_cfg(pad_config,
					4, 3 + port, 3 + port, 0);
		else
			BUG();

		if (port == 0) {
			switch (config->routing.usb0.ovrcur) {
			case stx7105_usb0_ovrcur_pio4_4:
				if (cpu_data->type == CPU_STX7105)
					/* usb0_prt_ovrcurr_sel: 0 = PIO4.4 */
					stm_pad_config_add_sys_cfg(pad_config,
							4, 5, 5, 0);
				else
					/* usb0_prt_ovrcurr_sel[0..1]:
					 * 0 = PIO4.4 */
					stm_pad_config_add_sys_cfg(pad_config,
							4, 5, 6, 0);
				stm_pad_config_add_pio_in(pad_config,
						4, 4, -1);
				break;
			case stx7105_usb0_ovrcur_pio12_5:
				if (cpu_data->type == CPU_STX7105)
					/* usb0_prt_ovrcurr_sel: 1 = PIO12.5 */
					stm_pad_config_add_sys_cfg(pad_config,
							4, 5, 5, 1);
				else
					/* usb0_prt_ovrcurr_sel[0..1]:
					 * 1 = PIO12.5 */
					stm_pad_config_add_sys_cfg(pad_config,
							4, 5, 6, 1);
				stm_pad_config_add_pio_in(pad_config,
						12, 5, -1);
				break;
			case stx7105_usb0_ovrcur_pio14_4:
				/* 7106 only! */
				BUG_ON(cpu_data->type != CPU_STX7106);
				/* usb0_prt_ovrcurr_sel[0..1]:
				 * 2 = PIO14.4 */
				stm_pad_config_add_sys_cfg(pad_config,
						4, 5, 6, 2);
				stm_pad_config_add_pio_in(pad_config,
						14, 4, -1);
				break;
			default:
				BUG();
				break;
			}
		} else {
			switch (config->routing.usb1.ovrcur) {
			case stx7105_usb1_ovrcur_pio4_6:
				/* usb1_prt_ovrcurr_sel: 0 = PIO4.6 */
				if (cpu_data->type == CPU_STX7105)
					stm_pad_config_add_sys_cfg(pad_config,
							4, 6, 6, 0);
				else
					stm_pad_config_add_sys_cfg(pad_config,
							4, 8, 8, 0);
				stm_pad_config_add_pio_in(pad_config,
						4, 6, -1);
				break;
			case stx7105_usb1_ovrcur_pio14_6:
				/* usb1_prt_ovrcurr_sel: 1 = PIO14.6 */
				if (cpu_data->type == CPU_STX7105)
					stm_pad_config_add_sys_cfg(pad_config,
							4, 6, 6, 1);
				else
					stm_pad_config_add_sys_cfg(pad_config,
							4, 8, 8, 1);
				stm_pad_config_add_pio_in(pad_config,
						14, 6, -1);
				break;
			default:
				BUG();
				break;
			}
		}
	}

	/* USB edge rise and DC shift - STLinux Bugzilla 10991 */
	sc = sysconf_claim(SYS_CFG, 4, 13+(port<<1), 14+(port<<1), "USB");
	sysconf_write(sc, 2);

	if (config->pwr_enabled) {
		if (port == 0) {
			switch (config->routing.usb0.pwr) {
			case stx7105_usb0_pwr_pio4_5:
				stm_pad_config_add_pio_out(pad_config,
						4, 5, 4);
				break;
			case stx7105_usb0_pwr_pio12_6:
				stm_pad_config_add_pio_out(pad_config,
						12, 6, 3);
				break;
			case stx7105_usb0_pwr_pio14_5:
				/* 7106 only! */
				BUG_ON(cpu_data->type != CPU_STX7106);
				stm_pad_config_add_pio_out(pad_config,
						14, 5, 1);
				break;
			default:
				BUG();
				break;
			}
		} else {
			switch (config->routing.usb1.pwr) {
			case stx7105_usb1_pwr_pio4_7:
				stm_pad_config_add_pio_out(pad_config,
						4, 7, 4);
				break;
			case stx7105_usb1_pwr_pio14_7:
				stm_pad_config_add_pio_out(pad_config,
						14, 7, 2);
				break;
			default:
				BUG();
				break;
			}
		}

	}

	platform_device_register(&stx7105_usb_devices[port]);
}



/* MiPHY resources -------------------------------------------------------- */

static struct stm_tap_sysconf tap_sysconf = {
	.tck 	= 	{ SYS_CFG, 33, 0, 0},
	.tms 	= 	{ SYS_CFG, 33, 5, 5},
	.tdi 	= 	{ SYS_CFG, 33, 1, 1},
	.tdo 	= 	{ SYS_STA, 0, 1, 1},
	.tap_en = 	{ SYS_CFG, 33, 6, 6},
	.trstn  = 	{ SYS_CFG, 33, 4, 4},
};

static struct stm_plat_tap_data stx7105_tap_platform_data = {
	.miphy_first = 0,
	.miphy_count = 2,
	.miphy_modes = (enum miphy_mode[2]) {SATA_MODE, SATA_MODE},
	.tap_sysconf = &tap_sysconf,
};

static struct platform_device stx7105_tap_device = {
	.name	= "stm-miphy-tap",
	.id	= 0,
	.num_resources = 0,
	.dev = {
		.platform_data = &stx7105_tap_platform_data,
	}
};

static int __init stx7105_miphy_postcore_setup(void)
{
	if (cpu_data->type == CPU_STX7105)
		stx7105_tap_platform_data.miphy_count = 1;

	return platform_device_register(&stx7105_tap_device);
}
postcore_initcall(stx7105_miphy_postcore_setup);



/* SATA resources --------------------------------------------------------- */

static struct sysconf_field *sata_host_reset_sysconf[2];
static struct sysconf_field *sata_phy_reset_sysconf[2];

static void stx7105_restart_sata(int port)
{
	/* Toggle the sysconf bits to reset the host and phy */
	sysconf_write(sata_host_reset_sysconf[port], 1);
	sysconf_write(sata_phy_reset_sysconf[port], 1);
	msleep(1);
	sysconf_write(sata_host_reset_sysconf[port], 0);
	sysconf_write(sata_phy_reset_sysconf[port], 0);
}

static void stx7105_sata_power(int port, enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 0 : 1;

	sysconf_write(sata_host_reset_sysconf[port], value);
	sysconf_write(sata_phy_reset_sysconf[port], value);
	mdelay(10);

}
static void stx7105_sata0_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	stx7105_sata_power(0, power);
	return ;
}

static void stx7105_sata1_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	stx7105_sata_power(1, power);
	return ;
}

static struct platform_device stx7105_sata_device = {
	.name = "sata-stm",
	.id = -1,
	.dev.platform_data = &(struct stm_plat_sata_data) {
		.phy_init = 0,
		.pc_glue_logic_init = 0,
		.only_32bit = 0,
		.host_restart = stx7105_restart_sata,
		.port_num = 0,
		.miphy_num = 0,
		.device_config = &(struct stm_device_config) {
			.sysconfs_num = 0,
			.power = stx7105_sata0_power,
		}
	},
	.num_resources = 3,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe209000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("hostc", evt2irq(0xb00), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("dmac", evt2irq(0xa80), -1),
	},
};

static struct platform_device stx7106_sata_devices[] = {
	{
		.name = "sata-stm",
		.id = 0,
		.dev.platform_data = &(struct stm_plat_sata_data) {
			.phy_init = 0,
			.pc_glue_logic_init = 0,
			.only_32bit = 0,
			.oob_wa = 1,
			.host_restart = stx7105_restart_sata,
			.port_num = 0,
			.miphy_num = 0,
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 0,
				.power = stx7105_sata0_power,
			}
		},
		.num_resources = 3,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe208000, 0x1000),
			STM_PLAT_RESOURCE_IRQ_NAMED("hostc",
					evt2irq(0xb00), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("dmac",
					evt2irq(0xa80), -1),
		},
	}, {
		.name = "sata-stm",
		.id = 1,
		.dev.platform_data = &(struct stm_plat_sata_data) {
			.phy_init = 0,
			.pc_glue_logic_init = 0,
			.only_32bit = 0,
			.oob_wa = 1,
			.host_restart = stx7105_restart_sata,
			.port_num = 1,
			.miphy_num = 1,
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 0,
				.power = stx7105_sata1_power,
			}
		},
		.num_resources = 3,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe209000, 0x1000),
			STM_PLAT_RESOURCE_IRQ_NAMED("hostc",
						ILC_EXT_IRQ(34), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("dmac",
						ILC_EXT_IRQ(33), -1),
		},
	}
};

void __init stx7105_configure_sata(int port)
{
	static int configured[ARRAY_SIZE(stx7106_sata_devices)];
	static int initialized;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7106_sata_devices));

	if (cpu_data->type == CPU_STX7105 &&
			(port != 0 || cpu_data->cut_major < 3)) {
		pr_warning("SATA port %d is not supported for this SOC!\n",
				port);
		return;
	}

	BUG_ON(configured[port]++);

	/* Unfortunately there is a lot of dependencies between
	 * PHYs & controllers... I didn't manage to get everything
	 * working when powering up only selected bits... */
	if (!initialized) {
		/* Power up & initialize PHY(s) */
		if (cpu_data->type == CPU_STX7105) {
			/* 7105 */
			sata_host_reset_sysconf[0] =
				sysconf_claim(SYS_CFG, 32, 11, 11, "SATA");
			sysconf_write(sata_host_reset_sysconf[0], 0);

			sata_phy_reset_sysconf[0] =
				sysconf_claim(SYS_CFG, 32, 9, 9, "SATA");
			BUG_ON(!sata_phy_reset_sysconf[0]);
			sysconf_write(sata_phy_reset_sysconf[0], 0);
		} else {
			/* 7106 */
			/* The second PHY _must not_ be powered while
			 * initializing the first one... */
			/* 7106 */
			sata_host_reset_sysconf[0] =
				sysconf_claim(SYS_CFG, 32, 10, 10, "SATA");
			sata_host_reset_sysconf[1] =
				sysconf_claim(SYS_CFG, 32, 11, 11, "SATA");
			sysconf_write(sata_host_reset_sysconf[0], 0);
			sysconf_write(sata_host_reset_sysconf[1], 0);

			sata_phy_reset_sysconf[0] =
				sysconf_claim(SYS_CFG, 32, 8, 8, "SATA");
			BUG_ON(!sata_phy_reset_sysconf[0]);

			sata_phy_reset_sysconf[1] =
				sysconf_claim(SYS_CFG, 32, 9, 9, "SATA");
			BUG_ON(!sata_phy_reset_sysconf[1]);
		}
		initialized = 1;
	}

	if (cpu_data->type == CPU_STX7105)
		platform_device_register(&stx7105_sata_device);
	else
		platform_device_register(&stx7106_sata_devices[port]);
}
