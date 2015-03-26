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
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/device.h>
#include <linux/stm/fli7510.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config fli7510_ethernet_pad_configs[] = {
	[fli7510_ethernet_mode_mii] = {
		.gpios_num = 19,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(18, 0, 1),	/* MDC */
			STM_PAD_PIO_IN(18, 1, -1),	/* COL */
			STM_PAD_PIO_IN(18, 2, -1),	/* CRS */
			STM_PAD_PIO_IN(18, 3, -1),	/* MDINT */
			STM_PAD_PIO_BIDIR(18, 4, 1),	/* MDIO */
			STM_PAD_PIO_OUT_NAMED(18, 5, 1, "PHYCLK"),
			STM_PAD_PIO_OUT(20, 0, 1),	/* TXD[0] */
			STM_PAD_PIO_OUT(20, 1, 1),	/* TXD[1] */
			STM_PAD_PIO_OUT(20, 2, 1),	/* TXD[2] */
			STM_PAD_PIO_OUT(20, 3, 1),	/* TXD[3] */
			STM_PAD_PIO_OUT(20, 4, 1),	/* TXEN */
			STM_PAD_PIO_IN_NAMED(20, -1, -1, "TXCLK"),
			STM_PAD_PIO_IN(21, 0, -1),	/* RXD[0] */
			STM_PAD_PIO_IN(21, 1, -1),	/* RXD[1] */
			STM_PAD_PIO_IN(21, 2, -1),	/* RXD[2] */
			STM_PAD_PIO_IN(21, 3, -1),	/* RXD[3] */
			STM_PAD_PIO_IN(21, 4, -1),	/* RXDV */
			STM_PAD_PIO_IN(21, 5, -1),	/* RX_ER */
			STM_PAD_PIO_IN(21, 6, -1),	/* RXCLK */
		},
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* gmac_mii_enable */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 8, 8, 1),
			/* gmac_phy_clock_sel
			 * value set in fli7510_configure_ethernet() */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 9, 9, -1),
			/* gmac_enable */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 24, 24, 1),
			/* phy_intf_sel */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 26, 28, 0),
		},
	},
	[fli7510_ethernet_mode_gmii] = { /* Not supported by 7510! */
		.gpios_num = 27,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(18, 0, 1),	/* MDC */
			STM_PAD_PIO_IN(18, 1, -1),	/* COL */
			STM_PAD_PIO_IN(18, 2, -1),	/* CRS */
			STM_PAD_PIO_IN(18, 3, -1),	/* MDINT */
			STM_PAD_PIO_BIDIR(18, 4, 1),	/* MDIO */
			STM_PAD_PIO_OUT(18, 5, 1),	/* PHYCLK */
			STM_PAD_PIO_OUT(20, 0, 1),	/* TXD[0] */
			STM_PAD_PIO_OUT(20, 1, 1),	/* TXD[1] */
			STM_PAD_PIO_OUT(20, 2, 1),	/* TXD[2] */
			STM_PAD_PIO_OUT(20, 3, 1),	/* TXD[3] */
			STM_PAD_PIO_OUT(20, 4, 1),	/* TXEN */
			STM_PAD_PIO_IN(20, 5, -1),	/* TXCLK */
			STM_PAD_PIO_IN(21, 0, -1),	/* RXD[0] */
			STM_PAD_PIO_IN(21, 1, -1),	/* RXD[1] */
			STM_PAD_PIO_IN(21, 2, -1),	/* RXD[2] */
			STM_PAD_PIO_IN(21, 3, -1),	/* RXD[3] */
			STM_PAD_PIO_IN(21, 4, -1),	/* RXDV */
			STM_PAD_PIO_IN(21, 5, -1),	/* RX_ER */
			STM_PAD_PIO_IN(21, 6, -1),	/* RXCLK */
			STM_PAD_PIO_IN(24, 0, -1),	/* RXD[4] */
			STM_PAD_PIO_IN(24, 1, -1),	/* RXD[5] */
			STM_PAD_PIO_IN(24, 2, -1),	/* RXD[6] */
			STM_PAD_PIO_IN(24, 3, -1),	/* RXD[7] */
			STM_PAD_PIO_OUT(24, 4, 1),	/* TXD[4] */
			STM_PAD_PIO_OUT(24, 5, 1),	/* TXD[5] */
			STM_PAD_PIO_OUT(24, 6, 1),	/* TXD[6] */
			STM_PAD_PIO_OUT(24, 7, 1),	/* TXD[7] */
		},
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* gmac_mii_enable */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 8, 8, 1),
			/* gmac_phy_clock_sel */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 9, 9, 1),
			/* gmac_enable */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 24, 24, 1),
			/* phy_intf_sel */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 26, 28, 0),
		},
	},
	[fli7510_ethernet_mode_rmii] = {
		.gpios_num = 11,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(18, 0, 1),	/* MDC */
			STM_PAD_PIO_IN(18, 3, -1),	/* MDINT */
			STM_PAD_PIO_BIDIR(18, 4, 1),	/* MDIO */
			STM_PAD_PIO_STUB_NAMED(18, 5, "PHYCLK"),
			STM_PAD_PIO_OUT(20, 0, 1),	/* TXD[0] */
			STM_PAD_PIO_OUT(20, 1, 1),	/* TXD[1] */
			STM_PAD_PIO_OUT(20, 4, 1),	/* TXEN */
			STM_PAD_PIO_IN(21, 0, -1),	/* RXD[0] */
			STM_PAD_PIO_IN(21, 1, -1),	/* RXD[1] */
			STM_PAD_PIO_IN(21, 4, -1),	/* RXDV */
			STM_PAD_PIO_IN(21, 5, -1),	/* RX_ER */
		},
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* gmac_mii_enable */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 8, 8, 1),
			/* gmac_phy_clock_sel
			 * value set in fli7510_configure_ethernet() */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 9, 9, -1),
			/* gmac_enable */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 24, 24, 1),
			/* phy_intf_sel */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 26, 28, 4),
		},
	},
	[fli7510_ethernet_mode_reverse_mii] = {
		.gpios_num = 19,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(18, 0, 1),	/* MDC */
			STM_PAD_PIO_IN(18, 1, -1),	/* COL */
			STM_PAD_PIO_OUT(18, 2, 1),	/* CRS */
			STM_PAD_PIO_OUT(18, 3, 1),	/* MDINT */
			STM_PAD_PIO_BIDIR(18, 4, 1),	/* MDIO */
			STM_PAD_PIO_OUT_NAMED(18, 5, 1, "PHYCLK"),
			STM_PAD_PIO_OUT(20, 0, 1),	/* TXD[0] */
			STM_PAD_PIO_OUT(20, 1, 1),	/* TXD[1] */
			STM_PAD_PIO_OUT(20, 2, 1),	/* TXD[2] */
			STM_PAD_PIO_OUT(20, 3, 1),	/* TXD[3] */
			STM_PAD_PIO_OUT(20, 4, 1),	/* TXEN */
			STM_PAD_PIO_IN_NAMED(20, -1, -1, "TXCLK"),
			STM_PAD_PIO_IN(21, 0, -1),	/* RXD[0] */
			STM_PAD_PIO_IN(21, 1, -1),	/* RXD[1] */
			STM_PAD_PIO_IN(21, 2, -1),	/* RXD[2] */
			STM_PAD_PIO_IN(21, 3, -1),	/* RXD[3] */
			STM_PAD_PIO_IN(21, 4, -1),	/* RXDV */
			STM_PAD_PIO_IN(21, 5, -1),	/* RX_ER */
			STM_PAD_PIO_IN(21, 6, -1),	/* RXCLK */
		},
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* gmac_mii_enable */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 8, 8, 0),
			/* gmac_phy_clock_sel
			 * value set in fli7510_configure_ethernet() */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 9, 9, -1),
			/* gmac_enable */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 24, 24, 1),
			/* phy_intf_sel */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 26, 28, 0),
		},
	},
};

