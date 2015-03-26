/*
 * arch/sh/boards/mach-hdk7108/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
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
#include <linux/stm/pci-glue.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
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


#define HDK7108_PIO_POWER_ON stm_gpio(5, 0)
#define HDK7108_PIO_PCI_RESET stm_gpio(6, 4)
#define HDK7108_PIO_POWER_ON_ETHERNET stm_gpio(15, 4)
#define HDK7108_GPIO_FLASH_WP stm_gpio(5, 5)
#define HDK7108_GPIO_MII_SPEED_SEL stm_gpio(21, 7)
#define HDK7108_GPIO_SPI_HOLD stm_gpio(2, 2)
#define HDK7108_GPIO_SPI_WRITE_PRO stm_gpio(2, 3)

static void __init hdk7108_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK7108 board initialisation\n");

	stx7108_early_device_init();

	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio24_4,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio24_5,
			.hw_flow_control = 0,
			.is_console = 1, });
	stx7108_configure_asc(1, &(struct stx7108_asc_config) {
			.hw_flow_control = 1, });
}



static struct platform_device hdk7108_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "GREEN",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 0),
			}, {
				.name = "RED",
				.gpio = stm_gpio(3, 1),
			},
		},
	},
};

static struct tm1668_key hdk7108_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk7108_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk7108_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(2, 5),
		.gpio_sclk = stm_gpio(2, 4),
		.gpio_stb = stm_gpio(2, 6),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk7108_front_panel_keys),
		.keys = hdk7108_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk7108_front_panel_characters),
		.characters = hdk7108_front_panel_characters,
		.text = "7108",
	},
};

static int hdk7108_phy_reset(void *bus)
{
	static int done;

	/* This line is shared between both MII interfaces */
	if (!done) {
		gpio_set_value(HDK7108_PIO_POWER_ON_ETHERNET, 0);
		udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
		gpio_set_value(HDK7108_PIO_POWER_ON_ETHERNET, 1);
		done = 1;
	}

	return 1;
}

static void hdk7108_mii_txclk_select(int txclk_250_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO21[7]. */
	if (txclk_250_not_25_mhz)
		gpio_set_value(HDK7108_GPIO_MII_SPEED_SEL, 1);
	else
		gpio_set_value(HDK7108_GPIO_MII_SPEED_SEL, 0);
}

#ifdef CONFIG_SH_ST_HDK7108_STMMAC0
static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	.bus_id = 0,
	.phy_reset = hdk7108_phy_reset,
	.phy_mask = 0,
};
#endif

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.bus_id = 1,
	.phy_reset = hdk7108_phy_reset,
	.phy_mask = 0,
};

/* NOR Flash */
static struct platform_device hdk7108_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 128*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= NULL,
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

/* NAND Flash */
static struct stm_nand_bank_data hdk7108_nand_flash = {
	.csn		= 1,
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


/*
 * Serial Flash (Setup depends on the board revision and silicon cut.  See
 * comments at top).
 */

/* SPI_GPIO: SPI bus for Serial Flash device */
static struct platform_device hdk7108_serial_flash_bus = {
	.name           = "spi_gpio",
	.id             = 8,
	.num_resources  = 0,
	.dev.platform_data = &(struct spi_gpio_platform_data) {
		.num_chipselect = 1,
		.sck = stm_gpio(1, 6),
#if defined(CONFIG_SH_ST_HDK7108_VER1_BOARD)
		/* MOSI/MISO swapped on board Rev 1.0 */
		.mosi = stm_gpio(2, 1),
		.miso = stm_gpio(2, 0),
#else
		.mosi = stm_gpio(2, 0),
		.miso = stm_gpio(2, 1),
#endif
	}
};

static struct mtd_partition hdk7108_serial_flash_parts[] = {
	{
		.name = "Serial Flash 1",
		.size = 0x00200000,
		.offset = 0,
	}, {
		.name = "Serial Flash 2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

/* SPI_GPIO: Serial Flash device */
static struct spi_board_info hdk7108_serial_flash[] =  {
	{
		.modalias       = "m25p80",
		.bus_num        = 8,
		.controller_data = (void *)stm_gpio(1, 7),
		.max_speed_hz   = 7000000,
		.mode           = SPI_MODE_3,
		.platform_data  = &(struct flash_platform_data) {
			.name = "m25p80",
			.parts = hdk7108_serial_flash_parts,
			.nr_parts = ARRAY_SIZE(hdk7108_serial_flash_parts),
		},
	},
};

#ifndef CONFIG_SH_ST_HDK7108_VER1_BOARD
/* SPI_FSM: Serial Flash device */
static struct stm_plat_spifsm_data hdk7108_spifsm_flash = {
	.parts = hdk7108_serial_flash_parts,
	.nr_parts = ARRAY_SIZE(hdk7108_serial_flash_parts),
};
#endif

static struct platform_device *hdk7108_devices[] __initdata = {
	&hdk7108_leds,
	&hdk7108_front_panel,
	&hdk7108_nor_flash,
};



static struct stm_plat_pci_config hdk7108_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED,
	},
	.serr_irq = PCI_PIN_DEFAULT,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED,
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = HDK7108_PIO_PCI_RESET,
};

#ifdef CONFIG_PCI

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	int irq;

