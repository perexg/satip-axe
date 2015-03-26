/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_GPIO_H
#define __LINUX_STM_GPIO_H

#include <linux/io.h>
#include <linux/platform_device.h>
#include <asm-generic/gpio.h>



/* Value indicating wrong GPIO number (gpio numbers are sadly unsigned...) */
#define STM_GPIO_INVALID (~0)

/* STM SoCs organise GPIO lines into ports, 8 lines (pins) in each */
#define STM_GPIO_PINS_PER_PORT 8

/* The following macros help to get a gpio-space number
 * basing on the port/pin pair and vice-versa (assuming
 * that passed gpio number is really an internal one...) */
#define stm_gpio(port, pin) ((port) * STM_GPIO_PINS_PER_PORT + (pin))
#define stm_gpio_port(gpio) ((gpio) / STM_GPIO_PINS_PER_PORT)
#define stm_gpio_pin(gpio) ((gpio) % STM_GPIO_PINS_PER_PORT)



/* Here are the generic GPIO access functions, optimised for internal
 * PIOs and using the gpiolib calls for the rest */

#define STM_GPIO_REG_SET_POUT 0x04
#define STM_GPIO_REG_CLR_POUT 0x08
#define STM_GPIO_REG_PIN 0x10

extern int stm_gpio_num; /* Number of available internal PIOs */
extern void __iomem **stm_gpio_bases; /* PIO blocks base addresses array */

static inline int gpio_get_value(unsigned gpio)
{
	if (likely(gpio < stm_gpio_num))
		return (readl(stm_gpio_bases[stm_gpio_port(gpio)] +
				STM_GPIO_REG_PIN)
				& (1 << stm_gpio_pin(gpio))) != 0;
	else
		return __gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned gpio, int value)
{
	if (likely(gpio < stm_gpio_num))
		writel(1 << stm_gpio_pin(gpio),
				stm_gpio_bases[stm_gpio_port(gpio)] +
				(value ? STM_GPIO_REG_SET_POUT :
				STM_GPIO_REG_CLR_POUT));
	else
		__gpio_set_value(gpio, value);
}

static inline int gpio_cansleep(unsigned gpio)
{
	if (likely(gpio < stm_gpio_num))
		return 0;
	else
		return __gpio_cansleep(gpio);
}

#define gpio_to_irq(gpio) __gpio_to_irq(gpio)

/* The gpiolib doesn't provides irq support so the following
 * functions are valid for internal PIOs only */
int irq_to_gpio(unsigned irq);


/* STM specific API */

/* Early initialisation */
void stm_gpio_early_init(struct platform_device pdevs[], int num, int irq_base);

/* Pin direction control */
#define STM_GPIO_DIRECTION_BIDIR 0x1
#define STM_GPIO_DIRECTION_OUT 0x2
#define STM_GPIO_DIRECTION_IN 0x4
#define STM_GPIO_DIRECTION_ALT_OUT 0x6
#define STM_GPIO_DIRECTION_ALT_BIDIR 0x7
int stm_gpio_direction(unsigned int gpio, unsigned int direction);

#endif
