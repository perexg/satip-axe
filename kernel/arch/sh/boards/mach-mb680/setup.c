/*
 * arch/sh/boards/st/mb680/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7105 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/lirc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/pci-glue.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi_gpio.h>
#include <asm/irq-ilc.h>
#include <mach/common.h>
#include "../mach-st/mb705-epld.h"



#define MB680_PIO_PHY_RESET stm_gpio(5, 5)
#define MB680_PIO_PCI_SERR stm_gpio(6, 4)
#define MB680_PIO_PCI_RESET stm_gpio(15, 6)
#define MB680_PIO_MII_BUS_SWITCH stm_gpio(11, 2)



static void __init mb680_setup(char** cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx7105 Mboard initialisation\n");

	stx7105_early_device_init();

	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7105_configure_asc(3, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}



static struct platform_device mb680_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "LD5",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(2, 4),
			}, {
				.name = "LD6",
				.gpio = stm_gpio(2, 3),
			},
		},
	},
};

/*
 * mb680 rev C added software control of the PHY reset, and buffers which
 * allow isolation of the MII pins so that their use as MODE pins is not
 * compromised by the PHY.
 */

/*
 * When connected to the mb705, MII reset is controlled by an EPLD register
 * on the mb705.
 * When used standalone a PIO pin is used, and J47-C must be fitted.
 *
 * Timings:
 *    PHY         | Reset low | Post reset stabilisation
 *    ------------+-----------+-------------------------
 *    DB83865     |   150uS   |         20mS
 *    LAN8700     |   100uS   |         800nS
 */
#ifdef CONFIG_SH_ST_MB705
static void ll_phy_reset(void)
{
	mb705_reset(EPLD_EMI_RESET_SW0, 150);
}
#else
static void ll_phy_reset(void)
{
	gpio_set_value(MB680_PIO_PHY_RESET, 0);
	udelay(150);
	gpio_set_value(MB680_PIO_PHY_RESET, 1);
}
#endif

static int mb680_phy_reset(void *bus)
{
	gpio_set_value(MB680_PIO_MII_BUS_SWITCH, 1);
	ll_phy_reset();
	gpio_set_value(MB680_PIO_MII_BUS_SWITCH, 0);
	mdelay(20);

	return 0;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = mb680_phy_reset,
	.phy_mask = 0,
};

static struct platform_device *mb680_devices[] __initdata = {
	&mb680_leds,
};

/* PCI configuration */

#ifdef CONFIG_SH_ST_MB705
static void mb705_epld_pci_reset(void)
{
	mb705_reset(EPLD_EMI_RESET_SW1, 1000);

	/* PCI spec says one second */
	mdelay(10);
}
#endif

/*
 * J22-A must be removed, J22-B must be 2-3.
 */
static struct stm_plat_pci_config mb680_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_DEFAULT,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED, /* Modified in mb680_device_init() below */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	/* When connected to the mb705, PCI reset is controlled by an EPLD
	 * register on the mb705. When used standalone a PIO pin is used,
	 * and J47-D, J9-G must be fitted. */
#ifdef CONFIG_SH_ST_MB705
	.pci_reset = mb705_epld_pci_reset,
#else
	.pci_reset_gpio = MB680_PIO_PCI_RESET,
#endif
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return stx7105_pcibios_map_platform_irq(&mb680_pci_config, pin);
}

void __init mbxxx_configure_audio_pins(int *pcm_reader, int *pcm_player)
{
	*pcm_reader = -1;
	*pcm_player = 0;
	stx7105_configure_audio(&(struct stx7105_audio_config) {
			.pcm_player_0_output =
					stx7105_pcm_player_0_output_6_channels,
			.spdif_player_output_enabled = 1,
			.pcm_reader_input_enabled =
					cpu_data->type == CPU_STX7105, });
}

#ifdef CONFIG_SH_ST_MB705
void __init mbxxx_configure_nand_flash(struct stm_nand_config *config)
{
	stx7105_configure_nand(config);
}

/* GPIO based SPI */
static struct platform_device mb680_serial_flash_bus = {
	.name           = "spi_gpio",
	.id             = 8,
	.num_resources  = 0,
	.dev.platform_data = &(struct spi_gpio_platform_data) {
		.sck = stm_gpio(15, 0),
		.mosi = stm_gpio(15, 1),
		.miso = stm_gpio(15, 3),
		.num_chipselect = 1,
	},
};

