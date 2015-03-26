/*
 * arch/sh/boards/mach-hdk7105/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics HDK7105-SDK support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/lirc.h>
#include <linux/gpio.h>
#include <linux/phy.h>
#include <linux/tm1668.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/pci-glue.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <asm/irq-ilc.h>

/*
 * Flash setup depends on boot-device:
 *
 * boot-from-       | NOR                NAND	            SPI
 * ----------------------------------------------------------------------------
 * JE2 (CS routing) | 0 (EMIA->NOR_CS)   1 (EMIA->NAND_CS)  0
 *                  |   (EMIB->NOR_CS)     (EMIB->NOR_CS)     (EMIB->NOR_CS)
 *                  |   (EMIC->NAND_CS)    (EMIC->NOR_CS)     (EMIC->NAND_CS)
 * JE3 (data width) | 0 (16bit)          1 (8bit)           N/A
 * JE5 (mode 15)    | 0 (boot NOR)       1 (boot NAND)	    0 (boot SPI)
 * JE6 (mode 16)    | 0                  0                  1
 * -----------------------------------------------------------------------------
 *
 * [Jumper settings based on board v1.2-011]
 */

#define HDK7105_PIO_PCI_SERR  stm_gpio(15, 4)
#define HDK7105_PIO_PHY_RESET stm_gpio(15, 5)
#define HDK7105_PIO_PCI_RESET stm_gpio(15, 7)
#define HDK7105_GPIO_FLASH_WP stm_gpio(6, 4)



static void __init hdk7105_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK7105 "
			"board initialisation\n");

	stx7105_early_device_init();

	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7105_configure_asc(3, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}

/* PCI configuration */
static struct stm_plat_pci_config hdk7105_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED, /* Modified in hdk7105_device_init() */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = HDK7105_PIO_PCI_RESET,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
        /* We can use the standard function on this board */
	return stx7105_pcibios_map_platform_irq(&hdk7105_pci_config, pin);
}

static struct platform_device hdk7105_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			/* The schematics actually describes these PIOs
			 * the other way round, but all tested boards
			 * had the bi-colour LED fitted like below... */
			{
				.name = "RED", /* This is also frontpanel LED */
				.gpio = stm_gpio(7, 0),
				.active_low = 1,
			}, {
				.name = "GREEN",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(7, 1),
				.active_low = 1,
			},
		},
	},
};

static struct tm1668_key hdk7105_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk7105_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk7105_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(11, 2),
		.gpio_sclk = stm_gpio(11, 3),
		.gpio_stb = stm_gpio(11, 4),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk7105_front_panel_keys),
		.keys = hdk7105_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk7105_front_panel_characters),
		.characters = hdk7105_front_panel_characters,
		.text = "7105",
	},
};



