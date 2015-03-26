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
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/emi.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stx7105.h>
#include <linux/clk.h>
#include <asm/irq-ilc.h>


/* EMI resources ---------------------------------------------------------- */

static int __initdata stx7105_emi_bank_configured[EMI_BANKS];

static void stx7105_emi_power(struct stm_device_state *device_state,
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

static struct platform_device stx7105_emi = {
	.name = "emi",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 128 * 1024 * 1024),
		STM_PLAT_RESOURCE_MEM(0xfe700000, 0x874),
	},
	.dev.platform_data = &(struct stm_device_config){
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYS_CFG(32, 1, 1, "EMI_PWR"),
			STM_DEVICE_SYS_STA(15, 1, 1, "EMI_ACK"),
		},
		.power = stx7105_emi_power,
	}
};



/* PATA resources --------------------------------------------------------- */

/* EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */
static struct resource stx7105_pata_resources[] = {
	/* I/O base: CS1=N, CS0=A */
	[0] = STM_PLAT_RESOURCE_MEM(1 << 20, 8 << 17),
	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
	[1] = STM_PLAT_RESOURCE_MEM((1 << 21) + (6 << 17), 4),
	/* IRQ */
	[2] = STM_PLAT_RESOURCE_IRQ(-1, -1),
};

static struct platform_device stx7105_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx7105_pata_resources),
	.resource	= stx7105_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift = 17,
	},
};

void __init stx7105_configure_pata(struct stx7105_pata_config *config)
{
	unsigned long bank_base;

	if (!config) {
		BUG();
		return;
	}

	BUG_ON(config->emi_bank < 0 || config->emi_bank >= EMI_BANKS);
	BUG_ON(stx7105_emi_bank_configured[config->emi_bank]);
	stx7105_emi_bank_configured[config->emi_bank] = 1;

	bank_base = emi_bank_base(config->emi_bank);

	stx7105_pata_resources[0].start += bank_base;
	stx7105_pata_resources[0].end += bank_base;
	stx7105_pata_resources[1].start += bank_base;
	stx7105_pata_resources[1].end += bank_base;
	stx7105_pata_resources[2].start = config->irq;
	stx7105_pata_resources[2].end = config->irq;

	emi_config_pata(config->emi_bank, config->pc_mode);

	platform_device_register(&stx7105_pata_device);
}

/* SPI FSM setup ---------------------------------------------------------- */

static struct platform_device stx7106_spifsm_device = {
	.name		= "stm-spi-fsm",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfe702000,
			.end	= 0xfe7024ff,
			.flags	= IORESOURCE_MEM,
		},
	},
};

static struct stm_pad_config stx7106_spifsm_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(15, 0, 1),	/* SPIBoot CLK */
		STM_PAD_PIO_OUT(15, 1, 1),	/* SPIBoot DOUT */
		STM_PAD_PIO_OUT(15, 2, 1),	/* SPIBoot NOTCS */
		STM_PAD_PIO_IN(15, 3, -1),	/* SPIBoot DIN */
	},
};

void __init stx7106_configure_spifsm(struct stm_plat_spifsm_data *data)
{
	/* Not available on stx7105 */
	if (cpu_data->type == CPU_STX7105)
		BUG();

	/* Configure pads for SPIBoot FSM */
	/* Note, output pads must be configured as ALT_OUT rather than ALT_BIDIR
	 * (see bug GNBvd8843).  As a result, FSM dual mode is not supported on
	 * stx7106.
	 */

	if (stm_pad_claim(&stx7106_spifsm_pad_config, "SPIFSM") == NULL)
		printk(KERN_ERR "Failed to claim SPIFSM pads!\n");

	stx7106_spifsm_device.dev.platform_data = data;

	platform_device_register(&stx7106_spifsm_device);
}


/* NAND Resources --------------------------------------------------------- */

static struct platform_device stx7105_nand_emi_device = {
	.name			= "stm-nand-emi",
	.dev.platform_data	= &(struct stm_plat_nand_emi_data) {
	},
};

