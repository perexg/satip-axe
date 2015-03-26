/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx7141.c
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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <linux/stm/sysconf.h>
#include <linux/stm/stx7141.h>
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include <asm/irq-ilc.h>

#include "stm_suspend.h"
#include <linux/stm/poke_table.h>

#define CGA				0xfe213000
#define CKGA_PLL(x)			((x) * 4)
#define CKGA_OSC_DIV_CFG(x)		(0x800 + (x) * 4)
#define CKGA_PLL0HS_DIV_CFG(x)		(0x900 + (x) * 4)
#define CKGA_PLL0LS_DIV_CFG(x)		(0xa10 + (x) * 4)
#define CKGA_PLL1_DIV_CFG(x)		(0xb00 + (x) * 4)
#define   CKGA_PLL_BYPASS		(1 << 20)
#define   CKGA_PLL_LOCK			(1 << 31)
#define CKGA_POWER_CFG			(0x010)
#define CKGA_CLKOPSRC_SWITCH_CFG(x)     (0x014 + ((x) * 0x10))


#define SYSCONF_BASE_ADDR		0xfe001000
#define _SYS_STA(x)			(4 * (x) + 0x8 + SYSCONF_BASE_ADDR)
#define _SYS_CFG(x)			(4 * (x) + 0x100 + SYSCONF_BASE_ADDR)

#define LMI_BASE			0xFE901000
#define   LMI_APPD(bank)		(0x1000 * (bank) + 0x14 + lmi)

static struct clk *ca_ref_clk;
static struct clk *ca_pll1_clk;
static struct clk *ca_ic_if_100_clk;
static unsigned long ca_ic_if_100_clk_rate;

static void __iomem *cga;
static void __iomem *lmi;
/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7141_standby_table[] __cacheline_aligned = {

END_MARKER,

END_MARKER
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx7141_mem_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode */
OR32(_SYS_CFG(38), (1 << 20)),
/* waits until the ack bit is zero */
WHILE_NE32(_SYS_STA(4), 1, 1),
/* Disable the analogue input buffers of the pads */
OR32(_SYS_CFG(12), (1 << 10)),
/* Disable the clock output */
UPDATE32(_SYS_CFG(4), ~(1 << 2), 0),
/* 1.1 Turn-off the ClockGenD */
OR32(_SYS_CFG(11), (1 << 12)),
/* wait clock gen lock */
WHILE_NE32(_SYS_STA(3), 1, 1),

POKE32(CGA + CKGA_OSC_DIV_CFG(17), 31),

END_MARKER,

POKE32(CGA + CKGA_OSC_DIV_CFG(17), 0),

UPDATE32(_SYS_CFG(12), ~(1 << 10), 0),
/* 1. Turn-on the LMI ClocksGenD */
UPDATE32(_SYS_CFG(11), ~(1 << 12), 0),
/* Wait LMI ClocksGenD lock */
WHILE_NE32(_SYS_STA(3), 1, 0),
/* Enable clock ouput */
OR32(_SYS_CFG(4), (1 << 2)),
/* Reset LMI Pad logic */
OR32(_SYS_CFG(11), (1 << 27)),
/* 2. Disables the DDR self refresh mode */
UPDATE32(_SYS_CFG(38), ~(1 << 20), 0),
/* waits until the ack bit is zero */
WHILE_NE32(_SYS_STA(4), 1, 0),

END_MARKER
};

static struct stm_wakeup_devices wkd;

static int stx7141_suspend_begin(suspend_state_t state)
{
	pr_info("[STM][PM] Analyzing the wakeup devices\n");

	stm_check_wakeup_devices(&wkd);

	if (wkd.hdmi_can_wakeup)
		/* Need to keep ic_if_100 @ 100 MHz */
		return 0;

	ca_ic_if_100_clk_rate = clk_get_rate(ca_ic_if_100_clk);
	clk_set_parent(ca_ic_if_100_clk, ca_ref_clk);
	/* 15 MHz is safe to go... */
	clk_set_rate(ca_ic_if_100_clk, clk_get_rate(ca_ref_clk)/2);

	return 0;
}

static void stx7141_suspend_wake(void)
{
	if (wkd.hdmi_can_wakeup)
		return;

	/* Restore ic_if_100 to previous rate */
	clk_set_parent(ca_ic_if_100_clk, ca_pll1_clk);
	clk_set_rate(ca_ic_if_100_clk, ca_ic_if_100_clk_rate);

}

