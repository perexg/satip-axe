/*
 * Copyright (C) 2007, 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clockgen hardware on the STx7200.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/stm/clk.h>

#include "clock-common.h"
#include "clock-utils.h"
#include "clock-oslayer.h"

#include "clock-stx7200.h"
#include "clock-regs-stx7200.h"

/* Values for mb519 */
#define SYSACLKIN	27000000
#define SYSBCLKIN	30000000

#define CLOCKGEN_BASE_ADDR	0xfd700000	/* Clockgen A */
#define CLOCKGENB_BASE_ADDR	0xfd701000	/* Clockgen B */
#define CLOCKGENC_BASE_ADDR	0xfd601000	/* Clockgen C */

/* Alternate clock for clockgen A, B and C respectivly */
/* B & C come from SYSCLKINALT pin, SYSCLKINALT2 from PIO2[2] */
unsigned long sysclkinalt[3] = { 0, 0, 0};

#define CLOCKGEN_PLL_CFG(pll)	(CLOCKGEN_BASE_ADDR + ((pll)*0x4))
#define   CLOCKGEN_PLL_CFG_BYPASS		(1<<20)
#define CLOCKGEN_MUX_CFG	(CLOCKGEN_BASE_ADDR + 0x0c)
#define   CLOCKGEN_MUX_CFG_SYSCLK_SRC		(1<<0)
#define   CLOCKGEN_MUX_CFG_PLL_SRC(pll)		(1<<((pll)+1))
#define   CLOCKGEN_MUX_CFG_DIV_SRC(pll)		(1<<((pll)+4))
#define   CLOCKGEN_MUX_CFG_FDMA_SRC(fdma)	(1<<((fdma)+7))
#define   CLOCKGEN_MUX_CFG_IC_REG_SRC		(1<<9)
#define CLOCKGEN_DIV_CFG	(CLOCKGEN_BASE_ADDR + 0x10)
#define CLOCKGEN_DIV2_CFG	(CLOCKGEN_BASE_ADDR + 0x14)
#define CLOCKGEN_CLKOBS_MUX_CFG	(CLOCKGEN_BASE_ADDR + 0x18)
#define CLOCKGEN_POWER_CFG	(CLOCKGEN_BASE_ADDR + 0x1c)

#define CLOCKGENB_PLL0_CFG	(CLOCKGENB_BASE_ADDR + 0x3c)
#define CLOCKGENB_IN_MUX_CFG	(CLOCKGENB_BASE_ADDR + 0x44)
#define   CLOCKGENB_IN_MUX_CFG_PLL_SRC		(1<<0)
#define CLOCKGENB_DIV_CFG	(CLOCKGENB_BASE_ADDR + 0x4c)
#define CLOCKGENB_OUT_MUX_CFG	(CLOCKGENB_BASE_ADDR + 0x48)
#define   CLOCKGENB_OUT_MUX_CFG_DIV_SRC		(1<<0)
#define CLOCKGENB_DIV2_CFG	(CLOCKGENB_BASE_ADDR + 0x50)
#define CLOCKGENB_CLKOBS_MUX_CFG (CLOCKGENB_BASE_ADDR + 0x54)
#define CLOCKGENB_POWER_CFG	(CLOCKGENB_BASE_ADDR + 0x58)

				    /* 0  1  2  3  4  5  6     7  */
static const unsigned int ratio1[] = { 1, 2, 3, 4, 6, 8, 1024, 1 };
static const unsigned int ratio2[] = { 0, 1, 2, 1024, 3, 3, 3, 3 };

static unsigned long final_divider(unsigned long input, int div_ratio, int div)
{
	switch (div_ratio) {
	case 1:
		return input / 1024;
	case 2:
	case 3:
		return input / div;
	}

	return 0;
}

static unsigned long pll02_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, pdiv, mdiv;

	mdiv = (cfg >>  0) & 0xff;
	ndiv = (cfg >>  8) & 0xff;
	pdiv = (cfg >> 16) & 0x7;
	freq = (((2 * (input / 1000) * ndiv) / mdiv) /
		(1 << pdiv)) * 1000;

	return freq;
}

static unsigned long pll1_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, mdiv;

	mdiv = (cfg >>  0) & 0x7;
	ndiv = (cfg >>  8) & 0xff;
	freq = (((input / 1000) * ndiv) / mdiv) * 1000;

	return freq;
}

