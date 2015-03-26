/*
 *   STMicroelectronics System-on-Chips' I2S to SPDIF converter driver
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
#include <linux/list.h>
#include <linux/spinlock.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#include "common.h"
#include "reg_aud_spdifpc.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Hardware-related definitions
 */

#define DEFAULT_OVERSAMPLING 128



/*
 * Converter instance structure
 */

struct snd_stm_conv_i2sspdif {
	/* System informations */
	struct snd_stm_conv_converter *converter;
	struct snd_stm_conv_i2sspdif_info *info;
	struct device *device;
	int index; /* ALSA controls index */
	int ver; /* IP version, used by register access macros */

	/* Resources */
	struct resource *mem_region;
	void *base;

	/* Default configuration */
	struct snd_aes_iec958 iec958_default;
	spinlock_t iec958_default_lock; /* Protects iec958_default */

	/* Runtime data */
	int enabled;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Internal routines
 */

/* Such a empty (zeroed) structure is pretty useful later... ;-) */
static struct snd_aes_iec958 snd_stm_conv_i2sspdif_iec958_zeroed;



#define CHA_STA_TRIES 50000

static int snd_stm_conv_i2sspdif_iec958_set(struct snd_stm_conv_i2sspdif
		*conv_i2sspdif, struct snd_aes_iec958 *iec958)
{
	int i, j, ok;
	unsigned long status[6];

	snd_stm_printd(1, "snd_stm_conv_i2sspdif_iec958_set(conv_i2sspdif=%p"
			", iec958=%p)\n", conv_i2sspdif, iec958);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));

	/* I2S to SPDIF converter should be used only for playing
	 * PCM (non compressed) data, so validity bit should be always
	 * zero... (it means "valid linear PCM data") */
	set__AUD_SPDIFPC_VAL__VALIDITY_BITS(conv_i2sspdif, 0);

	/* Well... User data bit... Frankly speaking there is no way
	 * of correctly setting them with a mechanism provided by
	 * converter hardware, so it is better not to do this at all... */
	set__AUD_SPDIFPC_DATA__USER_DATA_BITS(conv_i2sspdif, 0);

	BUG_ON(memcmp(snd_stm_conv_i2sspdif_iec958_zeroed.subcode,
			  iec958->subcode, sizeof(iec958->subcode)) != 0);

	if (conv_i2sspdif->ver < 4) {
		/* Converter hardware by default puts every single bit of
		 * status to separate SPDIF subframe (instead of putting
		 * the same bit to both left and right subframes).
		 * So we have to prepare a "duplicated" version of
		 * status bits... Note that in such way status will be
		 * transmitted twice in every block! This is definitely
		 * out of spec, but fortunately most of receivers pay
		 * attention only to first 36 bits... */

		for (i = 0; i < 6; i++) {
			unsigned long word = 0;

			for (j = 1; j >= 0; j--) {
				unsigned char byte = iec958->status[i * 2 + j];
				int k;

				for (k = 0; k < 8; k++) {
					word |= ((byte & 0x80) != 0);
					if (!(j == 0 && k == 7)) {
						word <<= 2;
						byte <<= 1;
					}
				}
			}

			status[i] = word | (word << 1);
		}
	} else {
		/* Fortunately in some hardware there is a "sane" mode
		 * of channel status registers operation... :-) */

		for (i = 0; i < 6; i++)
			status[i] = iec958->status[i * 4] |
					iec958->status[i * 4 + 1] << 8 |
					iec958->status[i * 4 + 2] << 16 |
					iec958->status[i * 4 + 3] << 24;
	}

	/* Set converter's channel status registers - they are realised
	 * in such a ridiculous way that write to them is enabled only
	 * in (about) 300us time window after CHL_STS_BUFF_EMPTY bit
	 * is asserted... And this happens once every 2ms (only when
	 * converter is enabled and gets data...) */

	ok = 0;
	for (i = 0; i < CHA_STA_TRIES; i++) {
		if (get__AUD_SPDIFPC_STA__CHL_STS_BUFF_EMPTY(conv_i2sspdif)) {
			for (j = 0; j < 6; j++)
				set__AUD_SPDIFPC_CHA_STA(conv_i2sspdif, j,
						status[j]);
			ok = 1;
			for (j = 0; j < 6; j++)
				if (get__AUD_SPDIFPC_CHA_STA(conv_i2sspdif,
						j) != status[j]) {
					ok = 0;
					break;
				}
			if (ok)
				break;
		}
	}
	if (!ok) {
		snd_stm_printe("WARNING! Failed to set channel status registers"
				" for converter %s! (tried %d times)\n",
			       dev_name(conv_i2sspdif->device), i);
		return -EINVAL;
	}

	snd_stm_printd(1, "Channel status registers set successfully "
			"in %i tries.\n", i);

	/* Set SPDIF player's VUC registers (these are used only
	 * for mute data formatting, and it should never happen ;-) */

	set__AUD_SPDIFPC_SUV__VAL_LEFT(conv_i2sspdif, 0);
	set__AUD_SPDIFPC_SUV__VAL_RIGHT(conv_i2sspdif, 0);

	set__AUD_SPDIFPC_SUV__DATA_LEFT(conv_i2sspdif, 0);
	set__AUD_SPDIFPC_SUV__DATA_RIGHT(conv_i2sspdif, 0);

	/* And this time the problem is that SPDIF player lets
	 * to set only first 36 bits of channel status bits...
	 * Hopefully no one needs more ever ;-) And well - at least
	 * it puts channel status bits to both subframes :-) */
	status[0] = iec958->status[0] | iec958->status[1] << 8 |
		iec958->status[2] << 16 | iec958->status[3] << 24;
	set__AUD_SPDIFPC_CL1__CHANNEL_STATUS(conv_i2sspdif, status[0]);
	set__AUD_SPDIFPC_SUV__CH_STA_LEFT(conv_i2sspdif,
			iec958->status[4] & 0xf);
	set__AUD_SPDIFPC_CR1__CH_STA(conv_i2sspdif, status[0]);
	set__AUD_SPDIFPC_SUV__CH_STA_RIGHT(conv_i2sspdif,
			iec958->status[4] & 0xf);

	return 0;
}