	irq = stm_pci_legacy_irq(dev);
	if (irq > 0)
		return irq;

	/* We can use the standard function on this board */
	return stx7108_pcibios_map_platform_irq(&hdk7108_pci_config, pin);
}

#endif

/* Mali parameters */

/* Memory allocated by Linux Kernel */
static struct stm_mali_resource hdk7108_mali_mem[1] = {
	{
		.name 	= "OS_MEMORY",
		.start 	=  0,
		.end	=  CONFIG_STM_HDK7108_MALI_OS_MEMORY_SIZE - 1,
	},
};

static struct stm_mali_resource hdk7108_mali_ext_mem[] = {
	{
		.name 	= "EXTERNAL_MEMORY_RANGE",
		.start 	=  CONFIG_MEMORY_START,
		.end	=  CONFIG_MEMORY_START + CONFIG_MEMORY_SIZE - 1,
	}
};

static struct stm_mali_config hdk7108_mali_config = {
	.num_mem_resources = ARRAY_SIZE(hdk7108_mali_mem),
	.mem = hdk7108_mali_mem,
	.num_ext_resources = ARRAY_SIZE(hdk7108_mali_ext_mem),
	.ext_mem = hdk7108_mali_ext_mem,
};



static int __init device_init(void)
{
	struct sysconf_field *sc;
	unsigned long boot_mode;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/* Configure Flash according to boot-device */
	if (cpu_data->cut_major >= 2) {
		sc = sysconf_claim(SYS_STA_BANK1, 3, 2, 6, "boot_mode");
		boot_mode = sysconf_read(sc);
	} else {
		sc = sysconf_claim(SYS_STA_BANK1, 3, 2, 5, "boot_mode");
		boot_mode = sysconf_read(sc);
		boot_mode |= 0x10;	/* use non-BCH boot encodings */
	}
	switch (boot_mode) {
	case 0x15:
		pr_info("Configuring FLASH for boot-from-NOR\n");
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(1) - nor_bank_base;
		hdk7108_nand_flash.csn = 1;
		break;
	case 0x14:
		pr_info("Configuring FLASH for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7108_nand_flash.csn = 0;
		break;
	case 0x1a:
		pr_info("Configuring FLASH for boot-from-SPI\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7108_nand_flash.csn = 0;
		break;
	default:
		BUG();
		break;
	}
	sysconf_release(sc);

	/* Update NOR Flash base address and size: */
	/*     - limit bank size to 64MB (some targetpacks define 128MB!) */
	if (nor_bank_size > 64*1024*1024)
		nor_bank_size = 64*1024*1024;
	/*     - reduce visibility of NOR flash to EMI bank size */
	if (hdk7108_nor_flash.resource[0].end > nor_bank_size - 1)
		hdk7108_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	hdk7108_nor_flash.resource[0].start += nor_bank_base;
	hdk7108_nor_flash.resource[0].end += nor_bank_base;

	stx7108_configure_pci(&hdk7108_pci_config);


	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(HDK7108_PIO_POWER_ON_ETHERNET, "POWER_ON_ETHERNET");
	gpio_direction_output(HDK7108_PIO_POWER_ON_ETHERNET, 0);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK7108_PIO_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK7108_PIO_POWER_ON, 1);

	/* NIM */
	stx7108_configure_ssc_i2c(1, NULL);
	/* AV */
	stx7108_configure_ssc_i2c(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio14_4,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio14_5, });
	/* SYS - EEPROM & MII0 */
	stx7108_configure_ssc_i2c(5, NULL);
	/* HDMI */
	stx7108_configure_ssc_i2c(6, NULL);

	stx7108_configure_lirc(&(struct stx7108_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stx7108_lirc_rx_mode_uhf, });
#else
			.rx_mode = stx7108_lirc_rx_mode_ir, });
