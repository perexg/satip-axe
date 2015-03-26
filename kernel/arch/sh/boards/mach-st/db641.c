/*
 * arch/sh/boards/st/common/db641.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STEM board with double Ethernet interface support
 *
 * Only first port (CN1) is supported. Required DB641 jumpers settings are:
 * - J1 and J5 = open
 * - J2, J3, J4 = 1-2
 *
 * STEM notCS0/notINTR0 lines are expected to be used - see
 * "arch/sh/include/mach-<board>/mach/stem.h" for more information about
 * main board configuration.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/smsc911x.h>
#include <linux/stm/emi.h>
#include <mach/stem.h>

static struct platform_device smsc_lan9117 = {
	.name		= "smsc911x",
	.id		= -1,
	.num_resources	= 2,
	.resource	= (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			/* .start & .end - see db641_init() */
		},
		{
			.flags = IORESOURCE_IRQ,
			/* .start & .end - see db641_init() */
		},
	},
	.dev.platform_data = &(struct smsc911x_platform_config) {
		.irq_polarity = SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
		.irq_type = SMSC911X_IRQ_TYPE_PUSH_PULL,
		.flags = SMSC911X_USE_32BIT,
	},
};

static int __init db641_init(void)
{
	smsc_lan9117.resource[0].start = emi_bank_base(STEM_CS0_BANK)
			+ STEM_CS0_OFFSET;
	smsc_lan9117.resource[0].end = smsc_lan9117.resource[0].start + 0xff;
	
	smsc_lan9117.resource[1].start = STEM_INTR0_IRQ;
	smsc_lan9117.resource[1].end = smsc_lan9117.resource[1].start;

	emi_bank_configure(STEM_CS0_BANK, (unsigned long[4]){ 0x041086f1,
			0x0e024400, 0x0e024400, 0 });

	return platform_device_register(&smsc_lan9117);
}
arch_initcall(db641_init);

