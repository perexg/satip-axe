/*
 * arch/sh/boards/st/mach-hdk7111/setup.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: John Boddie (john.boddie@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7111 HDK board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/lirc.h>
#include <linux/gpio.h>
#include <linux/phy.h>
#include <linux/input.h>
#include <linux/stm/platform.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/stm/stx7111.h>
#include <linux/stm/emi.h>
#include <linux/stm/pci-glue.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <asm/irq-ilc.h>


#define HDK7111_PIO_PHY_RESET stm_gpio(1, 6)

/* The hdk7111 board is populated with NOR, NAND, and Serial Flash.  The setup
 * below assumes the board is in its default boot-from-NOR configuration.  Other
 * boot configurations are possible but require board-level modifications to be
 * made, and equivalent changes to the setup here.  Note, only boot-from-NOR has
 * been fully tested.
 */

static void __init hdk7111_setup(char** cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx7111 HDK initialisation\n");

	stx7111_early_device_init();

	stx7111_configure_asc(2, &(struct stx7111_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });

}

static struct platform_device hdk7111_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB red",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 0),
			},
		},
	},
};

static struct gpio_keys_button hdk7111_buttons[] = {
	{
		.code = BTN_0,
		.gpio = stm_gpio(6, 4),
		.desc = "SW1",
	},
	{
		.code = BTN_1,
		.gpio = stm_gpio(6, 5),
		.desc = "SW2",
	},
	{
		.code = BTN_2,
		.gpio = stm_gpio(6, 6),
		.desc = "SW3",
	},
};

static struct gpio_keys_platform_data hdk7111_button_data = {
	.buttons = hdk7111_buttons,
	.nbuttons = ARRAY_SIZE(hdk7111_buttons),
};

static struct platform_device hdk7111_button_device = {
	.name = "gpio-keys",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &hdk7111_button_data,
	}
};

static struct platform_device hdk7111_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 32*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
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
			},
		},


	},
};

struct stm_nand_bank_data hdk7111_nand_flash = {
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
	.timing_data	= &(struct stm_nand_timing_data) {
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

static struct spi_board_info hdk7111_serial_flash_board_info =  {
	.modalias       = "m25p80",
	.bus_num	= 0,
	.max_speed_hz   = 7000000,
	.chip_select	= stm_gpio(6, 7),
	.mode		= SPI_MODE_3,
	.platform_data  = &(struct flash_platform_data) {
		.name = "m25p80",
		.type = "m25p16",
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

static int hdk7111_phy_reset(void *bus)
{
	gpio_set_value(HDK7111_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(HDK7111_PIO_PHY_RESET, 1);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = hdk7111_phy_reset,
	.phy_mask = 0,
};


#define HDK7111_PCI_IDSEL stm_gpio(4, 4)
#define HDK7111_PCI_SERR_IRQ ILC_EXT_IRQ(1)

static struct stm_plat_pci_config hdk7111_pci_config = {
	.pci_irq = {
		/* Bizarre irq usage */
		[0] = PCI_PIN_UNUSED,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_DEFAULT,
		[3] = PCI_PIN_UNUSED,
	},
	.serr_irq = HDK7111_PCI_SERR_IRQ,
	.idsel_lo = 30, /* Actually unused, connected to PIO */
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = stm_gpio(3, 7),
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	int ret = -EINVAL;

	/* Wacky interrupt routing, INTA is actually connected to
	 * what is usually INTC!
	 */
	if (pin == 1)
		ret = evt2irq(0xa40);
	else if (pin == 2)
		ret =  evt2irq(0xa20);

	return ret;
}

static struct platform_device *hdk7111_devices[] __initdata = {
	&hdk7111_leds,
	&hdk7111_button_device,
	&hdk7111_nor_flash,
};

static int __init hdk7111_devices_init(void)
{
	struct sysconf_field *sc;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/* Configure Flash according to boot-device */
	sc = sysconf_claim(SYS_STA, 1, 16, 17, "boot_device");
	switch (sysconf_read(sc)) {
	case 0x0:
		/* Boot-from-NOR: */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(1) - nor_bank_base;
		hdk7111_nand_flash.csn = 1;
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7111_nand_flash.csn = 0;
		break;
	case 0x2:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		/* NOR mapped to EMIB, with physical offset of 0x06000000! */
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7111_nand_flash.csn = 0;
		break;
	default:
		BUG();
		break;
	}
	sysconf_release(sc);

	/* Update NOR Flash base address and size: */
	/*     - reduce visibility of NOR flash to EMI bank size */
	if (hdk7111_nor_flash.resource[0].end > nor_bank_size - 1)
		hdk7111_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	hdk7111_nor_flash.resource[0].start += nor_bank_base;
	hdk7111_nor_flash.resource[0].end += nor_bank_base;

	stx7111_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &hdk7111_nand_flash,
			.rbn.flex_connected = 1,});

	spi_register_board_info(&hdk7111_serial_flash_board_info, 1);

	/* The hdk board is another board where the IDSEL line is erroneously
	 * connected to a PIO rather than to the address lines. Since there is
	 * only one card, we just claim it and drive it high permanently, so
	 * that card is always selected for config cycles.
	 */
	if (!gpio_request(HDK7111_PCI_IDSEL, "pci idsel"))
		gpio_direction_output(HDK7111_PCI_IDSEL, 1);
	else
		pr_err("Unable to claim IDSEL PCI signal\n");
	/* The SERR interrupt is connected to the external IRQ pins */
	set_irq_type(HDK7111_PCI_SERR_IRQ, IRQ_TYPE_LEVEL_LOW);
	stx7111_configure_pci(&hdk7111_pci_config);

	stx7111_configure_pwm(&(struct stx7111_pwm_config) {
				.out0_enabled = 1,
				.out1_enabled = 0,
				});

	stx7111_configure_ssc_spi(0, NULL);
	stx7111_configure_ssc_i2c(1);
	stx7111_configure_ssc_i2c(2);
	stx7111_configure_ssc_i2c(3);

	stx7111_configure_usb(&(struct stx7111_usb_config) {
				.invert_ovrcur = 1,
				});

	gpio_request(HDK7111_PIO_PHY_RESET, "eth_phy_reset");
	gpio_direction_output(HDK7111_PIO_PHY_RESET, 1);

	stx7111_configure_ethernet(&(struct stx7111_ethernet_config) {
				.mode = stx7111_ethernet_mode_mii,
				.ext_clk = 0,
				.phy_bus = 0,
				.phy_addr = 0,
				.mdio_bus_data = &stmmac_mdio_bus,
	      });

	stx7111_configure_lirc(&(struct stx7111_lirc_config) {
				.rx_mode = stx7111_lirc_rx_mode_ir,
				.tx_enabled = 1,
				.tx_od_enabled = 0,
				});


	return platform_add_devices(hdk7111_devices,
				    ARRAY_SIZE(hdk7111_devices));
}
arch_initcall(hdk7111_devices_init);

static void __iomem *hdk7111_ioport_map(unsigned long port, unsigned int size)
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

static void __init hdk7111_init_irq(void)
{
}

struct sh_machine_vector mv_hdk7111 __initmv = {
	.mv_name		= "STx7111 HDK",
	.mv_setup		= hdk7111_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= hdk7111_init_irq,
	.mv_ioport_map		= hdk7111_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
