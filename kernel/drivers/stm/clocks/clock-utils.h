/*******************************************************************************
 *
 * File name   : clock-utils.h
 * Description : Utility functions not related to the Low Level API
 *
 * COPYRIGHT (C) 2010 STMicroelectronics - All Rights Reserved
 * This file is under the GPL 2 License.
 *
 ******************************************************************************/

#include <linux/clkdev.h>

static inline int clk_register_table(struct clk *clks, int num, int enable)
{
	int i;

	for (i = 0; i < num; i++) {
		struct clk *clk = &clks[i];
		int ret;
		struct clk_lookup *cl;

		/*
		 * Some devices have clockgen outputs which are unused.
		 * In this case the LLA may still have an entry in its
		 * tables for that clock, and try and register that clock,
		 * so we need some way to skip it.
		 */
		if (!clk->name)
			continue;

		ret = clk_register(clk);
		if (ret)
			return ret;

		/*
		 * We must ignore the result of clk_enables as some of
		 * the LLA enables functions claim to support an
		 * enables function, but then fail if you call it!
		 */
		if (enable) {
			ret = clk_enable(clk);
			if (ret)
				pr_warning("Failed to enable clk %s, "
					   "ignoring\n", clk->name);
		}

		cl = clkdev_alloc(clk, clk->name, NULL);
		if (!cl)
			return -ENOMEM;
		clkdev_add(cl);
	}

	return 0;
}
