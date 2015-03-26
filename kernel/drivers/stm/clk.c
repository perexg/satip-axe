/*
 * STMicroelectronics clock framework
 *
 *  Copyright (C) 2009, STMicroelectronics
 *  Copyright (C) 2010, STMicroelectronics
 *  Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License V2 _ONLY_.  See the file "COPYING" in the main directory of
 * this archive for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/stm/clk.h>

static LIST_HEAD(clks_list);
static DEFINE_MUTEX(clks_list_sem);
static DEFINE_SPINLOCK(clock_lock);

static int __clk_init(struct clk *clk)
{
	int ret = 0;

	if (likely(clk->ops && clk->ops->init))
		ret = clk->ops->init(clk);

	return ret;
}

static int __clk_enable(struct clk *clk)
{
	int ret = 0;

	if (likely(clk->ops && clk->ops->enable))
		ret = clk->ops->enable(clk);

	return ret;
}

static int __clk_disable(struct clk *clk)
{
	int ret = 0;

	if (likely(clk->ops && clk->ops->disable))
		ret = clk->ops->disable(clk);

	return ret;
}

static int __clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = 0;

	if (likely(clk->ops && clk->ops->set_rate))
		ret = clk->ops->set_rate(clk, rate);

	return ret;
}

static int __clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = 0;

	if (likely(clk->ops && clk->ops->set_parent))
		ret = clk->ops->set_parent(clk, parent);

	return ret;
}

static int __clk_recalc_rate(struct clk *clk)
{
	int ret = 0;

	if (likely(clk->ops && clk->ops->recalc))
		ret = clk->ops->recalc(clk);

	return ret;
}

static int __clk_observe(struct clk *clk, unsigned long *div)
{
	int ret = 0;

	if (likely(clk->ops && clk->ops->observe))
		ret = clk->ops->observe(clk, div);

	return ret;
}

static int __clk_get_measure(struct clk *clk)
{
	int ret = 0;

	if (likely(clk->ops && clk->ops->get_measure))
		ret = clk->ops->get_measure(clk);

	return ret;
}

static inline int clk_is_always_enabled(struct clk *clk)
{
	return clk->flags & CLK_ALWAYS_ENABLED;
}

static int __clk_for_each_child(struct clk *clk,
		int (*fn)(struct clk *clk, void *data), void *data)
{
	struct clk *clkp;
	int result = 0;

	BUG_ON(!fn || !clk);

	list_for_each_entry(clkp, &clk->children, children_node) {
		result = fn(clkp, data);
		if (result)
			break;
	}

	return result;
}

static void clk_propagate(struct clk *clk);

static int __clk_propagate_rate(struct clk *clk, void *data)
{
	__clk_recalc_rate(clk);

	clk_propagate(clk);

	return 0;
}

static void clk_propagate(struct clk *clk)
{
	__clk_for_each_child(clk, __clk_propagate_rate, NULL);
}


static void _clk_disable(struct clk *clk);

int _clk_enable(struct clk *clk)
{
	int ret = 0;

	if (clk->usage_counter++ == 0) {
		if (clk->parent)
			_clk_enable(clk->parent);
		ret = __clk_enable(clk);
		if (ret) { /* on error */
			if (clk->parent)
				_clk_disable(clk->parent);
			--clk->usage_counter;
		} else
			clk_propagate(clk);
	}

	return ret;
}

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret;

	if (!clk)
		return -EINVAL;

	spin_lock_irqsave(&clock_lock, flags);
	ret = _clk_enable(clk);
	spin_unlock_irqrestore(&clock_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_enable);

