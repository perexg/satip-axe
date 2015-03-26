/*
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STx7200.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init plat_clk_alias_init(void)
{
	/* core clocks */
	clk_add_alias("cpu_clk", NULL, "st40_clk", NULL);
	clk_add_alias("module_clk", NULL,
		      (cpu_data->cut_major < 2) ? "st40_per_clk" : "ic_reg",
		      NULL);
	clk_add_alias("comms_clk", NULL, "ic_reg", NULL);

	/* EMI clock */
	clk_add_alias("emi_clk", NULL, "emi_master", NULL);

	/* fdmas clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.0", "fdma_clk0", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.1", "fdma_clk1", NULL);
	clk_add_alias("fdma_hi_clk", NULL, "ic_reg",  NULL);
	clk_add_alias("fdma_low_clk", NULL, "fdma_200", NULL);
	clk_add_alias("fdma_ic_clk", NULL, "ic_reg", NULL);

	/* USB clocks */
	clk_add_alias("usb_ic_clk", NULL, "ic_reg", NULL);
	/* usb_phy_clk and usb_48_clk managed internally in the wrapper */

	return 0;
}
