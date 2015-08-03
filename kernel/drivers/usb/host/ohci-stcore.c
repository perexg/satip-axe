/*
 * OHCI HCD (Host Controller Driver) for USB
 *
 * (C) copyright STMicroelectronics 2005
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * STMicroelectronics on-chip USB host controller Bus Glue.
 * Based on the StrongArm ohci-sa1111.c file
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include "./hcd-stm.h"

#undef dgb_print

#ifdef CONFIG_USB_DEBUG
#define dgb_print(fmt, args...)				\
		pr_debug("%s: " fmt, __func__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif


static int
stm_ohci_start(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret = 0;

	dgb_print("\n");
	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	if ((ret = ohci_run(ohci)) < 0) {
		err("can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM
static int stm_ohci_bus_suspend(struct usb_hcd *hcd)
{
	dgb_print("\n");
	ohci_bus_suspend(hcd);
	usb_root_hub_lost_power(hcd->self.root_hub);
	return 0;
}
#endif

static const struct hc_driver ohci_st40_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"STMicroelectronics OHCI Host Controller",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/* generic hardware linkage */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/* basic lifecycle operations */
	.start =		stm_ohci_start,
	.stop =			ohci_stop,
	.shutdown = ohci_shutdown,

	/* managing i/o requests and associated device resources */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/* scheduling support */
	.get_frame_number =	ohci_get_frame,

	/* root hub support */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
#ifdef CONFIG_PM
	.bus_suspend =		stm_ohci_bus_suspend,
	.bus_resume =		ohci_bus_resume,
#endif
	.start_port_reset =	ohci_start_port_reset,
};

static int ohci_hcd_stm_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd = NULL;
	int retval;
	struct resource *res;
	struct platform_device *stm_usb_pdev;

	dgb_print("\n");
	hcd = usb_create_hcd(&ohci_st40_hc_driver, &pdev->dev,
			     dev_name(&pdev->dev));

	if (!hcd) {
		pr_debug("hcd_create_hcd failed");
		retval = -ENOMEM;
		goto err0;
	}

	stm_usb_pdev = to_platform_device(pdev->dev.parent);

	res = platform_get_resource_byname(stm_usb_pdev,
			IORESOURCE_MEM, "ohci");
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,	hcd_name)) {
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

	ohci_hcd_init(hcd_to_ohci(hcd));

	res = platform_get_resource_byname(stm_usb_pdev,
			IORESOURCE_IRQ, "ohci");
	retval = usb_add_hcd(hcd, res->start, 0);
	if (retval == 0) {
#ifdef CONFIG_PM
		hcd->self.root_hub->do_remote_wakeup = 0;
		hcd->self.root_hub->persist_enabled = 0;
		hcd->self.root_hub->autosuspend_disabled = 1;
		hcd->self.root_hub->autoresume_disabled = 1;
#endif
		return retval;
	}

	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
err0:
	return retval;
}

static int ohci_hcd_stm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);

	return 0;
}

static struct platform_driver ohci_hcd_stm_driver = {
	.probe = ohci_hcd_stm_probe,
	.remove = ohci_hcd_stm_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "stm-ohci",
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_PM
static DEFINE_MUTEX(stm_ohci_usb_mutex); /* to serialize the operations.. */

int stm_ohci_hcd_unregister(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	int ret = 0;

	if (!hcd)
		return ret;

	mutex_lock(&stm_ohci_usb_mutex);
	ret = ohci_hcd_stm_remove(dev);
	mutex_unlock(&stm_ohci_usb_mutex);
	if (ret)
		dgb_print("[STM][USB] Error on %s %p\n", __func__, dev);
	return ret;
}
EXPORT_SYMBOL(stm_ohci_hcd_unregister);

int stm_ohci_hcd_register(struct platform_device *dev)
{
	int ret = 0;

	mutex_lock(&stm_ohci_usb_mutex);
	ret = ohci_hcd_stm_probe(dev);
	mutex_unlock(&stm_ohci_usb_mutex);
	if (ret)
		dgb_print("[STM][USB] Error on %s %p\n", __func__, dev);

	return ret;
}
EXPORT_SYMBOL(stm_ohci_hcd_register);
#endif
