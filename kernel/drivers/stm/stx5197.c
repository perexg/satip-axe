/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/phy.h>
#include <linux/ata_platform.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx5197.h>
#include <asm/irq-ilc.h>



/* Note that 5197 documentation defines PIO alternative functions starting
 * from 0. So for the "Pad Manager" use the STX5197_GPIO_FUNCTION defines
 * some other value to describe "generic PIO" mode. */
#define STX5197_GPIO_FUNCTION -42 /* Forty-two is such a nice number :-* */



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx5197_asc_pad_configs[] = {
	[0] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(0, 0, 0),	/* TX */
			STM_PAD_PIO_IN(0, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(0, 5, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(0, 4, 2, "RTS"),
		},
	},
	[1] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(4, 0, 2),	/* TX */
			STM_PAD_PIO_IN(4, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(4, 3, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(4, 2, 2, "RTS"),
		},
	},
	[2] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(1, 2, 1),	/* TX */
			STM_PAD_PIO_IN(1, 3, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(1, 5, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(1, 4, 1, "RTS"),
		},
	},
	[3] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 0, 1),	/* TX */
			STM_PAD_PIO_IN(2, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(2, 2, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(2, 5, 1, "RTS"),
		},
	},
};

static struct platform_device stx5197_asc_devices[] = {
	[0] = {
		.name = "stm-asc",
		/* .id set in stx5197_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd130000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(7), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 8),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 10),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5197_asc_pad_configs[0],
		},
	},
	[1] = {
		.name = "stm-asc",
		/* .id set in stx5197_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd131000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(8), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 9),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 11),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5197_asc_pad_configs[1],
		},
	},
	[2] = {
		.name = "stm-asc",
		/* .id set in stx5197_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd132000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(12), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 3),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 5),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5197_asc_pad_configs[2],
		},
	},
	[3] = {
		.name = "stm-asc",
		/* .id set in stx5197_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd133000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(13), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 4),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 6),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5197_asc_pad_configs[3],
		},
	},
};

/* Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation. */

/* the serial console device */
int __initdata stm_asc_console_device;

/* Platform devices to register */
unsigned int __initdata stm_asc_configured_devices_num = 0;
struct platform_device __initdata
		*stm_asc_configured_devices[ARRAY_SIZE(stx5197_asc_devices)];

void __init stx5197_configure_asc(int asc, struct stx5197_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx5197_asc_devices)];
	static int tty_id;
	struct stx5197_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx5197_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stx5197_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;

	if (!config->hw_flow_control) {
		stm_pad_set_pio_ignored(plat_data->pad_config, "RTS");
		stm_pad_set_pio_ignored(plat_data->pad_config, "CTS");
	}

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx5197_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx5197_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for SSC0 in I2C mode on PIO1.6/7 pads */
static struct stm_pad_config stx5197_ssc0_i2c_pio1_pad_config = {
	.gpios_num = 2,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_BIDIR_NAMED(1, 6, 2, "SCL"),
		STM_PAD_PIO_BIDIR_NAMED(1, 7, 2, "SDA"),
	},
	.sysconfs_num = 2,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* SPI_BOOTNOTCOMMS
		 * 0: SSC0 -> PIO1[7:6], 1: SSC0 -> SPI */
		STM_PAD_SYSCONF(CFG_CTRL_M, 14, 14, 0),
		/* PIO_FUNCTIONALITY_ON_PIO1_7
		 * 0: QAM validation, 1: Normal PIO */
		STM_PAD_SYSCONF(CFG_CTRL_I, 2, 2, 1),
	},
};

/* Pad configuration for SSC0 in I2C mode on SPI pads */
static struct stm_pad_config stx5197_ssc0_i2c_spi_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* SPI_BOOTNOTCOMMS
		 * 0: SSC0 -> PIO1[7:6], 1: SSC0 -> SPI */
		STM_PAD_SYSCONF(CFG_CTRL_M, 14, 14, 1),
	},
};

