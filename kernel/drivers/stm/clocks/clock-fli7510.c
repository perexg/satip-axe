/*****************************************************************************
 *
 * File name   : clock-fli7510.c
 * Description : Low Level API - HW specific implementation
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
 * Original code from stx7111 platform
*/

/* Includes ----------------------------------------------------------------- */

#include <linux/stm/fli7510.h>
#include <linux/stm/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "clock-fli7510.h"
#include "clock-regs-fli7510.h"

#include "clock-oslayer.h"
#include "clock-common.h"
#include "clock-utils.h"

static int clkgena_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgena_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgena_set_div(clk_t *clk_p, unsigned long *div_p);
static int clkgena_recalc(clk_t *clk_p);
static int clkgena_enable(clk_t *clk_p);
static int clkgena_disable(clk_t *clk_p);
static int clkgena_init(clk_t *clk_p);

#define SYSA_CLKIN			30	/* FE osc */

_CLK_OPS(clkgena,
	"Clockgen A",
	clkgena_init,
	clkgena_set_parent,
	clkgena_set_rate,
	clkgena_recalc,
	clkgena_enable,
	clkgena_disable,
	NULL,
	NULL,
	NULL
);

/* Physical clocks description */
clk_t clk_clocks[] = {
/* Top level clocks */
_CLK_F(CLK_SYSA, SYSA_CLKIN * 1000000),

/* Clockgen A */
_CLK_P(CLKA_REF, &clkgena, 0, 0, &clk_clocks[CLK_SYSA]),
_CLK_P(CLKA_PLL0HS, &clkgena, 900000000, 0, &clk_clocks[CLKA_REF]),
_CLK_P(CLKA_PLL0LS, &clkgena, 450000000, 0, &clk_clocks[CLKA_PLL0HS]),
_CLK_P(CLKA_PLL1, &clkgena, 800000000, 0, &clk_clocks[CLKA_REF]),

_CLK(CLKA_ST231_AUD_1, &clkgena, 0, 0),
_CLK(CLKA_VCPU, &clkgena, 0, 0),
_CLK(CLKA_TANGO, &clkgena, 0, 0),
_CLK(CLKA_ST231_AUD_2, &clkgena, 0, 0),
_CLK(CLKA_ST40_HOST, &clkgena, 0, 0),
_CLK(CLKA_ST40_RT, &clkgena, 0, 0),
_CLK(CLKA_ST231_DRA2, &clkgena, 0, 0),
_CLK(CLKA_FDMA, &clkgena, 0, 0),
_CLK(CLKA_BIT, &clkgena, 0, 0),
_CLK(CLKA_AATV, &clkgena, 0, 0),
_CLK(CLKA_EMI, &clkgena, 0, 0),
_CLK(CLKA_PP, &clkgena, 0, 0),
_CLK(CLKA_ETH_PHY, &clkgena, 0, 0),
_CLK(CLKA_PCI,  &clkgena, 0, 0),
_CLK(CLKA_IC_100, &clkgena, 0, 0),
_CLK(CLKA_IC_150, &clkgena, 0, 0),
_CLK(CLKA_IC_266, &clkgena, 0, 0),
_CLK(CLKA_IC_200, &clkgena, 0, 0),

};

/******************************************************************************
CLOCKGEN A (CPU+interco+comms) clocks group
******************************************************************************/

/* ========================================================================
Name:	     clkgena_get_index
Description: Returns index of given clockgenA clock and source reg infos
Returns:     idx==-1 if error, else >=0
======================================================================== */

static int clkgena_get_index(int clkid, unsigned long *srcreg, int *shift)
{
	int idx;

	/* Warning: This function assumes clock IDs are perfectly
	   following real implementation order. Each "hole" has therefore
	   to be filled with "CLKx_NOT_USED" */
	if (clkid < CLKA_ST231_AUD_1 || clkid > CLK_NOT_USED_2)
		return -1;

	idx = (clkid - CLKA_ST231_AUD_1) % 16;

	*srcreg = CKGA_CLKOPSRC_SWITCH_CFG + (0x10 * (idx / 16));
	*shift = 2 * (idx % 16);

	return idx;
}

