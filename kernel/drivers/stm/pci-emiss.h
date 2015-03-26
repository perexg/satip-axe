/*
 * drivers/stm/pci-emiss.h
 *
 * Defines for the EMISS PCI cell on STMicro devices
 *
 * Copyright 2009 ST Microelectronics (R&D) Ltd.
 * Author: David J. McKay (david.mckay@st.com)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the top level directory for more details.
 *
 */

#ifndef __PCI_EMISS_H__
#define __PCI_EMISS_H__


/* Controls various aspects of PCI. Since concievably other
 * drivers may want to to muck around with this stuff, should
 * this be in a separate driver?
 */

#define EMISS_CONFIG				0x1000
#define EMISS_CONFIG_PCI_CLOCK_MASTER 		0x1
#define EMISS_CONFIG_PCI_HOST_NOT_DEVICE	(0x1 << 5)
#define EMISS_CONFIG_CLOCK_SELECT_MASK 		(0x3 << 1)
#define EMISS_CONFIG_CLOCK_SELECT_PCI		(0x2 << 1)


#define EMISS_ARBITER_CONFIG			0x1004
#define EMISS_ARBITER_CONFIG_SLAVE_NOTMASTER	(1 << 0)
#define EMISS_ARBITER_CONFIG_GRANT_RETRACTION	(1 << 1)
#define EMISS_ARBITER_CONFIG_BYPASS_ARBITER 	(1 << 2)
#define EMISS_ARBITER_CONFIG_MASK_BUS_REQ0 	(1 << 4)
/* Can use req0 with following macro */
#define EMISS_ARBITER_CONFIG_MASK_BUS_REQ(n)	(1 << (4 + (n)))
#define EMISS_ARBITER_CONFIG_BUS_REQ_ALL_MASKED (0xf << 4)
#define EMISS_ARBITER_CONFIG_PCI_NOT_EMI	(1 << 8)
#define EMISS_ARBITER_CONFIG_BUS_FREE		(1 << 9)
#define EMISS_ARBITER_CONFIG_STATIC_NOT_DYNAMIC	(1 << 12)

#define EMISS_INTERFACE_EMI                    0
#define EMISS_INTERFACE_NAND                   1
#define EMISS_INTERFACE_PCI                    2
#define EMISS_INTERFACE_REQ0                   3
#define EMISS_INTERFACE_REQ1                   4
#define EMISS_INTERFACE_REQ2                   5
#define EMISS_INTERFACE_REQ3                   6

#define EMISS_FRAME_LENGTH(n)			(0x1010 + ((n)*0x10))
#define EMISS_HOLDOFF(n)			(0x1014 + ((n)*0x10))
#define EMISS_PRIORITY(n)			(0x1018 + ((n)*0x10))
#define EMISS_BANDWIDTH_LIMIT(n)		(0x101c + ((n)*0x10))

#define PCI_BRIDGE_CONFIG		(0x1400 + 0x000)
#define PCI_BRIDGE_CONFIG_RESET		  0x2
#define PCI_BRIDGE_CONFIG_HOST_NOT_DEVICE 0x1
#define PCI_BRIDGE_CONFIG_WRAP_ENABLE_ALL (0xff << 24)

#define PCI_BRIDGE_INT_DMA_ENABLE	(0x1400 + 0x004)
#define PCI_BRIDGE_INT_DMA_ENABLE_INT_ENABLE (1<<0)
#define PCI_BRIDGE_INT_DMA_ENABLE_INT_UNDEF_FN_ENABLE (1<<24)


#define PCI_BRIDGE_INT_DMA_STATUS	(0x1400 + 0x008)
#define PCI_BRIDGE_INT_DMA_CLEAR	(0x1400 + 0x00c)
#define PCI_TARGID_BARHIT		(0x1400 + 0x010)
#define PCI_INTERRUPT_OUT		(0x1400 + 0x040)
#define PCI_DEVICEINTMASK_INT_ENABLE	(0x1400 + 0x044)
#define PCI_DEVICEINTMASK_INT_STATUS	(0x1400 + 0x048)
#define PCI_DEVICEINTMASK_INT_CLEAR	(0x1400 + 0x04c)
#define PCI_PME_STATUSIN_STATEACK	(0x1400 + 0x080)
#define PCI_POWER_STATE			(0x1400 + 0x084)
#define PCI_PME_ENABLE_CLEAR		(0x1400 + 0x088)
#define PCI_PME_STATUSCHG_INT_ENABLE	(0x1400 + 0x090)
#define PCI_PME_STATUSCHG_INT_STATUS	(0x1400 + 0x094)
#define PCI_PME_STATUSCHG_INT_CLEAR	(0x1400 + 0x098)
#define PCI_PME_STATECHG_INT_ENABLE	(0x1400 + 0x0a0)
#define PCI_PME_STATECHG_INT_STATUS	(0x1400 + 0x0a4)
#define PCI_PME_STATECHG_INT_CLEAR	(0x1400 + 0x0a8)
#define PCI_BUFFADDR_FUNC(fn, m)	(0x1400	+ 0x100 + 0x20*(fn) + 0x04*(m))
#define PCI_FUNC_BUFF_CONFIG(fn) 	(0x1400 + 0x100 + 0x20*(fn) + 0x08)
#define PCI_FUNC_BUFF_CONFIG_ENABLE	(1<<31)
#define PCI_FUNC_BUFF_CONFIG_FUNC_ID(n)	((n)<<8)