static int snd_stm_conv_i2sspdif_oversampling(struct snd_stm_conv_i2sspdif
		*conv_i2sspdif)
{
	snd_stm_printd(1, "snd_stm_conv_i2sspdif_oversampling("
			"conv_i2sspdif=%p)\n", conv_i2sspdif);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));

	return DEFAULT_OVERSAMPLING;
}

static int snd_stm_conv_i2sspdif_enable(struct snd_stm_conv_i2sspdif
		*conv_i2sspdif)
{
	int oversampling;
	struct snd_aes_iec958 iec958;

	snd_stm_printd(1, "snd_stm_conv_i2sspdif_enable(conv_i2sspdif=%p)\n",
			conv_i2sspdif);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));
	BUG_ON(conv_i2sspdif->enabled);

	oversampling = snd_stm_conv_i2sspdif_oversampling(conv_i2sspdif);
	BUG_ON(oversampling <= 0);
	BUG_ON((oversampling % 128) != 0);

	set__AUD_SPDIFPC_CFG(conv_i2sspdif,
		mask__AUD_SPDIFPC_CFG__DEVICE_EN__ENABLED(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__SW_RESET__RUNNING(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__FIFO_EN__ENABLED(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__AUDIO_WORD_SIZE__24_BITS(conv_i2sspdif)
		| mask__AUD_SPDIFPC_CFG__REQ_ACK_EN__ENABLED(conv_i2sspdif));
	set__AUD_SPDIFPC_CTRL(conv_i2sspdif,
		mask__AUD_SPDIFPC_CTRL__OPERATION__PCM(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CTRL__ROUNDING__NO_ROUNDING(conv_i2sspdif));
	set__AUD_SPDIFPC_CTRL__DIVIDER(conv_i2sspdif, oversampling / 128);

	/* Full channel status processing - an undocumented feature that
	 * exists in some hardware... Normally channel status registers
	 * provides bits for each subframe, so only for 96 frames (a half
	 * of SPDIF block) - pathetic! ;-) Setting bit 6 of config register
	 * enables a mode in which channel status bits in L/R subframes
	 * are identical, and whole block is served... */
	if (conv_i2sspdif->ver >= 4)
		set__AUD_SPDIFPC_CFG__CHA_STA_BITS__FRAME(conv_i2sspdif);

	spin_lock(&conv_i2sspdif->iec958_default_lock);
	iec958 = conv_i2sspdif->iec958_default;
	spin_unlock(&conv_i2sspdif->iec958_default_lock);
	if (snd_stm_conv_i2sspdif_iec958_set(conv_i2sspdif, &iec958) != 0)
		snd_stm_printe("WARNING! Can't set channel status "
				"registers!\n");

	conv_i2sspdif->enabled = 1;

	return 0;
}

static int snd_stm_conv_i2sspdif_disable(struct snd_stm_conv_i2sspdif
		*conv_i2sspdif)
{
	snd_stm_printd(1, "snd_stm_conv_i2sspdif_disable(conv_i2sspdif=%p)\n",
			conv_i2sspdif);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));
	BUG_ON(!conv_i2sspdif->enabled);

	if (snd_stm_conv_i2sspdif_iec958_set(conv_i2sspdif,
			&snd_stm_conv_i2sspdif_iec958_zeroed) != 0)
		snd_stm_printe("WARNING! Failed to clear channel status "
				"registers!\n");

	set__AUD_SPDIFPC_CFG(conv_i2sspdif,
		mask__AUD_SPDIFPC_CFG__DEVICE_EN__DISABLED(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__SW_RESET__RESET(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__FIFO_EN__DISABLED(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__REQ_ACK_EN__DISABLED(conv_i2sspdif));
	set__AUD_SPDIFPC_CTRL(conv_i2sspdif,
		mask__AUD_SPDIFPC_CTRL__OPERATION__OFF(conv_i2sspdif));

	conv_i2sspdif->enabled = 0;

	return 0;
}



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_i2sspdif_get_format(void *priv)
{
	snd_stm_printd(1, "snd_stm_conv_i2sspdif_get_format(priv=%p)\n", priv);

	return SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS;
}

static int snd_stm_conv_i2sspdif_get_oversampling(void *priv)
{
	struct snd_stm_conv_i2sspdif *conv_i2sspdif = priv;

	snd_stm_printd(1, "snd_stm_conv_i2sspdif_get_oversampling(priv=%p)\n",
			priv);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));

	return snd_stm_conv_i2sspdif_oversampling(conv_i2sspdif);
}

static int snd_stm_conv_i2sspdif_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_i2sspdif *conv_i2sspdif = priv;

	snd_stm_printd(1, "snd_stm_conv_i2sspdif_set_enabled(enabled=%d, "
			"priv=%p)\n", enabled, priv);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));

	snd_stm_printd(1, "%sabling I2S to SPDIF converter '%s'.\n",
			enabled ? "En" : "Dis",
			dev_name(conv_i2sspdif->device));

	if (enabled)
		return snd_stm_conv_i2sspdif_enable(conv_i2sspdif);
	else
		return snd_stm_conv_i2sspdif_disable(conv_i2sspdif);
}

