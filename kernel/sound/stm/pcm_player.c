/*
 *   STMicroelectronics System-on-Chips' PCM player driver
 *
 *   Copyright (c) 2005-2011 STMicroelectronics Limited
 *
 *   Author: Pawel Moll <pawel.moll@st.com>
 *           Mark Glaisher
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

#include <asm/cacheflush.h>
#include <asm/clock.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/stm/pad.h>
#include <linux/stm/stm-dma.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/pcm_params.h>

#include "common.h"
#include "reg_aud_pcmout.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Some hardware-related definitions
 */

#define DEFAULT_FORMAT (SND_STM_FORMAT__I2S | \
		SND_STM_FORMAT__SUBFRAME_32_BITS)
#define DEFAULT_OVERSAMPLING 256

/* The sample count field (NSAMPLES in CTRL register) is 19 bits wide */
#define MAX_SAMPLES_PER_PERIOD ((1 << 19) - 1)



/*
 * PCM player instance definition
 */

struct snd_stm_pcm_player {
	/* System informations */
	struct snd_stm_pcm_player_info *info;
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
	struct clk *clock;
	struct snd_pcm_hw_constraint_list channels_constraint;
	struct snd_stm_conv_source *conv_source;

	/* Runtime data */
	struct snd_stm_conv_group *conv_group;
	struct snd_stm_buffer *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;
	int fdma_max_transfer_size;
	struct stm_dma_params fdma_params;
	struct stm_dma_req *fdma_request;
	struct stm_pad_state *pads;

	snd_stm_magic_field;
};



/*
 * Playing engine implementation
 */

static irqreturn_t snd_stm_pcm_player_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct snd_stm_pcm_player *pcm_player = dev_id;
	unsigned int status;

	snd_stm_printd(2, "snd_stm_pcm_player_irq_handler(irq=%d, "
			"dev_id=0x%p)\n", irq, dev_id);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = get__AUD_PCMOUT_ITS(pcm_player);
	set__AUD_PCMOUT_ITS_CLR(pcm_player, status);
	preempt_enable();

	/* Underflow? */
	if (unlikely(status & mask__AUD_PCMOUT_ITS__UNF__PENDING(pcm_player))) {
		snd_stm_printe("Underflow detected in PCM player '%s'!\n",
			       dev_name(pcm_player->device));

		snd_pcm_stop(pcm_player->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;
	} else if (likely(status &
			mask__AUD_PCMOUT_ITS__NSAMPLE__PENDING(pcm_player))) {
		/* Period successfully played */
		do {
			BUG_ON(!pcm_player->substream);

			snd_stm_printd(2, "Period elapsed ('%s')\n",
					dev_name(pcm_player->device));
			snd_pcm_period_elapsed(pcm_player->substream);

			result = IRQ_HANDLED;
		} while (0);
	}

	/* Some alien interrupt??? */
	BUG_ON(result != IRQ_HANDLED);

	return result;
}

static struct snd_pcm_hardware snd_stm_pcm_player_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S16_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 10,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* Values below were worked out mostly basing on ST media player
	 * requirements. They should, however, fit most "normal" cases...
	 * Note 1: that these value must be also calculated not to exceed
	 * NSAMPLE interrupt counter size (19 bits) - MAX_SAMPLES_PER_PERIOD.
	 * Note 2: for 16/16-bits data this counter is a "frames counter",
	 * not "samples counter" (two channels are read as one word).
	 * Note 3: period_bytes_min defines minimum time between period
	 * (NSAMPLE) interrupts... Keep it large enough not to kill
	 * the system... */
	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

