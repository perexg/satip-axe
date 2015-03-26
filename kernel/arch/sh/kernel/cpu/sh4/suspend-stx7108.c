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
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include <asm/irq-ilc.h>

#include "stm_suspend.h"
#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

#define CGA0			0xFDE98000
#define CGA1			0xFDAB8000
#define SYS_1_BASE_ADDRESS	0xFDE20000
/*
 * The Stx7108 uses the Synopsys IP Dram Controller
*/
#define DDR3SS0_REG		0xFDE50000
#define DDR3SS1_REG		0xFDE70000

#define CKGA_PLL_CFG(x)			(0x4 * (x))
#define   CKGA_PLL_CFG_LOCK		(1 << 31)
#define CKGA_POWER_CFG			0x010
#define CKGA_CLKOPSRC_SWITCH_CFG	0x014
#define CKGA_CLKOPSRC_SWITCH_CFG2	0x024
#define CKGA_OSC_DIV_CFG(x)		(0x800 + (x) * 4)
#define CKGA_PLL0HS_DIV_CFG(x)		(0x900 + (x) * 4)
#define CKGA_PLL0LS_DIV_CFG(x)		(0xA00 + (x) * 4)
#define CKGA_PLL1_DIV_CFG(x)		(0xB00 + (x) * 4)

#define CLKA0_ETH_PHY_ID		14
#define CLKA0_ETH_MAC_ID		15
/*
 * the following macros are valid only for SYSConf_Bank_1
  * where there are the ClockGen_D management registers
 */
#define SYS_BNK1_STA(x)		(0x4 * (x))
#define SYS_BNK1_CFG(x)		(0x4 * (x) + 0x3C)


static void __iomem *cga0;
static void __iomem *cga1;

static struct clk *ca0_pll1_clk;
static struct clk *ca0_eth_phy_clk;
static struct clk *ca1_ref_clk;
static struct clk *ca1_pll1_clk;
static struct clk *ca1_ic_lp_on_clk;
static unsigned long ca1_ic_lp_on_clk_rate;


