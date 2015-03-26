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
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/ata_platform.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/device.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */

static int __initdata stx7100_emi_bank_configured[EMI_BANKS];

static void stx7100_emi_power(struct stm_device_state *device_state,
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

static struct platform_device stx7100_emi = {
	.name = "emi",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 64 * 1024 * 1024),
		STM_PLAT_RESOURCE_MEM(0x1a100000, 0x874),
	},
	.dev.platform_data = &(struct stm_device_config){
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYS_CFG(32, 1, 1, "EMI_PWR"),
			STM_DEVICE_SYS_STA(15, 0, 0, "EMI_ACK"),
		},
		.power = stx7100_emi_power,
	}
};



/* PATA resources --------------------------------------------------------- */

/* EMI A21 = CS1 (active low)
 * EMI A20 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */
static struct resource stx7100_pata_resources[] = {
	/* I/O base: CS1=N, CS0=A */
	[0] = STM_PLAT_RESOURCE_MEM(1 << 21, 8 << 17),
	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
	[1] = STM_PLAT_RESOURCE_MEM((1 << 20) + (6 << 17), 4),
	/* IRQ */
	[2] = STM_PLAT_RESOURCE_IRQ(-1, -1),
};

static struct platform_device stx7100_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx7100_pata_resources),
	.resource	= stx7100_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift	= 17,
	},
};

void __init stx7100_configure_pata(struct stx7100_pata_config *config)
{
	unsigned long bank_base;

	if (!config) {
		BUG();
		return;
	}

	BUG_ON(config->emi_bank < 0 || config->emi_bank >= EMI_BANKS);
	BUG_ON(stx7100_emi_bank_configured[config->emi_bank]);
	stx7100_emi_bank_configured[config->emi_bank] = 1;

	bank_base = emi_bank_base(config->emi_bank);

	stx7100_pata_resources[0].start += bank_base;
	stx7100_pata_resources[0].end += bank_base;
	stx7100_pata_resources[1].start += bank_base;
	stx7100_pata_resources[1].end += bank_base;
	stx7100_pata_resources[2].start = config->irq;
	stx7100_pata_resources[2].end = config->irq;

	emi_config_pata(config->emi_bank, config->pc_mode);

	platform_device_register(&stx7100_pata_device);
}



/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7100 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x010 << 2), /* 0x8040 */
	.ptrn      = 0x8000 + (0x460 << 2), /* 0x9180 */
	.cntn      = 0x8000 + (0x462 << 2), /* 0x9188 */
	.saddrn    = 0x8000 + (0x463 << 2), /* 0x918c */
	.daddrn    = 0x8000 + (0x464 << 2), /* 0x9190 */
};

static struct stm_plat_fdma_hw stx7100_fdma_hw = {
	.slim_regs = {
		.id       = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver      = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en       = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.dmem = {
		.offset   = 0x8000,
		.size     = 0x600 << 2, /* 1536 * 4 = 6144 */
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
		.offset   = 0xc000,
		.size     = 0xa00 << 2, /* 2560 * 4 = 10240 */
	},
};

static struct stm_plat_fdma_data stx7100_fdma_platform_data = {
	.hw = &stx7100_fdma_hw,
	.fw = &stm_fdma_firmware_7100,
};

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7109c2 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x500 << 2), /* 0x9400 */
	.cntn      = 0x8000 + (0x502 << 2), /* 0x9408 */
	.saddrn    = 0x8000 + (0x503 << 2), /* 0x940c */
	.daddrn    = 0x8000 + (0x504 << 2), /* 0x9410 */
};

static struct stm_plat_fdma_hw stx7109c2_fdma_hw = {
	.slim_regs = {
		.id       = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver      = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en       = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.dmem = {
		.offset = 0x8000,
		.size   = 0x600 << 2, /* 1536 * 4 = 6144 */
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
		.size   = 0xa00 << 2, /* 2560 * 4 = 10240 */
	},
};

static struct stm_plat_fdma_data stx7109c2_fdma_platform_data = {
	.hw = &stx7109c2_fdma_hw,
	.fw = &stm_fdma_firmware_7109c2,
};

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7109c3 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x500 << 2), /* 0x9400 */
	.cntn      = 0x8000 + (0x502 << 2), /* 0x9408 */
	.saddrn    = 0x8000 + (0x503 << 2), /* 0x940c */
	.daddrn    = 0x8000 + (0x504 << 2), /* 0x9410 */
};

