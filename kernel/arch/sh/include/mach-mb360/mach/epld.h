/*
 * Copyright (C) 2001 David J. Mckay (david.mckay@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Defintions applicable to the STMicroelectronics ST40RA Eval board.
 */

#ifndef __ASM_SH_MB360_EPLD_H
#define __ASM_SH_MB360_EPLD_H

#define EPLD_BASE		0xa6000000
#define EPLD_SIZE		0x40

#define EPLD_REVID_PLD		(EPLD_BASE+0x00000000)
#define EPLD_REVID_BOARD	(EPLD_BASE+0x00000004)
#define EPLD_GPCR		(EPLD_BASE+0x00000018)
#define EPLD_INTSTAT0		(EPLD_BASE+0x00000020)
#define EPLD_INTSTAT1		(EPLD_BASE+0x00000024)
#define EPLD_INTMASK0		(EPLD_BASE+0x00000028)
#define EPLD_INTMASK0SET	(EPLD_BASE+0x0000002c)
#define EPLD_INTMASK0CLR	(EPLD_BASE+0x00000030)
#define EPLD_INTMASK1		(EPLD_BASE+0x00000034)
#define EPLD_INTMASK1SET	(EPLD_BASE+0x00000038)
#define EPLD_INTMASK1CLR	(EPLD_BASE+0x0000003c)

#ifndef __ASSEMBLY__
extern inline int harp_has_intmask_setclr(void)
{
	return 1;
}

void harp_init_irq(void);
#endif /* !__ASSEMBLY__ */

#endif
