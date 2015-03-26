/*
 * Copyright (C) 2005 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clockgen hardware on the STx7100.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/stm/clk.h>
#include <linux/io.h>
#include <asm-generic/div64.h>

#include "clock-utils.h"

static void __iomem *clkgena_base;

#define CLOCKGEN_PLL0_CFG	0x08
#define CLOCKGEN_PLL0_CLK1_CTRL	0x14
#define CLOCKGEN_PLL0_CLK2_CTRL	0x18
#define CLOCKGEN_PLL0_CLK3_CTRL	0x1c
#define CLOCKGEN_PLL0_CLK4_CTRL	0x20
#define CLOCKGEN_PLL1_CFG	0x24

/* to enable/disable and reduce the coprocessor clock*/
#define CLOCKGEN_CLK_DIV	0x30
#define CLOCKGEN_CLK_EN		0x34

			        /* 0  1  2  3  4  5  6  7  */
static unsigned char ratio1[8] = { 1, 2, 3, 4, 6, 8, 0, 0 };
static unsigned char ratio2[8] = { 1, 2, 3, 4, 6, 8, 0, 0 };
static unsigned char ratio3[8] = { 4, 2, 4, 4, 6, 8, 0, 0 };
static unsigned char ratio4[8] = { 1, 2, 3, 4, 6, 8, 0, 0 };

static int pll_freq(unsigned long addr)
{
	unsigned long freq, data, ndiv, pdiv, mdiv;

	data = readl(clkgena_base+addr);
	mdiv = (data >>  0) & 0xff;
	ndiv = (data >>  8) & 0xff;
	pdiv = (data >> 16) & 0x7;
	freq = (((2 * (CONFIG_SH_EXTERNAL_CLOCK / 1000) * ndiv) / mdiv) /
		(1 << pdiv)) * 1000;

	return freq;
}

static int pll_clk_init(struct clk *clk)
{
	clk->rate = pll_freq(
		(strcmp(clk->name, "pll0_clk") ?
		CLOCKGEN_PLL1_CFG : CLOCKGEN_PLL0_CFG));
	return 0;
}

static struct clk_ops pll_clk_ops = {
	.init = pll_clk_init,
};

static struct clk pll_clk[] = {
	{
		.name		= "pll0_clk",
		.ops		= &pll_clk_ops,
	}, {
		.name		= "pll1_clk",
		.ops		= &pll_clk_ops,
	}
};

struct clokgenA {
	unsigned long ctrl_reg;
	unsigned int div;
	unsigned char *ratio;
};


enum clockgenA_ID {
	SH4_CLK_ID = 0,
	SH4IC_CLK_ID,
	MODULE_ID,
	SLIM_ID,
	LX_AUD_ID,
	LX_VID_ID,
	LMISYS_ID,
	LMIVID_ID,
	IC_ID,
	IC_100_ID,
	EMI_ID
};

static int clockgenA_clk_recalc(struct clk *clk)
{
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;
	clk->rate = clk->parent->rate / cga->div;
	return 0;
}

static int clockgenA_clk_set_rate(struct clk *clk, unsigned long value)
{
	unsigned long data = readl(clkgena_base + CLOCKGEN_CLK_DIV);
	unsigned long val = 1 << (clk->id - 5);

	if (clk->id != LMISYS_ID && clk->id != LMIVID_ID)
		return -1;
	writel(0xc0de, clkgena_base);
	if (clk->rate > value) {/* downscale */
		writel(data | val, clkgena_base + CLOCKGEN_CLK_DIV);
		clk->rate /= 1024;
	} else {/* upscale */
		writel(data & ~val, clkgena_base + CLOCKGEN_CLK_DIV);
		clk->rate *= 1024;
	}
	writel(0x0, clkgena_base);
	return 0;
}

