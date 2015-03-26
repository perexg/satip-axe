/*
 * STMicroelectronics clock framework
 *
 *  Copyright (C) 2009, STMicroelectronics
 *  Copyright (C) 2010, STMicroelectronics
 *  Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_STM_CLK_H__
#define __ASM_STM_CLK_H__

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/spinlock.h>

struct clk;

struct clk_ops {
	int (*init)(struct clk *clk);
	int (*enable)(struct clk *clk);
	int (*disable)(struct clk *clk);
	int (*set_rate)(struct clk *clk, unsigned long rate);
	int (*set_parent)(struct clk *clk, struct clk *parent);
	int (*recalc)(struct clk *clk);
	long (*round_rate)(struct clk *clk, unsigned long rate);
	int (*observe)(struct clk *clk, unsigned long *div);
	unsigned long (*get_measure)(struct clk *clk);
};

struct clk {
	struct list_head	node;

	const char		*name;
	int			id;

	struct clk		*parent;
	struct clk_ops		*ops;

	void			*private_data;

	struct kref		kref;

	unsigned long		usage_counter;

	unsigned long		rate;
	unsigned long		flags;

	struct list_head	children;
	struct list_head	children_node;
};

#define CLK_ALWAYS_ENABLED	(1 << 0)

/* drivers/stm/clks/clock-... */

/* SoC specific clock initialisation */
int plat_clk_init(void);
int plat_clk_alias_init(void);

/* drivers/stm/clk.c */

/* Additions to the standard clk_... functions in include/linux/clk.h */

int clk_register(struct clk *);
void clk_unregister(struct clk *);

int clk_for_each(int (*fn)(struct clk *clk, void *data), void *data);
int clk_for_each_child(struct clk *clk,
		int (*fn)(struct clk *clk, void *data), void *data);

static inline void recalculate_root_clocks(void)
{}

static inline void clk_enable_init_clocks(void)
{}


/**
 * clk_observe - route the clock to an external pin (if possible)
 * @clk: clock source
 * @div: divider
 *
 * To help with debugging clock related code, it is sometimes possible
 * to route the clock directly to an external pin where it can be
 * observed, typically with an oscilloscope.
 *
 * To allow high frequency clocks to be observed it is sometimes
 * possible to divide the clock by a fixed amount before it is sent
 * to an external pin. This factor is specified by @div, which is also
 * used to return the actual divider used, which may not be the requested
 * factor.
 */
int clk_observe(struct clk *clk, unsigned long *div);

/**
 * clk_get_measure - evaluate the clock rate in hardware (if possible)
 * @clk: clock source
 *
 * Measure the frequency of the specified clock and return it.
 */
unsigned long clk_get_measure(struct clk *clk);

#endif /* __ASM_STM_CLOCK_H */
