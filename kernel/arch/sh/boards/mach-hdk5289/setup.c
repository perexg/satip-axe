/*
 * arch/sh/boards/mach-hdk5289/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx5289 reference board support.
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



#define HDK5289_GPIO_RST stm_gpio(2, 2)
#define HDK5289_POWER_ON stm_gpio(2, 3)


/*
 * FLASH setup depends on board boot configuration:
 *
 *				boot-from-
 * Boot Device		NOR		NAND		SPI
 * ----------------     -----		-----		-----
 * JE3-2 (Mode15)	0 (W)		1 (E)		0 (W)
 * JE3-1 (Mode16)	0 (W)		0 (W)		1 (E)
 *
 * Word Size
 * ----------------
 * JE2-2 (Mode13)	0 (W)		1 (E)		x
 *
 * CS-Routing
 * ----------------
 * JH1			NOR (N)		NAND (S)	NAND (S)
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

static void __init hdk5289_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK5289 board initialisation\n");

	stx5206_early_device_init();

	stx5206_configure_asc(2, &(struct stx5206_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
}



static struct tm1668_key hdk5289_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk5289_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk5289_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(3, 5),
		.gpio_sclk = stm_gpio(3, 4),
		.gpio_stb = stm_gpio(2, 7),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk5289_front_panel_keys),
		.keys = hdk5289_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk5289_front_panel_characters),
		.characters = hdk5289_front_panel_characters,
		.text = "5289",
	},
};



static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_mask = 0,
};

/* Serial Flash */
static struct stm_plat_spifsm_data hdk5289_serial_flash =  {
	.name		= "w25q64",
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
static struct stm_nand_bank_data hdk5289_nand_flash = {
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

/* NOR Flash */
static struct platform_device hdk5289_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 64*1024*1024 - 1,
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
static struct platform_device *hdk5289_devices[] __initdata = {
	&hdk5289_front_panel,
	&hdk5289_nor_flash,
};



static int __init hdk5289_devices_init(void)
{
	struct sysconf_field *sc;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/* Configure Flash according to boot-device */
	sc = sysconf_claim(SYS_STA, 1, 16, 17, "boot_device");
	switch (sysconf_read(sc)) {
	case 0x0:
		/* Boot-from-NOR */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(1) - nor_bank_base;
		hdk5289_nand_flash.csn = 1;
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk5289_nand_flash.csn = 0;
		break;
	case 0x2:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk5289_nand_flash.csn = 0;
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
	if (hdk5289_nor_flash.resource[0].end > nor_bank_size - 1)
		hdk5289_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	hdk5289_nor_flash.resource[0].start += nor_bank_base;
	hdk5289_nor_flash.resource[0].end += nor_bank_base;

	/* This board has a one PIO line used to reset almost everything:
	 * ethernet PHY, HDMI transmitter chip, PCI... */
	gpio_request(HDK5289_GPIO_RST, "GPIO_RST#");
	gpio_direction_output(HDK5289_GPIO_RST, 0);
	udelay(1000);
	gpio_set_value(HDK5289_GPIO_RST, 1);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK5289_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK5289_POWER_ON, 1);

	/* Internal I2C link */
	stx5206_configure_ssc_i2c(0);
	/* "FE": UC1 (LNB), NIM, JD2 */
	stx5206_configure_ssc_i2c(1);
	/* "AV": UI2 (EEPROM), UN1 (AV buffer & filter), HDMI,
	 *       JN4 (SCARD board connector) */
	stx5206_configure_ssc_i2c(3);

	stx5206_configure_usb();

	stx5206_configure_ethernet(&(struct stx5206_ethernet_config) {
			.mode = stx5206_ethernet_mode_mii,
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
			.banks = &hdk5289_nand_flash,
			.rbn.flex_connected = 1,});

	stx5206_configure_spifsm(&hdk5289_serial_flash);

	stx5206_configure_mmc();

	return platform_add_devices(hdk5289_devices,
			ARRAY_SIZE(hdk5289_devices));
}
arch_initcall(hdk5289_devices_init);



static void __iomem *hdk5289_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might
	 * think.  I used to use external ROM, but that can cause
	 * problems if you are in the middle of updating Flash. So I'm
	 * now using the processor core version register, which is
	 * guaranteed to be available, and non-writable. */
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_hdk5289 __initmv = {
	.mv_name		= "hdk5289",
	.mv_setup		= hdk5289_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= hdk5289_ioport_map,
};
