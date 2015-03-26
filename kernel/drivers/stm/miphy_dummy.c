#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/pio.h>
#include <linux/stm/platform.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/miphy.h>
#include "miphy.h"

#define NAME		"stm-miphy-dummy"

struct stm_miphy_miphy_device {
	struct stm_miphy_device miphy_dev;
};

static struct stm_miphy_miphy_device miphy_dummy_dev;

static u8 stm_miphy_dummy_reg_read(int port, u8 addr)
{
	return 0;
}
static void stm_miphy_dummy_reg_write(int port, u8 addr, u8 data)
{
}

static const struct miphy_if_ops stm_miphy_dummy_ops = {
	.reg_write = stm_miphy_dummy_reg_write,
	.reg_read = stm_miphy_dummy_reg_read,
};

static int stm_miphy_dummy_probe(struct platform_device *pdev)
{
	struct stm_plat_miphy_dummy_data *data = pdev->dev.platform_data;
	int result;

	result = miphy_if_register(&miphy_dummy_dev.miphy_dev, DUMMY_IF,
			data->miphy_first, data->miphy_count, data->miphy_modes,
			&pdev->dev, &stm_miphy_dummy_ops);

	if (result) {
		printk(KERN_ERR "Unable to Register DUMMY MiPHY device\n");
		return result;
	}

	return 0;
}

static int stm_miphy_dummy_remove(struct platform_device *pdev)
{
	miphy_if_unregister(&miphy_dummy_dev.miphy_dev);

	return 0;
}

static struct platform_driver stm_miphy_dummy_driver = {
	.driver.name = NAME,
	.driver.owner = THIS_MODULE,
	.probe = stm_miphy_dummy_probe,
	.remove = stm_miphy_dummy_remove,
};

static int __init stm_miphy_dummy_init(void)
{
	return platform_driver_register(&stm_miphy_dummy_driver);
}

postcore_initcall(stm_miphy_dummy_init);

MODULE_AUTHOR("STMicroelectronics @st.com");
MODULE_DESCRIPTION("STM MiPHY DUMMY driver");
MODULE_LICENSE("GPL");