/* Pad configuration for SSC0 in SPI mode (dedicated SPI pads) */
static struct stm_pad_config stx5197_ssc0_spi_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* SPI_BOOTNOTCOMMS
		 * 0: SSC0 -> PIO1[7:6], 1: SSC0 -> SPI */
		STM_PAD_SYSCONF(CFG_CTRL_M, 14, 14, 1),
	},
};

/* Pad configuration for SSC1 in I2C mode as internal QPSK bus */
static struct stm_pad_config stx5197_ssc1_i2c_qpsk_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* QPSK_DEBUG_CONFIG
		 * 0: IP289 I2C input from PIO1[0:1],
		 * 1: IP289 input from BE COMMS SSC1 */
		STM_PAD_SYSCONF(CFG_CTRL_C, 1, 1, 1),
	},
};

/* Pad configuration for SSC2 in I2C mode (always PIO3.3/2 pads) */
static struct stm_pad_config stx5197_ssc2_i2c_pad_config = {
	.gpios_num = 2,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_BIDIR_NAMED(3, 3, 1, "SCL"),
		STM_PAD_PIO_BIDIR_NAMED(3, 2, 1, "SDA"),
	},
};

/* Pad configuration for SSC1 in I2C mode on QAM_SCLT/QAM_SDAT pads */
static struct stm_pad_config stx5197_ssc1_i2c_qam_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* 0: QPSK repeater interface is routed to QAM_SCLT/SDAT,
		 * 1: SSC1 is routed to QAM_SCLT/SDAT. */
		STM_PAD_SYSCONF(CFG_CTRL_K, 27, 27, 1),
	},
};

static struct platform_device stx5197_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx5197_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd140000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(5), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx5197_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx5197_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd141000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(6), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx5197_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx5197_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd142000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(17), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.pad_config = &stx5197_ssc2_i2c_pad_config,
		},
	},
};

static int __initdata stx5197_ssc_configured[ARRAY_SIZE(stx5197_ssc_devices)];

int __init stx5197_configure_ssc_i2c(int ssc,
		struct stx5197_ssc_i2c_config *config)
{
	static int i2c_busnum;
	struct stx5197_ssc_i2c_config default_config;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx5197_ssc_devices));

	BUG_ON(stx5197_ssc_configured[ssc]);
	stx5197_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx5197_ssc_devices[ssc].name = "i2c-stm";
	stx5197_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx5197_ssc_devices[ssc].dev.platform_data;

	switch (ssc) {
	case 0:
		switch (config->routing.ssc0) {
		case stx5197_ssc0_i2c_pio1:
			plat_data->pad_config =
					&stx5197_ssc0_i2c_pio1_pad_config;
			break;
		case stx5197_ssc0_i2c_spi:
			plat_data->pad_config =
					&stx5197_ssc0_i2c_spi_pad_config;
			break;
		default:
			BUG();
			break;
		}
		break;
	case 1:
		switch (config->routing.ssc1) {
		case stx5197_ssc1_i2c_qpsk:
			plat_data->pad_config =
					&stx5197_ssc1_i2c_qpsk_pad_config;
			break;
		case stx5197_ssc1_i2c_qam:
			plat_data->pad_config =
					&stx5197_ssc1_i2c_qam_pad_config;
			break;
		default:
			BUG();
			break;
		}
		break;
	case 2:
		if (config->routing.ssc2 != stx5197_ssc2_i2c_pio3)
			BUG();
		/* Only one possible configuration, already set
		 * in device definition - nothing to do then. */
		break;
	default:
		BUG();
		break;
	}

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx5197_ssc_devices[ssc]);

	return i2c_busnum++;
}

static struct sysconf_field *stx5197_ssc0_spi_cs_sc;

static void stx5197_ssc0_spi_cs(struct spi_device *spi, int is_on)
{
	sysconf_write(stx5197_ssc0_spi_cs_sc, is_on ? 0 : 1);
}

