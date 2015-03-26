/*
 * STMicroelectronics Asynchronous Serial Controller (ASC) driver
 * FDMA extension
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/timer.h>
#include <linux/stm/stm-dma.h>
#include <linux/dma-mapping.h>

#include "stasc.h"


/* RX buffers */
#define RX_NODES_NUMBER 4  /* Keep it power of 2 */
#define RX_NODE_SIZE 2048  /* Keep it power of 2 */
#define RX_DATA_FIFO_SIZE 32



/* Self-documenting constants, aren't they? */
#define ALIGN_SIZE 0x4
#define ALIGN_MASK (ALIGN_SIZE - 1)



/* It is better to get error messages, isn't it? */
#define SHOW_ERROR

#ifdef SHOW_ERROR
#define ERROR(fmt, args...) printk(KERN_ERR "%s:%d: %s(): ERROR: " fmt, \
		__FILE__, __LINE__, __FUNCTION__, ## args)
#else
#define ERROR(...)
#endif



/* Define SHOW_TRACE to get a lot of tracing messages,
 * useful for debugging */
#undef SHOW_TRACE

#ifdef SHOW_TRACE
#define TRACE(fmt, args...) printk(KERN_INFO "%s:%d: %s(): " fmt, \
		__FILE__, __LINE__, __FUNCTION__, ## args)
#else
#define TRACE(...)
#endif



/********************************************************** TX track */



struct asc_port_fdma_tx_channel {
	int running;
	int channel;
	struct stm_dma_req *req;
	struct stm_dma_params params;
	unsigned long transfer_size;
};



static void asc_fdma_tx_callback_done(unsigned long param);
static void asc_fdma_tx_callback_error(unsigned long param);



static struct stm_dma_req_config tx_dma_req_config = {
	.rw        = REQ_CONFIG_WRITE,
	.opcode    = REQ_CONFIG_OPCODE_1,
	.count     = 4,
	.increment = 0,
	.hold_off  = 0,
	.initiator = 0,
};

static int asc_fdma_tx_prepare(struct uart_port *port)
{
	struct asc_port_fdma_tx_channel *tx;
	const char *fdmac_id[] = { STM_DMAC_ID, NULL };
	const char *lb_cap[] = { STM_DMA_CAP_LOW_BW, NULL };
	const char *hb_cap[] = { STM_DMA_CAP_HIGH_BW, NULL };

	TRACE("Preparing FDMA TX for port %p.\n", port);

	/* Allocate channel description structure */
	tx = kmalloc(sizeof *tx, GFP_KERNEL);
	if (tx == NULL) {
		ERROR("Can't get memory for TX channel description!\n");
		return -ENOMEM;
	}
	memset(tx, 0, sizeof *tx);
	asc_ports[port->line].fdma.tx = tx;

	/* Get DMA channel */
	tx->channel = request_dma_bycap(fdmac_id, lb_cap, "ASC_TX");
	if (tx->channel < 0) {
		tx->channel = request_dma_bycap(fdmac_id, hb_cap, "ASC_TX");
		if (tx->channel < 0) {
			ERROR("FDMA TX channel request failed!\n");
			kfree(tx);
			return -EBUSY;
		}
	}

	/* Prepare request line, using req_id set in stasc.c */
	tx->req = dma_req_config(tx->channel,
			asc_ports[port->line].fdma.tx_req_id,
			&tx_dma_req_config);
	if (tx->req == NULL) {
		ERROR("FDMA TX req line for port %p not available!\n", port);
		free_dma(tx->channel);
		kfree(tx);
		return -EBUSY;
	}

	/* Now - parameters initialisation */
	dma_params_init(&tx->params, MODE_PACED, STM_DMA_LIST_OPEN);

	/* Set callbacks */
	dma_params_comp_cb(&tx->params, asc_fdma_tx_callback_done,
			(unsigned long)port, STM_DMA_CB_CONTEXT_TASKLET);
	dma_params_err_cb(&tx->params, asc_fdma_tx_callback_error,
			(unsigned long)port, STM_DMA_CB_CONTEXT_TASKLET);

	/* Source pace - 1, destination pace - 0 */
	dma_params_DIM_1_x_0(&tx->params);

	/* Request line */
	dma_params_req(&tx->params, tx->req);

	/* Compile the paramters just to have .ops field filled... (!!!) */
	dma_compile_list(tx->channel, &tx->params, GFP_KERNEL);

	return 0;
}

