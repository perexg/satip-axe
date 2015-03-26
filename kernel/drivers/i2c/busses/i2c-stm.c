/*
 * --------------------------------------------------------------------
 *
 * i2c-stm.c
 * i2c algorithms for STMicroelectronics device
 * Version: 2.0 (1 April 2007)
 * Version: 2.0.1 (20 Dec 2007)
 *   + Removed the ssc layer.
 * Version: 2.1 (3 Jan 2008)
 *   + Added the timing glitch suppression support
 * Version: 2.2 (25th Apr 2008)
 *   + Added hardware glitch filter support
 *   + Improved reset and register configuration
 *   + Switch to using FIFO mode all the time
 *   + Use standard I2C clock settings
 *   + Adjust baud rate to give true 100/400 kHz operation
 *   + Fix bug in clock rounding
 *   + Use stpio_request_set_pin on init to set SDA and SCL to high
 *   + Fix compile-time warnings
 * Version: 2.3 (9th Mar 2008)
 *   + Added manual PIO reset on arbitration/slave mode detection
 *   + Ensure that clock stretching is always cleared (even in 1 byte read)
 *   + Check for an unexpected stop condition on the bus
 *   + Fix auto-retry on address error condition
 *   + Clear repstart bit
 * Version 2.4 (23rd Jul 2008)
 *   + Remove clock stretch check
 *   + Improve manual PIO reset
 *   + Add delay after stop to ensure I2C tBUF satisfied
 *   + Clear SSC status after reset
 *   + Reorder TX & I2C config register pokes in prepare to read phase
 * Version 2.5 (24th Oct 2008)  Carl Shaw <carl.shaw@st.com>
 *   + Rewrite START state - in case of write it now preloads TX FIFO
 *                           and it is also shared for REPSTART address
 *   + Change RX prepare - checks for NACK, reads address and then falls
 *                         through to read state
 *   + Change RX state - Use TEEN interrupt to reduce unnecessary interrupt
 *                       loading.  Previously, RIR was used which generated
 *                       an interrupt per byte
 *   + Change TX state - correctly unload RX FIFO (using both FIFO status
 *                       AND RIR bit)
 *                     - use TEEN interrupt rather than TIR
 *   + Change repstart address - just call START state but suppress STARTG
 *   + Only use PIO recovery as last-ditch effort
 *   + Fix retry method
 *   + Add auto retry in case of arbitration problem
 *   + Allow a PIO mode for clock of push-pull rather than BIDIR.  This
 *     can help with spurious noise glitches on the SCK line and in the
 *     case where SCK rise times are marginal due to capacitance on the
 *     bus.
 * Version 2.6 (1 Apr 2010) Francesco Virlinzi <francesco.virlinzi@st.com>
 *   + Added the new state (IDLE) in the i2c fsm.
 *		Now each transaction completed with a STOP condition
 *		leaves the SSC in I2C-Slave more. In this state the SSC
 *		seems less sensible to the noise on the bus.
 * Version 2.7  (01 Jun 2010) Bruno Strudel <bruno.strudel@st.com>
 *   + Fixed the IIC_FSM_REPSTART_ADDR to wait in case of clock stretching
 *
 * --------------------------------------------------------------------
 *
 *  Copyright (C) 2006 - 2010 STMicroelectronics
 *  Author: Francesco Virlinzi     <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License version 2.0 ONLY.  See linux/COPYING for more information.
 *
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/stm/platform.h>
#include <linux/stm/ssc.h>
#include "i2c-stm.h"

#undef dbg_print

#ifdef CONFIG_I2C_DEBUG_BUS
#define dbg_print(fmt, args...)  \
	printk(KERN_DEBUG "%s: " fmt, __func__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

#undef dbg_print2
#ifdef CONFIG_I2C_DEBUG_ALGO
#define dbg_print2(fmt, args...)  \
	printk(KERN_DEBUG "%s: " fmt, __func__ , ## args)
#else
#define dbg_print2(fmt, args...)
#endif

/* --- Defines for I2C --- */
/* These values WILL produce physical clocks which are slower */
/* Especially if hardware glitch suppression is enabled       */
/* They should probably be made board dependent?              */
#define I2C_RATE_NORMAL			100000
#define I2C_RATE_FASTMODE		400000

#define NANOSEC_PER_SEC			1000000000

/* Standard I2C timings */
#define REP_START_HOLD_TIME_NORMAL	4000
#define START_HOLD_TIME_NORMAL		4000
#define REP_START_SETUP_TIME_NORMAL	4700
#define DATA_SETUP_TIME_NORMAL		 250
#define STOP_SETUP_TIME_NORMAL		4000
#define BUS_FREE_TIME_NORMAL		4700

#define REP_START_HOLD_TIME_FAST	 600
#define START_HOLD_TIME_FAST		 600
#define REP_START_SETUP_TIME_FAST	 600
#define DATA_SETUP_TIME_FAST		 100
#define STOP_SETUP_TIME_FAST		 600
#define BUS_FREE_TIME_FAST		1300

/* Define for glitch suppression support */
#ifdef CONFIG_I2C_STM_GLITCH_SUPPORT
#if CONFIG_GLITCH_CLK_WIDTH > 0
#define GLITCH_WIDTH_CLOCK			CONFIG_GLITCH_CLK_WIDTH
#else
#define GLITCH_WIDTH_CLOCK			500	/* in nanosecs */
#endif
#if CONFIG_GLITCH_DATA_WIDTH > 0
#define GLITCH_WIDTH_DATA			CONFIG_GLITCH_DATA_WIDTH
#else
#define GLITCH_WIDTH_DATA			500	/* in nanosecs */
#endif
#else
#define GLITCH_WIDTH_DATA			0
#define GLITCH_WIDTH_CLOCK			0
#endif

#ifdef CONFIG_I2C_STM_HW_GLITCH
#if CONFIG_HW_GLITCH_WIDTH > 0
#define HW_GLITCH_WIDTH			CONFIG_HW_GLITCH_WIDTH
#else
#define HW_GLITCH_WIDTH			1	/* in microseconds */
#endif
#endif

/* To manage normal vs fast mode */
#define IIC_STM_CONFIG_SPEED_MASK          0x1
#define IIC_STM_CONFIG_SPEED_FAST          0x1
#define IIC_STM_READY_SPEED_MASK	   0x2
#define IIC_STM_READY_SPEED_FAST	   0x2

