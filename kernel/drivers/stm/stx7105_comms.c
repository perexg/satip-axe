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
#include <linux/stm/stx7105.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx7105_asc0_pio0_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(0, 0, 4),	/* TX */
		STM_PAD_PIO_IN(0, 1, -1),	/* RX */
		STM_PAD_PIO_IN_NAMED(0, 4, -1, "CTS"),
		STM_PAD_PIO_OUT_NAMED(0, 3, 4, "RTS"),
	},
};

static struct stm_pad_config stx7105_asc1_pio1_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(1, 0, 4),	/* TX */
		STM_PAD_PIO_IN(1, 1, -1),	/* RX */
		STM_PAD_PIO_IN_NAMED(1, 4, -1, "CTS"),
		STM_PAD_PIO_OUT_NAMED(1, 3, 4, "RTS"),
	},
};

static struct stm_pad_config stx7105_asc2_pio4_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(4, 0, 3),	/* TX */
		STM_PAD_PIO_IN(4, 1, -1),	/* RX */
		STM_PAD_PIO_IN_NAMED(4, 2, -1, "CTS"),
		STM_PAD_PIO_OUT_NAMED(4, 3, 3, "RTS"),
	},
	.sysconfs_num = 2,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* uart2_rxd_src_select: 0 = PIO4.1, 1 = PIO12.1 */
		STM_PAD_SYS_CFG(7, 1, 1, 0),
		/* uart2_cts_src_select: 0 = PIO4.2, 1 = PIO12.2 */
		STM_PAD_SYS_CFG(7, 2, 2, 0),
	},
};
static struct stm_pad_config stx7106_asc2_pio12_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(12, 0, 5),	/* TX */
		STM_PAD_PIO_IN(12, 1, -1),	/* RX */
		STM_PAD_PIO_IN_NAMED(12, 2, -1, "CTS"),
		STM_PAD_PIO_OUT_NAMED(12, 3, 5, "RTS"),
	},
	.sysconfs_num = 2,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* uart2_rxd_src_select:
		 * 0 = PIO4.1, 1 = PIO12.1, 2 = PIO14.1 */
		STM_PAD_SYS_CFG(7, 1, 2, 1),
		/* uart2_cts_src_select:
		 * 0 = PIO4.2, 1 = PIO12.2, 2 = PIO14.2  */
		STM_PAD_SYS_CFG(16, 1, 2, 1),
	},
};
static struct stm_pad_config stx7105_asc2_pio12_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(12, 0, 5),	/* TX */
		STM_PAD_PIO_IN(12, 1, -1),	/* RX */
		STM_PAD_PIO_IN_NAMED(12, 2, -1, "CTS"),
		STM_PAD_PIO_OUT_NAMED(12, 3, 5, "RTS"),
	},
	.sysconfs_num = 2,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* uart2_rxd_src_select: 0 = PIO4.1, 1 = PIO12.1 */
		STM_PAD_SYS_CFG(7, 1, 1, 1),
		/* uart2_cts_src_select: 0 = PIO4.2, 1 = PIO12.2 */
		STM_PAD_SYS_CFG(7, 2, 2, 1),
	},
};

static struct stm_pad_config stx7105_asc3_pio5_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(5, 0, 3),	/* TX */
		STM_PAD_PIO_IN(5, 1, -1),	/* RX */
		STM_PAD_PIO_IN_NAMED(5, 3, -1, "CTS"),
		STM_PAD_PIO_OUT_NAMED(5, 2, 3, "RTS"),
	},
};

