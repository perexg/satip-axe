/*
 *   STMicroelectronics NAND Controller
 *
 *   See ADCS #7864584: "NAND Flash support upgrades for FMI Functional
 *   Secification".
 *
 *   Copyright (c) 2008 STMicroelectronics Limited
 *   Author: Angus Clark <angus.clark@st.com>
 *
 *   May be copied or modified under the terms of the GNU General Public
 *   License.  See linux/COPYING for more information.
 *
 */

#ifndef STM_NANDC_REGS_H
#define STM_NANDC_REGS_H

#define EMINAND_CONFIG_SIZE			0x1000

/* Register Addresses (OFFSET from EMINAND_CONFIG_BASE) */
#define EMINAND_BOOTBANK_CONFIG			0x000
#define EMINAND_RBN_STATUS			0x004
#define EMINAND_INTERRUPT_ENABLE		0x010
#define EMINAND_INTERRUPT_STATUS		0x014
#define EMINAND_INTERRUPT_CLEAR			0x018
#define EMINAND_INTERRUPT_EDGECONFIG		0x01C
#define EMINAND_CONTROL_TIMING			0x040
#define EMINAND_WEN_TIMING			0x044
#define EMINAND_REN_TIMING			0x048
#define EMINAND_FLEXMODE_CONFIG			0x100
#define EMINAND_MUXCONTROL_REG			0x104
#define EMINAND_CSN_ALTERNATE			0x108
#define EMINAND_FLEX_DATAWRITE_CONFIG		0x10C
#define EMINAND_FLEX_DATAREAD_CONFIG		0x110
#define EMINAND_FLEX_COMMAND_REG		0x114
#define EMINAND_FLEX_ADDRESS_REG		0x118
#define EMINAND_FLEX_DATA			0x120
#define EMINAND_VERSION_REG			0x144
#define EMINAND_MULTI_CS_CONFIG_REG		0x1EC


/* Advanced Flex Mode registers (OFFSET from EMINAND_CONFIG_BASE) */
#define EMINAND_AFM_SEQUENCE_REG_1		0x200
#define EMINAND_AFM_SEQUENCE_REG_2		0x204
#define EMINAND_AFM_SEQUENCE_REG_3		0x208
#define EMINAND_AFM_SEQUENCE_REG_4		0x20C
#define EMINAND_AFM_ADDRESS_REG			0x210
#define EMINAND_AFM_EXTRA_REG			0x214
#define EMINAND_AFM_COMMAND_REG			0x218
#define EMINAND_AFM_SEQUENCE_CONFIG_REG		0x21C
#define EMINAND_AFM_GENERIC_CONFIG_REG		0x220
#define EMINAND_AFM_SEQUENCE_STATUS_REG		0x240
#define EMINAND_AFM_ECC_CHECKCODE_REG_0		0x280
#define EMINAND_AFM_ECC_CHECKCODE_REG_1		0x284
#define EMINAND_AFM_ECC_CHECKCODE_REG_2		0x288
#define EMINAND_AFM_ECC_CHECKCODE_REG_3		0x28C
#define EMINAND_AFM_DATA_FIFO			0x300

/* AFM Commands */
#define AFM_STOP				0x0
#define AFM_CMD					0x1
#define AFM_INC					0x2
#define AFM_DEC_JUMP				0x3
#define AFM_DATA				0x4
#define AFM_SPARE				0x5
#define AFM_CHECK				0x6
#define AFM_ADDR				0x7
#define AFM_WRBN				0xA

/* FLEX: Address Register Fields */
#define FLX_ADDR_REG_RBN			(0x1 << 27)
#define FLX_ADDR_REG_BEAT_1			(0x1 << 28)
#define FLX_ADDR_REG_BEAT_2			(0x2 << 28)
#define FLX_ADDR_REG_BEAT_3			(0x3 << 28)
#define FLX_ADDR_REG_BEAT_4			(0x0 << 28)
#define FLX_ADDR_REG_ADD8_VALID			(0x1 << 30)
#define FLX_ADDR_REG_CSN_STATUS			(0x1 << 31)

/* FLEX: Commad Register fields */
#define FLX_CMD_REG_RBN				(0x1 << 27)
#define FLX_CMD_REG_BEAT_1			(0x1 << 28)
#define FLX_CMD_REG_BEAT_2			(0x2 << 28)
#define FLX_CMD_REG_BEAT_3			(0x3 << 28)
#define FLX_CMD_REG_BEAT_4			(0x0 << 28)
#define FLX_CMD_REG_CSN_STATUS			(0x1 << 31)
#define FLX_CMD(x)				(((x) & 0xff) |		\
						 FLX_CMD_REG_RBN |	\
						 FLX_CMD_REG_BEAT_1 |	\
						 FLX_CMD_REG_CSN_STATUS)

/* FLEX: Data Config fields */
#define FLX_DATA_CFG_RBN			(0x1 << 27)
#define FLX_DATA_CFG_BEAT_1			(0x1 << 28)
#define FLX_DATA_CFG_BEAT_2			(0x2 << 28)
#define FLX_DATA_CFG_BEAT_3			(0x3 << 28)
#define FLX_DATA_CFG_BEAT_4			(0x0 << 28)
#define FLX_DATA_CFG_BYTES_1			(0x0 << 30)
#define FLX_DATA_CFG_BYTES_2			(0x1 << 30)
#define FLX_DATA_CFG_CSN_STATUS			(0x1 << 31)

/* AFM: Sequence Config fields */
#define AFM_SEQ_CFG_GO				(0x1 << 26)
#define AFM_SEQ_CFG_DIR_WRITE			(0x1 << 24)



#endif /* STM_NANDC_REGS_H */
