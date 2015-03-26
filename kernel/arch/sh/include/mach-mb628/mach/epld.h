/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Definitions applicable to the STMicroelectronics STx7141 Validation board.
 */

#ifndef __ASM_SH_MB671_EPLD_H
#define __ASM_SH_MB671_EPLD_H

#define EPLD_IDENT		0x010000
#define EPLD_TEST		0x020000
#define EPLD_RESET		0x030000
#define   EPLD_RESET_MII		(1<<5)
#define EPLD_AUDIO		0x040000
#define   EPLD_AUDIO_AUD_SW_CTRL_SHIFT	0
#define   EPLD_AUDIO_AUD_SW_CTRL_MASK	0x0f
#define   EPLD_AUDIO_PCMDAC1_SMUTE	(1<<4)
#define   EPLD_AUDIO_PCMDAC2_SMUTE	(1<<5)
#define   EPLD_AUDIO_DIGAUD_NOTRESET	(1<<6)
#define EPLD_FLASH		0x050000
#define   EPLD_FLASH_NOTWP		(1<<0)
#define   EPLD_FLASH_NOTRESET		(1<<1)
#define EPLD_IEEE		0x060000
#define EPLD_ENABLE		0x070000
#define   EPLD_ASC1_EN			(1<<0)
#define   EPLD_ASC2_EN			(1<<1)
#define   EPLD_ENABLE_HBEAT		(1<<2)
#define   EPLD_ENABLE_SPI_NOTCS		(1<<3)
#define   EPLD_ENABLE_IFE_NOTCS		(1<<4)
#define   EPLD_ENABLE_MII1		(1<<5)
#define   EPLD_ENABLE_MII0		(1<<6)
#define EPLD_CCARDCTRL		0x080000
#define EPLD_CCARDCTRL2		0x090000
#define EPLD_CCARDIMDIMODE	0x0A0000
#define EPLD_CCARDTS3INMODE	0x0B0000
#define EPLD_STATUS		0x0C0000

#endif
