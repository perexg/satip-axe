/*****************************************************************************
 *
 * File name   : clock-common.c
 * Description : Low Level API - Common LLA functions (SOC independant)
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
11/mar/10 fabrice.charpentier@st.com
	  clk_pll800_get_params() fully revisited.
10/dec/09 francesco.virlinzi@st.com
	  clk_pll1600_get_params() now same code for OS21 & Linux.
13/oct/09 fabrice.charpentier@st.com
	  clk_fsyn_get_rate() API changed. Now returns error code.
30/sep/09 fabrice.charpentier@st.com
	  Introducing clk_pll800_get_rate() & clk_pll1600_get_rate() to
	  replace clk_pll800_freq() & clk_pll1600_freq().
*/


#include <linux/clk.h>
#include <asm-generic/div64.h>

/*
 * Linux specific function
 */

/* Return the number of set bits in x. */
static unsigned int population(unsigned int x)
{
	/* This is the traditional branch-less algorithm for population count */
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0f0f0f0f;
	x = x + (x << 8);
	x = x + (x << 16);

	return x >> 24;
}

/* Return the index of the most significant set in x.
 * The results are 'undefined' is x is 0 (0xffffffff as it happens
 * but this is a mere side effect of the algorithm. */
static unsigned int most_significant_set_bit(unsigned int x)
{
	/* propagate the MSSB right until all bits smaller than MSSB are set */
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);

	/* now count the number of set bits [clz is population(~x)] */
	return population(x) - 1;
}

#include "clock-oslayer.h"
#include "clock-common.h"

/* ========================================================================
   Name:        clk_pll800_get_rate()
   Description: Convert input/mdiv/ndiv/pvid values to frequency for PLL800
   Params:      'input' freq (Hz), mdiv/ndiv/pvid values
   Output:      '*rate' updated
   Return:      Error code.
   ======================================================================== */

int clk_pll800_get_rate(unsigned long input, unsigned long mdiv,
	unsigned long ndiv, unsigned long pdiv, unsigned long *rate)
{
	if (!mdiv)
		mdiv++; /* mdiv=0 or 1 => MDIV=1 */

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = (((2 * (input/1000) * ndiv) / mdiv) / (1 << pdiv)) * 1000;

	return 0;
}

/* ========================================================================
   Name:        clk_pll1600_get_rate()
   Description: Convert input/mdiv/ndiv values to frequency for PLL1600
   Params:      'input' freq (Hz), mdiv/ndiv values
		Info: mdiv also called rdiv, ndiv also called ddiv
   Output:      '*rate' updated with value of HS output.
   Return:      Error code.
   ======================================================================== */

int clk_pll1600_get_rate(unsigned long input, unsigned long mdiv,
			 unsigned long ndiv, unsigned long *rate)
{
	if (!mdiv)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = ((2 * (input/1000) * ndiv) / mdiv) * 1000;

	return 0;
}

/* ========================================================================
   Name:        clk_pll800_get_params()
   Description: Freq to parameters computation for PLL800
   Input:       input & output freqs (Hz)
   Output:      updated *mdiv, *ndiv & *pdiv (register values)
   Return:      'clk_err_t' error code
   ======================================================================== */
/*
 * PLL800 in FS mode computation algo
 *
 *             2 * N * Fin Mhz
 * Fout Mhz = -----------------		[1]
 *                M * (2 ^ P)
 *
 * Rules:
 *   6.25Mhz <= output <= 800Mhz
 *   FS mode means 3 <= N <= 255
 *   1 <= M <= 255
 *   1Mhz <= PFDIN (input/M) <= 50Mhz
 *   200Mhz <= FVCO (input*2*N/M) <= 800Mhz
 *   For better long term jitter select M minimum && P maximum
 */