/* Note this returns the PLL frequency _after_ the bypass logic. */
static unsigned long pll_freq(int pll_num)
{
	unsigned long sysabclkin, input, output;
	unsigned long mux_cfg, pll_cfg;

	mux_cfg = readl(CLOCKGEN_MUX_CFG);
	if ((mux_cfg & CLOCKGEN_MUX_CFG_SYSCLK_SRC) == 0)
		sysabclkin = SYSACLKIN;
	else
		sysabclkin = SYSBCLKIN;

	if (mux_cfg & CLOCKGEN_MUX_CFG_PLL_SRC(pll_num))
		input = sysclkinalt[0];
	else
		input = sysabclkin;


	pll_cfg = readl(CLOCKGEN_PLL_CFG(pll_num));
	if (pll_num == 1)
		output = pll1_freq(input, pll_cfg);
	else
		output = pll02_freq(input, pll_cfg);

	if ((pll_cfg & CLOCKGEN_PLL_CFG_BYPASS) == 0)
		return output;
	else if ((mux_cfg & CLOCKGEN_MUX_CFG_DIV_SRC(pll_num)) == 0)
		return input;
	else
		return sysabclkin;

}

static int pll_clk_init(struct clk *clk)
{
	clk->rate = pll_freq((int)clk->private_data);
	return 0;
}

static struct clk_ops pll_clk_ops = {
	.init		= pll_clk_init,
};

#define CLK_PLL(_name, _id)					\
{	.name = _name,						\
	.ops    = &pll_clk_ops,					\
	.private_data = (void *)(_id),				\
}

static struct clk pllclks[3] = {
	CLK_PLL("pll0_clk", 0),
	CLK_PLL("pll1_clk", 1),
	CLK_PLL("pll2_clk", 2),
};

/* Note we ignore the possibility that we are in SH4 mode.
 * Should check DIV_CFG.sh4_clk_ctl and switch to FRQCR mode. */
static int sh4_clk_recalc(struct clk *clk)
{
	unsigned long shift = (unsigned long)clk->private_data;
	unsigned long div_cfg = readl(CLOCKGEN_DIV_CFG);
	unsigned long div1 = 1, div2;

	switch ((div_cfg >> 20) & 3) {
	case 0:
		return 0;
	case 1:
		div1 = 1;
		break;
	case 2:
	case 3:
		div1 = 2;
		break;
	}
	if (cpu_data->cut_major < 2)
		div2 = ratio1[(div_cfg >> shift) & 7];
	else
		div2 = ratio2[(div_cfg >> shift) & 7];
	clk->rate = (clk->parent->rate / div1) / div2;

	/* Note clk_sh4 and clk_sh4_ic have an extra clock gating
	 * stage here based on DIV2_CFG bits 0 and 1. clk_sh4_per (aka
	 * module_clock) doesn't.
	 *
	 * However if we ever implement this, remember that fdma0/1
	 * may use clk_sh4 prior to the clock gating.
	 */
	return 0;
}

static struct clk_ops sh4_clk_ops = {
	.init		= sh4_clk_recalc,
	.recalc		= sh4_clk_recalc,
};

#define SH4_CLK(_name, _shift)					\
{	.name = _name,						\
	.parent = &pllclks[0],					\
	.ops    = &sh4_clk_ops,					\
	.private_data = (void *)(_shift),			\
}

static struct clk sh4clks[3] = {
	SH4_CLK("st40_clk", 1),
	SH4_CLK("st40_ic_clk", 4),
	SH4_CLK("st40_per_clk", 7),
};

struct fdmalxclk {
	char fdma_num;
	char div_cfg_reg;
	char div_cfg_shift;
	char normal_div;
};

static int fdma_clk_init(struct clk *clk)
{
	struct fdmalxclk *fdmaclk =  (struct fdmalxclk *)clk->private_data;
	unsigned long mux_cfg = readl(CLOCKGEN_MUX_CFG);

	if ((mux_cfg & CLOCKGEN_MUX_CFG_FDMA_SRC(fdmaclk->fdma_num)) == 0)
		clk->parent = &sh4clks[0];
	else
		clk->parent = &pllclks[1];
	return 0;
}