static int stx7141_suspend_core(suspend_state_t state, int suspending)
{
	static unsigned char *clka_pll0_div;
	static unsigned char *clka_pll1_div;
	static unsigned long *clka_switch_cfg;
	static unsigned long saved_gplmi_appd;
	int i;

	if (suspending)
		goto on_suspending;

	if (!clka_pll0_div) /* there was an error on suspending */
		return 0;

	/* Resuming... */
	iowrite32(0, cga + CKGA_POWER_CFG);
	while (!(ioread32(cga + CKGA_PLL(0)) & CKGA_PLL_LOCK))
		;
	while (!(ioread32(cga + CKGA_PLL(1)) & CKGA_PLL_LOCK))
		;

	/* applay the original parents */
	iowrite32(clka_switch_cfg[0], cga + CKGA_CLKOPSRC_SWITCH_CFG(0));
	iowrite32(clka_switch_cfg[1], cga + CKGA_CLKOPSRC_SWITCH_CFG(1));

	/* restore all the clocks settings */
	for (i = 0; i < 18; ++i) {
		iowrite32(clka_pll0_div[i], cga + ((i < 4) ?
				CKGA_PLL0HS_DIV_CFG(i) :
				CKGA_PLL0LS_DIV_CFG(i)));
		iowrite32(clka_pll1_div[i], cga + CKGA_PLL1_DIV_CFG(i));
	}
	mdelay(10);
	pr_devel("[STM][PM] ClockGen A: restored\n");


	/* restore the APPD */
	iowrite32(saved_gplmi_appd, LMI_APPD(0));

	kfree(clka_pll0_div);
	kfree(clka_pll1_div);
	kfree(clka_switch_cfg);
	clka_switch_cfg = NULL;
	clka_pll0_div = clka_pll1_div = NULL;

	stx7141_suspend_wake();
	return 0;

on_suspending:
	clka_pll0_div = kmalloc(sizeof(char) * 18, GFP_ATOMIC);
	clka_pll1_div = kmalloc(sizeof(char) * 18, GFP_ATOMIC);
	clka_switch_cfg = kmalloc(sizeof(long) * 2, GFP_ATOMIC);

	if (!clka_pll0_div || !clka_pll1_div || !clka_switch_cfg)
		goto error;

	/* save the current APPD setting*/
	saved_gplmi_appd = ioread32(LMI_APPD(0));
	/* disable the APPD */
	iowrite32(0x0, LMI_APPD(0));


	/* save the original settings */
	clka_switch_cfg[0] = ioread32(cga + CKGA_CLKOPSRC_SWITCH_CFG(0));
	clka_switch_cfg[1] = ioread32(cga + CKGA_CLKOPSRC_SWITCH_CFG(1));

	for (i = 0; i < 18; ++i) {
		clka_pll0_div[i] = ioread32(cga +
			((i < 4) ? CKGA_PLL0HS_DIV_CFG(i) :
				CKGA_PLL0LS_DIV_CFG(i)));
		clka_pll1_div[i] = ioread32(cga + CKGA_PLL1_DIV_CFG(i));
	}
	pr_devel("[STM][PM] ClockGen A: saved\n");
	mdelay(10);

	/* to avoid the system is too much slow all
	 * the clocks are scaled @ 30 MHz
	 * the final setting is done in the tables
	 */
	/* to avoid the system is to much slow all
	 * the clocks are scaled @ 30 MHz
	 * the final setting is done in the tables
	 */
	for (i = 0; i < 18; ++i)
		iowrite32(0, cga + CKGA_OSC_DIV_CFG(i));

	/* almost all the clocks off */
	iowrite32(0xffcffcff, cga + CKGA_CLKOPSRC_SWITCH_CFG(0));
	iowrite32(0x3, cga + CKGA_CLKOPSRC_SWITCH_CFG(1));

	if (wkd.hdmi_can_wakeup) {/* cga_pll1 still powered */
		/* move the ic_if_100 again under pll1 */
		iowrite32(0x800, cga + CKGA_CLKOPSRC_SWITCH_CFG(0));
		iowrite32(1, cga + CKGA_POWER_CFG);
	} else {
		iowrite32(3, cga + CKGA_POWER_CFG);
		if (!wkd.lirc_can_wakeup)
			clk_set_rate(ca_ic_if_100_clk,
				    clk_get_rate(ca_ref_clk)/32);
	}

	return 0;

error:
	kfree(clka_pll1_div);
	kfree(clka_pll0_div);
	kfree(clka_switch_cfg);

	clka_switch_cfg = NULL;
	clka_pll0_div = clka_pll1_div = NULL;

	return -ENOMEM;
}

static int stx7141_suspend_pre_enter(suspend_state_t state)
{
	return stx7141_suspend_core(state, 1);
}

static int stx7141_suspend_post_enter(suspend_state_t state)
{
	return stx7141_suspend_core(state, 0);
}

static int stx7141_evttoirq(unsigned long evt)
{
	return ((evt == 0xa00) ? ilc2irq(evt) : evt2irq(evt));
}

static struct stm_platform_suspend_t stx7141_suspend __cacheline_aligned = {

	.ops.begin = stx7141_suspend_begin,

	.evt_to_irq = stx7141_evttoirq,
	.pre_enter = stx7141_suspend_pre_enter,
	.post_enter = stx7141_suspend_post_enter,

	.stby_tbl = (unsigned long)stx7141_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7141_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx7141_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7141_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init stx7141_suspend_setup(void)
{
	struct sysconf_field *sc[7];
	int i;

	sc[0] = sysconf_claim(SYS_STA, 4, 0, 0, "PM");
	sc[1] = sysconf_claim(SYS_STA, 3, 0, 0, "PM");
	sc[2] = sysconf_claim(SYS_CFG, 4, 2, 2, "PM");
	sc[3] = sysconf_claim(SYS_CFG, 11, 12, 12, "PM");
	sc[4] = sysconf_claim(SYS_CFG, 11, 27, 27, "PM");
	sc[5] = sysconf_claim(SYS_CFG, 12, 10, 10, "PM");
	sc[6] = sysconf_claim(SYS_CFG, 38, 20, 20, "PM");

	for (i = ARRAY_SIZE(sc)-1; i; --i)
		if (!sc[i])
			goto error;

	ca_ref_clk = clk_get(NULL, "CLKA_REF");
	ca_pll1_clk = clk_get(NULL, "CLKA_PLL1");
	ca_ic_if_100_clk = clk_get(NULL, "CLKA_IC_IF_100");

	if (!ca_ref_clk || !ca_pll1_clk || !ca_ic_if_100_clk)
		goto error;

	cga = ioremap(CGA, 0x1000);
	lmi = ioremap(LMI_BASE, 0x1000);

	return stm_suspend_register(&stx7141_suspend);

error:
	for (i = ARRAY_SIZE(sc)-1; i; --i)
		if (sc[i])
			sysconf_release(sc[i]);
	return 0;
}

module_init(stx7141_suspend_setup);
