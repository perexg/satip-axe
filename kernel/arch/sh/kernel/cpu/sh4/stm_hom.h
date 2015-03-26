/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#ifndef __STM_MEM_HIBERNATION_H__
#define __STM_MEM_HIBERNATION_H__

#include <linux/hom.h>

struct stm_mem_hibernation {
	long flags;
	long tbl_addr;
	long tbl_size;
	struct platform_hom_ops ops;
	void (*early_console)(void);
};

int stm_hom_register(struct stm_mem_hibernation *platform);

void stm_hom_exec_table(unsigned int tbl, unsigned int tbl_end,
		unsigned long lpj, unsigned int flags);


void stm_defrost_kernel(void);

int stm_freeze_board(void *data);
int stm_defrost_board(void *data);

void hom_printk(char *buf, ...);

#endif
