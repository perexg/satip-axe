/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2010  STMicroelectronics Limited
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ------------------------------------------------------------------------- */

#include <linux/device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

/*
 * The following functions will give the minimal
 * support to use the PM_RUNTIME support on
 * the platform_bus bus
 */

int platform_pm_runtime_suspend(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;
	int ret;

	ret = pm && pm->runtime_suspend ? pm->runtime_suspend(dev) : -EINVAL;

	return ret;
}

int platform_pm_runtime_resume(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;
	int ret;

	ret = pm && pm->runtime_resume ? pm->runtime_resume(dev) : -EINVAL;

	return ret;
}

int platform_pm_runtime_idle(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;
	int ret;

	if (pm && pm->runtime_idle) {
		ret = pm->runtime_idle(dev);
		if (ret)
			return ret;
	}

	return 0;
}