static int fdmalx_clk_recalc(struct clk *clk)
{
	struct fdmalxclk *fdmalxclk =  (struct fdmalxclk *)clk->private_data;
	unsigned long div_cfg;
	unsigned long div_ratio;
	unsigned long normal_div;

	div_cfg = readl(CLOCKGEN_DIV_CFG + fdmalxclk->div_cfg_reg);
	div_ratio = (div_cfg >> fdmalxclk->div_cfg_shift) & 3;
	normal_div = fdmalxclk->normal_div;
	clk->rate = final_divider(clk->parent->rate, div_ratio, normal_div);
	return 0;
}

static void lx_clk_XXable(struct clk *clk, int enable)
{
	struct fdmalxclk *fdmalxclk = (struct fdmalxclk *)clk->private_data;
	unsigned long div_cfg = readl(CLOCKGEN_DIV_CFG +
			fdmalxclk->div_cfg_reg);
	if (enable) {
		writel(div_cfg |
			(fdmalxclk->normal_div << fdmalxclk->div_cfg_shift),
			CLOCKGEN_DIV_CFG + fdmalxclk->div_cfg_reg);
		fdmalx_clk_recalc(clk); /* to evaluate the rate */
	} else {
		writel(div_cfg & ~(0x3<<fdmalxclk->div_cfg_shift),
			CLOCKGEN_DIV_CFG + fdmalxclk->div_cfg_reg);
		clk->rate = 0;
	}
}
static int lx_clk_enable(struct clk *clk)
{
	lx_clk_XXable(clk, 1);
	return 0;
}
static int lx_clk_disable(struct clk *clk)
{
	lx_clk_XXable(clk, 0);
	return 0;
}

static struct clk_ops fdma_clk_ops = {
	.init		= fdma_clk_init,
	.recalc		= fdmalx_clk_recalc,
};

static struct clk_ops lx_clk_ops = {
	.recalc		= fdmalx_clk_recalc,
	.enable		= lx_clk_enable,
	.disable	= lx_clk_disable,
};

static int ic266_clk_recalc(struct clk *clk)
{
	unsigned long div_cfg;
	unsigned long div_ratio;

	div_cfg = readl(CLOCKGEN_DIV2_CFG);
	div_ratio = ((div_cfg & (1<<5)) == 0) ? 1024 : 3;
	clk->rate = clk->parent->rate / div_ratio;
	return 0;
}

static struct clk_ops ic266_clk_ops = {
	.recalc		= ic266_clk_recalc,
};

#define CLKGENA(_name, _parent, _ops, _flags)			\
	{							\
		.name		= #_name,			\
		.parent		= _parent,			\
		.ops		= &_ops,			\
	}

static struct clk miscclks[1] = {
	CLKGENA(ic_266, &pllclks[2], ic266_clk_ops, 0),
};

#define CLKGENA_FDMALX(_name, _parent, _ops, _fdma_num,		\
	_div_cfg_reg, _div_cfg_shift, _normal_div)		\
{								\
	.name		= #_name,				\
	.parent		= _parent,				\
	.ops		= &_ops,				\
	.private_data = (void *) &(struct fdmalxclk)		\
	{							\
		.fdma_num = _fdma_num,				\
		.div_cfg_reg = _div_cfg_reg - CLOCKGEN_DIV_CFG,	\
		.div_cfg_shift = _div_cfg_shift,		\
		.normal_div = _normal_div,			\
	}							\
}

#define CLKGENA_FDMA(name, num)					\
	CLKGENA_FDMALX(name, NULL, fdma_clk_ops, num,		\
			CLOCKGEN_DIV_CFG, 10, 1)

#define CLKGENA_LX(name, shift)				\
	CLKGENA_FDMALX(name, &pllclks[1], lx_clk_ops, 0,	\
			CLOCKGEN_DIV_CFG, shift, 1)

#define CLKGENA_MISCDIV(name, shift, ratio)		\
	CLKGENA_FDMALX(name, &pllclks[2], lx_clk_ops, 0,	\
			CLOCKGEN_DIV2_CFG, shift, ratio)

static struct clk fdma_lx_miscdiv_clks[] = {
	CLKGENA_FDMA(fdma_clk0, 0),
	CLKGENA_FDMA(fdma_clk1, 1),

