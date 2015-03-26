/*
 * Copyright (C) 2005,7 STMicroelectronics Limited
 * Authors: Mark Glaisher <Mark.Glaisher@st.com>
 *          Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License. See linux/COPYING for more information.
 */

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/dmapool.h>
#include <linux/stm/platform.h>
#include <linux/stm/stm-dma.h>
#include <linux/libelf.h>
#include <linux/stm/clk.h>

#include "fdma.h"

#define FDMA_CLKS_NR		4
#define FDMA_SLIM_CLK	0
#define FDMA_HI_CLK	1
#define FDMA_LOW_CLK	2
#define FDMA_IC_CLK	3

#define FDMA_MIN_CHANNEL 0
#define FDMA_MAX_CHANNEL 15
#define FDMA_MAX_DEVICES 3

static char *fdma_channels[FDMA_MAX_DEVICES];
module_param_array_named(channels, fdma_channels, charp, NULL, S_IRUGO);
MODULE_PARM_DESC(channels, "Limit channels to be used by each of the FDMA "
		"devices: channels=ch_min-ch_max[,ch_min-ch_max[...]]");



static int fdma_setup_freerunning_node(struct stm_dma_params *params,
		struct fdma_llu_entry *llu)
{
	memset(llu, 0, sizeof(*llu));

	if (params->node_pause)
		llu->control |= SET_NODE_COMP_PAUSE | SET_NODE_COMP_IRQ;

	if (params->node_interrupt)
		llu->control |= SET_NODE_COMP_IRQ;

	if (DIM_SRC(params->dim) == 0)
		llu->control |= NODE_ADDR_STATIC << SOURCE_ADDR;
	else
		llu->control |= NODE_ADDR_INCR << SOURCE_ADDR;

	if (DIM_DST(params->dim) == 0)
		llu->control |= NODE_ADDR_STATIC << DEST_ADDR;
	else
		llu->control |= NODE_ADDR_INCR << DEST_ADDR;

	llu->line_len = params->line_len;
	llu->sstride = params->sstride;
	llu->dstride = params->dstride;
	return 0;
}

static int fdma_setup_paced_node(struct stm_dma_params *params,
		struct fdma_llu_entry *llu)
{
	memset(llu, 0, sizeof(*llu));

	/* Moved this into the extrapolate functions so that we can
	 * change channel in the same way as address. Yech */
	/* llu->control = params->req_line; */
	llu->size_bytes = params->node_bytes;
	llu->line_len = params->node_bytes;

	if (params->node_pause)
		/* In order to recieve the pause interrupt
		 * we must also enable end of node interrupts. */
		llu->control |= SET_NODE_COMP_PAUSE | SET_NODE_COMP_IRQ;

	if (params->node_interrupt)
		llu->control |= SET_NODE_COMP_IRQ;

	if (DIM_SRC(params->dim) == 0)
		llu->control |= NODE_ADDR_STATIC << SOURCE_ADDR;
	else
		llu->control |= NODE_ADDR_INCR << SOURCE_ADDR;

	if (DIM_DST(params->dim) == 0)
		llu->control |= NODE_ADDR_STATIC << DEST_ADDR;
	else
		llu->control |= NODE_ADDR_INCR << DEST_ADDR;

	return 0;
}

static struct fdma_llu_node *fdma_extrapolate_simple(
		struct stm_dma_params *params,
		struct fdma_xfer_descriptor *desc,
		struct fdma_llu_node *llu_node)
{
	struct fdma_llu_entry *dest_llu = llu_node->virt_addr;

	dest_llu->control = desc->template_llu.control |
			(params->req ? params->req->req_line : 0);
	dest_llu->size_bytes = params->node_bytes;
	dest_llu->saddr	 = params->sar;
	dest_llu->daddr	 = params->dar;
	if (desc->extrapolate_line_len)
		dest_llu->line_len = params->node_bytes;
	else
		dest_llu->line_len = desc->template_llu.line_len;
	dest_llu->sstride = desc->template_llu.sstride;
	dest_llu->dstride = desc->template_llu.dstride;

	return llu_node;
}

static struct fdma_llu_node *fdma_extrapolate_sg_src(
		struct stm_dma_params *params,
		struct fdma_xfer_descriptor *desc,
		struct fdma_llu_node *llu_node)
{
	int i;
	struct scatterlist *sg = params->srcsg;
	struct fdma_llu_node *last_llu_node = llu_node;

	for (i = 0; i < params->sglen; i++) {
		struct fdma_llu_entry *dest_llu = llu_node->virt_addr;

		dest_llu->control = desc->template_llu.control;
		dest_llu->size_bytes = sg_dma_len(sg);
		dest_llu->saddr	= sg_dma_address(sg);
		dest_llu->daddr	= params->dar;
		if (desc->extrapolate_line_len)
			dest_llu->line_len = sg_dma_len(sg);
		else
			dest_llu->line_len = desc->template_llu.line_len;
		dest_llu->sstride = desc->template_llu.sstride;
		dest_llu->dstride = 0;

		last_llu_node = llu_node++;
		dest_llu->next_item = llu_node->dma_addr;
		sg++;
	}

	return last_llu_node;
}

static struct fdma_llu_node *fdma_extrapolate_sg_dst(
		struct stm_dma_params *params,
		struct fdma_xfer_descriptor *desc,
		struct fdma_llu_node *llu_node)
{
	int i;
	struct scatterlist *sg = params->dstsg;
	struct fdma_llu_node *last_llu_node = llu_node;

	for (i = 0; i < params->sglen; i++) {
		struct fdma_llu_entry *dest_llu = llu_node->virt_addr;

		dest_llu->control = desc->template_llu.control;
		dest_llu->size_bytes = sg_dma_len(sg);
		dest_llu->saddr	 = params->sar;
		dest_llu->daddr	 = sg_dma_address(sg);
		if (desc->extrapolate_line_len)
			dest_llu->line_len = sg_dma_len(sg);
		else
			dest_llu->line_len = desc->template_llu.line_len;
		dest_llu->sstride = 0;
		dest_llu->dstride = desc->template_llu.dstride;

		last_llu_node = llu_node++;
		dest_llu->next_item = llu_node->dma_addr;
		sg++;
	}

	return last_llu_node;
}