static struct stm_plat_fdma_hw stx7109c3_fdma_hw = {
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

static struct stm_plat_fdma_data stx7109c3_fdma_platform_data = {
	.hw = &stx7109c3_fdma_hw,
	.fw = &stm_fdma_firmware_7109c3,
};

static struct platform_device stx7100_fdma_device = {
	.name = "stm-fdma",
	.id = -1,
	.num_resources	= 2,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0x19220000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(140, -1),
	},
};

static void stx7100_fdma_setup(void)
{
	switch (cpu_data->type) {
	case CPU_STX7100:
		stx7100_fdma_device.dev.platform_data =
				&stx7100_fdma_platform_data;
		break;
	case CPU_STX7109:
		switch (cpu_data->cut_major) {
		case 1:
			BUG();
			break;
		case 2:
			stx7100_fdma_device.dev.platform_data =
					&stx7109c2_fdma_platform_data;
			break;
		default:
			stx7100_fdma_device.dev.platform_data =
					&stx7109c3_fdma_platform_data;
			break;
		}
		break;
	default:
		BUG();
		break;
	}
}



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7100_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x19250000, 0x1000),
	}
};

static struct platform_device stx7100_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x19250000, 0x1000),
	}
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx7100_pio_devices[] = {
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18020000, 0x100),
			STM_PLAT_RESOURCE_IRQ(80, -1),
		},
	},
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18021000, 0x100),
			STM_PLAT_RESOURCE_IRQ(84, -1),
		},
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18022000, 0x100),
			STM_PLAT_RESOURCE_IRQ(88, -1),
		},
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18023000, 0x100),
			STM_PLAT_RESOURCE_IRQ(115, -1),
		},
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18024000, 0x100),
			STM_PLAT_RESOURCE_IRQ(114, -1),
		},
	},
	[5] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x18025000, 0x100),
			STM_PLAT_RESOURCE_IRQ(113, -1),
		},
	},
};

static int stx7100_pio_config(unsigned gpio,
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

static struct platform_device stx7100_sysconf_device = {
	.name		= "stm-sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x19001000, 0x194),
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
void __init stx7100_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_7109, chip_revision;

	/* Create a PMB mapping so that the ioremap calls these drivers
	 * will make can be satisfied without having to call get_vm_area
	 * or cause a fault. Its probably also a good for efficiency as
	 * there will be lots of devices in this range.
	 */
	ioremap_nocache(0x18000000, 0x04000000);

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7100_sysconf_device, 1);
	stm_gpio_early_init(stx7100_pio_devices,
			ARRAY_SIZE(stx7100_pio_devices), 176);
	stm_pad_init(ARRAY_SIZE(stx7100_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     -1, 0, stx7100_pio_config);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_7109 = (((devid >> 12) & 0x3ff) == 0x02c);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "%s version %ld.x\n",
	       chip_7109 ? "STx7109" : "STx7100", chip_revision);

	if (chip_7109) {
		boot_cpu_data.type = CPU_STX7109;
		sc = sysconf_claim(SYS_STA, 9, 0, 7, "devid");
		devid = sysconf_read(sc);
		printk(KERN_INFO "Chip version %ld.%ld\n",
				(devid >> 4)+1, devid & 0xf);
		boot_cpu_data.cut_minor = devid & 0xf;
		if (devid == 0x24) {
			/*
			 * See ADCS 8135002 "STI7109 CUT 4.0 CHANGES
			 * VERSUS CUT 3.X" for details of this change.
			 */
			printk(KERN_INFO "Setting version to 4.0 to match "
			       "commercial branding\n");
			boot_cpu_data.cut_major = 4;
			boot_cpu_data.cut_minor = 0;
		}
	}

	/* Configure the ST40 RTC to source its clock from clockgenB.
	 * In theory this should be board specific, but so far nobody
	 * has ever done this. */
	sc = sysconf_claim(SYS_CFG, 8, 1, 1, "rtc");
	sysconf_write(sc, 1);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx7100_postcore_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7100_pio_devices); i++)
		platform_device_register(&stx7100_pio_devices[i]);

	return platform_device_register(&stx7100_emi);
}
postcore_initcall(stx7100_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx7100_devices[] __initdata = {
	&stx7100_fdma_device,
	&stx7100_sysconf_device,
	&stx7100_rng_hwrandom_device,
	&stx7100_rng_devrandom_device,
};

static int __init stx7100_devices_setup(void)
{
	stx7100_fdma_setup();

	return platform_add_devices(stx7100_devices,
			ARRAY_SIZE(stx7100_devices));
}
device_initcall(stx7100_devices_setup);
