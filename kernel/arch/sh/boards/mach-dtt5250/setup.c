/*
 * arch/sh/boards/mach-dtt5250/setup.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: Nunzio Raciti <nunzio.raciti@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx5250 reference board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/tm1668.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/emi.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx5206.h>
#include <asm/irq.h>


#define DTT5250_RED_LED stm_gpio(1, 5)
#define DTT5250_ETH_RST stm_gpio(2, 4)
#define DTT5250_POWER_ON stm_gpio(2, 3)


/*
 * FLASH setup depends on board boot configuration:
 *
 * Boot Device      NAND    SPI
 * ---------------  -----   -----
 * JE3-1 (Mode15)   0 ON    1 OFF
 * JE3-2 (Mode16)   1 OFF   0 ON
 *
 */

static void __init dtt5250_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics DTT5250 board initialisation\n");

	stx5206_early_device_init();

	stx5206_configure_asc(2, &(struct stx5206_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
}

static struct platform_device dtt5250_leds = {
    .name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			/* Red Led on board */
			{
				.name = "RED",
				.default_trigger = "heartbeat",
				.gpio = DTT5250_RED_LED,
				.active_low = 0,
			},
		},
	},
};

static struct tm1668_key dtt5250_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character dtt5250_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device dtt5250_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(3, 5),
		.gpio_sclk = stm_gpio(3, 4),
		.gpio_stb = stm_gpio(2, 7),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(dtt5250_front_panel_keys),
		.keys = dtt5250_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(dtt5250_front_panel_characters),
		.characters = dtt5250_front_panel_characters,
		.text = "5250",
	},
};

static int dtt5250_phy_reset(void *bus)
{
	gpio_set_value(DTT5250_ETH_RST, 0);
	udelay(15000);
	gpio_set_value(DTT5250_ETH_RST, 1);
	udelay(10000);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = dtt5250_phy_reset,
	.phy_mask = 0,
};

/* Serial Flash */
static struct stm_plat_spifsm_data dtt5250_serial_flash =  {
	.name		= "n25q128",
	.nr_parts	= 2,
	.parts = (struct mtd_partition []) {
		{
			.name = "Serial Flash 1",
			.size = 0x00080000,
			.offset = 0,
		}, {
			.name = "Serial Flash 2",
			.size = MTDPART_SIZ_FULL,
			.offset = MTDPART_OFS_NXTBLK,
		},
	},
};

/* NAND Flash */
static struct stm_nand_bank_data dtt5250_nand_flash = {
	.csn		= 0,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.nr_partitions	= 2,
	.partitions	= (struct mtd_partition []) {
		{
			.name	= "NAND Flash 1",
			.offset	= 0,
			.size 	= 0x00800000
		}, {
			.name	= "NAND Flash 2",
			.offset = MTDPART_OFS_NXTBLK,
			.size	= MTDPART_SIZ_FULL
		},
	},
	.timing_data	=  &(struct stm_nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30,		/* in us */
	},
};


static struct platform_device *dtt5250_devices[] __initdata = {
	&dtt5250_leds,
	&dtt5250_front_panel,
};

static int __init dtt5250_devices_init(void)
{
	struct sysconf_field *sc;

	/* Configure Flash according to boot-device */
	sc = sysconf_claim(SYS_STA, 1, 16, 17, "boot_device");
	switch (sysconf_read(sc)) {
	case 0x0:
		/* Boot-from-NOR */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		break;
	case 0x2:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		break;
	default:
		BUG();
		break;
	}
	sysconf_release(sc);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(DTT5250_POWER_ON, "POWER_ON");
	gpio_direction_output(DTT5250_POWER_ON, 1);

	gpio_request(DTT5250_ETH_RST, "GPIO_RST#");
	gpio_direction_output(DTT5250_ETH_RST, 0);
	udelay(15000);
	gpio_set_value(DTT5250_ETH_RST, 1);

	/* Internal I2C link */
	stx5206_configure_ssc_i2c(0);

	stx5206_configure_ssc_i2c(1);

	stx5206_configure_ssc_i2c(3);

	stx5206_configure_usb();

	stx5206_configure_ethernet(&(struct stx5206_ethernet_config) {
			.mode = stx5206_ethernet_mode_rmii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	stx5206_configure_lirc(&(struct stx5206_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stx5206_lirc_rx_mode_uhf, });
#else
			.rx_mode = stx5206_lirc_rx_mode_ir, });
#endif
	stx5206_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &dtt5250_nand_flash,
			.rbn.flex_connected = 1,});

	stx5206_configure_spifsm(&dtt5250_serial_flash);

	stx5206_configure_mmc();

	return platform_add_devices(dtt5250_devices,
			ARRAY_SIZE(dtt5250_devices));
}
arch_initcall(dtt5250_devices_init);


static void __iomem *dtt5250_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might
	 * think.  I used to use external ROM, but that can cause
	 * problems if you are in the middle of updating Flash. So I'm
	 * now using the processor core version register, which is
	 * guaranteed to be available, and non-writable. */
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_dtt5250 __initmv = {
	.mv_name		= "dtt5250",
	.mv_setup		= dtt5250_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map	= dtt5250_ioport_map,
};
