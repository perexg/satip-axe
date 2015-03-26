/*
 * kernel/power/hom.c - Hibernation on Memory core functionality.
 *
 * Copyright (c) 2010 STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * This file is released under the GPLv2
 *
 * This files reuses most of the code used to manage
 * system wide suspend and system wide hibernation
 * to support a new kind of system wide power operation:
 *
 * Hibernation on Memory where the
 * - Chip is off
 * - DRAM is in self-refresh mode
 *
 */

#include <linux/hom.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/syscalls.h>

#include <asm-generic/sections.h>

#include "power.h"

static struct platform_hom_ops *hom_ops;

/**
 *	hom_set_ops - Set the global power method table.
 *	@ops:   Pointer to ops structure.
 */
int hom_set_ops(struct platform_hom_ops *ops)
{
	pr_debug("%s\n", __func__);
	if (!ops)
		return -EINVAL;
	mutex_lock(&pm_mutex);
	hom_ops = ops;
	mutex_unlock(&pm_mutex);
	return 0;
}
EXPORT_SYMBOL(hom_set_ops);

static inline int platform_begin(void)
{
	if (hom_ops->begin)
		return hom_ops->begin();
	return 0;
}

static inline int platform_prepare(void)
{
	if (hom_ops->prepare)
		return hom_ops->prepare();

	return 0;
}

#ifndef CONFIG_HMEM_MODE_TEST
static inline int platform_enter(void)
{
	if (hom_ops->enter)
		return hom_ops->enter();

	return 0;
}
#else
static inline int platform_enter(void) { return 0; }
#endif

static inline int platform_complete(void)
{
	if (hom_ops->complete)
		return hom_ops->complete();
	return 0;
}

static inline int platform_end(void)
{
	if (hom_ops->end)
		return hom_ops->end();
	return 0;
}

static int prepare_processes(void)
{
	int error = 0;

	if (freeze_processes()) {
		error = -EBUSY;
		thaw_processes();
	}
	return error;
}

/**
 *	hom_prepare - Do prep work before entering low-power state.
 *
 *	This is common code that is called for each state that we're entering.
 *	Run suspend notifiers, allocate a console and stop all processes.
 */
static int hom_prepare(void)
{
	int error;

	pm_prepare_console();

	error = pm_notifier_call_chain(PM_HIBERNATION_PREPARE);
	if (error)
		goto Exit;

	error = usermodehelper_disable();
	if (error)
		goto Exit;

	pr_info("PM: Syncing filesystems ... ");
	sys_sync();
	pr_info("done.\n");

	error = prepare_processes();
	if (!error)
		return 0;

	usermodehelper_enable();
Exit:
	pm_notifier_call_chain(PM_POST_HIBERNATION);
	pm_restore_console();

	return error;
}

/**
 *	hom_freeze_devices_and_enter -
 *	freeze devices and enter in hibernation on memory
 */
static int hibernation_on_memory_enter(void)
{
	int error = 0;
	unsigned long pe_counter;

	if (!hom_ops)
		return -ENOSYS;

	pr_debug("[STM]:[PM]: platform_begin\n");
	error = platform_begin();
	if (error)
		goto Close;

	suspend_console();

	pr_debug("[STM]:[PM]: Suspend devices\n");
	error = dpm_suspend_start(PMSG_FREEZE);
	if (error)
		goto Resume_devices;

	pr_debug("[STM]:[PM]: Suspend devices (noirq)\n");
	error = dpm_suspend_noirq(PMSG_FREEZE);
	if (error)
		goto Resume_devices_noirq;

	error = disable_nonboot_cpus();
	if (error)
		goto Enable_cpus;

	local_irq_disable();

	pr_debug("[STM]:[PM]: Suspend sysdevices\n");
	error = sysdev_suspend(PMSG_FREEZE);
	if (error)
		goto Enable_irqs;

	pr_debug("[STM]:[PM]: platform_prepare\n");
	error = platform_prepare();
	if (error) {
		pr_err("[STM]:[PM]: platform_prepare has refused the HoM\n");
		goto Skip_enter;
	}

	pr_debug("[STM]:[PM]: platform_enter\n");


	pe_counter = preempt_count();

	platform_enter();

	BUG_ON(pe_counter != preempt_count());

 Skip_enter:
	pr_debug("[STM]:[PM]: platform_complete\n");
	platform_complete();

	pr_debug("[STM]:[PM]: Resumed sysdevices\n");
	sysdev_resume();

 Enable_irqs:
	local_irq_enable();

 Enable_cpus:
	enable_nonboot_cpus();

 Resume_devices_noirq:

	pr_debug("[STM]:[PM]: Resume devices (noirq)\n");
	dpm_resume_noirq(PMSG_RESTORE);

 Resume_devices:

	pr_debug("[STM]:[PM]: Resume devices\n");
	dpm_resume_end(PMSG_RESTORE);

	resume_console();

 Close:
	pr_debug("[STM]:[PM]: platform_end\n");
	platform_end();

	pr_debug("[STM]:[PM]: exit\n");
	return error;
}

/**
 *	hom_finish - Do final work before exiting suspend sequence.
 *
 *	Call platform code to clean up, restart processes, and free the
 *	console that we've allocated. This is not called for suspend-to-disk.
 */
static void hom_finish(void)
{
	pr_debug("%s\n", __func__);

	thaw_processes();

	usermodehelper_enable();

	pm_notifier_call_chain(PM_POST_HIBERNATION);
	pm_restore_console();
}

static int hom_enter(void)
{
	int err;

	mutex_lock(&pm_mutex);

	err = hom_prepare();
	if (err)
		goto Finish;

	err = hibernation_on_memory_enter();

	hom_finish();

Finish:
	mutex_unlock(&pm_mutex);

	pr_info("[STM]:[PM]: HoM End...\n");
	return err;
}

int hibernate_on_memory(void)
{
	return hom_enter();
}
EXPORT_SYMBOL(hibernate_on_memory);