void __init mbxxx_configure_serial_flash(struct spi_board_info *serial_flash)
{
	/* Specify CSn and SPI bus */
	serial_flash->bus_num = 8;
	serial_flash->controller_data = (void *)stm_gpio(15, 2);

	/* Register SPI bus and flash devices */
	platform_device_register(&mb680_serial_flash_bus);
	spi_register_board_info(serial_flash, 1);
}
#endif

static int __init mb680_devices_init(void)
{
	/* Setup the PCI_SERR# PIO
	 * J20-A - open, J27-E - closed */
	if (gpio_request(MB680_PIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(MB680_PIO_PCI_SERR);
		mb680_pci_config.serr_irq = gpio_to_irq(MB680_PIO_PCI_SERR);
		set_irq_type(mb680_pci_config.serr_irq, IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_WARNING "mb680: Failed to claim PCI_SERR PIO!\n");
	}
	stx7105_configure_pci(&mb680_pci_config);

	stx7105_configure_sata(0);

	/* Valid only for mb680 rev. A & rev. B (they had two SATA lines) */
	stx7105_configure_sata(1);

	stx7105_configure_pwm(&(struct stx7105_pwm_config) {
			.out0 = stx7105_pwm_out0_pio13_0,
			.out1 = stx7105_pwm_out1_disabled, });

	/* NIM CD I2C bus*/
	stx7105_configure_ssc_i2c(1, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc1_sclk_pio2_5,
			.routing.ssc1.mtsr = stx7105_ssc1_mtsr_pio2_6, });
	/* NIM AB/STRecord I2C bus*/
	stx7105_configure_ssc_i2c(2, &(struct stx7105_ssc_config) {
			.routing.ssc2.sclk = stx7105_ssc2_sclk_pio3_4,
			.routing.ssc2.mtsr = stx7105_ssc2_mtsr_pio3_5, });
	/* HDMI I2C bus */
	stx7105_configure_ssc_i2c(3, &(struct stx7105_ssc_config) {
			.routing.ssc3.sclk = stx7105_ssc3_sclk_pio3_6,
			.routing.ssc3.mtsr = stx7105_ssc3_mtsr_pio3_7, });

	/*
	 * Note that USB port configuration depends on jumper
	 * settings:
	 *
	 *	  PORT 0	       		PORT 1
	 *	+-----------------------------------------------------------
	 * norm	|  4[4]	J5A:2-3			 4[6]	J10A:2-3
	 * alt	| 12[5]	J5A:1-2  J6F:open	14[6]	J10A:1-2  J11G:open
	 * norm	|  4[5]	J5B:2-3			 4[7]	J10B:2-3
	 * alt	| 12[6]	J5B:1-2  J6G:open	14[7]	J10B:1-2  J11H:open
	 */
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

	gpio_request(MB680_PIO_PHY_RESET, "notPioResetMII");
	gpio_direction_output(MB680_PIO_PHY_RESET, 1);
	gpio_request(MB680_PIO_MII_BUS_SWITCH, "MIIBusSwitchnotOE");
	gpio_direction_output(MB680_PIO_MII_BUS_SWITCH, 1);

	stx7105_configure_ethernet(0, &(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	/*
	 * Check jumpers before using IR:
	 * On the mb705:
	 *	J25A : 1-2
	 *	J25B : 1-2 (UHF), 2-3 (IR)
	 * On the mb680:
	 *	J15A : fitted
	 */
	stx7105_configure_lirc(&(struct stx7105_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stx7105_lirc_rx_mode_uhf,
#else
			.rx_mode = stx7105_lirc_rx_mode_ir,
#endif
			.tx_enabled = 1,
			.tx_od_enabled = 1, });

	return platform_add_devices(mb680_devices, ARRAY_SIZE(mb680_devices));
}
arch_initcall(mb680_devices_init);

static void __iomem *mb680_ioport_map(unsigned long port, unsigned int size)
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

static void __init mb680_init_irq(void)
{
#ifndef CONFIG_SH_ST_MB705
	/* Configure STEM interrupts as active low. */
	set_irq_type(ILC_EXT_IRQ(1), IRQ_TYPE_LEVEL_LOW);
	set_irq_type(ILC_EXT_IRQ(2), IRQ_TYPE_LEVEL_LOW);
#endif
}

struct sh_machine_vector mv_mb680 __initmv = {
	.mv_name		= "STx7105 Mboard",
	.mv_setup		= mb680_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb680_init_irq,
	.mv_ioport_map		= mb680_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
