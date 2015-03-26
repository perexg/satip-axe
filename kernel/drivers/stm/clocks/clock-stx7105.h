/*****************************************************************************
 *
 * File name   : clock-stx7105.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#define LLA_VERSION 20110406

enum {
	/* Top level clocks */
	CLK_SYS,           /* SYSA_CLKIN/FE input clock */
	CLK_SYSALT,         /* Optional alternate input clock */

	/* Clockgen A */
	CLKA_REF,           /* Clockgen A reference clock = OSC clock */
	CLKA_PLL0HS,        /* PLL0 HS output */
	CLKA_PLL0LS,        /* PLL0 LS output */
	CLKA_PLL1,          /* PLL1 power control & output clock */

	CLKA_IC_STNOC,      /* HS[0] */
	CLKA_FDMA0,
	CLKA_FDMA1,
	CLKA_ETH1_PHY,      /* HS[3]. Note: UNUSED on 7105 */
	CLKA_ST40_ICK,      /* LS[4] */
	CLKA_IC_IF_100,
	CLKA_LX_DMU_CPU,
	CLKA_LX_AUD_CPU,
	CLKA_IC_BDISP_200,
	CLKA_IC_DISP_200,
	CLKA_IC_TS_200,
	CLKA_DISP_PIPE_200,
	CLKA_BLIT_PROC,
	CLKA_IC_DELTA_200,  /* Same clock than CLKA_BLIT_PROC */
	CLKA_ETH0_PHY,      /* Note: Equiv to CLKA_ETH_PHY on 7105 */
	CLKA_PCI,
	CLKA_EMI_MASTER,
	CLKA_IC_COMPO_200,
	CLKA_IC_IF_200,

	/* Clockgen B */
	CLKB_REF,           /* Clockgen B reference clock */
	CLKB_FS0_CH1,
	CLKB_FS0_CH2,
	CLKB_FS0_CH3,
	CLKB_FS0_CH4,
	CLKB_FS1_CH1,
	CLKB_FS1_CH2,
	CLKB_FS1_CH3,
	CLKB_FS1_CH4,

	CLKB_TMDS_HDMI,
	CLKB_656_1,
	CLKB_PIX_HD,
	CLKB_DISP_HD,
	CLKB_656_0, /* Note: Equiv to CLKB_656 on 7105 */
	CLKB_GDP3,
	CLKB_DISP_ID,
	CLKB_PIX_SD,
	CLKB_PIX_FROM_DVP,
	CLKB_DVP,  /* Possible source of CLKB_DVP */
	CLKB_PP,
	CLKB_LPC,
	CLKB_DSS,
	CLKB_DAA,
	CLKB_PIP, /* NOT in clockgenB. Sourced from clk_disp_sd
		   * or clk_disp_hd
		   */

	CLKB_SPARE04, /* Spare FS0, CH4 */
/*
 * the 7105 has a not used channel in the FS_1_ch_2 while
 * the 7106 uses this channel as CLK_MMC
 */
	CLKB_MMC,       	/* Note: Spare FS1, CH2 on 7105 */

	/* Clockgen C (Audio) */
	CLKC_REF,
	CLKC_FS0_CH1,
	CLKC_FS0_CH2,
	CLKC_FS0_CH3,
	CLKC_FS0_CH4,

	/* Clockgen D */
	CLKD_REF,           /* Clockgen D reference clock */
	CLKD_LMI2X,

	/* Clockgen E = USB PHY */
	CLKE_REF,           /* Clockgen E reference clock */
};
