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
#include <linux/stm/pad.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7108.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx7108_asc_pad_configs[] = {
	[0] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(4, 0, 2),	/* TX */
			STM_PAD_PIO_IN(4, 1, 2),	/* RX */
			STM_PAD_PIO_IN_NAMED(4, 4, 2, "CTS"),
			STM_PAD_PIO_OUT_NAMED(4, 5, 2, "RTS"),
		},
	},
	[1] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(5, 1, 1),	/* TX */
			STM_PAD_PIO_IN(5, 2, 1),	/* RX */
			STM_PAD_PIO_IN_NAMED(5, 3, 1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(5, 4, 1, "RTS"),
		},
	},
	[2] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(14, 4, 1),	/* TX */
			STM_PAD_PIO_IN(14, 5, 1),	/* RX */
			STM_PAD_PIO_IN_NAMED(14, 7, 1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(14, 6, 1, "RTS"),
		},
	},
	/* .pad_config for ASC3 built in stx7108_asc_config() */
};

static struct platform_device stx7108_asc_devices[] = {
	[0] = {
		.name		= "stm-asc",
		/* .id set in stx7108_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd730000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(40), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7108_asc_pad_configs[0],
		},
	},
	[1] = {
		.name		= "stm-asc",
		/* .id set in stx7108_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd731000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(41), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7108_asc_pad_configs[1],
		},
	},
	[2] = {
		.name		= "stm-asc",
		/* .id set in stx7108_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd732000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(42), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7108_asc_pad_configs[2],
		},
	},
	[3] = {
		.name		= "stm-asc",
		/* .id set in stx7108_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd733000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(43), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			/* .pad_config for ASC3 built in stx7108_asc_config() */
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
		*stm_asc_configured_devices[ARRAY_SIZE(stx7108_asc_devices)];

void __init stx7108_configure_asc(int asc, struct stx7108_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx7108_asc_devices)];
	static int tty_id;
	struct stx7108_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx7108_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stx7108_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;
	plat_data->txfifo_bug = 1;

	if (asc == 3) {
		pad_config = stm_pad_config_alloc(4, 0);
		plat_data->pad_config = pad_config;

		switch (config->routing.asc3.txd) {
		case stx7108_asc3_txd_pio21_0:
			stm_pad_config_add_pio_out(pad_config, 21, 0, 2);
			break;
		case stx7108_asc3_txd_pio24_4:
			stm_pad_config_add_pio_out(pad_config, 24, 4, 1);
			break;
		default:
			BUG();
			break;
		}

		switch (config->routing.asc3.rxd) {
		case stx7108_asc3_rxd_pio21_1:
			stm_pad_config_add_pio_in(pad_config, 21, 1, 2);
			break;
		case stx7108_asc3_rxd_pio24_5:
			stm_pad_config_add_pio_in(pad_config, 24, 5, 1);
			break;
		default:
			BUG();
			break;
		}

		if (config->hw_flow_control) {
			switch (config->routing.asc3.cts) {
			case stx7108_asc3_cts_pio21_4:
				stm_pad_config_add_pio_in(pad_config, 21, 4, 2);
				break;
			case stx7108_asc3_cts_pio25_0:
				stm_pad_config_add_pio_in(pad_config, 25, 0, 1);
				break;
			default:
				BUG();
				break;
			}

			switch (config->routing.asc3.rts) {
			case stx7108_asc3_rts_pio21_3:
				stm_pad_config_add_pio_out(pad_config,
						21, 3, 2);
				break;
			case stx7108_asc3_rts_pio24_7:
				stm_pad_config_add_pio_out(pad_config,
						24, 7, 1);
				break;
			default:
				BUG();
				break;
			}
		}
	} else {
		pad_config = &stx7108_asc_pad_configs[asc];

		if (!config->hw_flow_control) {
			/* Don't claim RTS/CTS pads */
			stm_pad_set_pio_ignored(pad_config, "RTS");
			stm_pad_set_pio_ignored(pad_config, "CTS");
		}
	}

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx7108_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx7108_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C mode */
static struct stm_pad_config stx7108_ssc_i2c_pad_configs[] = {
	[0] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(1, 6, 2, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(1, 7, 2, "SDA"),
		},
	},
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(9, 6, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(9, 7, 1, "SDA"),
		},
	},
	/* Configuration for SSC2 is created in stx7108_configure_ssc_*(),
	 * according to passed routing information */
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(5, 2, 2, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(5, 3, 2, "SDA"),
		},
	},
	[4] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(13, 6, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(13, 7, 1, "SDA"),
		},
	},
	[5] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(5, 6, 2, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(5, 7, 2, "SDA"),
		},
	},
	[6] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(15, 2, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(15, 3, 1, "SDA"),
		},
	},
};

