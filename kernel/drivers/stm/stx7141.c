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
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/device.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7141.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */

static int __initdata stx7141_emi_bank_configured[EMI_BANKS];

static void stx7141_emi_power(struct stm_device_state *device_state,
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

static struct platform_device stx7141_emi = {
	.name = "emi",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 128*1024*1024),
		STM_PLAT_RESOURCE_MEM(0xfe700000, 0x874),
	},
	.dev.platform_data = &(struct stm_device_config){
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYS_CFG(32, 1, 1, "EMI_PWR"),
			STM_DEVICE_SYS_STA(15, 1, 1, "EMI_ACK"),
		},
		.power = stx7141_emi_power,
	}
};



/* COMMS block ILC -------------------------------------------------------- */

static struct platform_device stx7141_comms_ilc_device = {
	.name		= "ilc3",
	.id		= 1,
	.num_resources  = 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd000000, 0x900),
	},
	.dev.platform_data = &(struct stm_plat_ilc3_data) {
		.inputs_num = COMMS_ILC_NR_IRQS,
		.outputs_num = 16,
		.first_irq = COMMS_ILC_FIRST_IRQ,
	},
};



/* PATA resources --------------------------------------------------------- */

/* EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */
static struct resource stx7141_pata_resources[] = {
	/* I/O base: CS1=N, CS0=A */
	[0] = STM_PLAT_RESOURCE_MEM(1 << 20, 8 << 17),
	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
	[1] = STM_PLAT_RESOURCE_MEM((1 << 21) + (6 << 17), 4),
	/* IRQ */
	[2] = STM_PLAT_RESOURCE_IRQ(-1, -1),
};

static struct platform_device stx7141_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx7141_pata_resources),
	.resource	= stx7141_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift = 17,
	},
};

void __init stx7141_configure_pata(struct stx7141_pata_config *config)
{
	static int configured;
	unsigned long bank_base;

	BUG_ON(configured);
	configured = 1;

	if (!config) {
		BUG();
		return;
	}

	BUG_ON(config->emi_bank < 0 || config->emi_bank >= EMI_BANKS);
	BUG_ON(stx7141_emi_bank_configured[config->emi_bank]);
	stx7141_emi_bank_configured[config->emi_bank] = 1;

	bank_base = emi_bank_base(config->emi_bank);

	stx7141_pata_resources[0].start += bank_base;
	stx7141_pata_resources[0].end += bank_base;
	stx7141_pata_resources[1].start += bank_base;
	stx7141_pata_resources[1].end += bank_base;
	stx7141_pata_resources[2].start = config->irq;
	stx7141_pata_resources[2].end = config->irq;

	emi_config_pata(config->emi_bank, config->pc_mode);

	platform_device_register(&stx7141_pata_device);
}



/* NAND Resources --------------------------------------------------------- */

static struct platform_device stx7141_nand_emi_device = {
	.name			= "stm-nand-emi",
	.dev.platform_data	= &(struct stm_plat_nand_emi_data) {
	},
};

static struct platform_device stx7141_nand_flex_device = {
	.num_resources		= 2,
	.resource		= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("flex_mem", 0xFE701000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(150), -1),
	},
	.dev.platform_data	= &(struct stm_plat_nand_flex_data) {
	},
};

void __init stx7141_configure_nand(struct stm_nand_config *config)
{
	struct stm_plat_nand_flex_data *flex_data;
	struct stm_plat_nand_emi_data *emi_data;

	switch (config->driver) {
	case stm_nand_emi:
		/* Configure device for stm-nand-emi driver */
		emi_data = stx7141_nand_emi_device.dev.platform_data;
		emi_data->nr_banks = config->nr_banks;
		emi_data->banks = config->banks;
		emi_data->emi_rbn_gpio = config->rbn.emi_gpio;
		platform_device_register(&stx7141_nand_emi_device);
		break;
	case stm_nand_flex:
	case stm_nand_afm:
		/* Configure device for stm-nand-flex/afm driver */
		flex_data = stx7141_nand_flex_device.dev.platform_data;
		flex_data->nr_banks = config->nr_banks;
		flex_data->banks = config->banks;
		flex_data->flex_rbn_connected = config->rbn.flex_connected;
		stx7141_nand_flex_device.name =
			(config->driver == stm_nand_flex) ?
			"stm-nand-flex" : "stm-nand-afm";
		platform_device_register(&stx7141_nand_flex_device);
		break;
	}
}


