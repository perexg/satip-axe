/*****************************************************************************
 *
 * File name   : clock-stx7108.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
11/mar/10 fabrice.charpentier@st.com
	  Added CKGA0 & A1 PLL1 (PLL800) set rate feature.
03/mar/10 fabrice.charpentier@st.com
	  CLKAx_PLL0, CLKB_FS0, CLKB_FS1, CLKC_FS0 identifiers removed.
	  CLKA_SH4L2_ICK, CLKA_SH4_ICK, & CLKA_IC_TS_DMA set as ALWAYS ENABLED.
12/feb/10 fabrice.charpentier@st.com
	  clkgenc_enable()/clkgenc_disable()/clkgenc_xable_fsyn() revisited.
15/dec/09 fabrice.charpent/francesco.virlinzi@st.com
	  Bug fix in clkgenax_recalc() for CLKA1_PLL0LS.
	  Replaced REGISTER_OPS by
	  _CLK_OPS macro. Added LLA_VERSION in 'clock-stx7108.h'.
20/nov/09 francesco.virlinzi@st.com
	  ClockGenA/B/C managed as bank
03/nov/09 fabrice.charpentier@st.com
	  Updated clkgenaX_set_rate() to allow PLL0 setup.
	  clkgenb_observe() bug fix.
06/oct/09 fabrice.charpentier@st.com
	  Linux feedbacks integration + fixes.
29/sep/09 fabrice.charpentier@st.com
	  Changes due to tests on first cut1.0 samples.
01/sep/09 fabrice.charpentier@st.com
	  Revisited on first Linux feedback.
21/aug/09 fabrice.charpentier@st.com
	  Revisited. Enhancements & fixes.
23/jun/09 fabrice.charpentier@st.com
	  Revisited. Syconf change to support 7108.
01/may/09 benjamin.colson@st.com
	  Original version
*/

/* Includes --------------------------------------------------------------- */

#include <linux/stm/stx7108.h>
#include <linux/stm/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "clock-stx7108.h"
#include "clock-regs-stx7108.h"

#include "clock-oslayer.h"
#include "clock-common.h"
#include "clock-utils.h"

