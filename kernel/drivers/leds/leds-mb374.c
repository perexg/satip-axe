/*
 * linux/drivers/leds/leds-mb374.c
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
#include <linux/stm/pio.h>
#include <asm/io.h>
#include <asm/mb374/epld.h>

struct mb374_led {
	struct led_classdev	cdev;
	union {
		struct stpio_pin *pio;
		int bit;
	} u;
};

static void mb374_led_pio_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct mb374_led *led_dev =
		container_of(led_cdev, struct mb374_led, cdev);
	stpio_set_pin(led_dev->u.pio, brightness);
}

static void mb374_led_epld_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct mb374_led *led_dev =
		container_of(led_cdev, struct mb374_led, cdev);
	ctrl_outl(1 << led_dev->u.bit, brightness ? EPLD_LED_SET : EPLD_LED_CLR);
}

#define MB374_EPLD_LED(_n, _bit)				\
	{							\
		.cdev = {					\
			.name = "mb374-led" #_n,		\
			.brightness_set = mb374_led_epld_set,	\
		},						\
		.u.bit = _bit					\
	}
#define MB374_PIO_LED(_n)					\
	{							\
		.cdev = {					\
			.name = "mb374-led" #_n,		\
			.brightness_set = mb374_led_pio_set,	\
		},						\
	}

static struct mb374_led mb374_leds[8] = {
	MB374_EPLD_LED(0,0),
	MB374_EPLD_LED(1,1),
	MB374_EPLD_LED(2,2),
	MB374_EPLD_LED(3,3),
	MB374_EPLD_LED(4,4),
	MB374_EPLD_LED(5,5),
	MB374_PIO_LED(6),
	MB374_PIO_LED(7)
};

static int __init mb374_led_init(void)
{
	int i;

	mb374_leds[6].u.pio = stpio_request_pin(0,3, "LED", STPIO_OUT);
	mb374_leds[7].u.pio = stpio_request_pin(0,0, "LED", STPIO_OUT);

	mb374_leds[0].cdev.default_trigger = "heartbeat";

	for (i=0; i<8; i++) {
		led_classdev_register(NULL, &mb374_leds[i].cdev);
	}
}

static void __exit mb374_led_exit(void)
{
	int i;

	for (i=0; i<8; i++) {
		led_classdev_unregister(&mb374_leds[i].cdev);
	}
}

module_init(mb374_led_init);
module_exit(mb374_led_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Front LED support for Mb374 Server");
MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