/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7141 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x560 << 2), /* 0x9580 */
	.cntn      = 0x8000 + (0x562 << 2), /* 0x9588 */
	.saddrn    = 0x8000 + (0x563 << 2), /* 0x958c */
	.daddrn    = 0x8000 + (0x564 << 2), /* 0x9590 */
};

static struct stm_plat_fdma_hw stx7141_fdma_hw = {
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

static struct stm_plat_fdma_data stx7141_fdma_platform_data = {
	.hw = &stx7141_fdma_hw,
	.fw = &stm_fdma_firmware_7141,
};

static struct platform_device stx7141_fdma_devices[] = {
	{
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe220000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(44), -1),
		},
		.dev.platform_data = &stx7141_fdma_platform_data,
	}, {
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfe410000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(45), -1),
		},
		.dev.platform_data = &stx7141_fdma_platform_data,
	}
};

static struct platform_device stx7141_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe420000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7141_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

static struct platform_device stx7141_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};



/* Internal temperature sensor resources ---------------------------------- */
static void stx7141_temp_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 1 : 0;

	stm_device_sysconf_write(device_state, "TEMP_PWR", value);
}

static unsigned long stx7141_temp1_get_data(void *priv)
{
	/* Some "bright sparkle" decided to split the data field
	 * between SYS_STA12 & SYS_STA13 registers, having 11 (!!!)
	 * bits unused in the SYS_STA13... WHY OH WHY?!?!?! */
	static struct sysconf_field *data1_0_3, *data1_4_6;

	if (!data1_0_3)
		data1_0_3 = sysconf_claim(SYS_STA, 12, 28, 31, "stm-temp.1");
	if (!data1_4_6)
		data1_4_6 = sysconf_claim(SYS_STA, 13, 0, 2, "stm-temp.1");
	if (!data1_0_3 || !data1_4_6)
		return 0;

	return (sysconf_read(data1_4_6) << 4) | sysconf_read(data1_0_3);
}

static struct platform_device stx7141_temp_devices[] = {
	{
		.name			= "stm-temp",
		.id			= 0,
		.dev.platform_data	= &(struct plat_stm_temp_data) {
			.dcorrect = { SYS_CFG, 41, 5, 9 },
			.overflow = { SYS_STA, 12, 8, 8 },
			.data = { SYS_STA, 12, 10, 16 },
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 1,
				.power = stx7141_temp_power,
				.sysconfs = (struct stm_device_sysconf []){
					STM_DEVICE_SYS_CFG(41, 4, 4,
							"TEMP_PWR"),
				},
			},
		},
	}, {
		.name			= "stm-temp",
		.id			= 1,
		.dev.platform_data	= &(struct plat_stm_temp_data) {
			.dcorrect = { SYS_CFG, 41, 15, 19 },
			.overflow = { SYS_STA, 12, 26, 26 },
			.custom_get_data = stx7141_temp1_get_data,
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 1,
				.power = stx7141_temp_power,
				.sysconfs = (struct stm_device_sysconf []){
					STM_DEVICE_SYS_CFG(41, 14, 14,
							"TEMP_PWR"),
				},
			},
		},
	}, {
		.name			= "stm-temp",
		.id			= 2,
		.dev.platform_data	= &(struct plat_stm_temp_data) {
			.dcorrect = { SYS_CFG, 41, 25, 29 },
			.overflow = { SYS_STA, 13, 12, 12 },
			.data = { SYS_STA, 13, 14, 20 },
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 1,
				.power = stx7141_temp_power,
				.sysconfs = (struct stm_device_sysconf []){
					STM_DEVICE_SYS_CFG(41, 24, 24,
							"TEMP_PWR"),
				},
			},
		},
	}
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx7141_pio_devices[] = {
	/* Some eee... individual named the first PIO block PIO1,
	 * so there is no stm-gpio.0 here... */

	/* COMMS PIO blocks */
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd020000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(49), -1),
		},
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd021000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(50), -1),
		},
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd022000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(51), -1),
		},
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd023000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(52), -1),
		},
	},
	[5] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd024000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(53), -1),
		},
	},
	[6] = {
		.name = "stm-gpio",
		.id = 6,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd025000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(54), -1),
		},
	},
	[7] = {
		.name = "stm-gpio",
		.id = 7,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd026000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(55), -1),
		},
	},

	/* Standalone PIO blocks */
	[8] = {
		.name = "stm-gpio",
		.id = 8,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe010000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(59), -1),
		},
	},
	[9] = {
		.name = "stm-gpio",
		.id = 9,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe011000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(60), -1),
		},
	},
	[10] = {
		.name = "stm-gpio",
		.id = 10,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe012000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(61), -1),
		},
	},
	[11] = {
		.name = "stm-gpio",
		.id = 11,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe013000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(62), -1),
		},
	},
	[12] = {
		.name = "stm-gpio",
		.id = 12,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe014000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(63), -1),
		},
	},
	[13] = {
		.name = "stm-gpio",
		.id = 13,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe015000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(64), -1),
		},
	},
	[14] = {
		.name = "stm-gpio",
		.id = 14,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe016000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(65), -1),
		},
	},
	[15] = {
		.name = "stm-gpio",
		.id = 15,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe017000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(66), -1),
		},
	},
	[16] = {
		.name = "stm-gpio",
		.id = 16,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe018000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(67), -1),
		},
	},
};