static int clkgena1_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgena0_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenax_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenb_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenc_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgena1_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgena0_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenb_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenc_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenax_set_div(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_set_div(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_set_fsclock(clk_t *clk_p, unsigned long freq);
static int clkgenax_recalc(clk_t *clk_p);
static int clkgenb_recalc(clk_t *clk_p);
static int clkgenb_fsyn_recalc(clk_t *clk_p);
static int clkgenc_recalc(clk_t *clk_p);
static int clkgend_recalc(clk_t *clk_p);
static int clkgene_recalc(clk_t *clk_p);     /* Added to get infos for USB */
static int clkgenax_enable(clk_t *clk_p);
static int clkgenb_enable(clk_t *clk_p);
static int clkgenc_enable(clk_t *clk_p);
static int clkgenax_disable(clk_t *clk_p);
static int clkgenb_disable(clk_t *clk_p);
static int clkgenc_disable(clk_t *clk_p);
static unsigned long clkgena1_get_measure(clk_t *clk_p);
static unsigned long clkgena0_get_measure(clk_t *clk_p);
static int clkgentop_init(clk_t *clk_p);
static int clkgenax_init(clk_t *clk_p);
static int clkgenb_init(clk_t *clk_p);
static int clkgenc_init(clk_t *clk_p);
static int clkgend_init(clk_t *clk_p);
static int clkgene_init(clk_t *clk_p);

/* Boards top input clocks. */
#define SYS_CLKIN		30
#define SYS_CLKALT		30	/* ALT input. Assumed 30Mhz */

_CLK_OPS(clkgentop,
	"Top clocks",
	clkgentop_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,		/* No measure function */
	NULL		/* No observation point */
);
_CLK_OPS(clkgena0,
	"Clockgen A 0/left",
	clkgenax_init,
	clkgenax_set_parent,
	clkgena0_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgena0_observe,
	clkgena0_get_measure,
	"PIO6[4]"       /* Observation point */
);
_CLK_OPS(clkgena1,
	"Clockgen A 1/right",
	clkgenax_init,
	clkgenax_set_parent,
	clkgena1_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgena1_observe,
	clkgena1_get_measure,
	"PIO6[6]"       /* Observation point */
);
_CLK_OPS(clkgenb,
	"Clockgen B/video",
	clkgenb_init,
	clkgenb_set_parent,
	clkgenb_set_rate,
	clkgenb_recalc,
	clkgenb_enable,
	clkgenb_disable,
	clkgenb_observe,
	NULL,               /* No measure function */
	"PIO23[5]"          /* Observation point. There is also PIO23[4] */
);
_CLK_OPS(clkgenc,
	"Clockgen C/audio",
	clkgenc_init,
	clkgenc_set_parent,
	clkgenc_set_rate,
	clkgenc_recalc,
	clkgenc_enable,
	clkgenc_disable,
	NULL,
	NULL,		/* No measure function */
	NULL		/* No observation point */
);
_CLK_OPS(clkgend,
	"Clockgen D/DDRSS",
	clkgend_init,
	NULL,
	NULL,
	clkgend_recalc,
	NULL,
	NULL,
	NULL,
	NULL,		/* No measure function */
	NULL		/* No observation point */
);
_CLK_OPS(clkgene,
	"USB",
	clkgene_init,
	NULL,
	NULL,
	clkgene_recalc,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL	/* No observation point */
);

/* Physical clocks description */
clk_t clk_clocks[] = {
/* Top level clocks */
_CLK(CLK_SYSIN, &clkgentop, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK(CLK_SYSALT, &clkgentop, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

/* Clockgen A 0/Left */
_CLK_P(CLKA0_REF, &clkgena0, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED, &clk_clocks[CLK_SYSIN]),
_CLK_P(CLKA0_PLL0HS, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKA0_REF]),
_CLK_P(CLKA0_PLL0LS, &clkgena0, 500000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKA0_PLL0HS]),
_CLK_P(CLKA0_PLL1, &clkgena0, 450000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKA0_REF]),

_CLK(CLKA_DMU_PREPROC,    &clkgena0,    200000000,    0),
_CLK(CLKA_IC_GPU,         &clkgena0,    250000000,    0),
_CLK(CLKA_SH4L2_ICK,      &clkgena0,    500000000,    CLK_ALWAYS_ENABLED),
_CLK(CLKA_SH4_ICK,        &clkgena0,    500000000,    CLK_ALWAYS_ENABLED),
_CLK(CLKA_LX_DMU_CPU,     &clkgena0,    450000000,    0),
_CLK(CLKA_LX_AUD_CPU,     &clkgena0,    500000000,    0),
_CLK(CLKA_LX_SEC_CPU,     &clkgena0,    500000000,    0),
_CLK(CLKA_IC_CPU,         &clkgena0,    500000000,    0),
_CLK(CLKA_SYS_EMISS,      &clkgena0,    100000000,    0),
_CLK(CLKA_PCI,            &clkgena0,     33000000,    0),
_CLK(CLKA_SYS_MMC_SS,     &clkgena0,    100000000,    0),
_CLK(CLKA_CARD_MMC_SS,    &clkgena0,     50000000,    0),
_CLK(CLKA_ETH_PHY_1,      &clkgena0,     50000000,    0),
_CLK(CLKA_IC_GMAC_1,      &clkgena0,    100000000,    0),
_CLK(CLKA_IC_STNOC,       &clkgena0,    450000000,    0),
_CLK(CLKA_IC_DMU,         &clkgena0,    250000000,    0),

/* Clockgen A 1/Right */
_CLK_P(CLKA1_REF, &clkgena1, 0,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED, &clk_clocks[CLK_SYSIN]),
_CLK_P(CLKA1_PLL0HS, &clkgena1, 666666666,
	CLK_RATE_PROPAGATES,  &clk_clocks[CLKA1_REF]),
_CLK_P(CLKA1_PLL0LS, &clkgena1, 333333333,
	CLK_RATE_PROPAGATES,  &clk_clocks[CLKA1_PLL0HS]),
_CLK_P(CLKA1_PLL1, &clkgena1, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKA1_REF]),

_CLK(CLKA_SLIM_FDMA_0,    &clkgena1,   400000000,    0),
_CLK(CLKA_SLIM_FDMA_1,    &clkgena1,   400000000,    0),
_CLK(CLKA_SLIM_FDMA_2,    &clkgena1,   400000000,    0),
_CLK(CLKA_HQ_VDP_PROC,    &clkgena1,   333000000,    0),
_CLK(CLKA_IC_COMPO_DISP,  &clkgena1,   200000000,    0),
_CLK(CLKA_BLIT_PROC,	  &clkgena1,   266666666,    0),
_CLK(CLKA_SECURE,	  &clkgena1,   200000000,    0),
_CLK(CLKA_IC_TS_DMA,      &clkgena1,   200000000,    CLK_ALWAYS_ENABLED),
_CLK(CLKA_ETH_PHY_0,      &clkgena1,    50000000,    0),
_CLK(CLKA_IC_GMAC_0,      &clkgena1,   100000000,    0),
_CLK(CLKA_IC_REG_LP_OFF,  &clkgena1,   100000000,    0),
_CLK(CLKA_IC_REG_LP_ON,   &clkgena1,   100000000,    CLK_ALWAYS_ENABLED),
_CLK(CLKA_TP,             &clkgena1,   333333333,    0),
_CLK(CLKA_AUX_DISP_PIPE,  &clkgena1,   200000000,    0),
_CLK(CLKA_PRV_T1_BUS,     &clkgena1,    50000000,    0),
_CLK(CLKA_IC_BDISP,	  &clkgena1,   200000000,    0),

/* Clockgen B */
_CLK(CLKB_REF, &clkgenb, 0,
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

_CLK_P(CLKB_FS0_CH1, &clkgenb, 0,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),
_CLK_P(CLKB_FS0_CH2, &clkgenb, 36864000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),
_CLK_P(CLKB_FS0_CH3, &clkgenb, 32768000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),
_CLK_P(CLKB_FS0_CH4, &clkgenb, 0,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),
_CLK_P(CLKB_FS1_CH1, &clkgenb, 0,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),
_CLK_P(CLKB_FS1_CH2, &clkgenb, 0,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),
_CLK_P(CLKB_FS1_CH3, &clkgenb, 48000000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),
_CLK_P(CLKB_FS1_CH4, &clkgenb, 48000000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),

_CLK_P(CLKB_FS0_CHAN0, &clkgenb, 0,
	0, &clk_clocks[CLKB_FS0_CH1]),
_CLK(CLKB_HD_TO_VID_DIV,  &clkgenb, 0, CLK_RATE_PROPAGATES),
_CLK(CLKB_SD_TO_VID_DIV,  &clkgenb, 0, CLK_RATE_PROPAGATES),
_CLK_P(CLKB_DSS, &clkgenb, 36864,
			0, &clk_clocks[CLKB_FS0_CH2]),
_CLK_P(CLKB_DAA, &clkgenb, 32768,
			0, &clk_clocks[CLKB_FS0_CH3]),
_CLK_P(CLKB_CLK48, &clkgenb, 48000000,
			0, &clk_clocks[CLKB_FS1_CH3]),
_CLK_P(CLKB_CCSC, &clkgenb, 0,
			0, &clk_clocks[CLKB_FS1_CH2]),
_CLK_P(CLKB_LPC, &clkgenb, 46875,
			0, &clk_clocks[CLKB_FS1_CH4]),

_CLK(CLKB_PIX_HD,   &clkgenb, 0, 0),   /* CLK_OUT_0 */
_CLK(CLKB_DISP_HD,  &clkgenb, 0, 0),   /* CLK_OUT_1 */
_CLK(CLKB_DISP_PIP, &clkgenb, 0, 0),   /* CLK_OUT_2 */
_CLK(CLKB_GDP1,     &clkgenb, 0, 0),   /* CLK_OUT_3 */
_CLK(CLKB_GDP2,     &clkgenb, 0, 0),   /* CLK_OUT_4 */
_CLK(CLKB_DISP_ID,  &clkgenb, 0, 0),   /* CLK_OUT_5 */
_CLK(CLKB_PIX_SD,   &clkgenb, 0, 0),   /* CLK_OUT_6 */
_CLK(CLKB_656,      &clkgenb, 0, 0),   /* CLK_OUT_7 */

/* Clockgen C (AUDIO) */
_CLK(CLKC_REF, &clkgenc, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKC_FS0_CH1, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS0_CH2, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS0_CH3, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS0_CH4, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),

/* Clockgen D (DDR-subsystem) */
_CLK_P(CLKD_REF, &clkgend, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED, &clk_clocks[CLK_SYSIN]),
_CLK_P(CLKD_IC_DDRCTRL, &clkgend, 266000000,
		0, &clk_clocks[CLKD_REF]),
_CLK_P(CLKD_DDR, &clkgend, 1066000000,
		0, &clk_clocks[CLKD_IC_DDRCTRL]),

/* Clockgen E (USB) */
_CLK_P(CLKE_REF, &clkgene, 30000000,
		CLK_ALWAYS_ENABLED, &clk_clocks[CLK_SYSIN]),

};



SYSCONF(SYS_CFG_BANK1, 4, 8, 15);
SYSCONF(SYS_CFG_BANK1, 4, 16, 23);
SYSCONF(SYS_CFG_BANK1, 4, 24, 26);

SYSCONF(SYS_CFG_BANK2, 6, 16, 17);
SYSCONF(SYS_CFG_BANK2, 6, 24, 25);

SYSCONF(SYS_CFG_BANK2, 16, 20, 20);
SYSCONF(SYS_CFG_BANK2, 16, 22, 22);

SYSCONF(SYS_CFG_BANK3, 0, 0, 7);	/* stop clock: one bit for each clock */
SYSCONF(SYS_CFG_BANK3, 0, 12, 19);	/* sel clock: one bit for each clock */
SYSCONF(SYS_CFG_BANK3, 1, 0, 15);	/* div clock: two bits or each clock */

SYSCONF(SYS_CFG_BANK3, 2, 8, 13);/* [c_out_4_7][c_out_0_3][grp_sel][c_selt] */

SYSCONF(SYS_CFG_BANK4, 8, 20, 21);
SYSCONF(SYS_CFG_BANK4, 14, 5, 5);

int __init plat_clk_init(void)
{
	int ret = 0;

	SYSCONF_CLAIM(SYS_CFG_BANK1, 4, 8, 15);
	SYSCONF_CLAIM(SYS_CFG_BANK1, 4, 16, 23);
	SYSCONF_CLAIM(SYS_CFG_BANK1, 4, 24, 26);

	SYSCONF_CLAIM(SYS_CFG_BANK2, 6, 16, 17);
	SYSCONF_CLAIM(SYS_CFG_BANK2, 6, 24, 25);

	SYSCONF_CLAIM(SYS_CFG_BANK2, 16, 20, 20);
	SYSCONF_CLAIM(SYS_CFG_BANK2, 16, 22, 22);

	/* stop clock: 1 bit for each clock */
	SYSCONF_CLAIM(SYS_CFG_BANK3, 0, 0, 7);
	/* sel clock: 2 bits for each clock */
	SYSCONF_CLAIM(SYS_CFG_BANK3, 0, 12, 19);
	/* div clock: two bits or each clock */
	SYSCONF_CLAIM(SYS_CFG_BANK3, 1, 0, 15);
	/* Clock_select */
	SYSCONF_CLAIM(SYS_CFG_BANK3, 2, 8, 13);

	SYSCONF_CLAIM(SYS_CFG_BANK4, 8, 20, 21);
	SYSCONF_CLAIM(SYS_CFG_BANK4, 14, 5, 5);

	ret = clk_register_table(clk_clocks, CLKB_REF, 1);

	ret |= clk_register_table(&clk_clocks[CLKB_REF],
				  ARRAY_SIZE(clk_clocks) - CLKB_REF, 0);
	return ret;
}

/******************************************************************************
Top level clocks group
******************************************************************************/

/* ========================================================================
   Name:        clkgentop_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgentop_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Top recalc function */
	switch (clk_p->id) {
	case CLK_SYSIN:
		clk_p->rate = SYS_CLKIN * 1000000;
		break;
	case CLK_SYSALT:
		clk_p->rate = SYS_CLKALT * 1000000;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}
	clk_p->parent = NULL;

	return CLK_ERR_NONE;
}

