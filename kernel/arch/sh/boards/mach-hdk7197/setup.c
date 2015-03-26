/*
 * arch/sh/boards/mach-hdk7197/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Srinivas Kandagatla (srinivas.kandagatla@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STi7197-HDK board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/tm1668.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/i2c-gpio.h>
#include <linux/spi/flash.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/io.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>

/*
 * Flash setup depends on boot-device:
 *
 *                     NOR                NAND	              SPI
 * ----------------------------------------------------------------------------
 * JE6-1 (mode 15)     ON (boot NOR)	  OFF (boot NAND)     ON (boot SPI)
 * JE6-2 (mode 16)     ON		  ON		      OFF
 *
 * JE5-1 (port size)   ON (x16)	          OFF (x8)	      x
 *
 * JE3	 (CS Routing)  1-2 (EMIA->NOR)    2-3 (EMIA->NAND)    2-3 (EMIA->NAND)
 * JE4		       1-2 (EMIB->NAND)	  2-3 (EMIB->NOR)     2-3 (EMIB->NOR)
 *
 * Post-boot Access
 *          NOR	       EMIA (64MB)	  EMIB (8MB)	     EMIB (8MB)
 *          NAND       EMIB/FLEXB	  FLEXA		     FLEXA
 *          SPI	       SPI_GPIO/SSC1      SPI_GPIO/SSC1      SPI_GPIO/SSC1
 *
 * -----------------------------------------------------------------------------
 */

#define HDK7197_GPIO_POWER_ON_ETH	stm_gpio(15, 5)
#define HDK7197_GREEN_LED		stm_gpio(11, 5)
#define HDK7197_GPIO_COMS_NOTCS		stm_gpio(2, 4)
#define HDK7197_GPIO_COMS_CLK		stm_gpio(2, 5)
#define HDK7197_GPIO_COMS_DIN		stm_gpio(2, 7)
#define HDK7197_GPIO_COMS_DOUT		stm_gpio(2, 6)

static void __init hdk7197_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STi7197-HDK initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });
}


/* NOTE:if using Front Panel STi51XX-SDK_FP V1.1,
 * It requires R16 to be changed to around 2K */

static struct platform_device hdk7197_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP green",
				.default_trigger = "heartbeat",
				.gpio = HDK7197_GREEN_LED,
				.active_low = 1,
			},
		},
	},
};

static struct tm1668_key hdk7197_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk7197_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk7197_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(11, 2),
		.gpio_sclk = stm_gpio(11, 3),
		.gpio_stb = stm_gpio(11, 4),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk7197_front_panel_keys),
		.keys = hdk7197_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk7197_front_panel_characters),
		.characters = hdk7197_front_panel_characters,
		.text = "7197",
	},
};

static int hdk7197_phy_reset(void *bus)
{
	gpio_set_value(HDK7197_GPIO_POWER_ON_ETH, 0);
	udelay(100);
	gpio_set_value(HDK7197_GPIO_POWER_ON_ETH, 1);
	udelay(1);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = hdk7197_phy_reset,
	.phy_mask = 0,
};

/* NOR Flash */
static struct platform_device hdk7197_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 64 * 1024 * 1024 - 1,
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

/* GPIO SPI bus for Serial Flash device
 *
 * The Serial Flash device is connected directly to the SPI Boot pads and to
 * COMS SSC1 via 3k3 resistors.  This setup seems to limit the speed at which
 * the SCC can drive the SPI bus, with corruptions seen above 2MHz (RC issues
 * suspected).  As a result, it is actually faster to drive the Serial Flash
 * device with GPIO SPI bus!
 */
static struct platform_device hdk7197_serial_flash_bus = {
	.name		= "spi_gpio",
	.id		= 0,
	.num_resources  = 0,
	.dev		= {
			.platform_data = &(struct spi_gpio_platform_data) {
				.sck = stm_gpio(15, 0),
				.mosi = stm_gpio(15, 1),
				.miso = stm_gpio(15, 3),
				.num_chipselect = 1,
			},
	},
};


/* Serial Flash */
static struct spi_board_info hdk7197_serial_flash =  {
	.modalias       = "m25p80",
	.bus_num        = 0,
	.max_speed_hz   = 7000000,
	.mode           = SPI_MODE_3,
	.controller_data = (void *)stm_gpio(15, 2),
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

/* NAND Flash */
struct stm_nand_bank_data hdk7197_nand_flash = {
	.csn            = 1,
	.options        = NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
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
		.sig_setup      = 50,           /* times in ns */
		.sig_hold       = 50,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on          = 10,
		.wr_off         = 40,
		.rd_on          = 10,
		.rd_off         = 40,
		.chip_delay     = 50,           /* in us */
	},
};



static struct platform_device *hdk7197_devices[] __initdata = {
	&hdk7197_leds,
	&hdk7197_nor_flash,
	&hdk7197_front_panel,
	&hdk7197_serial_flash_bus,
};

#ifdef CONFIG_SND

/* Enable CVBS output to both (TV & VCR) SCART outputs &
 * PR_R, Y_G, PB_B to Component output. */
#define STV6418_REG_COUNT	(9)
static int hdk7197_scart_audio_init(struct i2c_client *client, void *priv)
{
	/* Refer to STV6417/18 AN001 for More details on register values */
	char cmd[STV6418_REG_COUNT * 2] = { 0x00, 0x00,
			0x01, 0x00, /* Audio Disable*/
			0x02, 0x11, /* Encoder input for TV & VCR SCART */
			0x03, 0x00,
			0x04, 0xa0, /* Bi-directional Y/C connections ??*/
			0x05, 0x81, /* Video encoder DC-coupled */
			0x06, 0x00,
			0x07, 0x01, /* HD Output enable */
			0x08, 0x00  /* All I/0 out of standby */
			};
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[STV6418_REG_COUNT];
	int i;

	for (i = 0 ; i < STV6418_REG_COUNT ; i++) {
		msg[i].addr = client->addr;
		msg[i].flags = client->flags & I2C_M_TEN;
		msg[i].len = sizeof(char) * 2;
		msg[i].buf = &cmd[i*2];
	}

	return i2c_transfer(adap, &msg[0], STV6418_REG_COUNT);

}

