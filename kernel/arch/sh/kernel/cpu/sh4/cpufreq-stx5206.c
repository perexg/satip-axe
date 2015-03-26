/*
 * Cpufreq driver for the STx5206 platform
 *
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

#include "cpufreq-stm.h"


static int stx5206_update(unsigned int set);

static struct stm_cpufreq stx5206_cpufreq = {
	.num_frequency = 3,
	.update = stx5206_update,
};

static int stx5206_update(unsigned int set)
{
	static int current_set ;
	int right = 1, shift = 1;

	if (set == current_set)
		return 0;

	if ((set + current_set) == 2)
		shift = 2;

	if (set > current_set)
		right = 0;

	if (right)
		clk_set_rate(stx5206_cpufreq.cpu_clk,
			clk_get_rate(stx5206_cpufreq.cpu_clk) >> shift);
	else
		clk_set_rate(stx5206_cpufreq.cpu_clk,
			clk_get_rate(stx5206_cpufreq.cpu_clk) << shift);
	current_set = set;
	return 0;
}

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
static void __init stx5206_observe(void)
{
	unsigned long div = 2;
	clk_observe(stx5206_cpufreq.cpu_clk, &div);
}
#else
#define stx5206_observe()
#endif

static int __init stx5206_cpufreq_init(void)
{
	stx5206_cpufreq.cpu_clk = clk_get(NULL, "CLKA_SH4_ICK");
	if (!stx5206_cpufreq.cpu_clk)
		return -EINVAL;
	stm_cpufreq_register(&stx5206_cpufreq);
	stx5206_observe();
	return 0;
}

static void stx5206_cpufreq_exit(void)
{
	stm_cpufreq_remove(&stx5206_cpufreq);
}

module_init(stx5206_cpufreq_init);
module_exit(stx5206_cpufreq_exit);
