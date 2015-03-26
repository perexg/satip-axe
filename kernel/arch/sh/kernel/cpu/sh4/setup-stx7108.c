/*
 * STx7108 SH-4 Setup
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



/* SH4-only resources ----------------------------------------------------- */

static struct platform_device stx7108_host_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfda34000, 0x900),
		/* Currently we route all ILC3 interrupts to the 0'th output,
		 * which is connected to INTC2: group 0 interrupt 0 (INTEVT
		 * code 0xa00) */
		STM_PLAT_RESOURCE_IRQ(evt2irq(0xa00), -1),
	},
	.dev.platform_data = &(struct stm_plat_ilc3_data) {
		.inputs_num = ILC_NR_IRQS,
		.outputs_num = 16,
		.first_irq = ILC_FIRST_IRQ,
	},
};

/* L2-Cache is available on the HOST core only! */
static struct platform_device stx7108_host_l2_cache_device = {
	.name 		= "stm-l2-cache",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfef04000, 0x200),
	},
};

static struct platform_device *stx7108_host_devices[] __initdata = {
	&stx7108_host_ilc3_device,
	&stx7108_host_l2_cache_device
};

static struct platform_device stx7108_rt_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfda3c000, 0x900),
		/* Currently we route all ILC3 interrupts to the 0'th output,
		 * which is connected to INTC2: group 0 interrupt 0 (INTEVT
		 * code 0xa00) */
		STM_PLAT_RESOURCE_IRQ(evt2irq(0xa00), -1),
	},
	.dev.platform_data = &(struct stm_plat_ilc3_data) {
		.inputs_num = ILC_NR_IRQS,
		.outputs_num = 16,
		.first_irq = ILC_FIRST_IRQ,
	},
};

static int __init stx7108_sh4_devices_setup(void)
{
	if (STX7108_HOST_CORE) {
		return platform_add_devices(stx7108_host_devices,
				ARRAY_SIZE(stx7108_host_devices));
	} else {
		return platform_device_register(&stx7108_rt_ilc3_device);
	}
}
core_initcall(stx7108_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2,
	WDT,
	HUDI,
};

static struct intc_vect stx7108_intc_vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_prio_reg stx7108_intc_prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,     0 } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(stx7108_intc_desc, "stx7108",
		stx7108_intc_vectors, NULL,
		NULL, stx7108_intc_prio_registers, NULL);

void __init plat_irq_setup(void)
{
	void *intc2_base;

	register_intc_controller(&stx7108_intc_desc);

	if (STX7108_HOST_CORE)
		intc2_base = ioremap(0xfda30000, 0x100);
	else
		intc2_base = ioremap(0xfda38000, 0x100);

	/* Enable the INTC2 */
	writel(7, intc2_base + 0x00);	/* INTPRI00 */
	writel(1, intc2_base + 0x60);	/* INTMSKCLR00 */
}
