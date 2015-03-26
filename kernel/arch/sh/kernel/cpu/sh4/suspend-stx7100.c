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

#include <linux/stm/stx7100.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>


#include <asm/irq.h>
#include <asm/system.h>

#include "stm_suspend.h"
#include <linux/stm/poke_table.h>

#define SYSSTA(x)		(4 * (x) + 0xb9001000 + 0x008)
#define SYSCONF(x)		(4 * (x) + 0xb9001000 + 0x100)

#define CGA			0xb9213000	/* Clockgen A */


#define CLKA_GEN_LOCK			(0x00)
#define CLKA_PLL(x)			(0x08 + (x) * (0x24 - 0x8))
#define   CLKA_PLL0_BYPASS		(1 << 20)
#define   CLKA_PLL_ENABLE		(1 << 19)

#define CLKA_PLL_LOCK(x)		(0x10 + (x) * (0x2C - 0x10))
#define   CLKA_PLL_LOCK_LOCKED		0x01

#define CLKA_ST40			(0x14)
#define CLKA_ST40_IC			(0x18)
#define CLKA_ST40_PER			(0x1c)

#define CLKA_CLK_DIV			(0x30)
#define CLKA_CLK_EN			(0x34)
#define CLKA_CLK_EN_DEFAULT	((1 << 0) | (1 << 1) | (1 << 4) | (1 << 5))

#define CLKA_PLL1_BYPASS		(0x3c)


#define   CLKA_PLL0_SUSPEND	((5 << 16) | (5 << 8) | 	\
		(CONFIG_SH_EXTERNAL_CLOCK / 1000000))

