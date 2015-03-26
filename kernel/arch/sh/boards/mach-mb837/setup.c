/*
 * arch/sh/boards/mach-mb837/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STi7108 processor board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c/pcf857x.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/spi/spi_gpio.h>
#include <linux/stm/pci-glue.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <asm/irq.h>
#include <mach/common.h>
#include "../mach-st/mb705-epld.h"

/* PCF8575 I2C PIO Extender (IC12) */
#define PIO_EXTENDER_BASE 220
#define PIO_EXTENDER_GPIO(port, pin) (PIO_EXTENDER_BASE + (port * 8) + (pin))



#define MB837_NOTPIORESETMII0 PIO_EXTENDER_GPIO(1, 1)
#define MB837_NOTPIORESETMII1 PIO_EXTENDER_GPIO(1, 2)



static void __init mb837_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STi7108-MBOARD (mb837) "
			"initialisation\n");

	stx7108_early_device_init();

	stx7108_configure_asc(2, &(struct stx7108_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio21_0,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio21_1,
			.routing.asc3.cts = stx7108_asc3_cts_pio21_4,
			.routing.asc3.rts = stx7108_asc3_rts_pio21_3,
			.hw_flow_control = 1, });
}



static struct platform_device mb837_led = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "Heartbeat (LD6)", /* J23 1-2 */
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(5, 4),
			},
		},
	},
};



#ifdef CONFIG_SH_ST_MB837_STMMAC0
/* J14-B must be fitted */
static int mb837_mii0_phy_reset(void *bus)
{
	static int requested;

	if (!requested && gpio_request(MB837_NOTPIORESETMII0,
				"notPioResetMii0") == 0) {
		gpio_direction_output(MB837_NOTPIORESETMII0, 1);
		requested = 1;
	} else {
		pr_warning("mb837: Failed to request notPioResetMii0!\n");
	}

	if (requested) {
		gpio_set_value(MB837_NOTPIORESETMII0, 0);
		udelay(15000);
		gpio_set_value(MB837_NOTPIORESETMII0, 1);
	}

	return 0;
}

static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	.bus_id = 0,
	.phy_reset = mb837_mii0_phy_reset,
	.phy_mask = 0,
};
#endif


#ifdef CONFIG_SH_ST_MB837_STMMAC1
/* J14-A must be fitted */
static int mb837_mii1_phy_reset(void *bus)
{
	static int requested;

	if (!requested && gpio_request(MB837_NOTPIORESETMII1,
				"notPioResetMii1") == 0) {
		gpio_direction_output(MB837_NOTPIORESETMII1, 1);
		requested = 1;
	} else {
		pr_warning("mb837: Failed to request notPioResetMii1!\n");
	}

	if (requested) {
		gpio_set_value(MB837_NOTPIORESETMII1, 0);
		udelay(15000);
		gpio_set_value(MB837_NOTPIORESETMII1, 1);
	}

	return 0;
}

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.bus_id = 1,
	.phy_reset = mb837_mii1_phy_reset,
	.phy_mask = 0,
};
#endif

/* PCF8575 I2C PIO Extender (IC12) */
static struct i2c_board_info mb837_pio_extender = {
	I2C_BOARD_INFO("pcf857x", 0x27),
	.type = "pcf8575",
	.platform_data = &(struct pcf857x_platform_data) {
		.gpio_base = PIO_EXTENDER_BASE,
	},
};



static struct platform_device *mb837_devices[] __initdata = {
	&mb837_led,
};



static struct stm_plat_pci_config mb837_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_DEFAULT,
		[3] = PCI_PIN_DEFAULT,
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
	.pci_reset_gpio = -EINVAL,
};

#ifdef CONFIG_SH_ST_MB705
void __init mbxxx_configure_nand_flash(struct stm_nand_config *config)
{
	stx7108_configure_nand(config);
}

/* SPI PIO Bus for Serial Flash on mb705 peripheral board:
 *	CLK	J42-F closed (J42-E open)
 *	CSn	J42-H closed (J42-G open)
 *      DIn	J30-A closed
 *	DOut	J30-B closed
 * Note, leave SSC0 (I2C MII1) unconfigured  */
static struct platform_device mb837_serial_flash_bus = {
	.name           = "spi_gpio",
	.id             = 8,
	.num_resources  = 0,
	.dev.platform_data = &(struct spi_gpio_platform_data) {
		.num_chipselect = 1,
		.sck = stm_gpio(1, 6),
		.mosi = stm_gpio(2, 1),
		.miso = stm_gpio(2, 0),
	}
};

