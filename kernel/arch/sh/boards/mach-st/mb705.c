/*
 * arch/sh/boards/st/common/mb705.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STB peripherals board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/bug.h>
#include <linux/irq.h>
#include <asm/processor.h>
#include <asm/irq-ilc.h>
#include <mach/common.h>
#include "mb705-epld.h"


/*
 * NOR/NAND CS routing should be configured according to main board's
 * boot-device:
 *		NOR		NAND		[SPI]
 *	       ------          ------          -------
 * SW8-1	ON		OFF		 OFF
 *   EMIA  ->	NOR		NAND		 NAND
 *   EMIB  ->	NAND		NOR		 NOR
 *
 */

static DEFINE_SPINLOCK(misc_lock);
char mb705_rev = '?';

static struct platform_device mb705_gpio_led = {
	.name = "leds-gpio",
	.id = 1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(2, 0),
				.active_low = 1,
			},
		},
	},
};

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x04800000,
			.end	= 0x048002ff,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 16,
	},
};


static struct platform_device mb705_display_device = {
	.name = "mb705-display",
	.id = -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x04800140,
			.end	= 0x048001bf,
			.flags	= IORESOURCE_MEM,
		}
	},
};

#ifdef CONFIG_SH_ST_MB680
static struct platform_device mb705_fpbutton_device = {
	.name = "mb705-fpbutton",
	.id = -1,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= ILC_EXT_IRQ(0),
			.end	= ILC_EXT_IRQ(0),
			.flags	= IORESOURCE_IRQ,
		}, {
			/* Mask for the EPLD status register */
			.name	= "mask",
			.start	= 1<<9,
			.end	= 1<<9,
			.flags	= IORESOURCE_IRQ,
		}
	},
};
#endif

/* NOR Flash */
static void nor_set_vpp(struct map_info *info, int enable)
{
	u16 reg;

	spin_lock(&misc_lock);

	reg = epld_read(EPLD_EMI_MISC);
	if (enable)
		reg |= EPLD_EMI_MISC_NORFLASHVPPEN;
	else
		reg &= ~EPLD_EMI_MISC_NORFLASHVPPEN;
	epld_write(reg, EPLD_EMI_MISC);

	spin_unlock(&misc_lock);
}

static struct platform_device mb705_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= nor_set_vpp,
		.nr_parts	= 3,
		.parts		= (struct mtd_partition []) {
			{
				.name = "NOR Flash 1",
				.size = 0x00080000,
				.offset = 0x00000000,
			}, {
				.name = "NOR Flash 2",
				.size = 0x00200000,
				.offset = MTDPART_OFS_NXTBLK,
			}, {
				.name = "NOR Flash 3",
				.size = MTDPART_SIZ_FULL,
				.offset = MTDPART_OFS_NXTBLK,
			}
		},
	},
};

/* Serial Flash */
static struct spi_board_info mb705_serial_flash =  {
	/* .bus_num and .chip_select set by processor board */
	.modalias	= "m25p80",
	.max_speed_hz	= 7000000,
	.mode		= SPI_MODE_3,
	.platform_data = &(struct flash_platform_data) {
		.name = "m25p80",
		.type = "m25p32",
		.nr_parts = 2,
		.parts = (struct mtd_partition []) {
			{
				.name = "Serial Flash 1",
				.size = 0x00080000,
				.offset = 0,
			}, {
				.name = "Serial Flash 2",
				.size = MTDPART_SIZ_FULL,
				.offset = MTDPART_OFS_NXTBLK,
			},
		},
	},
};

/* NAND Device */
struct stm_nand_bank_data mb705_nand_flash = {
	.csn		= 1,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.nr_partitions	= 2,
	.partitions	= (struct mtd_partition []) {
		{
			.name	= "NAND Flash 1",
			.offset	= 0,
			.size 	= 0x00800000
		}, {
			.name	= "NAND Flash 2",
			.offset = MTDPART_OFS_NXTBLK,
			.size	= MTDPART_SIZ_FULL
		},
	},
	.timing_data		= &(struct stm_nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30,		/* in us */
	},
};

static struct platform_device *mb705_devices[] __initdata = {
	&epld_device,
	&mb705_gpio_led,
	&mb705_display_device,
#ifdef CONFIG_SH_ST_MB680
	&mb705_fpbutton_device,
#endif
	&mb705_nor_flash,
};

static DEFINE_SPINLOCK(mb705_reset_lock);

