/*
 *   STMicroelectronics System-on-Chips' SPDIF player driver
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
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/asoundef.h>

#include "common.h"
#include "reg_aud_spdif.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Some hardware-related definitions
 */

#define DEFAULT_OVERSAMPLING 128

/* The sample count field (MEMREAD in CTRL register) is 17 bits wide */
#define MAX_SAMPLES_PER_PERIOD ((1 << 17) - 1)

#define PREAMBLE_BYTES 8


/*
 * SPDIF player instance definition
 */

enum snd_stm_spdif_player_input_mode {
	SNDRV_STM_SPDIF_INPUT_MODE_NORMAL,
	SNDRV_STM_SPDIF_INPUT_MODE_RAW
};

enum snd_stm_spdif_player_encoding_mode {
	SNDRV_STM_SPDIF_ENCODING_MODE_PCM,
	SNDRV_STM_SPDIF_ENCODING_MODE_ENCODED
};

struct snd_stm_spdif_player_settings {
	enum snd_stm_spdif_player_input_mode input_mode;
	enum snd_stm_spdif_player_encoding_mode encoding_mode;
	struct snd_aes_iec958 iec958;
	unsigned char iec61937_preamble[PREAMBLE_BYTES]; /* Used in */
	unsigned int iec61937_audio_repetition;          /* encoded */
	unsigned int iec61937_pause_repetition;          /* mode */
};

struct snd_stm_spdif_player {
	/* System informations */
	struct snd_stm_spdif_player_info *info;
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
	struct snd_stm_conv_source *conv_source;

	/* Default settings (controlled by controls ;-) */
	struct snd_stm_spdif_player_settings default_settings;
	spinlock_t default_settings_lock; /* Protects default_settings */

	/* Runtime data */
	struct snd_stm_conv_group *conv_group;
	struct snd_stm_buffer *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;
	int fdma_max_transfer_size;
	struct stm_dma_params fdma_params;
	struct stm_dma_req *fdma_request;
	struct snd_stm_spdif_player_settings stream_settings;
	int stream_iec958_status_cnt;
	int stream_iec958_subcode_cnt;
	struct stm_pad_state *pads;

	snd_stm_magic_field;
};



/*
 * Playing engine implementation
 */

static irqreturn_t snd_stm_spdif_player_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct snd_stm_spdif_player *spdif_player = dev_id;
	unsigned int status;

	snd_stm_printd(2, "snd_stm_spdif_player_irq_handler(irq=%d, "
			"dev_id=0x%p)\n", irq, dev_id);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = get__AUD_SPDIF_ITS(spdif_player);
	set__AUD_SPDIF_ITS_CLR(spdif_player, status);
	preempt_enable();

	if (unlikely(status &
			mask__AUD_SPDIF_ITS__UNF__PENDING(spdif_player))) {
		snd_stm_printe("Underflow detected in SPDIF player '%s'!\n",
			       dev_name(spdif_player->device));

		snd_pcm_stop(spdif_player->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;
	} else if (likely(status &
			mask__AUD_SPDIF_ITS__NSAMPLE__PENDING(spdif_player))) {
		/* Period successfully played */
		do {
			BUG_ON(!spdif_player->substream);

			snd_stm_printd(2, "Period elapsed ('%s')\n",
					dev_name(spdif_player->device));
			snd_pcm_period_elapsed(spdif_player->substream);

			result = IRQ_HANDLED;
		} while (0);
	}

	/* Some alien interrupt??? */
	BUG_ON(result != IRQ_HANDLED);

	return result;
}

/* In normal mode we are preparing SPDIF formating "manually".
 * It means:
 * 1. A lot of parsing...
 * 2. MMAPing is impossible...
 * 3. We can handle some other formats! */
static struct snd_pcm_hardware snd_stm_spdif_player_hw_normal = {
	.info		= (SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S24_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 2,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* Values below were worked out mostly basing on ST media player
	 * requirements. They should, however, fit most "normal" cases...
	 * Note 1: that these value must be also calculated not to exceed
	 * NSAMPLE interrupt counter size (19 bits) - MAX_SAMPLES_PER_PERIOD.
	 * Note 2: period_bytes_min defines minimum time between period
	 * (NSAMPLE) interrupts... Keep it large enough not to kill
	 * the system... */
	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

/* In raw mode SPDIF formatting must be prepared by user. Every sample
 * (one channel) is a 32 bits word containing up to 24 bits of data
 * and 4 SPDIF control bits: V(alidty flag), U(ser data), C(hannel status),
 * P(arity bit):
 *
 *      +---------------+---------------+---------------+---------------+
 *      |3|3|2|2|2|2|2|2|2|2|2|2|1|1|1|1|1|1|1|1|1|1|0|0|0|0|0|0|0|0|0|0|
 * bit: |1|0|9|8|6|7|5|4|3|2|1|0|9|8|7|6|5|4|3|2|1|0|9|8|7|6|5|4|3|2|1|0|
 *      +---------------+---------------+---------------+-------+-------+
 *      |M                                             L|       |       |
 *      |S          sample data (up to 24 bits)        S|0|0|0|0|V|U|C|0|
 *      |B                                             B|       |       |
 *      +-----------------------------------------------+-------+-------+
 *
 * SPDIF player sends subframe's sync preamble first (thanks at least
 * for this ;-)), than data starting from LSB (so samples smaller than
 * 24 bits should be aligned to MSB and have zeros as LSBs), than VUC bits
 * and finally adds a parity bit (thanks again ;-).
 */
static struct snd_pcm_hardware snd_stm_spdif_player_hw_raw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 2,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* See above... */
	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

static int snd_stm_spdif_player_open(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "snd_stm_spdif_player_open(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));
	BUG_ON(!runtime);

	snd_pcm_set_sync(substream);  /* TODO: ??? */

	/* Get attached converters handle */

	spdif_player->conv_group =
			snd_stm_conv_request_group(spdif_player->conv_source);
	if (spdif_player->conv_group)
		snd_stm_printd(1, "'%s' is attached to '%s' converter(s)...\n",
				dev_name(spdif_player->device),
				snd_stm_conv_get_name(
				spdif_player->conv_group));
	else
		snd_stm_printd(1, "No converter attached to '%s'!\n",
				dev_name(spdif_player->device));

	/* Get default data */

	spin_lock(&spdif_player->default_settings_lock);
	spdif_player->stream_settings = spdif_player->default_settings;
	spin_unlock(&spdif_player->default_settings_lock);

	/* Set up constraints & pass hardware capabilities info to ALSA */

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
			spdif_player->fdma_max_transfer_size * 4);
	if (result < 0) {
		snd_stm_printe("Can't set buffer bytes constraint!\n");
		return result;
	}

	if (spdif_player->stream_settings.input_mode ==
			SNDRV_STM_SPDIF_INPUT_MODE_NORMAL)
		runtime->hw = snd_stm_spdif_player_hw_normal;
	else
		runtime->hw = snd_stm_spdif_player_hw_raw;

	/* Interrupt handler will need the substream pointer... */
	spdif_player->substream = substream;

	return 0;
}

static int snd_stm_spdif_player_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_spdif_player_close(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	if (spdif_player->conv_group) {
		snd_stm_conv_release_group(spdif_player->conv_group);
		spdif_player->conv_group = NULL;
	}

	spdif_player->substream = NULL;

	return 0;
}

static int snd_stm_spdif_player_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "snd_stm_spdif_player_hw_free(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));
	BUG_ON(!runtime);

