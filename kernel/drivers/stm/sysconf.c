/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/bootmem.h>
#include <linux/io.h>
#include <linux/sysdev.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/stm/platform.h>
#include <linux/stm/sysconf.h>

#define DRIVER_NAME "stm-sysconf"

struct sysconf_field {
	u8 group, num;
	u8 lsb, msb;
	void __iomem *reg;
	const char *owner;
	struct list_head list;
#ifdef DEBUG
	enum { magic_good = 0x600df00d, magic_bad = 0xdeadbeef } magic;
#define MAGIC_SET(field) field->magic = magic_good
#define MAGIC_CLEAR(field) field->magic = magic_bad
#define MAGIC_CHECK(field) BUG_ON(field->magic != magic_good)
#else
#define MAGIC_SET(field)
#define MAGIC_CLEAR(field)
#define MAGIC_CHECK(field)
#endif
};

struct sysconf_group {
	void __iomem *base;
	const char *name;
	const char *(*reg_name)(int num);
	struct sysconf_block *block;
};

struct sysconf_block {
	void __iomem *base;
	unsigned long size;
	struct platform_device *pdev;
#ifdef CONFIG_PM
	unsigned long *snapshot;
#endif
};

static int sysconf_blocks_num;
static struct sysconf_block *sysconf_blocks;

static int sysconf_groups_num;
static struct sysconf_group *sysconf_groups;

static LIST_HEAD(sysconf_fields);
static DEFINE_SPINLOCK(sysconf_fields_lock);

static DEFINE_SPINLOCK(sysconf_registers_lock);



/* We need a small stash of allocations before kmalloc becomes available */
#define NUM_EARLY_FIELDS	128
#define EARLY_BITS_MAPS_SIZE	DIV_ROUND_UP(NUM_EARLY_FIELDS, BITS_PER_LONG)

static struct sysconf_field early_fields[NUM_EARLY_FIELDS];
static unsigned long early_fields_map[EARLY_BITS_MAPS_SIZE];

static struct sysconf_field *field_alloc(void)
{
	int i;

	for (i = 0; i < NUM_EARLY_FIELDS; i++)
		if (test_and_set_bit(i, early_fields_map) == 0)
			return &early_fields[i];

	return kmalloc(sizeof(struct sysconf_field), GFP_KERNEL);
}

static void field_free(struct sysconf_field *field)
{
	if (field >= early_fields && field < early_fields + NUM_EARLY_FIELDS)
		clear_bit(field - early_fields, early_fields_map);
	else
		kfree(field);
}



struct sysconf_field *sysconf_claim(int group, int num, int lsb, int msb,
				    const char *devname)
{
	struct sysconf_field *field, *entry;
	enum {
		status_searching,
		status_found_register,
		status_add_field_here,
		status_conflict,
	} status = status_searching;
	int bit_avail = 0;

	pr_debug("%s(group=%d, num=%d, lsb=%d, msb=%d, devname='%s')\n",
			__func__, group, num, lsb, msb, devname);

	BUG_ON(group < 0 || group >= sysconf_groups_num);
	BUG_ON(num < 0 || num > ((1 << 8) - 1));
	BUG_ON(lsb < 0 || lsb > 32);
	BUG_ON(msb < 0 || msb > 32);
	BUG_ON(lsb > msb);

	field = field_alloc();
	if (!field)
		return NULL;

	field->group = group;
	field->num = num;
	field->lsb = lsb;
	field->msb = msb;

	field->reg = sysconf_groups[group].base + (num * 4);
	BUG_ON(field->reg >= sysconf_groups[group].block->base +
			sysconf_groups[group].block->size);
	field->owner = devname;
	MAGIC_SET(field);

	spin_lock(&sysconf_fields_lock);

	/* The list is always in group->num->lsb/msb order, so it's easy to
	 * find a place to insert a new field (and to detect conflicts) */
	list_for_each_entry(entry, &sysconf_fields, list) {
		if (entry->group == group && entry->num == num) {
			status = status_found_register;
			/* Someone already claimed a field from this
			 * register - let's try to find some space for
			 * requested bits... */
			if (bit_avail <= lsb && msb < entry->lsb) {
				status = status_add_field_here;
				break;
			}
			bit_avail = entry->msb + 1;
		} else if ((entry->group == group && entry->num > num) ||
				entry->group > group) {
			/* Ok, there is no point of looking further -
			 * the group and/or num values are bigger then
			 * the ones we are looking for */
			if ((status == status_found_register &&
					bit_avail <= lsb) ||
					status == status_searching)
				/* A remainder of the given register is not
				 * used or the register wasn't used at all */
				status = status_add_field_here;
			else
				/* Apparently some bits of the claimed field
				 * are already in use... */
				status = status_conflict;
			break;
		}
	}

	switch (status) {
	case status_searching:
	case status_found_register:
		/* Either nothing was on the list at all or the claimed
		 * field should be added as the last element... */
		list_add_tail(&field->list, &sysconf_fields);
		break;
	case status_add_field_here:
		/* So we should insert the new field between current
		 * list entry and the previous one */
		list_add(&field->list, entry->list.prev);
		break;
	case status_conflict:
		/* Apparently there was no place in the
		 * register to fulfill the request... */
		MAGIC_CLEAR(field);
		field_free(field);
		field = NULL;
		pr_debug("%s(): conflict!\n", __func__);
		break;
	default:
		BUG();
	}

	spin_unlock(&sysconf_fields_lock);

	pr_debug("%s()=0x%p\n", __func__, field);

	return field;
}
EXPORT_SYMBOL(sysconf_claim);

