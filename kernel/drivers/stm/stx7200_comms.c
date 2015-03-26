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
#include <linux/stm/pad.h>
#include <linux/stm/stx7200.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx7200_asc_pad_configs[] = {
	[0] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(0, 0, 1),	/* TX */
			STM_PAD_PIO_IN(0, 1, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(0, 4, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(0, 7, 1, "RTS"),
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* Route UART0/1 and MPX instead of DVP to pins:
			 * CONF_PAD_PIO[5] = 0 */
			STM_PAD_SYS_CFG(7, 29, 29, 0),
			/* Route UART0 instead of PDES to pins:
			 * PDES_SCMUX_OUT = 0 */
			STM_PAD_SYS_CFG(0, 0, 0, 0),
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
		.sysconfs_num = 2, /* !!! see stx7200_configure_asc() */
		.sysconfs = (struct stm_pad_sysconf []) {
			/* CONF_PAD_PIO[5] = 0 - route UART0/1 and
			 * MPX instead of DVP to pins */
			STM_PAD_SYS_CFG(7, 29, 29, 0),
			/* Route UART1 RTS/CTS instead of dvo to pins.
			 * CONF_PAD_PIO[0] = 0 */
			STM_PAD_SYS_CFG(7, 24, 24, 0),
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
		.sysconfs_num = 4, /* !!! see stx7200_configure_asc() */
		.sysconfs = (struct stm_pad_sysconf []) {
			/* Route UART2 (in and out) and PWM_OUT0 instead of
			 * SCI to pins: SSC2_MUX_SEL = 0 */
			STM_PAD_SYS_CFG(7, 2, 2, 0),
			/* Route UART2&3/SCI outputs instead of DVP to pins.
			 * CONF_PAD_PIO[1]=0 */
			STM_PAD_SYS_CFG(7, 25, 25, 0),
			/* No idea, more routing:
			 * CONF_PAD_PIO[0] = 0 */
			STM_PAD_SYS_CFG(7, 24, 24, 0),
			/* Route UART2&3 RTS/CTS or SCI inputs instead of DVP
			 * to pins: CONF_PAD_DVP = 0 */
			STM_PAD_SYS_CFG(40, 16, 16, 0),
		},
	},
	[3] = {
		.gpios_num = 4, /* !!! see stx7200_configure_asc() */
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(5, 4, 1),	/* TX */
			STM_PAD_PIO_IN(5, 3, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(5, 5, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(5, 6, 1, "RTS"),
		},
		.sysconfs_num = 2, /* !!! see stx7200_configure_asc() */
		.sysconfs = (struct stm_pad_sysconf []) {
			/* Route UART3 (in and out) instead of SCI to pins:
			 * SSC3_MUX_SEL = 0 */
			STM_PAD_SYS_CFG(7, 3, 3, 0),
			/* No idea, more routing (RTS/CTS):
			 * CONF_PAD_PIO[4] = 0 */
			STM_PAD_SYS_CFG(7, 28, 28, 0),
		},
	},
};

static struct platform_device stx7200_asc_devices[] = {
	[0] = {
		.name		= "stm-asc",
		/* .id set in stx7200_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd030000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(104), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 19),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 23),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7200_asc_pad_configs[0],
		},
	},
	[1] = {
		.name		= "stm-asc",
		/* .id set in stx7200_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd031000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(105), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 20),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 24),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7200_asc_pad_configs[1],
		},
	},
	[2] = {
		.name		= "stm-asc",
		/* .id set in stx7200_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd032000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(106), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 21),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 25),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7200_asc_pad_configs[2],
		},
	},
	[3] = {
		.name		= "stm-asc",
		/* .id set in stx7200_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd033000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(107), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 22),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 26),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7200_asc_pad_configs[3],
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
		*stm_asc_configured_devices[ARRAY_SIZE(stx7200_asc_devices)];

void __init stx7200_configure_asc(int asc, struct stx7200_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx7200_asc_devices)];
	static int tty_id;
	struct stx7200_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx7200_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	if (!config->hw_flow_control) {
		struct stm_pad_config *pad_config =
				&stx7200_asc_pad_configs[asc];

		/* Don't claim RTS/CTS pads */
		stm_pad_set_pio_ignored(pad_config, "RTS");
		stm_pad_set_pio_ignored(pad_config, "CTS");

		/* sysconf values responsible for RTS/CTS routing
		 * are defined as the last ones... */
		if (asc == 1 || asc == 2 || asc == 3)
			pad_config->sysconfs_num--;
	}

	pdev = &stx7200_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx7200_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx7200_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C mode */
static struct stm_pad_config stx7200_ssc_i2c_pad_configs[] = {
	[0] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(2, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(2, 1, 1, "SDA"),
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC0_MUX_SEL = 0 (default assignment) */
			STM_PAD_SYS_CFG(7, 0, 0, 0),
		},
	},
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(3, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(3, 1, 1, "SDA"),
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC1_MUX_SEL = 0 (default assignment) */
			STM_PAD_SYS_CFG(7, 1, 1, 0),
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
			STM_PAD_SYS_CFG(7, 2, 2, 0),
		},
	},
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(5, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(5, 1, 1, "SDA"),
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC3_MUX_SEL = 0 (separate PIOs) */
			STM_PAD_SYS_CFG(7, 3, 3, 0),
		},
	},
	[4] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(7, 6, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(7, 7, 1, "SDA"),
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC4_MUX_SEL = 0 (separate PIOs) */
			STM_PAD_SYS_CFG(7, 4, 4, 0),
		},
	},
};

