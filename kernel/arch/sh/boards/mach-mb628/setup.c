/*
 * arch/sh/boards/st/mb628/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7141 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/stm/emi.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7141.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <mach/epld.h>
#include <sound/stm.h>
#include <mach/common.h>

/*
 * Flash setup depends on boot-device:
 *
 *                                    boot-from-XXX
 *                               NOR                NAND
 * ---------------------------------------------------------------------
 * J70   (CS routing)            2-3 (EMIA->NOR_CS) 1-2 (EMIA->NAND_CS)
 * SW8-4 (mode 14:bus width)     ON (16bit)         OFF (8bit)
 * SW9-1 (mode 16,17)            ON (boot NOR)      OFF (boot NAND)
 * SW9-2                         ON                 ON
 * ---------------------------------------------------------------------
 *
 * Notes: SW8-4 was found not to work correctly on early rev A boards.
 *        J69 must be in position 2-3 to enable the on-board Flash devices (both
 *        NOR and NAND) rather than STEM).
 *        J89 and J84 must be both in position 1-2 to avoid shorting A15.
 *        Board modifications are required to achieve boot from SPI (not
 *        supported here).
 *        Jumper settings based on board rev A
 */

static struct platform_device mb628_epld_device;

static void __init mb628_setup(char **cmdline_p)
{
	u8 test;

	printk(KERN_INFO "STMicroelectronics STx7141 Mboard initialisation\n");

	stx7141_early_device_init();

#ifndef CONFIG_SH_ST_MB628_STMMAC0
	/* Cannot use the ASC 1 when configure the GMAC0
	 * due to a PIO conflict */
	stx7141_configure_asc(1, &(struct stx7141_asc_config) {
			.routing.asc1 = stx7141_asc1_pio10,
			.hw_flow_control = 1,
			.is_console = 1, });
#endif
	stx7141_configure_asc(2, &(struct stx7141_asc_config) {
			.routing.asc2 = stx7141_asc2_pio6,
			.hw_flow_control = 1,
			.is_console = 0, });

	epld_early_init(&mb628_epld_device);

	epld_write(0xab, EPLD_TEST);
	test = epld_read(EPLD_TEST);
	printk(KERN_INFO "mb628 EPLD version %ld, test %s\n",
	       epld_read(EPLD_IDENT),
	       (test == (u8)(~0xab)) ? "passed" : "failed");
}

/* Serial Flash Chip Select
 *
 * SPI_CS is controlled via an EPLD register.  Here we create a virtual GPIO
 * device which maps onto the register.  This allows the default SPI chip select
 * function to be used.
 */
#define EPLD_SPI_CHIPSELECT_GPIO_BASE	200	/* Avoid clash with real PIOs */
static void epld_spi_chipselect_set(struct gpio_chip *chip, unsigned offset,
				   int value)
{
	u8 reg;

	reg = epld_read(EPLD_ENABLE);
	if (value)
		reg |= EPLD_ENABLE_SPI_NOTCS;
	else
		reg &= ~EPLD_ENABLE_SPI_NOTCS;

	epld_write(reg, EPLD_ENABLE);
}

static int epld_spi_chipselect_output(struct gpio_chip *chip, unsigned offset,
				      int value)
{
	epld_spi_chipselect_set(chip, offset, value);

	return 0;
}

static struct gpio_chip epld_spi_chipselect = {
	.set		= epld_spi_chipselect_set,
	.direction_output = epld_spi_chipselect_output,
	.base		= EPLD_SPI_CHIPSELECT_GPIO_BASE,
	.ngpio		= 1,
};

/* Serial Flash */
static struct spi_board_info mb628_serial_flash =  {
	.modalias	= "m25p80",
	.bus_num	= 0,
	.chip_select	= EPLD_SPI_CHIPSELECT_GPIO_BASE,
	.max_speed_hz	= 7000000,
	.mode		= SPI_MODE_3,
	.platform_data = &(struct flash_platform_data) {
		.name = "m25p80",
		.type = "m25p32",
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
	}
};

/* NOR Flash */
static void mb628_nor_set_vpp(struct map_info *info, int enable)
{
	epld_write((enable ? EPLD_FLASH_NOTWP : 0) | EPLD_FLASH_NOTRESET,
		   EPLD_FLASH);
}

