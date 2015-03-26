/******************************************************************************
 *
 * File name   : clock-stx5197.c
 * Description : Low Level API - 5197 specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License v2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
22/mar/10 fabrice.charpentier@st.com
	  Replaced use of clk_pll800_freq() by clk_pll800_get_rate().
04/mar/10 fabrice.charpentier@st.com
	  FSA & FSB identifiers removed. clkgen_fs_init()/clkgen_fs_recalc()
	  revisited.
30/nov/09 francesco.virlinzi@st.com
14/aug/09 fabrice.charpentier@st.com
	  Added PLLA/PLLB power down/up + clkgen_pll_set_parent() capabilities.
20/jul/09 francesco.virlinzi@st.com
	  Redesigned all the implementation
09/jul/09 fabrice.charpentier@st.com
	  Revisited for LLA & Linux compliancy.
*/

/* Includes --------------------------------------------------------------- */
#include <linux/clk.h>
#include <linux/stm/clk.h>
#include <linux/stm/stx5197.h>
#include <linux/delay.h>
#include <linux/io.h>
#include "clock-stx5197.h"
#include "clock-regs-stx5197.h"

#include "clock-oslayer.h"
#include "clock-common.h"
#include "clock-utils.h"

#define CRYSTAL  30000000

/* Private Function prototypes -------------------------------------------- */

static void sys_service_lock(int lock_enable);
static int clkgen_xtal_init(clk_t *clk_p);
static int clkgen_pll_init(clk_t *clk_p);
static int clkgen_pll_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgen_pll_set_rate(clk_t *clk_p, unsigned long rate);
static int clkgen_pll_recalc(clk_t *clk_p);
static int clkgen_pll_enable(clk_t *clk_p);
static int clkgen_pll_disable(clk_t *clk_p);
static int clkgen_pll_observe(clk_t *clk_p, unsigned long *divd);
static int clkgen_fs_set_rate(clk_t *clk_p, unsigned long rate);
static int clkgen_fs_enable(clk_t *clk_p);
static int clkgen_fs_disable(clk_t *clk_p);
static int clkgen_fs_observe(clk_t *clk_p, unsigned long *freq);
static int clkgen_fs_init(clk_t *clk_p);
static int clkgen_fs_recalc(clk_t *clk_p);

/*---------------------------------------------------------------------*/

#define FRAC(whole, half)	((whole << 1) | (half ? 1 : 0))

#define DIVIDER(depth, seq, hno, even)				\
	((hno << 25) | (even << 24) | (depth << 20) | (seq << 0))

#define COMBINE_DIVIDER(depth, seq, hno, even)			\
	.value = DIVIDER(depth, seq, hno, even),		\
	.cfg_0 = (seq & 0xffff),				\
	.cfg_1 = (seq >> 16),					\
	.cfg_2 = (depth | (even << 5) | (hno << 6))

