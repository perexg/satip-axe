/*
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STx7108.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init plat_clk_alias_init(void)
{
	/* core clocks */
	clk_add_alias("cpu_clk", NULL, "CLKA_SH4L2_ICK", NULL);
	clk_add_alias("module_clk", NULL, "CLKA_IC_REG_LP_ON", NULL);
	clk_add_alias("comms_clk", NULL, "CLKA_IC_REG_LP_ON", NULL);

	/* EMI clock */
	clk_add_alias("emi_clk", NULL, "CLKA_SYS_EMISS", NULL);

	/* fdmas clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.0", "CLKA_SLIM_FDMA_0", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.1", "CLKA_SLIM_FDMA_1", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.2", "CLKA_SLIM_FDMA_2", NULL);
	clk_add_alias("fdma_hi_clk", NULL, "CLKA_IC_REG_LP_ON",  NULL);
	clk_add_alias("fdma_low_clk", NULL, "CLKA_IC_TS_DMA", NULL);
	clk_add_alias("fdma_ic_clk", NULL, "CLKA_IC_REG_LP_ON", NULL);

	/* SDHCI clocks */
	clk_add_alias("sdhci_clk", NULL, "CLKA_SYS_MMC_SS",  NULL);
	clk_add_alias("sdhci_card_clk", NULL, "CLKA_CARD_MMC_SS",  NULL);

	/* USB clocks */
	clk_add_alias("usb_48_clk", NULL, "CLKB_FS1_CH3", NULL);
	clk_add_alias("usb_ic_clk", NULL, "CLKA_IC_REG_LP_OFF", NULL);
	/* usb_phy_clk got from external oscillator */

	return 0;
}