/* ========================================================================
   Name:	clkgena_set_parent
   Description: Set clock source for clockgenA when possible
   Returns:     0=NO error
   ======================================================================== */

static int clkgena_set_parent(clk_t *clk_p, clk_t *src_p)
{
	unsigned long clk_src, val;
	int idx, shift;
	unsigned long srcreg;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA_ST231_AUD_1 || clk_p->id > CLK_NOT_USED_2)
		return CLK_ERR_BAD_PARAMETER;

	switch (src_p->id) {
	case CLKA_REF:
		clk_src = 0;
		break;
	case CLKA_PLL0LS:
	case CLKA_PLL0HS:
		clk_src = 1;
		break;
	case CLKA_PLL1:
		clk_src = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	val = CLK_READ(CKGA_BASE_ADDRESS + srcreg) & ~(0x3 << shift);
	val = val | (clk_src << shift);
	CLK_WRITE(CKGA_BASE_ADDRESS + srcreg, val);

	clk_p->parent = &clk_clocks[src_p->id];

	return clkgena_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgena_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_identify_parent(clk_t *clk_p)
{
	int idx;
	unsigned long src_sel;
	unsigned long srcreg;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKA_REF) {
		/*src_sel = SYSCONF_READ(SYS_STA, 1, 0, 1);*/
		clk_p->parent = &clk_clocks[CLK_SYSA + src_sel];
		return 0;
	}

	if (clk_p->id < CLKA_ST231_AUD_1)
		/* Statically initialized with _CLK_P() macro */
		return 0;

	/* Which divider to setup ? */
	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Identifying source */
	src_sel = (CLK_READ(CKGA_BASE_ADDRESS + srcreg) >> shift) & 0x3;
	switch (src_sel) {
	case 0:
		clk_p->parent = &clk_clocks[CLKA_REF];
		break;
	case 1:
		if (idx <= 3)
			clk_p->parent = &clk_clocks[CLKA_PLL0HS];
		else
			clk_p->parent = &clk_clocks[CLKA_PLL0LS];
		break;
	case 2:
		clk_p->parent = &clk_clocks[CLKA_PLL1];
		break;
	case 3:
		clk_p->parent = NULL;
		clk_p->rate = 0;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgena_xable_pll
   Description: Enable/disable PLL
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_xable_pll(clk_t *clk_p, int enable)
{
	unsigned long val;
	int bit, err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id != CLKA_PLL0HS && clk_p->id != CLKA_PLL1)
		return CLK_ERR_BAD_PARAMETER;

	bit = (clk_p->id == CLKA_PLL0HS ? 0 : 1);
	val = CLK_READ(CKGA_BASE_ADDRESS + CKGA_POWER_CFG);
	if (enable)
		val &= ~(1 << bit);
	else
		val |= (1 << bit);
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_POWER_CFG, val);

	if (enable)
		err = clkgena_recalc(clk_p);
	else
		clk_p->rate = 0;

	return err;
}

/* ========================================================================
   Name:	clkgena_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_enable(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		/* Unsupported. Init must be called first. */
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power up */
	if (clk_p->id >= CLKA_PLL0HS && clk_p->id <= CLKA_PLL1)
		return clkgena_xable_pll(clk_p, 1);

	err = clkgena_set_parent(clk_p, clk_p->parent);
	/* clkgena_set_parent() is performing also a recalc() */

	return err;
}

/* ========================================================================
   Name:	clkgena_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_disable(clk_t *clk_p)
{
	unsigned long val;
	int idx, shift;
	unsigned long srcreg;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA_PLL0HS || clk_p->id > CLK_NOT_USED_2)
		return CLK_ERR_BAD_PARAMETER;

	/* Can this clock be disabled ? */
	if (clk_p->flags & CLK_ALWAYS_ENABLED)
		return 0;

	/* PLL power down */
	if (clk_p->id >= CLKA_PLL0HS && clk_p->id <= CLKA_PLL1)
		return clkgena_xable_pll(clk_p, 0);

	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	val = CLK_READ(CKGA_BASE_ADDRESS + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);	 /* 3 = STOP clock */
	CLK_WRITE(CKGA_BASE_ADDRESS + srcreg, val);
	clk_p->rate = 0;

	return 0;
}

