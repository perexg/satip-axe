/*
 * arch/sh/boards/mach-cab5197/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics cab5197 reference board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx5197.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>

#define CAB5197_PIO_PHY_RESET stm_gpio(1, 4)

static void __init cab5197_setup(char **cmdline_p)
{
	printk(KERN_INFO
		"STMicroelectronics cab5197 reference board initialisation\n");

	stx5197_early_device_init();
	stx5197_configure_asc(3, &(struct stx5197_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
}

/*
 * Configure SSC0 as SPI to drive serial Flash attached to SPI (requires
 * SSC driver SPI) or as I2C to drive I2C devices such as HDMI.
 *
 */

static int cab_phy_reset(void *bus)
{
	gpio_set_value(CAB5197_PIO_PHY_RESET, 0);
	udelay(20);
	gpio_set_value(CAB5197_PIO_PHY_RESET, 1);
	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = cab_phy_reset,
	.phy_mask = 0,
};

/* Note to use the cab5197's SPI Flash device J10 needs to be in
 * position 1-2.
 */

static struct spi_board_info cab5197_spi_device = {
	.modalias       = "mtd_dataflash",
	.chip_select    = 0,
	.max_speed_hz   = 120000,
	.bus_num        = 0,
};

static struct platform_device *cab5197_devices[] __initdata = {
};

static int __init cab5197_device_init(void)
{
	stx5197_configure_ssc_spi(0);
	stx5197_configure_ssc_i2c(1, &(struct stx5197_ssc_i2c_config) {
			.routing.ssc1 = stx5197_ssc1_i2c_qpsk, });
	stx5197_configure_ssc_i2c(2, &(struct stx5197_ssc_i2c_config) {
			.routing.ssc2 = stx5197_ssc2_i2c_pio3, });

	stx5197_configure_usb();

	gpio_request(CAB5197_PIO_PHY_RESET, "notPioResetMII");
	stx5197_configure_ethernet(&(struct stx5197_ethernet_config) {
			.mode = stx5197_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	stx5197_configure_lirc(&(struct stx5197_lirc_config) {
			.rx_mode = stx5197_lirc_rx_mode_ir,
			.tx_enabled = 1, });

	spi_register_board_info(&cab5197_spi_device, 1);

	return platform_add_devices(cab5197_devices,
				ARRAY_SIZE(cab5197_devices));
}
arch_initcall(cab5197_device_init);

static void __iomem *cab5197_ioport_map(unsigned long port, unsigned int size)
{
	/* No I/O ports */
	BUG();
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_cab5197 __initmv = {
	.mv_name		= "cab5197",
	.mv_setup		= cab5197_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= cab5197_ioport_map,
};