/* Pad configuration for SPI mode */
static struct stm_pad_config stx7200_ssc_spi_pad_configs[] = {
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
			STM_PAD_SYS_CFG(7, 0, 0, 0),
			/* !!FIXME!! For some reason, nand_rb signal (PIO2[7])
			 * must be disabled for SSC0/SPI to get input.
			 * CONF_PAD_NAND_FLASH = 0 */
			STM_PAD_SYS_CFG(7, 15, 15, 0),
		},
	},
	[1] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(3, 0, 1),	/* SCK */
			STM_PAD_PIO_OUT(3, 1, 1),	/* MOSI */
			STM_PAD_PIO_IN(3, 2, -1),	/* MISO */
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC1_MUX_SEL = 0 (default assignment) */
			STM_PAD_SYS_CFG(7, 1, 1, 0),
		},
	},
	[3] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(5, 0, 1),	/* SCK */
			STM_PAD_PIO_OUT(5, 1, 1),	/* MOSI */
			STM_PAD_PIO_IN(5, 2, -1),	/* MISO */
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* SSC3_MUX_SEL = 0 (separate PIOs) */
			STM_PAD_SYS_CFG(7, 3, 3, 0),
		},
	},
};

static struct platform_device stx7200_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx7200_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd040000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(108), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7200_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx7200_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd041000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(109), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7200_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx7200_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd042000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(110), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7200_configure_ssc_*() */
		},
	},
	[3] = {
		/* .name & .id set in stx7200_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd043000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(111), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7200_configure_ssc_*() */
		},
	},
	[4] = {
		/* .name & .id set in stx7200_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd044000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(112), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7200_configure_ssc_*() */
		},
	},
};

static int __initdata stx7200_ssc_configured[ARRAY_SIZE(stx7200_ssc_devices)];

int __init stx7200_configure_ssc_i2c(int ssc)
{
	static int i2c_busnum;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7200_ssc_devices));

	BUG_ON(stx7200_ssc_configured[ssc]);
	stx7200_ssc_configured[ssc] = 1;

	stx7200_ssc_devices[ssc].name = "i2c-stm";
	stx7200_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx7200_ssc_devices[ssc].dev.platform_data;
	plat_data->pad_config = &stx7200_ssc_i2c_pad_configs[ssc];

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx7200_ssc_devices[ssc]);

	return i2c_busnum++;
}

int __init stx7200_configure_ssc_spi(int ssc,
		struct stx7200_ssc_spi_config *config)
{
	static int spi_busnum;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7200_ssc_devices));

	BUG_ON(stx7200_ssc_configured[ssc]);
	stx7200_ssc_configured[ssc] = 1;

	/* These two SSC can't be used in SPI mode - there is no
	 * MRST pin available */
	BUG_ON(ssc == 2 || ssc == 4);

	stx7200_ssc_devices[ssc].name = "spi-stm";
	stx7200_ssc_devices[ssc].id = spi_busnum;

	plat_data = stx7200_ssc_devices[ssc].dev.platform_data;
	if (config)
		plat_data->spi_chipselect = config->chipselect;
	plat_data->pad_config = &stx7200_ssc_spi_pad_configs[ssc];

	platform_device_register(&stx7200_ssc_devices[ssc]);

	return spi_busnum++;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx7200_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd018000, 0x234),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(116), -1),
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

void __init stx7200_configure_lirc(struct stx7200_lirc_config *config)
{
	static int configured;
	struct stx7200_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx7200_lirc_device.dev.platform_data;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = stm_pad_config_alloc(3, 1);
	BUG_ON(!pad_config);

	plat_data->txenabled = config->tx_enabled || config->tx_od_enabled;
	plat_data->pads = pad_config;

	switch (config->rx_mode) {
	case stx7200_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx7200_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, 3, 3, -1);
		break;
	case stx7200_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, 3, 4, -1);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, 3, 5, 1);

	if (config->tx_od_enabled) {
		stm_pad_config_add_pio_out(pad_config, 3, 6, 1);
		stm_pad_config_add_sys_cfg(pad_config, 20, 21, 21, 1);
	};

	platform_device_register(&stx7200_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data stx7200_pwm_platform_data = {
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(4, 6, 1),
			},
			.sysconfs_num = 1,
			.sysconfs = (struct stm_pad_sysconf []) {
				/* Route UART2 and PWM_OUT0 instead of
				 * SCI to pins: SSC2_MUX_SEL = 0 */
				STM_PAD_SYS_CFG(7, 2, 2, 0),
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

static struct platform_device stx7200_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd010000, 0x68),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(114), -1),
	},
	.dev.platform_data = &stx7200_pwm_platform_data,
};

void __init stx7200_configure_pwm(struct stx7200_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config) {
		stx7200_pwm_platform_data.channel_enabled[0] =
				config->out0_enabled;
		stx7200_pwm_platform_data.channel_enabled[1] =
				config->out1_enabled;
	}

	platform_device_register(&stx7200_pwm_device);
}

/* Low Power Controller ---------------------------------------------------- */

static struct platform_device stx7200_lpc_device = {
	.name           = "stm-rtc",
	.id             = -1,
	.num_resources  = 2,
	.resource       = (struct resource[]){
		STM_PLAT_RESOURCE_MEM(0xfd008000, 0x600),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(120), -1),
	},
	.dev.platform_data = &(struct stm_plat_rtc_lpc) {
		.clk_id = "CLKB_LPC",
		.need_wdt_reset = 1,
		.irq_edge_level = IRQ_TYPE_EDGE_FALLING,
	}
};

static int __init stx7200_add_lpc(void)
{
	return platform_device_register(&stx7200_lpc_device);
}

module_init(stx7200_add_lpc);