static void asc_fdma_tx_shutdown(struct uart_port *port)
{
	struct asc_port_fdma_tx_channel *tx = asc_ports[port->line].fdma.tx;

	TRACE("Shutting down FDMA TX for port %p.\n", port);

	BUG_ON(tx->running);

	dma_params_free(&tx->params);
	dma_req_free(tx->channel, tx->req);
	free_dma(tx->channel);

	kfree(tx);
}



static void asc_fdma_tx_callback_done(unsigned long param)
{
	struct uart_port *port = (struct uart_port *)param;
	struct asc_port_fdma_tx_channel *tx = asc_ports[port->line].fdma.tx;
	struct circ_buf *xmit = &port->state->xmit;

	TRACE("Transmission of %lu bytes on ASC FDMA TX done for port %p.\n",
			tx->transfer_size, port);

	/* If channel was stopped (tx->running == 0), do nothing */
	if (unlikely(!tx->running))
		return;

	tx->running = 0;

	/* Move buffer pointer by transferred size */
	xmit->tail = (xmit->tail + tx->transfer_size) & (UART_XMIT_SIZE - 1);
	port->icount.tx += tx->transfer_size;

	/* If appropriate, notify higher layers that
	 * there is some place in buffer */
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	/* If there is (still or again) some data in buffer -
	 * transmit it now. */
	if (CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE) > 0)
		asc_fdma_tx_start(port);
}

static void asc_fdma_tx_callback_error(unsigned long param)
{
	ERROR("ASC FDMA TX error\n");
}

int asc_fdma_tx_start(struct uart_port *port)
{
	struct asc_port_fdma_tx_channel *tx = asc_ports[port->line].fdma.tx;
	struct circ_buf *xmit = &port->state->xmit;
	int result = 0;

	TRACE("Starting ASC FDMA TX on port %p.\n", port);

	BUG_ON(!asc_ports[port->line].fdma.enabled);

	/* Do nothing if some transfer is in progress... */
	if (tx->running)
		return 0;

	/* Get the transfer size - as this is a "simulated" circular
	 * buffer (I mean modulo), we can just transfer data from tail
	 * to end of physical buffer (or to tail, whichever comes first);
	 * the wrapped rest (if exist) will be transmitted when this one
	 * is done (see asc_fdma_tx_callback_done). */
	tx->transfer_size = CIRC_CNT_TO_END(xmit->head, xmit->tail,
			UART_XMIT_SIZE);
	BUG_ON(tx->transfer_size <= 0);

	/* No jokes, please. The buffer _must_ be aligned! */
	BUG_ON(virt_to_phys(xmit->buf) & ALIGN_MASK);

	/* If first character to be transmitted is unaligned - DIY.
	 * If transfer size is lower than 4 bytes (minimum FDMA
	 * transfer unit) - DIY */
	while (tx->transfer_size > 0 && (xmit->tail & ALIGN_MASK ||
			tx->transfer_size < ALIGN_SIZE)) {
		int size = ALIGN_SIZE - (xmit->tail & ALIGN_MASK);
		int i;

		if (tx->transfer_size < size)
			size = tx->transfer_size;

		TRACE("Transmitting %d (beginning from byte %d in buffer)"
				" bytes on port %p via ASC TX register.\n",
				size, xmit->tail, port);

		/* Put the data into the TX FIFO and forget :-) */
		for (i = 0; i < size; i++)
			asc_out(port, TXBUF, xmit->buf[xmit->tail++]);

		/* We assumed that would not wrap around this
		 * circular buffer... */
		BUG_ON(xmit->tail > UART_XMIT_SIZE);

		/* Update counters */
		xmit->tail &= UART_XMIT_SIZE - 1;
		port->icount.tx += size;

		tx->transfer_size -= size;
	}

	/* Anything left for FDMAzilla? :-) */
	if (tx->transfer_size > 0) {
		/* Align transfer size to 4 bytes and carry on -
		 * the rest will be transmitted after finishing this transfer */
		tx->transfer_size &= ~ALIGN_MASK;

		TRACE("Transmitting %lu bytes on port %p via FDMA.\n",
				tx->transfer_size, port);

		/* Check buffer alignment */
		BUG_ON(virt_to_phys(xmit->buf + xmit->tail) & ALIGN_MASK);

		/* Write back the buffer cache */
		dma_map_single(port->dev, xmit->buf + xmit->tail,
				tx->transfer_size, DMA_TO_DEVICE);

		/* Setup DMA transfer parameters
		 * (source & destination addresses and size) */
		dma_params_addrs(&tx->params,
				virt_to_phys(xmit->buf + xmit->tail),
				(unsigned long)(port->membase + ASC_TXBUF),
				tx->transfer_size);

		result = dma_compile_list(tx->channel, &tx->params, GFP_KERNEL);
		if (result == 0) {
			/* Launch transfer */
			result = dma_xfer_list(tx->channel, &tx->params);
			if (result == 0)
				tx->running = 1;
			else
				ERROR("Can't launch TX DMA transfer!\n");
		} else {
			ERROR("Can't compile TX DMA paramters list!\n");
		}
	}

	return result;
}