static int hdk7105_phy_reset(void *bus)
{
	gpio_set_value(HDK7105_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(HDK7105_PIO_PHY_RESET, 1);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = hdk7105_phy_reset,
	.phy_mask = 0,
};

/* NOR Flash */
static struct platform_device hdk7105_nor_flash = {
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
struct stm_nand_bank_data hdk7105_nand_flash = {
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
static struct spi_board_info hdk7105_serial_flash = {
	.modalias       = "m25p80",
	.bus_num        = 0,
	.chip_select    = stm_gpio(2, 4),
	.max_speed_hz   = 7000000,
	.mode           = SPI_MODE_3,
	.platform_data  = &(struct flash_platform_data) {
		.name = "m25p80",
		.type = "m25p32",
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
	},
};

static struct platform_device *hdk7105_devices[] __initdata = {
/*	&hdk7105_leds,*/
	&hdk7105_front_panel,
	&hdk7105_nor_flash,
};

static int __init hdk7105_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/* Configure Flash according to boot-device */
	sc = sysconf_claim(SYS_STA, 1, 15, 16, "boot_device");
	switch (sysconf_read(sc)) {
	case 0x0:
		/* Boot-from-NOR: */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		/* NOR mapped to EMIA + EMIB (FMI_A26 = EMI_CSA#) */
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7105_nand_flash.csn = 2;
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7105_nand_flash.csn = 0;
		break;
	case 0x2:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		/* NOR mapped to EMIB, with physical offset of 0x06000000! */
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7105_nand_flash.csn = 2;
		break;
	default:
		BUG();
		break;
	}
	sysconf_release(sc);

	/* Update NOR Flash base address and size: */
	/*     - reduce visibility of NOR flash to EMI bank size */
	if (hdk7105_nor_flash.resource[0].end > nor_bank_size - 1)
		hdk7105_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	hdk7105_nor_flash.resource[0].start += nor_bank_base;
	hdk7105_nor_flash.resource[0].end += nor_bank_base;

	/* Setup the PCI_SERR# PIO */
	if (gpio_request(HDK7105_PIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(HDK7105_PIO_PCI_SERR);
		hdk7105_pci_config.serr_irq =
				gpio_to_irq(HDK7105_PIO_PCI_SERR);
		set_irq_type(hdk7105_pci_config.serr_irq, IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_WARNING "hdk7105: Failed to claim PCI SERR PIO!\n");
	}
	stx7105_configure_pci(&hdk7105_pci_config);

	stx7105_configure_sata(0);

	stx7105_configure_pwm(&(struct stx7105_pwm_config) {
			.out0 = stx7105_pwm_out0_pio13_0,
			.out1 = stx7105_pwm_out1_disabled, });

	/* Set SPI Boot pads as inputs to avoid contention with SSC1 */
	gpio_request(stm_gpio(15, 0), "SPI Boot CLK");
	gpio_direction_input(stm_gpio(15, 0));
	gpio_request(stm_gpio(15, 1), "SPI Boot DOUT");
	gpio_direction_input(stm_gpio(15, 1));
	gpio_request(stm_gpio(15, 2), "SPI Boot NOTCS");
	gpio_direction_input(stm_gpio(15, 2));
	gpio_request(stm_gpio(15, 3), "SPI Boot DIN");
	gpio_direction_input(stm_gpio(15, 3));

	/*
	 * Fix the reset chain so it correct to start with in case the
	 * watchdog expires or we trigger a reset.
	 */
	sc = sysconf_claim(SYS_CFG, 9, 27, 28, "reset_chain");
	sysconf_write(sc, 0);
	/* Release the sysconf bits so the coprocessor driver can claim them */
	sysconf_release(sc);

	/* I2C_xxxA - HDMI */
	stx7105_configure_ssc_i2c(0, &(struct stx7105_ssc_config) {
			.routing.ssc0.sclk = stx7105_ssc0_sclk_pio2_2,
			.routing.ssc0.mtsr = stx7105_ssc0_mtsr_pio2_3, });
	/* SPI - SerialFLASH */
	/*stx7105_configure_ssc_spi(1, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc1_sclk_pio2_5,
			.routing.ssc1.mtsr = stx7105_ssc1_mtsr_pio2_6,
			.routing.ssc1.mrst = stx7105_ssc1_mrst_pio2_7});*/

        stx7105_configure_ssc_i2c(1, &(struct stx7105_ssc_config) {
                        .routing.ssc2.sclk = stx7105_ssc1_sclk_pio2_5,
                        .routing.ssc2.mtsr = stx7105_ssc1_mtsr_pio2_6, });

	/* I2C_xxxC - JN1 (NIM), JN3, UT1 (CI chip), US2 (EEPROM) */
	stx7105_configure_ssc_i2c(2, &(struct stx7105_ssc_config) {
			.routing.ssc2.sclk = stx7105_ssc2_sclk_pio3_4,
			.routing.ssc2.mtsr = stx7105_ssc2_mtsr_pio3_5, });
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

	gpio_request(HDK7105_PIO_PHY_RESET, "eth_phy_reset");
	gpio_direction_output(HDK7105_PIO_PHY_RESET, 1);

	stx7105_configure_ethernet(0, &(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = 0,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	stx7105_configure_lirc(&(struct stx7105_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stx7105_lirc_rx_mode_uhf,
#else
			.rx_mode = stx7105_lirc_rx_mode_ir,
#endif
			.tx_enabled = 0,
			.tx_od_enabled = 0, });

	stx7105_configure_audio(&(struct stx7105_audio_config) {
			.spdif_player_output_enabled = 1, });

	/*
	 * FLASH_WP is shared between between NOR and NAND FLASH.  However,
	 * since NAND MTD has no concept of write-protect, we permanently
	 * disable WP.
	 */
	gpio_request(HDK7105_GPIO_FLASH_WP, "FLASH_WP");
	gpio_direction_output(HDK7105_GPIO_FLASH_WP, 1);

	stx7105_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &hdk7105_nand_flash,
			.rbn.flex_connected = -1,});

	spi_register_board_info(&hdk7105_serial_flash, 1);

	return platform_add_devices(hdk7105_devices,
			ARRAY_SIZE(hdk7105_devices));
}
arch_initcall(hdk7105_device_init);

static void __iomem *hdk7105_ioport_map(unsigned long port, unsigned int size)
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

struct sh_machine_vector mv_hdk7105 __initmv = {
	.mv_name		= "hdk7105",
	.mv_setup		= hdk7105_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= hdk7105_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
