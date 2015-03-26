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
#include <linux/stm/stx7108.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */

static int __initdata stx7108_emi_bank_configured[EMI_BANKS];

#if defined(CONFIG_SUSPEND) && defined(CONFIG_HIBERNATION_ON_MEMORY)
/*
 * Don't provide power management of the EMI if both suspend and HoM
 * are configured.  If we do, it appears to be impossible to perform a
 * suspend following a HoM hibernate/resume cycle.
 */

static int stx7108_emi_init(struct stm_device_state *device_state)
{
	stm_device_sysconf_write(device_state, "EMI_PWR", 0);
	return 0;
}
#define stx7108_emi_power NULL
#else
#define stx7108_emi_init NULL

static void stx7108_emi_power(struct stm_device_state *device_state,
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
#endif

static struct platform_device stx7108_emi = {
	.name = "emi",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 128 * 1024 * 1024),
		STM_PLAT_RESOURCE_MEM(0xfe900000, 0x874),
	},
	.dev.platform_data = &(struct stm_device_config){
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYS_CFG_BANK(2, 30, 0, 0, "EMI_PWR"),
			STM_DEVICE_SYS_STA_BANK(2, 1, 0, 0, "EMI_ACK"),
		},
		.init = stx7108_emi_init,
		.power = stx7108_emi_power,
	}
};
/* Mali resources --------------------------------------------------------- */

static struct platform_device stx7108_mali_device = {
	.name = "mali",
	.id = 0,
	.num_resources = 9,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400GP", 0xfe710000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400PP-0", 0xfe718000, 0x10F0),
		STM_PLAT_RESOURCE_MEM_NAMED("MMU-1", 0xfe713000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MMU-2", 0xfe714000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400L2",  0xfe711000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("MALI400GP", ILC_IRQ(115), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("MALI400PP-0", ILC_IRQ(113), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("MMU-1", ILC_IRQ(116), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("MMU-2", ILC_IRQ(114), -1),
	},
};

void stx7108_configure_mali(struct stm_mali_config *priv_data)
{
	stx7108_mali_device.dev.platform_data = priv_data;
	platform_device_register(&stx7108_mali_device);
}


/* PATA resources --------------------------------------------------------- */

/* EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */
static struct resource stx7108_pata_resources[] = {
	/* I/O base: CS1=N, CS0=A */
	[0] = STM_PLAT_RESOURCE_MEM(1 << 20, 8 << 17),
	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
	[1] = STM_PLAT_RESOURCE_MEM((1 << 21) + (6 << 17), 4),
	/* IRQ */
	[2] = STM_PLAT_RESOURCE_IRQ(-1, -1),
};

static struct platform_device stx7108_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx7108_pata_resources),
	.resource	= stx7108_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift = 17,
	},
};

void __init stx7108_configure_pata(struct stx7108_pata_config *config)
{
	unsigned long bank_base;

	if (!config) {
		BUG();
		return;
	}

	BUG_ON(config->emi_bank < 0 || config->emi_bank >= EMI_BANKS);
	BUG_ON(stx7108_emi_bank_configured[config->emi_bank]);
	stx7108_emi_bank_configured[config->emi_bank] = 1;

	bank_base = emi_bank_base(config->emi_bank);

	stx7108_pata_resources[0].start += bank_base;
	stx7108_pata_resources[0].end += bank_base;
	stx7108_pata_resources[1].start += bank_base;
	stx7108_pata_resources[1].end += bank_base;
	stx7108_pata_resources[2].start = config->irq;
	stx7108_pata_resources[2].end = config->irq;

	emi_config_pata(config->emi_bank, config->pc_mode);

	platform_device_register(&stx7108_pata_device);
}

/* SPI FSM setup ---------------------------------------------------------- */

static struct platform_device stx7108_spifsm_device = {
	.name		= "stm-spi-fsm",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfe902000,
			.end	= 0xfe9024ff,
			.flags	= IORESOURCE_MEM,
		},
	},
};