void _clk_disable(struct clk *clk)
{
	int ret;

	if (clk_is_always_enabled(clk)) {
		/*
		 * this clock can not be disabled;
		 * This means this is:
		 * - an external oscillator
		 * - a system critical clock
		 */
		if (--clk->usage_counter == 0)
			clk->usage_counter = 1;
		return;
	}

	if (--clk->usage_counter == 0) {
		ret = __clk_disable(clk);
		if (ret) {/* on error */
			clk->usage_counter++;
			return;
		}
		clk_propagate(clk);
		if (clk->parent)
			_clk_disable(clk->parent);
	}
	return;
}

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	if (!clk)
		return;

	spin_lock_irqsave(&clock_lock, flags);
	_clk_disable(clk);
	spin_unlock_irqrestore(&clock_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

int clk_register(struct clk *clk)
{
	BUG_ON(!clk || !clk->name);

	mutex_lock(&clks_list_sem);

	list_add_tail(&clk->node, &clks_list);
	INIT_LIST_HEAD(&clk->children);

	clk->usage_counter = 0;

	__clk_init(clk);

	if (clk->parent)
		list_add_tail(&clk->children_node, &clk->parent->children);

	kref_init(&clk->kref);

	mutex_unlock(&clks_list_sem);

	if (clk_is_always_enabled(clk))
		clk_enable(clk);

	return 0;
}
EXPORT_SYMBOL(clk_register);

void clk_unregister(struct clk *clk)
{
	BUG_ON(!clk);

	mutex_lock(&clks_list_sem);
	list_del(&clk->node);
	if (clk->parent)
		list_del(&clk->children_node);
	mutex_unlock(&clks_list_sem);
}
EXPORT_SYMBOL(clk_unregister);

unsigned long clk_get_rate(struct clk *clk)
{
	BUG_ON(!clk);
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&clock_lock, flags);

	ret = __clk_set_rate(clk, rate);

	if (!ret)
		clk_propagate(clk);

	spin_unlock_irqrestore(&clock_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (likely(clk->ops && clk->ops->round_rate)) {
		unsigned long flags, rounded;

		spin_lock_irqsave(&clock_lock, flags);
		rounded = clk->ops->round_rate(clk, rate);
		spin_unlock_irqrestore(&clock_lock, flags);

		return rounded;
	}

	return clk_get_rate(clk);
}
EXPORT_SYMBOL(clk_round_rate);

struct clk *clk_get_parent(struct clk *clk)
{
	BUG_ON(!clk);
	return clk->parent;
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	unsigned long flags;
	int ret = -EINVAL;
	struct clk *old_parent;
	unsigned long old_rate;

	if (!parent || !clk)
		return 0;

	if (parent == clk_get_parent(clk))
		return 0;

	spin_lock_irqsave(&clock_lock, flags);
	old_parent = clk_get_parent(clk);
	old_rate = clk_get_rate(clk);

	if (old_rate)
		/* enable the new parent if required */
		_clk_enable(parent);

	ret = __clk_set_parent(clk, parent);

	/* update the parent field */
	clk->parent = (ret ? old_parent : parent);

	if (old_rate)
		/* notify to the parent the 'disable' clock */
		_clk_disable(ret ? parent : old_parent);

	/* propagate if required */
	if (!ret)
		clk_propagate(clk);

	spin_unlock_irqrestore(&clock_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_set_parent);

int clk_observe(struct clk *clk, unsigned long *div)
{
	BUG_ON(!clk);
	return __clk_observe(clk, div);
}
EXPORT_SYMBOL(clk_observe);

unsigned long clk_get_measure(struct clk *clk)
{
	BUG_ON(!clk);
	return __clk_get_measure(clk);
}
EXPORT_SYMBOL(clk_get_measure);

int clk_for_each(int (*fn)(struct clk *clk, void *data), void *data)
{
	struct clk *clkp;
	int result = 0;

	if (!fn)
		return -EINVAL;

	mutex_lock(&clks_list_sem);
	list_for_each_entry(clkp, &clks_list, node) {
		result = fn(clkp, data);
		if (result)
			break;
	}
	mutex_unlock(&clks_list_sem);

	return result;
}
EXPORT_SYMBOL(clk_for_each);

int clk_for_each_child(struct clk *clk, int (*fn)(struct clk *clk, void *data),
		void *data)
{
	int ret = 0;

	mutex_lock(&clks_list_sem);
	ret = __clk_for_each_child(clk, fn, data);
	mutex_unlock(&clks_list_sem);

	return ret;
}
EXPORT_SYMBOL(clk_for_each_child);

#ifdef CONFIG_PROC_FS
static void *clk_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return seq_list_next(v, &clks_list, pos);
}

static void *clk_seq_start(struct seq_file *s, loff_t *pos)
{
	mutex_lock(&clks_list_sem);
	return seq_list_start(&clks_list, *pos);
}

static void clk_seq_show_clk(struct seq_file *s, struct clk *clk, int depth)
{
	unsigned long rate = clk_get_rate(clk);
	struct clk *child_clk;

	seq_printf(s, "%*s%-*s: %4ld.%02ldMHz",
		depth*2, "", 30-(depth*2), clk->name,
		rate / 1000000, (rate % 1000000) / 10000);
	if (clk_is_always_enabled(clk))
		seq_printf(s, " always enabled");
	else
		seq_printf(s, " users=%ld", clk->usage_counter);
	seq_printf(s, "\n");

	list_for_each_entry(child_clk, &clk->children, children_node)
		clk_seq_show_clk(s, child_clk, depth+1);
}

static int clk_seq_show(struct seq_file *s, void *v)
{
	struct clk *clk = list_entry(v, struct clk, node);

	if (!clk->parent)
		clk_seq_show_clk(s, clk, 0);

	return 0;
}

static void clk_seq_stop(struct seq_file *s, void *v)
{
	mutex_unlock(&clks_list_sem);
}

static const struct seq_operations clk_seq_ops = {
	.start = clk_seq_start,
	.next = clk_seq_next,
	.stop = clk_seq_stop,
	.show = clk_seq_show,
};

static int clk_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &clk_seq_ops);
}