void asc_fdma_tx_stop(struct uart_port *port)
{
	struct asc_port_fdma_tx_channel *tx = asc_ports[port->line].fdma.tx;

	TRACE("Stopping ASC FDMA TX on port %p.\n", port);

	BUG_ON(!asc_ports[port->line].fdma.enabled);
	BUG_ON(!tx->running);

	dma_stop_channel(tx->channel);

	tx->running = 0;
}



/********************************************************** RX track */



struct asc_port_fdma_rx_data {
	int remainder;
	unsigned char *chars;
	int size;
};

struct asc_port_fdma_rx_channel {
	/* Port pointer */
	struct uart_port *port;

	/* FDMA channel data */
	int running;
	int channel;
	struct stm_dma_req *req;
	struct stm_dma_params params[RX_NODES_NUMBER];

	/* Buffers */
	int order;
	unsigned char *chars;  /* size defined by order */
	unsigned char *flags;  /* size defined by order */
	unsigned char *remainders;  /* one page */
	struct asc_port_fdma_rx_data data[RX_DATA_FIFO_SIZE];

	/* Data processing work */
	struct work_struct ldisc_work;

	/* Run-time data */
	int last_residue;
	int remainders_head;
	int remainders_tail;
	int data_head;
	int data_tail;
};



static void asc_fdma_rx_callback_done(unsigned long param);
static void asc_fdma_rx_callback_error(unsigned long param);
static void asc_fdma_rx_ldisc_work(struct work_struct *work);



static struct stm_dma_req_config asc_fdma_rx_req_config = {
	.rw        = REQ_CONFIG_READ,
	.opcode    = REQ_CONFIG_OPCODE_1,
	.count     = 4,
	.increment = 0,
	.hold_off  = 0,
	.initiator = 0,
};