static const struct {
	unsigned long ratio, value;
	unsigned short cfg_0;
	unsigned char cfg_1, cfg_2;
} divide_table[] = {
	{
	FRAC(2, 0), COMBINE_DIVIDER(0x01, 0x00AAA, 0x1, 0x1)}, {
	FRAC(2, 5), COMBINE_DIVIDER(0x04, 0x05AD6, 0x1, 0x0)}, {
	FRAC(3, 0), COMBINE_DIVIDER(0x01, 0x00DB6, 0x0, 0x0)}, {
	FRAC(3, 5), COMBINE_DIVIDER(0x03, 0x0366C, 0x1, 0x0)}, {
	FRAC(4, 0), COMBINE_DIVIDER(0x05, 0x0CCCC, 0x1, 0x1)}, {
	FRAC(4, 5), COMBINE_DIVIDER(0x07, 0x3399C, 0x1, 0x0)}, {
	FRAC(5, 0), COMBINE_DIVIDER(0x04, 0x0739C, 0x0, 0x0)}, {
	FRAC(5, 5), COMBINE_DIVIDER(0x00, 0x0071C, 0x1, 0x0)}, {
	FRAC(6, 0), COMBINE_DIVIDER(0x01, 0x00E38, 0x1, 0x1)}, {
	FRAC(6, 5), COMBINE_DIVIDER(0x02, 0x01C78, 0x1, 0x0)}, {
	FRAC(7, 0), COMBINE_DIVIDER(0x03, 0x03C78, 0x0, 0x0)}, {
	FRAC(7, 5), COMBINE_DIVIDER(0x04, 0x07878, 0x1, 0x0)}, {
	FRAC(8, 0), COMBINE_DIVIDER(0x05, 0x0F0F0, 0x1, 0x1)}, {
	FRAC(8, 5), COMBINE_DIVIDER(0x06, 0x1E1F0, 0x1, 0x0)}, {
	FRAC(9, 0), COMBINE_DIVIDER(0x07, 0x3E1F0, 0x0, 0x0)}, {
	FRAC(9, 5), COMBINE_DIVIDER(0x08, 0x7C1F0, 0x1, 0x0)}, {
	FRAC(10, 0), COMBINE_DIVIDER(0x09, 0xF83E0, 0x1, 0x1)}, {
	FRAC(11, 0), COMBINE_DIVIDER(0x00, 0x007E0, 0x0, 0x0)}, {
	FRAC(12, 0), COMBINE_DIVIDER(0x01, 0x00FC0, 0x1, 0x1)}, {
	FRAC(13, 0), COMBINE_DIVIDER(0x02, 0x01FC0, 0x0, 0x0)}, {
	FRAC(14, 0), COMBINE_DIVIDER(0x03, 0x03F80, 0x1, 0x1)}, {
	FRAC(15, 0), COMBINE_DIVIDER(0x04, 0x07F80, 0x0, 0x0)}, {
	FRAC(16, 0), COMBINE_DIVIDER(0x05, 0x0FF00, 0x1, 0x1)}, {
	FRAC(17, 0), COMBINE_DIVIDER(0x06, 0x1FF00, 0x0, 0x0)}, {
	FRAC(18, 0), COMBINE_DIVIDER(0x07, 0x3FE00, 0x1, 0x1)}, {
	FRAC(19, 0), COMBINE_DIVIDER(0x08, 0x7FE00, 0x0, 0x0)}, {
	FRAC(20, 0), COMBINE_DIVIDER(0x09, 0xFFC00, 0x1, 0x1)}
};

_CLK_OPS(Top,
	"Top clocks",
	clkgen_xtal_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL
);

_CLK_OPS(PLL,
	"PLL",
	clkgen_pll_init,
	clkgen_pll_set_parent,
	clkgen_pll_set_rate,
	clkgen_pll_recalc,
	clkgen_pll_enable,
	clkgen_pll_disable,
	clkgen_pll_observe,
	NULL,
	NULL
);

_CLK_OPS(FS,
	"FS",
	clkgen_fs_init,
	NULL,
	clkgen_fs_set_rate,
	clkgen_fs_recalc,
	clkgen_fs_enable,
	clkgen_fs_disable,
	clkgen_fs_observe,
	NULL,
	NULL
);

/* Clocks identifier list */