/* Pad configuration for SPI mode */
static struct stm_pad_config stx7108_ssc_spi_pad_configs[] = {
	[0] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(1, 6, 2),	/* SCK */
			STM_PAD_PIO_OUT(1, 7, 2),	/* MOSI */
			STM_PAD_PIO_IN(2, 0, 2),	/* MISO */
		},
	},
	[1] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(9, 6, 1),	/* SCK */
			STM_PAD_PIO_OUT(9, 7, 1),	/* MOSI */
			STM_PAD_PIO_IN(9, 5, 1),	/* MISO */
		},
	},
	/* Configuration for SSC2 is created in stx7108_configure_ssc_*(),
	 * according to passed routing information */
	[3] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(5, 2, 2),	/* SCK */
			STM_PAD_PIO_OUT(5, 3, 2),	/* MOSI */
			STM_PAD_PIO_IN(5, 4, 2),	/* MISO */
		},
	},
	[4] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(13, 6, 1),	/* SCK */
			STM_PAD_PIO_OUT(13, 7, 1),	/* MOSI */
			STM_PAD_PIO_IN(13, 0, 1),	/* MISO */
		},
	},
	[5] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(5, 6, 2),	/* SCK */
			STM_PAD_PIO_OUT(5, 7, 2),	/* MOSI */
			STM_PAD_PIO_IN(5, 5, 2),	/* MISO */
		},
	},
	[6] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(15, 2, 1),	/* SCK */
			STM_PAD_PIO_OUT(15, 3, 1),	/* MOSI */
			STM_PAD_PIO_IN(15, 4, 1),	/* MISO */
		},
	},
};

static struct platform_device stx7108_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd740000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(33), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7108_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd741000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(34), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7108_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd742000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(35), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7108_configure_ssc_*() */
		},
	},
	[3] = {
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd743000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(36), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7108_configure_ssc_*() */
		},
	},
	[4] = {
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd744000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(37), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7108_configure_ssc_*() */
		},
	},
	[5] = {
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd745000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(38), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7108_configure_ssc_*() */
		},
	},
	[6] = {
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd746000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(39), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7108_configure_ssc_*() */
		},
	},
};

static int __initdata stx7108_ssc_configured[ARRAY_SIZE(stx7108_ssc_devices)];

int __init stx7108_configure_ssc_i2c(int ssc, struct stx7108_ssc_config *config)
{
	static int i2c_busnum;
	struct stx7108_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7108_ssc_devices));

	BUG_ON(stx7108_ssc_configured[ssc]);
	stx7108_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx7108_ssc_devices[ssc].name = "i2c-stm";
	stx7108_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx7108_ssc_devices[ssc].dev.platform_data;

	if (ssc == 2) {
		pad_config = stm_pad_config_alloc(2, 0);

		/* SCL */
		switch (config->routing.ssc2.sclk) {
		case stx7108_ssc2_sclk_pio1_3:
			stm_pad_config_add_pio_bidir_named(pad_config,
					1, 3, 2, "SCL");
			break;
		case stx7108_ssc2_sclk_pio14_4:
			stm_pad_config_add_pio_bidir_named(pad_config,
					14, 4, 2, "SCL");
			break;
		default:
			BUG();
			break;
		}

		/* SDA */
		switch (config->routing.ssc2.mtsr) {
		case stx7108_ssc2_mtsr_pio1_4:
			stm_pad_config_add_pio_bidir_named(pad_config,
					1, 4, 2, "SDA");
			break;
		case stx7108_ssc2_mtsr_pio14_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
					14, 5, 2, "SDA");
			break;
		default:
			BUG();
			break;
		}
	} else {
		pad_config = &stx7108_ssc_i2c_pad_configs[ssc];
	}

	plat_data->pad_config = pad_config;

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx7108_ssc_devices[ssc]);

	return i2c_busnum++;
}