/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7108_standby_table[] __cacheline_aligned = {
END_MARKER,

END_MARKER
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx7108_mem_table_c1[] __cacheline_aligned = {
synopsys_ddr32_in_self_refresh(DDR3SS0_REG),

OR32(DDR3SS0_REG + DDR_PHY_IOCRV1, 1),
OR32(DDR3SS0_REG + DDR_PHY_DXCCR, 1),

synopsys_ddr32_in_self_refresh(DDR3SS1_REG),

OR32(DDR3SS1_REG + DDR_PHY_IOCRV1, 1),
OR32(DDR3SS1_REG + DDR_PHY_DXCCR, 1),

OR32(DDR3SS0_REG + DDR_PHY_PIR, 1 << 7),
OR32(DDR3SS1_REG + DDR_PHY_PIR, 1 << 7),

/*WHILE_NE32(SYS_BNK1_STA(5), 1, 0),*/

 /* END. */
END_MARKER,

UPDATE32(DDR3SS0_REG + DDR_PHY_PIR, ~(1 << 7), 0),
UPDATE32(DDR3SS1_REG + DDR_PHY_PIR, ~(1 << 7), 0),
/*WHILE_NE32(SYS_BNK1_STA(5), 1, 1),*/

UPDATE32(DDR3SS0_REG + DDR_PHY_IOCRV1, ~1, 0),
UPDATE32(DDR3SS0_REG + DDR_PHY_DXCCR, ~1, 0),

UPDATE32(DDR3SS1_REG + DDR_PHY_IOCRV1, ~1, 0),
UPDATE32(DDR3SS1_REG + DDR_PHY_DXCCR, ~1, 0),


/* 2. Disables the DDR self refresh mode based on paraghaph 7.1.3
 *    -> from LowPower to Access
 */
synopsys_ddr32_out_of_self_refresh(DDR3SS0_REG),
synopsys_ddr32_out_of_self_refresh(DDR3SS1_REG),
END_MARKER
};


static unsigned long stx7108_mem_table_c2[] __cacheline_aligned = {
synopsys_ddr32_in_self_refresh(DDR3SS0_REG),
synopsys_ddr32_in_self_refresh(DDR3SS1_REG),

/*WHILE_NE32(SYS_BNK1_STA(5), 1, 0),*/

OR32(DDR3SS0_REG + DDR_PHY_DXCCR, 1),
OR32(DDR3SS1_REG + DDR_PHY_DXCCR, 1),

OR32(DDR3SS0_REG + DDR_PHY_PIR, 1 << 7),
OR32(DDR3SS1_REG + DDR_PHY_PIR, 1 << 7),
 /* END. */
END_MARKER,

UPDATE32(DDR3SS0_REG + DDR_PHY_PIR, ~(1 << 7), 0),
UPDATE32(DDR3SS1_REG + DDR_PHY_PIR, ~(1 << 7), 0),


UPDATE32(DDR3SS0_REG + DDR_PHY_DXCCR, ~1, 0),
UPDATE32(DDR3SS1_REG + DDR_PHY_DXCCR, ~1, 0),

synopsys_ddr32_out_of_self_refresh(DDR3SS0_REG),
synopsys_ddr32_out_of_self_refresh(DDR3SS1_REG),

END_MARKER
};
static struct stm_wakeup_devices wkd;

void __attribute__ ((weak)) pm_suspend_board_ntfy(void) {};
void __attribute__ ((weak)) pm_wake_board_ntfy(void) {};

static int stx7108_suspend_begin(suspend_state_t state)
{
	pr_info("[STM][PM] Analyzing the wakeup devices\n");

	pm_suspend_board_ntfy();

	stm_check_wakeup_devices(&wkd);
	mdelay(40);

	if (wkd.hdmi_can_wakeup)
		return 0;

	ca1_ic_lp_on_clk_rate = clk_get_rate(ca1_ic_lp_on_clk);
	/* Set ic_if_100 @ 15MHz */
	clk_set_parent(ca1_ic_lp_on_clk, ca1_ref_clk);
	clk_set_rate(ca1_ic_lp_on_clk, clk_get_rate(ca1_ref_clk)/2);

	return 0;
}

static void stx7108_suspend_wake(void)
{
	if (wkd.hdmi_can_wakeup)
		return;

	/* ic_if_100 @ 30 MHz */
	clk_set_parent(ca1_ic_lp_on_clk, ca1_pll1_clk);
	clk_set_rate(ca1_ic_lp_on_clk, ca1_ic_lp_on_clk_rate);
}


static int stx7108_suspend_core(suspend_state_t state, int suspending)
{
	static char *pll0_regs;
	static char *pll1_regs;
	static long *switch_cfg;
	int i;
	unsigned long cfg_a0_0, pwr_a0;

	if (suspending)
		goto on_suspending;

	if (!(pll0_regs && pll1_regs && switch_cfg))
		return 0;

	iowrite32(0, cga0 + CKGA_POWER_CFG);
	iowrite32(0, cga1 + CKGA_POWER_CFG);

	for (i = 0; i < 2; ++i)
		while (!(ioread32(cga0 + CKGA_PLL_CFG(i)) & CKGA_PLL_CFG_LOCK));
	for (i = 0; i < 2; ++i)
		while (!(ioread32(cga1 + CKGA_PLL_CFG(i)) & CKGA_PLL_CFG_LOCK));

	/* apply the original parents */
	iowrite32(switch_cfg[0], cga0 + CKGA_CLKOPSRC_SWITCH_CFG);
	iowrite32(switch_cfg[1], cga0 + CKGA_CLKOPSRC_SWITCH_CFG2);
	iowrite32(switch_cfg[2], cga1 + CKGA_CLKOPSRC_SWITCH_CFG);
	iowrite32(switch_cfg[3], cga1 + CKGA_CLKOPSRC_SWITCH_CFG2);

	for (i = 0; i < 18; ++i) {
		iowrite32(pll0_regs[i], cga0 +
			((i < 4) ? CKGA_PLL0HS_DIV_CFG(i) :
				CKGA_PLL0LS_DIV_CFG(i)));
		iowrite32(pll1_regs[i], cga0 + CKGA_PLL1_DIV_CFG(i));

		iowrite32(pll0_regs[i + 18], cga1 +
			((i < 4) ? CKGA_PLL0HS_DIV_CFG(i) :
				CKGA_PLL0LS_DIV_CFG(i)));
		iowrite32(pll1_regs[i + 18], cga1 + CKGA_PLL1_DIV_CFG(i));
	}
	kfree(pll0_regs);
	kfree(pll1_regs);
	kfree(switch_cfg);

	switch_cfg = NULL;
	pll0_regs = NULL;
	pll1_regs = NULL;

	stx7108_suspend_wake();
	pr_debug("[STM][PM] ClockGens A: restored\n");
	pm_wake_board_ntfy();
	return 0;

on_suspending:
	pll0_regs = kmalloc(2 * 18, GFP_ATOMIC);
	pll1_regs = kmalloc(2 * 18, GFP_ATOMIC);
	switch_cfg = kmalloc(sizeof(long) * 2 * 2, GFP_ATOMIC);

	if (!(pll0_regs && pll1_regs && switch_cfg))
		goto error;

	/* Save the original parents */
	switch_cfg[0] = ioread32(cga0 + CKGA_CLKOPSRC_SWITCH_CFG);
	switch_cfg[1] = ioread32(cga0 + CKGA_CLKOPSRC_SWITCH_CFG2);
	switch_cfg[2] = ioread32(cga1 + CKGA_CLKOPSRC_SWITCH_CFG);
	switch_cfg[3] = ioread32(cga1 + CKGA_CLKOPSRC_SWITCH_CFG2);

	/* 18 OSC register for each bank */
	for (i = 0; i < 18; ++i) {
		/* CGA 0 */
		pll0_regs[i] = (char)ioread32(cga0 + ((i < 4) ?
			CKGA_PLL0HS_DIV_CFG(i) : CKGA_PLL0LS_DIV_CFG(i)));
		pll1_regs[i] = (char)ioread32(cga0 + CKGA_PLL1_DIV_CFG(i));
		/* CGA 1 */
		pll0_regs[i + 18] = (char)ioread32(cga1 + ((i < 4) ?
			CKGA_PLL0HS_DIV_CFG(i) : CKGA_PLL0LS_DIV_CFG(i)));
		pll1_regs[i + 18] = (char)ioread32(cga1 + CKGA_PLL1_DIV_CFG(i));

	}

	/* Almost all the A0 clocks are off */
	cfg_a0_0 = 0xFFC3FCFF;
	pwr_a0 = 0x3; /* the AO.PLLs are off */

	/*
	 * WOL needs:
	 *  - gmac_clk enabled (at any frequency)
	 *  - eth_phy_clk enabled (at 25MHz)
	 *  - eth_phy_clk's parent PLL enabled (to ensure correct eth_phy_clk)
	 * Use the same PLL which is driving the phy_clk to drive gmac_clk
	 * as we don't care about the actual frequency as long as it has a
	 * clock.
	 */
	if (wkd.eth1_phy_can_wakeup) {
		unsigned long pll_id;
		/* identify the eth_phy_clk parent */
		pll_id = (clk_get_parent(ca0_eth_phy_clk) == ca0_pll1_clk) ?
			2 : 1;
		pwr_a0 &= ~pll_id;
		cfg_a0_0 &= ~(0x3 << (2 * CLKA0_ETH_PHY_ID));
		cfg_a0_0 &= ~(0x3 << (2 * CLKA0_ETH_MAC_ID));
		cfg_a0_0 |= (pll_id << (2 * CLKA0_ETH_PHY_ID));
		cfg_a0_0 |= (pll_id << (2 * CLKA0_ETH_MAC_ID));
	}

	iowrite32(cfg_a0_0, cga0 + CKGA_CLKOPSRC_SWITCH_CFG);
	iowrite32(0xF0FFFFFF, cga1 + CKGA_CLKOPSRC_SWITCH_CFG);

	if (state == PM_SUSPEND_MEM) {
		/* all the clocks off */
		iowrite32(0xF, cga0 + CKGA_CLKOPSRC_SWITCH_CFG2);
		iowrite32(0xF, cga1 + CKGA_CLKOPSRC_SWITCH_CFG2);

		/* turn-off not required cga_0 plls */
		iowrite32(pwr_a0, cga0 + CKGA_POWER_CFG);
		/* turn-off cga_1.pll_0 and cga_1.pll_1 */
		iowrite32(3, cga1 + CKGA_POWER_CFG);
	}

	pr_debug("[STM][PM] ClockGens A: saved\n");
	return 0;
error:
	kfree(pll1_regs);
	kfree(pll0_regs);
	kfree(switch_cfg);

	switch_cfg = NULL;
	pll0_regs = NULL;
	pll1_regs = NULL;

	return -ENOMEM;
}

static int stx7108_suspend_pre_enter(suspend_state_t state)
{
	return stx7108_suspend_core(state, 1);
}

static int stx7108_suspend_post_enter(suspend_state_t state)
{
	return stx7108_suspend_core(state, 0);
}

static int stx7108_evttoirq(unsigned long evt)
{
	return ((evt == 0xa00) ? ilc2irq(evt) : evt2irq(evt));
}

static struct stm_platform_suspend_t stx7108_suspend __cacheline_aligned = {
	.ops.begin = stx7108_suspend_begin,

	.evt_to_irq = stx7108_evttoirq,
	.pre_enter = stx7108_suspend_pre_enter,
	.post_enter = stx7108_suspend_post_enter,

	.stby_tbl = (unsigned long)stx7108_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7108_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx7108_mem_table_c1,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7108_mem_table_c1)
			* sizeof(long), L1_CACHE_BYTES),

};