static struct platform_device stx7105_asc_devices[] = {
	[0] = {
		.name = "stm-asc",
		/* .id set in stx7105_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd030000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1160), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7105_asc0_pio0_pad_config,
		},
	},
	[1] = {
		.name = "stm-asc",
		/* .id set in stx7105_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd031000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1140), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7105_asc1_pio1_pad_config
		},
	},
	[2] = {
		.name = "stm-asc",
		/* .id set in stx7105_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd032000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1120), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			/* .pad_config set in stx7105_configure_asc() */
		},
	},
	[3] = {
		.name = "stm-asc",
		/* .id set in stx7105_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd033000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1100), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7105_asc3_pio5_pad_config,
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
		*stm_asc_configured_devices[ARRAY_SIZE(stx7105_asc_devices)];

void __init stx7105_configure_asc(int asc, struct stx7105_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx7105_asc_devices)];
	static int tty_id;
	struct stx7105_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx7105_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stx7105_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;

	if (cpu_data->type == CPU_STX7106)
		plat_data->txfifo_bug = 1;

	if (asc == 2) {
		switch (config->routing.asc2) {
		case stx7105_asc2_pio4:
			plat_data->pad_config = &stx7105_asc2_pio4_pad_config;
			break;
		case stx7105_asc2_pio12:
			if (cpu_data->type == CPU_STX7106)
				plat_data->pad_config =
					&stx7106_asc2_pio12_pad_config;
			else
				plat_data->pad_config =
					&stx7105_asc2_pio12_pad_config;
			break;
		default:
			BUG();
			break;
		}
	}

	if (!config->hw_flow_control) {
		struct stm_pad_config *pad_config = plat_data->pad_config;

		stm_pad_set_pio_ignored(pad_config, "RTS");
		stm_pad_set_pio_ignored(pad_config, "CTS");

		if (asc == 2)
			pad_config->sysconfs_num--;
	}

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx7105_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx7105_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C mode */
static struct stm_pad_config stx7105_ssc_i2c_pad_configs[] = {
	[0] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(2, 2, 3, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(2, 3, 3, "SDA"),
		},
	},
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(2, 5, 3, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(2, 6, 3, "SDA"),
		},
	},
	/* Configurations for SSC2 & SSC3 are created in
	 * stx7105_configure_ssc_*(), according to passed routing
	 * information */
};

/* Pad configuration for SPI mode */
static struct stm_pad_config stx7105_ssc_spi_pad_configs[] = {
	[0] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 2, 3),	/* SCK */
			STM_PAD_PIO_OUT(2, 3, 3),	/* MOSI */
			STM_PAD_PIO_IN(2, 4, -1),	/* MISO */
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* ssc0_mrst_in_sel: 0 = PIO2.3, 1 = PIO2.4 */
			STM_PAD_SYS_CFG(16, 0, 0, 1),
		},
	},
	[1] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 5, 3),	/* SCK */
			STM_PAD_PIO_OUT(2, 6, 3),	/* MOSI */
			STM_PAD_PIO_IN(2, 7, -1),	/* MISO */
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* ssc1_mrst_in_sel: 0 = PIO2.6, 1 = PIO2.7 */
			STM_PAD_SYS_CFG(16, 3, 3, 1),
		},
	},
	/* Configurations for SSC2 & SSC3 are created in
	 * stx7105_configure_ssc_*(), according to passed routing
	 * information */
};

static struct platform_device stx7105_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx7105_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd040000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10e0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7105_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx7105_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd041000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10c0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7105_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx7105_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd042000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10a0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7105_configure_ssc_*() */
		},
	},
	[3] = {
		/* .name & .id set in stx7105_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd043000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1080), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7105_configure_ssc_*() */
		},
	},
};

static int __initdata stx7105_ssc_configured[ARRAY_SIZE(stx7105_ssc_devices)];