static struct snd_stm_conv_ops snd_stm_conv_i2sspdif_ops = {
	.get_format = snd_stm_conv_i2sspdif_get_format,
	.get_oversampling = snd_stm_conv_i2sspdif_get_oversampling,
	.set_enabled = snd_stm_conv_i2sspdif_set_enabled,
};



/*
 * ALSA controls
 */

static int snd_stm_conv_i2sspdif_ctl_default_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_i2sspdif *conv_i2sspdif =
			snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_conv_i2sspdif_ctl_default_get("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));

	spin_lock(&conv_i2sspdif->iec958_default_lock);
	ucontrol->value.iec958 = conv_i2sspdif->iec958_default;
	spin_unlock(&conv_i2sspdif->iec958_default_lock);

	return 0;
}

static int snd_stm_conv_i2sspdif_ctl_default_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_i2sspdif *conv_i2sspdif =
			snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "snd_stm_conv_i2sspdif_ctl_default_put("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));

	spin_lock(&conv_i2sspdif->iec958_default_lock);
	if (snd_stm_iec958_cmp(&conv_i2sspdif->iec958_default,
				&ucontrol->value.iec958) != 0) {
		conv_i2sspdif->iec958_default = ucontrol->value.iec958;
		changed = 1;
	}
	spin_unlock(&conv_i2sspdif->iec958_default_lock);

	return changed;
}

