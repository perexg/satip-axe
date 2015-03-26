/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __ASM_SH_MB680_STEM_H
#define __ASM_SH_MB680_STEM_H

#include <asm/irq-ilc.h>

/*
 * If used standalone STEM CS0 = BANK2. In this mode
 * need to set J14A to 1-2 (notStemCS(0) <= notEMICSC) and
 * J4 to 1-2 and fit J2A (notStemIntr(0) <= SysIRQ2).
 *
 * If used with mb705 STEMCS0 is routed via the EPLD (J14A in position 2-3)
 * which subdecodes STEMCS0 as CSC (bank 2) and A[25:24] != 00 (the EPLDs
 * occupy this same bank). Similarly StemIntr(0) is routed via the EPLD,
 * which we program up to route it directly to SysIRQ2, see mb705_init()
 * for more details
 */
#ifdef CONFIG_SH_ST_MB705
#define STEM_CS0_BANK 2
#define STEM_CS0_OFFSET (1<<24)
#else
#define STEM_CS0_BANK 2
#define STEM_CS0_OFFSET 0
#endif

/* STEM CS1 = BANK3 */
/* Need to set J14B to 1-2 (notStemCS(1) <= notEMICSD) and
 * fit J2B (notStemIntr(1) <= SysIRQ1) if mb680 used
 * standalone. */
#define STEM_CS1_BANK 3
#define STEM_CS1_OFFSET 0

#define STEM_INTR0_IRQ ILC_EXT_IRQ(2)
#define STEM_INTR1_IRQ ILC_EXT_IRQ(1)

#endif