	/* This callback may be called more than once... */

	if (snd_stm_buffer_is_allocated(spdif_player->buffer)) {
		/* Let the FDMA stop */
		dma_wait_for_completion(spdif_player->fdma_channel);

		/* Free buffer */
		snd_stm_buffer_free(spdif_player->buffer);

		/* Free FDMA parameters */

		dma_params_free(&spdif_player->fdma_params);
		dma_req_free(spdif_player->fdma_channel,
				spdif_player->fdma_request);
	}

	return 0;
}

static int snd_stm_spdif_player_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, frame_bytes, transfer_bytes;
	unsigned int transfer_size;
	struct stm_dma_req_config fdma_req_config = {
		.rw        = REQ_CONFIG_WRITE,
		.opcode    = REQ_CONFIG_OPCODE_4,
		.increment = 0,
		.hold_off  = 0,
		.initiator = spdif_player->info->fdma_initiator,
	};

	snd_stm_printd(1, "snd_stm_spdif_player_hw_params(substream=0x%p,"
			" hw_params=0x%p)\n", substream, hw_params);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));
	BUG_ON(!runtime);

	/* This function may be called many times, so let's be prepared... */
	if (snd_stm_buffer_is_allocated(spdif_player->buffer))
		snd_stm_spdif_player_hw_free(substream);

	/* Allocate buffer */

	buffer_bytes = params_buffer_bytes(hw_params);
	result = snd_stm_buffer_alloc(spdif_player->buffer, substream,
			buffer_bytes);
	if (!spdif_player->buffer) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
			       buffer_bytes, dev_name(spdif_player->device));
		result = -ENOMEM;
		goto error_buf_alloc;
	}

	/* Set FDMA transfer size (number of opcodes generated
	 * after request line assertion) */

	frame_bytes = snd_pcm_format_physical_width(params_format(hw_params)) *
			params_channels(hw_params) / 8;
	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bytes,
			spdif_player->fdma_max_transfer_size * 4);
	transfer_size = transfer_bytes / 4;
	snd_stm_printd(1, "FDMA request trigger limit and transfer size set "
			"to %d.\n", transfer_size);

	BUG_ON(buffer_bytes % transfer_bytes != 0);
	BUG_ON(transfer_size > spdif_player->fdma_max_transfer_size);
	fdma_req_config.count = transfer_size;

	if (spdif_player->ver >= 4) {
		/* FDMA request trigger control was introduced in
		 * STx7111... */
		BUG_ON(transfer_size != 1 && transfer_size % 2 != 0);
		BUG_ON(transfer_size >
		       mask__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(spdif_player));
		set__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(spdif_player,
				transfer_size);
	}

	/* Configure FDMA transfer */

	spdif_player->fdma_request = dma_req_config(spdif_player->fdma_channel,
			spdif_player->info->fdma_request_line,
			&fdma_req_config);
	if (!spdif_player->fdma_request) {
		snd_stm_printe("Can't configure FDMA pacing channel for player"
			       " '%s'!\n", dev_name(spdif_player->device));
		result = -EINVAL;
		goto error_req_config;
	}

	dma_params_init(&spdif_player->fdma_params, MODE_PACED,
			STM_DMA_LIST_CIRC);

	dma_params_DIM_1_x_0(&spdif_player->fdma_params);

	dma_params_req(&spdif_player->fdma_params, spdif_player->fdma_request);

	dma_params_addrs(&spdif_player->fdma_params, runtime->dma_addr,
			spdif_player->fifo_phys_address, buffer_bytes);

	result = dma_compile_list(spdif_player->fdma_channel,
				&spdif_player->fdma_params, GFP_KERNEL);
	if (result < 0) {
		snd_stm_printe("Can't compile FDMA parameters for player"
			       " '%s'!\n", dev_name(spdif_player->device));
		goto error_compile_list;
	}

	return 0;

error_compile_list:
	dma_req_free(spdif_player->fdma_channel,
			spdif_player->fdma_request);
error_req_config:
	snd_stm_buffer_free(spdif_player->buffer);
error_buf_alloc:
	return result;
}

