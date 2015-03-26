/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <mach/common.h>

#define DRIVER_NAME "epld"

static void __iomem *epld_base;
static int epld_opsize;

void epld_write(unsigned long value, unsigned long offset)
{
	if (epld_opsize == 16)
		writew(value, epld_base + offset);
	else
		writeb(value, epld_base + offset);
}

unsigned long epld_read(unsigned long offset)
{
	if (epld_opsize == 16)
		return readw(epld_base + offset);
	else
		return readb(epld_base + offset);
}

void __init epld_early_init(struct platform_device *pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;
	struct plat_epld_data *data = pdev->dev.platform_data;

	epld_base = ioremap(pdev->resource[0].start, size);
	if (!epld_base)
		panic("Unable to ioremap EPLD");

	if (data)
		epld_opsize = data->opsize;
}

static int __init epld_probe(struct platform_device *pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(pdev->resource[0].start, size, pdev->name))
		return -EBUSY;

	if (epld_base)
		return 0;

	epld_early_init(pdev);

	return 0;
}

static struct platform_driver epld_driver = {
	.probe		= epld_probe,
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init epld_init(void)
{
	return platform_driver_register(&epld_driver);
}

arch_initcall(epld_init);
