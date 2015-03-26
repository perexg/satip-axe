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
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/phy.h>
#include <linux/stm/miphy.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stx7200.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config *stx7200_ethernet_pad_configs[] = {
	[0] = (struct stm_pad_config []) {
		[stx7200_ethernet_mode_mii] = {
			.sysconfs_num = 8,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_ETHERNET_MII_MODE0:
				 *   0 = MII, 1 = RMII */
				STM_PAD_SYS_CFG(41, 0, 0, 0),
				/* CONF_ETHERNET_PHY_CLK_EXT0:
				 *   0 = PHY clock provided by the 7200,
				 *   1 = external PHY clock source;
				 *   set by stx7200_configure_ethernet() */
				STM_PAD_SYS_CFG(41, 2, 2, -1),
				/* CONF_ETHERNET_VCI_ACK_SOURCE0 */
				STM_PAD_SYS_CFG(41, 6, 6, 0),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(41, 8, 8, 1),
				/* DISABLE_MSG_FOR_READ0 */
				STM_PAD_SYS_CFG(41, 12, 12, 0),
				/* DISABLE_MSG_FOR_WRITE0 */
				STM_PAD_SYS_CFG(41, 14, 14, 0),
				/* CONF_PAD_ETH0: MII0RXERR, MII0TXD.2-3,
				 *   MII0RXCLK, MII0RXD.2-3 pads function:
				 *   0 = ethernet, 1 = transport,
				 * CONF_PAD_ETH1: MII0TXCLK pad direction:
				 *   0 = output, 1 = input, */
				STM_PAD_SYS_CFG(41, 16 + 0, 16 + 1, 0),
				/* CONF_PAD_ETH10: MII0MDC, MII0TXEN &
				 *   MII0TXD[3:0] pads function:
				 *   0 = ethernet, 1 = transport. */
				STM_PAD_SYS_CFG(41, 16 + 10, 16 + 10, 0),
			},
		},
		[stx7200_ethernet_mode_rmii] = {
			.sysconfs_num = 7,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_ETHERNET_MII_MODE0:
				 *   0 = MII, 1 = RMII */
				STM_PAD_SYS_CFG(41, 0, 0, 1),
				/* CONF_ETHERNET_PHY_CLK_EXT0:
				 *   0 = PHY clock provided by the 7200,
				 *   1 = external PHY clock source;
				 *   set by stx7200_configure_ethernet() */
				STM_PAD_SYS_CFG(41, 2, 2, -1),
				/* CONF_ETHERNET_VCI_ACK_SOURCE0 */
				STM_PAD_SYS_CFG(41, 6, 6, 0),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(41, 8, 8, 1),
				/* DISABLE_MSG_FOR_READ0 */
				STM_PAD_SYS_CFG(41, 12, 12, 0),
				/* DISABLE_MSG_FOR_WRITE0 */
				STM_PAD_SYS_CFG(41, 14, 14, 0),
				/* CONF_PAD_ETH10: MII0MDC, MII0TXEN &
				 *   MII0TXD[3:0] pads function:
				 *   0 = ethernet, 1 = transport. */
				STM_PAD_SYS_CFG(41, 16 + 10, 16 + 10, 0),
			},
		},
	},
	[1] = (struct stm_pad_config []) {
		[stx7200_ethernet_mode_mii] = {
			.sysconfs_num = 9,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_ETHERNET_MII_MODE1:
				 *   0 = MII, 1 = RMII */
				STM_PAD_SYS_CFG(41, 1, 1, 0),
				/* CONF_ETHERNET_PHY_CLK_EXT1:
				 *   0 = PHY clock provided by the 7200,
				 *   1 = external PHY clock source;
				 *   set by stx7200_configure_ethernet() */
				STM_PAD_SYS_CFG(41, 3, 3, -1),
				/* CONF_ETHERNET_VCI_ACK_SOURCE1 */
				STM_PAD_SYS_CFG(41, 7, 7, 0),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(41, 9, 9, 1),
				/* DISABLE_MSG_FOR_READ1 */
				STM_PAD_SYS_CFG(41, 13, 13, 0),
				/* DISABLE_MSG_FOR_WRITE1 */
				STM_PAD_SYS_CFG(41, 15, 15, 0),
				/* CONF_PAD_ETH2: MII1TXEN & MII1TXD.1-3
				 *   pads function:
				 *   0 = ethernet, 1 = transport,
				 * CONF_PAD_ETH3: MII1MDC, MII1RXCLK, &
				 *   MIIRXD.0-1 pads function:
				 *   0 = ethernet, 1 = transport,
				 * CONF_PAD_ETH4: MII1RXD.3, MII1TXCLK,
				 *   MII1COL, MII1CRS, MII1MDINT &
				 *   MII1PHYCLK pads function:
				 *   0 = ethernet, 1 = audio */
				STM_PAD_SYS_CFG(41, 16 + 2, 16 + 4, 0),
				/* CONF_PAD_ETH6: MII1TXD.0 pad direction:
				 *   0 = output, 1 = input, */
				STM_PAD_SYS_CFG(41, 16 + 6, 16 + 6, 0),
				/* CONF_PAD_ETH9: MII1RXCLK & MII1RXD.0-1
				 *   pads function:
				 *   0 = ethernet, 1 = transport, */
				STM_PAD_SYS_CFG(41, 16 + 9, 16 + 9, 0),
			},
		},
		[stx7200_ethernet_mode_rmii] = {
			.sysconfs_num = 9,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* CONF_ETHERNET_MII_MODE1:
				 *   0 = MII, 1 = RMII */
				STM_PAD_SYS_CFG(41, 1, 1, 1),
				/* CONF_ETHERNET_PHY_CLK_EXT1:
				 *   0 = PHY clock provided by the 7200,
				 *   1 = external PHY clock source;
				 *   set by stx7200_configure_ethernet() */
				STM_PAD_SYS_CFG(41, 3, 3, -1),
				/* CONF_ETHERNET_VCI_ACK_SOURCE1 */
				STM_PAD_SYS_CFG(41, 7, 7, 0),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(41, 9, 9, 1),
				/* DISABLE_MSG_FOR_READ1 */
				STM_PAD_SYS_CFG(41, 13, 13, 0),
				/* DISABLE_MSG_FOR_WRITE1 */
				STM_PAD_SYS_CFG(41, 15, 15, 0),
				/* CONF_PAD_ETH2: MII1TXEN & MII1TXD.1-3
				 *   pads function:
				 *   0 = ethernet, 1 = transport,
				 * CONF_PAD_ETH3: MII1MDC, MII1RXCLK, &
				 *   MIIRXD.0-1 pads function:
				 *   0 = ethernet, 1 = transport, */
				STM_PAD_SYS_CFG(41, 16 + 2, 16 + 3, 0),
				/* CONF_PAD_ETH6: MII1TXD.0 pad direction:
				 *   0 = output, 1 = input, */
				STM_PAD_SYS_CFG(41, 16 + 6, 16 + 6, 0),
				/* CONF_PAD_ETH9: MII1RXCLK & MII1RXD.0-1
				 *   pads function:
				 *   0 = ethernet, 1 = transport, */
				STM_PAD_SYS_CFG(41, 16 + 9, 16 + 9, 0),
			},
		},
	},
};