/******************************************************************************
CLOCKGEN A (CPU+interco+comms) clocks group
******************************************************************************/

static inline int clkgenax_get_bank(int clk_id)
{
	return ((clk_id >= CLKA1_REF) ? 1 : 0);
}

static inline unsigned long clkgenax_get_base_address(int clk_id)
{
	return (clkgenax_get_bank(clk_id) == 0 ?
		CKGA0_BASE_ADDRESS : CKGA1_BASE_ADDRESS);
}

/* ========================================================================
Name:        clkgenax_get_index
Description: Returns index of given clockgenA clock and source reg infos
Returns:     idx==-1 if error, else >=0
======================================================================== */

static int clkgenax_get_index(int clkid, unsigned long *srcreg, int *shift)
{
	int idx;

	/* If 'clkid' is from A1, let's shift to A0 */
	clkid = (clkgenax_get_bank(clkid) ?
		(clkid - CLKA_NOT_USED_1_00 + CLKA_NOT_USED_0_00) : clkid);

	/* Warning: This function assumes clock IDs are perfectly
	   following real implementation order. Each "hole" has therefore
	   to be filled with "CLKx_NOT_USED_y" */
	if (clkid < CLKA_NOT_USED_0_00 || clkid > CLKA_IC_DMU)
		return -1;

	idx = clkid - CLKA_NOT_USED_0_00; /* from 0 to 15 are ok */

	if (idx <= 15) {
		*srcreg = CKGA_CLKOPSRC_SWITCH_CFG;
		*shift = idx * 2;
	} else {
		*srcreg = CKGA_CLKOPSRC_SWITCH_CFG2;
		*shift = (idx - 16) * 2;
	}

	return idx;
}

/* ========================================================================
   Name:        clkgenax_set_parent
   Description: Set clock source for clockgenA when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_parent(clk_t *clk_p, clk_t *src_p)
{
	unsigned long clk_src, val;
	int idx, shift;
	unsigned long srcreg, base;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;

	/* check if they are on the same bank */
	if (clkgenax_get_bank(clk_p->id) != clkgenax_get_bank(src_p->id))
		return CLK_ERR_BAD_PARAMETER;

	switch (src_p->id) {
	case CLKA0_REF:
	case CLKA1_REF:
		clk_src = 0;
		break;
	case CLKA0_PLL0LS:
	case CLKA0_PLL0HS:
	case CLKA1_PLL0LS:
	case CLKA1_PLL0HS:
		clk_src = 1;
		break;
	case CLKA0_PLL1:
	case CLKA1_PLL1:
		clk_src = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	base = clkgenax_get_base_address(clk_p->id);
	val = CLK_READ(base + srcreg) & ~(0x3 << shift);
	val = val | (clk_src << shift);
	CLK_WRITE(base + srcreg, val);

	clk_p->parent = &clk_clocks[src_p->id];
	return clkgenax_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgenax_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_identify_parent(clk_t *clk_p)
{
	int idx;
	unsigned long src_sel;
	unsigned long srcreg;
	unsigned long base_addr, base_id;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if ((clk_p->id >= CLKA0_REF && clk_p->id <= CLKA0_PLL1) ||
	    (clk_p->id >= CLKA1_REF && clk_p->id <= CLKA1_PLL1))
		/* statically initialized */
		return 0;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Identifying source */
	base_addr = clkgenax_get_base_address(clk_p->id);
	base_id = ((clk_p->id >= CLKA1_REF) ? CLKA1_REF : CLKA0_REF);
	src_sel = (CLK_READ(base_addr + srcreg) >> shift) & 0x3;
	switch (src_sel) {
	case 0:
		clk_p->parent = &clk_clocks[base_id + 0];	/* CLKAx_REF */
		break;
	case 1:
		if (idx <= 3)
			/* CLKAx_PLL0HS */
			clk_p->parent = &clk_clocks[base_id + 1];
		else	/* CLKAx_PLL0LS */
			clk_p->parent = &clk_clocks[base_id + 2];
		break;
	case 2:
		clk_p->parent = &clk_clocks[base_id + 3]; /* CLKAx_PLL1 */
		break;
	case 3:
		clk_p->parent = NULL;
		clk_p->rate = 0;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenax_xable_pll
   Description: Enable/disable PLL
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_xable_pll(clk_t *clk_p, int enable)
{
	unsigned long val, base_addr;
	int bit, err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id != CLKA0_PLL0HS && clk_p->id != CLKA0_PLL1 &&
	    clk_p->id != CLKA1_PLL0HS && clk_p->id != CLKA1_PLL1)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKA0_PLL1 || clk_p->id == CLKA1_PLL1)
		bit = 1;
	else
		bit = 0;
	base_addr = clkgenax_get_base_address(clk_p->id);
	val = CLK_READ(base_addr + CKGA_POWER_CFG);
	if (enable)
		val &= ~(1 << bit);
	else
		val |= (1 << bit);
	CLK_WRITE(base_addr + CKGA_POWER_CFG, val);

	if (enable)
		err = clkgenax_recalc(clk_p);
	else
		clk_p->rate = 0;

	return err;
}

/* ========================================================================
   Name:        clkgenax_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_enable(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		/* Unsupported. Init must be called first. */
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power up */
	if ((clk_p->id >= CLKA0_PLL0HS && clk_p->id <= CLKA0_PLL1) ||
	    (clk_p->id >= CLKA1_PLL0HS && clk_p->id <= CLKA1_PLL1))
		return clkgenax_xable_pll(clk_p, 1);

	err = clkgenax_set_parent(clk_p, clk_p->parent);
	/* clkgenax_set_parent() is performing also a recalc() */

	return err;
}

/* ========================================================================
   Name:        clkgenax_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_disable(clk_t *clk_p)
{
	unsigned long val;
	int idx, shift;
	unsigned long srcreg;
	unsigned long base_address;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Can this clock be disabled ? */
	if (clk_p->flags & CLK_ALWAYS_ENABLED)
		return 0;

	/* PLL power down */
	if ((clk_p->id >= CLKA0_PLL0HS && clk_p->id <= CLKA0_PLL1) ||
	    (clk_p->id >= CLKA1_PLL0HS && clk_p->id <= CLKA1_PLL1))
		return clkgenax_xable_pll(clk_p, 0);

	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	base_address = clkgenax_get_base_address(clk_p->id);
	val = CLK_READ(base_address + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);     /* 3 = STOP clock */
	CLK_WRITE(base_address + srcreg, val);
	clk_p->rate = 0;

	return 0;
}

/* ========================================================================
   Name:        clkgenax_set_div
   Description: Set divider ratio for clockgenA when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_div(clk_t *clk_p, unsigned long *div_p)
{
	int idx;
	unsigned long div_cfg = 0;
	unsigned long srcreg, offset;
	int shift;
	unsigned long base_address;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing divider config */
	div_cfg = (*div_p - 1) & 0x1F;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Now according to parent, let's write divider ratio */
	if (clk_p->parent->id >= CLKA1_REF)
		offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKA1_REF);
	else
		offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKA0_REF);
	base_address = clkgenax_get_base_address(clk_p->id);
	CLK_WRITE(base_address + offset + (4 * idx), div_cfg);

	return 0;
}

