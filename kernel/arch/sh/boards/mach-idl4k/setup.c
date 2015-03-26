/*
 * arch/sh/boards/mach-idl4k/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * 2011-05-20 : modifications for Inverto idl4k (msz@inverto.tv)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/tm1668.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/spi/flash.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <linux/stm/pio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pm_notify.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <asm/sizes.h>
#include <asm/irq-ilc.h>

#undef GMAC_RGMII_MODE
/*#define GMAC_RGMII_MODE*/

/*
 * The FLASH devices are configured according to the boot-mode:
 *
 *                                    boot-from-XXX
 * --------------------------------------------------------------------
 *                         NOR             NAND            SPI
 * Mode Pins              (x16)        (x8, LP, LA)        (ST)
 * --------------------------------------------------------------------
 * JH2 1 (M2)             1 (E)            0 (W)          0 (W)
 *     2 (M3)             0 (W)            0 (W)          1 (E)
 * JH4 1 (M4)             1 (E)            1 (E)          0 (W)
 *     2 (M5)             0 (W)            0 (W)          1 (E)
 *
 * CS Routing
 * --------------------------------------------------------------------
 * JF-2                  2-1 (E)           2-3 (W)         2-3 (W)
 * JF-3                  2-1 (E)           2-3 (W)         2-3 (W)
 *
 * Post-boot Access
 * --------------------------------------------------------------------
 * NOR (limit)         EMIA (64MB)     EMIB (8MB)[1]    EMIB (8MB)[2]
 * NAND                EMIB/FLEXB          FLEXA           FLEXA [2]
 * SPI                 SPI_PIO/FSM     SPI_PIO/FSM       SPI_PIO/FSM
 * --------------------------------------------------------------------
 *
 *
 * Switch positions are given in terms of (N)orth, (E)ast, (S)outh, and (W)est,
 * when viewing the board with the LED display to the South and the power
 * connector to the North.
 *
 * [1] It is also possible to map NOR Flash onto EMIC.  This gives access to
 *     40MB, but has the side-effect of setting A26 which imposes a 64MB offset
 *     from the start of the Flash device.
 *
 * [2] An alternative configuration is map NAND onto EMIB/FLEXB, and NOR onto
 *     EMIC (using the boot-from-NOR "CS Routing").  This allows the EMI
 *     bit-banging driver to be used for NAND Flash, and gives access to 40MB
 *     NOR Flash, subject to the conditions in note [1].
 *
 * [3] Serial Flash setup depends on the board revision and silicon cut:
 *     Rev 1.0, cut x.x: GPIO SPI bus (MISO/MOSI swapped) and "m25p80"
 *                       Serial Flash driver
 *     Rev 2.x, cut 1.0: GPIO SPI bus and "m25p80" Serial Flash driver
 *     Rev 2.x, cut 2.0: SPI FSM Serial Flash Controller
 *
 *     Note, Rev 1.0 kernel builds include only SPI GPIO support.  Rev 2.0
 *     builds include SPI GPIO and SPI FSM support; the appropriate driver is
 *     selected at runtime (see device_init() below).
 */


#define IDL4K_PIO_POWER_ON stm_gpio(5, 0)
#define IDL4K_PIO_POWER_ON_ETHERNET stm_gpio(15, 4)
#define IDL4K_GPIO_MII_SPEED_SEL stm_gpio(21, 7)
#define IDL4K_GPIO_VLNA_ON stm_gpio(14, 3)

#define IDL4K_PIO_LED_POWER_WHITE stm_gpio(3, 1)
#define IDL4K_PIO_LED_POWER_RED 	stm_gpio(3, 0)

typedef void (*stdby_callback_f)( int stdbyNoWakeUp );

static stdby_callback_f stdby_callback_tab[4] = { 0 };

static void __init idl4k_setup(char **cmdline_p)
{
	printk(KERN_INFO "Inverto idl4k board initialisation\n");

	stx7108_early_device_init();

	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio24_4,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio24_5,
			.hw_flow_control = 0,
			.is_console = 1, });
}

