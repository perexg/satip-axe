/*
 * linux/drivers/leds/leds-mb628.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/io.h>
#include <mach/epld.h>
#include <mach/common.h>

static void mb628_led_set(struct led_classdev *led_cdev,
			  enum led_brightness brightness)
{
	u8 reg;

	/* Locking required here */
	reg = epld_read(EPLD_ENABLE);
	if (brightness)
		reg |= EPLD_ENABLE_HBEAT;
	else
		reg &= ~EPLD_ENABLE_HBEAT;
	epld_write(reg, EPLD_ENABLE);
}

static struct led_classdev mb628_led = {
	.name = "mb628-led",
	.brightness_set = mb628_led_set,
	.default_trigger = "heartbeat"
};

static int __init mb628_led_init(void)
{
	return led_classdev_register(NULL, &mb628_led);
}

static void __exit mb628_led_exit(void)
{
	led_classdev_unregister(&mb628_led);
}

module_init(mb628_led_init);
module_exit(mb628_led_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED support for STMicroelectronics mb628");
MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
