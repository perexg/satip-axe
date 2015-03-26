/*
 * Copyright (C) 2010 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 * Code to handle the arch clocks on the STx7141.
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

	return ret;
}
