/*
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Definitions applicable to the STMicroelectronics STb7100 Validation board.
 */

#ifndef __ASM_SH_MB411_EPLD_H
#define __ASM_SH_MB411_EPLD_H

#define EPLD_BASE		0x03000000
#define EPLD_SIZE		0x00E00000

#define EPLD_EPLDVER		0x000000
#define EPLD_PCBVER		0x020000
#define EPLD_STEM		0x040000
#define EPLD_DRIVER		0x060000
#define EPLD_RESET		0x080000
#define EPLD_INTSTAT0		0x0a0000
#define EPLD_INTSTAT1		0x0c0000
#define EPLD_INTMASK0		0x0e0000
#define EPLD_INTMASK0SET	0x100000
#define EPLD_INTMASK0CLR	0x120000
#define EPLD_INTMASK1		0x140000
#define EPLD_INTMASK1SET	0x160000
#define EPLD_INTMASK1CLR	0x180000
#define EPLD_TEST		0x1e0000

#define EPLD_FLASH		0x400000
#define EPLD_ATAPI		0x900000

#define EPLD_DAC_CTRL		0xa00000
#define EPLD_DAC_PNOTS		0xb00000
#define EPLD_DAC_SPMUX		0xd00000

/* Some registers are also available in the POD EPLD */
#define EPLD_POD_BASE		0x02100000
#define EPLD_POD_REVID		0x00
#define EPLD_POD_LED		0x10
#define EPLD_POD_DEVID		0x1c

#define EPLD_LED_ON     1
#define EPLD_LED_OFF    0

#endif