	CLKGENA_LX(lx_aud0_cpu_clk, 12),
	CLKGENA_LX(lx_aud1_cpu_clk, 14),
	CLKGENA_LX(lx_dmu0_cpu_clk, 16),
	CLKGENA_LX(lx_dmu1_cpu_clk, 18),

	CLKGENA_MISCDIV(dmu0_266, 18, 3),
	CLKGENA_MISCDIV(disp_266, 22, 3),
	CLKGENA_MISCDIV(bdisp_200, 6, 4),
	CLKGENA_MISCDIV(fdma_200, 14, 4)
};

enum clockgen2B_ID {
	DIV2_B_BDISP266_ID = 0,
	DIV2_B_COMPO200_ID,
	DIV2_B_DISP200_ID,
	DIV2_B_VDP200_ID,
	DIV2_B_DMU1266_ID,
};

enum clockgen3B_ID{
	MISC_B_ICREG_ID = 0,
	MISC_B_ETHERNET_ID,
	MISC_B_EMIMASTER_ID
};

static int pll_clkB_init(struct clk *clk)
{
	unsigned long input, output;
	unsigned long mux_cfg, pll_cfg;

	/* FIXME: probably needs more work! */

	mux_cfg = readl(CLOCKGENB_IN_MUX_CFG);
	if (mux_cfg & CLOCKGENB_IN_MUX_CFG_PLL_SRC)
		input = sysclkinalt[1];
	else
		input = SYSBCLKIN;


	pll_cfg = readl(CLOCKGENB_PLL0_CFG);
	output = pll02_freq(input, pll_cfg);

	if (!(pll_cfg & CLOCKGEN_PLL_CFG_BYPASS))
		clk->rate = output;
	else if (!(mux_cfg & CLOCKGENB_OUT_MUX_CFG_DIV_SRC))
		clk->rate = input;
	else
		clk->rate = SYSBCLKIN;

	return 0;
}

static void pll_clkB_XXable(struct clk *clk, int enable)
{
	unsigned long bps = readl(CLOCKGENB_PLL0_CFG);
	unsigned long pwr = readl(CLOCKGENB_POWER_CFG);

	if (enable) {
		writel(pwr & ~(1<<15), CLOCKGENB_POWER_CFG);	 /* turn-on  */
		mdelay(1);
		writel(bps & ~(1<<20), CLOCKGENB_PLL0_CFG);	/* bypass off*/
		pll_clkB_init(clk); /* to evaluate the rate */
	} else {
		writel(bps | 1<<20, CLOCKGENB_PLL0_CFG);	/* bypass on */
		writel(pwr | 1<<15, CLOCKGENB_POWER_CFG); 	/* turn-off  */
		clk->rate = 0;
	}
}

static int pll_clkB_enable(struct clk *clk)
{
	pll_clkB_XXable(clk, 1);
	return 0;
}

static int pll_clkB_disable(struct clk *clk)
{
	pll_clkB_XXable(clk, 0);
	return 0;
}

static struct clk_ops pll_clkB_ops = {
	.init		= pll_clkB_init,
	.enable		= pll_clkB_enable,
	.disable	= pll_clkB_disable,
};

static struct clk clkB_pllclks[1] = {
	{
	.name		= "b_pll0_clk",
	.ops		= &pll_clkB_ops,
	.private_data	= NULL,
	}
};


struct clkgenBdiv2 {
	char   div_cfg_shift;
	char   normal_div;
};

#define CLKGENB(_id, _name, _ops, _flags)			\
	{							\
		.name		= #_name,			\
		.parent		= &clkB_pllclks[0],		\
		.ops		= &_ops,			\
		.id		= (_id),			\
	}

#define CLKGENB_DIV2(_id, _name, _div_cfg_shift, _normal_div)	\
{								\
	.name		= #_name,				\
	.parent		= &clkB_pllclks[0],			\
	.ops		= &clkgenb_div2_ops,			\
	.id		= _id,					\
	.private_data   = (void *) &(struct clkgenBdiv2)	\
	 {							\
		.div_cfg_shift = _div_cfg_shift,		\
		.normal_div = _normal_div,			\
	}							\
}

