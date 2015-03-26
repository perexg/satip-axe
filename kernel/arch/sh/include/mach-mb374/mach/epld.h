/*
 * Copyright (C) 2001 David J. Mckay (david.mckay@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Defintions applicable to the STMicroelectronics ST40RA Starter Board.
 */

#ifndef __ASM_SH_MB374_EPLD_H
#define __ASM_SH_MB374_EPLD_H

#define EPLD_BASE		0xa7000000
#define EPLD_SIZE		0x34

#define EPLD_REVID		(EPLD_BASE+0x00000000)
#define EPLD_RESET		(EPLD_BASE+0x00000004)
#define EPLD_LED_SET		(EPLD_BASE+0x00000008)
#define EPLD_LED_CLR		(EPLD_BASE+0x0000000c)
#define EPLD_VPP		(EPLD_BASE+0x00000010)
#define EPLD_INTMASK0		(EPLD_BASE+0x00000014)
#define EPLD_INTMASK0SET	(EPLD_BASE+0x00000018)
#define EPLD_INTMASK0CLR	(EPLD_BASE+0x0000001c)
#define EPLD_INTMASK1		(EPLD_BASE+0x00000020)
#define EPLD_INTMASK1SET	(EPLD_BASE+0x00000024)
#define EPLD_INTMASK1CLR	(EPLD_BASE+0x00000028)
#define EPLD_INTSTAT0		(EPLD_BASE+0x0000002c)
#define EPLD_INTSTAT1		(EPLD_BASE+0x00000030)

#define EPLD_LED_ON   1
#define EPLD_LED_OFF  0

#ifndef __ASSEMBLY__
extern inline int harp_has_intmask_setclr(void)
{
	return 1;
}

extern inline void harp_set_vpp_on(void)
{
	ctrl_outl(1, EPLD_VPP);
}

extern inline void harp_set_vpp_off(void)
{
	ctrl_outl(0, EPLD_VPP);
}

void harp_init_irq(void);
#endif /* !__ASSEMBLY__ */

#endif
