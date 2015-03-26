/*
 * arch/sh/boards/mach-hdk7106/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
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
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/tm1668.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/stm/pci-glue.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <asm/irq-ilc.h>

/*
 * FLASH setup depends on board boot configuration:
 *
 *				boot-from-
 * Mode Pins		NOR		NAND		SPI
 * ----------------     -----		-----		-----
 * JE8-1 (Mode5)	ON  (E)		OFF (W)		ON  (E)
 * JE8-2 (Mode6)	ON  (E)		ON  (E)		OFF (W)
 * JE6-1 (Mode7)	ON  (W)		ON  (W)		ON  (W)
 * JE6-2 (Mode8)	ON  (W)		OFF (E)		OFF (E)
 *
 * CS-Routing
 * ----------------
 * JE2			1-2 (N)		2-3 (S)		2-3 (S)
 * JE11			1-2 (N)		2-3 (S)		2-3 (S)
 *
 * Post-boot Access
 * ----------------
 * NOR (limit)		EMIA (64MB)	EMIB (8MB)	EMIB (8MB)
 * NAND			EMIB/FLEXB	FLEXA		FLEXA
 * SPI			SPIFSM		SPIFSM		SPIFSM
 *
 * Switch positions given in terms of (N)orth, (E)ast, (S)outh, and (W)est,
 * viewing board with LED display South and power connector North.
 *
 */

#define HDK7106_PIO_POWER_ON_ETHERNET stm_gpio(4, 2)
#define HDK7106_PIO_POWER_ON stm_gpio(4, 3)
#define HDK7106_PIO_PCI_SERR stm_gpio(15, 7)



static void __init hdk7106_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK7106 board initialisation\n");

	stx7105_early_device_init();

	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 0,
			.is_console = 1, });
}



static struct platform_device hdk7106_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP green",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 1),
				.active_low = 1,
			},
		},
	},
};

static struct tm1668_key hdk7106_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk7106_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk7106_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(5, 1),
		.gpio_sclk = stm_gpio(5, 0),
		.gpio_stb = stm_gpio(5, 2),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk7106_front_panel_keys),
		.keys = hdk7106_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk7106_front_panel_characters),
		.characters = hdk7106_front_panel_characters,
		.text = "7106",
	},
};



/* PCI configuration */
static struct stm_plat_pci_config hdk7106_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED, /* SERR PIO is shared with MII1_CRS */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = HDK7106_PIO_PCI_SERR,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	/* We can use the standard function on this board */
	return stx7105_pcibios_map_platform_irq(&hdk7106_pci_config, pin);
}



static int hdk7106_phy_reset(void *bus)
{
	gpio_set_value(HDK7106_PIO_POWER_ON_ETHERNET, 0);
	udelay(10000); /* 10 milliseconds is enough for everyone ;-) */
	gpio_set_value(HDK7106_PIO_POWER_ON_ETHERNET, 1);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	.bus_id = 0,
	.phy_reset = hdk7106_phy_reset,
	.phy_mask = 0,
};

