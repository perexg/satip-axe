/*
 *   STMicroelectronics System-on-Chips' dummy DAC driver
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
#include <sound/stm.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Dummy converter instance structure
 */

struct snd_stm_conv_dummy {
	struct snd_stm_conv_converter *converter;
	struct snd_stm_conv_dummy_info *info;

	snd_stm_magic_field;
};



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_dummy_get_format(void *priv)
{
	struct snd_stm_conv_dummy *conv_dummy = priv;

	snd_stm_printd(1, "snd_stm_conv_dummy_get_format(priv=%p)\n", priv);

	BUG_ON(!conv_dummy);
	BUG_ON(!snd_stm_magic_valid(conv_dummy));

	return conv_dummy->info->format;
}

static int snd_stm_conv_dummy_get_oversampling(void *priv)
{
	struct snd_stm_conv_dummy *conv_dummy = priv;

	snd_stm_printd(1, "snd_stm_conv_dummy_get_oversampling(priv=%p)\n",
			priv);

	BUG_ON(!conv_dummy);
	BUG_ON(!snd_stm_magic_valid(conv_dummy));

	return conv_dummy->info->oversampling;
}

static struct snd_stm_conv_ops snd_stm_conv_dummy_ops = {
	.get_format = snd_stm_conv_dummy_get_format,
	.get_oversampling = snd_stm_conv_dummy_get_oversampling,
};



/*
 * Platform driver routines
 */

static int snd_stm_conv_dummy_probe(struct platform_device *pdev)
{
	struct snd_stm_conv_dummy *conv_dummy;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!pdev->dev.platform_data);

	conv_dummy = kzalloc(sizeof(*conv_dummy), GFP_KERNEL);
	if (!conv_dummy) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		return -ENOMEM;
	}
	snd_stm_magic_set(conv_dummy);
	conv_dummy->info = pdev->dev.platform_data;

	snd_stm_printd(0, "This dummy DAC is attached to PCM player '%s'.\n",
			conv_dummy->info->source_bus_id);
	conv_dummy->converter = snd_stm_conv_register_converter(
			conv_dummy->info->group,
			&snd_stm_conv_dummy_ops, conv_dummy,
			&platform_bus_type, conv_dummy->info->source_bus_id,
			conv_dummy->info->channel_from,
			conv_dummy->info->channel_to, NULL);
	if (!conv_dummy->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		return -EINVAL;
	}

	/* Done now */

	platform_set_drvdata(pdev, conv_dummy);

	return 0;
}

static int snd_stm_conv_dummy_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_dummy *conv_dummy = platform_get_drvdata(pdev);

	BUG_ON(!conv_dummy);
	BUG_ON(!snd_stm_magic_valid(conv_dummy));

	snd_stm_conv_unregister_converter(conv_dummy->converter);

	snd_stm_magic_clear(conv_dummy);
	kfree(conv_dummy);

	return 0;
}

static struct platform_driver snd_stm_conv_dummy_driver = {
	.driver.name = "snd_conv_dummy",
	.probe = snd_stm_conv_dummy_probe,
	.remove = snd_stm_conv_dummy_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_dummy_init(void)
{
	return platform_driver_register(&snd_stm_conv_dummy_driver);
}

static void __exit snd_stm_conv_dummy_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_dummy_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics dummy audio converter driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_dummy_init);
module_exit(snd_stm_conv_dummy_exit);
