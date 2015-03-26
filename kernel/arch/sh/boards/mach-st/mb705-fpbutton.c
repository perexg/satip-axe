/*
 * arch/sh/boards/st/common/mb705-fpbutton.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Driver for the front pannel button on the mb705 (SW12).
 *
 * Note that some mb705 EPLD revisions disable the ability for the
 * FPButton to generate an interrupt, which renders this driver useless.
 *
 * Note SW8-4 on the mb705 must be on to disable propogation to the mb680
 * which interprets the FP button as a reset.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/input.h>
#include <mach/common.h>
#include "mb705-epld.h"

#define SCAN_INTERVAL 100

static struct timer_list fpbutton_timer;

static irqreturn_t fpbutton_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
        struct input_dev *input = platform_get_drvdata(pdev);

	input_event(input, EV_KEY, BTN_0, 1);
	input_sync(input);

	disable_irq(platform_get_irq(pdev, 0));
	mod_timer(&fpbutton_timer, jiffies + msecs_to_jiffies(SCAN_INTERVAL));

	return IRQ_HANDLED;
}

static void fpbutton_timer_callback(unsigned long data)
{
	struct platform_device *pdev = (struct platform_device *)data;
        struct input_dev *input = platform_get_drvdata(pdev);
	int mask = platform_get_irq_byname(pdev, "mask");
	u16 status = epld_read(EPLD_EMI_INT_STATUS);

	if (status & mask) {
		mod_timer(&fpbutton_timer,
			  jiffies + msecs_to_jiffies(SCAN_INTERVAL));
	} else {
		input_event(input, EV_KEY, BTN_0, 0);
		input_sync(input);

		enable_irq(platform_get_irq(pdev, 0));
	}
}

static int __init mb705_fpbutton_probe(struct platform_device *pdev)
{
	struct input_dev *input;
	int error;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	platform_set_drvdata(pdev, input);

	input->evbit[0] = BIT(EV_KEY);

	input->name = pdev->name;
	input->phys = "mb705-fpbutton/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	input_set_capability(input, EV_KEY, BTN_0);

        error = input_register_device(input);
	if (error)
		goto fail;

	init_timer(&fpbutton_timer);
        fpbutton_timer.function = fpbutton_timer_callback;
        fpbutton_timer.data = (unsigned long)pdev;

	error = request_irq(platform_get_irq(pdev, 0), fpbutton_isr,
			    IRQF_SAMPLE_RANDOM, "mb705-fpbutton", pdev);
	if (error)
		goto fail;

	return 0;

fail:
	input_free_device(input);

	return error;
}

static int __exit mb705_fpbutton_remove(struct platform_device *pdev)
{
	struct input_dev *input = platform_get_drvdata(pdev);

	del_timer_sync(&fpbutton_timer);
	free_irq(platform_get_irq(pdev, 0), pdev);
	input_unregister_device(input);

        return 0;
}

static struct platform_driver mb705_fpbutton_driver = {
	.remove		= __exit_p(mb705_fpbutton_remove),
	.driver		= {
		.name	= "mb705-fpbutton",
		.owner	= THIS_MODULE,
	},
};

static int __init mb705_fpbutton_init(void)
{
	return platform_driver_probe(&mb705_fpbutton_driver,
				     mb705_fpbutton_probe);
}

static void __exit mb705_fpbutton_exit(void)
{
	platform_driver_unregister(&mb705_fpbutton_driver);
}

module_init(mb705_fpbutton_init);
module_exit(mb705_fpbutton_exit);
