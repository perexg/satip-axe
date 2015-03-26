/*
 * arch/sh/boards/st/mb602/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb5202 Reference board support.
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



#define MB602_PIO_STE100P_RESET stm_gpio(2, 4)
#define MB602_PIO_SMC91X_RESET stm_gpio(2, 6)



void __init mb602_setup(char** cmdline_p)
{
	printk("STMicroelectronics STb5202 Reference board initialisation\n");

	stx7100_early_device_init();

	stx7100_configure_asc(2, &(struct stx7100_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7100_configure_asc(3, &(struct stx7100_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}

static struct platform_device mb602_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 8*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
	},
};



static struct platform_device mb602_smsc_lan9117 = {
	.name		= "smc911x",
	.id		= -1,
	.num_resources	= 4,
	.resource	= (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x03000000,
			.end   = 0x030000ff,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = IRL0_IRQ,
			.end   = IRL0_IRQ,
		},
		/* See end of "drivers/net/smsc_911x/smsc9118.c" file
		 * for description of two following resources. */
		{
			.flags = IORESOURCE_IRQ,
			.name  = "polarity",
			.start = 1,
			.end   = 1,
		},
		{
			.flags = IORESOURCE_IRQ,
			.name  = "type",
			.start = 1,
			.end   = 1,
		},
	},
};

static int mb602_phy_reset(void* bus)
{
	gpio_set_value(MB602_PIO_STE100P_RESET, 1);
	udelay(1);
	gpio_set_value(MB602_PIO_STE100P_RESET, 0);
	udelay(1);
	gpio_set_value(MB602_PIO_STE100P_RESET, 1);

	return 1;
}



#define STMMAC_PHY_ADDR 14
static int stmmac_phy_irqs[PHY_MAX_ADDR] = {
	[STMMAC_PHY_ADDR] = IRL3_IRQ,
};
static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = mb602_phy_reset,
	.phy_mask = 1,
	.irqs = stmmac_phy_irqs,
};

static struct platform_device *mb602_devices[] __initdata = {
	&mb602_physmap_flash,
	&mb602_smsc_lan9117,
};

static int __init mb602_device_init(void)
{
	stx7100_configure_sata();

	stx7100_configure_pwm(&(struct stx7100_pwm_config) {
			.out0_enabled = 0,
			.out1_enabled = 1, });

	stx7100_configure_ssc_i2c(0);
	stx7100_configure_ssc_spi(1, NULL);
	stx7100_configure_ssc_i2c(2);

	stx7100_configure_usb();

	stx7100_configure_lirc(&(struct stx7100_lirc_config) {
			.rx_mode = stx7100_lirc_rx_mode_ir,
			.tx_enabled = 0,
			.tx_od_enabled = 0, });

	stx7100_configure_pata(&(struct stx7100_pata_config) {
			.emi_bank = 3,
			.pc_mode = 1,
			.irq = IRL1_IRQ, });

	gpio_request(MB602_PIO_STE100P_RESET, "STE100P reset");
	gpio_direction_output(MB602_PIO_STE100P_RESET, 1);

	stx7100_configure_ethernet(&(struct stx7100_ethernet_config) {
			.mode = stx7100_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = STMMAC_PHY_ADDR,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	/* Reset the SMSC 9117 Ethernet chip */
	gpio_request(MB602_PIO_SMC91X_RESET, "SMC91x reset");
	gpio_direction_output(MB602_PIO_SMC91X_RESET, 0);
	udelay(1);
	gpio_set_value(MB602_PIO_SMC91X_RESET, 1);
	udelay(1);
	gpio_set_value(MB602_PIO_SMC91X_RESET, 0);

	return platform_add_devices(mb602_devices,
				    ARRAY_SIZE(mb602_devices));
}
device_initcall(mb602_device_init);
