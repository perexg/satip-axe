/*
 * arch/sh/boards/mach-fudb/setup.c
 *
 * Copyright (C) 2010-2011 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics Freeman Ultra Development Board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/platform.h>
#include <linux/stm/fli7510.h>
#include <linux/stm/pci-glue.h>
#include <asm/irq-ilc.h>
#include <sound/stm.h>

#define FUDB_PIO_RESET_OUTN stm_gpio(11, 5)


static void __init fudb_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics Freeman Ultra Development Board "
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



static struct platform_device fudb_led_df1 = {
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

static struct platform_device *fudb_devices[] __initdata = {
	&fudb_led_df1,
};


/* Serial Flash */
static struct stm_plat_spifsm_data fudb_spifsm_flash = {
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
static struct stm_nand_bank_data fudb_nand_flash = {
	.csn		= 0,
	.options	= NAND_USE_FLASH_BBT,
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


static int __init fudb_device_init(void)
{

	/* This is a board-level reset line, which goes to the
	 * Ethernet PHY, audio amps & number of extension connectors */
	if (gpio_request(FUDB_PIO_RESET_OUTN, "RESET_OUTN") == 0) {
		gpio_direction_output(FUDB_PIO_RESET_OUTN, 0);
		udelay(10000); /* 10ms is the Ethernet PHY requirement */
		gpio_set_value(FUDB_PIO_RESET_OUTN, 1);
	} else {
		printk(KERN_ERR "fudb: Failed to claim RESET_OUTN PIO!\n");
	}

	fli7510_configure_pwm(&(struct fli7510_pwm_config) {
#if 0
			/* PWM driver doesn't support these yet... */
			.out2_enabled = 1,
			.out3_enabled = 1,
#endif
			});

	/* CNB2 ("I2C1" connector), CNJ2 ("FE Board" connector) */
	fli7510_configure_ssc_i2c(0);
	/* CNB3 ("I2C2" connector), UB4 (EEPROM), UH4 (STM8 uC) */
	fli7510_configure_ssc_i2c(1);
	/* CNB5 ("I2C3" connector), CND1 ("Mini PCI Express" connector),
	 * CNF3 ("LVDS Out" connector), CNL1 ("Extension Board" connector) */
	fli7510_configure_ssc_i2c(2);
	/* CNK4 ("VGA In" connector), UK1 (EEPROM) */
	fli7510_configure_ssc_i2c(3);
	/* Leave SSC4 unconfigured, using SPI-FSM for Serial Flash */

	fli7510_configure_spifsm(&fudb_spifsm_flash);

	fli7540_configure_pcie(&(struct fli7540_pcie_config) {
			.reset_gpio = stm_gpio(8, 4),
			});

	fli7510_configure_usb(0, &(struct fli7510_usb_config) {
			.ovrcur_mode = fli7510_usb_ovrcur_active_low, });
	fli7510_configure_usb(1, &(struct fli7510_usb_config) {
			.ovrcur_mode = fli7510_usb_ovrcur_active_low, });

	fli7510_configure_ethernet(&(struct fli7510_ethernet_config) {
			.mode = fli7510_ethernet_mode_rmii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = 1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	fli7510_configure_lirc();

	fli7510_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &fudb_nand_flash,
			.rbn.flex_connected = 1,});

	fli7510_configure_mmc();

	fli7510_configure_audio(&(struct fli7510_audio_config) {
			.pcm_player_0_output_mode =
					fli7510_pcm_player_0_output_8_channels,
			.spdif_player_output_enabled = 1, });

	return platform_add_devices(fudb_devices,
			ARRAY_SIZE(fudb_devices));
}
arch_initcall(fudb_device_init);

#ifdef CONFIG_PCI
int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	return stm_pci_legacy_irq(dev);
}
#endif



static void __iomem *fudb_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_fudb __initmv = {
	.mv_name	= "fudb",
	.mv_setup	= fudb_setup,
	.mv_nr_irqs	= NR_IRQS,
	.mv_ioport_map	= fudb_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
