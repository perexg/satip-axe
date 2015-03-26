/*
 * Copyright (C) 2005,7 STMicroelectronics Limited
 * Authors: Mark Glaisher <Mark.Glaisher@st.com>
 *          Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __STM_FDMA_H
#define __STM_FDMA_H

#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/stm/stm-dma.h>

#define NAME_MAX_LEN 22 /* "fdma_<cpu_subtype>_X.elf" */
#define CHAN_ALL_ENABLE 				3

#define NODE_DATA_OFFSET				0x40
#define CMD_STAT_OFFSET       				0x04

/**cmd stat vals*/
#define SET_NODE_COMP_PAUSE		    		(1 << 30)
#define SET_NODE_COMP_IRQ				(1 << 31)
#define NODE_ADDR_STATIC 				0x01
#define NODE_ADDR_INCR	 				0x02

#define SOURCE_ADDR 					0x05
#define DEST_ADDR   					0x07

#define CMDSTAT_FDMA_CMD_MASK				0x1f
#define CMDSTAT_FDMA_START_CHANNEL			1
#define CMDSTAT_FDMA_RESTART_CHANNEL			0

#define FDMA_CHANS					16
#define FDMA_REQ_LINES					32

/*******************************/
/*MBOX SETUP VALUES*/

#define MBOX_CMD_FLUSH_CHANNEL		 		3
#define MBOX_CMD_PAUSE_CHANNEL		 		2
#define MBOX_CMD_START_CHANNEL       			1
#define CLEAR_WORD					0XFFFFFFFF

#define CMD_STAT_REG(_chan_num) \
		(fdma->io_base + fdma->regs.cmd_statn + \
		(_chan_num * CMD_STAT_OFFSET))

#define FDMA_CHANNEL_IDLE 		0
#define FDMA_CHANNEL_RUNNING 		2
#define FDMA_CHANNEL_PAUSED 		3

/*FDMA Channel FLAGS*/
/*values below D28 are reserved for REQ_LINE parameter*/
#define REQ_LINE_MASK 	0x1f

#define CHAN_NUM(chan) ((chan) - chip.channel)

/* SLIM doesn't yet have an officially recognised ELF ID, so we use this */
#define EM_SLIM 102

/* Use the e_flags field of the ELF header to distinguish between SLIM usage */
#define EF_SLIM_FDMA 2

struct fdma_llu_entry {
	u32 next_item;
	u32 control;
	u32 size_bytes;
	u32 saddr;
	u32 daddr;
	u32 line_len;
	u32 sstride;
	u32 dstride;
};


struct fdma_llu_node {
	struct fdma_llu_entry *virt_addr;
	dma_addr_t dma_addr;
};


struct fdma_xfer_descriptor {
	struct fdma_llu_node *(*extrapolate_fn)(struct stm_dma_params *xfer,
			struct fdma_xfer_descriptor *desc,
			struct fdma_llu_node *nodes);
	int extrapolate_line_len;
	struct fdma_llu_entry template_llu;

	/* only used when this is the first parameter in a list */
	struct fdma_llu_node *llu_nodes;
	int alloced_nodes;
};


enum fdma_state {
	FDMA_IDLE,
	FDMA_CONFIGURED,
	FDMA_RUNNING,
	FDMA_STOPPING,
	FDMA_PAUSING,
	FDMA_PAUSED,
};

struct fdma;

struct stm_dma_req {
	int req_line;
};

struct fdma_channel {
	struct fdma *fdma;
	int chan_num;
	enum fdma_state sw_state;
	struct dma_channel *dma_chan;
	struct stm_dma_params *params;
	struct tasklet_struct fdma_complete;
	struct tasklet_struct fdma_error;
};

struct fdma_regs {
	unsigned long id;
	unsigned long ver;
	unsigned long en;
	unsigned long clk_gate;
	unsigned long rev_id;
	unsigned long cmd_statn;
	unsigned long ptrn;
	unsigned long cntn;
	unsigned long saddrn;
	unsigned long daddrn;
	unsigned long req_ctln;
	unsigned long sync_reg;
	unsigned long cmd_sta;
	unsigned long cmd_set;
	unsigned long cmd_clr;
	unsigned long cmd_mask;
	unsigned long int_sta;
	unsigned long int_set;
	unsigned long int_clr;
	unsigned long int_mask;
};

#define FDMA_NAME_LEN 20
struct fdma_segment_pm {
	void *data;
	unsigned long size;
	unsigned long offset;
};

struct fdma {
	char name[FDMA_NAME_LEN];
	char fw_name[NAME_MAX_LEN + 1];
	struct platform_device *pdev;
	/*
	 * The FDMA-IP needs 4 clocks
	 * - a T1 port
	 * - a T2 port (High priority)
	 * - a T2 port (Low priority)
	 * - a slim clock
	 */
	struct clk *clks[4];
	struct dma_info dma_info;
	struct fdma_channel channels[FDMA_CHANS];
	spinlock_t channels_lock; /* protects channels array */

	struct resource *phys_mem;
	void __iomem *io_base;

	struct stm_dma_req reqs[FDMA_REQ_LINES];
	unsigned long reqs_used_mask;
	spinlock_t reqs_lock; /* protects reqs_used_mask */

	u32 firmware_loaded;
	u8 ch_min;
	u8 ch_max;
	u8 irq;
	u8 fdma_num;
	u32 ch_status_mask;
	struct dma_pool *llu_pool;
	wait_queue_head_t fw_load_q;

	struct stm_plat_fdma_hw *hw;
	struct stm_plat_fdma_fw_regs *fw;
#ifdef CONFIG_HIBERNATION
	struct fdma_segment_pm segment_pm[2]; /* saved segment (text/data) */
#endif
	struct fdma_regs regs;
};

struct fdma_req_router {
	int (*route)(struct fdma_req_router *router, int input_req_line,
			int fdma, int fdma_req_line);
};

int fdma_register_req_router(struct fdma_req_router *router);
void fdma_unregister_req_router(struct fdma_req_router *router);



typedef volatile unsigned long device_t;

#define fdma_printk(level, fd, format, arg...)	\
	dev_printk(level, &fd->dma_info.pdev->dev, format, ## arg);
#define fdma_info(fd, format, arg...)		\
	fdma_printk(KERN_INFO, fd, format, ## arg)

#if defined(CONFIG_STM_DMA_DEBUG)
#define fdma_dbg(fd, format, arg...)		\
	fdma_printk(KERN_DEBUG, fd, format, ## arg)
#else
#define fdma_dbg(fd, format, arg...)		do { } while (0)
#endif

#endif