static void fli7510_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data fli7510_ethernet_platform_data = {
	.pbl = 32,
	.has_gmac = 1,
	.enh_desc = 1,
	.tx_coe = 1,
	.bugged_jumbo =1,
	.fix_mac_speed = fli7510_ethernet_fix_mac_speed,
	.init = &stmmac_claim_resource,
};

static struct platform_device fli7510_ethernet_device = {
	.name = "stmmaceth",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd920000, 0x08000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(40), -1),
	},
	.dev.platform_data = &fli7510_ethernet_platform_data,
};

void __init fli7510_configure_ethernet(struct fli7510_ethernet_config *config)
{
	static int configured;
	struct fli7510_ethernet_config default_config;
	struct stm_pad_config *pad_config;
	const int interfaces[] = {
		[fli7510_ethernet_mode_mii] = PHY_INTERFACE_MODE_MII,
		[fli7510_ethernet_mode_gmii] = PHY_INTERFACE_MODE_GMII,
		[fli7510_ethernet_mode_rmii] = PHY_INTERFACE_MODE_RMII,
		[fli7510_ethernet_mode_reverse_mii] = PHY_INTERFACE_MODE_MII,
	};

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = &fli7510_ethernet_pad_configs[config->mode];

	fli7510_ethernet_platform_data.custom_cfg = (void *) pad_config;
	fli7510_ethernet_platform_data.interface = interfaces[config->mode];
	fli7510_ethernet_platform_data.bus_id = config->phy_bus;
	fli7510_ethernet_platform_data.phy_addr = config->phy_addr;
	fli7510_ethernet_platform_data.mdio_bus_data = config->mdio_bus_data;

	switch (config->mode) {
	case fli7510_ethernet_mode_mii:
	case fli7510_ethernet_mode_reverse_mii:
		if (config->ext_clk) {
			stm_pad_set_pio_ignored(pad_config, "PHYCLK");
			pad_config->sysconfs[1].value = 1;
		} else {
			pad_config->sysconfs[1].value = 0;
		}
		stm_pad_set_pio(pad_config, "TXCLK", 20,
				cpu_data->type == CPU_FLI7510 ? 6 : 5);
		break;
	case fli7510_ethernet_mode_gmii:
		if (cpu_data->type == CPU_FLI7510) {
			BUG(); /* Not supported */
			return;
		}
		break;
	case fli7510_ethernet_mode_rmii:
		if (config->ext_clk) {
			stm_pad_set_pio_in(pad_config, "PHYCLK", -1);
			pad_config->sysconfs[1].value = 1;
		} else {
			stm_pad_set_pio_out(pad_config, "PHYCLK", 1);
			pad_config->sysconfs[1].value = 0;
		}
		break;
	default:
		BUG();
		break;
	}

	fli7510_ethernet_platform_data.bsp_priv =
			sysconf_claim(CFG_COMMS_CONFIG_2, 25, 25,
			"gmac_mac_speed");

	platform_device_register(&fli7510_ethernet_device);
}