static int snd_stm_spdif_player_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int oversampling;
	unsigned long status;
	struct snd_aes_iec958 *iec958;
	int result;

	snd_stm_printd(1, "snd_stm_spdif_player_prepare(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));
	BUG_ON(!runtime);
	BUG_ON(runtime->period_size * runtime->channels >=
	       MAX_SAMPLES_PER_PERIOD);

	/* Configure SPDIF-PCM synchronisation */

	/* TODO */

	/* Get oversampling value from connected converter */

	if (spdif_player->conv_group) {
		unsigned int format = snd_stm_conv_get_format(
				spdif_player->conv_group);

		BUG_ON((format & SND_STM_FORMAT__MASK) !=
		       SND_STM_FORMAT__SPDIF);

		oversampling = snd_stm_conv_get_oversampling(
				spdif_player->conv_group);
		if (oversampling == 0)
			oversampling = DEFAULT_OVERSAMPLING;
	} else {
		oversampling = DEFAULT_OVERSAMPLING;
	}

	snd_stm_printd(1, "Player %s: sampling frequency %d, oversampling %d\n",
			dev_name(spdif_player->device), runtime->rate,
			oversampling);

	BUG_ON(oversampling <= 0);

	/* Allowed oversampling values (SPDIF subframe is 32 bits long,
	 * so oversampling must be multiple of 128... */
	BUG_ON(oversampling % 128 != 0);

	/* Set up frequency synthesizer */

	result = clk_enable(spdif_player->clock);
	if (result != 0) {
		snd_stm_printe("Can't enable clock for player '%s'!\n",
				dev_name(spdif_player->device));
		return result;
	}

	result = clk_set_rate(spdif_player->clock,
				runtime->rate * oversampling);
	if (result != 0) {
		snd_stm_printe("Can't configure clock for player '%s'!\n",
				dev_name(spdif_player->device));
		clk_disable(spdif_player->clock);
		return result;
	}

	/* Configure SPDIF player frequency divider
	 *
	 *                        Fdacclk
	 * divider = ------------------------------- =
	 *            2 * Fs * bits_in_output_frame
	 *
	 *            Fs * oversampling     oversampling
	 *         = ------------------- = --------------
	 *            2 * Fs * (32 * 2)         128
	 * where:
	 *   - Fdacclk - frequency of DAC clock signal, known also as PCMCLK,
	 *               MCLK (master clock), "system clock" etc.
	 *   - Fs - sampling rate (frequency)
	 */

	set__AUD_SPDIF_CTRL__CLK_DIV(spdif_player, oversampling / 128);

	/* Configure NSAMPLE interrupt (in samples,
	 * so period size times channels) */

	set__AUD_SPDIF_CTRL__MEMREAD(spdif_player, runtime->period_size * 2);

	/* Reset IEC958 software formatting counters */

	spdif_player->stream_iec958_status_cnt = 0;
	spdif_player->stream_iec958_subcode_cnt = 0;

	/* Set VUC register settings */

	/* Channel status */
	iec958 = &spdif_player->stream_settings.iec958;
	status = iec958->status[0] | iec958->status[1] << 8 |
		iec958->status[2] << 16 | iec958->status[3] << 24;
	set__AUD_SPDIF_CL1__CL1(spdif_player, status);
	set__AUD_SPDIF_CL2_CR2_UV__CL2(spdif_player, iec958->status[4] & 0xf);
	set__AUD_SPDIF_CR1__CR1(spdif_player, status);
	set__AUD_SPDIF_CL2_CR2_UV__CR2(spdif_player, iec958->status[4] & 0xf);

	/* User data - well, can't do too much here... */
	set__AUD_SPDIF_CL2_CR2_UV__LU(spdif_player, 0);
	set__AUD_SPDIF_CL2_CR2_UV__RU(spdif_player, 0);

	if (spdif_player->stream_settings.encoding_mode ==
			SNDRV_STM_SPDIF_ENCODING_MODE_PCM) {
		/* Linear PCM: validity bit are zeroed */
		set__AUD_SPDIF_CL2_CR2_UV__LV(spdif_player, 0);
		set__AUD_SPDIF_CL2_CR2_UV__RV(spdif_player, 0);
	} else {
		struct snd_stm_spdif_player_settings *settings =
				&spdif_player->stream_settings;

		/* Encoded mode: validity bits are one */
		set__AUD_SPDIF_CL2_CR2_UV__LV(spdif_player, 1);
		set__AUD_SPDIF_CL2_CR2_UV__RV(spdif_player, 1);

		/* Number of frames is data/pause bursts */
		set__AUD_SPDIF_BST_FL__DBURST(spdif_player,
				settings->iec61937_audio_repetition);
		set__AUD_SPDIF_BST_FL__PDBURST(spdif_player,
				settings->iec61937_pause_repetition);

		/* IEC61937 Preamble */
		set__AUD_SPDIF_PA_PB__PA(spdif_player,
				settings->iec61937_preamble[0] |
				settings->iec61937_preamble[1] << 8);
		set__AUD_SPDIF_PA_PB__PB(spdif_player,
				settings->iec61937_preamble[2] |
				settings->iec61937_preamble[3] << 8);
		set__AUD_SPDIF_PC_PD__PC(spdif_player,
				settings->iec61937_preamble[4] |
				settings->iec61937_preamble[5] << 8);
		set__AUD_SPDIF_PC_PD__PD(spdif_player,
				settings->iec61937_preamble[6] |
				settings->iec61937_preamble[7] << 8);

		/* TODO: set AUD_SPDIF_PAU_LAT NPD_BURST somehow... */
	}

	return 0;
}