enum iic_state_machine {
	IIC_FSM_VOID = 0,
	IIC_FSM_IDLE,
	IIC_FSM_PREPARE,
	IIC_FSM_NOREPSTART,
	IIC_FSM_START,
	IIC_FSM_DATA_WRITE,
	IIC_FSM_PREPARE_2_READ,
	IIC_FSM_DATA_READ,
	IIC_FSM_STOP,
	IIC_FSM_COMPLETE,
	IIC_FSM_REPSTART,
	IIC_FSM_REPSTART_ADDR,
	IIC_FSM_ABORT
};

#ifdef CONFIG_I2C_DEBUG_ALGO
char *statename[] = {
	"VOID", "PREPARE", "START", "DATA_WRITE", "PREPARE_2_READ",
	"DATA_READ", "STOP", "COMPLETE", "REPSTART", "REPSTART_ADDR", "ABORT"
};
#endif

enum iic_fsm_error {
	IIC_E_NO_ERROR = 0x0,
	IIC_E_RUNNING = 0x1,
	IIC_E_NOTACK = 0x2,
	IIC_E_ARBL = 0x4,
	IIC_E_BUSY = 0x8
};

/*
 * With the struct iic_transaction more information
 * on the required transaction are moved on
 * the thread stack instead of (iic_ssc) adapter descriptor...
 */
struct iic_transaction {
	enum iic_state_machine start_state;
	enum iic_state_machine state;
	enum iic_state_machine next_state;
	struct i2c_msg *msgs_queue;
	int attempt;
	int queue_length;
	int current_msg;	/* the message on going */
	int idx_current_msg;	/* the byte in the message */
	enum iic_fsm_error status_error;
	int waitcondition;
};

struct iic_ssc {
	void __iomem *base;
	struct clk *clk;
	struct iic_transaction *trns;
	struct i2c_adapter adapter;
	unsigned long config;
	wait_queue_head_t wait_queue;
	struct stm_pad_state *pad_state;
};

#define jump_on_fsm_start(x)	{ (x)->state = IIC_FSM_START;	\
				goto be_fsm_start;	}

#define jump_on_fsm_stop(x)	do { (x)->state = IIC_FSM_STOP;    \
				goto be_fsm_stop; } while (0)

#define jump_on_fsm_abort(x)	do { (x)->state = IIC_FSM_ABORT; \
				goto be_fsm_abort;	} while (0)

#define jump_on_fsm_complete(x)	{ (x)->state = IIC_FSM_STOP;    \
				     goto be_fsm_complete; }

#define check_fastmode(adap)	\
	(!!((adap)->config & IIC_STM_CONFIG_SPEED_MASK))

#define check_ready_fastmode(adap)	\
	(!!((adap)->config & IIC_STM_READY_SPEED_FAST))

#define set_ready_fastmode(adap) ((adap)->config |= IIC_STM_READY_SPEED_FAST)

#define clear_ready_fastmode(adap) ((adap)->config &= ~IIC_STM_READY_SPEED_FAST)

#if defined(CONFIG_I2C_STM_NOSTOP_API)
#define LAST_I2C_WAS_NO_STOP		0x4
#define LAST_I2C_WAS_NO_STOP_MASK	0x4
#define check_lasti2cwas_nostop(adap)	(((adap)->config & \
					LAST_I2C_WAS_NO_STOP_MASK) ? 1 : 0 )
#define set_lasti2cwas_nostop(adap)	((adap)->config |=  LAST_I2C_WAS_NO_STOP)
#define clear_lasti2cwas_nostop(adap)	((adap)->config &= ~LAST_I2C_WAS_NO_STOP_MASK)
#else
#define check_lasti2cwas_nostop(adap)	(1==0)
#define set_lasti2cwas_nostop(adap)	{}
#define clear_lasti2cwas_nostop(adap)	{}
#endif


static void iic_stm_setup_timing(struct iic_ssc *adap);

