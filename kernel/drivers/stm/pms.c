/*
 * -------------------------------------------------------------------------
 * (C) STMicroelectronics 2009
 * (C) STMicroelectronics 2010
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 * -------------------------------------------------------------------------
 * May be copied or modified under the terms of the GNU General Public
 * License v.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/stm/pms.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/parser.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/spinlock.h>

#include <linux/stm/clk.h>
#include <linux/hom.h>

#include <asm/atomic.h>

#include "../base/power/power.h"
#include "../base/base.h"

#define PMS_STATE_NAME_SIZE		24

enum pms_type {
#ifdef CONFIG_CPU_FREQ
	PMS_TYPE_CPU,
#endif
	PMS_TYPE_CLK,
	PMS_TYPE_DEV,
	PMS_TYPE_MAX
};

struct pms_object {
	int type;
	union {
		void *data;
		int cpu_id;		/* all the cpu managed by pms */
		struct clk *clk;	/* all the clock  managed by pms */
		struct device *dev;	/* all the device managed by pms */
	};
	struct list_head node;
	struct list_head constraints;	/* all the constraint on this object */
};

struct pms_state {
	struct kobject kobj;
	int is_active:1;
	char name[PMS_STATE_NAME_SIZE];
	struct list_head node;	/* the states list */
	struct list_head constraints;	/* the constraint list */
};

struct pms_constraint {
	struct pms_object *obj;	/* the constraint owner */
	struct pms_state *state;
	unsigned long value;	/* the constraint value */
	struct list_head obj_node;
	struct list_head state_node;
};

static LIST_HEAD(pms_state_list);
static DECLARE_MUTEX(pms_sem);
static spinlock_t pms_lock = __SPIN_LOCK_UNLOCKED();
static char *pms_active_buf;	/* the last command line */

struct kobject *pms_kobj;
/*
 * The PMS uses 'PMS_TYPE_MAX lists' of objects
 * if CONFIG_CPU_FREQ is defined we have:
 *   - 0. cpus
 *   - 1. clocks
 *   - 2. devices
 * else we have:
 *   - 0. clocks
 *   - 1. devices
 */
static struct list_head pms_obj_lists[PMS_TYPE_MAX] = {
	LIST_HEAD_INIT(pms_obj_lists[0]),
	LIST_HEAD_INIT(pms_obj_lists[1]),
#ifdef CONFIG_CPU_FREQ
	LIST_HEAD_INIT(pms_obj_lists[2]),
#endif

};

/*
 * Utility functions
 */

static inline char
*_strsep(char **s, const char *d)
{
	int i, len = strlen(d);
retry:
	if (!(*s) || !(**s))
		return NULL;
	for (i = 0; i < len; ++i) {
		if (**s != *(d + i))
			continue;
		++(*s);
		goto retry;
	}
	return strsep(s, d);
}

static inline int clk_is_readonly(struct clk *clk)
{
	return !clk->ops || !clk->ops->set_rate;
}

/*
 * End Utility functions
 */

static int
pms_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t ret = -EIO;
	struct kobj_attribute *k_attr
	    = container_of(attr, struct kobj_attribute, attr);
	if (k_attr->show)
		ret = k_attr->show(kobj, k_attr, buf);
	return ret;
}

static ssize_t
pms_attr_store(struct kobject *kobj, struct attribute *attr,
	       const char *buf, size_t count)
{
	ssize_t ret = -EIO;
	struct kobj_attribute *k_attr
	    = container_of(attr, struct kobj_attribute, attr);
	if (k_attr->store)
		ret = k_attr->store(kobj, k_attr, buf, count);
	return ret;
}

static struct sysfs_ops pms_sysfs_ops = {
	.show = pms_attr_show,
	.store = pms_attr_store,
};

static struct kobj_type ktype_pms = {
	.sysfs_ops = &pms_sysfs_ops,
};

