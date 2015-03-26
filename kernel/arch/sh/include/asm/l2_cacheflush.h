#ifndef __ASM_SH_L2_CACHEFLUSH_H
#define __ASM_SH_L2_CACHEFLUSH_H

#if defined(CONFIG_STM_L2_CACHE)

#include <asm/stm-l2-cache.h>

static inline void __l2_flush_wback_region(void *start, int size)
{
	stm_l2_flush_wback((unsigned long)start, size, 0);
}

static inline void __l2_flush_purge_region(void *start, int size)
{
	stm_l2_flush_purge((unsigned long)start, size, 0);
}

static inline void __l2_flush_invalidate_region(void *start, int size)
{
	stm_l2_flush_invalidate((unsigned long)start, size, 0);
}

#define __l2_flush_wback_phys(start, size) \
		stm_l2_flush_wback(start, size, 1)

#define __l2_flush_purge_phys(start, size) \
		stm_l2_flush_purge(start, size, 1)

#define __l2_flush_invalidate_phys(start, size) \
		stm_l2_flush_invalidate(start, size, 1)

#else

static inline void __l2_flush_wback_region(void *start, int size)
{
}

static inline void __l2_flush_purge_region(void *start, int size)
{
}

static inline void __l2_flush_invalidate_region(void *start, int size)
{
}

static inline void __l2_flush_wback_phys(unsigned long start, int size)
{
}

static inline void __l2_flush_purge_phys(unsigned long start, int size)
{
}

static inline void __l2_flush_invalidate_phys(unsigned long start, int size)
{
}

#endif

#endif
