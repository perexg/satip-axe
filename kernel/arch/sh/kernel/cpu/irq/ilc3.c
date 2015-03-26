/*
 * linux/arch/sh/kernel/cpu/irq/ilc3.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interrupts routed through the Interrupt Level Controller (ILC3)
 */

#include <linux/kernel.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/stm/platform.h>
#include <linux/io.h>

#include <asm/hw_irq.h>
#include <asm/system.h>
#include <asm/irq-ilc.h>

#include "ilc3.h"

#define DRIVER_NAME "ilc3"



struct ilc_irq {
#define ilc_get_priority(_ilc)		((_ilc)->priority)
#define ilc_set_priority(_ilc, _prio)	((_ilc)->priority = (_prio))
	unsigned char priority;

#define ILC_STATE_USED			0x1
#define ILC_WAKEUP_ENABLED		0x2
#define ILC_ENABLED			0x4

#define ilc_is_used(_ilc)		(((_ilc)->state & ILC_STATE_USED) != 0)
#define ilc_set_used(_ilc)		((_ilc)->state |= ILC_STATE_USED)
#define ilc_set_unused(_ilc)		((_ilc)->state &= ~(ILC_STATE_USED))

#define ilc_set_wakeup(_ilc)		((_ilc)->state |= ILC_WAKEUP_ENABLED)
#define ilc_reset_wakeup(_ilc)		((_ilc)->state &= ~ILC_WAKEUP_ENABLED)
#define ilc_wakeup_enabled(_ilc)  (((_ilc)->state & ILC_WAKEUP_ENABLED) != 0)

#define ilc_set_enabled(_ilc)		((_ilc)->state |= ILC_ENABLED)
#define ilc_set_disabled(_ilc)		((_ilc)->state &= ~ILC_ENABLED)
#define ilc_is_enabled(_ilc)		(((_ilc)->state & ILC_ENABLED) != 0)
	unsigned char state;

	unsigned char trigger_mode; /* used to restore the right mode
				     * after a resume from hibernation */
};

struct ilc {
	struct list_head list;
	const char *name;
	void *base;
	struct clk *clk;
	unsigned short inputs_num, outputs_num;
	unsigned int first_irq;
	int disable_wakeup:1;
	spinlock_t lock;
	struct sys_device sysdev;
	struct ilc_irq *irqs;
#ifdef CONFIG_HIBERNATION
	pm_message_t state;
#endif
	unsigned long **priority;
};

static LIST_HEAD(ilcs_list);

static struct sysdev_class ilc_sysdev_class;

#define sysdev_to_ilc(x) container_of((x), struct ilc, sysdev)


/*
 * Debug printk macro
 */

/* #define ILC_DEBUG */
/* #define ILC_DEBUG_DEMUX */