static int clkgenb_div2_recalc(struct clk *clk)
{
	struct clkgenBdiv2 *clkgenBdiv2 =
		(struct clkgenBdiv2 *)clk->private_data;
	unsigned long div_cfg;
	unsigned long div_ratio;

	div_cfg = readl(CLOCKGENB_DIV2_CFG);
	div_ratio = (div_cfg >> clkgenBdiv2->div_cfg_shift) & 3;
	clk->rate =  final_divider(clk->parent->rate, div_ratio,
				  clkgenBdiv2->normal_div);
	return 0;
}

static int clkgenb_div2_XXable(struct clk *clk, int enable)
{
	struct clkgenBdiv2 *clkgenBdiv2 =
			(struct clkgenBdiv2 *)clk->private_data;
	unsigned long div_cfg = readl(CLOCKGENB_DIV2_CFG);
	unsigned long div_ratio = (div_cfg >> clkgenBdiv2->div_cfg_shift) & 3;

	if (enable) {
		div_cfg |= clkgenBdiv2->normal_div <<
			clkgenBdiv2->div_cfg_shift;
		writel(div_cfg, CLOCKGENB_DIV2_CFG);
		final_divider(clk->parent->rate, div_ratio,
			clkgenBdiv2->normal_div); /* to evaluate the rate */
	} else {
		div_cfg &= ~(0x3<<clkgenBdiv2->div_cfg_shift);
		writel(div_cfg, CLOCKGENB_DIV2_CFG);
		clk->rate = 0;
	}
	return 0;
}

static int clkgenb_div2_enable(struct clk *clk)
{
	return clkgenb_div2_XXable(clk, 1);
}
static int clkgenb_div2_disable(struct clk *clk)
{
	return clkgenb_div2_XXable(clk, 0);
}

static struct clk_ops clkgenb_div2_ops = {
	.enable		= clkgenb_div2_enable,
	.disable	= clkgenb_div2_disable,
	.recalc		= clkgenb_div2_recalc,
};

struct clk clkB_div2clks[5] = {
	CLKGENB_DIV2(DIV2_B_BDISP266_ID, bdisp_266,  6, 3),
	CLKGENB_DIV2(DIV2_B_COMPO200_ID, compo_200,  8, 4),
	CLKGENB_DIV2(DIV2_B_DISP200_ID, disp_200,  10, 4),
	CLKGENB_DIV2(DIV2_B_VDP200_ID, vdp_200,   12, 4),
	CLKGENB_DIV2(DIV2_B_DMU1266_ID, dmu1_266,  20, 3)
};

static int icreg_emi_eth_clk_recalc(struct clk *clk)
{
	unsigned long mux_cfg;
	unsigned long div_ratio;

	mux_cfg = readl(CLOCKGEN_MUX_CFG);
	div_ratio = ((mux_cfg & (CLOCKGEN_MUX_CFG_IC_REG_SRC)) == 0) ? 8 : 6;
	clk->rate = clk->parent->rate / div_ratio;
	return 0;
}

static int icreg_emi_eth_clk_XXable(struct clk *clk, int enable)
{
	unsigned long id = (unsigned long)clk->private_data;
	unsigned long tmp = readl(CLOCKGENB_DIV2_CFG);
	if (enable) {
		writel(tmp & ~(1<<(id+2)), CLOCKGENB_DIV2_CFG);
		icreg_emi_eth_clk_recalc(clk); /* to evaluate the rate */
	} else {
		writel(tmp | (1<<(id+2)), CLOCKGENB_DIV2_CFG);
		clk->rate = 0;
	}
	return 0;
}

static int icreg_emi_eth_clk_enable(struct clk *clk)
{
	return icreg_emi_eth_clk_XXable(clk, 1);
}
static int icreg_emi_eth_clk_disable(struct clk *clk)
{
	return icreg_emi_eth_clk_XXable(clk, 0);
}

static struct clk_ops icreg_emi_eth_clk_ops = {
	.init		= icreg_emi_eth_clk_recalc,
	.recalc		= icreg_emi_eth_clk_recalc,
#if 0
/* I have to check why the following function have problem on cut 2 */
	.enable		= icreg_emi_eth_clk_enable,
	.disable	= icreg_emi_eth_clk_disable
#endif
};

