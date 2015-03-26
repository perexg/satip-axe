/*
 *  kptrace - Kprobes-based tracing
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2008, 2010
 *
 * 2007-Jul	Created by Chris Smith <chris.smith@st.com>
 * 2008-Aug     Chris Smith <chris.smith@st.com> added a sysfs interface for
 *              user space tracing.
 */
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/prctl.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <linux/sysdev.h>
#include <linux/futex.h>
#include <asm/kptrace.h>
#include <net/sock.h>
#include <asm/sections.h>

#define INIT_SYSCALL_PROBE(x) create_tracepoint(set, #x, syscall_pre_handler, \
				syscall_rp_handler);

#define INIT_CUSTOM_SYSCALL_PROBE(x,y) create_tracepoint(set, #x, y, syscall_rp_handler);

enum kptrace_tracepoint_states {
	TP_UNUSED,
	TP_USED,
	TP_INUSE
};

typedef struct {
	struct kprobe kp;
	struct kretprobe rp;
	struct kobject kobj;
	int enabled;
	int callstack;
	int inserted;
	int stopon;
	int starton;
	int user_tracepoint;
	int late_tracepoint;
	struct file_operations *ops;
	struct list_head list;
} tracepoint_t;

typedef struct {
	int enabled;
	struct kobject kobj;
	struct file_operations *ops;
	struct list_head list;
} tracepoint_set_t;

static LIST_HEAD(tracepoint_sets);
static LIST_HEAD(tracepoints);

static char kpprintf_buf[KPTRACE_BUF_SIZE];
EXPORT_SYMBOL(kpprintf_buf);
static char kpprintf_buf_irq[KPTRACE_BUF_SIZE];
EXPORT_SYMBOL(kpprintf_buf_irq);
static struct mutex kpprintf_mutex;
static DEFINE_SPINLOCK(kpprintf_lock);

/* file-static data*/
static char trace_buf[KPTRACE_BUF_SIZE];
static char stack_buf[KPTRACE_BUF_SIZE];
static char user_new_symbol[KPTRACE_BUF_SIZE];
static tracepoint_set_t *user_set;
static struct kobject userspace;
static int user_stopon;
static int user_starton;
static int timestamping_enabled = 1;
static int stackdepth = 16;
static const int interface_version = 2;
static unsigned long do_execve_addr;

/* relay data */
static struct rchan *chan;
static struct dentry *dir;
static int logging;
static int mappings;
static int suspended;
static size_t dropped;
static size_t subbuf_size = 262144;
static size_t n_subbufs = 4;
#define KPTRACE_MAXSUBBUFSIZE 16777216
#define KPTRACE_MAXSUBBUFS 256

/* channel-management control files */
static struct dentry *enabled_control;
static struct dentry *create_control;
static struct dentry *subbuf_size_control;
static struct dentry *n_subbufs_control;
static struct dentry *dropped_control;

/* produced/consumed control files */
static struct dentry *produced_control;
static struct dentry *consumed_control;

/* control file fileop declarations */
static struct file_operations enabled_fops;
static struct file_operations create_fops;
static struct file_operations subbuf_size_fops;
static struct file_operations n_subbufs_fops;
static struct file_operations dropped_fops;
static struct file_operations produced_fops;
static struct file_operations consumed_fops;

/* forward declarations */
static int create_controls(void);
static void destroy_channel(void);
static void remove_controls(void);
static void start_tracing(void);
static void stop_tracing(void);
static tracepoint_t *create_tracepoint(tracepoint_set_t * set, const char *name,
				       int (*entry_handler) (struct kprobe *,
							     struct pt_regs *),
				       int (*return_handler) (struct
							      kretprobe_instance
							      *,
							      struct pt_regs
							      *));
static int user_pre_handler(struct kprobe *p, struct pt_regs *regs);
static int user_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs);
extern void get_stack(char *buf, unsigned long *sp, size_t size, size_t depth);
static void write_trace_record_no_callstack(const char *rec);

/* protection for the formatting temporary buffer */
static DEFINE_SPINLOCK(tmpbuf_lock);

static struct attribute *tracepoint_attribs[] = {
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "enabled",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "callstack",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	NULL
};

static struct attribute *tracepoint_set_attribs[] = {
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "enabled",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	NULL
};

static struct attribute *user_tp_attribs[] = {
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "new_symbol",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "add",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "enabled",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "stopon",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "starton",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	NULL
};

static struct attribute *userspace_attribs[] = {
	&(struct attribute){
			    .owner = THIS_MODULE,
			    .name = "new_record",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	NULL
};

static ssize_t tracepoint_set_show_attrs(struct kobject *kobj,
					 struct attribute *attr, char *buffer)
{
	tracepoint_set_t *set = container_of(kobj, tracepoint_set_t, kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (set->enabled) {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint set \"%s\" is enabled\n",
				 kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint set \"%s\" is disabled\n",
				 kobj->name);
		}
	}
	return strlen(buffer);
}

static ssize_t tracepoint_set_store_attrs(struct kobject *kobj,
					  struct attribute *attr,
					  const char *buffer, size_t size)
{
	tracepoint_set_t *set = container_of(kobj, tracepoint_set_t, kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0) {
			set->enabled = 1;
		} else {
			set->enabled = 0;
		}
	}
	return size;
}

static ssize_t tracepoint_show_attrs(struct kobject *kobj,
				     struct attribute *attr, char *buffer)
{
	tracepoint_t *tp = container_of(kobj, tracepoint_t, kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (tp->enabled == 1) {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint on %s is enabled\n", kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint on %s is disabled\n", kobj->name);
		}
	}

	if (strcmp(attr->name, "callstack") == 0) {
		if (tp->callstack == 1) {
			snprintf(buffer, PAGE_SIZE,
				 "Callstack gathering on %s is enabled\n",
				 kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Callstack gathering on %s is disabled\n",
				 kobj->name);
		}
	}

	return strlen(buffer);
}

static ssize_t tracepoint_store_attrs(struct kobject *kobj,
				      struct attribute *attr,
				      const char *buffer, size_t size)
{
	tracepoint_t *tp = container_of(kobj, tracepoint_t, kobj);

	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0) {
			tp->enabled = 1;
		} else {
			tp->enabled = 0;
		}
	}

	if (strcmp(attr->name, "callstack") == 0) {
		if (strncmp(buffer, "1", 1) == 0) {
			tp->callstack = 1;
		} else {
			tp->callstack = 0;
		}
	}

	return size;
}

static ssize_t kptrace_stackdepth_show_attrs(struct sys_device *device,
					     struct sysdev_attribute *attr,
					     char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "Callstack depth is %d\n",
			stackdepth);
}

static ssize_t kptrace_stackdepth_store_attrs(struct sys_device *device,
					      struct sysdev_attribute *attr,
					      const char *buffer, size_t size)
{
	stackdepth = simple_strtoul(buffer, NULL, 10);
	return size;
}

static ssize_t kptrace_configured_show_attrs(struct sys_device *device,
					     struct sysdev_attribute *attr,
					     char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "Used to start/stop tracing\n");
}

static ssize_t kptrace_configured_store_attrs(struct sys_device *device,
					      struct sysdev_attribute *attr,
					      const char *buffer, size_t size)
{
	if (*buffer == '1') {
		start_tracing();
	} else {
		stop_tracing();
	}
	return size;
}

static ssize_t kptrace_version_show_attrs(struct sys_device *device,
					  struct sysdev_attribute *attr,
					  char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%d\n", interface_version);
}

static ssize_t kptrace_version_store_attrs(struct sys_device *device,
					   struct sysdev_attribute *attr,
					   const char *buffer, size_t size)
{
	/* Nothing happens */
	return size;
}

static ssize_t kptrace_pause_show_attrs(struct sys_device *device,
					struct sysdev_attribute *attr,
					char *buffer)
{
	return snprintf(buffer, PAGE_SIZE,
			"Write to this file to pause tracing\n");
}

static ssize_t kptrace_pause_store_attrs(struct sys_device *device,
					 struct sysdev_attribute *attr,
					 const char *buffer, size_t size)
{
	kptrace_pause();
	return size;
}

static ssize_t kptrace_restart_show_attrs(struct sys_device *device,
					  struct sysdev_attribute *attr,
					  char *buffer)
{
	return snprintf(buffer, PAGE_SIZE,
			"Write to this file to restart tracing\n");
}

static ssize_t kptrace_restart_store_attrs(struct sys_device *device,
					   struct sysdev_attribute *attr,
					   const char *buffer, size_t size)
{
	kptrace_restart();
	return size;
}

static ssize_t user_show_attrs(struct kobject *kobj, struct attribute *attr,
			       char *buffer)
{
	if (strcmp(attr->name, "new_symbol") == 0) {
		return snprintf(buffer, PAGE_SIZE, "new_symbol = %s\n",
				user_new_symbol);
	}

	if (strcmp(attr->name, "add") == 0) {
		return snprintf(buffer, PAGE_SIZE,
				"Adding new tracepoint %s\n", user_new_symbol);
	}

	if (strcmp(attr->name, "enabled") == 0) {
		if (user_set->enabled) {
			return snprintf(buffer, PAGE_SIZE,
					"User-defined tracepoints are enabled");
		} else {
			return snprintf(buffer, PAGE_SIZE,
					"User-defined tracepoints are disabled");
		}
	}

	if (strcmp(attr->name, "stopon") == 0) {
		if (user_stopon) {
			return snprintf(buffer, PAGE_SIZE,
					"Stop logging on this tracepoint: on");
		} else {
			return snprintf(buffer, PAGE_SIZE,
					"Stop logging on this tracepoint: off");
		}
	}

	if (strcmp(attr->name, "starton") == 0) {
		if (user_stopon) {
			return snprintf(buffer, PAGE_SIZE,
					"Start logging on this tracepoint: on");
		} else {
			return snprintf(buffer, PAGE_SIZE,
					"Start logging on this tracepoint: off");
		}
	}

	return snprintf(buffer, PAGE_SIZE, "Unknown attribute\n");
}

