/*
 * STx7200 SH-4 Setup
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7200.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



/* SH4-only resources ----------------------------------------------------- */

static struct platform_device stx7200_sh4_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 16,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd804000, 0x900),
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
		.disable_wakeup = 1,
	},
};

static struct platform_device *stx7200_sh4_devices[] __initdata = {
	&stx7200_sh4_ilc3_device,
};

static int __init stx7200_sh4_devices_setup(void)
{
	return platform_add_devices(stx7200_sh4_devices,
			ARRAY_SIZE(stx7200_sh4_devices));
}
core_initcall(stx7200_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	TMU0, TMU1, TMU2, RTC, SCIF, WDT, HUDI,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(RTC, 0x480), INTC_VECT(RTC, 0x4a0), INTC_VECT(RTC, 0x4c0),
	INTC_VECT(SCIF, 0x4e0), INTC_VECT(SCIF, 0x500),
		INTC_VECT(SCIF, 0x520), INTC_VECT(SCIF, 0x540),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_prio_reg prio_registers[] = {
					/*  15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */ { TMU0, TMU1, TMU2,   RTC } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */ {  WDT,    0, SCIF,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */ {    0,    0,    0,  HUDI } },
};

static DECLARE_INTC_DESC(intc_desc, "stx7200", vectors, NULL,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	struct sysconf_field *sc;

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&intc_desc);
}