static irqreturn_t iic_state_machine(int this_irq, void *data)
{
	struct iic_ssc *adap = (struct iic_ssc *)data;
	struct iic_transaction *trsc = adap->trns;
	unsigned int status;
	unsigned int idx, previntmask, lim, intflags, conflags = 0;
#ifdef CONFIG_I2C_DEBUG_ALGO
	unsigned int cw;
#endif
	unsigned short address;
	struct i2c_msg *pmsg;
	char fast_mode;
	union {
		char bytes[2];
		unsigned short word;
	} tmp;
	unsigned short txbuff[SSC_TXFIFO_SIZE];
	unsigned int txbuffcount;
	unsigned int cnt = 0;

	fast_mode = check_fastmode(adap);
	pmsg = trsc->msgs_queue + trsc->current_msg;

	/* Disable interrupts */
	previntmask = ssc_load32(adap, SSC_IEN);
	ssc_store32(adap, SSC_IEN, 0);

	status = ssc_load32(adap, SSC_STA);
	dbg_print2("ISR status = 0x%08x\n", status);

	/*
	 * Slave mode detection
	 * this should never happen as we don't support multi-master
	 */
	if (trsc->state > IIC_FSM_START && ((status & SSC_STA_ARBL)
					    || !(ssc_load32(adap, SSC_CTL) &
						 SSC_CTL_MS))) {
		dbg_print2("SLAVE mode (state %d, status %08x)\n",
			   trsc->state, status);
		dbg_print2(" Message [%d of %d] is %s address 0x%02x bus %d\n",
			   trsc->current_msg + 1, trsc->queue_length,
			   (pmsg->flags & I2C_M_RD) ? "READ from" : "WRITE to",
			   pmsg->addr, adap->adapter.nr);
		dbg_print2
		    ("   data is %d bytes, currently at %d, last 0x%02x\n",
		     pmsg->len, trsc->idx_current_msg,
		     (trsc->idx_current_msg >
		      0) ? pmsg->buf[trsc->idx_current_msg - 1] : 0);
		dbg_print2("Prev State: %s Changing to state: %s \n",
			   statename[trsc->state], statename[trsc->next_state]);

		dbg_print2
		    ("Status: 0x%08x SSC_IEN 0x%08x "
			"SSC_CTL 0x%08x SSC_I2C 0x%08x\n",
		     status, previntmask, ssc_load32(adap, SSC_CTL),
		     ssc_load32(adap, SSC_I2C));

		trsc->status_error = IIC_E_ARBL;
		ssc_store32(adap, SSC_CLR, 0xdc0);
		trsc->waitcondition = 0;
		wake_up(&(adap->wait_queue));
		return IRQ_HANDLED;
	}

	trsc->state = trsc->next_state;

	switch (trsc->state) {
	case IIC_FSM_PREPARE:
		dbg_print2("-Prepare\n");
		if (check_lasti2cwas_nostop(adap)) {
		  clear_lasti2cwas_nostop(adap);
			/* repstart */
			dbg_print(" STOP - REPSTART\n");
			trsc->next_state = IIC_FSM_REPSTART_ADDR;
			ssc_store32(adap, SSC_I2C,
				    SSC_I2C_I2CM |
				    SSC_I2C_TXENB |
				    SSC_I2C_REPSTRTG);
			ssc_store32(adap, SSC_IEN,
				    SSC_IEN_REPSTRTEN |
				    SSC_IEN_ARBLEN);
			break;
		}
		/*
		 * check if the i2c timing register
		 * of ssc are ready to use
		 */
		if ((check_fastmode(adap) && !check_ready_fastmode(adap)) ||
		    (!check_fastmode(adap) && check_ready_fastmode(adap)))
			iic_stm_setup_timing(adap);

		trsc->start_state = IIC_FSM_START;

		/* Enable RX FIFO, enable clock stretch on TX empty */
		ssc_store32(adap, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
			    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8 |
			    SSC_CTL_EN_RX_FIFO | SSC_CTL_EN_TX_FIFO);

		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM);

		/* NO break! */

	case IIC_FSM_NOREPSTART:
		ssc_store32(adap, SSC_CLR, 0xdc0);
		trsc->state = IIC_FSM_START;
		conflags = SSC_I2C_STRTG;

		if (!check_fastmode(adap))
			ndelay(4000);
		else
			ndelay(700);

		/* NO break! */

	case IIC_FSM_START:
be_fsm_start:
		dbg_print2("-Start address 0x%x\n", pmsg->addr);

		trsc->idx_current_msg = 0;

		address = (pmsg->addr << 2) | 0x1;
		if (pmsg->flags & I2C_M_RD) {
			dbg_print2(" Reading %d bytes\n", pmsg->len);

			address |= 0x2;
			trsc->next_state = IIC_FSM_PREPARE_2_READ;
			intflags =
			    SSC_IEN_NACKEN | SSC_IEN_TEEN | SSC_IEN_ARBLEN;

			txbuff[0] = address;
			txbuffcount = 1;
		} else {
			trsc->next_state = IIC_FSM_DATA_WRITE;
			intflags =
			    SSC_IEN_NACKEN | SSC_IEN_TEEN | SSC_IEN_ARBLEN;

			dbg_print2("-Writing %d bytes\n", pmsg->len);
			txbuff[0] = address;
			txbuffcount = 1;
			idx = SSC_TXFIFO_SIZE - 1;
			dbg_print2(" TX FIFO %d empty slots\n", idx);

			/* In the write case, we also preload the TX buffer
			 * with some data to reduce the interrupt loading
			 */
			while (idx && trsc->idx_current_msg < pmsg->len) {
				tmp.bytes[0] =
				    pmsg->buf[trsc->idx_current_msg++];
				txbuff[txbuffcount] = tmp.word << 1 | 0x1;
				dbg_print2(" write 0x%02x\n", tmp.bytes[0]);
				txbuffcount++;
				idx--;
			}
		}

		/* drive SDA... */
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB);
		for (idx = 0; idx < txbuffcount; idx++)
			ssc_store32(adap, SSC_TBUF, txbuff[idx]);
		ssc_store32(adap, SSC_IEN, intflags);

		/* Check for bus busy.  This shouldn't happen but if the RX
		 * FIFOs are not empty in the last transaction BEFORE the SSC
		 * reset occurs, then we get a BUSY error here...
		 */
		if (trsc->start_state != IIC_FSM_REPSTART) {
			if (ssc_load32(adap, SSC_STA) & SSC_STA_BUSY) {
				dbg_print2(" bus BUSY!\n");
				trsc->waitcondition = 0;
				trsc->status_error = IIC_E_BUSY;
			} else {
				/* START! */
				ssc_store32(adap, SSC_I2C,
					    SSC_I2C_I2CM | SSC_I2C_TXENB |
					    conflags);
			}
		}

		break;

	case IIC_FSM_REPSTART_ADDR:
		dbg_print2("-Rep Start addr 0x%x\n", pmsg->addr);

		/* Check that slave is not doing a clock stretch */
		while (((ssc_load32(adap, SSC_STA) & SSC_STA_CLST)
					!= SSC_STA_CLST) && (cnt++ < 1000))
			ndelay(100);

		if (cnt >= 1000)
			jump_on_fsm_abort(trsc);

		/* Check that slave is not doing a clock stretching */
		while(((ssc_load32(adap, SSC_STA) & SSC_STA_CLST) != SSC_STA_CLST) &&
		      (cnt++<1000));
		if(cnt>=1000){
		  jump_on_fsm_abort(trsc);
		}

		/* Clear NACK */
		ssc_store32(adap, SSC_CLR, 0xdc0);

		trsc->start_state = IIC_FSM_REPSTART;
		conflags = 0;

		jump_on_fsm_start(trsc);


	case IIC_FSM_PREPARE_2_READ:
		dbg_print2("-Prepare to Read...\n");

		/* Read address */
		ssc_load32(adap, SSC_RBUF);

		/* Check for NACK */
		if (status & SSC_STA_NACK) {
			dbg_print2(" read: NACK detected\n");
			jump_on_fsm_abort(trsc);
		}

		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_ACKG);

		trsc->next_state = IIC_FSM_DATA_READ;

		/* NO break */

	case IIC_FSM_DATA_READ:
		dbg_print2("-Read\n");
		/* Clear the RX buffer */
		idx = (ssc_load32(adap, SSC_RX_FSTAT) & SSC_RX_FSTAT_STATUS);
		if (!idx && (ssc_load32(adap, SSC_STA) & SSC_STA_RIR))
			idx = SSC_RXFIFO_SIZE;

		dbg_print2(" Rx %d bytes in FIFO...\n", idx);
		while (idx && trsc->idx_current_msg < pmsg->len) {
			tmp.word = ssc_load32(adap, SSC_RBUF);
			tmp.word = tmp.word >> 1;
			pmsg->buf[trsc->idx_current_msg++] = tmp.bytes[0];
			dbg_print2(" Rx Data 0x%02x\n", tmp.bytes[0] & 0xff);
			idx--;
		}

		/* If end of RX, issue STOP or REPSTART */
		if (trsc->idx_current_msg == pmsg->len)
			jump_on_fsm_stop(trsc);

		/*    Generate clock for another set of bytes.
		 *    We have to process the last byte separately as we need to
		 *    NOT generate an ACK
		 */
		if (trsc->idx_current_msg == (pmsg->len - 1)) {
			/* last byte - disable ACKG */
			dbg_print2(" Rx last byte\n");
			ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM);
			ssc_store32(adap, SSC_IEN,
				    SSC_IEN_NACKEN | SSC_IEN_ARBLEN);
			ssc_store32(adap, SSC_TBUF, 0x1ff);
		} else {
#ifdef CONFIG_I2C_DEBUG_ALGO
			cw = 0;
#endif
			idx = SSC_TXFIFO_SIZE;
			dbg_print2(" idx=%d status=0x%08x\n", idx,
				   ssc_load32(adap, SSC_STA));

			while (idx
			       && (trsc->idx_current_msg +
				   (SSC_TXFIFO_SIZE - idx)) < (pmsg->len - 1)) {
				ssc_store32(adap, SSC_TBUF, 0x1ff);
#ifdef CONFIG_I2C_DEBUG_ALGO
				cw++;
#endif
				idx--;
			}
			dbg_print2("Clock writes: %d\n", cw);

			/* only take one interrupt when transmit FIFO empty! */
			ssc_store32(adap, SSC_IEN,
				    SSC_IEN_ARBLEN | SSC_IEN_TEEN);
		}

		break;

	case IIC_FSM_DATA_WRITE:
		dbg_print2("-Write\n");
		/* Clear RX data from FIFO */

		/* It is not clear from the SSC4 datasheet, but the RX_FSTAT
		 * register does not tell the whole story...  if RIR is set in
		 * the STATUS reg then we need to read 8 words.  RX_FSTAT only
		 * notifies us of at most 7 words - hence the extra check below
		 */

		lim = ssc_load32(adap, SSC_RX_FSTAT) & SSC_RX_FSTAT_STATUS;
		if (!lim && (status & SSC_STA_RIR))
			lim = 8;

		dbg_print2(" clearing %d RX words\n", lim);
		for (idx = 0; idx < lim; idx++)
			ssc_load32(adap, SSC_RBUF);

		/* Clear status bits EXCEPT NACK */
		ssc_store32(adap, SSC_CLR, 0x9c0);

		/* Check for NACK */
		status = ssc_load32(adap, SSC_STA);
		dbg_print2("write: status = 0x%08x\n", status);
		if (status & SSC_STA_NACK) {
			dbg_print2(" NACK detected\n");
			jump_on_fsm_abort(trsc);
		}

		/* If end of TX, issue STOP or REPSTART */
		if (trsc->idx_current_msg == pmsg->len)
			jump_on_fsm_stop(trsc);

		trsc->next_state = IIC_FSM_DATA_WRITE;

		/* Interrupt when TX buffer empty */
		ssc_store32(adap, SSC_IEN,
			    SSC_IEN_TEEN | SSC_IEN_NACKEN | SSC_IEN_ARBLEN);

		idx = SSC_TXFIFO_SIZE;
		while (idx && trsc->idx_current_msg < pmsg->len) {
			tmp.bytes[0] = pmsg->buf[trsc->idx_current_msg++];
			ssc_store32(adap, SSC_TBUF, tmp.word << 1 | 0x1);
			dbg_print2(" Write 0x%02x\n", tmp.bytes[0]);
			idx--;
		}

		break;

	case IIC_FSM_ABORT:
