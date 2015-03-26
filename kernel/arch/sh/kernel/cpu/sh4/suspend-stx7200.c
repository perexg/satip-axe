/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx7200.c
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/io.h>

#include <linux/stm/stx7200.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>

#include <asm/irq-ilc.h>

#include "stm_suspend.h"
#include <linux/stm/poke_table.h>

static struct clk *comms_clk;
static unsigned long comms_clk_rate;
static void __iomem *cga;
static void __iomem *cgb;

#define _SYS_STA(x) 		(0xfd704000 + 0x008 + (x) * 0x4)
#define _SYS_CFG(x)		(0xfd704000 + 0x100 + (x) * 0x4)


#define CLKA_PLL(x)			((x) * 0x04)
#define   CLKA_PLL_BYPASS		(1 << 20)
#define   CLKA_PLL_ENABLE_STATUS	(1 << 19)
#define   CLKA_PLL_LOCK			(1 << 31)
#define CLKA_PWR_CFG			0x1C

#define CLKB_PLL0_CFG			0x3C
#define   CLKB_PLL_LOCK			(1 << 31)
#define   CLKB_PLL_BYPASS		(1 << 20)
#define CLKB_PWR_CFG			0x58
#define   CLKB_PLL0_OFF			(1 << 15)

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7200_standby_table[] __cacheline_aligned = {

END_MARKER,

END_MARKER
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */

static unsigned long stx7200_mem_table[] __cacheline_aligned = {

/* 1. Enables the DDR self refresh mode */
OR32(_SYS_CFG(38), 1 << 20),
OR32(_SYS_CFG(39), 1 << 20),

/* waits until the ack bit is zero */
WHILE_NE32(_SYS_STA(4), 1, 1),
WHILE_NE32(_SYS_STA(6), 1, 1),

/* stop LMI clocks */
UPDATE32(_SYS_CFG(11), ~1, 0),
UPDATE32(_SYS_CFG(15), ~1, 0),

/* disable the analogue input buffers */
OR32(_SYS_CFG(12), 1 << 10),
OR32(_SYS_CFG(16), 1 << 10),

/* PLL_LMI power down */
OR32(_SYS_CFG(11), 1 << 12),

END_MARKER,

/* PLL_LMI power on */
UPDATE32(_SYS_CFG(11), ~(1 << 12), 0),

/* enable the analogue input buffers */
UPDATE32(_SYS_CFG(12), ~(1 << 10), 0),
UPDATE32(_SYS_CFG(16), ~(1 << 10), 0),

/* start LMI clocks */
OR32(_SYS_CFG(11), 1),
OR32(_SYS_CFG(15), 1),


/* LMI out of self-refresh */
UPDATE32(_SYS_CFG(38), ~(1 << 20), 0),
UPDATE32(_SYS_CFG(39), ~(1 << 20), 0),
/* wait until the ack bit is high        */
WHILE_NE32(_SYS_STA(4), 1, 0),
WHILE_NE32(_SYS_STA(6), 1, 0),

END_MARKER
};



static int stx7200_suspend_begin(suspend_state_t state)
{
	pr_info("[STM][PM] Analysing the wakeup devices\n");

	comms_clk_rate = clk_get_rate(comms_clk);
	comms_clk->rate = 30000000;	/* 30 MHz */

	return 0;
}

static int stx7200_suspend_core(suspend_state_t state, int suspending)
{
	unsigned long tmp;
	int i;

	if (suspending)
		goto on_suspending;

	/*
	 * ClockGen A Management
	 */
	/* power on the Plls */
	tmp = ioread32(cga + CLKA_PWR_CFG);
	iowrite32(tmp & ~7, cga + CLKA_PWR_CFG);


	for (i = 0; i < 3; ++i) {
		/* Wait the PLLs are stable */
		while ((ioread32(cga + CLKA_PLL(i)) & CLKA_PLL_LOCK)
			!= CLKA_PLL_LOCK)
				;
		/* remove the bypass */
		tmp = ioread32(cga + CLKA_PLL(i));
		iowrite32(tmp & ~(CLKA_PLL_BYPASS), cga + CLKA_PLL(i));
	}

	/*
	 * ClockGen B Management
	 */
	tmp = readl(cgb + CLKB_PWR_CFG);
	writel(tmp & ~CLKB_PLL0_OFF, cgb + CLKB_PWR_CFG);
	/* Wait PllB lock */
	while ((readl(cgb + CLKB_PLL0_CFG) & CLKB_PLL_LOCK) != CLKB_PLL_LOCK)
		;
	tmp = readl(cgb + CLKB_PLL0_CFG);
	writel(tmp & ~CLKB_PLL_BYPASS, cgb + CLKB_PLL0_CFG);

	comms_clk->rate = comms_clk_rate;
	return 0;

on_suspending:

	tmp = readl(cgb + CLKB_PLL0_CFG);
	writel(tmp | CLKB_PLL_BYPASS, cgb + CLKB_PLL0_CFG);
	tmp = readl(cgb + CLKB_PWR_CFG);
	writel(tmp | CLKB_PLL0_OFF, cgb + CLKB_PWR_CFG);

	tmp = ioread32(cga + CLKA_PLL(0));
	iowrite32(tmp | CLKA_PLL_BYPASS, cga + CLKA_PLL(0));

	tmp = ioread32(cga + CLKA_PLL(2));
	iowrite32(tmp | CLKA_PLL_BYPASS, cga + CLKA_PLL(2));

	if (state == PM_SUSPEND_STANDBY) {
		tmp = ioread32(cga + CLKA_PWR_CFG);
		iowrite32(tmp & ~5, cga + CLKA_PWR_CFG);
	} else {
		tmp = ioread32(cga + CLKA_PLL(1));
		iowrite32(tmp | CLKA_PLL_BYPASS, cga + CLKA_PLL(1));

		tmp = ioread32(cga + CLKA_PWR_CFG);
		iowrite32(tmp & ~7, cga + CLKA_PWR_CFG);
	}
	return 0;
}

static int stx7200_suspend_pre_enter(suspend_state_t state)
{
	return stx7200_suspend_core(state, 1);
}

static int stx7200_suspend_post_enter(suspend_state_t state)
{
	return stx7200_suspend_core(state, 0);
}

static int stx7200_evttoirq(unsigned long evt)
{
	return ((evt < 0x400) ? ilc2irq(evt) : evt2irq(evt));
}

static struct stm_platform_suspend_t stx7200_suspend __cacheline_aligned = {
	.ops.begin = stx7200_suspend_begin,

	.evt_to_irq = stx7200_evttoirq,
	.pre_enter = stx7200_suspend_pre_enter,
	.post_enter = stx7200_suspend_post_enter,

	.stby_tbl = (unsigned long)stx7200_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7200_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx7200_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7200_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init stx7200_suspend_setup(void)
{
	struct sysconf_field *sc[4];
	int i;

	sc[0] = sysconf_claim(SYS_STA, 4, 0, 0, "PM");
	sc[1] = sysconf_claim(SYS_STA, 6, 0, 0, "PM");
	sc[2]  = sysconf_claim(SYS_CFG, 38, 20, 20, "PM");
	sc[3]  = sysconf_claim(SYS_CFG, 39, 20, 20, "PM");

	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (!sc[i])
			goto error;

	cga = ioremap(0xfd700000, 0x1000);
	cgb = ioremap(0xfd701000, 0x1000);
	comms_clk = clk_get(NULL, "comms_clk");

	return stm_suspend_register(&stx7200_suspend);

error:
	for (i = ARRAY_SIZE(sc)-1; i; --i)
		if (sc[i])
			sysconf_release(sc[i]);
	return 0;
}

module_init(stx7200_suspend_setup);