ssize_t user_store_attrs(struct kobject * kobj, struct attribute * attr,
			 const char *buffer, size_t size)
{
	struct list_head *p;
	tracepoint_t *tp, *new_tp = NULL;

	if (strcmp(attr->name, "new_symbol") == 0) {
		strncpy(user_new_symbol, buffer, KPTRACE_BUF_SIZE);
	}

	if (strcmp(attr->name, "add") == 0) {
		/* Check it doesn't already exist, to avoid duplicates */
		list_for_each(p, &tracepoints) {
			tp = list_entry(p, tracepoint_t, list);
			if (tp != NULL && tp->user_tracepoint == 1) {
				if (strncmp(kobject_name(&tp->kobj),
					    user_new_symbol,
					    KPTRACE_BUF_SIZE) == 0)
					return size;
			}
		}

		new_tp = create_tracepoint(user_set, user_new_symbol,
					   &user_pre_handler, &user_rp_handler);

		if (!new_tp) {
			printk(KERN_ERR "kptrace: Cannot create tracepoint\n");
			return -ENOSYS;
		} else {
			new_tp->stopon = user_stopon;
			new_tp->starton = user_starton;
			new_tp->user_tracepoint = 1;
			return size;
		}
	}

	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0) {
			user_set->enabled = 1;
		} else {
			user_set->enabled = 0;
		}

	}

	if (strcmp(attr->name, "stopon") == 0) {
		if (strncmp(buffer, "1", 1) == 0) {
			user_stopon = 1;
		} else {
			user_stopon = 0;
		}
	}

	if (strcmp(attr->name, "starton") == 0) {
		if (strncmp(buffer, "1", 1) == 0) {
			user_starton = 1;
		} else {
			user_starton = 0;
		}
	}

	return size;
}

static ssize_t userspace_show_attrs(struct kobject *kobj,
				    struct attribute *attr, char *buffer)
{
	if (strcmp(attr->name, "new_record") == 0)
		return snprintf(buffer, PAGE_SIZE,
				"Used to add records from user space\n");

	return snprintf(buffer, PAGE_SIZE, "Unknown attribute\n");
}

static ssize_t userspace_store_attrs(struct kobject *kobj,
				     struct attribute *attr, const char *buffer,
				     size_t size)
{
	if (strcmp(attr->name, "new_record") == 0)
		write_trace_record_no_callstack(buffer);

	return size;
}

/* Main control is a sysdev */
struct sysdev_class kptrace_sysdev = {
	.name = "kptrace"
};

SYSDEV_ATTR(configured, S_IRUGO | S_IWUSR, kptrace_configured_show_attrs,
	    kptrace_configured_store_attrs);
SYSDEV_ATTR(stackdepth, S_IRUGO | S_IWUSR, kptrace_stackdepth_show_attrs,
	    kptrace_stackdepth_store_attrs);
SYSDEV_ATTR(version, S_IRUGO | S_IWUSR, kptrace_version_show_attrs,
	    kptrace_version_store_attrs);
SYSDEV_ATTR(pause, S_IRUGO | S_IWUSR, kptrace_pause_show_attrs,
	    kptrace_pause_store_attrs);
SYSDEV_ATTR(restart, S_IRUGO | S_IWUSR, kptrace_restart_show_attrs,
	    kptrace_restart_store_attrs);

static struct sys_device kptrace_device = {
	.id = 0,
	.cls = &kptrace_sysdev,
};

/* Operations for the three kobj types */
struct sysfs_ops tracepoint_sysfs_ops = {
	&tracepoint_show_attrs, &tracepoint_store_attrs
};

struct sysfs_ops tracepoint_set_sysfs_ops = {
	&tracepoint_set_show_attrs, &tracepoint_set_store_attrs
};
struct sysfs_ops user_sysfs_ops = { &user_show_attrs, &user_store_attrs };

struct sysfs_ops userspace_sysfs_ops = {
	&userspace_show_attrs, &userspace_store_attrs
};

/* Three kobj types: tracepoints, tracepoint sets, the special "user" tracepoint set */
struct kobj_type tracepoint_type = {
	NULL, &tracepoint_sysfs_ops, tracepoint_attribs
};

struct kobj_type tracepoint_set_type = { NULL, &tracepoint_set_sysfs_ops,
	tracepoint_set_attribs
};
struct kobj_type user_type = { NULL, &user_sysfs_ops, user_tp_attribs };

struct kobj_type userspace_type = {
	NULL, &userspace_sysfs_ops, userspace_attribs
};

static tracepoint_t *__create_tracepoint(tracepoint_set_t * set,
					 const char *name,
					 int (*entry_handler) (struct kprobe *,
							struct pt_regs
							*),
					 int (*return_handler) (struct
							kretprobe_instance
							*,
							struct pt_regs
							*),
					 int late_tracepoint)
{
	tracepoint_t *tp;
	tp = kzalloc(sizeof(*tp), GFP_KERNEL);
	if (!tp) {
		printk(KERN_WARNING
		       "kptrace: Failed to allocate memory for tracepoint %s\n",
		       name);
		return NULL;
	}

	tp->enabled = 0;
	tp->callstack = 0;
	tp->stopon = 0;
	tp->starton = 0;
	tp->user_tracepoint = 0;
	tp->late_tracepoint = late_tracepoint;
	tp->inserted = TP_UNUSED;

	if (entry_handler != NULL) {
		tp->kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name(name);

		if (tp->late_tracepoint == 1) {
			tp->kp.flags |= KPROBE_FLAG_DISABLED;
		}

		if (!tp->kp.addr) {
			printk(KERN_WARNING "kptrace: Symbol %s not found\n",
			       name);
			kfree(tp);
			return NULL;
		}
		tp->kp.pre_handler = entry_handler;
	}

	if (return_handler != NULL) {
		if (entry_handler != NULL)
			tp->rp.kp.addr = tp->kp.addr;
		else
			tp->rp.kp.addr = (kprobe_opcode_t *)
			    kallsyms_lookup_name(name);

		tp->rp.handler = return_handler;
		tp->rp.maxactive = 128;
	}

	list_add(&tp->list, &tracepoints);

	if (kobject_init_and_add(&tp->kobj, &tracepoint_type, &set->kobj, name)
	    < 0) {
		printk(KERN_WARNING "kptrace: Failed add to add kobject %s\n",
		       name);
		return NULL;
	}

	return tp;
}

/*
 * Creates a tracepoint in the given set. Pointers to entry and/or return
 * handlers can be NULL if it is not necessary to track those events.
 *
 * This function only initializes the data structures and adds the sysfs node.
 * To actually add the kprobes and start tracing, use insert_tracepoint().
 */
static tracepoint_t *create_tracepoint(tracepoint_set_t * set, const char *name,
				       int (*entry_handler) (struct kprobe *,
							     struct pt_regs *),
				       int (*return_handler) (struct
							      kretprobe_instance
							      *,
							      struct pt_regs *))
{
	return __create_tracepoint(set, name, entry_handler, return_handler, 0);
}

/*
 * As create_tracepoint(), except that the tracepoint is not armed until all
 * tracepoints have been added. This is useful when tracing a function used
 * in the kprobe code, such as mutex_lock().
 */
#ifdef CONFIG_KPTRACE_SYNC
static tracepoint_t *create_late_tracepoint(tracepoint_set_t * set,
					    const char *name,
					    int (*entry_handler) (struct
							kprobe *,
							struct pt_regs
							*),
					    int (*return_handler) (struct
							kretprobe_instance
							*,
							struct
							pt_regs
							*))
{
	return __create_tracepoint(set, name, entry_handler, return_handler, 1);
}
#endif

/*
 * Registers the kprobes for the tracepoint, so that it will start to
 * be logged.
 *
 * kretprobes are only registered the first time. After that, we only
 * register and unregister the initial kprobe. This prevents race
 * conditions where a function is halfway through execution when the
 * probe is removed.
 */
static void insert_tracepoint(tracepoint_t * tp)
{
	if (tp->inserted != TP_INUSE) {
		if (tp->kp.addr != NULL) {
			register_kprobe(&tp->kp);
		}

		if (tp->rp.kp.addr != NULL) {
			if (tp->inserted == TP_UNUSED) {
				register_kretprobe(&tp->rp);
			} else if (tp->inserted == TP_USED) {
				register_kprobe(&tp->rp.kp);
			}
		}

		tp->inserted = TP_INUSE;
	}
}

/* Insert all enabled tracepoints in this set */
static void insert_tracepoints_in_set(tracepoint_set_t * set)
{
	struct list_head *p;
	tracepoint_t *tp;

	list_for_each(p, &tracepoints) {
		tp = list_entry(p, tracepoint_t, list);
		if (tp->kobj.parent) {
			if ((strcmp
			     (tp->kobj.parent->name,
			      set->kobj.name) == 0) && (tp->enabled == 1))
				insert_tracepoint(tp);
		}
	}
}

/*
 * Unregister the kprobes for the tracepoint. From kretprobes,
 * only unregister the initial kprobe to prevent race condition
 * when function is halfway through execution when the probe is
 * removed.
 */
int unregister_tracepoint(tracepoint_t * tp)
{
	if (tp->kp.addr != NULL) {
		if (tp->late_tracepoint)
			arch_disarm_kprobe(&tp->kp);
		unregister_kprobe(&tp->kp);
	}

	if (tp->rp.kp.addr != NULL) {
		unregister_kprobe(&tp->rp.kp);
	}

	tp->inserted = TP_USED;

	return 0;
}

/*
 * Allocates the data structures for a new tracepoint set and
 * creates a sysfs node for it.
 */
static tracepoint_set_t *create_tracepoint_set(const char *name)
{
	tracepoint_set_t *set;
	set = kzalloc(sizeof(*set), GFP_KERNEL);
	if (!set)
		return NULL;

	list_add(&set->list, &tracepoint_sets);

	if (kobject_init_and_add(&set->kobj, &tracepoint_set_type,
				 &kptrace_sysdev.kset.kobj, name) < 0)
		printk(KERN_WARNING "kptrace: Failed to add kobject %s\n",
		       name);

	set->enabled = 0;

	return set;
}

