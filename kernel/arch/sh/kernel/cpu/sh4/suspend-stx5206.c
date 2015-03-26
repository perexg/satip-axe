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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/io.h>

#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>
#include <linux/stm/stx5206.h>
#include <linux/stm/wakeup_devices.h>

#include "stm_suspend.h"
#include <linux/stm/poke_table.h>

#include <asm/irq.h>
#include <asm/irq-ilc.h>

#define CGA				0xFE213000

#define CKGA_PLL0_CFG			0x000
#define CKGA_PLL1_CFG			0x004
#define   CKGA_PLL_CFG_LOCK		(1 << 31)
#define CKGA_POWER_CFG			0x010

#define CKGA_OSC_DIV_CFG(x)		(0x800 + (x) * 4)
#define CKGA_PLL0HS_DIV_CFG(x)		(0x900 + (x) * 4)
#define CKGA_PLL0LS_DIV_CFG(x)		(0xA00 + (x) * 4)
#define CKGA_PLL1_DIV_CFG(x)		(0xB00 + (x) * 4)

#define CKGA_CLKOPSRC_SWITCH_CFG	0x014
#define CKGA_CLKOPSRC_SWITCH_CFG2	0x024

#define CLKA_SH4_ICK_ID			4
#define CLKA_IC_IF_100_ID		5
#define CLKA_ETH_PHY_ID			13
#define CLKA_IC_COMPO_200_ID		16
#define CLKA_IC_IF_200_ID		17

#define LMI_BASE			0xFE901000
#define   LMI_APPD(bank)		(0x1000 * (bank) + 0x14 + lmi)

#define SYSCONF(x)			(0xFE001000 + (x * 0x4) + 0x100)
#define SYSSTA(x)			(0xFE001000 + (x * 0x4) + 0x8)

static void __iomem *cga;
static void __iomem *lmi;
static struct clk *ca_ref_clk;
static struct clk *ca_pll1_clk;
static struct clk *ca_ic_if_100_clk;
static unsigned long ca_ic_if_100_clk_rate;


/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx5206_standby_table[] __cacheline_aligned = {

END_MARKER,

END_MARKER
};


/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx5206_mem_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode */
OR32(SYSCONF(38), (1 << 20)),
/* waits until the ack bit is zero */
WHILE_NE32(SYSSTA(4), 1, 1),
/* Disable the clock output */
UPDATE32(SYSCONF(4), ~(1 << 2), 0),
/* Disable the analogue input buffers of the pads */
OR32(SYSCONF(12), (1 << 10)),

/* 1.1 Turn-off the ClockGenD */
OR32(SYSCONF(11), (1 << 12)),
/* wait clock gen lock */
WHILE_NE32(SYSSTA(3), 1, 1),

END_MARKER,

UPDATE32(SYSCONF(12), ~(1 << 10), 0),
/* Reset LMI Pad logic */
OR32(SYSCONF(11), (1 << 27)),
/* 1. Turn-on the LMI ClocksGenD */
UPDATE32(SYSCONF(11), ~(1 << 12), 0),
/* Wait LMI ClocksGenD lock */
WHILE_NE32(SYSSTA(3), 1, 1),

/* Enable clock ouput */
OR32(SYSCONF(4), (1 << 2)),
/* 2. Disables the DDR self refresh mode */
UPDATE32(SYSCONF(38), ~(1 << 20), 0),
/* waits until the ack bit is zero */
WHILE_NE32(SYSSTA(4), 1, 0),

END_MARKER
};

static struct stm_wakeup_devices wkd;

static int stx5206_suspend_begin(suspend_state_t state)
{
	pr_info("[STM][PM] Analyzing the wakeup devices\n");

	stm_check_wakeup_devices(&wkd);

	if (wkd.hdmi_can_wakeup)
		/* need to keep ic_if_100 @ 100 MHz */
		return 0;

	ca_ic_if_100_clk_rate = clk_get_rate(ca_ic_if_100_clk);
	clk_set_parent(ca_ic_if_100_clk, ca_ref_clk);
	/* 15 MHz is safe to go... */
	clk_set_rate(ca_ic_if_100_clk, clk_get_rate(ca_ref_clk)/2);

	return 0;
}