/* ========================================================================
   Name:        clkgena0_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena0_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div, mdiv, ndiv, pdiv, data;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA0_PLL0HS || clk_p->id > CLKA_IC_DMU)
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	switch (clk_p->id) {
	case CLKA0_PLL0HS:
	case CLKA0_PLL0LS:
		if (clk_p->id == CLKA0_PLL0LS)
			freq = freq * 2;
		err = clk_pll1600_get_params(clk_clocks[CLKA0_REF].rate,
					     freq, &mdiv, &ndiv);
		if (err != 0)
			break;
		data = CLK_READ(CKGA0_BASE_ADDRESS + CKGA_PLL0_CFG) &
			~(0xff << 8) & ~0x7;
		data = data | ((ndiv & 0xff) << 8) | (mdiv & 0x7);
		CLK_WRITE(CKGA0_BASE_ADDRESS + CKGA_PLL0_CFG, data);
		if (clk_p->id == CLKA0_PLL0LS)
			err = clkgenax_recalc(&clk_clocks[CLKA0_PLL0HS]);
		break;
	case CLKA0_PLL1:
		err = clk_pll800_get_params(clk_clocks[CLKA0_REF].rate,
					     freq, &mdiv, &ndiv, &pdiv);
		if (err != 0)
			break;
		data = CLK_READ(CKGA0_BASE_ADDRESS + CKGA_PLL1_CFG)
			& 0xfff80000;
		data |= (pdiv<<16 | ndiv<<8 | mdiv);
		CLK_WRITE(CKGA0_BASE_ADDRESS + CKGA_PLL1_CFG, data);
		break;
	case CLKA_DMU_PREPROC ... CLKA_IC_DMU:
		div = clk_p->parent->rate / freq;
		err = clkgenax_set_div(clk_p, &div);
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	if (!err)
		err = clkgenax_recalc(clk_p);
	return err;
}

/* ========================================================================
   Name:        clkgenax_recalc
   Description: Get CKGA programmed clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_recalc(clk_t *clk_p)
{
	unsigned long data, ratio;
	unsigned long srcreg, offset, base_address;
	int shift, err, idx;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	/* Reading clock programmed value */
	base_address = clkgenax_get_base_address(clk_p->id);
	switch (clk_p->id) {
	case CLKA0_REF:  /* Clockgen A reference clock */
	case CLKA1_REF:  /* Clockgen A reference clock */
		clk_p->rate = clk_p->parent->rate;
		break;

	case CLKA0_PLL0HS:
	case CLKA1_PLL0HS:
		data = CLK_READ(base_address + CKGA_PLL0_CFG);
		err =
			clk_pll1600_get_rate(clk_p->parent->rate, data & 0x7,
					(data >> 8) & 0xff, &clk_p->rate);
		return err;
	case CLKA0_PLL0LS:
	case CLKA1_PLL0LS:
		clk_p->rate = clk_p->parent->rate / 2;
		return 0;
	case CLKA0_PLL1:
	case CLKA1_PLL1:
		data = CLK_READ(base_address + CKGA_PLL1_CFG);
		return clk_pll800_get_rate(clk_p->parent->rate, data & 0xff,
					   (data >> 8) & 0xff,
					   (data >> 16) & 0x7, &clk_p->rate);

	default:
		idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
		if (idx == -1)
			return CLK_ERR_BAD_PARAMETER;

		/* Now according to source, let's get divider ratio */
		if (clk_p->parent->id >= CLKA1_REF)
			offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKA1_REF);
		else
			offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKA0_REF);
		data =  CLK_READ(base_address + offset + (4 * idx));
		ratio = (data & 0x1F) + 1;
		clk_p->rate = clk_p->parent->rate / ratio;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgena0_observe
   Description: allows to observe a clock on a SYSACLK_OUT
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena0_observe(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long src = 0;
	unsigned long divcfg;
	/* WARNING: the obs_table[] must strictly follows clockgen
	 * enum order taking into account any "holes" (CLKA0_NOT_USED_y)
	 * filled with 0xff
	 */
	static const unsigned char obs_table_a0[] = {
		0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
	};

	if (!clk_p || !div_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA_DMU_PREPROC || clk_p->id > CLKA_IC_DMU)
		return CLK_ERR_BAD_PARAMETER;

	src = obs_table_a0[clk_p->id - CLKA_DMU_PREPROC];
	if (src == 0xff)
		return 0;

	switch (*div_p) {
	case 2:
		divcfg = 0;
		break;
	case 4:
		divcfg = 1;
		break;
	default:
		divcfg = 2;
		*div_p = 1;
		break;
	}
	CLK_WRITE((CKGA0_BASE_ADDRESS + CKGA_CLKOBS_MUX1_CFG),
		  (divcfg << 6) | src);

	/* Configuring PIO6[4] for clock output */
	/* Selecting alternate 3, sysconf6 bank2 */
	SYSCONF_WRITE(SYS_CFG_BANK2, 6, 16, 17, 3);
	/* Enabling, sysconf16 bank2 */
	SYSCONF_WRITE(SYS_CFG_BANK2, 16, 20, 20, 1);

	return 0;
}

/* ========================================================================
   Name:        clkgena0_get_measure
   Description: Use internal HW feature (when avail.) to measure clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static unsigned long clkgena0_get_measure(clk_t *clk_p)
{
	unsigned long src, data;
	unsigned long measure;
	/* WARNING: the measure_table[] must strictly follows clockgen
	 * enum order taking into account any "holes" (CLKA_NOT_USED)
	 * filled with 0xff
	 */
	static const unsigned char measure_table_a0[] = {
		0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
	};
	int i;

	if (!clk_p)
		return 0;
	if (clk_p->id < CLKA_DMU_PREPROC || clk_p->id > CLKA_IC_DMU)
		return 0;

	src = measure_table_a0[clk_p->id - CLKA_DMU_PREPROC];
	if (src == 0xff)
		return 0;
	measure = 0;

	/* Loading the MAX Count 1000 in 30MHz Oscillator Counter */
	CLK_WRITE(CKGA0_BASE_ADDRESS + CKGA_CLKOBS_MASTER_MAXCOUNT, 0x3E8);
	CLK_WRITE(CKGA0_BASE_ADDRESS + CKGA_CLKOBS_CMD, 3);

	/* Selecting clock to observe */
	CLK_WRITE(CKGA0_BASE_ADDRESS + CKGA_CLKOBS_MUX1_CFG, (1 << 7) | src);

	/* Start counting */
	CLK_WRITE(CKGA0_BASE_ADDRESS + CKGA_CLKOBS_CMD, 0);

	for (i = 0; i < 10; i++) {
		mdelay(10);

		data = CLK_READ(CKGA0_BASE_ADDRESS + CKGA_CLKOBS_STATUS);
		if (data & 1)
			break; /* IT */
	}
	if (i == 10)
		return 0;

	/* Reading value */
	data = CLK_READ(CKGA0_BASE_ADDRESS + CKGA_CLKOBS_SLAVE0_COUNT);
	measure = 30 * data * 1000;

	CLK_WRITE(CKGA0_BASE_ADDRESS + CKGA_CLKOBS_CMD, 3);

	return measure;
}

/* ========================================================================
   Name:        clkgenax_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenax_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgenax_identify_parent(clk_p);
	if (!err)
		err = clkgenax_recalc(clk_p);

	return err;
}

/******************************************************************************
CLOCKGEN A1 (A right) clocks group
******************************************************************************/