struct stx7141_pio_sysconf {
	unsigned char num, lsb, msb;
	struct sysconf_field *field;
};

static struct stx7141_pio_sysconf stx7141_pio_sysconfs[][8] = {
	{
		/* PIO0 doesn't exist */
	}, {
		{ 19,  0,  1 },	/* PIO1[0] */
		{ 19,  2,  3 },	/* PIO1[1] */
		{ 19,  4,  5 },	/* PIO1[2] */
		{ 19,  5,  7 },	/* PIO1[3] */
		{ 19,  8,  8 },	/* PIO1[4] */
		{ 19,  9,  9 },	/* PIO1[5] */
		{ 19, 10, 10 },	/* PIO1[6] */
		{ 19, 11, 11 },	/* PIO1[7] */
	}, {
		{ 19, 12, 12 },	/* PIO2[0] */
		{ 19, 13, 13 },	/* PIO2[1] */
		{ 19, 14, 14 },	/* PIO2[2] */
		{ 19, 15, 15 },	/* PIO2[3] */
		{ 19, 16, 16 },	/* PIO2[4] */
		{ 19, 17, 17 },	/* PIO2[5] */
		{ 19, 18, 18 },	/* PIO2[6] */
		{ 19, 19, 19 },	/* PIO2[7] */
	}, {
		{ 19, 20, 20 },	/* PIO3[0] */
		{ 19, 21, 21 },	/* PIO3[1] */
		{ 19, 22, 23 },	/* PIO3[2] */
		{ 19, 24, 25 },	/* PIO3[3] */
		{ 19, 26, 27 },	/* PIO3[4] */
		{ 19, 28, 29 },	/* PIO3[5] */
		{ 19, 30, 31 },	/* PIO3[6] */
		{ 20,  0,  0 },	/* PIO3[7] */
	}, {
		{ 20,  1,  2 },	/* PIO4[0] */
		{ 20,  3,  4 },	/* PIO4[1] */
		{ 20,  5,  6 },	/* PIO4[2] */
		{ 20,  7,  8 },	/* PIO4[3] */
		{ 20,  9, 10 },	/* PIO4[4] */
		{ 20, 11, 12 },	/* PIO4[5] */
		{ 20, 13, 13 },	/* PIO4[6] */
		{ 20, 14, 14 },	/* PIO4[7] */
	}, {
		{ 20, 15, 16 },	/* PIO5[0] */
		{ 20, 17, 18 },	/* PIO5[1] */
		{ 20, 19, 19 },	/* PIO5[2] */
		{ 20, 20, 20 },	/* PIO5[3] */
		{ 20, 21, 21 },	/* PIO5[4] */
		{ 20, 22, 23 },	/* PIO5[5] */
		{ 20, 24, 24 },	/* PIO5[6] */
		{ 20, 25, 26 },	/* PIO5[7] */
	}, {
		{ 20, 27, 28 },	/* PIO6[0] */
		{ 20, 29, 30 },	/* PIO6[1] */
		{ 25,  0,  1 },	/* PIO6[2] */
		{ 25,  2,  3 },	/* PIO6[3] */
		{ 25,  4,  5 },	/* PIO6[4] */
		{ 25,  6,  7 },	/* PIO6[5] */
		{ 25,  8,  9 },	/* PIO6[6] */
		{ 25, 10, 11 },	/* PIO6[7] */
	}, {
		{ 25, 12, 13 },	/* PIO7[0] */
		{ 25, 14, 15 },	/* PIO7[1] */
		{ 25, 16, 17 },	/* PIO7[2] */
		{ 25, 18, 19 },	/* PIO7[3] */
		{ 25, 20, 21 },	/* PIO7[4] */
		{ 25, 22, 23 },	/* PIO7[5] */
		{ 25, 24, 25 },	/* PIO7[6] */
		{ 25, 26, 27 },	/* PIO7[7] */
	}, {
		{ 25, 28, 30 },	/* PIO8[0] */
		{ 35,  0,  2 },	/* PIO8[1] */
		{ 35,  3,  5 },	/* PIO8[2] */
		{ 35,  6,  8 },	/* PIO8[3] */
		{ 35,  9, 11 },	/* PIO8[4] */
		{ 35, 12, 14 },	/* PIO8[5] */
		{ 35, 15, 17 },	/* PIO8[6] */
		{ 35, 18, 20 },	/* PIO8[7] */
	}, {
		{ 35, 21, 22 },	/* PIO9[0] */
		{ 35, 23, 24 },	/* PIO9[1] */
		{ 35, 25, 26 },	/* PIO9[2] */
		{ 35, 27, 28 },	/* PIO9[3] */
		{ 35, 29, 30 },	/* PIO9[4] */
		{ 46,  0,  1 },	/* PIO9[5] */
		{ 46,  2,  3 },	/* PIO9[6] */
		{ 46,  4,  5 },	/* PIO9[7] */
	}, {
		{ 46,  6,  7 },	/* PIO10[0] */
		{ 46,  8,  9 },	/* PIO10[1] */
		{ 46, 10, 11 },	/* PIO10[2] */
		{ 46, 12, 13 },	/* PIO10[3] */
		{ 46, 14, 15 },	/* PIO10[4] */
		{ 46, 16, 17 },	/* PIO10[5] */
		{ 46, 18, 19 },	/* PIO10[6] */
		{ 46, 20, 21 },	/* PIO10[7] */
	}, {
		{ 46, 22, 23 },	/* PIO11[0] */
		{ 46, 24, 26 },	/* PIO11[1] */
		{ 46, 27, 29 },	/* PIO11[2] */
		{ 47,  0,  2 },	/* PIO11[3] */
		{ 47,  3,  5 },	/* PIO11[4] */
		{ 47,  6,  8 },	/* PIO11[5] */
		{ 47,  9, 11 },	/* PIO11[6] */
		{ 47, 12, 14 },	/* PIO11[7] */
	}, {
		{ 47, 15, 17 },	/* PIO12[0] */
		{ 47, 18, 20 },	/* PIO12[1] */
		{ 47, 21, 23 },	/* PIO12[2] */
		{ 47, 24, 25 },	/* PIO12[3] */
		{ 47, 26, 27 },	/* PIO12[4] */
		{ 47, 28, 29 },	/* PIO12[5] */
		{ 48,  0,  2 },	/* PIO12[6] */
		{ 48,  3,  5 },	/* PIO12[7] */
	}, {
		{ 48,  6,  8 },	/* PIO13[0] */
		{ 48,  9, 11 },	/* PIO13[1] */
		{ 48, 12, 14 },	/* PIO13[2] */
		{ 48, 15, 17 },	/* PIO13[3] */
		{ 48, 18, 20 },	/* PIO13[4] */
		{ 48, 21, 23 },	/* PIO13[5] */
		{ 48, 24, 25 },	/* PIO13[6] */
		{ 48, 26, 27 },	/* PIO13[7] */
	}, {
		{ 48, 28, 30 },	/* PIO14[0] */
		{ 49,  0,  2 },	/* PIO14[1] */
		{ 49,  3,  5 },	/* PIO14[2] */
		{ 49,  6,  8 },	/* PIO14[3] */
		{ 49,  9, 11 },	/* PIO14[4] */
		{ 49, 12, 14 },	/* PIO14[5] */
		{ 49, 15, 17 },	/* PIO14[6] */
		{ 49, 18, 19 },	/* PIO14[7] */
	}, {
		{ 49, 20, 21 },	/* PIO15[0] */
		{ 49, 22, 23 },	/* PIO15[1] */
		{ 49, 24, 25 },	/* PIO15[2] */
		{ 49, 26, 27 },	/* PIO15[3] */
		{ 49, 28, 28 },	/* PIO15[4] */
		{ 49, 29, 29 },	/* PIO15[5] */
		{ 49, 30, 30 },	/* PIO15[6] */
		{ 50,  0,  1 },	/* PIO15[7] */
	}, {
		{ 50,  2,  3 },	/* PIO16[0] */
		{ 50,  4,  5 },	/* PIO16[1] */
		{ 50,  6,  7 },	/* PIO16[2] */
		{ 50,  8,  8 },	/* PIO16[3] */
		{ 50,  9,  9 },	/* PIO16[4] */
		{ 50, 10, 10 },	/* PIO16[5] */
		{ 50, 11, 11 },	/* PIO16[6] */
		{ 50, 12, 12 },	/* PIO16[7] */
	}
};