static struct clk *comms_clk;
static unsigned long comms_clk_rate;
static void __iomem *cga;

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7100_standby_table[] __cacheline_aligned = {
POKE32(CGA + CLKA_GEN_LOCK, 0xc0de),

POKE32(CGA + CLKA_ST40_PER, 0x5),
POKE32(CGA + CLKA_ST40_IC, 0x5),
POKE32(CGA + CLKA_ST40, 0x3),

END_MARKER,

POKE32(CGA + CLKA_ST40, 0x0),
POKE32(CGA + CLKA_ST40_IC, 0x1),
POKE32(CGA + CLKA_ST40_PER, 0x0),

POKE32(CGA + CLKA_GEN_LOCK, 0x0),

END_MARKER
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx7100_mem_table[] __cacheline_aligned = {
/* Enables the DDR self refresh mode */
OR32(SYSCONF(11), (1 << 28) | (1 << 30)),
WHILE_NE32(SYSSTA(12), (1 << 28), (1 << 28)),
WHILE_NE32(SYSSTA(13), (1 << 28), (1 << 28)),

POKE32(CGA + CLKA_GEN_LOCK, 0xc0de),

/* PLL1 on External oscillator */
OR32(CGA + CLKA_PLL1_BYPASS, 2),
UPDATE32(CGA + CLKA_PLL(1), ~CLKA_PLL_ENABLE, 0x0),

/* Turn-off the LMI clocks and the ST231 clocks */
UPDATE32(CGA + CLKA_CLK_EN, ~CLKA_CLK_EN_DEFAULT, 0x0),

POKE32(CGA + CLKA_ST40_PER, 0x5),
POKE32(CGA + CLKA_ST40_IC,  0x5),
POKE32(CGA + CLKA_ST40, 0x3),

END_MARKER,

/* Restore the highest frequency cpu/bus/per ratios */
POKE32(CGA + CLKA_ST40, 0x0),
POKE32(CGA + CLKA_ST40_IC, 0x1),
POKE32(CGA + CLKA_ST40_PER, 0x0),

/* Turn-on the LMI clocks and the ST231 clocks */
OR32(CGA + CLKA_CLK_EN, CLKA_CLK_EN_DEFAULT),

/* PLL1 at the standard frequency */
OR32(CGA + CLKA_PLL(1), CLKA_PLL_ENABLE),
WHILE_NE32(CGA + CLKA_PLL_LOCK(1), CLKA_PLL_LOCK_LOCKED, CLKA_PLL_LOCK_LOCKED),
UPDATE32(CGA + CLKA_PLL1_BYPASS, ~2, 0x0),

DELAY(2),

/* Disables DDR self refresh */
UPDATE32(SYSCONF(11), ~((1 << 28) | (1 << 30)), 0x0),

WHILE_NE32(SYSSTA(12), (1 << 28), 0x0),
WHILE_NE32(SYSSTA(13), (1 << 28), 0x0),

POKE32(CGA + CLKA_GEN_LOCK, 0x0),

DELAY(2),

END_MARKER
};

static int stx7100_suspend_begin(suspend_state_t state)
{
	comms_clk_rate = clk_get_rate(comms_clk);
	comms_clk->rate = CONFIG_SH_EXTERNAL_CLOCK / 4; /* 6.75 Mhz ~ 7.5 Mhz */
	return 0;
}

static int stx7100_suspend_core(suspend_state_t state, int suspending)
{
	static unsigned long cga_pll0_cfg;
	unsigned long tmp;
#ifdef CONFIG_32BIT
	unsigned int i, tmp_addr, tmp_data;
	static int pmb_addr_14, pmb_data_14, invalidated;
#endif
	if (!suspending)
		goto on_resuming;

	cga_pll0_cfg = ioread32(cga + CLKA_PLL(0)) & 0x7ffff;

	/* CGA.PLL0 management */
	iowrite32(0xc0de, cga + CLKA_GEN_LOCK);	/* unlock */
	tmp = ioread32(cga + CLKA_PLL(0));
	iowrite32(tmp | CLKA_PLL0_BYPASS, cga + CLKA_PLL(0));

	tmp &= ~0x7ffff; /* reset pdiv, ndiv, mdiv */

	tmp |= CLKA_PLL0_SUSPEND | CLKA_PLL_ENABLE;
	iowrite32(tmp | CLKA_PLL0_BYPASS, cga + CLKA_PLL(0));

	while ((ioread32(cga + CLKA_PLL_LOCK(0)) & CLKA_PLL_LOCK_LOCKED) !=
		CLKA_PLL_LOCK_LOCKED)
		;

	iowrite32(tmp, cga + CLKA_PLL(0));
	iowrite32(0, cga + CLKA_GEN_LOCK);		/* lock */

#ifdef CONFIG_32BIT
	invalidated = 0;
	/*
	 * Invalidate all the entries [2, 14]
	 * to avoid multi hit on the PMB
	 */
	for (i = 2; i < 14; ++i) {
		tmp_addr = ctrl_inl(PMB_ADDR | (i << PMB_E_SHIFT));
		tmp_data = ctrl_inl(PMB_DATA | (i << PMB_E_SHIFT));
		if ((tmp_addr & PMB_V) || (tmp_data & PMB_V)) {
			invalidated |= (1 << i);
			ctrl_outl(tmp_addr & ~PMB_V,
					PMB_ADDR | (i << PMB_E_SHIFT));
			ctrl_outl(tmp_data & ~PMB_V,
					PMB_DATA | (i << PMB_E_SHIFT));

		}
	}
	pmb_addr_14 = ctrl_inl(PMB_ADDR | (14 << PMB_E_SHIFT));
	pmb_data_14 = ctrl_inl(PMB_DATA | (14 << PMB_E_SHIFT));

	/* Create an entry ad-hoc to simulate the P2 area */
	ctrl_outl(pmb_addr_14 & ~PMB_V, PMB_ADDR | (14 << PMB_E_SHIFT));
	ctrl_outl(pmb_data_14 & ~PMB_V, PMB_DATA | (14 << PMB_E_SHIFT));

	ctrl_outl(0xb8000000, PMB_ADDR | (14 << PMB_E_SHIFT));
	ctrl_outl(0x18000000 | PMB_V | PMB_SZ_64M | PMB_UB | PMB_WT,
			PMB_DATA | (14 << PMB_E_SHIFT));
#endif

	comms_clk->rate = comms_clk_rate;

	return 0;

on_resuming:
#ifdef CONFIG_32BIT
	tmp_addr = ctrl_inl(PMB_ADDR | (14 << PMB_E_SHIFT));
	tmp_data = ctrl_inl(PMB_DATA | (14 << PMB_E_SHIFT));
	ctrl_outl(tmp_addr & ~PMB_V, PMB_ADDR | (14 << PMB_E_SHIFT));
	ctrl_outl(tmp_data & ~PMB_V, PMB_DATA | (14 << PMB_E_SHIFT));

	/*
	 * restore the 14-th entry as it was
	 */
	ctrl_outl(pmb_addr_14, PMB_ADDR | (14 << PMB_E_SHIFT));
	ctrl_outl(pmb_data_14, PMB_DATA | (14 << PMB_E_SHIFT));
	ctrl_inl(PMB_ADDR | (14 << PMB_E_SHIFT));
	ctrl_inl(PMB_DATA | (14 << PMB_E_SHIFT));
	/*
	 * restore all the other entries
	 */
	for (i = 2; i < 14; ++i)
		if (invalidated & (1 << i)) {
			tmp_data = ctrl_inl(PMB_DATA | (i << PMB_E_SHIFT));
			ctrl_outl(tmp_data | PMB_V, PMB_DATA |
				(i << PMB_E_SHIFT));
			ctrl_inl(PMB_DATA | (i << PMB_E_SHIFT));
		}
#endif
	/* CGA.PLL0 management */
	iowrite32(0xc0de, cga + CLKA_GEN_LOCK);
	tmp = ioread32(cga + CLKA_PLL(0));
	iowrite32(tmp | CLKA_PLL0_BYPASS, cga + CLKA_PLL(0));
	tmp &= ~0x7ffff;
	tmp |= cga_pll0_cfg | CLKA_PLL_ENABLE;
	iowrite32(tmp | CLKA_PLL0_BYPASS, cga + CLKA_PLL(0));

	while ((ioread32(cga + CLKA_PLL_LOCK(0)) & CLKA_PLL_LOCK_LOCKED) !=
		CLKA_PLL_LOCK_LOCKED)
		;

	iowrite32(tmp, cga + CLKA_PLL(0));
	iowrite32(0, cga + CLKA_GEN_LOCK);
	return 0;
}

static int stx7100_suspend_pre_enter(suspend_state_t state)
{
	return stx7100_suspend_core(state, 1);;
}

static int stx7100_suspend_post_enter(suspend_state_t state)
{
	return stx7100_suspend_core(state, 0);
}

static int stx7100_evttoirq(unsigned long evt)
{
	return evt2irq(evt);
}

static struct stm_platform_suspend_t stx7100_suspend __cacheline_aligned = {
	.ops.begin = stx7100_suspend_begin,

	.evt_to_irq = stx7100_evttoirq,
	.pre_enter = stx7100_suspend_pre_enter,
	.post_enter = stx7100_suspend_post_enter,

	.stby_tbl = (unsigned long)stx7100_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7100_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl =  (unsigned long)stx7100_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7100_mem_table) *
			sizeof(long), L1_CACHE_BYTES),
};

static int __init stx7100_suspend_setup(void)
{
	struct sysconf_field *sc[4];
	int i;

	cga = ioremap(0x19213000, 0x1000); /* used to manage CGA.PLL0 */

	sc[0] = sysconf_claim(SYS_STA, 12, 28, 28, "PM");
	sc[1] = sysconf_claim(SYS_STA, 13, 28, 28, "PM");
	sc[2] = sysconf_claim(SYS_CFG, 11, 28, 28, "PM");
	sc[3] = sysconf_claim(SYS_CFG, 11, 30, 30, "PM");

	for (i = ARRAY_SIZE(sc)-1; i; --i)
		if (!sc[i])
			goto error;

	comms_clk = clk_get(NULL, "comms_clk");

	return stm_suspend_register(&stx7100_suspend);

error:
	for (i = ARRAY_SIZE(sc)-1; i; --i)
		if (sc[i])
			sysconf_release(sc[i]);
	return 0;
}

module_init(stx7100_suspend_setup);