static int clkgena_set_div(clk_t *clk_p, unsigned long *div_p)
{
	int idx;
	unsigned long div_cfg = 0;
	unsigned long srcreg, offset;
	int shift;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing divider config */
	div_cfg = (*div_p - 1) & 0x1F;

	/* Which divider to setup ? */
	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Now according to parent, let's write divider ratio */
	offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKA_REF);
	CLK_WRITE(CKGA_BASE_ADDRESS + offset + (4 * idx), div_cfg);

	return 0;
}

static int clkgena_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA_PLL0HS || clk_p->id > CLK_NOT_USED_2)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL set rate: to be completed */
	if ((clk_p->id >= CLKA_PLL0HS) && (clk_p->id <= CLKA_PLL1))
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	div = clk_p->parent->rate / freq;
	err = clkgena_set_div(clk_p, &div);
	if (!err)
		clk_p->rate = clk_p->parent->rate / div;

	return err;
}

static int clkgena_recalc(clk_t *clk_p)
{
	unsigned long data, ratio;
	int idx;
	unsigned long srcreg, offset;
	int shift, err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	/* Reading clock programmed value */
	switch (clk_p->id) {
	case CLKA_REF:  /* Clockgen A reference clock */
		clk_p->rate = clk_p->parent->rate;
		break;
	case CLKA_PLL0HS:
		data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_PLL0_CFG);
		err = clk_pll1600_get_rate(clk_p->parent->rate, data & 0x7,
				(data >> 8) & 0xff, &clk_p->rate);
		return err;
	case CLKA_PLL0LS:
		clk_p->rate = clk_p->parent->rate / 2;
		return 0;
	case CLKA_PLL1:
		data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_PLL1_CFG);
		return clk_pll800_get_rate(clk_p->parent->rate, data & 0xff,
			(data >> 8) & 0xff, (data >> 16) & 0x7, &clk_p->rate);

	default:
		idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
		if (idx == -1)
			return CLK_ERR_BAD_PARAMETER;

		/* Now according to source, let's get divider ratio */
		offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKA_REF);
		data = CLK_READ(CKGA_BASE_ADDRESS + offset + (4 * idx));

		ratio = (data & 0x1F) + 1;

		clk_p->rate = clk_p->parent->rate / ratio;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgena_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgena_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgena_identify_parent(clk_p);
	if (!err)
		err = clkgena_recalc(clk_p);

	return err;
}

/*
 * Most of the code on Clock_Gen_C was done by:
 * Pawel Moll
 */
static void *clkgen_audio_base;

static int clkgen_audio_enable(struct clk *clk)
{
	unsigned long value = readl(CTL_SYNTH4X_AUD_N(clkgen_audio_base,
				clk->id - CLKC_FS_FREE_RUN + 1));

	value &= ~NSB__MASK;
	value |= NSB__ACTIVE;

	writel(value, CTL_SYNTH4X_AUD_N(clkgen_audio_base,
		clk->id - CLKC_FS_FREE_RUN + 1));
	return 0;
}

static int clkgen_audio_disable(struct clk *clk)
{
	unsigned long value = readl(CTL_SYNTH4X_AUD_N(clkgen_audio_base,
				clk->id - CLKC_FS_FREE_RUN + 1));

	value &= ~NSB__MASK;
	value |= NSB__STANDBY;

	writel(value, CTL_SYNTH4X_AUD_N(clkgen_audio_base,
		clk->id - CLKC_FS_FREE_RUN + 1));

       return 0;
}