static int fdma_resize_nodelist_mem(struct fdma *fdma,
		struct fdma_xfer_descriptor *desc, unsigned int new_nnodes,
		gfp_t context)
{
	int old_list_size, new_list_size;
	unsigned int cur_nnodes;
	struct fdma_llu_node *new_nodes;

	/* This holds the number of allocated nodes, which may differ
	 * from the old or new size. It must be maintained so that
	 * free_list works. */
	cur_nnodes = desc->alloced_nodes;

	/* The only resize down we need to support is freeing everything. */
	if (new_nnodes == 0)
		goto free_list;

	/* this happens if the DMA firmware was not loaded yet. */
	if(!fdma->llu_pool)
		return -ENOMEM;

	old_list_size = sizeof(struct fdma_llu_node) * desc->alloced_nodes;
	new_list_size = sizeof(struct fdma_llu_node) * new_nnodes;
	new_nodes = kmalloc(new_list_size, context);
	if (new_nodes == NULL)
		goto free_list;

	if (old_list_size > 0) {
		memcpy(new_nodes, desc->llu_nodes, old_list_size);
		kfree(desc->llu_nodes);
	}

	desc->llu_nodes = new_nodes;

	for (new_nodes += desc->alloced_nodes; cur_nnodes < new_nnodes;
			cur_nnodes++, new_nodes++) {
		new_nodes->virt_addr = dma_pool_alloc(fdma->llu_pool,
				context, &new_nodes->dma_addr);
		if (new_nodes->virt_addr == NULL)
			goto free_list;
	}

	desc->alloced_nodes = new_nnodes;
	return 0;

free_list:
	for (new_nodes = desc->llu_nodes; cur_nnodes;
			cur_nnodes--, new_nodes++)
		dma_pool_free(fdma->llu_pool, new_nodes->virt_addr,
				new_nodes->dma_addr);
	kfree(desc->llu_nodes);

	desc->llu_nodes = NULL;
	desc->alloced_nodes = 0;

	return -ENOMEM;
}

static void fdma_start_channel(struct fdma_channel *channel,
		unsigned long start_addr, unsigned long initial_count)
{
	struct fdma *fdma = channel->fdma;
	u32 cmd_sta_value = (start_addr | CMDSTAT_FDMA_START_CHANNEL);

	/* See comment in fdma_get_residue() for why we do this. */
	writel(initial_count, fdma->io_base + (channel->chan_num *
			NODE_DATA_OFFSET) + fdma->regs.cntn);

	writel(cmd_sta_value, CMD_STAT_REG(channel->chan_num));
	writel(MBOX_CMD_START_CHANNEL << (channel->chan_num * 2),
			fdma->io_base + fdma->regs.cmd_set);
}

static int fdma_get_engine_status(struct fdma_channel *channel)
{
	struct fdma *fdma = channel->fdma;

	return readl(CMD_STAT_REG(channel->chan_num)) & 3;
}

static inline void fdma_handle_fdma_err_irq(struct fdma_channel *channel)
{
	struct fdma *fdma = channel->fdma;
	void (*err_cb)(unsigned long) = channel->params->err_cb;
	unsigned long err_cb_parm = channel->params->err_cb_parm;

	spin_lock(&fdma->channels_lock);

	/* err is bits 2-4 */
	fdma_dbg(fdma, "%s: FDMA error %d on channel %d\n", __FUNCTION__,
			(readl(CMD_STAT_REG(channel->chan_num)) >> 2) & 0x7,
			channel->chan_num);

	/* According to the spec, in case of error transfer "may be
	 * aborted" (or may not be, sigh) so let's make the situation
	 * clear and stop it explicitly now. */
	writel(MBOX_CMD_PAUSE_CHANNEL << (channel->chan_num * 2),
			fdma->io_base + fdma->regs.cmd_set);
	channel->sw_state = FDMA_STOPPING;

	spin_unlock(&fdma->channels_lock);

	wake_up(&channel->dma_chan->wait_queue);

	if (err_cb) {
		if (channel->params->err_cb_isr)
			err_cb(err_cb_parm);
		else
			tasklet_schedule(&channel->fdma_error);
	}
}

static inline void fdma_handle_fdma_completion_irq(struct fdma_channel *channel)
{
	struct fdma *fdma = channel->fdma;
	void (*comp_cb)(unsigned long) = channel->params->comp_cb;
	unsigned long comp_cb_parm = channel->params->comp_cb_parm;

	spin_lock(&fdma->channels_lock);

	switch (fdma_get_engine_status(channel)) {
	case FDMA_CHANNEL_PAUSED:
		switch (channel->sw_state) {
		case FDMA_RUNNING:	/* Hit a pause node */
		case FDMA_PAUSING:
			channel->sw_state = FDMA_PAUSED;
			break;
		case FDMA_STOPPING:
			writel(0, CMD_STAT_REG(channel->chan_num));
			channel->sw_state = FDMA_IDLE;
			break;
		default:
			BUG();
		}
		break;
	case FDMA_CHANNEL_IDLE:
		switch (channel->sw_state) {
		case FDMA_RUNNING:
		case FDMA_PAUSING:
		case FDMA_STOPPING:
			channel->sw_state = FDMA_IDLE;
			break;
		default:
			BUG();
		}
		break;
	case FDMA_CHANNEL_RUNNING:
		break;
	default:
		fdma_dbg(fdma, "ERR::FDMA2 unknown interrupt status \n");
	}

	spin_unlock(&fdma->channels_lock);

	wake_up(&channel->dma_chan->wait_queue);

	if (comp_cb) {
		if (channel->params->comp_cb_isr)
			comp_cb(comp_cb_parm);
		else
			tasklet_schedule(&channel->fdma_complete);
	}
}

static irqreturn_t fdma_irq(int irq, void *dev_id)
{
	struct fdma *fdma = dev_id;
	int chan_num;
	u32 status = readl(fdma->io_base + fdma->regs.int_sta);
	u32 masked = status & fdma->ch_status_mask;

	writel(masked, fdma->io_base + fdma->regs.int_clr);
	for (masked >>= fdma->ch_min * 2, chan_num = fdma->ch_min;
			masked != 0; masked >>= 2, chan_num++) {
		struct fdma_channel *channel = &fdma->channels[chan_num];
		/* error interrupts will raise boths bits, so check
		 * the err bit first */
		if (unlikely(masked & 2))
			fdma_handle_fdma_err_irq(channel);
		else if (masked & 1)
			fdma_handle_fdma_completion_irq(channel);
	}

	/* Here we check to see if there is still pending ints for the other
	 * dmac, if so rely on it to signal IRQ_HANDLED once all vectors are
	 * cleared, we return IRQ_NONE. Otherwise we have handled everything
	 * so we can now safely returnd IRQ_HANDLED to lower the IRQ. */
	return IRQ_RETVAL(!(status & (~fdma->ch_status_mask)));
}

