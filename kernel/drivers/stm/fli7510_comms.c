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
#include <linux/stm/fli7510.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config fli7510_asc_pad_configs[] = {
	[0] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(9, 3, 1),	/* TX */
			STM_PAD_PIO_IN(9, 2, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(9, 1, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(9, 0, 1, "RTS"),
		},
	},
	[1] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(25, 5, 1),	/* TX */
			STM_PAD_PIO_IN(25, 4, -1),	/* RX */
			STM_PAD_PIO_IN_NAMED(25, 3, -1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(25, 2, 1, "RTS"),
		},
	},
	[2] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(25, 7, 0),	/* TX */
			STM_PAD_PIO_IN(25, 6, -1),	/* RX */
		},
	},
};

static struct platform_device fli7510_asc_devices[] = {
	[0] = {
		.name = "stm-asc",
		/* .id set in fli7510_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb30000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(24), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 5),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 6),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &fli7510_asc_pad_configs[0],
		},
	},
	[1] = {
		.name = "stm-asc",
		/* .id set in fli7510_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb31000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(25), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 7),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 8),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &fli7510_asc_pad_configs[1],
		},
	},
	[2] = {
		.name = "stm-asc",
		/* .id set in fli7510_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb32000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(26), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 9),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 10),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &fli7510_asc_pad_configs[2],
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
		*stm_asc_configured_devices[ARRAY_SIZE(fli7510_asc_devices)];

void __init fli7510_configure_asc(int asc, struct fli7510_asc_config *config)
{
	static int configured[ARRAY_SIZE(fli7510_asc_devices)];
	static int tty_id;
	struct fli7510_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(fli7510_asc_devices));

	BUG_ON(configured[asc]++);

	if (!config)
		config = &default_config;

	pdev = &fli7510_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;
	plat_data->txfifo_bug = 1;

	/* ASC2 doesn't have hardware flow control pins at all */
	BUG_ON(asc == 2 && plat_data->hw_flow_control);
	if (asc != 2 && !config->hw_flow_control) {
		struct stm_pad_config *pad_config = plat_data->pad_config;

		stm_pad_set_pio_ignored(pad_config, "RTS");
		stm_pad_set_pio_ignored(pad_config, "CTS");
	}

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init fli7510_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(fli7510_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C mode */
static struct stm_pad_config fli7510_ssc_i2c_pad_configs[] = {
	[0] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(10, 2, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(10, 3, 1, "SDA"),
		},
	},
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(9, 4, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(9, 5, 1, "SDA"),
		},
	},
	[2] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(9, 6, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(9, 7, 1, "SDA"),
		},
	},
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(10, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(10, 1, 1, "SDA"),
		},
	},
	[4] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(17, 2, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(17, 3, 1, "SDA"),
		},
		.sysconfs_num = 2,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* Have to disable SPI boot controller in
			 * order to connect SSC4 to PIOs... */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 13, 13, 0),
			/* Selects the SBAG alternate function on PIO21[6],
			 * PIO21[2],PIO21[3],PIO20[5] and PIO18[2:1] (Ultra)
			 * when "0" : selects MII/RMII/SPI function on
			 *            PIO21/20/18
			 * when "1" : Selects SBAG signals on PIO21/20/18 */
			STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 17, 17, 0),
		},
	},
};

static struct stm_pad_config fli7520_ssc2_i2c_pad_config = {
	.gpios_num = 2,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_BIDIR_NAMED(26, 7, 1, "SCL"),
		STM_PAD_PIO_BIDIR_NAMED(27, 6, 1, "SDA"),
	},
};

static struct stm_pad_config fli7520_ssc4_i2c_pad_config = {
	.gpios_num = 2,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_BIDIR_NAMED(26, 7, 1, "SCL"),
		STM_PAD_PIO_BIDIR_NAMED(27, 6, 1, "SDA"),
	},
	.sysconfs_num = 3,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* Have to disable SPI boot controller in
		 * order to connect SSC4 to PIOs... */
		STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 13, 13, 0),
		/* Selects the SBAG alternate function on PIO21[6],
		 * PIO21[2],PIO21[3],PIO20[5] and PIO18[2:1] (Ultra)
		 * when "0" : selects MII/RMII/SPI function on PIO21/20/18
		 * when "1" : Selects SBAG signals on PIO21/20/18 */
		STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 17, 17, 0),
		/* Selects SSC4 mode (SPI backup) for PIO21[3] (SPI_MOSI)
		 * when "0" : in MTSR mode
		 * when "1" : in MRST mode */
		STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 1, 1, 0),
	},
};