static int clkgen_audio_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long pe, md, sdiv;
	unsigned long value;
	int divider = (int)clk->private_data;

	if (divider)
		rate *= divider;

	if (clk_fsyn_get_params(clk_get_rate(clk->parent), rate,
			&md, &pe, &sdiv) != 0)
		return -EINVAL;

	value = readl(CTL_SYNTH4X_AUD_N(clkgen_audio_base,
		clk->id - CLKC_FS_FREE_RUN + 1));

	value &= ~MD__MASK;
	value |= MD__(md);

	value &= ~PE__MASK;
	value |= PE__(pe);

	value &= ~SDIV__MASK;
	value |= SDIV__(sdiv);

	writel(value, CTL_SYNTH4X_AUD_N(clkgen_audio_base,
		clk->id - CLKC_FS_FREE_RUN + 1));

	if (clk_fsyn_get_rate(clk_get_rate(clk->parent), pe, md,
		sdiv, &clk->rate))
		return -EINVAL;

	if (divider)
		clk->rate /= divider;


	return 0;
}

static struct clk_ops clkgen_audio_clk_ops = {
	.enable = clkgen_audio_enable,
	.disable = clkgen_audio_disable,
	.set_rate = clkgen_audio_set_rate,
};

#define clk_c(_id, _private_data)		\
{						\
	.name = #_id,				\
	.id = _id,				\
	.parent = &clk_clocks[CLK_SYSA],	\
	.ops = &clkgen_audio_clk_ops,		\
	.private_data = (void *)(_private_data)	\
}

static struct clk clkgen_audio_clks[] = {
	clk_c(CLKC_FS_FREE_RUN, 2),
	clk_c(CLKC_FS_DEC_1, 0),
	clk_c(CLKC_SPDIF, 0),
	clk_c(CLKC_FS_DEC_2, 0)
};

static int __init clkgen_audio_init(void)
{
	int err = 0;
	unsigned long value;
	int i;

	if (cpu_data->type == CPU_FLI7510)
		clkgen_audio_base = ioremap(0xfdee0000, 0x30);
	else
		clkgen_audio_base = ioremap(0xfe0e0000, 0x30);

	if (!clkgen_audio_base)
		return -EFAULT;

	/* Configure clkgen */

	value = EN_CLK_256FS_FREE_RUN__ENABLED;
	value |= EN_CLK_256FS_DEC_1__ENABLED;
	value |= EN_CLK_SPDIF_RX__ENABLED;
	value |= EN_CLK_256FS_DEC_2__ENABLED;
	writel(value, CTL_EN(clkgen_audio_base));

	value = SYNTH4X_AUD_NDIV__30_MHZ;
	value |= SYNTH4X_AUD_SELCLKIN__CLKIN1V2;
	value |= SYNTH4X_AUD_SELBW__VERY_GOOD_REFERENCE;
	value |= SYNTH4X_AUD_NPDA__ACTIVE;
	value |= SYNTH4X_AUD_NRST__RESET;
	writel(value, CTL_SYNTH4X_AUD(clkgen_audio_base));

	value = SEL_CLK_OUT__FSYNTH;
	value |= NSB__STANDBY;
	value |= NSDIV3__BYPASSED;
	for (i = 0; i < ARRAY_SIZE(clkgen_audio_clks); i++) {
		writel(value, CTL_SYNTH4X_AUD_N(clkgen_audio_base,
				i + 1));
		/* Set "safe" rate */
		clkgen_audio_set_rate(&clkgen_audio_clks[i], 32000 * 128);
	}

	/* Unreset ;-) it now */
	value = readl(CTL_SYNTH4X_AUD(clkgen_audio_base));
	value &= ~SYNTH4X_AUD_NRST__MASK;
	value |= SYNTH4X_AUD_NRST__NORMAL;
	writel(value, CTL_SYNTH4X_AUD(clkgen_audio_base));

	/* Register clocks */

	err = clk_register_table(clkgen_audio_clks,
			ARRAY_SIZE(clkgen_audio_clks), 0);

	return err;
}

int __init plat_clk_init(void)
{
	int ret;

	ret = clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 1);

	if (ret)
		return ret;

	ret = clkgen_audio_init();

	return ret;
}