int __init stx5197_configure_ssc_spi(int ssc)
{
	struct stm_plat_ssc_data *plat_data;

	/* Only SSC0 is SPI-capable */
	BUG_ON(ssc != 0);

	BUG_ON(stx5197_ssc_configured[0]);
	stx5197_ssc_configured[0] = 1;

	stx5197_ssc_devices[0].name = "spi-stm";
	stx5197_ssc_devices[0].id = 0;

	plat_data = stx5197_ssc_devices[ssc].dev.platform_data;
	plat_data->pad_config = &stx5197_ssc0_spi_pad_config;
	plat_data->spi_chipselect = stx5197_ssc0_spi_cs;
	stx5197_ssc0_spi_cs_sc = sysconf_claim(CFG_CTRL_M,
			13, 13, "ssc");
	sysconf_write(stx5197_ssc0_spi_cs_sc, 1); /* CS not active (yet) */

	platform_device_register(&stx5197_ssc_devices[0]);

	return 0;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx5197_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 3,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd118000, 0x234),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(19), -1),
		STM_PLAT_RESOURCE_IRQ(ILC_EXT_IRQ(4), -1),
	},
	.dev.platform_data = &(struct stm_plat_lirc_data) {
		/* The clock settings will be calculated by
		 * the driver from the system clock */
		.irbclock	= 0, /* use current_cpu data */
		.irbclkdiv	= 0, /* automatically calculate */
		.irbperiodmult	= 0,
		.irbperioddiv	= 0,
		.irbontimemult	= 0,
		.irbontimediv	= 0,
		.irbrxmaxperiod = 0x5000,
		.sysclkdiv	= 1,
		.rxpolarity	= 1,
	},
};

void __init stx5197_configure_lirc(struct stx5197_lirc_config *config)
{
	static int configured;
	struct stx5197_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx5197_lirc_device.dev.platform_data;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = stm_pad_config_alloc(2, 0);
	BUG_ON(!pad_config);

	plat_data->txenabled = config->tx_enabled;
	plat_data->pads = pad_config;

	switch (config->rx_mode) {
	case stx5197_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx5197_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, 2, 5, -1);
		break;
	case stx5197_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, 2, 6, -1);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, 2, 7, 1);

	platform_device_register(&stx5197_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data stx5197_pwm_platform_data = {
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(2, 4, 1),
			},
		},
	},
};

static struct platform_device stx5197_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd110000, 0x68),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(43), -1),
	},
	.dev.platform_data = &stx5197_pwm_platform_data,
};

void __init stx5197_configure_pwm(struct stx5197_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config)
		stx5197_pwm_platform_data.channel_enabled[0] =
				config->out0_enabled;

	platform_device_register(&stx5197_pwm_device);
}



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config stx5197_ethernet_pad_configs[] = {
	[stx5197_ethernet_mode_mii] = {
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* Ethernet interface on */
			STM_PAD_SYSCONF(CFG_CTRL_E, 0, 0, 1),
			/* RMII/MII pin mode */
			STM_PAD_SYSCONF(CFG_CTRL_E, 7, 8, 3),
			/* MII mode */
			STM_PAD_SYSCONF(CFG_CTRL_E, 2, 2, 1),
			/* MII phyclk out enable: 0=output, 1=input */
			STM_PAD_SYSCONF(CFG_CTRL_E, 6, 6, 0),
		},
	},
	[stx5197_ethernet_mode_rmii] = {
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* Ethernet interface on */
			STM_PAD_SYSCONF(CFG_CTRL_E, 0, 0, 1),
			/* RMII/MII pin mode */
			STM_PAD_SYSCONF(CFG_CTRL_E, 7, 8, 2),
			/* MII mode */
			STM_PAD_SYSCONF(CFG_CTRL_E, 2, 2, 0),
			/* MII phyclk out enable: 0=output, 1=input,
			 * set in stx5197_configure_ethernet() */
			STM_PAD_SYSCONF(CFG_CTRL_E, 6, 6, -1),
		},
	},
};

static void stx5197_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx5197_ethernet_platform_data = {
	.pbl = 32,
	.fix_mac_speed = stx5197_ethernet_fix_mac_speed,
	.init = &stmmac_claim_resource,
};

static struct platform_device stx5197_ethernet_device = {
	.name = "stmmaceth",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfde00000, 0x10000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(24), -1),
	},
	.dev.platform_data = &stx5197_ethernet_platform_data,
};

