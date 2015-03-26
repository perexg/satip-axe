/*
 * drivers/char/hw_random/stm-rng.c
 *
 * RNG driver for ST Microelectronics STx7xxx  SoCs
 *
 * Author: Carl Shaw <carl.shaw@st.com>
 *
 * Copyright (c) 2007-2008 ST Microelectronics R&D Ltd.
 *
 * Based on omap RNG driver
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/hw_random.h>

#include <asm/io.h>

#define STM_RNG_STATUS_REG 0x20
#define STM_RNG_DATA_REG   0x24

static void __iomem *rng_base;

static u32 stm_rng_read_reg(int reg)
{
	return __raw_readl(rng_base + reg);
}

static int stm_rng_data_present(struct hwrng *rng)
{
	return ((stm_rng_read_reg(STM_RNG_STATUS_REG) & 3) == 0);
}

static int stm_rng_data_read(struct hwrng *rng, u32 *data)
{
	*data = stm_rng_read_reg(STM_RNG_DATA_REG);

	return 2;
}

static struct hwrng stm_rng_ops = {
	.name		= "stm",
	.data_present	= stm_rng_data_present,
	.data_read	= stm_rng_data_read,
};

static int __init stm_rng_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	BUG_ON(rng_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		printk(KERN_ERR "STM hw_random device probe failed."
				"Please Check your SoC config.\n");
		return -ENOENT;
	}

	rng_base = (u32 __iomem *)ioremap(res->start, 0x28);
	if (!rng_base) {
		printk(KERN_ERR "STM hw_random device: cannot ioremap\n");
		return -ENOMEM;
	}

	ret = hwrng_register(&stm_rng_ops);
	if (ret) {
		iounmap(rng_base);
		rng_base = NULL;
		return ret;
	}

	dev_info(&pdev->dev, "STM Random Number Generator ver. 0.1\n");

	return 0;
}

static int __exit stm_rng_remove(struct platform_device *pdev)
{
	hwrng_unregister(&stm_rng_ops);

	iounmap(rng_base);
	rng_base = NULL;

	return 0;
}

static struct platform_driver stm_rng_driver = {
	.driver = {
		.name		= "stm-hwrandom",
		.owner		= THIS_MODULE,
	},
	.probe		= stm_rng_probe,
	.remove		= __exit_p(stm_rng_remove),
};

static int __init stm_rng_init(void)
{
	return platform_driver_register(&stm_rng_driver);
}

static void __exit stm_rng_exit(void)
{
	platform_driver_unregister(&stm_rng_driver);
}

module_init(stm_rng_init);
module_exit(stm_rng_exit);

MODULE_AUTHOR("ST Microelectronics <carl.shaw@st.com>");
MODULE_LICENSE("GPL");