void sysconf_release(struct sysconf_field *field)
{
	pr_debug("%s(field=0x%p)\n", __func__, field);

	BUG_ON(!field);
	MAGIC_CHECK(field);

	spin_lock(&sysconf_fields_lock);
	list_del(&field->list);
	spin_unlock(&sysconf_fields_lock);

	MAGIC_CLEAR(field);
	field_free(field);
}
EXPORT_SYMBOL(sysconf_release);

void sysconf_write(struct sysconf_field *field, unsigned long value)
{
	int field_bits;

	pr_debug("%s(field=0x%p (%s %d[%d:%d]) = 0x%08lx)\n", __func__, field,
		 sysconf_groups[field->group].name, field->num,
		 field->msb, field->lsb, value);

	BUG_ON(!field);
	MAGIC_CHECK(field);

	field_bits = field->msb - field->lsb + 1;
	BUG_ON(field_bits < 1 || field_bits > 32);

	if (field_bits == 32) {
		/* Operating on the whole register, nice and easy */
		writel(value, field->reg);
	} else {
		unsigned long flags;
		u32 reg_mask;
		u32 tmp;

		reg_mask = ~(((1 << field_bits) - 1) << field->lsb);

		spin_lock_irqsave(&sysconf_registers_lock, flags);
		tmp = readl(field->reg);
		tmp &= reg_mask;
		tmp |= value << field->lsb;
		writel(tmp, field->reg);
		spin_unlock_irqrestore(&sysconf_registers_lock, flags);
	}
}
EXPORT_SYMBOL(sysconf_write);

unsigned long sysconf_read(struct sysconf_field *field)
{
	int field_bits;
	u32 result;

	pr_debug("%s(field=0x%p (%s %d[%d:%d]))\n", __func__, field,
		 sysconf_groups[field->group].name, field->num,
		 field->msb, field->lsb);

	BUG_ON(!field);
	MAGIC_CHECK(field);

	field_bits = field->msb - field->lsb + 1;
	BUG_ON(field_bits < 1 || field_bits > 32);

	result = readl(field->reg);

	if (field_bits != 32) {
		result >>= field->lsb;
		result &= (1 << field_bits) - 1;
	}

	pr_debug("%s()=0x%u\n", __func__, result);

	return result;
}
EXPORT_SYMBOL(sysconf_read);

void *sysconf_address(struct sysconf_field *field)
{
	pr_debug("%s(field=0x%p)\n", __func__, field);

	BUG_ON(!field);
	MAGIC_CHECK(field);

	pr_debug("%s()=0x%p\n", __func__, field->reg);

	return field->reg;
}
EXPORT_SYMBOL(sysconf_address);

unsigned long sysconf_mask(struct sysconf_field *field)
{
	int field_bits;
	u32 result;

	pr_debug("%s(field=0x%p)\n", __func__, field);

	BUG_ON(!field);
	MAGIC_CHECK(field);

	field_bits = field->msb - field->lsb + 1;
	BUG_ON(field_bits < 1 || field_bits > 32);

	if (field_bits == 32)
		result = ~0UL;
	else
		result = ((1 << field_bits) - 1) << field->lsb;

	pr_debug("%s()=0x%u\n", __func__, result);

	return result;
}
EXPORT_SYMBOL(sysconf_mask);

const char *sysconf_group_name(int group)
{
	BUG_ON(group < 0 || group >= sysconf_groups_num);

	return sysconf_groups[group].name;

}
EXPORT_SYMBOL(sysconf_group_name);

const char *sysconf_reg_name(int group, int num)
{
	BUG_ON(group < 0 || group >= sysconf_groups_num);

	return sysconf_groups[group].reg_name ?
			sysconf_groups[group].reg_name(num) : NULL;
}
EXPORT_SYMBOL(sysconf_reg_name);


