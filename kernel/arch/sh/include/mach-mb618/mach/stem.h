/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __ASM_SH_MB618_STEM_H
#define __ASM_SH_MB618_STEM_H

#include <asm/irq-ilc.h>

/* STEM CS0 = BANK1 (notCSB). This assumes J30-B is in the 4-5 position */
/* Note R100 needs to be fitted */
#define STEM_CS0_BANK 1
#define STEM_CS0_OFFSET 0

/* STEM CS1 = BANK3 (notCSD). This assumes J11 is in the 1-2 position. */
/* Note R109 needs to be fitted */
#define STEM_CS1_BANK 3
#define STEM_CS1_OFFSET 0

#define STEM_INTR0_IRQ ILC_EXT_IRQ(2)
#define STEM_INTR1_IRQ ILC_EXT_IRQ(1)

#endif
