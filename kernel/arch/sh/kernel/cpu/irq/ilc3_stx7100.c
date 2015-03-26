/*
 * linux/arch/sh/kernel/cpu/irq/ilc3_stx7100.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interrupt handling for ST40 Interrupt Level Controler (ILC).
 */


#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>
#include <linux/pm.h>
#include <linux/io.h>

#include <asm/system.h>

#include "ilc3.h"

static void __iomem *ilc_base;

#define DRIVER_NAME "ilc3"


void __init ilc_early_init(struct platform_device *pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	ilc_base = ioremap(pdev->resource[0].start, size);
}

static int __init ilc_probe(struct platform_device *pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(pdev->resource[0].start, size, pdev->name))
		return -EBUSY;

	/* Have we already been set up through ilc_early_init? */
	if (ilc_base)
		return 0;

	ilc_early_init(pdev);

	return 0;
}

static struct platform_driver ilc_driver = {
	.probe		= ilc_probe,
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init ilc_init(void)
{
	return platform_driver_register(&ilc_driver);
}

core_initcall(ilc_init);

struct ilc_route_log {
	int ilc_irq;
	int ext_out;
	int invert;
};

static struct ilc_route_log ilc_log[4];

/*
 * it was '__init' but in the PM System we have to route the irq again...
 */
void ilc_route_external(int ilc_irq, int ext_out, int invert)
{
	int offset = ext_out-4;
	ILC_SET_PRI(ilc_base, ilc_irq, 0x8000 | ext_out);
	ILC_SET_TRIGMODE(ilc_base, ilc_irq, invert ? ILC_TRIGGERMODE_LOW :
		ILC_TRIGGERMODE_HIGH);
	ILC_SET_ENABLE(ilc_base, ilc_irq);
	ilc_log[offset].ilc_irq = ilc_irq;
	ilc_log[offset].ext_out = ext_out;
	ilc_log[offset].invert  = invert;
}

#ifdef CONFIG_PM
static int ilc_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	int idx;
	static pm_message_t prev_state;

	if (state.event == PM_EVENT_ON &&
	    prev_state.event == PM_EVENT_FREEZE) /* Resuming from hibernation*/
		  for (idx = 0; idx < ARRAY_SIZE(ilc_log); ++idx)
			ilc_route_external(ilc_log[idx].ilc_irq,
					   ilc_log[idx].ext_out,
					   ilc_log[idx].invert);
	prev_state = state;
	return 0;
}

static int ilc_sysdev_resume(struct sys_device *dev)
{
	return ilc_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_driver ilc_sysdev_driver = {
	.suspend = ilc_sysdev_suspend,
	.resume = ilc_sysdev_resume,
};

static int __init ilc_sysdev_init(void)
{
	return sysdev_driver_register(&cpu_sysdev_class, &ilc_sysdev_driver);
}

module_init(ilc_sysdev_init);
#endif