static void stx5206_suspend_wake(void)
{
	if (wkd.hdmi_can_wakeup)
		return;

	/* Restore ic_if_100 to previous rate*/
	clk_set_parent(ca_ic_if_100_clk, ca_pll1_clk);
	clk_set_rate(ca_ic_if_100_clk, ca_ic_if_100_clk_rate);
}

static int stx5206_suspend_core(suspend_state_t state, int suspending)
{
	static unsigned int *clka_switch_cfg;
	static unsigned char *clka_pll0_div;
	static unsigned char *clka_pll1_div;
	static unsigned long saved_gplmi_appd;
	int i;
	long pwr = 0x3;			/* PLL_0/PLL_1 both OFF */
	long cfg_0, cfg_1;

	/* almost all the clocks off, except some critical ones */
	cfg_0 = 0xffffffff;
	cfg_0 &= ~(0x3 << (2 * CLKA_SH4_ICK_ID));
	cfg_0 &= ~(0x3 << (2 * CLKA_IC_IF_100_ID));
	cfg_1 = 0xf;
	cfg_1 &= ~(0x3 << (2 * (CLKA_IC_COMPO_200_ID-16)));
	cfg_1 &= ~(0x3 << (2 * (CLKA_IC_IF_200_ID-16)));

	if (suspending)
		goto on_suspending;

	if (!clka_pll0_div) /* there was an error on suspending */
		return 0;

	/* Resuming... */
	iowrite32(0, cga + CKGA_POWER_CFG);
	while (!(ioread32(cga + CKGA_PLL0_CFG) & CKGA_PLL_CFG_LOCK))
		;
	while (!(ioread32(cga + CKGA_PLL1_CFG) & CKGA_PLL_CFG_LOCK))
		;

	/* applay the original parents */
	iowrite32(clka_switch_cfg[0], cga + CKGA_CLKOPSRC_SWITCH_CFG);
	iowrite32(clka_switch_cfg[1], cga + CKGA_CLKOPSRC_SWITCH_CFG2);

	/* restore all the clocks settings */
	for (i = 0; i < 18; ++i) {
		iowrite32(clka_pll0_div[i], cga +
			((i < 4) ? CKGA_PLL0HS_DIV_CFG(i) :
				CKGA_PLL0LS_DIV_CFG(i)));
		iowrite32(clka_pll1_div[i], cga + CKGA_PLL1_DIV_CFG(i));
	}
	mdelay(10);
	pr_devel("[STM][PM] ClockGen A: restored\n");

	/* Update the gpLMI.APPD */
	iowrite32(saved_gplmi_appd, LMI_APPD(0));

	kfree(clka_pll0_div);
	kfree(clka_pll1_div);
	kfree(clka_switch_cfg);
	clka_switch_cfg = NULL;
	clka_pll0_div = clka_pll1_div = NULL;

	stx5206_suspend_wake();
	return 0;


on_suspending:
	clka_pll0_div = kmalloc(sizeof(char) * 18, GFP_ATOMIC);
	clka_pll1_div = kmalloc(sizeof(char) * 18, GFP_ATOMIC);
	clka_switch_cfg = kmalloc(sizeof(long) * 2, GFP_ATOMIC);

	if (!clka_pll0_div || !clka_pll1_div || !clka_switch_cfg)
		goto error;

	/* Turn-off the gpLMI.APPD */
	saved_gplmi_appd = ioread32(LMI_APPD(0));
	/* disable the APPD */
	iowrite32(0x0, LMI_APPD(0));

	/* save the original settings */
	clka_switch_cfg[0] = ioread32(cga + CKGA_CLKOPSRC_SWITCH_CFG);
	clka_switch_cfg[1] = ioread32(cga + CKGA_CLKOPSRC_SWITCH_CFG2);


	for (i = 0; i < 18; ++i) {
		clka_pll0_div[i] = ioread32(cga +
			((i < 4) ? CKGA_PLL0HS_DIV_CFG(i) :
				CKGA_PLL0LS_DIV_CFG(i)));
		clka_pll1_div[i] = ioread32(cga + CKGA_PLL1_DIV_CFG(i));
		}
	pr_devel("[STM][PM] ClockGen A: saved\n");
	mdelay(10);

	if (wkd.hdmi_can_wakeup) {
		/* Pll_1 on */
		pwr &= ~2;
		/* ic_if_100 under pll1 */
		cfg_0 &=  ~(0x3 << (2 * CLKA_IC_IF_100_ID));
		cfg_0 |= (0x2 << (2 * CLKA_IC_IF_100_ID));
	}
	if (wkd.eth_phy_can_wakeup) {
		/* Pll_0 on */
		pwr &= ~1;
		/* eth_phy_clk under pll0 */
		cfg_0 &= ~(0x3 << (2 * CLKA_ETH_PHY_ID));
		cfg_0 |= (0x1 << (2 * CLKA_ETH_PHY_ID));
	}

	iowrite32(cfg_0, cga + CKGA_CLKOPSRC_SWITCH_CFG);
	iowrite32(cfg_1, cga + CKGA_CLKOPSRC_SWITCH_CFG2);
	iowrite32(pwr, cga + CKGA_POWER_CFG);

	if (!wkd.lirc_can_wakeup)
		clk_set_rate(ca_ic_if_100_clk,
			    clk_get_rate(ca_ref_clk)/32);
	return 0;

error:
	kfree(clka_pll1_div);
	kfree(clka_pll0_div);
	kfree(clka_switch_cfg);

	clka_switch_cfg = NULL;
	clka_pll0_div = clka_pll1_div = NULL;

	return -ENOMEM;
}