/* Request lines management */

static struct fdma_req_router *fdma_req_router;

int fdma_register_req_router(struct fdma_req_router *router)
{
	BUG_ON(fdma_req_router);

	fdma_req_router = router;

	return 0;
}

void fdma_unregister_req_router(struct fdma_req_router *router)
{
	fdma_req_router = NULL;
}

static struct stm_dma_req *fdma_req_allocate(struct fdma_channel *channel,
		unsigned int soc_req_line)
{
	struct fdma *fdma = channel->fdma;
	struct stm_dma_req *req = NULL;
	int req_line = -EINVAL;

	spin_lock(&fdma->reqs_lock);

	if (fdma_req_router) {
		/* There is a request lines crossbar registered - we can
		 * use any of the available "local" request lines... */
		if (fdma->reqs_used_mask < ~0UL) {
			req_line = ffz(fdma->reqs_used_mask);

			if (fdma_req_router->route(fdma_req_router,
						soc_req_line, fdma->pdev->id,
						req_line) == 0)
				fdma->reqs_used_mask |= (1 << req_line);
			else
				req_line = -EBUSY;
		}
	} else {
		/* No crossbar - request lines are connected directly then */
		unsigned long mask = 1 << soc_req_line;

		BUG_ON(soc_req_line < 0 || soc_req_line >= FDMA_REQ_LINES);

		if ((fdma->reqs_used_mask & mask) == 0) {
			fdma->reqs_used_mask |= mask;
			req_line = soc_req_line;
		}
	}

	if (req_line >= 0) {
		req = &fdma->reqs[req_line];
		req->req_line = req_line;
	}

	spin_unlock(&fdma->reqs_lock);

	return req;
}