static ssize_t pms_constraint_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, ret = 0;
	struct pms_state *state = (struct pms_state *)
		container_of(kobj, struct pms_state, kobj);
	struct pms_object *obj;
	struct pms_constraint *constr;

	pr_debug("\n");
	ret += sprintf(buf + ret, " -- state: %s --\n", state->name);
	for (i = 0; i < PMS_TYPE_MAX; ++i)
		list_for_each_entry(obj, &pms_obj_lists[i], node)
		    list_for_each_entry(constr, &obj->constraints, obj_node) {
		if (constr->state == state) {
			switch (obj->type) {
#ifdef CONFIG_CPU_FREQ
			case PMS_TYPE_CPU:
				ret += sprintf(buf + ret,
					       " + cpu: %10u @ %10u\n",
					       (unsigned int)obj->cpu_id,
					       (unsigned int)constr->value);
				break;
#endif
			case PMS_TYPE_CLK:
				ret += sprintf(buf + ret,
					       " + clk: %10s @ %10u\n",
					       obj->clk->name,
					       (unsigned int)constr->value);
				break;
			case PMS_TYPE_DEV:
				ret += sprintf(buf + ret,
					       " + dev: %10s is ",
					       dev_name(obj->dev));
				if (constr->value == PM_EVENT_ON)
					ret += sprintf(buf + ret, "on\n");
				else
					ret += sprintf(buf + ret, "off\n");
			}
		}
		}
	return ret;
}

static struct kobj_attribute pms_constraints = (struct kobj_attribute)
    __ATTR(constraints, S_IRWXU, pms_constraint_show, NULL);

static ssize_t pms_valids_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct pms_state *main_state = (struct pms_state *)
		container_of(kobj, struct pms_state, kobj);
	struct pms_state *statep;
	pr_debug("\n");
	list_for_each_entry(statep, &pms_state_list, node) {
		if (statep == main_state)
			continue;
		if (!pms_check_valid(main_state, statep))
			ret += sprintf(buf + ret, " + %s\n", statep->name);
	}
	return ret;
}

static struct kobj_attribute pms_valids = (struct kobj_attribute)
    __ATTR(valids, S_IRUGO, pms_valids_show, NULL);

struct pms_state *pms_state_get(const char *name)
{
	struct pms_state *statep;
	if (!name)
		return NULL;
	list_for_each_entry(statep, &pms_state_list, node)
	    if (!strcmp(name, statep->name))
		return statep;
	return NULL;
}

static struct pms_object *pms_find_object(int type, void *data)
{
	struct pms_object *obj;
	struct list_head *head;

	head = &pms_obj_lists[type];

	list_for_each_entry(obj, head, node)
	    if (obj->data == data)
		return obj;
	return NULL;
}

static int dev_match_address(struct device *dev, void *child)
{
	return (dev == (struct device *)child);
}

static inline int device_is_parent(struct device *parent, struct device *child)
{
	if (device_find_child(parent, child, dev_match_address))
		return 1;
	return 0;
}

static int pms_register_object(struct pms_object *obj)
{
	unsigned long flags;
	void *data_parent;
	int no_childs;
	struct pms_object *parent = NULL;
	struct pms_object *entry;
	struct list_head *head;

	pr_debug("\n");
	head = &pms_obj_lists[obj->type];
	switch (obj->type) {
	case PMS_TYPE_CLK:
		data_parent = (void *)obj->clk->parent;
		no_childs = list_empty(&obj->clk->children);
		break;
	case PMS_TYPE_DEV:
		data_parent = (void *)obj->dev->parent;
		no_childs = list_empty(&obj->dev->p->klist_children.k_list);
		break;
#ifdef CONFIG_CPU_FREQ
	case PMS_TYPE_CPU:
		data_parent = NULL;
		no_childs = (1 == 1);
		break;
#endif
	default:
		pr_err("[STM][PMS]: Error Object type not supported\n");
		return -1;
	}

	spin_lock_irqsave(&pms_lock, flags);
	/* objects are in a sorted list */
	/* 1. No parent... go in head */
	if (!data_parent) {
		list_add(&obj->node, head);
		goto reg_complete;
	}
	/* 2. with parent and no child... go in tail */
	if (no_childs) {
		list_add_tail(&obj->node, head);
		goto reg_complete;
	}
	/* 3. with parent and child... go after your parent */
	/* 3.1 check if the parent is registerd */
	list_for_each_entry(entry, head, node)
	    if (entry->data == data_parent) {
		parent = entry;
		break;
	}
	if (!parent)
		/* the parent isn't registered...
		   go to the head (safe for child)... */
		list_add(&obj->node, head);
	else
		/* 3.1.1 added after the parent */
		list_add(&obj->node, &parent->node);

reg_complete:
	spin_unlock_irqrestore(&pms_lock, flags);
	return 0;
}

static struct pms_object *pms_create_object(int type, void *data)
{
	struct pms_object *obj = NULL;

	pr_debug("\n");
	if (pms_find_object(type, data)) {
		pr_info("[STM][PMS]: object already registered\n");
		goto err_0;
	}

	obj = (struct pms_object *)
	    kmalloc(sizeof(struct pms_object), GFP_KERNEL);
	if (!obj)
		goto err_0;

