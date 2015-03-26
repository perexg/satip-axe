/*****************************************************************************
 *
 * File name   : clock-regs-fli7510.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_H
#define __CLOCK_LLA_REGS_H


/* --- Base addresses ---------------------------------------- */
#define CKGA_BASE_ADDRESS		0xfde00000

/* --- CKGA registers --- */
#define CKGA_PLL0_CFG			0x000
#define CKGA_PLL1_CFG			0x004
#define CKGA_POWER_CFG			0x010
#define CKGA_CLKOPSRC_SWITCH_CFG	0x014
#define CKGA_OSC_ENABLE_FB		0x018
#define CKGA_PLL0_ENABLE_FB		0x01c
#define CKGA_PLL1_ENABLE_FB		0x020
#define CKGA_CLKOPSRC_SWITCH_CFG2	0x024

#define CKGA_CLKOBS_MUX1_CFG		0x030
#define CKGA_CLKOBS_MASTER_MAXCOUNT	0x034
#define CKGA_CLKOBS_CMD			0x038
#define CKGA_CLKOBS_STATUS		0x03c
#define CKGA_CLKOBS_SLAVE0_COUNT	0x040
#define CKGA_OSCMUX_DEBUG		0x044
#define CKGA_CLKOBS_MUX2_CFG		0x048
#define CKGA_LOW_POWER_CTRL		0x04C

/*
 * The CKGA_SOURCE_CFG(..) replaces the
 * - CKGA_OSC_DIV0_CFG
 * - CKGA_PLL0HS_DIV0_CFG
 * - CKGA_PLL0LS_DIV0_CFG
 * - CKGA_PLL1_DIV0_CFG
 * macros.
 * The _parent_id identifies the parent as:
 * - 0: OSC
 * - 1: PLL0_HS
 * - 2: PLL0_LS
 * - 3: PLL1
 */
#define CKGA_SOURCE_CFG(_parent_id)	(0x800 + (_parent_id) * 0x100)

/*
 * Audio clockgen ---------------------------------------------------------
 * Original Code from Pawel Moll
 */
#define CTL_EN(base)		   ((base) + 0x00)
#define EN_CLK_512FS_FREE_RUN		0
#define EN_CLK_512FS_FREE_RUN__DISABLED	(0 << EN_CLK_512FS_FREE_RUN)
#define EN_CLK_512FS_FREE_RUN__ENABLED	(1 << EN_CLK_512FS_FREE_RUN)
#define EN_CLK_256FS_FREE_RUN		1
#define EN_CLK_256FS_FREE_RUN__DISABLED	(0 << EN_CLK_256FS_FREE_RUN)
#define EN_CLK_256FS_FREE_RUN__ENABLED	(1 << EN_CLK_256FS_FREE_RUN)
#define EN_CLK_FS_FREE_RUN		2
#define EN_CLK_FS_FREE_RUN__DISABLED	(0 << EN_CLK_FS_FREE_RUN)
#define EN_CLK_FS_FREE_RUN__ENABLED	(1 << EN_CLK_FS_FREE_RUN)
#define EN_CLK_256FS_DEC_1		3
#define EN_CLK_256FS_DEC_1__DISABLED	(0 << EN_CLK_256FS_DEC_1)
#define EN_CLK_256FS_DEC_1__ENABLED	(1 << EN_CLK_256FS_DEC_1)
#define EN_CLK_FS_DEC_1			4
#define EN_CLK_FS_DEC_1__DISABLED	(0 << EN_CLK_FS_DEC_1)
#define EN_CLK_FS_DEC_1__ENABLED	(1 << EN_CLK_FS_DEC_1)
#define EN_CLK_SPDIF_RX			5
#define EN_CLK_SPDIF_RX__DISABLED	(0 << EN_CLK_SPDIF_RX)
#define EN_CLK_SPDIF_RX__ENABLED	(1 << EN_CLK_SPDIF_RX)
#define EN_CLK_256FS_DEC_2		6
#define EN_CLK_256FS_DEC_2__DISABLED	(0 << EN_CLK_256FS_DEC_2)
#define EN_CLK_256FS_DEC_2__ENABLED	(1 << EN_CLK_256FS_DEC_2)
#define EN_CLK_FS_DEC_2			7
#define EN_CLK_FS_DEC_2__DISABLED	(0 << EN_CLK_FS_DEC_2)
#define EN_CLK_FS_DEC_2__ENABLED	(1 << EN_CLK_FS_DEC_2)

