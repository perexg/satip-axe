/*
 * HCD (Host Controller Driver) for USB.
 *
 * Copyright (c) 2009 STMicroelectronics Limited
 * Author: Francesco Virlinzi
 *
 * Bus Glue for STMicroelectronics STx710x devices.
 *
 * This file is licenced under the GPL.
 */
#ifndef __ST_USB_HCD__
#define __ST_USB_HCD__

/* The transaction opcode is programmed in this register */
#define AHB2STBUS_STBUS_OPC_OFFSET      0x00    /* From PROTOCOL_BASE */
#define AHB2STBUS_STBUS_OPC_4BIT        0x00
#define AHB2STBUS_STBUS_OPC_8BIT        0x01
#define AHB2STBUS_STBUS_OPC_16BIT       0x02
#define AHB2STBUS_STBUS_OPC_32BIT       0x03
#define AHB2STBUS_STBUS_OPC_64BIT       0x04

/* The message length in number of packets is programmed in this register. */
#define AHB2STBUS_MSGSIZE_OFFSET        0x04    /* From PROTOCOL_BASE */
#define AHB2STBUS_MSGSIZE_DISABLE       0x0
#define AHB2STBUS_MSGSIZE_2             0x1
#define AHB2STBUS_MSGSIZE_4             0x2
#define AHB2STBUS_MSGSIZE_8             0x3
#define AHB2STBUS_MSGSIZE_16            0x4
#define AHB2STBUS_MSGSIZE_32            0x5
#define AHB2STBUS_MSGSIZE_64            0x6

/* The chunk size in number of packets is programmed in this register */
#define AHB2STBUS_CHUNKSIZE_OFFSET      0x08    /* From PROTOCOL_BASE */
#define AHB2STBUS_CHUNKSIZE_DISABLE     0x0
#define AHB2STBUS_CHUNKSIZE_2           0x1
#define AHB2STBUS_CHUNKSIZE_4           0x2
#define AHB2STBUS_CHUNKSIZE_8           0x3
#define AHB2STBUS_CHUNKSIZE_16          0x4
#define AHB2STBUS_CHUNKSIZE_32          0x5
#define AHB2STBUS_CHUNKSIZE_64          0x6

#define AHB2STBUS_TIMEOUT		0x0c

#define AHB2STBUS_SW_RESET		0x10

/* Wrapper Glue registers */

#define AHB2STBUS_STRAP_OFFSET          0x14    /* From WRAPPER_GLUE_BASE */
#define AHB2STBUS_STRAP_PLL             0x08    /* undocumented */
#define AHB2STBUS_STRAP_8_BIT           0x00    /* ss_word_if */
#define AHB2STBUS_STRAP_16_BIT          0x04    /* ss_word_if */


/* Extensions to the standard USB register set */

/* Define a bus wrapper IN/OUT threshold of 128 */
#define AHB2STBUS_INSREG01_OFFSET       (0x10 + 0x84) /* From EHCI_BASE */
#define AHB2STBUS_INOUT_THRESHOLD       0x00800080

#include <linux/clk.h>
#include "../core/hcd.h"

#define USB_CLKS_NR			3

struct drv_usb_data {
	/*
	 * USB-IP needs 2 clocks:
	 * - a 48 MHz oscillator (to generate a final 480 MHz)
	 * - a 100 MHz oscillator (for the NI)
	 * - an oscillator for Phy
	 * other clocks are generated internally using
	 * direclty the  external oscillator
	 */
	struct clk *clks[USB_CLKS_NR];
	void *ahb2stbus_wrapper_glue_base;
	void *ahb2stbus_protocol_base;
	struct platform_device *ehci_device;
	struct platform_device *ohci_device;
	struct stm_device_state *device_state;
};

#ifdef CONFIG_PM_RUNTIME
int stm_ehci_hcd_register(struct platform_device *);
int stm_ehci_hcd_unregister(struct platform_device *);

int stm_ohci_hcd_register(struct platform_device *);
int stm_ohci_hcd_unregister(struct platform_device *);
#endif
#endif