	obj->data = data;
	obj->type = type;
	INIT_LIST_HEAD(&obj->constraints);
	if (pms_register_object(obj))
		goto err_1;
#if 0
	/* Initializes the constraints value for the state already registered */
	list_for_each_entry(statep, &pms_state_list, node) {
		struct pms_constraint *constr;
		constr = (struct pms_constraint *)
		    kmalloc(sizeof(struct pms_constraint), GFP_KERNEL);
		constr->obj = obj;
		constr->state = statep;
		list_add(&constr->obj_node, &obj->constraints);
		list_add(&constr->state_node, &statep->constraints);
		switch (type) {
		case PMS_TYPE_CLK:
			constr->value = clk_get_rate(obj->clk);
			break;
		case PMS_TYPE_DEV:
			constr->value = PM_EVENT_ON;
			break;
#ifdef CONFIG_CPU_FREQ
		case PMS_TYPE_CPU:
			constr->value =
			    cpufreq_get((unsigned int)obj->cpu_id) * 1000;
			break;
#endif
		}
	}
#endif

	return obj;
err_1:
	kfree(obj);
err_0:
	return NULL;
}

static struct pms_object *pms_check_and_add_object(int type, void *data)
{
	struct pms_object *obj;
	pr_debug("\n");
/*
 * If the object is already registerd then returns the object it-self
 */
	obj = pms_find_object(type, data);
	if (obj)
		return obj;
	return pms_create_object(type, data);
}

struct pms_object *pms_register_clock(struct clk *clk)
{
	return pms_check_and_add_object(PMS_TYPE_CLK, (void *)clk);
}
EXPORT_SYMBOL(pms_register_clock);

struct pms_object *pms_register_device(struct device *dev)
{
	return pms_check_and_add_object(PMS_TYPE_DEV, (void *)dev);
}
EXPORT_SYMBOL(pms_register_device);

struct pms_object *pms_register_cpu(int cpu_id)
{
#ifdef CONFIG_CPU_FREQ
	if (cpu_id >= NR_CPUS)
		return NULL;
	return pms_check_and_add_object(PMS_TYPE_CPU, (void *)cpu_id);
#else
	return NULL;
#endif
}
EXPORT_SYMBOL(pms_register_cpu);


struct pms_object *pms_register_clock_n(char *name)
{
	struct clk *clk;
	clk = clk_get(NULL, name);
	if (!clk) {
		pr_debug("Clock not declared\n");
		return NULL;
	}
	pr_debug("cmd_add_clk: '%s'\n", name);
	return pms_register_clock(clk);
}
EXPORT_SYMBOL(pms_register_clock_n);

struct bus_type *find_bus(char *name);

struct pms_object *pms_register_device_n(char *name)
{
	struct bus_type *bus;
	struct device *dev;
	char *bus_name, *dev_name;
	char *loc_dev_path = kzalloc(strlen(name) + 1, GFP_KERNEL);

	if (!loc_dev_path)
		return NULL;

	strncpy(loc_dev_path, name, strlen(name));
	bus_name = _strsep((char **)&loc_dev_path, "/ \n\t\0");
	if (!bus_name) {
		pr_debug("Error on bus name\n");
		goto err_0;
	}
	dev_name = _strsep((char **)&loc_dev_path, " \n\t\0");
	if (!dev_name) {
		pr_debug("Error on dev name\n");
		goto err_0;
	}
	bus = find_bus(bus_name);
	if (!bus) {
		pr_debug("Bus not declared\n");
		goto err_0;
	}
	dev = bus_find_device_by_name(bus, NULL, dev_name);
	if (!dev) {
		pr_debug("Device not found\n");
		goto err_0;
	}
	kfree(loc_dev_path);
	return pms_register_device(dev);
err_0:
	kfree(loc_dev_path);
	return NULL;
}
EXPORT_SYMBOL(pms_register_device_n);

static int pms_unregister_object(int type, void *_obj)
{
	struct pms_object *obj;
	struct pms_constraint *constraint;

	obj = pms_find_object(type, _obj);
	if (!obj)
		return -1;
	list_del(&obj->node);	/* removed in the object list */
	list_for_each_entry(constraint, &obj->constraints, obj_node) {
		list_del(&constraint->state_node);
		kfree(constraint);
	}
	kfree(obj);
	return 0;
}

int pms_unregister_clock(struct clk *clk)
{
	return pms_unregister_object(PMS_TYPE_CLK, (void *)clk);
}
EXPORT_SYMBOL(pms_unregister_clock);

