/*
 * Copyright (C) 2005,7 STMicroelectronics Limited
 * Authors: Mark Glaisher <Mark.Glaisher@st.com>
 *          Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef STM_DMA_H
#define STM_DMA_H

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/string.h>
#include <linux/module.h>


/*DMA Modes */
#define MODE_FREERUNNING   		0x01	/* FDMA, GPDMA */
#define MODE_PACED  		 	0x02	/* FDMA */
#define MODE_SRC_SCATTER		0x04
#define MODE_DST_SCATTER		0x05

/* DMA dimensions */
#define DIM_SRC_SHIFT 0
#define DIM_DST_SHIFT 2
#define DIM_SRC(x) (((x) >> DIM_SRC_SHIFT) & 3)
#define DIM_DST(x) (((x) >> DIM_DST_SHIFT) & 3)
enum stm_dma_dimensions {
	DIM_0_x_0 = (0 << DIM_SRC_SHIFT) | (0 << DIM_DST_SHIFT),
	DIM_0_x_1 = (0 << DIM_SRC_SHIFT) | (1 << DIM_DST_SHIFT),
	DIM_0_x_2 = (0 << DIM_SRC_SHIFT) | (2 << DIM_DST_SHIFT),
	DIM_1_x_0 = (1 << DIM_SRC_SHIFT) | (0 << DIM_DST_SHIFT),
	DIM_1_x_1 = (1 << DIM_SRC_SHIFT) | (1 << DIM_DST_SHIFT),
	DIM_1_x_2 = (1 << DIM_SRC_SHIFT) | (2 << DIM_DST_SHIFT),
	DIM_2_x_0 = (2 << DIM_SRC_SHIFT) | (0 << DIM_DST_SHIFT),
	DIM_2_x_1 = (2 << DIM_SRC_SHIFT) | (1 << DIM_DST_SHIFT),
	DIM_2_x_2 = (2 << DIM_SRC_SHIFT) | (2 << DIM_DST_SHIFT),
};

enum stm_dma_flags {
	STM_DMA_INTER_NODE_PAUSE=0x800,
	STM_DMA_NODE_COMP_INT=0x1000,
	STM_DMA_CB_CONTEXT_ISR=0x2000,
	STM_DMA_CB_CONTEXT_TASKLET=0x4000,
	STM_DMA_CHANNEL_PAUSE_FLUSH=0x20000,
	STM_DMA_CHANNEL_PAUSE_NOFLUSH=0x40000,
	STM_DMA_NOBLOCK_MODE=0x80000,
	STM_DMA_BLOCK_MODE=0x100000,
	STM_DMA_LIST_CIRC=0x200000,
	STM_DMA_LIST_OPEN=0x400000,
};

#define DMA_CHANNEL_STATUS_IDLE 		0
#define DMA_CHANNEL_STATUS_RUNNING 		2
#define DMA_CHANNEL_STATUS_PAUSED 		3

/* Parameters to request_dma_bycap() */
#define STM_DMAC_ID 			"fdma_dmac"
#define STM_DMA_CAP_HIGH_BW		"STM_DMA_HIGH_BANDWIDTH"
#define STM_DMA_CAP_LOW_BW		"STM_DMA_LOW_BANDWIDTH"
#define STM_DMA_CAP_ETH_BUF		"STM_DMA_ETH_BUFFER"

/* dma_extend() operations */
#define STM_DMA_OP_FLUSH      1
#define STM_DMA_OP_PAUSE      2
#define STM_DMA_OP_UNPAUSE    3
#define STM_DMA_OP_STOP       4
#define STM_DMA_OP_COMPILE    5
#define STM_DMA_OP_STATUS     6
#define STM_DMA_OP_REQ_CONFIG 7
#define STM_DMA_OP_REQ_FREE   8

/* Generic DMA request line configuration */

/* Read/Write */
#define REQ_CONFIG_READ            0
#define REQ_CONFIG_WRITE           1

/* Opcodes */
#define REQ_CONFIG_OPCODE_1        0x00
#define REQ_CONFIG_OPCODE_2        0x01
#define REQ_CONFIG_OPCODE_4        0x02
#define REQ_CONFIG_OPCODE_8        0x03
#define REQ_CONFIG_OPCODE_16       0x04
#define REQ_CONFIG_OPCODE_32       0x05

