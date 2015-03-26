/*****************************************************************************
 *
 * File name   : clock-stx7200.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* LLA version: YYYYMMDD */
#define LLA_VERSION 20110310

enum {
    /* Top level clocks */
    CLK_SYS,

    /* Clockgen C (Audio) */
    CLKC_REF,
    CLKC_FS0_CH1,
    CLKC_FS0_CH2,
    CLKC_FS0_CH3,
    CLKC_FS0_CH4,
    CLKC_FS1_CH1,
    CLKC_FS1_CH2,
    CLKC_FS1_CH3,
    CLKC_FS1_CH4,
};
