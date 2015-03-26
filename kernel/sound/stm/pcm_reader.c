/*
 *   STMicroelectronics System-on-Chips' PCM reader driver
 *
 *   Copyright (c) 2005-2011 STMicroelectronics Limited
 *
 *   Author: Pawel Moll <pawel.moll@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <linux/stm/pad.h>
#include <linux/stm/stm-dma.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/pcm_params.h>

#include "common.h"
#include "reg_aud_pcmin.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Some hardware-related definitions
 */

#define DEFAULT_FORMAT (SND_STM_FORMAT__I2S | \
		SND_STM_FORMAT__SUBFRAME_32_BITS)



/*
 * PCM reader instance definition
 */

struct snd_stm_pcm_reader {
	/* System informations */
	struct snd_stm_pcm_reader_info *info;
	struct device *device;
	struct snd_pcm *pcm;
	int ver; /* IP version, used by register access macros */

	/* Resources */
	struct resource *mem_region;
	void *base;
	unsigned long fifo_phys_address;
	unsigned int irq;
	int fdma_channel;

	/* Environment settings */
	struct snd_pcm_hw_constraint_list channels_constraint;
	struct snd_stm_conv_source *conv_source;

	/* Runtime data */
	struct snd_stm_conv_group *conv_group;
	struct snd_stm_buffer *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;
	int fdma_max_transfer_size;
	struct stm_dma_params *fdma_params_list;
	struct stm_dma_req *fdma_request;
	int running;
	struct stm_pad_state *pads;

	snd_stm_magic_field;
};



/*
 * Capturing engine implementation
 */

static irqreturn_t snd_stm_pcm_reader_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct snd_stm_pcm_reader *pcm_reader = dev_id;
	unsigned int status;

	snd_stm_printd(2, "snd_stm_pcm_reader_irq_handler(irq=%d, "
			"dev_id=0x%p)\n", irq, dev_id);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = get__AUD_PCMIN_ITS(pcm_reader);
	set__AUD_PCMIN_ITS_CLR(pcm_reader, status);
	preempt_enable();

	/* Overflow? */
	if (unlikely(status & mask__AUD_PCMIN_ITS__OVF__PENDING(pcm_reader))) {
		snd_stm_printe("Overflow detected in PCM reader '%s'!\n",
			       dev_name(pcm_reader->device));

		snd_pcm_stop(pcm_reader->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;
	}

	/* Some alien interrupt??? */
	BUG_ON(result != IRQ_HANDLED);

	return result;
}

static void snd_stm_pcm_reader_callback_node_done(unsigned long param)
{
	struct snd_stm_pcm_reader *pcm_reader =
			(struct snd_stm_pcm_reader *)param;

	snd_stm_printd(2, "snd_stm_pcm_reader_callback_node_done(param=0x%lx"
			")\n", param);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	if (!pcm_reader->running)
		return;

	snd_stm_printd(2, "Period elapsed ('%s')\n",
			dev_name(pcm_reader->device));

	snd_pcm_period_elapsed(pcm_reader->substream);
}

static void snd_stm_pcm_reader_callback_node_error(unsigned long param)
{
	struct snd_stm_pcm_reader *pcm_reader =
			(struct snd_stm_pcm_reader *)param;

	snd_stm_printd(2, "snd_stm_pcm_reader_callback_node_error(param=0x%lx"
			")\n", param);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	if (!pcm_reader->running)
		return;

	snd_stm_printe("Error during FDMA transfer in reader '%s'!\n",
		       dev_name(pcm_reader->device));

	snd_pcm_stop(pcm_reader->substream, SNDRV_PCM_STATE_XRUN);
}

static struct snd_pcm_hardware snd_stm_pcm_reader_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE),

	/* Keep in mind that we are working in slave mode, so sampling
	 * rate is determined by external components... */
	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 10,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* Values below were worked out mostly basing on ST media player
	 * requirements. They should, however, fit most "normal" cases...
	 * Note: period_bytes_min defines minimum time between FDMA transfer
	 * interrupts... Keep it large enough not to kill the system... */

	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