static struct platform_device stx7105_nand_flex_device = {
	.num_resources		= 2,
	.resource		= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("flex_mem", 0xFE701000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x14a0), -1),
	},
	.dev.platform_data	= &(struct stm_plat_nand_flex_data) {
	},
};

void __init stx7105_configure_nand(struct stm_nand_config *config)
{
	struct stm_plat_nand_flex_data *flex_data;
	struct stm_plat_nand_emi_data *emi_data;

	switch (config->driver) {
	case stm_nand_emi:
		/* Configure device for stm-nand-emi driver */
		emi_data = stx7105_nand_emi_device.dev.platform_data;
		emi_data->nr_banks = config->nr_banks;
		emi_data->banks = config->banks;
		emi_data->emi_rbn_gpio = config->rbn.emi_gpio;
		platform_device_register(&stx7105_nand_emi_device);
		break;
	case stm_nand_flex:
	case stm_nand_afm:
		/* Configure device for stm-nand-flex/afm driver */
		flex_data = stx7105_nand_flex_device.dev.platform_data;
		flex_data->nr_banks = config->nr_banks;
		flex_data->banks = config->banks;
		flex_data->flex_rbn_connected = config->rbn.flex_connected;
		stx7105_nand_flex_device.name =
			(config->driver == stm_nand_flex) ?
			"stm-nand-flex" : "stm-nand-afm";
		platform_device_register(&stx7105_nand_flex_device);
		break;
	}
}


/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7105 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x560 << 2), /* 0x9580 */
	.cntn      = 0x8000 + (0x562 << 2), /* 0x9588 */
	.saddrn    = 0x8000 + (0x563 << 2), /* 0x958c */
	.daddrn    = 0x8000 + (0x564 << 2), /* 0x9590 */
};

static struct stm_plat_fdma_hw stx7105_fdma_hw = {
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

static struct stm_plat_fdma_data stx7105_fdma_platform_data = {
	.hw = &stx7105_fdma_hw,
	.fw = &stm_fdma_firmware_7105,
};

static struct platform_device stx7105_fdma_devices[] = {
	{
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe220000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1380), -1),
		},
		.dev.platform_data = &stx7105_fdma_platform_data,
	}, {
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfe410000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x13a0), -1),
		},
		.dev.platform_data = &stx7105_fdma_platform_data,
	}
};

static struct platform_device stx7105_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe420000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7105_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

static struct platform_device stx7105_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};



/* Internal temperature sensor resources ---------------------------------- */
static void stx7105_temp_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 1 : 0;

	stm_device_sysconf_write(device_state, "TEMP_PWR", value);
}

static struct platform_device stx7105_temp_device = {
	.name			= "stm-temp",
	.id			= -1,
	.dev.platform_data	= &(struct plat_stm_temp_data) {
		.dcorrect = { SYS_CFG, 41, 5, 9 },
		.overflow = { SYS_STA, 12, 8, 8 },
		.data = { SYS_STA, 12, 10, 16 },
		.device_config = &(struct stm_device_config) {
			.sysconfs_num = 1,
			.power = stx7105_temp_power,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG(41, 4, 4, "TEMP_PWR"),
			},
		}
	},
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx7105_pio_devices[] = {
	/* COMMS PIO blocks */
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd020000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0xc00), -1),
		},
	},
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd021000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0xc80), -1),
		},
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd022000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0xd00), -1),
		},
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd023000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1060), -1),
		},
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd024000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1040), -1),
		},
	},
	[5] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd025000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1020), -1),
		},
	},
	[6] = {
		.name = "stm-gpio",
		.id = 6,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd026000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1000), -1),
		},
	},

	/* Standalone PIO blocks */
	/* All the following block use the same interrupt, which is
	 * defined as a separate platform device below */
	[7] = {
		.name = "stm-gpio",
		.id = 7,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe010000, 0x100),
		},
	},
	[8] = {
		.name = "stm-gpio",
		.id = 8,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe011000, 0x100),
		},
	},
	[9] = {
		.name = "stm-gpio",
		.id = 9,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe012000, 0x100),
		},
	},
	[10] = {
		.name = "stm-gpio",
		.id = 10,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe013000, 0x100),
		},
	},
	[11] = {
		.name = "stm-gpio",
		.id = 11,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe014000, 0x100),
		},
	},
	[12] = {
		.name = "stm-gpio",
		.id = 12,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe015000, 0x100),
		},
	},
	[13] = {
		.name = "stm-gpio",
		.id = 13,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe016000, 0x100),
		},
	},
	[14] = {
		.name = "stm-gpio",
		.id = 14,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe017000, 0x100),
		},
	},
	[15] = {
		.name = "stm-gpio",
		.id = 15,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe018000, 0x100),
		},
	},
	[16] = {
		.name = "stm-gpio",
		.id = 16,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe019000, 0x100),
		},
	},
};

