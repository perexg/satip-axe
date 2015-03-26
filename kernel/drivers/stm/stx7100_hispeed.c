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
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/phy.h>
#include <linux/stm/pad.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config stx7100_ethernet_pad_configs[] = {
	[stx7100_ethernet_mode_mii] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			/* Claimed only when config->ext_clk == 0 */
			STM_PAD_PIO_OUT_NAMED(3, 7, 1, "PHYCLK"),
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* DVO_ETH_PAD_DISABLE and ETH_IF_ON */
			STM_PAD_SYS_CFG(7, 16, 17, 3),
			/* RMII_MODE */
			STM_PAD_SYS_CFG(7, 18, 18, 0),
			/*
			 * PHY_CLK_EXT: PHY external clock
			 * 0: PHY clock is provided by STx7109
			 * 1: PHY clock is provided by an external source
			 * Value assigned in stx7100_configure_ethernet()
			 */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
		},
	},
	[stx7100_ethernet_mode_rmii] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			/* Claimed only when config->ext_clk == 0 */
			STM_PAD_PIO_OUT_NAMED(3, 7, 1, "PHYCLK"),
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* DVO_ETH_PAD_DISABLE and ETH_IF_ON */
			STM_PAD_SYS_CFG(7, 16, 17, 3),
			/* RMII_MODE */
			STM_PAD_SYS_CFG(7, 18, 18, 1),
			/* PHY_CLK_EXT */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
		},
	},
};

static void stx7100_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx7100_ethernet_platform_data = {
	/* .pbl is set in stx7100_configure_ethernet() */
	.has_gmac = 0,
	.enh_desc = 0,
	.fix_mac_speed = stx7100_ethernet_fix_mac_speed,
	.init = &stmmac_claim_resource,
};

static struct platform_device stx7100_ethernet_device = {
	.name = "stmmaceth",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18110000, 0x10000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", 133, -1),
	},
	.dev.platform_data = &stx7100_ethernet_platform_data,
};

void __init stx7100_configure_ethernet(struct stx7100_ethernet_config *config)
{
	static int configured;
	struct stx7100_ethernet_config default_config;
	struct stm_pad_config *pad_config;
	int interface;

	/* 7100 doesn't have a MAC */
	if (cpu_data->type == CPU_STX7100)
		return;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = &stx7100_ethernet_pad_configs[config->mode];

	switch (config->mode) {
	case stx7100_ethernet_mode_mii:
		if (config->ext_clk)
			stm_pad_set_pio_ignored(pad_config, "PHYCLK");
		interface = PHY_INTERFACE_MODE_MII;
		break;
	case stx7100_ethernet_mode_rmii:
		if (config->ext_clk)
			stm_pad_set_pio_in(pad_config, "PHYCLK", -1);
		interface = PHY_INTERFACE_MODE_RMII;
		break;
	default:
		BUG();
		break;
	}
	pad_config->sysconfs[2].value = (config->ext_clk ? 1 : 0);

	stx7100_ethernet_platform_data.custom_cfg = (void *) pad_config;
	stx7100_ethernet_platform_data.interface = interface;
	stx7100_ethernet_platform_data.bus_id = config->phy_bus;
	stx7100_ethernet_platform_data.phy_addr = config->phy_addr;
	stx7100_ethernet_platform_data.mdio_bus_data = config->mdio_bus_data;

	/* MAC_SPEED_SEL */
	stx7100_ethernet_platform_data.bsp_priv =
			sysconf_claim(SYS_CFG, 7, 20, 20, "stmmac");

	/* Configure the ethernet MAC PBL depending on the cut of the chip */
	stx7100_ethernet_platform_data.pbl =
			(cpu_data->cut_major == 1) ? 1 : 32;

	platform_device_register(&stx7100_ethernet_device);
}

/* USB resources ---------------------------------------------------------- */

static int stx7100_usb_oc_gpio = -EINVAL;
static int stx7100_usb_pwr_gpio = -EINVAL;

static int stx7100_usb_pad_claim(struct stm_pad_state *state, void *priv)
{
	/* Work around for USB over-current detection chip being
	 * active low, and the 710x being active high.
	 *
	 * This test is wrong for 7100 cut 3.0 (which needs the work
	 * around), but as we can't reliably determine the minor
	 * revision number, hard luck, this works for most people.
	 */
	if ((cpu_data->type == CPU_STX7109 && cpu_data->cut_major < 2) ||
			(cpu_data->type == CPU_STX7100 &&
			cpu_data->cut_major < 3)) {
		stx7100_usb_oc_gpio =
			stm_pad_gpio_request_output(state, "OC", 0);
		BUG_ON(stx7100_usb_oc_gpio == STM_GPIO_INVALID);
	}

	/*
	 * There have been two changes to the USB power enable signal:
	 *
	 * - 7100 upto and including cut 3.0 and 7109 1.0 generated an
	 *   active high enables signal. From 7100 cut 3.1 and 7109 cut 2.0
	 *   the signal changed to active low.
	 *
	 * - The 710x ref board (mb442) has always used power distribution
	 *   chips which have active high enables signals (on rev A and B
	 *   this was a TI TPS2052, rev C used the ST equivalent a ST2052).
	 *   However rev A and B had a pull up on the enables signal, while
	 *   rev C changed this to a pull down.
	 *
	 * The net effect of all this is that the easiest way to drive
	 * this signal is ignore the USB hardware and drive it as a PIO
	 * pin.
	 *
	 * (Note the USB over current input on the 710x changed from active
	 * high to low at the same cuts, but board revs A and B had a resistor
	 * option to select an inverted output from the TPS2052, so no
	 * software work around is required.)
	 */
	stx7100_usb_pwr_gpio = stm_pad_gpio_request_output(state, "PWR", 1);
	BUG_ON(stx7100_usb_pwr_gpio == STM_GPIO_INVALID);

	return 0;
}