static int asc_fdma_rx_prepare(struct uart_port *port)
{
	struct asc_port_fdma_rx_channel *rx;
	const char *fdmac_id[] = { STM_DMAC_ID, NULL };
	const char *lb_cap[] = { STM_DMA_CAP_LOW_BW, NULL };
	const char *hb_cap[] = { STM_DMA_CAP_HIGH_BW, NULL };
	int result;
	int i;

	TRACE("Preparing FDMA RX for port %p.\n", port);

	/* Allocate and clear channel description structure */
	rx = kmalloc(sizeof *rx, GFP_KERNEL);
	asc_ports[port->line].fdma.rx = rx;
	if (rx == NULL) {
		ERROR("Can't get memory for RX channel description!\n");
		result = -ENOMEM;
		goto error_description;
	}
	memset(rx, 0, sizeof *rx);
	rx->port = port;

	/* Pages are allocated in "order" units ;-) */
	rx->order = get_order(RX_NODE_SIZE * RX_NODES_NUMBER);

	/* Allocate buffer filled with TTY_NORMAL character flag */
	rx->flags = (unsigned char *)__get_free_pages(GFP_KERNEL | __GFP_DMA,
			rx->order);
	if (rx->flags == NULL) {
		ERROR("Can't get memory pages for flags buffer!\n");
		result = -ENOMEM;
		goto error_flags;
	}
	memset(rx->flags, TTY_NORMAL, RX_NODE_SIZE * RX_NODES_NUMBER);

	/* Allocate characters buffer */
	rx->chars = (unsigned char *)__get_free_pages(GFP_KERNEL | __GFP_DMA,
			rx->order);
	if (rx->chars == NULL) {
		ERROR("Can't get memory pages for chars buffer!\n");
		result = -ENOMEM;
		goto error_chars;
	}
	/* Well, finally it is going to be used by FDMA! */
	dma_map_single(port->dev, rx->chars, RX_NODE_SIZE * RX_NODES_NUMBER,
			DMA_FROM_DEVICE);

	/* Allocate remainders buffer */
	rx->remainders = (unsigned char *)__get_free_page(GFP_KERNEL);
	if (rx->remainders == NULL) {
		ERROR("Can't get memory page for remainders buffer!\n");
		result = -ENOMEM;
		goto error_remainders;
	}

	/* Get DMA channel */
	rx->channel = request_dma_bycap(fdmac_id, lb_cap, "ASC_RX");
	if (rx->channel < 0) {
		rx->channel = request_dma_bycap(fdmac_id, hb_cap, "ASC_RX");
		if (rx->channel < 0) {
			ERROR("FDMA RX channel request failed!\n");
			result = -EBUSY;
			goto error_channel;
		}
	}

	/* Prepare request line, using req_id set in stasc.c */
	rx->req = dma_req_config(rx->channel,
			asc_ports[port->line].fdma.rx_req_id,
			&asc_fdma_rx_req_config);
	if (rx->req == NULL) {
		ERROR("DMA RX req line for port %p not available!\n", port);
		result = -EBUSY;
		goto error_req;
	}

	/* Now - parameters initialisation */
	for (i = 0; i < RX_NODES_NUMBER; i++) {
		dma_params_init(&rx->params[i], MODE_PACED, STM_DMA_LIST_CIRC);

		/* Link nodes */
		if (i > 0)
			dma_params_link(&rx->params[i - 1], &rx->params[i]);

		/* Set callbacks */
		dma_params_comp_cb(&rx->params[i], asc_fdma_rx_callback_done,
				(unsigned long)port, STM_DMA_CB_CONTEXT_ISR);
		dma_params_err_cb(&rx->params[i], asc_fdma_rx_callback_error,
				(unsigned long)port, STM_DMA_CB_CONTEXT_ISR);

		/* Get callback every time a node is completed */
		dma_params_interrupts(&rx->params[i], STM_DMA_NODE_COMP_INT);

		/* Source pace - 0, destination pace - 1 */
		dma_params_DIM_0_x_1(&rx->params[i]);

		/* Request line */
		dma_params_req(&rx->params[i], rx->req);

		/* Node buffer address */
		dma_params_addrs(&rx->params[i],
				(unsigned long)(port->membase + ASC_RXBUF),
				virt_to_phys(rx->chars + (i * RX_NODE_SIZE)),
				RX_NODE_SIZE);
	}

	/* Compile the parameters (the first one, in fact)
	 * to be ready to launch... */
	result = dma_compile_list(rx->channel, rx->params, GFP_KERNEL);
	if (result != 0) {
		ERROR("Can't compile RX DMA paramters list!\n");
		goto error_compile;
	}

	/* Initialize data parsing work for shared workqueue */
	INIT_WORK(&rx->ldisc_work, asc_fdma_rx_ldisc_work);

	return 0;

error_compile:
	dma_params_free(rx->params); /* Frees whole list */
error_req:
	free_dma(rx->channel);
error_channel:
	free_page((unsigned long)rx->remainders);
error_remainders:
	free_pages((unsigned long)rx->chars, rx->order);
error_chars:
	free_pages((unsigned long)rx->flags, rx->order);
error_flags:
	kfree(rx);
error_description:
	return result;
}

