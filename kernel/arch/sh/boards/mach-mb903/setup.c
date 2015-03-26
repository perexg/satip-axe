/*
 * arch/sh/boards/mach-mb903/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/tm1668.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



#define MB903_PIO_GMII0_NOTRESET stm_gpio(20, 0)
#define MB903_PIO_GMII1_NOTRESET stm_gpio(15, 4)
#define MB903_PIO_GMII1_PHY_CLKOUTNOTTX_CLK_SEL stm_gpio(19, 1)

/* All the power-related PIOs are pulled up, so "ON" by default,
 * there is no need to touch them unless one really wants to... */
#define MB903_PIO_POWER_ON_5V stm_gpio(21, 7)
#define MB903_PIO_POWER_ON_1V5LMI stm_gpio(22, 0)
#define MB903_PIO_POWER_ON_SATA stm_gpio(22, 2)
#define MB903_PIO_POWER_ON_SHUTDOWN stm_gpio(22, 3)



static void __init mb903_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics mb903 board initialisation\n");

	stx7108_early_device_init();

	/* CN1 ("RS232") */
	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio24_4,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio24_5,
			/* First mb903 revision has RTS connected in
			 * a wrong way, therefore HW flow control
			 * is unusable */
#if 0
			.routing.asc3.cts = stx7108_asc3_cts_pio25_0,
			.routing.asc3.rts = stx7108_asc3_rts_pio24_7,
			.hw_flow_control = 1,
#endif
			.is_console = 1, });

	/* CN17 ("MODEM") */
	stx7108_configure_asc(1, &(struct stx7108_asc_config) {
			.hw_flow_control = 1, });
}



static struct platform_device mb903_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 3,
		.leds = (struct gpio_led[]) {
			{
				.name = "LD5 (GREEN)",
				.gpio = stm_gpio(6, 4),
				.default_trigger = "heartbeat",
			}, {
				.name = "LD4 (GREEN)",
				.gpio = stm_gpio(6, 5),
			}, {
				.name = "FP_POWER_ON",
				.gpio = stm_gpio(22, 1),
				.active_low = 1,
			},
		},
	},
};

static struct tm1668_key mb903_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character mb903_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_SEGMENTS,
	{ 'M', 0x037 },
	{ 'O', 0x03f },
	{ 'C', 0x039 },
	{ 'A', 0x077 },
};

static struct platform_device mb903_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_sclk = stm_gpio(2, 4),
		.gpio_dio = stm_gpio(2, 5),
		.gpio_stb = stm_gpio(2, 6),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(mb903_front_panel_keys),
		.keys = mb903_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(mb903_front_panel_characters),
		.characters = mb903_front_panel_characters,
		.text = "MOCA",
	},
};


static int mb903_mii0_phy_reset(void *bus)
{
	gpio_set_value(MB903_PIO_GMII0_NOTRESET, 0);
	udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(MB903_PIO_GMII0_NOTRESET, 1);

	return 1;
}

static int mb903_mii1_phy_reset(void *bus)
{
	gpio_set_value(MB903_PIO_GMII1_NOTRESET, 0);
	udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(MB903_PIO_GMII1_NOTRESET, 1);

	return 1;
}

static void mb903_mi11_txclk_select(int txclk_250_not_25_mhz)
{
	gpio_set_value(MB903_PIO_GMII1_PHY_CLKOUTNOTTX_CLK_SEL,
			!!txclk_250_not_25_mhz);
}

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.bus_id = 1,
	.phy_reset = mb903_mii1_phy_reset,
	.phy_mask = 0,
};

static struct fixed_phy_status stmmac0_fixed_phy_status = {
	.link = 1,
	.speed = 100,
	.duplex = 1,
};

static struct platform_device *mb903_devices[] __initdata = {
	&mb903_leds,
	&mb903_front_panel,
};



static int __init mb903_device_init(void)
{
	gpio_request(MB903_PIO_GMII0_NOTRESET, "GMII0_notRESET");
	gpio_direction_output(MB903_PIO_GMII0_NOTRESET, 0);

	gpio_request(MB903_PIO_GMII1_NOTRESET, "GMII1_notRESET");
	gpio_direction_output(MB903_PIO_GMII1_NOTRESET, 0);

	gpio_request(MB903_PIO_GMII1_PHY_CLKOUTNOTTX_CLK_SEL,
			"GMII1_PHY_CLKOUTnotTX_CLK_SEL");
	gpio_direction_output(MB903_PIO_GMII1_PHY_CLKOUTNOTTX_CLK_SEL, 1);

	/* FRONTEND: CN18 ("I2C FRONTEND"), CN20 ("I2C FRONTEND MODULE") */
	stx7108_configure_ssc_i2c(1, NULL);
	/* ZIGBEE MODULE: U55 (SPZB260) */
	stx7108_configure_ssc_spi(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio1_3,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio1_4,
			.routing.ssc2.mrst = stx7108_ssc2_mrst_pio1_5, });
	/* PIO_HDMI: CN4 ("HDMI" via U8),
	 * BACKEND: U21 (EEPROM), U29 (STTS75 tempereature sensor),
	 * U26 (STV6440), CN10 ("GMII MODULE"), CN12 ("I2C BACKEND") */
	stx7108_configure_ssc_i2c(6, NULL);
	/* SYS - EEPROM & MII0 */
	stx7108_configure_ssc_i2c(5, NULL);

	stx7108_configure_lirc(&(struct stx7108_lirc_config) {
			.rx_mode = stx7108_lirc_rx_mode_ir, });

	/* J1 ("FAN Connector") */
	stx7108_configure_pwm(&(struct stx7108_pwm_config) {
			.out0_enabled = 1, });

	stx7108_configure_usb(0);
	stx7108_configure_usb(1);
	stx7108_configure_usb(2);

	stx7108_configure_miphy(&(struct stx7108_miphy_config) {
			.modes = (enum miphy_mode[2]) {
				SATA_MODE, SATA_MODE },
			});
	stx7108_configure_sata(0, &(struct stx7108_sata_config) { });
	stx7108_configure_sata(1, &(struct stx7108_sata_config) { });
	BUG_ON(fixed_phy_add(PHY_POLL, 1, &stmmac0_fixed_phy_status));
	stx7108_configure_ethernet(0, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = 1,
		});

	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_gmii_gtx,
			.ext_clk = 0,
			.phy_bus = 1,
			.txclk_select = mb903_mi11_txclk_select,
			.phy_addr = 1,
			.mdio_bus_data = &stmmac1_mdio_bus,
		});


	return platform_add_devices(mb903_devices,
			ARRAY_SIZE(mb903_devices));
}
arch_initcall(mb903_device_init);


static void __iomem *mb903_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_mb903 __initmv = {
	.mv_name = "mb903",
	.mv_setup = mb903_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = mb903_ioport_map,
};


