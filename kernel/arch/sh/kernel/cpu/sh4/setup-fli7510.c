/*
 * Freeman 510 (FLI7510) SH-4 Setup
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
#include <linux/stm/fli7510.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



/* SH4-only resources ----------------------------------------------------- */

static struct platform_device fli7510_st40host_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 16,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd218000, 0x900),
		/* The ILC outputs (16 lines) are connected to the IRL0..3
		 * INTC inputs via priority level encoder */
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

static struct platform_device fli7510_st40rt_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 16,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd210000, 0x900),
		/* The ILC outputs (16 lines) are connected to the IRL0..3
		 * INTC inputs via priority level encoder */
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

/* L2-Cache is available on the FLI75[234]0 HOST core only! */
static struct platform_device fli7520_l2_cache_device = {
	.name = "stm-l2-cache",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe600000, 0x200),
	},
};

static int __init fli7510_sh4_devices_setup(void)
{
	int result;

	if (FLI7510_ST40HOST_CORE)
		result = platform_device_register(
				&fli7510_st40host_ilc3_device);
	else
		result = platform_device_register(&fli7510_st40rt_ilc3_device);

	if (result == 0 && cpu_data->type != CPU_FLI7510)
		result = platform_device_register(&fli7520_l2_cache_device);

	return result;
}
core_initcall(fli7510_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, WDT, HUDI,
};

static struct intc_vect fli7510_intc_vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_prio_reg fli7510_intc_prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,     0 } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(fli7510_intc_desc, "fli7510",
		fli7510_intc_vectors, NULL,
		NULL, fli7510_intc_prio_registers, NULL);

void __init plat_irq_setup(void)
{
	register_intc_controller(&fli7510_intc_desc);
}