static int snd_stm_pcm_player_open(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "snd_stm_pcm_player_open(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));
	BUG_ON(!runtime);

	snd_pcm_set_sync(substream);  /* TODO: ??? */

	/* Get attached converters handle */

	pcm_player->conv_group =
			snd_stm_conv_request_group(pcm_player->conv_source);
	if (pcm_player->conv_group)
		snd_stm_printd(1, "'%s' is attached to '%s' converter(s)...\n",
				dev_name(pcm_player->device),
				snd_stm_conv_get_name(pcm_player->conv_group));
	else
		snd_stm_printd(1, "No converter attached to '%s'!\n",
				dev_name(pcm_player->device));

	/* Set up constraints & pass hardware capabilities info to ALSA */

	result = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_CHANNELS,
			&pcm_player->channels_constraint);
	if (result < 0) {
		snd_stm_printe("Can't set channels constraint!\n");
		return result;
	}

	/* It is better when buffer size is an integer multiple of period
	 * size... Such thing will ensure this :-O */
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
			pcm_player->fdma_max_transfer_size * 4);
	if (result < 0) {
		snd_stm_printe("Can't set buffer bytes constraint!\n");
		return result;
	}

	runtime->hw = snd_stm_pcm_player_hw;

	/* Interrupt handler will need the substream pointer... */
	pcm_player->substream = substream;

	return 0;
}

static int snd_stm_pcm_player_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_pcm_player_close(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	if (pcm_player->conv_group) {
		snd_stm_conv_release_group(pcm_player->conv_group);
		pcm_player->conv_group = NULL;
	}

	pcm_player->substream = NULL;

	return 0;
}

static int snd_stm_pcm_player_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "snd_stm_pcm_player_hw_free(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));
	BUG_ON(!runtime);

	/* This callback may be called more than once... */

	if (snd_stm_buffer_is_allocated(pcm_player->buffer)) {
		/* Let the FDMA stop */
		dma_wait_for_completion(pcm_player->fdma_channel);

		/* Free buffer */
		snd_stm_buffer_free(pcm_player->buffer);

		/* Free FDMA parameters & configuration */
		dma_params_free(&pcm_player->fdma_params);
		dma_req_free(pcm_player->fdma_channel,
				pcm_player->fdma_request);
	}

	return 0;
}

static int snd_stm_pcm_player_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, frame_bytes, transfer_bytes;
	unsigned int transfer_size;
	struct stm_dma_req_config fdma_req_config = {
		.rw        = REQ_CONFIG_WRITE,
		.opcode    = REQ_CONFIG_OPCODE_4,
		.increment = 0,
		.hold_off  = 0,
		.initiator = pcm_player->info->fdma_initiator,
	};

	snd_stm_printd(1, "snd_stm_pcm_player_hw_params(substream=0x%p,"
			" hw_params=0x%p)\n", substream, hw_params);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));
	BUG_ON(!runtime);

	/* This function may be called many times, so let's be prepared... */
	if (snd_stm_buffer_is_allocated(pcm_player->buffer))
		snd_stm_pcm_player_hw_free(substream);

	/* Allocate buffer */

	buffer_bytes = params_buffer_bytes(hw_params);
	result = snd_stm_buffer_alloc(pcm_player->buffer, substream,
			buffer_bytes);
	if (result != 0) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
			       buffer_bytes, dev_name(pcm_player->device));
		result = -ENOMEM;
		goto error_buf_alloc;
	}

	/* Set FDMA transfer size (number of opcodes generated
	 * after request line assertion) */

	frame_bytes = snd_pcm_format_physical_width(params_format(hw_params)) *
			params_channels(hw_params) / 8;
	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bytes,
			pcm_player->fdma_max_transfer_size * 4);
	transfer_size = transfer_bytes / 4;
	snd_stm_printd(1, "FDMA request trigger limit and transfer size set "
			"to %d.\n", transfer_size);

	BUG_ON(buffer_bytes % transfer_bytes != 0);
	BUG_ON(transfer_size > pcm_player->fdma_max_transfer_size);
	fdma_req_config.count = transfer_size;

	BUG_ON(transfer_size != 1 && transfer_size % 2 != 0);
	BUG_ON(transfer_size >
	       mask__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(pcm_player));
	set__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(pcm_player, transfer_size);

	/* Configure FDMA transfer */

	pcm_player->fdma_request = dma_req_config(pcm_player->fdma_channel,
			pcm_player->info->fdma_request_line, &fdma_req_config);
	if (!pcm_player->fdma_request) {
		snd_stm_printe("Can't configure FDMA pacing channel for player"
			       " '%s'!\n", dev_name(pcm_player->device));
		result = -EINVAL;
		goto error_req_config;
	}

	dma_params_init(&pcm_player->fdma_params, MODE_PACED,
			STM_DMA_LIST_CIRC);

	dma_params_DIM_1_x_0(&pcm_player->fdma_params);

	dma_params_req(&pcm_player->fdma_params, pcm_player->fdma_request);

	dma_params_addrs(&pcm_player->fdma_params, runtime->dma_addr,
			pcm_player->fifo_phys_address, buffer_bytes);

	result = dma_compile_list(pcm_player->fdma_channel,
				&pcm_player->fdma_params, GFP_KERNEL);
	if (result < 0) {
		snd_stm_printe("Can't compile FDMA parameters for player"
			       " '%s'!\n", dev_name(pcm_player->device));
		goto error_compile_list;
	}

	return 0;

