/*
 * STx5197 SH-4 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <asm/irq-ilc.h>



/* SH4-only resources ----------------------------------------------------- */

static struct platform_device stx5197_sh4_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 16,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd100000, 0x900),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x200), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x220), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x240), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x260), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x280), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x2a0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x2c0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x2e0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x300), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x320), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x340), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x360), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x380), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x3a0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x3c0), -1),
	},
	.dev.platform_data = &(struct stm_plat_ilc3_data) {
		.inputs_num = ILC_NR_IRQS,
		.outputs_num = 16,
		.first_irq = ILC_FIRST_IRQ,
	},
};

static struct platform_device *stx5197_sh4_devices[] __initdata = {
	&stx5197_sh4_ilc3_device,
};

static int __init stx5197_sh4_devices_setup(void)
{
	return platform_add_devices(stx5197_sh4_devices,
				    ARRAY_SIZE(stx5197_sh4_devices));
}
core_initcall(stx5197_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, WDT, HUDI,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,       } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(intc_desc, "stx5197", vectors, NULL,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	register_intc_controller(&intc_desc);
}
