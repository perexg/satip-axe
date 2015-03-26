/*
 *   STMicroelectronics System-on-Chips' GPIO-controlled ADC/DAC driver
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
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Internal DAC instance structure
 */

struct snd_stm_conv_gpio {
	/* System informations */
	const char *dev_name;
	struct snd_stm_conv_converter *converter;
	struct snd_stm_conv_gpio_info *info;
	struct snd_stm_conv_ops ops;

	/* Runtime data */
	int may_sleep;
	struct work_struct work; /* Used if may_sleep */
	int work_enable_value;
	int work_mute_value;
	spinlock_t work_lock; /* Protects work_*_value */

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Sleeping-safe GPIO access implementation
 */

static void snd_stm_conv_gpio_work(struct work_struct *work)
{
	struct snd_stm_conv_gpio *conv_gpio = container_of(work,
			struct snd_stm_conv_gpio, work);
	int enable_value, mute_value;

	snd_stm_printd(1, "snd_stm_conv_gpio_work(work=%p)\n", work);

	BUG_ON(!conv_gpio);
	BUG_ON(!snd_stm_magic_valid(conv_gpio));

	spin_lock(&conv_gpio->work_lock);

	enable_value = conv_gpio->work_enable_value;
	conv_gpio->work_enable_value = -1;

	mute_value = conv_gpio->work_mute_value;
	conv_gpio->work_mute_value = -1;

	spin_unlock(&conv_gpio->work_lock);

	if (enable_value != -1)
		gpio_set_value_cansleep(conv_gpio->info->enable_gpio,
				enable_value);

	if (mute_value != -1)
		gpio_set_value_cansleep(conv_gpio->info->mute_gpio,
				mute_value);
}

static void snd_stm_conv_gpio_set_value(struct snd_stm_conv_gpio *conv_gpio,
		int enable_not_mute, int value)
{
	snd_stm_printd(1, "snd_stm_conv_gpio_set_value(conv_gpio=%p, "
			"enable_not_mute=%d, value=%d)\n",
			conv_gpio, enable_not_mute, value);

	BUG_ON(!conv_gpio);
	BUG_ON(!snd_stm_magic_valid(conv_gpio));

	if (conv_gpio->may_sleep) {
		spin_lock(&conv_gpio->work_lock);
		if (enable_not_mute)
			conv_gpio->work_enable_value = value;
		else
			conv_gpio->work_mute_value = value;
		schedule_work(&conv_gpio->work);
		spin_unlock(&conv_gpio->work_lock);
	} else {
		gpio_set_value(enable_not_mute ? conv_gpio->info->enable_gpio :
				conv_gpio->info->mute_gpio, value);
	}
}



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_gpio_get_format(void *priv)
{
	struct snd_stm_conv_gpio *conv_gpio = priv;

	snd_stm_printd(1, "snd_stm_conv_gpio_get_format(priv=%p)\n", priv);

	BUG_ON(!conv_gpio);
	BUG_ON(!snd_stm_magic_valid(conv_gpio));

	return conv_gpio->info->format;
}

static int snd_stm_conv_gpio_get_oversampling(void *priv)
{
	struct snd_stm_conv_gpio *conv_gpio = priv;

	snd_stm_printd(1, "snd_stm_conv_gpio_get_oversampling(priv=%p)\n",
			priv);

	BUG_ON(!conv_gpio);
	BUG_ON(!snd_stm_magic_valid(conv_gpio));

	return conv_gpio->info->oversampling;
}

static int snd_stm_conv_gpio_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_gpio *conv_gpio = priv;

	snd_stm_printd(1, "snd_stm_conv_gpio_enable(enabled=%d, priv=%p)\n",
			enabled, priv);

	BUG_ON(!conv_gpio);
	BUG_ON(!snd_stm_magic_valid(conv_gpio));
	BUG_ON(!conv_gpio->info->enable_supported);

	snd_stm_printd(1, "%sabling DAC %s's.\n", enabled ? "En" : "Dis",
			conv_gpio->dev_name);

	snd_stm_conv_gpio_set_value(conv_gpio, 1,
			enabled ? conv_gpio->info->enable_value :
			!conv_gpio->info->enable_value);

	return 0;
}

static int snd_stm_conv_gpio_set_muted(int muted, void *priv)
{
	struct snd_stm_conv_gpio *conv_gpio = priv;

	snd_stm_printd(1, "snd_stm_conv_gpio_set_muted(muted=%d, priv=%p)\n",
			muted, priv);

	BUG_ON(!conv_gpio);
	BUG_ON(!snd_stm_magic_valid(conv_gpio));
	BUG_ON(!conv_gpio->info->mute_supported);

	snd_stm_printd(1, "%suting DAC %s.\n", muted ? "M" : "Unm",
			conv_gpio->dev_name);

	snd_stm_conv_gpio_set_value(conv_gpio, 0,
			muted ? conv_gpio->info->mute_value :
			!conv_gpio->info->mute_value);

	return 0;
}



/*
 * Procfs status callback
 */

static void snd_stm_conv_gpio_read_info(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_gpio *conv_gpio = entry->private_data;

	BUG_ON(!conv_gpio);
	BUG_ON(!snd_stm_magic_valid(conv_gpio));

	snd_iprintf(buffer, "--- %s ---\n", conv_gpio->dev_name);

	snd_iprintf(buffer, "enable_gpio(%d) = %d\n",
			conv_gpio->info->enable_gpio,
			gpio_get_value(conv_gpio->info->enable_gpio));
	if (conv_gpio->info->mute_supported)
		snd_iprintf(buffer, "mute_gpio(%d) = %d\n",
				conv_gpio->info->mute_gpio,
				gpio_get_value(conv_gpio->info->mute_gpio));

	snd_iprintf(buffer, "\n");
}