static void fdma_req_free(struct fdma_channel *channel, struct stm_dma_req *req)
{
	struct fdma *fdma = channel->fdma;

	spin_lock(&fdma->reqs_lock);

	fdma->reqs_used_mask &= ~(1 << req->req_line);

	spin_unlock(&fdma->reqs_lock);
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * FIRMWARE DOWNLOAD & ENGINE INIT
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static void fdma_initialise(struct fdma *fdma)
{
/* These pokes come from the current STAPI tree.
 * The three magic vals are pokes to undocumented regs so
 * we don't know what they mean.
 *
 * The effect is to turn on and initialise the clocks
 * and set all channels off */

	/* clear the status regs MBOX & IRQ */
	writel(CLEAR_WORD, fdma->io_base + fdma->regs.int_clr);
	writel(CLEAR_WORD, fdma->io_base + fdma->regs.cmd_clr);

	/* Enable the FDMA block */
	writel(1, fdma->io_base + fdma->regs.sync_reg);
	writel(5, fdma->io_base + fdma->regs.clk_gate);
	writel(0, fdma->io_base + fdma->regs.clk_gate);

}
/* this function enables messaging and intr generation for all channels &
 * starts the fdma running */
static int fdma_enable_all_channels(struct fdma *fdma)
{
	writel(CLEAR_WORD, fdma->io_base + fdma->regs.int_mask);
	writel(CLEAR_WORD, fdma->io_base + fdma->regs.cmd_mask);
	writel(1, fdma->io_base + fdma->regs.en);
	return readl(fdma->io_base + fdma->regs.en) & 1;
}
static int fdma_disable_all_channels(struct fdma *fdma)
{
	writel(0, fdma->io_base + fdma->regs.int_mask);
	writel(0, fdma->io_base + fdma->regs.cmd_mask);
	writel(0, fdma->io_base + fdma->regs.en);
	return readl(fdma->io_base + fdma->regs.en) & ~1;
}

static void fdma_reset_channels(struct fdma *fdma)
{
	int chan_num;

	for (chan_num = fdma->ch_min; chan_num <= fdma->ch_max; chan_num++)
		writel(0, CMD_STAT_REG(chan_num));
}

static struct stm_dma_req *fdma_configure_pace_channel(
		struct fdma_channel *channel,
		struct stm_dma_req_config *req_config)
{
	struct fdma *fdma = channel->fdma;
	void __iomem *req_base_reg = fdma->io_base + fdma->regs.req_ctln;
	struct stm_dma_req *req;

	req = fdma_req_allocate(channel, req_config->req_line);

	if (req) {
		u32 req_ctl = 0;

		req_ctl |= (req_config->hold_off & 0x0f) <<  0; /* Bits 3..0 */
		req_ctl |= (req_config->opcode & 0x0f) <<  4; /* 7..4 */
		req_ctl |= (req_config->rw & 0x01) << 14; /* 14 */
		req_ctl |= (req_config->initiator & 0x03) << 22; /* 23..22 */
		req_ctl |= ((req_config->count - 1) & 0x1f) << 24; /* 28..24 */
		req_ctl |= (req_config->increment & 0x01) << 29; /* 29 */

		writel(req_ctl, req_base_reg +
				(req->req_line * CMD_STAT_OFFSET));
	}

	return req;
}

static int fdma_register_caps(struct fdma *fdma)
{
	int chan_num, dma_chan_num;
	int err;
	int num_caps = fdma->ch_max - fdma->ch_min + 1;
	struct dma_chan_caps dmac_caps[num_caps];
	static const char *hb_caps[] = {STM_DMA_CAP_HIGH_BW, NULL};
	static const char *lb_caps[] = {STM_DMA_CAP_LOW_BW, NULL};
	static const char *eth_caps[] = {STM_DMA_CAP_ETH_BUF, NULL};

	for (chan_num = fdma->ch_min, dma_chan_num = 0;
			chan_num <= fdma->ch_max;
			chan_num++, dma_chan_num++) {
		dmac_caps[dma_chan_num].ch_num = dma_chan_num;
		switch (chan_num) {
		case 0 ... 3:
			dmac_caps[dma_chan_num].caplist = hb_caps;
			break;
		case 11:
			dmac_caps[dma_chan_num].caplist = eth_caps;
			break;
		default:
			dmac_caps[dma_chan_num].caplist = lb_caps;
			break;
		}
	}

	err = register_chan_caps(fdma->name, dmac_caps);
	if (err != 0) {
		fdma_dbg(fdma, "%s %s failed to register capabilities err-%d\n",
				__FUNCTION__, fdma->name, err);
		return -ENODEV;
	}

	return 0;
}

static int fdma_run_initialise_sequence(struct fdma *fdma)
{
	fdma->llu_pool = dma_pool_create(fdma->name, NULL,
			sizeof(struct fdma_llu_entry), 32, 0);

	if (fdma->llu_pool == NULL) {
		fdma_dbg(fdma, "%s Can't allocate dma_pool memory\n",
				__FUNCTION__);
		return -ENOMEM;
	}

	fdma_initialise(fdma);
	fdma_reset_channels(fdma);

	if (!fdma_enable_all_channels(fdma))
		return -ENODEV;

	return 0;
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * FIRMWARE DOWNLOAD & ENGINE INIT
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static void fdma_get_fw_revision(struct fdma *fdma, int *major, int *minor)
{
	int reg = readl(fdma->io_base + fdma->regs.rev_id);

	*major = (reg & 0xff0000) >> 16;
	*minor = (reg & 0xff00) >> 8;
}

static void fdma_get_hw_revision(struct fdma *fdma, int *major, int *minor)
{
	*major = readl(fdma->io_base + fdma->regs.id);
	*minor = readl(fdma->io_base + fdma->regs.ver);
}

static int fdma_segment_ok(signed long offset, unsigned long size,
			   struct stm_plat_fdma_ram *ram)
{
	return (offset >= ram->offset) &&
		((offset + size) <= (ram->offset + ram->size));
}

static int fdma_load_segment(struct fdma *fdma, struct ELFinfo *elfinfo, int i)
{
	Elf32_Phdr *phdr = &elfinfo->progbase[i];
	void *data = elfinfo->base;
	signed long offset = phdr->p_paddr - fdma->phys_mem->start;
	unsigned long size = phdr->p_memsz;

	if (!(fdma_segment_ok(offset, size, &fdma->hw->dmem) ||
	      fdma_segment_ok(offset, size, &fdma->hw->imem)))
	    return -EINVAL;

	memcpy_toio(fdma->io_base + offset, data + phdr->p_offset, size);

#ifdef CONFIG_HIBERNATION
	/*
	 * Save the segment in private datas
	 */
	fdma->segment_pm[i].size = size;
	fdma->segment_pm[i].offset = offset;
	fdma->segment_pm[i].data = kmalloc(size, GFP_KERNEL);

	if (!fdma->segment_pm[i].data)
		return -ENOMEM;

	memcpy(fdma->segment_pm[i].data, data + phdr->p_offset, size);
#endif
	return 0;
}

static int fdma_load_elf(const struct firmware *fw, struct fdma *fdma)
{
	struct ELFinfo *elfinfo = NULL;
	int i;
	int fw_major, fw_minor;
	int hw_major, hw_minor;
	int res;

	if (!fw) {
		fdma_info(fdma, "Unable to load FDMA firmware: not present?\n");
		return -EINVAL;
	}

	elfinfo = (struct ELFinfo *)ELF_initFromMem((uint8_t *)fw->data,
							fw->size, 0);
	if (elfinfo == NULL)
		return -ENOMEM;

	if ((elfinfo->header->e_type != ET_EXEC) ||
	    (elfinfo->header->e_machine != EM_SLIM) ||
	    (elfinfo->header->e_flags != EF_SLIM_FDMA)) {
		res = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < elfinfo->header->e_phnum; i++)
		if (elfinfo->progbase[i].p_type == PT_LOAD) {
			res = fdma_load_segment(fdma, elfinfo, i);
			if (res)
				goto fail;
		}

	ELF_free(elfinfo);
	fdma_get_hw_revision(fdma, &hw_major, &hw_minor);
	fdma_get_fw_revision(fdma, &fw_major, &fw_minor);
	fdma_info(fdma, "SLIM hw %d.%d, FDMA fw %d.%d\n",
			hw_major, hw_minor, fw_major, fw_minor);

	if (fdma_run_initialise_sequence(fdma) != 0)
		return -ENODEV;

	fdma->firmware_loaded = 1;
	return 1;

fail:
	ELF_free(elfinfo);
	return res;
}

static int fdma_do_bootload(struct fdma *fdma)
{
	int err;
	int result;

	result = snprintf(fdma->fw_name, sizeof(fdma->fw_name),
			  "fdma_%s_%d.elf", get_cpu_subtype(&current_cpu_data),
			  (fdma->pdev->id == -1) ? 0 : fdma->pdev->id);
	BUG_ON(result >= sizeof(fdma->fw_name)); /* was the string truncated? */

	fdma_dbg(fdma, "FDMA: Loading ELF Firmware (%s)...\n", fdma->fw_name);

	err = request_firmware_nowait(THIS_MODULE, 1, fdma->fw_name,
					&fdma->pdev->dev, (struct fdma *)fdma,
					(void *)fdma_load_elf);
	if (err)
		return -ENOMEM;

	wake_up(&fdma->fw_load_q);

	return 0;
}

static int fdma_load_firmware(struct fdma *fdma)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fdma->channels_lock, irqflags);
	switch (fdma->firmware_loaded) {
	case 0:
		fdma->firmware_loaded = -1;
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
		if (fdma_do_bootload(fdma) != 0) {
			fdma->firmware_loaded = 0;
			return -ENOMEM;
		}

		return (fdma->firmware_loaded == 1) ? 0: -ENODEV;
	case 1:
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
		return 0;
	default:
	case -1:
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
		wait_event_interruptible(fdma->fw_load_q,
				(fdma->firmware_loaded == 1));
		if (!fdma->firmware_loaded)
			return -ENODEV;
		else
			return 0;
	}

	return 0;
}