static int snd_stm_pcm_reader_open(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "snd_stm_pcm_reader_open(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));
	BUG_ON(!runtime);

	snd_pcm_set_sync(substream);  /* TODO: ??? */

	/* Get attached converters handle */

	pcm_reader->conv_group =
			snd_stm_conv_request_group(pcm_reader->conv_source);
	if (pcm_reader->conv_group)
		snd_stm_printd(1, "'%s' is attached to '%s' converter(s)...\n",
				dev_name(pcm_reader->device),
				snd_stm_conv_get_name(pcm_reader->conv_group));
	else
		snd_stm_printd(1, "No converter attached to '%s'!\n",
				dev_name(pcm_reader->device));

	/* Set up constraints & pass hardware capabilities info to ALSA */

	result = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_CHANNELS,
			&pcm_reader->channels_constraint);
	if (result < 0) {
		snd_stm_printe("Can't set channels constraint!\n");
		return result;
	}

	/* Buffer size must be an integer multiple of a period size to use
	 * FDMA nodes as periods... Such thing will ensure this :-O */
	result = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (result < 0) {
		snd_stm_printe("Can't set periods constraint!\n");
		return result;
	}

	/* Make the period (so buffer as well) length (in bytes) a multiply
	 * of a FDMA transfer bytes (which varies depending on channels
	 * number and sample bytes) */
	result = snd_stm_pcm_hw_constraint_transfer_bytes(runtime,
			pcm_reader->fdma_max_transfer_size * 4);
	if (result < 0) {
		snd_stm_printe("Can't set buffer bytes constraint!\n");
		return result;
	}

	runtime->hw = snd_stm_pcm_reader_hw;

	/* Interrupt handlers will need the substream pointer... */
	pcm_reader->substream = substream;

	return 0;
}

static int snd_stm_pcm_reader_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_pcm_reader_close(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	if (pcm_reader->conv_group) {
		snd_stm_conv_release_group(pcm_reader->conv_group);
		pcm_reader->conv_group = NULL;
	}

	pcm_reader->substream = NULL;

	return 0;
}

static int snd_stm_pcm_reader_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "snd_stm_pcm_reader_hw_free(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));
	BUG_ON(!runtime);

	/* This callback may be called more than once... */

	if (snd_stm_buffer_is_allocated(pcm_reader->buffer)) {
		/* Let the FDMA stop */
		dma_wait_for_completion(pcm_reader->fdma_channel);

		/* Free buffer */
		snd_stm_buffer_free(pcm_reader->buffer);

		/* Free FDMA parameters (whole list) */
		dma_params_free(pcm_reader->fdma_params_list);
		dma_req_free(pcm_reader->fdma_channel,
				pcm_reader->fdma_request);
		kfree(pcm_reader->fdma_params_list);
	}

	return 0;
}

