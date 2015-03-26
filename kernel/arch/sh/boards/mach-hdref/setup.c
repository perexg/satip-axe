/*
 * arch/sh/boards/st/hdref/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics HDref Reference board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/phy.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <asm/irl.h>



void __init hdref_setup(char** cmdline_p)
{
	printk("STMicroelectronics HDref Reference board initialisation\n");

	stx7100_early_device_init();

	stx7100_configure_asc(2, &(struct stx7100_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
	stx7100_configure_asc(3, &(struct stx7100_asc_config) {
			.hw_flow_control = 0,
			.is_console = 0, });
}



static struct platform_device hdref_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 8*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= NULL,
	},
};

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_mask = 0,
};

static struct platform_device *hdref_devices[] __initdata = {
	&hdref_physmap_flash,
};

static int __init hdref_device_init(void)
{
	stx7100_configure_sata();

	stx7100_configure_ssc_i2c(0);
	stx7100_configure_ssc_spi(1, NULL);
	stx7100_configure_ssc_i2c(2);

	stx7100_configure_usb();

	stx7100_configure_lirc(&(struct stx7100_lirc_config) {
			.rx_mode = stx7100_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 0, });

	stx7100_configure_pata(&(struct stx7100_pata_config) {
			.emi_bank = 3,
			.pc_mode = 1,
			.irq = IRL1_IRQ, });

	stx7100_configure_ethernet(&(struct stx7100_ethernet_config) {
			.mode = stx7100_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	return platform_add_devices(hdref_devices,
				    ARRAY_SIZE(hdref_devices));
}
device_initcall(hdref_device_init);
