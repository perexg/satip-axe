/*
 * This driver implements a RTC using the Low Power Timer in
 * the System Service of stx5197 STMicroelectronics devices.
 *
 * See ADCS 8101263B for more details on the hardware.
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/platform.h>
#include <linux/stm/clk.h>

#define DRV_NAME "stm-rtc"
#define DRV_VERSION "0.1"

#define SS_LOCK_CFG		0x300

/* Low Power Timer */
#define LPC_LPT_LSB_OFF		0x200
#define LPC_LPT_MSB_OFF		0x204
#define LPC_LPT_START_OFF	0x208

/* Low Power Alarm */
#define LPC_LPA_LSB_OFF		0x120
#define LPC_LPA_LSB_MASK	0xffff

#define LPC_LPA_MSB_OFF		0x124
#define LPC_LPA_MSB_MASK	0x00f

#define LPC_LPA_START_ENABLE	(1 << 4)

#define LPA_CLK			27000000
struct stm_rtc {
	struct rtc_device *rtc_dev;
	void __iomem *ioaddr;
	struct resource *res;
	spinlock_t lock;
	struct rtc_wkalrm alarm;
};

static void _rtc_hw_unlock(struct stm_rtc *rtc)
{
	writel(0xf0, rtc->ioaddr + SS_LOCK_CFG);
	writel(0x0f, rtc->ioaddr + SS_LOCK_CFG);
}

static void _rtc_hw_lock(struct stm_rtc *rtc)
{
	writel(0x100, rtc->ioaddr + SS_LOCK_CFG);
}

static int stm_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct stm_rtc *rtc = dev_get_drvdata(dev);
	unsigned long long lpt;
	unsigned long lpt_lsb, lpt_msb;

	spin_lock(&rtc->lock);
	do {
		lpt_msb = readl(rtc->ioaddr + LPC_LPT_MSB_OFF);
		lpt_lsb = readl(rtc->ioaddr + LPC_LPT_LSB_OFF);
	} while (readl(rtc->ioaddr + LPC_LPT_MSB_OFF) != lpt_msb);

	lpt = ((unsigned long long)lpt_msb << 32) | lpt_lsb;
	do_div(lpt, LPA_CLK);
	rtc_time_to_tm(lpt, tm);

	spin_unlock(&rtc->lock);

	return 0;
}

static int stm_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct stm_rtc *rtc = dev_get_drvdata(dev);
	unsigned long secs;
	int ret;
	unsigned long long lpt;

	ret = rtc_tm_to_time(tm, &secs);
	if (ret != 0)
		return ret;

	pr_info("%s\n", __func__);

	spin_lock(&rtc->lock);
	lpt = secs * LPA_CLK;
	_rtc_hw_unlock(rtc);
	writel(lpt >> 32, rtc->ioaddr + LPC_LPT_MSB_OFF);
	writel(lpt, rtc->ioaddr + LPC_LPT_LSB_OFF);
	writel(1, rtc->ioaddr + LPC_LPT_START_OFF);
	_rtc_hw_lock(rtc);
	spin_unlock(&rtc->lock);

	return 0;
}


static int stm_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *wkalrm)
{
	struct stm_rtc *rtc = dev_get_drvdata(dev);
	spin_lock(&rtc->lock);
	memcpy(wkalrm, &rtc->alarm, sizeof(struct rtc_wkalrm));
	spin_unlock(&rtc->lock);
	return 0;
}

static int stm_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	struct stm_rtc *rtc = dev_get_drvdata(dev);
	struct rtc_time now;
	unsigned long now_secs;
	unsigned long alarm_secs;

	pr_info("%s\n", __func__);
	stm_rtc_read_time(dev, &now);
	rtc_tm_to_time(&now, &now_secs);
	rtc_tm_to_time(&t->time, &alarm_secs);

	if (now_secs > alarm_secs)
		return -EINVAL; /* invalid alarm time */

	device_set_wakeup_enable(dev, 1);

	memcpy(&rtc->alarm, t, sizeof(struct rtc_wkalrm));

	alarm_secs -= now_secs; /* now many secs to fire */

	spin_lock(&rtc->lock);
	_rtc_hw_unlock(rtc);
	writel(alarm_secs & LPC_LPA_LSB_MASK,
		rtc->ioaddr + LPC_LPA_LSB_OFF);
	writel(((alarm_secs >> 16) & LPC_LPA_MSB_MASK)
		| LPC_LPA_START_ENABLE, rtc->ioaddr + LPC_LPA_MSB_OFF);
	_rtc_hw_lock(rtc);
	spin_unlock(&rtc->lock);

	return 0;
}


