
/*
 * arch/sh/boards/st/7102isdb/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Srinivas Kandagatla (srinivas.kandagatla@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STi7102ISDB-SDK support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/lirc.h>
#include <linux/i2c.h>
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
#include <linux/i2c-gpio.h>
#include <asm/irq-ilc.h>
#include <sound/stm.h>

/*
 * Flash setup depends on boot-device...
 *
 * Flash Boot Up Jumper Config (board Version A, Rev:1.0)
 * 			NOR Flash	Serial Flash
 * Mode13:JE6(1,4)	OFF(0)		ON(1)
 * Mode16:JE6(2,3)	OFF(0)		ON(1)
 */

#define STI7102ISDB_PIO_PHY_RESET 	stm_gpio(15, 5)
#define STI7102ISDB_HSC_0 		stm_gpio(0, 0)
#define STI7102ISDB_HSC_1 		stm_gpio(0, 1)
#define STI7102ISDB_HBC_0 		stm_gpio(5, 0)
#define STI7102ISDB_HBC_1		stm_gpio(5, 1)

#define STI7102ISDB_SCLA		stm_gpio(2, 2)
#define STI7102ISDB_SDAA		stm_gpio(2, 3)


static void __init sti7102isdb_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STi7102-ISDB board init\n");
	stx7105_early_device_init();
	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });
}

static int sti7102isdb_phy_reset(void *bus)
{
	gpio_set_value(STI7102ISDB_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(STI7102ISDB_PIO_PHY_RESET, 1);
	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = sti7102isdb_phy_reset,
	.phy_mask = 0,
};

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00200000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00240000,
	}
};

static struct physmap_flash_data isdb7102_nor_flash_data = {
	.width		= 2, /* 16 bit width data */
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device isdb7102_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 64*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &isdb7102_nor_flash_data,
	},
};