static int snd_stm_spdif_player_start(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_spdif_player_start(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	/* Un-reset SPDIF player */

	set__AUD_SPDIF_RST__SRSTP__RUNNING(spdif_player);

	/* Launch FDMA transfer */

	result = dma_xfer_list(spdif_player->fdma_channel,
			&spdif_player->fdma_params);
	if (result != 0) {
		snd_stm_printe("Can't launch FDMA transfer for player '%s'!\n",
			       dev_name(spdif_player->device));
		return -EINVAL;
	}
	while (dma_get_status(spdif_player->fdma_channel) !=
			DMA_CHANNEL_STATUS_RUNNING)
		udelay(5);

	/* Enable player interrupts (and clear possible stalled ones) */

	enable_irq(spdif_player->irq);
	set__AUD_SPDIF_ITS_CLR__NSAMPLE__CLEAR(spdif_player);
	set__AUD_SPDIF_IT_EN_SET__NSAMPLE__SET(spdif_player);
	set__AUD_SPDIF_ITS_CLR__UNF__CLEAR(spdif_player);
	set__AUD_SPDIF_IT_EN_SET__UNF__SET(spdif_player);

	/* Launch the player */

	if (spdif_player->stream_settings.encoding_mode ==
			SNDRV_STM_SPDIF_ENCODING_MODE_PCM)
		set__AUD_SPDIF_CTRL__MODE__PCM(spdif_player);
	else
		set__AUD_SPDIF_CTRL__MODE__ENCODED(spdif_player);

	/* Wake up & unmute converter */

	if (spdif_player->conv_group) {
		snd_stm_conv_enable(spdif_player->conv_group,
				0, substream->runtime->channels - 1);
		snd_stm_conv_unmute(spdif_player->conv_group);
	}

	return 0;
}

static int snd_stm_spdif_player_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_spdif_player_stop(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	/* Mute & shutdown converter */

	if (spdif_player->conv_group) {
		snd_stm_conv_mute(spdif_player->conv_group);
		snd_stm_conv_disable(spdif_player->conv_group);
	}

	/* Disable interrupts */

	set__AUD_SPDIF_IT_EN_CLR__NSAMPLE__CLEAR(spdif_player);
	set__AUD_SPDIF_IT_EN_CLR__UNF__CLEAR(spdif_player);
	disable_irq_nosync(spdif_player->irq);

	/* Stop SPDIF player */

	set__AUD_SPDIF_CTRL__MODE__OFF(spdif_player);

	/* Stop FDMA transfer */

	dma_stop_channel(spdif_player->fdma_channel);

	/* Stop the clock and reset SPDIF player */

	clk_disable(spdif_player->clock);
	set__AUD_SPDIF_RST__SRSTP__RESET(spdif_player);

	return 0;
}

static int snd_stm_spdif_player_pause(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_spdif_player_pause(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	/* "Mute" player
	 * Documentation describes this mode in a wrong way - data is _not_
	 * consumed in the "mute" mode, so it is actually a "pause" mode */

	if (spdif_player->stream_settings.encoding_mode ==
			SNDRV_STM_SPDIF_ENCODING_MODE_PCM)
		set__AUD_SPDIF_CTRL__MODE__MUTE_PCM_NULL(spdif_player);
	else
		set__AUD_SPDIF_CTRL__MODE__MUTE_PAUSE_BURSTS(spdif_player);

	return 0;
}

static int snd_stm_spdif_player_release(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
		snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "snd_stm_spdif_player_release(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	/* "Unmute" player */

	if (spdif_player->stream_settings.encoding_mode ==
			SNDRV_STM_SPDIF_ENCODING_MODE_PCM)
		set__AUD_SPDIF_CTRL__MODE__PCM(spdif_player);
	else
		set__AUD_SPDIF_CTRL__MODE__ENCODED(spdif_player);

	return 0;
}

static int snd_stm_spdif_player_trigger(struct snd_pcm_substream *substream,
		int command)
{
	snd_stm_printd(1, "snd_stm_spdif_player_trigger(substream=0x%p,"
			" command=%d)\n", substream, command);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_stm_spdif_player_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_spdif_player_stop(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return snd_stm_spdif_player_pause(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return snd_stm_spdif_player_release(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t snd_stm_spdif_player_pointer(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_spdif_player *spdif_player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue, hwptr;
	snd_pcm_uframes_t pointer;

	snd_stm_printd(2, "snd_stm_spdif_player_pointer(substream=0x%p)\n",
			substream);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));
	BUG_ON(!runtime);

	residue = get_dma_residue(spdif_player->fdma_channel);
	hwptr = (runtime->dma_bytes - residue) % runtime->dma_bytes;
	pointer = bytes_to_frames(runtime, hwptr);

	snd_stm_printd(2, "FDMA residue value is %i and buffer size is %u"
			" bytes...\n", residue, runtime->dma_bytes);
	snd_stm_printd(2, "... so HW pointer in frames is %lu (0x%lx)!\n",
			pointer, pointer);

	return pointer;
}

#define VUC_MASK 0xf
#define V_BIT (1 << 3)
#define U_BIT (1 << 2)
#define C_BIT (1 << 1)

#define GET_SAMPLE(kernel_var, user_ptr, memory_format) \
	do { \
		__get_user(kernel_var, (memory_format __user *)user_ptr); \
		(*((memory_format __user **)&user_ptr))++; \
	} while (0);

static void snd_stm_spdif_player_format_frame(struct snd_stm_spdif_player
		*spdif_player, unsigned long *left_subframe,
		unsigned long *right_subframe)
{
	unsigned char data;

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	/* Clean VUC bits */
	*left_subframe &= ~VUC_MASK;
	*right_subframe &= ~VUC_MASK;

	/* Validity bit should be set to one when non-PCM data are
	 * transmitted... */
	if (spdif_player->stream_settings.encoding_mode) {
		*left_subframe |= V_BIT;
		*right_subframe |= V_BIT;
	}

	/* User data consists of both subframe U-bits */
	data = spdif_player->stream_settings.iec958.subcode[
			spdif_player->stream_iec958_subcode_cnt / 8];
	if (data & (1 << (spdif_player->stream_iec958_subcode_cnt % 8)))
		*left_subframe |= U_BIT;
	spdif_player->stream_iec958_subcode_cnt++;
	if (data & (1 << (spdif_player->stream_iec958_subcode_cnt % 8)))
		*right_subframe |= U_BIT;
	spdif_player->stream_iec958_subcode_cnt =
			(spdif_player->stream_iec958_subcode_cnt + 1) % 1176;

	/* Channel status bit shall be the same for both subframes
	 * (except channel number field, which we ignore.) */
	data = spdif_player->stream_settings.iec958.status[
			spdif_player->stream_iec958_status_cnt / 8];
	if (data & (1 << (spdif_player->stream_iec958_status_cnt % 8))) {
		*left_subframe |= C_BIT;
		*right_subframe |= C_BIT;
	}
	spdif_player->stream_iec958_status_cnt =
			(spdif_player->stream_iec958_status_cnt + 1) % 192;
}

static int snd_stm_spdif_player_copy(struct snd_pcm_substream *substream,
		int channel, snd_pcm_uframes_t pos,
		void __user *src, snd_pcm_uframes_t count)
{
	struct snd_stm_spdif_player *spdif_player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(2, "snd_stm_spdif_player_copy(substream=0x%p, "
			"channel=%d, pos=%lu, buf=0x%p, count=%lu)\n",
			substream, channel, pos, src, count);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));
	BUG_ON(!runtime);
	BUG_ON(channel != -1); /* Interleaved buffer */

	if (spdif_player->stream_settings.input_mode ==
			SNDRV_STM_SPDIF_INPUT_MODE_NORMAL) {
		unsigned long *dest = (unsigned long *)(runtime->dma_area +
				frames_to_bytes(runtime, pos));
		int i;

		if (!access_ok(VERIFY_READ, src, frames_to_bytes(runtime,
						count)))
			return -EFAULT;

		snd_stm_printd(2, "Formatting SPDIF frame (format=%d)\n",
				runtime->format);

#if 0
		{
			unsigned char data[64];

			copy_from_user(data, src, 64);

			snd_stm_printd(0, "Input:\n");
			snd_stm_hex_dump(data, 64);
		}
#endif

		for (i = 0; i < count; i++) {
			unsigned long left_subframe, right_subframe;

			switch (runtime->format) {
			case SNDRV_PCM_FORMAT_S32_LE:
				GET_SAMPLE(left_subframe, src, u32);
				GET_SAMPLE(right_subframe, src, u32);
				break;
			case SNDRV_PCM_FORMAT_S24_LE:
				/* 24-bits sample are in lower 3 bytes,
				 * while we want them in upper 3... ;-) */
				GET_SAMPLE(left_subframe, src, u32);
				left_subframe <<= 8;
				GET_SAMPLE(right_subframe, src, u32);
				right_subframe <<= 8;
				break;
			default:
				snd_BUG();
				return -EINVAL;
			}

			snd_stm_spdif_player_format_frame(spdif_player,
					&left_subframe, &right_subframe);

			*(dest++) = left_subframe;
			*(dest++) = right_subframe;
		}

#if 0
		snd_stm_printd(0, "Output:\n");
		snd_stm_hex_dump(runtime->dma_area +
				frames_to_bytes(runtime, pos), 64);
#endif
	} else {
		/* RAW mode */
		if (copy_from_user(runtime->dma_area +
				frames_to_bytes(runtime, pos), src,
				frames_to_bytes(runtime, count)) != 0)
			return -EFAULT;
	}

	return 0;
}

static int snd_stm_spdif_player_silence(struct snd_pcm_substream *substream,
		int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	int result = 0;
	struct snd_stm_spdif_player *spdif_player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(2, "snd_stm_spdif_player_silence(substream=0x%p, "
			"channel=%d, pos=%lu, count=%lu)\n",
			substream, channel, pos, count);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));
	BUG_ON(!runtime);
	BUG_ON(channel != -1); /* Interleaved buffer */

	if (spdif_player->stream_settings.input_mode ==
			SNDRV_STM_SPDIF_INPUT_MODE_NORMAL) {
		unsigned long *buffer = (unsigned long *)(runtime->dma_area +
				frames_to_bytes(runtime, pos));
		unsigned long left_subframe = 0;
		unsigned long right_subframe = 0;
		int i;

		for (i = 0; i < count; i++) {
			snd_stm_spdif_player_format_frame(spdif_player,
					&left_subframe, &right_subframe);
			*(buffer++) = left_subframe;
			*(buffer++) = right_subframe;
		}
	} else {
		/* RAW mode */
		result = snd_pcm_format_set_silence(runtime->format,
				runtime->dma_area +
				frames_to_bytes(runtime, pos),
				runtime->channels * count);
	}

	return result;
}

