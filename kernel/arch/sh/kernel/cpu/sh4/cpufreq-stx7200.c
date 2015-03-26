/*
 * arch/sh/kernel/pm/cpufreq-stx7200.c
 *
 * Cpufreq driver for the STx7200 platform.
 *
 * Copyright (C) 2008 STMicroelectronics
 * Copyright (C) 2009 STMicroelectronics
 * Copyright (C) 2010 STMicroelectronics
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is under the terms of the
 * General Public License version 2 ONLY
 *
 */
#include <linux/cpufreq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/stm/clk.h>
#include <linux/io.h>

#include "cpufreq-stm.h"


static struct clk *pll0_clk;
static struct clk *sh4_ic_clk;
static struct clk *module_clk;

#define CLOCKGENA_BASE_ADDR	0xFD700000      /* Clockgen A */
#define CLKA_DIV_CFG		0x10
#define CLKA_CLKOUT_SEL		0x18
#define SH4_CLK_MASK		(0x1ff << 1)
#define SH4_CLK_MASK_C2		(0x3 << 1)
/*
 *	value: 0  1  2  3  4  5  6     7
 *	ratio: 1, 2, 3, 4, 6, 8, 1024, 1
 */
static unsigned long stx7200c1_ratios[] = {
/*	  cpu	   bus	    per */
	(0 << 1) | (1 << 4) | (3 << 7),	/* 1:1 - 1:2 - 1:4 */
	(1 << 1) | (3 << 4) | (3 << 7),	/* 1:2 - 1:4 - 1:4 */
	(3 << 1) | (5 << 4) | (5 << 7)	/* 1:4 - 1:8 - 1:8 */
};

static unsigned long stx7200c2_ratios[] = { /* ratios for Cut 2.0 */
/*        cpu   */
	(1 << 1),
	(2 << 1),
};

static unsigned long *stx7200_ratios = stx7200c2_ratios;

static int stx7200_update(unsigned int set);

static struct stm_cpufreq stx7200_cpufreq = {
	.num_frequency = ARRAY_SIZE(stx7200c2_ratios),
	.update = stx7200_update,
};

static int stx7200_update(unsigned int set)
{
	static unsigned int sh_current_set;
	unsigned long clks_address = CLKA_DIV_CFG + CLOCKGENA_BASE_ADDR;
	unsigned long clks_value = ctrl_inl(clks_address);
	unsigned long l_p_j;
	unsigned long previos_set = sh_current_set;
	unsigned long shift, flag;

	l_p_j = (cpu_data[raw_smp_processor_id()].loops_per_jiffy * HZ) / 1000;
	l_p_j >>= 3;	/* l_p_j = 125 usec (for each HZ) */

	if (set == sh_current_set)
		return 0;

	shift = ((set + sh_current_set) == 2) ? 2 : 1;

	if (set > sh_current_set)	/* down scaling... */
		l_p_j >>= shift;
	else				/* up scaling... */
		l_p_j <<= shift;

	if (cpu_data->cut_major < 2)
		clks_value &= ~SH4_CLK_MASK;
	else
		clks_value &= ~SH4_CLK_MASK_C2;

	clks_value |= stx7200_ratios[set];

	/*
	 * After changing the clock divider we need a short delay
	 * to allow the clocks to stabailize, during which there must
	 * be no external memory accesses from the CPU. So inline
	 *   raw_writel(clks_value, clks_address);
	 *   mdelay(1);
	 * in a single cache line. Should also try and prevent prefetching
	 * but so far that hasn't been seen to be a problem.
	 */
	local_irq_save(flag);
	asm volatile (".balign	32	\n"
		      "mov.l	%1, @%0	\n"
		      "tst	%2, %2	\n"
		      "1:		\n"
		      "bf/s	1b	\n"
		      " dt	%2	\n"
		 : : "r" (clks_address),
		      "r"(clks_value),
		      "r"(l_p_j)
		 : "t", "memory");

	cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER,
		"st_cpufreq_update_clocks:", "\n");
	sh_current_set = set;
	local_irq_restore(flag);

	if (sh_current_set > previos_set)
		stx7200_cpufreq.cpu_clk->rate <<= shift;
	else
		stx7200_cpufreq.cpu_clk->rate >>= shift;

	if (cpu_data->cut_major < 2) {
		sh4_ic_clk->rate = clk_get_rate(stx7200_cpufreq.cpu_clk) >> 1;
		module_clk->rate = clk_get_rate(pll0_clk) >> 3;
		if (set == 2)
			module_clk->rate >>= 1;
	}
	return 0;
}

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
static void __init stx7200_observe(void)
{
	/* route the sh4/2  clock frequency */
	iowrite32(0xc, CLOCKGENA_BASE_ADDR + CLKA_CLKOUT_SEL);
}
#else
#define stx7200_observe()
#endif

static int __init stx7200_cpufreq_init(void)
{
	if (cpu_data->cut_major < 2)
		return -ENODEV;
	stx7200_cpufreq.cpu_clk = clk_get(NULL, "st40_clk");
	if (!stx7200_cpufreq.cpu_clk)
		return -EINVAL;

	pll0_clk = clk_get(NULL, "pll0_clk");

	if (!pll0_clk) {
		pr_err("[STM][CPUfreq]: Error on clk_get(pll0_clk)\n");
		return -ENODEV;
	}

	if (cpu_data->cut_major < 2) {
		stx7200_cpufreq.num_frequency = ARRAY_SIZE(stx7200c1_ratios);
		stx7200_ratios = stx7200c1_ratios;
		sh4_ic_clk = clk_get(NULL, "st40_ic_clk");
		module_clk = clk_get(NULL, "st40_per_clk");
		if (!sh4_ic_clk) {
			pr_err("[STM][CPUfreq]: Error "
				"on clk_get(sh4_ic_clk)\n");
			return -ENODEV;
		}
		if (!module_clk) {
			pr_err("[STM][CPUfreq]: Error "
				"on clk_get(module_clk)\n");
			return -ENODEV;
		}
	}

	stx7200_observe();
	stm_cpufreq_register(&stx7200_cpufreq);
	return 0;
}

static void stx7200_cpufreq_exit(void)
{
	stm_cpufreq_remove(&stx7200_cpufreq);
}

module_init(stx7200_cpufreq_init);
module_exit(stx7200_cpufreq_exit);