static int snd_stm_pcm_reader_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, period_bytes, periods, frame_bytes, transfer_bytes;
	unsigned int transfer_size;
	struct stm_dma_req_config fdma_req_config = {
		.rw        = REQ_CONFIG_READ,
		.opcode    = REQ_CONFIG_OPCODE_4,
		.increment = 0,
		.hold_off  = 0,
		.initiator = pcm_reader->info->fdma_initiator,
	};
	int i;

	snd_stm_printd(1, "snd_stm_pcm_reader_hw_params(substream=0x%p,"
			" hw_params=0x%p)\n", substream, hw_params);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));
	BUG_ON(!runtime);

	/* This function may be called many times, so let's be prepared... */
	if (snd_stm_buffer_is_allocated(pcm_reader->buffer))
		snd_stm_pcm_reader_hw_free(substream);

	/* Get the numbers... */

	buffer_bytes = params_buffer_bytes(hw_params);
	periods = params_periods(hw_params);
	period_bytes = buffer_bytes / periods;
	BUG_ON(periods * period_bytes != buffer_bytes);

	/* Allocate buffer */

	result = snd_stm_buffer_alloc(pcm_reader->buffer, substream,
			buffer_bytes);
	if (result != 0) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
			       buffer_bytes, dev_name(pcm_reader->device));
		result = -ENOMEM;
		goto error_buf_alloc;
	}

	/* Set FDMA transfer size (number of opcodes generated
	 * after request line assertion) */

	frame_bytes = snd_pcm_format_physical_width(params_format(hw_params)) *
			params_channels(hw_params) / 8;
	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bytes,
			pcm_reader->fdma_max_transfer_size * 4);
	transfer_size = transfer_bytes / 4;

	snd_stm_printd(1, "FDMA request trigger limit set to %d.\n",
			transfer_size);
	BUG_ON(buffer_bytes % transfer_bytes != 0);
	BUG_ON(transfer_size > pcm_reader->fdma_max_transfer_size);
	if (pcm_reader->ver > 3) {
		BUG_ON(transfer_size != 1 && transfer_size % 2 == 0);
		BUG_ON(transfer_size >
		       mask__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(pcm_reader));
		set__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(pcm_reader, transfer_size);
		set__AUD_PCMIN_FMT__BACK_STALLING__DISABLED(pcm_reader);

		/* This is a workaround for a problem in early releases
		 * of multi-channel PCM Readers with FIFO underrunning (!!!),
		 * caused by spurious request line generation... */
		if (pcm_reader->ver < 6 && transfer_size > 2)
			fdma_req_config.count = transfer_size / 2;
		else
			fdma_req_config.count = transfer_size;
	} else {
		fdma_req_config.count = transfer_size;
	}
	snd_stm_printd(1, "FDMA transfer size set to %d.\n",
			fdma_req_config.count);

	/* Configure FDMA transfer */

	pcm_reader->fdma_request = dma_req_config(pcm_reader->fdma_channel,
			pcm_reader->info->fdma_request_line, &fdma_req_config);
	if (!pcm_reader->fdma_request) {
		snd_stm_printe("Can't configure FDMA pacing channel for player"
			       " '%s'!\n", dev_name(pcm_reader->device));
		result = -EINVAL;
		goto error_req_config;
	}

	pcm_reader->fdma_params_list =
			kmalloc(sizeof(*pcm_reader->fdma_params_list) *
			periods, GFP_KERNEL);
	if (!pcm_reader->fdma_params_list) {
		snd_stm_printe("Can't allocate %d bytes for FDMA parameters "
				"list!\n", sizeof(*pcm_reader->fdma_params_list)
				* periods);
		result = -ENOMEM;
		goto error_params_alloc;
	}

	snd_stm_printd(1, "Configuring FDMA transfer nodes:\n");

	for (i = 0; i < periods; i++) {
		dma_params_init(&pcm_reader->fdma_params_list[i], MODE_PACED,
				STM_DMA_LIST_CIRC);

		if (i > 0)
			dma_params_link(&pcm_reader->fdma_params_list[i - 1],
					(&pcm_reader->fdma_params_list[i]));

		dma_params_comp_cb(&pcm_reader->fdma_params_list[i],
				snd_stm_pcm_reader_callback_node_done,
				(unsigned long)pcm_reader,
				STM_DMA_CB_CONTEXT_ISR);

		dma_params_err_cb(&pcm_reader->fdma_params_list[i],
				snd_stm_pcm_reader_callback_node_error,
				(unsigned long)pcm_reader,
				STM_DMA_CB_CONTEXT_ISR);

		/* Get callback every time a node is completed */
		dma_params_interrupts(&pcm_reader->fdma_params_list[i],
				STM_DMA_NODE_COMP_INT);

		dma_params_DIM_0_x_1(&pcm_reader->fdma_params_list[i]);

		dma_params_req(&pcm_reader->fdma_params_list[i],
				pcm_reader->fdma_request);

		snd_stm_printd(1, "- %d: %d bytes from 0x%08x\n", i,
				period_bytes,
				runtime->dma_addr + i * period_bytes);

		dma_params_addrs(&pcm_reader->fdma_params_list[i],
				pcm_reader->fifo_phys_address,
				runtime->dma_addr + i * period_bytes,
				period_bytes);
	}

	result = dma_compile_list(pcm_reader->fdma_channel,
				pcm_reader->fdma_params_list, GFP_KERNEL);
	if (result < 0) {
		snd_stm_printe("Can't compile FDMA parameters for"
			" reader '%s'!\n", dev_name(pcm_reader->device));
		goto error_compile_list;
	}

	return 0;

error_compile_list:
	kfree(pcm_reader->fdma_params_list);
