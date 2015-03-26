#include <linux/module.h>
#include <linux/platform_device.h>
#include "fdma.h"



struct fdma_xbar {
	struct resource *memory;
	void *base;
	struct fdma_req_router router;
};



static int fdma_xbar_route(struct fdma_req_router *router, int input_req_line,
		int fdma, int fdma_req_line)
{
	struct fdma_xbar *xbar = container_of(router, struct fdma_xbar, router);
	int output_line = (fdma * FDMA_REQ_LINES) + fdma_req_line;

	writel(input_req_line, xbar->base + (output_line * 4));

	return 0;
}

static int __init fdma_xbar_probe(struct platform_device *pdev)
{
	struct fdma_xbar *xbar;
	struct resource *memory;
	unsigned long phys_base, phys_size;

	xbar = kzalloc(sizeof(*xbar), GFP_KERNEL);
	if (xbar == NULL)
		return -ENOMEM;

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	BUG_ON(!memory);
	phys_base = memory->start;
	phys_size = memory->end - memory->start + 1;

	xbar->memory = request_mem_region(phys_base, phys_size,
					  dev_name(&pdev->dev));
	if (xbar->memory == NULL) {
		kfree(xbar);
		return -EBUSY;
	}

	xbar->base = ioremap_nocache(phys_base, phys_size);
	if (xbar->base == NULL) {
		release_resource(xbar->memory);
		kfree(xbar);
		return -EBUSY;
	}

	xbar->router.route = fdma_xbar_route;

	platform_set_drvdata(pdev, xbar);

	if (fdma_register_req_router(&xbar->router) < 0) {
		iounmap(xbar->base);
		release_resource(xbar->memory);
		kfree(xbar);
		return -EINVAL;
	}

	return 0;
}

static int fdma_xbar_remove(struct platform_device *pdev)
{
	struct fdma_xbar *xbar = platform_get_drvdata(pdev);

	fdma_unregister_req_router(&xbar->router);

	iounmap(xbar->base);
	release_resource(xbar->memory);
	kfree(xbar);

	return 0;
}

static struct platform_driver fdma_xbar_driver = {
	.driver.name = "stm-fdma-xbar",
	.probe = fdma_xbar_probe,
	.remove = fdma_xbar_remove,
};

static int __init fdma_xbar_init(void)
{
	return platform_driver_register(&fdma_xbar_driver);
}

static void __exit fdma_xbar_exit(void)
{
	platform_driver_unregister(&fdma_xbar_driver);
}

module_init(fdma_xbar_init)
module_exit(fdma_xbar_exit)