void __init stx7108_configure_spifsm(struct stm_plat_spifsm_data *data)
{
	stx7108_spifsm_device.dev.platform_data = data;

	platform_device_register(&stx7108_spifsm_device);
}


/* NAND Resources --------------------------------------------------------- */

static struct platform_device stx7108_nand_emi_device = {
	.name			= "stm-nand-emi",
	.dev.platform_data	= &(struct stm_plat_nand_emi_data) {
	},
};

static struct platform_device stx7108_nand_flex_device = {
	.num_resources		= 2,
	.resource		= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("flex_mem", 0xFE901000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(121), -1),
	},
	.dev.platform_data	= &(struct stm_plat_nand_flex_data) {
	},
};

void __init stx7108_configure_nand(struct stm_nand_config *config)
{
	struct stm_plat_nand_flex_data *flex_data;
	struct stm_plat_nand_emi_data *emi_data;

	switch (config->driver) {
	case stm_nand_emi:
		/* Configure device for stm-nand-emi driver */
		emi_data = stx7108_nand_emi_device.dev.platform_data;
		emi_data->nr_banks = config->nr_banks;
		emi_data->banks = config->banks;
		emi_data->emi_rbn_gpio = config->rbn.emi_gpio;
		platform_device_register(&stx7108_nand_emi_device);
		break;
	case stm_nand_flex:
	case stm_nand_afm:
		/* Configure device for stm-nand-flex/afm driver */
		flex_data = stx7108_nand_flex_device.dev.platform_data;
		flex_data->nr_banks = config->nr_banks;
		flex_data->banks = config->banks;
		flex_data->flex_rbn_connected = config->rbn.flex_connected;
		stx7108_nand_flex_device.name =
			(config->driver == stm_nand_flex) ?
			"stm-nand-flex" : "stm-nand-afm";
		platform_device_register(&stx7108_nand_flex_device);
		break;
	default:
		return;
	}
}


/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7108 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x560 << 2), /* 0x9580 */
	.cntn      = 0x8000 + (0x562 << 2), /* 0x9588 */
	.saddrn    = 0x8000 + (0x563 << 2), /* 0x958c */
	.daddrn    = 0x8000 + (0x564 << 2), /* 0x9590 */
};

static struct stm_plat_fdma_hw stx7108_fdma_hw = {
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

static struct stm_plat_fdma_data stx7108_fdma_platform_data = {
	.hw = &stx7108_fdma_hw,
	.fw = &stm_fdma_firmware_7108,
};

static struct platform_device stx7108_fdma_devices[] = {
	{
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda00000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(27), -1),
		},
		.dev.platform_data = &stx7108_fdma_platform_data,
	}, {
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfda10000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(29), -1),
		},
		.dev.platform_data = &stx7108_fdma_platform_data,
	}, {
		.name = "stm-fdma",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfda20000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(31), -1),
		},
		.dev.platform_data = &stx7108_fdma_platform_data,
	}
};

static struct platform_device stx7108_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfdabb000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7108_devhwrandom_device = {
	.name		= "stm_hwrandom",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfdabd000, 0x1000),
	}
};