/*
 * Platform driver routines
 */

static int snd_stm_conv_gpio_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_gpio *conv_gpio;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!pdev->dev.platform_data);

	conv_gpio = kzalloc(sizeof(*conv_gpio), GFP_KERNEL);
	if (!conv_gpio) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_gpio);
	conv_gpio->dev_name = dev_name(&pdev->dev);
	conv_gpio->info = pdev->dev.platform_data;

	conv_gpio->ops.get_format = snd_stm_conv_gpio_get_format;
	conv_gpio->ops.get_oversampling = snd_stm_conv_gpio_get_oversampling;
	if (conv_gpio->info->enable_supported)
		conv_gpio->ops.set_enabled = snd_stm_conv_gpio_set_enabled;
	if (conv_gpio->info->mute_supported)
		conv_gpio->ops.set_muted = snd_stm_conv_gpio_set_muted;

	/* Get connections */

	BUG_ON(!conv_gpio->info->source_bus_id);
	snd_stm_printd(0, "This DAC is attached to PCM player '%s'.\n",
			conv_gpio->info->source_bus_id);
	conv_gpio->converter = snd_stm_conv_register_converter(
			conv_gpio->info->group, &conv_gpio->ops, conv_gpio,
			&platform_bus_type, conv_gpio->info->source_bus_id,
			conv_gpio->info->channel_from,
			conv_gpio->info->channel_to, NULL);
	if (!conv_gpio->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		result = -EINVAL;
		goto error_attach;
	}

	/* Reserve & initialize GPIO lines (enabled & mute) */

	if (conv_gpio->info->enable_supported) {
		result = gpio_request(conv_gpio->info->enable_gpio,
				conv_gpio->dev_name);
		if (result != 0) {
			snd_stm_printe("Can't reserve 'enable' GPIO line!\n");
			goto error_gpio_request_enable;
		}

		if (gpio_direction_output(conv_gpio->info->enable_gpio,
				!conv_gpio->info->enable_value) != 0) {
			snd_stm_printe("Can't set 'enable' GPIO line as "
					"output!\n");
			goto error_gpio_direction_output_enable;
		}

		conv_gpio->may_sleep = gpio_cansleep(
				conv_gpio->info->enable_gpio);
	}

	if (conv_gpio->info->mute_supported) {
		result = gpio_request(conv_gpio->info->mute_gpio,
				conv_gpio->dev_name);
		if (result != 0) {
			snd_stm_printe("Can't reserve 'mute' GPIO line!\n");
			goto error_gpio_request_mute;
		}

		if (gpio_direction_output(conv_gpio->info->mute_gpio,
				conv_gpio->info->mute_value) != 0) {
			snd_stm_printe("Can't set 'mute' GPIO line as output!"
					"\n");
			goto error_gpio_direction_output_mute;
		}

		conv_gpio->may_sleep |= gpio_cansleep(
				conv_gpio->info->mute_gpio);
	}

	if (conv_gpio->may_sleep) {
		INIT_WORK(&conv_gpio->work, snd_stm_conv_gpio_work);
		spin_lock_init(&conv_gpio->work_lock);
		conv_gpio->work_enable_value = -1;
		conv_gpio->work_mute_value = -1;
	}

	/* Additional procfs info */

	snd_stm_info_register(&conv_gpio->proc_entry,
			conv_gpio->dev_name,
			snd_stm_conv_gpio_read_info,
			conv_gpio);

	/* Done now */

	platform_set_drvdata(pdev, conv_gpio);

	return 0;

error_gpio_direction_output_mute:
	if (conv_gpio->info->mute_supported)
		gpio_free(conv_gpio->info->mute_gpio);
error_gpio_request_mute:
error_gpio_direction_output_enable:
	if (conv_gpio->info->enable_supported)
		gpio_free(conv_gpio->info->enable_gpio);
error_gpio_request_enable:
	snd_stm_conv_unregister_converter(conv_gpio->converter);
error_attach:
	snd_stm_magic_clear(conv_gpio);
	kfree(conv_gpio);
error_alloc:
	return result;
}

static int snd_stm_conv_gpio_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_gpio *conv_gpio = platform_get_drvdata(pdev);

	BUG_ON(!conv_gpio);
	BUG_ON(!snd_stm_magic_valid(conv_gpio));

	snd_stm_conv_unregister_converter(conv_gpio->converter);

	/* Remove procfs entry */

	snd_stm_info_unregister(conv_gpio->proc_entry);

	/* Wait for the possibly scheduled work... */

	if (conv_gpio->may_sleep)
		flush_scheduled_work();

	/* Muting and disabling - just to be sure ;-) */

	if (conv_gpio->info->mute_supported) {
		gpio_set_value_cansleep(conv_gpio->info->mute_gpio,
				conv_gpio->info->mute_value);
		gpio_free(conv_gpio->info->mute_gpio);
	}

	if (conv_gpio->info->enable_supported) {
		gpio_set_value_cansleep(conv_gpio->info->enable_gpio,
				!conv_gpio->info->enable_value);
		gpio_free(conv_gpio->info->enable_gpio);
	}

	snd_stm_magic_clear(conv_gpio);
	kfree(conv_gpio);

	return 0;
}

static struct platform_driver snd_stm_conv_gpio_driver = {
	.driver.name = "snd_conv_gpio",
	.probe = snd_stm_conv_gpio_probe,
	.remove = snd_stm_conv_gpio_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_gpio_init(void)
{
	return platform_driver_register(&snd_stm_conv_gpio_driver);
}

static void __exit snd_stm_conv_gpio_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_gpio_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics GPIO-controlled audio converter driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_gpio_init);
module_exit(snd_stm_conv_gpio_exit);