int __init stx7105_configure_ssc_i2c(int ssc, struct stx7105_ssc_config *config)
{
	static int i2c_busnum;
	struct stx7105_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7105_ssc_devices));

	BUG_ON(stx7105_ssc_configured[ssc]);
	stx7105_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx7105_ssc_devices[ssc].name = "i2c-stm";
	stx7105_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx7105_ssc_devices[ssc].dev.platform_data;

	switch (ssc) {
	case 0:
	case 1:
		pad_config = &stx7105_ssc_i2c_pad_configs[ssc];
		break;
	case 2:
		pad_config = stm_pad_config_alloc(2, 2);

		/* SCL */
		switch (config->routing.ssc2.sclk) {
		case stx7105_ssc2_sclk_pio2_4: /* 7106 only! */
			BUG_ON(cpu_data->type != CPU_STX7106);
			stm_pad_config_add_pio_bidir_named(pad_config,
					2, 4, 2, "SCL");
			/* ssc2_sclk_in: 00 = PIO2.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 0);

			break;
		case stx7105_ssc2_sclk_pio3_4:
			stm_pad_config_add_pio_bidir_named(pad_config,
					3, 4, 2, "SCL");
			/* ssc2_sclk_in: 01 = PIO3.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 1);
			break;
		case stx7105_ssc2_sclk_pio12_0:
			stm_pad_config_add_pio_bidir_named(pad_config,
					12, 0, 3, "SCL");
			/* ssc2_sclk_in: 10 = PIO12.0 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 2);
			break;
		case stx7105_ssc2_sclk_pio13_4:
			stm_pad_config_add_pio_bidir_named(pad_config,
					13, 4, 2, "SCL");
			/* ssc2_sclk_in: 11 = PIO13.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 3);
			break;
		}

		/* SDA */
		switch (config->routing.ssc2.mtsr) {
		case stx7105_ssc2_mtsr_pio2_0:
			stm_pad_config_add_pio_bidir_named(pad_config,
					2, 0, 3, "SDA");
			/* ssc2_mtsr_in: 00 = PIO2.0 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 9, 10, 0);
			break;
		case stx7105_ssc2_mtsr_pio3_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
					3, 5, 2, "SDA");
			/* ssc2_mtsr_in: 01 = PIO3.5 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 9, 10, 1);
			break;
		case stx7105_ssc2_mtsr_pio12_1:
			stm_pad_config_add_pio_bidir_named(pad_config,
					12, 1, 3, "SDA");
			/* ssc2_mtsr_in: 10 = PIO12.1 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 9, 10, 2);
			break;
		case stx7105_ssc2_mtsr_pio13_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
					13, 5, 2, "SDA");
			/* ssc2_mtsr_in: 11 = PIO13.5 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 9, 10, 3);
			break;
		}

		break;
	case 3:
		pad_config = stm_pad_config_alloc(2, 2);

		/* SCL */
		switch (config->routing.ssc3.sclk) {
		case stx7105_ssc3_sclk_pio2_7: /* 7106 only! */
			BUG_ON(cpu_data->type != CPU_STX7106);
			stm_pad_config_add_pio_bidir_named(pad_config,
					2, 7, 2, "SCL");
			/* ssc3_sclk_in: 00 = PIO2.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 0);
			break;
		case stx7105_ssc3_sclk_pio3_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
					3, 6, 2, "SCL");
			/* ssc3_sclk_in: 01 = PIO3.6 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 1);
			break;
		case stx7105_ssc3_sclk_pio13_2:
			stm_pad_config_add_pio_bidir_named(pad_config,
					13, 2, 4, "SCL");
			/* ssc3_sclk_in: 10 = PIO13.2 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 2);
			break;
		case stx7105_ssc3_sclk_pio13_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
					13, 6, 2, "SCL");
			/* ssc3_sclk_in: 11 = PIO13.6 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 3);
			break;
		}

		/* SDA */
		switch (config->routing.ssc3.mtsr) {
		case stx7105_ssc3_mtsr_pio2_1:
			stm_pad_config_add_pio_bidir_named(pad_config,
					2, 1, 3, "SDA");
			/* ssc3_mtsr_in: 00 = PIO2.1 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 16, 17, 0);
			break;
		case stx7105_ssc3_mtsr_pio3_7:
			stm_pad_config_add_pio_bidir_named(pad_config,
					3, 7, 2, "SDA");
			/* ssc3_mtsr_in: 01 = PIO3.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 16, 17, 1);
			break;
		case stx7105_ssc3_mtsr_pio13_3:
			stm_pad_config_add_pio_bidir_named(pad_config,
					13, 3, 4, "SDA");
			/* ssc3_mtsr_in: 10 = PIO13.3 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 16, 17, 2);
			break;
		case stx7105_ssc3_mtsr_pio13_7:
			stm_pad_config_add_pio_bidir_named(pad_config,
					13, 7, 2, "SDA");
			/* ssc3_mtsr_in: 11 = PIO13.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 16, 17, 3);
			break;
		}

		break;
	default:
		BUG();
		pad_config = NULL; /* Keep the compiler happy ;-) */
		break;
	}

	plat_data->pad_config = pad_config;

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx7105_ssc_devices[ssc]);

	return i2c_busnum++;
}

int __init stx7105_configure_ssc_spi(int ssc, struct stx7105_ssc_config *config)
{
	static int spi_busnum;
	struct stx7105_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7105_ssc_devices));

	BUG_ON(stx7105_ssc_configured[ssc]);
	stx7105_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx7105_ssc_devices[ssc].name = "spi-stm";
	stx7105_ssc_devices[ssc].id = spi_busnum;

	plat_data = stx7105_ssc_devices[ssc].dev.platform_data;

	switch (ssc) {
	case 0:
	case 1:
		pad_config = &stx7105_ssc_spi_pad_configs[ssc];
		break;
	case 2:
		pad_config = stm_pad_config_alloc(3, 2);

		/* SCK */
		switch (config->routing.ssc2.sclk) {
		case stx7105_ssc2_sclk_pio2_4: /* 7106 only! */
			BUG_ON(cpu_data->type != CPU_STX7106);
			stm_pad_config_add_pio_out(pad_config, 2, 4, 2);
			/* ssc2_sclk_in: 00 = PIO2.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 0);
			break;
		case stx7105_ssc2_sclk_pio3_4:
			stm_pad_config_add_pio_out(pad_config, 3, 4, 2);
			/* ssc2_sclk_in: 01 = PIO3.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 1);
			break;
		case stx7105_ssc2_sclk_pio12_0:
			stm_pad_config_add_pio_out(pad_config, 12, 0, 3);
			/* ssc2_sclk_in: 10 = PIO12.0 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 2);
			break;
		case stx7105_ssc2_sclk_pio13_4:
			stm_pad_config_add_pio_out(pad_config, 13, 4, 2);
			/* ssc2_sclk_in: 11 = PIO13.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 3);
			break;
		}

		/* MOSI */
		switch (config->routing.ssc2.mtsr) {
		case stx7105_ssc2_mtsr_pio2_0:
			stm_pad_config_add_pio_out(pad_config, 2, 0, 3);
			break;
		case stx7105_ssc2_mtsr_pio3_5:
			stm_pad_config_add_pio_out(pad_config, 3, 5, 2);
			break;
		case stx7105_ssc2_mtsr_pio12_1:
			stm_pad_config_add_pio_out(pad_config, 12, 1, 3);
			break;
		case stx7105_ssc2_mtsr_pio13_5:
			stm_pad_config_add_pio_out(pad_config, 13, 5, 2);
			break;
		}

		/* MISO */
		switch (config->routing.ssc2.mrst) {
		case stx7105_ssc2_mrst_pio2_0:
			stm_pad_config_add_pio_in(pad_config, 2, 0, -1);
			/* ssc2_mrst_in: 00 = PIO2.0 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 7, 8, 0);
			break;
		case stx7105_ssc2_mrst_pio3_5:
			stm_pad_config_add_pio_in(pad_config, 3, 5, -1);
			/* ssc2_mrst_in: 01 = PIO3.5 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 7, 8, 1);
			break;
		case stx7105_ssc2_mrst_pio12_1:
			stm_pad_config_add_pio_in(pad_config, 12, 1, -1);
			/* ssc2_mrst_in: 10 = PIO12.1 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 7, 8, 2);
			break;
		case stx7105_ssc2_mrst_pio13_5:
			stm_pad_config_add_pio_in(pad_config, 13, 5, -1);
			/* ssc2_mrst_in: 11 = PIO13.5 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 7, 8, 3);
			break;
		}

		break;
	case 3:
		pad_config = stm_pad_config_alloc(3, 2);

		/* SCK */
		switch (config->routing.ssc3.sclk) {
		case stx7105_ssc3_sclk_pio2_7: /* 7106 only! */
			BUG_ON(cpu_data->type != CPU_STX7106);
			stm_pad_config_add_pio_out(pad_config, 2, 7, 2);
			/* ssc3_sclk_in: 00 = PIO2.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 0);
			break;
		case stx7105_ssc3_sclk_pio3_6:
			stm_pad_config_add_pio_out(pad_config, 3, 6, 2);
			/* ssc3_sclk_in: 00 = PIO3.6 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 0);
			break;
		case stx7105_ssc3_sclk_pio13_2:
			stm_pad_config_add_pio_out(pad_config, 13, 2, 4);
			/* ssc3_sclk_in: 01 = PIO13.2 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 1);
			break;
		case stx7105_ssc3_sclk_pio13_6:
			stm_pad_config_add_pio_out(pad_config, 13, 6, 2);
			/* ssc3_sclk_in: 1x = PIO13.6 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 2);
			break;
		}

		/* MOSI */
		switch (config->routing.ssc3.mtsr) {
		case stx7105_ssc3_mtsr_pio2_1:
			stm_pad_config_add_pio_out(pad_config, 2, 1, 3);
			break;
		case stx7105_ssc3_mtsr_pio3_7:
			stm_pad_config_add_pio_out(pad_config, 3, 7, 3);
			break;
		case stx7105_ssc3_mtsr_pio13_3:
			stm_pad_config_add_pio_out(pad_config, 13, 3, 4);
			break;
		case stx7105_ssc3_mtsr_pio13_7:
			stm_pad_config_add_pio_out(pad_config, 13, 7, 2);
			break;
		}

		/* MISO */
		switch (config->routing.ssc3.mrst) {
		case stx7105_ssc3_mrst_pio2_1:
			stm_pad_config_add_pio_in(pad_config, 2, 1, -1);
			/* ssc3_mrst_in: 00 = PIO2.1 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 14, 15, 0);
			break;
		case stx7105_ssc3_mrst_pio3_7:
			stm_pad_config_add_pio_in(pad_config, 3, 7, -1);
			/* ssc3_mrst_in: 01 = PIO3.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 14, 15, 1);
			break;
		case stx7105_ssc3_mrst_pio13_3:
			stm_pad_config_add_pio_in(pad_config, 13, 3, -1);
			/* ssc3_mrst_in: 10 = PIO13.3 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 14, 15, 2);
			break;
		case stx7105_ssc3_mrst_pio13_7:
			stm_pad_config_add_pio_in(pad_config, 13, 7, -1);
			/* ssc3_mrst_in: 11 = PIO13.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 14, 15, 3);
			break;
		}

		break;
	default:
		BUG();
		pad_config = NULL; /* Keep the compiler happy ;-) */
		break;
	}

	plat_data->spi_chipselect = config->spi_chipselect;
	plat_data->pad_config = pad_config;

	platform_device_register(&stx7105_ssc_devices[ssc]);

	return spi_busnum++;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx7105_lirc_device = {
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

void __init stx7105_configure_lirc(struct stx7105_lirc_config *config)
{
	static int configured;
	struct stx7105_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx7105_lirc_device.dev.platform_data;
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
	case stx7105_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx7105_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, 3, 0, -1);
		break;
	case stx7105_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, 3, 1, -1);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, 3, 2, 3);

	if (config->tx_od_enabled)
		stm_pad_config_add_pio_out(pad_config, 3, 3, 3);

	platform_device_register(&stx7105_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_pad_config stx7105_pwm_out0_pio4_4_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(4, 4, 3),
	},
};

static struct stm_pad_config stx7105_pwm_out0_pio13_0_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(13, 0, 3),
	},
};

static struct stm_pad_config stx7105_pwm_out1_pio4_5_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(4, 5, 3),
	},
};