void __init stx5197_configure_ethernet(struct stx5197_ethernet_config *config)
{
	static int configured;
	struct stx5197_ethernet_config default_config;
	struct stm_pad_config *pad_config;
	int interface;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	if (config->mode == stx5197_ethernet_mode_rmii)
		interface = PHY_INTERFACE_MODE_RMII;
	else
		interface = PHY_INTERFACE_MODE_MII;

	pad_config = &stx5197_ethernet_pad_configs[config->mode];

	if (config->mode == stx5197_ethernet_mode_rmii)
		pad_config->sysconfs[3].value = (config->ext_clk ? 1 : 0);

	stx5197_ethernet_platform_data.custom_cfg = (void *) pad_config;
	stx5197_ethernet_platform_data.interface = interface;
	stx5197_ethernet_platform_data.bus_id = config->phy_bus;
	stx5197_ethernet_platform_data.phy_addr = config->phy_addr;
	stx5197_ethernet_platform_data.mdio_bus_data = config->mdio_bus_data;

	/* MAC_SPEED_SEL */
	stx5197_ethernet_platform_data.bsp_priv = sysconf_claim(CFG_CTRL_E,
		1, 1, "stmmac");

	platform_device_register(&stx5197_ethernet_device);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx5197_usb_dma_mask = DMA_BIT_MASK(32);

static void stx5197_usb_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, "USB_PWR", value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, "USB_ACK")
			== value)
			break;
		mdelay(10);
	}

	return;
}

static struct stm_plat_usb_data stx5197_usb_platform_data = {
	.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
		STM_PLAT_USB_FLAGS_STRAP_PLL |
		STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
	.device_config = &(struct stm_device_config) {
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYSCONF(CFG_CTRL_H, 8, 8, "USB_PWR"),
			STM_DEVICE_SYSCONF(CFG_MONITOR_E, 30, 30, "USB_ACK"),
		},
		.power = stx5197_usb_power,
		.pad_config = &(struct stm_pad_config) {
			.sysconfs_num = 1,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* DDR enable for ULPI:
				 * 0 = 8 bit SDR ULPI, 1 = 4 bit DDR ULPI */
				STM_PAD_SYSCONF(CFG_CTRL_M, 12, 12, 0),
			},
		},
	},
};

static struct platform_device stx5197_usb_device = {
	.name = "stm-usb",
	.id = -1,
	.dev = {
		.dma_mask = &stx5197_usb_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &stx5197_usb_platform_data,
	},
	.num_resources = 6,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfddffe00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(29), -1),
		STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfddffc00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(28), -1),
		STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfdd00000, 0x100),
		STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfddfff00, 0x100),
	},
};

void __init stx5197_configure_usb(void)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	platform_device_register(&stx5197_usb_device);
}



/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs stx5197_fdma_fw = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x560 << 2), /* 0x9580 */
	.cntn      = 0x8000 + (0x562 << 2), /* 0x9588 */
	.saddrn    = 0x8000 + (0x563 << 2), /* 0x958c */
	.daddrn    = 0x8000 + (0x564 << 2), /* 0x9590 */
};

static struct stm_plat_fdma_hw stx5197_fdma_hw = {
	.slim_regs = {
		.id       = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver      = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en       = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.dmem = {
		.offset = 0x8000,
		.size   = 0x800 << 2, /* 2048 * 4 = 8192 */
	},
	.periph_regs = {
		.sync_reg = 0x8000 + (0xfe2 << 2), /* 0xbf88 */
		.cmd_sta  = 0x8000 + (0xff0 << 2), /* 0xbfc0 */
		.cmd_set  = 0x8000 + (0xff1 << 2), /* 0xbfc4 */
		.cmd_clr  = 0x8000 + (0xff2 << 2), /* 0xbfc8 */
		.cmd_mask = 0x8000 + (0xff3 << 2), /* 0xbfcc */
		.int_sta  = 0x8000 + (0xff4 << 2), /* 0xbfd0 */
		.int_set  = 0x8000 + (0xff5 << 2), /* 0xbfd4 */
		.int_clr  = 0x8000 + (0xff6 << 2), /* 0xbfd8 */
		.int_mask = 0x8000 + (0xff7 << 2), /* 0xbfdc */
	},
	.imem = {
		.offset = 0xc000,
		.size   = 0x1000 << 2, /* 4096 * 4 = 16384 */
	},
};

static struct platform_device stx5197_fdma_device = {
	.name = "stm-fdma",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfdb00000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(34), -1),
	},
	.dev.platform_data = &(struct stm_plat_fdma_data) {
		.hw = &stx5197_fdma_hw,
		.fw = &stx5197_fdma_fw,
	},
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx5197_pio_devices[] = {
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd120000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(0), -1),
		},
	},
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd121000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(1), -1),
		},
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd122000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(2), -1),
		},
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd123000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(3), -1),
		},
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd124000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(4), -1),
		},
	},
};