static struct platform_device stx7108_devrandom_device = {
	.name		= "stm_rng",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfdabd000, 0x1000),
	}
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx7108_pio_devices[] = {
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd720000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(129), -1),
		},
	},
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd721000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(130), -1),
		},
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd722000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(131), -1),
		},
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd723000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(132), -1),
		},
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd724000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(133), -1),
		},
	},
	[5] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd725000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(134), -1),
		},
	},
	[6] = {
		.name = "stm-gpio",
		.id = 6,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd726000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(135), -1),
		},
	},
	[7] = {
		.name = "stm-gpio",
		.id = 7,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd727000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(136), -1),
		},
	},
	[8] = {
		.name = "stm-gpio",
		.id = 8,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd728000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(137), -1),
		},
	},
	[9] = {
		.name = "stm-gpio",
		.id = 9,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd729000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(138), -1),
		},
	},

	[10] = {
		.name = "stm-gpio",
		.id = 10,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda60000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(151), -1),
		},
	},
	[11] = {
		.name = "stm-gpio",
		.id = 11,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda61000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(152), -1),
		},
	},
	[12] = {
		.name = "stm-gpio",
		.id = 12,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda62000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(153), -1),
		},
	},
	[13] = {
		.name = "stm-gpio",
		.id = 13,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda63000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(154), -1),
		},
	},
	[14] = {
		.name = "stm-gpio",
		.id = 14,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda64000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(155), -1),
		},
	},

	[15] = {
		.name = "stm-gpio",
		.id = 15,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe740000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(139), -1),
		},
	},
	[16] = {
		.name = "stm-gpio",
		.id = 16,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe741000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(140), -1),
		},
	},
	[17] = {
		.name = "stm-gpio",
		.id = 17,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe742000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(141), -1),
		},
	},
	[18] = {
		.name = "stm-gpio",
		.id = 18,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe743000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(142), -1),
		},
	},
	[19] = {
		.name = "stm-gpio",
		.id = 19,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe744000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(143), -1),
		},
	},
	[20] = {
		.name = "stm-gpio",
		.id = 20,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe745000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(144), -1),
		},
	},
	[21] = {
		.name = "stm-gpio",
		.id = 21,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe746000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(145), -1),
		},
	},
	[22] = {
		.name = "stm-gpio",
		.id = 22,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe747000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(146), -1),
		},
	},
	[23] = {
		.name = "stm-gpio",
		.id = 23,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe748000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(147), -1),
		},
	},
	[24] = {
		.name = "stm-gpio",
		.id = 24,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe749000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(148), -1),
		},
	},

	[25] = {
		.name = "stm-gpio",
		.id = 25,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe720000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(149), -1),
		},
	},

	[26] = {
		.name = "stm-gpio",
		.id = 26,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe721000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(150), -1),
		},
	},
};

/* Pad control registers */
static struct sysconf_field *stx7108_pio_0_14_output_enable[4];
static struct sysconf_field *stx7108_pio_0_14_pull_up[4];
static struct sysconf_field *stx7108_pio_0_14_open_drain[4];
static struct sysconf_field *stx7108_pio_15_26_output_enable[3];
static struct sysconf_field *stx7108_pio_15_26_pull_up[3];
static struct sysconf_field *stx7108_pio_15_26_open_drain[3];

static void stx7108_pio_config_direction(int port, int pin,
		enum stm_pad_gpio_direction direction,
		struct stx7108_pio_mode_config *custom_mode)
{
	struct sysconf_field *output_enable;
	struct sysconf_field *pull_up;
	struct sysconf_field *open_drain;
	unsigned long oe_value, pu_value, od_value;
	unsigned long mask;

	pr_debug("%s(port=%d, pin=%d, direction=%d)\n",
			__func__, port, pin, direction);

	switch (port) {
	case 0 ... 14:
		output_enable = stx7108_pio_0_14_output_enable[port / 4];
		pull_up = stx7108_pio_0_14_pull_up[port / 4];
		open_drain = stx7108_pio_0_14_open_drain[port / 4];
		break;
	case 15 ... 26:
		port -= 15;
		output_enable = stx7108_pio_15_26_output_enable[port / 4];
		pull_up = stx7108_pio_15_26_pull_up[port / 4];
		open_drain = stx7108_pio_15_26_open_drain[port / 4];
		break;
	default:
		BUG();
		return;
	}

	mask = 1 << (((port * 8) + pin) % 32);

	oe_value = sysconf_read(output_enable);
	pu_value = sysconf_read(pull_up);
	od_value = sysconf_read(open_drain);