static struct snd_pcm_ops snd_stm_spdif_player_spdif_ops = {
	.open =      snd_stm_spdif_player_open,
	.close =     snd_stm_spdif_player_close,
	.mmap =      snd_stm_buffer_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = snd_stm_spdif_player_hw_params,
	.hw_free =   snd_stm_spdif_player_hw_free,
	.prepare =   snd_stm_spdif_player_prepare,
	.trigger =   snd_stm_spdif_player_trigger,
	.pointer =   snd_stm_spdif_player_pointer,
	.copy =      snd_stm_spdif_player_copy,
	.silence =   snd_stm_spdif_player_silence,
};



/*
 * ALSA controls
 */

static int snd_stm_spdif_player_ctl_default_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_default_get("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	ucontrol->value.iec958 = spdif_player->default_settings.iec958;
	spin_unlock(&spdif_player->default_settings_lock);

	return 0;
}

static int snd_stm_spdif_player_ctl_default_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_default_put("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	if (snd_stm_iec958_cmp(&spdif_player->default_settings.iec958,
				&ucontrol->value.iec958) != 0) {
		spdif_player->default_settings.iec958 = ucontrol->value.iec958;
		changed = 1;
	}
	spin_unlock(&spdif_player->default_settings_lock);

	return changed;
}

