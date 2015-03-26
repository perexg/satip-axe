/*
 *   STMicroelectronics System-on-Chips' internal (sysconf controlled)
 *   audio DAC driver
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
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/stm/sysconf.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Hardware-related definitions
 */

#define FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS)
#define OVERSAMPLING 256



/*
 * Internal DAC instance structure
 */

struct snd_stm_conv_dac_sc {
	/* System informations */
	struct snd_stm_conv_converter *converter;
	const char *bus_id;

	/* Control bits */
	struct sysconf_field *nrst;
	struct sysconf_field *mode;
	struct sysconf_field *nsb;
	struct sysconf_field *softmute;
	struct sysconf_field *pdana;
	struct sysconf_field *pndbg;

	snd_stm_magic_field;
};



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_dac_sc_get_format(void *priv)
{
	snd_stm_printd(1, "snd_stm_conv_dac_sc_get_format(priv=%p)\n", priv);

	return FORMAT;
}

static int snd_stm_conv_dac_sc_get_oversampling(void *priv)
{
	snd_stm_printd(1, "snd_stm_conv_dac_sc_get_oversampling(priv=%p)\n",
			priv);

	return OVERSAMPLING;
}

static int snd_stm_conv_dac_sc_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc = priv;

	snd_stm_printd(1, "snd_stm_conv_dac_sc_set_enabled(enabled=%d, "
			"priv=%p)\n", enabled, priv);

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	snd_stm_printd(1, "%sabling DAC %s's digital part.\n",
			enabled ? "En" : "Dis", conv_dac_sc->bus_id);

	if (enabled) {
		sysconf_write(conv_dac_sc->nsb, 1); /* NORMAL */
		sysconf_write(conv_dac_sc->nrst, 1); /* NORMAL */
	} else {
		sysconf_write(conv_dac_sc->nrst, 0); /* RESET */
		sysconf_write(conv_dac_sc->nsb, 0); /* POWER_DOWN */
	}

	return 0;
}

static int snd_stm_conv_dac_sc_set_muted(int muted, void *priv)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc = priv;

	snd_stm_printd(1, "snd_stm_conv_dac_sc_set_muted(muted=%d, priv=%p)\n",
		       muted, priv);

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	snd_stm_printd(1, "%suting DAC %s.\n", muted ? "M" : "Unm",
			conv_dac_sc->bus_id);

	if (muted)
		sysconf_write(conv_dac_sc->softmute, 1); /* MUTE */
	else
		sysconf_write(conv_dac_sc->softmute, 0); /* NORMAL */

	return 0;
}

static struct snd_stm_conv_ops snd_stm_conv_dac_sc_ops = {
	.get_format = snd_stm_conv_dac_sc_get_format,
	.get_oversampling = snd_stm_conv_dac_sc_get_oversampling,
	.set_enabled = snd_stm_conv_dac_sc_set_enabled,
	.set_muted = snd_stm_conv_dac_sc_set_muted,
};



/*
 * ALSA lowlevel device implementation
 */

static int snd_stm_conv_dac_sc_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc =
			snd_device->device_data;

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	/* Initialize DAC with digital part down, analog up and muted */

	sysconf_write(conv_dac_sc->nrst, 0); /* RESET */
	sysconf_write(conv_dac_sc->mode, 0); /* DEFAULT */
	sysconf_write(conv_dac_sc->nsb, 0); /* POWER_DOWN */
	sysconf_write(conv_dac_sc->softmute, 1); /* MUTE */
	sysconf_write(conv_dac_sc->pdana, 1); /* NORMAL */
	sysconf_write(conv_dac_sc->pndbg, 1); /* NORMAL */

	return 0;
}

static int __exit snd_stm_conv_dac_sc_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc =
			snd_device->device_data;

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	/* Global power done & mute mode */

	sysconf_write(conv_dac_sc->nrst, 0); /* RESET */
	sysconf_write(conv_dac_sc->mode, 0); /* DEFAULT */
	sysconf_write(conv_dac_sc->nsb, 0); /* POWER_DOWN */
	sysconf_write(conv_dac_sc->softmute, 1); /* MUTE */
	sysconf_write(conv_dac_sc->pdana, 0); /* POWER_DOWN */
	sysconf_write(conv_dac_sc->pndbg, 0); /* POWER_DOWN */

	return 0;
}