be_fsm_abort:
		dbg_print2("Abort - issuing STOP\n");
		trsc->status_error |= IIC_E_NOTACK;

		ssc_store32(adap, SSC_CLR, 0xdc0);
		trsc->next_state = IIC_FSM_IDLE;

		ssc_store32(adap, SSC_IEN, SSC_IEN_STOPEN | SSC_IEN_ARBLEN);
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_STOPG);
		break;

	case IIC_FSM_STOP:
be_fsm_stop:
		dbg_print2("-Stop\n");

		if (++trsc->current_msg < trsc->queue_length) {
			/* More transactions left... */
			if (pmsg->flags & I2C_M_NOREPSTART) {
				/* no repstart - stop then start */
				dbg_print2(" STOP - STOP\n");
				trsc->next_state = IIC_FSM_NOREPSTART;
				ssc_store32(adap, SSC_I2C,
					    SSC_I2C_I2CM | SSC_I2C_TXENB |
					    SSC_I2C_STOPG);
				ssc_store32(adap, SSC_IEN,
					    SSC_IEN_STOPEN | SSC_IEN_ARBLEN);
			} else {
				/* repstart */
				dbg_print2(" STOP - REPSTART\n");
				trsc->next_state = IIC_FSM_REPSTART_ADDR;
				ssc_store32(adap, SSC_I2C,
					    SSC_I2C_I2CM |
					    SSC_I2C_TXENB |
					    SSC_I2C_REPSTRTG);
				ssc_store32(adap, SSC_IEN,
					    SSC_IEN_REPSTRTEN |
					    SSC_IEN_ARBLEN);
			}
		} else {
			/* stop */
			dbg_print2(" STOP - STOP\n");
			if (pmsg->flags & I2C_M_NOSTOP &&
				!(status & SSC_STA_NACK)) {
				dbg_print(" STOP - NOSTOP\n");
				set_lasti2cwas_nostop(adap);
				jump_on_fsm_complete(trsc);
			}

			clear_lasti2cwas_nostop(adap);
			trsc->next_state = IIC_FSM_IDLE;

			ssc_store32(adap, SSC_I2C,
				    SSC_I2C_I2CM | SSC_I2C_TXENB |
				    SSC_I2C_STOPG);
			ssc_store32(adap, SSC_IEN,
				    SSC_IEN_STOPEN | SSC_IEN_ARBLEN);
		}

		break;
	case IIC_FSM_IDLE:
		/* In Idle state the SSC still remains
		 * in I2C mode but with the SSC_CTL.Master mode disabled.
		 * In this configuration it seems much more stable
		 * during a plugging/unplugging of HDMI-cable
		 * i.e.: it is much less sensible to the noice on the cable
		 */
		dbg_print2("-Idle\n");
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM);
		/* push the data line high */
		ssc_store32(adap, SSC_TBUF, 0x1ff);
		ssc_store32(adap, SSC_CTL, SSC_CTL_EN |
			    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8);
		/* No break here! */
	case IIC_FSM_COMPLETE:
