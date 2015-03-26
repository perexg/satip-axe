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
#include <linux/stm/device.h>
#include <linux/stm/stx7141.h>
#include <linux/delay.h>
#include <asm/irq-ilc.h>

/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config *stx7141_ethernet_pad_configs[] = {
	[0] = (struct stm_pad_config []) {
		[stx7141_ethernet_mode_mii] = {
			.gpios_num = 20,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(8, 0, -1),	/* TXCLK */
				STM_PAD_PIO_OUT(8, 1, 1),	/* TXEN */
				STM_PAD_PIO_OUT(8, 2, 1),	/* TXERR */
				STM_PAD_PIO_OUT(8, 3, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(8, 4, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(8, 5, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(8, 6, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(9, 3, -1),	/* RXCLK */
				STM_PAD_PIO_IN(9, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(9, 5, -1),	/* RXERR */
				STM_PAD_PIO_IN(9, 6, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(9, 7, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(10, 0, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(10, 1, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(10, 6, -1),	/* CRS */
				STM_PAD_PIO_IN(10, 7, -1),	/* COL */
				STM_PAD_PIO_OUT(11, 0, 1),	/* MDC */
				STM_PAD_PIO_BIDIR(11, 1, 1),	/* MDIO */
				STM_PAD_PIO_IN(11, 2, -1),	/* MDINT */
				STM_PAD_PIO_OUT_NAMED(11, 3, 1, "PHYCLK"),
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_GMII0_CLOCK_OUT:
				 * 0 = PIO11.3 is controlled by PIO muxing,
				 * 1 = PIO11.3 is delayed version of PIO8.0
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 13, 13, 1),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* PHY_INTF_SEL0: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 24, 26, 0),
				/* ENMII0: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
			},
		},
		[stx7141_ethernet_mode_gmii] = {
			.gpios_num = 28,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(8, 0, -1),	/* TXCLK */
				STM_PAD_PIO_OUT(8, 1, 1),	/* TXEN */
				STM_PAD_PIO_OUT(8, 2, 1),	/* TXERR */
				STM_PAD_PIO_OUT(8, 3, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(8, 4, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(8, 5, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(8, 6, 1),	/* TXD.3 */
				STM_PAD_PIO_OUT(8, 7, 1),	/* TXD.4 */
				STM_PAD_PIO_OUT(9, 0, 1),	/* TXD.5 */
				STM_PAD_PIO_OUT(9, 1, 1),	/* TXD.6 */
				STM_PAD_PIO_OUT(9, 2, 1),	/* TXD.7 */
				STM_PAD_PIO_IN(9, 3, -1),	/* RXCLK */
				STM_PAD_PIO_IN(9, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(9, 5, -1),	/* RXERR */
				STM_PAD_PIO_IN(9, 6, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(9, 7, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(10, 0, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(10, 1, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(10, 2, -1),	/* RXD.4 */
				STM_PAD_PIO_IN(10, 3, -1),	/* RXD.5 */
				STM_PAD_PIO_IN(10, 4, -1),	/* RXD.6 */
				STM_PAD_PIO_IN(10, 5, -1),	/* RXD.7 */
				STM_PAD_PIO_IN(10, 6, -1),	/* CRS */
				STM_PAD_PIO_IN(10, 7, -1),	/* COL */
				STM_PAD_PIO_OUT(11, 0, 1),	/* MDC */
				STM_PAD_PIO_BIDIR(11, 1, 1),	/* MDIO */
				STM_PAD_PIO_IN(11, 2, -1),	/* MDINT */
				STM_PAD_PIO_OUT_NAMED(11, 3, 1, "PHYCLK"),
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_GMII0_CLOCK_OUT:
				 * 0 = PIO11.3 is controlled by PIO muxing,
				 * 1 = PIO11.3 is delayed version of PIO8.0
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 13, 13, 1),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* PHY_INTF_SEL0: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 24, 26, 0),
				/* ENMII0: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
			},
		},
		[stx7141_ethernet_mode_rgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_sgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_rmii] = {
			.gpios_num = 12,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(8, 1, 1),	/* TXEN */
				STM_PAD_PIO_OUT(8, 2, 1),	/* TXERR */
				STM_PAD_PIO_OUT(8, 3, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(8, 4, 1),	/* TXD.1 */
				STM_PAD_PIO_IN(9, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(9, 5, -1),	/* RXERR */
				STM_PAD_PIO_IN(9, 6, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(9, 7, -1),	/* RXD.1 */
				STM_PAD_PIO_OUT(11, 0, 1),	/* MDC */
				STM_PAD_PIO_BIDIR(11, 1, 1),	/* MDIO */
				STM_PAD_PIO_IN(11, 2, -1),	/* MDINT */
				STM_PAD_PIO_OUT_NAMED(11, 3, 1, "PHYCLK"),
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_GMII0_CLOCK_OUT:
				 * 0 = PIO11.3 is controlled by PIO muxing,
				 * 1 = PIO11.3 is delayed version of PIO8.0
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 13, 13, 1),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* PHY_INTF_SEL0: 4 = RMII */
				STM_PAD_SYS_CFG(7, 24, 26, 4),
				/* ENMII0: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
			},
		},
		[stx7141_ethernet_mode_reverse_mii] = {
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_GMII0_CLOCK_OUT:
				 * 0 = PIO11.3 is controlled by PIO muxing,
				 * 1 = PIO11.3 is delayed version of PIO8.0
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 13, 13, 1),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* PHY_INTF_SEL0: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 24, 26, 0),
				/* ENMII0: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 0),
			},
			.gpios_num = 20,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(8, 0, -1),	/* TXCLK */
				STM_PAD_PIO_OUT(8, 1, 1),	/* TXEN */
				STM_PAD_PIO_OUT(8, 2, 1),	/* TXERR */
				STM_PAD_PIO_OUT(8, 3, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(8, 4, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(8, 5, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(8, 6, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(9, 3, -1),	/* RXCLK */
				STM_PAD_PIO_IN(9, 4, -1),	/* RXDV */
				STM_PAD_PIO_IN(9, 5, -1),	/* RXERR */
				STM_PAD_PIO_IN(9, 6, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(9, 7, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(10, 0, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(10, 1, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(10, 6, -1),	/* CRS */
				STM_PAD_PIO_IN(10, 7, -1),	/* COL */
				STM_PAD_PIO_OUT(11, 0, 1),	/* MDC */
				STM_PAD_PIO_BIDIR(11, 1, 1),	/* MDIO */
				STM_PAD_PIO_IN(11, 2, -1),	/* MDINT */
				STM_PAD_PIO_OUT_NAMED(11, 3, 1, "PHYCLK"),
			},
		},
	},
	[1] = (struct stm_pad_config []) {
		[stx7141_ethernet_mode_mii] = {
			.gpios_num = 20,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(11, 4, -1),	/* TXCLK */
				STM_PAD_PIO_OUT(11, 5, 1),	/* TXEN */
				STM_PAD_PIO_OUT(11, 6, 1),	/* TXERR */
				STM_PAD_PIO_OUT(11, 7, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(12, 0, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(12, 1, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(12, 2, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(12, 7, -1),	/* RXCLK */
				STM_PAD_PIO_IN(13, 0, -1),	/* RXDV */
				STM_PAD_PIO_IN(13, 1, -1),	/* RXERR */
				STM_PAD_PIO_IN(13, 2, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(13, 3, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(13, 4, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(13, 5, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(14, 2, -1),	/* CRS */
				STM_PAD_PIO_IN(14, 3, -1),	/* COL */
				STM_PAD_PIO_OUT(14, 4, 1),	/* MDC */
				STM_PAD_PIO_BIDIR(14, 5, 1),	/* MDIO */
				STM_PAD_PIO_IN(14, 6, -1),	/* MDINT */
				STM_PAD_PIO_OUT_NAMED(14, 7, 1, "PHYCLK"),
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_GMII1_CLOCK_OUT:
				 * 0 = PIO14.7 is controlled by PIO muxing,
				 * 1 = PIO14.7 is delayed version of PIO11.4
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 15, 15, 1),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(7, 17, 17, 1),
				/* PHY_INTF_SEL1: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 28, 30, 0),
				/* ENMII1: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 31, 31, 1),
			},
		},
		[stx7141_ethernet_mode_gmii] = {
			.gpios_num = 28,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(11, 4, -1),	/* TXCLK */
				STM_PAD_PIO_OUT(11, 5, 1),	/* TXEN */
				STM_PAD_PIO_OUT(11, 6, 1),	/* TXERR */
				STM_PAD_PIO_OUT(11, 7, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(12, 0, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(12, 1, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(12, 2, 1),	/* TXD.3 */
				STM_PAD_PIO_OUT(12, 3, 1),	/* TXD.4 */
				STM_PAD_PIO_OUT(12, 4, 1),	/* TXD.5 */
				STM_PAD_PIO_OUT(12, 5, 1),	/* TXD.6 */
				STM_PAD_PIO_OUT(12, 6, 1),	/* TXD.7 */
				STM_PAD_PIO_IN(12, 7, -1),	/* RXCLK */
				STM_PAD_PIO_IN(13, 0, -1),	/* RXDV */
				STM_PAD_PIO_IN(13, 1, -1),	/* RXERR */
				STM_PAD_PIO_IN(13, 2, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(13, 3, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(13, 4, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(13, 5, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(13, 6, -1),	/* RXD.4 */
				STM_PAD_PIO_IN(13, 7, -1),	/* RXD.5 */
				STM_PAD_PIO_IN(14, 0, -1),	/* RXD.6 */
				STM_PAD_PIO_IN(14, 1, -1),	/* RXD.7 */
				STM_PAD_PIO_IN(14, 2, -1),	/* CRS */
				STM_PAD_PIO_IN(14, 3, -1),	/* COL */
				STM_PAD_PIO_OUT(14, 4, 1),	/* MDC */
				STM_PAD_PIO_BIDIR(14, 5, 1),	/* MDIO */
				STM_PAD_PIO_IN(14, 6, -1),	/* MDINT */
				STM_PAD_PIO_OUT_NAMED(14, 7, 1, "PHYCLK"),
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_GMII1_CLOCK_OUT:
				 * 0 = PIO14.7 is controlled by PIO muxing,
				 * 1 = PIO14.7 is delayed version of PIO11.4
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 15, 15, 1),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(7, 17, 17, 1),
				/* PHY_INTF_SEL1: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 28, 30, 0),
				/* ENMII1: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 31, 31, 1),
			},
		},
		[stx7141_ethernet_mode_rgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_sgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_rmii] = {
			.gpios_num = 12,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(11, 5, 1),	/* TXEN */
				STM_PAD_PIO_OUT(11, 6, 1),	/* TXERR */
				STM_PAD_PIO_OUT(11, 7, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(12, 0, 1),	/* TXD.1 */
				STM_PAD_PIO_IN(13, 0, -1),	/* RXDV */
				STM_PAD_PIO_IN(13, 1, -1),	/* RXERR */
				STM_PAD_PIO_IN(13, 2, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(13, 3, -1),	/* RXD.1 */
				STM_PAD_PIO_OUT(14, 4, 1),	/* MDC */
				STM_PAD_PIO_BIDIR(14, 5, 1),	/* MDIO */
				STM_PAD_PIO_IN(14, 6, -1),	/* MDINT */
				STM_PAD_PIO_OUT_NAMED(14, 7, 1, "PHYCLK"),
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_GMII1_CLOCK_OUT:
				 * 0 = PIO14.7 is controlled by PIO muxing,
				 * 1 = PIO14.7 is delayed version of PIO11.4
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 15, 15, 1),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(7, 17, 17, 1),
				/* PHY_INTF_SEL1: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 28, 30, 0),
				/* ENMII1: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 31, 31, 1),
			},
		},
		[stx7141_ethernet_mode_reverse_mii] = {
			.gpios_num = 20,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(11, 4, -1),	/* TXCLK */
				STM_PAD_PIO_OUT(11, 5, 1),	/* TXEN */
				STM_PAD_PIO_OUT(11, 6, 1),	/* TXERR */
				STM_PAD_PIO_OUT(11, 7, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT(12, 0, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT(12, 1, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT(12, 2, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(12, 7, -1),	/* RXCLK */
				STM_PAD_PIO_IN(13, 0, -1),	/* RXDV */
				STM_PAD_PIO_IN(13, 1, -1),	/* RXERR */
				STM_PAD_PIO_IN(13, 2, -1),	/* RXD.0 */
				STM_PAD_PIO_IN(13, 3, -1),	/* RXD.1 */
				STM_PAD_PIO_IN(13, 4, -1),	/* RXD.2 */
				STM_PAD_PIO_IN(13, 5, -1),	/* RXD.3 */
				STM_PAD_PIO_IN(14, 2, -1),	/* CRS */
				STM_PAD_PIO_IN(14, 3, -1),	/* COL */
				STM_PAD_PIO_OUT(14, 4, 1),	/* MDC */
				STM_PAD_PIO_BIDIR(14, 5, 1),	/* MDIO */
				STM_PAD_PIO_IN(14, 6, -1),	/* MDINT */
				STM_PAD_PIO_OUT_NAMED(14, 7, 1, "PHYCLK"),
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_GMII1_CLOCK_OUT:
				 * 0 = PIO14.7 is controlled by PIO muxing,
				 * 1 = PIO14.7 is delayed version of PIO11.4
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 15, 15, 1),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(7, 17, 17, 1),
				/* PHY_INTF_SEL1: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 28, 30, 0),
				/* ENMII1: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 31, 31, 0),
			},
		},
	},
};

static void stx7141_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

/*
 * Cut 2 of 7141 has AHB wrapper bug for ethernet gmac.
 * Need to disable read-ahead - performance impact
 */
#define GMAC_AHB_CONFIG		0x7000
#define GMAC_AHB_CONFIG_READ_AHEAD_MASK	0xFFCFFFFF
static void stx7141_ethernet_bus_setup(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + GMAC_AHB_CONFIG);
	value &= GMAC_AHB_CONFIG_READ_AHEAD_MASK;
	writel(value, ioaddr + GMAC_AHB_CONFIG);
}

static struct plat_stmmacenet_data stx7141_ethernet_platform_data[] = {
	[0] = {
		.pbl = 32,
		.has_gmac = 1,
		.enh_desc = 1,
		.tx_coe = 1,
		.bugged_jumbo =1,
		.pmt = 1,
		.fix_mac_speed = stx7141_ethernet_fix_mac_speed,
		.init = &stmmac_claim_resource,
	},
	[1] = {
		.pbl = 32,
		.has_gmac = 1,
		.enh_desc = 1,
		.tx_coe = 1,
		.bugged_jumbo =1,
		.pmt = 1,
		.fix_mac_speed = stx7141_ethernet_fix_mac_speed,
		.init = &stmmac_claim_resource,
	},
};

static struct platform_device stx7141_ethernet_devices[] = {
	[0] = {
		.name = "stmmaceth",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd110000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(40), -1),
		},
		.dev.platform_data = &stx7141_ethernet_platform_data[0],
	},
	[1] = {
		.name = "stmmaceth",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd118000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(47), -1),
		},
		.dev.platform_data = &stx7141_ethernet_platform_data[1],
	},
};

void __init stx7141_configure_ethernet(int port,
		struct stx7141_ethernet_config *config)
{
	static int configured[ARRAY_SIZE(stx7141_ethernet_devices)];
	struct stx7141_ethernet_config default_config;
	struct stm_pad_config *pad_config;
	const int interfaces[] = {
		[stx7141_ethernet_mode_mii] = PHY_INTERFACE_MODE_MII,
		[stx7141_ethernet_mode_gmii] = PHY_INTERFACE_MODE_GMII,
		[stx7141_ethernet_mode_rgmii] = PHY_INTERFACE_MODE_RGMII,
		[stx7141_ethernet_mode_sgmii] = PHY_INTERFACE_MODE_SGMII,
		[stx7141_ethernet_mode_rmii] = PHY_INTERFACE_MODE_RMII,
		[stx7141_ethernet_mode_reverse_mii] = PHY_INTERFACE_MODE_MII,
	};

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7141_ethernet_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	if (!config)
		config = &default_config;

	/* TODO: RGMII and SGMII configurations */
	BUG_ON(config->mode == stx7141_ethernet_mode_rgmii);
	BUG_ON(config->mode == stx7141_ethernet_mode_sgmii);

	pad_config = &stx7141_ethernet_pad_configs[port][config->mode];

	/* TODO: ext_clk configuration */

	stx7141_ethernet_platform_data[port].custom_cfg = (void *) pad_config;
	stx7141_ethernet_platform_data[port].interface =
		interfaces[config->mode];
	stx7141_ethernet_platform_data[port].bus_id = config->phy_bus;
	stx7141_ethernet_platform_data[port].phy_addr = config->phy_addr;
	stx7141_ethernet_platform_data[port].mdio_bus_data =
		config->mdio_bus_data;

	/* mac_speed */
	stx7141_ethernet_platform_data[port].bsp_priv = sysconf_claim(SYS_CFG,
			7, 20 + port, 20 + port, "stmmac");

	if (cpu_data->cut_major == 2)
		stx7141_ethernet_platform_data[port].bus_setup =
					stx7141_ethernet_bus_setup;

	platform_device_register(&stx7141_ethernet_devices[port]);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7141_usb_dma_mask = DMA_BIT_MASK(32);

static int stx7141_usb_enable(struct stm_pad_state *state, void *priv);
static void stx7141_usb_disable(struct stm_pad_state *state, void *priv);

#define USB_PWR "USB_PWR"
#define USB_ACK "USB_ACK"
#define USB_ENABLE "USB_ENABLE"

static void stx7141_usb_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;
	int enable_value = (power == stm_device_power_on) ? 1 : 0;

	if (stm_device_get_config(device_state)->sysconfs_num > 2)
		stm_device_sysconf_write(device_state, USB_ENABLE,
					 enable_value);

	stm_device_sysconf_write(device_state, USB_PWR, value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, USB_ACK)
				 == value)
			break;
		mdelay(10);
	}
}

static struct stm_plat_usb_data stx7141_usb_platform_data[] = {
	[0] = { /* USB 2.0 port */
		.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
		.device_config = &(struct stm_device_config){
			/* pad_config done in stx7141_configure_usb() */
			.sysconfs_num = 3,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG(32, 7, 7, USB_PWR),
				STM_DEVICE_SYS_STA(15, 7, 7, USB_ACK),
				STM_DEVICE_SYS_CFG(4, 1, 1, USB_ENABLE),
			},
			.power = stx7141_usb_power,
		},
	},
	[1] = { /* USB 2.0 port */
		.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
		.device_config = &(struct stm_device_config){
			/* pad_config done in stx7141_configure_usb() */
			.sysconfs_num = 3,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG(32, 8, 8, USB_PWR),
				STM_DEVICE_SYS_STA(15, 8, 8, USB_ACK),
				STM_DEVICE_SYS_CFG(4, 14, 14, USB_ENABLE),
			},
			.power = stx7141_usb_power,
		},
	},
	[2] = { /* USB 1.1 port */
		.flags = STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.device_config = &(struct stm_device_config){
			/* pad_config done in stx7141_configure_usb() */
			.sysconfs_num = 3,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG(32, 9, 9, USB_PWR),
				STM_DEVICE_SYS_STA(15, 9, 9, USB_ACK),
				STM_DEVICE_SYS_CFG(4, 8, 8, USB_ENABLE),
			},
			.power = stx7141_usb_power,
		},
	},
	[3] = { /* USB 1.1 port */
		.flags = STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.device_config = &(struct stm_device_config){
			/* pad_config done in stx7141_configure_usb() */
			.sysconfs_num = 3,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG(32, 10, 10, USB_PWR),
				STM_DEVICE_SYS_STA(15, 10, 10, USB_ACK),
				STM_DEVICE_SYS_CFG(4, 13, 13, USB_ENABLE),
			},
			.power = stx7141_usb_power,
		},
	},
};

static struct platform_device stx7141_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7141_usb_platform_data[0],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfe1ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(93), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfe1ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(94), -1),
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
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7141_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfeaffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(95), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfeaffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(96), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfea00000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfeafff00,
						    0x100),
		},
	},
	[2] = {
		.name = "stm-usb",
		.id = 2,
		.dev = {
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7141_usb_platform_data[2],
		},
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfebffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(97), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfeb00000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfebfff00,
						    0x100),
		},
	},
	[3] = {
		.name = "stm-usb",
		.id = 3,
		.dev = {
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7141_usb_platform_data[3],
		},
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfecffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(98), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfec00000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfecfff00,
						    0x100),
		},
	},
};

static DEFINE_SPINLOCK(stx7141_usb_spin_lock);
static struct sysconf_field *stx7141_usb_phy_clock_sc;
static struct sysconf_field *stx7141_usb_clock_sc;
static struct sysconf_field *stx7141_usb_pll_sc;

enum stx7141_usb_ports {
	stx7141_usb_2_ports = 0,
	stx7141_usb_11_ports = 1,
};
static int stx7141_usb_enabled_ports[2];
static struct sysconf_field *stx7141_usb_oc_enable_sc[2];
static struct sysconf_field *stx7141_usb_oc_invert_sc[2];

static struct sysconf_field *stx7141_usb_enable_sc[4];
static struct sysconf_field *stx7141_usb_pwr_req_sc[4];
static struct sysconf_field *stx7141_usb_pwr_ack_sc[4];

/* stx7141_configure_usb() must ensure the pairs of ports are the same */
static enum stx7141_usb_overcur_mode stx7141_usb_overcur_mode[4];

static int stx7141_usb_enable(struct stm_pad_state *state, void *priv)
{
	int port = (int)priv;
	enum stx7141_usb_ports port_group = port >> 1;

	spin_lock(&stx7141_usb_spin_lock);

/*
 *	The USB clock management is moved in the clock-stx7141.c file
 *	like any other clock
 */
	/* State shared by two ports */
	if ((stx7141_usb_enabled_ports[port_group] == 0) &&
	    (cpu_data->cut_major >= 2)) {
		const struct {
			unsigned char enable, invert;
		} oc_mode[2][3] = {
			{
				/* USB 2 ports, internally active low oc */
				[stx7141_usb_ovrcur_disabled] = { 0, 0 },
				[stx7141_usb_ovrcur_active_high] = { 1, 1 },
				[stx7141_usb_ovrcur_active_low] = { 1, 0 },
			}, {
				/* USB 1.1 ports, internally active high oc */
				[stx7141_usb_ovrcur_disabled] = { 0, 0 },
				[stx7141_usb_ovrcur_active_high] = { 1, 0 },
				[stx7141_usb_ovrcur_active_low] = { 1, 1 },
			}
		};
		enum stx7141_usb_overcur_mode mode =
			stx7141_usb_overcur_mode[port];

		stx7141_usb_oc_enable_sc[port_group] =
			sysconf_claim(SYS_CFG, 4, 12 - port_group,
				      12 - port_group, "USB");
		stx7141_usb_oc_invert_sc[port_group] =
			sysconf_claim(SYS_CFG, 4, 7 - port_group,
				      7 - port_group, "USB");
		sysconf_write(stx7141_usb_oc_enable_sc[port_group],
			      oc_mode[port_group][mode].enable);
		sysconf_write(stx7141_usb_oc_invert_sc[port_group],
			      oc_mode[port_group][mode].invert);
	}

	stx7141_usb_enabled_ports[port_group]++;

	spin_unlock(&stx7141_usb_spin_lock);

	return 0;
}

static void stx7141_usb_disable(struct stm_pad_state *state, void *priv)
{
	int port = (int)priv;
	enum stx7141_usb_ports port_group = (port < 2) ?
			stx7141_usb_2_ports : stx7141_usb_11_ports;

	spin_lock(&stx7141_usb_spin_lock);

	/* Power down USB */
	sysconf_write(stx7141_usb_pwr_req_sc[port], 1);
	sysconf_release(stx7141_usb_pwr_req_sc[port]);
	sysconf_release(stx7141_usb_pwr_ack_sc[port]);

	/* Put USB into reset */
	if (cpu_data->cut_major >= 2) {
		sysconf_write(stx7141_usb_enable_sc[port], 0);
		sysconf_release(stx7141_usb_enable_sc[port]);
	}

	/* State shared by two ports */
	if ((stx7141_usb_enabled_ports[port_group] == 1) &&
			(cpu_data->cut_major >= 2)) {
		sysconf_release(stx7141_usb_oc_invert_sc[port_group]);
		sysconf_release(stx7141_usb_oc_enable_sc[port_group]);
	}

	/* USB 1.1 clock source */
	if ((port_group == stx7141_usb_11_ports) &&
	    (stx7141_usb_enabled_ports[stx7141_usb_11_ports] == 1))
		sysconf_release(stx7141_usb_clock_sc);

	/* State shared by all four ports */
	if ((stx7141_usb_enabled_ports[stx7141_usb_2_ports] +
	     stx7141_usb_enabled_ports[stx7141_usb_11_ports]) == 0) {
		sysconf_release(stx7141_usb_phy_clock_sc);
		if (cpu_data->cut_major >= 2)
			sysconf_release(stx7141_usb_pll_sc);
	}

	spin_unlock(&stx7141_usb_spin_lock);
}

void __init stx7141_configure_usb(int port, struct stx7141_usb_config *config)
{
	static int configured[ARRAY_SIZE(stx7141_usb_devices)];
	struct stm_pad_config *pad_config;
	struct sysconf_field *sc;

	BUG_ON(port < 0 || port > ARRAY_SIZE(stx7141_usb_devices));

	BUG_ON(configured[port]);

	/* USB edge rise and DC shift - STLinux Bugzilla 10991 */
	sc = sysconf_claim(SYS_CFG, 7, 0+(port<<1), 1+(port<<1), "USB");
	sysconf_write(sc, 2);

	/* USB over current configuration is complicated.
	 * Cut 1 had hardwired configuration: USB 2 ports active low,
	 * USB 1.1 ports active high.
	 * Cut 2 introduced sysconf registers to control this, but the
	 * bits are shared so both USB 2 ports and both USB 1.1 ports must
	 * agree on the configuration.
	 * See GNBvd63856 and GNBvd65685 for details. */
	if (cpu_data->cut_major == 1) {
		enum stx7141_usb_overcur_mode avail_mode;

		avail_mode = (port < 2) ? stx7141_usb_ovrcur_active_low :
				stx7141_usb_ovrcur_active_high;

		if (config->ovrcur_mode != avail_mode)
			goto fail;
	} else {
		int shared_port = port ^ 1;

		if (configured[shared_port] &&
				(stx7141_usb_overcur_mode[shared_port] !=
				config->ovrcur_mode))
			goto fail;
	}

	if (cpu_data->cut_major < 2)
		stx7141_usb_platform_data[port].device_config->sysconfs_num--;

	configured[port] = 1;
	stx7141_usb_overcur_mode[port] = config->ovrcur_mode;

	pad_config = stm_pad_config_alloc(2, 0);
	stx7141_usb_platform_data[port].device_config->pad_config = pad_config;

	if (config->ovrcur_mode != stx7141_usb_ovrcur_disabled) {
		static const struct {
			unsigned char port, pin;
		} oc_pins[4] = {
			{ 4, 6 },
			{ 5, 0 },
			{ 4, 2 },
			{ 4, 4 },
		};

		stm_pad_config_add_pio_in(pad_config, oc_pins[port].port,
				oc_pins[port].pin, -1);
	}

	if (config->pwr_enabled) {
		static const struct {
			unsigned char port, pin, function;
		} pwr_pins[4] = {
			{ 4, 7, 1 },
			{ 5, 1, 1 },
			{ 4, 3, 1 },
			{ 4, 5, 1 },
		};

		stm_pad_config_add_pio_out(pad_config, pwr_pins[port].port,
				pwr_pins[port].pin, pwr_pins[port].function);
	}

	pad_config->custom_claim = stx7141_usb_enable;
	pad_config->custom_release = stx7141_usb_disable,
	pad_config->custom_priv = (void *)port;

	platform_device_register(&stx7141_usb_devices[port]);

	return;

fail:
	printk(KERN_WARNING "Disabling USB port %d, "
			"as requested over current mode not available", port);
	return;
}



/* MiPHY resources -------------------------------------------------------- */

static struct stm_tap_sysconf tap_sysconf = {
	.tck = { SYS_CFG, 33, 0, 0},
	.tms = { SYS_CFG, 33, 5, 5},
	.tdi = { SYS_CFG, 33, 1, 1},
	.tdo = { SYS_STA, 0, 1, 1},
	.tap_en = { SYS_CFG, 33, 6, 6, POL_INVERTED},
	.trstn = { SYS_CFG, 33, 4, 4},
};

struct stm_plat_tap_data stx7141_tap_platform_data = {
	.miphy_first = 0,
	.miphy_count = 1,
	.miphy_modes = (enum miphy_mode[1]) {SATA_MODE},
	.tap_sysconf = &tap_sysconf,
};

static struct platform_device stx7141_tap_device = {
	.name	= "stm-miphy-tap",
	.id	= 0,
	.num_resources = 0,
	.dev = {
		.platform_data = &stx7141_tap_platform_data,
	}
};
static int __init stx7141_miphy_postcore_setup(void)
{
	return platform_device_register(&stx7141_tap_device);
}
postcore_initcall(stx7141_miphy_postcore_setup);



/* SATA resources --------------------------------------------------------- */

static void stx7141_sata_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	/*
	 * At the moment it isn't clear how turn-off the sata host controller
	 * The SysCfg_36[6] doesn't work as described in ADCS 8247910
	 * The SysSta_15[5] returns always zero
	 * Also some experiment on SysCfg_4[9] did bad result.
	 *
	 * So currently the system totally bypasses the sata host controller
	 */
	return ;
}

static struct platform_device stx7141_sata_device = {
	.name = "sata-stm",
	.id = -1,
	.dev.platform_data = &(struct stm_plat_sata_data) {
		.phy_init = 0,
		.pc_glue_logic_init = 0,
		.only_32bit = 0,
		.host_restart = NULL,
		.port_num = 0,
		.miphy_num = 0,
		.device_config = &(struct stm_device_config) {
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYS_CFG(32, 6, 6, "SATA_PWR"),
				STM_DEVICE_SYS_STA(15, 5, 5, "SATA_ACK"),
			},
			.power = stx7141_sata_power,
		}
	},
	.num_resources = 3,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe209000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("hostc", ILC_IRQ(89), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("dmac", ILC_IRQ(88), -1),
	},
};

void __init stx7141_configure_sata(void)
{
	static int configured;
	struct sysconf_field *sc;

	BUG_ON(configured++);

	if (cpu_data->cut_major < 2) {
		pr_warning("SATA is only supported on cut 2 or later\n");
		return;
	}
	/* SATA_ENABLE */
	sc = sysconf_claim(SYS_CFG, 4, 9, 9, "SATA");
	BUG_ON(!sc);
	sysconf_write(sc, 1);

	/* SATA_SLUMBER_POWER_MODE */
	sc = sysconf_claim(SYS_CFG, 32, 6, 6, "SATA");
	BUG_ON(!sc);
	sysconf_write(sc, 1);
	sysconf_release(sc);	/* make available for stm_device */

	platform_device_register(&stx7141_sata_device);
}