/* Inserts all the tracepoints in each enabled set */
static void start_tracing(void)
{
	struct list_head *p, *tmp;
	tracepoint_set_t *set;
	tracepoint_t *tp;

	list_for_each(p, &tracepoint_sets) {
		set = list_entry(p, tracepoint_set_t, list);
		if (set->enabled) {
			insert_tracepoints_in_set(set);
		}
	}

	/* Arm any "late" tracepoints */
	list_for_each_safe(p, tmp, &tracepoints) {
		tp = list_entry(p, tracepoint_t, list);
		if (tp->late_tracepoint && tp->enabled) {
			if (kprobe_disabled(&tp->kp))
				arch_arm_kprobe(&tp->kp);
		}
	}

	logging = 1;
}

/* Remove all tracepoints */
static void stop_tracing(void)
{
	struct list_head *p, *tmp;
	tracepoint_t *tp;

	list_for_each_safe(p, tmp, &tracepoints) {
		tp = list_entry(p, tracepoint_t, list);

		if (tp->inserted == TP_INUSE) {
			unregister_tracepoint(tp);
		}

		if (tp->user_tracepoint == 1) {
			kobject_put(&tp->kobj);
			tp->kp.addr = NULL;
			list_del(p);
		} else {
			tp->enabled = 0;
		}
	}
}

/*
 * Write a trace record to the relay buffer.
 *
 * Prepends a timestamp and the current PID, and puts a callstack
 * on the end where requested.
 */
static void write_trace_record(struct kprobe *kp, struct pt_regs *regs,
			       const char *rec)
{
	unsigned tlen;
	struct timeval tv;
	unsigned long flags;
	tracepoint_t *tp = NULL;

	spin_lock_irqsave(&tmpbuf_lock, flags);

	if (timestamping_enabled) {
		do_gettimeofday(&tv);
	} else {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	}

	tp = container_of(kp, tracepoint_t, kp);

	if (kp && tp->starton == 1)
		logging = 1;

	if (!logging) {
		spin_unlock_irqrestore(&tmpbuf_lock, flags);
		return;
	}

	if (kp && tp->callstack == 1) {
		get_stack(stack_buf, (unsigned long *)regs->regs[15],
			  KPTRACE_BUF_SIZE, stackdepth);
		tlen =
		    snprintf(trace_buf, KPTRACE_BUF_SIZE,
			     "%lu.%06lu %d %s\n%s\n", tv.tv_sec, tv.tv_usec,
			     current->pid, rec, stack_buf);
	} else {
		tlen = snprintf(trace_buf, KPTRACE_BUF_SIZE,
				"%lu.%06lu %d %s\n",
				tv.tv_sec, tv.tv_usec, current->pid, rec);
	}

	relay_write(chan, trace_buf, tlen);

	spin_unlock_irqrestore(&tmpbuf_lock, flags);

	if (kp && tp->stopon == 1)
		logging = 0;
}

/*
 * Write a trace record to the relay buffer.
 *
 * Because the current kprobe and regs are not provided,
 * no callstack can be added.
 */
static void write_trace_record_no_callstack(const char *rec)
{
	write_trace_record(NULL, NULL, rec);
}

/*
 * Primary interface for user to new static tracepoints.
 */
void kptrace_write_record(const char *rec)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "K %s", rec);
	write_trace_record_no_callstack(tbuf);
}
EXPORT_SYMBOL(kptrace_write_record);

/*
 * Provides a printf-style interface on the trace buffer.
 */
void kpprintf(char *fmt, ...)
{
	unsigned long flags;
	va_list ap;
	va_start(ap, fmt);

	/* Only spin in interrupt context, to reduce intrusion */
	if (in_interrupt()) {
		spin_lock_irqsave(&kpprintf_lock, flags);
		vsnprintf(kpprintf_buf_irq, KPTRACE_BUF_SIZE, fmt, ap);
		kptrace_write_record(kpprintf_buf_irq);
		spin_unlock_irqrestore(&kpprintf_lock, flags);
	} else {
		mutex_lock(&kpprintf_mutex);
		vsnprintf(kpprintf_buf, KPTRACE_BUF_SIZE, fmt, ap);
		kptrace_write_record(kpprintf_buf);
		mutex_unlock(&kpprintf_mutex);
	}
}
EXPORT_SYMBOL(kpprintf);

/*
 * Indicates that this is an interesting point in the code.
 * Intended to be highlighted prominently in a GUI.
 */
void kptrace_mark(void)
{
	write_trace_record_no_callstack("KM");
}

EXPORT_SYMBOL(kptrace_mark);

/*
 * Stops the logging of trace records until kptrace_restart()
 * is called.
 */
void kptrace_pause(void)
{
	write_trace_record_no_callstack("KP");
	logging = 0;
}

EXPORT_SYMBOL(kptrace_pause);

/*
 * Restarts logging of trace after a kptrace_pause()
*/
void kptrace_restart(void)
{
	logging = 1;
	write_trace_record_no_callstack("KR");
}

EXPORT_SYMBOL(kptrace_restart);

static int user_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "U %.8x 0x%.8x 0x%.8x 0x%.8x 0x%.8x",
		 (int)regs->pc, (int)regs->regs[4], (int)regs->regs[5],
		 (int)regs->regs[6], (int)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int user_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	u32 probe_func_addr = (u32) ri->rp->kp.addr;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "u %d %.8x", (int)regs->regs[0],
		 probe_func_addr);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int syscall_rp_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "X %.8x %d",
		 (unsigned int)ri->rp->kp.addr, (int)regs->regs[0]);

	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int softirq_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "S %.8x", (unsigned int)regs->pc);

	write_trace_record(p, regs, tbuf);
	return 0;
}

static int softirq_rp_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	write_trace_record_no_callstack("s");
	return 0;
}

static int wake_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "W %d",
		 ((struct task_struct *)regs->regs[4])->pid);

	/* If we try and put a timestamp on this, we'll cause a deadlock */
	timestamping_enabled = 0;
	write_trace_record(p, regs, tbuf);
	timestamping_enabled = 1;

	return 0;
}

static int context_switch_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	int prev, next;
	prev = ((struct task_struct *)regs->regs[4])->pid;
	next = ((struct task_struct *)regs->regs[5])->pid;

	snprintf(tbuf, KPTRACE_SMALL_BUF, "C %d %d", prev, next);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int irq_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	write_trace_record_no_callstack("i");
	return 0;
}

static int irq_exit_rp_handler(struct kretprobe_instance *ri,
			       struct pt_regs *regs)
{
	write_trace_record_no_callstack("Ix");
	return 0;
}

static int kthread_create_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "KC 0x%.8x\n",
		 (unsigned int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int exit_thread_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	write_trace_record(p, regs, "KX");
	return 0;
}

static int daemonize_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	char name[KPTRACE_SMALL_BUF];

	if (strncpy_from_user(name, (char *)regs->regs[4],
			      KPTRACE_SMALL_BUF) < 0)
		snprintf(name, KPTRACE_SMALL_BUF, "<copy_from_user failed>");

	snprintf(tbuf, KPTRACE_SMALL_BUF, "KD %s\n", name);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int kernel_thread_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "KT 0x%.8x\n",
		 (unsigned int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int kthread_create_rp_handler(struct kretprobe_instance *ri,
				     struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	char name[KPTRACE_SMALL_BUF];
	struct task_struct *new_task = (struct task_struct *)regs->regs[0];

	if (strncpy_from_user(name, (char *)new_task->comm,
			      KPTRACE_SMALL_BUF) < 0)
		snprintf(name, KPTRACE_SMALL_BUF, "<copy_from_user failed>");

	snprintf(tbuf, KPTRACE_SMALL_BUF, "Kc %d %s\n", new_task->pid, name);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int irq_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "I %d", (int)regs->regs[4]);

	write_trace_record(p, regs, tbuf);
	return 0;
}

static int syscall_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x 0x%.8x 0x%.8x 0x%.8x 0x%.8x",
		 (unsigned)regs->pc, (unsigned)regs->regs[4],
		 (unsigned)regs->regs[5], (unsigned)regs->regs[6],
		 (unsigned)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

/* Special syscall handler for prctl, in order to get the process name
   out of prctl(PR_SET_NAME) calls. */
static int syscall_prctl_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	char static_buf[KPTRACE_SMALL_BUF];

	if ((unsigned)regs->regs[4] == PR_SET_NAME) {
		if (strncpy_from_user(static_buf, (char *)regs->regs[5],
				      KPTRACE_SMALL_BUF) < 0)
			snprintf(static_buf, KPTRACE_SMALL_BUF,
				 "<copy_from_user failed>");

		snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d %s",
			 (int)regs->pc, (unsigned)regs->regs[4], static_buf);
	} else {
		snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d 0x%.8x 0x%.8x 0x%.8x",
			 (int)regs->pc, (unsigned)regs->regs[4],
			 (unsigned)regs->regs[5], (unsigned)regs->regs[6],
			 (unsigned)regs->regs[7]);
	}
	write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in int, hex, hex, hex format */
static int syscall_ihhh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d 0x%.8x 0x%.8x 0x%.8x",
		 (int)regs->pc, (unsigned)regs->regs[4],
		 (unsigned)regs->regs[5], (unsigned)regs->regs[6],
		 (unsigned)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in int, hex, int, hex format */
static int syscall_ihih_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d 0x%.8x %d 0x%.8x",
		 (int)regs->pc, (int)regs->regs[4], (unsigned)regs->regs[5],
		 (int)regs->regs[6], (unsigned)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in int, int, hex, hex format */
static int syscall_iihh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d %d 0x%.8x 0x%.8x",
		 (int)regs->pc, (int)regs->regs[4], (int)regs->regs[5],
		 (unsigned)regs->regs[6], (unsigned)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in hex, int, int, hex format */
static int syscall_hiih_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x 0x%.8x %d %d 0x%.8x",
		 (int)regs->pc, (unsigned)regs->regs[4], (int)regs->regs[5],
		 (int)regs->regs[6], (unsigned)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in hex, int, hex, hex format */