	switch (direction) {
	case stm_pad_gpio_direction_in:
		/* oe = 0, pu = 0, od = 0 */
		oe_value &= ~mask;
		pu_value &= ~mask;
		od_value &= ~mask;
		break;
	case stm_pad_gpio_direction_out:
		/* oe = 1, pu = 0, od = 0 */
		oe_value |= mask;
		pu_value &= ~mask;
		od_value &= ~mask;
		break;
	case stm_pad_gpio_direction_bidir:
		/* oe = 1, pu = 0, od = 1 */
		oe_value |= mask;
		pu_value &= ~mask;
		od_value |= mask;
		break;
	case stm_pad_gpio_direction_custom:
		BUG_ON(!custom_mode);
		if (custom_mode->oe)
			oe_value |= mask;
		else
			oe_value &= ~mask;
		if (custom_mode->pu)
			pu_value |= mask;
		else
			pu_value &= ~mask;
		if (custom_mode->od)
			od_value |= mask;
		else
			od_value &= ~mask;
		break;
	default:
		BUG();
		break;
	}

	sysconf_write(output_enable, oe_value);
	sysconf_write(pull_up, pu_value);
	sysconf_write(open_drain, od_value);
}

/* Alternative function selector registers */
static struct sysconf_field *stx7108_pio_0_14_function[15];
static struct sysconf_field *stx7108_pio_15_26_function[12];

static void stx7108_pio_config_function(int port, int pin, int function)
{
	struct sysconf_field *selector;
	int offset;
	unsigned long val;

	pr_debug("%s(port=%d, pin=%d, function=%d)\n",
			__func__, port, pin, function);

	switch (port) {
	case 0 ... 14:
		selector = stx7108_pio_0_14_function[port];
		break;
	case 15 ... 26:
		port -= 15;
		selector = stx7108_pio_15_26_function[port];
		break;
	default:
		BUG();
		return;
	}

	offset = pin * 4;

	val = sysconf_read(selector);
	val &= ~(0xf << offset);
	val |= function << offset;
	sysconf_write(selector, val);
}

/* Retiming registers */
static struct sysconf_field *stx7108_pio_1_retime[2];
static struct sysconf_field *stx7108_pio_6_14_retime[9][2];
static struct sysconf_field *stx7108_pio_15_23_retime[9][2];

void stx7108_pio_config_retime(int port, int pin,
		struct stx7108_pio_retime_config *config)
{
	struct sysconf_field **regs;
	unsigned long values[2];
	unsigned long mask;

	switch (port) {
	case 1:
		regs = stx7108_pio_1_retime;
		break;
	case 6 ... 14:
		regs = stx7108_pio_6_14_retime[port - 6];
		break;
	case 15 ... 23:
		regs = stx7108_pio_15_23_retime[port - 15];
		break;
	default:
		BUG();
		return;
	}

	values[0] = sysconf_read(regs[0]);
	values[1] = sysconf_read(regs[1]);

	mask = 1 << pin;
	if (config->clk1notclk0 == 0)
		values[0] &= ~mask;
	else if (config->clk1notclk0 == 1)
		values[0] |= mask;;

	mask = 1 << (pin + 8);
	if (config->clknotdata == 0)
		values[0] &= ~mask;
	else if (config->clknotdata == 1)
		values[0] |= mask;

	mask = 1 << (pin + 16);
	if (config->delay_input == 0)
		values[0] &= ~mask;
	else if (config->delay_input == 1)
		values[0] |= mask;

	mask = 1 << (pin + 24);
	if (config->double_edge == 0)
		values[0] &= ~mask;
	else if (config->double_edge == 1)
		values[0] |= mask;

	mask = 1 << pin;
	if (config->invertclk == 0)
		values[1] &= ~mask;
	else if (config->invertclk == 1)
		values[1] |= mask;

	mask = 1 << (pin + 8);
	if (config->retime == 0)
		values[1] &= ~mask;
	else if (config->retime == 1)
		values[1] |= mask;

	sysconf_write(regs[0], values[0]);
	sysconf_write(regs[1], values[1]);
}

