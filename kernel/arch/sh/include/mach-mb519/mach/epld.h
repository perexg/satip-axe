/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Definitions applicable to the STMicroelectronics STb7100 Validation board.
 */

#ifndef __ASM_SH_MB519_EPLD_H
#define __ASM_SH_MB519_EPLD_H

#define EPLD_BASE	0x05000000
#define EPLD_SIZE	0x01000000

#define EPLD_EPLDVER		0x000000
#define EPLD_PCBVER		0x020000
#define EPLD_STEM		0x040000
#define EPLD_DRIVER		0x060000
#define EPLD_RESET		0x080000
#define EPLD_INTSTAT0		0x0A0000
#define EPLD_INTSTAT1		0x0C0000
#define EPLD_INTMASK0		0x0E0000
#define EPLD_INTMASK0SET	0x100000
#define EPLD_INTMASK0CLR	0x120000
#define EPLD_INTMASK1		0x140000
#define EPLD_INTMASK1SET	0x160000
#define EPLD_INTMASK1CLR	0x180000
#define EPLD_LEDSTDADDR		0x1A0000

#define EPLD_FLASH		0x400000
#define EPLD_STEM2		0x500000
#define EPLD_STEMSET		0x600000
#define EPLD_STEMCLR		0x700000
#define EPLD_DACSPMUX		0xD00000

#endif
