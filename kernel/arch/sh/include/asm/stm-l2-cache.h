/*
 * Copyright (C) 2009 STMicroelectronics
 * Written by Richard P. Curnow
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef __ASM_SH_STM_L2_CACHE_H
#define __ASM_SH_STM_L2_CACHE_H

/* This API can take either virtual or physical address...
 * The "is_phys" parameter gives this information... */

void stm_l2_flush_wback(unsigned long start, int size, int is_phys);
void stm_l2_flush_purge(unsigned long start, int size, int is_phys);
void stm_l2_flush_invalidate(unsigned long start, int size, int is_phys);

#endif /* __ASM_SH_STM_L2_CACHE_H */