static void stx7100_usb_pad_release(struct stm_pad_state *state, void *priv)
{
	if (gpio_is_valid(stx7100_usb_oc_gpio))
		stm_pad_gpio_free(state, stx7100_usb_oc_gpio);
	if (gpio_is_valid(stx7100_usb_pwr_gpio))
		stm_pad_gpio_free(state, stx7100_usb_pwr_gpio);
}

#define USB_PWR "USB_PWR"

static void stx7100_usb_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	stm_device_sysconf_write(device_state, USB_PWR, 
		(power == stm_device_power_on) ? 0 : 1);
	mdelay(30);
}

static struct stm_plat_usb_data stx7100_usb_platform_data = {
	.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
			STM_PLAT_USB_FLAGS_STRAP_PLL |
			STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
	.device_config = &(struct stm_device_config){
		.pad_config = &(struct stm_pad_config) {
			.gpios_num = 2,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN_NAMED(5, 6, -1, "OC"),
				STM_PAD_PIO_OUT_NAMED(5, 7, 1, "PWR"),
			},
			.custom_claim = stx7100_usb_pad_claim,
			.custom_release = stx7100_usb_pad_release,
			.sysconfs_num = 1,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* USB_AT */
				STM_PAD_SYS_CFG(2, 1, 1, 0),
			},
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_device_sysconf []) {
			STM_DEVICE_SYS_CFG(2, 4, 5, USB_PWR),
		},
		.power = stx7100_usb_power,
	},
};

static u64 stx7100_usb_dma_mask = DMA_BIT_MASK(32);

static struct platform_device stx7100_usb_device = {
	.name = "stm-usb",
	.id = 0,
	.dev = {
		.dma_mask = &stx7100_usb_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &stx7100_usb_platform_data,
	},
	.num_resources = 6,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0x191ffe00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ehci", 169, -1),
		STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0x191ffc00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ohci", 168, -1),
		STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0x19100000, 0x100),
		STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0x191fff00, 0x100),
	},
};

void __init stx7100_configure_usb(void)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	platform_device_register(&stx7100_usb_device);
}



/* MiPHY resources -------------------------------------------------------- */

static struct stm_plat_miphy_dummy_data stx7100_miphy_dummy_platform_data = {
	.miphy_first = 0,
	.miphy_count = 1,
	.miphy_modes = (enum miphy_mode[1]) {SATA_MODE},
};

static struct platform_device stx7100_miphy_dummy_device = {
	.name	= "stm-miphy-dummy",
	.id	= 0,
	.num_resources = 0,
	.dev = {
		.platform_data = &stx7100_miphy_dummy_platform_data,
	}
};

static int __init stx7100_miphy_postcore_setup(void)
{
	return platform_device_register(&stx7100_miphy_dummy_device);
}
postcore_initcall(stx7100_miphy_postcore_setup);



/* SATA resources --------------------------------------------------------- */
static struct stm_plat_sata_data stx7100_sata_platform_data = {
	/* filled in stx7100_configure_sata() */
	.device_config = &(struct stm_device_config){},
};

static struct platform_device stx7100_sata_device = {
	.name = "sata-stm",
	.id = -1,
	.dev.platform_data = &stx7100_sata_platform_data,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x19209000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(170, -1),
	},
};

void __init stx7100_configure_sata(void)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (cpu_data->type == CPU_STX7100 && cpu_data->cut_major == 1) {
		/* 7100 cut 1.x */
		stx7100_sata_platform_data.phy_init = 0x0013704A;
	} else {
		/* 7100 cut 2.x and cut 3.x and 7109 */
		stx7100_sata_platform_data.phy_init = 0x388fc;
	}

	if ((cpu_data->type == CPU_STX7109 && cpu_data->cut_major == 1) ||
			cpu_data->type == CPU_STX7100) {
		stx7100_sata_platform_data.only_32bit = 1;
		stx7100_sata_platform_data.pc_glue_logic_init = 0x1ff;
	} else {
		stx7100_sata_platform_data.only_32bit = 0;
		stx7100_sata_platform_data.pc_glue_logic_init = 0x100ff;
	}

	platform_device_register(&stx7100_sata_device);
}