static struct platform_device mb628_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		/* updated in mb628_device_init() */
		STM_PLAT_RESOURCE_MEM(0, 32*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= mb628_nor_set_vpp,
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
			},
		},
	},
};


/* NAND Flash */
static struct stm_nand_bank_data mb628_nand_flash = {
	.csn		= 0,	/* updated in mb628_device_init() */
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
		.wr_off         = 40,
		.rd_on          = 10,
		.rd_off         = 40,
		.chip_delay     = 30,           /* in us */
	},
	.emi_withinbankoffset	= 0,
};

static int mb628_phy_reset(void *bus)
{
	u8 reg;
	static int first = 1;

	/* Both PHYs share the same reset signal, only act on the first. */
	if (!first)
		return 1;
	first = 0;

	reg = epld_read(EPLD_RESET);
	reg &= ~EPLD_RESET_MII;
	epld_write(reg, EPLD_RESET);
	udelay(150);
	reg |= EPLD_RESET_MII;
	epld_write(reg, EPLD_RESET);

	/* DP83865 (PHY chip) has a looong initialization
	 * procedure... Let's give him some time to settle down... */
	udelay(1000);

	/*
	 * The SMSC LAN8700 requires a 21mS delay after reset. This
	 * matches the power on reset signal period, which should only
	 * be applied after power on, but experimentally appears to be
	 * applied post reset as well.
	 */
	mdelay(25);

	return 1;
}

/*
 * Several things need to be configured to use the GMAC0 with the
 * mb539 - SMSC LAN8700 PHY board:
 *
 * - normally the PHY's internal 1V8 regulator is used, which is
 *   is enabled at PHY power up (not reset) by sampling RXCLK/REGOFF.
 *   It appears that the STx7141's internal pull up resistor on this
 *   signal is enabled at power on, defeating the internal pull down
 *   in the SMSC device. Thus it is necessary to fix an external
 *   pull down resistor to RXCLK/REGOFF. 10K appears to be sufficient.
 *
 *   Alternativly fitting J2 on the mb539 supplies power from an
 *   off-chip regulator, working around this problem.
 *
 * - various signals are muxed with the MII pins (as well as DVO_DATA).
 *   + ASC1_RXD and ASC1_RTS, so make sure J101 is set to 2-3. This
 *     allows the EPLD to disable the level converter.
 *   + PCIREQ1 and PCIREQ2 need to be disabled by removing J104 and J98
 *     (near the PCI slot).
 *   + SYSITRQ1 needs to be disabled, which requires removing R232
 *     (near CN17). See DDTS INSbl29196 for details.
 *   + PCIGNT2 needs to be disabled. This can be done either by removing
 *     R241, or by ensuring that jumper J89 is not in position 1-2 (by
 *     either removing it completely or putting it in position 2-3).
 *
 * - other jumper and switch settings for the mb539:
 *   + J1 fit 1-2 (use on board crystal)
 *   + SW1: 1:on, 2:off, 3:off, 4:off
 *   + SW2: 1:off, 2:off, 3:off, 4:off
 *
 * - For reliable SMI signalling it is necessary to have a
 *   pull up resistor on the MDIO signal. This can be done by
 *   installing R3 on the mb539 which is normally a DNF.
 *
 * - to use the MDINT signal, R148 needs to be in position 1-2.
 *   To disable this, replace the irq with -1 in the data below.
 */
#ifdef CONFIG_SH_ST_MB628_STMMAC0
static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	.bus_id = 0,
	.phy_reset = mb628_phy_reset,
	.phy_mask = 0,
	.probed_phy_irq = ILC_IRQ(43),
};
#endif

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.bus_id = 1,
	.phy_reset = mb628_phy_reset,
	.phy_mask = 0,
};

static struct platform_device mb628_epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x05000000,
			/* Minimum size to ensure mapped by PMB */
			.end	= 0x05000000+(8*1024*1024)-1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 8,
	},
};

#ifdef CONFIG_SND
/* CS8416 SPDIF to I2S converter (IC14) */
static struct platform_device mb628_snd_spdif_input = {
	.name = "snd_conv_dummy",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "SPDIF Input",

		.source_bus_id = "snd_pcm_reader.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
	},
};