static void asc_fdma_rx_shutdown(struct uart_port *port)
{
	struct asc_port_fdma_rx_channel *rx = asc_ports[port->line].fdma.rx;

	TRACE("Shutting down FDMA RX for port %p.\n", port);

	BUG_ON(rx->running);

	dma_params_free(rx->params); /* Frees whole list */
	dma_req_free(rx->channel, rx->req);
	free_dma(rx->channel);

	free_page((unsigned long)rx->remainders);
	free_pages((unsigned long)rx->chars, rx->order);
	free_pages((unsigned long)rx->flags, rx->order);

	kfree(rx);
}



static inline void asc_fdma_disable_tne_interrupt(struct uart_port *port)
{
	unsigned long intenable;

	/* Clear TNE (Timeout not empty) interrupt enable in INTEN */
	intenable = asc_in(port, INTEN);
	intenable &= ~ASC_INTEN_TNE;
	asc_out(port, INTEN, intenable);
}

static inline void asc_fdma_enable_tne_interrupt(struct uart_port *port)
{
	unsigned long intenable;

	/* Set TNE (Timeout not empty) interrupt enable in INTEN */
	intenable = asc_in(port, INTEN);
	intenable |= ASC_INTEN_TNE;
	asc_out(port, INTEN, intenable);
}

static inline void asc_fdma_rx_add_data(struct asc_port_fdma_rx_channel *rx,
		int remainder, unsigned char *chars, int size)
{
	TRACE("Adding %d bytes of%s data (%p) to RX %p FIFO.\n",
			size, remainder ? " remainder" : "", chars, rx);

	rx->data[rx->data_tail].remainder = remainder;
	rx->data[rx->data_tail].chars = chars;
	rx->data[rx->data_tail].size = size;
	rx->data_tail = (rx->data_tail + 1) & (RX_DATA_FIFO_SIZE - 1);
}

