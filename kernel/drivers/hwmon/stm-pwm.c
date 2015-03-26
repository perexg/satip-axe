/*
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Contains code copyright (C) Echostar Technologies Corporation
 * Author: Anthony Jackson <anthony.jackson@echostar.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/stm/platform.h>
#include <asm/io.h>

struct stm_pwm {
	struct resource *mem;
	void* base;
	struct device *hwmon_dev;
};

/* PWM registers */
#define PWM0_VAL		0x00
#define PWM1_VAL		0x04
#define PWM_CTRL		0x50
#define PWM_CTRL_PWM_EN			(1<<9)
#define PWM_CTRL_PWM_CLK_VAL0_SHIFT	0
#define PWM_CTRL_PWM_CLK_VAL0_MASK	0x0f
#define PWM_CTRL_PWM_CLK_VAL4_SHIFT	11
#define PWM_CTRL_PWM_CLK_VAL4_MASK	0xf0
#define PWM_INT_EN		0x54

/* Prescale value (clock dividor):
 * 0: divide by 1
 * 0xff: divide by 256 */
#define PWM_CLK_VAL		0

static ssize_t show_pwm(struct device *dev, char * buf, int offset)
{
	struct stm_pwm *pwm = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", readl(pwm->base + offset));
}

static ssize_t store_pwm(struct device *dev, const char * buf, size_t count,
			 int offset)
{
	struct stm_pwm *pwm = dev_get_drvdata(dev);
	char* p;
	long val = simple_strtol(buf, &p, 10);

	if (p != buf) {
		val &= 0xff;
		writel(val, pwm->base + offset);
		return p-buf;
	}
	return -EINVAL;
}

#define pwm_fns(n)							\
static ssize_t								\
show_pwm##n(struct device *dev,	struct device_attribute *attr,		\
	    char *buf)							\
{									\
	return show_pwm(dev, buf, PWM##n##_VAL);			\
}									\
static ssize_t								\
store_pwm##n(struct device *dev, struct device_attribute *attr,		\
	     const char *buf, size_t count)				\
{									\
	return store_pwm(dev, buf, count, PWM##n##_VAL);		\
}									\
static DEVICE_ATTR(pwm##n, S_IRUGO| S_IWUSR, show_pwm##n, store_pwm##n);

pwm_fns(0)
pwm_fns(1)

static int
stm_pwm_init(struct platform_device* pdev, struct stm_pwm *pwm,
	     struct stm_plat_pwm_data *plat_data)
{
	int error;
	u32 reg = 0;

	/* disable PWM if currently running */
	reg = readl(pwm->base + PWM_CTRL);
	reg &= ~PWM_CTRL_PWM_EN;
	writel(reg, pwm->base + PWM_CTRL);

	/* disable all PWM related interrupts */
	reg = 0;
	writel(reg, pwm->base + PWM_INT_EN);

	/* Set global PWM state:
	 * disable capture... */
	reg = 0;

	/* set prescale value... */
	reg |= (PWM_CLK_VAL & PWM_CTRL_PWM_CLK_VAL0_MASK) << PWM_CTRL_PWM_CLK_VAL0_SHIFT;
	reg |= (PWM_CLK_VAL & PWM_CTRL_PWM_CLK_VAL4_MASK) << PWM_CTRL_PWM_CLK_VAL4_SHIFT;

	/* enable */
	reg |= PWM_CTRL_PWM_EN;
	writel(reg, pwm->base + PWM_CTRL);

	/* Initial value */
	writel(0, pwm->base + PWM0_VAL);
	writel(0, pwm->base + PWM1_VAL);

	if (plat_data->channel_enabled[0]) {
		if (!devm_stm_pad_claim(&pdev->dev,
				plat_data->channel_pad_config[0],
				dev_name(&pdev->dev))) {
			error = -ENODEV;
			goto error_pad_claim_0;
		}
		error = device_create_file(&pdev->dev, &dev_attr_pwm0);
		if (error != 0)
			goto error_create_file_0;
	}

	if (plat_data->channel_enabled[1]) {
		if (!devm_stm_pad_claim(&pdev->dev,
				plat_data->channel_pad_config[1],
				dev_name(&pdev->dev))) {
			error = -ENODEV;
			goto error_pad_claim_1;
		}
		error = device_create_file(&pdev->dev, &dev_attr_pwm1);
		if (error != 0)
			goto error_create_file_1;
	}

	return 0;

error_create_file_1:
error_pad_claim_1:
	if (plat_data->channel_enabled[0])
		device_remove_file(&pdev->dev, &dev_attr_pwm0);
error_create_file_0:
error_pad_claim_0:
	return error;
}

static int stm_pwm_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct stm_pwm *pwm;
	int err;

	pwm = kmalloc(sizeof(struct stm_pwm), GFP_KERNEL);
	if (pwm == NULL) {
		return -ENOMEM;
	}
	memset(pwm, 0, sizeof(*pwm));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!res) {
                err = -ENODEV;
		goto failed1;
	}

	pwm->mem = request_mem_region(res->start, res->end - res->start + 1, "stm-pwn");
	if (pwm->mem == NULL) {
		dev_err(&pdev->dev, "failed to claim memory region\n");
                err = -EBUSY;
		goto failed1;
	}

	pwm->base = ioremap(res->start, res->end - res->start + 1);
	if (pwm->base == NULL) {
		dev_err(&pdev->dev, "failed ioremap");
		err = -EINVAL;
		goto failed2;
	}

	pwm->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(pwm->hwmon_dev)) {
		err = PTR_ERR(pwm->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n", err);
		goto failed3;
	}

	platform_set_drvdata(pdev, pwm);
	dev_info(&pdev->dev, "registers at 0x%x, mapped to 0x%p\n",
		 res->start, pwm->base);

	return stm_pwm_init(pdev, pwm, pdev->dev.platform_data);

failed3:
	iounmap(pwm->base);
failed2:
	release_resource(pwm->mem);
failed1:
	kfree(pwm);
	return err;
}

static int stm_pwm_remove(struct platform_device *pdev)
{
	struct stm_pwm *pwm = platform_get_drvdata(pdev);

	if (pwm) {
		hwmon_device_unregister(pwm->hwmon_dev);
		iounmap(pwm->base);
		release_resource(pwm->mem);
		kfree(pwm);
	}
	return 0;
}

static struct platform_driver stm_pwm_driver = {
	.driver = {
		.name		= "stm-pwm",
	},
	.probe		= stm_pwm_probe,
	.remove		= stm_pwm_remove,
};

static int __init stm_pwm_module_init(void)
{
	return platform_driver_register(&stm_pwm_driver);
}

static void __exit stm_pwm_module_exit(void)
{
	platform_driver_unregister(&stm_pwm_driver);
}

module_init(stm_pwm_module_init);
module_exit(stm_pwm_module_exit);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics simple PWM driver");
MODULE_LICENSE("GPL");
