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
#include <linux/stm/stx5206.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx5206_asc_pad_configs[] = {
	[0] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(0, 0, 0),	/* TX */
			STM_PAD_PIO_IN(0, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(0, 4, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(0, 7, 0, "RTS"),
		},
	},
	[1] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 7, 1),	/* TX */
			STM_PAD_PIO_IN(3, 4, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(3, 6, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(3, 5, 1, "RTS"),
		},
	},
	[2] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(1, 2, 0),	/* TX */
			STM_PAD_PIO_IN(1, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(1, 4, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(1, 3, 0, "RTS"),
		},
	},
	[3] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 4, 0),	/* TX */
			STM_PAD_PIO_IN(2, 3, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(2, 5, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(2, 6, 0, "RTS"),
		},
	},
};

static struct platform_device stx5206_asc_devices[] = {
	[0] = {
		.name		= "stm-asc",
		/* .id set in stx5206_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd030000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1160), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 9),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 13),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5206_asc_pad_configs[0],
		},
	},
	[1] = {
		.name		= "stm-asc",
		/* .id set in stx5206_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd031000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1140), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 10),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 14),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5206_asc_pad_configs[1],
		},
	},
	[2] = {
		.name		= "stm-asc",
		/* .id set in stx5206_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd032000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1120), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5206_asc_pad_configs[2],
		},
	},
	[3] = {
		.name		= "stm-asc",
		/* .id set in stx5206_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd033000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1100), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5206_asc_pad_configs[3],
		},
	},
};

/* Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation. */

/* the serial console device */
int __initdata stm_asc_console_device;

/* Platform devices to register */
unsigned int __initdata stm_asc_configured_devices_num;
struct platform_device __initdata
		*stm_asc_configured_devices[ARRAY_SIZE(stx5206_asc_devices)];

void __init stx5206_configure_asc(int asc, struct stx5206_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx5206_asc_devices)];
	static int tty_id;
	struct stx5206_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx5206_asc_devices));

	BUG_ON(configured[asc]++);

	if (!config)
		config = &default_config;

	pdev = &stx5206_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;
	plat_data->txfifo_bug = 1;

	if (!config->hw_flow_control) {
		struct stm_pad_config *pad_config = plat_data->pad_config;

		stm_pad_set_pio_ignored(pad_config, "RTS");
		stm_pad_set_pio_ignored(pad_config, "CTS");
	}

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx5206_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx5206_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C mode */
static struct stm_pad_config stx5206_ssc_i2c_pad_configs[] = {
	/* SSC0 is an internal I2C link - doesn't need any
	 * special treatment here... */
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(2, 0, 0, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(2, 1, 0, "SDA"),
		},
	},
	[2] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(3, 4, 0, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(3, 5, 0, "SDA"),
		},
	},
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(3, 6, 0, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(3, 7, 0, "SDA"),
		},
	},
};

/* Pad configuration for SSC1 in SPI mode */
static struct stm_pad_config stx5206_ssc1_spi_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(2, 0, 0),	/* SCK */
		STM_PAD_PIO_OUT(2, 1, 0),	/* MOSI */
		STM_PAD_PIO_IN(2, 2, -1),	/* MISO */
	},
};

static struct platform_device stx5206_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx5206_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd040000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10e0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx5206_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx5206_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd041000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10c0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx5206_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx5206_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd042000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10a0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx5206_configure_ssc_*() */
		},
	},
	[3] = {
		/* .name & .id set in stx5206_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd043000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1080), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx5206_configure_ssc_*() */
		},
	},
};

static int __initdata stx5206_ssc_configured[ARRAY_SIZE(stx5206_ssc_devices)];

int __init stx5206_configure_ssc_i2c(int ssc)
{
	static int i2c_busnum;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx5206_ssc_devices));

	BUG_ON(stx5206_ssc_configured[ssc]);
	stx5206_ssc_configured[ssc] = 1;

	stx5206_ssc_devices[ssc].name = "i2c-stm";
	stx5206_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx5206_ssc_devices[ssc].dev.platform_data;
	plat_data->pad_config = &stx5206_ssc_i2c_pad_configs[ssc];

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx5206_ssc_devices[ssc]);

	return i2c_busnum++;
}

int __init stx5206_configure_ssc_spi(int ssc,
		struct stx5206_ssc_spi_config *config)
{
	static int spi_busnum;
	struct stm_plat_ssc_data *plat_data;

	/* Only SSC1 can be used as a SPI device */
	BUG_ON(ssc != 1);

	BUG_ON(stx5206_ssc_configured[ssc]);
	stx5206_ssc_configured[ssc] = 1;

	stx5206_ssc_devices[ssc].name = "spi-stm";
	stx5206_ssc_devices[ssc].id = spi_busnum;

	plat_data = stx5206_ssc_devices[ssc].dev.platform_data;
	if (config)
		plat_data->spi_chipselect = config->chipselect;
	plat_data->pad_config = &stx5206_ssc1_spi_pad_config;

	platform_device_register(&stx5206_ssc_devices[ssc]);

	return spi_busnum++;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx5206_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd018000, 0x234),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x11a0), -1),
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

void __init stx5206_configure_lirc(struct stx5206_lirc_config *config)
{
	static int configured;
	struct stx5206_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx5206_lirc_device.dev.platform_data;
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
	case stx5206_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx5206_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, 1, 7, -1);
		break;
	case stx5206_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, 1, 7, -1);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, 3, 3, 0);

	if (config->tx_od_enabled)
		stm_pad_config_add_pio_out(pad_config, 1, 6, 1);

	platform_device_register(&stx5206_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data stx5206_pwm_platform_data = {
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(3, 0, 0),
			},
		},
	},
};

static struct platform_device stx5206_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd010000, 0x68),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x11c0), -1),
	},
	.dev.platform_data = &stx5206_pwm_platform_data,
};

void __init stx5206_configure_pwm(struct stx5206_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	/* Just output 0 is available */
	if (config)
		stx5206_pwm_platform_data.channel_enabled[0] =
				config->out_enabled;

	platform_device_register(&stx5206_pwm_device);
}

/* Low Power Controller ---------------------------------------------------- */

static struct platform_device stx5206_lpc_device = {
	.name           = "stm-rtc",
	.id             = -1,
	.num_resources  = 2,
	.resource       = (struct resource[]){
		STM_PLAT_RESOURCE_MEM(0xfd008000, 0x600),
		STM_PLAT_RESOURCE_IRQ(ILC_EXT_IRQ(7), -1),
	},
	.dev.platform_data = &(struct stm_plat_rtc_lpc) {
		.clk_id = "CLKB_LPC",
		.need_wdt_reset = 1,
		.irq_edge_level = IRQ_TYPE_EDGE_FALLING,
	}
};

static int __init stx5206_add_lpc(void)
{
	return platform_device_register(&stx5206_lpc_device);
}

module_init(stx5206_add_lpc);