static int fdma_check_firmware_state(struct fdma *fdma)
{
	return (fdma->firmware_loaded) ? 0 : fdma_load_firmware(fdma);
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * Linux -SH DMA API hooks
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

/* returns the number of bytes left to transfer for the current node */
static int fdma_get_residue(struct dma_channel *dma_chan)
{
	struct fdma_channel *channel = dma_chan->priv_data;
	struct fdma *fdma = channel->fdma;
	struct stm_dma_params *params = channel->params;
	unsigned long irqflags;
	u32 count = 0;

	spin_lock_irqsave(&fdma->channels_lock, irqflags);

	if (likely(channel->sw_state != FDMA_IDLE)) {
		struct fdma_xfer_descriptor *desc =
			(struct fdma_xfer_descriptor *)params->priv;
		void __iomem *chan_base = fdma->io_base +
				(channel->chan_num * NODE_DATA_OFFSET);
		unsigned long current_node_phys;
		unsigned long stat1, stat2;
		struct fdma_llu_node *current_node;
		int node_num;

		/* Get info about current node */
		do {
			stat1 = readl(CMD_STAT_REG(channel->chan_num));
			count = readl(chan_base + fdma->regs.cntn);
			stat2 = readl(CMD_STAT_REG(channel->chan_num));
		} while (stat1 != stat2);

		current_node_phys = stat1 & ~0x1f;
		for (node_num = 0, current_node = desc->llu_nodes;
				current_node->dma_addr != current_node_phys;
				node_num++, current_node++)
			BUG_ON(node_num == desc->alloced_nodes);

		switch (stat1 & 3) {
		case FDMA_CHANNEL_IDLE:
			/* Channel has stopped, but we haven't taken
			 * the interrupt to change the ->sw_state
			 * field yet. We could legitimatly return zero
			 * here, but instead pretend we haven't quite
			 * finished yet. Is this the right thing to
			 * do? */
			count = 1;
			goto unlock;

		case FDMA_CHANNEL_RUNNING:
		case FDMA_CHANNEL_PAUSED:
			/* Unfortuntaly the firmware appears to modify
			 * CMD_STAT before it has modifed the COUNT.
			 * However we write the count in
			 * fdma_start_channel() so can assume it is
			 * valid. */
			break;

		case CMDSTAT_FDMA_START_CHANNEL:
			/* Channel hasn't started running yet, so count
			 * hasn't yet been loaded from the node. But again
			 * the value was written in fdma_start_channel()
			 * so the value read from hardware is valid. */
			break;
		}

		while (++node_num < desc->alloced_nodes) {
			current_node++;
			count += current_node->virt_addr->size_bytes;
		}
	}

unlock:
	spin_unlock_irqrestore(&fdma->channels_lock, irqflags);

	return count;
}

/* must only be called when channel is in paused state */
static int fdma_unpause(struct fdma_channel *channel)
{
	struct fdma *fdma = channel->fdma;
	unsigned long irqflags = 0;
	u32 cmd_sta_value;

	spin_lock_irqsave(&fdma->channels_lock, irqflags);
	if (channel->sw_state != FDMA_PAUSED) {
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
		return -EBUSY;
	}

	cmd_sta_value = readl(CMD_STAT_REG(channel->chan_num));
	cmd_sta_value &= ~CMDSTAT_FDMA_CMD_MASK;
	cmd_sta_value |= CMDSTAT_FDMA_RESTART_CHANNEL;
	writel(cmd_sta_value, CMD_STAT_REG(channel->chan_num));

	writel(MBOX_CMD_START_CHANNEL << (channel->chan_num * 2),
			fdma->io_base + fdma->regs.cmd_set);
	channel->sw_state = FDMA_RUNNING;

	spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
	return 0;
}

static int fdma_pause(struct fdma_channel *channel, int flush)
{
	struct fdma *fdma = channel->fdma;
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fdma->channels_lock, irqflags);
	switch (channel->sw_state) {
	case FDMA_IDLE:
	case FDMA_CONFIGURED:
		/* Hardware isn't set up yet, so treat this as an error */
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
		return -EBUSY;
	case FDMA_PAUSED:
		/* Hardware is already paused */
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
		return 0;
	case FDMA_RUNNING:
		/* Hardware is running, send the command */
		writel((flush ? MBOX_CMD_FLUSH_CHANNEL : MBOX_CMD_PAUSE_CHANNEL)
				<< (channel->chan_num * 2),
				fdma->io_base + fdma->regs.cmd_set);
		/* Fall through */
	case FDMA_PAUSING:
	case FDMA_STOPPING:
		/* Hardware is pausing already, wait for interrupt */
		channel->sw_state = FDMA_PAUSING;
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
#if 0
		/* In some cases this is called from a context which cannot
		 * block, so disable the wait at the moment. */
		wait_event(channel->cur_cfg->wait_queue,
				channel->sw_state == FDMA_PAUSED);
#endif
		break;
	}

	return 0;
}

static int fdma_request(struct dma_channel *dma_chan)
{
	struct fdma_channel *channel = dma_chan->priv_data;
	struct fdma *fdma = channel->fdma;

	if (fdma_check_firmware_state(fdma) == 0)
		return 0;

	return -ENOSYS;
}

static int fdma_stop(struct fdma_channel *channel)
{
	struct fdma *fdma = channel->fdma;
	unsigned long cmd_val = MBOX_CMD_PAUSE_CHANNEL <<
			(channel->chan_num * 2);
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fdma->channels_lock, irqflags);
	switch (channel->sw_state) {
	case FDMA_IDLE:
	case FDMA_CONFIGURED:
	case FDMA_PAUSED:
		/* Hardware is already idle, simply change state */
		channel->sw_state = FDMA_IDLE;
		writel(0, CMD_STAT_REG(channel->chan_num));
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
		break;
	case FDMA_RUNNING:
		/* Hardware is running, send the command */
		writel(cmd_val, fdma->io_base + fdma->regs.cmd_set);
		/* Fall through */
	case FDMA_PAUSING:
	case FDMA_STOPPING:
		/* Hardware is pausing already, wait for interrupt */
		channel->sw_state = FDMA_STOPPING;
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
#if 0
		/* In some cases this is called from a context which cannot
		 * block, so disable the wait at the moment. */
		wait_event(channel->cur_cfg->wait_queue,
				channel->sw_state == FDMA_IDLE);
#endif
		break;
	}

	return 0;
}