static struct platform_device mb628_snd_external_dacs = {
	.name = "snd_conv_epld",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_epld_info) {
		.group = "External DACs",

		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 9,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,

		.mute_supported = 1,
		.mute_offset = EPLD_AUDIO,
		.mute_mask = EPLD_AUDIO_PCMDAC1_SMUTE |
				EPLD_AUDIO_PCMDAC2_SMUTE,
		.mute_value = EPLD_AUDIO_PCMDAC1_SMUTE |
				EPLD_AUDIO_PCMDAC2_SMUTE,
		.unmute_value = 0,
	},
};
#endif

static struct platform_device *mb628_devices[] __initdata = {
	&mb628_epld_device,
#ifdef CONFIG_SND
	&mb628_snd_spdif_input,
	&mb628_snd_external_dacs,
#endif
};

static int __init mb628_device_init(void)
{
	struct sysconf_field *sc;

	/* Configure FLASH devices */
	sc = sysconf_claim(SYS_STA, 1, 16, 17, "boot_mode");
	switch (sysconf_read(sc)) {
	case 0x0:
		/* Boot-from-NOR: */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		mb628_nor_flash.resource[0].start = 0x00000000;
		mb628_nor_flash.resource[0].end = emi_bank_base(1) - 1;
		platform_device_register(&mb628_nor_flash);
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		mb628_nand_flash.csn = 0;
		stx7141_configure_nand(&(struct stm_nand_config) {
					.driver = stm_nand_flex,
					.nr_banks = 1,
					.banks = &mb628_nand_flash,
					.rbn.flex_connected = 1,});
		/* The MTD NAND code doesn't understand the concept of VPP, (or
		 * hardware write protect) so permanently enable it.
		 */
		epld_write(EPLD_FLASH_NOTWP | EPLD_FLASH_NOTRESET, EPLD_FLASH);
		break;
	default:
		BUG();
		break;
	}


	/*
	 * Can't enable PWM output without conflicting with either
	 * SSC6 (audio) or USB1A OC (which is disabled in cut 1 because it
	 * has the wrong OC polarity but would still result in contention).
	 *
	 * stx7141_configure_pwm(0, 1);
	 */
	stx7141_configure_ssc_spi(0, NULL);
	stx7141_configure_ssc_spi(1, NULL);
	stx7141_configure_ssc_i2c(2);
	stx7141_configure_ssc_i2c(3);
	stx7141_configure_ssc_i2c(4);
	stx7141_configure_ssc_i2c(5);
	stx7141_configure_ssc_i2c(6);

	stx7141_configure_usb(0, &(struct stx7141_usb_config) {
		.ovrcur_mode = stx7141_usb_ovrcur_active_low,
		.pwr_enabled = 1 });

	/* This requires fitting jumpers J52A 1-2 and J52B 4-5 */
	stx7141_configure_usb(1, &(struct stx7141_usb_config) {
		.ovrcur_mode = stx7141_usb_ovrcur_active_low,
		.pwr_enabled = 1 });

	/*
	 * USB1.1 ports on mb628 rev A are missing the series
	 * resistors, which can make them unreliable, and also the
	 * pull down resistors, which causes them to report spurious
	 * device connection. Unfortunately we can't determine board
	 * revision, but most boards are now rev B, so make the ports
	 * available here. Boards with cut 1 Si will have these ports
	 * disabled anyway, becuase the OC polarity is wrong.  Users
	 * with rev A boards and cut 2 Si will need to remove these
	 * lines.
	 */
	stx7141_configure_usb(2, &(struct stx7141_usb_config) {
		.ovrcur_mode = stx7141_usb_ovrcur_active_low,
		.pwr_enabled = 1 });
	stx7141_configure_usb(3, &(struct stx7141_usb_config) {
		.ovrcur_mode = stx7141_usb_ovrcur_active_low,
		.pwr_enabled = 1 });

	stx7141_configure_sata();

	/* Note R253 must be removed for Ethernet MDIO signal to work. */

#ifdef CONFIG_SH_ST_MB628_STMMAC0
	/* Must disable ASC1 if using GMII0 */
	epld_write(epld_read(EPLD_ENABLE) | EPLD_ASC1_EN | EPLD_ENABLE_MII0,
		   EPLD_ENABLE);

	/* Configure GMII0 MDINT for active low */
	set_irq_type(ILC_IRQ(43), IRQ_TYPE_LEVEL_LOW);

	stx7141_configure_ethernet(0, &(struct stx7141_ethernet_config) {
			.mode = stx7141_ethernet_mode_mii,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac0_mdio_bus,
		});
#endif

	epld_write(epld_read(EPLD_ENABLE) | EPLD_ENABLE_MII1, EPLD_ENABLE);
	stx7141_configure_ethernet(1, &(struct stx7141_ethernet_config) {
			.mode = stx7141_ethernet_mode_gmii,
			.phy_bus = 1,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac1_mdio_bus,
		});

	stx7141_configure_lirc(&(struct stx7141_lirc_config) {
			.rx_mode = stx7141_lirc_rx_disabled,
			.tx_enabled = 1,
			.tx_od_enabled = 1 });

	/* Audio peripherals
	 *
	 * WARNING! Board rev. A has swapped silkscreen labels of J16 & J32!
	 *
	 * The recommended audio setup of MB628 is as follows:
	 * SW2[1..4] - [ON, OFF, OFF, ON]
	 * SW5[1..4] - [OFF, OFF, OFF, OFF]
	 * SW3[1..4] - [OFF, OFF, ON, OFF]
	 * SW12[1..4] - [OFF, OFF, OFF, OFF]
	 * SW13[1..4] - [OFF, OFF, OFF, OFF]
	 * J2 - 2-3
	 * J3 - 1-2
	 * J6 - 1-2
	 * J7 - 1-2
	 * J8 - 1-2
	 * J12 - 1-2
	 * J16-A - 1-2, J16-B - 1-2
	 * J23-A - 2-3, J23-B - 2-3
	 * J26-A - 1-2, J26-B - 2-3
	 * J34-A - 1-2, J34-B - 2-3
	 * J41-A - 3-2, J41-B - 3-2
	 *
	 * Additionally the audio EPLD should be updated to the latest
	 * available release.
	 *
	 * With such settings the audio outputs layout presents as follows:
	 *
	 * +--------------------------------------+
	 * |                                      |
	 * |  (S.I)   (1.R)  (1.L)  (0.4)  (0.3)  | TOP
	 * |                                      |
	 * |  (---)   (0.2)  (0.1)  (0.10) (0.9)  |
	 * |                                      |
	 * |  (S.O)   (0.6)  (0.5)  (0.8)  (0.7)  | BOTTOM
	 * |                                      |
	 * +--------------------------------------+
	 *     CN6     CN5    CN4    CN3     CN2
	 *
	 * where:
	 *   - S.I - SPDIF input - PCM Reader #0
	 *   - S.O - SPDIF output - SPDIF Player (HDMI)
	 *   - 1.R, 1.L - audio outputs - PCM Player #1, channel L(1)/R(2)
	 *   - 0.1-10 - audio outputs - PCM Player #0, channels 1 to 10
	 */

	/* As digital audio outputs are now GPIOs, we have to claim them... */
	stx7141_configure_audio(&(struct stx7141_audio_config) {
			.pcm_player_0_output =
					stx7141_pcm_player_0_output_10_channels,
			.pcm_player_1_output_enabled = 0,
			.spdif_player_output_enabled = 1,
			.pcm_reader_0_input_enabled = 1,
			.pcm_reader_1_input_enabled = 1 });

	/* We use both DACs to get full 10-channels output from
	 * PCM Player #0 (EPLD muxing mode #1) */
	{
		unsigned int value = epld_read(EPLD_AUDIO);

		value &= ~(EPLD_AUDIO_AUD_SW_CTRL_MASK <<
				EPLD_AUDIO_AUD_SW_CTRL_SHIFT);
		value |= 0x1 << EPLD_AUDIO_AUD_SW_CTRL_SHIFT;

		epld_write(value, EPLD_AUDIO);
	}

	gpiochip_add(&epld_spi_chipselect);
	spi_register_board_info(&mb628_serial_flash, 1);

	return platform_add_devices(mb628_devices, ARRAY_SIZE(mb628_devices));
}
arch_initcall(mb628_device_init);

static void __iomem *mb628_ioport_map(unsigned long port, unsigned int size)
{
	/*
	 * No IO ports on this device, but to allow safe probing pick
	 * somewhere safe to redirect all reads and writes.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb628_init_irq(void)
{
}

struct sh_machine_vector mv_mb628 __initmv = {
	.mv_name		= "mb628",
	.mv_setup		= mb628_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb628_init_irq,
	.mv_ioport_map		= mb628_ioport_map,
};