static void asc_fdma_rx_callback_done(unsigned long param)
{
	struct uart_port *port = (struct uart_port *)param;
	struct asc_port_fdma_rx_channel *rx = asc_ports[port->line].fdma.rx;
	int current_residue;
	int remainder_length = 0;
	int remainders_start = rx->remainders_tail;

	TRACE("Transfer on ASC FDMA RX done for port %p (status 0x%08x).\n",
			port, asc_in(port, STA));

	/* If channel was stopped - do nothing... */
	if (unlikely(!rx->running))
		return;

	/* We have to service timeout case as quick as possible! */
	preempt_disable();

	/* How much data was actually transferred by FDMA so far? */
	current_residue = get_dma_residue(rx->channel);

	TRACE("FDMA residue value is %d for port %p (rx %p).\n",
			current_residue, port, rx);

	/* Paused channel? */
	if (dma_get_status(rx->channel) == DMA_CHANNEL_STATUS_PAUSED) {
		unsigned long status = asc_in(port, STA);

		/* Read RX FIFO until empty, but no more than half of FIFO
		 * size - if there is more (or HALF FULL bit is signalled
		 * again) it means that some data came again.
		 * Let FDMA handle this... */
		while (!(status & ASC_STA_RHF) &&
				(remainder_length < FIFO_SIZE / 2) &&
				(status & ASC_STA_RBF)) {
			unsigned long ch = asc_in(port, RXBUF);

			rx->remainders[rx->remainders_tail] = ch & 0xff;
			rx->remainders_tail = (rx->remainders_tail + 1) &
					(PAGE_SIZE - 1);
			remainder_length++;
			status = asc_in(port, STA);

			/* That means buffer overrun! */
			BUG_ON(rx->remainders_tail == rx->remainders_head);
		}

		/* Launch FDMA again */
		dma_unpause_channel(rx->channel);
		asc_fdma_enable_tne_interrupt(port);

		TRACE("Got %d bytes of remainder from port's %p RX FIFO.\n",
				remainder_length, port);
	}

	/* Ahhh... The most important thing is done! */
	preempt_enable();

	/* Add residue data (if any) to fifo */
	if (current_residue < rx->last_residue) {
		int offset = (RX_NODES_NUMBER * RX_NODE_SIZE) -
				rx->last_residue;
		int size = rx->last_residue - current_residue;
		asc_fdma_rx_add_data(rx, 0, rx->chars + offset, size);
	} else if (current_residue > rx->last_residue) {
		/* FDMA buffer has just wrapped
		 * So, lets pass received data in two parts -
		 * first the ending... */
		int offset = (RX_NODES_NUMBER * RX_NODE_SIZE) -
				rx->last_residue;
		int size = rx->last_residue;
		asc_fdma_rx_add_data(rx, 0, rx->chars + offset, size);
		/* Now the beginning (offset = 0) */
		size = (RX_NODES_NUMBER * RX_NODE_SIZE) - current_residue;
		asc_fdma_rx_add_data(rx, 0, rx->chars, size);
	}

	/* Add remainders to disc fifo (if any) */
	if (remainder_length > 0) {
		if (rx->remainders_tail > rx->remainders_head) {
			asc_fdma_rx_add_data(rx, 1,
					rx->remainders + remainders_start,
					remainder_length);
		} else {
			/* Wrapped situation again */
			asc_fdma_rx_add_data(rx, 1,
					rx->remainders + remainders_start,
					PAGE_SIZE - remainders_start);
			asc_fdma_rx_add_data(rx, 1, rx->remainders,
					rx->remainders_tail);
		}
	}

	/* Schedule work that will pass aquired data to TTY line discipline */
	schedule_work(&rx->ldisc_work);

	/* Update statistics */
	port->icount.rx += remainder_length +
		((rx->last_residue - current_residue) &
		(RX_NODES_NUMBER * RX_NODE_SIZE - 1));

	/* Well, almost done */
	rx->last_residue = current_residue;
}

static void asc_fdma_rx_callback_error(unsigned long param)
{
	ERROR("ASC FDMA RX error.\n");
}

static void asc_fdma_rx_ldisc_work(struct work_struct *work)
{
	struct asc_port_fdma_rx_channel *rx = container_of(work,
			struct asc_port_fdma_rx_channel, ldisc_work);
	struct uart_port *port = rx->port;
	struct tty_ldisc *ldisc = tty_ldisc_ref_wait(port->state->port.tty);

	while (rx->data_head != rx->data_tail) {
		/* Get a data portion from fifo */
		struct asc_port_fdma_rx_data *data = &rx->data[rx->data_head];

		TRACE("Passing %d bytes of%s data (%p) from port %p"
				" to discipline %p.\n", data->size,
				data->remainder ? " remainder" : "",
				data->chars, port, ldisc);

		/* Feed TTY line discipline with received data and flags buffer
		 * (already prepared and filled with TTY_NORMAL flags) */
		ldisc->ops->receive_buf(port->state->port.tty, data->chars,
					rx->flags, data->size);

		if (data->remainder)
			/* Free space in circular buffer */
			rx->remainders_head = (rx->remainders_head +
					data->size) & (PAGE_SIZE - 1);
		else
			/* Invalidate buffer's cache, if transferred by FDMA */
			dma_map_single(port->dev, data->chars, data->size,
					DMA_FROM_DEVICE);

		/* Remove the entry from fifo */
		rx->data_head = (rx->data_head + 1) & (RX_DATA_FIFO_SIZE - 1);
	}

	tty_ldisc_deref(ldisc);
}



