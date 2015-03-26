/*
 * -------------------------------------------------------------------------
 * <linux_root>/include/linux/stm/pm_notify.h
 * -------------------------------------------------------------------------
 * STMicroelectronics
 * -------------------------------------------------------------------------
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#ifndef __STM_LINUX_NOTIFY__
#define __STM_LINUX_NOTIFY__

enum stm_pm_type {
	STM_PM_SUSPEND,
	STM_PM_MEMSUSPEND,
	STM_PM_HIBERNATION,
};

enum stm_pm_notify_return {
	STM_PM_RET_OK,
	STM_PM_RET_ERROR,
	STM_PM_RET_AGAIN,
};

struct stm_pm_notify {

	enum stm_pm_notify_return (*pre_enter)
		(enum stm_pm_type type);

	enum stm_pm_notify_return (*post_enter)
		(enum stm_pm_type type, int wkirq);
};

enum stm_pm_notify_return
stm_pm_prepare_enter(enum stm_pm_type type);

enum stm_pm_notify_return
stm_pm_post_enter(enum stm_pm_type type, int wkirq);

int stm_register_pm_notify(struct stm_pm_notify *notify);

#endif