#endif

	stx7108_configure_usb(0);
	stx7108_configure_usb(1);
	stx7108_configure_usb(2);

#if defined(CONFIG_SH_ST_HDK7108_VER1_BOARD) || \
	defined(CONFIG_SH_ST_HDK7108_VER1_1_BOARD)
	/* 2 SATA's with CUT1.0 and 1 SATA with CUT2.0 */
	if (cpu_data->cut_major >= 2) {
		/*
		 * Cut 2.0 has 2 MiPHYs, where as cut 1 only has 1.
		 * If we use cut 2.0 chip with ver 1.0 board,
		 * port 1 will NOT-WORK for this combination, as
		 * MIPHY1 powerlines are not connected in Ver1.0 board.
		 * We also have to force the use of the TAP instead of
		 * microport, as this is clocked from MiPHY 1.
		 */
		stx7108_configure_miphy(&(struct stx7108_miphy_config) {
				.force_jtag = 1,
				.modes = (enum miphy_mode[2]) {
					SATA_MODE, UNUSED_MODE },
				});
		stx7108_configure_sata(0, &(struct stx7108_sata_config) { });
	} else {
		stx7108_configure_miphy(&(struct stx7108_miphy_config) {
				.modes = (enum miphy_mode[2]) {
					SATA_MODE, SATA_MODE },
				});
		stx7108_configure_sata(0, &(struct stx7108_sata_config) { });
		stx7108_configure_sata(1, &(struct stx7108_sata_config) { });
	}
#elif defined(CONFIG_SH_ST_HDK7108_VER2_BOARD)
	/* PCIe + 1 SATA */
	stx7108_configure_miphy(&(struct stx7108_miphy_config) {
			.modes = (enum miphy_mode[2]) {
				SATA_MODE, PCIE_MODE },
			});
	stx7108_configure_sata(0, &(struct stx7108_sata_config) { });

	stx7108_configure_pcie(&(struct stx7108_pcie_config) {
					.reset_gpio = stm_gpio(24, 6)
				});
#elif defined(CONFIG_SH_ST_HDK7108_VER2_1A_BOARD)
	/* 2 SATA's */
	stx7108_configure_miphy(&(struct stx7108_miphy_config) {
			.modes = (enum miphy_mode[2]) {
				SATA_MODE, SATA_MODE },
			});
	stx7108_configure_sata(0, &(struct stx7108_sata_config) { });
	stx7108_configure_sata(1, &(struct stx7108_sata_config) { });
#endif


#ifdef CONFIG_SH_ST_HDK7108_STMMAC0
	stx7108_configure_ethernet(0, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac0_mdio_bus,
		});
#endif /* CONFIG_SH_ST_HDK7108_STMMAC0  */

	/* HW changes needed to use the GMII mode (GTX CLK) on the
	 * HDK7108V1 GMAC1 (default MAC on this platform):
	 *		RP18    RP42    RP14    Rp17
	 *	GMII    NC       51R    NC      51R
	 *	MII     NC       NC     51R     51R
	 *
	 * On the HDK7108V1/2: remove R31 and place it at R39.
	 *
	 * RGMII more requires the following HW change (on both
	 * HDK7108V1 and HDK7108V2): remove R29 and place it at R37
	 *
	 */
	gpio_request(HDK7108_GPIO_MII_SPEED_SEL, "stmmac");
	gpio_direction_output(HDK7108_GPIO_MII_SPEED_SEL, 0);

	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
