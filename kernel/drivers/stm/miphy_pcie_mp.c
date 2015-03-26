/*
 * drivers/stm/miphy_pcie_mp.c
 *
 *  Copyright (c) 2010 STMicroelectronics Limited
 *  Author: Srinivas.Kandagatla <srinivas.kandagatla@st.com>
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *
 *
 * Support for the UPort interface to MiPhy.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/pio.h>
#include <linux/stm/platform.h>
#include <linux/stm/miphy.h>
#include "miphy.h"

#define NAME	"pcie-mp"

struct pcie_mp_device{
	struct stm_miphy_device miphy_dev;
	void __iomem *mp_base;
	void (*mp_select)(int port);
	struct miphy_if_ops *ops;
};
static struct pcie_mp_device *mp_dev;

static void stm_pcie_mp_register_write(int miphyselect, u8 address, u8 data)
{
	BUG_ON(!mp_dev || !mp_dev->mp_select);
	mp_dev->mp_select(miphyselect);
	iowrite8(data, mp_dev->mp_base + address);
}

static u8 stm_pcie_mp_register_read(int miphyselect, u8 address)
{
	u8 data;
	BUG_ON(!mp_dev || !mp_dev->mp_select);
	mp_dev->mp_select(miphyselect);
	data = ioread8(mp_dev->mp_base + address);
	return data;
}

static const struct miphy_if_ops stm_miphy_mp_ops = {
	.reg_write = stm_pcie_mp_register_write,
	.reg_read = stm_pcie_mp_register_read,
};

static int __init pcie_mp_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct stm_plat_pcie_mp_data *data =
			(struct stm_plat_pcie_mp_data *)pdev->dev.platform_data;
	int result;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	mp_dev = kzalloc(sizeof(struct pcie_mp_device), GFP_KERNEL);
	if (!mp_dev)
		return -ENOMEM;

	if (!devm_request_mem_region(&pdev->dev, res->start,
			     res->end - res->start, "pcie-mp")) {
		printk(KERN_ERR NAME " Request mem 0x%x region failed\n",
		       res->start);
		return -ENOMEM;
	}
	mp_dev->mp_base = devm_ioremap_nocache(&pdev->dev,
					res->start, resource_size(res));
	mp_dev->mp_select = data->mp_select;

	if (!mp_dev->mp_base)
		return -ENOMEM;

	result = miphy_if_register(&mp_dev->miphy_dev, UPORT_IF,
			data->miphy_first, data->miphy_count, data->miphy_modes,
			&pdev->dev, &stm_miphy_mp_ops);

	if (result) {
		printk(KERN_ERR "Unable to Register uPort MiPHY device\n");
		return result;
	}

	return 0;
}
static int  pcie_mp_remove(struct platform_device *pdev)
{
	miphy_if_unregister(&mp_dev->miphy_dev);
	kfree(mp_dev->ops);
	kfree(mp_dev);
	mp_dev = NULL;
	return 0;
}

static struct platform_driver pcie_mp_driver = {
	.driver.name = NAME,
	.driver.owner = THIS_MODULE,
	.probe = pcie_mp_probe,
	.remove = pcie_mp_remove,
};

static int __init pcie_mp_init(void)
{
	return platform_driver_register(&pcie_mp_driver);
}

postcore_initcall(pcie_mp_init);

MODULE_AUTHOR("Srinivas.Kandagatla <srinivas.kandagatla@st.com>");
MODULE_DESCRIPTION("STM PCIE-UPort driver");
MODULE_LICENSE("GPL");
