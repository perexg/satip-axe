/*****************************************************************************
 *
 * File name   : clock-stx5197.h
 * Description : Low Level API - Clocks identifiers
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License v2 only.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* LLA version: YYYYMMDD */
#define LLA_VERSION 20100322

enum {
    OSC_REF,	    /* PLLs reference clock */

    PLLA,           /* PLLA */
    PLLB,           /* PLLB */

    PLL_CPU,        /* Div0 */
    PLL_LMI,
    PLL_BIT,
    PLL_SYS,
    PLL_FDMA,
    PLL_DDR,
    PLL_AV,
    PLL_SPARE,
    PLL_ETH,
    PLL_ST40_ICK,
    PLL_ST40_PCK,

    /* FSs clocks */
    FSA_SPARE,
    FSA_PCM,
    FSA_SPDIF,
    FSA_DSS,
    FSB_PIX,
    FSB_FDMA_FS,
    FSB_AUX,
    FSB_USB,
};