#ifdef ILC_DEBUG
#define DPRINTK(fmt, args...)		\
	printk(KERN_INFO"%s: " fmt, __func__, ## args)
#else
#define DPRINTK(args...)
#endif

/*
 * Beware this one; the ASC has ILC ints too...
 */

#ifdef ILC_DEBUG_DEMUX
#define DPRINTK2(args...)   		\
	printk(KERN_INFO"%s: " fmt, __func__, ## args)
#else
#define DPRINTK2(args...)
#endif



/*
 * From evt2irq to ilc2irq
 */
int ilc2irq(unsigned int evtcode)
{
	struct ilc *ilc = get_irq_data(evt2irq(evtcode));
#if	defined(CONFIG_CPU_SUBTYPE_STX5206) || \
	defined(CONFIG_CPU_SUBTYPE_STX7108) || \
	defined(CONFIG_CPU_SUBTYPE_STX7111) || \
	defined(CONFIG_CPU_SUBTYPE_STX7141)
	unsigned int priority = 7;
#elif	defined(CONFIG_CPU_SUBTYPE_FLI7510) || \
	defined(CONFIG_CPU_SUBTYPE_STX5197) || \
	defined(CONFIG_CPU_SUBTYPE_STX7105) || \
	defined(CONFIG_CPU_SUBTYPE_STX7200)
	unsigned int priority = 14 - evt2irq(evtcode);
#endif
	unsigned long status;
	int idx;

	for (idx = 0, status = 0;
	     idx < DIV_ROUND_UP(ilc->inputs_num, 32) && !status;
	     ++idx)
		status = readl(ilc->base + ILC_BASE_STATUS + (idx << 2)) &
			readl(ilc->base + ILC_BASE_ENABLE + (idx << 2)) &
			ilc->priority[priority][idx];

	if (!status)
		return -1;

	return ilc->first_irq + ((idx-1) * 32) + (ffs(status) - 1);
}

/*
 * The interrupt demux function. Check if this was an ILC interrupt, and
 * if so which device generated the interrupt.
 */
void ilc_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	struct ilc *ilc = get_irq_data(irq);
#if	defined(CONFIG_CPU_SUBTYPE_STX5206) || \
	defined(CONFIG_CPU_SUBTYPE_STX7108) || \
	defined(CONFIG_CPU_SUBTYPE_STX7111) || \
	defined(CONFIG_CPU_SUBTYPE_STX7141)
	unsigned int priority = 7;
#elif	defined(CONFIG_CPU_SUBTYPE_FLI7510) || \
	defined(CONFIG_CPU_SUBTYPE_STX5197) || \
	defined(CONFIG_CPU_SUBTYPE_STX7105) || \
	defined(CONFIG_CPU_SUBTYPE_STX7200)
	unsigned int priority = 14 - irq;
#endif
	int handled = 0;
	int idx;

	DPRINTK2("%s: irq %d\n", __func__, irq);

	for (idx = 0; idx < DIV_ROUND_UP(ilc->inputs_num, 32); ++idx) {
		unsigned long status;
		unsigned int input;
		struct irq_desc *desc;

		status = readl(ilc->base + ILC_BASE_STATUS + (idx << 2)) &
			readl(ilc->base + ILC_BASE_ENABLE + (idx << 2)) &
			ilc->priority[priority][idx];
		if (!status)
			continue;

		input = (idx * 32) + ffs(status) - 1;
		desc = irq_desc + input + ilc->first_irq;
		desc->handle_irq(ilc->first_irq + input, desc);
		handled = 1;
		ILC_CLR_STATUS(ilc->base, input);
	}

	if (likely(handled))
		return;

	atomic_inc(&irq_err_count);

	printk(KERN_INFO "ILC: spurious interrupt demux %d\n", irq);

	printk(KERN_DEBUG "ILC:  inputs   status  enabled    used\n");

	for (idx = 0; idx < DIV_ROUND_UP(ilc->inputs_num, 32); ++idx) {
		unsigned long status, enabled, used;

		status = readl(ilc->base + ILC_BASE_STATUS + (idx << 2));
		enabled = readl(ilc->base + ILC_BASE_ENABLE + (idx << 2));
		used = 0;
		for (priority = 0; priority < ilc->outputs_num; ++priority)
			used |= ilc->priority[priority][idx];

		printk(KERN_DEBUG "ILC: %3d-%3d: %08lx %08lx %08lx"
				"\n", idx * 32, (idx * 32) + 31,
				status, enabled, used);
	}
}

static unsigned int startup_ilc_irq(unsigned int irq)
{
	struct ilc *ilc = get_irq_chip_data(irq);
	unsigned int priority;
	unsigned long flags;
	int input = irq - ilc->first_irq;
	struct ilc_irq *ilc_irq;

	DPRINTK("%s: irq %d\n", __func__, irq);

	if ((input < 0) || (input >= ilc->inputs_num))
		return -ENODEV;

	ilc_irq = &ilc->irqs[input];
	priority = ilc_irq->priority;

	spin_lock_irqsave(&ilc->lock, flags);
	ilc_set_used(ilc_irq);
	ilc_set_enabled(ilc_irq);
	ilc->priority[priority][_BANK(input)] |= _BIT(input);
	spin_unlock_irqrestore(&ilc->lock, flags);

#if	defined(CONFIG_CPU_SUBTYPE_STX5206) || \
	defined(CONFIG_CPU_SUBTYPE_STX7111)
	/* ILC_EXT_OUT[4] -> IRL[0] (default priority 13 = irq  2) */
	/* ILC_EXT_OUT[5] -> IRL[1] (default priority 10 = irq  5) */
	/* ILC_EXT_OUT[6] -> IRL[2] (default priority  7 = irq  8) */
	/* ILC_EXT_OUT[7] -> IRL[3] (default priority  4 = irq 11) */
	ILC_SET_PRI(ilc->base, input, 0x8007);
#elif	defined(CONFIG_CPU_SUBTYPE_FLI7510) || \
	defined(CONFIG_CPU_SUBTYPE_STX5197) || \
	defined(CONFIG_CPU_SUBTYPE_STX7105) || \
	defined(CONFIG_CPU_SUBTYPE_STX7200)
	ILC_SET_PRI(ilc->base, input, priority);
#elif	defined(CONFIG_CPU_SUBTYPE_STX7108) || \
	defined(CONFIG_CPU_SUBTYPE_STX7141)
	ILC_SET_PRI(ilc->base, input, 0x0);
#endif

	ILC_SET_ENABLE(ilc->base, input);

	return 0;
}

static void shutdown_ilc_irq(unsigned int irq)
{
	struct ilc *ilc = get_irq_chip_data(irq);
	struct ilc_irq *ilc_irq;
	unsigned int priority;
	unsigned long flags;
	int input = irq - ilc->first_irq;

	DPRINTK("%s: irq %d\n", __func__, irq);

	WARN_ON(!ilc_is_used(&ilc->irqs[input]));

	if ((input < 0) || (input >= ilc->inputs_num))
		return;

	ilc_irq = &ilc->irqs[input];
	priority = ilc_irq->priority;

	ILC_CLR_ENABLE(ilc->base, input);
	ILC_SET_PRI(ilc->base, input, 0);

	spin_lock_irqsave(&ilc->lock, flags);
	ilc_set_disabled(ilc_irq);
	ilc_set_unused(ilc_irq);
	ilc->priority[priority][_BANK(input)] &= ~(_BIT(input));
	spin_unlock_irqrestore(&ilc->lock, flags);
}

static void unmask_ilc_irq(unsigned int irq)
{
	struct ilc *ilc = get_irq_chip_data(irq);
	int input = irq - ilc->first_irq;
	struct ilc_irq *ilc_irq = &ilc->irqs[input];

	DPRINTK2("%s: irq %d\n", __func__, irq);

	ILC_SET_ENABLE(ilc->base, input);
	ilc_set_enabled(ilc_irq);
}

static void mask_ilc_irq(unsigned int irq)
{
	struct ilc *ilc = get_irq_chip_data(irq);
	int input = irq - ilc->first_irq;
	struct ilc_irq *ilc_irq = &ilc->irqs[input];

	DPRINTK2("%s: irq %d\n", __func__, irq);

	ILC_CLR_ENABLE(ilc->base, input);
	ilc_set_disabled(ilc_irq);
}

static void mask_and_ack_ilc(unsigned int irq)
{
	struct ilc *ilc = get_irq_chip_data(irq);
	int input = irq - ilc->first_irq;
	struct ilc_irq *ilc_irq = &ilc->irqs[input];

	DPRINTK2("%s: irq %d\n", __func__, irq);

	ILC_CLR_ENABLE(ilc->base, input);
	(void)ILC_GET_ENABLE(ilc->base, input); /* Defeat write posting */
	ilc_set_disabled(ilc_irq);
}

static int set_type_ilc_irq(unsigned int irq, unsigned int flow_type)
{
	struct ilc *ilc = get_irq_chip_data(irq);
	int input = irq - ilc->first_irq;
	int mode;

	switch (flow_type) {
	case IRQ_TYPE_EDGE_RISING:
		mode = ILC_TRIGGERMODE_RISING;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		mode = ILC_TRIGGERMODE_FALLING;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		mode = ILC_TRIGGERMODE_ANY;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		mode = ILC_TRIGGERMODE_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		mode = ILC_TRIGGERMODE_LOW;
		break;
	default:
		return -EINVAL;
	}

	ILC_SET_TRIGMODE(ilc->base, input, mode);
	ilc->irqs[input].trigger_mode = (unsigned char)mode;

	return 0;
}

#ifdef CONFIG_SUSPEND
static int set_wake_ilc_irq(unsigned int irq, unsigned int on)
{
	struct ilc *ilc = get_irq_chip_data(irq);
	int input = irq - ilc->first_irq;
	struct ilc_irq *ilc_irq = &ilc->irqs[input];

	if (ilc->disable_wakeup)
		return 0;

	if (on) {
		ilc_set_wakeup(ilc_irq);
		ILC_WAKEUP_ENABLE(ilc->base, input);
		ILC_WAKEUP(ilc->base, input, 1);
	} else {
		ilc_reset_wakeup(ilc_irq);
		ILC_WAKEUP_DISABLE(ilc->base, input);
	}

	return 0;
}
#else
#define set_wake_ilc_irq	NULL
#endif	/* CONFIG_SUSPEND */

static struct irq_chip ilc_chip = {
	.name		= "ILC3",
	.startup	= startup_ilc_irq,
	.shutdown	= shutdown_ilc_irq,
	.mask		= mask_ilc_irq,
	.mask_ack	= mask_and_ack_ilc,
	.unmask		= unmask_ilc_irq,
	.set_type	= set_type_ilc_irq,
	.set_wake	= set_wake_ilc_irq,
};

static void __init ilc_demux_init(struct platform_device *pdev)
{
	struct ilc *ilc = platform_get_drvdata(pdev);
	int irq;
	int i;

	/* Default all interrupts to active high. */
	for (i = 0, irq = ilc->first_irq; i < ilc->inputs_num; i++, irq++) {
		ILC_SET_TRIGMODE(ilc->base, i, ILC_TRIGGERMODE_HIGH);

		/* SIM: Should we do the masking etc in ilc_irq_demux and
		 * then change this to handle_simple_irq? */
		set_irq_chip_and_handler_name(irq, &ilc_chip,
				handle_level_irq, ilc->name);
		set_irq_chip_data(irq, ilc);
	}

	i = 0;
	irq = platform_get_irq(pdev, i++);
	while (irq >= 0) {
		set_irq_chip_and_handler(irq, &dummy_irq_chip, ilc_irq_demux);
		set_irq_data(irq, ilc);
		irq = platform_get_irq(pdev, i++);
	}

	return;
}

static int __init ilc_probe(struct platform_device *pdev)
{
	struct stm_plat_ilc3_data *pdata = pdev->dev.platform_data;
	struct resource *memory;
	int memory_size;
	struct ilc *ilc;
	int i;
	int error = 0;

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!memory)
		return -EINVAL;
	memory_size = resource_size(memory);

	if (!request_mem_region(memory->start, memory_size, pdev->name))
		return -EBUSY;

	ilc = kzalloc(sizeof(struct ilc), GFP_KERNEL);
	if (!ilc)
		return -ENOMEM;
	platform_set_drvdata(pdev, ilc);

	ilc->clk = clk_get(&pdev->dev, "comms_clk");
	clk_enable(ilc->clk); /* Just to increase the usage_counter */

	spin_lock_init(&ilc->lock);

	ilc->name = dev_name(&pdev->dev);
	ilc->inputs_num = pdata->inputs_num;
	ilc->outputs_num = pdata->outputs_num;
	ilc->first_irq = pdata->first_irq;
	ilc->disable_wakeup = pdata->disable_wakeup;

	ilc->base = ioremap(memory->start, memory_size);
	if (!ilc->base)
		return -ENOMEM;

	ilc->irqs = kzalloc(sizeof(struct ilc_irq) * ilc->inputs_num,
			GFP_KERNEL);
	if (!ilc->irqs)
		return -ENOMEM;

	for (i = 0; i < ilc->inputs_num; ++i) {
		ilc->irqs[i].priority = 7;
		ilc->irqs[i].trigger_mode = ILC_TRIGGERMODE_HIGH;
	}

	ilc->priority = kzalloc(ilc->outputs_num * sizeof(long*), GFP_KERNEL);
	if (!ilc->priority)
		return -ENOMEM;

	for (i = 0; i < ilc->outputs_num; ++i) {
		ilc->priority[i] = kzalloc(sizeof(long) *
				DIV_ROUND_UP(ilc->inputs_num, 32), GFP_KERNEL);
		if (!ilc->priority[i])
			return -ENOMEM;
	}

	ilc_demux_init(pdev);

	list_add(&ilc->list, &ilcs_list);

	/* sysdev doesn't appear to like id's of -1 */
	ilc->sysdev.id = (pdev->id == -1) ? 0 : pdev->id;

	ilc->sysdev.cls = &ilc_sysdev_class,
	error = sysdev_register(&ilc->sysdev);

	return error;
}