static void stx7200_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx7200_ethernet_platform_data[] = {
	[0] = {
		.pbl = 32,
		.has_gmac = 0,
		.enh_desc = 0,
		.fix_mac_speed = stx7200_ethernet_fix_mac_speed,
		.init = &stmmac_claim_resource,
	},
	[1] = {
		.pbl = 32,
		.has_gmac = 0,
		.enh_desc = 0,
		.fix_mac_speed = stx7200_ethernet_fix_mac_speed,
		.init = &stmmac_claim_resource,
	},
};

static struct platform_device stx7200_ethernet_devices[] = {
	[0] = {
		.name = "stmmaceth",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd500000, 0x10000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(92), -1),
		},
		.dev.platform_data = &stx7200_ethernet_platform_data[0],
	},
	[1] = {
		.name = "stmmaceth",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd510000, 0x10000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(94), -1),
		},
		.dev.platform_data = &stx7200_ethernet_platform_data[1],
	},
};

void __init stx7200_configure_ethernet(int port,
		struct stx7200_ethernet_config *config)
{
	static int configured[ARRAY_SIZE(stx7200_ethernet_devices)];
	struct stx7200_ethernet_config default_config;
	struct stm_pad_config *pad_config;
	int interface;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7200_ethernet_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	if (!config)
		config = &default_config;