static struct platform_device stx7105_pio_irqmux_device = {
	.name = "stm-gpio-irqmux",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe01f080, 0x4),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0xb40), -1),
	},
	.dev.platform_data = &(struct stm_plat_pio_irqmux_data) {
		.port_first = 7,
		.ports_num = 10,
	}
};

static int stx7105_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	static struct {
		u8 sys_cfg_num;
		u8 max;
		struct sysconf_field *sc;
	} functions[] = {
		[0] = { 19, 5, },
		[1] = { 20, 4, },
		[2] = { 21, 4, },
		[3] = { 25, 4, },
		[4] = { 34, 4, },
		[5] = { 35, 4, },
		[6] = { 36, 4, },
		[7] = { 37, 4, },
		[8] = { 46, 3, },
		[9] = { 47, 4, },
		[10] = { 39, 2, },
		[11] = { 53, 4, },
		[12] = { 48, 5, },
		[13] = { 49, 5, },
		[14] = { 0, 1, },
		[15] = { 50, 4, },
		[16] = { 54, 2, },
	};
	int port = stm_gpio_port(gpio);
	int pin = stm_gpio_pin(gpio);

	BUG_ON(port > ARRAY_SIZE(functions));

	if (function == 0) {
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
		int sys_cfg_num = functions[port].sys_cfg_num;
		int function_max = functions[port].max;

		if (port == 14 && function == 2)
			function = 1;

		BUG_ON(function < 1);
		BUG_ON(function > function_max);

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

		if (sys_cfg_num) {
			struct sysconf_field *sc = functions[port].sc;
			unsigned long val;

			if (!sc) {
				sc = sysconf_claim(SYS_CFG, sys_cfg_num,
						0, 31, "PIO Config");
				BUG_ON(!sc);
				functions[port].sc = sc;
			}

			function--;

			val = sysconf_read(sc);

			if (function_max > 1) {
				val &= ~(1 << pin);
				val |= (function & 1) << pin;
			}

			if (function_max > 2) {
				val &= ~(1 << (pin + 8));
				val |= (function & (1 << 1)) << (pin + 8 - 1);
			}

			if (function_max > 4) {
				val &= ~(1 << (pin + 16));
				val |= (function & (1 << 2)) << (pin + 16 - 2);
			}

			sysconf_write(sc, val);
		}
	}

	return 0;
}

/* MMC/SD resources ------------------------------------------------------ */

/*
 * MMC is supposed to be configured as ALT_OUT, not ALT_BIDIR.
 * GNBvd78840 MMC/SPI interface is working not ok when PIO is set in BD
 */