static int stx7108_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	int port = stm_gpio_port(gpio);
	int pin = stm_gpio_pin(gpio);
	struct stx7108_pio_config *config = priv;

	BUG_ON(port > ARRAY_SIZE(stx7108_pio_devices));
	BUG_ON(function < 0 || function > 5);

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
	} else {
		stx7108_pio_config_direction(port, pin, direction,
				config ? config->mode : NULL);
	}

	stx7108_pio_config_function(port, pin, function);
	if (config && config->retime)
		stx7108_pio_config_retime(port, pin, config->retime);

	return 0;
}

static void __init stx7108_pio_init(void)
{
	int i, j;

	/* Pad control registers */
	for (i = 0; i < ARRAY_SIZE(stx7108_pio_0_14_output_enable); i++) {
		stx7108_pio_0_14_output_enable[i] = sysconf_claim(SYS_CFG_BANK2,
				15 + i, 0, 31, "PIO Output Enable Control");
		BUG_ON(!stx7108_pio_0_14_output_enable[i]);
		stx7108_pio_0_14_pull_up[i] = sysconf_claim(SYS_CFG_BANK2,
				19 + i, 0, 31, "PIO Pull Up Control");
		BUG_ON(!stx7108_pio_0_14_pull_up[i]);
		stx7108_pio_0_14_open_drain[i] = sysconf_claim(SYS_CFG_BANK2,
				23 + i, 0, 31, "PIO Open Drain Control");
		BUG_ON(!stx7108_pio_0_14_open_drain[i]);
	}
	for (i = 0; i < ARRAY_SIZE(stx7108_pio_15_26_output_enable); i++) {
		stx7108_pio_15_26_output_enable[i] =
				sysconf_claim(SYS_CFG_BANK4,
				12 + i, 0, 31, "PIO Output Enable Control");
		BUG_ON(!stx7108_pio_15_26_output_enable[i]);
		stx7108_pio_15_26_pull_up[i] = sysconf_claim(SYS_CFG_BANK4,
				16 + i, 0, 31, "PIO Pull Up Control");
		BUG_ON(!stx7108_pio_15_26_pull_up[i]);
		stx7108_pio_15_26_open_drain[i] = sysconf_claim(SYS_CFG_BANK4,
				20 + i, 0, 31, "PIO Open Drain Control");
		BUG_ON(!stx7108_pio_15_26_open_drain[i]);
	}

	/* Alternative function selector registers */
	for (i = 0; i < ARRAY_SIZE(stx7108_pio_0_14_function); i++) {
		stx7108_pio_0_14_function[i] = sysconf_claim(SYS_CFG_BANK2,
				i, 0, 31, "PIO Alternative Function Selector");
		BUG_ON(!stx7108_pio_0_14_function[i]);
	}
	for (i = 0; i < ARRAY_SIZE(stx7108_pio_15_26_function); i++) {
		stx7108_pio_15_26_function[i] = sysconf_claim(SYS_CFG_BANK4,
				i, 0, 31, "PIO Alternative Function Selector");
		BUG_ON(!stx7108_pio_15_26_function[i]);
	}

	/* Retiming registers */
	for (j = 0; j < 2; j++) {
		stx7108_pio_1_retime[j] = sysconf_claim(SYS_CFG_BANK2,
				32 + j, 0, 31, "PIO Retiming Configuration");
		BUG_ON(!stx7108_pio_1_retime[j]);
	}
	for (i = 0; i < ARRAY_SIZE(stx7108_pio_6_14_retime); i++) {
		for (j = 0; j < 2; j++) {
			stx7108_pio_6_14_retime[i][j] =
				sysconf_claim(SYS_CFG_BANK2, 34 + (i * 2) + j,
				0, 31, "PIO Retiming Configuration");
			BUG_ON(!stx7108_pio_6_14_retime[i][j]);
		}
	}
	for (i = 0; i < ARRAY_SIZE(stx7108_pio_15_23_retime); i++) {
		for (j = 0; j < 2; j++) {
			stx7108_pio_15_23_retime[i][j] =
				sysconf_claim(SYS_CFG_BANK4, 48 + (i * 2) + j,
				0, 31, "PIO Retiming Configuration");
			BUG_ON(!stx7108_pio_15_23_retime[i][j]);
		}
	}
}