error_compile_list:
	dma_req_free(pcm_player->fdma_channel,
			pcm_player->fdma_request);
error_req_config:
	snd_stm_buffer_free(pcm_player->buffer);
error_buf_alloc:
	return result;
}

static int snd_stm_pcm_player_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int format, lr_pol;
	int oversampling, bits_in_output_frame;
	int result;

	snd_stm_printd(1, "snd_stm_pcm_player_prepare(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));
	BUG_ON(!runtime);
	BUG_ON(runtime->period_size * runtime->channels >=
	       MAX_SAMPLES_PER_PERIOD);

	/* Configure SPDIF synchronisation */

	/* TODO */

	/* Get format & oversampling value from connected converter */

	if (pcm_player->conv_group) {
		format = snd_stm_conv_get_format(pcm_player->conv_group);
		oversampling = snd_stm_conv_get_oversampling(
				pcm_player->conv_group);
		if (oversampling == 0)
			oversampling = DEFAULT_OVERSAMPLING;
	} else {
		format = DEFAULT_FORMAT;
		oversampling = DEFAULT_OVERSAMPLING;
	}

	snd_stm_printd(1, "Player %s: sampling frequency %d, oversampling %d\n",
			dev_name(pcm_player->device), runtime->rate,
			oversampling);

	BUG_ON(oversampling < 0);

	/* For 32 bits subframe oversampling must be a multiple of 128,
	 * for 16 bits - of 64 */
	BUG_ON((format & SND_STM_FORMAT__SUBFRAME_32_BITS) &&
		(oversampling % 128 != 0));
	BUG_ON(!(format & SND_STM_FORMAT__SUBFRAME_16_BITS) &&
		(oversampling % 64 != 0));

	/* Set up frequency synthesizer */

	result = clk_enable(pcm_player->clock);
	if (result != 0) {
		snd_stm_printe("Can't enable clock for player '%s'!\n",
				dev_name(pcm_player->device));
		return result;
	}

	result = clk_set_rate(pcm_player->clock,
				runtime->rate * oversampling);
	if (result != 0) {
		snd_stm_printe("Can't configure clock for player '%s'!\n",
				dev_name(pcm_player->device));
		clk_disable(pcm_player->clock);
		return result;
	}

	/* Set up player hardware */

	snd_stm_printd(1, "Player %s format configuration:\n",
			dev_name(pcm_player->device));

	/* Number of bits per subframe (which is one channel sample)
	 * on output - it determines serial clock frequency, which is
	 * 64 times sampling rate for 32 bits subframe (2 channels 32
	 * bits each means 64 bits per frame) and 32 times sampling
	 * rate for 16 bits subframe
	 * (you know why, don't you? :-) */

	switch (format & SND_STM_FORMAT__SUBFRAME_MASK) {
	case SND_STM_FORMAT__SUBFRAME_32_BITS:
		snd_stm_printd(1, "- 32 bits per subframe\n");
		set__AUD_PCMOUT_FMT__NBIT__32_BITS(pcm_player);
		if (pcm_player->ver > 5)
			set__AUD_PCMOUT_FMT__DATA_SIZE__32_BITS(pcm_player);
		else
			set__AUD_PCMOUT_FMT__DATA_SIZE__24_BITS(pcm_player);
		bits_in_output_frame = 64; /* frame = 2 * subframe */
		break;
	case SND_STM_FORMAT__SUBFRAME_16_BITS:
		snd_stm_printd(1, "- 16 bits per subframe\n");
		set__AUD_PCMOUT_FMT__NBIT__16_BITS(pcm_player);
		set__AUD_PCMOUT_FMT__DATA_SIZE__16_BITS(pcm_player);
		bits_in_output_frame = 32; /* frame = 2 * subframe */
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Serial audio interface format - for detailed explanation
	 * see ie.:
	 * http://www.cirrus.com/en/pubs/appNote/AN282REV1.pdf */

	set__AUD_PCMOUT_FMT__ORDER__MSB_FIRST(pcm_player);

	/* Value FALLING of SCLK_EDGE bit in AUD_PCMOUT_FMT register that
	 * actually means "data clocking (changing) on the falling edge"
	 * (and we usually want this...) - STx7100 and cuts < 3.0 of
	 * STx7109 have this bit inverted comparing to what their
	 * datasheets claim... (specs say 1) */

	set__AUD_PCMOUT_FMT__SCLK_EDGE__FALLING(pcm_player);

	switch (format & SND_STM_FORMAT__MASK) {
	case SND_STM_FORMAT__I2S:
		snd_stm_printd(1, "- I2S\n");
		set__AUD_PCMOUT_FMT__ALIGN__LEFT(pcm_player);
		set__AUD_PCMOUT_FMT__PADDING__1_CYCLE_DELAY(pcm_player);
		lr_pol = value__AUD_PCMOUT_FMT__LR_POL__LEFT_LOW(pcm_player);
		break;
	case SND_STM_FORMAT__LEFT_JUSTIFIED:
		snd_stm_printd(1, "- left justified\n");
		set__AUD_PCMOUT_FMT__ALIGN__LEFT(pcm_player);
		set__AUD_PCMOUT_FMT__PADDING__NO_DELAY(pcm_player);
		lr_pol = value__AUD_PCMOUT_FMT__LR_POL__LEFT_HIGH(pcm_player);
		break;
	case SND_STM_FORMAT__RIGHT_JUSTIFIED:
		snd_stm_printd(1, "- right justified\n");
		set__AUD_PCMOUT_FMT__ALIGN__RIGHT(pcm_player);
		set__AUD_PCMOUT_FMT__PADDING__NO_DELAY(pcm_player);
		lr_pol = value__AUD_PCMOUT_FMT__LR_POL__LEFT_HIGH(pcm_player);
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Configure PCM player frequency divider
	 *
	 *             Fdacclk             Fs * oversampling
	 * divider = ----------- = ------------------------------- =
	 *            2 * Fsclk     2 * Fs * bits_in_output_frame
	 *
	 *                  oversampling
	 *         = --------------------------
	 *            2 * bits_in_output_frame
	 * where:
	 *   - Fdacclk - frequency of DAC clock signal, known also as PCMCLK,
	 *               MCLK (master clock), "system clock" etc.
	 *   - Fsclk - frequency of SCLK (serial clock) aka BICK (bit clock)
	 *   - Fs - sampling rate (frequency)
	 *   - bits_in_output_frame - number of bits in output signal _frame_
	 *                (32 or 64, depending on NBIT field of FMT register)
	 */

	set__AUD_PCMOUT_CTRL__CLK_DIV(pcm_player,
			oversampling / (2 * bits_in_output_frame));

	/* Configure data memory format & NSAMPLE interrupt */

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		/* One data word contains two samples */
		set__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_16_BITS(pcm_player);

		/* Workaround for a problem with L/R channels swap in case of
		 * 16/16 memory model: PCM player expects left channel data in
		 * word's upper two bytes, but due to little endianess
		 * character of our memory there is right channel data there;
		 * the workaround is to invert L/R signal, however it is
		 * cheating, because in such case channel phases are shifted
		 * by one sample...
		 * (ask me for more details if above is not clear ;-)
		 * TODO this somehow better... */
		set__AUD_PCMOUT_FMT__LR_POL(pcm_player, !lr_pol);

		/* One word of data is two samples (two channels...) */
		set__AUD_PCMOUT_CTRL__NSAMPLE(pcm_player,
				runtime->period_size * runtime->channels / 2);
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "32/28/24/20/18/16 bits
		 * on the left than zeros (if less than 32 bites)"... ;-) */
		set__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_0_BITS(pcm_player);

		/* In x/0 bits memory mode there is no problem with
		 * L/R polarity */
		set__AUD_PCMOUT_FMT__LR_POL(pcm_player, lr_pol);

		/* One word of data is one sample, so period size
		 * times channels */
		set__AUD_PCMOUT_CTRL__NSAMPLE(pcm_player,
				runtime->period_size * runtime->channels);
		break;

	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Number of channels... */

	BUG_ON(runtime->channels % 2 != 0);
	BUG_ON(runtime->channels < 2);
	BUG_ON(runtime->channels > 10);

	set__AUD_PCMOUT_FMT__NUM_CH(pcm_player, runtime->channels / 2);

	return 0;
}

static int snd_stm_pcm_player_start(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_pcm_player_start(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	/* Un-reset PCM player */

	set__AUD_PCMOUT_RST__SRSTP__RUNNING(pcm_player);

	/* Launch FDMA transfer */

	result = dma_xfer_list(pcm_player->fdma_channel,
			&pcm_player->fdma_params);
	if (result != 0) {
		snd_stm_printe("Can't launch FDMA transfer for player '%s'!\n",
			       dev_name(pcm_player->device));
		clk_disable(pcm_player->clock);
		return -EINVAL;
	}
	while (dma_get_status(pcm_player->fdma_channel) !=
			DMA_CHANNEL_STATUS_RUNNING)
		udelay(5);

	/* Enable player interrupts (and clear possible stalled ones) */

	enable_irq(pcm_player->irq);
	set__AUD_PCMOUT_ITS_CLR__NSAMPLE__CLEAR(pcm_player);
	set__AUD_PCMOUT_IT_EN_SET__NSAMPLE__SET(pcm_player);
	set__AUD_PCMOUT_ITS_CLR__UNF__CLEAR(pcm_player);
	set__AUD_PCMOUT_IT_EN_SET__UNF__SET(pcm_player);

	/* Launch the player */

	set__AUD_PCMOUT_CTRL__MODE__PCM(pcm_player);

	/* Wake up & unmute DAC */

	if (pcm_player->conv_group) {
		snd_stm_conv_enable(pcm_player->conv_group,
				0, substream->runtime->channels - 1);
		snd_stm_conv_unmute(pcm_player->conv_group);
	}

	return 0;
}

static int snd_stm_pcm_player_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_pcm_player_stop(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	/* Mute & shutdown DAC */

	if (pcm_player->conv_group) {
		snd_stm_conv_mute(pcm_player->conv_group);
		snd_stm_conv_disable(pcm_player->conv_group);
	}

	/* Disable interrupts */

	set__AUD_PCMOUT_IT_EN_CLR__NSAMPLE__CLEAR(pcm_player);
	set__AUD_PCMOUT_IT_EN_CLR__UNF__CLEAR(pcm_player);
	disable_irq_nosync(pcm_player->irq);

	/* Stop PCM player */

	set__AUD_PCMOUT_CTRL__MODE__OFF(pcm_player);

	/* Stop FDMA transfer */

	dma_stop_channel(pcm_player->fdma_channel);

	/* Stop the clock & reset PCM player */

	clk_disable(pcm_player->clock);
	set__AUD_PCMOUT_RST__SRSTP__RESET(pcm_player);

	return 0;
}

static int snd_stm_pcm_player_pause(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_pcm_player_pause(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	/* "Mute" player
	 * Documentation describes this mode in a wrong way - data is _not_
	 * consumed in the "mute" mode, so it is actually a "pause" mode */

	set__AUD_PCMOUT_CTRL__MODE__MUTE(pcm_player);

	return 0;
}

static int snd_stm_pcm_player_release(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
		snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_pcm_player_release(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	/* "Unmute" player */

	set__AUD_PCMOUT_CTRL__MODE__PCM(pcm_player);

	return 0;
}

static int snd_stm_pcm_player_trigger(struct snd_pcm_substream *substream,
		int command)
{
	snd_stm_printd(1, "snd_stm_pcm_player_trigger(substream=0x%p,"
			" command=%d)\n", substream, command);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_stm_pcm_player_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_pcm_player_stop(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return snd_stm_pcm_player_pause(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return snd_stm_pcm_player_release(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t snd_stm_pcm_player_pointer(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_pcm_player *pcm_player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue, hwptr;
	snd_pcm_uframes_t pointer;

	snd_stm_printd(2, "snd_stm_pcm_player_pointer(substream=0x%p)\n",
			substream);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));
	BUG_ON(!runtime);

	residue = get_dma_residue(pcm_player->fdma_channel);
	hwptr = (runtime->dma_bytes - residue) % runtime->dma_bytes;
	pointer = bytes_to_frames(runtime, hwptr);

	snd_stm_printd(2, "FDMA residue value is %i and buffer size is %u"
			" bytes...\n", residue, runtime->dma_bytes);
	snd_stm_printd(2, "... so HW pointer in frames is %lu (0x%lx)!\n",
			pointer, pointer);

	return pointer;
}

static struct snd_pcm_ops snd_stm_pcm_player_pcm_ops = {
	.open =      snd_stm_pcm_player_open,
	.close =     snd_stm_pcm_player_close,
	.mmap =      snd_stm_buffer_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = snd_stm_pcm_player_hw_params,
	.hw_free =   snd_stm_pcm_player_hw_free,
	.prepare =   snd_stm_pcm_player_prepare,
	.trigger =   snd_stm_pcm_player_trigger,
	.pointer =   snd_stm_pcm_player_pointer,
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_PCMOUT_%s (offset 0x%02x) =" \
				" 0x%08x\n", __stringify(r), \
				offset__AUD_PCMOUT_##r(pcm_player), \
				get__AUD_PCMOUT_##r(pcm_player))

static void snd_stm_pcm_player_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_pcm_player *pcm_player = entry->private_data;

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	snd_iprintf(buffer, "--- %s ---\n", dev_name(pcm_player->device));
	snd_iprintf(buffer, "base = 0x%p\n", pcm_player->base);

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

static int snd_stm_pcm_player_register(struct snd_device *snd_device)
{
	struct snd_stm_pcm_player *pcm_player = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	/* Set reset mode */

	set__AUD_PCMOUT_RST__SRSTP__RESET(pcm_player);

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what does it actually mean? */

	if (pcm_player->ver > 5)
		set__AUD_PCMOUT_FMT__BACK_STALLING__DISABLED(pcm_player);
	set__AUD_PCMOUT_CTRL__RND__NO_ROUNDING(pcm_player);

	/* Get frequency synthesizer channel */

	BUG_ON(!pcm_player->info->clock_name);
	snd_stm_printd(0, "Player connected to clock '%s'.\n",
			pcm_player->info->clock_name);
	pcm_player->clock = snd_stm_clk_get(pcm_player->device,
			pcm_player->info->clock_name, snd_device->card,
			pcm_player->info->card_device);
	if (!pcm_player->clock || IS_ERR(pcm_player->clock)) {
		snd_stm_printe("Failed to get a clock for '%s'!\n",
				dev_name(pcm_player->device));
		return -EINVAL;
	}

	/* Registers view in ALSA's procfs */

	snd_stm_info_register(&pcm_player->proc_entry,
			dev_name(pcm_player->device),
			snd_stm_pcm_player_dump_registers, pcm_player);

	return 0;
}

static int __exit snd_stm_pcm_player_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_pcm_player *pcm_player = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	snd_stm_clk_put(pcm_player->clock);

	snd_stm_info_unregister(pcm_player->proc_entry);

	return 0;
}

static struct snd_device_ops snd_stm_pcm_player_snd_device_ops = {
	.dev_register = snd_stm_pcm_player_register,
	.dev_disconnect = snd_stm_pcm_player_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_pcm_player_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_pcm_player *pcm_player;
	struct snd_card *card = snd_stm_card_get();
	int i;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);

	pcm_player = kzalloc(sizeof(*pcm_player), GFP_KERNEL);
	if (!pcm_player) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(pcm_player);
	pcm_player->info = pdev->dev.platform_data;
	BUG_ON(!pcm_player->info);
	pcm_player->ver = pcm_player->info->ver;
	BUG_ON(pcm_player->ver <= 0);
	pcm_player->device = &pdev->dev;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &pcm_player->mem_region,
			&pcm_player->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}
	pcm_player->fifo_phys_address = pcm_player->mem_region->start +
		offset__AUD_PCMOUT_DATA(pcm_player);
	snd_stm_printd(0, "FIFO physical address: 0x%lx.\n",
			pcm_player->fifo_phys_address);

	result = snd_stm_irq_request(pdev, &pcm_player->irq,
			snd_stm_pcm_player_irq_handler, pcm_player);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	result = snd_stm_fdma_request(pdev, &pcm_player->fdma_channel);
	if (result < 0) {
		snd_stm_printe("FDMA request failed!\n");
		goto error_fdma_request;
	}

	/* FDMA transfer size depends (among others ;-) on FIFO length,
	 * which is:
	 * - 30 cells (120 bytes) in STx7100/9 and STx7200 cut 1.0
	 * - 70 cells (280 bytes) in STx7111 and STx7200 cut 2.0. */

	if (pcm_player->ver < 5)
		pcm_player->fdma_max_transfer_size = 2;
	else if (pcm_player->ver == 5)
		pcm_player->fdma_max_transfer_size = 20;
	else
		pcm_player->fdma_max_transfer_size = 30;

	/* Get player capabilities */

	snd_stm_printd(0, "Player's name is '%s'\n", pcm_player->info->name);

	BUG_ON(pcm_player->info->channels <= 0);
	BUG_ON(pcm_player->info->channels > 10);
	BUG_ON(pcm_player->info->channels % 2 != 0);
	if (pcm_player->ver > 1) {
		static unsigned int channels_2_10[] = { 2, 4, 6, 8, 10 };

		pcm_player->channels_constraint.list = channels_2_10;
		pcm_player->channels_constraint.count =
			pcm_player->info->channels / 2;
	} else {
		/* In STx7100 cut < 3.0 PCM player ignored NUM_CH setting in
		 * AUD_PCMOUT_FMT register (and it was always in 10 channels
		 * mode...) */
		static unsigned int channels_10[] = { 10 };

		pcm_player->channels_constraint.list = channels_10;
		pcm_player->channels_constraint.count = 1;
	}
	pcm_player->channels_constraint.mask = 0;
	for (i = 0; i < pcm_player->channels_constraint.count; i++)
		snd_stm_printd(0, "Player capable of playing %u-channels PCM."
				"\n", pcm_player->channels_constraint.list[i]);

	/* STx7100 has a problem with 16/16 bits FIFO organization,
	 * so we disable the 16 bits samples capability... */
	if (pcm_player->ver <= 2)
		snd_stm_pcm_player_hw.formats &= ~SNDRV_PCM_FMTBIT_S16_LE;

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, pcm_player,
			&snd_stm_pcm_player_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, pcm_player->info->card_device, 1, 0,
			&pcm_player->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	pcm_player->pcm->private_data = pcm_player;
	strcpy(pcm_player->pcm->name, pcm_player->info->name);

	snd_pcm_set_ops(pcm_player->pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_stm_pcm_player_pcm_ops);

	/* Initialize buffer */

	pcm_player->buffer = snd_stm_buffer_create(pcm_player->pcm,
			pcm_player->device,
			snd_stm_pcm_player_hw.buffer_bytes_max);
	if (!pcm_player->buffer) {
		snd_stm_printe("Cannot initialize buffer!\n");
		result = -ENOMEM;
		goto error_buffer_init;
	}

	/* Register in converters router */

	pcm_player->conv_source = snd_stm_conv_register_source(
		&platform_bus_type, dev_name(&pdev->dev),
			pcm_player->info->channels,
			card, pcm_player->info->card_device);
	if (!pcm_player->conv_source) {
		snd_stm_printe("Cannot register in converters router!\n");
		result = -ENOMEM;
		goto error_conv_register_source;
	}

	/* Claim the pads */

	if (pcm_player->info->pad_config) {
		pcm_player->pads = stm_pad_claim(pcm_player->info->pad_config,
				dev_name(&pdev->dev));
		if (!pcm_player->pads) {
			snd_stm_printe("Failed to claimed pads for '%s'!\n",
					dev_name(&pdev->dev));
			result = -EBUSY;
			goto error_pad_claim;
		}
	}

	/* Done now */

	platform_set_drvdata(pdev, pcm_player);

	return 0;

error_pad_claim:
	snd_stm_conv_unregister_source(pcm_player->conv_source);
error_conv_register_source:
	snd_stm_buffer_dispose(pcm_player->buffer);
error_buffer_init:
	/* snd_pcm_free() is not available - PCM device will be released
	 * during card release */
error_pcm:
	snd_device_free(card, pcm_player);
error_device:
	snd_stm_fdma_release(pcm_player->fdma_channel);
error_fdma_request:
	snd_stm_irq_release(pcm_player->irq, pcm_player);
error_irq_request:
	snd_stm_memory_release(pcm_player->mem_region, pcm_player->base);
error_memory_request:
	snd_stm_magic_clear(pcm_player);
	kfree(pcm_player);
error_alloc:
	return result;
}

static int snd_stm_pcm_player_remove(struct platform_device *pdev)
{
	struct snd_stm_pcm_player *pcm_player = platform_get_drvdata(pdev);

	snd_stm_printd(1, "snd_stm_pcm_player_remove(pdev=%p)\n", pdev);

	BUG_ON(!pcm_player);
	BUG_ON(!snd_stm_magic_valid(pcm_player));

	if (pcm_player->pads)
		stm_pad_release(pcm_player->pads);
	snd_stm_conv_unregister_source(pcm_player->conv_source);
	snd_stm_buffer_dispose(pcm_player->buffer);
	snd_stm_fdma_release(pcm_player->fdma_channel);
	snd_stm_irq_release(pcm_player->irq, pcm_player);
	snd_stm_memory_release(pcm_player->mem_region, pcm_player->base);

	snd_stm_magic_clear(pcm_player);
	kfree(pcm_player);

	return 0;
}

static struct platform_driver snd_stm_pcm_player_driver = {
	.driver.name = "snd_pcm_player",
	.probe = snd_stm_pcm_player_probe,
	.remove = snd_stm_pcm_player_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_pcm_player_init(void)
{
	return platform_driver_register(&snd_stm_pcm_player_driver);
}

static void __exit snd_stm_pcm_player_exit(void)
{
	platform_driver_unregister(&snd_stm_pcm_player_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics PCM player driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_pcm_player_init);
module_exit(snd_stm_pcm_player_exit);
