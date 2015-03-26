/*
 * arch/sh/oprofile/op_model_sh_tmu.c
 *
 * Copyright (C) 2010 STMicroelectronics
 * Author : Marc Titinger <marc.titinger-amesys@st.com>
 *
 * oprofile sh model using the "generic timer" API
 * from Stuart Menefy for interrupt trigger.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/smp.h>
#include <linux/oprofile.h>
#include <linux/signal.h>
#include <linux/interrupt.h>
#include <linux/profile.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/stm/platform.h>
#include <linux/io.h>

#include <linux/fs.h>

#include <linux/sh_timer.h>
#include "op_impl.h"

#define DEFAULT_SAMPLE_RATE	 987 /*Hz*/

static struct sh_timer_callb *timer;
static unsigned long sampling_rate;


/***********************************************************************
 * sh_op_tmu_setrate()
 *
 *
 */
static void sh_op_tmu_setrate(struct op_counter_config *unused)
{
		return;
}

static ssize_t sh_op_tmu_read_rate(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	return oprofilefs_ulong_to_user(sampling_rate, buf, count, ppos);
}

static ssize_t sh_op_tmu_write_rate(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	unsigned long val;

	if (oprofilefs_ulong_from_user(&val, buf, count))
		return -EFAULT;

	if (val > 10000)
		sampling_rate = 10000;
	if (val < 250)
		sampling_rate = 250;
	else
		sampling_rate = val;

	printk(KERN_NOTICE
	"please restart the profiler to apply new rate %ld Hz.\n",sampling_rate);
	printk(KERN_NOTICE
	"You may need to run \"opcontrol --reset\" for data coherence.\n");

	return count;
}

static const struct file_operations rate_fops = {
	.read	= sh_op_tmu_read_rate,
	.write	= sh_op_tmu_write_rate,
};

static int sh_op_tmu_create_files(struct super_block *sb, struct dentry *dir)
{
	return oprofilefs_create_file(sb, dir, "rate", &rate_fops);
}

/***********************************************************************
 * sh_op_tmu_irq()
 *
 */
static void sh_op_tmu_irq(void *data)
{
	struct pt_regs *regs = get_irq_regs();

	/* Give the sample to oprofile. */
	oprofile_add_sample(regs, 0);

	return;
}

/***********************************************************************
 * sh_op_tmu_stop()
 *
 *
 */
static void sh_op_tmu_stop(void *args)
{
	if (timer == NULL)
		return;

	timer->timer_stop(timer->tmu_priv);
	sh_timer_unregister(timer->tmu_priv);
	timer = NULL;
}

/***********************************************************************
 * sh_op_tmu_start()
 *
 *
 */
static void sh_op_tmu_start(void *args)
{
	if (timer == NULL)
		timer = sh_timer_register(sh_op_tmu_irq,  (void *)0);

	if (timer == NULL) {
		printk(KERN_ERR "sh_op_tmu_start: no available TMU timer, start failed!\n");
		return;
	}

	printk(KERN_NOTICE "sh_op_tmu_setrate: settings TMU timer at %ld Hz\n",
			sampling_rate);

	timer->set_rate(timer->tmu_priv, sampling_rate);
	timer->timer_start(timer->tmu_priv);
}

int sh_op_tmu_init(void)
{
	timer = NULL ;
	sampling_rate = DEFAULT_SAMPLE_RATE;
	return 0;
}

void sh_op_tmu_exit(void)
{
	sh_op_tmu_stop(NULL);
}

static void sh_op_tmu_setup(void *dummy)
{
}

struct op_sh_model op_sh_tmu_ops = {
	.cpu_type	= "timer",	/*for compat with the usermode app*/
	.num_counters	= 1, 	/*dir "0"*/
	.reg_setup	= sh_op_tmu_setrate,
	.cpu_setup	= sh_op_tmu_setup,
	.cpu_start	= sh_op_tmu_start,
	.cpu_stop	= sh_op_tmu_stop,
	.init	= sh_op_tmu_init,
	.exit	= sh_op_tmu_exit,
	.create_files	= sh_op_tmu_create_files,
};