static struct stm_pad_config stx7105_mmc_pad_config = {
	.gpios_num = 14,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(11, 2, -1),	/* Card Detect */
		STM_PAD_PIO_OUT_NAMED(11, 3, 1, "MMCCLK"),/* MMC clock */
		STM_PAD_PIO_OUT(11, 4, 1),	/* MMC command */
		STM_PAD_PIO_OUT(11, 5, 1),	/* MMC Data[0]*/
		STM_PAD_PIO_OUT(11, 6, 1),	/* MMC Data[1]*/
		STM_PAD_PIO_OUT(11, 7, 1),	/* MMC Data[2]*/
		STM_PAD_PIO_OUT(16, 0, 1),	/* MMC Data[3]*/
		STM_PAD_PIO_OUT(16, 1, 1),	/* MMC Data[4]*/
		STM_PAD_PIO_OUT(16, 2, 1),	/* MMC Data[5]*/
		STM_PAD_PIO_OUT(16, 3, 1),	/* MMC Data[6]*/
		STM_PAD_PIO_OUT(16, 4, 1),	/* MMC Data[7]*/
		STM_PAD_PIO_OUT(16, 5, 1),	/* MMC LED On */
		STM_PAD_PIO_OUT(16, 6, 1),	/* MMC Power On */
		STM_PAD_PIO_IN(16, 7, -1),	/* MMC Write Protection */
	},
};

static int mmc_pad_resources(struct sdhci_host *sdhci)
{
	if (!devm_stm_pad_claim(sdhci->mmc->parent, &stx7105_mmc_pad_config,
				dev_name(sdhci->mmc->parent)))
		return -ENODEV;

	return 0;
}

static struct sdhci_pltfm_data stx7105_mmc_platform_data = {
		.init = mmc_pad_resources,
		.quirks = SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
};

static struct platform_device stx7105_mmc_device = {
		.name = "sdhci",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd100000, 0x400),
			STM_PLAT_RESOURCE_IRQ_NAMED("mmcirq",
						    ILC_EXT_IRQ(41), -1),
		},
		.dev = {
			.platform_data = &stx7105_mmc_platform_data,
		}
};

void __init stx7105_configure_mmc(void)
{
	struct sysconf_field *sc;

	/* MMC clock comes from the ClockGen_B bank1, channel 2;
	 * this clock has been set to 52MHz.
	 * For supporting SD High-Speed Mode we need to set it
	 * to 50MHz. */
	struct clk *clk = clk_get(NULL, "CLKB_FS1_CH2");
	clk_set_rate(clk, 50000000);

	/* Out Enable coms from the MMC */
	sc = sysconf_claim(SYS_CFG, 17, 0, 0, "mmc");
	sysconf_write(sc, 1);

	platform_device_register(&stx7105_mmc_device);
}


/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx7105_sysconf_device = {
	.name		= "stm-sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe001000, 0x1e0),
	},
	.dev.platform_data = &(struct stm_plat_sysconf_data) {
		.groups_num = 3,
		.groups = (struct stm_plat_sysconf_group []) {
			PLAT_SYSCONF_GROUP(SYS_DEV, 0x000),
			PLAT_SYSCONF_GROUP(SYS_STA, 0x008),
			PLAT_SYSCONF_GROUP(SYS_CFG, 0x100),
		},
	},
};



/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stx7105_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7105_sysconf_device, 1);
	stm_gpio_early_init(stx7105_pio_devices,
			ARRAY_SIZE(stx7105_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);
	stm_pad_init(ARRAY_SIZE(stx7105_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     -1, 0, stx7105_pio_config);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	/* Because 7106c2 was never manufactured, 7106c3 is identified
	 * as revision "2", and so on... */
	if (cpu_data->type == CPU_STX7106 && chip_revision > 1)
		chip_revision++;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx710%d version %ld.x\n",
			cpu_data->type == CPU_STX7105 ? 5 : 6, chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx7105_postcore_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7105_pio_devices); i++)
		platform_device_register(&stx7105_pio_devices[i]);
	platform_device_register(&stx7105_pio_irqmux_device);

	return platform_device_register(&stx7105_emi);
}
postcore_initcall(stx7105_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx7105_devices[] __initdata = {
	&stx7105_fdma_devices[0],
	&stx7105_fdma_devices[1],
	&stx7105_fdma_xbar_device,
	&stx7105_sysconf_device,
	&stx7105_rng_hwrandom_device,
	&stx7105_rng_devrandom_device,
	&stx7105_temp_device,
};

static int __init stx7105_devices_setup(void)
{
	return platform_add_devices(stx7105_devices,
			ARRAY_SIZE(stx7105_devices));
}
device_initcall(stx7105_devices_setup);