#define CTL_SYNTH4X_AUD(base)			((base) + 0x04)
#define SYNTH4X_AUD_NDIV		0
#define SYNTH4X_AUD_NDIV__30_MHZ		(0 << SYNTH4X_AUD_NDIV)
#define SYNTH4X_AUD_NDIV__60_MHZ		(1 << SYNTH4X_AUD_NDIV)
#define SYNTH4X_AUD_SELCLKIN		1
#define SYNTH4X_AUD_SELCLKIN__CLKIN2V5		(0 << SYNTH4X_AUD_SELCLKIN)
#define SYNTH4X_AUD_SELCLKIN__CLKIN1V2		(1 << SYNTH4X_AUD_SELCLKIN)
#define SYNTH4X_AUD_SELBW		2
#define SYNTH4X_AUD_SELBW__VERY_GOOD_REFERENCE	(0 << SYNTH4X_AUD_SELBW)
#define SYNTH4X_AUD_SELBW__GOOD_REFERENCE	(1 << SYNTH4X_AUD_SELBW)
#define SYNTH4X_AUD_SELBW__BAD_REFERENCE	(2 << SYNTH4X_AUD_SELBW)
#define SYNTH4X_AUD_SELBW__VERY_BAD_REFERENCE  (3 << SYNTH4X_AUD_SELBW)
#define SYNTH4X_AUD_NPDA		4
#define SYNTH4X_AUD_NPDA__POWER_DOWN		(0 << SYNTH4X_AUD_NPDA)
#define SYNTH4X_AUD_NPDA__ACTIVE		(1 << SYNTH4X_AUD_NPDA)
#define SYNTH4X_AUD_NRST		5
#define SYNTH4X_AUD_NRST__MASK			(1 << SYNTH4X_AUD_NRST)
#define SYNTH4X_AUD_NRST__RESET		(0 << SYNTH4X_AUD_NRST)
#define SYNTH4X_AUD_NRST__NORMAL		(1 << SYNTH4X_AUD_NRST)

/* Warning! Registers spec defines these registers as 1 ... 4,
 * instead of 0 ... 3! */
#define CTL_SYNTH4X_AUD_N(base, n)     ((base) + 0x08 + ((n - 1) * 0x4))
#define MD				0
#define MD__MASK				(0x1f << MD)
#define MD__(value)			(((value) << MD) & MD__MASK)
#define SDIV				5
#define SDIV__MASK				(0x7 << SDIV)
#define SDIV__(value)			(((value) << SDIV) & SDIV__MASK)
#define PE				8
#define PE__MASK			(0xffff << PE)
#define PE__(value)			(((value) << PE) & PE__MASK)
#define SEL_CLK_OUT			24
#define SEL_CLK_OUT__EXTCLK			(0 << SEL_CLK_OUT)
#define SEL_CLK_OUT__FSYNTH			(1 << SEL_CLK_OUT)
#define NSB				25
#define NSB__MASK				(1 << NSB)
#define NSB__STANDBY				(0 << NSB)
#define NSB__ACTIVE				(1 << NSB)
#define NSDIV3				26
#define NSDIV3__ACTIVE				(0 << NSDIV3)
#define NSDIV3__BYPASSED			(1 << NSDIV3)

#endif  /* End __CLOCK_LLA_REGS_H */