/* Physical clocks description */
clk_t clk_clocks[] = {
/*	 clkID	       Ops	 Nominalrate   Flags */
_CLK(OSC_REF, &Top, CRYSTAL, CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

/* PLLs */
_CLK_P(PLLA, &PLL, 700000000, CLK_RATE_PROPAGATES, &clk_clocks[OSC_REF]),
_CLK_P(PLLB, &PLL, 800000000, CLK_RATE_PROPAGATES, &clk_clocks[OSC_REF]),
/* PLL child */
_CLK(PLL_CPU, &PLL, 800000000, 0),
_CLK(PLL_LMI, &PLL, 200000000, 0),
_CLK(PLL_BIT, &PLL, 200000000, 0),
_CLK(PLL_SYS, &PLL, 133000000, CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK(PLL_FDMA, &PLL, 350000000, CLK_RATE_PROPAGATES),
_CLK(PLL_DDR, &PLL, 0, 0),
_CLK(PLL_AV, &PLL, 100000000, 0),
_CLK(PLL_SPARE, &PLL, 50000000, 0),
_CLK(PLL_ETH, &PLL, 100000000, 0),
_CLK(PLL_ST40_ICK, &PLL, 350000000, CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK(PLL_ST40_PCK, &PLL, 133000000, CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

/*FS A */
_CLK_P(FSA_SPARE, &FS, 36000000, 0, &clk_clocks[OSC_REF]),
_CLK_P(FSA_PCM, &FS, 72000000, 0, &clk_clocks[OSC_REF]),
_CLK_P(FSA_SPDIF, &FS, 36000000, 0, &clk_clocks[OSC_REF]),
_CLK_P(FSA_DSS, &FS, 13800000, 0, &clk_clocks[OSC_REF]),

/*FS B */
_CLK_P(FSB_PIX, &FS, 13800000, 0, &clk_clocks[OSC_REF]),
_CLK_P(FSB_FDMA_FS, &FS, 13800000, 0, &clk_clocks[OSC_REF]),
_CLK_P(FSB_AUX, &FS, 27000000, 0, &clk_clocks[OSC_REF]),
_CLK_P(FSB_USB, &FS, 48000000, 0, &clk_clocks[OSC_REF]),

};

int __init plat_clk_init(void)
{
	return clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 1);
}


static const short pll_cfg0_offset[] = {
	CLKDIV0_CONFIG0,	/* cpu	  */
	CLKDIV1_CONFIG0,	/* lmi	  */
	CLKDIV2_CONFIG0,	/* blit	 */
	CLKDIV3_CONFIG0,	/* sys	  */
	CLKDIV4_CONFIG0,	/* fdma	 */
	-1,			/* no configuration for ddr clk */
	CLKDIV6_CONFIG0,	/* av	   */
	CLKDIV7_CONFIG0,	/* spare	*/
	CLKDIV8_CONFIG0,	/* eth	  */
	CLKDIV9_CONFIG0,	/* st40_ick  */
	CLKDIV10_CONFIG0	/* st40_pck  */
};

static void sys_service_lock(int lock_enable)
{
	if (lock_enable) {
		CLK_WRITE(SYS_SERVICE_ADDR + CLK_LOCK_CFG, 0x100);
		return;
	}
	CLK_WRITE(SYS_SERVICE_ADDR + CLK_LOCK_CFG, 0xF0);
	CLK_WRITE(SYS_SERVICE_ADDR + CLK_LOCK_CFG, 0x0F);
}

static unsigned long pll_hw_evaluate(unsigned long input, unsigned long div_num)
{
	short offset;
	unsigned long config0, config1, config2;
	unsigned long seq, depth, hno, even;
	unsigned long combined, i;

	offset = pll_cfg0_offset[div_num];

	config0 = CLK_READ(SYS_SERVICE_ADDR + offset);
	config1 = CLK_READ(SYS_SERVICE_ADDR + offset + 0x4);
	config2 = CLK_READ(SYS_SERVICE_ADDR + offset + 0x8);

	seq = (config0 & 0xffff) | ((config1 & 0xf) << 16);
	depth = config2 & 0xf;
	hno = (config2 & (1 << 6)) ? 1 : 0;
	even = (config2 & (1 << 5)) ? 1 : 0;
	combined = DIVIDER(depth, seq, hno, even);

	for (i = 0; i < ARRAY_SIZE(divide_table); i++)
		if (divide_table[i].value == combined)
			return (input * 2) / divide_table[i].ratio;

	return 0;
}

static int clkgen_pll_recalc(clk_t *clk_p)
{
	if (clk_p->id < PLL_CPU || clk_p->id > PLL_ST40_PCK)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == PLL_DDR) {
		clk_p->rate = clk_p->parent->rate;
		return 0;
	}
	clk_p->rate = pll_hw_evaluate(clk_p->parent->rate, clk_p->id - PLL_CPU);
	return 0;
}

/*==========================================================
Name:		clkgen_xtal_init
description	Top Level System Lock
===========================================================*/

static int clkgen_xtal_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Top recalc function */
	if (clk_p->id == OSC_REF)
		clk_p->rate = CRYSTAL;

	return 0;
}

static unsigned long clkgen_pll_eval(unsigned long input, int id)
{
	unsigned long config0, config1, rate;
	unsigned long pll_num = id - PLLA;

	config0 = CLK_READ(SYS_SERVICE_ADDR + 8 * pll_num);
	config1 = CLK_READ(SYS_SERVICE_ADDR + 8 * pll_num + 4);

#define  CLK_PLL_CONFIG1_POFF   (1 << 13)
	if (config1 & CLK_PLL_CONFIG1_POFF)
		return 0;

	if (clk_pll800_get_rate(input, config0 & 0xff, (config0 >> 8) & 0xff,
	    config1 & 0x7, &rate) != 0)
		return 0;
	return rate;
}

/*==========================================================
Name:		clkgen_pll_init
description	PLL clocks init
===========================================================*/

static int clkgen_pll_init(clk_t *clk_p)
{
	unsigned long data;

	if (!clk_p || clk_p->id < PLLA || clk_p->id > PLL_ST40_PCK)
		return CLK_ERR_BAD_PARAMETER;

	/* 1. set the right parent */
	if (clk_p->id < PLL_CPU) {	/* PLLA and PLLB */
		clk_p->rate = clkgen_pll_eval(clk_p->parent->rate, clk_p->id);
		return 0;
	}

	data = CLK_READ(SYS_SERVICE_ADDR + PLL_SELECT_CFG) >> 1;
	clk_p->parent = &clk_clocks[PLLA +
			(data & (1 << (clk_p->id - PLL_CPU)) ? 1 : 0)];

	/* 2. evaluate the rate */
	clkgen_pll_recalc(clk_p);

	return 0;
}

/*==========================================================
Name:		clkgen_pll_set_parent
description	PLL clocks set parent
===========================================================*/

static int clkgen_pll_set_parent(clk_t *clk_p, clk_t *src_p)
{
	unsigned long val;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < PLL_CPU) || (clk_p->id > PLL_ST40_PCK))
		return CLK_ERR_BAD_PARAMETER;
	if ((src_p->id < PLLA) || (src_p->id > PLLB))
		return CLK_ERR_BAD_PARAMETER;

	if (src_p->id == PLLA) {
		val = CLK_READ(SYS_SERVICE_ADDR +
			  PLL_SELECT_CFG) & ~(1 << (clk_p->id - PLL_CPU +
						    1));
		clk_p->parent = &clk_clocks[PLLA];
	} else {
		val = CLK_READ(SYS_SERVICE_ADDR +
			  PLL_SELECT_CFG) | (1 << (clk_p->id - PLL_CPU + 1));
		clk_p->parent = &clk_clocks[PLLB];
	}

	sys_service_lock(0);
	CLK_WRITE(SYS_SERVICE_ADDR + PLL_SELECT_CFG, val);
	sys_service_lock(1);

	/* Updating the rate */
	clkgen_pll_recalc(clk_p);

	return 0;
}

