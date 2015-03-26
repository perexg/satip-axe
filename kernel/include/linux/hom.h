/*
 *  hom.h - Hibernation on memory interface
 *
 *  Copyright (C) 2010 STMicroelectronics
 *  Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 */
#ifndef __LINUX_HOM__
#define __LINUX_HOM__

/**
 *
 * struct platform_hom_ops
 *
 * @begin:	called __before__ the device are frozen
 * @prepare:	called __after__ the device are frozen but before the
 *		core (cpu + Dram) are frozen
 * @enter:	this function does __all__ the CPU + DRAM job
 *		the system returns when __resumed__
 * @complete:	called when the ente returns but before the devices are
 *		resumed
 * @end:	called when the devices are resumed
 */
struct platform_hom_ops {
	int (*begin)(void);
	int (*prepare)(void);
	int (*enter)(void);
	int (*complete)(void);
	int (*end)(void);
};

#ifdef CONFIG_HIBERNATION_ON_MEMORY

int hom_set_ops(struct platform_hom_ops *hom_ops);

int hibernate_on_memory(void);

#else

static inline int hom_set_ops(struct platform_hom_ops *hom_ops) { return 0; }

static inline int hibernate_on_memory(void) { return 0; }

#endif /* CONFIG_HIBERNATION_ON_MEMORY */

#endif /* __LINUX_HOM____ */
