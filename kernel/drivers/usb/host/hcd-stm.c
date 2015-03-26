/*
 * HCD (Host Controller Driver) for USB.
 *
 * Copyright (c) 2009 STMicroelectronics Limited
 * Author: Francesco Virlinzi
 *
 * Bus Glue for STMicroelectronics STx710x devices.
 *
 * This file is licenced under the GPL.
 */

#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/stm/device.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb.h>
#include <linux/io.h>
#include "../core/hcd.h"
#include "hcd-stm.h"

#undef dgb_print

#ifdef CONFIG_USB_DEBUG
#define dgb_print(fmt, args...)			\
		pr_debug("%s: " fmt, __func__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

#define USB_48_CLK	0
#define USB_IC_CLK	1
#define USB_PHY_CLK	2

static void stm_usb_clk_xxable(struct drv_usb_data *drv_data, int enable)
{
	int i;
	struct clk *clk;
	for (i = 0; i < USB_CLKS_NR; ++i) {
		clk = drv_data->clks[i];
		if (!clk || IS_ERR(clk))
			continue;
		if (enable) {
			/*
			 * On some chip the USB_48_CLK comes from
			 * ClockGen_B. In this case a clk_set_rate
			 * is welcome because the code becomes
			 * target_pack independant
			 */
			if (clk_enable(clk) == 0 && i == USB_48_CLK)
				clk_set_rate(clk, 48000000);
		} else
			clk_disable(clk);
	}
}

static void stm_usb_clk_enable(struct drv_usb_data *drv_data)
{
	stm_usb_clk_xxable(drv_data, 1);
}

static void stm_usb_clk_disable(struct drv_usb_data *drv_data)
{
	stm_usb_clk_xxable(drv_data, 0);
}

static int stm_usb_boot(struct platform_device *pdev)
{
	struct stm_plat_usb_data *pl_data = pdev->dev.platform_data;
	struct drv_usb_data *usb_data = platform_get_drvdata(pdev);
	void *wrapper_base = usb_data->ahb2stbus_wrapper_glue_base;
	void *protocol_base = usb_data->ahb2stbus_protocol_base;
	unsigned long reg, req_reg;

	if (pl_data->flags &
		(STM_PLAT_USB_FLAGS_STRAP_8BIT |
		 STM_PLAT_USB_FLAGS_STRAP_16BIT)) {
		/* Set strap mode */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		if (pl_data->flags & STM_PLAT_USB_FLAGS_STRAP_16BIT)
			reg |= AHB2STBUS_STRAP_16_BIT;
		else
			reg &= ~AHB2STBUS_STRAP_16_BIT;
		writel(reg, wrapper_base + AHB2STBUS_STRAP_OFFSET);
	}

	if (pl_data->flags & STM_PLAT_USB_FLAGS_STRAP_PLL) {
		/* Start PLL */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		writel(reg | AHB2STBUS_STRAP_PLL,
			wrapper_base + AHB2STBUS_STRAP_OFFSET);
		mdelay(30);
		writel(reg & (~AHB2STBUS_STRAP_PLL),
			wrapper_base + AHB2STBUS_STRAP_OFFSET);
		mdelay(30);
	}

	if (pl_data->flags & STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE) {
		/* Set the STBus Opcode Config for load/store 32 */
		writel(AHB2STBUS_STBUS_OPC_32BIT,
			protocol_base + AHB2STBUS_STBUS_OPC_OFFSET);

		/* Set the Message Size Config to n packets per message */
		writel(AHB2STBUS_MSGSIZE_4,
			protocol_base + AHB2STBUS_MSGSIZE_OFFSET);

		/* Set the chunksize to n packets */
		writel(AHB2STBUS_CHUNKSIZE_4,
			protocol_base + AHB2STBUS_CHUNKSIZE_OFFSET);
	}

	if (pl_data->flags &
		(STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128 |
		STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256)) {

		req_reg = (1<<21) |  /* Turn on read-ahead */
			  (5<<16) |  /* Opcode is store/load 32 */
			  (0<<15) |  /* Turn off write posting */
			  (1<<14) |  /* Enable threshold */
			  (3<<9)  |  /* 2**3 Packets in a chunk */
			  (0<<4)  ;  /* No messages */
		req_reg |= ((pl_data->flags &
			STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128) ?
				(7<<0) :	/* 128 */
				(8<<0));	/* 256 */
		do {
			writel(req_reg, protocol_base +
				AHB2STBUS_MSGSIZE_OFFSET);
			reg = readl(protocol_base + AHB2STBUS_MSGSIZE_OFFSET);
		} while ((reg & 0x7FFFFFFF) != req_reg);
	}
	return 0;
}

static int stm_usb_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct drv_usb_data *dr_data = platform_get_drvdata(pdev);

	stm_device_power(dr_data->device_state, stm_device_power_off);

	stm_usb_clk_disable(dr_data);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wrapper");
	devm_release_mem_region(dev, res->start, resource_size(res));
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "protocol");
	devm_release_mem_region(dev, res->start, resource_size(res));

	if (dr_data->ehci_device)
		platform_device_unregister(dr_data->ehci_device);
	if (dr_data->ohci_device)
		platform_device_unregister(dr_data->ohci_device);

	return 0;
}

