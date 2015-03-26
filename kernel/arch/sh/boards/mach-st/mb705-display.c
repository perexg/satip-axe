/*
 * arch/sh/boards/st/common/mb705-display.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Driver for the two HDSP-253x display modules on the mb705.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/processor.h>
#include <mach/common.h>
#include "mb705-epld.h"

/* HDSP-253x display register offsets */
/* All offsets must be shifted by 1 to allow for shifted address lines. */
#define UDC_ADDRESS_OFF	(0<<4)
#define UDC_RAM_OFF	(1<<4)
#define CHAR_RAM_OFF	(3<<4)

struct display_data {
	spinlock_t lock;
};

static struct device_attribute dev_attr_udef[];

static ssize_t store_text(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t count)
{
	struct display_data *dd = dev_get_drvdata(dev);
	int c;

	spin_lock(&dd->lock);

	for (c=0; c<16; c++) {
		int base = ((c<8) ? EPLD_TS_DISPLAY1_BASE : EPLD_TS_DISPLAY0_BASE) + CHAR_RAM_OFF;
		int off = c & 7;
		int data = (c<count) ? buf[c] : ' ';
		epld_write(data, base + (off << 1));
	}

	spin_unlock(&dd->lock);

	return count;
}

static DEVICE_ATTR(text, S_IWUSR, NULL, store_text);

static ssize_t store_udef(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t count)
{
	struct display_data *dd = dev_get_drvdata(dev);
	int code = attr - dev_attr_udef;
	int display;

	spin_lock(&dd->lock);

	for (display=0; display < 2; display++) {
		int base = (display == 0) ? EPLD_TS_DISPLAY0_BASE : EPLD_TS_DISPLAY1_BASE;
		int i;

		epld_write(code, base + UDC_ADDRESS_OFF);
		for (i=0; (i < 7) && (i < count); i++)
			epld_write(buf[i], base + UDC_RAM_OFF + (i << 1));
	}

	spin_unlock(&dd->lock);

	return count;
}

static struct device_attribute dev_attr_udef[16] = {
	__ATTR(udef0, S_IWUSR, NULL, store_udef),
	__ATTR(udef1, S_IWUSR, NULL, store_udef),
	__ATTR(udef2, S_IWUSR, NULL, store_udef),
	__ATTR(udef3, S_IWUSR, NULL, store_udef),
	__ATTR(udef4, S_IWUSR, NULL, store_udef),
	__ATTR(udef5, S_IWUSR, NULL, store_udef),
	__ATTR(udef6, S_IWUSR, NULL, store_udef),
	__ATTR(udef7, S_IWUSR, NULL, store_udef),
	__ATTR(udef8, S_IWUSR, NULL, store_udef),
	__ATTR(udef9, S_IWUSR, NULL, store_udef),
	__ATTR(udef10, S_IWUSR, NULL, store_udef),
	__ATTR(udef11, S_IWUSR, NULL, store_udef),
	__ATTR(udef12, S_IWUSR, NULL, store_udef),
	__ATTR(udef13, S_IWUSR, NULL, store_udef),
	__ATTR(udef14, S_IWUSR, NULL, store_udef),
	__ATTR(udef15, S_IWUSR, NULL, store_udef)
};

static int __init mb705_display_probe(struct platform_device *pdev)
{
        struct device *dev = &pdev->dev;
	int res;
	struct display_data *dd;
	int i;
	char string[17];
	int string_len;

	dd = devm_kzalloc(dev, sizeof(*dd), GFP_KERNEL);
	if (dd == NULL)
		return -ENOMEM;

	spin_lock_init(&dd->lock);

	platform_set_drvdata(pdev, dd);

	res = device_create_file(dev, &dev_attr_text);
	if (res)
		return res;

	for (i=0; i<16; i++) {
		if (device_create_file(dev, &dev_attr_udef[i])) {
			dev_warn(dev, "%s: failed to create udef sysfs entry\n",
				 __func__);
		}
	}

	/* notFL signal is controlled by DisplayCtrlReg[0] */
	epld_write(1, EPLD_TS_DISPLAY_CTRL_REG);

	string_len = scnprintf(string, sizeof(string), "MB705%c", mb705_rev);
	store_text(dev, NULL, string, string_len);

	return 0;
}

static int __exit mb705_display_remove(struct platform_device *pdev)
{
	int i;

	device_remove_file(&pdev->dev, &dev_attr_text);

	for (i=0; i<4; i++) {
		device_remove_file(&pdev->dev, &dev_attr_udef[i]);
	}

        return 0;
}

static struct platform_driver mb705_display_driver = {
	.remove		= __exit_p(mb705_display_remove),
	.driver		= {
		.name	= "mb705-display",
		.owner	= THIS_MODULE,
	},
};

static int __init mb705_display_init(void)
{
	return platform_driver_probe(&mb705_display_driver,
				     mb705_display_probe);
}

static void __exit mb705_display_exit(void)
{
	platform_driver_unregister(&mb705_display_driver);
}

module_init(mb705_display_init);
module_exit(mb705_display_exit);