#ifdef CONFIG_PM
static int sysconf_pm_freeze(void)
{
	int result = 0;
	int i;

	pr_debug("%s()\n", __func__);

	for (i = 0; i < sysconf_blocks_num; i++) {
		struct sysconf_block *block = &sysconf_blocks[i];
		int j;

		block->snapshot = kmalloc(block->size, GFP_NOWAIT);
		if (!block->snapshot) {
			pr_err("Failed to freeze %s!\n",
			       dev_name(&block->pdev->dev));
			result = -ENOMEM;
			continue;
		}

		for (j = 0; j < block->size; j += sizeof(unsigned long))
			block->snapshot[j / sizeof(unsigned long)] =
					readl(block->base + j);
	}

	pr_debug("%s()=%d\n", __func__, result);

	return result;
}

static int sysconf_pm_restore(void)
{
	int result = 0;
	int i;

	pr_debug("%s()\n", __func__);

	for (i = 0; i < sysconf_blocks_num; i++) {
		struct sysconf_block *block = &sysconf_blocks[i];
		int j;

		if (!block->snapshot) {
			pr_err("Failed to restore %s!\n",
			       dev_name(&block->pdev->dev));
			result = -EINVAL;
			continue;
		}

		for (j = 0; j < block->size; j += sizeof(unsigned long))
			writel(block->snapshot[j / sizeof(unsigned long)],
					block->base + j);

		kfree(block->snapshot);
		block->snapshot = NULL;
	}

	pr_debug("%s()=%d\n", __func__, result);

	return result;
}

static int sysconf_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	int result = 0;
	static unsigned long prev_state = PM_EVENT_ON;

	pr_debug("%s()\n", __func__);

	switch (state.event) {
	case PM_EVENT_ON:
		if (prev_state == PM_EVENT_FREEZE)
			result = sysconf_pm_restore();
		break;
	case PM_EVENT_SUSPEND:
		break;
	case PM_EVENT_FREEZE:
		result = sysconf_pm_freeze();
		break;
	}

	prev_state = state.event;

	pr_debug("%s()=%d\n", __func__, result);

	return result;
}

static int sysconf_sysdev_resume(struct sys_device *dev)
{
	return sysconf_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_class sysconf_sysdev_class = {
	.name = "sysconf",
	.suspend = sysconf_sysdev_suspend,
	.resume = sysconf_sysdev_resume,
};

struct sys_device sysconf_sysdev_dev = {
	.id = 0,
	.cls = &sysconf_sysdev_class,
};

static int __init sysconf_sysdev_init(void)
{
	int ret;

	ret = sysdev_class_register(&sysconf_sysdev_class);
	if (ret)
		return ret;

	ret = sysdev_register(&sysconf_sysdev_dev);
	if (ret)
		return ret;

	return 0;
}

module_init(sysconf_sysdev_init);
#endif



#ifdef CONFIG_DEBUG_FS

enum sysconf_seq_state { state_blocks, state_groups, state_fields, state_last };

static void *sysconf_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= state_last)
		return NULL;

	return pos;
}

static void *sysconf_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;

	if (*pos >= state_last)
		return NULL;

	return pos;
}

static void sysconf_seq_stop(struct seq_file *s, void *v)
{
}

static int sysconf_seq_show_blocks(struct seq_file *s)
{
	int i;

	seq_printf(s, "blocks:\n");

	for (i = 0; i < sysconf_blocks_num; i++) {
		struct sysconf_block *block = &sysconf_blocks[i];
		struct resource *mem = platform_get_resource(block->pdev,
				IORESOURCE_MEM, 0);

		seq_printf(s, "- %s: 0x%08x (0x%p), 0x%lxb\n",
			   dev_name(&block->pdev->dev), mem->start,
			   block->base, block->size);
	}

	seq_printf(s, "\n");

	return 0;
}

static int sysconf_seq_show_groups(struct seq_file *s)
{
	int i;

	seq_printf(s, "groups:\n");

	for (i = 0; i < sysconf_groups_num; i++) {
		struct sysconf_group *group = &sysconf_groups[i];

		seq_printf(s, "- %s: 0x%p (%s)\n",
			   group->name, group->base,
			   dev_name(&group->block->pdev->dev));
	}

	seq_printf(s, "\n");

	return 0;
}

static int sysconf_seq_show_fields(struct seq_file *s)
{
	struct sysconf_field *field;

	seq_printf(s, "claimed fields:\n");

	spin_lock(&sysconf_fields_lock);

	list_for_each_entry(field, &sysconf_fields, list) {
		struct sysconf_group *group = &sysconf_groups[field->group];

		if (group->reg_name)
			seq_printf(s, "- %s[", group->reg_name(field->num));
		else
			seq_printf(s, "- %s%d[", group->name, field->num);

		if (field->msb == field->lsb)
			seq_printf(s, "%d", field->msb);
		else
			seq_printf(s, "%d:%d", field->msb, field->lsb);

		seq_printf(s, "] = 0x%0*lx (0x%p, %s)\n",
				(field->msb - field->lsb + 4) / 4,
				sysconf_read(field),
				field->reg, field->owner);
	}

	spin_unlock(&sysconf_fields_lock);

	seq_printf(s, "\n");

	return 0;
}