static void pll_hw_set(unsigned long addr, unsigned long cfg0,
		    unsigned long cfg1, unsigned long cfg2)
{
	unsigned long flags;
	addr += SYS_SERVICE_ADDR;

	sys_service_lock(0);

/*
 * On the 5197 platform it's mandatory change the clock setting with an
 * asm code because in X1 mode all the clocks are routed on Xtal
 * and it could be dangerous a memory access
 *
 * All the code is self-contained in a single icache line
 */
	local_irq_save(flags);

	asm volatile (".balign  32   \n"
		   "mov.l    %5, @%4 \n"	/* in X1 mode */
		   "mov.l    %1, @(0,%0)\n"	/* set     */
		   "mov.l    %2, @(4,%0)\n"	/*  the    */
		   "mov.l    %3, @(8,%0)\n"	/*   ratio */
		   "mov.l    %6, @%4 \n"	/* in Prog mode */
		   "tst      %7, %7  \n"	/* wait stable signal */
		   "2:	       \n"
		   "bf/s     2b      \n"
		   " dt      %7      \n"
		   : : "r" (addr), "r" (cfg0),
		      "r" (cfg1), "r" (cfg2),	/* with enable */
		   "r" (SYS_SERVICE_ADDR + MODE_CONTROL),
		   "r" (MODE_CTRL_X1),
		   "r" (MODE_CTRL_PROG), "r"(1000000)
		   : "memory");

	local_irq_restore(flags);

	sys_service_lock(1);
}