#ifndef CONFIG_SH_ST_HDK7108_GMAC_RGMII_MODE
			.mode  = stx7108_ethernet_mode_gmii_gtx,
#else
			.mode  = stx7108_ethernet_mode_rgmii_gtx,
#endif /* CONFIG_SH_ST_HDK7108_GMAC_RGMII_MODE */
			.ext_clk  = 0,
			.phy_bus = 1,
			.txclk_select = hdk7108_mii_txclk_select,
			.phy_addr = 1,
			.mdio_bus_data = &stmmac1_mdio_bus,
		});

	/*
	 * FLASH_WP is shared between between NOR and NAND FLASH.  However,
	 * since NAND MTD has no concept of write-protect, we permanently
	 * disable WP.
	 */
	gpio_request(HDK7108_GPIO_FLASH_WP, "FLASH_WP");
	gpio_direction_output(HDK7108_GPIO_FLASH_WP, 1);

#if defined(CONFIG_SH_ST_HDK7108_VER1_BOARD) || \
	defined(CONFIG_SH_ST_HDK7108_VER1_1_BOARD)
	/*
	 * Rev 1.x boards only; Rev 2.x boards are populated with MLC NAND which
	 * is not supported.
	 */
	stx7108_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &hdk7108_nand_flash,
			.rbn.flex_connected = 1,});
#endif

	/*
	 * Serial Flash setup depends on board revision and silicon cut.  See
	 * comments above.
	 */
#ifndef CONFIG_SH_ST_HDK7108_VER1_BOARD
	if (cpu_data->cut_major >= 2) {
		stx7108_configure_spifsm(&hdk7108_spifsm_flash);
	} else {
#endif
	/* Disable hold and write-protect signals */
	gpio_request(HDK7108_GPIO_SPI_HOLD, "SPI_HOLD");
	gpio_direction_output(HDK7108_GPIO_SPI_HOLD, 1);
	gpio_request(HDK7108_GPIO_SPI_WRITE_PRO, "SPI_WRITE_PRO");
	gpio_direction_output(HDK7108_GPIO_SPI_WRITE_PRO, 1);

	platform_device_register(&hdk7108_serial_flash_bus);

	spi_register_board_info(hdk7108_serial_flash,
				ARRAY_SIZE(hdk7108_serial_flash));
#ifndef CONFIG_SH_ST_HDK7108_VER1_BOARD
	}
#endif

#ifdef CONFIG_SH_ST_HDK7108_MMC_SLOT
	stx7108_configure_mmc(0);
#elif defined(CONFIG_SH_ST_HDK7108_MMC_EMMC)
	stx7108_configure_mmc(1);
#endif

	stx7108_configure_mali(&hdk7108_mali_config);

	stx7108_configure_audio(&(struct stx7108_audio_config) {
			.spdif_player_output_enabled = 1, });

	return platform_add_devices(hdk7108_devices,
			ARRAY_SIZE(hdk7108_devices));
}
arch_initcall(device_init);


static void __iomem *hdk7108_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_hdk7108 __initmv = {
	.mv_name = "hdk7108",
	.mv_setup = hdk7108_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = hdk7108_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};

#ifdef CONFIG_HIBERNATION_ON_MEMORY
int stm_freeze_board(void *data)
{
	gpio_direction_output(HDK7108_PIO_POWER_ON, 0);
	return 0;
}

int stm_defrost_board(void *data)
{
	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_direction_output(HDK7108_PIO_POWER_ON_ETHERNET, 0);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_direction_output(HDK7108_PIO_POWER_ON, 1);

	/* HW changes needed to use the GMII mode (GTX CLK) on the
	 * HDK7108V1
	 */
	gpio_direction_output(HDK7108_GPIO_MII_SPEED_SEL, 0);

	/*
	 * hdk7108_phy_reset(...);
	 */
	gpio_set_value(HDK7108_PIO_POWER_ON_ETHERNET, 0);
	udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(HDK7108_PIO_POWER_ON_ETHERNET, 1);

	return 0;
}
#endif