static struct snd_kcontrol_new snd_stm_conv_i2sspdif_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_iec958_info,
		.get = snd_stm_conv_i2sspdif_ctl_default_get,
		.put = snd_stm_conv_i2sspdif_ctl_default_put,
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
	},
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_SPDIFPC_%s (offset 0x%03x) =" \
				" 0x%08x\n", __stringify(r), \
				offset__AUD_SPDIFPC_##r(conv_i2sspdif), \
				get__AUD_SPDIFPC_##r(conv_i2sspdif))

static void snd_stm_conv_i2sspdif_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_i2sspdif *conv_i2sspdif =
		entry->private_data;
	int i;

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));

	snd_iprintf(buffer, "--- %s ---\n", dev_name(conv_i2sspdif->device));
	snd_iprintf(buffer, "base = 0x%p\n", conv_i2sspdif->base);

	DUMP_REGISTER(CFG);
	DUMP_REGISTER(STA);
	DUMP_REGISTER(IT_EN);
	DUMP_REGISTER(ITS);
	DUMP_REGISTER(IT_CLR);
	DUMP_REGISTER(VAL);
	DUMP_REGISTER(DATA);
	for (i = 0; i <= 5; i++)
		snd_iprintf(buffer, "AUD_SPDIFPC_CHA_STA%d_CHANNEL_STATUS_BITS"
				" (offset 0x%03x) = 0x%08x\n", i,
				offset__AUD_SPDIFPC_CHA_STA(conv_i2sspdif, i),
				get__AUD_SPDIFPC_CHA_STA(conv_i2sspdif, i));
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(SPDIFSTA);
	DUMP_REGISTER(PAUSE);
	DUMP_REGISTER(DATA_BURST);
	DUMP_REGISTER(PA_PB);
	DUMP_REGISTER(PC_PD);
	DUMP_REGISTER(CL1);
	DUMP_REGISTER(CR1);
	DUMP_REGISTER(SUV);

	snd_iprintf(buffer, "\n");
}