/* ========================================================================
   Name:        clkgena1_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena1_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div, mdiv, ndiv, pdiv, data;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKA1_PLL0HS) || (clk_p->id > CLKA_IC_BDISP))
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	switch (clk_p->id) {
	case CLKA1_PLL0HS:
	case CLKA1_PLL0LS:
		if (clk_p->id == CLKA1_PLL0LS)
			freq = freq * 2;
		err = clk_pll1600_get_params(clk_clocks[CLKA1_REF].rate,
					     freq, &mdiv, &ndiv);
		if (err != 0)
			break;
		data = CLK_READ(CKGA1_BASE_ADDRESS + CKGA_PLL0_CFG) &
			~(0xff << 8) & ~0x7;
		data = data | ((ndiv & 0xff) << 8) | (mdiv & 0x7);
		CLK_WRITE(CKGA1_BASE_ADDRESS + CKGA_PLL0_CFG, data);
		if (clk_p->id == CLKA1_PLL0LS)
			err = clkgenax_recalc(&clk_clocks[CLKA1_PLL0HS]);
		break;
	case CLKA1_PLL1:
		err = clk_pll800_get_params(clk_clocks[CLKA1_REF].rate,
					     freq, &mdiv, &ndiv, &pdiv);
		if (err != 0)
			break;
		data = CLK_READ(CKGA1_BASE_ADDRESS + CKGA_PLL1_CFG)
				& 0xfff80000;
		data |= (pdiv<<16 | ndiv<<8 | mdiv);
		CLK_WRITE(CKGA1_BASE_ADDRESS + CKGA_PLL1_CFG, data);
		break;
	case CLKA_SLIM_FDMA_0 ... CLKA_IC_BDISP:
		div = clk_p->parent->rate / freq;
		err = clkgenax_set_div(clk_p, &div);
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	if (!err)
		err = clkgenax_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgena1_observe
   Description: allows to observe a clock on a SYSACLK_OUT
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena1_observe(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long src = 0;
	unsigned long divcfg;
	/* WARNING: the obs_table[] must strictly follows clockgen enum order
	 * taking into account any "holes" (CLKA1_NOT_USED_y) filled
	 * with 0xff
	 */
	static const unsigned char obs_table_a1[] = {
		0x9, 0xa, 0xb, 0xc, 0xd, 0xff, 0xf, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
	};

	if (!clk_p || !div_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA_SLIM_FDMA_0 || clk_p->id > CLKA_IC_BDISP)
		return CLK_ERR_BAD_PARAMETER;

	src = obs_table_a1[clk_p->id - CLKA_SLIM_FDMA_0];
	if (src == 0xff)
		return 0;

	switch (*div_p) {
	case 2:
		divcfg = 0;
		break;
	case 4:
		divcfg = 1;
		break;
	default:
		divcfg = 2;
		*div_p = 1;
		break;
	}
	CLK_WRITE((CKGA1_BASE_ADDRESS + CKGA_CLKOBS_MUX1_CFG),
		  (divcfg << 6) | src);

	/* Configuring PIO6[6] for clock output */
	/* Selecting alternate 3, sysconf6 bank2 */
	SYSCONF_WRITE(SYS_CFG_BANK2, 6, 24, 25, 3);
	/* Enabling, sysconf16 bank2 */
	SYSCONF_WRITE(SYS_CFG_BANK2, 16, 22, 22, 1);

	return 0;
}

/* ========================================================================
   Name:        clkgena1_get_measure
   Description: Use internal HW feature (when avail.) to measure clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static unsigned long clkgena1_get_measure(clk_t *clk_p)
{
	unsigned long src, data;
	unsigned long measure;
	/* WARNING: the measure_table[] must strictly follows clockgen
	 * enum order taking into account any "holes" (CLKA_NOT_USED)
	 * filled with 0xff
	 */
	static const unsigned char measure_table_a1[] = {
		0x9, 0xa, 0xb, 0xc, 0xd, 0xff, 0x0f, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
	};
	int i;

	if (!clk_p)
		return 0;
	if (clk_p->id < CLKA_SLIM_FDMA_0 || clk_p->id > CLKA_IC_BDISP)
		return 0;

	src = measure_table_a1[clk_p->id - CLKA_SLIM_FDMA_0];
	if (src == 0xff)
		return 0;


	measure = 0;

	/* Loading the MAX Count 1000 in 30MHz Oscillator Counter */
	CLK_WRITE(CKGA1_BASE_ADDRESS + CKGA_CLKOBS_MASTER_MAXCOUNT, 0x3E8);
	CLK_WRITE(CKGA1_BASE_ADDRESS + CKGA_CLKOBS_CMD, 3);

	/* Selecting clock to observe */
	CLK_WRITE(CKGA1_BASE_ADDRESS + CKGA_CLKOBS_MUX1_CFG, (1 << 7) | src);

	/* Start counting */
	CLK_WRITE(CKGA1_BASE_ADDRESS + CKGA_CLKOBS_CMD, 0);

	for (i = 0; i < 10; i++) {
		mdelay(10);
		data = CLK_READ(CKGA1_BASE_ADDRESS + CKGA_CLKOBS_STATUS);
		if (data & 1)
			break;	/* IT */
	}
	if (i == 10)
		return 0;

	/* Reading value */
	data = CLK_READ(CKGA1_BASE_ADDRESS + CKGA_CLKOBS_SLAVE0_COUNT);
	measure = 30 * data * 1000;

	CLK_WRITE(CKGA1_BASE_ADDRESS + CKGA_CLKOBS_CMD, 3);

	return measure;
}

/******************************************************************************
CLOCKGEN B/Video
******************************************************************************/

static void clkgenb_unlock(void)
{
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_LOCK, 0xc0de);
}

static void clkgenb_lock(void)
{
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_LOCK, 0xc1a0);
}

/* ========================================================================
   Name:        clkgenb_xable_fsyn
   Description: Enable/disable FSYN
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_xable_fsyn(clk_t *clk_p, unsigned long enable)
{
	unsigned long val, clkout, ctrl, bit, ctrlval;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKB_FS0_CH1 || clk_p->id > CLKB_FS1_CH4)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKB_FS1_CH1) {
		clkout = CKGB_FS0_CLKOUT_CTRL;
		ctrl = CKGB_FS0_CTRL;
	} else {
		clkout = CKGB_FS1_CLKOUT_CTRL;
		ctrl = CKGB_FS1_CTRL;
	}

	clkgenb_unlock();

	/* Powering down/up digital part */
	val = CLK_READ(CKGB_BASE_ADDRESS + clkout);
	bit = (clk_p->id - CLKB_FS0_CH1) % 4;
	if (enable)
		val |= (1 << bit);
	else
		val &= ~(1 << bit);
	CLK_WRITE(CKGB_BASE_ADDRESS + clkout, val);

	/* Powering down/up analog part */
	ctrlval = CLK_READ(CKGB_BASE_ADDRESS + ctrl);
	if (enable)
		ctrlval |= (1 << 4);
	else {
		/* If all channels are off then power down FSx */
		if ((val & 0xf) == 0)
			ctrlval &= ~(1 << 4);
	}
	CLK_WRITE(CKGB_BASE_ADDRESS + ctrl, ctrlval);
	clkgenb_lock();

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgenb_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:        clkgenb_xable_clock
   Description: Enable/disable clock (Clockgen B)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_xable_clock(clk_t *clk_p, unsigned long enable)
{
	unsigned long val, bit;
	int err = 0;
	/* Power en/dis table for clkgen B.
	   WARNING: enum order !! */
	static const unsigned char power_table[] = {
		3, 9, 1, 0, 12, 11, 13
	};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id >= CLKB_PIX_HD && clk_p->id <= CLKB_656) {
		/* Clocks from "Video Clock Controller".
		STOP clock controlled thru SYSTEM_CONFIG 0 of bank3 */
		unsigned long tmp = SYSCONF_READ(SYS_CFG_BANK3, 0, 0, 7);
		bit = clk_p->id - CLKB_PIX_HD;
		if (enable)
			tmp &= ~(1 << bit);
		else
			tmp |= (1 << bit);
		SYSCONF_WRITE(SYS_CFG_BANK3, 0, 0, 7, tmp);
	} else if (clk_p->id >= CLKB_HD_TO_VID_DIV && clk_p->id <= CLKB_LPC) {
		/* Clocks from clockgen B master */
		bit = power_table[clk_p->id - CLKB_HD_TO_VID_DIV];
		val = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE);
		if (enable)
			val |= (1 << bit);
		else
			val &= ~(1 << bit);
		clkgenb_unlock();
		CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE, val);
		clkgenb_lock();
	} else
		return CLK_ERR_BAD_PARAMETER;

	if (enable)
		err = clkgenb_recalc(clk_p);
	else
		clk_p->rate = 0;
	return err;
}

