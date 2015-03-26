/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __ASM_SH_MB519_STEM_H
#define __ASM_SH_MB519_STEM_H

#include <asm/irq-ilc.h>

#define STEM_CS0_BANK 1
#define STEM_CS0_OFFSET 0

#define STEM_CS1_BANK 1
#define STEM_CS1_OFFSET (1 << 23)

#define STEM_INTR0_IRQ ILC_IRQ(6)

/* STEM INTR1 cannot be used on this board - see comments in
 * arch/sh/boards/st/mb519/setup.c, function mb519_init_irq(). */
#undef STEM_INTR1_IRQ

#endif
