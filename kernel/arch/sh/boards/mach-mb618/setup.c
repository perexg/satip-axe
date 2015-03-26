/*
 * arch/sh/boards/st/mb618/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7111 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
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
#include <asm/irl.h>
#include <sound/stm.h>
#include <mach/common.h>



/* Whether the hardware supports NOR or NAND Flash depends on J34.
 * In position 1-2 CSA selects NAND, in position 2-3 is selects NOR.
 * Note that J30A must be in position 2-3 to select the on board Flash
 * (both NOR and NAND).
 */
#define FLASH_NOR
#define MB618_PIO_FLASH_VPP stm_gpio(3, 4)



static void __init mb618_setup(char** cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx7111 Mboard initialisation\n");

	stx7111_early_device_init();

	stx7111_configure_asc(2, &(struct stx7111_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7111_configure_asc(3, &(struct stx7111_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}



static struct platform_device mb618_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB green",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(6, 0),
			}, {
				.name = "HB red",
				.gpio = stm_gpio(6, 1),
			},
		},
	},
};



static struct gpio_keys_button mb618_buttons[] = {
	{
		.code = BTN_0,
		.gpio = stm_gpio(6, 2),
		.desc = "SW2",
	}, {
		.code = BTN_1,
		.gpio = stm_gpio(6, 3),
		.desc = "SW3",
	}, {
		.code = BTN_2,
		.gpio = stm_gpio(6, 4),
		.desc = "SW4",
	}, {
		.code = BTN_3,
		.gpio = stm_gpio(6, 5),
		.desc = "SW5",
	},
};

static struct platform_device mb618_button_device = {
	.name = "gpio-keys",
	.id = -1,
	.num_resources = 0,
	.dev.platform_data = &(struct gpio_keys_platform_data) {
		.buttons = mb618_buttons,
		.nbuttons = ARRAY_SIZE(mb618_buttons),
	},
};



#ifdef FLASH_NOR
/* J34 must be in the 1-2 position to enable NOR Flash */
static void mb618_nor_set_vpp(struct map_info *info, int enable)
{
	gpio_set_value(MB618_PIO_FLASH_VPP, enable);
}

static struct platform_device mb618_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 32*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= mb618_nor_set_vpp,
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
#else
struct stm_nand_bank_data mb618_nand_flash = {
	.csn		= 0,
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
		.wr_on		= 20,
		.wr_off		= 50,
		.rd_on		= 20,
		.rd_off		= 50,
		.chip_delay	= 50,		/* in us */
	},
};
#endif

/* Serial Flash (Board Rev D and later) */
static struct spi_board_info mb618_serial_flash = {
	.modalias       = "m25p80",
	.bus_num        = 0,
	.chip_select    = stm_gpio(6, 7),
	.max_speed_hz   = 7000000,
	.mode           = SPI_MODE_3,
	.platform_data  = &(struct flash_platform_data) {
		.name = "m25p80",
		.type = "m25p80",
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

static int mb618_phy_reset(void *bus)
{
	epld_write(1, 0);	/* bank = Ctrl */

	/* Bring the PHY out of reset in MII mode */
	epld_write(0x4 | 0, 4);
	epld_write(0x4 | 1, 4);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = mb618_phy_reset,
	.phy_mask = 0,
};

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x06000000,
			/* Minimum size to ensure mapped by PMB */
			.end	= 0x06000000+(8*1024*1024)-1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 8,
	},
};



static struct stm_plat_pci_config mb618_pci_config = {
	/* We don't bother with INT[BCD] as they are shared with the ssc
	 * J20-A must be removed, J20-B must be 5-6 */
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_DEFAULT, /* J32-F fitted */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = -EINVAL,	/* Reset done by EPLD on power on */
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return stx7111_pcibios_map_platform_irq(&mb618_pci_config, pin);
}

static struct platform_device *mb618_devices[] __initdata = {
	&mb618_leds,
	&epld_device,
#ifdef FLASH_NOR
	&mb618_nor_flash,
#endif
	&mb618_button_device,
};

/* SCART switch simple control */

/* Enable CVBS output to both (TV & VCR) SCART outputs */
static int mb618_scart_audio_init(struct i2c_client *client, void *priv)
{
	const char cmd[] = { 0x2, 0x11 };
	int cmd_len = sizeof(cmd);

	return i2c_master_send(client, cmd, cmd_len) != cmd_len;
}

