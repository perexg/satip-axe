/*
 * arch/sh/boards/mach-eud7141/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 * Author: Srinivas.Kandagatla (srinivas.kandagatla@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7141 EUD Reference Board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/tm1668.h>
#include <linux/leds.h>
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
#include <sound/stm.h>

#define EUD7141_PIO_PHY_RESET stm_gpio(5, 3)

static void __init eud7141_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx7141 EUD Reference Board initialisation\n");

	stx7141_early_device_init();

	/* For "STi7141-EUD Wave V1.1A" Board,  Smart card cable needs to be
	 * unplugged if you want to control console */
	stx7141_configure_asc(2, &(struct stx7141_asc_config) {
			.is_console = 1, });

}

/* NOR Flash */
static struct platform_device eud7141_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		/* updated in eud7141_device_init() */
		STM_PLAT_RESOURCE_MEM(0, 128*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
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
static struct stm_nand_bank_data eud7141_nand_flash = {
	.csn		= 0,	/* updated in eud7141_device_init() */
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
		.wr_off         = 30,
		.rd_on          = 10,
		.rd_off         = 30,
		.chip_delay     = 40,           /* in us */
	},
};

/* Serial Flash */
static struct spi_board_info eud7141_serial_flash =  {
	.modalias       = "m25p80",
	.bus_num	= 0,
	.max_speed_hz   = 7000000,
	.mode	   	= SPI_MODE_3,
	.chip_select	= stm_gpio(2, 5),
	.platform_data  = &(struct flash_platform_data) {
		.name = "m25p80",
		.type = "m25px64",
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

/* FrontPanel */
static struct platform_device eud7141_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP green",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(7, 6),
				.active_low = 1,
			},
		},
	},
};

static struct tm1668_key eud7141_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character eud7141_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device eud7141_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(15, 5),
		.gpio_sclk = stm_gpio(15, 6),
		.gpio_stb = stm_gpio(15, 4),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(eud7141_front_panel_keys),
		.keys = eud7141_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(eud7141_front_panel_characters),
		.characters = eud7141_front_panel_characters,
		.text = "7141",
	},
};


static int eud7141_phy_reset(void *bus)
{
	static int first = 1;

	/* Both PHYs share the same reset signal, only act on the first. */
	if (!first)
		return 1;
	first = 0;

	printk(KERN_DEBUG "Resetting PHY\n");
	gpio_set_value(EUD7141_PIO_PHY_RESET, 1);
	udelay(10);
	gpio_set_value(EUD7141_PIO_PHY_RESET, 0);
	mdelay(50);
	gpio_set_value(EUD7141_PIO_PHY_RESET, 1);
	udelay(10);

	return 1;
}

#ifdef CONFIG_SH_ST_EUD7141_STMMAC0
#define STMMAC0_PHY_ADDR 1
static int stmmac0_phy_irqs[PHY_MAX_ADDR] = {
		[STMMAC0_PHY_ADDR] = ILC_IRQ(43),
};
static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	.bus_id = 0,
	.phy_reset = eud7141_phy_reset,
	.phy_mask = 0,
	.irqs = stmmac0_phy_irqs,
};
#endif

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.bus_id = 1,
	.phy_reset = eud7141_phy_reset,
	.phy_mask = 0,
};

#ifdef CONFIG_SND

#define STV6417_REG_COUNT	(9)
static int eud7141_scart_av_init(struct i2c_client *client, void *priv)
{
	/* Refer to STV6417/18 AN001 for More details on register values */
	char cmd[STV6417_REG_COUNT * 2] = { 0x00, 0x00,
			0x01, 0x09, /* Audio Enable */
			0x02, 0x11, /* Encoder input for TV & VCR SCART */
			0x03, 0x00,
			0x04, 0xa0, /* Bi-directional Y/C connections ??*/
			0x05, 0x81, /* Video encoder DC-coupled */
			0x06, 0x00,
			0x07, 0x00, /* Detection Disabled */
			0x08, 0x00  /* All I/0 out of standby */
			};
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[STV6417_REG_COUNT];
	int i;

	for (i = 0 ; i < STV6417_REG_COUNT ; i++) {
		msg[i].addr = client->addr;
		msg[i].flags = client->flags & I2C_M_TEN;
		msg[i].len = sizeof(char) * 2;
		msg[i].buf = &cmd[i*2];
	}
	return i2c_transfer(adap, &msg[0], STV6417_REG_COUNT);
}

static int eud7141_comp_av_init(struct i2c_client *client, void *priv)
{
	char cmd[] = { 0x0, 0x00 };
	int cmd_len = sizeof(cmd);
	i2c_master_send(client, cmd, cmd_len);
	cmd[0] = 0x1;
	cmd[1] = 0x0;
	i2c_master_send(client, cmd, cmd_len);
	return 0;
}

static struct i2c_board_info eud7141_av_ctrl[] = {
	/* Component Audio/Video outputs control */
	{
		I2C_BOARD_INFO("snd_conv_i2c", 0x4a),
		.type = "STV6440",
		.platform_data = &(struct snd_stm_conv_i2c_info)
		{
			.group = "Analog Output",
			.source_bus_id = "snd_pcm_player.1",
			.channel_from = 0,
			.channel_to = 1,
			.format = SND_STM_FORMAT__I2S |
					SND_STM_FORMAT__SUBFRAME_32_BITS,
			.oversampling = 256,
			.mute_supported = 1,
			.mute_cmd = (char []){ 0x00, 0x80 },
			.mute_cmd_len = 2,
			.init = eud7141_comp_av_init,
			.enable_supported = 1,
			.enable_cmd = (char []){ 0x00, 0x00 },
			.enable_cmd_len = 2,
			.disable_cmd = (char []){ 0x00, 0x80 },
			.disable_cmd_len = 2,
		},
	},
	/* Audio/Video on SCART outputs control */
	{
		I2C_BOARD_INFO("snd_conv_i2c", 0x4b),
		.type = "STV6417",
		.platform_data = &(struct snd_stm_conv_i2c_info)
		{
			.group = "Analog Output",
			.source_bus_id = "snd_pcm_player.1",
			.channel_from = 0,
			.channel_to = 1,
			.format = SND_STM_FORMAT__I2S |
					SND_STM_FORMAT__SUBFRAME_32_BITS,
			.oversampling = 256,
			.init = eud7141_scart_av_init,
			.enable_supported = 1,
			.enable_cmd = (char []){ 0x01, 0x09 },
			.enable_cmd_len = 2,
			.disable_cmd = (char []){ 0x01, 0x00 },
			.disable_cmd_len = 2,
		},
	},
};
#endif


