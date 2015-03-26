/*
 * -------------------------------------------------------------------------
 * <linux_root>/drivers/stm/pm_notify.c
 * -------------------------------------------------------------------------
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#include <linux/irqflags.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/stm/pm_notify.h>

static DEFINE_MUTEX(pm_notify_mutex);
static struct stm_pm_notify *stm_notifier;

enum stm_pm_notify_return
stm_pm_prepare_enter(enum stm_pm_type type)
{
	if (stm_notifier && stm_notifier->pre_enter)
		return stm_notifier->pre_enter(type);

	return STM_PM_RET_OK;
}

enum stm_pm_notify_return
stm_pm_post_enter(enum stm_pm_type type, int wkirq)
{
	if (stm_notifier && stm_notifier->post_enter)
		return stm_notifier->post_enter(type, wkirq);

	return STM_PM_RET_OK;
}

int stm_register_pm_notify(struct stm_pm_notify *notify)
{
	if (stm_notifier || !notify)
		return -EINVAL;

	mutex_lock(&pm_notify_mutex);
	stm_notifier = notify;
	mutex_unlock(&pm_notify_mutex);

	return 0;
}
EXPORT_SYMBOL(stm_register_pm_notify);