static struct rtc_class_ops stm_rtc_ops = {
	.read_time = stm_rtc_read_time,
	.set_time = stm_rtc_set_time,
	.read_alarm = stm_rtc_read_alarm,
	.set_alarm = stm_rtc_set_alarm,
};

#ifdef CONFIG_PM
static int stm_rtc_resume(struct device *dev)
{
	struct stm_rtc *rtc = dev_get_drvdata(dev);

	pr_info("Resetting rtc-lpc devices\n");

	/*
	 * it needs to clean the 'rtc->alarm' to allow
	 * a new .set_alarm to the upper RTC layer
	 */
	memset(&rtc->alarm, 0, sizeof(struct rtc_wkalrm));

	spin_lock(&rtc->lock);
	/* turn-off the alarm */
	_rtc_hw_unlock(rtc);
	writel(0, rtc->ioaddr + LPC_LPA_LSB_OFF);
	writel(0, rtc->ioaddr + LPC_LPA_MSB_OFF);
	_rtc_hw_lock(rtc);
	spin_unlock(&rtc->lock);

	return 0;
}

static int stm_rtc_suspend(struct device *dev)
{
	if (device_may_wakeup(dev))
		return 0;

	return stm_rtc_resume(dev);
}

static struct dev_pm_ops stm_rtc_pm_ops = {
	.suspend = stm_rtc_suspend,  /* on standby/memstandby */
	.resume = stm_rtc_resume,
};
#endif

static int __devinit stm_rtc_probe(struct platform_device *pdev)
{
	struct rtc_time tm_check;
	struct stm_rtc *rtc;
	struct resource *res;
	int size;
	int ret = 0;

	rtc = kzalloc(sizeof(struct stm_rtc), GFP_KERNEL);
	if (unlikely(!rtc))
		return -ENOMEM;

	spin_lock_init(&rtc->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(res == NULL)) {
		dev_err(&pdev->dev, "No IO resource\n");
		ret = -ENOENT;
		goto err_badres;
	}

	size = res->end - res->start + 1;
	/*
	 * Memory can not be request because shared between several IPs..
	 */

	rtc->ioaddr = ioremap(res->start, size);
	if (!rtc->ioaddr) {
		ret = -EINVAL;
		goto err_badres;
	}

	platform_set_drvdata(pdev, rtc);
	device_set_wakeup_capable(&pdev->dev, 1);
	rtc->rtc_dev = rtc_device_register(DRV_NAME, &pdev->dev,
					   &stm_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		ret = PTR_ERR(rtc->rtc_dev);
		goto err_badreg;
	}

	/*
	 * The RTC-LPC is able to manage date.year > 2038
	 * but currently the kernel can not manage this date!
	 * If the RTC-LPC has a date.year > 2038 then
	 * it's set to the epoch "Jan 1st 2000"
	 */
	stm_rtc_read_time(&pdev->dev, &tm_check);

	if (tm_check.tm_year >=  (2038 - 1900)) {
		memset(&tm_check, 0, sizeof(tm_check));
		tm_check.tm_year = 100;
		/*
		 * FIXME:
		 *   the 'tm_check.tm_mday' should be set to zero but the func-
		 *   tions rtc_tm_to_time and rtc_time_to_time aren't coherent.
		 */
		tm_check.tm_mday = 1;
		stm_rtc_set_time(&pdev->dev, &tm_check);
	}

	return ret;

err_badreg:
	iounmap(rtc->ioaddr);
err_badres:
	kfree(rtc);
	return ret;
}

static int __devexit stm_rtc_remove(struct platform_device *pdev)
{
	struct stm_rtc *rtc = platform_get_drvdata(pdev);

	if (likely(rtc->rtc_dev))
		rtc_device_unregister(rtc->rtc_dev);

	iounmap(rtc->ioaddr);
	platform_set_drvdata(pdev, NULL);
	kfree(rtc);

	return 0;
}

static struct platform_driver stm_rtc_platform_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &stm_rtc_pm_ops,
#endif
	},
	.probe = stm_rtc_probe,
	.remove = __devexit_p(stm_rtc_remove),
};

static int __init stm_rtc_init(void)
{
	return platform_driver_register(&stm_rtc_platform_driver);
}

static void __exit stm_rtc_exit(void)
{
	platform_driver_unregister(&stm_rtc_platform_driver);
}

module_init(stm_rtc_init);
module_exit(stm_rtc_exit);

MODULE_DESCRIPTION("STMicroelectronics STx5197 RTC driver");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
