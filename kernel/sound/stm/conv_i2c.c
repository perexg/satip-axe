/*
 *   STMicroelectronics System-on-Chips' I2C-controlled ADC/DAC driver
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
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Internal converter instance structure
 */

struct snd_stm_conv_i2c {
	/* System informations */
	struct i2c_client *client;
	struct snd_stm_conv_converter *converter;
	struct snd_stm_conv_i2c_info *info;
	struct snd_stm_conv_ops ops;

	/* Runtime data */
	struct work_struct work;
	int work_enable_value;
	int work_mute_value;
	spinlock_t work_lock; /* Protects work_*_value */

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Implementation
 */

static void snd_stm_conv_i2c_work(struct work_struct *work)
{
	struct snd_stm_conv_i2c *conv_i2c = container_of(work,
			struct snd_stm_conv_i2c, work);
	int enable_value, mute_value;
	const char *cmd;
	int cmd_len;

	snd_stm_printd(1, "snd_stm_conv_i2c_work(work=%p)\n", work);

	BUG_ON(!conv_i2c);
	BUG_ON(!snd_stm_magic_valid(conv_i2c));

	spin_lock(&conv_i2c->work_lock);

	enable_value = conv_i2c->work_enable_value;
	conv_i2c->work_enable_value = -1;

	mute_value = conv_i2c->work_mute_value;
	conv_i2c->work_mute_value = -1;

	spin_unlock(&conv_i2c->work_lock);

	cmd = NULL;
	if (enable_value == 1) {
		cmd = conv_i2c->info->enable_cmd;
		cmd_len = conv_i2c->info->enable_cmd_len;
	} else if (enable_value == 0) {
		cmd = conv_i2c->info->disable_cmd;
		cmd_len = conv_i2c->info->disable_cmd_len;
	}
	if (cmd) {
		int result = i2c_master_send(conv_i2c->client, cmd, cmd_len);

		if (result != cmd_len)
			snd_stm_printe("WARNING! Failed to %sable I2C converter"
					" '%s'! (%d)\n",
					enable_value ? "en" : "dis",
					dev_name(&conv_i2c->client->dev),
					result);
	}

	cmd = NULL;
	if (mute_value == 1) {
		cmd = conv_i2c->info->mute_cmd;
		cmd_len = conv_i2c->info->mute_cmd_len;
	} else if (mute_value == 0) {
		cmd = conv_i2c->info->unmute_cmd;
		cmd_len = conv_i2c->info->unmute_cmd_len;
	}
	if (cmd) {
		int result = i2c_master_send(conv_i2c->client, cmd, cmd_len);

		if (result != cmd_len)
			snd_stm_printe("WARNING! Failed to %smute I2C converter"
					" '%s'! (%d)\n", mute_value ? "" : "un",
					dev_name(&conv_i2c->client->dev),
					result);
	}
}


static unsigned int snd_stm_conv_i2c_get_format(void *priv)
{
	struct snd_stm_conv_i2c *conv_i2c = priv;

	snd_stm_printd(1, "snd_stm_conv_i2c_get_format(priv=%p)\n", priv);

	BUG_ON(!conv_i2c);
	BUG_ON(!snd_stm_magic_valid(conv_i2c));

	return conv_i2c->info->format;
}

static int snd_stm_conv_i2c_get_oversampling(void *priv)
{
	struct snd_stm_conv_i2c *conv_i2c = priv;

	snd_stm_printd(1, "snd_stm_conv_i2c_get_oversampling(priv=%p)\n",
			priv);

	BUG_ON(!conv_i2c);
	BUG_ON(!snd_stm_magic_valid(conv_i2c));

	return conv_i2c->info->oversampling;
}

static int snd_stm_conv_i2c_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_i2c *conv_i2c = priv;

	snd_stm_printd(1, "snd_stm_conv_i2c_enable(enabled=%d, priv=%p)\n",
			enabled, priv);

	BUG_ON(!conv_i2c);
	BUG_ON(!snd_stm_magic_valid(conv_i2c));
	BUG_ON(!conv_i2c->info->enable_supported);

	snd_stm_printd(1, "%sabling DAC %s's.\n", enabled ? "En" : "Dis",
			dev_name(&conv_i2c->client->dev));

	spin_lock(&conv_i2c->work_lock);
	conv_i2c->work_enable_value = enabled;
	schedule_work(&conv_i2c->work);
	spin_unlock(&conv_i2c->work_lock);

	return 0;
}

static int snd_stm_conv_i2c_set_muted(int muted, void *priv)
{
	struct snd_stm_conv_i2c *conv_i2c = priv;

	snd_stm_printd(1, "snd_stm_conv_i2c_set_muted(muted=%d, priv=%p)\n",
			muted, priv);

	BUG_ON(!conv_i2c);
	BUG_ON(!snd_stm_magic_valid(conv_i2c));
	BUG_ON(!conv_i2c->info->mute_supported);

	snd_stm_printd(1, "%suting DAC %s.\n", muted ? "M" : "Unm",
			dev_name(&conv_i2c->client->dev));

	spin_lock(&conv_i2c->work_lock);
	conv_i2c->work_mute_value = muted;
	schedule_work(&conv_i2c->work);
	spin_unlock(&conv_i2c->work_lock);

	return 0;
}



