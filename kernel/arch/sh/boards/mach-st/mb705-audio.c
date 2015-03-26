/*
 * arch/sh/boards/mach-st/mb705-audio.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * WARNING! This driver (so far) supports only mb705+mb680 duet.
 * (SW2[1..4] should be OFF)
 * In that case (mb705+mb680) audio outputs layout presents as follows:
 *
 * +--------------------------------------+
 * |                                      |
 * |  (S.I)   (---)  (---)  (0.5)  (0.6)  | TOP
 * |                                      |
 * |  (---)   (---)  (---)  (0.3)  (0.4)  |
 * |                                      |
 * |  (S.O)   (---)  (---)  (0.1)  (0.2)  | BOTTOM
 * |                                      |
 * +--------------------------------------+
 *     CN5     CN4    CN3    CN2     CN1
 *
 * where:
 *   - S.I - SPDIF input - PCM Reader #0
 *   - S.O - SPDIF output - SPDIF Player (HDMI)
 *   - 0.1-6 - audio outputs - PCM Player #0, channels 1 to 6
 *     (PCM Player #0 has 8-channels output, however only 3 pairs
 *     are available on pads)
 */

#include <linux/init.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <sound/stm.h>
#include <mach/common.h>
#include "mb705-epld.h"

struct mb705_audio_conv {
	struct snd_stm_conv_converter *conv;

	unsigned int reset_mask;

	unsigned int mute_offset;
	unsigned int mute_mask;
};

static struct mb705_audio_conv mb705_audio_spdif_input = {
	.reset_mask = EPLD_AUDIO_RESET_SPDIFIN,
};

static struct mb705_audio_conv mb705_audio_8ch_dac = {
	.reset_mask = EPLD_AUDIO_RESET_AUDDAC2,
	.mute_offset = EPLD_AUDIO_DAC2,
	.mute_mask = EPLD_AUDIO_DAC2_SMUTE,
};



static unsigned int mb705_audio_get_format(void *priv)
{
	return SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS;
}

static int mb705_audio_get_oversampling(void *priv)
{
	return 256;
}

static int mb705_audio_set_enabled(int enabled, void *priv)
{
	struct mb705_audio_conv *data = priv;
	unsigned int value = epld_read(EPLD_AUDIO_RESET);

	if (enabled)
		value &= ~data->reset_mask;
	else
		value |= data->reset_mask;

	epld_write(value, EPLD_AUDIO_RESET);

	return 0;
}

static int mb705_audio_set_muted(int muted, void *priv)
{
	struct mb705_audio_conv *data = priv;
	unsigned int value = epld_read(data->mute_offset);

	if (muted)
		value |= data->mute_mask;
	else
		value &= ~data->mute_mask;

	epld_write(value, data->mute_offset);

	return 0;
}

static struct snd_stm_conv_ops mb705_audio_enable_ops = {
	.get_format = mb705_audio_get_format,
	.get_oversampling = mb705_audio_get_oversampling,
	.set_enabled = mb705_audio_set_enabled,
};

static struct snd_stm_conv_ops mb705_audio_enable_mute_ops = {
	.get_format = mb705_audio_get_format,
	.get_oversampling = mb705_audio_get_oversampling,
	.set_enabled = mb705_audio_set_enabled,
	.set_muted = mb705_audio_set_muted,
};