static struct platform_driver ilc_driver = {
	.driver	= {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = ilc_probe,
};

#if defined(CONFIG_DEBUG_FS)

static void *ilc_seq_start(struct seq_file *s, loff_t *pos)
{
	seq_printf(s, "input irq status enabled used priority mode wakeup\n");

	return seq_list_start(&ilcs_list, *pos);
}

static void *ilc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return seq_list_next(v, &ilcs_list, pos);
}

static void ilc_seq_stop(struct seq_file *s, void *v)
{
}

static int ilc_seq_show(struct seq_file *s, void *v)
{
	struct ilc *ilc = list_entry(v, struct ilc, list);
	int i, status, enabled, used, wakeup;

	seq_printf(s, "ILC: %s\n", ilc->name);
	for (i = 0; i < ilc->inputs_num; ++i) {
		status = (ILC_GET_STATUS(ilc->base, i) != 0);
		enabled = (ILC_GET_ENABLE(ilc->base, i) != 0);
		used = ilc_is_used(&ilc->irqs[i]);
		wakeup = ilc_wakeup_enabled(&ilc->irqs[i]);
		seq_printf(s, "%3d %3d %d %d %d %d %d %d",
			i, i + ilc->first_irq,
			status, enabled, used,
			readl(ilc->base + ILC_PRIORITY_REG(i)),
			readl(ilc->base + ILC_TRIGMODE_REG(i)), wakeup);
		if (enabled && !used)
			seq_printf(s, " !!!");
		seq_printf(s, "\n");
	}

	return 0;
}

