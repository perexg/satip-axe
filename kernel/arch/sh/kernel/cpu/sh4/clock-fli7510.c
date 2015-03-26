/*
 * Copyright (C) 2009 STMicroelectronics Limited
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init arch_clk_init(void)
{
	int ret;

	ret = plat_clk_init();
	if (ret)
		return ret;

	ret = plat_clk_alias_init();
	if (ret)
		return ret;

	/*
	 * Turn-on the core system clocks
	 */
	clk_enable(clk_get(NULL, "CLKA_ST40_HOST"));
	clk_enable(clk_get(NULL, "CLKA_IC_100"));
	clk_enable(clk_get(NULL, "CLKA_IC_150"));
	clk_enable(clk_get(NULL, "CLKA_IC_200"));

	return ret;
}