/* "Raw Data" switch controls data input mode - "RAW" means that played
 * data are already properly formated (VUC bits); in "normal" mode
 * this data will be added by driver according to setting passed in\
 * following controls */

static int snd_stm_spdif_player_ctl_raw_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_raw_get(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	ucontrol->value.integer.value[0] =
			(spdif_player->default_settings.input_mode ==
			SNDRV_STM_SPDIF_INPUT_MODE_RAW);
	spin_unlock(&spdif_player->default_settings_lock);

	return 0;
}

static int snd_stm_spdif_player_ctl_raw_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	enum snd_stm_spdif_player_input_mode input_mode;

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_raw_put(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	if (ucontrol->value.integer.value[0])
		input_mode = SNDRV_STM_SPDIF_INPUT_MODE_RAW;
	else
		input_mode = SNDRV_STM_SPDIF_INPUT_MODE_NORMAL;

	spin_lock(&spdif_player->default_settings_lock);
	changed = (input_mode != spdif_player->default_settings.input_mode);
	spdif_player->default_settings.input_mode = input_mode;
	spin_unlock(&spdif_player->default_settings_lock);

	return changed;
}

/* "Encoded Data" switch selects linear PCM or encoded operation of
 * SPDIF player - the difference is in generating mute data; PCM mode
 * will generate NULL data, encoded - pause bursts */

static int snd_stm_spdif_player_ctl_encoded_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_encoded_get(kcontrol=0x%p, "
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	ucontrol->value.integer.value[0] =
			(spdif_player->default_settings.encoding_mode ==
			SNDRV_STM_SPDIF_ENCODING_MODE_ENCODED);
	spin_unlock(&spdif_player->default_settings_lock);

	return 0;
}

static int snd_stm_spdif_player_ctl_encoded_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	enum snd_stm_spdif_player_encoding_mode encoding_mode;

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_encoded_put(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	if (ucontrol->value.integer.value[0])
		encoding_mode = SNDRV_STM_SPDIF_ENCODING_MODE_ENCODED;
	else
		encoding_mode = SNDRV_STM_SPDIF_ENCODING_MODE_PCM;

	spin_lock(&spdif_player->default_settings_lock);
	changed = (encoding_mode !=
			spdif_player->default_settings.encoding_mode);
	spdif_player->default_settings.encoding_mode = encoding_mode;
	spin_unlock(&spdif_player->default_settings_lock);

	return changed;
}

/* Three following controls are valid for encoded mode only - they
 * control IEC 61937 preamble and data burst periods (see mentioned
 * standard for more informations) */

static int snd_stm_spdif_player_ctl_preamble_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = PREAMBLE_BYTES;
	return 0;
}

static int snd_stm_spdif_player_ctl_preamble_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_preamble_get(kcontrol=0x%p"
			", ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	memcpy(ucontrol->value.bytes.data,
			spdif_player->default_settings.iec61937_preamble,
			PREAMBLE_BYTES);
	spin_unlock(&spdif_player->default_settings_lock);

	return 0;
}

static int snd_stm_spdif_player_ctl_preamble_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_preamble_put(kcontrol=0x%p"
			", ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	if (memcmp(spdif_player->default_settings.iec61937_preamble,
			ucontrol->value.bytes.data, PREAMBLE_BYTES) != 0) {
		changed = 1;
		memcpy(spdif_player->default_settings.iec61937_preamble,
				ucontrol->value.bytes.data, PREAMBLE_BYTES);
	}
	spin_unlock(&spdif_player->default_settings_lock);

	return changed;
}

static int snd_stm_spdif_player_ctl_repetition_info(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff;
	return 0;
}