static struct platform_device *eud7141_devices[] __initdata = {
	&eud7141_leds,
	&eud7141_front_panel,
	&eud7141_nor_flash,
};

static int __init eud7141_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/* Configure Flash according to boot-device */
	sc = sysconf_claim(SYS_STA, 1, 16, 17, "boot_mode");
	switch (sysconf_read(sc)) {
	case 0x0:
		/* Boot-from-NOR (ensure J2:1-2)
		 *	EMIA -> NOR
		 *	EMIB -> NOR (+FMI_A26: offset 0x4000000)
		 *	EMIC -> NAND
		 */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		eud7141_nand_flash.csn = 2;
		break;
	case 0x1:
		/* Boot-from-NAND (ensure J2:2-3)
		 *      EMIA -> NAND
		 *      EMIB -> NOR
		 *     [EMIC -> NOR (+FMI_A26 0x4000000) not used]
		 */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		eud7141_nand_flash.csn = 0;
		break;
	default:
		pr_info("Invalid boot mode.\n");
		BUG();
		break;
	}
	sysconf_release(sc);

	/* Update NOR Flash base address and size: */
	/*     - reduce visibility of NOR flash to EMI bank size */
	if (eud7141_nor_flash.resource[0].end > nor_bank_size - 1)
		eud7141_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	eud7141_nor_flash.resource[0].start += nor_bank_base;
	eud7141_nor_flash.resource[0].end += nor_bank_base;

	stx7141_configure_nand(&(struct stm_nand_config) {
					.driver = stm_nand_flex,
					.nr_banks = 1,
					.banks = &eud7141_nand_flash,
					.rbn.flex_connected = 1,});

	stx7141_configure_ssc_spi(0, NULL);
	stx7141_configure_ssc_i2c(2);
	stx7141_configure_ssc_i2c(3);
	stx7141_configure_ssc_i2c(4);
	stx7141_configure_ssc_i2c(5);
	stx7141_configure_ssc_i2c(6);

	stx7141_configure_usb(0, &(struct stx7141_usb_config) {
		.ovrcur_mode = stx7141_usb_ovrcur_active_low,
		.pwr_enabled = 1 });

	stx7141_configure_usb(1, &(struct stx7141_usb_config) {
		.ovrcur_mode = stx7141_usb_ovrcur_active_low,
		.pwr_enabled = 1 });

	if (cpu_data->cut_major > 1)
		stx7141_configure_sata();

	gpio_request(EUD7141_PIO_PHY_RESET, "eth_phy_reset");
	gpio_direction_output(EUD7141_PIO_PHY_RESET, 1);

#ifdef CONFIG_SH_ST_EUD7141_STMMAC0
	/* Configure GMII0 MDINT for active low */
	set_irq_type(ILC_IRQ(43), IRQ_TYPE_LEVEL_LOW);

	stx7141_configure_ethernet(0, &(struct stx7141_ethernet_config) {
			.mode = stx7141_ethernet_mode_mii,
			.phy_bus = 0,
			.phy_addr = STMMAC0_PHY_ADDR,
			.mdio_bus_data = &stmmac0_mdio_bus,
		});
#endif
	stx7141_configure_ethernet(1, &(struct stx7141_ethernet_config) {
			.mode = stx7141_ethernet_mode_gmii,
			.phy_bus = 1,
			.phy_addr = 1,
			.mdio_bus_data = &stmmac1_mdio_bus,
		});

	stx7141_configure_lirc(&(struct stx7141_lirc_config) {
			.rx_mode = stx7141_lirc_rx_disabled,
			.tx_enabled = 1,
			.tx_od_enabled = 1 });

	/* Register Serial Flash device */
	spi_register_board_info(&eud7141_serial_flash, 1);

#ifdef CONFIG_SND_STM
	/* Configure Audio */
	i2c_register_board_info(4, eud7141_av_ctrl,
				ARRAY_SIZE(eud7141_av_ctrl));

	stx7141_configure_audio(&(struct stx7141_audio_config) {
					pcm_player_1_output_enabled = 1,
					spdif_player_output_enabled = 1,
					pcm_reader_0_input_enabled  = 0,
					pcm_player_1_output_enabled = 0 });

#endif

	return platform_add_devices(eud7141_devices,
					ARRAY_SIZE(eud7141_devices));
}
arch_initcall(eud7141_device_init);

static void __iomem *eud7141_ioport_map(unsigned long port, unsigned int size)
{
	/*
	 * No IO ports on this device, but to allow safe probing pick
	 * somewhere safe to redirect all reads and writes.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init eud7141_init_irq(void)
{
}

struct sh_machine_vector mv_eud7141 __initmv = {
	.mv_name		= "eud7141",
	.mv_setup		= eud7141_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= eud7141_init_irq,
	.mv_ioport_map		= eud7141_ioport_map,
};