be_fsm_complete:
		dbg_print2("-Complete\n");

		if (!(trsc->status_error & IIC_E_NOTACK))
			trsc->status_error = IIC_E_NO_ERROR;

		trsc->waitcondition = 0;
		wake_up(&(adap->wait_queue));
		break;

	default:
		printk(KERN_ERR "i2c-stm: Error in the FSM\n");
		;
	}

	return IRQ_HANDLED;
}

/*
 * Wait for stop to be detected on bus
 */
static int iic_wait_stop_condition(struct iic_ssc *adap)
{
	unsigned int idx;

	dbg_print("\n");
	for (idx = 0; idx < 5; ++idx) {
		if (ssc_load32(adap, SSC_STA) & SSC_STA_STOP)
			return 1;
		mdelay(2);
	}

	printk(KERN_ERR "*** iic_wait_stop_condition: TIMED OUT ***\n");
	return 0;
}

/*
 * Reset SSC bus
 */
static void iic_ssc_reset(struct iic_ssc *adap)
{
	unsigned int lim, status, idx;

	/* Ensure RX buffer empty */
	status = ssc_load32(adap, SSC_STA);
	lim = ssc_load32(adap, SSC_RX_FSTAT) & SSC_RX_FSTAT_STATUS;
	if (!lim && (status & SSC_STA_RIR))
		lim = 8;

	for (idx = 0; idx < lim; idx++)
		ssc_load32(adap, SSC_RBUF);

	/* Reset SSC */
	ssc_store32(adap, SSC_CTL, SSC_CTL_SR | SSC_CTL_EN | SSC_CTL_MS |
		    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8);

	/* enable RX, TX FIFOs - clear SR bit */
	ssc_store32(adap, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
		    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8 |
		    SSC_CTL_EN_TX_FIFO | SSC_CTL_EN_RX_FIFO);
}

/*
 * Wait for bus to become free
 */
static int iic_wait_free_bus(struct iic_ssc *adap)
{
	unsigned int reg = 0;
	unsigned int idx;

	dbg_print("\n");

	if (check_lasti2cwas_nostop(adap)){
		dbg_print("i2c-stm: wait_free_bus last transaction nostop.\n");
		return 1;
	}
	iic_ssc_reset(adap);

	for (idx = 0; idx < 10; ++idx) {
		reg = ssc_load32(adap, SSC_STA);
		dbg_print("iic_wait_free_bus: status = 0x%08x\n", reg);
		if (!(reg & SSC_STA_BUSY))
			return 1;
		mdelay(2);
	}

	printk(KERN_ERR "*** iic_wait_free_bus: TIMED OUT ***\n");

	return 0;
}

/*
 * Issue stop condition on the bus by toggling PIO lines
 */
static void iic_pio_stop(struct iic_ssc *adap)
{
	unsigned scl, sda;
	int cnt = 0;

	if (!adap->pad_state)
		return; /* SSC hard wired */

	printk(KERN_WARNING "i2c-stm: doing PIO stop!\n");

	/* Send STOP */
	scl = stm_pad_gpio_request_output(adap->pad_state, "SCL", 0);
	BUG_ON(scl == STM_GPIO_INVALID);
	sda = stm_pad_gpio_request_output(adap->pad_state, "SDA", 0);
	BUG_ON(sda == STM_GPIO_INVALID);
	udelay(20);
	gpio_set_value(scl, 1);
	udelay(20);
	gpio_set_value(sda, 1);
	udelay(30);
	stm_pad_gpio_free(adap->pad_state, scl);
	stm_pad_gpio_free(adap->pad_state, sda);

	/* Reset SSC */
	ssc_store32(adap, SSC_CTL, SSC_CTL_SR | SSC_CTL_EN | SSC_CTL_MS |
		    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8);
	ssc_store32(adap, SSC_CLR, 0xdc0);

	/* Make sure SSC thinks the bus is free before continuing */
	while (cnt < 10
	       && (ssc_load32(adap, SSC_STA) & (SSC_STA_BUSY | SSC_STA_NACK))) {
		mdelay(2);
		cnt++;
	}

	if (cnt == 10)
		printk(KERN_ERR
		       "i2c-stm:  Cannot recover bus.  Status: 0x%08x\n",
		       ssc_load32(adap, SSC_STA));
}

/*
 * Description: Prepares the controller for a transaction
 */
static int iic_stm_xfer(struct i2c_adapter *i2c_adap,
			struct i2c_msg msgs[], int num)
{
	unsigned long flag;
	int result;
	int timeout;
#ifdef CONFIG_I2C_DEBUG_BUS
	int i;
#endif
	struct iic_ssc *adap =
	    (struct iic_ssc *)container_of(i2c_adap, struct iic_ssc, adapter);
	struct iic_transaction transaction = {
		.msgs_queue = msgs,
		.queue_length = num,
		.current_msg = 0x0,
		.attempt = 0x0,
		.status_error = IIC_E_RUNNING,
		.next_state = IIC_FSM_PREPARE,
		.waitcondition = 1,
	};

	dbg_print("\n");

	adap->trns = &transaction;

#ifdef CONFIG_I2C_DEBUG_BUS
	for (i = 0; i < num; ++i) {
		if (msgs[i].len == 0)
			printk(KERN_INFO
			       "[%d of %d] ZERO LENGTH TRANSACTION : "
			       "%s addr 0x%02x len %d\n",
			       i + 1, num,
			       (msgs[i].
				flags & I2C_M_RD) ? "read from " :
				"write to ",
				msgs[i].addr, msgs[i].len);
		else
			printk(KERN_INFO
			       "[%d of %d] TRANSACTION : "
			       "%s addr 0x%02x len %d\n",
			       i + 1, num,
			       (msgs[i].
				flags & I2C_M_RD) ? "read from " :
				"write to ",
			       msgs[i].addr, msgs[i].len);
	}
#endif /* CONFIG_I2C_DEBUG_BUS */

iic_xfer_retry:

	/* Wait for bus to become free - do a forced PIO reset if necessary to
	 * recover the bus
	 */
	if (!iic_wait_free_bus(adap))
		iic_pio_stop(adap);

        adap->config &= ~IIC_STM_CONFIG_SPEED_MASK;
        if (msgs[0].flags & I2C_M_FASTMODE)
            adap->config |= IIC_STM_CONFIG_SPEED_FAST;

	iic_state_machine(0, adap);

	timeout = wait_event_interruptible_timeout(adap->wait_queue,
						   (transaction.waitcondition ==
						    0), i2c_adap->timeout * HZ);

	local_irq_save(flag);

	result = transaction.current_msg;

