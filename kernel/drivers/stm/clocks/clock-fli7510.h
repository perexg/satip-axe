/*****************************************************************************
 *
 * File name   : clock-fli7510.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 *****************************************************************************/


enum {
	/* Top level clocks */
	CLK_SYSA,		   /* SYSA_CLKIN/FE input clock */

	/* Clockgen A */
	CLKA_REF,		/* Clockgen A reference clock */
	CLKA_PLL0HS,		/* PLL0 HS output */
	CLKA_PLL0LS,		/* PLL0 LS output */
	CLKA_PLL1,

	CLKA_ST231_AUD_1,		/* HS[0] */
	CLKA_VCPU,
	CLKA_TANGO,
	CLKA_ST231_AUD_2,

	CLKA_ST40_HOST,		/* LS[4] */
	CLKA_ST40_RT,
	CLKA_ST231_DRA2,
	CLKA_FDMA,
	CLKA_BIT,
	CLKA_AATV,
	CLKA_EMI,
	CLKA_PP,
	CLKA_ETH_PHY,
	CLKA_PCI,
	CLKA_IC_100,
	CLKA_IC_150,
	CLKA_IC_266,		/* Ls[16] */
	CLKA_IC_200,
	CLK_NOT_USED_2,

	CLKC_FS_FREE_RUN,
	CLKC_FS_DEC_1,
	CLKC_SPDIF,
	CLKC_FS_DEC_2,
};