static int asc_fdma_rx_start(struct uart_port *port)
{
	struct asc_port_fdma_rx_channel *rx = asc_ports[port->line].fdma.rx;
	int result;

	TRACE("Starting ASC FDMA RX on port %p (status 0x%04x).\n",
			port, asc_in(port, STA));

	BUG_ON(rx->running);

	/* Initialize pointers */
	rx->remainders_head = 0;
	rx->remainders_tail = 0;
	rx->data_head = 0;
	rx->data_tail = 0;
	rx->last_residue = RX_NODE_SIZE * RX_NODES_NUMBER;

	/* Launch transfer */
	result = dma_xfer_list(rx->channel, rx->params);
	if (result == 0) {
		rx->running = 1;

		/* Turn on timeout interrupt */
		asc_fdma_enable_tne_interrupt(port);
	} else
		ERROR("Can't launch RX DMA transfer!\n");

	return result;
}

void asc_fdma_rx_stop(struct uart_port *port)
{
	struct asc_port_fdma_rx_channel *rx = asc_ports[port->line].fdma.rx;

	TRACE("Stopping ASC FDMA RX on port %p.\n", port);

	BUG_ON(!asc_ports[port->line].fdma.enabled);
	BUG_ON(!rx->running);

	asc_fdma_disable_tne_interrupt(port);

	dma_stop_channel(rx->channel);

	rx->running = 0;
}

void asc_fdma_rx_timeout(struct uart_port *port)
{
	struct asc_port_fdma_rx_channel *rx = asc_ports[port->line].fdma.rx;

	BUG_ON(!asc_ports[port->line].fdma.enabled);
	BUG_ON(!rx->running);

	TRACE("Timeout on ASC FDMA port %p when RX FIFO is not empty."
			" (status 0x%08x)\n", port, asc_in(port, STA));

	asc_fdma_disable_tne_interrupt(port);

	/* Let's flush (and pause) the channel's buffer - when done,
	 * normal callback will be called */
	dma_flush_channel(rx->channel);
}



/********************************************************** Generic */



int asc_fdma_startup(struct uart_port *port)
{
	struct asc_port_fdma *fdma = &asc_ports[port->line].fdma;
	int result;

	TRACE("Starting up ASC FDMA support for port %p (line %d).\n",
			port, port->line);

	BUG_ON(fdma->ready);

	result = asc_fdma_tx_prepare(port);
	if (result == 0) {
		result = asc_fdma_rx_prepare(port);
		if (result == 0)
			fdma->ready = 1;
		else {
			ERROR("FDMA RX preparation failed!\n");
			asc_fdma_tx_shutdown(port);
		}
	} else
		ERROR("FDMA TX preparation failed!\n");

	return result;
}

void asc_fdma_shutdown(struct uart_port *port)
{
	struct asc_port_fdma *fdma = &asc_ports[port->line].fdma;

	TRACE("Shutting down ASC FDMA support for port %p.\n", port);

	if (fdma->enabled)
		asc_fdma_disable(port);

	if (fdma->ready) {
		asc_fdma_rx_shutdown(port);
		asc_fdma_tx_shutdown(port);
		fdma->ready = 0;
	}
}



int asc_fdma_enable(struct uart_port *port)
{
	struct asc_port_fdma *fdma = &asc_ports[port->line].fdma;
	int result;

	TRACE("Enabling ASC FDMA acceleration for port %p.\n", port);

	BUG_ON(fdma->enabled);

	result = asc_fdma_rx_start(port);
	if (result == 0)
		fdma->enabled = 1;
	else
		ERROR("Can't start receiving!\n");

	return result;
}

void asc_fdma_disable(struct uart_port *port)
{
	struct asc_port_fdma *fdma = &asc_ports[port->line].fdma;

	TRACE("Disabling ASC FDMA acceleration for port %p.\n", port);

	BUG_ON(!fdma->enabled);

	if (fdma->tx->running)
		asc_fdma_tx_stop(port);

	if (fdma->rx->running)
		asc_fdma_rx_stop(port);

	fdma->enabled = 0;
}
