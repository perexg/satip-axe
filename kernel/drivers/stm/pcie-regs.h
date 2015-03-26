/*
 * drivers/stm/pcie-regs.h
 *
 * Registers for STMicro PCIe hardware
 *
 * Copyright 2010 ST Microelectronics (R&D) Ltd.
 * Author: David J. McKay (david.mckay@st.com)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the top level directory for more details.
 *
 */

#ifndef __PCIE_REGS_H__
#define __PCIE_REGS_H__

#define TRANSLATION_CONTROL		0x900
/* No effect in RC mode */
#define EP_TRANSLATION_ENABLE		(1<<0)
/* Controls if area is inclusive or exclusive */
#define RC_PASS_ADDR_RANGE		(1<<1)

/* Reserved in RC mode */
#define PIM0_MEM_ADDR_START		0x910
#define PIM1_MEM_ADDR_START		0x914
#define PIM2_MEM_ADDR_START		0x918

/* Base of area reserved for config accesses. Fixed
 * size of 64K.
 */
#define CFG_BASE_ADDRESS		0x92c

#define CFG_REGION_SIZE			65536

/* The PCIe capability registers start at this offset in configuration
 * space. Strictly speaking we should follow the caps linked list pointer,
 * but there seems little point since this is fixed
 */
#define CFG_PCIE_CAP			0x70

/* First 4K of config space has this BDF (bus,device,function) */
#define FUNC0_BDF_NUM			0x930

/* There are other registers to control function 1 etc, split up into
 * 4K chunks. I cannot see any use for these, it is simpler to always
 * use FUNC0 and reprogram it as needed to drive the appropriate config
 * cycle
 */
#define FUNC_BDF_NUM(x)			(0x930 + (((x) / 2) * 4))

/* Reserved in RC mode */
#define POM0_MEM_ADDR_START		0x960
/* Start address of region 0 to be blocked/passed */
#define IN0_MEM_ADDR_START		0x964
/* End address of region 0 to be blocked/passed */
#define IN0_MEM_ADDR_LIMIT		0x968

/* Reserved in RC mode */
#define POM1_MEM_ADDR_START		0x970
/* Start address of region 1 to be blocked/passed */
#define IN1_MEM_ADDR_START		0x974
/* End address of region 1 to be blocked/passed */
#define IN1_MEM_ADDR_LIMIT		0x978

/* MSI registers */
#define MSI_ADDRESS			0x820
#define MSI_UPPER_ADDRESS		0x824
#define MSI_OFFSET_REG(n) 		((n) * 0xc)
#define MSI_INTERRUPT_ENABLE(n)		(0x828 + MSI_OFFSET_REG(n))
#define MSI_INTERRUPT_MASK(n)		(0x82c + MSI_OFFSET_REG(n))
#define MSI_INTERRUPT_STATUS(n)		(0x830 + MSI_OFFSET_REG(n))
#define MSI_GPIO_REG			0x888
#define MSI_NUM_ENDPOINTS		8


/* This actually containes the LTSSM state machine state */
#define PORT_LOGIC_DEBUG_REG_0		0x728

/* LTSSM state machine values 	*/
#define DEBUG_REG_0_LTSSM_MASK		0x1f
#define S_DETECT_QUIET			0x00
#define S_DETECT_ACT			0x01
#define S_POLL_ACTIVE			0x02
#define S_POLL_COMPLIANCE		0x03
#define S_POLL_CONFIG			0x04
#define S_PRE_DETECT_QUIET		0x05
#define S_DETECT_WAIT			0x06
#define S_CFG_LINKWD_START		0x07
#define S_CFG_LINKWD_ACEPT		0x08
#define S_CFG_LANENUM_WAIT		0x09
#define S_CFG_LANENUM_ACEPT		0x0A
#define S_CFG_COMPLETE			0x0B
#define S_CFG_IDLE			0x0C
#define S_RCVRY_LOCK			0x0D
#define S_RCVRY_SPEED			0x0E
#define S_RCVRY_RCVRCFG			0x0F
#define S_RCVRY_IDLE			0x10
#define S_L0				0x11
#define S_L0S				0x12
#define S_L123_SEND_EIDLE		0x13
#define S_L1_IDLE			0x14
#define S_L2_IDLE			0x15
#define S_L2_WAKE			0x16
#define S_DISABLED_ENTRY		0x17
#define S_DISABLED_IDLE			0x18
#define S_DISABLED			0x19
#define S_LPBK_ENTRY			0x1A
#define S_LPBK_ACTIVE			0x1B
#define S_LPBK_EXIT			0x1C
#define S_LPBK_EXIT_TIMEOUT		0x1D
#define S_HOT_RESET_ENTRY		0x1E
#define S_HOT_RESET			0x1F

#endif  /* __PCIE_REGS_H__ */
