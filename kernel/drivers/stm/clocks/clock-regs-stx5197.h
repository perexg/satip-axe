/*****************************************************************************
 *
 * File name   : clock-regs-stx5197.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_H
#define __CLOCK_LLA_REGS_H

/* Base addresses */
#define SYSCFG_BASE_ADDRESS			0xFDC00000
#define SYS_SERVICE_ADDR			0xFDC00000

/* Register offsets */
#define DCO_MODE_CFG				0x170
#define FSA_SETUP				0x010
#define FSB_SETUP				0x050
#define FS_DEFAULT_SETUP			0x010
#define FS_ANALOG_POFF				(1 << 3)
#define FS_NOT_RESET				(1 << 4)
#define FS_DIGITAL_PON(id)			(1 << ((id) + 8))

/* Spare clock is recovered in 5197 */
/*
 * the CLK_FS_SETUP(...) manages the:
 * - CLK_SPARE	@	0x014
 * - CLK_PCM	@	0x020
 * - CLK_SPDIF	@	0x030
 * - CLK_DSS	@	0x040
 * - CLK_PIX	@	0x054
 * - CLK_FDM	@	0x060
 * - CLK_AUX	@	0x070
 * - CLK_USB	@	0x080
 */
#define CLK_FS_SETUP(x)		(0x10 * ((x) + 1) + (((x) % 4) == 0 ? 4 : 0))
#define FS_PROG_EN				(1 << 5)
#define FS_SEL_OUT				(1 << 9)
#define FS_OUT_ENABLED				(1 << 11)

#define CAPTURE_COUNTER_PCM      		0x168

#define MODE_CONTROL				0x110
#define   MODE_CTRL_NULL		0x0
#define   MODE_CTRL_X1			0x1
#define   MODE_CTRL_PROG		0x2
#define   MODE_CTRL_STDB		0x3

#define CLK_LOCK_CFG				0x300
#define CLK_OBS_CFG				0x188
#define FORCE_CFG				0x184
#define PLL_SELECT_CFG				0x180
/*
 * PLL_CONFIG
 *  - Bank as [0, 1] to say A or B
 *  - Num  as [0, 1] to sat 0 or 1
 */
#define PLL_CONFIG(bank, num)		(bank * 8 + num * 4)

#define CLKDIV0_CONFIG0				0x090
#define CLKDIV1_CONFIG0				0x0A0
#define CLKDIV2_CONFIG0				0x0AC
#define CLKDIV3_CONFIG0				0x0B8
#define CLKDIV4_CONFIG0				0x0C4
#define CLKDIV6_CONFIG0				0x0D0
#define CLKDIV7_CONFIG0				0x0DC
#define CLKDIV8_CONFIG0				0x0E8
#define CLKDIV9_CONFIG0				0x0F4
#define CLKDIV10_CONFIG0			0x100
#define DYNAMIC_PWR_CONFIG			0x128
#define LOW_PWR_CTRL				0x118
#define LOW_PWR_CTRL1				0x11C

#endif  /* End __CLOCK_LLA_REGS_H */
