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
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/phy.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/device.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7111.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config stx7111_ethernet_pad_configs[] = {
	[stx7111_ethernet_mode_mii] = {
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* ETHERNET_INTERFACE_ON: 0 = off, 1 = on */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* ETHERNET_PHY_CLK_EXT:
			 * 0 = MII_PHYCLK is an output,
			 * 1 = MII_PHYCLK is an input
			 * set by stx7111_configure_ethernet() */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* ETHERNET_PHY_INTF_SEL: 0 = MII, 4 = RMII */
			STM_PAD_SYS_CFG(7, 24, 26, 0),
			/* ENMII: 0 = reverse MII, 1 = MII */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
		},
	},
	[stx7111_ethernet_mode_rmii] = {
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* ETHERNET_INTERFACE_ON: 0 = off, 1 = on */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* ETHERNET_PHY_CLK_EXT:
			 * 0 = MII_PHYCLK is an output,
			 * 1 = MII_PHYCLK is an input
			 * set by stx7111_configure_ethernet() */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* ETHERNET_PHY_INTF_SEL: 0 = MII, 4 = RMII */
			STM_PAD_SYS_CFG(7, 24, 26, 4),
			/* ENMII: 0 = reverse MII, 1 = MII */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
		},
	},
	[stx7111_ethernet_mode_reverse_mii] = {
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* ETHERNET_INTERFACE_ON: 0 = off, 1 = on */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* ETHERNET_PHY_CLK_EXT:
			 * 0 = MII_PHYCLK is an output,
			 * 1 = MII_PHYCLK is an input
			 * set by stx7111_configure_ethernet() */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* ETHERNET_PHY_INTF_SEL: 0 = MII, 4 = RMII */
			STM_PAD_SYS_CFG(7, 24, 26, 0),
			/* ENMII: 0 = reverse MII, 1 = MII */
			STM_PAD_SYS_CFG(7, 27, 27, 0),
		},
	},
};

static void stx7111_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx7111_ethernet_platform_data = {
	.pbl = 32,
	.has_gmac = 1,
	.enh_desc = 1,
	.tx_coe = 1,
	.bugged_jumbo =1,
	.pmt = 1,
	.fix_mac_speed = stx7111_ethernet_fix_mac_speed,
	.init = &stmmac_claim_resource,
};

static struct platform_device stx7111_ethernet_device = {
	.name = "stmmaceth",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd110000, 0x08000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", evt2irq(0x12a0), -1),
	},
	.dev.platform_data = &stx7111_ethernet_platform_data,
};

void __init stx7111_configure_ethernet(struct stx7111_ethernet_config *config)
{
	static int configured;
	struct stx7111_ethernet_config default_config;
	struct stm_pad_config *pad_config;
	const int interfaces[] = {
		[stx7111_ethernet_mode_mii] = PHY_INTERFACE_MODE_MII,
		[stx7111_ethernet_mode_rmii] = PHY_INTERFACE_MODE_RMII,
		[stx7111_ethernet_mode_reverse_mii] = PHY_INTERFACE_MODE_MII,
	};

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = &stx7111_ethernet_pad_configs[config->mode];

	stx7111_ethernet_platform_data.custom_cfg = (void *) pad_config;
	stx7111_ethernet_platform_data.interface = interfaces[config->mode];
	stx7111_ethernet_platform_data.bus_id = config->phy_bus;
	stx7111_ethernet_platform_data.phy_addr = config->phy_addr;
	stx7111_ethernet_platform_data.mdio_bus_data = config->mdio_bus_data;

	pad_config->sysconfs[1].value = (config->ext_clk ? 1 : 0);

	/* MAC_SPEED_SEL */
	stx7111_ethernet_platform_data.bsp_priv = sysconf_claim(SYS_CFG,
			7, 20, 20, "stmmac");

	platform_device_register(&stx7111_ethernet_device);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7111_usb_dma_mask = DMA_BIT_MASK(32);

#define USB_PWR "USB_PWR"
#define USB_ACK "USB_ACK"

static void stx7111_usb_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, USB_PWR, value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, USB_ACK)
				 == value)
			break;
		mdelay(10);
	}
}

static struct stm_plat_usb_data stx7111_usb_platform_data = {
	.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
		STM_PLAT_USB_FLAGS_STRAP_PLL |
		STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
	.device_config = &(struct stm_device_config){
		.pad_config = &(struct stm_pad_config) {
			.gpios_num = 2,
			.gpios = (struct stm_pad_gpio []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN(5, 6, -1),
				/* USB power enable */
				STM_PAD_PIO_OUT(5, 7, 1),
			},
			.sysconfs_num = 1,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* OC invert, see stx7111_configure_usb() */
				STM_PAD_SYS_CFG(6, 29, 29, 0),
			},
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYS_CFG(32, 4, 4, USB_PWR),
			STM_DEVICE_SYS_STA(15, 4, 4, USB_ACK),
		},
		.power = stx7111_usb_power,
	},
};

static struct platform_device stx7111_usb_device = {
	.name = "stm-usb",
	.id = 0,
	.dev = {
		.dma_mask = &stx7111_usb_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &stx7111_usb_platform_data,
	},
	.num_resources = 6,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfe1ffe00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ehci", evt2irq(0x1720), -1),
		STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfe1ffc00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ohci", evt2irq(0x1700), -1),
		STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfe100000, 0x100),
		STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfe1fff00, 0x100),
	},
};

void __init stx7111_configure_usb(struct stx7111_usb_config *config)
{
	static int configured;
	struct sysconf_field *sc;

	BUG_ON(configured);
	configured = 1;

	if (config) {
		struct stm_device_config *dev_config;
		dev_config = stx7111_usb_platform_data.device_config;
		dev_config->pad_config->sysconfs[0].value =
				(config->invert_ovrcur ? 1 : 0);
		/* USB edge rise and DC shift - STLinux Bugzilla 10991 */
		sc = sysconf_claim(SYS_CFG, 4, 3, 4, "USB");
		sysconf_write(sc, 2);

	}

	platform_device_register(&stx7111_usb_device);
}

