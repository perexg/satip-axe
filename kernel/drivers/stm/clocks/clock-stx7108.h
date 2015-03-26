/*****************************************************************************
 *
 * File name   : clock-stx7108.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* LLA version: YYYYMMDD */
#define LLA_VERSION 20100520

enum {
	CLK_SYSIN,
	CLK_SYSALT,

	/* Clockgen A-0 */
	CLKA0_REF,		    /* OSC clock */
	CLKA0_PLL0HS,
	CLKA0_PLL0LS,	/* LS = HS/2 */
	CLKA0_PLL1,

	CLKA_NOT_USED_0_00, /* HS[0] */
	CLKA_NOT_USED_0_01,
	CLKA_DMU_PREPROC,
	CLKA_IC_GPU,        /* HS[3] */
	CLKA_SH4L2_ICK,
	CLKA_SH4_ICK,
	CLKA_LX_DMU_CPU,
	CLKA_LX_AUD_CPU,
	CLKA_LX_SEC_CPU,
	CLKA_IC_CPU,
	CLKA_SYS_EMISS,
	CLKA_PCI,
	CLKA_SYS_MMC_SS,
	CLKA_CARD_MMC_SS,
	CLKA_ETH_PHY_1,
	CLKA_IC_GMAC_1,
	CLKA_IC_STNOC,
	CLKA_IC_DMU,

	/* Clockgen A-1 */
	CLKA1_REF,		    /* OSC clock */
	CLKA1_PLL0HS,
	CLKA1_PLL0LS,	/* LS = HS/2 */
	CLKA1_PLL1,

	CLKA_NOT_USED_1_00, /* HS[0] */
	CLKA_SLIM_FDMA_0,
	CLKA_SLIM_FDMA_1,
	CLKA_SLIM_FDMA_2,   /* HS[3] */
	CLKA_HQ_VDP_PROC,
	CLKA_IC_COMPO_DISP,
	CLKA_NOT_USED_106,
	CLKA_BLIT_PROC,
	CLKA_SECURE,
	CLKA_IC_TS_DMA,
	CLKA_ETH_PHY_0,
	CLKA_IC_GMAC_0,
	CLKA_IC_REG_LP_OFF,
	CLKA_IC_REG_LP_ON,
	CLKA_TP,
	CLKA_AUX_DISP_PIPE,
	CLKA_PRV_T1_BUS,
	CLKA_IC_BDISP,

	/* Clockgen B/Video */
	CLKB_REF,
	CLKB_FS0_CH1,
	CLKB_FS0_CH2,
	CLKB_FS0_CH3,
	CLKB_FS0_CH4,
	CLKB_FS1_CH1,
	CLKB_FS1_CH2,
	CLKB_FS1_CH3,
	CLKB_FS1_CH4,

	CLKB_FS0_CHAN0,
	CLKB_HD_TO_VID_DIV,
	CLKB_SD_TO_VID_DIV,
	CLKB_DSS,
	CLKB_DAA,
	CLKB_CLK48,
	CLKB_CCSC,
	CLKB_LPC,

	/* Clockgen B, from Video Clock Controller */
	CLKB_PIX_HD,		/* Channel 0 */
	CLKB_DISP_HD,
	CLKB_DISP_PIP,
	CLKB_GDP1,
	CLKB_GDP2,
	CLKB_DISP_ID,
	CLKB_PIX_SD,
	CLKB_656,			/* Channel 7 */

	/* Clockgen C/Audio */
	CLKC_REF,
	CLKC_FS0_CH1,
	CLKC_FS0_CH2,
	CLKC_FS0_CH3,
	CLKC_FS0_CH4,

	/* Clockgen D/DDR sub-system */
	CLKD_REF,
	CLKD_IC_DDRCTRL, /* DDR sub-system input.
			  * 240Mhz on cut1, 266Mhz after
			  */
	CLKD_DDR,        /* DDR real datarate */

	/* Clockgen E/USB */
	CLKE_REF,

};
