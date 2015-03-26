/*
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
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/io.h>

#include <linux/stm/clk.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stx5197.h>

#include <asm/irq.h>
#include <asm/irq-ilc.h>

#include "stm_suspend.h"
#include <linux/stm/poke_table.h>

#define XTAL			30000000
#define SYS_CLK			0xfdc00000

#define CLK_PLL_CONFIG0(x)	((x * 8) + 0x0)
#define CLK_PLL_CONFIG1(x)	((x * 8) + 0x4)
#define  CLK_PLL_CONFIG1_POFF	(1 << 13)
#define  CLK_PLL_CONFIG1_LOCK	(1 << 15)

#define CLK_MODE_CTRL		0x110
#define   CLK_MODE_CTRL_NULL	0x0
#define   CLK_MODE_CTRL_X1	0x1
#define   CLK_MODE_CTRL_PROG	0x2
#define   CLK_MODE_CTRL_STDB	0x3

#define CLK_REDUCED_PM_CTRL	0x114
#define   CLK_REDUCED_ON_XTAL_MEMSTDBY	(1 << 11)
#define   CLK_REDUCED_ON_XTAL_STDBY	(~(0x22))

#define CLK_LP_MODE_DIS0	0x118
#define   CLK_LP_MODE_DIS0_VALUE	(0x3 << 11)

#define CLK_LOCK_CFG		0x300


#define _CFG_HI_DENSITY		0xfd901000
#define _CFG_CTRL_H		0x14
#define _CFG_MONITOR_J		0x3c

#define LMI_BASE		0xFE000000
#define LMI_CONTROL		0x10
#define LMI_BURST_REFRESH	2
#define LMI_SCR_CST		100

/*
 * System Service Finite State Machine
 *	+-------+   +------+    +------+
 *	| reset |-->|  X1  |<-->| Prog |
 *	+-------+   +------+    +------+
 *	    	        ^	   |
 *	    		|	   V
 *		wakeup	|       +-------+
 *		event	+-------|Standby|
 *			        +-------+
 */
static struct clk *comms_clk;
static unsigned long comms_clk_rate;


