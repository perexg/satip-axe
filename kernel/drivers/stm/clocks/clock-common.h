/************************************************************************
File  : Low Level clock API
	Common LLA functions (SOC independant)

Author: F. Charpentier <fabrice.charpentier@st.com>

Copyright (C) 2008 STMicroelectronics
************************************************************************/

#ifndef __CLKLLA_COMMON_H
#define __CLKLLA_COMMON_H

/* ========================================================================
   Name:        clk_pll800_get_rate()
   Description: Convert input/mdiv/ndiv/pvid values to frequency for PLL800
   Params:      'input' freq (Hz), mdiv/ndiv/pvid values
   Output:      '*rate' updated
   Return:      Error code.
   ======================================================================== */

int clk_pll800_get_rate(unsigned long input, unsigned long mdiv,
	unsigned long ndiv, unsigned long pdiv, unsigned long *rate);

/* ========================================================================
   Name:        clk_pll1600_get_rate()
   Description: Convert input/mdiv/ndiv values to frequency for PLL1600
   Params:      'input' freq (Hz), mdiv/ndiv values
		Info: mdiv also called rdiv, ndiv also called ddiv
   Output:      '*rate' updated with value of HS output
   Return:      Error code.
   ======================================================================== */

int clk_pll1600_get_rate(unsigned long input, unsigned long mdiv,
	unsigned long ndiv, unsigned long *rate);

/* ========================================================================
   Name:        clk_pll800_get_params()
   Description: Freq to parameters computation for PLL800
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *mdiv, *ndiv & *pdiv
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_pll800_get_params(unsigned long input, unsigned long output,
	unsigned long *mdiv, unsigned long *ndiv, unsigned long *pdiv);

/* ========================================================================
   Name:        clk_pll1600_get_params()
   Description: Freq to parameters computation for PLL1600
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *mdiv & *ndiv
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_pll1600_get_params(unsigned long input, unsigned long output,
			   unsigned long *mdiv, unsigned long *ndiv);

/* ========================================================================
   Name:        clk_fsyn_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
		WARNING: parameters are HARDWARE CODED values, not the one
		used in formula.
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

int clk_fsyn_get_rate(unsigned long input, unsigned long pe, unsigned long md,
	unsigned long sd, unsigned long *rate);

/* ========================================================================
   Name:        clk_fsyn_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe & *sdiv
		WARNING: parameters are HARDWARE CODED values, not the one
		used in formula.
   Return:      'clk_err_t' error code
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

int clk_fsyn_get_params(unsigned long input, unsigned long output,
	unsigned long *md, unsigned long *pe, unsigned long *sdiv);

#endif /* #ifndef __CLKLLA_COMMON_H */
