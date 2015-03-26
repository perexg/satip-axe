/*
 * arch/sh/boards/st/mb448/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7109E Reference board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <asm/irl.h>



#define MB448_PIO_SMC91X_RESET stm_gpio(2, 6)
#define MB448_PIO_FLASH_VPP stm_gpio(2, 7)



void __init mb448_setup(char **cmdline_p)
{
	printk("STMicroelectronics STb7109E Reference board initialisation\n");

	stx7100_early_device_init();

	stx7100_configure_asc(2, &(struct stx7100_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
	stx7100_configure_asc(3, &(struct stx7100_asc_config) {
			.hw_flow_control = 0,
			.is_console = 0, });
}



static struct resource mb448_smc91x_resources[] = {
	[0] = {
		.start	= 0xa2000300,
		.end	= 0xa2000300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRL3_IRQ,
		.end	= IRL3_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device mb448_smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(mb448_smc91x_resources),
	.resource	= mb448_smc91x_resources,
};



static void mb448_set_vpp(struct map_info *info, int enable)
{
	gpio_set_value(MB448_PIO_FLASH_VPP, enable);
}

static struct platform_device mb448_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 8*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= mb448_set_vpp,
	},
};



#define STMMAC_PHY_ADDR 14
static int stmmac_phy_irqs[PHY_MAX_ADDR] = {
	[STMMAC_PHY_ADDR] = IRL0_IRQ,
};
static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_mask = 1,
	.irqs = stmmac_phy_irqs,
};

static struct platform_device *mb448_devices[] __initdata = {
	&mb448_smc91x_device,
	&mb448_physmap_flash,
};

static int __init mb448_device_init(void)
{
	stx7100_configure_sata();

	stx7100_configure_ssc_i2c(0);
	stx7100_configure_ssc_spi(1, NULL);
	stx7100_configure_ssc_i2c(2);

	stx7100_configure_usb();

	stx7100_configure_ethernet(&(struct stx7100_ethernet_config) {
			.mode = stx7100_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = STMMAC_PHY_ADDR,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	gpio_request(MB448_PIO_FLASH_VPP, "Flash VPP");
	gpio_direction_output(MB448_PIO_FLASH_VPP, 0);

	/* Reset the SMSC 91C111 Ethernet chip */
	gpio_request(MB448_PIO_SMC91X_RESET, "SMC91x reset");
	gpio_direction_output(MB448_PIO_SMC91X_RESET, 0);
	udelay(1);
	gpio_set_value(MB448_PIO_SMC91X_RESET, 1);
	udelay(1);
	gpio_set_value(MB448_PIO_SMC91X_RESET, 0);

	return platform_add_devices(mb448_devices,
			ARRAY_SIZE(mb448_devices));
}

device_initcall(mb448_device_init);