static int clkgen_pll_set_rate(clk_t *clk_p, unsigned long rate)
{
	unsigned long i;
	short offset;

	if (clk_p->id < PLL_CPU)
		return CLK_ERR_BAD_PARAMETER;

	for (i = 0; i < ARRAY_SIZE(divide_table); i++)
		if (((clk_get_rate(clk_p->parent) * 2) /
		  divide_table[i].ratio) == rate)
			break;

	if (i == ARRAY_SIZE(divide_table))	/* not found! */
		return CLK_ERR_BAD_PARAMETER;

	offset = pll_cfg0_offset[clk_p->id - PLL_CPU];

	if (offset == -1)	/* ddr case */
		return CLK_ERR_BAD_PARAMETER;

	pll_hw_set(offset, divide_table[i].cfg_0,
		   divide_table[i].cfg_1, divide_table[i].cfg_2 | (1 << 4));

	clk_p->rate = rate;
	return 0;
}

static int clkgen_xable_pll(clk_t *clk_p, unsigned long enable)
{
	unsigned long reg_cfg0, reg_cfg1, reg_cfg2;
	short offset;

	if (!clk_p || (clk_p->id < PLLA) || (clk_p->id > PLL_ST40_PCK))
		return CLK_ERR_BAD_PARAMETER;

	/* Some clocks should never switched off */
	if (!enable && clk_p->flags & CLK_ALWAYS_ENABLED)
		return 0;

	/* PLL power down/up support for PLLA & PLLB */
	if ((clk_p->id == PLLA) || (clk_p->id == PLLB)) {
		offset = PLL_CONFIG(clk_p->id - PLLA, 1);
		if (enable)
			reg_cfg1 =
			 CLK_READ(SYS_SERVICE_ADDR + offset) & ~(1 << 13);
		else
			reg_cfg1 =
			 CLK_READ(SYS_SERVICE_ADDR + offset) | (1 << 13);
		sys_service_lock(0);
		CLK_WRITE(SYS_SERVICE_ADDR + offset, reg_cfg1);
		sys_service_lock(1);
		if (enable)
			clk_p->rate =
			 clkgen_pll_eval(clk_p->parent->rate, clk_p->id);
		else
			clk_p->rate = 0;
		return 0;
	}

	/* Other clocks */
	offset = pll_cfg0_offset[clk_p->id - PLL_CPU];

	if (offset == -1) {	/* ddr case */
		clk_p->rate = clk_p->parent->rate;
		return 0;
	}

	reg_cfg0 = CLK_READ(SYS_SERVICE_ADDR + offset);
	reg_cfg1 = CLK_READ(SYS_SERVICE_ADDR + offset + 4);
	reg_cfg2 = CLK_READ(SYS_SERVICE_ADDR + offset + 8);

	if (enable)
		reg_cfg2 |= (1 << 4);
	else
		reg_cfg2 &= ~(1 << 4);

	pll_hw_set(offset, reg_cfg0, reg_cfg1, reg_cfg2);

	clk_p->rate =
	 (enable ? pll_hw_evaluate(clk_p->parent->rate, clk_p->id - PLL_CPU)
	  : 0);
	return 0;
}

static int clkgen_pll_enable(clk_t *clk_p)
{
	return clkgen_xable_pll(clk_p, 1);
}

static int clkgen_pll_disable(clk_t *clk_p)
{
	return clkgen_xable_pll(clk_p, 0);
}