static int idl4k_phy_reset(void *bus)
{
	static int done;

	/* This line is shared between both MII interfaces */
	if (!done) {
		gpio_set_value(IDL4K_PIO_POWER_ON_ETHERNET, 0);
		mdelay(10); /* 10 miliseconds is enough for everyone ;-) */
		gpio_set_value(IDL4K_PIO_POWER_ON_ETHERNET, 1);
        mdelay(30); /* 30 miliseconds as for Realtek chip ;-) */
		done = 1;
	}

	return 1;
}

static void idl4k_mii_txclk_select(int txclk_250_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO21[7]. */
	if (txclk_250_not_25_mhz)
		gpio_set_value(IDL4K_GPIO_MII_SPEED_SEL, 1);
	else
		gpio_set_value(IDL4K_GPIO_MII_SPEED_SEL, 0);
}

#define STMMAC_PHY_ADDR 1
#define GMII1_MDINT     25
static int stmmac_phy_irqs[PHY_MAX_ADDR] = {
	/* PIO15_6 - MII1 Management Data Interrupt ETH_1_MDINT
	 * ILC3 input 'External 25'
	 * (ST7108 datasheet (8183887) rev C: page 114, 161)
	 */
	[STMMAC_PHY_ADDR] = GMII1_MDINT,
};

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.bus_id = 1,
	.phy_reset = idl4k_phy_reset,
	.phy_mask = 0,
	.irqs = stmmac_phy_irqs,
};

/* NAND Flash */
static struct stm_nand_bank_data idl4k_nand_flash = {
	.csn		= 0,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.nr_partitions	= 5,
	.partitions	= (struct mtd_partition []) {
		{
			.name   = "nand-env",
			.offset = 0,
			.size   = 0x0020000
		}, {
			.name   = "nand-system",
			.offset = MTDPART_OFS_NXTBLK,
			.size   = 0x0400000
		}, {
			.name   = "nand-fw1",
			.offset = MTDPART_OFS_NXTBLK,
			.size   = 0x2000000
		}, {
			.name   = "nand-fw2",
			.offset = MTDPART_OFS_NXTBLK,
			.size   = 0x2000000
		}, {
			.name   = "nand-data",
			.offset = MTDPART_OFS_NXTBLK,
			.size   = MTDPART_SIZ_FULL
		},
	},
	.timing_data = &(struct stm_nand_timing_data) {
		.sig_setup      = 10,           /* times in ns */
		.sig_hold       = 10,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on          = 10,
		.wr_off         = 30,
		.rd_on          = 10,
		.rd_off         = 30,
		.chip_delay     = 30,           /* in us */
	},
};

static struct at24_platform_data idl4k_m24lc64 = {
	.byte_len	= SZ_64K / 8,
	.page_size	= 32,
	.flags=AT24_FLAG_ADDR16,
};

static struct i2c_board_info idl4k_i2c_devices[] = {
	{
		I2C_BOARD_INFO("24c64", 0x50), /* E0=0, E1=0, E2=0 */
		.platform_data = &idl4k_m24lc64,
	},
};

void idl4k_switch_standby_led(int on)
{
	if ( on )
	{
		gpio_direction_output(IDL4K_PIO_LED_POWER_WHITE, 0);
		gpio_direction_output(IDL4K_PIO_LED_POWER_RED, 1);
	}
	else
	{
		gpio_direction_output(IDL4K_PIO_LED_POWER_WHITE, 1);
		gpio_direction_output(IDL4K_PIO_LED_POWER_RED, 0);
	}
}

void pm_suspend_board_ntfy(void)
{
    int i;
	gpio_direction_output(IDL4K_PIO_POWER_ON, 1);
	gpio_direction_output(IDL4K_GPIO_VLNA_ON, 0);

	gpio_direction_output(IDL4K_PIO_LED_POWER_WHITE, 0);
	gpio_direction_output(IDL4K_PIO_LED_POWER_RED, 1);
	mdelay(20);

    for ( i = 0; i < sizeof(stdby_callback_tab)/sizeof(stdby_callback_tab[0]); ++i)
    {
        if ( stdby_callback_tab[i] == 0 )
            break;

        stdby_callback_tab[i]( ~0 );
    }
}