/* USB resources ---------------------------------------------------------- */

static u64 fli7510_usb_dma_mask = DMA_BIT_MASK(32);

static int fli7510_usb_xtal_initialized;
static struct sysconf_field *fli7510_usb_xtal_sc;

static int fli7510_usb_xtal_claim(struct stm_pad_state *state, void *priv)
{
	if (!fli7510_usb_xtal_initialized++) {
		fli7510_usb_xtal_sc = sysconf_claim(CFG_SPARE_1, 1, 1,
				"USB_xtal_valid");
		BUG_ON(!fli7510_usb_xtal_sc);
		sysconf_write(fli7510_usb_xtal_sc, 1);
	}

	return 0;
}

static void fli7510_usb_xtal_release(struct stm_pad_state *state, void *priv)
{
	if (!--fli7510_usb_xtal_initialized)
		sysconf_release(fli7510_usb_xtal_sc);
}

static struct stm_pad_config fli7510_usb_pad_configs[] = {
	[fli7510_usb_ovrcur_disabled] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(27, 2, 1),	/* USB_A_PWREN */
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* usba_enable_pad_override */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 12, 12, 1),
			/* usba_ovrcur */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 13, 13, 1),
		},
		.custom_claim = fli7510_usb_xtal_claim,
		.custom_release = fli7510_usb_xtal_release,

	},
	[fli7510_usb_ovrcur_active_high] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(27, 1, -1),	/* USB_A_OVRCUR */
			STM_PAD_PIO_OUT(27, 2, 1),	/* USB_A_PWREN */
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* usba_enable_pad_override */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 12, 12, 0),
			/* usba_ovrcur_polarity */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 11, 11, 0),
		},
		.custom_claim = fli7510_usb_xtal_claim,
		.custom_release = fli7510_usb_xtal_release,
	},
	[fli7510_usb_ovrcur_active_low] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(27, 1, -1),	/* USB_A_OVRCUR */
			STM_PAD_PIO_OUT(27, 2, 1),	/* USB_A_PWREN */
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* usba_enable_pad_override */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 12, 12, 0),
			/* usba_ovrcur_polarity */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 11, 11, 1),
		},
		.custom_claim = fli7510_usb_xtal_claim,
		.custom_release = fli7510_usb_xtal_release,
	}
};

