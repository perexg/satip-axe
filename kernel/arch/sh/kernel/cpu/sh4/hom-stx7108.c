/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/io.h>

#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/gpio.h>

#include <asm/irq-ilc.h>

#include "stm_hom.h"
#include <linux/stm/poke_table.h>

/*
 * The Stx7108 uses the Synopsys IP Dram Controller
 * For registers description see:
 * 'DesignWare Cores DDR3/2 SDRAM Protocol - Controller -
 *  Databook - Version 2.10a - February 4, 2009'
 */
#define DDR3SS0_REG		0xFDE50000

#define DDR_SCTL		0x4
#define  DDR_SCTL_CFG			0x1
#define  DDR_SCTL_GO			0x2
#define  DDR_SCTL_SLEEP			0x3
#define  DDR_SCTL_WAKEUP		0x4

#define DDR_STAT		0x8
#define  DDR_STAT_CONFIG		0x1
#define  DDR_STAT_ACCESS		0x3
#define  DDR_STAT_LOW_POWER		0x5

#define DDR_PHY_IOCRV1		0x31C
#define DDR_PHY_PIR		0x404
#define DDR_PHY_DXCCR		0x434

#define PCLK	100000000
#define BAUDRATE_VAL_M1(bps)    	\
	((((bps * (1 << 14)) + (1 << 13)) / (PCLK / (1 << 6))))

/*
 * Based on the "stx7108 - external micro protocol"
 * stx7108 has to notify the 'remove power now'
 * pulling down the i2c lines (SCL-SDA)
 * To do that the st40 has to turns-off the SSC (just
 * put the pins into PIO mode and drive them directly).
 */
#define GPIO(_x)		(0xfd720000 + (_x) * 0x1000)
#define GPIO_SET_OFFSET		0x4 /* to push high */
#define GPIO_RESET_OFFSET	0x8 /* to push low */

#define SYS_BANK_2		0xfda50000
#define SYS_PIO_5		0x14

#define SYS_BANK_1             0xfde20000
#define SYS_B1_CFG4            0x4C


static unsigned long stx7108_hom_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode based on paraghaph. 7.1.4
 *    -> from ACCESS to LowPower
 */
POKE32(DDR3SS0_REG + DDR_SCTL, DDR_SCTL_SLEEP),
WHILE_NE32(DDR3SS0_REG + DDR_STAT, DDR_STAT_LOW_POWER, DDR_STAT_LOW_POWER),

OR32(DDR3SS0_REG + DDR_PHY_DXCCR, 1),

OR32(DDR3SS0_REG + DDR_PHY_PIR, 1 << 7),

/*
 * Force the SCL/SDA lines low to guarantee the i2c violation
 */
OR32(SYS_BANK_1 + SYS_B1_CFG4, 1),
UPDATE32(SYS_BANK_2 + SYS_PIO_5, ~(3 << 24), 0),
UPDATE32(SYS_BANK_2 + SYS_PIO_5, ~(3 << 28), 0),
POKE32(GPIO(5) + STM_GPIO_REG_CLR_POUT, 0x3 << 6),

/* END. */
END_MARKER,

};

static void __iomem *early_console_base;

static void stx7108_hom_early_console(void)
{
	writel(0x1189 & ~0x80, early_console_base + 0x0c); /* ctrl */
	writel(BAUDRATE_VAL_M1(115200), early_console_base); /* baud */
	writel(20, early_console_base + 0x1c);  /* timeout */
	writel(1, early_console_base + 0x10); /* int */
	writel(0x1189, early_console_base + 0x0c); /* ctrl */
	mdelay(100);
	pr_info("Early console ready\n");
}

static int stx7108_hom_prepare(void)
{
	stm_freeze_board(NULL);

	/*
	 * Set SCA/SCL high temporarily.
	 * They will be pushed low in the poketable
	 */
	stm_gpio_direction(stm_gpio(5, 6), STM_GPIO_DIRECTION_OUT);
	stm_gpio_direction(stm_gpio(5, 7), STM_GPIO_DIRECTION_OUT);

	return 0;
}

static int stx7108_hom_complete(void)
{
	hom_printk("%s - Enter ", __func__);
	/* Enable the INTC2 */
	writel(7, 0xfda30000 + 0x00);	/* INTPRI00 */
	writel(1, 0xfda30000 + 0x60);	/* INTMSKCLR00 */

	stm_defrost_board(NULL);
	hom_printk("%s - DONE ", __func__);
	return 0;
}

static struct stm_mem_hibernation stx7108_hom = {

	.tbl_addr = (unsigned long)stx7108_hom_table,
	.tbl_size = DIV_ROUND_UP(ARRAY_SIZE(stx7108_hom_table) *
			sizeof(long), L1_CACHE_BYTES),

	.ops.prepare = stx7108_hom_prepare,
	.ops.complete = stx7108_hom_complete,
};

static int __init hom_stx7108_setup(void)
{
	int ret;

	ret = stm_hom_register(&stx7108_hom);
	if (!ret) {
		early_console_base = (void *)
		ioremap(stm_asc_configured_devices[stm_asc_console_device]
			->resource[0].start, 0x1000);
		pr_info("[STM]: [PM]: [HoM]: Early console @ 0x%x\n",
			early_console_base);
		stx7108_hom.early_console = stx7108_hom_early_console;
	}
	return ret;
}

module_init(hom_stx7108_setup);