static int stx7141_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	int port = stm_gpio_port(gpio);
	int pin = stm_gpio_pin(gpio);
	struct stx7141_pio_sysconf *sysconf;

	BUG_ON(port < 1);
	BUG_ON(port > ARRAY_SIZE(stx7141_pio_sysconfs));

	sysconf = &stx7141_pio_sysconfs[port][pin];

	if (direction == stm_pad_gpio_direction_in) {
		BUG_ON(function != -1);
		stm_gpio_direction(gpio, STM_GPIO_DIRECTION_IN);
	} else {
		BUG_ON(function < 0);
		BUG_ON(function > (1 << (sysconf->msb - sysconf->lsb + 1)) - 1);

		switch (direction) {
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

		if (!sysconf->field) {
			sysconf->field = sysconf_claim(SYS_CFG, sysconf->num,
					sysconf->lsb, sysconf->msb,
					"PIO Config");
			BUG_ON(!sysconf->field);
		}

		sysconf_write(sysconf->field, function);
	}

	return 0;
}



/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx7141_sysconf_device = {
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
void __init stx7141_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7141_sysconf_device, 1);
	stm_gpio_early_init(stx7141_pio_devices,
			ARRAY_SIZE(stx7141_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);
	stm_pad_init(ARRAY_SIZE(stx7141_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     -1, 0, stx7141_pio_config);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7141 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static struct platform_device *stx7141_postcore_devices[] __initdata = {
	&stx7141_emi,
	&stx7141_comms_ilc_device,
};

static int __init stx7141_postcore_setup(void)
{
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7141_pio_devices) && !err; i++)
		if (stx7141_pio_devices[i].name) /* No PIO0 */
			err = platform_device_register(&stx7141_pio_devices[i]);

	return err ? err : platform_add_devices(stx7141_postcore_devices,
			ARRAY_SIZE(stx7141_postcore_devices));
}
postcore_initcall(stx7141_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx7141_devices[] __initdata = {
	&stx7141_fdma_devices[0],
	&stx7141_fdma_devices[1],
	&stx7141_fdma_xbar_device,
	&stx7141_sysconf_device,
	&stx7141_rng_hwrandom_device,
	&stx7141_rng_devrandom_device,
	&stx7141_temp_devices[0],
	&stx7141_temp_devices[1],
	&stx7141_temp_devices[2],
};

static int __init stx7141_devices_setup(void)
{
	return platform_add_devices(stx7141_devices,
			ARRAY_SIZE(stx7141_devices));
}
device_initcall(stx7141_devices_setup);