/* ========================================================================
   Name:        clkgenb_enable
   Description: Enable clock or FSYN (clockgen B)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_enable(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id >= CLKB_FS0_CH1 && clk_p->id <= CLKB_FS1_CH4)
		err = clkgenb_xable_fsyn(clk_p, 1);
	else
		err = clkgenb_xable_clock(clk_p, 1);

	return err;
}

/* ========================================================================
   Name:        clkgenb_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_disable(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id >= CLKB_FS0_CH1 && clk_p->id <= CLKB_FS1_CH4)
		err = clkgenb_xable_fsyn(clk_p, 0);
	else
		err = clkgenb_xable_clock(clk_p, 0);

	return err;
}

/* ========================================================================
   Name:        clkgenb_set_parent
   Description: Set clock source for clockgenB when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	unsigned long set = 0;	/* Each bit set to 1 will be SETTED */
	unsigned long reset = 0;	/* Each bit set to 1 will be RESETTED */
	unsigned long reg;	/* Register address */
	unsigned long val, chan;

	if (!clk_p || !parent_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKB_REF:
		switch (parent_p->id) {
		case CLK_SYSIN:
			reset = 3;
			break;
		case CLK_SYSALT:
			reset = 2;
			set = 1;
			break;
		default:
			return CLK_ERR_BAD_PARAMETER;
		}
		reg = CKGB_BASE_ADDRESS + CKGB_CRISTAL_SEL;
		clkgenb_unlock();
		val = CLK_READ(reg);
		val = val & ~reset;
		val = val | set;
		CLK_WRITE(reg, val);
		clkgenb_lock();
		break;

	/* Clockgen B master clocks have no source muxing
	   capability. */

	/* Clocks from "Video Clock Controller".
	   Parent controlled from SYSCONF0 of bank 3,
	   SEL_CLK field: 0=HD, 1=SD */
	case CLKB_PIX_HD ... CLKB_656:
		chan = clk_p->id - CLKB_PIX_HD;
		val = SYSCONF_READ(SYS_CFG_BANK3, 0, 12, 19) & ~(1 << chan);
		switch (parent_p->id) {
		case CLKB_HD_TO_VID_DIV:
			val = val | (0 << chan);
			break;
		case CLKB_SD_TO_VID_DIV:
			val = val | (1 << chan);
			break;
		default:
			return CLK_ERR_BAD_PARAMETER;
		}
		/* Set bank3, SYSCONFIG0 [19:12]: sel_clk */
		SYSCONF_WRITE(SYS_CFG_BANK3, 0, 12, 19, val);
		break;

	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	clk_p->parent = parent_p;

	return clkgenb_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgenb_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	if (clk_p->id < CLKB_FS0_CH1 || clk_p->id > CLKB_656)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKB_FS0_CH1 ... CLKB_FS1_CH4:
		err = clkgenb_set_fsclock(clk_p, freq);
		break;

	/* Following clocks have a fixed parent */
	case CLKB_FS0_CHAN0:
	case CLKB_DSS:
	case CLKB_DAA:
	case CLKB_CLK48:
		err = clkgenb_set_rate(clk_p->parent, freq);
		break;

	default:
		/* Other clocks are assumed to be from Video Clock Controller */
		div = clk_p->parent->rate / freq  + (((freq/2) > (clk_p->parent->rate % freq))?0:1);
		err = clkgenb_set_div(clk_p, &div);
		break;
	}

	if (!err)
		err = clkgenb_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgenb_set_fsclock
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_set_fsclock(clk_t *clk_p, unsigned long freq)
{
	unsigned long md, pe, sdiv;
	int bank, channel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;
	if (clk_p->id < CLKB_FS0_CH1 || clk_p->id > CLKB_FS1_CH4)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fsyn_get_params(clk_p->parent->rate, freq, &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	bank = (clk_p->id - CLKB_FS0_CH1) / 4;
	channel = (clk_p->id - CLKB_FS0_CH1) % 4;

	clkgenb_unlock();
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_MD(bank, channel), md);
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_PE(bank, channel), pe);
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_SDIV(bank, channel), sdiv);
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_EN_PRG(bank, channel), 0x1);
	clkgenb_lock();

	return 0;
}

/* ========================================================================
   Name:        clkgenb_set_div
   Description: Set divider ratio for clockgenB when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_set_div(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long set = 0;	/* Each bit set to 1 will be SETTED */
	unsigned long reset = 0;/* Each bit set to 1 will be RESETTED */
	unsigned long val, chan, tmp;

	static const unsigned char div_table[] = {
		/* 1  2     3  4     5     6     7  8 */
		   0, 1, 0xff, 2, 0xff, 0xff, 0xff, 3 };

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKB_HD_TO_VID_DIV:
		if (*div_p == 1024)
			set = 1 << 3;
		else {
			*div_p = 1;   /* Wrong div defaulted to 1 */
			reset = 1 << 3;
		}
		val = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN);
		val = (val & ~reset) | set;
		clkgenb_unlock();
		CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN, val);
		clkgenb_lock();
		break;
	case CLKB_SD_TO_VID_DIV:
		if (*div_p == 1024)
			set = 1 << 7;
		else
			reset = 1 << 7;
		val = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN);
		val = (val & ~reset) | set;
		clkgenb_unlock();
		CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN, val);
		clkgenb_lock();

		if (CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SELECT) & 0x2) {
			/* clk_pix_sd sourced from Fsyn1 */
			switch (*div_p) {
			case 2:
				reset = 0x3 << 10;
				break;
			case 4:
				reset = 1 << 13;
				set = 1 << 12;
				break;
			case 8:
				set = 1 << 13;
				reset = 1 << 12;
				break;
			default: /* Wrong div defaulted to 1 */
			case 1:
				set = 0x3 << 12;
				*div_p = 1;
				break;
			}
		} else {   /* clk_pix_sd sourced from Fsyn0 */
			switch (*div_p) {
			case 4:
				reset = 1 << 11;
				set = 1 << 10;
				break;
			case 8:
				set = 1 << 13;
				reset = 1 << 12;
				break;
			default: /* Wrong div defaulted to 2 */
			case 2:
				reset = 0x3 << 10;
				*div_p = 2;
				break;
			}
		}
		val = CLK_READ(CKGB_BASE_ADDRESS + CKGB_DISPLAY_CFG);
		val = (val & ~reset) | set;
		clkgenb_unlock();
		CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_DISPLAY_CFG, val);
		clkgenb_lock();
		break;
	case CLKB_CCSC:
		if (*div_p == 1024)
			set = 1 << 8;
		else {
			*div_p = 1;   /* Wrong div defaulted to 1 */
			reset = 1 << 8;
		}
		val = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN);
		val = (val & ~reset) | set;
		clkgenb_unlock();
		CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN, val);
		clkgenb_lock();
		break;
	case CLKB_LPC:
		*div_p = 1024;
		break;

	/* Clocks from "Video Clock Controller". */
	case CLKB_PIX_HD ... CLKB_656:


		if (*div_p < 1 || *div_p > 8)
			return CLK_ERR_BAD_PARAMETER;

		set = div_table[*div_p - 1];
		if (set == 0xff)
			return CLK_ERR_BAD_PARAMETER;

		chan = clk_p->id - CLKB_PIX_HD;
		/* Set Bank 3, SYSCONFIG 1 [15:0]: div_mode
		 * (2bits per channel)
		 */
		tmp = SYSCONF_READ(SYS_CFG_BANK3, 1, 0, 15);
		tmp &= ~(3 << (chan * 2));
		tmp |= set << (chan * 2);
		SYSCONF_WRITE(SYS_CFG_BANK3, 1, 0, 15, tmp);
		break;

	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenb_observe
   Description: Allows to observe a clock on a PIO5_2
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_observe(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long out0, out1 = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKB_FS0_CHAN0 || clk_p->id > CLKB_656)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id >= CLKB_FS0_CHAN0 && clk_p->id <= CLKB_LPC) {
		/* ClkgenB master clocks.
		   WARNING: enum order from CLKB_FS0_CHAN0 to CLKB_LPC !! */
		static const unsigned char observe_table[] = {
			0, 3, 8, 9, 10, 12, 11, 13 };

		out0 = observe_table[clk_p->id - CLKB_FS0_CHAN0];
		if (out0 == 0xff)
			return CLK_ERR_FEATURE_NOT_SUPPORTED;

		if (clk_p->id == CLKB_CLK48)
			out1 = 11;

		clkgenb_unlock();
		CLK_WRITE((CKGB_BASE_ADDRESS + CKGB_OUT_CTRL),
				(out1 << 4) | out0);
		clkgenb_lock();

		/* Note: to have all clockgen B clocks visible on the same
		   output choosing PIO23[5] even if clockgen B master clocks
		   only can be observed on PIO23[4] */
		SYSCONF_WRITE(SYS_CFG_BANK3, 2, 8, 13, 0);
	} else if (clk_p->id >= CLKB_PIX_HD && clk_p->id <= CLKB_656) {
		/* Video Clock Controler clocks.
		   WARNING: enum order from CLKB_PIX_HD to CLKB_656 !! */
		/* Group (0=channels 0->3, 1=channels 4->7 */
		static const unsigned char observe_table2[] = {
			0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13 };
		unsigned long group, out;

		group = observe_table2[clk_p->id - CLKB_PIX_HD] >> 4;
		out = observe_table2[clk_p->id - CLKB_PIX_HD] & 0xf;

		/* Selection clock for observation, sysconf2 bank3 */
		SYSCONF_WRITE(SYS_CFG_BANK3, 2, 8, 13,
			      (out << 4) | (out << 2) | (group << 1) | 1);
	}

	/* Selecting alternate mode 2 for PIO23[5] */
	SYSCONF_WRITE(SYS_CFG_BANK4, 8, 20, 21, 2);
	SYSCONF_WRITE(SYS_CFG_BANK4, 14, 5, 5, 1);

	/* No possible predivider on clockgen B */
	*div_p = 1;

	return 0;
}