/*
 * Slightly modified version of platform_device_register_simple()
 * which assigns parent and has no resources.
 */
static struct platform_device
*stm_usb_device_create(const char *name, int id, struct platform_device *parent)
{
	struct platform_device *pdev;
	int retval;

	pdev = platform_device_alloc(name, id);
	if (!pdev) {
		retval = -ENOMEM;
		goto error;
	}

	pdev->dev.parent = &parent->dev;
	pdev->dev.dma_mask = parent->dev.dma_mask;

	retval = platform_device_add(pdev);
	if (retval)
		goto error;

	return pdev;

error:
	platform_device_put(pdev);
	return ERR_PTR(retval);
}

static int __init stm_usb_probe(struct platform_device *pdev)
{
	struct stm_plat_usb_data *plat_data = pdev->dev.platform_data;
	struct drv_usb_data *dr_data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0, i;
	static char __initdata *usb_clks_n[USB_CLKS_NR] = {
		[USB_48_CLK] = "usb_48_clk",
		[USB_IC_CLK] = "usb_ic_clk",
		[USB_PHY_CLK] = "usb_phy_clk"
	};
	resource_size_t len;

	dgb_print("\n");
	dr_data = kzalloc(sizeof(struct drv_usb_data), GFP_KERNEL);
	if (!dr_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, dr_data);

	for (i = 0; i < USB_CLKS_NR; ++i) {
		dr_data->clks[i] = clk_get(dev, usb_clks_n[i]);
		if (!dr_data->clks[i] || IS_ERR(dr_data->clks[i]))
			pr_warning("%s: %s clock not found for %s\n",
				__func__, usb_clks_n[i], dev_name(dev));
	}

	stm_usb_clk_enable(dr_data);

	dr_data->device_state = devm_stm_device_init(&pdev->dev,
		plat_data->device_config);
	if (!dr_data->device_state) {
		ret = -EBUSY;
		goto err_0;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wrapper");
	if (!res) {
		ret = -ENXIO;
		goto err_0;
	}
	len = resource_size(res);
	if (devm_request_mem_region(dev, res->start, len, pdev->name) < 0) {
		ret = -EBUSY;
		goto err_0;
	}
	dr_data->ahb2stbus_wrapper_glue_base =
		devm_ioremap_nocache(dev, res->start, len);

	if (!dr_data->ahb2stbus_wrapper_glue_base) {
		ret = -EFAULT;
		goto err_1;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "protocol");
	if (!res) {
		ret = -ENXIO;
		goto err_2;
	}
	len = resource_size(res);
	if (devm_request_mem_region(dev, res->start, len, pdev->name) < 0) {
		ret = -EBUSY;
		goto err_2;
	}
	dr_data->ahb2stbus_protocol_base =
		devm_ioremap_nocache(dev, res->start, len);

	if (!dr_data->ahb2stbus_protocol_base) {
		ret = -EFAULT;
		goto err_3;
	}
	stm_usb_boot(pdev);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ehci");
	if (res) {
		dr_data->ehci_device = stm_usb_device_create("stm-ehci",
			pdev->id, pdev);
		if (IS_ERR(dr_data->ehci_device)) {
			ret = (int)dr_data->ehci_device;
			goto err_4;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ohci");
	if (res) {
		dr_data->ohci_device =
			stm_usb_device_create("stm-ohci", pdev->id, pdev);
		if (IS_ERR(dr_data->ohci_device)) {
			if (dr_data->ehci_device)
				platform_device_del(dr_data->ehci_device);
			ret = (int)dr_data->ohci_device;
			goto err_4;
		}
	}

	/* Initialize the pm_runtime fields */
	pm_runtime_set_active(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);

	return ret;

err_4:
	devm_iounmap(dev, dr_data->ahb2stbus_protocol_base);
err_3:
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "protocol");
	devm_release_mem_region(dev, res->start, resource_size(res));
err_2:
	devm_iounmap(dev, dr_data->ahb2stbus_wrapper_glue_base);
err_1:
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wrapper");
	devm_release_mem_region(dev, res->start, resource_size(res));
err_0:
	kfree(dr_data);
	return ret;
}

static void stm_usb_shutdown(struct platform_device *pdev)
{
	struct drv_usb_data *dr_data = platform_get_drvdata(pdev);
	dgb_print("\n");

	stm_device_power(dr_data->device_state, stm_device_power_off);

	stm_usb_clk_disable(dr_data);
}

#ifdef CONFIG_PM
static int stm_usb_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drv_usb_data *dr_data = dev_get_drvdata(dev);
	struct stm_plat_usb_data *pl_data = pdev->dev.platform_data;
	void *wrapper_base = dr_data->ahb2stbus_wrapper_glue_base;
	void *protocol_base = dr_data->ahb2stbus_protocol_base;
	long reg;
	dgb_print("\n");

#ifdef CONFIG_PM_RUNTIME
	if (dev->power.runtime_status != RPM_ACTIVE)
		return 0; /* usb already suspended via runtime_suspend */
#endif

	if (pl_data->flags & STM_PLAT_USB_FLAGS_STRAP_PLL) {
		/* PLL turned off */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		writel(reg | AHB2STBUS_STRAP_PLL,
			wrapper_base + AHB2STBUS_STRAP_OFFSET);
	}

	writel(0, wrapper_base + AHB2STBUS_STRAP_OFFSET);
	writel(0, protocol_base + AHB2STBUS_STBUS_OPC_OFFSET);
	writel(0, protocol_base + AHB2STBUS_MSGSIZE_OFFSET);
	writel(0, protocol_base + AHB2STBUS_CHUNKSIZE_OFFSET);
	writel(0, protocol_base + AHB2STBUS_MSGSIZE_OFFSET);

	writel(1, protocol_base + AHB2STBUS_SW_RESET);
	mdelay(10);
	writel(0, protocol_base + AHB2STBUS_SW_RESET);

	stm_device_power(dr_data->device_state, stm_device_power_off);

	stm_usb_clk_disable(dr_data);

	return 0;
}

static int stm_usb_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drv_usb_data *dr_data = dev_get_drvdata(dev);

#ifdef CONFIG_PM_RUNTIME
	if (dev->power.runtime_status == RPM_SUSPENDED)
		return 0; /* usb wants resume via runtime_resume... */
#endif

	dgb_print("\n");

	stm_usb_clk_enable(dr_data);

	stm_device_power(dr_data->device_state, stm_device_power_on);

	stm_usb_boot(pdev);

	return 0;
}
#else
#define stm_usb_suspend NULL
#define stm_usb_resume NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int stm_usb_runtime_suspend(struct device *dev)
{
	struct drv_usb_data *dr_data = dev_get_drvdata(dev);

	if (dev->power.runtime_status == RPM_SUSPENDED) {
		dgb_print("%s already suspended\n", dev_name(dev));
		return 0;
	}

	dgb_print("Runtime suspending %s\n", dev_name(dev));
#if defined(CONFIG_USB_EHCI_HCD) || defined(CONFIG_USB_EHCI_HCD_MODULE)
	if (dr_data->ehci_device)
		stm_ehci_hcd_unregister(dr_data->ehci_device);
#endif

#if defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)
	if (dr_data->ohci_device)
		stm_ohci_hcd_unregister(dr_data->ohci_device);
#endif

	return stm_usb_suspend(dev);
}
static int stm_usb_runtime_resume(struct device *dev)
{
	struct drv_usb_data *dr_data = dev_get_drvdata(dev);

	if (dev->power.runtime_status == RPM_ACTIVE) {
		dgb_print("%s already active\n", dev_name(dev));
		return 0;
	}
	dgb_print("Runtime resuming: %s\n", dev_name(dev));
	stm_usb_resume(dev);
#if defined(CONFIG_USB_EHCI_HCD) || defined(CONFIG_USB_EHCI_HCD_MODULE)
	if (dr_data->ehci_device)
		stm_ehci_hcd_register(dr_data->ehci_device);
#endif

#if defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)
	if (dr_data->ohci_device)
		stm_ohci_hcd_register(dr_data->ohci_device);
#endif
	return 0;
}
#else
#define stm_usb_runtime_suspend		NULL
#define stm_usb_runtime_resume		NULL
#endif

static struct dev_pm_ops stm_usb_pm = {
	.suspend = stm_usb_suspend,  /* on standby/memstandby */
	.resume = stm_usb_resume,    /* resume from standby/memstandby */
	.runtime_suspend = stm_usb_runtime_suspend,
	.runtime_resume = stm_usb_runtime_resume,
};

static struct platform_driver stm_usb_driver = {
	.driver = {
		.name = "stm-usb",
		.owner = THIS_MODULE,
		.pm = &stm_usb_pm,
	},
	.probe = stm_usb_probe,
	.shutdown = stm_usb_shutdown,
	.remove = stm_usb_remove,
};

static int __init stm_usb_init(void)
{
	return platform_driver_register(&stm_usb_driver);
}

static void __exit stm_usb_exit(void)
{
	platform_driver_unregister(&stm_usb_driver);
}

MODULE_LICENSE("GPL");

module_init(stm_usb_init);
module_exit(stm_usb_exit);
