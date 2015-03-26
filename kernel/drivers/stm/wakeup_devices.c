/*
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 */

#include <linux/stm/wakeup_devices.h>
#include <linux/device.h>
#include <linux/platform_device.h>

static void stm_wake_init(struct stm_wakeup_devices *wkd)
{
	wkd->lirc_can_wakeup = 0;
	wkd->hdmi_can_wakeup = 0;
	wkd->eth_phy_can_wakeup = 0;
	wkd->eth1_phy_can_wakeup = 0;
}

static int __check_wakeup_device(struct device *dev, void *data)
{
	struct stm_wakeup_devices *wkd = (struct stm_wakeup_devices *)data;

	if (device_may_wakeup(dev)) {
		pr_info("[STM][PM] -> device %s can wakeup\n", dev_name(dev));
		if (!strcmp(dev_name(dev), "lirc"))
			wkd->lirc_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "hdmi"))
			wkd->hdmi_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "stmmaceth"))
			wkd->eth_phy_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "stmmaceth.0"))
			wkd->eth_phy_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "stmmaceth.1"))
			wkd->eth1_phy_can_wakeup = 1;

	}
	return 0;
}

int stm_check_wakeup_devices(struct stm_wakeup_devices *wkd)
{
	stm_wake_init(wkd);
	bus_for_each_dev(&platform_bus_type, NULL, wkd, __check_wakeup_device);
	return 0;
}

EXPORT_SYMBOL(stm_check_wakeup_devices);