	if (config->mode == stx7200_ethernet_mode_rmii)
		interface = PHY_INTERFACE_MODE_RMII;
	else
		interface = PHY_INTERFACE_MODE_MII;

	pad_config = &stx7200_ethernet_pad_configs[port][config->mode];

	stx7200_ethernet_platform_data[port].custom_cfg = (void *) pad_config;
	stx7200_ethernet_platform_data[port].interface = interface;
	stx7200_ethernet_platform_data[port].bus_id = config->phy_bus;
	stx7200_ethernet_platform_data[port].phy_addr = config->phy_addr;
	stx7200_ethernet_platform_data[port].mdio_bus_data =
		config->mdio_bus_data;

	pad_config->sysconfs[1].value = (config->ext_clk ? 1 : 0);

	/* MAC_SPEED_SEL */
	stx7200_ethernet_platform_data[port].bsp_priv = sysconf_claim(SYS_CFG,
			41, 4 + port, 4 + port, "stmmac");

	platform_device_register(&stx7200_ethernet_devices[port]);
}



/* USB resources ---------------------------------------------------------- */
#define USB_PWR "USB_PWR"
#define USB_ACK "USB_ACK"

static void stx7200_usb_power(struct stm_device_state *device_state,
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

static struct sysconf_field *stx7200_usb_conf_pad_pio_2_3;

static int stx7200_usb_claim(struct stm_pad_state *state, void *priv)
{
	if (!stx7200_usb_conf_pad_pio_2_3) {
		/* route USB and parts of MAFE instead of DVO:
		 * CONF_PAD_PIO[2] = 0
		 * DVO output selection (probably ignored):
		 * CONF_PAD_PIO[3] = 0 */
		stx7200_usb_conf_pad_pio_2_3 = sysconf_claim(SYS_CFG,
				7, 26, 27, "USB");
		BUG_ON(!stx7200_usb_conf_pad_pio_2_3);
		sysconf_write(stx7200_usb_conf_pad_pio_2_3, 0);
	}

	return 0;
}

static void stx7200_usb_release(struct stm_pad_state *state, void *priv)
{
	if (stx7200_usb_conf_pad_pio_2_3) {
		sysconf_release(stx7200_usb_conf_pad_pio_2_3);
		stx7200_usb_conf_pad_pio_2_3 = NULL;
	}
}

static u64 stx7200_usb_dma_mask = DMA_BIT_MASK(32);

static struct stm_plat_usb_data stx7200_usb_platform_data[] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.device_config = &(struct stm_device_config){
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection, must be
					 * BIDIR for cut 1 -
					 * see stx7200_configure_usb()
					 */
					STM_PAD_PIO_IN_NAMED(7, 0, -1, "OC"),
					/* USB power enable */
					STM_PAD_PIO_OUT(7, 1, 1),
				},
				.custom_claim = stx7200_usb_claim,
				.custom_release = stx7200_usb_release,
			},
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYS_CFG(22, 3, 3, USB_PWR),
				STM_DEVICE_SYS_STA(13, 2, 2, USB_ACK),
				},
			.power = stx7200_usb_power,
			},
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.device_config = &(struct stm_device_config){
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection, must be
					 * BIDIR for cut 1 -
					 * see stx7200_configure_usb()
					 */
					STM_PAD_PIO_IN_NAMED(7, 2, -1, "OC"),
					/* USB power enable */
				STM_PAD_PIO_OUT(7, 3, 1),
				},
				.custom_claim = stx7200_usb_claim,
				.custom_release = stx7200_usb_release,
			},
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYS_CFG(22, 4, 4, USB_PWR),
				STM_DEVICE_SYS_STA(13, 3, 3, USB_ACK),
				},
			.power = stx7200_usb_power,
			},
	},
	[2] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.device_config = &(struct stm_device_config){
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* USB power enable */
					STM_PAD_PIO_OUT(7, 4, 1),
					/* Overcurrent detection, must be
					 * BIDIR for cut 1 -
					 * see stx7200_configure_usb()
					 */
					STM_PAD_PIO_IN_NAMED(7, 5, -1, "OC"),
				},
				.custom_claim = stx7200_usb_claim,
				.custom_release = stx7200_usb_release,
			},
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYS_CFG(22, 5, 5, USB_PWR),
				STM_DEVICE_SYS_STA(13, 4, 4, USB_ACK),
			},
			.power = stx7200_usb_power,
		},
	},
};