static int snd_stm_conv_i2sspdif_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_i2sspdif *conv_i2sspdif = snd_device->device_data;
	int i;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));
	BUG_ON(conv_i2sspdif->enabled);

	/* Initialize converter's input & SPDIF player as disabled */

	set__AUD_SPDIFPC_CFG(conv_i2sspdif,
		mask__AUD_SPDIFPC_CFG__DEVICE_EN__DISABLED(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__SW_RESET__RESET(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__FIFO_EN__DISABLED(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__REQ_ACK_EN__DISABLED(conv_i2sspdif));

	set__AUD_SPDIFPC_CTRL(conv_i2sspdif,
		mask__AUD_SPDIFPC_CTRL__OPERATION__OFF(conv_i2sspdif));

	/* Additional procfs info */

	snd_stm_info_register(&conv_i2sspdif->proc_entry,
			dev_name(conv_i2sspdif->device),
			snd_stm_conv_i2sspdif_dump_registers,
			conv_i2sspdif);

	/* Create ALSA controls */

	for (i = 0; i < ARRAY_SIZE(snd_stm_conv_i2sspdif_ctls); i++) {
		int result;

		snd_stm_conv_i2sspdif_ctls[i].device =
				snd_stm_conv_get_card_device(
				conv_i2sspdif->converter);
		snd_stm_conv_i2sspdif_ctls[i].index = conv_i2sspdif->index;
		result = snd_ctl_add(snd_stm_card_get(),
				snd_ctl_new1(&snd_stm_conv_i2sspdif_ctls[i],
				conv_i2sspdif));
		if (result < 0) {
			snd_stm_printe("Failed to add I2S-SPDIF converter "
					"ALSA control!\n");
			return result;
		}
	}

	return 0;
}

static int snd_stm_conv_i2sspdif_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_i2sspdif *conv_i2sspdif = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));
	BUG_ON(conv_i2sspdif->enabled);

	/* Remove procfs entry */

	snd_stm_info_unregister(conv_i2sspdif->proc_entry);

	/* Power done mode, just to be sure :-) */

	set__AUD_SPDIFPC_CFG(conv_i2sspdif,
		mask__AUD_SPDIFPC_CFG__DEVICE_EN__DISABLED(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__SW_RESET__RESET(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__FIFO_EN__DISABLED(conv_i2sspdif) |
		mask__AUD_SPDIFPC_CFG__REQ_ACK_EN__DISABLED(conv_i2sspdif));

	set__AUD_SPDIFPC_CTRL(conv_i2sspdif,
		mask__AUD_SPDIFPC_CTRL__OPERATION__OFF(conv_i2sspdif));

	return 0;
}

static struct snd_device_ops snd_stm_conv_i2sspdif_snd_device_ops = {
	.dev_register = snd_stm_conv_i2sspdif_register,
	.dev_disconnect = snd_stm_conv_i2sspdif_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_conv_i2sspdif_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_i2sspdif_info *conv_i2sspdif_info =
			pdev->dev.platform_data;
	struct snd_stm_conv_i2sspdif *conv_i2sspdif;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!conv_i2sspdif_info);

	conv_i2sspdif = kzalloc(sizeof(*conv_i2sspdif), GFP_KERNEL);
	if (!conv_i2sspdif) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_i2sspdif);
	conv_i2sspdif->ver = conv_i2sspdif_info->ver;
	BUG_ON(conv_i2sspdif->ver <= 0);
	conv_i2sspdif->info = conv_i2sspdif_info;
	conv_i2sspdif->device = &pdev->dev;
	spin_lock_init(&conv_i2sspdif->iec958_default_lock);

	/* Get resources */

	result = snd_stm_memory_request(pdev, &conv_i2sspdif->mem_region,
			&conv_i2sspdif->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Get connections */

	BUG_ON(!conv_i2sspdif_info->source_bus_id);
	snd_stm_printd(0, "This I2S-SPDIF converter is attached to PCM player"
			" '%s'.\n",
			conv_i2sspdif_info->source_bus_id);
	conv_i2sspdif->converter = snd_stm_conv_register_converter(
			"HDMI Output",
			&snd_stm_conv_i2sspdif_ops, conv_i2sspdif,
			&platform_bus_type, conv_i2sspdif_info->source_bus_id,
			conv_i2sspdif_info->channel_from,
			conv_i2sspdif_info->channel_to,
			&conv_i2sspdif->index);
	if (!conv_i2sspdif->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		result = -EINVAL;
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(snd_stm_card_get(), SNDRV_DEV_LOWLEVEL,
			conv_i2sspdif, &snd_stm_conv_i2sspdif_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, conv_i2sspdif);

	return result;

error_device:
error_attach:
	snd_stm_memory_release(conv_i2sspdif->mem_region,
			conv_i2sspdif->base);
error_memory_request:
	snd_stm_magic_clear(conv_i2sspdif);
	kfree(conv_i2sspdif);
error_alloc:
	return result;
}

static int snd_stm_conv_i2sspdif_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_i2sspdif *conv_i2sspdif =
			platform_get_drvdata(pdev);

	BUG_ON(!conv_i2sspdif);
	BUG_ON(!snd_stm_magic_valid(conv_i2sspdif));

	snd_stm_conv_unregister_converter(conv_i2sspdif->converter);
	snd_stm_memory_release(conv_i2sspdif->mem_region, conv_i2sspdif->base);

	snd_stm_magic_clear(conv_i2sspdif);
	kfree(conv_i2sspdif);

	return 0;
}

static struct platform_driver snd_stm_conv_i2sspdif_driver = {
	.driver.name = "snd_conv_i2sspdif",
	.probe = snd_stm_conv_i2sspdif_probe,
	.remove = snd_stm_conv_i2sspdif_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_i2sspdif_init(void)
{
	return platform_driver_register(&snd_stm_conv_i2sspdif_driver);
}

static void __exit snd_stm_conv_i2sspdif_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_i2sspdif_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics I2S to SPDIF converter driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_i2sspdif_init);
module_exit(snd_stm_conv_i2sspdif_exit);