void __init mbxxx_configure_serial_flash(struct spi_board_info *serial_flash)
{
	/* Specify CSn and SPI bus */
	serial_flash->bus_num = 8;
	serial_flash->controller_data = (void *)stm_gpio(1, 7);

	/* Register SPI bus and flash devices */
	platform_device_register(&mb837_serial_flash_bus);
	spi_register_board_info(serial_flash, 1);
}
#endif



int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	/* We can use the standard function on this board */
	return stx7108_pcibios_map_platform_irq(&mb837_pci_config, pin);
}



void __init mbxxx_configure_audio_pins(int *pcm_reader, int *pcm_player)
{
	*pcm_reader = -1;
	*pcm_player = 2;
	stx7108_configure_audio(&(struct stx7108_audio_config) {
		.pcm_player_2_output = stx7108_pcm_player_2_output_8_channels,
		.spdif_player_output_enabled = 1,
		.pcm_reader_input_enabled = 1, });
}



static int __init mb837_devices_init(void)
{
	int ssc2_i2c;

	/* PCI jumper settings:
	 * J18 not fitted, J19 2-3, J30-G fitted, J34-B fitted,
	 * J35-A fitted, J35-B not fitted, J35-C fitted, J35-D not fitted,
	 * J39-E fitted, J39-F not fitted */
	stx7108_configure_pci(&mb837_pci_config);

#ifndef CONFIG_SH_ST_MB705
	/* MII1 & TS Connectors (inc. Cable Card one) - J42E & J42G */
	stx7108_configure_ssc_i2c(0, NULL);
#endif

	/* STRec to going to MB705 - J41C & J41D */
	stx7108_configure_ssc_i2c(1, NULL);
	/* MII0 & PIO Extender - J42A & J42C */
	ssc2_i2c = stx7108_configure_ssc_i2c(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio1_3,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio1_4, });
	/* NIM AB - J25 1-2 & J27 1-2 */
	stx7108_configure_ssc_i2c(3, NULL);
	/* NIM CDI - J24 1-2 & J26 1-2 */
	stx7108_configure_ssc_i2c(5, NULL);
	/* HDMI - J55C & J55D */
	stx7108_configure_ssc_i2c(6, NULL);

	/* PIO extender is connected to SSC2 */
	i2c_register_board_info(ssc2_i2c, &mb837_pio_extender, 1);

	stx7108_configure_lirc(&(struct stx7108_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stx7108_lirc_rx_mode_uhf,
#else
			.rx_mode = stx7108_lirc_rx_mode_ir,
#endif
			.tx_enabled = 1,
			.tx_od_enabled = 1, });

	/* J56A & J56B fitted */
	stx7108_configure_usb(0);
	/* J53A & J53B fitted */
	stx7108_configure_usb(1);
	/* J53C & J53D fitted */
	stx7108_configure_usb(2);

	if (cpu_data->cut_major >= 2) {
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

#ifdef CONFIG_SH_ST_MB837_STMMAC0
	stx7108_configure_ethernet(0, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac0_mdio_bus,
		});
#endif
#ifdef CONFIG_SH_ST_MB837_STMMAC1
	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 1,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac1_mdio_bus,
		});
#endif

	/* MMC Hardware settings:
	 * - Jumpers to be set:
	 * 	J52[A..H] , J51[A..C] , J51-D, J42-B and J42-D
	 * - Jumpers to be unset: J42-A and J42-C
	 * HW workaround on board:
	 * MMC lines pull-up resistors
	 * In mb837 v1.0 schematic, the following pull-up values are wrong:
	 * R(MMC_Cmd) = R258-2 = 61k and  R(MMC_Data2) = R261 = 10k.
	 * They should be replaced by: R(MMC_Data2) = 61k R(MMC_Cmd) = 10k.
	 */
	stx7108_configure_mmc(0);

	return platform_add_devices(mb837_devices, ARRAY_SIZE(mb837_devices));
}
arch_initcall(mb837_devices_init);



static void __iomem *mb837_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might
	 * think.  I used to use external ROM, but that can cause
	 * problems if you are in the middle of updating Flash. So I'm
	 * now using the processor core version register, which is
	 * guaranteed to be available, and non-writable. */
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_mb837 __initmv = {
	.mv_name		= "mb837",
	.mv_setup		= mb837_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= mb837_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