static struct snd_device_ops snd_stm_conv_dac_sc_snd_device_ops = {
	.dev_register = snd_stm_conv_dac_sc_register,
	.dev_disconnect = snd_stm_conv_dac_sc_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_conv_dac_sc_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_dac_sc_info *info =
			pdev->dev.platform_data;
	struct snd_stm_conv_dac_sc *conv_dac_sc;
	struct snd_card *card = snd_stm_card_get();

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);
	BUG_ON(!info);

	conv_dac_sc = kzalloc(sizeof(*conv_dac_sc), GFP_KERNEL);
	if (!conv_dac_sc) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_dac_sc);
	conv_dac_sc->bus_id = dev_name(&pdev->dev);

	/* Get resources */

	conv_dac_sc->nrst = sysconf_claim(info->nrst.group, info->nrst.num,
			info->nrst.lsb, info->nrst.msb, "NRST");
	BUG_ON(!conv_dac_sc->nrst);
	conv_dac_sc->mode = sysconf_claim(info->mode.group, info->mode.num,
			info->mode.lsb, info->mode.msb, "MODE");
	BUG_ON(!conv_dac_sc->mode);
	conv_dac_sc->nsb = sysconf_claim(info->nsb.group, info->nsb.num,
			info->nsb.lsb, info->nsb.msb, "NSB");
	BUG_ON(!conv_dac_sc->nsb);
	conv_dac_sc->softmute = sysconf_claim(info->softmute.group,
			info->softmute.num, info->softmute.lsb,
			info->softmute.msb, "SOFTMUTE");
	BUG_ON(!conv_dac_sc->softmute);
	conv_dac_sc->pdana = sysconf_claim(info->pdana.group, info->pdana.num,
			info->pdana.lsb, info->pdana.msb, "PDANA");
	BUG_ON(!conv_dac_sc->pdana);
	conv_dac_sc->pndbg = sysconf_claim(info->pndbg.group, info->pndbg.num,
			info->pndbg.lsb, info->pndbg.msb, "PNDBG");
	BUG_ON(!conv_dac_sc->pndbg);

	/* Get connections */

	BUG_ON(!info->source_bus_id);
	snd_stm_printd(0, "This DAC is attached to PCM player '%s'.\n",
			info->source_bus_id);
	conv_dac_sc->converter = snd_stm_conv_register_converter(
			"Analog Output", &snd_stm_conv_dac_sc_ops, conv_dac_sc,
			&platform_bus_type, info->source_bus_id,
			info->channel_from, info->channel_to, NULL);
	if (!conv_dac_sc->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_dac_sc,
			&snd_stm_conv_dac_sc_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, conv_dac_sc);

	return 0;

error_device:
error_attach:
	snd_stm_magic_clear(conv_dac_sc);
	kfree(conv_dac_sc);
error_alloc:
	return result;
}

static int snd_stm_conv_dac_sc_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc = platform_get_drvdata(pdev);

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	snd_stm_conv_unregister_converter(conv_dac_sc->converter);

	sysconf_release(conv_dac_sc->nrst);
	sysconf_release(conv_dac_sc->mode);
	sysconf_release(conv_dac_sc->nsb);
	sysconf_release(conv_dac_sc->softmute);
	sysconf_release(conv_dac_sc->pdana);
	sysconf_release(conv_dac_sc->pndbg);

	snd_stm_magic_clear(conv_dac_sc);
	kfree(conv_dac_sc);

	return 0;
}

static struct platform_driver snd_stm_conv_dac_sc_driver = {
	.driver.name = "snd_conv_dac_sc",
	.probe = snd_stm_conv_dac_sc_probe,
	.remove = snd_stm_conv_dac_sc_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_dac_sc_init(void)
{
	return platform_driver_register(&snd_stm_conv_dac_sc_driver);
}

static void __exit snd_stm_conv_dac_sc_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_dac_sc_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sysconf-based audio DAC driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_dac_sc_init);
module_exit(snd_stm_conv_dac_sc_exit);
