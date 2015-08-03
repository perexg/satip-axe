/*
 * EHCI HCD (Host Controller Driver) for USB.
 *
 * Copyright (c) 2005 STMicroelectronics Limited
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * Bus Glue for STMicroelectronics STx710x devices.
 *
 * This file is licenced under the GPL.
 */

#include <linux/platform_device.h>
#include "./hcd-stm.h"

/* Define a bus wrapper IN/OUT threshold of 128 */
#define AHB2STBUS_INSREG01_OFFSET       (0x10 + 0x84) /* From EHCI_BASE */
#define AHB2STBUS_INOUT_THRESHOLD       0x00800080

#undef dgb_print

#ifdef CONFIG_USB_DEBUG
#define dgb_print(fmt, args...)			\
		pr_debug("%s: " fmt, __func__, ## args)
#else
#define dgb_print(fmt, args...)
#endif

static int ehci_st40_reset(struct usb_hcd *hcd)
{
	writel(AHB2STBUS_INOUT_THRESHOLD,
	       hcd->regs + AHB2STBUS_INSREG01_OFFSET);
	return ehci_init(hcd);
}

#ifdef CONFIG_PM
static int
stm_ehci_bus_suspend(struct usb_hcd *hcd)
{
	ehci_bus_suspend(hcd);
/*
 * force the root hub to be resetted on resume!
 * re-enumerates everything during a standby, mem and hibernation...
 */
	usb_root_hub_lost_power(hcd->self.root_hub);
	return 0;
}

static int
stm_ehci_bus_resume(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci;

	ehci = hcd_to_ehci(hcd);
	ehci_writel(ehci, FLAG_CF, &ehci->regs->configured_flag);

	return ehci_bus_resume(hcd);
}
#else
#define stm_ehci_bus_suspend		NULL
#define stm_ehci_bus_resume		NULL
#endif

static const struct hc_driver ehci_stm_hc_driver = {
	.description = hcd_name,
	.product_desc = "STMicroelectronics EHCI Host Controller",
	.hcd_priv_size = sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq = ehci_irq,
	.flags = HCD_MEMORY | HCD_USB2,

	/*
	 * basic lifecycle operations
	 */
	.reset = ehci_st40_reset,
	.start = ehci_run,
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,
	.endpoint_reset	= ehci_endpoint_reset,

	/*
	 * scheduling support
	 */
	.get_frame_number = ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
/*
 * The ehci_bus_suspend suspends all the root hub ports but
 * it leaves all the interrupts enabled on insert/remove devices
 */
	.bus_suspend = stm_ehci_bus_suspend,
	.bus_resume = stm_ehci_bus_resume,
	.relinquish_port = ehci_relinquish_port,
	.port_handed_over = ehci_port_handed_over,

	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,
};

static int ehci_hcd_stm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);

	return 0;
}

static int ehci_hcd_stm_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct usb_hcd *hcd;
        struct ehci_hcd *ehci;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct platform_device *stm_usb_pdev;

	dgb_print("\n");
	hcd = usb_create_hcd(&ehci_stm_hc_driver, dev, dev_name(dev));
	if (!hcd) {
		retval = -ENOMEM;
		goto err0;
	}

	stm_usb_pdev = to_platform_device(pdev->dev.parent);

	res = platform_get_resource_byname(stm_usb_pdev,
			IORESOURCE_MEM, "ehci");
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		pr_debug("request_mem_region failed");
		retval = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		pr_debug("ioremap failed");
		retval = -ENOMEM;
		goto err2;
	}

	ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs + HC_LENGTH(ehci_readl(ehci,
						      &ehci->caps->hc_capbase));

	/* cache this readonly data; minimize device reads */
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);

	/*
	 * Fix the reset port issue on a load-unload-load sequence
	 */
	ehci->has_reset_port_bug = 1,
	res = platform_get_resource_byname(stm_usb_pdev,
			IORESOURCE_IRQ, "ehci");

	retval = usb_add_hcd(hcd, res->start, 0);
	if (unlikely(retval))
		goto err3;

#ifdef CONFIG_PM
	hcd->self.root_hub->do_remote_wakeup = 0;
	hcd->self.root_hub->persist_enabled = 0;
	hcd->self.root_hub->autosuspend_disabled = 1;
	hcd->self.root_hub->autoresume_disabled = 1;
#endif
	return 0;
err3:
	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
err0:
	return retval;
}

static struct platform_driver ehci_hcd_stm_driver = {
	.probe = ehci_hcd_stm_probe,
	.remove = ehci_hcd_stm_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "stm-ehci",
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_PM_RUNTIME
static DEFINE_MUTEX(stm_ehci_usb_mutex); /* to serialize the operations.. */

int stm_ehci_hcd_unregister(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	int ret = 0;
	if (!hcd)
		return ret;

	mutex_lock(&stm_ehci_usb_mutex);
	ret = ehci_hcd_stm_remove(dev);
	mutex_unlock(&stm_ehci_usb_mutex);
	if (ret)
		dgb_print("[STM][USB] Error on %s %p\n", __func__, dev);
	return ret;

}
EXPORT_SYMBOL(stm_ehci_hcd_unregister);

int stm_ehci_hcd_register(struct platform_device *dev)
{
	int ret = 0;

	mutex_lock(&stm_ehci_usb_mutex);
	ret = ehci_hcd_stm_probe(dev);
	mutex_unlock(&stm_ehci_usb_mutex);
	if (ret)
		dgb_print("[STM][USB] Error on %s %p\n", __func__, dev);
	return ret;
}
EXPORT_SYMBOL(stm_ehci_hcd_register);
#endif