static int syscall_hihh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x 0x%.8x %d 0x%.8x 0x%.8x",
		 (int)regs->pc, (unsigned)regs->regs[4], (int)regs->regs[5],
		 (unsigned)regs->regs[6], (unsigned)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in string, hex, hex, hex format */
static int syscall_shhh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char *dyn_buf;
	char static_buf[KPTRACE_SMALL_BUF];
	char filename[KPTRACE_SMALL_BUF];
	int len = 0;

	if (regs->pc == do_execve_addr) {
		/* Don't need to strncpy_from_user in this case */
		snprintf(filename, KPTRACE_SMALL_BUF, (char *)regs->regs[4]);
	} else if (strncpy_from_user(filename, (char *)regs->regs[4],
				     KPTRACE_SMALL_BUF) < 0)
		snprintf(filename, KPTRACE_SMALL_BUF,
			 "<copy_from_user failed>");

	len =
	    snprintf(static_buf, KPTRACE_SMALL_BUF, "E %.8x %s 0x%.8x 0x%.8x 0x%.8x",
		     (int)regs->pc, filename, (unsigned)regs->regs[5],
		     (unsigned)regs->regs[6], (unsigned)regs->regs[7]);
	if (len < KPTRACE_SMALL_BUF) {
		write_trace_record(p, regs, static_buf);
	} else {
		dyn_buf = kzalloc(len + 1, GFP_KERNEL);
		snprintf(dyn_buf, len, "E %.8x %s 0x%.8x 0x%.8x 0x%.8x",
			 (int)regs->pc, filename, (unsigned)regs->regs[5],
			 (unsigned)regs->regs[6], (unsigned)regs->regs[7]);
		write_trace_record(p, regs, dyn_buf);
		kfree(dyn_buf);
	}

	return 0;
}

/* Output syscall arguments in string, int, hex, hex format. */
static int syscall_sihh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char *dyn_buf;
	char static_buf[KPTRACE_SMALL_BUF];
	char filename[KPTRACE_SMALL_BUF];
	int len = 0;

	if (strncpy_from_user(filename, (char *)regs->regs[4],
			      KPTRACE_SMALL_BUF) < 0)
		snprintf(filename, KPTRACE_SMALL_BUF,
			 "<copy_from_user failed>");

	len = snprintf(static_buf, KPTRACE_SMALL_BUF, "E %.8x %s %d 0x%.8x 0x%.8x",
		       (int)regs->pc, filename, (int)regs->regs[5],
		       (unsigned)regs->regs[6], (unsigned)regs->regs[7]);

	if (len < KPTRACE_SMALL_BUF) {
		write_trace_record_no_callstack(static_buf);
	} else {
		dyn_buf = kzalloc(len + 1, GFP_KERNEL);
		snprintf(dyn_buf, len, "E %.8x %s %d 0x%.8x 0x%.8x",
			 (int)regs->pc, filename, (int)regs->regs[5],
			 (unsigned)regs->regs[6], (unsigned)regs->regs[7]);
		write_trace_record_no_callstack(dyn_buf);
		kfree(dyn_buf);
	}

	return 0;
}

static int hash_futex_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	union futex_key *key = (union futex_key *)regs->regs[4];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "HF 0x%.8lx %p 0x%.8x",
		 key->both.word, key->both.ptr, key->both.offset);

	write_trace_record(p, regs, tbuf);

	return 0;
}