static int fdma_free_params(struct stm_dma_params *params)
{
	struct fdma *fdma = params->params_ops_priv;
	struct stm_dma_params *this;

	for (this = params; this; this = this->next) {
		struct fdma_xfer_descriptor *desc = this->priv;
		if (desc) {
			fdma_resize_nodelist_mem(fdma, desc, 0, 0);
			kfree(desc);
		}
	}

	return 0;
}

static struct params_ops fdma_params_ops = {
	.free_params = fdma_free_params
};

/* Compile params part 1: generate template nodes */
static int fdma_compile1(struct fdma *fdma, struct stm_dma_params *params)
{
	struct stm_dma_params *this;

	for (this = params; this; this = this->next) {
		struct fdma_xfer_descriptor *desc = this->priv;

		if (desc != NULL)
			continue;

		desc = kzalloc(sizeof(struct fdma_xfer_descriptor),
				params->context);
		if (desc == NULL)
			return -ENOMEM;
		this->priv = desc;

		if (this->mode == MODE_SRC_SCATTER)
			desc->extrapolate_fn = fdma_extrapolate_sg_src;
		else if (this->mode == MODE_DST_SCATTER)
			desc->extrapolate_fn = fdma_extrapolate_sg_dst;
		else
			desc->extrapolate_fn = fdma_extrapolate_simple;

		if (this->mode == MODE_PACED)
			fdma_setup_paced_node(this, &desc->template_llu);
		else
			fdma_setup_freerunning_node(this, &desc->template_llu);

		/* For any 1D transfers, line_len = nbytes */
		desc->extrapolate_line_len = !((DIM_SRC(this->dim) == 2) ||
				(DIM_DST(this->dim) == 2));
	}

	return 0;
}

/* Compile params part 2: allocate node list */
static int fdma_compile2(struct fdma *fdma, struct stm_dma_params *params)
{
	struct stm_dma_params *this;
	int numnodes = 0;
	struct fdma_xfer_descriptor *desc;

	for (this = params; this; this = this->next) {
		if (this->mode == MODE_SRC_SCATTER ||
				this->mode == MODE_DST_SCATTER)
			numnodes += this->sglen;
		else
			numnodes++;
	}

	desc = params->priv;
	if (desc->alloced_nodes < numnodes) {
		int err = fdma_resize_nodelist_mem(fdma, desc, numnodes,
				params->context);

		if (err)
			return err;
	}

	return 0;
}

/* Compile params part 3: extrapolate */
static int fdma_compile3(struct fdma *fdma, struct stm_dma_params *params)
{
	struct stm_dma_params *this;
	struct fdma_xfer_descriptor *this_desc;
	struct fdma_llu_node *first_node, *last_node, *node;

	this = params;
	this_desc = (struct fdma_xfer_descriptor *)this->priv;
	first_node = this_desc->llu_nodes;

	node = first_node;
	while (1) {
		last_node = this_desc->extrapolate_fn(this, this_desc, node);

		this = this->next;
		if (this == NULL)
			break;

		this_desc = (struct fdma_xfer_descriptor *)this->priv;
		node = last_node + 1;
		last_node->virt_addr->next_item = node->dma_addr;
	}

	if (params->circular_llu)
		last_node->virt_addr->next_item = first_node->dma_addr;
	else
		last_node->virt_addr->next_item = 0;

	return 0;
}

static int fdma_compile_params(struct fdma_channel *channel,
		struct stm_dma_params *params)
{
	struct fdma *fdma = channel->fdma;
	int res;

	res = fdma_compile1(fdma, params);
	if (res)
		return res;

	res = fdma_compile2(fdma, params);
	if (res)
		return res;

	res = fdma_compile3(fdma, params);
	if (res == 0) {
		params->params_ops = &fdma_params_ops;
		params->params_ops_priv = fdma;
	}

	return res;
}

static void fdma_free(struct dma_channel *dma_chan)
{
	struct fdma_channel *channel = dma_chan->priv_data;
	struct fdma *fdma = channel->fdma;
	unsigned long irq_flags = 0;

	spin_lock_irqsave(&fdma->channels_lock, irq_flags);

	if (channel->sw_state != FDMA_IDLE) {
		spin_unlock_irqrestore(&fdma->channels_lock, irq_flags);
		fdma_dbg(fdma, "%s channel not idle\n", __FUNCTION__);
		return;
	}

	BUG_ON(fdma_get_engine_status(channel) != FDMA_CHANNEL_IDLE);

	spin_unlock_irqrestore(&fdma->channels_lock, irq_flags);
}

/* Note although this returns an int, the dma-api code throws this away. */
static int fdma_configure(struct dma_channel *dma_chan,
		unsigned long flags)
{
	struct fdma_channel *channel = dma_chan->priv_data;
	struct fdma *fdma = channel->fdma;
	struct stm_dma_params *params = (struct stm_dma_params *)flags;
	unsigned long irq_flags = 0;

	spin_lock_irqsave(&fdma->channels_lock, irq_flags);
	if (channel->sw_state != FDMA_IDLE) {
		spin_unlock_irqrestore(&fdma->channels_lock, irq_flags);
		fdma_dbg(fdma, "%s channel not idle\n", __FUNCTION__);
		return -EBUSY;
	}

	if (!((struct fdma_xfer_descriptor *)(params->priv))->llu_nodes) {
		fdma_dbg(fdma, "%s no nodelist alloced\n", __FUNCTION__);
		spin_unlock_irqrestore(&fdma->channels_lock, irq_flags);
		return -ENOMEM;
	}

	/* Now we are associating the compiled transfer llu & params to the
	 * channel */
	channel->params = params;
	tasklet_init(&channel->fdma_complete, params->comp_cb,
			(unsigned long)params->comp_cb_parm);
	tasklet_init(&channel->fdma_error, params->err_cb,
			(unsigned long)params->err_cb_parm);
	channel->sw_state = FDMA_CONFIGURED;

	spin_unlock_irqrestore(&fdma->channels_lock, irq_flags);

	return 0;
}