static int __init mb705_audio_init(void)
{
	int pcm_reader, pcm_player;
	static char *pcm_reader_bus_id = "snd_pcm_reader.x";
	static char *pcm_player_bus_id = "snd_pcm_player.x";

	/* To be defined in processor board, which knows what SOC is there... */
	mbxxx_configure_audio_pins(&pcm_reader, &pcm_player);

	if (pcm_reader < 0)
		sprintf(pcm_reader_bus_id, "snd_pcm_reader");
	else if (pcm_reader < 10)
		sprintf(pcm_reader_bus_id, "snd_pcm_reader.%d", pcm_reader);
	else
		BUG();

	if (pcm_player < 0)
		sprintf(pcm_player_bus_id, "snd_pcm_player");
	else if (pcm_player < 10)
		sprintf(pcm_player_bus_id, "snd_pcm_player.%d", pcm_player);
	else
		BUG();

	/* Check the SPDIF test mode */
	if ((epld_read(EPLD_AUDIO_SWITCH2) & (EPLD_AUDIO_SWITCH1_SW21 |
			EPLD_AUDIO_SWITCH1_SW22 | EPLD_AUDIO_SWITCH1_SW23 |
			EPLD_AUDIO_SWITCH1_SW24)) == 0) {
		printk(KERN_WARNING "WARNING! MB705 is in audio test mode!\n");
		printk(KERN_WARNING "You won't hear any generated sound!\n");
	}

	/* Disable (enable reset) all converters */

	epld_write(EPLD_AUDIO_RESET_AUDDAC0 | EPLD_AUDIO_RESET_AUDDAC1 |
			EPLD_AUDIO_RESET_AUDDAC2 | EPLD_AUDIO_RESET_SPDIFIN |
			EPLD_AUDIO_RESET_SPDIFOUT, EPLD_AUDIO_RESET);

	/* Configure and register SPDIF-I2S converter (CS8416, IC5) */

	epld_write(EPLD_AUDIO_SPDIFIN_C | EPLD_AUDIO_SPDIFIN_RCBL,
			EPLD_AUDIO_SPDIFIN);
	mb705_audio_spdif_input.conv = snd_stm_conv_register_converter("SPDIF"
			" Input", &mb705_audio_enable_ops,
			&mb705_audio_spdif_input, &platform_bus_type,
			pcm_reader_bus_id, 0, 1, NULL);
	if (!mb705_audio_spdif_input.conv) {
		printk(KERN_ERR "%s:%u: Can't register SPDIF Input converter!"
				"\n", __FILE__, __LINE__);
		goto error;
	}

	/* Configure and register 8-channels external DAC (AK4359, IC1) */

	epld_write(EPLD_AUDIO_DAC2_DIF0 | EPLD_AUDIO_DAC2_DIF1 |
			EPLD_AUDIO_DAC2_SMUTE | EPLD_AUDIO_DAC2_ACKS |
			EPLD_AUDIO_DAC2_DEM0 | EPLD_AUDIO_DAC2_PNOTS,
			EPLD_AUDIO_DAC2);
	mb705_audio_8ch_dac.conv = snd_stm_conv_register_converter("External "
			"8-channels DAC", &mb705_audio_enable_mute_ops,
			&mb705_audio_8ch_dac, &platform_bus_type,
			pcm_player_bus_id, 0, 7, NULL);
	if (!mb705_audio_8ch_dac.conv) {
		printk(KERN_ERR "%s:%u: Can't register external 8-channels "
				"DAC!\n", __FILE__, __LINE__);
		goto error;
	}

	return 0;

error:
	if (mb705_audio_8ch_dac.conv)
		snd_stm_conv_unregister_converter(mb705_audio_8ch_dac.conv);
	if (mb705_audio_spdif_input.conv)
		snd_stm_conv_unregister_converter(mb705_audio_spdif_input.conv);

	return -ENODEV;
}

static void __exit mb705_audio_exit(void)
{
	/* Disable all converters, just to be sure ;-) */

	epld_write(EPLD_AUDIO_RESET_AUDDAC0 | EPLD_AUDIO_RESET_AUDDAC1 |
			EPLD_AUDIO_RESET_AUDDAC2 | EPLD_AUDIO_RESET_SPDIFIN |
			EPLD_AUDIO_RESET_SPDIFOUT, EPLD_AUDIO_RESET);

	/* Unregister converters */

	snd_stm_conv_unregister_converter(mb705_audio_8ch_dac.conv);
	snd_stm_conv_unregister_converter(mb705_audio_spdif_input.conv);
}

module_init(mb705_audio_init);
module_exit(mb705_audio_exit);
