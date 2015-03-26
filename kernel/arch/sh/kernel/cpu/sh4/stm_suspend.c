/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2008  STMicroelectronics
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/kobject.h>
#include <linux/clk.h>
#include <linux/hardirq.h>
#include <linux/jiffies.h>
#include <linux/io.h>

#include <linux/stm/pm_notify.h>

#include <asm/system.h>
#include <asm/cacheflush.h>
#include <asm-generic/bug.h>

#include <cpu/mmu_context.h>
#include "stm_suspend.h"

#undef  dbg_print

#ifdef CONFIG_PM_DEBUG
#define dbg_print(fmt, args...)		\
		printk(KERN_INFO "%s: " fmt, __func__, ## args)
#else
#define dbg_print(fmt, args...)
#endif

static struct stm_platform_suspend_t *platform_suspend;

unsigned int wokenup_by;

static unsigned long stm_read_intevt(void)
{
	return ctrl_inl(INTEVT);
}

static int stm_suspend_enter(suspend_state_t state)
{
	unsigned long soc_flags;
	unsigned long tbl, tbl_size;
	unsigned long lpj =
		(cpu_data[raw_smp_processor_id()].loops_per_jiffy * HZ) / 1000;
	enum stm_pm_type type = (state == PM_SUSPEND_STANDBY) ?
		STM_PM_SUSPEND : STM_PM_MEMSUSPEND;
	enum stm_pm_notify_return notify_ret;
	int err = 0;

	/* Must wait for serial buffers to clear */
	/*printk(KERN_INFO "CPU is sleeping\n");*/
	mdelay(2000);

	flush_cache_all();

	if (platform_suspend->pre_enter)
		err = platform_suspend->pre_enter(state);

	/*
	 * If the platform pre_enter returns an error suspend is
	 * aborted.
	 */
	if (err)
		goto on_error;

	/* sets the right instruction table */
	if (state == PM_SUSPEND_STANDBY) {
		tbl = platform_suspend->stby_tbl;
		tbl_size = platform_suspend->stby_size;
		soc_flags = ((platform_suspend->flags & NO_SLEEP_ON_STANDBY)
				? 1 : 0);
		soc_flags += ((platform_suspend->flags &
			EARLY_ACTION_ON_STANDBY) ? 2 : 0);
	} else {
		tbl = platform_suspend->mem_tbl;
		tbl_size = platform_suspend->mem_size;
		soc_flags = ((platform_suspend->flags & NO_SLEEP_ON_MEMSTANDBY)
				? 1 : 0);
		soc_flags += ((platform_suspend->flags &
			EARLY_ACTION_ON_MEMSTANDBY) ? 2 : 0);
	}

	BUG_ON(in_irq());

__stm_again_suspend:
	notify_ret = stm_pm_prepare_enter(type);
	if (notify_ret == STM_PM_RET_ERROR)
		goto __stm_skip_suspend;

	stm_exec_table(tbl, tbl_size, lpj, soc_flags);

	BUG_ON(in_irq());

	wokenup_by = stm_read_intevt();
	if (platform_suspend->evt_to_irq)
		wokenup_by = platform_suspend->evt_to_irq(wokenup_by);
	else
		wokenup_by = evt2irq(wokenup_by);

__stm_skip_suspend:
	notify_ret = stm_pm_post_enter(type, wokenup_by);
	if (notify_ret == STM_PM_RET_AGAIN)
		goto __stm_again_suspend;

	if (platform_suspend->post_enter)
		platform_suspend->post_enter(state);

	printk(KERN_INFO "CPU woken up by: 0x%x\n", wokenup_by);

	return 0;

on_error:
	if (platform_suspend->post_enter)
		platform_suspend->post_enter(state);
	pr_err("[STM][PM] Error on Core Suspend\n");
	return -EINVAL;
}

static int stm_suspend_valid_both(suspend_state_t state)
{
	return 1;
}

int __init stm_suspend_register(struct stm_platform_suspend_t *_suspend)
{
	if (!_suspend)
		return -EINVAL;

	platform_suspend = _suspend;
	platform_suspend->ops.enter = stm_suspend_enter;

	if (platform_suspend->stby_tbl && platform_suspend->stby_size)
		platform_suspend->ops.valid = stm_suspend_valid_both;
	else
		platform_suspend->ops.valid = suspend_valid_only_mem;

	suspend_set_ops(&platform_suspend->ops);

	printk(KERN_INFO "[STM]: [PM]: Suspend support registered\n");

	return 0;
}
