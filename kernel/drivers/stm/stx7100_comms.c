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
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx7100_asc_pad_configs[] = {
	[0] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(0, 0, 1),	/* TX */
			STM_PAD_PIO_IN(0, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(0, 4, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(0, 7, 1, "RTS"),
		},
	},
	[1] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(1, 0, 1),	/* TX */
			STM_PAD_PIO_IN(1, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(1, 4, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(1, 5, 1, "RTS"),
		},
	},
	[2] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(4, 3, 1),	/* TX */
			STM_PAD_PIO_IN(4, 2, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(4, 4, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(4, 5, 1, "RTS"),
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SCIF_PIO_OUT_EN = 0 */
			STM_PAD_SYS_CFG(7, 0, 0, 0),
		},
	},
	[3] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(5, 0, 1),	/* TX */
			STM_PAD_PIO_IN(5, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(5, 2, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(5, 3, 1, "RTS"),
		},
	},
};

static struct platform_device stx7100_asc_devices[] = {
	[0] = {
		.name		= "stm-asc",
		/* .id set in stx7100_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18030000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(123, -1),
			/* DMA Requests set in stx7100_configure_asc() */
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", -1),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", -1),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7100_asc_pad_configs[0],
		},
	},
	[1] = {
		.name		= "stm-asc",
		/* .id set in stx7100_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18031000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(122, -1),
			/* DMA Requests set in stx7100_configure_asc() */
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", -1),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", -1),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7100_asc_pad_configs[1],
		},
	},
	[2] = {
		.name		= "stm-asc",
		/* .id set in stx7100_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18032000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(121, -1),
			/* DMA Requests set in stx7100_configure_asc() */
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", -1),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", -1),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7100_asc_pad_configs[2],
		},
	},
	[3] = {
		.name		= "stm-asc",
		/* .id set in stx7100_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18033000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(120, -1),
			/* DMA Requests set in stx7100_configure_asc() */
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", -1),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", -1),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7100_asc_pad_configs[3],
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
		*stm_asc_configured_devices[ARRAY_SIZE(stx7100_asc_devices)];

void __init stx7100_configure_asc(int asc, struct stx7100_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx7100_asc_devices)];
	static int tty_id;
	struct stx7100_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;
	unsigned int fdma_requests_7100[][2] = {
		[0] = { 14, 18 }, /* rx_half_full, tx_half_empty */
		[1] = { 15, 19 },
		[2] = { 16, 20 },
		[3] = { 17, 21 },
	};
	unsigned int fdma_requests_7109[][2] = {
		[0] = { 12, 16 }, /* rx_half_full, tx_half_empty */
		[1] = { 13, 17 },
		[2] = { 14, 18 },
		[3] = { 15, 19 },
	};

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx7100_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	if (!config->hw_flow_control) {
		stm_pad_set_pio_ignored(&stx7100_asc_pad_configs[asc], "RTS");
		stm_pad_set_pio_ignored(&stx7100_asc_pad_configs[asc], "CTS");
	}

	pdev = &stx7100_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	switch (cpu_data->type) {
	case CPU_STX7100:
		pdev->resource[2].start = fdma_requests_7100[asc][0];
		pdev->resource[2].end = fdma_requests_7100[asc][0];
		pdev->resource[3].start = fdma_requests_7100[asc][1];
		pdev->resource[3].end = fdma_requests_7100[asc][1];
		break;

	case CPU_STX7109:
		pdev->resource[2].start = fdma_requests_7109[asc][0];
		pdev->resource[2].end = fdma_requests_7109[asc][0];
		pdev->resource[3].start = fdma_requests_7109[asc][1];
		pdev->resource[3].end = fdma_requests_7109[asc][1];
		break;

	default:
		BUG();
		break;
	}

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx7100_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx7100_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C/SSC mode */
static struct stm_pad_config stx7100_ssc_i2c_pad_configs[] = {
	[0] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(2, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(2, 1, 1, "SDA"),
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC0_MUX_SEL = 0 (default assignment) */
			STM_PAD_SYS_CFG(7, 1, 1, 0),
			/* DVO_OUT_ON = 0 (SSC not DVO) */
			STM_PAD_SYS_CFG(7, 10, 10, 0),
		},
	},
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(3, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(3, 1, 1, "SDA"),
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC1_MUX_SEL = 0 (default assignment) */
			STM_PAD_SYS_CFG(7, 2, 2, 0),
			/* DVO_OUT_ON = 0 (SSC not DVO) */
			STM_PAD_SYS_CFG(7, 10, 10, 0),
		},
	},
	[2] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(4, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(4, 1, 1, "SDA"),
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC2_MUX_SEL = 0 (separate PIOs) */
			STM_PAD_SYS_CFG(7, 3, 3, 0),
		},
	},
};

