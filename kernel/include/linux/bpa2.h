/*
 * Copyright (c) STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Derived from mm/bigphysarea.c which was:
 * Copyright (c) 1996 by Matt Welsh.
 * Extended by Roger Butenuth (butenuth@uni-paderborn.de), October 1997
 * Extended for linux-2.1.121 till 2.4.0 (June 2000)
 *     by Pauline Middelink <middelink@polyware.nl>
 *
 * This is a set of routines which allow you to reserve a large (?)
 * amount of physical memory at boot-time, which can be allocated/deallocated
 * by drivers. This memory is intended to be used for devices such as
 * video framegrabbers which need a lot of physical RAM (above the amount
 * allocated by kmalloc). This is by no means efficient or recommended;
 * to be used only in extreme circumstances.
 */

#ifndef __LINUX_BPA2_H
#define __LINUX_BPA2_H

#include <linux/types.h>



/*
 * BPA2 Interface
 */

#define BPA2_NORMAL    0x00000001

struct bpa2_partition_desc {
	const char *name;
	unsigned long start;
	unsigned long size;
	unsigned long flags;
	const char **aka;
};

struct bpa2_part;

void bpa2_init(struct bpa2_partition_desc *partdescs, int nparts);

struct bpa2_part *bpa2_find_part(const char *name);
int bpa2_low_part(struct bpa2_part *part);

#if defined(CONFIG_BPA2_ALLOC_TRACE)
#define bpa2_alloc_pages(part, count, align, priority) \
		__bpa2_alloc_pages(part, count, align, priority, \
		__FILE__, __LINE__)
#else
#define bpa2_alloc_pages(part, count, align, priority) \
		__bpa2_alloc_pages(part, count, align, priority, NULL, 0)
#endif
unsigned long __bpa2_alloc_pages(struct bpa2_part *part, int count, int align,
	       int priority, const char *trace_file, int trace_line);
void bpa2_free_pages(struct bpa2_part *part, unsigned long base);

void bpa2_memory(struct bpa2_part *part, unsigned long *base,
		 unsigned long *size);

/*
 * Backward compatibility interface (bigphysarea)
 */

/* Original interface */
#define bigphysarea_alloc(size) \
		bigphysarea_alloc_pages(PAGE_ALIGN(size) >> PAGE_SHIFT, \
		1, GFP_KERNEL)
#define bigphysarea_free(addr, size) \
		bigphysarea_free_pages(addr)

/* New(er) interface */
#if defined(CONFIG_BPA2_ALLOC_TRACE)
#define bigphysarea_alloc_pages(count, align, priority) \
		__bigphysarea_alloc_pages(count, align, priority, \
		__FILE__, __LINE__)
#else
#define bigphysarea_alloc_pages(count, align, priority) \
		__bigphysarea_alloc_pages(count, align, priority, NULL, 0)
#endif
caddr_t	__bigphysarea_alloc_pages(int count, int align, int priority,
		const char *trace_file, int trace_line);
void bigphysarea_free_pages(caddr_t base);

/* low level interface */
void     bigphysarea_memory(unsigned long *base, unsigned long *size);

#endif /* __LINUX_BPA2_H */