static struct stm_pad_config stx7105_pwm_out1_pio13_1_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(13, 1, 3),
	},
};

/* Set in stx7105_configure_pwm() */
static struct stm_plat_pwm_data stx7105_pwm_platform_data;

static struct platform_device stx7105_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd010000, 0x68),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x11c0), -1),
	},
	.dev.platform_data = &stx7105_pwm_platform_data,
};

void __init stx7105_configure_pwm(struct stx7105_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config) {
		switch (config->out0) {
		case stx7105_pwm_out0_disabled:
			/* Nothing to do... */
			break;
		case stx7105_pwm_out0_pio4_4:
			stx7105_pwm_platform_data.channel_enabled[0] = 1;
			stx7105_pwm_platform_data.channel_pad_config[0]	=
					&stx7105_pwm_out0_pio4_4_pad_config;
			break;
		case stx7105_pwm_out0_pio13_0:
			stx7105_pwm_platform_data.channel_enabled[0] = 1;
			stx7105_pwm_platform_data.channel_pad_config[0]	=
					&stx7105_pwm_out0_pio13_0_pad_config;
			break;
		default:
			BUG();
			break;
		}

		switch (config->out1) {
		case stx7105_pwm_out1_disabled:
			/* Nothing to do... */
			break;
		case stx7105_pwm_out1_pio4_5:
			stx7105_pwm_platform_data.channel_enabled[1] = 1;
			stx7105_pwm_platform_data.channel_pad_config[1]	=
					&stx7105_pwm_out1_pio4_5_pad_config;
			break;
		case stx7105_pwm_out1_pio13_1:
			stx7105_pwm_platform_data.channel_enabled[1] = 1;
			stx7105_pwm_platform_data.channel_pad_config[1]	=
					&stx7105_pwm_out1_pio13_1_pad_config;
			break;
		default:
			BUG();
			break;
		}
	}

	platform_device_register(&stx7105_pwm_device);
}

/* Low Power Controller ---------------------------------------------------- */

static struct platform_device stx7105_lpc_device = {
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

static int __init stx7105_add_lpc(void)
{
	return platform_device_register(&stx7105_lpc_device);
}

module_init(stx7105_add_lpc);