#define PCI_FUNC_BUFFDEPTH(fn)		(0x1400 + 0x100 + 0x20*(fn) + 0x0c)
#define PCI_CURADDRPTR_FUNC(fn)		(0x1400 + 0x100 + 0x20*(fn) + 0x10)
#define PCI_FRAME_ADDR			(0x1400 + 0x200)
#define PCI_FRAMEADDR_MASK		(0x1400 + 0x204)
#define PCI_BOOTCFG_ADDR		(0x1400 + 0x300)
#define PCI_BOOTCFG_DATA		(0x1400 + 0x304)
#define PCI_SD_CONFIG			(0x1400 + 0x340)

#define PCI_AD_CONFIG			(0x1400 + 0x344)
#define PCI_AD_CONFIG_THRESHOLD(n)	 	(n)
#define PCI_AD_CONFIG_CHUNKS_IN_MSG(n)	 	((n) << 4)
#define PCI_AD_CONFIG_PCKS_IN_CHUNK(n) 		((n) << 9)
#define PCI_AD_CONFIG_TRIGGER_MODE(n)	 	((n) << 14)
#define PCI_AD_CONFIG_POSTED(n)			((n) << 15)
#define PCI_AD_CONFIG_MAX_OPCODE(n)		((n) << 16)
#define PCI_AD_CONFIG_READ_AHEAD(n)		((n) << 21)

#define PCI_AD_CONFIG_BUSY			(1<<31)


/* NEW BLOCK OF REGISTERS */

#define PCI_CRP_ADDR				0x000
#define PCI_CRP_ADDR_CRP_ADDRESS_MASK		0xff
#define PCI_CRP_ADDR_CRP_FUNCTION_MASK		(0x7<<8)
#define PCI_CRP_ADDR_CRP_COMMAND_MASK		(0xf<<16)
#define PCI_CRP_ADDR_CRP_BYTE_ENABLE_MASK	(0xf<<20) /* ACTIVE LOW!!! */

#define PCI_CRP(addr, func, cmd, be)  ((((addr) & 0xff) << 0) | \
				      (((func) & 0x07) << 8) | \
				      (((cmd) & 0x0f) << 16) | \
				      (((be) & 0x0f)  << 20))

#define PCI_CRP_WR_DATA				0x004
#define PCI_CRP_RD_DATA				0x008
#define PCI_CSR_ADDR				0x00c
#define PCI_CSR_BE_CMD				0x010
#define PCI_CSR_BE_CMD_VAL(cmd, be)	(((be) << 4) | (cmd))

#define PCI_CSR_WR_DATA				0x014
#define PCI_CSR_RD_DATA				0x018
#define PCI_CSR_PCI_ERROR			0x01c
#define PCI_CSR_PCI_ERROR_ADDR			0x020
#define PCI_CSR_AHB_ERROR			0x024
#define PCI_CSR_AHB_ERROR_ADDR			0x028
#define PCI_CSR_FLUSH_PCI_FIFO			0x02c
#define PCI_CSR_TAR				0x030
#define PCI_CSR_MAS_ID				0x034


/* The PCI COMMAND types, the CRP takes these directly */
#define PCI_IO_READ                              0x2
#define PCI_IO_WRITE                             0x3
#define PCI_MEMORY_READ                          0x6
#define PCI_MEMORY_WRITE                         0x7
#define PCI_CONFIG_READ 	                 0xa
#define PCI_CONFIG_WRITE        	         0xb
#define PCI_MEMORY_READ_LINE                     0xe
#define PCI_MEMORY_READ_MULTIPLE                 0xc
#define PCI_MEMORY_WRITE_INVALIDATE              0xf


#endif