	if (unlikely
	    (transaction.status_error != IIC_E_NO_ERROR || timeout <= 0)) {
		dbg_print2(KERN_ERR
			   "xfer: ERROR status %d, timeout %d, attempt %d\n",
			   transaction.status_error, timeout,
			   transaction.attempt);

		dbg_print2(KERN_ERR
			   "Status: 0x%08x SSC_IEN 0x%08x "
			   "SSC_CTL 0x%08x SSC_I2C 0x%08x\n",
			   ssc_load32(adap, SSC_STA), ssc_load32(adap, SSC_IEN),
			   ssc_load32(adap, SSC_CTL), ssc_load32(adap,
								 SSC_I2C));

		if (((transaction.status_error & IIC_E_NOTACK)
		     && transaction.start_state == IIC_FSM_START)
		    || (transaction.status_error & IIC_E_BUSY)) {
			clear_lasti2cwas_nostop(adap);
			if (++transaction.attempt <= adap->adapter.retries) {
				dbg_print2("RETRYING operation\n");
				/*
				 * Error on the address - automatically retry
				 *
				 * This used to be done in the FSM complete
				 * but it was not safe there as we need to
				 * wait for the bus to not be busy before
				 * doing another transaction
				 */
				transaction.status_error = 0;
				transaction.next_state = IIC_FSM_START;
				transaction.waitcondition = 1;
				local_irq_restore(flag);
				goto iic_xfer_retry;
			} else {
				local_irq_restore(flag);
				if (transaction.status_error & IIC_E_NOTACK) {
					dbg_print("Error: Slave NACK\n");
					result = -EREMOTEIO;
				} else {
					dbg_print("Error: Bus BUSY\n");
					result = -EBUSY;
				}
			}
		} else if (transaction.status_error == IIC_E_ARBL
			   || (ssc_load32(adap, SSC_CTL) & SSC_CTL_MS) == 0) {
			/* Arbitration error */
			printk(KERN_ERR "i2c-stm: arbitration error\n");

			ssc_store32(adap, SSC_CLR, SSC_CLR_SSCARBL);
			ssc_store32(adap, SSC_CTL,
				    ssc_load32(adap, SSC_CTL) | SSC_CTL_MS);
			transaction.status_error = 0;
			transaction.next_state = IIC_FSM_START;
			transaction.waitcondition = 1;
			local_irq_restore(flag);

			if (!iic_wait_free_bus(adap)) {
				/* Last ditch effort */
				iic_pio_stop(adap);
			}
			clear_lasti2cwas_nostop(adap);

			if (++transaction.attempt <= adap->adapter.retries) {
				dbg_print2("RETRYING operation\n");
				goto iic_xfer_retry;
			}
		} else {
			/* There was another problem */
			if (timeout <= 0) {
				/* There was a timeout or signal.
				   - disable the interrupt
				   - generate a stop condition on the bus
				   all this task are done without interrupt....
				 */
				dbg_print
				    ("xfer: Wait cnd err %d, status error %d\n",
				     timeout, transaction.status_error);
				ssc_store32(adap, SSC_IEN, 0x0);

				/* Check if bus free */
				if (!iic_wait_free_bus(adap)) {
					/* No - generate stop condition */
					ssc_store32(adap, SSC_I2C,
						    SSC_I2C_I2CM | SSC_I2C_STOPG
						    | SSC_I2C_TXENB);

					local_irq_restore(flag);

					if (!iic_wait_stop_condition(adap)) {
						/* Reset SSC */
						ssc_store32(adap, SSC_CTL,
							    SSC_CTL_SR |
							    SSC_CTL_EN |
							    SSC_CTL_MS |
							    SSC_CTL_PO |
							    SSC_CTL_PH |
							    SSC_CTL_HB | 0x8);
						ssc_store32(adap, SSC_CLR,
							    0xdc0);

						if (!iic_wait_free_bus(adap)) {
							/* Last ditch effort */
							iic_pio_stop(adap);
						}
					}
				} else {
					local_irq_restore(flag);
				}
				clear_lasti2cwas_nostop(adap);
			} else
				local_irq_restore(flag);

			if (!timeout) {
				dbg_print
				    ("i2c-stm: Error timeout in the FSM\n");
				result = -ETIMEDOUT;
			} else if (timeout < 0) {
				dbg_print
				    ("i2c-stm: wait event interrupt/error\n");
				result = timeout;
			} else {
				dbg_print("i2c-stm: slave failed to respond\n");
				result = -EREMOTEIO;
			}
		}
	} else
		local_irq_restore(flag);

#ifdef CONFIG_I2C_DEBUG_BUS
	printk(KERN_INFO "i2c-stm: i2c_stm_xfer returned %d\n", result);
#endif
	return result;
}

#ifdef CONFIG_I2C_DEBUG_BUS
static void iic_stm_timing_trace(struct iic_ssc *adap)
{
	dbg_print("SSC_BRG  %d\n", ssc_load32(adap, SSC_BRG));
	dbg_print("SSC_REP_START_HOLD %d\n",
		  ssc_load32(adap, SSC_REP_START_HOLD));
	dbg_print("SSC_REP_START_SETUP %d\n",
		  ssc_load32(adap, SSC_REP_START_SETUP));
	dbg_print("SSC_START_HOLD %d\n", ssc_load32(adap, SSC_START_HOLD));
	dbg_print("SSC_DATA_SETUP %d\n", ssc_load32(adap, SSC_DATA_SETUP));
	dbg_print("SSC_STOP_SETUP %d\n", ssc_load32(adap, SSC_STOP_SETUP));
	dbg_print("SSC_BUS_FREE %d\n", ssc_load32(adap, SSC_BUS_FREE));
	dbg_print("SSC_PRE_SCALER_BRG %d\n",
		  ssc_load32(adap, SSC_PRE_SCALER_BRG));
	dbg_print("SSC_NOISE_SUPP_WIDTH %d\n",
		  ssc_load32(adap, SSC_NOISE_SUPP_WIDTH));
	dbg_print("SSC_PRSCALER %d\n", ssc_load32(adap, SSC_PRSCALER));
	dbg_print("SSC_NOISE_SUPP_WIDTH_DATAOUT %d\n",
		  ssc_load32(adap, SSC_NOISE_SUPP_WIDTH_DATAOUT));
	dbg_print("SSC_PRSCALER_DATAOUT %d\n",
		  ssc_load32(adap, SSC_PRSCALER_DATAOUT));
}
#endif