/* Configuration for Serial Flash */
static struct mtd_partition sti7102isdb_serialflash_partitions[] = {
	{
		.name = "SFLASH_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "SFLASH_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

static struct flash_platform_data sti7102isdb_serialflash_data = {
	.name = "m25p80",
	.parts = sti7102isdb_serialflash_partitions,
	.nr_parts = ARRAY_SIZE(sti7102isdb_serialflash_partitions),
	.type = "m25p128",
};

static struct spi_board_info spi_serialflash[] =  {
	{
		.modalias       = "m25p80",
		.bus_num	= 0,
		.chip_select    = stm_gpio(2, 4),
		.max_speed_hz   = 5000000,
		.platform_data  = &sti7102isdb_serialflash_data,
		.mode	   = SPI_MODE_3,
	},
};

/* I2C_xxxA - HDMI */

struct platform_device gpio_i2c_hdmi = {
	.name = "i2c-gpio",
	.id = 2,
	.dev.platform_data = &(struct i2c_gpio_platform_data) {
		.sda_pin = STI7102ISDB_SDAA,
		.sda_is_open_drain = 0,
		.scl_pin = STI7102ISDB_SCLA,
		.scl_is_open_drain = 0,
		.scl_is_output_only = 0,
	}
};

static struct platform_device *sti7102isdb_devices[] __initdata = {
};

/* Enable analog audio output */
static int dac_audio_init(struct i2c_client *client, void *priv)
{
	const char cmd[] = { 0x0, 0x00 };
	int cmd_len = sizeof(cmd);
	return i2c_master_send(client, cmd, cmd_len) != cmd_len;
}

/* Audio on outputs control */
static struct i2c_board_info i2c_dac_ctrl __initdata = {
	I2C_BOARD_INFO("snd_conv_i2c", 0x4a),
	.type = "STV6440",
	.platform_data = &(struct snd_stm_conv_i2c_info) {
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
		.init = dac_audio_init,
		.enable_supported = 1,
		.enable_cmd = (char []){ 0x00, 0x00 },
		.enable_cmd_len = 2,
		.disable_cmd = (char []){ 0x00, 0x80 },
		.disable_cmd_len = 2,
	},
};
static int __init device_init(void)
{
	u32 bank1_start;
	u32 boot_mode;
	u32 hsc; /* Hardware & Software Configuration */
	u32 hbc; /* Hardware Bom Configuration */
	struct sysconf_field *sc;

	gpio_request(STI7102ISDB_HSC_0, NULL); /* UART0_RXD */
	gpio_direction_input(STI7102ISDB_HSC_0);
	hsc = gpio_get_value(STI7102ISDB_HSC_0) * 2;

	gpio_request(STI7102ISDB_HSC_1, NULL); /* UART0_TXD */
	gpio_direction_input(STI7102ISDB_HSC_1);
	hsc += gpio_get_value(STI7102ISDB_HSC_1) + 1;

	gpio_request(STI7102ISDB_HBC_0, NULL); /* UART3_RXD */
	gpio_direction_input(STI7102ISDB_HBC_0);
	hbc = gpio_get_value(STI7102ISDB_HBC_0) * 2;

	gpio_request(STI7102ISDB_HBC_1, NULL); /* UART3_TXD */
	gpio_direction_input(STI7102ISDB_HBC_1);
	hbc += gpio_get_value(STI7102ISDB_HBC_1) + 1;

	pr_info("STi7102-ISDB Board Version-%d & Board Type-%d\n", hsc, hbc);

	sc = sysconf_claim(SYS_STA, 1, 15, 16, "boot_mode");
	boot_mode = sysconf_read(sc);
	sysconf_release(sc);

	bank1_start = emi_bank_base(1);
	/* Configure FLASH according to boot device mode pins */
	switch (boot_mode) {
	case 0x0:
		/* Boot-from-NOR */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		isdb7102_nor_flash.resource[0].start = 0x00000000;
		isdb7102_nor_flash.resource[0].end = bank1_start - 1;
		if (platform_device_register(&isdb7102_nor_flash) != 0)
			pr_info("DEBUG: Unable to register NOR Flash\n");
		break;
	case 0x2:
		/* Boot-from-SerialFlash */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		break;
	}

	/*
	 * The serial FLASH device may be driven by PIO15[0-3] (SPIboot
	 * interface) or PIO2[4-6] (SSC0).  The two sets of PIOs are connected
	 * via 3k3 resistor.  Here we are using the SSC.  Therefore to avoid
	 * drive contention, the SPIboot PIOs should be configured as inputs.
	 */
	gpio_request(stm_gpio(15, 0), "SPI Boot CLK");
	gpio_direction_input(stm_gpio(15, 0));
	gpio_request(stm_gpio(15, 1), "SPI Boot DOUT");
	gpio_direction_input(stm_gpio(15, 1));
	gpio_request(stm_gpio(15, 2), "SPI Boot NOTCS");
	gpio_direction_input(stm_gpio(15, 2));
	gpio_request(stm_gpio(15, 3), "SPI Boot DIN");
	gpio_direction_input(stm_gpio(15, 3));

	/* I2C_xxxA HDMI  */
	platform_device_register(&gpio_i2c_hdmi);
	/* SPI - SerialFLASH */
	stx7105_configure_ssc_spi(1, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc1_sclk_pio2_5,
			.routing.ssc1.mtsr = stx7105_ssc1_mtsr_pio2_6,
			.routing.ssc1.mrst = stx7105_ssc1_mrst_pio2_7});
	/* I2C_xxxC - UF2, UD12 (EEPROM) */
	stx7105_configure_ssc_i2c(2, &(struct stx7105_ssc_config) {
			.routing.ssc2.sclk = stx7105_ssc2_sclk_pio3_4,
			.routing.ssc2.mtsr = stx7105_ssc2_mtsr_pio3_5, });
	/* I2C_xxxD - J1 (AV Board)-> U1 */
	stx7105_configure_ssc_i2c(3, &(struct stx7105_ssc_config) {
			.routing.ssc3.sclk = stx7105_ssc3_sclk_pio3_6,
			.routing.ssc3.mtsr = stx7105_ssc3_mtsr_pio3_7, });

#ifdef CONFIG_SND_STM
	i2c_register_board_info(2, &i2c_dac_ctrl, 1);
#endif
	/* USB Configuration */

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

	/* Ethernet configure */
	gpio_request(STI7102ISDB_PIO_PHY_RESET, "eth_phy_reset");
	gpio_direction_output(STI7102ISDB_PIO_PHY_RESET, 1);

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
	/* Audio Pins configure */
	stx7105_configure_audio(&(struct stx7105_audio_config) {
			.spdif_player_output_enabled = 1, });

	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));

	return platform_add_devices(sti7102isdb_devices,
				ARRAY_SIZE(sti7102isdb_devices));
}
arch_initcall(device_init);

static void __iomem *sti7102isdb_ioport_map(unsigned long port,
	unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector sti7102isdb __initmv = {
	.mv_name = "STi7102ISDB",
	.mv_setup = sti7102isdb_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = sti7102isdb_ioport_map,
};