int __init stx7108_configure_ssc_spi(int ssc, struct stx7108_ssc_config *config)
{
	static int spi_busnum;
	struct stx7108_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7108_ssc_devices));

	BUG_ON(stx7108_ssc_configured[ssc]);
	stx7108_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx7108_ssc_devices[ssc].name = "spi-stm";
	stx7108_ssc_devices[ssc].id = spi_busnum;

	plat_data = stx7108_ssc_devices[ssc].dev.platform_data;

	if (ssc == 2) {
		pad_config = stm_pad_config_alloc(3, 0);

		/* SCK */
		switch (config->routing.ssc2.sclk) {
		case stx7108_ssc2_sclk_pio1_3:
			stm_pad_config_add_pio_bidir_named(pad_config,
					1, 3, 2, "SCL");
			break;
		case stx7108_ssc2_sclk_pio14_4:
			stm_pad_config_add_pio_bidir_named(pad_config,
					14, 4, 2, "SCL");
			break;
		default:
			BUG();
			break;
		}

		/* MOSI */
		switch (config->routing.ssc2.mtsr) {
		case stx7108_ssc2_mtsr_pio1_4:
			stm_pad_config_add_pio_bidir_named(pad_config,
					1, 4, 2, "SDA");
			break;
		case stx7108_ssc2_mtsr_pio14_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
					14, 5, 2, "SDA");
			break;
		default:
			BUG();
			break;
		}

		/* MISO */
		switch (config->routing.ssc2.mrst) {
		case stx7108_ssc2_mrst_pio1_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
					1, 5, 2, "SDA");
			break;
		case stx7108_ssc2_mrst_pio14_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
					14, 6, 2, "SDA");
			break;
		default:
			BUG();
			break;
		}
	} else {
		pad_config = &stx7108_ssc_spi_pad_configs[ssc];
	}

	plat_data->spi_chipselect = config->spi_chipselect;
	plat_data->pad_config = pad_config;

	platform_device_register(&stx7108_ssc_devices[ssc]);

	return spi_busnum++;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx7108_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd718000, 0x234),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(45), -1),
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

void __init stx7108_configure_lirc(struct stx7108_lirc_config *config)
{
	static int configured;
	struct stx7108_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx7108_lirc_device.dev.platform_data;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = stm_pad_config_alloc(3, 0);
	BUG_ON(!pad_config);

	plat_data->txenabled = config->tx_enabled || config->tx_od_enabled;
	plat_data->pads = pad_config;

	switch (config->rx_mode) {
	case stx7108_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx7108_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, 2, 7, 1);
		break;
	case stx7108_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, 3, 0, 1);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, 3, 1, 1);

	if (config->tx_od_enabled)
		stm_pad_config_add_pio_out(pad_config, 3, 2, 1);

	platform_device_register(&stx7108_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data stx7108_pwm_platform_data = {
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(26, 4, 1),
			},
		},
		[1] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(14, 1, 1),
			},
		},
	},
};

static struct platform_device stx7108_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd710000, 0x68),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(51), -1),
	},
	.dev.platform_data = &stx7108_pwm_platform_data,
};

void __init stx7108_configure_pwm(struct stx7108_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config) {
		stx7108_pwm_platform_data.channel_enabled[0] =
				config->out0_enabled;
		stx7108_pwm_platform_data.channel_enabled[1] =
				config->out1_enabled;
	}

	platform_device_register(&stx7108_pwm_device);
}

/* Low Power Controller ---------------------------------------------------- */

static struct platform_device stx7108_lpc_device = {
	.name		= "stm-rtc",
	.id		= -1,
	.num_resources	= 2,
	.resource	= (struct resource[]){
		STM_PLAT_RESOURCE_MEM(0xfd708000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(6), -1),
	},
	.dev.platform_data = &(struct stm_plat_rtc_lpc) {
		.clk_id = "CLKB_LPC",
		.need_wdt_reset = 1,
		.irq_edge_level = IRQ_TYPE_EDGE_FALLING,
	}
};

static int __init stx7108_add_lpc(void)
{
	return platform_device_register(&stx7108_lpc_device);
}

module_init(stx7108_add_lpc);
