/*
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STx7100.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init plat_clk_alias_init(void)
{
	/* core clocks */
	clk_add_alias("cpu_clk", NULL, "st40_clk", NULL);
	clk_add_alias("module_clk", NULL, "st40_per_clk", NULL);
	clk_add_alias("comms_clk", NULL, "ic_100_clk", NULL);

	/* EMI clock */
	/* alias not required because already registered as "emi_clk" */

	/* fdmas clocks */
	clk_add_alias("fdma_slim_clk", NULL, "slim_clk", NULL);
	clk_add_alias("fdma_hi_clk", NULL, "ic_100_clk",  NULL);
	clk_add_alias("fdma_low_clk", NULL, "ic_clk", NULL);
	clk_add_alias("fdma_ic_clk", NULL, "ic_100_clk", NULL);

	/* USB clocks */
	clk_add_alias("usb_ic_clk", NULL, "ic_100_clk", NULL);
	/* usb_phy_clk generated internally to the wrapped system PLL */
	/* usb_48_clk generated internally to the wrapped system PLL */

	return 0;
}