/*==========================================================
Name:		clkgen_fs_init
description	Sets the parent of the FS channel and recalculates its freq
===========================================================*/

static int clkgen_fs_init(clk_t *clk_p)
{
	if (!clk_p || clk_p->id < FSA_SPARE || clk_p->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	/* Parents are statically set */

	/* Computing rate */
	return clkgen_fs_recalc(clk_p);
}

/*==========================================================
Name:		clkgen_fs_set_rate
description	Sets the freq of the FS channels
===========================================================*/

static int clkgen_fs_set_rate(clk_t *clk_p, unsigned long rate)
{
	unsigned long md, pe, sdiv, val;
	unsigned long setup0, used_dco = 0;

	if (!clk_p || clk_p->id < FSA_SPARE || clk_p->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	if ((clk_fsyn_get_params(clk_p->parent->rate, rate, &md, &pe, &sdiv)) !=
	 0)
		return CLK_ERR_BAD_PARAMETER;

	setup0 = CLK_FS_SETUP(clk_p->id - FSA_SPARE);

	md &= 0x1f;		/* fix sign */
	sdiv &= 0x7;		/* fix sign */
	pe &= 0xffff;		/* fix sign */

	val = md | (sdiv << 6);	/* set [md, sdiv] */
	val |= FS_SEL_OUT | FS_OUT_ENABLED;

	sys_service_lock(0);

	if (clk_p->id == FSA_PCM || clk_p->id == FSA_SPDIF ||
		clk_p->id == FSB_PIX) {
		CLK_WRITE(SYS_SERVICE_ADDR + DCO_MODE_CFG, 0);
		used_dco++;
	}

	CLK_WRITE(SYS_SERVICE_ADDR + setup0 + 4, pe);
	CLK_WRITE(SYS_SERVICE_ADDR + setup0, val);
	CLK_WRITE(SYS_SERVICE_ADDR + setup0, val | FS_PROG_EN);

	if (used_dco) {
		CLK_WRITE(SYS_SERVICE_ADDR + DCO_MODE_CFG, 1 | FS_PROG_EN);
		CLK_WRITE(SYS_SERVICE_ADDR + DCO_MODE_CFG, 0);
	}

	CLK_WRITE(SYS_SERVICE_ADDR + setup0, val);
	sys_service_lock(1);

	clk_p->rate = rate;
	return 0;
}

/*==========================================================
Name:		clkgen_fs_clk_enable
description	enables the FS channels
===========================================================*/

static int clkgen_fs_enable(clk_t *clk_p)
{
	unsigned long fs_setup, fs_value;
	unsigned long setup0, setup0_value;

	if (!clk_p || clk_p->id < FSA_SPARE || clk_p->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	fs_setup = (clk_p->id < FSB_PIX ? FSA_SETUP : FSB_SETUP);
	fs_value = CLK_READ(SYS_SERVICE_ADDR + fs_setup);

	/* not reset */
	fs_value |= FS_NOT_RESET;

	/* enable analog part in fbX_setup */
	fs_value &= ~FS_ANALOG_POFF;

	/* enable i-th digital part in fbX_setup */
	fs_value |= FS_DIGITAL_PON((clk_p->id - FSA_SPARE) % 4);

	setup0 = CLK_FS_SETUP(clk_p->id - FSA_SPARE);
	setup0_value = CLK_READ(SYS_SERVICE_ADDR + setup0);
	setup0_value |= FS_SEL_OUT | FS_OUT_ENABLED;

	sys_service_lock(0);
	CLK_WRITE(SYS_SERVICE_ADDR + fs_setup, fs_value);
	CLK_WRITE(SYS_SERVICE_ADDR + setup0, setup0_value);
	sys_service_lock(1);

	return clkgen_fs_recalc(clk_p);
}

/*==========================================================
Name:		clkgen_fs_clk_disable
description	disables the individual channels of the FSA
===========================================================*/

static int clkgen_fs_disable(clk_t *clk_p)
{
	unsigned long setup0, tmp, fs_setup;

	if (!clk_p || clk_p->id < FSA_SPARE || clk_p->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	setup0 = CLK_FS_SETUP(clk_p->id - FSA_SPARE);

	sys_service_lock(0);

	tmp = CLK_READ(SYS_SERVICE_ADDR + setup0);
	/* output disabled */
	CLK_WRITE(SYS_SERVICE_ADDR + setup0, tmp & ~FS_OUT_ENABLED);

	fs_setup = (clk_p->id < FSB_PIX ? FSA_SETUP : FSB_SETUP);
	tmp = CLK_READ(SYS_SERVICE_ADDR + fs_setup);
	/* disable the i-th digital part */
	tmp &= ~FS_DIGITAL_PON((clk_p->id - FSA_SPARE) % 4);

	if ((tmp & (0xf << 8)) == (0xf << 8))
		/* disable analog and digital parts */
		CLK_WRITE(SYS_SERVICE_ADDR + fs_setup, tmp | FS_ANALOG_POFF);
	else
		/* disable only digital part */
		CLK_WRITE(SYS_SERVICE_ADDR + fs_setup, tmp);

	sys_service_lock(1);
	clk_p->rate = 0;
	return 0;
}

/*==========================================================
Name:		clkgen_fs_recalc
description	Tells the programmed freq of the FS channel
===========================================================*/

static int clkgen_fs_recalc(clk_t *clk_p)
{
	unsigned long md = 0x1f;
	unsigned long sdiv = 0x1C0;
	unsigned long pe = 0xFFFF;
	unsigned long fs_setup, setup0, val;

	if (!clk_p || clk_p->id < FSA_SPARE || clk_p->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	/* Is the FS analog part ON ? */
	fs_setup = (clk_p->id < FSB_PIX ? FSA_SETUP : FSB_SETUP);
	val = CLK_READ(SYS_SERVICE_ADDR + fs_setup);
	if (val & 1 << 3) {
		clk_p->rate = 0;
		return 0;
	}

	setup0 = CLK_FS_SETUP(clk_p->id - FSA_SPARE);
	val = CLK_READ(SYS_SERVICE_ADDR + setup0);

	/* Is this channel enabled ? */
	if ((val & 1<<11) == 0) {
		clk_p->rate = 0;
		return 0;
	}

	/* Ok, channel enabled. Let's compute its frequency */
	md &= val;		/* 0-4 bits */
	sdiv &= val;		/* 6-8 bits */
	sdiv >>= 6;
	pe &= CLK_READ(SYS_SERVICE_ADDR + setup0 + 4);
	return clk_fsyn_get_rate(clk_p->parent->rate, pe, md,
					sdiv, &clk_p->rate);
}

/*********************************************************************

Functions to observe the clk on the test point provided on the
	board-Debug Functions

**********************************************************************/

static int clkgen_fs_observe(clk_t *clk_p, unsigned long *freq)
{
	static const unsigned long obs_table[] = {
		11, 12, 13, 14, 15, 16, -1, 17 };
	unsigned long clk_out_sel = 0;

	if (!clk_p || clk_p->id < FSA_SPARE || clk_p->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	clk_out_sel = obs_table[clk_p->id - FSA_SPARE];

	sys_service_lock(0);
	if (clk_out_sel == -1)	/* o_f_synth_6 */
		clk_out_sel = 0;
	else
		clk_out_sel |= (1 << 5);
	CLK_WRITE(SYS_SERVICE_ADDR + CLK_OBS_CFG, clk_out_sel);
	sys_service_lock(1);
	return 0;

}

static int clkgen_pll_observe(clk_t *clk_p, unsigned long *divd)
{
	unsigned long clk_out_sel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < PLL_CPU || clk_p->id > PLL_ST40_PCK)
		return CLK_ERR_FEATURE_NOT_SUPPORTED;

	clk_out_sel = clk_p->id - PLL_CPU;

	sys_service_lock(0);
	CLK_WRITE(SYS_SERVICE_ADDR + CLK_OBS_CFG, clk_out_sel | (1 << 5));
	sys_service_lock(1);
	return 0;
}