static const struct file_operations clk_proc_ops = {
	.owner = THIS_MODULE,
	.open = clk_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __init clk_proc_init(void)
{
	struct proc_dir_entry *p;

	p = create_proc_entry("clocks", S_IRUGO, NULL);

	if (unlikely(!p))
		return -EINVAL;

	p->proc_fops = &clk_proc_ops;

	return 0;
}
module_init(clk_proc_init);
#endif

#ifdef CONFIG_HIBERNATION
static void clk_resume_from_hibernation(struct clk *clk)
{
	unsigned long rate = clk->rate;
	struct clk *old_parent = clk->parent;

	if (clk->parent)
		/* re-parent to the frozen one! */
		__clk_set_parent(clk, clk->parent);

	if (rate) {
		__clk_enable(clk);
		/* force the frozen rate */
		__clk_set_rate(clk, rate);
	} else
		__clk_disable(clk);

	if (clk_get_rate(clk) != rate)
		pr_warning("[STM][CLK]: %s wrong final rate (%u/%u)\n",
			clk->name, clk_get_rate(clk), rate);
	if (clk_get_parent(clk) != old_parent)
		pr_warning("[STM][CLK]: %s wrong final parent (%u/%u)\n",
			clk->name, clk_get_parent(clk), old_parent);
}

static int clks_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	static pm_message_t prev_state;
	struct clk *clkp;

	if (state.event == PM_EVENT_ON && prev_state.event == PM_EVENT_FREEZE)
		list_for_each_entry(clkp, &clks_list, node)
			clk_resume_from_hibernation(clkp);

	prev_state = state;
	return 0;
}

static int clks_sysdev_resume(struct sys_device *dev)
{
	return clks_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_class clk_sysdev_class = {
	.name = "clks",
	.suspend = clks_sysdev_suspend,
	.resume = clks_sysdev_resume,
};

static struct sys_device clks_sysdev_dev = {
	.id = 0,
	.cls = &clk_sysdev_class,
};

static int __init clk_sysdev_init(void)
{
	int ret;
	ret = sysdev_class_register(&clk_sysdev_class);
	if (ret)
		return ret;

	ret = sysdev_register(&clks_sysdev_dev);
	if (ret)
		return ret;

	return 0;
};

module_init(clk_sysdev_init);

#endif