int pms_unregister_device(struct device *dev)
{
	return pms_unregister_object(PMS_TYPE_DEV, (void *)dev);
}
EXPORT_SYMBOL(pms_unregister_device);

int pms_unregister_cpu(int cpu_id)
{
#ifdef CONFIG_CPU_FREQ
	return pms_unregister_object(PMS_TYPE_CPU, (void *)cpu_id);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(pms_unregister_cpu);

int pms_set_wakeup(struct pms_object *obj, int enable)
{
	struct device *dev = (struct device *)obj->data;

	if (obj->type != PMS_TYPE_DEV)
		return -EINVAL;

	if (!device_can_wakeup(dev))
		return -EINVAL;

	device_set_wakeup_enable(dev, enable);
	return 0;
}
EXPORT_SYMBOL(pms_set_wakeup);

int pms_get_wakeup(struct pms_object *obj)
{
	struct device *dev = (struct device *)obj->data;
	if (obj->type != PMS_TYPE_DEV)
		return -EINVAL;
	return device_may_wakeup(dev);
}
EXPORT_SYMBOL(pms_get_wakeup);

struct pms_state *pms_create_state(char *name)
{
	struct pms_state *state = NULL;
	unsigned long flags;

	pr_debug("\n");
	if (!name)
		return state;

	state = pms_state_get(name);
	if (state)
		return state;
	state = kzalloc(sizeof(struct pms_state), GFP_KERNEL);
	if (!state)
		return state;
	/* Initialise some fields... */
	strncpy(state->name, name, PMS_STATE_NAME_SIZE);
	INIT_LIST_HEAD(&state->constraints);
	kobject_init(&state->kobj, &ktype_pms);
	kobject_set_name(&state->kobj, name);

	if (kobject_add(&state->kobj, pms_kobj, name))
		goto error;
	/* add to the list */
	spin_lock_irqsave(&pms_lock, flags);
	list_add_tail(&state->node, &pms_state_list);
	spin_unlock_irqrestore(&pms_lock, flags);

	if (sysfs_create_file(&state->kobj, &pms_constraints.attr))
		;
	if (sysfs_create_file(&state->kobj, &pms_valids.attr))
		;
#if 0
/* Initializes the constraints value for all the
 * objects already registered
 */
	for (idx = 0; idx < PMS_TYPE_MAX; ++idx)
		list_for_each_entry(obj, pms_obj_lists[idx], node) {
		struct pms_constraint *constr;
		constr = (struct pms_constraint *)
		    kmalloc(sizeof(struct pms_constraint), GFP_KERNEL);
		constr->obj = obj;
		constr->state = state;
		list_add(&constr->obj_node, &obj->constraints);
		list_add(&constr->state_node, &state->constraints);
		switch (obj->type) {
		case PMS_TYPE_CLK:
			constr->value = clk_get_rate(obj->clk);
			break;
		case PMS_TYPE_DEV:
			constr->value = PM_EVENT_ON;
			break;
#ifdef CONFIG_CPU_FREQ
		case PMS_TYPE_CPU:
			constr->value =
			    cpufreq_get((unsigned int)obj->cpu_id) * 1000;
			break;
#endif
		}		/* switch */
		}		/* list... */
#endif
	return state;

error:
	pr_debug("Error to register the state %s\n", name);
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL(pms_create_state);

int pms_destry_state(struct pms_state *state)
{
	struct pms_constraint *constraint;
	unsigned long flags;
	if (!state)
		return -1;
	spin_lock_irqsave(&pms_lock, flags);
	list_del(&state->node);
	list_for_each_entry(constraint, &state->constraints, state_node) {
		list_del(&constraint->obj_node);
		kfree(constraint);
	}
	spin_unlock_irqrestore(&pms_lock, flags);
	kobject_del(&state->kobj);
	kfree(state);
	return 0;
}
EXPORT_SYMBOL(pms_destry_state);

int pms_set_constraint(struct pms_state *state,
		       struct pms_object *obj, unsigned long value)
{
	struct pms_constraint *constraint;
	pr_debug("\n");
	if (!obj || !state || obj->type >= PMS_TYPE_MAX)
		return -1;
#if 0
	pr_debug("state: %s - obj: %s - data: %u\n",
		  state->name,
		  (obj->type ==
		   PMS_TYPE_CLK) ? obj->clk->name : dev_name(obj->dev),
		  (unsigned int)value);
#endif
	list_for_each_entry(constraint, &state->constraints, state_node)
	    if (constraint->obj == obj) {
		constraint->value = value;
		return 0;
	}
/* 	there is no contraint already created
 *  	therefore I have to create it
 */
	pr_debug("New constraint created\n");
	constraint = (struct pms_constraint *)
	    kmalloc(sizeof(struct pms_constraint), GFP_KERNEL);
	constraint->obj = obj;
	constraint->state = state;
	constraint->value = value;
	list_add(&constraint->obj_node, &obj->constraints);
	list_add(&constraint->state_node, &state->constraints);
	return 0;
}
EXPORT_SYMBOL(pms_set_constraint);

int pms_check_valid(struct pms_state *a, struct pms_state *b)
{
	struct pms_object *obj;
	struct pms_constraint *constraint;
	struct pms_constraint *ca, *cb;
	int idx;
	pr_debug("\n");
	for (idx = 0; idx < PMS_TYPE_MAX; ++idx)
		list_for_each_entry(obj, &pms_obj_lists[idx], node) {
		ca = cb = NULL;
		list_for_each_entry(constraint, &obj->constraints, obj_node) {
			if (constraint->state == a)
				ca = constraint;
			if (constraint->state == b)
				cb = constraint;
			if (ca && cb && ca->value != cb->value)
				/* this means both the states have a
				 * contraint on the object obj
				 */
				return -EPERM;
		}
		}
	return 0;
}
EXPORT_SYMBOL(pms_check_valid);

/*
 * Check if the constraint is already taken
 */
static int pms_check_constraint(struct pms_constraint *constraint)
{
	switch (constraint->obj->type) {
	case PMS_TYPE_CLK:
		return clk_get_rate(constraint->obj->clk)
		    == constraint->value;
	case PMS_TYPE_DEV:
		return constraint->obj->dev->power.runtime_status
		    == constraint->value;
#ifdef CONFIG_CPU_FREQ
	case PMS_TYPE_CPU:
		return cpufreq_get((unsigned int)constraint->obj->cpu_id) * 1000
		    == constraint->value;
#endif
	default:
		pr_err("[STM][PMS]: pms_check_constraint: "
		       "Constraint type invalid...\n");
		return -1;
	}
	return 0;
}

static void pms_update_constraint(struct pms_constraint *constraint)
{
	struct clk *clk = constraint->obj->clk;
	struct device *dev = constraint->obj->dev;

	switch (constraint->obj->type) {
#ifdef CONFIG_CPU_FREQ
	case PMS_TYPE_CPU:{
			struct cpufreq_policy policy;
			cpufreq_get_policy(&policy,
					   (unsigned int)constraint->obj->
					   cpu_id);
			if (!strncmp(policy.governor->name, "userspace", 8))
				cpufreq_driver_target(&policy,
						      constraint->value / 1000,
						      0);
			else
				pr_err("[STM][PMS]: "
				       "Try to force a cpu rate while using"
				       " a not 'userspace' governor");
		}
		return;
#endif
	case PMS_TYPE_CLK:
		if (!constraint->value) {
			clk_disable(clk);
			return;
		}
		if (clk_get_rate(clk) == 0)
			clk_enable(clk);
		clk_set_rate(clk, constraint->value);
		return;
	case PMS_TYPE_DEV:
		if (constraint->value == RPM_ACTIVE) {
			pr_debug("[STM][PMS]: resumes device %s\n",
			       dev_name(dev));
			pm_runtime_resume(dev);
		} else {
			pm_runtime_suspend(dev);
			pr_debug("[STM][PMS]: suspends device %s\n",
			       dev_name(dev));
		}
		return;
	}			/* switch... */
	return;
}

static int pms_active_state(struct pms_state *state)
{
	struct pms_constraint *constraint;
	struct pms_object *obj;
	int idx;

	pr_debug("\n");
	if (!state) {
		pr_debug("State NULL\n");
		return -1;
	}
	/* Check for global agreement */
#if 0
	list_for_each_entry(constraintp, &new_state->clk_constr, node) {
		struct clk *clk = constraintp->obj->clk;
		if (!constraintp->value)
			ret |= clk_disable(clk, 0);
		else {
			if (clk_get_rate(clk) == 0)
				ret |= clk_enable(clk, 0);
			else
				ret |= clk_set_rate(clk, constraintp->value);
		}
		if (ret)
			return -1;
	}
#endif
	/* 1.nd step... */
	for (idx = 0; idx < PMS_TYPE_MAX; ++idx)
		list_for_each_entry(obj, &pms_obj_lists[idx], node)
		    list_for_each_entry(constraint, &obj->constraints, obj_node)
		    if (constraint->state == state &&
			!pms_check_constraint(constraint))
			pms_update_constraint(constraint);

	state->is_active = 1;
	return 0;
}

int pms_set_current_states(char *buf)
{
	char *buf0, *buf1;
	int n_state = 0, i;
	struct pms_state **new_active_states;
	struct pms_state *state;

	pr_debug("\n");
	if (!buf)
		return -1;
	pr_debug("Parsing of: %s\n", buf);

	buf0 = kmalloc(strlen(buf) + 1, GFP_KERNEL);
	strcpy(buf0, buf);
	buf1 = buf0;
	for (; _strsep(&buf1, " ;\n") != NULL; ++n_state)
		;
	pr_debug("Found %d state\n", n_state);

	new_active_states = (struct pms_state **)
	    kmalloc(sizeof(struct pms_state *) * n_state, GFP_KERNEL);
	if (!new_active_states) {
		pr_debug("No Memory\n");
		return -ENOMEM;
	}
	strcpy(buf0, buf);
	buf1 = buf0;
	for (i = 0; i < n_state; ++i) {
		new_active_states[i] = pms_state_get(_strsep(&buf1, " ;\n"));
		if (!new_active_states[i])
			goto error_pms_set_current_states;
	}
#ifdef CONFIG_PMS_CHECK_GROUP
/* before we active the states we check if
 * there is no conclict in the set it-self
 */
	{
		int j;
		for (i = 0; i < n_state - 1; ++i)
			for (j = i + 1; j < n_state; ++j)
				if (pms_check_valid(new_active_states[i],
						    new_active_states[j])) {
					pr_err("[STM][PMS]: "
					   "pms error: states invalid: %s - %s",
					       new_active_states[i]->name,
					       new_active_states[j]->name);
					goto error_pms_set_current_states;
				}
	}
#endif
	/* declare the previous set as not_active */
	list_for_each_entry(state, &pms_state_list, node)
		state->is_active = 0;

	/* now migrate to the new 'states' */
	for (i = 0; i < n_state; ++i)
		/* active the state */
		pms_active_state(*(new_active_states + i));

	kfree(pms_active_buf);
	kfree(new_active_states);
	strcpy(buf0, buf);
	pms_active_buf = buf0;
	return 0;

error_pms_set_current_states:
	pr_debug("Error in the sets of state required\n");
	kfree(buf0);
	kfree(new_active_states);
	return -EINVAL;
}
EXPORT_SYMBOL(pms_set_current_states);

char *pms_get_current_state(void)
{
	pr_debug("\n");
	return pms_active_buf;
}
EXPORT_SYMBOL(pms_get_current_state);

extern unsigned int wokenup_by;
int pms_global_standby(enum pms_standby_e state)
{
	int ret = -EINVAL;
	switch (state) {
#ifdef CONFIG_SUSPEND
	case PMS_GLOBAL_STANDBY:
	case PMS_GLOBAL_MEMSTANDBY:
		ret = pm_suspend(state == PMS_GLOBAL_STANDBY ?
				 PM_SUSPEND_STANDBY : PM_SUSPEND_MEM);
		if (ret >= 0)
			ret = (int)wokenup_by;
		break;
#endif
#ifdef CONFIG_HIBERNATION
	case PMS_GLOBAL_HIBERNATION:
		ret = hibernate();
		break;
#endif
#ifdef CONFIG_HIBERNATION_ON_MEMORY
	case PMS_GLOBAL_MEMHIBERNATION:
		ret = hibernate_on_memory();
		break;
#endif
	default:
		pr_err("PMS Error: in %s state 0x%x not supported\n",
		       __func__, state);
	}

	return ret;
}
EXPORT_SYMBOL(pms_global_standby);

#ifndef CONFIG_STM_LPC
#include <linux/rtc.h>
#endif
 int stm_lpc_set(int enable, unsigned long long tick);

 int pms_set_wakeup_timers(unsigned long long second)
 {
#ifdef CONFIG_STM_LPC
        return stm_lpc_set(1, second);
#else
       static struct rtc_device *dev;
       unsigned long secs_wake = 0;
       struct rtc_wkalrm wake_time;


       if (!dev)
               dev = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

       pr_info("%s - %d on %s\n", __func__, (int)second, dev_name(&dev->dev));
       if (!second) {
               wake_time.enabled = 0;
               rtc_tm_to_time(&wake_time.time, &secs_wake);
               return 0;
       }
       rtc_read_time(dev, &wake_time.time);
       rtc_tm_to_time(&wake_time.time, &secs_wake);
       secs_wake += second;

       rtc_time_to_tm(secs_wake, &wake_time.time);

       wake_time.enabled = 1;
       rtc_set_alarm(dev, &wake_time);

       return 0;
#endif
 }
 EXPORT_SYMBOL(pms_set_wakeup_timers);




enum {
	cmd_add_clk_constr,
	cmd_add_dev_constr,
	cmd_add_cpu_constr,
};

static match_table_t tokens = {
	{cmd_add_clk_constr, "clock_rate"},
	{cmd_add_dev_constr, "device_state"},
	{cmd_add_cpu_constr, "cpu_rate"},
};

static ssize_t pms_control_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int token;
	char *p;
	pr_debug(" count = %d\n", count);

	while ((p = _strsep((char **)&buf, " \t\n"))) {
		token = match_token(p, tokens, NULL);
		pr_debug("token: %d\n", token);
		switch (token) {
		case cmd_add_clk_constr:{
			char *clk_name;
			char *state_name;
			char *rate_name;
			unsigned long rate;
			struct clk *clk;
			struct pms_state *state;
			struct pms_object *obj;
			/* State.Clock */
			pr_debug("Adding ...cmd_add_clock_constraint\n");

			state_name = _strsep((char **)&buf, ": \t\n");
			if (!state_name)
				continue;
			clk_name = _strsep((char **)&buf, " \n\t");
			if (!clk_name)
				continue;
			rate_name = _strsep((char **)&buf, "- \t\n");
			if (!rate_name)
				continue;
			rate = simple_strtoul(rate_name, NULL, 10);
			pr_debug("cmd_add_clock_constraint '%s.%s' @ %10u\n",
			     state_name, clk_name, (unsigned int)rate);
			clk = clk_get(NULL, clk_name);
			if (!clk) {
				pr_debug("Clock not declared\n");
				continue;
			}
			state = pms_state_get(state_name);
			if (!state)
				/* create the state if required */
				state = pms_create_state(state_name);

			if (clk_is_readonly(clk)) {
				pr_debug("Clock read only\n");
				continue;
			}
			/* check if the clock is in the pms */
			obj = pms_find_object(PMS_TYPE_CLK, (void *)clk);
			if (!obj)
				/* register the clock if required */
				obj = pms_register_clock(clk);

			pms_set_constraint(state, obj, rate);
		}
			break;
		case cmd_add_dev_constr:{
			char *state_name, *bus_name;
			char *dev_name, *pmstate_name;

			struct bus_type *bus;
			struct device *dev;

			unsigned long pmstate;

			struct pms_state *state;
			struct pms_object *obj;

			/* State.Bus.Device on/off */
			pr_debug("Adding ...cmd_add_dev_constraint\n");
			state_name = _strsep((char **)&buf, ": \t\n");
			if (!state_name)
				continue;
			bus_name = _strsep((char **)&buf, "/ \n\t");
			if (!bus_name)
				continue;
			dev_name = _strsep((char **)&buf, " \n\t");
			if (!dev_name)
				continue;
			pmstate_name = _strsep((char **)&buf, " \n\t");
			if (!pmstate_name)
				continue;
			pr_debug("cmd_add_dev_constraint '%s\n", dev_name);

			bus = find_bus(bus_name);
			if (!bus) {
				pr_debug("State and/or Bus not declared\n");
				continue;
			}
			dev = bus_find_device_by_name(bus, NULL,
					dev_name);
			if (!dev) {
				pr_debug("Device not found\n");
				continue;
			}

			state = pms_state_get(state_name);
			if (!state)
				/* create the state if required */
				state = pms_create_state(state_name);

			/* check if the device is in the pms */
			obj = pms_find_object(PMS_TYPE_DEV, (void *)dev);
			if (!obj)
				/* register the devi if required */
				obj = pms_register_device(dev);

			if (!strcmp(pmstate_name, "on")) {
				pmstate = RPM_ACTIVE;
				pr_debug("Device state: %s.%s.%s state: on\n",
				     state_name, bus_name, dev_name);
			} else {
				pmstate = RPM_SUSPENDED;
				pr_debug("Device state: %s.%s.%s state: off\n",
				     state_name, bus_name, dev_name);
			}
			pms_set_constraint(state, obj, pmstate);
		}
			break;
		case cmd_add_cpu_constr:{
			char *state_name, *cpu_id_name, *rate_name;
			struct pms_state *state;
			int cpu_id;
			unsigned long rate;
			struct pms_object *obj;
			pr_debug("Adding ...cmd_add_cpu_constraint\n");
			state_name = _strsep((char **)&buf, ": \t\n");
			if (!state_name)
				continue;
			cpu_id_name = _strsep((char **)&buf, " \t\n");
			if (!cpu_id_name)
				continue;
			rate_name = _strsep((char **)&buf, " \t\n");
			if (!rate_name)
				continue;
			state = pms_state_get(state_name);
			if (!state)
				/* create the state if required */
				state = pms_create_state(state_name);
			rate = simple_strtoul(rate_name, NULL, 10);
			cpu_id = simple_strtoul(cpu_id_name, NULL, 10);
#ifdef CONFIG_CPU_FREQ
			/* check if the cpu is in the pms */
			obj = pms_find_object(PMS_TYPE_CPU, (void *)cpu_id);
			if (!obj)
				/* register the cpu if required */
				obj = pms_register_cpu(cpu_id);
			pr_debug("CPU-%d constreint @ %u MHz\n",
				  cpu_id, (unsigned int)rate);
			pms_set_constraint(state, obj, rate);
#endif
			}
			break;
		}		/* switch */
	}			/* while */
	return count;
}

static struct kobj_attribute pms_control_attr = (struct kobj_attribute)
    __ATTR(control, S_IWUSR, NULL, pms_control_store);

static ssize_t pms_current_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct pms_state *state;

	pr_debug("\n");
	list_for_each_entry(state, &pms_state_list, node)
		if (state->is_active)
			ret += sprintf(buf + ret, "%s;", state->name);
	ret += sprintf(buf + ret, "\n");
	return ret;
}