static struct clk clkB_miscclks[3] = {
/* Propages to comms_clk */
CLKGENB(MISC_B_ICREG_ID, ic_reg, icreg_emi_eth_clk_ops, CLK_RATE_PROPAGATES),
CLKGENB(MISC_B_ETHERNET_ID, ethernet,   icreg_emi_eth_clk_ops, 0),
CLKGENB(MISC_B_EMIMASTER_ID, emi_master, icreg_emi_eth_clk_ops, 0),
};

/******************************************************************************
CLOCKGEN C (audio)
******************************************************************************/

/* ========================================================================
   Name:	clkgenc_fsyn_recalc
   Description: Get CKGC FSYN clocks frequencies function
   Returns:     0=NO error
   ======================================================================== */
static int clkgenc_init(clk_t *clk_p);
static int clkgenc_enable(clk_t *clk_p);
static int clkgenc_disable(clk_t *clk_p);
static int clkgenc_recalc(clk_t *clk_p);
static int clkgenc_set_rate(clk_t *clk_p, unsigned long freq);

_CLK_OPS(clkgenc,
	"Clockgen C/Audio",
	clkgenc_init,
	NULL,
	clkgenc_set_rate,
	clkgenc_recalc,
	clkgenc_enable,
	clkgenc_disable,
	NULL,
	NULL,	   /* No measure function */
	NULL	    /* No observation point */
);

static clk_t clk_clocks[] = {
/* Clockgen C (AUDIO) */
_CLK_P(CLKC_REF, &clkgenc, 0, CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED, NULL),
_CLK_P(CLKC_FS0_CH1, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS0_CH2, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS0_CH3, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS0_CH4, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS1_CH1, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS1_CH2, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS1_CH3, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
_CLK_P(CLKC_FS1_CH4, &clkgenc, 0, 0, &clk_clocks[CLKC_REF]),
};

/******************************************************************************
CLOCKGEN C (audio)
******************************************************************************/

/* ========================================================================
   Name:	clkgenc_fsyn_recalc
   Description: Get CKGC FSYN clocks frequencies function
   Returns:	0=NO error
   ======================================================================== */

static int clkgenc_fsyn_recalc(clk_t *clk_p)
{
	int bank, channel;
	unsigned long cfg, dig_bit;
	unsigned long pe, md, sdiv;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKC_FS0_CH1 || clk_p->id > CLKC_FS1_CH4)
		return CLK_ERR_BAD_PARAMETER;

	bank = (clk_p->id - CLKC_FS0_CH1) / 4;
	channel = (clk_p->id - CLKC_FS0_CH1) % 4;

	/* Is FSYN analog UP ? */
	cfg = CLK_READ(CLOCKGENC_BASE_ADDR + CKGC_FS_CFG(bank));
	if (!(cfg & (1 << 14))) {       /* Analog power down */
		clk_p->rate = 0;
		return 0;
	}

	/* Is FSYN digital part UP ? */
	dig_bit = 10 + channel;

	if ((cfg & (1 << dig_bit)) == 0) {      /* digital part in standby */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN up & running.
	   Computing frequency */
	pe = CLK_READ(CLOCKGENC_BASE_ADDR + CKGC_FS_PE(bank, channel));
	md = CLK_READ(CLOCKGENC_BASE_ADDR + CKGC_FS_MD(bank, channel));
	sdiv = CLK_READ(CLOCKGENC_BASE_ADDR + CKGC_FS_SDIV(bank, channel));
	err = clk_fsyn_get_rate(clk_p->parent->rate, pe, md,
					sdiv, &clk_p->rate);

	return err;
}

/* ========================================================================
   Name:	clkgenc_recalc
   Description: Get CKGC clocks frequencies function
   Returns:	0=NO error
   ======================================================================== */

static int clkgenc_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKC_REF:
		clk_p->rate = CONFIG_SH_EXTERNAL_CLOCK;
		break;
	case CLKC_FS0_CH1 ... CLKC_FS0_CH4:  /* FS0 clocks */
	case CLKC_FS1_CH1 ... CLKC_FS1_CH4:  /* FS1 clocks */
		return clkgenc_fsyn_recalc(clk_p);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenc_set_rate
   Description: Set CKGC clocks frequencies
   Returns:	0=NO error
   ======================================================================== */

static int clkgenc_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long md, pe, sdiv;
	unsigned long reg_value = 0;
	int bank, channel;
	static const unsigned char set_rate_table[] = {
		0x04, 0x08, 0x10, 0x20 };

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKC_FS0_CH1 || clk_p->id > CLKC_FS1_CH4)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fsyn_get_params(clk_p->parent->rate, freq, &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	bank = (clk_p->id - CLKC_FS0_CH1) / 4;
	channel = (clk_p->id - CLKC_FS0_CH1) % 4;

	reg_value = CLK_READ(CLOCKGENC_BASE_ADDR + CKGC_FS_CFG(bank));
	reg_value &= ~set_rate_table[channel];

	/* Select FS clock only for the clock specified */
	CLK_WRITE(CLOCKGENC_BASE_ADDR + CKGC_FS_CFG(bank), reg_value);

	CLK_WRITE(CLOCKGENC_BASE_ADDR + CKGC_FS_PE(bank, channel), pe);
	CLK_WRITE(CLOCKGENC_BASE_ADDR + CKGC_FS_MD(bank, channel), md);
	CLK_WRITE(CLOCKGENC_BASE_ADDR + CKGC_FS_SDIV(bank, channel), sdiv);
	CLK_WRITE(CLOCKGENC_BASE_ADDR + CKGC_FS_EN_PRG(bank, channel), 0x01);
	CLK_WRITE(CLOCKGENC_BASE_ADDR + CKGC_FS_EN_PRG(bank, channel), 0x00);

	return clkgenc_recalc(clk_p);
}