static int fdma_xfer(struct dma_channel *dma_chan,
		unsigned long sar, unsigned long dar,
		size_t count, unsigned int mode)
{
	struct fdma_channel *channel = dma_chan->priv_data;
	struct fdma *fdma = channel->fdma;
	struct fdma_xfer_descriptor *desc;
	unsigned long irqflags = 0;

	/* we need to check that the compile has been completed */
	spin_lock_irqsave(&fdma->channels_lock, irqflags);

	if (channel->sw_state != FDMA_CONFIGURED) {
		spin_unlock_irqrestore(&fdma->channels_lock, irqflags);
		return -EINVAL;
	}

	desc = (struct fdma_xfer_descriptor *)channel->params->priv;

	BUG_ON(fdma_get_engine_status(channel) != FDMA_CHANNEL_IDLE);

	fdma_start_channel(channel, desc->llu_nodes->dma_addr,
			desc->llu_nodes->virt_addr->size_bytes);
	channel->sw_state = FDMA_RUNNING;

	spin_unlock_irqrestore(&fdma->channels_lock, irqflags);

	return 0;
}

static int fdma_extended(struct dma_channel *dma_chan,
		unsigned long opcode, void *ext_param)
{
	struct fdma_channel *channel = dma_chan->priv_data;

	switch (opcode) {
	case STM_DMA_OP_FLUSH:
		return fdma_pause(channel, 1);
	case STM_DMA_OP_PAUSE:
		return fdma_pause(channel, 0);
	case STM_DMA_OP_UNPAUSE:
		return fdma_unpause(channel);
	case STM_DMA_OP_STOP:
		return fdma_stop(channel);
	case STM_DMA_OP_COMPILE:
		return fdma_compile_params(channel, ext_param);
	case STM_DMA_OP_STATUS:
		return fdma_get_engine_status(channel);
	case STM_DMA_OP_REQ_CONFIG:
		return (int)fdma_configure_pace_channel(channel, ext_param);
	case STM_DMA_OP_REQ_FREE:
		fdma_req_free(channel, ext_param);
		return 0;
	default:
		return -ENOSYS;
	}
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * MODULE INIT & REGISTRATION
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static void __init fdma_channels_parse(struct fdma *fdma)
{
	char *str;
	int min, max;

	/* All channels available by default */
	fdma->ch_min = FDMA_MIN_CHANNEL;
	fdma->ch_max = FDMA_MAX_CHANNEL;

	if (fdma->fdma_num >= FDMA_MAX_DEVICES) {
		printk(KERN_WARNING "%s(): Increase FDMA_MAX_DEVICES!\n",
				__func__);
		return;
	}

	str = fdma_channels[fdma->fdma_num];
	if (!str) /* No parameter */
		return;

	/* Should be a number followed by a hyphen */
	if (get_option(&str, &min) != 3 || min < FDMA_MIN_CHANNEL) {
		printk(KERN_ERR "%s(): Wrong channels range '%s' for FDMA%d!\n",
				__func__, fdma_channels[fdma->fdma_num],
				fdma->fdma_num);
		return;
	}

	/* Skip the hyphen */
	str++;

	/* Should be a number followed by a '\0' */
	if (get_option(&str, &max) != 1 || *str || max > FDMA_MAX_CHANNEL) {
		printk(KERN_ERR "%s(): Wrong channels range '%s' for FDMA%d!\n",
				__func__, fdma_channels[fdma->fdma_num],
				fdma->fdma_num);
		return;
	}

	fdma->ch_min = min;
	fdma->ch_max = max;
}

static void stm_fdma_clk_xxable(struct fdma *fdma, int enable)
{
	int i;
	struct clk *clk;

	for (i = 0; i < FDMA_CLKS_NR; ++i) {
		clk = fdma->clks[i];
		if (!clk || IS_ERR(clk))
			continue;
		if (enable)
			clk_enable(clk);
		else
			clk_disable(clk);
	}
}

static void stm_fdma_clk_enable(struct fdma *fdma)
{
	stm_fdma_clk_xxable(fdma, 1);
}

static void stm_fdma_clk_disable(struct fdma *fdma)
{
	stm_fdma_clk_xxable(fdma, 0);
}

static struct dma_ops fdma_ops = {
	.request	= fdma_request,
	.free		= fdma_free,
	.get_residue	= fdma_get_residue,
	.xfer		= fdma_xfer,
	.configure	= fdma_configure,
	.extend		= fdma_extended,
};

static int __init fdma_driver_probe(struct platform_device *pdev)
{
	static __initdata char *fdma_clks_n[FDMA_CLKS_NR] = {
		[FDMA_SLIM_CLK] = "fdma_slim_clk",
		[FDMA_HI_CLK] = "fdma_hi_clk",
		[FDMA_LOW_CLK] = "fdma_low_clk",
		[FDMA_IC_CLK] = "fdma_ic_clk",
	};
	struct stm_plat_fdma_data *plat_data;
	struct fdma *fdma = NULL;
	struct resource *res;
	int chan_num, i;
	int err = 0;

	plat_data = pdev->dev.platform_data;

	fdma = kzalloc(sizeof(struct fdma), GFP_KERNEL);
	if (fdma == NULL)
		return -ENOMEM;

	for (i = 0; i < FDMA_CLKS_NR; ++i) {
		fdma->clks[i] = clk_get(&pdev->dev, fdma_clks_n[i]);
		if (!fdma->clks[i] || IS_ERR(fdma->clks[i]))
			pr_warning("%s: clk %s not found\n",
				dev_name(&pdev->dev), fdma_clks_n[i]);
	}

	stm_fdma_clk_enable(fdma);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	fdma->phys_mem = request_mem_region(res->start,
			res->end - res->start + 1, pdev->name);
	if (fdma->phys_mem == NULL)
		return -EBUSY;

	fdma->io_base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (fdma->io_base == NULL)
		return -EINVAL;

	fdma->pdev = pdev;
	fdma->fdma_num = (pdev->id != -1 ? pdev->id : 0);
	fdma_channels_parse(fdma);

	fdma->ch_status_mask = ((1ULL << ((fdma->ch_max + 1) * 2)) - 1ULL) ^
			((1 << (fdma->ch_min * 2)) - 1);

	fdma->fw = plat_data->fw;
	fdma->hw = plat_data->hw;

	fdma->regs.id = fdma->hw->slim_regs.id;
	fdma->regs.ver = fdma->hw->slim_regs.ver;
	fdma->regs.en = fdma->hw->slim_regs.en;
	fdma->regs.clk_gate = fdma->hw->slim_regs.clk_gate;
	fdma->regs.rev_id = fdma->fw->rev_id;
	fdma->regs.cmd_statn = fdma->fw->cmd_statn;
	fdma->regs.req_ctln = fdma->fw->req_ctln;
	fdma->regs.ptrn = fdma->fw->ptrn;
	fdma->regs.cntn = fdma->fw->cntn;
	fdma->regs.saddrn = fdma->fw->saddrn;
	fdma->regs.daddrn = fdma->fw->daddrn;
	fdma->regs.sync_reg = fdma->hw->periph_regs.sync_reg;
	fdma->regs.cmd_sta = fdma->hw->periph_regs.cmd_sta;
	fdma->regs.cmd_set = fdma->hw->periph_regs.cmd_set;
	fdma->regs.cmd_clr = fdma->hw->periph_regs.cmd_clr;
	fdma->regs.cmd_mask = fdma->hw->periph_regs.cmd_mask;
	fdma->regs.int_sta = fdma->hw->periph_regs.int_sta;
	fdma->regs.int_set = fdma->hw->periph_regs.int_set;
	fdma->regs.int_clr = fdma->hw->periph_regs.int_clr;
	fdma->regs.int_mask = fdma->hw->periph_regs.int_mask;

	/* Req lines 0 and 31 are basically unusable */
	fdma->reqs_used_mask = (1 << 31) | (1 << 0);
	spin_lock_init(&fdma->reqs_lock);

	spin_lock_init(&fdma->channels_lock);
	init_waitqueue_head(&fdma->fw_load_q);

	fdma->dma_info.nr_channels = fdma->ch_max - fdma->ch_min + 1;
	fdma->dma_info.ops = &fdma_ops;
	fdma->dma_info.flags = DMAC_CHANNELS_TEI_CAPABLE;
	strlcpy(fdma->name, STM_DMAC_ID, FDMA_NAME_LEN);
	if (pdev->id != -1) {
		int len = strlen(fdma->name);
		snprintf(fdma->name + len, FDMA_NAME_LEN - len, ".%d",
				pdev->id);
	}
	fdma->dma_info.name = fdma->name;

	if (register_dmac(&fdma->dma_info) != 0)
		printk(KERN_ERR "%s(): Error Registering DMAC\n", __func__);

	for (chan_num = fdma->ch_min; chan_num <= fdma->ch_max; chan_num++) {
		struct fdma_channel *channel = &fdma->channels[chan_num];
		struct dma_channel *dma_chan;

		channel->chan_num = chan_num;
		channel->fdma = fdma;

		dma_chan = get_dma_channel(chan_num - fdma->ch_min +
					   fdma->dma_info.first_vchannel_nr);

		dma_chan->priv_data = channel;
		channel->dma_chan = dma_chan;
	}

	fdma->irq = platform_get_irq(pdev, 0);
	err = request_irq(fdma->irq, fdma_irq,
			 IRQF_DISABLED | IRQF_SHARED, fdma->name, fdma);
	if (err < 0)
		panic("Cant Register irq %d for FDMA engine err %d\n",
				fdma->irq, err);

	fdma_register_caps(fdma);

	fdma_check_firmware_state(fdma);

	platform_set_drvdata(pdev, fdma);

	return 0;
}

static int fdma_driver_remove(struct platform_device *pdev)
{
	struct fdma *fdma = platform_get_drvdata(pdev);

	fdma_disable_all_channels(fdma);
	stm_fdma_clk_disable(fdma);
	iounmap(fdma->io_base);
	dma_pool_destroy(fdma->llu_pool);
	free_irq(fdma->irq, fdma);
	unregister_dmac(&fdma->dma_info);
	release_resource(fdma->phys_mem);
	kfree(fdma);

	return 0;
}

#ifdef CONFIG_PM
static int fdma_suspend_freeze_noirq(struct device *dev)
{
	struct fdma *fdma = dev_get_drvdata(dev);

	if (fdma->firmware_loaded != 1)
		return 0;
	/*
	 * At this point the channel users are already
	 * suspended this makes safe the 'disable_all_channels'
	 * call.
	 * BTW the channels have to be stopped because
	 * we have to avoid any access on memory while
	 * it is in self-refresh
	 */

	fdma_disable_all_channels(fdma);

	stm_fdma_clk_disable(fdma);

	return 0;
}

static int fdma_resume_noirq(struct device *dev)
{
	struct fdma *fdma = dev_get_drvdata(dev);

	if (fdma->firmware_loaded != 1)
		return 0;

	stm_fdma_clk_enable(fdma);

	fdma_enable_all_channels(fdma);

	return 0;
}

#ifdef CONFIG_HIBERNATION
static int fdma_restore_noirq(struct device *dev)
{
	struct fdma *fdma = dev_get_drvdata(dev);
	int i; /* index segments */

	if (fdma->firmware_loaded != 1)
		return 0;

	stm_fdma_clk_enable(fdma);

	for (i = 0; i < 2; ++i)
		memcpy_toio(fdma->io_base + fdma->segment_pm[i].offset,
			fdma->segment_pm[i].data,
			fdma->segment_pm[i].size);

	fdma_initialise(fdma);
	fdma_reset_channels(fdma);

	if (!fdma_enable_all_channels(fdma))
		return -ENODEV;
	return 0;
}
#endif

static struct dev_pm_ops fdma_pm_ops = {
	.suspend_noirq = fdma_suspend_freeze_noirq,
	.resume_noirq = fdma_resume_noirq,
#ifdef CONFIG_HIBERNATION
	.freeze_noirq = fdma_suspend_freeze_noirq,
	.restore_noirq = fdma_restore_noirq,
#endif
};
#else
static struct dev_pm_ops fdma_pm_ops;
#endif

static struct platform_driver fdma_driver = {
	.driver.name = "stm-fdma",
	.driver.pm = &fdma_pm_ops,
	.probe = fdma_driver_probe,
	.remove = fdma_driver_remove,
};

static int __init fdma_init(void)
{
	return platform_driver_register(&fdma_driver);
}

static void __exit fdma_exit(void)
{
	platform_driver_unregister(&fdma_driver);
}

module_init(fdma_init)
module_exit(fdma_exit)