static const struct seq_operations ilc_seq_ops = {
	.start = ilc_seq_start,
	.next = ilc_seq_next,
	.stop = ilc_seq_stop,
	.show = ilc_seq_show,
};

static int ilc_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ilc_seq_ops);
}

static const struct file_operations ilc_debugfs_ops = {
	.owner = THIS_MODULE,
	.open = ilc_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __init ilc_debugfs_init(void)
{
	debugfs_create_file("ilc", S_IFREG | S_IRUGO,
			NULL, NULL, &ilc_debugfs_ops);

	return 0;
}
subsys_initcall(ilc_debugfs_init);

#endif /* CONFIG_DEBUG_FS */



#ifdef CONFIG_HIBERNATION

static int ilc_resume_from_hibernation(struct ilc *ilc)
{
	unsigned long flag;
	int i, irq;
	local_irq_save(flag);
	for (i = 0; i < ilc->inputs_num; ++i) {
		irq = i + ilc->first_irq;
		ILC_SET_PRI(ilc->base, i, ilc->irqs[i].priority);
		ILC_SET_TRIGMODE(ilc->base, i, ilc->irqs[i].trigger_mode);
		if (ilc_is_used(&ilc->irqs[i])) {
			startup_ilc_irq(irq);
			if (ilc_is_enabled(&ilc->irqs[i]))
				unmask_ilc_irq(irq);
			else
				mask_ilc_irq(irq);
			}
		}
	local_irq_restore(flag);
	return 0;
}

static int ilc_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	struct ilc *ilc = sysdev_to_ilc(dev);

	ilc->state = state;

	return 0;
}

static int ilc_sysdev_resume(struct sys_device *dev)
{
	struct ilc *ilc = sysdev_to_ilc(dev);
	if (ilc->state.event == PM_EVENT_FREEZE) {
		ilc_resume_from_hibernation(ilc);
		ilc->state = PMSG_ON;
	}
	return 0;
}

#else
#define ilc_sysdev_suspend NULL
#define ilc_sysdev_resume NULL
#endif

static struct sysdev_class ilc_sysdev_class = {
	.name = "ilc3",
	.suspend = ilc_sysdev_suspend,
	.resume = ilc_sysdev_resume,
};

static int __init ilc_init(void)
{
	int ret;

	ret = sysdev_class_register(&ilc_sysdev_class);
	if (ret)
		return ret;

	ret = platform_driver_register(&ilc_driver);
	if (ret)
		return ret;

	return 0;	
}
core_initcall(ilc_init);