static const char *pms_global_state_n[] = {
	"pms_standby",
	"pms_memstandby",
	"pms_hibernation",
	"pms_memhibernation"
};

static ssize_t pms_current_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i;
	int tmp;
	pr_debug("\n");
	if (!buf)
		return -1;
	for (i = 0; i < ARRAY_SIZE(pms_global_state_n); ++i) {
		if (!strcmp(pms_global_state_n[i], buf)) {
			tmp = pms_global_standby((enum pms_standby_e) i);
			pr_debug("PMS: woken up by %d\n", tmp);
			return count;
		}
	}

	if (pms_set_current_states((char *)buf) < 0)
		return -1;
	return count;
}

static struct kobj_attribute pms_current_attr = (struct kobj_attribute)
__ATTR(current_state, S_IRUSR | S_IWUSR, pms_current_show, pms_current_store);

#ifdef CONFIG_STM_LPC
static ssize_t pms_timeout_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long long second = simple_strtoul(buf, NULL, 10);
	pr_info("%s\n", __func__);
	pms_set_wakeup_timers(second);
	return count;
}

static struct kobj_attribute pms_timeout_attr = (struct kobj_attribute)
	__ATTR(timeout, S_IWUSR, NULL, pms_timeout_store);
#endif

static ssize_t pms_objects_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct pms_object *obj;
	int i, ret = 0;
	for (i = 0; i < PMS_TYPE_MAX; ++i)
		list_for_each_entry(obj, &pms_obj_lists[i], node)
		    switch (obj->type) {
#ifdef CONFIG_CPU_FREQ
		case PMS_TYPE_CPU:
			ret += sprintf(buf + ret, " + cpu: %10u\n",
				       (unsigned int)obj->cpu_id);
			break;
#endif
		case PMS_TYPE_CLK:
			ret += sprintf(buf + ret, " + clk: %10s\n",
				       obj->clk->name);
			break;
		case PMS_TYPE_DEV:
			ret += sprintf(buf + ret, " + dev: %10s\n",
				       dev_name(obj->dev));
			break;
		}
	return ret;
}

static struct kobj_attribute pms_objects_attr = (struct kobj_attribute)
    __ATTR(objects, S_IRUSR, pms_objects_show, NULL);


static struct attribute *pms_attrs[] = {
	&pms_current_attr.attr,
	&pms_control_attr.attr,
	&pms_objects_attr.attr,
#ifdef CONFIG_STM_LPC
	&pms_timeout_attr.attr,
#endif
	NULL
};

static struct attribute_group pms_attr_group = {
	.attrs = pms_attrs,
	.name = "attributes"
};

static int __init pms_init(void)
{
	pr_debug("pms initialization\n");

	pms_kobj = kobject_create_and_add("pms", NULL);

	if (!pms_kobj)
		return -ENOMEM;

	pms_kobj->ktype = &ktype_pms;
	sysfs_update_group(pms_kobj, &pms_attr_group);

	return 0;
}

subsys_initcall(pms_init);