static int stx5206_suspend_pre_enter(suspend_state_t state)
{
	return stx5206_suspend_core(state, 1);
}

static int stx5206_suspend_post_enter(suspend_state_t state)
{
	return stx5206_suspend_core(state, 0);
}

static int stx5206_evt_to_irq(unsigned long evt)
{
	return ((evt < 0x400) ? ilc2irq(evt) : evt2irq(evt));
}

static struct stm_platform_suspend_t stx5206_suspend __cacheline_aligned = {

	.ops.begin = stx5206_suspend_begin,

	.evt_to_irq = stx5206_evt_to_irq,
	.pre_enter = stx5206_suspend_pre_enter,
	.post_enter = stx5206_suspend_post_enter,

	.stby_tbl = (unsigned long)stx5206_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx5206_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx5206_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx5206_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init stx5206_suspend_setup(void)
{
	int i;
	struct sysconf_field *sc[7];

	sc[0] = sysconf_claim(SYS_CFG, 4, 2, 2, "LMI - PM");
	sc[1] = sysconf_claim(SYS_CFG, 11, 12, 12, "LMI - PM");
	sc[2] = sysconf_claim(SYS_CFG, 11, 27, 27, "LMI - PM");
	sc[3] = sysconf_claim(SYS_CFG, 12, 10, 10, "LMI - PM");
	sc[4] = sysconf_claim(SYS_CFG, 38, 20, 20, "LMI - PM");

	sc[5] = sysconf_claim(SYS_STA, 3, 0, 0, "LMI - PM");
	sc[6] = sysconf_claim(SYS_STA, 4, 0, 0, "LMI - PM");

	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (!sc[i])
			goto error;

	cga = ioremap(CGA, 0x1000);
	lmi = ioremap(LMI_BASE, 0x1000);

	ca_ref_clk = clk_get(NULL, "CLKA_REF");
	ca_pll1_clk = clk_get(NULL, "CLKA_PLL1");
	ca_ic_if_100_clk = clk_get(NULL, "CLKA_IC_IF_100");

	if (!ca_ref_clk || !ca_pll1_clk || !ca_ic_if_100_clk)
		goto error;

	return stm_suspend_register(&stx5206_suspend);
error:
	pr_err("[STM][PM] Error to acquire the sysconf registers\n");
	for (i = 0; i > ARRAY_SIZE(sc); ++i)
		if (sc[i])
			sysconf_release(sc[i]);

	return -1;
}

module_init(stx5206_suspend_setup);