static int stx5197_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	int port = stm_gpio_port(gpio);
	int pin = stm_gpio_pin(gpio);

	BUG_ON(port > ARRAY_SIZE(stx5197_pio_devices));

	if (function == STX5197_GPIO_FUNCTION) {
		switch (direction) {
		case stm_pad_gpio_direction_in:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_IN);
			break;
		case stm_pad_gpio_direction_out:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_OUT);
			break;
		case stm_pad_gpio_direction_bidir:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_BIDIR);
			break;
		default:
			BUG();
			break;
		}
	} else if (direction == stm_pad_gpio_direction_in) {
		BUG_ON(function != -1);
		stm_gpio_direction(gpio, STM_GPIO_DIRECTION_IN);
	} else {
		static struct sysconf_field *cfg_ctrl_fgo[3];
		struct sysconf_field *cfg;
		int offset;
		unsigned int value;

		BUG_ON(function < 0);
		BUG_ON(function > 3);

		switch (direction) {
		case stm_pad_gpio_direction_out:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stm_pad_gpio_direction_bidir:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		default:
			BUG();
			break;
		}

		if (!cfg_ctrl_fgo[0]) {
			cfg_ctrl_fgo[0] = sysconf_claim(CFG_CTRL_F, 0, 31,
					"PIO Config");
			BUG_ON(!cfg_ctrl_fgo[0]);
			cfg_ctrl_fgo[1] = sysconf_claim(CFG_CTRL_G, 0, 31,
					"PIO Config");
			BUG_ON(!cfg_ctrl_fgo[1]);
			cfg_ctrl_fgo[2] = sysconf_claim(CFG_CTRL_O, 0, 15,
					"PIO Config");
			BUG_ON(!cfg_ctrl_fgo[2]);
		}

		cfg = cfg_ctrl_fgo[port / 2];
		offset = (port % 2) * 16 + pin;

		value = sysconf_read(cfg);
		value &= ~((1 << (offset + 8)) | (1 << offset));
		value |= ((function >> 1) & 1) << (offset + 8);
		value |= (function & 1) << offset;
		sysconf_write(cfg, value);
	}

	return 0;
}



/* sysconf resources ------------------------------------------------------ */

#ifdef CONFIG_DEBUG_FS

#define SYSCONF_REG(field) _SYSCONF_REG(#field, field)
#define _SYSCONF_REG(name, group, num) case num: return name