static struct platform_device stx7200_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &stx7200_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7200_usb_platform_data[0],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfd2ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(80), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfd2ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(81), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfd200000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfd2fff00,
						    0x100),
		},
	},
	[1] = {
		.name = "stm-usb",
		.id = 1,
		.dev = {
			.dma_mask = &stx7200_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7200_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfd3ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(82), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfd3ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(83), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfd300000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfd3fff00,
						    0x100),
		},
	},
	[2] = {
		.name = "stm-usb",
		.id = 2,
		.dev = {
			.dma_mask = &stx7200_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7200_usb_platform_data[2],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfd4ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(84), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfd4ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(85), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfd400000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfd4fff00,
						    0x100),
		},
	},
};

/* Workaround for USB problems on 7200 cut 1;
 * alternative to RC delay on board */
static int __init stx7200_usb_soft_jtag_reset(void)
{
	int i, j;
	struct sysconf_field *sc;
	static int done;

	if (done)
		return 0;
	done = 1;

	/* Enable soft JTAG mode for USB and SATA
	 * soft_jtag_en = 1 */
	sc = sysconf_claim(SYS_CFG, 33, 6, 6, "usb");
	sysconf_write(sc, 1);
	sysconf_release(sc);
	/* tck = tdi = trstn_usb = tms_usb = 0 */
	sc = sysconf_claim(SYS_CFG, 33, 0, 3, "usb");
	sysconf_write(sc, 0);
	sysconf_release(sc);

	sc = sysconf_claim(SYS_CFG, 33, 0, 6, "usb");

	/* ENABLE SOFT JTAG */
	sysconf_write(sc, 0x00000040);

	/* RELEASE TAP RESET */
	sysconf_write(sc, 0x00000044);

	/* SET TAP INTO IDLE STATE */
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 101 select TCB*/
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TCB */
	for (i = 0; i <= 53; i++) {
		if ((i == 0) || (i == 1) || (i == 19) || (i == 36)) {
			sysconf_write(sc, 0x00000044);
			sysconf_write(sc, 0x00000045);
		}

		if ((i == 53)) {
			sysconf_write(sc, 0x0000004c);
			sysconf_write(sc, 0x0000004D);
		}
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	for (i = 0; i <= 53; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	sysconf_write(sc, 0x00000040);

	/* RELEASE TAP RESET */
	sysconf_write(sc, 0x00000044);

	/* SET TAP INTO IDLE STATE */
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 110 select TPR*/
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDO */
	for (i = 0; i <= 366; i++) {
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	for (j = 0; j < 2; j++) {
		for (i = 0; i <= 365; i++) {
			if ((i == 71) || (i == 192) || (i == 313)) {
				sysconf_write(sc, 0x00000044);
				sysconf_write(sc, 0x00000045);
			}
			sysconf_write(sc, 0x00000044);
			sysconf_write(sc, 0x00000045);

			if ((i == 365))	{
				sysconf_write(sc, 0x0000004c);
				sysconf_write(sc, 0x0000004d);
			}
		}
	}

	for (i = 0; i <= 366; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004C);
	sysconf_write(sc, 0x0000004D);
	sysconf_write(sc, 0x0000004C);
	sysconf_write(sc, 0x0000004D);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 101 select TCB */
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TCB */
	for (i = 0; i <= 53; i++) {
		if ((i == 0) || (i == 1) || (i == 18) ||
				(i == 19) || (i == 36) || (i == 37)) {
			sysconf_write(sc, 0x00000046);
			sysconf_write(sc, 0x00000047);
		}
		if ((i == 53)) {
			sysconf_write(sc, 0x0000004c);
			sysconf_write(sc, 0x0000004D);
		}
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);


	for (i = 0; i <= 53; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 110 select TPR*/
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	for (i = 0; i <= 366; i++) {
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	mdelay(20);
	sysconf_write(sc, 0x00000040);
	sysconf_release(sc);

	return 0;
}

void __init stx7200_configure_usb(int port)
{
	static int configured[ARRAY_SIZE(stx7200_usb_devices)];

	BUG_ON(port < 0 || port > ARRAY_SIZE(stx7200_usb_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	/* Cut 1.0 suffered from (just) a few issues with USB... */
	if (cpu_data->cut_major < 2) {
		struct stm_pad_config *pad_config =
			stx7200_usb_platform_data[port]
				.device_config->pad_config;

		stm_pad_set_pio_bidir(pad_config, "OC", 1);

		stx7200_usb_soft_jtag_reset();
	}

	platform_device_register(&stx7200_usb_devices[port]);
}



/* MiPHY resources -------------------------------------------------------- */

static struct stm_tap_sysconf tap_sysconf = {
	.tck = { SYS_CFG, 33, 0, 0},
	.tms = { SYS_CFG, 33, 5, 5},
	.tdi = { SYS_CFG, 33, 1, 1},
	.tdo = { SYS_STA, 0, 1, 1},
	.tap_en = { SYS_CFG, 33, 6, 6},
	.trstn = { SYS_CFG, 33, 4, 4},
};

struct stm_plat_tap_data stx7200_tap_platform_data = {
	.miphy_first = 0,
	.miphy_count = 1,
	.miphy_modes = (enum miphy_mode[1]) {SATA_MODE},
	.tap_sysconf = &tap_sysconf,
};

static struct platform_device stx7200_tap_device = {
	.name	= "stm-miphy-tap",
	.id	= 0,
	.num_resources = 0,
	.dev = {
		.platform_data = &stx7200_tap_platform_data,
	}
};

static int __init stx7200_miphy_postcore_setup(void)
{
	int ret = 0;

	if (cpu_data->cut_major >= 3)
		ret = platform_device_register(&stx7200_tap_device);

	return ret;
}
postcore_initcall(stx7200_miphy_postcore_setup);



/* SATA resources --------------------------------------------------------- */
static void stx7200_sata_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 0 : 1;
	int i;

	stm_device_sysconf_write(device_state, "SATA_PWR", value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, "SATA_ACK")
			== value)
			break;
		mdelay(10);
	}

	return ;
}

/* Ok to have same private data for both controllers */
static struct stm_plat_sata_data stx7200_sata_platform_data = {
	.phy_init = 0,
	.pc_glue_logic_init = 0,
	.only_32bit = 0,
	.host_restart = NULL,
	.port_num = 0,
	.miphy_num = 0,
	.device_config = &(struct stm_device_config) {
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYS_CFG(22, 1, 1, "SATA_PWR"),
				STM_DEVICE_SYS_STA(13, 0, 0, "SATA_ACK"),
			},
			.power = stx7200_sata_power,
		}
};

static struct platform_device stx7200_sata_device = {
	.name = "sata-stm",
	.id = -1,
	.dev.platform_data = &stx7200_sata_platform_data,
	.num_resources = 3,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd520000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("hostc", ILC_IRQ(89), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("dmac", ILC_IRQ(88), -1),
	},
};

void __init stx7200_configure_sata(void)
{
	static int configured;
	BUG_ON(configured++);

	if (cpu_data->cut_major < 3) {
		pr_warning("SATA is only supported on cut 3 or later\n");
		return;
	}

	platform_device_register(&stx7200_sata_device);
}