/* Audio on SCART outputs control */
static struct i2c_board_info hdk7197_scart_audio __initdata = {
	I2C_BOARD_INFO("snd_conv_i2c", 0x4b),
	.type = "STV6418",
	.platform_data = &(struct snd_stm_conv_i2c_info) {
		.group = "Analog Output",
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
		.init = hdk7197_scart_audio_init,
		.enable_supported = 1,
		.enable_cmd = (char []){ 0x01, 0x09 },
		.enable_cmd_len = 2,
		.disable_cmd = (char []){ 0x01, 0x00 },
		.disable_cmd_len = 2,
	},
};
#endif

static int __init device_init(void)
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
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(1) - nor_bank_base;
		hdk7197_nand_flash.csn = 1;
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7197_nand_flash.csn = 0;
		break;
	case 0x2:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		hdk7197_nand_flash.csn = 0;
		break;
	default:
		BUG();
		break;
	}
	sysconf_release(sc);

	/* Update NOR Flash base address and size: */
	/*     - reduce visibility of NOR flash to EMI bank size */
	if (hdk7197_nor_flash.resource[0].end > nor_bank_size - 1)
		hdk7197_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	hdk7197_nor_flash.resource[0].start += nor_bank_base;
	hdk7197_nor_flash.resource[0].end += nor_bank_base;

	/* Set SSC1 pads as GPIO inputs to avoid contention with SPI GPIO bus */
	if (gpio_request(HDK7197_GPIO_COMS_CLK, "COMS CLK") == 0)
		gpio_direction_input(HDK7197_GPIO_COMS_CLK);
	else
		printk(KERN_ERR "hdk7197: Failed to claim SPI COMS CLK!\n");

	if (gpio_request(HDK7197_GPIO_COMS_DOUT, "COMS DOUT") == 0)
		gpio_direction_input(HDK7197_GPIO_COMS_DOUT);
	else
		printk(KERN_ERR "hdk7197: Failed to claim SPI COMS DOUT!\n");
	if (gpio_request(HDK7197_GPIO_COMS_NOTCS, "COMS NOTCS") == 0)
		gpio_direction_input(HDK7197_GPIO_COMS_NOTCS);
	else
		printk(KERN_ERR "hdk7197: Failed to claim SPI COMS NOTCS!\n");

	if (gpio_request(HDK7197_GPIO_COMS_DIN, "COMS DIN") == 0)
		gpio_direction_input(HDK7197_GPIO_COMS_DIN);
	else
		printk(KERN_ERR "hdk7197: Failed to claim SPI COMS DIN!\n");

	/*I2C_xxxA HDMI */
	stx7105_configure_ssc_i2c(0, &(struct stx7105_ssc_config) {
			.routing.ssc0.sclk = stx7105_ssc0_sclk_pio2_2,
			.routing.ssc0.mtsr = stx7105_ssc0_mtsr_pio2_3, });

	/*I2C_xxxC */
	stx7105_configure_ssc_i2c(2, &(struct stx7105_ssc_config) {
			.routing.ssc2.sclk = stx7105_ssc2_sclk_pio3_4,
			.routing.ssc2.mtsr = stx7105_ssc2_mtsr_pio3_5, });
	/* I2C_xxxD - Audio/Video Switch */
	stx7105_configure_ssc_i2c(3, &(struct stx7105_ssc_config) {
			.routing.ssc3.sclk = stx7105_ssc3_sclk_pio3_6,
			.routing.ssc3.mtsr = stx7105_ssc3_mtsr_pio3_7, });


	spi_register_board_info(&hdk7197_serial_flash, 1);

	/* Nand configure */
	stx7105_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &hdk7197_nand_flash,
			.rbn.flex_connected = 1,});

	/* USB configure */
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

	if (gpio_request(HDK7197_GPIO_POWER_ON_ETH, "POWER_ON_ETH") == 0)
		gpio_direction_output(HDK7197_GPIO_POWER_ON_ETH, 1);
	else
		printk(KERN_ERR "hdk7197: Failed to claim POWER_ON_ETH "
				"PIO!\n");
	stx7105_configure_ethernet(0, &(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	/* RemoteControl configure */
	stx7105_configure_lirc(&(struct stx7105_lirc_config) {
			.rx_mode = stx7105_lirc_rx_mode_ir,
			.tx_enabled = 0,
			.tx_od_enabled = 0, });

#ifdef CONFIG_SND
	i2c_register_board_info(2, &hdk7197_scart_audio, 1);
#endif
	/* Audio Pins configure */
	stx7105_configure_audio(&(struct stx7105_audio_config) {
			.pcm_player_1_enabled = 1,
			.spdif_player_output_enabled = 1, });
	/*e-sata configure*/
	stx7105_configure_sata(0);
	return platform_add_devices(hdk7197_devices,
				    ARRAY_SIZE(hdk7197_devices));
}
arch_initcall(device_init);

static void __iomem *hdk7197_ioport_map(unsigned long port,
					unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_hdk7197 __initmv = {
	.mv_name = "hdk7197",
	.mv_setup = hdk7197_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = hdk7197_ioport_map,
};