/*
 * I2C driver routines
 */

static int snd_stm_conv_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int result = 0;
	struct snd_stm_conv_i2c *conv_i2c;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&client->dev));

	BUG_ON(!client->dev.platform_data);

	conv_i2c = kzalloc(sizeof(*conv_i2c), GFP_KERNEL);
	if (!conv_i2c) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_i2c);
	conv_i2c->client = client;
	conv_i2c->info = client->dev.platform_data;

	conv_i2c->ops.get_format = snd_stm_conv_i2c_get_format;
	conv_i2c->ops.get_oversampling = snd_stm_conv_i2c_get_oversampling;
	if (conv_i2c->info->enable_supported)
		conv_i2c->ops.set_enabled = snd_stm_conv_i2c_set_enabled;
	if (conv_i2c->info->mute_supported)
		conv_i2c->ops.set_muted = snd_stm_conv_i2c_set_muted;

	/* Call the user-provided init function */

	if (conv_i2c->info->init) {
		result = conv_i2c->info->init(client, conv_i2c->info->priv);
		if (result != 0) {
			snd_stm_printe("User's init function failed for I2C "
					"converter %s! (%d)\n",
					dev_name(&conv_i2c->client->dev),
					result);
			goto error_init;
		}
	}

	/* Get connections */

	BUG_ON(!conv_i2c->info->source_bus_id);
	snd_stm_printd(0, "This converter is attached to '%s'.\n",
			conv_i2c->info->source_bus_id);
	conv_i2c->converter = snd_stm_conv_register_converter(
			conv_i2c->info->group, &conv_i2c->ops, conv_i2c,
			&platform_bus_type, conv_i2c->info->source_bus_id,
			conv_i2c->info->channel_from,
			conv_i2c->info->channel_to, NULL);
	if (!conv_i2c->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		result = -EINVAL;
		goto error_attach;
	}

	/* Initialize the converter */

	INIT_WORK(&conv_i2c->work, snd_stm_conv_i2c_work);
	spin_lock_init(&conv_i2c->work_lock);
	conv_i2c->work_enable_value = -1;
	conv_i2c->work_mute_value = -1;

	if (conv_i2c->info->enable_supported) {
		result = i2c_master_send(client, conv_i2c->info->disable_cmd,
				conv_i2c->info->disable_cmd_len);
		if (result != conv_i2c->info->disable_cmd_len) {
			snd_stm_printe("Failed to disable I2C converter '%s'!"
					" (%d)\n",
					dev_name(&conv_i2c->client->dev),
					result);
			goto error_set_enabled;
		}
	}

	if (conv_i2c->info->mute_supported) {
		result = i2c_master_send(client, conv_i2c->info->mute_cmd,
				conv_i2c->info->mute_cmd_len);
		if (result != conv_i2c->info->mute_cmd_len) {
			snd_stm_printe("Failed to mute I2C converter '%s'!"
					" (%d)\n",
					dev_name(&conv_i2c->client->dev),
					result);
			goto error_set_muted;
		}
	}

	/* Done now */

	i2c_set_clientdata(client, conv_i2c);

	return 0;

error_set_muted:
error_set_enabled:
	snd_stm_conv_unregister_converter(conv_i2c->converter);
error_attach:
error_init:
	snd_stm_magic_clear(conv_i2c);
	kfree(conv_i2c);
error_alloc:
	return result;
}

static int snd_stm_conv_i2c_remove(struct i2c_client *client)
{
	struct snd_stm_conv_i2c *conv_i2c = i2c_get_clientdata(client);

	BUG_ON(!conv_i2c);
	BUG_ON(!snd_stm_magic_valid(conv_i2c));

	snd_stm_conv_unregister_converter(conv_i2c->converter);

	/* Wait for the possibly scheduled work... */

	flush_scheduled_work();

	/* Muting and disabling - just to be sure ;-) */

	if (conv_i2c->info->mute_supported)
		i2c_master_send(client, conv_i2c->info->mute_cmd,
				conv_i2c->info->mute_cmd_len);
	if (conv_i2c->info->enable_supported)
		i2c_master_send(client, conv_i2c->info->disable_cmd,
				conv_i2c->info->disable_cmd_len);

	snd_stm_magic_clear(conv_i2c);
	kfree(conv_i2c);

	return 0;
}

static struct i2c_driver snd_stm_conv_i2c_driver = {
	.driver.name = "snd_conv_i2c",
	.probe = snd_stm_conv_i2c_probe,
	.remove = snd_stm_conv_i2c_remove,
};


/*
 * Initialization
 */

static int __init snd_stm_conv_i2c_init(void)
{
	return i2c_add_driver(&snd_stm_conv_i2c_driver);
}

static void __exit snd_stm_conv_i2c_exit(void)
{
	i2c_del_driver(&snd_stm_conv_i2c_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics I2C-controlled audio converter driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_i2c_init);
module_exit(snd_stm_conv_i2c_exit);
