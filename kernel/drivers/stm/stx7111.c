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
#include <linux/stm/stx7111.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */

static void stx7111_emi_power(struct stm_device_state *device_state,
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

static struct platform_device stx7111_emi = {
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
		.power = stx7111_emi_power,
	}
};



/* NAND Resources --------------------------------------------------------- */

static struct platform_device stx7111_nand_emi_device = {
	.name			= "stm-nand-emi",
	.dev.platform_data	= &(struct stm_plat_nand_emi_data) {
	},
};

static struct platform_device stx7111_nand_flex_device = {
	.num_resources		= 2,
	.resource		= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("flex_mem", 0xfe701000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x14c0), -1),
	},
	.dev.platform_data	= &(struct stm_plat_nand_flex_data) {
	},
};

void __init stx7111_configure_nand(struct stm_nand_config *config)
{
	struct stm_plat_nand_flex_data *flex_data;
	struct stm_plat_nand_emi_data *emi_data;

	switch (config->driver) {
	case stm_nand_emi:
		/* Configure device for stm-nand-emi driver */
		emi_data = stx7111_nand_emi_device.dev.platform_data;
		emi_data->nr_banks = config->nr_banks;
		emi_data->banks = config->banks;
		emi_data->emi_rbn_gpio = config->rbn.emi_gpio;
		platform_device_register(&stx7111_nand_emi_device);
		break;
	case stm_nand_flex:
	case stm_nand_afm:
		/* Configure device for stm-nand-flex/afm driver */
		flex_data = stx7111_nand_flex_device.dev.platform_data;
		flex_data->nr_banks = config->nr_banks;
		flex_data->banks = config->banks;
		flex_data->flex_rbn_connected = config->rbn.flex_connected;
		stx7111_nand_flex_device.name =
			(config->driver == stm_nand_flex) ?
			"stm-nand-flex" : "stm-nand-afm";
		platform_device_register(&stx7111_nand_flex_device);
		break;
	}
}


/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7111 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x560 << 2), /* 0x9580 */
	.cntn      = 0x8000 + (0x562 << 2), /* 0x9588 */
	.saddrn    = 0x8000 + (0x563 << 2), /* 0x958c */
	.daddrn    = 0x8000 + (0x564 << 2), /* 0x9590 */
};

static struct stm_plat_fdma_hw stx7111_fdma_hw = {
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

static struct stm_plat_fdma_data stx7111_fdma_platform_data = {
	.hw = &stx7111_fdma_hw,
	.fw = &stm_fdma_firmware_7111,
};

static struct platform_device stx7111_fdma_devices[] = {
	{
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe220000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1380), -1),
		},
		.dev.platform_data = &stx7111_fdma_platform_data,
	}, {
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfe410000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x13a0), -1),
		},
		.dev.platform_data = &stx7111_fdma_platform_data,
	}
};

static struct platform_device stx7111_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe420000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7111_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

static struct platform_device stx7111_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};



/* Internal temperature sensor resources ---------------------------------- */
static void stx7111_temp_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 1 : 0;

	stm_device_sysconf_write(device_state, "TEMP_PWR", value);
}

static struct platform_device stx7111_temp_device = {
	.name			= "stm-temp",
	.id			= -1,
	.dev.platform_data	= &(struct plat_stm_temp_data) {
		.dcorrect = { SYS_CFG, 41, 5, 9 },
		.overflow = { SYS_STA, 12, 8, 8 },
		.data = { SYS_STA, 12, 10, 16 },
		.device_config = &(struct stm_device_config) {
			.sysconfs_num = 1,
			.power = stx7111_temp_power,
			.sysconfs = (struct stm_device_sysconf []){
				STM_DEVICE_SYS_CFG(41, 4, 4, "TEMP_PWR"),
			},
		}
	},
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx7111_pio_devices[] = {
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
};

static struct platform_device stx7111_pio_irqmux_device = {
	.name = "stm-gpio-irqmux",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe01f080, 0x4),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0xbc0), -1),
	},
	.dev.platform_data = &(struct stm_plat_pio_irqmux_data) {
		.port_first = 7,
		.ports_num = 5,
	}
};

static int stx7111_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	switch (direction) {
	case stm_pad_gpio_direction_in:
		BUG_ON(function != -1);
		stm_gpio_direction(gpio, STM_GPIO_DIRECTION_IN);
		break;
	case stm_pad_gpio_direction_out:
		BUG_ON(function < 0);
		BUG_ON(function > 1);
		stm_gpio_direction(gpio, function ?
				STM_GPIO_DIRECTION_ALT_OUT :
				STM_GPIO_DIRECTION_OUT);
		break;
	case stm_pad_gpio_direction_bidir:
		BUG_ON(function < 0);
		BUG_ON(function > 1);
		stm_gpio_direction(gpio, function ?
				STM_GPIO_DIRECTION_ALT_BIDIR :
				STM_GPIO_DIRECTION_BIDIR);
		break;
	default:
		BUG();
		break;
	}

	return 0;
}



/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx7111_sysconf_device = {
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
void __init stx7111_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7111_sysconf_device, 1);
	stm_gpio_early_init(stx7111_pio_devices,
			ARRAY_SIZE(stx7111_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);
	stm_pad_init(ARRAY_SIZE(stx7111_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     -1, 0, stx7111_pio_config);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7111 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx7111_postcore_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7111_pio_devices); i++)
		platform_device_register(&stx7111_pio_devices[i]);
	platform_device_register(&stx7111_pio_irqmux_device);

	return platform_device_register(&stx7111_emi);
}
postcore_initcall(stx7111_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx7111_devices[] __initdata = {
	&stx7111_fdma_devices[0],
	&stx7111_fdma_devices[1],
	&stx7111_fdma_xbar_device,
	&stx7111_sysconf_device,
	&stx7111_rng_hwrandom_device,
	&stx7111_rng_devrandom_device,
	&stx7111_temp_device,
};

static int __init stx7111_devices_setup(void)
{
	return platform_add_devices(stx7111_devices,
			ARRAY_SIZE(stx7111_devices));
}
device_initcall(stx7111_devices_setup);