struct stm_dma_req_config
{
	unsigned char req_line;		/* Request line index number */
	unsigned char rw;		/* Access type: Read or Write */
	unsigned char opcode;		/* Size of word access */
	unsigned char count;		/* Number of transfers per request */
	unsigned char increment;	/* Whether to increment */
	unsigned char hold_off;		/* Holdoff value between req signal samples (in clock cycles)*/
	unsigned char initiator;	/* Which STBus initiatator to use */
};

struct stm_dma_req;

/* Generic STM DMA params */

struct stm_dma_params;

struct params_ops {
	int (*free_params)(struct stm_dma_params* params);
};

struct stm_dma_params {

	/* Transfer mode eg MODE_DST_SCATTER */
	unsigned long mode;

	/* a pointer to a callback function of type void foo(void*)
	 * which will be called on completion of the entire
	 * transaction or after each transfer suceeds if
	 * NODE_PAUSE_ISR is specifed */
	void				(*comp_cb)(unsigned long);
	unsigned long			comp_cb_parm;

	/* a pointer to a callback function of type void foo(void*)
	 * which will be called upon failure of a transfer or
	 * transaction*/
	void				(*err_cb)(unsigned long);
	unsigned long			err_cb_parm;

	/*Source location line stride for use in 0/1/2 x 2D modes*/
	unsigned long			sstride;

	/*Source location line stride for use in 2D x 0/1/2 modes*/
	unsigned long			dstride;

	/* Line length for any 2D modes */
	unsigned long			line_len;

	/*source addr - given in phys*/
	unsigned long 			sar;

	/*dest addr  - given in phys*/
	unsigned long 			dar;

	unsigned long 			node_bytes;

	struct scatterlist * srcsg;
	struct scatterlist * dstsg;
	int sglen;

	int err_cb_isr	:1;
	int comp_cb_isr	:1;

	int node_pause		:1;
	int node_interrupt	:1;
	int circular_llu        :1;

	unsigned long dim;

	/* Parameters for paced transfers */
	struct stm_dma_req *req;

	/* Pointer to compiled parameters
	 * this includes the *template* llu node and
	 * its assoc'd memory */
	void* priv;

	/* Next pointer for linked list of params */
	struct stm_dma_params *next;

	/* Pointer to DMAC specific operators on the parameters.
	 * Filled in by dma_compile_list(). */
	struct params_ops *params_ops;
	void* params_ops_priv;

	/* This is only used in the call to dma_compile_list(), so
	 * shouldn't really be here, but it saves us packing and unpacking
	 * the parameters into another struct. */
	gfp_t context;
};

static inline void dma_params_init(struct stm_dma_params * p,
				  unsigned long mode,
				  unsigned long list_type)
{
	memset(p,0,sizeof(struct stm_dma_params));
	p->mode = mode;
	p->circular_llu = (STM_DMA_LIST_CIRC ==list_type ?1:0);
};

static inline int dma_get_status(unsigned int vchan)
{
	return dma_extend(vchan, STM_DMA_OP_STATUS, NULL);
}

/* Flush implies pause - I mean pause+flush */
static inline int dma_flush_channel(unsigned int vchan)
{
	return dma_extend(vchan, STM_DMA_OP_FLUSH, NULL);
}

static inline int dma_pause_channel(unsigned int vchan)
{
	return dma_extend(vchan, STM_DMA_OP_PAUSE, NULL);
}

static inline void dma_unpause_channel(unsigned int vchan)
{
	dma_extend(vchan, STM_DMA_OP_UNPAUSE, NULL);
}

static inline int dma_stop_channel(unsigned int vchan)
{
	return dma_extend(vchan, STM_DMA_OP_STOP, NULL);
}

static inline int dma_params_free(struct stm_dma_params *params)
{
	return params->params_ops->free_params(params);
}

static inline int dma_compile_list(unsigned int vchan,
				   struct stm_dma_params *params,
				   gfp_t gfp_mask)
{
	params->context = gfp_mask;
	return dma_extend(vchan, STM_DMA_OP_COMPILE, params);
}

static inline int dma_xfer_list(unsigned int vchan, struct stm_dma_params *p)
{
	/*TODO :- this is a bit 'orrible -
	 * should really extend arch/sh/drivers/dma/dma-api.c
	 * to include a 'set_dma_channel'*/
	dma_configure_channel(vchan, (unsigned long)p);
	return dma_xfer(vchan, 0, 0, 0, 0);
}

