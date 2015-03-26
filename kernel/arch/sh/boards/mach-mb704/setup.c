/*
 * arch/sh/boards/mach-mb704/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx5197 processor board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx5197.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/spi/flash.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>

static void __init mb704_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx5197 Mboard initialisation\n");

	stx5197_early_device_init();

	stx5197_configure_asc(2, &(struct stx5197_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
	stx5197_configure_asc(3, &(struct stx5197_asc_config) {
			.hw_flow_control = 0,
			.is_console = 0, });
}



static struct platform_device mb704_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 6),
				.active_low = 1,
			},
		},
	},
};



static struct mtd_partition mb704_serial_flash_partitions[] = {
	{
		.name = "sflash_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "sflash_2",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x20000
	},
};

/* Serial FLASH is AT45DB321x, handled by 'mtd_dataflash' SPI driver */
static struct flash_platform_data mb704_serial_flash_data = {
	.parts = mb704_serial_flash_partitions,
	.nr_parts = ARRAY_SIZE(mb704_serial_flash_partitions),
};

/* Note to use the mb704's SPI Flash device J10 needs to be in position 1-2. */
static struct spi_board_info mb704_serial_flash = {
	.modalias       = "mtd_dataflash",
	.chip_select    = 0,
	.max_speed_hz   = 120000,
	.bus_num        = 0,
	.platform_data  = &mb704_serial_flash_data,
};



static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_mask = 0,
};

static struct platform_device *mb704_devices[] __initdata = {
	&mb704_leds,
};

static int __init mb704_devices_init(void)
{
	/* By default we don't configure PWM as the mb704 has J2C fitted
	 * which results in contention.
	 * stx5197_configure_pwm();
	 */

	/* Configure SSC0 as SPI to drive serial Flash attached to SPI
	 * (requires SSC driver SPI) or as I2C to drive I2C devices such
	 * as HDMI.
	 * To use SSC0 as I2C bus 0 (PIO1:1:0]) it is necessary to fit
	 * jumpers J2-E, J2-G, J2-F and J2-H. */
	stx5197_configure_ssc_spi(0);
	/* I2C bus 1 can be routed to on chip QPSK block (setting routing to
	 * STX7141_ROUTING_SSC1_I2C_QPSK) or the pins QAM_SCLT/SDAT
	 * (setting STX7141_ROUTING_SSC1_I2C_QAM). Both require SSC driver. */
	stx5197_configure_ssc_i2c(1, &(struct stx5197_ssc_i2c_config) {
			.routing.ssc1 = stx5197_ssc1_i2c_qpsk, });
	/* To use I2C bus 2 (PIO3[3:2]) it is necessary to remove jumpers
	 * J7-A, J7-C, J6-H and J6-F and fit J7-B and J6-G. */
	stx5197_configure_ssc_i2c(2, &(struct stx5197_ssc_i2c_config) {
			.routing.ssc2 = stx5197_ssc2_i2c_pio3, });

	stx5197_configure_usb();

	stx5197_configure_ethernet(&(struct stx5197_ethernet_config) {
			.mode = stx5197_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	/* To have UHF available on MB704:
	 * need one wire from J14-C to J14-E */
	stx5197_configure_lirc(&(struct stx5197_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stx5197_lirc_rx_mode_uhf.
#else
			.rx_mode = stx5197_lirc_rx_mode_ir,
#endif
			.tx_enabled = 1, });

	spi_register_board_info(&mb704_serial_flash, 1);

	return platform_add_devices(mb704_devices, ARRAY_SIZE(mb704_devices));
}
arch_initcall(mb704_devices_init);

static void __iomem *mb704_ioport_map(unsigned long port, unsigned int size)
{
	/*
	 * However picking somewhere safe isn't as easy as you might
	 * think.  I used to use external ROM, but that can cause
	 * problems if you are in the middle of updating Flash. So I'm
	 * now using the processor core version register, which is
	 * guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb704_init_irq(void)
{
}

struct sh_machine_vector mv_mb704 __initmv = {
	.mv_name		= "mb704",
	.mv_setup		= mb704_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb704_init_irq,
	.mv_ioport_map		= mb704_ioport_map,
};
