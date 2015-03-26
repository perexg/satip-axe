/*
 * arch/sh/boards/mach-fldb/setup.c
 *
 * Copyright (C) 2010-2011 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics Freeman Lite Development Board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/stm/emi.h>
#include <linux/stm/platform.h>
#include <linux/stm/fli7510.h>
#include <linux/stm/pci-glue.h>
#include <asm/irq-ilc.h>
#include <sound/stm.h>



#define FLDB_PIO_RESET_OUTN stm_gpio(11, 6)
#define FLDB_PIO_PCI_IDSEL stm_gpio(16, 2)
#define FLDB_PIO_PCI_RESET stm_gpio(16, 5)



static void __init fldb_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics Freeman Lite Development Board "
			"initialisation\n");

	fli7510_early_device_init();

	/* CNB1 ("UART 2" connector) */
	fli7510_configure_asc(1, &(struct fli7510_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });

	/* CNB4 ("UART 3" connector) */
	fli7510_configure_asc(2, &(struct fli7510_asc_config) {
			.hw_flow_control = 0,
			.is_console = 0, });
}



static struct platform_device fldb_led_df1 = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "DF1 orange",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(8, 5),
			},
		},
	},
};



static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_mask = 0,
};

/* Serial Flash */
static struct stm_plat_spifsm_data fldb_spifsm_flash = {
	.name = "m25px64",
	.nr_parts = 2,
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
static struct stm_nand_bank_data fldb_nand_flash = {
	.csn		= 1,	/* updated in fldb_device_init() */
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
static struct platform_device fldb_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{		/* updated in fldb_device_init() */
			.start		= 0x00000000,
			.end		= 32*1024*1024 - 1,
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

static struct platform_device *fldb_devices[] __initdata = {
	&fldb_led_df1,
	&fldb_nor_flash,
};

static struct stm_plat_pci_config fldb_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = FLDB_PIO_PCI_RESET,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return fli7510_pcibios_map_platform_irq(&fldb_pci_config, pin);
}



static int __init fldb_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/* Configure Flash according to boot device  */
	sc = sysconf_claim(CFG_MODE_PIN_STATUS, 7, 8, "boot_device");
	switch (sysconf_read(sc)) {
	case 0x0:
		pr_info("Configuring Flash for boot-from-NOR\n");
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(1) - nor_bank_base;
		fldb_nand_flash.csn = 1;
		break;
	case 0x1:
		pr_info("Configuring Flash for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		fldb_nand_flash.csn = 0;
		break;
	case 0x2:
		pr_info("Configuring Flash for boot-from-SPI\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		fldb_nand_flash.csn = 0;
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
	if (fldb_nor_flash.resource[0].end > nor_bank_size - 1)
		fldb_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	fldb_nor_flash.resource[0].start += nor_bank_base;
	fldb_nor_flash.resource[0].end += nor_bank_base;

	/* This is a board-level reset line, which goes to the
	 * Ethernet PHY, audio amps & number of extension connectors */
	if (gpio_request(FLDB_PIO_RESET_OUTN, "RESET_OUTN") == 0) {
		gpio_direction_output(FLDB_PIO_RESET_OUTN, 0);
		udelay(10000); /* 10ms is the Ethernet PHY requirement */
		gpio_set_value(FLDB_PIO_RESET_OUTN, 1);
	} else {
		printk(KERN_ERR "fldb: Failed to claim RESET_OUTN PIO!\n");
	}

	/* The IDSEL line is connected to PIO16.2 only... Luckily
	 * there is just one slot, so we can just force 1... */
	if (gpio_request(FLDB_PIO_PCI_IDSEL, "PCI_IDSEL") == 0)
		gpio_direction_output(FLDB_PIO_PCI_IDSEL, 1);
	else
		printk(KERN_ERR "fldb: Failed to claim PCI_IDSEL PIO!\n");

	/* And finally! */
	fli7510_configure_pci(&fldb_pci_config);

	fli7510_configure_pwm(&(struct fli7510_pwm_config) {
			.out0_enabled = 1,
#if 0
			/* Connected to DF1 LED, currently used as a
			 * GPIO-controlled one (see above) */
			.out1_enabled = 1,
#endif
#if 0
			/* PWM driver doesn't support these yet... */
			.out2_enabled = 1,
			.out3_enabled = 1,
#endif
			});

	/* CNB2 ("I2C1" connector), CNJ2 ("FE Board" connector) */
	fli7510_configure_ssc_i2c(0);
	/* CNB3 ("I2C2" connector), UB2 (EEPROM), UH4 (STM8 uC),
	 * CNJ2 ("FE Board" connector) */
	fli7510_configure_ssc_i2c(1);
	/* CNB5 ("I2C3" connector), CNF3 ("LVDS Out C and D" connector),
	 * CNL1 ("Extension Board" connector) */
	fli7510_configure_ssc_i2c(2);
	/* CNK4 ("VGA In" connector), UK1 (EEPROM) */
	fli7510_configure_ssc_i2c(3);
	/* Leave SSC4 unconfigured, using SPI-FSM for Serial Flash */

	fli7510_configure_spifsm(&fldb_spifsm_flash);

	fli7510_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &fldb_nand_flash,
			.rbn.flex_connected = 1,});

	fli7510_configure_usb(0, &(struct fli7510_usb_config) {
			.ovrcur_mode = fli7510_usb_ovrcur_active_low, });

	fli7510_configure_ethernet(&(struct fli7510_ethernet_config) {
			.mode = fli7510_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = 1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	fli7510_configure_lirc();

	/*
	 * To use the MMC/SD card with the external
	 * CNG6 connector, the CNG4 has to be connected to CNG3.
	 */
	fli7510_configure_mmc();

	fli7510_configure_audio(&(struct fli7510_audio_config) {
			.pcm_player_0_output_mode =
					fli7510_pcm_player_0_output_8_channels,
			.spdif_player_output_enabled = 1, });

	return platform_add_devices(fldb_devices,
			ARRAY_SIZE(fldb_devices));
}
arch_initcall(fldb_device_init);



static void __iomem *fldb_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_fldb __initmv = {
	.mv_name	= "fldb",
	.mv_setup	= fldb_setup,
	.mv_nr_irqs	= NR_IRQS,
	.mv_ioport_map	= fldb_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