void mb705_reset(int mask, unsigned long usdelay)
{
	u16 reg;

	spin_lock(&mb705_reset_lock);
	reg = epld_read(EPLD_EMI_RESET);
	reg |= mask;
	epld_write(reg, EPLD_EMI_RESET);
	spin_unlock(&mb705_reset_lock);

	udelay(usdelay);

	spin_lock(&mb705_reset_lock);
	reg = epld_read(EPLD_EMI_RESET);
	reg &= ~mask;
	epld_write(reg, EPLD_EMI_RESET);
	spin_unlock(&mb705_reset_lock);
}

static int __init mb705_init(void)
{
	int i;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/* We are't actually doing this early here... */
	epld_early_init(&epld_device);

	/* Check out the EPLDs */
	for (i=0; i<3; i++) {
		int ident_offset = (0x100 * i) + 0;
		int test_offset = (0x100 * i) + 2;
		u16 ident;
		u16 test;
		u16 mask = (i==0) ? 0xffff : 0xff;

		ident = epld_read(ident_offset);
		epld_write(0xab12+i, test_offset);
		test = epld_read(test_offset);

		printk(KERN_INFO
		       "mb705 %s_EPLD: board rev %c, EPLD rev %d, test %s\n",
		       (char*[3]){"EMI", "TS", "AUD" }[i],
		       ((ident >> 4) & 0xf) - 1 + 'A', ident & 0xf,
		       (((test ^ (0xab12+i)) & mask) == mask) ?
		        "passed" : "failed");
	}

	mb705_rev = ((epld_read(EPLD_EMI_IDENT) >> 4) & 0xf) - 1 + 'A';

	i = epld_read(EPLD_EMI_SWITCH);
	if (i & EPLD_EMI_SWITCH_BOOTFROMNOR) {
		pr_info("mb705: EMIA -> NAND; EMIB -> NOR\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		mb705_nand_flash.csn = 0;
	} else {
		pr_info("mb705: EMIA -> NOR; EMIB -> NAND\n");
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(1) - nor_bank_base;
		mb705_nand_flash.csn = 1;
	}

	/* Update NOR Flash base address and size: */
	/*     - limit bank size to 64MB (some targetpacks define 128MB!) */
	if (nor_bank_size > 64*1024*1024)
		nor_bank_size = 64*1024*1024;
	/*     - reduce visibility of NOR flash to EMI bank size */
	if (mb705_nor_flash.resource[0].end > nor_bank_size - 1)
		mb705_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	mb705_nor_flash.resource[0].start += nor_bank_base;
	mb705_nor_flash.resource[0].end += nor_bank_base;

	/*
	 * The MTD NAND code doesn't understand the concept of VPP,
	 * (or hardware write protect) so permanently enable it.
	 * Also disable NOR VPP enable just in case.
	 */
	i = epld_read(EPLD_EMI_MISC);
	i &= ~EPLD_EMI_MISC_NORFLASHVPPEN;
	i |= EPLD_EMI_MISC_NOTNANDFLASHWP;
	epld_write(i, EPLD_EMI_MISC);

	mbxxx_configure_nand_flash(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &mb705_nand_flash,
			.rbn.flex_connected = 1,});

	/* Interrupt routing.
	 * At the moment we only care about a small number of
	 * interrupts, so simply set up a static one-to-one routing.
	 *
	 * Interrupt sources:
	 *  0 : MAFE
	 *  1 : VOIP
	 *  2 : SPDIF out
	 *  3 : STRec status
	 *  4 : STEM0 (-> SysIrq(2) this matches the mb680 but active high)
	 *  5 : STEM1 (-> SysIrq(1) this matches the mb680 but active high))
	 *  6 : DVB
	 *  7 : DVB CD1
	 *  8 : DVB CD2
	 *  9 : FButton (-> SysIrq(0))
	 * 10 : EPLD intr in
	 */
	epld_write(0, EPLD_EMI_INT_PRI(9));
	epld_write(1, EPLD_EMI_INT_PRI(5));
	epld_write(2, EPLD_EMI_INT_PRI(4));
	epld_write((1<<4)|(1<<5)|(1<<9), EPLD_EMI_INT_MASK);

	mbxxx_configure_serial_flash(&mb705_serial_flash);

	return platform_add_devices(mb705_devices, ARRAY_SIZE(mb705_devices));
}
arch_initcall(mb705_init);