static struct stm_plat_usb_data fli7510_usb_platform_data = {
	.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
		STM_PLAT_USB_FLAGS_STRAP_PLL |
		STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
	.device_config = &(struct stm_device_config){
		/* .pad_config set in fli7510_configure_usb() */
	},
};

static struct platform_device fli7510_usb_device = {
	.name = "stm-usb",
	.id = -1,
	.dev = {
		.dma_mask = &fli7510_usb_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &fli7510_usb_platform_data,
	},
	.num_resources = 6,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfdaffe00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(54), -1),
		STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfdaffc00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(55), -1),
		STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfda00000, 0x100),
		STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfdafff00, 0x100),
	},
};

static struct stm_pad_config *fli7520_usb_pad_configs[] = {
	[0] = (struct stm_pad_config []) {
		[fli7510_usb_ovrcur_disabled] = {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(26, 4, 1), /* USB_A_PWREN */
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* usb1_enable_pad_override */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 12, 12, 1),
				/* usb1_ovrcur */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 13, 13, 1),
				/* conf_usb1_clk_en */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 21, 21, 1),
				/* conf_usb1_rst_n */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 23, 23, 1),
			},
			.custom_claim = fli7510_usb_xtal_claim,
			.custom_release = fli7510_usb_xtal_release,

		},
		[fli7510_usb_ovrcur_active_high] = {
			.gpios_num = 2,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(26, 3, -1), /* USB_A_OVRCUR */
				STM_PAD_PIO_OUT(26, 4, 1), /* USB_A_PWREN */
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* usb1_ovrcur_polarity */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 11, 11, 0),
				/* usb1_enable_pad_override */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 12, 12, 0),
				/* conf_usb1_clk_en */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 21, 21, 1),
				/* conf_usb1_rst_n */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 23, 23, 1),
			},
			.custom_claim = fli7510_usb_xtal_claim,
			.custom_release = fli7510_usb_xtal_release,
		},
		[fli7510_usb_ovrcur_active_low] = {
			.gpios_num = 2,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(26, 3, -1), /* USB_A_OVRCUR */
				STM_PAD_PIO_OUT(26, 4, 1), /* USB_A_PWREN */
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* usb1_ovrcur_polarity */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 11, 11, 1),
				/* usb1_enable_pad_override */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 12, 12, 0),
				/* conf_usb1_clk_en */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 21, 21, 1),
				/* conf_usb1_rst_n */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 23, 23, 1),
			},
			.custom_claim = fli7510_usb_xtal_claim,
			.custom_release = fli7510_usb_xtal_release,
		}
	},
	[1] = (struct stm_pad_config []) {
		[fli7510_usb_ovrcur_disabled] = {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(26, 6, 1), /* USB_C_PWREN */
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* usb2_enable_pad_override */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 15, 15, 1),
				/* usb2_ovrcur */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 16, 16, 1),
				/* conf_usb2_clk_en */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 22, 22, 1),
				/* conf_usb2_rst_n */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 24, 24, 1),
			},
			.custom_claim = fli7510_usb_xtal_claim,
			.custom_release = fli7510_usb_xtal_release,

		},
		[fli7510_usb_ovrcur_active_high] = {
			.gpios_num = 2,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(26, 5, -1), /* USB_C_OVRCUR */
				STM_PAD_PIO_OUT(26, 6, 1), /* USB_C_PWREN */
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* usb2_ovrcur_polarity */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 14, 14, 0),
				/* usb2_enable_pad_override */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 15, 15, 0),
				/* conf_usb2_clk_en */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 22, 22, 1),
				/* conf_usb2_rst_n */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 24, 24, 1),
			},
			.custom_claim = fli7510_usb_xtal_claim,
			.custom_release = fli7510_usb_xtal_release,
		},
		[fli7510_usb_ovrcur_active_low] = {
			.gpios_num = 2,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(26, 5, -1), /* USB_C_OVRCUR */
				STM_PAD_PIO_OUT(26, 6, 1), /* USB_C_PWREN */
			},
			.sysconfs_num = 4,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* usb2_ovrcur_polarity */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 14, 14, 1),
				/* usb2_enable_pad_override */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 15, 15, 0),
				/* conf_usb2_clk_en */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 22, 22, 1),
				/* conf_usb2_rst_n */
				STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 24, 24, 1),
			},
			.custom_claim = fli7510_usb_xtal_claim,
			.custom_release = fli7510_usb_xtal_release,
		}
	},
};