static int snd_stm_spdif_player_ctl_audio_repetition_get(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_audio_repetition_get("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	ucontrol->value.integer.value[0] =
		spdif_player->default_settings.iec61937_audio_repetition;
	spin_unlock(&spdif_player->default_settings_lock);

	return 0;
}

static int snd_stm_spdif_player_ctl_audio_repetition_put(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_audio_repetition_put("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	if (spdif_player->default_settings.iec61937_audio_repetition !=
			ucontrol->value.integer.value[0]) {
		changed = 1;
		spdif_player->default_settings.iec61937_audio_repetition =
				ucontrol->value.integer.value[0];
	}
	spin_unlock(&spdif_player->default_settings_lock);

	return changed;
}

static int snd_stm_spdif_player_ctl_pause_repetition_get(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_pause_repetition_get("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	ucontrol->value.integer.value[0] =
		spdif_player->default_settings.iec61937_pause_repetition;
	spin_unlock(&spdif_player->default_settings_lock);

	return 0;
}

static int snd_stm_spdif_player_ctl_pause_repetition_put(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "snd_stm_spdif_player_ctl_pause_repetition_put("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	spin_lock(&spdif_player->default_settings_lock);
	if (spdif_player->default_settings.iec61937_pause_repetition !=
			ucontrol->value.integer.value[0]) {
		changed = 1;
		spdif_player->default_settings.iec61937_pause_repetition =
				ucontrol->value.integer.value[0];
	}
	spin_unlock(&spdif_player->default_settings_lock);

	return changed;
}

static struct snd_kcontrol_new snd_stm_spdif_player_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_iec958_info,
		.get = snd_stm_spdif_player_ctl_default_get,
		.put = snd_stm_spdif_player_ctl_default_put,
	}, {
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name =	SNDRV_CTL_NAME_IEC958("", PLAYBACK, CON_MASK),
		.info =	snd_stm_ctl_iec958_info,
		.get = snd_stm_ctl_iec958_mask_get_con,
	}, {
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name =	SNDRV_CTL_NAME_IEC958("", PLAYBACK, PRO_MASK),
		.info =	snd_stm_ctl_iec958_info,
		.get = snd_stm_ctl_iec958_mask_get_pro,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Raw Data ", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_boolean_info,
		.get = snd_stm_spdif_player_ctl_raw_get,
		.put = snd_stm_spdif_player_ctl_raw_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Encoded Data ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_boolean_info,
		.get = snd_stm_spdif_player_ctl_encoded_get,
		.put = snd_stm_spdif_player_ctl_encoded_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Preamble ", PLAYBACK, DEFAULT),
		.info = snd_stm_spdif_player_ctl_preamble_info,
		.get = snd_stm_spdif_player_ctl_preamble_get,
		.put = snd_stm_spdif_player_ctl_preamble_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Audio Burst Period ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_spdif_player_ctl_repetition_info,
		.get = snd_stm_spdif_player_ctl_audio_repetition_get,
		.put = snd_stm_spdif_player_ctl_audio_repetition_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Pause Burst Period ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_spdif_player_ctl_repetition_info,
		.get = snd_stm_spdif_player_ctl_pause_repetition_get,
		.put = snd_stm_spdif_player_ctl_pause_repetition_put,
	}
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_SPDIF_%s (offset 0x%02x) =" \
				" 0x%08x\n", __stringify(r), \
				offset__AUD_SPDIF_##r(spdif_player), \
				get__AUD_SPDIF_##r(spdif_player))

static void snd_stm_spdif_player_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_spdif_player *spdif_player = entry->private_data;

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	snd_iprintf(buffer, "--- %s ---\n", dev_name(spdif_player->device));
	snd_iprintf(buffer, "base = 0x%p\n", spdif_player->base);

	DUMP_REGISTER(RST);
	DUMP_REGISTER(DATA);
	DUMP_REGISTER(ITS);
	DUMP_REGISTER(ITS_CLR);
	DUMP_REGISTER(IT_EN);
	DUMP_REGISTER(IT_EN_SET);
	DUMP_REGISTER(IT_EN_CLR);
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(STA);
	DUMP_REGISTER(PA_PB);
	DUMP_REGISTER(PC_PD);
	DUMP_REGISTER(CL1);
	DUMP_REGISTER(CR1);
	DUMP_REGISTER(CL2_CR2_UV);
	DUMP_REGISTER(PAU_LAT);
	DUMP_REGISTER(BST_FL);
	if (spdif_player->ver >= 4)
		DUMP_REGISTER(CONFIG);

	snd_iprintf(buffer, "\n");
}

static int snd_stm_spdif_player_register(struct snd_device *snd_device)
{
	int result;
	struct snd_stm_spdif_player *spdif_player = snd_device->device_data;
	int i;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	/* Initialize hardware (format etc.) */

	set__AUD_SPDIF_RST__SRSTP__RESET(spdif_player);

	/* TODO: well, hardcoded - shall anyone use it?
	 * and what does it actually mean? */
	set__AUD_SPDIF_CTRL__RND__NO_ROUNDING(spdif_player);

	set__AUD_SPDIF_CTRL__IDLE__NORMAL(spdif_player);

	/* Hardware stuffing is not implemented yet... */
	/* TODO: oh, is that so? */
	set__AUD_SPDIF_CTRL__STUFFING__SOFTWARE(spdif_player);

	/* Get frequency synthesizer channel */

	BUG_ON(!spdif_player->info->clock_name);
	snd_stm_printd(0, "Player connected to clock '%s'.\n",
			spdif_player->info->clock_name);
	spdif_player->clock = snd_stm_clk_get(spdif_player->device,
			spdif_player->info->clock_name, snd_device->card,
			spdif_player->info->card_device);
	if (!spdif_player->clock || IS_ERR(spdif_player->clock)) {
		snd_stm_printe("Failed to get a clock for '%s'!\n",
				dev_name(spdif_player->device));
		return -EINVAL;
	}

	/* Registers view in ALSA's procfs */

	snd_stm_info_register(&spdif_player->proc_entry,
			dev_name(spdif_player->device),
			snd_stm_spdif_player_dump_registers, spdif_player);

	/* Create SPDIF ALSA controls */

	for (i = 0; i < ARRAY_SIZE(snd_stm_spdif_player_ctls); i++) {
		snd_stm_spdif_player_ctls[i].device =
				spdif_player->info->card_device;
		result = snd_ctl_add(snd_device->card,
				snd_ctl_new1(&snd_stm_spdif_player_ctls[i],
				spdif_player));
		if (result < 0) {
			snd_stm_printe("Failed to add SPDIF ALSA control!\n");
			return result;
		}
		snd_stm_spdif_player_ctls[i].index++;
	}

	return 0;
}

static int snd_stm_spdif_player_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_spdif_player *spdif_player = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	snd_stm_clk_put(spdif_player->clock);

	snd_stm_info_unregister(spdif_player->proc_entry);

	return 0;
}

