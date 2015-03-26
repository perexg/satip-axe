/*
 * arch/sh/boards/mach-hmp7105/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Chris Smith <chris.smith@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics HMP7105 support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/pci-glue.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <asm/irq-ilc.h>

/* Note, the board is populated with NOR flash and NAND flash.  By default, the
 * board is configured to boot from NOR flash.  Due to board-level logic, it is
 * not possible to access NAND flash in this configuration (contention on EMI
 * bus when using EMICS1#).  Furthermore, the logic to generate EMI_A26 seems to
 * be flawed, limiting access to NOR flash to 64MB.  The flash setup below
 * reflects these issues.
 *
 * Board mods are required to enable boot-from-NAND, but currently untested and
 * unsupported due to lack of a suitable board.
 */

#define HMP7105_PIO_PHY_RESET stm_gpio(11, 0)
#define HMP7105_PIO_NAND_ENABLE stm_gpio(10, 7)

static void __init hmp7105_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HMP7105 board initialisation\n");

	stx7105_early_device_init();

	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7105_configure_asc(3, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}

static struct platform_device hmp7105_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 4,
		.leds = (struct gpio_led[]) {
			{
				.name = "AmberLED0",
				.gpio = stm_gpio(16, 1),
			},
			{
				.name = "AmberLED1",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(16, 2),
				.active_low = 1,
			},
			{
				.name = "RedLED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(16, 3),
				.active_low = 1,
			},
			{
				.name = "GreenLED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(16, 4),
				.active_low = 1,
			},
		},
	},
};

static int hmp7105_phy_reset(void *bus)
{
	gpio_set_value(HMP7105_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(HMP7105_PIO_PHY_RESET, 1);

	return 0;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = hmp7105_phy_reset,
	.phy_mask = 0,
};

static struct platform_device hmp7105_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 64*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.nr_parts	= 3,
		.parts		= (struct mtd_partition []) {
			{
				.name = "NOR Flash 1",
				.size = 0x00080000,
				.offset = 0x00000000,
			}, {
				.name = "NOR Flash 2",
				.size = 0x00200000,
				.offset = MTDPART_OFS_NXTBLK,
			}, {
				.name = "NOR Flash 3",
				.size = MTDPART_SIZ_FULL,
				.offset = MTDPART_OFS_NXTBLK,
			}
		},
	},
};

static struct platform_device *hmp7105_devices[] __initdata = {
	&hmp7105_leds,
	&hmp7105_nor_flash,
};

static int __init hmp7105_devices_init(void)
{
	stx7105_configure_sata(0);

	stx7105_configure_pwm(&(struct stx7105_pwm_config) {
			.out0 = stx7105_pwm_out0_pio13_0,
			.out1 = stx7105_pwm_out1_disabled, });

	stx7105_configure_ssc_i2c(0, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc0_sclk_pio2_2,
			.routing.ssc1.mtsr = stx7105_ssc0_mtsr_pio2_3, });
	stx7105_configure_ssc_i2c(1, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc1_sclk_pio2_5,
			.routing.ssc1.mtsr = stx7105_ssc1_mtsr_pio2_6, });
	stx7105_configure_ssc_i2c(2, &(struct stx7105_ssc_config) {
			.routing.ssc2.sclk = stx7105_ssc2_sclk_pio3_4,
			.routing.ssc2.mtsr = stx7105_ssc2_mtsr_pio3_5, });
	stx7105_configure_ssc_i2c(3, &(struct stx7105_ssc_config) {
			.routing.ssc3.sclk = stx7105_ssc3_sclk_pio3_6,
			.routing.ssc3.mtsr = stx7105_ssc3_mtsr_pio3_7, });

	stx7105_configure_usb(0, &(struct stx7105_usb_config) {
			.ovrcur_mode = stx7105_usb_ovrcur_active_low,
			.pwr_enabled = 1,
			.routing.usb0.ovrcur = stx7105_usb0_ovrcur_pio4_4,
			.routing.usb0.pwr = stx7105_usb0_pwr_pio4_5, });
	stx7105_configure_usb(1, &(struct stx7105_usb_config) {
			.ovrcur_mode = stx7105_usb_ovrcur_active_low,
			.pwr_enabled = 1,
			.routing.usb1.ovrcur = stx7105_usb1_ovrcur_pio4_6,
			.routing.usb1.pwr = stx7105_usb1_pwr_pio4_7, });

	gpio_request(HMP7105_PIO_PHY_RESET, "notPioResetMII");
	gpio_direction_output(HMP7105_PIO_PHY_RESET, 1);
	stx7105_configure_ethernet(0, &(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	stx7105_configure_lirc(&(struct stx7105_lirc_config) {
			.rx_mode = stx7105_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 1, });

	gpio_request(HMP7105_PIO_NAND_ENABLE, "NANDEnable");
	gpio_direction_output(HMP7105_PIO_NAND_ENABLE, 1);

	return platform_add_devices(hmp7105_devices,
						ARRAY_SIZE(hmp7105_devices));
}
arch_initcall(hmp7105_devices_init);


static void __iomem *hmp7105_ioport_map(unsigned long port, unsigned int size)
{
	/*
	 * If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called.
	 */
	BUG();
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_hmp7105 __initmv = {
	.mv_name		= "STx7105-HMP",
	.mv_setup		= hmp7105_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= hmp7105_ioport_map,
#ifdef CONFIG_SH_ST_SYNOPSYS_PCI
	STM_PCI_IO_MACHINE_VEC
#endif
};