/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx5197_standby_table[] __cacheline_aligned = {
POKE32(SYS_CLK + CLK_LOCK_CFG, 0xf0),
POKE32(SYS_CLK + CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */

POKE32(SYS_CLK + CLK_MODE_CTRL, CLK_MODE_CTRL_X1),

/* 1. Move all the clock on OSC */
OR32(SYS_CLK + CLK_REDUCED_PM_CTRL, CLK_REDUCED_ON_XTAL_STDBY),
OR32(SYS_CLK + CLK_PLL_CONFIG1(0), CLK_PLL_CONFIG1_POFF),
POKE32(SYS_CLK + CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
POKE32(SYS_CLK + CLK_LOCK_CFG, 0x100), /* Lock the clocks */

END_MARKER,

POKE32(SYS_CLK + CLK_LOCK_CFG, 0xf0),
POKE32(SYS_CLK + CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */
POKE32(SYS_CLK + CLK_MODE_CTRL, CLK_MODE_CTRL_X1),
UPDATE32(SYS_CLK + CLK_REDUCED_PM_CTRL, ~CLK_REDUCED_ON_XTAL_STDBY, 0),
UPDATE32(SYS_CLK + CLK_PLL_CONFIG1(0), ~CLK_PLL_CONFIG1_POFF, 0),
WHILE_NE32(SYS_CLK + CLK_PLL_CONFIG1(0),
	   CLK_PLL_CONFIG1_LOCK, CLK_PLL_CONFIG1_LOCK),
POKE32(SYS_CLK + CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
POKE32(SYS_CLK + CLK_LOCK_CFG, 0x100), /* Lock the clocks */
DELAY(1),

END_MARKER
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */

static unsigned long stx5197_mem_table[] __cacheline_aligned = {
POKE32(LMI_BASE + LMI_CONTROL,
       (0x4 | (LMI_BURST_REFRESH << 4) | (LMI_SCR_CST << 16))),
OR32(_CFG_HI_DENSITY + _CFG_CTRL_H, (1 << 26)),
WHILE_NE32(_CFG_HI_DENSITY + _CFG_MONITOR_J, (1 << 24), (1 << 24)),

POKE32(SYS_CLK + CLK_LOCK_CFG, 0xf0),
POKE32(SYS_CLK + CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */

/* disable PLLs in standby */
OR32(SYS_CLK + CLK_LP_MODE_DIS0, CLK_LP_MODE_DIS0_VALUE),
POKE32(SYS_CLK + CLK_MODE_CTRL, CLK_MODE_CTRL_STDB), /* IN STANDBY */

END_MARKER,
/*
 * On a wakeup Event the System Service goes directly in X1 mode */
UPDATE32(SYS_CLK + CLK_PLL_CONFIG1(0), ~CLK_PLL_CONFIG1_POFF, 0),
UPDATE32(SYS_CLK + CLK_PLL_CONFIG1(1), ~CLK_PLL_CONFIG1_POFF, 0),
/* Wait PLLs lock */
WHILE_NE32(SYS_CLK + CLK_PLL_CONFIG1(0),
	   CLK_PLL_CONFIG1_LOCK, CLK_PLL_CONFIG1_LOCK),
WHILE_NE32(SYS_CLK + CLK_PLL_CONFIG1(1),
	   CLK_PLL_CONFIG1_LOCK, CLK_PLL_CONFIG1_LOCK),

POKE32(SYS_CLK + CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
POKE32(SYS_CLK + CLK_LOCK_CFG, 0x100), /* Lock the clocks */

DELAY(1),

UPDATE32(_CFG_HI_DENSITY + _CFG_CTRL_H, ~(1 << 26), 0),
WHILE_NE32(_CFG_HI_DENSITY + _CFG_MONITOR_J, (1 << 24), 0),


DELAY(1),
POKE32(LMI_BASE + LMI_CONTROL, 0),

DELAY(1),
END_MARKER
};

/*
 * STx5197 clock architecture is very different from other SoC's, and
 * we don't want to risk making clock transitions using the clock
 * framework while suspending. So perform all the changes using the poke
 * table, and directly change the clock rates in the clock framework
 * here.
 */

static int stx5197_suspend_begin(suspend_state_t state)
{
	comms_clk_rate = clk_get_rate(comms_clk);
	comms_clk->rate = XTAL;
	return 0;
}

static int stx5197_suspend_post_enter(suspend_state_t state)
{
	comms_clk->rate = comms_clk_rate;
	return 0;
}

static int stx5197_evt_to_irq(unsigned long evt)
{
	return ((evt < 0x400) ? ilc2irq(evt) : evt2irq(evt));
}

static struct stm_platform_suspend_t stx5197_suspend __cacheline_aligned = {

	.flags = NO_SLEEP_ON_MEMSTANDBY,

	.ops.begin = stx5197_suspend_begin,

	.evt_to_irq = stx5197_evt_to_irq,
	.post_enter = stx5197_suspend_post_enter,

	.stby_tbl = (unsigned long)stx5197_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx5197_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx5197_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx5197_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init stx5197_suspend_setup(void)
{
	int i;
	struct sysconf_field *sc[2];

	sc[0] = sysconf_claim(CFG_MONITOR_J, 24, 24, "PM - LMI");
	sc[1] = sysconf_claim(CFG_CTRL_H, 26, 26, "PM - LMI");

	for (i = ARRAY_SIZE(sc)-1; i; --i)
		if (!sc[i])
			goto error;

	comms_clk = clk_get(NULL, "comms_clk");

	return stm_suspend_register(&stx5197_suspend);

error:
	for (i = ARRAY_SIZE(sc)-1; i; --i)
		if (sc[i])
			sysconf_release(sc[i]);
	return -EINVAL;
}

module_init(stx5197_suspend_setup);
