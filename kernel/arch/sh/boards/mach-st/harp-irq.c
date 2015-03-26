/*
 * Copyright (C) 2000 STMicroelectronics Limited
 * Author: David J. Mckay (david.mckay@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Looks after interrupts on the HARP board.
 *
 * Bases on the IPR irq system
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <asm/system.h>
#include <mach/epld.h>
#include <mach/common.h>

#define NUM_EXTERNAL_IRQS 16

static void disable_harp_irq(unsigned int irq)
{
	unsigned maskReg;
	unsigned mask;
	int pri;

	if (irq < 0 || irq >= NUM_EXTERNAL_IRQS)
		return;

	pri = 15 - irq;

	if (pri < 8) {
		maskReg = EPLD_INTMASK0CLR;
	} else {
		maskReg = EPLD_INTMASK1CLR;
		pri -= 8;
	}
	mask = 1 << pri;

	epld_write(mask, maskReg);

	/* Read back the value we just wrote to flush any write posting */
	epld_read(maskReg);
}

static void enable_harp_irq(unsigned int irq)
{
	unsigned maskReg;
	unsigned mask;
	int pri;

	if (irq < 0 || irq >= NUM_EXTERNAL_IRQS)
		return;

	pri = 15 - irq;

	if (pri < 8) {
		maskReg = EPLD_INTMASK0SET;
	} else {
		maskReg = EPLD_INTMASK1SET;
		pri -= 8;
	}
	mask = 1 << pri;

	epld_write(mask, maskReg);
}

static void __init disable_all_interrupts(void)
{
	epld_write(0x00, EPLD_INTMASK0);
	epld_write(0x00, EPLD_INTMASK1);
}

static struct irq_chip harp_chips[NUM_EXTERNAL_IRQS] = {
	[0 ... NUM_EXTERNAL_IRQS-1] = {
		.mask = disable_harp_irq,
		.unmask = enable_harp_irq,
		.mask_ack = disable_harp_irq,
		.name = "harp",
	}
};

void __init harp_init_irq(void)
{
	int irq;

	disable_all_interrupts();

	for (irq = 0; irq < NUM_EXTERNAL_IRQS; irq++) {
		disable_irq_nosync(irq);
		set_irq_chip_and_handler_name(irq, &harp_chips[irq],
			handle_level_irq, "level");
		disable_harp_irq(irq);
	}
}