/* Pad configuration for SPI/SSC mode */
static struct stm_pad_config stx7100_ssc_spi_pad_configs[] = {
	[0] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 0, 1),	/* SCK */
			STM_PAD_PIO_OUT(2, 1, 1),	/* MOSI */
			STM_PAD_PIO_IN(2, 2, -1),	/* MISO */
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC0_MUX_SEL = 0 (default assignment) */
			STM_PAD_SYS_CFG(7, 1, 1, 0),
			/* DVO_OUT_ON = 0 (SSC not DVO) */
			STM_PAD_SYS_CFG(7, 10, 10, 0),
		},
	},
	[1] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(3, 0, 1),	/* SCK */
			STM_PAD_PIO_OUT(3, 1, 1),	/* MOSI */
			STM_PAD_PIO_IN(3, 2, -1),	/* MISO */
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC1_MUX_SEL = 0 (default assignment) */
			STM_PAD_SYS_CFG(7, 2, 2, 0),
			/* DVO_OUT_ON = 0 (SSC not DVO) */
			STM_PAD_SYS_CFG(7, 10, 10, 0),
		},
	},
};

static struct platform_device stx7100_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx7100_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18040000, 0x110),
			STM_PLAT_RESOURCE_IRQ(119, -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7100_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx7100_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18041000, 0x110),
			STM_PLAT_RESOURCE_IRQ(118, -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7100_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx7100_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18042000, 0x110),
			STM_PLAT_RESOURCE_IRQ(117, -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7100_configure_ssc_*() */
		},
	},
};

static int __initdata stx7100_ssc_configured[ARRAY_SIZE(stx7100_ssc_devices)];

int __init stx7100_configure_ssc_i2c(int ssc)
{
	static int i2c_busnum;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7100_ssc_devices));

	BUG_ON(stx7100_ssc_configured[ssc]);
	stx7100_ssc_configured[ssc] = 1;

	stx7100_ssc_devices[ssc].name = "i2c-stm";
	stx7100_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx7100_ssc_devices[ssc].dev.platform_data;
	plat_data->pad_config = &stx7100_ssc_i2c_pad_configs[ssc];

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx7100_ssc_devices[ssc]);

	return i2c_busnum++;
}

int __init stx7100_configure_ssc_spi(int ssc,
		struct stx7100_ssc_spi_config *config)
{
	static int spi_busnum;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7100_ssc_devices));

	BUG_ON(stx7100_ssc_configured[ssc]);
	stx7100_ssc_configured[ssc] = 1;

	/* SSC2 can't be used in SPI mode - there is no MRST pin available */
	BUG_ON(ssc == 2);

	stx7100_ssc_devices[ssc].name = "spi-stm";
	stx7100_ssc_devices[ssc].id = spi_busnum;

	plat_data = stx7100_ssc_devices[ssc].dev.platform_data;
	if (config)
		plat_data->spi_chipselect = config->chipselect;
	plat_data->pad_config = &stx7100_ssc_spi_pad_configs[ssc];

	platform_device_register(&stx7100_ssc_devices[ssc]);

	return spi_busnum++;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx7100_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0x18018000, 0x234),
		STM_PLAT_RESOURCE_IRQ(125, -1),
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

void __init stx7100_configure_lirc(struct stx7100_lirc_config *config)
{
	static int configured;
	struct stx7100_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx7100_lirc_device.dev.platform_data;
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
	case stx7100_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx7100_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, 3, 3, -1);
		break;
	case stx7100_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, 3, 4, -1);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, 3, 5, 1);

	if (config->tx_od_enabled)
		stm_pad_config_add_pio_out(pad_config, 3, 6, 1);

	platform_device_register(&stx7100_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data stx7100_pwm_platform_data = {
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(4, 6, 1),
			},
			.sysconfs_num = 1,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* SCIF_PIO_OUT_EN = 0
				 * (regular PIO, not the SCIF output) */
				STM_PAD_SYS_CFG(7, 0, 0, 0),
			},
		},
		[1] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(4, 7, 1),
			},
		},
	},
};

static struct platform_device stx7100_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18010000, 0x68),
		STM_PLAT_RESOURCE_IRQ(126, -1),
	},
	.dev.platform_data = &stx7100_pwm_platform_data,
};

void __init stx7100_configure_pwm(struct stx7100_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config) {
		stx7100_pwm_platform_data.channel_enabled[0] =
				config->out0_enabled;
		stx7100_pwm_platform_data.channel_enabled[1] =
				config->out1_enabled;
	}

	platform_device_register(&stx7100_pwm_device);
}