static int clockgenA_clk_init(struct clk *clk)
{
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;
	if (cga->ratio) {
		unsigned long data = readl(clkgena_base + cga->ctrl_reg) & 0x7;
		unsigned char ratio = cga->ratio[data];
		BUG_ON(!ratio);
		cga->div = 2*ratio;
	}
	clk->rate = clk->parent->rate / cga->div;
	return 0;
}

static int clockgenA_clk_XXable(struct clk *clk, int enable)
{
	unsigned long tmp, value;
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;

	if (clk->id != LMISYS_ID && clk->id != LMIVID_ID)
		return 0;

	tmp   = readl(clkgena_base+cga->ctrl_reg) ;
	value = 1 << (clk->id - 5);
	writel(0xc0de, clkgena_base);
	if (enable) {
		writel(tmp | value, clkgena_base + cga->ctrl_reg);
		clockgenA_clk_init(clk); /* to evaluate the rate */
	} else {
		writel(tmp & ~value, clkgena_base + cga->ctrl_reg);
		clk->rate = 0;
	}
	writel(0x0, clkgena_base);
	return 0;
}

static int clockgenA_clk_enable(struct clk *clk)
{
	return clockgenA_clk_XXable(clk, 1);
}

static int clockgenA_clk_disable(struct clk *clk)
{
	return clockgenA_clk_XXable(clk, 0);
}

static struct clk_ops clokgenA_ops = {
	.init		= clockgenA_clk_init,
	.recalc		= clockgenA_clk_recalc,
	.set_rate	= clockgenA_clk_set_rate,
	.enable		= clockgenA_clk_enable,
	.disable	= clockgenA_clk_disable,
};

#define CLKGENA(_id, clock, pll, _ctrl_reg, _div, _ratio)	\
[_id] = {							\
	.name	= #clock "_clk",				\
	.parent	= &(pll),					\
	.ops	= &clokgenA_ops,				\
	.id	= (_id),					\
	.private_data = &(struct clokgenA){			\
		.div = (_div),					\
		.ctrl_reg = (_ctrl_reg),			\
		.ratio = (_ratio)				\
		},						\
	}

static struct clk clkgena_clks[] = {
CLKGENA(SH4_CLK_ID,   st40, pll_clk[0], CLOCKGEN_PLL0_CLK1_CTRL, 1, ratio1),
CLKGENA(SH4IC_CLK_ID, st40_ic, pll_clk[0], CLOCKGEN_PLL0_CLK2_CTRL, 1, ratio2),
CLKGENA(MODULE_ID,    st40_per, pll_clk[0], CLOCKGEN_PLL0_CLK3_CTRL, 1, ratio3),
CLKGENA(SLIM_ID,      slim,     pll_clk[0], CLOCKGEN_PLL0_CLK4_CTRL, 1, ratio4),

CLKGENA(LX_AUD_ID,	st231aud, pll_clk[1], CLOCKGEN_CLK_EN, 1, NULL),
CLKGENA(LX_VID_ID,	st231vid, pll_clk[1], CLOCKGEN_CLK_EN, 1, NULL),
CLKGENA(LMISYS_ID,	lmisys,   pll_clk[1], 0, 1, NULL),
CLKGENA(LMIVID_ID,	lmivid,   pll_clk[1], 0, 1, NULL),
CLKGENA(IC_ID,	ic,	pll_clk[1], 0, 2, NULL),
CLKGENA(IC_100_ID,	ic_100,   pll_clk[1], 0, 4, NULL),
CLKGENA(EMI_ID,	emi,    pll_clk[1], 0, 4, NULL)
};

int __init plat_clk_init(void)
{
	int ret;

	/**************/
	/* Clockgen A */
	/**************/
	clkgena_base = ioremap(0x19213000, 0x100);
	if (!clkgena_base)
		return -ENOMEM;

	ret = clk_register_table(pll_clk, ARRAY_SIZE(pll_clk), 1);
	if (ret)
		return ret;

	ret = clk_register_table(clkgena_clks, ARRAY_SIZE(clkgena_clks), 1);
	return ret;
}
