/*
 * include/linux/stm/pio.h
 *
 * Copyright (c) 2004 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * ST40 General Purpose IO pins support.
 *
 * This layer enables other device drivers to configure PIO
 * pins, get and set their values, and register an interrupt
 * routine for when input pins change state.
 */

#ifndef __LINUX_STM_PIO_H
#define __LINUX_STM_PIO_H

#include <linux/kernel.h>

/* Pin configuration constants */
/* Note that behaviour for modes 0, 6 and 7 differ between the ST40STB1
 * datasheet (implementation restrictions appendix), and the ST40
 * architectural defintions.
 */
#define STPIO_NONPIO		0	/* Non-PIO function (ST40 defn) */
#define STPIO_BIDIR_Z1     	0	/* Input weak pull-up (arch defn) */
#define STPIO_BIDIR		1	/* Bidirectonal open-drain */
#define STPIO_OUT		2	/* Output push-pull */
#define STPIO_IN		4	/* Input Hi-Z */
#define STPIO_ALT_OUT		6	/* Alt output push-pull (arch defn) */
#define STPIO_ALT_BIDIR		7	/* Alt bidir open drain (arch defn) */

struct stpio_pin;

/* Request and release exclusive use of a PIO pin */
#define stpio_request_pin(portno, pinno, name, direction) \
		__stpio_request_pin(portno, pinno, name, direction, 0, 0)
#define stpio_request_set_pin(portno, pinno, name, direction, value) \
		__stpio_request_pin(portno, pinno, name, direction, 1, value)
struct stpio_pin *__stpio_request_pin(unsigned int portno,
		unsigned int pinno, const char *name, int direction,
		int __set_value, unsigned int value);
void stpio_free_pin(struct stpio_pin *pin);

/* Change the mode of an existing pin */
void stpio_configure_pin(struct stpio_pin *pin, int direction);

/* Get, set value */
void stpio_set_pin(struct stpio_pin* pin, unsigned int value);
unsigned int stpio_get_pin(struct stpio_pin* pin);

/* Interrupt on external value change */
int stpio_flagged_request_irq(struct stpio_pin *pin, int comp,
                       void (*handler)(struct stpio_pin *pin, void *dev),
                       void *dev, unsigned long irqflags);
static inline int __deprecated stpio_request_irq(struct stpio_pin *pin,
		       int comp,
                       void (*handler)(struct stpio_pin *pin, void *dev),
                       void *dev)
{
	return stpio_flagged_request_irq(pin, comp, handler, dev, 0);
}
void stpio_free_irq(struct stpio_pin* pin);
void stpio_enable_irq(struct stpio_pin* pin, int mode);
void stpio_disable_irq(struct stpio_pin* pin);
void stpio_disable_irq_nosync(struct stpio_pin* pin);
void stpio_set_irq_type(struct stpio_pin* pin, int triggertype);

#ifdef CONFIG_PM
int stpio_set_wakeup(struct stpio_pin *pin, int enabled);
#else
#define stpio_set_wakeup(pin, enabled)	do {} while(0)
#endif

#endif /* __LINUX_STM_PIO_H */