static inline struct stm_dma_req *dma_req_config(unsigned int vchan,
	unsigned int req_line,
	struct stm_dma_req_config* req_config)
{
	req_config->req_line = req_line;
	return (struct stm_dma_req *)
		dma_extend(vchan, STM_DMA_OP_REQ_CONFIG, req_config);
}

static inline void dma_req_free(unsigned int vchan, struct stm_dma_req *req)
{
	dma_extend(vchan, STM_DMA_OP_REQ_FREE, req);
}

static inline  void dma_params_sg(	struct stm_dma_params *p,
					struct scatterlist * sg,
					int nents)
{
	if(MODE_SRC_SCATTER==p->mode)
		p->srcsg=sg;
	else if (MODE_DST_SCATTER==p->mode)
		p->dstsg = sg;
	else
		BUG();
	p->sglen = nents;
}

static inline void dma_params_link(	struct stm_dma_params * parent,
					struct stm_dma_params * child)
{
	parent->next=child;
}

static inline void dma_params_req(	struct stm_dma_params *p,
					struct stm_dma_req *req)
{
	p->req = req;
}

static inline void dma_params_addrs(	struct stm_dma_params *p,
					unsigned long src,
					unsigned long dst,
					unsigned long bytes)
{
	p->sar = src;
	p->dar = dst;
	p->node_bytes = bytes;
}

static inline void dma_params_interrupts(struct stm_dma_params *p,
					unsigned long isrflag)
{
	if(isrflag & STM_DMA_INTER_NODE_PAUSE)
		p->node_pause=1;
	if(isrflag & STM_DMA_NODE_COMP_INT )
		p->node_interrupt=1;

}

static inline void dma_params_comp_cb(	struct stm_dma_params *p,
					void (*fn)(unsigned long param),
					unsigned long param,
					int isr_context)
{
	p->comp_cb = fn;
	p->comp_cb_parm = param;
	p->comp_cb_isr = (isr_context == STM_DMA_CB_CONTEXT_ISR ?1:0);
}

static inline void dma_params_err_cb(	struct stm_dma_params *p,
					void (*fn)(unsigned long param),
	      				unsigned long param,
	      				int isr_context)
{
	p->err_cb = fn;
	p->err_cb_parm = param;
	p->err_cb_isr = (isr_context == STM_DMA_CB_CONTEXT_ISR ?1:0);
}

static inline void dma_params_dim(	struct stm_dma_params *p,
					unsigned long line_len,
					unsigned long sstride,
					unsigned long dstride,
					unsigned long dim)
{
	p->line_len = line_len;
	p->sstride = sstride;
	p->dstride = dstride;
	p->dim =dim;
}

static inline void dma_params_DIM_0_x_0(struct stm_dma_params *p)
{
	dma_params_dim(p, 0,0,0, DIM_0_x_0);
}

static inline void dma_params_DIM_0_x_1(struct stm_dma_params *p)
{
	dma_params_dim(p, 0,0,0, DIM_0_x_1);
}

static inline void dma_params_DIM_0_x_2(struct stm_dma_params *p,
					unsigned long line_len,
					unsigned long dstride)
{
	dma_params_dim(p, line_len, 0, dstride, DIM_0_x_2);
}

static inline void dma_params_DIM_1_x_0(struct stm_dma_params *p)
{
	dma_params_dim(p, 0,0,0, DIM_1_x_0);
}

static inline void dma_params_DIM_1_x_1(struct stm_dma_params *p)
{
	dma_params_dim(p, 0,0,0, DIM_1_x_1);
}

static inline void dma_params_DIM_1_x_2(struct stm_dma_params *p,
					unsigned long line_len,
					unsigned long dstride)
{
	dma_params_dim(p, line_len, line_len, dstride, DIM_1_x_2);
}

static inline void dma_params_DIM_2_x_0(struct stm_dma_params *p,
					unsigned long line_len,
					unsigned long sstride)
{
	dma_params_dim(p, line_len, sstride, 0, DIM_2_x_0);
}

static inline void dma_params_DIM_2_x_1(struct stm_dma_params *p,
					unsigned long line_len,
					unsigned long sstride)
{
	dma_params_dim(p, line_len, sstride, line_len, DIM_2_x_1);
}
#endif