void pm_wake_board_ntfy(void)
{
    int i;
	gpio_direction_output(IDL4K_PIO_POWER_ON, 0);
	gpio_direction_output(IDL4K_GPIO_VLNA_ON, 1);

	mdelay(20);

	gpio_direction_output(IDL4K_PIO_LED_POWER_WHITE, 1);
	gpio_direction_output(IDL4K_PIO_LED_POWER_RED, 0);

    for ( i = 0; i < sizeof(stdby_callback_tab)/sizeof(stdby_callback_tab[0]); ++i)
    {
        if ( stdby_callback_tab[i] == 0 )
            break;

        stdby_callback_tab[i]( 0 );
    }
}

static int __init device_init(void)
{
	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(IDL4K_PIO_POWER_ON_ETHERNET, "POWER_ON_ETHERNET");
	gpio_direction_output(IDL4K_PIO_POWER_ON_ETHERNET, 0);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(IDL4K_PIO_POWER_ON, "POWER_ON");
	gpio_direction_output(IDL4K_PIO_POWER_ON, 0);

	/*gpio_request(IDL4K_GPIO_VLNA_ON, "VLNA_ON");
	gpio_direction_output(IDL4K_GPIO_VLNA_ON, 0);*/

	/* NIM */
	stx7108_configure_ssc_i2c(1, NULL);
	/* SYS - EEPROM */
	stx7108_configure_ssc_i2c(5, NULL);
	/* NIM VLNA */
	/*stx7108_configure_ssc_i2c(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio14_4,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio14_5, });*/

	i2c_register_board_info(1, idl4k_i2c_devices, ARRAY_SIZE(idl4k_i2c_devices));

	stx7108_configure_usb(0);
	stx7108_configure_usb(1);
	stx7108_configure_usb(2);

	gpio_request(IDL4K_GPIO_MII_SPEED_SEL, "stmmac");
	gpio_direction_output(IDL4K_GPIO_MII_SPEED_SEL, 1);

	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
    	.mode  = stx7108_ethernet_mode_gmii_gtx,
    	.ext_clk  = 0,
		.phy_bus = 1,
		.txclk_select = idl4k_mii_txclk_select,
		.phy_addr = STMMAC_PHY_ADDR,
		.mdio_bus_data = &stmmac1_mdio_bus,
	});

    stx7108_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &idl4k_nand_flash,
			.rbn.flex_connected = 1,});

	gpio_request(IDL4K_PIO_LED_POWER_WHITE, "PWRL_WHITE");

	gpio_request(IDL4K_PIO_LED_POWER_WHITE, "PWRL_WHITE");
	gpio_request(IDL4K_PIO_LED_POWER_RED, "PWRL_RED");

	gpio_direction_output(IDL4K_PIO_LED_POWER_WHITE, 1);
	gpio_direction_output(IDL4K_PIO_LED_POWER_RED, 0);

	/* PMEB pio */
	gpio_request(stm_gpio(23, 1), "PMEB_PIO");
	gpio_direction_input(stm_gpio(23, 1));

	return 0;
}
arch_initcall(device_init);


static void __iomem *idl4k_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_idl4k __initmv = {
	.mv_name = "idl4k",
	.mv_setup = idl4k_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = idl4k_ioport_map,
};

void idl4k_register_stdby_callback( stdby_callback_f stdby_callback )
{
    int i;
    for ( i = 0; i < sizeof(stdby_callback_tab)/sizeof(stdby_callback_tab[0]); ++i)
        if ( stdby_callback_tab[i] == 0 )
        {
            stdby_callback_tab[i] = stdby_callback;
            break;
        }
}
EXPORT_SYMBOL(idl4k_register_stdby_callback);
EXPORT_SYMBOL(idl4k_switch_standby_led);