static const char *stx5197_sysconf_hd_reg_name(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_CTRL_C);
	SYSCONF_REG(CFG_CTRL_D);
	SYSCONF_REG(CFG_CTRL_E);
	SYSCONF_REG(CFG_CTRL_F);
	SYSCONF_REG(CFG_CTRL_G);
	SYSCONF_REG(CFG_CTRL_H);
	SYSCONF_REG(CFG_CTRL_I);
	SYSCONF_REG(CFG_CTRL_J);

	SYSCONF_REG(CFG_CTRL_K);
	SYSCONF_REG(CFG_CTRL_L);
	SYSCONF_REG(CFG_CTRL_M);
	SYSCONF_REG(CFG_CTRL_N);
	SYSCONF_REG(CFG_CTRL_O);
	SYSCONF_REG(CFG_CTRL_P);
	SYSCONF_REG(CFG_CTRL_Q);
	SYSCONF_REG(CFG_CTRL_R);

	SYSCONF_REG(CFG_MONITOR_C);
	SYSCONF_REG(CFG_MONITOR_D);
	SYSCONF_REG(CFG_MONITOR_E);
	SYSCONF_REG(CFG_MONITOR_F);
	SYSCONF_REG(CFG_MONITOR_G);
	SYSCONF_REG(CFG_MONITOR_H);
	SYSCONF_REG(CFG_MONITOR_I);
	SYSCONF_REG(CFG_MONITOR_J);

	SYSCONF_REG(CFG_MONITOR_K);
	SYSCONF_REG(CFG_MONITOR_L);
	SYSCONF_REG(CFG_MONITOR_M);
	SYSCONF_REG(CFG_MONITOR_N);
	SYSCONF_REG(CFG_MONITOR_O);
	SYSCONF_REG(CFG_MONITOR_P);
	SYSCONF_REG(CFG_MONITOR_Q);
	SYSCONF_REG(CFG_MONITOR_R);
	}

	return "???";
}

static const char *stx5197_sysconf_hs_reg_name(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_CTRL_A);
	SYSCONF_REG(CFG_CTRL_B);

	SYSCONF_REG(CFG_MONITOR_A);
	SYSCONF_REG(CFG_MONITOR_B);
	}

	return "???";
}

#endif

static struct platform_device stx5197_sysconf_devices[] = {
	{
		.name		= "stm-sysconf",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd901000, 0x80),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = HD_CFG,
					.offset = 0,
					.name = "High Density group ",
#ifdef CONFIG_DEBUG_FS
					.reg_name = stx5197_sysconf_hd_reg_name,
#endif
				},
			},
		}
	}, {
		.name		= "stm-sysconf",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd902000, 0x10),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = HS_CFG,
					.offset = 0,
					.name = "High Speed group ",
#ifdef CONFIG_DEBUG_FS
					.reg_name = stx5197_sysconf_hs_reg_name,
#endif
				},
			},
		}
	},
};



/* EMI resources ---------------------------------------------------------- */
static void stx5197_emi_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, "EMI_PWR", value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, "EMI_ACK")
			== value)
			break;
		mdelay(10);
	}

	return;
}

static struct platform_device stx5197_emi = {
	.name = "emi",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 128 * 1024 * 1024),
		STM_PLAT_RESOURCE_MEM(0xfde30000, 0x874),
	},
	.dev.platform_data = &(struct stm_device_config){
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYSCONF(CFG_CTRL_I, 1, 1, "EMI_PWR"),
			STM_DEVICE_SYSCONF(CFG_MONITOR_J, 1, 1, "EMI_ACK"),
		},
		.power = stx5197_emi_power,
	}
};



/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stx5197_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(stx5197_sysconf_devices, 2);
	stm_gpio_early_init(stx5197_pio_devices,
			ARRAY_SIZE(stx5197_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);
	stm_pad_init(ARRAY_SIZE(stx5197_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     -1, STX5197_GPIO_FUNCTION, stx5197_pio_config);

	sc = sysconf_claim(CFG_MONITOR_H, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx5197 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx5197_postcore_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx5197_pio_devices); i++)
		platform_device_register(&stx5197_pio_devices[i]);

	return platform_device_register(&stx5197_emi);
}
postcore_initcall(stx5197_postcore_setup);


/* Low Power Controller ---------------------------------------------------- */

static struct platform_device stx5197_rtc_device = {
	.name           = "stm-rtc",
	.id             = -1,
	.num_resources  = 1,
	.resource       = (struct resource[]){
		STM_PLAT_RESOURCE_MEM(0xFDC00000, 0x1000),
	},
};

/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx5197_devices[] __initdata = {
	&stx5197_fdma_device,
	&stx5197_sysconf_devices[0],
	&stx5197_sysconf_devices[1],
	&stx5197_rtc_device,
};

static int __init stx5197_devices_setup(void)
{
	return platform_add_devices(stx5197_devices,
			ARRAY_SIZE(stx5197_devices));
}
device_initcall(stx5197_devices_setup);