/* MMC/SD resources ------------------------------------------------------ */

/* Custom PAD configuration for the MMC Host controller */
#define STX7108_PIO_MMC_CLK_OUT(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_custom, \
		.function = 1, \
		.name = "MMCCLK", \
		.priv = &(struct stx7108_pio_config) {	\
			.mode = &(struct stx7108_pio_mode_config) { \
				.oe = 1, \
				.pu = 1, \
				.od = 1, \
			}, \
		}, \
	}

#define STX7108_PIO_MMC_OUT(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_custom, \
		.function = 1, \
		.priv = &(struct stx7108_pio_config) {	\
			.mode = &(struct stx7108_pio_mode_config) { \
				.oe = 1, \
				.pu = 1, \
				.od = 1, \
			}, \
		}, \
	}
#define STX7108_PIO_MMC_BIDIR(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_custom, \
		.function = 1, \
		.priv = &(struct stx7108_pio_config) {	\
			.mode = &(struct stx7108_pio_mode_config) { \
				.oe = 1, \
				.pu = 0, \
				.od = 0, \
			}, \
		}, \
	}
#define STX7108_PIO_MMC_IN(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = 1, \
	}


static struct stm_pad_config stx7108_mmc_pad_config = {
	.gpios_num = 14,
	.gpios = (struct stm_pad_gpio []) {
		STX7108_PIO_MMC_CLK_OUT(1, 0),
		STX7108_PIO_MMC_OUT(1, 1),	/* MMC command */
		STX7108_PIO_MMC_IN(1, 2),	/* MMC Write Protection */
		STX7108_PIO_MMC_IN(1, 3),	/* MMC Card Detect */
		STX7108_PIO_MMC_OUT(1, 4),	/* MMC LED on */
		STX7108_PIO_MMC_OUT(1, 5),	/* MMC Card PWR */
		STX7108_PIO_MMC_BIDIR(0, 0),	/* MMC Data[0]*/
		STX7108_PIO_MMC_BIDIR(0, 1),	/* MMC Data[1]*/
		STX7108_PIO_MMC_BIDIR(0, 2),	/* MMC Data[2]*/
		STX7108_PIO_MMC_BIDIR(0, 3),	/* MMC Data[3]*/
		STX7108_PIO_MMC_BIDIR(0, 4),	/* MMC Data[4]*/
		STX7108_PIO_MMC_BIDIR(0, 5),	/* MMC Data[5]*/
		STX7108_PIO_MMC_BIDIR(0, 6),	/* MMC Data[6]*/
		STX7108_PIO_MMC_BIDIR(0, 7),	/* MMC Data[7]*/
	},
};

static int mmc_pad_resources(struct sdhci_host *sdhci)
{
	if (!devm_stm_pad_claim(sdhci->mmc->parent, &stx7108_mmc_pad_config,
				dev_name(sdhci->mmc->parent)))
		return -ENODEV;

	return 0;
}

static struct sdhci_pltfm_data stx7108_mmc_platform_data = {
		.init = mmc_pad_resources,
		.quirks = SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
};

static struct platform_device stx7108_mmc_device = {
		.name = "sdhci",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdaba000, 0x1000),
			STM_PLAT_RESOURCE_IRQ_NAMED("mmcirq",
						    ILC_IRQ(120), -1),
		},
		.dev = {
			.platform_data = &stx7108_mmc_platform_data,
		}
};

#define PIO1_CFG_CLKNODATA	0x100

void __init stx7108_configure_mmc(int emmc)
{
	struct sysconf_field **regs;
	unsigned long value;
	struct sdhci_pltfm_data *plat_data;

	regs = stx7108_pio_1_retime;
	value = sysconf_read(regs[0]);
	value |= PIO1_CFG_CLKNODATA; /* Output clock */
	sysconf_write(regs[0], value);

	plat_data = &stx7108_mmc_platform_data;

	if (unlikely(emmc))
		plat_data->quirks |= SDHCI_QUIRK_NONREMOVABLE_CARD;

	platform_device_register(&stx7108_mmc_device);
}