/* ========================================================================
   Name:        clkgenb_fsyn_recalc
   Description: Check FSYN & channels status... active, disabled, standby
		'clk_p->rate' is updated accordingly.
   Returns:     Error code.
   ======================================================================== */

static int clkgenb_fsyn_recalc(clk_t *clk_p)
{
	unsigned long val, bit;
	unsigned long clkout = CKGB_FS0_CLKOUT_CTRL, ctrl = CKGB_FS0_CTRL;
	unsigned long pe, md, sdiv;
	int bank, channel;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	bank = (clk_p->id - CLKB_FS0_CH1) / 4;
	channel = (clk_p->id - CLKB_FS0_CH1) % 4;

	/* Which FSYN control registers to use ? */
	if (clk_p->id >= CLKB_FS0_CH1 && clk_p->id <= CLKB_FS1_CH4) {
		clkout += (CKGB_FS1_CLKOUT_CTRL - CKGB_FS0_CLKOUT_CTRL) * bank;
		ctrl += (CKGB_FS1_CTRL - CKGB_FS0_CTRL) * bank;
	} else
		return CLK_ERR_BAD_PARAMETER;

	/* Is FSYN analog part UP ? */
	val = CLK_READ(CKGB_BASE_ADDRESS + ctrl);
	if ((val & (1 << 4)) == 0) {	/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}

	/* Is FSYN digital part UP ? */
	bit = (clk_p->id - CLKB_FS0_CH1) % 4;
	val = CLK_READ(CKGB_BASE_ADDRESS + clkout);
	if ((val & (1 << bit)) == 0) {
		/* Digital standby */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN is up and running.
	   Now computing frequency */
	pe = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_PE(bank, channel));
	md = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_MD(bank, channel));
	sdiv = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SDIV(bank, channel));
	return clk_fsyn_get_rate(clk_p->parent->rate,
				pe, md, sdiv, &clk_p->rate);
}

/* ========================================================================
   Name:        clkgenb_recalc
   Description: Get CKGB clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

/* Check clock enable value for clockgen B.
   Returns: 1=RUNNING, 0=DISABLED */
static int clkgenb_is_running(int bit)
{
	unsigned long power;

	power = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE);
	if (power & (1 << bit))
		return 1;

	return 0;
}

static int clkgenb_recalc(clk_t *clk_p)
{
	unsigned long displaycfg, powerdown, fs_sel;
	unsigned long chan, val;
	static unsigned char tab2481[] = { 2, 4, 8, 1 };
	static unsigned char tab2482[] = { 2, 4, 8, 2 };
	static unsigned char tab1248[] = { 1, 2, 4, 8 };
	/* Power en/dis table for clkgen B.
	   WARNING: enum order !! */
	static unsigned char power_table[] = { 3, 9, 1, 0, 12, 11, 13 };

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	/* Read muxes */
	displaycfg = CLK_READ(CKGB_BASE_ADDRESS + CKGB_DISPLAY_CFG);
	powerdown = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN);
	fs_sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SELECT);

	switch (clk_p->id) {
	case CLKB_REF:
	case CLKB_FS0_CHAN0:
		clk_p->rate = clk_p->parent->rate;
		break;

	case CLKB_FS0_CH1 ... CLKB_FS1_CH4:
		return clkgenb_fsyn_recalc(clk_p);

	case CLKB_HD_TO_VID_DIV:	/* pix_hd */
		if (!clkgenb_is_running
		 (power_table[clk_p->id - CLKB_HD_TO_VID_DIV]))
			clk_p->rate = 0;
		else if (powerdown & (1 << 3))
			clk_p->rate = clk_p->parent->rate / 1024;
		else
			clk_p->rate = clk_p->parent->rate;
		break;
	case CLKB_SD_TO_VID_DIV:	/* pix_sd */
		if (!clkgenb_is_running
		 (power_table[clk_p->id - CLKB_HD_TO_VID_DIV]))
			clk_p->rate = 0;
		else if (powerdown & (1 << 7))
			clk_p->rate = clk_p->parent->rate / 1024;
		else if (fs_sel & (1 << 1))	/* Source is FSYN 1 */
			clk_p->rate =
			 clk_p->parent->rate /
			 tab2481[(displaycfg >> 12) & 0x3];
		else		/* Source is FSYN 0 */
			clk_p->rate =
			 clk_p->parent->rate /
			 tab2482[(displaycfg >> 10) & 0x3];
		break;
	case CLKB_CCSC:
		if (!clkgenb_is_running
		 (power_table[clk_p->id - CLKB_HD_TO_VID_DIV]))
			clk_p->rate = 0;
		else if (powerdown & (1 << 8))
			clk_p->rate = clk_p->parent->rate / 1024;
		else
			clk_p->rate = clk_p->parent->rate;
		break;
	case CLKB_LPC:
		if (!clkgenb_is_running
		 (power_table[clk_p->id - CLKB_HD_TO_VID_DIV]))
			clk_p->rate = 0;
		else
			clk_p->rate = clk_p->parent->rate / 1024;
		break;
	case CLKB_DSS:
	case CLKB_DAA:
	case CLKB_CLK48:
		if (!clkgenb_is_running
		 (power_table[clk_p->id - CLKB_HD_TO_VID_DIV]))
			clk_p->rate = 0;
		else
			clk_p->rate = clk_p->parent->rate;
		break;

	/* Clocks from "Video Clock Controller" */
	case CLKB_PIX_HD ... CLKB_656:
		chan = clk_p->id - CLKB_PIX_HD;
		/* Is the channel stopped ? */
		val = (SYSCONF_READ(SYS_CFG_BANK3, 0, 0, 7) >> chan) & 1;
		if (val)
			clk_p->rate = 0;
		else {
			/* What is the divider ratio ? */
			val = (SYSCONF_READ(SYS_CFG_BANK3, 1, 0, 15)
					>> (chan * 2)) & 3;
			clk_p->rate = clk_p->parent->rate / tab1248[val];
		}
		break;

	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenb_identify_parent
   Description: Identify parent clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_identify_parent(clk_t *clk_p)
{
	unsigned long sel, fs_sel, val;
	unsigned long displaycfg, chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKB_REF || clk_p->id > CLKB_656)
		return CLK_ERR_BAD_PARAMETER;

	fs_sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SELECT);
	displaycfg = CLK_READ(CKGB_BASE_ADDRESS + CKGB_DISPLAY_CFG);

	switch (clk_p->id) {
	case CLKB_REF:		/* What is clockgen B ref clock ? */
		sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_CRISTAL_SEL);
		switch (sel & 0x3) {
		case 0:
			clk_p->parent = &clk_clocks[CLK_SYSIN];
			clk_p->rate = clk_p->parent->rate;
			break;
		case 1:
			clk_p->parent = &clk_clocks[CLK_SYSALT];
			clk_p->rate = clk_p->parent->rate;
			break;
		default:
			clk_p->parent = NULL;
			break;
		}
		break;

	case CLKB_HD_TO_VID_DIV:	/* pix_hd */
		if (displaycfg & (1 << 14))  /* Source = FSYN 1 */
			clk_p->parent = &clk_clocks[CLKB_FS1_CH1];
		else
			clk_p->parent = &clk_clocks[CLKB_FS0_CH1];
		break;
	case CLKB_SD_TO_VID_DIV:	/* pix_sd */
		if (fs_sel & (1 << 1))  /* Source = FSYN 1 */
			clk_p->parent = &clk_clocks[CLKB_FS1_CH1];
		else
			clk_p->parent = &clk_clocks[CLKB_FS0_CH1];
		break;

	/* Clocks from "Video Clock Controller".
	   WARNING: must follow enum order !! */
	case CLKB_PIX_HD ... CLKB_656:
		chan = clk_p->id - CLKB_PIX_HD;
		val = SYSCONF_READ(SYS_CFG_BANK3, 0, 12, 19) & (1 << chan);
		if (val == 0)
			clk_p->parent = &clk_clocks[CLKB_HD_TO_VID_DIV];
		else
			clk_p->parent = &clk_clocks[CLKB_SD_TO_VID_DIV];
		break;

	/* Other clockgen B clocks are statically initialized
	   thanks to _CLK_P() macro */
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenb_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenb_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgenb_identify_parent(clk_p);
	if (!err)
		err = clkgenb_recalc(clk_p);

	return err;
}