static struct snd_device_ops snd_stm_spdif_player_snd_device_ops = {
	.dev_register = snd_stm_spdif_player_register,
	.dev_disconnect = snd_stm_spdif_player_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_spdif_player_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_spdif_player *spdif_player;
	struct snd_card *card = snd_stm_card_get();
	int buffer_bytes_max;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);

	spdif_player = kzalloc(sizeof(*spdif_player), GFP_KERNEL);
	if (!spdif_player) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(spdif_player);
	spdif_player->info = pdev->dev.platform_data;
	BUG_ON(!spdif_player->info);
	spdif_player->ver = spdif_player->info->ver;
	BUG_ON(spdif_player->ver <= 0);
	spdif_player->device = &pdev->dev;

	spin_lock_init(&spdif_player->default_settings_lock);

	/* Get resources */

	result = snd_stm_memory_request(pdev, &spdif_player->mem_region,
			&spdif_player->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}
	spdif_player->fifo_phys_address = spdif_player->mem_region->start +
			offset__AUD_SPDIF_DATA(spdif_player);
	snd_stm_printd(0, "FIFO physical address: 0x%lx.\n",
			spdif_player->fifo_phys_address);

	result = snd_stm_irq_request(pdev, &spdif_player->irq,
			snd_stm_spdif_player_irq_handler, spdif_player);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	result = snd_stm_fdma_request(pdev, &spdif_player->fdma_channel);
	if (result < 0) {
		snd_stm_printe("FDMA request failed!\n");
		goto error_fdma_request;
	}

	/* FDMA transfer size depends (among others ;-) on FIFO length,
	 * which is:
	 * - 6 cells (24 bytes) in STx7100/9 and STx7200 cut 1.0
	 * - 30 cells (120 bytes) in STx7111 and STx7200 cut 2.0. */

	if (spdif_player->ver < 3)
		spdif_player->fdma_max_transfer_size = 2;
	else if (spdif_player->ver == 3)
		spdif_player->fdma_max_transfer_size = 4;
	else
		spdif_player->fdma_max_transfer_size = 20;

	/* Get component caps */

	snd_stm_printd(0, "Player's name is '%s'\n", spdif_player->info->name);

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, spdif_player,
			&snd_stm_spdif_player_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, spdif_player->info->card_device, 1, 0,
			&spdif_player->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	spdif_player->pcm->private_data = spdif_player;
	strcpy(spdif_player->pcm->name, spdif_player->info->name);

	snd_pcm_set_ops(spdif_player->pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_stm_spdif_player_spdif_ops);

	/* Initialize buffer */

	buffer_bytes_max = snd_stm_spdif_player_hw_normal.buffer_bytes_max;
	if (buffer_bytes_max < snd_stm_spdif_player_hw_raw.buffer_bytes_max)
		buffer_bytes_max = snd_stm_spdif_player_hw_raw.buffer_bytes_max;
	spdif_player->buffer = snd_stm_buffer_create(spdif_player->pcm,
			spdif_player->device, buffer_bytes_max);
	if (!spdif_player->buffer) {
		snd_stm_printe("Cannot initialize buffer!\n");
		result = -ENOMEM;
		goto error_buffer_create;
	}

	/* Register in converters router */

	spdif_player->conv_source = snd_stm_conv_register_source(
			&platform_bus_type, dev_name(&pdev->dev),
			2, card, spdif_player->info->card_device);
	if (!spdif_player->conv_source) {
		snd_stm_printe("Cannot register in converters router!\n");
		result = -ENOMEM;
		goto error_conv_register_source;
	}

	/* Claim the pads */

	if (spdif_player->info->pad_config) {
		spdif_player->pads = stm_pad_claim(
				spdif_player->info->pad_config,
				dev_name(&pdev->dev));
		if (!spdif_player->pads) {
			snd_stm_printe("Failed to claimed pads for '%s'!\n",
					dev_name(&pdev->dev));
			result = -EBUSY;
			goto error_pad_claim;
		}
	}

	/* Done now */

	platform_set_drvdata(pdev, spdif_player);

	return 0;

error_pad_claim:
	snd_stm_conv_unregister_source(spdif_player->conv_source);
error_conv_register_source:
	snd_stm_buffer_dispose(spdif_player->buffer);
error_buffer_create:
	/* snd_pcm_free() is not available - PCM device will be released
	 * during card release */
error_pcm:
	snd_device_free(card, spdif_player);
error_device:
	snd_stm_fdma_release(spdif_player->fdma_channel);
error_fdma_request:
	snd_stm_irq_release(spdif_player->irq, spdif_player);
error_irq_request:
	snd_stm_memory_release(spdif_player->mem_region, spdif_player->base);
error_memory_request:
	snd_stm_magic_clear(spdif_player);
	kfree(spdif_player);
error_alloc:
	return result;
}

static int snd_stm_spdif_player_remove(struct platform_device *pdev)
{
	struct snd_stm_spdif_player *spdif_player = platform_get_drvdata(pdev);

	snd_stm_printd(1, "snd_stm_spdif_player_remove(pdev=%p)\n", pdev);

	BUG_ON(!spdif_player);
	BUG_ON(!snd_stm_magic_valid(spdif_player));

	if (spdif_player->pads)
		stm_pad_release(spdif_player->pads);
	snd_stm_conv_unregister_source(spdif_player->conv_source);
	snd_stm_buffer_dispose(spdif_player->buffer);
	snd_stm_fdma_release(spdif_player->fdma_channel);
	snd_stm_irq_release(spdif_player->irq, spdif_player);
	snd_stm_memory_release(spdif_player->mem_region, spdif_player->base);

	snd_stm_magic_clear(spdif_player);
	kfree(spdif_player);

	return 0;
}

static struct platform_driver snd_stm_spdif_player_driver = {
	.driver.name = "snd_spdif_player",
	.probe = snd_stm_spdif_player_probe,
	.remove = snd_stm_spdif_player_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_spdif_player_init(void)
{
	return platform_driver_register(&snd_stm_spdif_player_driver);
}

static void __exit snd_stm_spdif_player_exit(void)
{
	platform_driver_unregister(&snd_stm_spdif_player_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics SPDIF player driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_spdif_player_init);
module_exit(snd_stm_spdif_player_exit);