/* Audio on SCART outputs control */
static struct i2c_board_info mb618_scart_audio __initdata = {
	I2C_BOARD_INFO("snd_conv_i2c", 0x4b),
	.type = "STV6417",
	.platform_data = &(struct snd_stm_conv_i2c_info) {
		.group = "Analog Output",
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
		.init = mb618_scart_audio_init,
		.enable_supported = 1,
		.enable_cmd = (char []){ 0x01, 0x09 },
		.enable_cmd_len = 2,
		.disable_cmd = (char []){ 0x01, 0x00 },
		.disable_cmd_len = 2,
	},
};

static int __init mb618_devices_init(void)
{
	int peripherals_i2c_bus;

	stx7111_configure_pci(&mb618_pci_config);

	stx7111_configure_pwm(&(struct stx7111_pwm_config) {
			.out0_enabled = 1,
			.out1_enabled = 0, });

	stx7111_configure_ssc_spi(0, NULL);
	stx7111_configure_ssc_i2c(1); /* J12=1-2, J16=1-2 */
	peripherals_i2c_bus = stx7111_configure_ssc_i2c(2);
	stx7111_configure_ssc_i2c(3);

	stx7111_configure_usb(&(struct stx7111_usb_config) {
			.invert_ovrcur = 1, });

	stx7111_configure_ethernet(&(struct stx7111_ethernet_config) {
			.mode = stx7111_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	stx7111_configure_lirc(&(struct stx7111_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stx7111_lirc_rx_mode_uhf,
#else
			.rx_mode = stx7111_lirc_rx_mode_ir,
#endif
			.tx_enabled = 1,
			.tx_od_enabled = 0, });

	gpio_request(MB618_PIO_FLASH_VPP, "Flash VPP");
	gpio_direction_output(MB618_PIO_FLASH_VPP, 0);

	i2c_register_board_info(peripherals_i2c_bus, &mb618_scart_audio, 1);
	spi_register_board_info(&mb618_serial_flash, 1);

#ifndef FLASH_NOR
	stx7111_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &mb618_nand_flash,
			.rbn.flex_connected = 1,});

	/* The MTD NAND code doesn't understand the concept of VPP,
	 * (or hardware write protect) so permanently enable it. */
	gpio_direction_output(MB618_PIO_FLASH_VPP, 1);
#endif

	return platform_add_devices(mb618_devices, ARRAY_SIZE(mb618_devices));
}
arch_initcall(mb618_devices_init);

static void __iomem *mb618_ioport_map(unsigned long port, unsigned int size)
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

/*
 * We now only support version 6 or later EPLDs:
 *
 * off        read       write         reset
 *  0         Ident      Bank          46 (Bank register defaults to 0)
 *  4 bank=0  Test       Test          55
 *  4 bank=1  Ctrl       Ctrl          0e
 *  4 bank=2  IntPri0    IntPri0  f9
 *  4 bank=3  IntPri1    IntPri1  f0
 *  8         IntStat    IntMaskSet    -
 *  c         IntMask    IntMaskClr    00
 *
 * Ctrl register bits:
 *  0 = Ethernet Phy notReset
 *  1 = RMIInotMIISelect
 *  2 = Mode Select_7111 (ModeSelect when D0 == 1)
 *  3 = Mode Select_8700 (ModeSelect when D0 == 0)
 */

static void __init mb618_init_irq(void)
{
	unsigned char epld_reg;
	const int test_offset = 4;
	const int version_offset = 0;
	int version;

	epld_early_init(&epld_device);

	epld_write(0, 0);	/* bank = Test */
	epld_write(0x63, test_offset);
	epld_reg = epld_read(test_offset);
	if (epld_reg != (unsigned char)(~0x63)) {
		printk(KERN_WARNING
		       "Failed mb618 EPLD test (off %02x, res %02x)\n",
		       test_offset, epld_reg);
		return;
	}

	version = epld_read(version_offset) & 0x1f;
	printk(KERN_INFO "mb618 EPLD version %02d\n", version);

	/*
	 * We have the nice new shiny interrupt system at last.
	 * For the moment just replicate the functionality to
	 * route the STEM interrupt through.
	 */

	/* Route STEM Int0 (EPLD int 4) to output 2 */
	epld_write(3, 0);	/* bank = IntPri1 */
	epld_reg = epld_read(4);
	epld_reg &= 0xfc;
	epld_reg |= 2;
	epld_write(epld_reg, 4);

	/* Enable it */
	epld_write(1<<4, 8);
}

struct sh_machine_vector mv_mb618 __initmv = {
	.mv_name		= "STx7111 Mboard",
	.mv_setup		= mb618_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb618_init_irq,
	.mv_ioport_map		= mb618_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
