/*
 * arch/sh/boards/mach-mb839/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Carl Shaw (carl.shaw@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics MB839 board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/lirc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/pci-glue.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <asm/irq-ilc.h>

#define MB839_PIO_PHY_NOTINTMODE	stm_gpio(7, 0)
#define MB839_PIO_PHY_MODE1		stm_gpio(7, 1)
#define MB839_PIO_PHY_MODE2		stm_gpio(7, 2)
#define MB839_PIO_PHY_RESET		stm_gpio(7, 3)

static void __init mb839_setup(char **cmdline_p)
{
	printk("STMicroelectronics MB839 initialisation\n");

	stx7105_early_device_init();

	/* smart card on PIO0, modem on PIO4, uart on PIO5 */
	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 0, });
	stx7105_configure_asc(3, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
}

static struct platform_device mb839_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data){
		.num_leds = 2,
		.leds = (struct gpio_led[]){
			{
				.name 		= "LD1",
				.default_trigger= "heartbeat",
				.gpio		= stm_gpio(2, 0),
				.active_low	= 1,
			}, {
				.name		= "LD2",
				.gpio		= stm_gpio(2, 1),
				.active_low	= 1,
			},
		},
	},
};

/*
 * On the MB839, the PHY is held in reset and needs enabling via PIO
 * pins.  Some e/net lines are also used as SoC mode pins and we need to
 * drive these high in order to get e/net to work.
 */

static int mb839_phy_reset(void *bus)
{
	gpio_set_value(MB839_PIO_PHY_NOTINTMODE, 1);
	gpio_set_value(MB839_PIO_PHY_MODE1, 1);
	gpio_set_value(MB839_PIO_PHY_MODE2, 1);

	gpio_set_value(MB839_PIO_PHY_RESET, 1);
	udelay(1);
	gpio_set_value(MB839_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(MB839_PIO_PHY_RESET, 1);
	udelay(1);

	return 0;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = mb839_phy_reset,
	.phy_mask = 0,
};

static struct mtd_partition mb839_mtd_parts_table[] = {
	{
		.name = "NOR FLASH",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00000000,
	 }
};

static struct physmap_flash_data mb839_physmap_flash_data = {
	.width = 2,
	.nr_parts = ARRAY_SIZE(mb839_mtd_parts_table),
	.parts = mb839_mtd_parts_table
};

static struct platform_device mb839_physmap_flash = {
	.name = "physmap-flash",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]){
		{
			.start = 0x00000000,	/* Can be overridden */
			.end = 128 * 1024 * 1024 - 1,
			.flags = IORESOURCE_MEM,
		}
	},
	.dev = {
		.platform_data = &mb839_physmap_flash_data,
	},
};

static struct platform_device *mb839_devices[] __initdata = {
	&mb839_leds,
	&mb839_physmap_flash,
};

static int __init device_init(void)
{
	stx7105_configure_sata(0);

	stx7105_configure_pwm(&(struct stx7105_pwm_config) {
			.out0 = stx7105_pwm_out0_pio13_0,
			.out1 = stx7105_pwm_out1_disabled, });

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

	gpio_request(MB839_PIO_PHY_NOTINTMODE, "PhyNotIntMode");
	gpio_request(MB839_PIO_PHY_MODE1, "PhyMode1Sel");
	gpio_request(MB839_PIO_PHY_MODE2, "PhyMode2Sel");
	gpio_request(MB839_PIO_PHY_RESET, "ResetMII");

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

	return platform_add_devices(mb839_devices, ARRAY_SIZE(mb839_devices));
}

arch_initcall(device_init);

static void __iomem *mb839_ioport_map(unsigned long port, unsigned int size)
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

static void __init mb839_init_irq(void)
{
	/* Configure STEM interrupts as active low. */
	set_irq_type(ILC_EXT_IRQ(1), IRQ_TYPE_LEVEL_LOW);
	set_irq_type(ILC_EXT_IRQ(2), IRQ_TYPE_LEVEL_LOW);
}

struct sh_machine_vector mv_mb839 __initmv = {
	.mv_name		= "MB839 Reference Board",
	.mv_setup		= mb839_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb839_init_irq,
	.mv_ioport_map		= mb839_ioport_map,
#ifdef CONFIG_SH_ST_SYNOPSYS_PCI
	STM_PCI_IO_MACHINE_VEC
#endif
};
