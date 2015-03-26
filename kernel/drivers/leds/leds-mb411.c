/*
 * linux/drivers/leds/leds-mb411.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
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
#include <asm/io.h>
#include <asm/mb411/epld.h>

static void mb411_led_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	ctrl_outl(EPLD_LED, brightness ? EPLD_LED_ON : EPLD_LED_OFF);
}

static struct led_classdev mb411_led = {
	.name = "mb411-led",
	.brightness_set = mb411_led_set,
	.default_trigger = "heartbeat";
};

static int __init mb411_led_init(void)
{
	led_classdev_register(NULL, &mb411_led);
}

static void __exit mb411_led_exit(void)
{
	led_classdev_unregister(&mb411_led);
}

module_init(mb411_led_init);
module_exit(mb411_led_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED support for STMicroelectronics mb411");
MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