static struct stm_pad_config fli7510_ssc4_spi_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(17, 2, 1),	/* SCK */
		STM_PAD_PIO_OUT(17, 3, 1),	/* MOSI */
		STM_PAD_PIO_IN(17, 5, -1),	/* MISO */
	},
	.sysconfs_num = 2,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* Have to disable SPI boot controller in
		 * order to connect SSC4 to PIOs... */
		STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 13, 13, 0),
		/* Selects the SBAG alternate function on PIO21[6],
		 * PIO21[2],PIO21[3],PIO20[5] and PIO18[2:1] (Ultra)
		 * when "0" : selects MII/RMII/SPI function on PIO21/20/18
		 * when "1" : Selects SBAG signals on PIO21/20/18 */
		STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 17, 17, 0),
	},
};

static struct stm_pad_config fli7520_ssc4_spi_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(21, 2, 1),	/* SCK */
		STM_PAD_PIO_OUT(21, 3, 1),	/* MOSI */
		STM_PAD_PIO_IN(20, 5, -1),	/* MISO */
	},
	.sysconfs_num = 3,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* Have to disable SPI boot controller in
		 * order to connect SSC4 to PIOs... */
		STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 13, 13, 0),
		/* Selects the SBAG alternate function on PIO21[6],
		 * PIO21[2],PIO21[3],PIO20[5] and PIO18[2:1] (Ultra)
		 * when "0" : selects MII/RMII/SPI function on PIO21/20/18
		 * when "1" : Selects SBAG signals on PIO21/20/18 */
		STM_PAD_SYSCONF(CFG_COMMS_CONFIG_2, 17, 17, 0),
		/* Selects SSC4 mode (SPI backup) for PIO21[3] (SPI_MOSI)
		 * when "0" : in MTSR mode
		 * when "1" : in MRST mode */
		STM_PAD_SYSCONF(CFG_COMMS_CONFIG_1, 1, 1, 0),
	},
};

static struct platform_device fli7510_ssc_devices[] = {
	[0] = {
		/* .name & .id set in fli7510_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb40000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(19), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7510_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in fli7510_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb41000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(20), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7510_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in fli7510_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb42000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(21), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7510_configure_ssc_*() */
		},
	},
	[3] = {
		/* .name & .id set in fli7510_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb43000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(22), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7510_configure_ssc_*() */
		},
	},
	[4] = {
		/* .name & .id set in fli7510_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb44000, 0x110),
			/* IRQ set in fli7510_configure_ssc_*() */
			STM_PLAT_RESOURCE_IRQ(-1, -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7510_configure_ssc_*() */
		},
	},
};

static int __initdata fli7510_ssc_configured[ARRAY_SIZE(fli7510_ssc_devices)];

int __init fli7510_configure_ssc_i2c(int ssc)
{
	static int i2c_busnum;
	struct platform_device *dev;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(fli7510_ssc_devices));

	BUG_ON(fli7510_ssc_configured[ssc]);
	fli7510_ssc_configured[ssc] = 1;

	dev = &fli7510_ssc_devices[ssc];

	dev->name = "i2c-stm";
	dev->id = i2c_busnum;

	plat_data = fli7510_ssc_devices[ssc].dev.platform_data;
	if (cpu_data->type != CPU_FLI7510 && ssc == 2)
		plat_data->pad_config = &fli7520_ssc2_i2c_pad_config;
	else if (cpu_data->type != CPU_FLI7510 && ssc == 4)
		plat_data->pad_config = &fli7520_ssc4_i2c_pad_config;
	else
		plat_data->pad_config = &fli7510_ssc_i2c_pad_configs[ssc];

	if (ssc == 4) {
		struct resource *res = platform_get_resource(dev,
				IORESOURCE_IRQ, 0);

		res->start = ILC_IRQ(cpu_data->type == CPU_FLI7510 ? 23 : 47);
		res->end = res->start;
	}

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(dev);

	return i2c_busnum++;
}