/******************************************************************************
CLOCKGEN C (audio)
******************************************************************************/

/* Clockgen C infos
   Channel 0 => PCM0 (tvout ss)
   Channel 1 => PCM1 (analog out)
   Channel 2 => SPDIF (tvout ss)
   Channel 3 => PCM2 (digital out)
 */

/* ========================================================================
   Name:        clkgenc_fsyn_recalc
   Description: Get CKGC FSYN clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_fsyn_recalc(clk_t *clk_p)
{
	unsigned long cfg, dig_bit;
	unsigned long pe, md, sdiv;
	int channel, err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKC_FS0_CH1 || clk_p->id > CLKC_FS0_CH4)
		return CLK_ERR_BAD_PARAMETER;

	/* Checking FSYN analog status */
	cfg = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG);
	if (!(cfg & (1 << 14))) {   /* Analog power down */
		clk_p->rate = 0;
		return 0;
	}

	/* Checking FSYN digital part */
	dig_bit = (clk_p->id - CLKC_FS0_CH1) + 10;

	if ((cfg & (1 << dig_bit)) == 0) {	/* digital part in standby */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN up & running.
	   Computing frequency */
	channel = (clk_p->id - CLKC_FS0_CH1) % 4;
	pe = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_PE(0, channel));
	md = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_MD(0, channel));
	sdiv = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_SDIV(0, channel));
	err = clk_fsyn_get_rate(clk_p->parent->rate, pe, md,
				sdiv, &clk_p->rate);

	return err;
}

/* ========================================================================
   Name:        clkgenc_recalc
   Description: Get CKGC clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKC_REF:
		clk_p->rate = clk_p->parent->rate;
		break;
	case CLKC_FS0_CH1 ... CLKC_FS0_CH4:  /* FS0 clocks */
		return clkgenc_fsyn_recalc(clk_p);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenc_set_rate
   Description: Set CKGC clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long md, pe, sdiv;
	unsigned long reg_value = 0;
	int channel;
	static const unsigned char set_rate_table[] = {
		0x06, 0x0A, 0x12, 0x22 };

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKC_FS0_CH1) || (clk_p->id > CLKC_FS0_CH4))
		return CLK_ERR_BAD_PARAMETER;

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fsyn_get_params(clk_p->parent->rate, freq, &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	reg_value = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG);
	channel = (clk_p->id - CLKC_FS0_CH1) % 4;
	reg_value |= set_rate_table[clk_p->id - CLKC_FS0_CH1];
	reg_value |=1; /*LILLE FIX*/
	/* Select FS clock only for the clock specified */
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS0_CFG, reg_value);

	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_PE(0, channel), pe);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_MD(0, channel), md);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_SDIV(0, channel), sdiv);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_EN_PRG(0, channel), 0x01);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_EN_PRG(0, channel), 0x00);

	return clkgenc_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgenc_identify_parent
   Description: Identify parent clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_identify_parent(clk_t *clk_p)
{
	unsigned long sel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id != CLKC_REF)
		/* already done statically */
		return 0;
	sel = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG) >> 23;
	switch (sel & 0x3) {
	case 0:
		clk_p->parent = &clk_clocks[CLK_SYSIN];
		break;
	case 1:
		clk_p->parent = &clk_clocks[CLK_SYSALT];
		break;
	default:
		clk_p->parent = NULL;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenc_set_parent
   Description: Set parent clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	unsigned long sel, data;

	if (!clk_p || !parent_p)
		return CLK_ERR_BAD_PARAMETER;
	/* Only CLKC_REF's parent can be changed */
	if (clk_p->id != CLKC_REF)
		return CLK_ERR_BAD_PARAMETER;

	switch (parent_p->id) {
	case CLK_SYSIN:
		sel = 0;
		break;
	case CLK_SYSALT:
		sel = 1;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}
	data = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG) & ~(0x3 << 23);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS0_CFG, data | (sel << 23));
	clk_p->parent = parent_p;
	clk_p->rate = parent_p->rate;
	return 0;
}

/* ========================================================================
   Name:        clkgenc_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKC_REF) {
		unsigned long data = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG);
		data |= (1 << 0); /* reset NOT active */
		CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS0_CFG, data);
	}
	err = clkgenc_identify_parent(clk_p);
	if (!err)
		err = clkgenc_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgenc_xable_fsyn
   Description: Enable/Disable FSYN. If all channels OFF, FSYN is powered
		down.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_xable_fsyn(clk_t *clk_p, unsigned long enable)
{
	unsigned long val;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKC_FS0_CH1 || clk_p->id > CLKC_FS0_CH4)
		return CLK_ERR_BAD_PARAMETER;

	val = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG);

	/* Powering */
	if (enable) {
		/* Powering digital parts */
		val |= (1 << (10 + (clk_p->id - CLKC_FS0_CH1)));
		/*Put FS out of reset */
		val |= 1;

		/* Set the freq source */
		val |= (1 << (2 + (clk_p->id - CLKC_FS0_CH1)));
		/* Enable FS too  */
		val |= (1 << (6 + (clk_p->id - CLKC_FS0_CH1)));

		/* Powering analog part */
		val |= (1 << 14);
	} else {
		val &= ~(1 << (10 + (clk_p->id - CLKC_FS0_CH1)));

		/* If all channels are off then power down FS0 */
		if ((val & 0x3c00) == 0)
			val &= ~(1 << 14);
	}

	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS0_CFG, val);

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgenc_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:        clkgenc_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_enable(clk_t *clk_p)
{
	return clkgenc_xable_fsyn(clk_p, 1);
}

/* ========================================================================
   Name:        clkgenc_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_disable(clk_t *clk_p)
{
	return clkgenc_xable_fsyn(clk_p, 0);
}

/******************************************************************************
CLOCKGEN D (DDR sub-systems)
******************************************************************************/

/* ========================================================================
   Name:        clkgend_recalc
   Description: Get CKGD (LMI) clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgend_recalc(clk_t *clk_p)
{
	unsigned long pdiv, ndiv, mdiv;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKD_REF)
		clk_p->rate = clk_p->parent->rate;
	else if (clk_p->id == CLKD_IC_DDRCTRL) {
		/* Accessing SYSCONF_CFG4 of BANK1 */
		pdiv = SYSCONF_READ(SYS_CFG_BANK1, 4, 24, 26);
		ndiv = SYSCONF_READ(SYS_CFG_BANK1, 4, 16, 23);
		mdiv = SYSCONF_READ(SYS_CFG_BANK1, 4, 8, 15);
		return clk_pll800_get_rate
			(clk_p->parent->rate, mdiv, ndiv, pdiv,
			 &(clk_p->rate));
	} else if (clk_p->id == CLKD_DDR)
		clk_p->rate = clk_p->parent->rate * 4;
	else
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */

	return 0;
}

/* ========================================================================
   Name:        clkgend_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgend_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Parents are static. No idenfication required */
	return clkgend_recalc(clk_p);
}

/******************************************************************************
CLOCKGEN E (USB)
******************************************************************************/

/* ========================================================================
   Name:        clkgene_recalc
   Description: Get CKGE (USB) clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id != CLKE_REF)
		return CLK_ERR_BAD_PARAMETER;

	clk_p->rate = clk_p->parent->rate;

	return 0;
}

/* ========================================================================
   Name:        clkgene_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgene_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Parents are static. No idenfication required */
	return clkgene_recalc(clk_p);
}