#ifdef CONFIG_SH_ST_HDK7106_STMMAC1
static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.bus_id = 1,
	.phy_reset = hdk7106_phy_reset,
	.phy_mask = 0,
};
#endif
/* NOR Flash */
static struct platform_device hdk7106_nor_flash = {
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
	.dev.platform_data	= &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= NULL,
		.nr_parts	= 3,
		.parts		=  (struct mtd_partition []) {
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
struct stm_nand_bank_data hdk7106_nand_flash = {
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
	.timing_data		= &(struct stm_nand_timing_data) {
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

/* Serial Flash */
static struct stm_plat_spifsm_data hdk7106_serial_flash =  {
	.name = "m25p32",
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

static struct platform_device *hdk7106_devices[] __initdata = {
	&hdk7106_leds,
	&hdk7106_front_panel,
	&hdk7106_nor_flash,
};



static int __init device_init(void)
{
	struct sysconf_field *sc;
	u32 boot_mode;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/* Configure Flash according to boot-device */
	sc = sysconf_claim(SYS_STA, 1, 5, 8, "boot_device");
	boot_mode = sysconf_read(sc);
	if (boot_mode == 0 || boot_mode == 5) {
		/* Boot-from-NOR */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(1) - nor_bank_base;
		hdk7106_nand_flash.csn = 1;
	} else if ((boot_mode > 0 && boot_mode < 5) ||
		   (boot_mode > 5 && boot_mode < 10)) {
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7106_nand_flash.csn = 0;
	} else if (boot_mode == 10 || boot_mode == 11) {
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7106_nand_flash.csn = 0;
	} else {
		BUG();
	}
	sysconf_release(sc);

	/* Update NOR Flash base address and size: */
	/*     - limit bank size to 64MB (some targetpacks define 128MB!) */
	if (nor_bank_size > 64*1024*1024)
		nor_bank_size = 64*1024*1024;
	/*     - reduce visibility of NOR flash to EMI bank size */
	if (hdk7106_nor_flash.resource[0].end > nor_bank_size - 1)
		hdk7106_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	hdk7106_nor_flash.resource[0].start += nor_bank_base;
	hdk7106_nor_flash.resource[0].end += nor_bank_base;

	/* Setup the PCI_SERR# PIO */
	if (gpio_request(HDK7106_PIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(HDK7106_PIO_PCI_SERR);
		hdk7106_pci_config.serr_irq =
				gpio_to_irq(HDK7106_PIO_PCI_SERR);
		set_irq_type(hdk7106_pci_config.serr_irq, IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_WARNING "hdk7106: Failed to claim PCI SERR PIO!\n");
	}
	stx7105_configure_pci(&hdk7106_pci_config);

	stx7105_configure_sata(0);
	stx7105_configure_sata(1);

	/* I2C_xxxA - HDMI */
	stx7105_configure_ssc_i2c(0, &(struct stx7105_ssc_config) {
			.routing.ssc0.sclk = stx7105_ssc0_sclk_pio2_2,
			.routing.ssc0.mtsr = stx7105_ssc0_mtsr_pio2_3, });
	/* I2C_xxxB - US2 (EEPROM), JV4 (SCART board connector),
	 *            UV1 (AV buffer & filter), JN6 (MII1 connector),
	 *            JN1 (NIM), JN3 */
	stx7105_configure_ssc_i2c(1, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc1_sclk_pio2_5,
			.routing.ssc1.mtsr = stx7105_ssc1_mtsr_pio2_6, });
	/* I2C_xxxD - JN2 (NIM), JN4 */
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

	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(HDK7106_PIO_POWER_ON_ETHERNET, "POWER_ON_ETHERNET");
	gpio_direction_output(HDK7106_PIO_POWER_ON_ETHERNET, 0);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK7106_PIO_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK7106_PIO_POWER_ON, 1);

	stx7105_configure_ethernet(0, &(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = 1,
			.mdio_bus_data = &stmmac0_mdio_bus,
		});

#ifdef CONFIG_SH_ST_HDK7106_STMMAC1
	stx7105_configure_ethernet(1, &(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.routing.mii1.mdio = stx7105_ethernet_mii1_mdio_pio3_4,
			.routing.mii1.mdc = stx7105_ethernet_mii1_mdc_pio3_5,
			.ext_clk = 1,
			.phy_bus = 1,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac1_mdio_bus,
		});
#endif

	stx7105_configure_lirc(NULL);

	stx7105_configure_audio(&(struct stx7105_audio_config) {
			.spdif_player_output_enabled = 1, });

	/* HW note:
	 * There is and error on the 7106-HDK MMC daughter board V 1.0
	 * schematics.
	 * The STMPS2151STR Pin 4 should be 'EN' and the Pin 5 should be
	 * 'Power_IN'. These are reverse.
	 */
	stx7105_configure_mmc();

	stx7105_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &hdk7106_nand_flash,
			.rbn.flex_connected = 1,});

	stx7106_configure_spifsm(&hdk7106_serial_flash);

	return platform_add_devices(hdk7106_devices,
			ARRAY_SIZE(hdk7106_devices));
}
arch_initcall(device_init);


static void __iomem *hdk7106_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_hdk7106 __initmv = {
	.mv_name = "hdk7106",
	.mv_setup = hdk7106_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = hdk7106_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};