static int kmalloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MM %d %d", (int)regs->regs[4],
		 (int)regs->regs[5]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int kmalloc_rp_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mm 0x%.8x ", (int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int kfree_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MF 0x%.8x", (int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int do_page_fault_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MD 0x%.8x %d 0x%.8x",
		 ((unsigned int)((struct pt_regs *)regs->regs[4])->pc),
		 (int)regs->regs[5], (unsigned int)regs->regs[6]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int vmalloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MV %d", (int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int vmalloc_rp_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mv 0x%.8x ", (int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int vfree_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MQ 0x%.8x", (int)regs->regs[0]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int get_free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MG %d %d", (int)regs->regs[4],
		 (int)regs->regs[5]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int get_free_pages_rp_handler(struct kretprobe_instance *ri,
				     struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mg 0x%.8x ", (int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MA %d %d", (int)regs->regs[4],
		 (int)regs->regs[5]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int alloc_pages_rp_handler(struct kretprobe_instance *ri,
				  struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Ma 0x%.8x", (int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MZ 0x%.8x %d",
		 (unsigned int)regs->regs[4], (int)regs->regs[5]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int kmem_cache_alloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MS 0x%.8x %d",
		 (unsigned int)regs->regs[4], (int)regs->regs[5]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int kmem_cache_alloc_rp_handler(struct kretprobe_instance *ri,
				       struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Ms 0x%.8x ",
		 (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int kmem_cache_free_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MX 0x%.8x 0x%.8x",
		 (unsigned int)regs->regs[4], (unsigned int)regs->regs[5]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

#ifdef CONFIG_BPA2
static int bigphysarea_alloc_pages_pre_handler(struct kprobe *p,
					       struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MB %d %d %d",
		 (int)regs->regs[4], (int)regs->regs[5], (int)regs->regs[6]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int bigphysarea_alloc_pages_rp_handler(struct kretprobe_instance *ri,
					      struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mb 0x%.8x",
		 (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int bigphysarea_free_pages_pre_handler(struct kprobe *p,
					      struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MC 0x%.8x",
		 (unsigned int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int bpa2_alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MH 0x%.8x %d %d %d",
		 (unsigned int)regs->regs[4], (int)regs->regs[5],
		 (int)regs->regs[6], (int)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int bpa2_alloc_pages_rp_handler(struct kretprobe_instance *ri,
				       struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mh 0x%.8x",
		 (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);
	return 0;
}

static int bpa2_free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MI 0x%.8x 0x%.8x",
		 (unsigned int)regs->regs[4], (unsigned int)regs->regs[5]);
	write_trace_record(p, regs, tbuf);
	return 0;
}
#endif

static int netif_receive_skb_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	write_trace_record(p, regs, "NR");
	return 0;
}

static int netif_receive_skb_rp_handler(struct kretprobe_instance *ri,
					struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Nr %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

static int dev_queue_xmit_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	write_trace_record(p, regs, "NX");
	return 0;
}

static int dev_queue_xmit_rp_handler(struct kretprobe_instance *ri,
				     struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Nx %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

static int sock_sendmsg_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "SS 0x%.8x %d",
		 (unsigned int)regs->regs[4], (int)regs->regs[6]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int sock_sendmsg_rp_handler(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Ss %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

static int sock_recvmsg_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "SR 0x%.8x %d %d",
		 (unsigned int)regs->regs[4], (int)regs->regs[6],
		 (int)regs->regs[7]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int sock_recvmsg_rp_handler(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Sr %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

static int do_setitimer_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct itimerval *value = (struct itimerval *)regs->regs[5];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "IS %d %li.%06li %li.%06li",
		 (int)regs->regs[4], value->it_interval.tv_sec,
		 value->it_interval.tv_usec, value->it_value.tv_sec,
		 value->it_value.tv_usec);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int do_setitimer_rp_handler(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Is %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

static int it_real_fn_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "IE 0x%.8x",
		 (unsigned int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;
}

static int it_real_fn_rp_handler(struct kretprobe_instance *ri,
				 struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Ie %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

#ifdef CONFIG_KPTRACE_SYNC
static int mutex_lock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZM 0x%.8x",
		 (unsigned int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int mutex_unlock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zm 0x%.8x",
		 (unsigned int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int lock_kernel_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	write_trace_record(p, regs, "ZL");
	return 0;

}

static int unlock_kernel_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	write_trace_record(p, regs, "Zl");
	return 0;

}

static int down_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct semaphore *sem = (struct semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZD 0x%.8x %d",
		 (unsigned int)regs->regs[4], (unsigned int)sem->count);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_interruptible_pre_handler(struct kprobe *p,
					  struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct semaphore *sem = (struct semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZI 0x%.8x %d",
		 (unsigned int)regs->regs[4], (unsigned int)sem->count);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_trylock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct semaphore *sem = (struct semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZT 0x%.8x %d",
		 (unsigned int)regs->regs[4], (unsigned int)sem->count);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_trylock_rp_handler(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zt %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

static int up_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct semaphore *sem = (struct semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZU 0x%.8x %d",
		 (unsigned int)regs->regs[4], (unsigned int)sem->count);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int underscore_up_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zu 0x%.8x",
		 (unsigned int)regs->regs[4]);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_read_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZR 0x%.8x %d",
		 (unsigned int)regs->regs[4], sem->activity);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_read_trylock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZA 0x%.8x %d",
		 (unsigned int)regs->regs[4], sem->activity);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_read_trylock_rp_handler(struct kretprobe_instance *ri,
					struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Za %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

static int up_read_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zr 0x%.8x %d",
		 (unsigned int)regs->regs[4], sem->activity);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_write_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZW 0x%.8x %d",
		 (unsigned int)regs->regs[4], sem->activity);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_write_trylock_pre_handler(struct kprobe *p,
					  struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZB 0x%.8x %d",
		 (unsigned int)regs->regs[4], sem->activity);
	write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_write_trylock_rp_handler(struct kretprobe_instance *ri,
					 struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zb %d", (unsigned int)regs->regs[0]);
	write_trace_record_no_callstack(tbuf);

	return 0;
}

static int up_write_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->regs[4];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zw 0x%.8x %d",
		 (unsigned int)regs->regs[4], sem->activity);
	write_trace_record(p, regs, tbuf);
	return 0;

}
#endif /* CONFIG_KPTRACE_SYNC */

/* Add the main sysdev and the "user" tracepoint set */
static int create_sysfs_tree(void)
{
	sysdev_class_register(&kptrace_sysdev);
	sysdev_register(&kptrace_device);
	sysdev_create_file(&kptrace_device, &attr_configured);
	sysdev_create_file(&kptrace_device, &attr_stackdepth);
	sysdev_create_file(&kptrace_device, &attr_version);
	sysdev_create_file(&kptrace_device, &attr_pause);
	sysdev_create_file(&kptrace_device, &attr_restart);

	user_set = kzalloc(sizeof(*user_set), GFP_KERNEL);
	if (!user_set) {
		printk(KERN_WARNING
		       "kptrace: Failed to allocate memory for sysdev\n");
		return 0;
	}
	list_add(&user_set->list, &tracepoint_sets);

	if (kobject_init_and_add(&user_set->kobj, &user_type,
				 &kptrace_sysdev.kset.kobj, "user") < 0)
		printk(KERN_WARNING "kptrace: Failed to add kobject user\n");
	user_set->enabled = 0;

	if (kobject_init_and_add(&userspace,
				 &userspace_type, &kptrace_sysdev.kset.kobj,
				 "userspace") < 0)
		printk(KERN_WARNING
		       "kptrace: Failed to add kobject userspace\n");

	return 1;
}

static void init_core_event_logging(void)
{
	tracepoint_set_t *set = create_tracepoint_set("core_kernel_events");
	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create core tracepoint set.\n");
		return;
	}

	create_tracepoint(set, "handle_simple_irq", irq_pre_handler,
			  irq_rp_handler);
	create_tracepoint(set, "handle_level_irq", irq_pre_handler,
			  irq_rp_handler);
	create_tracepoint(set, "handle_fasteoi_irq", irq_pre_handler,
			  irq_rp_handler);
	create_tracepoint(set, "handle_edge_irq", irq_pre_handler,
			  irq_rp_handler);
	create_tracepoint(set, "irq_exit", NULL, irq_exit_rp_handler);
	create_tracepoint(set, "__switch_to", context_switch_pre_handler, NULL);
	create_tracepoint(set, "tasklet_hi_action", softirq_pre_handler,
			  softirq_rp_handler);
	create_tracepoint(set, "net_tx_action", softirq_pre_handler,
			  softirq_rp_handler);
	create_tracepoint(set, "net_rx_action", softirq_pre_handler,
			  softirq_rp_handler);
	create_tracepoint(set, "blk_done_softirq", softirq_pre_handler,
			  softirq_rp_handler);
	create_tracepoint(set, "tasklet_action", softirq_pre_handler,
			  softirq_rp_handler);
	create_tracepoint(set, "kthread_create", kthread_create_pre_handler,
			  kthread_create_rp_handler);
	create_tracepoint(set, "kernel_thread", kernel_thread_pre_handler,
			  NULL);
	create_tracepoint(set, "daemonize", daemonize_pre_handler, NULL);
	create_tracepoint(set, "exit_thread", exit_thread_pre_handler, NULL);
}

static void init_syscall_logging(void)
{
	tracepoint_set_t *set = create_tracepoint_set("syscalls");
	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create syscall tracepoint set.\n");
		return;
	}

	INIT_SYSCALL_PROBE(sys_restart_syscall);
	INIT_SYSCALL_PROBE(sys_exit);
	INIT_SYSCALL_PROBE(sys_fork);
	INIT_CUSTOM_SYSCALL_PROBE(sys_read, syscall_ihih_pre_handler);
	INIT_CUSTOM_SYSCALL_PROBE(sys_write, syscall_ihih_pre_handler);
	INIT_CUSTOM_SYSCALL_PROBE(sys_open, syscall_shhh_pre_handler);
	INIT_CUSTOM_SYSCALL_PROBE(sys_close, syscall_ihhh_pre_handler);
	INIT_CUSTOM_SYSCALL_PROBE(sys_waitpid, syscall_ihhh_pre_handler);
	INIT_SYSCALL_PROBE(sys_creat);
	INIT_SYSCALL_PROBE(sys_link);
	INIT_SYSCALL_PROBE(sys_unlink);
	INIT_CUSTOM_SYSCALL_PROBE(do_execve, syscall_shhh_pre_handler);
	INIT_SYSCALL_PROBE(sys_chdir);
	INIT_SYSCALL_PROBE(sys_time);
	INIT_SYSCALL_PROBE(sys_mknod);
	INIT_SYSCALL_PROBE(sys_chmod);
	INIT_SYSCALL_PROBE(sys_lchown16);
	INIT_SYSCALL_PROBE(sys_stat);
	INIT_SYSCALL_PROBE(sys_lseek);
	INIT_SYSCALL_PROBE(sys_getpid);
	INIT_SYSCALL_PROBE(sys_mount);
	INIT_SYSCALL_PROBE(sys_oldumount);
	INIT_SYSCALL_PROBE(sys_setuid16);
	INIT_SYSCALL_PROBE(sys_getuid16);
	INIT_SYSCALL_PROBE(sys_stime);
	INIT_SYSCALL_PROBE(sys_ptrace);
	INIT_SYSCALL_PROBE(sys_alarm);
	INIT_SYSCALL_PROBE(sys_fstat);
	INIT_SYSCALL_PROBE(sys_pause);
	INIT_SYSCALL_PROBE(sys_utime);
	INIT_CUSTOM_SYSCALL_PROBE(sys_access, syscall_sihh_pre_handler);
	INIT_SYSCALL_PROBE(sys_nice);
	INIT_SYSCALL_PROBE(sys_sync);
	INIT_SYSCALL_PROBE(sys_kill);
	INIT_SYSCALL_PROBE(sys_rename);
	INIT_SYSCALL_PROBE(sys_mkdir);
	INIT_SYSCALL_PROBE(sys_rmdir);
	INIT_CUSTOM_SYSCALL_PROBE(sys_dup, syscall_ihhh_pre_handler);
	INIT_SYSCALL_PROBE(sys_pipe);
	INIT_SYSCALL_PROBE(sys_times);
	INIT_SYSCALL_PROBE(sys_brk);
	INIT_SYSCALL_PROBE(sys_setgid16);
	INIT_SYSCALL_PROBE(sys_getgid16);
	INIT_SYSCALL_PROBE(sys_signal);
	INIT_SYSCALL_PROBE(sys_geteuid16);
	INIT_SYSCALL_PROBE(sys_getegid16);
	INIT_SYSCALL_PROBE(sys_acct);
	INIT_SYSCALL_PROBE(sys_umount);
	INIT_CUSTOM_SYSCALL_PROBE(sys_ioctl, syscall_ihhh_pre_handler);
	INIT_SYSCALL_PROBE(sys_fcntl);
	INIT_SYSCALL_PROBE(sys_setpgid);
	INIT_SYSCALL_PROBE(sys_umask);
	INIT_SYSCALL_PROBE(sys_chroot);
	INIT_SYSCALL_PROBE(sys_ustat);
	INIT_CUSTOM_SYSCALL_PROBE(sys_dup2, syscall_iihh_pre_handler);
	INIT_SYSCALL_PROBE(sys_getppid);
	INIT_SYSCALL_PROBE(sys_getpgrp);
	INIT_SYSCALL_PROBE(sys_setsid);
	INIT_SYSCALL_PROBE(sys_sigaction);
	INIT_SYSCALL_PROBE(sys_sgetmask);
	INIT_SYSCALL_PROBE(sys_ssetmask);
	INIT_SYSCALL_PROBE(sys_setreuid16);
	INIT_SYSCALL_PROBE(sys_setregid16);
	INIT_SYSCALL_PROBE(sys_sigsuspend);
	INIT_SYSCALL_PROBE(sys_sigpending);
	INIT_SYSCALL_PROBE(sys_sethostname);
	INIT_SYSCALL_PROBE(sys_setrlimit);
	INIT_SYSCALL_PROBE(sys_old_getrlimit);
	INIT_SYSCALL_PROBE(sys_getrusage);
	INIT_SYSCALL_PROBE(sys_gettimeofday);
	INIT_SYSCALL_PROBE(sys_settimeofday);
	INIT_SYSCALL_PROBE(sys_getgroups16);
	INIT_SYSCALL_PROBE(sys_setgroups16);
	INIT_SYSCALL_PROBE(sys_symlink);
	INIT_SYSCALL_PROBE(sys_lstat);
	INIT_SYSCALL_PROBE(sys_readlink);
	INIT_SYSCALL_PROBE(sys_uselib);
	INIT_SYSCALL_PROBE(sys_swapon);
	INIT_SYSCALL_PROBE(sys_reboot);
	INIT_SYSCALL_PROBE(sys_old_readdir);
	INIT_CUSTOM_SYSCALL_PROBE(old_mmap, syscall_hiih_pre_handler);
	INIT_CUSTOM_SYSCALL_PROBE(sys_munmap, syscall_hihh_pre_handler);
	INIT_SYSCALL_PROBE(sys_truncate);
	INIT_SYSCALL_PROBE(sys_ftruncate);
	INIT_SYSCALL_PROBE(sys_fchmod);
	INIT_SYSCALL_PROBE(sys_fchown16);
	INIT_SYSCALL_PROBE(sys_getpriority);
	INIT_SYSCALL_PROBE(sys_setpriority);
	INIT_SYSCALL_PROBE(sys_statfs);
	INIT_SYSCALL_PROBE(sys_fstatfs);
	INIT_SYSCALL_PROBE(sys_socketcall);
	INIT_SYSCALL_PROBE(sys_syslog);
	INIT_SYSCALL_PROBE(sys_setitimer);
	INIT_SYSCALL_PROBE(sys_getitimer);
	INIT_SYSCALL_PROBE(sys_newstat);
	INIT_SYSCALL_PROBE(sys_newlstat);
	INIT_SYSCALL_PROBE(sys_newfstat);
	INIT_SYSCALL_PROBE(sys_uname);
	INIT_SYSCALL_PROBE(sys_vhangup);
	INIT_SYSCALL_PROBE(sys_wait4);
	INIT_SYSCALL_PROBE(sys_swapoff);
	INIT_SYSCALL_PROBE(sys_sysinfo);
	INIT_SYSCALL_PROBE(sys_ipc);
	INIT_SYSCALL_PROBE(sys_fsync);
	INIT_SYSCALL_PROBE(sys_sigreturn);
	INIT_SYSCALL_PROBE(sys_clone);
	INIT_SYSCALL_PROBE(sys_setdomainname);
	INIT_SYSCALL_PROBE(sys_newuname);
	INIT_SYSCALL_PROBE(sys_cacheflush);
	INIT_SYSCALL_PROBE(sys_adjtimex);
	INIT_SYSCALL_PROBE(sys_mprotect);
	INIT_SYSCALL_PROBE(sys_sigprocmask);
	INIT_SYSCALL_PROBE(sys_init_module);
	INIT_SYSCALL_PROBE(sys_delete_module);
	INIT_SYSCALL_PROBE(sys_quotactl);
	INIT_SYSCALL_PROBE(sys_getpgid);
	INIT_SYSCALL_PROBE(sys_fchdir);
	INIT_SYSCALL_PROBE(sys_bdflush);
	INIT_SYSCALL_PROBE(sys_sysfs);
	INIT_SYSCALL_PROBE(sys_personality);
	INIT_SYSCALL_PROBE(sys_setfsuid16);
	INIT_SYSCALL_PROBE(sys_setfsgid16);
	INIT_SYSCALL_PROBE(sys_llseek);
	INIT_SYSCALL_PROBE(sys_getdents);
	INIT_SYSCALL_PROBE(sys_select);
	INIT_SYSCALL_PROBE(sys_flock);
	INIT_SYSCALL_PROBE(sys_msync);
	INIT_SYSCALL_PROBE(sys_readv);
	INIT_SYSCALL_PROBE(sys_writev);
	INIT_SYSCALL_PROBE(sys_getsid);
	INIT_SYSCALL_PROBE(sys_fdatasync);
	INIT_SYSCALL_PROBE(sys_sysctl);
	INIT_SYSCALL_PROBE(sys_mlock);
	INIT_SYSCALL_PROBE(sys_munlock);
	INIT_SYSCALL_PROBE(sys_mlockall);
	INIT_SYSCALL_PROBE(sys_munlockall);
	INIT_SYSCALL_PROBE(sys_sched_setparam);
	INIT_SYSCALL_PROBE(sys_sched_getparam);
	INIT_SYSCALL_PROBE(sys_sched_setscheduler);
	INIT_SYSCALL_PROBE(sys_sched_getscheduler);
	INIT_SYSCALL_PROBE(sys_sched_yield);
	INIT_SYSCALL_PROBE(sys_sched_get_priority_max);
	INIT_SYSCALL_PROBE(sys_sched_get_priority_min);
	INIT_SYSCALL_PROBE(sys_sched_rr_get_interval);
	INIT_SYSCALL_PROBE(sys_nanosleep);
	INIT_SYSCALL_PROBE(sys_mremap);
	INIT_SYSCALL_PROBE(sys_setresuid16);
	INIT_SYSCALL_PROBE(sys_getresuid16);
	INIT_SYSCALL_PROBE(sys_poll);
	INIT_SYSCALL_PROBE(sys_nfsservctl);
	INIT_SYSCALL_PROBE(sys_setresgid16);
	INIT_SYSCALL_PROBE(sys_getresgid16);
	INIT_CUSTOM_SYSCALL_PROBE(sys_prctl, syscall_prctl_pre_handler);
	INIT_SYSCALL_PROBE(sys_rt_sigreturn);
	INIT_SYSCALL_PROBE(sys_rt_sigaction);
	INIT_SYSCALL_PROBE(sys_rt_sigprocmask);
	INIT_SYSCALL_PROBE(sys_rt_sigpending);
	INIT_SYSCALL_PROBE(sys_rt_sigtimedwait);
	INIT_SYSCALL_PROBE(sys_rt_sigqueueinfo);
	INIT_SYSCALL_PROBE(sys_rt_sigsuspend);
	INIT_SYSCALL_PROBE(sys_pread_wrapper);
	INIT_SYSCALL_PROBE(sys_pwrite_wrapper);
	INIT_SYSCALL_PROBE(sys_chown16);
	INIT_SYSCALL_PROBE(sys_getcwd);
	INIT_SYSCALL_PROBE(sys_capget);
	INIT_SYSCALL_PROBE(sys_capset);
	INIT_SYSCALL_PROBE(sys_sigaltstack);
	INIT_SYSCALL_PROBE(sys_sendfile);
	INIT_SYSCALL_PROBE(sys_vfork);
	INIT_SYSCALL_PROBE(sys_getrlimit);
	INIT_CUSTOM_SYSCALL_PROBE(sys_mmap2, syscall_hiih_pre_handler);
	INIT_SYSCALL_PROBE(sys_truncate64);
	INIT_SYSCALL_PROBE(sys_ftruncate64);
	INIT_SYSCALL_PROBE(sys_stat64);
	INIT_SYSCALL_PROBE(sys_lstat64);
	INIT_SYSCALL_PROBE(sys_fstat64);
	INIT_SYSCALL_PROBE(sys_lchown);
	INIT_SYSCALL_PROBE(sys_getuid);
	INIT_SYSCALL_PROBE(sys_getgid);
	INIT_SYSCALL_PROBE(sys_geteuid);
	INIT_SYSCALL_PROBE(sys_getegid);
	INIT_SYSCALL_PROBE(sys_setreuid);
	INIT_SYSCALL_PROBE(sys_setregid);
	INIT_SYSCALL_PROBE(sys_getgroups);
	INIT_SYSCALL_PROBE(sys_setgroups);
	INIT_SYSCALL_PROBE(sys_fchown);
	INIT_SYSCALL_PROBE(sys_setresuid);
	INIT_SYSCALL_PROBE(sys_getresuid);
	INIT_SYSCALL_PROBE(sys_setresgid);
	INIT_SYSCALL_PROBE(sys_getresgid);
	INIT_SYSCALL_PROBE(sys_chown);
	INIT_SYSCALL_PROBE(sys_setuid);
	INIT_SYSCALL_PROBE(sys_setgid);
	INIT_SYSCALL_PROBE(sys_setfsuid);
	INIT_SYSCALL_PROBE(sys_setfsgid);
	INIT_SYSCALL_PROBE(sys_pivot_root);
	INIT_SYSCALL_PROBE(sys_mincore);
	INIT_SYSCALL_PROBE(sys_madvise);
	INIT_SYSCALL_PROBE(sys_getdents64);
	INIT_CUSTOM_SYSCALL_PROBE(sys_fcntl64, syscall_ihhh_pre_handler);
	INIT_SYSCALL_PROBE(sys_gettid);
	INIT_SYSCALL_PROBE(sys_readahead);
	INIT_SYSCALL_PROBE(sys_setxattr);
	INIT_SYSCALL_PROBE(sys_lsetxattr);
	INIT_SYSCALL_PROBE(sys_fsetxattr);
	INIT_SYSCALL_PROBE(sys_getxattr);
	INIT_SYSCALL_PROBE(sys_lgetxattr);
	INIT_SYSCALL_PROBE(sys_fgetxattr);
	INIT_SYSCALL_PROBE(sys_listxattr);
	INIT_SYSCALL_PROBE(sys_llistxattr);
	INIT_SYSCALL_PROBE(sys_flistxattr);
	INIT_SYSCALL_PROBE(sys_removexattr);
	INIT_SYSCALL_PROBE(sys_lremovexattr);
	INIT_SYSCALL_PROBE(sys_fremovexattr);
	INIT_SYSCALL_PROBE(sys_tkill);
	INIT_SYSCALL_PROBE(sys_sendfile64);
	INIT_SYSCALL_PROBE(sys_futex);
	create_tracepoint(set, "hash_futex", hash_futex_handler, NULL);
	INIT_SYSCALL_PROBE(sys_sched_setaffinity);
	INIT_SYSCALL_PROBE(sys_sched_getaffinity);
	INIT_SYSCALL_PROBE(sys_io_setup);
	INIT_SYSCALL_PROBE(sys_io_destroy);
	INIT_SYSCALL_PROBE(sys_io_getevents);
	INIT_SYSCALL_PROBE(sys_io_submit);
	INIT_SYSCALL_PROBE(sys_io_cancel);
	INIT_SYSCALL_PROBE(sys_fadvise64);
	INIT_SYSCALL_PROBE(sys_exit_group);
	INIT_SYSCALL_PROBE(sys_lookup_dcookie);
	INIT_SYSCALL_PROBE(sys_epoll_create);
	INIT_SYSCALL_PROBE(sys_epoll_ctl);
	INIT_SYSCALL_PROBE(sys_epoll_wait);
	INIT_SYSCALL_PROBE(sys_remap_file_pages);
	INIT_SYSCALL_PROBE(sys_set_tid_address);
	INIT_SYSCALL_PROBE(sys_timer_create);
	INIT_SYSCALL_PROBE(sys_timer_settime);
	INIT_SYSCALL_PROBE(sys_timer_gettime);
	INIT_SYSCALL_PROBE(sys_timer_getoverrun);
	INIT_SYSCALL_PROBE(sys_timer_delete);
	INIT_SYSCALL_PROBE(sys_clock_settime);
	INIT_SYSCALL_PROBE(sys_clock_gettime);
	INIT_SYSCALL_PROBE(sys_clock_getres);
	INIT_SYSCALL_PROBE(sys_clock_nanosleep);
	INIT_SYSCALL_PROBE(sys_statfs64);
	INIT_SYSCALL_PROBE(sys_fstatfs64);
	INIT_SYSCALL_PROBE(sys_tgkill);
	INIT_SYSCALL_PROBE(sys_utimes);
	INIT_SYSCALL_PROBE(sys_fadvise64_64_wrapper);
	INIT_SYSCALL_PROBE(sys_mq_open);
	INIT_SYSCALL_PROBE(sys_mq_unlink);
	INIT_SYSCALL_PROBE(sys_mq_timedsend);
	INIT_SYSCALL_PROBE(sys_mq_timedreceive);
	INIT_SYSCALL_PROBE(sys_mq_notify);
	INIT_SYSCALL_PROBE(sys_mq_getsetattr);
	INIT_SYSCALL_PROBE(sys_kexec_load);
	INIT_SYSCALL_PROBE(sys_waitid);
	INIT_SYSCALL_PROBE(sys_add_key);
	INIT_SYSCALL_PROBE(sys_request_key);
	INIT_SYSCALL_PROBE(sys_keyctl);
	INIT_SYSCALL_PROBE(sys_ioprio_set);
	INIT_SYSCALL_PROBE(sys_ioprio_get);
	INIT_SYSCALL_PROBE(sys_inotify_init);
	INIT_SYSCALL_PROBE(sys_inotify_add_watch);
	INIT_SYSCALL_PROBE(sys_inotify_rm_watch);
	INIT_SYSCALL_PROBE(sys_migrate_pages);
	INIT_SYSCALL_PROBE(sys_openat);
	INIT_SYSCALL_PROBE(sys_mkdirat);
	INIT_SYSCALL_PROBE(sys_mknodat);
	INIT_SYSCALL_PROBE(sys_fchownat);
	INIT_SYSCALL_PROBE(sys_futimesat);
	INIT_SYSCALL_PROBE(sys_fstatat64);
	INIT_SYSCALL_PROBE(sys_unlinkat);
	INIT_SYSCALL_PROBE(sys_renameat);
	INIT_SYSCALL_PROBE(sys_linkat);
	INIT_SYSCALL_PROBE(sys_symlinkat);
	INIT_SYSCALL_PROBE(sys_readlinkat);
	INIT_SYSCALL_PROBE(sys_fchmodat);
	INIT_SYSCALL_PROBE(sys_faccessat);
	INIT_SYSCALL_PROBE(sys_unshare);
	INIT_SYSCALL_PROBE(sys_set_robust_list);
	INIT_SYSCALL_PROBE(sys_get_robust_list);
	INIT_SYSCALL_PROBE(sys_splice);
	INIT_SYSCALL_PROBE(sys_sync_file_range);
	INIT_SYSCALL_PROBE(sys_tee);
	INIT_SYSCALL_PROBE(sys_vmsplice);
}

static void init_memory_logging(void)
{
	tracepoint_set_t *set = create_tracepoint_set("memory_events");
	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create memory tracepoint set.\n");
		return;
	}

	create_tracepoint(set, "__kmalloc", kmalloc_pre_handler,
			  kmalloc_rp_handler);
	create_tracepoint(set, "kfree", kfree_pre_handler, NULL);
	create_tracepoint(set, "do_page_fault", do_page_fault_pre_handler,
			  NULL);
	create_tracepoint(set, "vmalloc", vmalloc_pre_handler,
			  vmalloc_rp_handler);
	create_tracepoint(set, "vfree", vfree_pre_handler, NULL);
	create_tracepoint(set, "__get_free_pages", get_free_pages_pre_handler,
			  get_free_pages_rp_handler);
	create_tracepoint(set, "__alloc_pages_internal",
			  alloc_pages_pre_handler, alloc_pages_rp_handler);
	create_tracepoint(set, "free_pages", free_pages_pre_handler, NULL);
	create_tracepoint(set, "kmem_cache_alloc", kmem_cache_alloc_pre_handler,
			  kmem_cache_alloc_rp_handler);
	create_tracepoint(set, "kmem_cache_free", kmem_cache_free_pre_handler,
			  NULL);
#ifdef CONFIG_BPA2
	create_tracepoint(set, "__bigphysarea_alloc_pages",
			  bigphysarea_alloc_pages_pre_handler,
			  bigphysarea_alloc_pages_rp_handler);
	create_tracepoint(set, "bigphysarea_free_pages",
			  bigphysarea_free_pages_pre_handler, NULL);
	create_tracepoint(set, "__bpa2_alloc_pages",
			  bpa2_alloc_pages_pre_handler,
			  bpa2_alloc_pages_rp_handler);
	create_tracepoint(set, "bpa2_free_pages",
			  bpa2_free_pages_pre_handler, NULL);
#endif
}

static void init_network_logging(void)
{
	tracepoint_set_t *set = create_tracepoint_set("network_events");
	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create network tracepoint set.\n");
		return;
	}

	create_tracepoint(set, "netif_receive_skb",
			  netif_receive_skb_pre_handler,
			  netif_receive_skb_rp_handler);
	create_tracepoint(set, "dev_queue_xmit", dev_queue_xmit_pre_handler,
			  dev_queue_xmit_rp_handler);
	create_tracepoint(set, "sock_sendmsg", sock_sendmsg_pre_handler,
			  sock_sendmsg_rp_handler);
	create_tracepoint(set, "sock_recvmsg", sock_recvmsg_pre_handler,
			  sock_recvmsg_rp_handler);
}

static void init_timer_logging(void)
{
	tracepoint_set_t *set = create_tracepoint_set("timer_events");
	if (!set) {
		printk(KERN_WARNING
		       "kptrace: unable to create timer tracepoint set.\n");
		return;
	}

	create_tracepoint(set, "do_setitimer", do_setitimer_pre_handler,
			  do_setitimer_rp_handler);
	create_tracepoint(set, "it_real_fn", it_real_fn_pre_handler,
			  it_real_fn_rp_handler);
	create_tracepoint(set, "run_timer_softirq",
			  softirq_pre_handler, softirq_rp_handler);
	create_tracepoint(set, "try_to_wake_up", wake_pre_handler, NULL);
}

#ifdef CONFIG_KPTRACE_SYNC
static void init_synchronization_logging(void)
{
	tracepoint_set_t *set = create_tracepoint_set("synchronization_events");
	if (!set) {
		printk(KERN_WARNING
		       "kptrace: unable to create synchronization tracepoint "
		       "set.\n");
		return;
	}

	create_late_tracepoint(set, "mutex_lock", mutex_lock_pre_handler, NULL);
	create_late_tracepoint(set, "mutex_unlock", mutex_unlock_pre_handler,
			       NULL);

	create_tracepoint(set, "lock_kernel", lock_kernel_pre_handler, NULL);
	create_tracepoint(set, "unlock_kernel", unlock_kernel_pre_handler,
			  NULL);

	create_tracepoint(set, "down", down_pre_handler, NULL);
	create_tracepoint(set, "down_interruptible",
			  down_interruptible_pre_handler, NULL);
	create_tracepoint(set, "down_trylock", down_trylock_pre_handler,
			  down_trylock_rp_handler);
	create_tracepoint(set, "up", up_pre_handler, NULL);
	create_tracepoint(set, "__up", underscore_up_pre_handler, NULL);

	create_tracepoint(set, "down_read", down_read_pre_handler, NULL);
	create_tracepoint(set, "down_read_trylock",
			  down_read_trylock_pre_handler,
			  down_read_trylock_rp_handler);
	create_tracepoint(set, "down_write", down_write_pre_handler, NULL);
	create_tracepoint(set, "down_write_trylock",
			  down_write_trylock_pre_handler,
			  down_write_trylock_rp_handler);
	create_tracepoint(set, "up_read", up_read_pre_handler, NULL);
	create_tracepoint(set, "up_write", up_write_pre_handler, NULL);
}
#endif

/**
 *	remove_channel_controls - removes produced/consumed control files
 */
static void remove_channel_controls(void)
{
	if (produced_control) {
		debugfs_remove(produced_control);
		produced_control = NULL;
	}

	if (consumed_control) {
		debugfs_remove(consumed_control);
		consumed_control = NULL;
	}
}

/**
 *	create_channel_controls - creates produced/consumed control files
 *
 *	Returns 1 on success, 0 otherwise.
 */
static int create_channel_controls(struct dentry *parent,
				   const char *base_filename,
				   struct rchan *chan)
{
	char *tmpname = kmalloc(NAME_MAX + 1, GFP_KERNEL);
	if (!tmpname)
		return 0;

	sprintf(tmpname, "%s0.produced", base_filename);
	produced_control = debugfs_create_file(tmpname, 0444, parent,
					       chan->buf[0], &produced_fops);
	if (!produced_control) {
		printk(KERN_WARNING "Couldn't create relay control file %s\n",
		       tmpname);
		goto cleanup_control_files;
	}

	sprintf(tmpname, "%s0.consumed", base_filename);
	consumed_control = debugfs_create_file(tmpname, 0644, parent,
					       chan->buf[0], &consumed_fops);
	if (!consumed_control) {
		printk(KERN_WARNING "Couldn't create relay control file %s.\n",
		       tmpname);
		goto cleanup_control_files;
	}

	kfree(tmpname);
	return 1;

cleanup_control_files:
	kfree(tmpname);
	remove_channel_controls();
	return 0;
}

/*
 * subbuf_start() relay callback.
 */
static int subbuf_start_handler(struct rchan_buf *buf,
				void *subbuf,
				void *prev_subbuf, unsigned int prev_padding)
{
	if (prev_subbuf)
		*((unsigned *)prev_subbuf) = prev_padding;

	subbuf_start_reserve(buf, sizeof(unsigned int));

	return 1;
}

/*
 * file_create() callback.  Creates relay file in debugfs.
 */
static struct dentry *create_buf_file_handler(const char *filename,
					      struct dentry *parent,
					      int mode,
					      struct rchan_buf *buf,
					      int *is_global)
{
	struct dentry *buf_file;

	buf_file = debugfs_create_file(filename, mode, parent, buf,
				       &relay_file_operations);

	return buf_file;
}

/*
 * file_remove() default callback.  Removes relay file in debugfs.
 */
static int remove_buf_file_handler(struct dentry *dentry)
{
	debugfs_remove(dentry);

	return 0;
}

/*
 * relay callbacks
 */
static struct rchan_callbacks relay_callbacks = {
	.subbuf_start = subbuf_start_handler,
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

/**
 *	create_channel - creates channel /debug/kptrace/trace0
 *
 *	Creates channel along with associated produced/consumed control files
 *
 *	Returns channel on success, NULL otherwise
 */
static struct rchan *create_channel(unsigned subbuf_size, unsigned n_subbufs)
{
	struct rchan *tmpchan;

	tmpchan = relay_open("trace", dir, subbuf_size, n_subbufs,
			     &relay_callbacks, NULL);

	if (!tmpchan) {
		printk(KERN_WARNING "relay app channel creation failed\n");
		return NULL;
	}

	if (!create_channel_controls(dir, "trace", tmpchan)) {
		relay_close(tmpchan);
		printk(KERN_WARNING
		       "kptrace: unable to create relayfs channel\n");
		return NULL;
	}

	logging = 0;
	mappings = 0;
	suspended = 0;
	dropped = 0;

	return tmpchan;
}

/**
 *	destroy_channel - destroys channel /debug/kptrace/trace0
 *
 *	Destroys channel along with associated produced/consumed control files
 */
static void destroy_channel(void)
{
	if (chan) {
		relay_close(chan);
		chan = NULL;
	}
	remove_channel_controls();
}

/**
 *	remove_controls - removes channel management control files
 */
static void remove_controls(void)
{
	if (enabled_control)
		debugfs_remove(enabled_control);

	if (subbuf_size_control)
		debugfs_remove(subbuf_size_control);

	if (n_subbufs_control)
		debugfs_remove(n_subbufs_control);

	if (create_control)
		debugfs_remove(create_control);

	if (dropped_control)
		debugfs_remove(dropped_control);
}

/**
 *	create_controls - creates channel management control files
 *
 *	Returns 1 on success, 0 otherwise.
 */
static int create_controls(void)
{
	enabled_control = debugfs_create_file("enabled", 0, dir,
					      NULL, &enabled_fops);

	if (!enabled_control) {
		printk("Couldn't create relay control file 'enabled'.\n");
		goto fail;
	}

	subbuf_size_control = debugfs_create_file("subbuf_size", 0, dir,
						  NULL, &subbuf_size_fops);
	if (!subbuf_size_control) {
		printk("Couldn't create relay control file 'subbuf_size'.\n");
		goto fail;
	}

	n_subbufs_control = debugfs_create_file("n_subbufs", 0, dir,
						NULL, &n_subbufs_fops);
	if (!n_subbufs_control) {
		printk("Couldn't create relay control file 'n_subbufs'.\n");
		goto fail;
	}

	create_control = debugfs_create_file("create", 0, dir,
					     NULL, &create_fops);
	if (!create_control) {
		printk("Couldn't create relay control file 'create'.\n");
		goto fail;
	}

	dropped_control = debugfs_create_file("dropped", 0, dir,
					      NULL, &dropped_fops);
	if (!dropped_control) {
		printk("Couldn't create relay control file 'dropped'.\n");
		goto fail;
	}

	return 1;
fail:
	remove_controls();
	return 0;
}

/*
 * control file fileop definitions
 */

/*
 * control files for relay channel management
 */

static ssize_t enabled_read(struct file *filp, char __user * buffer,
			    size_t count, loff_t * ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d\n", logging);
	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t enabled_write(struct file *filp, const char __user * buffer,
			     size_t count, loff_t * ppos)
{
	char buf[16];
	char *tmp;
	int enabled;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	enabled = simple_strtol(buf, &tmp, 10);
	if (tmp == buf)
		return -EINVAL;

	if (enabled && chan)
		logging = 1;
	else if (!enabled) {
		logging = 0;
		if (chan)
			relay_flush(chan);
	}

	return count;
}

/*
 * 'enabled' file operations - boolean r/w
 *
 *  toggles logging to the relay channel
 */
static struct file_operations enabled_fops = {
	.owner = THIS_MODULE,
	.read = enabled_read,
	.write = enabled_write,
};

static ssize_t create_read(struct file *filp, char __user * buffer,
			   size_t count, loff_t * ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d\n", !!chan);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t create_write(struct file *filp, const char __user * buffer,
			    size_t count, loff_t * ppos)
{
	char buf[16];
	char *tmp;
	int create;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	create = simple_strtol(buf, &tmp, 10);
	if (tmp == buf)
		return -EINVAL;

	if (create) {
		destroy_channel();
		chan = create_channel(subbuf_size, n_subbufs);
		if (!chan)
			return -ENOSYS;
	} else
		destroy_channel();

	return count;
}

/*
 * 'create' file operations - boolean r/w
 *
 *  creates/destroys the relay channel
 */
static struct file_operations create_fops = {
	.owner = THIS_MODULE,
	.read = create_read,
	.write = create_write,
};

static ssize_t subbuf_size_read(struct file *filp, char __user * buffer,
				size_t count, loff_t * ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%zu\n", subbuf_size);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t subbuf_size_write(struct file *filp, const char __user * buffer,
				 size_t count, loff_t * ppos)
{
	char buf[16];
	char *tmp;
	size_t size;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	size = simple_strtol(buf, &tmp, 10);

	if (tmp == buf || size < 1 || size > KPTRACE_MAXSUBBUFSIZE)
		return -EINVAL;

	subbuf_size = size;

	return count;
}

/*
 * 'subbuf_size' file operations - r/w
 *
 *  gets/sets the subbuffer size to use in channel creation
 */
static struct file_operations subbuf_size_fops = {
	.owner = THIS_MODULE,
	.read = subbuf_size_read,
	.write = subbuf_size_write,
};

static ssize_t n_subbufs_read(struct file *filp, char __user * buffer,
			      size_t count, loff_t * ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%zu\n", n_subbufs);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t n_subbufs_write(struct file *filp, const char __user * buffer,
			       size_t count, loff_t * ppos)
{
	char buf[16];
	char *tmp;
	size_t n;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	n = simple_strtol(buf, &tmp, 10);

	if (tmp == buf || n < 1 || n > KPTRACE_MAXSUBBUFS)
		return -EINVAL;

	n_subbufs = n;

	return count;
}

/*
 * 'n_subbufs' file operations - r/w
 *
 *  gets/sets the number of subbuffers to use in channel creation
 */
static struct file_operations n_subbufs_fops = {
	.owner = THIS_MODULE,
	.read = n_subbufs_read,
	.write = n_subbufs_write,
};

static ssize_t dropped_read(struct file *filp, char __user * buffer,
			    size_t count, loff_t * ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%zu\n", dropped);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

/*
 * 'dropped' file operations - r
 *
 *  gets the number of dropped events seen
 */
static struct file_operations dropped_fops = {
	.owner = THIS_MODULE,
	.read = dropped_read,
};

/*
 * control files for relay produced/consumed sub-buffer counts
 */

static int produced_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;

	return 0;
}

static ssize_t produced_read(struct file *filp, char __user * buffer,
			     size_t count, loff_t * ppos)
{
	struct rchan_buf *buf = filp->private_data;

	return simple_read_from_buffer(buffer, count, ppos,
				       &buf->subbufs_produced,
				       sizeof(buf->subbufs_produced));
}

/*
 * 'produced' file operations - r, binary
 *
 *  There is a .produced file associated with each relay file.
 *  Reading a .produced file returns the number of sub-buffers so far
 *  produced for the associated relay buffer.
 */
static struct file_operations produced_fops = {
	.owner = THIS_MODULE,
	.open = produced_open,
	.read = produced_read
};

static int consumed_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;

	return 0;
}

static ssize_t consumed_read(struct file *filp, char __user * buffer,
			     size_t count, loff_t * ppos)
{
	struct rchan_buf *buf = filp->private_data;

	return simple_read_from_buffer(buffer, count, ppos,
				       &buf->subbufs_consumed,
				       sizeof(buf->subbufs_consumed));
}

static ssize_t consumed_write(struct file *filp, const char __user * buffer,
			      size_t count, loff_t * ppos)
{
	struct rchan_buf *buf = filp->private_data;
	size_t consumed;

	if (copy_from_user(&consumed, buffer, sizeof(consumed)))
		return -EFAULT;

	relay_subbufs_consumed(buf->chan, buf->cpu, consumed);

	return count;
}

/*
 * 'consumed' file operations - r/w, binary
 *
 *  There is a .consumed file associated with each relay file.
 *  Writing to a .consumed file adds the value written to the
 *  subbuffers-consumed count of the associated relay buffer.
 *  Reading a .consumed file returns the number of sub-buffers so far
 *  consumed for the associated relay buffer.
 */
static struct file_operations consumed_fops = {
	.owner = THIS_MODULE,
	.open = consumed_open,
	.read = consumed_read,
	.write = consumed_write,
};

/*
 * ktprace_init - initialize the relay channel and the sysfs tree
 */
static int __init kptrace_init(void)
{
	mutex_init(&kpprintf_mutex);

	dir = debugfs_create_dir("kptrace", NULL);

	if (!dir) {
		printk(KERN_ERR "Couldn't create relay app directory.\n");
		return -ENOMEM;
	}

	if (!create_controls()) {
		printk(KERN_ERR "Couldn't create debugfs files\n");
		debugfs_remove(dir);
		return -ENOMEM;
	}

	if (!create_sysfs_tree()) {
		debugfs_remove(dir);
		printk(KERN_ERR "Couldn't create sysfs tree\n");
		return -ENOSYS;
	}

	init_core_event_logging();
	init_syscall_logging();
	init_memory_logging();
	init_network_logging();
	init_timer_logging();
#ifdef CONFIG_KPTRACE_SYNC
	init_synchronization_logging();
#endif
	do_execve_addr = kallsyms_lookup_name("do_execve");

	printk(KERN_INFO "kptrace: initialised\n");

	return 0;
}

/*
 * kptrace_cleanup - free all the tracepoints and sets, remove the sysdev and
 * destroy the relay channel.
 */
static void kptrace_cleanup(void)
{
	struct list_head *p;
	tracepoint_set_t *set;
	tracepoint_t *tp;
	stop_tracing();

	list_for_each(p, &tracepoint_sets) {
		set = list_entry(p, tracepoint_set_t, list);
		if (set != NULL) {
			kobject_put(&set->kobj);
			kfree(set);
		}
	}

	list_for_each(p, &tracepoints) {
		tp = list_entry(p, tracepoint_t, list);
		if (tp != NULL) {
			kobject_put(&tp->kobj);
			kfree(tp);
		}
	}

	sysdev_class_unregister(&kptrace_sysdev);

	destroy_channel();
	remove_controls();
	if (dir)
		debugfs_remove(dir);
}

module_init(kptrace_init);
module_exit(kptrace_cleanup);
MODULE_LICENSE("GPL");