error_params_alloc:
	dma_req_free(pcm_reader->fdma_channel,
			pcm_reader->fdma_request);
error_req_config:
	snd_stm_buffer_free(pcm_reader->buffer);
error_buf_alloc:
	return result;
}

static int snd_stm_pcm_reader_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int format, lr_pol;

	snd_stm_printd(1, "snd_stm_pcm_reader_prepare(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));
	BUG_ON(!runtime);

	/* Get format value from connected converter */

	if (pcm_reader->conv_group)
		format = snd_stm_conv_get_format(pcm_reader->conv_group);
	else
		format = DEFAULT_FORMAT;

	/* Number of bits per subframe (which is one channel sample)
	 * on input. */

	switch (format & SND_STM_FORMAT__SUBFRAME_MASK) {
	case SND_STM_FORMAT__SUBFRAME_32_BITS:
		snd_stm_printd(1, "- 32 bits per subframe\n");
		set__AUD_PCMIN_FMT__NBIT__32_BITS(pcm_reader);
		set__AUD_PCMIN_FMT__DATA_SIZE__24_BITS(pcm_reader);
		break;
	case SND_STM_FORMAT__SUBFRAME_16_BITS:
		snd_stm_printd(1, "- 16 bits per subframe\n");
		set__AUD_PCMIN_FMT__NBIT__16_BITS(pcm_reader);
		set__AUD_PCMIN_FMT__DATA_SIZE__16_BITS(pcm_reader);
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Serial audio interface format -
	 * for detailed explanation see ie.
	 * http://www.cirrus.com/en/pubs/appNote/AN282REV1.pdf */

	set__AUD_PCMIN_FMT__ORDER__MSB_FIRST(pcm_reader);
	set__AUD_PCMIN_FMT__SCLK_EDGE__RISING(pcm_reader);
	switch (format & SND_STM_FORMAT__MASK) {
	case SND_STM_FORMAT__I2S:
		snd_stm_printd(1, "- I2S\n");
		set__AUD_PCMIN_FMT__ALIGN__LEFT(pcm_reader);
		set__AUD_PCMIN_FMT__PADDING__1_CYCLE_DELAY(pcm_reader);
		lr_pol = value__AUD_PCMIN_FMT__LR_POL__LEFT_LOW(pcm_reader);
		break;
	case SND_STM_FORMAT__LEFT_JUSTIFIED:
		snd_stm_printd(1, "- left justified\n");
		set__AUD_PCMIN_FMT__ALIGN__LEFT(pcm_reader);
		set__AUD_PCMIN_FMT__PADDING__NO_DELAY(pcm_reader);
		lr_pol = value__AUD_PCMIN_FMT__LR_POL__LEFT_HIGH(pcm_reader);
		break;
	case SND_STM_FORMAT__RIGHT_JUSTIFIED:
		snd_stm_printd(1, "- right justified\n");
		set__AUD_PCMIN_FMT__ALIGN__RIGHT(pcm_reader);
		set__AUD_PCMIN_FMT__PADDING__NO_DELAY(pcm_reader);
		lr_pol = value__AUD_PCMIN_FMT__LR_POL__LEFT_HIGH(pcm_reader);
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Configure data memory format */

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "24/20/18/16 bits on the
		 * left than zeros"... ;-) */
		set__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_0_BITS(pcm_reader);

		/* In x/0 bits memory mode there is no problem with
		 * L/R polarity */
		set__AUD_PCMIN_FMT__LR_POL(pcm_reader, lr_pol);
		break;

	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Number of channels... */

	BUG_ON(runtime->channels % 2 != 0);
	BUG_ON(runtime->channels < 2);
	BUG_ON(runtime->channels > 10);

	if (pcm_reader->ver > 3)
		set__AUD_PCMIN_FMT__NUM_CH(pcm_reader, runtime->channels / 2);

	return 0;
}

static int snd_stm_pcm_reader_start(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_pcm_reader_start(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	/* Un-reset PCM reader */

	set__AUD_PCMIN_RST__RSTP__RUNNING(pcm_reader);

	/* Launch FDMA transfer */

	result = dma_xfer_list(pcm_reader->fdma_channel,
			pcm_reader->fdma_params_list);
	if (result != 0) {
		snd_stm_printe("Can't launch FDMA transfer for reader '%s'!\n",
			       dev_name(pcm_reader->device));
		return -EINVAL;
	}
	while (dma_get_status(pcm_reader->fdma_channel) !=
			DMA_CHANNEL_STATUS_RUNNING)
		udelay(5);

	/* Enable required reader interrupt (and clear possible stalled) */

	enable_irq(pcm_reader->irq);
	set__AUD_PCMIN_ITS_CLR__OVF__CLEAR(pcm_reader);
	set__AUD_PCMIN_IT_EN_SET__OVF__SET(pcm_reader);

	/* Launch the reader */

	set__AUD_PCMIN_CTRL__MODE__PCM(pcm_reader);

	/* Wake up & unmute ADC */

	if (pcm_reader->conv_group) {
		snd_stm_conv_enable(pcm_reader->conv_group,
				0, substream->runtime->channels - 1);
		snd_stm_conv_unmute(pcm_reader->conv_group);
	}

	pcm_reader->running = 1;

	return 0;
}

static int snd_stm_pcm_reader_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_pcm_reader_stop(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	pcm_reader->running = 0;

	/* Mute & shutdown DAC */

	if (pcm_reader->conv_group) {
		snd_stm_conv_mute(pcm_reader->conv_group);
		snd_stm_conv_disable(pcm_reader->conv_group);
	}

	/* Disable interrupts */

	set__AUD_PCMIN_IT_EN_CLR__OVF__CLEAR(pcm_reader);
	disable_irq_nosync(pcm_reader->irq);

	/* Stop PCM reader */

	set__AUD_PCMIN_CTRL__MODE__OFF(pcm_reader);

	/* Stop FDMA transfer */

	dma_stop_channel(pcm_reader->fdma_channel);

	/* Reset PCM reader */

	set__AUD_PCMIN_RST__RSTP__RESET(pcm_reader);

	return 0;
}

static int snd_stm_pcm_reader_trigger(struct snd_pcm_substream *substream,
		int command)
{
	snd_stm_printd(1, "snd_stm_pcm_reader_trigger(substream=0x%p,"
		       "command=%d)\n", substream, command);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_stm_pcm_reader_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_pcm_reader_stop(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t snd_stm_pcm_reader_pointer(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue, hwptr;
	snd_pcm_uframes_t pointer;

	snd_stm_printd(2, "snd_stm_pcm_reader_pointer(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));
	BUG_ON(!runtime);

	residue = get_dma_residue(pcm_reader->fdma_channel);
	hwptr = (runtime->dma_bytes - residue) % runtime->dma_bytes;
	pointer = bytes_to_frames(runtime, hwptr);

	snd_stm_printd(2, "FDMA residue value is %i and buffer size is %u"
			" bytes...\n", residue, runtime->dma_bytes);
	snd_stm_printd(2, "... so HW pointer in frames is %lu (0x%lx)!\n",
			pointer, pointer);

	return pointer;
}

static struct snd_pcm_ops snd_stm_pcm_reader_pcm_ops = {
	.open =      snd_stm_pcm_reader_open,
	.close =     snd_stm_pcm_reader_close,
	.mmap =      snd_stm_buffer_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = snd_stm_pcm_reader_hw_params,
	.hw_free =   snd_stm_pcm_reader_hw_free,
	.prepare =   snd_stm_pcm_reader_prepare,
	.trigger =   snd_stm_pcm_reader_trigger,
	.pointer =   snd_stm_pcm_reader_pointer,
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_PCMIN_%s (offset 0x%02x) = 0x%08x\n", \
				__stringify(r), \
				offset__AUD_PCMIN_##r(pcm_reader), \
				get__AUD_PCMIN_##r(pcm_reader))

static void snd_stm_pcm_reader_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_pcm_reader *pcm_reader = entry->private_data;

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	snd_iprintf(buffer, "--- %s ---\n", dev_name(pcm_reader->device));
	snd_iprintf(buffer, "base = 0x%p\n", pcm_reader->base);

	DUMP_REGISTER(RST);
	DUMP_REGISTER(DATA);
	DUMP_REGISTER(ITS);
	DUMP_REGISTER(ITS_CLR);
	DUMP_REGISTER(IT_EN);
	DUMP_REGISTER(IT_EN_SET);
	DUMP_REGISTER(IT_EN_CLR);
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(STA);
	DUMP_REGISTER(FMT);

	snd_iprintf(buffer, "\n");
}

static int snd_stm_pcm_reader_register(struct snd_device *snd_device)
{
	struct snd_stm_pcm_reader *pcm_reader = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	/* Set reset mode */

	set__AUD_PCMIN_RST__RSTP__RESET(pcm_reader);

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what does it actually mean? */

	set__AUD_PCMIN_CTRL__RND__NO_ROUNDING(pcm_reader);

	/* Registers view in ALSA's procfs */

	snd_stm_info_register(&pcm_reader->proc_entry,
			dev_name(pcm_reader->device),
			snd_stm_pcm_reader_dump_registers, pcm_reader);

	return 0;
}

static int snd_stm_pcm_reader_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_pcm_reader *pcm_reader = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	snd_stm_info_unregister(pcm_reader->proc_entry);

	return 0;
}

static struct snd_device_ops snd_stm_pcm_reader_snd_device_ops = {
	.dev_register = snd_stm_pcm_reader_register,
	.dev_disconnect = snd_stm_pcm_reader_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_pcm_reader_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_pcm_reader *pcm_reader;
	struct snd_card *card = snd_stm_card_get();
	int i;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);

	pcm_reader = kzalloc(sizeof(*pcm_reader), GFP_KERNEL);
	if (!pcm_reader) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(pcm_reader);
	pcm_reader->info = pdev->dev.platform_data;
	BUG_ON(!pcm_reader->info);
	pcm_reader->ver = pcm_reader->info->ver;
	BUG_ON(pcm_reader->ver <= 0);
	pcm_reader->device = &pdev->dev;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &pcm_reader->mem_region,
			&pcm_reader->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}
	pcm_reader->fifo_phys_address = pcm_reader->mem_region->start +
			offset__AUD_PCMIN_DATA(pcm_reader);
	snd_stm_printd(0, "FIFO physical address: 0x%lx.\n",
			pcm_reader->fifo_phys_address);

	result = snd_stm_irq_request(pdev, &pcm_reader->irq,
			snd_stm_pcm_reader_irq_handler, pcm_reader);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	result = snd_stm_fdma_request(pdev, &pcm_reader->fdma_channel);
	if (result < 0) {
		snd_stm_printe("FDMA request failed!\n");
		goto error_fdma_request;
	}

	/* FDMA transfer size depends (among others ;-) on FIFO length,
	 * which is:
	 * - 2 cells (8 bytes) in STx7100/9 and STx7200 cut 1.0
	 * - 70 cells (280 bytes) in STx7111 and STx7200 cut 2.0. */

	if (pcm_reader->ver < 4)
		pcm_reader->fdma_max_transfer_size = 2;
	else
		pcm_reader->fdma_max_transfer_size = 30;

	/* Get component capabilities */

	snd_stm_printd(0, "Reader's name is '%s'\n", pcm_reader->info->name);

	if (pcm_reader->ver < 5) {
		/* STx7111 has a hardware bug in PCM reader in multichannels
		 * mode, so we will just not be using it ;-) */
		static unsigned int channels_2[] = { 2 };

		BUG_ON(pcm_reader->info->channels != 2);
		pcm_reader->channels_constraint.list = channels_2;
		pcm_reader->channels_constraint.count = 1;
	} else {
		static unsigned int channels_2_10[] = { 2, 4, 6, 8, 10 };

		BUG_ON(pcm_reader->info->channels <= 0);
		BUG_ON(pcm_reader->info->channels > 10);
		BUG_ON(pcm_reader->info->channels % 2 != 0);
		pcm_reader->channels_constraint.list = channels_2_10;
		pcm_reader->channels_constraint.count =
			pcm_reader->info->channels / 2;
	}
	pcm_reader->channels_constraint.mask = 0;
	for (i = 0; i < pcm_reader->channels_constraint.count; i++)
		snd_stm_printd(0, "Reader capable of capturing %u-channels PCM."
				"\n", pcm_reader->channels_constraint.list[i]);

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, pcm_reader,
			&snd_stm_pcm_reader_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, pcm_reader->info->card_device, 0, 1,
		       &pcm_reader->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	pcm_reader->pcm->private_data = pcm_reader;
	strcpy(pcm_reader->pcm->name, pcm_reader->info->name);

	snd_pcm_set_ops(pcm_reader->pcm, SNDRV_PCM_STREAM_CAPTURE,
			&snd_stm_pcm_reader_pcm_ops);

	/* Initialize buffer */

	pcm_reader->buffer = snd_stm_buffer_create(pcm_reader->pcm,
			pcm_reader->device,
			snd_stm_pcm_reader_hw.buffer_bytes_max);
	if (!pcm_reader->buffer) {
		snd_stm_printe("Cannot initialize buffer!\n");
		result = -ENOMEM;
		goto error_buffer_create;
	}

	/* Register in converters router */

	pcm_reader->conv_source = snd_stm_conv_register_source(
			&platform_bus_type, dev_name(&pdev->dev),
			pcm_reader->info->channels,
			card, pcm_reader->info->card_device);
	if (!pcm_reader->conv_source) {
		snd_stm_printe("Cannot register in converters router!\n");
		result = -ENOMEM;
		goto error_conv_register_source;
	}

	/* Claim the pads */

	if (pcm_reader->info->pad_config) {
		pcm_reader->pads = stm_pad_claim(pcm_reader->info->pad_config,
				dev_name(&pdev->dev));
		if (!pcm_reader->pads) {
			snd_stm_printe("Failed to claimed pads for '%s'!\n",
					dev_name(&pdev->dev));
			result = -EBUSY;
			goto error_pad_claim;
		}
	}

	/* Done now */

	platform_set_drvdata(pdev, pcm_reader);

	return 0;

error_pad_claim:
	snd_stm_conv_unregister_source(pcm_reader->conv_source);
error_conv_register_source:
	snd_stm_buffer_dispose(pcm_reader->buffer);
error_buffer_create:
	/* snd_pcm_free() is not available - PCM device will be released
	 * during card release */
error_pcm:
	snd_device_free(card, pcm_reader);
error_device:
	snd_stm_fdma_release(pcm_reader->fdma_channel);
error_fdma_request:
	snd_stm_irq_release(pcm_reader->irq, pcm_reader);
error_irq_request:
	snd_stm_memory_release(pcm_reader->mem_region, pcm_reader->base);
error_memory_request:
	snd_stm_magic_clear(pcm_reader);
	kfree(pcm_reader);
error_alloc:
	return result;
}

static int snd_stm_pcm_reader_remove(struct platform_device *pdev)
{
	struct snd_stm_pcm_reader *pcm_reader = platform_get_drvdata(pdev);

	snd_stm_printd(1, "snd_stm_pcm_reader_remove(pdev=%p)\n", pdev);

	BUG_ON(!pcm_reader);
	BUG_ON(!snd_stm_magic_valid(pcm_reader));

	if (pcm_reader->pads)
		stm_pad_release(pcm_reader->pads);
	snd_stm_conv_unregister_source(pcm_reader->conv_source);
	snd_stm_buffer_dispose(pcm_reader->buffer);
	snd_stm_fdma_release(pcm_reader->fdma_channel);
	snd_stm_irq_release(pcm_reader->irq, pcm_reader);
	snd_stm_memory_release(pcm_reader->mem_region, pcm_reader->base);

	snd_stm_magic_clear(pcm_reader);
	kfree(pcm_reader);

	return 0;
}

static struct platform_driver snd_stm_pcm_reader_driver = {
	.driver.name = "snd_pcm_reader",
	.probe = snd_stm_pcm_reader_probe,
	.remove = snd_stm_pcm_reader_remove,
};

/*
 * Initialization
 */

static int __init snd_stm_pcm_reader_init(void)
{
	return platform_driver_register(&snd_stm_pcm_reader_driver);
}

static void __exit snd_stm_pcm_reader_exit(void)
{
	platform_driver_unregister(&snd_stm_pcm_reader_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics PCM reader driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_pcm_reader_init);
module_exit(snd_stm_pcm_reader_exit);
