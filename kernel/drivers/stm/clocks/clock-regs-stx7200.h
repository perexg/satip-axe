/*****************************************************************************
 *
 * File name   : clock-regs-stx7200.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_H
#define __CLOCK_LLA_REGS_H


/* --- Audio config registers --- */
#define CKGC_FS_CFG(_bk)		(0x100 * (_bk))
#define CKGC_FS_MD(_bk, _chan)		\
	(0x100 * (_bk) + 0x10 + 0x10 * (_chan))
#define CKGC_FS_PE(_bk, _chan)		(0x4 + CKGC_FS_MD(_bk, _chan))
#define CKGC_FS_SDIV(_bk, _chan)	(0x8 + CKGC_FS_MD(_bk, _chan))
#define CKGC_FS_EN_PRG(_bk, _chan)	(0xc + CKGC_FS_MD(_bk, _chan))


#endif  /* End __CLOCK_LLA_REGS_H */