static int clkgenc_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	clkgenc_recalc(clk_p);

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
	int bank, channel;
	unsigned long val;
	/* Digital standby bits table.
	   Warning: enum order: CLKC_FS0_CH1 ... CLKC_FS0_CH3
                                CLKC_FS1_CH1 ... CLKC_FS1_CH3 */
	static const unsigned char dig_bit[] = {10, 11, 12, 13};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKC_FS0_CH1 || clk_p->id > CLKC_FS1_CH4)
		return CLK_ERR_BAD_PARAMETER;

	bank = (clk_p->id - CLKC_FS0_CH1) / 4;
	channel = (clk_p->id - CLKC_FS0_CH1) % 4;

	val = CLK_READ(CLOCKGENC_BASE_ADDR + CKGC_FS_CFG(bank));

	/* Powering down/up digital part */
	if (enable) {
		val |= (1 << dig_bit[channel]);
	} else {
		val &= ~(1 << dig_bit[channel]);
	}

	/* Powering down/up analog part */
	if (enable)
		val |= (1 << 14);
	else {
		/* If all channels are off then power down the fsynth */
		if ((val & 0x3fc0) == 0)
			val &= ~(1 << 14);
	}

	CLK_WRITE(CLOCKGENC_BASE_ADDR + CKGC_FS_CFG(bank), val);

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

int __init plat_clk_init(void)
{
	int ret;

	/* Clockgen A */
	ret = clk_register_table(pllclks, ARRAY_SIZE(pllclks), 1);
	if (ret)
		return ret;

	if (cpu_data->cut_major < 2) {
		ret = clk_register_table(sh4clks, ARRAY_SIZE(sh4clks), 1);
		if (ret)
			return ret;
	} else {
		ret = clk_register_table(sh4clks, 1, 1);
		if (ret)
			return ret;
	}

	ret = clk_register_table(fdma_lx_miscdiv_clks,
			ARRAY_SIZE(fdma_lx_miscdiv_clks), 1);
	if (ret)
		return ret;

	/* Clockgen B */
	writel(readl(CLOCKGENB_IN_MUX_CFG) & ~0xf, CLOCKGENB_IN_MUX_CFG);

	ret = clk_register_table(clkB_pllclks, ARRAY_SIZE(clkB_pllclks), 1);
	if (ret)
		return ret;

	ret = clk_register_table(clkB_div2clks,	ARRAY_SIZE(clkB_div2clks), 1);

	if (ret)
		return ret;

	ret = clk_register_table(clkB_miscclks, ARRAY_SIZE(clkB_miscclks), 1);

	if (ret)
		return ret;

	/* clock gen C */
	ret = clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);

	return 0;
}