static struct stm_plat_usb_data fli7520_usb_platform_data[] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
			STM_PLAT_USB_FLAGS_STRAP_PLL |
			STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		.device_config = &(struct stm_device_config){
			/* .pad_config set in fli7510_configure_usb() */
		},
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
			STM_PLAT_USB_FLAGS_STRAP_PLL |
			STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		.device_config = &(struct stm_device_config){
			/* .pad_config set in fli7510_configure_usb() */
		},
	},
};

static struct platform_device fli7520_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &fli7510_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &fli7520_usb_platform_data[0],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfdaffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(54), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfdaffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(55), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfda00000,
					0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfdafff00,
					0x100),
		},
	},
	[1] = {
		.name = "stm-usb",
		.id = 1,
		.dev = {
			.dma_mask = &fli7510_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &fli7520_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfdcffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(56), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfdcffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(57), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfdc00000,
					0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfdcfff00,
					0x100),
		},
	}
};

void __init fli7510_configure_usb(int port, struct fli7510_usb_config *config)
{
	static int configured[ARRAY_SIZE(fli7520_usb_devices)];
	struct fli7510_usb_config default_config;

	BUG_ON(port < 0 || port > ARRAY_SIZE(fli7520_usb_devices));

	if (port != 0 && cpu_data->type == CPU_FLI7510) {
		BUG();
		return;
	}

	BUG_ON(configured[port]++);

	if (!config)
		config = &default_config;

	if (cpu_data->type == CPU_FLI7510) {
		fli7510_usb_platform_data.device_config->pad_config =
				&fli7510_usb_pad_configs[config->ovrcur_mode];

		platform_device_register(&fli7510_usb_device);
	} else {
		fli7520_usb_platform_data[port].device_config->pad_config =
				&fli7520_usb_pad_configs[port]
				[config->ovrcur_mode];

		platform_device_register(&fli7520_usb_devices[port]);
	}
}

/* Cut 0 has a problem with accessing the MiPhy, so we have to use the dummy
 * driver as the PCIe driver expects a phy
 */
static struct stm_plat_miphy_dummy_data fli7540_miphy_dummy_platform_data = {
	.miphy_first = 0,
	.miphy_count = 1,
	.miphy_modes = (enum miphy_mode[1]) {PCIE_MODE},
};

static struct platform_device fli7540_miphy_dummy_device = {
	.name	= "stm-miphy-dummy",
	.id	= -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &fli7540_miphy_dummy_platform_data,
	}
};

static void fli7540_pcie_mp_select(int port)
{
	/* Freeman only has one port */
}

struct stm_plat_pcie_mp_data fli7540_pcie_mp_platform_data = {
	.miphy_first = 0,
	.miphy_count = 1,
	.miphy_modes = (enum miphy_mode[1]) {PCIE_MODE},
	.mp_select = fli7540_pcie_mp_select,
};

static struct platform_device fli7540_pcie_mp_device = {
	.name	= "pcie-mp",
	.id	= -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe104000, 0xff)
	},
	.dev = {
		.platform_data = &fli7540_pcie_mp_platform_data,
	}
};

static int __init fli7540_miphy_postcore_setup(void)
{
	if (cpu_data->cut_major >= 1)
		return platform_device_register(&fli7540_pcie_mp_device);

	return platform_device_register(&fli7540_miphy_dummy_device);
}

postcore_initcall(fli7540_miphy_postcore_setup);
