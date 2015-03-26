/*****************************************************************************
 *
 * File name   : clock-stx7111.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* LLA version: YYYYMMDD */
#define LLA_VERSION 20110406

enum {
    /* Top level clocks */
    CLK_SYSA,           /* SYSA_CLKIN/FE input clock */
    CLK_SYSB,           /* SYSB_CLKIN/USB oscillator ? */
    CLK_SYSALT,         /* Optional alternate input clock */

    /* Clockgen A */
    CLKA_REF,           /* Clockgen A reference clock */
    CLKA_PLL0HS,        /* PLL0 HS output */
    CLKA_PLL0LS,        /* PLL0 LS output */
    CLKA_PLL1,

    CLKA_IC_STNOC,      /* HS[0] */
    CLKA_FDMA0,
    CLKA_FDMA1,
    CLKA_NOT_USED3,     /* HS[3], NOT used */
    CLKA_ST40_ICK,		/* LS[4] */
    CLKA_IC_IF_100,
    CLKA_LX_DMU_CPU,
    CLKA_LX_AUD_CPU,
    CLKA_IC_BDISP_200,
    CLKA_IC_DISP_200,
    CLKA_IC_TS_200,
    CLKA_DISP_PIPE_200,
    CLKA_BLIT_PROC,
    CLKA_IC_DELTA_200,  /* Same clock than CLKA_BLIT_PROC */
    CLKA_ETH_PHY,
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
    CLKB_PIX_HDMI,
    CLKB_PIX_HD,
    CLKB_DISP_HD,
    CLKB_656,
    CLKB_GDP3,
    CLKB_DISP_ID,
    CLKB_PIX_SD,
    CLKB_DVP,
    CLKB_PIX_FROM_DVP, /* Possible source of CLKB_DVP */
    CLKB_PP,
    CLKB_LPC,
    CLKB_DSS,
    CLKB_DAA,
    CLKB_PIP,		/* NOT really in clockgenB. Sourced from clk_disp_sd
			 * or clk_disp_hd
			 */

    CLKB_SPARE04,       /* Spare FS0, CH4 */
    CLKB_SPARE12,       /* Spare FS1, CH2 */

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