static int sysconf_seq_show(struct seq_file *s, void *v)
{
	enum sysconf_seq_state state = *((loff_t *)v);

	switch (state) {
	case state_blocks:
		return sysconf_seq_show_blocks(s);
	case state_groups:
		return sysconf_seq_show_groups(s);
	case state_fields:
		return sysconf_seq_show_fields(s);
	default:
		BUG();
		return -EINVAL;
	}
}

static struct seq_operations sysconf_seq_ops = {
	.start = sysconf_seq_start,
	.next = sysconf_seq_next,
	.stop = sysconf_seq_stop,
	.show = sysconf_seq_show,
};

static int sysconf_debugfs_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &sysconf_seq_ops);
}

static const struct file_operations sysconf_debugfs_ops = {
	.owner = THIS_MODULE,
	.open = sysconf_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __init sysconf_debugfs_init(void)
{
	debugfs_create_file("sysconf", S_IFREG | S_IRUGO,
			NULL, NULL, &sysconf_debugfs_ops);

	return 0;
}
subsys_initcall(sysconf_debugfs_init);

#endif



/* This is called early to allow board start up code to use sysconf
 * registers (in particular console devices). */
void __init sysconf_early_init(struct platform_device *pdevs, int pdevs_num)
{
	int i;

	pr_debug("%s(pdevs=%p, pdevs_num=%d)\n", __func__, pdevs, pdevs_num);

	/* I don't like panicing, but if we failed here, we probably
	 * don't have any way to report things have gone wrong.
	 * So a panic here at least gives some hope of being
	 * able to debug the problem. */

	sysconf_blocks_num = pdevs_num;
	sysconf_blocks = alloc_bootmem(sizeof(*sysconf_blocks) *
			sysconf_blocks_num);
	if (!sysconf_blocks)
		panic("Failed to allocate memory for sysconf blocks!");

	for (i = 0; i < sysconf_blocks_num; i++) {
		struct sysconf_block *block = &sysconf_blocks[i];
		struct stm_plat_sysconf_data *data = pdevs[i].dev.platform_data;
		struct resource *mem;

		block->pdev = &pdevs[i];

		mem = platform_get_resource(&pdevs[i], IORESOURCE_MEM, 0);
		BUG_ON(!mem);

		block->size = mem->end - mem->start + 1;
		block->base = ioremap(mem->start, block->size);
		if (!block->base)
			panic("Unable to ioremap %s registers!",
			      dev_name(&block->pdev->dev));

		sysconf_groups_num += data->groups_num;
	}

	sysconf_groups = alloc_bootmem(sizeof(*sysconf_groups) *
			sysconf_groups_num);
	if (!sysconf_groups)
		panic("Failed to allocate memory for sysconf groups!\n");

	for (i = 0; i < sysconf_blocks_num; i++) {
		struct stm_plat_sysconf_data *data = pdevs[i].dev.platform_data;
		struct sysconf_block *block = &sysconf_blocks[i];
		int j;

		for (j = 0; j < data->groups_num; j++) {
			struct stm_plat_sysconf_group *info = &data->groups[j];
			struct sysconf_group *group;

			BUG_ON(info->group < 0 ||
					info->group >= sysconf_groups_num);

			group = &sysconf_groups[info->group];

			BUG_ON(group->base != NULL);

			group->base = block->base + info->offset;
			group->name = info->name;
			group->reg_name = info->reg_name;
			group->block = block;
		}

	}
}



static int __init sysconf_probe(struct platform_device *pdev)
{
	int result = -EINVAL;
	int i;

	pr_debug("%s(pdev=%p)\n", __func__, pdev);

	/* Confirm that the device has been initialized earlier */
	for (i = 0; i < sysconf_blocks_num; i++) {
		if (sysconf_blocks[i].pdev == pdev) {
			result = 0;
			break;
		}
	}

	if (result == 0) {
		struct resource *mem = platform_get_resource(pdev,
				IORESOURCE_MEM, 0);

		BUG_ON(!mem);

		if (request_mem_region(mem->start, mem->end -
				mem->start + 1, pdev->name) == NULL) {
			pr_err("Memory region request failed for %s!\n",
			       dev_name(&pdev->dev));
			result = -EBUSY;
		}
	}

	pr_debug("%s()=%d\n", __func__, result);

	return result;
}

static struct platform_driver sysconf_driver = {
	.probe		= sysconf_probe,
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init sysconf_init(void)
{
	return platform_driver_register(&sysconf_driver);
}
arch_initcall(sysconf_init);
