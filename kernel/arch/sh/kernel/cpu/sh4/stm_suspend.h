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
#ifndef __STM_SUSPEND_h__
#define __STM_SUSPEND_h__

#include <linux/suspend.h>

#define NO_SLEEP_ON_STANDBY		1
#define NO_SLEEP_ON_MEMSTANDBY		2
#define EARLY_ACTION_ON_STANDBY		4
#define EARLY_ACTION_ON_MEMSTANDBY	8

struct stm_platform_suspend_t {
	long flags;
	long stby_tbl;
	long stby_size;
	long mem_tbl;
	long mem_size;
	int (*evt_to_irq)(unsigned long evt);
	int (*pre_enter)(suspend_state_t state);
	int (*post_enter)(suspend_state_t state);
	struct platform_suspend_ops ops;
};

int stm_suspend_register(struct stm_platform_suspend_t *platform);

void stm_exec_table(unsigned int tbl, unsigned int tbl_end,
		unsigned long lpj, unsigned int flags);

#endif