static void iic_stm_setup_timing(struct iic_ssc *adap)
{
	unsigned long iic_baudrate;
	unsigned short iic_rep_start_hold;
	unsigned short iic_start_hold;
	unsigned short iic_rep_start_setup;
	unsigned short iic_data_setup;
	unsigned short iic_stop_setup;
	unsigned short iic_bus_free;
	unsigned short iic_pre_scale_baudrate = 1;
#ifdef CONFIG_I2C_STM_HW_GLITCH
	unsigned short iic_glitch_width;
	unsigned short iic_glitch_width_dataout;
	unsigned char iic_prescaler;
	unsigned short iic_prescaler_dataout;
#endif
	unsigned long ns_per_clk, clock ;

	dbg_print("Assuming %lu MHz for the Timing Setup\n", clock / 1000000);

	clock = clk_get_rate(adap->clk) + 500000; /* +0.5 Mhz for rounding */
	ns_per_clk = NANOSEC_PER_SEC / clock;

	if (check_fastmode(adap)) {
		set_ready_fastmode(adap);
		iic_baudrate = clock / (2 * I2C_RATE_FASTMODE);
		iic_rep_start_hold =
		    (REP_START_HOLD_TIME_FAST + GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_rep_start_setup =
		    (REP_START_SETUP_TIME_FAST +
		     GLITCH_WIDTH_CLOCK) / ns_per_clk;
		if (GLITCH_WIDTH_DATA < 200)
			iic_start_hold =
			    (START_HOLD_TIME_FAST +
			     GLITCH_WIDTH_DATA) / ns_per_clk;
		else
			iic_start_hold = (5 * GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_data_setup =
		    (DATA_SETUP_TIME_FAST + GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_stop_setup =
		    (STOP_SETUP_TIME_FAST + GLITCH_WIDTH_CLOCK) / ns_per_clk;
		iic_bus_free =
		    (BUS_FREE_TIME_FAST + GLITCH_WIDTH_DATA) / ns_per_clk;
	} else {
		clear_ready_fastmode(adap);
		iic_baudrate = clock / (2 * I2C_RATE_NORMAL);
		iic_rep_start_hold =
		    (REP_START_HOLD_TIME_NORMAL +
		     GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_rep_start_setup =
		    (REP_START_SETUP_TIME_NORMAL +
		     GLITCH_WIDTH_CLOCK) / ns_per_clk;
		if (GLITCH_WIDTH_DATA < 1200)
			iic_start_hold =
			    (START_HOLD_TIME_NORMAL +
			     GLITCH_WIDTH_DATA) / ns_per_clk;
		else
			iic_start_hold = (5 * GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_data_setup =
		    (DATA_SETUP_TIME_NORMAL + GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_stop_setup =
		    (STOP_SETUP_TIME_NORMAL + GLITCH_WIDTH_CLOCK) / ns_per_clk;
		iic_bus_free =
		    (BUS_FREE_TIME_NORMAL + GLITCH_WIDTH_DATA) / ns_per_clk;
	}

	/* set baudrate */
	ssc_store32(adap, SSC_BRG, iic_baudrate);
	ssc_store32(adap, SSC_PRE_SCALER_BRG, iic_pre_scale_baudrate);

	/* enable I2C mode */
	ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM);

	/* set other timings */
	ssc_store32(adap, SSC_REP_START_HOLD, iic_rep_start_hold);
	ssc_store32(adap, SSC_START_HOLD, iic_start_hold);
	ssc_store32(adap, SSC_REP_START_SETUP, iic_rep_start_setup);
	ssc_store32(adap, SSC_DATA_SETUP, iic_data_setup);
	ssc_store32(adap, SSC_STOP_SETUP, iic_stop_setup);
	ssc_store32(adap, SSC_BUS_FREE, iic_bus_free);

	/*
	 * Set Slave Address to allow 'General Call'
	 */
	ssc_store32(adap, SSC_SLAD, 0x7f);

#ifdef CONFIG_I2C_STM_HW_GLITCH
	/* See DDTS GNBvd40668 */
	iic_prescaler = 1;
	iic_glitch_width = HW_GLITCH_WIDTH * clock / 100000000;	/* in uS */
	iic_glitch_width_dataout = 1;
	iic_prescaler_dataout = clock / 10000000;

/*  This should work, but causes lock-up after repstart
    iic_prescaler = clock / 10000000;
    iic_glitch_width = HW_GLITCH_WIDTH;
    iic_glitch_width_dataout = 1;
    iic_prescaler_dataout = clock / 10000000;
    printk("*** iic_prescaler = %d *** \n", iic_prescaler);
*/

	ssc_store32(adap, SSC_PRSCALER, iic_prescaler);
	ssc_store32(adap, SSC_NOISE_SUPP_WIDTH, iic_glitch_width);
	ssc_store32(adap, SSC_NOISE_SUPP_WIDTH_DATAOUT,
		    iic_glitch_width_dataout);
	ssc_store32(adap, SSC_PRSCALER_DATAOUT, iic_prescaler_dataout);
#else
	/* disable SSC glitch filter */
	ssc_store32(adap, SSC_NOISE_SUPP_WIDTH, 0);
#endif

#ifdef CONFIG_I2C_DEBUG_BUS
	iic_stm_timing_trace(adap);
#endif
	return;
}

static int iic_stm_control(struct i2c_adapter *adapter,
			   unsigned int cmd, unsigned long arg)
{
	struct iic_ssc *iic_adap =
	    container_of(adapter, struct iic_ssc, adapter);
	switch (cmd) {
	case I2C_STM_IOCTL_FAST:
		dbg_print("ioctl fast 0x%lx\n", arg);
		iic_adap->config &= ~IIC_STM_CONFIG_SPEED_MASK;
		if (arg)
			iic_adap->config |= IIC_STM_CONFIG_SPEED_FAST;
		break;
	default:
		printk(KERN_WARNING " %s: i2c-ioctl not managed\n",
		       __func__);
	}

	return 0;
}

static u32 iic_stm_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm iic_stm_algo = {
	.master_xfer = iic_stm_xfer,
	.functionality = iic_stm_func,
/*	.algo_control = iic_stm_control */
};

static ssize_t iic_bus_show_fastmode(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct i2c_adapter *adapter =
	    container_of(dev, struct i2c_adapter, dev);
	struct iic_ssc *iic_stm =
	    container_of(adapter, struct iic_ssc, adapter);
	return sprintf(buf, "%u\n", check_fastmode(iic_stm));
}

static ssize_t iic_bus_store_fastmode(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct i2c_adapter *adapter =
	    container_of(dev, struct i2c_adapter, dev);
	unsigned long val = strict_strtoul(buf, 10, NULL);

	iic_stm_control(adapter, I2C_STM_IOCTL_FAST, val);

	return count;
}

static DEVICE_ATTR(fastmode, S_IRUGO | S_IWUSR, iic_bus_show_fastmode,
		   iic_bus_store_fastmode);

static int __init iic_stm_probe(struct platform_device *pdev)
{
	struct stm_plat_ssc_data *plat_data = pdev->dev.platform_data;
	struct iic_ssc *i2c_stm;
	struct resource *res;
	int err;

	i2c_stm = devm_kzalloc(&pdev->dev, sizeof(struct iic_ssc), GFP_KERNEL);

	if (!i2c_stm)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	if (!devm_request_mem_region
	    (&pdev->dev, res->start, resource_size(res), "i2c")) {
		dev_err(&pdev->dev, "Request mem region %pR failed\n", res);
		return -ENOMEM;
	}

	i2c_stm->base = devm_ioremap_nocache(&pdev->dev, res->start,
					     resource_size(res));
	if (!i2c_stm->base) {
		dev_err(&pdev->dev, "ioremap mem region %pR failed\n", res);
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "Platform data does not specify IRQ\n");
		return -ENODEV;
	}
	if (devm_request_irq(&pdev->dev, res->start, iic_state_machine,
			     IRQF_DISABLED, "i2c", i2c_stm) < 0) {
		dev_err(&pdev->dev, "Request irq %d failed\n", res->start);
		return -ENODEV;
	}

	/* If SSC is hard wired, there will be no pad configurations */
	if (plat_data->pad_config) {
		i2c_stm->pad_state = devm_stm_pad_claim(&pdev->dev,
				plat_data->pad_config, "i2c-stm");
		if (!i2c_stm->pad_state) {
			dev_err(&pdev->dev, "Pads request failed\n");
			return -ENODEV;
		}
	}

	platform_set_drvdata(pdev, i2c_stm);
	i2c_stm->adapter.timeout = 2;
	i2c_stm->adapter.retries = 0;
	i2c_stm->adapter.class = I2C_CLASS_HWMON | I2C_CLASS_TV_ANALOG |
		I2C_CLASS_TV_DIGITAL | I2C_CLASS_DDC | I2C_CLASS_SPD;;
	sprintf(i2c_stm->adapter.name, "i2c-stm%d", pdev->id);
	i2c_stm->adapter.nr = pdev->id;
	i2c_stm->adapter.algo = &iic_stm_algo;
	i2c_stm->adapter.dev.parent = &(pdev->dev);
	i2c_stm->clk = clk_get(&(pdev->dev), "comms_clk");
	if (!i2c_stm->clk) {
		dev_err(&pdev->dev, "Comms clock not found!\n");
		return -ENODEV;
	}

	clk_enable(i2c_stm->clk);

	iic_stm_setup_timing(i2c_stm);
	init_waitqueue_head(&(i2c_stm->wait_queue));
	if (i2c_add_numbered_adapter(&(i2c_stm->adapter)) < 0) {
		dev_err(&pdev->dev, "I2C core refuses the i2c/stm adapter\n");
		return -ENODEV;
	}

	err = device_create_file(&(i2c_stm->adapter.dev), &dev_attr_fastmode);
	if (err) {
		dev_err(&pdev->dev, "Cannot create fastmode sysfs entry\n");
		return err;
	}

	/* by default the device is on */
	pm_runtime_set_active(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);

	return 0;
}

static int iic_stm_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct iic_ssc *iic_stm = platform_get_drvdata(pdev);

	clk_disable(iic_stm->clk);

	i2c_del_adapter(&iic_stm->adapter);
	/* irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	devm_free_irq(&pdev->dev, res->start, iic_stm);
	/* mem */
	devm_iounmap(&pdev->dev, iic_stm->base);
	/* kmem */
	devm_kfree(&pdev->dev, iic_stm);
	return 0;
}

#ifdef CONFIG_PM
static int iic_stm_suspend(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct iic_ssc *i2c_bus = platform_get_drvdata(pdev);
#if 0
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	if (pio_info->pio[0].pio_port != SSC_NO_PIO) {
		stpio_configure_pin(pio_info->clk, STPIO_IN);
		stpio_configure_pin(pio_info->sdout, STPIO_IN);
	}
#endif
	ssc_store32(i2c_bus, SSC_IEN, 0);
	ssc_store32(i2c_bus, SSC_CTL, 0);
	clk_disable(i2c_bus->clk);

	return 0;
}
static int iic_stm_resume(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct iic_ssc *i2c_bus = platform_get_drvdata(pdev);
#if 0
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;

	if (pio_info->pio[0].pio_port != SSC_NO_PIO) {
		/* configure the pins */
		stpio_configure_pin(pio_info->clk, (pio_info->clk_unidir ?
			STPIO_ALT_BIDIR : STPIO_ALT_BIDIR));
		stpio_configure_pin(pio_info->sdout, STPIO_ALT_BIDIR);
	}
#endif
	clk_enable(i2c_bus->clk);
	/* enable RX, TX FIFOs - clear SR bit */
	ssc_store32(i2c_bus, SSC_CTL, SSC_CTL_EN |
		    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8);
	ssc_store32(i2c_bus, SSC_I2C, SSC_I2C_I2CM);
	iic_stm_setup_timing(i2c_bus);
	return 0;
}
#else
#define iic_stm_suspend NULL
#define iic_stm_resume NULL
#endif

static struct dev_pm_ops stm_i2c_pm_ops = {
	.suspend = iic_stm_suspend,
	.resume = iic_stm_resume,
	.freeze = iic_stm_suspend,
	.restore = iic_stm_resume,
	.runtime_suspend = iic_stm_suspend,
	.runtime_resume = iic_stm_resume,
};

static struct platform_driver i2c_stm_driver = {
	.driver = {
		.name = "i2c-stm",
		.owner = THIS_MODULE,
		.pm = &stm_i2c_pm_ops,
	},
	.probe = iic_stm_probe,
	.remove = iic_stm_remove,
};

static int __init iic_stm_init(void)
{
	platform_driver_register(&i2c_stm_driver);
	return 0;
}

static void __exit iic_stm_exit(void)
{
	platform_driver_unregister(&i2c_stm_driver);
}

module_init(iic_stm_init);
module_exit(iic_stm_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("i2c-stm algorithm for STMicroelectronics devices");
MODULE_LICENSE("GPL");
