/*
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STx5197.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init plat_clk_alias_init(void)
{
	/* core clocks */
	clk_add_alias("cpu_clk", NULL, "PLL_ST40_ICK", NULL);
	clk_add_alias("module_clk", NULL, "PLL_ST40_PCK", NULL);
	clk_add_alias("comms_clk", NULL, "PLL_SYS", NULL);

	/* EMI clock */
	clk_add_alias("emi_clk", NULL, "PLL_SYS", NULL);

	/* fdma's clocks */
	clk_add_alias("fdma_slim_clk", NULL, "PLL_FDMA", NULL);
	clk_add_alias("fdma_hi_clk", NULL, "PLL_SYS",  NULL);
	clk_add_alias("fdma_low_clk", NULL, "PLL_SYS", NULL);
	clk_add_alias("fdma_ic_clk", NULL, "PLL_SYS", NULL);

	/* USB clocks */
	clk_add_alias("usb_48_clk", NULL, "FSB_USB", NULL);
	clk_add_alias("usb_ic_clk", NULL, "PLL_SYS", NULL);
	clk_add_alias("usb_phy_clk", NULL, "OSC_REF", NULL);

	return 0;
}