int clk_pll800_get_params(unsigned long input, unsigned long output,
	unsigned long *mdiv, unsigned long *ndiv, unsigned long *pdiv)
{
	unsigned long m, n, pfdin, fvco;
	unsigned long deviation, new_freq;
	long new_deviation, pi;

	/* Output clock range: 6.25Mhz to 800Mhz */
	if (output < 6250000 || output > 800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	deviation = output;
	for (pi = 5; pi >= 0 && deviation; pi--) {
		for (m = 1; (m < 255) && deviation; m++) {
			n = m * (1 << pi) * output / (input * 2);

			/* Checks */
			if (n < 3)
				continue;
			if (n > 255)
				break;
			pfdin = input / m; /* 1Mhz <= PFDIN <= 50Mhz */
			if (pfdin < 1000 || pfdin > 50000)
				continue;
			/* 200Mhz <= FVCO <= 800Mhz */
			fvco = (input * 2 * n) / m;
			if (fvco > 800000)
				continue;
			if (fvco < 200000)
				break;

			new_freq = (input * 2 * n) / (m * (1 << pi));
			new_deviation = new_freq - output;
			if (new_deviation < 0)
				new_deviation = -new_deviation;
			if (!new_deviation || new_deviation < deviation) {
				*mdiv	= m;
				*ndiv	= n;
				*pdiv	= pi;
				deviation = new_deviation;
			}
		}
	}

	if (deviation == output) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

    return 0;
}

/* ========================================================================
   Name:        clk_pll1600_get_params()
   Description: Freq to parameters computation for PLL1600
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *mdiv (rdiv) & *ndiv (ddiv)
   Return:      'clk_err_t' error code
   ======================================================================== */


/*
 * Rules:
 *   600Mhz <= output (FVCO) <= 1800Mhz
 *   1 <= M (also called R) <= 7
 *   4 <= N <= 255
 *   4Mhz <= PFDIN (input/M) <= 75Mhz
 */

int clk_pll1600_get_params(unsigned long input, unsigned long output,
			   unsigned long *mdiv, unsigned long *ndiv)
{
	unsigned long m, n, pfdin;
	unsigned long deviation, new_freq;
	long new_deviation;

	/* Output clock range: 600Mhz to 1800Mhz */
	if (output < 600000000 || output > 1800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	deviation = output;
	for (m = 1; (m < 7) && deviation; m++) {
		n = m * output / (input * 2);

		/* Checks */
		if (n < 4)
			continue;
		if (n > 255)
			break;
		pfdin = input / m; /* 4Mhz <= PFDIN <= 75Mhz */
		if (pfdin < 4000 || pfdin > 75000)
			continue;

		new_freq = (input * 2 * n) / m;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			*mdiv	= m;
			*ndiv	= n;
			deviation = new_deviation;
		}
	}

	if (deviation == output) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	return 0;
}

/* ========================================================================
   Name:        clk_fsyn_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
		WARNING: parameters are HARDWARE CODED values, not the one
		used in formula.
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

int clk_fsyn_get_rate(unsigned long input, unsigned long pe,
		unsigned long md, unsigned long sd, unsigned long *rate)
{
	int md2 = md;
	long long p, q, r, s, t;
	if (md & 0x10)
		md2 = md | 0xfffffff0;/* adjust the md sign */

	input *= 8;

	p = 1048576ll * input;
	q = 32768 * md2;
	r = 1081344 - pe;
	s = r + q;
	t = (1 << (sd + 1)) * s;
	*rate = div64_u64(p, t);

	return 0;
}

/* ========================================================================
   Name:        clk_fsyn_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe & *sdiv
		WARNING: parameters are HARDWARE CODED values, not the one
		used in formula.
   Return:      'clk_err_t' error code
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types.
   Currently based on C090_4FS216_25. */

int clk_fsyn_get_params(unsigned long input, unsigned long output,
			unsigned long *md, unsigned long *pe,
			unsigned long *sdiv)
{
	unsigned long long p, q;
	unsigned int predivide;
	int preshift; /* always +ve but used in subtraction */
	unsigned int lsdiv;
	int lmd;
	unsigned int lpe = 1 << 14;

	/* pre-divide the frequencies */
	p = 1048576ull * input * 8;    /* <<20? */
	q = output;

	predivide = (unsigned int)div64_u64(p, q);

	/* determine an appropriate value for the output divider using eqn. #4
	 * with md = -16 and pe = 32768 (and round down) */
	lsdiv = predivide / 524288;
	if (lsdiv > 1) {
		/* sdiv = fls(sdiv) - 1; // this doesn't work
		 * for some unknown reason */
		lsdiv = most_significant_set_bit(lsdiv);
	} else
		lsdiv = 1;

	/* pre-shift a common sub-expression of later calculations */
	preshift = predivide >> lsdiv;

	/* determine an appropriate value for the coarse selection using eqn. #5
	 * with pe = 32768 (and round down which for signed values means away
	 * from zero) */
	lmd = ((preshift - 1048576) / 32768) - 1;         /* >>15? */

	/* calculate a value for pe that meets the output target */
	lpe = -1 * (preshift - 1081344 - (32768 * lmd));  /* <<15? */

	/* finally give sdiv its true hardware form */
	lsdiv--;
	/* special case for 58593.75Hz and harmonics...
	* can't quite seem to get the rounding right */
	if (lmd == -17 && lpe == 0) {
		lmd = -16;
		lpe = 32767;
	}

	/* update the outgoing arguments */
	*sdiv = lsdiv;
	*md = lmd;
	*pe = lpe;

	/* return 0 if all variables meet their contraints */
	return (lsdiv <= 7 && -16 <= lmd && lmd <= -1 && lpe <= 32767) ? 0 : -1;
}

