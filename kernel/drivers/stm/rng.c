/*
 * 	File: stm_rng.c
 *
 *	Hardware Random Number Generator Support for  STM STx7109 / STx7200 RNG
 *	(c) Copyright 2008 ST Microelectronics (R&D) Ltd.
 *
 *	Author: <carl.shaw@st.com>
 *
 *	----------------------------------------------------------
 *	This software may be used and distributed according to the terms
 *	of the GNU General Public License v2, incorporated herein by reference.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/io.h>

/*
 * core module and version information
 */
#define RNG_VERSION "1.0"
#define RNG_MODULE_NAME "stm-rng"
#define RNG_DRIVER_NAME RNG_MODULE_NAME " hardware driver " RNG_VERSION
#define PFX RNG_MODULE_NAME ": "

/* Debug Macros */
/* #define DEBUG */

#ifdef DEBUG
#define DPRINTK(fmt, args...) \
		printk(KERN_DEBUG PFX "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/*
 * Defines
 */
/* per-cpu buffer size in 16 bit words */
#define STM_RNG_BUFFSIZE   512
/* register offsets */
#define STM_RNG_STATUS_REG 0x20
#define STM_RNG_DATA_REG   0x24

/*
 * Local variables
 */

struct timer_list      stm_rng_timer;
static void __iomem   *stm_rng_base;
static unsigned short *stm_rng_buffer;
static unsigned long   stm_rng_bufcnt;
static spinlock_t      stm_rng_spinlock = SPIN_LOCK_UNLOCKED;

/*
 * The real work is done by the poll function below.
 * The timer should fire every 10 ms
 */

static void stm_rng_poll(unsigned long arg)
{
	u32 val;

	mod_timer(&stm_rng_timer, jiffies + (HZ / 100));

	spin_lock(&stm_rng_spinlock);

	/* data OK ? */
	val = readl(stm_rng_base + STM_RNG_STATUS_REG);

	if ((val & 3) == 0) {
		/* Get random number and add to our entropy pool */
		val = readl(stm_rng_base + STM_RNG_DATA_REG);
		stm_rng_buffer[stm_rng_bufcnt] =
				(unsigned short)(val & 0x0000ffff);
		stm_rng_bufcnt++;

		if (stm_rng_bufcnt == STM_RNG_BUFFSIZE) {
			DPRINTK("adding RNG data to /dev/random\n");
			add_random_data((char *)stm_rng_buffer,
				STM_RNG_BUFFSIZE*sizeof(short));
			stm_rng_bufcnt = 0;
		}
	}

	spin_unlock(&stm_rng_spinlock);
}

/*
 * Platform bus support
 */

static int __init stm_rng_probe(struct platform_device *rng_device)
{
	struct resource *res;

	if (!rng_device->name) {
		pr_err(PFX "Device probe failed. "
				"Check your kernel SoC config!!\n");
		return -ENODEV;
	}

	res = platform_get_resource(rng_device, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err(PFX "RNG config not found. "
				"Check your kernel SoC config!!\n");
		return -ENODEV;
	}

	DPRINTK("RNG physical base address = 0x%08x\n", res->start);

	if (res->start == 0) {
		pr_err(PFX "RNG base address undefined. "
				"Check your SoC config!!\n");
		return -ENODEV;
	}

	stm_rng_base = ioremap(res->start, 0x28);

	if (stm_rng_base == NULL) {
		pr_err(PFX "Cannot ioremap RNG memory\n");
		return -EBUSY;
	}

	stm_rng_buffer = kmalloc(STM_RNG_BUFFSIZE * sizeof(short), GFP_KERNEL);
	if (stm_rng_buffer == NULL) {
		pr_err(PFX "Cannot allocate entropy words buffer\n");
		return -ENOMEM;
	}

	stm_rng_bufcnt = 0;

	init_timer(&stm_rng_timer);
	stm_rng_timer.function = stm_rng_poll;
	stm_rng_timer.expires  = jiffies + (HZ / 100);
	add_timer(&stm_rng_timer);

	pr_info(RNG_DRIVER_NAME " configured\n");

	return 0;
}

static int stm_rng_remove(struct platform_device *dev)
{
	del_timer_sync(&stm_rng_timer);
	iounmap(stm_rng_base);
	kfree(stm_rng_buffer);

	return 0;
}

static struct platform_driver stm_rng_driver = {
	.driver.name = "stm-rng",
	.driver.owner = THIS_MODULE,
	.probe = stm_rng_probe,
	.remove = stm_rng_remove,
};

/*
 * rng_init - initialize RNG poll timer
 */

static int __init stm_rng_init(void)
{
	return platform_driver_register(&stm_rng_driver);
}

/*
 * rng_init - shutdown RNG module
 */

static void __exit stm_rng_cleanup(void)
{
	platform_driver_unregister(&stm_rng_driver);
}

MODULE_AUTHOR("ST Microelectronics R&D Ltd. <carl.shaw@st.com>");
MODULE_DESCRIPTION("STM H/W Random Number Generator (RNG) driver");
MODULE_LICENSE("GPL");

module_init(stm_rng_init);
module_exit(stm_rng_cleanup);
