/*
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/thermal.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/device.h>

struct stm_temp_sensor {
	struct stm_device_state *device_state;
	struct thermal_zone_device *th_dev;
	struct plat_stm_temp_data *plat_data;

	struct sysconf_field *dcorrect;
	struct sysconf_field *overflow;
	struct sysconf_field *data;

	unsigned long (*custom_get_data)(void *priv);
};

static int stm_thermal_get_temp(struct thermal_zone_device *th,
		unsigned long *temperature)
{
	struct stm_temp_sensor *sensor =
		(struct stm_temp_sensor *)th->devdata;
	unsigned long data;
	int overflow;

	overflow = sysconf_read(sensor->overflow);

	if (sensor->plat_data->custom_get_data)
		data = sensor->plat_data->custom_get_data(
				sensor->plat_data->custom_priv);
	else
		data = sysconf_read(sensor->data);

	overflow |= sysconf_read(sensor->overflow);

	data = (data + 20) * 1000;

	*temperature = data;

	return overflow;
}

static int stm_thermal_set_mode(struct thermal_zone_device *th,
	enum thermal_device_mode val)
{
	struct stm_temp_sensor *sensor =
		(struct stm_temp_sensor *)th->devdata;

	if (val == THERMAL_DEVICE_DISABLED)
		stm_device_power(sensor->device_state, stm_device_power_off);
	else
		stm_device_power(sensor->device_state, stm_device_power_on);

	return 0;
}

static struct thermal_zone_device_ops stm_thermal_ops = {
	.get_temp = stm_thermal_get_temp,
	.set_mode = stm_thermal_set_mode,
};

static int __devinit stm_temp_probe(struct platform_device *pdev)
{
	struct stm_temp_sensor *sensor = platform_get_drvdata(pdev);
	struct plat_stm_temp_data *plat_data = pdev->dev.platform_data;
	const char *name = dev_name(&pdev->dev);
	int err;

	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		dev_err(&pdev->dev, "Out of memory!\n");
		err = -ENOMEM;
		goto error_kzalloc;
	}

	sensor->plat_data = plat_data;

	err = -EBUSY;

	sensor->device_state = devm_stm_device_init(&pdev->dev,
		plat_data->device_config);

	if (!sensor->device_state) {
		err = -EBUSY;
		goto error_device_init;
	}

	if (!plat_data->custom_set_dcorrect) {
		sensor->dcorrect = sysconf_claim(plat_data->dcorrect.group,
				plat_data->dcorrect.num,
				plat_data->dcorrect.lsb,
				plat_data->dcorrect.msb, name);
		if (!sensor->dcorrect) {
			dev_err(&pdev->dev, "Can't claim DCORRECT sysconf "
					"bits!\n");
			goto error_device_init;
		}
	}

	sensor->overflow = sysconf_claim(plat_data->overflow.group,
			plat_data->overflow.num, plat_data->overflow.lsb,
			plat_data->overflow.msb, name);
	if (!sensor->overflow) {
		dev_err(&pdev->dev, "Can't claim OVERFLOW sysconf bit!\n");
		goto error_overflow;
	}

	if (!plat_data->custom_get_data) {
		sensor->data = sysconf_claim(plat_data->data.group,
				plat_data->data.num, plat_data->data.lsb,
				plat_data->data.msb, name);
		if (!sensor->data) {
			dev_err(&pdev->dev, "Can't claim DATA sysconf bits!\n");
			goto error_data;
		}
	}

	if (plat_data->custom_set_dcorrect) {
		plat_data->custom_set_dcorrect(plat_data->custom_priv);
	} else {
		if (!plat_data->calibrated)
			plat_data->calibration_value = 16;

		sysconf_write(sensor->dcorrect, plat_data->calibration_value);
	}

	platform_set_drvdata(pdev, sensor);

	sensor->th_dev = thermal_zone_device_register(
		(char *)dev_name(&pdev->dev), 0, (void *)sensor,
		&stm_thermal_ops, 0, 0, 10000, 10000);

	if (IS_ERR(sensor->th_dev))
		goto error_class_dev;

	/* Initialize the pm_runtime fields */
	pm_runtime_set_active(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);

	return 0;

error_class_dev:
	if (sensor->data)
		sysconf_release(sensor->data);
error_data:
	sysconf_release(sensor->overflow);
error_overflow:
	if (sensor->dcorrect)
		sysconf_release(sensor->dcorrect);
error_device_init:
	kfree(sensor);
error_kzalloc:
	return err;
}

static int __devexit stm_temp_remove(struct platform_device *pdev)
{
	struct stm_temp_sensor *sensor = platform_get_drvdata(pdev);

	stm_device_power(sensor->device_state, stm_device_power_off);

	thermal_zone_device_unregister(sensor->th_dev);

	if (sensor->dcorrect)
		sysconf_release(sensor->dcorrect);
	sysconf_release(sensor->overflow);
	if (sensor->data)
		sysconf_release(sensor->data);

	kfree(sensor);

	return 0;
}

#ifdef CONFIG_PM
static int stm_temp_suspend(struct device *dev)
{
	struct stm_temp_sensor *sensor = dev_get_drvdata(dev);

#ifdef CONFIG_PM_RUNTIME
	if (dev->power.runtime_status != RPM_ACTIVE)
		return 0; /* sensor already suspended via runtime_suspend */
#endif

	stm_device_power(sensor->device_state, stm_device_power_off);
	return 0;
}

static int stm_temp_resume(struct device *dev)
{
	struct stm_temp_sensor *sensor = dev_get_drvdata(dev);

#ifdef CONFIG_PM_RUNTIME
	if (dev->power.runtime_status == RPM_SUSPENDED)
		return 0; /* usb wants resume via runtime_resume... */
#endif
	stm_device_power(sensor->device_state, stm_device_power_on);
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int stm_temp_runtime_suspend(struct device *dev)
{
	if (dev->power.runtime_status == RPM_SUSPENDED)
		return 0;

	stm_temp_suspend(dev);
	return 0;
}

static int stm_temp_runtime_resume(struct device *dev)
{
	if (dev->power.runtime_status == RPM_ACTIVE)
		return 0;

	stm_temp_resume(dev);
	return 0;
}
#else
#define stm_temp_runtime_suspend	NULL
#define stm_temp_runtime_resume		NULL
#endif

static struct dev_pm_ops stm_temp_pm = {
	.suspend = stm_temp_suspend,  /* on standby/memstandby */
	.resume = stm_temp_resume,    /* resume from standby/memstandby */
	.freeze = stm_temp_suspend,
	.restore = stm_temp_resume,
	.runtime_suspend = stm_temp_runtime_suspend,
	.runtime_resume = stm_temp_runtime_resume,
};
#else
static struct dev_pm_ops stm_temp_pm;
#endif

static struct platform_driver stm_temp_driver = {
	.driver = {
		.name	= "stm-temp",
		.pm = &stm_temp_pm,
	},
	.probe		= stm_temp_probe,
	.remove		= stm_temp_remove,
};

static int __init stm_temp_init(void)
{
	return platform_driver_register(&stm_temp_driver);
}

static void __exit stm_temp_exit(void)
{
	platform_driver_unregister(&stm_temp_driver);
}

module_init(stm_temp_init);
module_exit(stm_temp_exit);

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics SOC internal temperature sensor driver");
MODULE_LICENSE("GPL");