static int __init stx7108_suspend_setup(void)
{
	struct sysconf_field *sc[2];
	int i;

	if (cpu_data->cut_major > 1) {
		stx7108_suspend.mem_tbl = (unsigned long)stx7108_mem_table_c2;
		stx7108_suspend.mem_size = DIV_ROUND_UP(
			ARRAY_SIZE(stx7108_mem_table_c2) *
			sizeof(long), L1_CACHE_BYTES);
	}

	/* ClockGen_D.Pll power up/down*/
	sc[0] = sysconf_claim(SYS_CFG_BANK1, 4, 0, 0, "PM");
	/* ClockGen_D.Pll lock status */
	sc[1] = sysconf_claim(SYS_STA_BANK1, 5, 0, 0, "PM");


	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (!sc[i])
			goto error;

	ca0_pll1_clk = clk_get(NULL, "CLKA0_PLL1");
	ca0_eth_phy_clk = clk_get(NULL, "CLKA_ETH_PHY_1");
	ca1_ref_clk = clk_get(NULL, "CLKA1_REF");
	ca1_pll1_clk = clk_get(NULL, "CLKA1_PLL1");
	ca1_ic_lp_on_clk = clk_get(NULL, "CLKA_IC_REG_LP_ON");

	if (!ca1_ref_clk || !ca1_pll1_clk || !ca1_ic_lp_on_clk ||
	    !ca0_pll1_clk || !ca0_eth_phy_clk)
		goto error;

	cga0 = ioremap(CGA0, 0x1000);
	cga1 = ioremap(CGA1, 0x1000);

	return stm_suspend_register(&stx7108_suspend);

error:
	pr_err("[STM][PM] Error to acquire the sysconf registers\n");
	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (sc[i])
			sysconf_release(sc[i]);

	return -EBUSY;
}

late_initcall(stx7108_suspend_setup);