/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx7108_sysconf_devices[] = {
	{
		.name		= "sysconf",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfde30000, 0x34),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = SYS_STA_BANK0,
					.offset = 0,
					.name = "BANK0 SYS_STA",
				}, {
					.group = SYS_CFG_BANK0,
					.offset = 4,
					.name = "BANK0 SYS_CFG",
				}
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfde20000, 0x94),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = SYS_STA_BANK1,
					.offset = 0,
					.name = "BANK1 SYS_STA",
				}, {
					.group = SYS_CFG_BANK1,
					.offset = 0x3c,
					.name = "BANK1 SYS_CFG",
				}
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 2,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda50000, 0xfc),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = SYS_CFG_BANK2,
					.offset = 0,
					.name = "BANK2 SYS_CFG",
				}, {
					.group = SYS_STA_BANK2,
					.offset = 0xe4,
					.name = "BANK2 SYS_STA",
				}
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 3,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd500000, 0x40),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = SYS_STA_BANK3,
					.offset = 0,
					.name = "BANK3 SYS_STA",
				}, {
					.group = SYS_CFG_BANK3,
					.offset = 0x18,
					.name = "BANK3 SYS_CFG",
				}
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 4,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe700000, 0x12c),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = SYS_CFG_BANK4,
					.offset = 0,
					.name = "BANK4 SYS_CFG",
				}, {
					.group = SYS_STA_BANK4,
					.offset = 0x11c,
					.name = "BANK4 SYS_STA",
				}
			},
		}
	},
};



/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stx7108_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(stx7108_sysconf_devices,
			ARRAY_SIZE(stx7108_sysconf_devices));
	stx7108_pio_init();
	stm_gpio_early_init(stx7108_pio_devices,
			ARRAY_SIZE(stx7108_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);
	stm_pad_init(ARRAY_SIZE(stx7108_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     0, 0, stx7108_pio_config);

	sc = sysconf_claim(SYS_STA_BANK1, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7108 version %ld.x, %s core\n", chip_revision,
			STX7108_HOST_CORE ? "HOST" : "RT");

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx7108_postcore_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7108_pio_devices); i++)
		platform_device_register(&stx7108_pio_devices[i]);

	return platform_device_register(&stx7108_emi);
}
postcore_initcall(stx7108_postcore_setup);

/* Internal temperature sensor resources ---------------------------------- */
static void stx7108_temp_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 1 : 0;

	stm_device_sysconf_write(device_state, "TEMP_PWR", value);
}

static struct platform_device stx7108_temp_device = {
	.name		   = "stm-temp",
	.id		     = -1,
	.dev.platform_data      = &(struct plat_stm_temp_data) {
		.dcorrect = { SYS_CFG_BANK1, 8, 4, 8 },
		.overflow = { SYS_STA_BANK1, 7, 8, 8 },
		.data = { SYS_STA_BANK1, 7, 10, 16 },
		.device_config = &(struct stm_device_config) {
			.sysconfs_num = 1,
			.power = stx7108_temp_power,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG_BANK(1, 8, 9, 9, "TEMP_PWR"),
			},
		}
	},
};


/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx7108_devices[] __initdata = {
	&stx7108_fdma_devices[0],
	&stx7108_fdma_devices[1],
	&stx7108_fdma_devices[2],
	&stx7108_fdma_xbar_device,
	&stx7108_sysconf_devices[0],
	&stx7108_sysconf_devices[1],
	&stx7108_sysconf_devices[2],
	&stx7108_sysconf_devices[3],
	&stx7108_sysconf_devices[4],
	&stx7108_devhwrandom_device,
	&stx7108_devrandom_device,
	&stx7108_temp_device,
};

static int __init stx7108_devices_setup(void)
{
	return platform_add_devices(stx7108_devices,
			ARRAY_SIZE(stx7108_devices));
}
device_initcall(stx7108_devices_setup);