int __init fli7510_configure_ssc_spi(int ssc,
		struct fli7510_ssc_spi_config *config)
{
	static int spi_busnum;
	struct platform_device *dev;
	struct stm_plat_ssc_data *plat_data;

	/* Only SSC4 can be used as a SPI device */
	BUG_ON(ssc != 4);

	BUG_ON(fli7510_ssc_configured[ssc]);
	fli7510_ssc_configured[ssc] = 1;

	dev = &fli7510_ssc_devices[ssc];

	dev->name = "spi-stm";
	dev->id = spi_busnum;

	if (ssc == 4) {
		struct resource *res = platform_get_resource(dev,
				IORESOURCE_IRQ, 0);

		res->start = ILC_IRQ(cpu_data->type == CPU_FLI7510 ? 23 : 47);
		res->end = res->start;
	}

	plat_data = fli7510_ssc_devices[ssc].dev.platform_data;
	if (config)
		plat_data->spi_chipselect = config->chipselect;
	if (cpu_data->type == CPU_FLI7510)
		plat_data->pad_config = &fli7510_ssc4_spi_pad_config;
	else
		plat_data->pad_config = &fli7520_ssc4_spi_pad_config;

	platform_device_register(dev);

	return spi_busnum++;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device fli7510_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfdb18000, 0x234),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(28), -1),
	},
	.dev.platform_data = &(struct stm_plat_lirc_data) {
		/* The clock settings will be calculated by
		 * the driver from the system clock */
		.irbclock = 0, /* use current_cpu data */
		.irbclkdiv = 0, /* automatically calculate */
		.irbperiodmult = 0,
		.irbperioddiv = 0,
		.irbontimemult = 0,
		.irbontimediv = 0,
		.irbrxmaxperiod = 0x5000,
		.sysclkdiv = 1,
		.rxpolarity = 1,
		.pads = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(26, 2, -1),
			},
		},
		.rxuhfmode = 0,
	},
};

void __init fli7510_configure_lirc(void)
{
	static int configured;

	BUG_ON(configured++);

	platform_device_register(&fli7510_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data fli7510_pwm_platform_data = {
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(8, 4, 1),
			},
		},
		[1] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(8, 5, 1),
			},
		},
	},
};

/* At the time of writing this code, PWM driver
 * supported only 2 channels of PWM... */
#if 0
		[2] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(8, 6, 1),
			},
		},
		[3] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(8, 7, 1),
			},
		},
#endif

static struct platform_device fli7510_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd010000, 0x68),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x11c0), -1),
	},
	.dev.platform_data = &fli7510_pwm_platform_data,
};

void __init fli7510_configure_pwm(struct fli7510_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config) {
		fli7510_pwm_platform_data.channel_enabled[0] =
				config->out0_enabled;
		/* PWM output 0 is physically unavailable on 7520 & 7530 */
		WARN_ON(config->out0_enabled &&
				(cpu_data->type == CPU_FLI7520 ||
				cpu_data->type == CPU_FLI7530));

		fli7510_pwm_platform_data.channel_enabled[1] =
				config->out1_enabled;

		/* At the time of writing this code, PWM driver
		 * supported only 2 channels of PWM... */

#if 0
		fli7510_pwm_platform_data.channel_enabled[2] =
				config->out2_enabled;
		/* PWM output 0 is physically unavailable on 7520 & 7530 */
		WARN_ON(config->out2_enabled &&
				(cpu_data->type == CPU_FLI7520 ||
				cpu_data->type == CPU_FLI7530));
#endif
		WARN_ON(config->out2_enabled);

#if 0
		fli7510_pwm_platform_data.channel_enabled[3] =
				config->out3_enabled;
#endif
		WARN_ON(config->out3_enabled);
	}

	platform_device_register(&fli7510_pwm_device);
}
