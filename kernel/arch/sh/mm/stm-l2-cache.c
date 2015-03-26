/*
 * arch/sh/mm/stm-l2-cache.c
 *
 * Copyright (C) 2008 STMicroelectronics
 *
 * Authors: Richard P. Curnow
 *          Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/init.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/threads.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/cache.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/uaccess.h>
#include <asm/addrspace.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>
#include <asm/stm-l2-cache.h>



#define L2VCR  0x00
#define L2CFG  0x04
#define L2CCR  0x08
#define L2SYNC 0x0c
#define L2IA   0x10
#define L2FA   0x14
#define L2PA   0x18
#define L2FE   0x24
#define L2IS   0x30

#define L2PMC    0x70
#define L2ECO    0x74
#define L2CCO    0x78
#define L2ECA(n) (0x100 + (n * 4))
#define L2CCA(n) (0x180 + (n * 8))



enum stm_l2_mode {
	MODE_BYPASS,
	MODE_WRITE_THROUGH,
	MODE_COPY_BACK,
	MODE_LAST
};



static void *stm_l2_base;
static int stm_l2_block_size;
static int stm_l2_n_sets;
static int stm_l2_n_ways;
static enum stm_l2_mode stm_l2_current_mode = MODE_BYPASS;
static DEFINE_SPINLOCK(stm_l2_current_mode_lock);



/* Performance informations */

#if defined(CONFIG_DEBUG_FS)

static struct stm_l2_perf_counter {
	enum { EVENT, CYCLE } type;
	int index;
	const char *name;
	const char *description;
} stm_l2_perf_counters[] = {
	{ EVENT,  0, "L32H", "32-byte Load Hit" },
	{ EVENT,  1, "L32M", "32-byte Load Miss" },
	{ EVENT,  2, "S32H", "32-byte Store Hit" },
	{ EVENT,  3, "S32M", "32-byte Store Miss" },
	{ EVENT,  4, "OLH",  "Other Load Hit" },
	{ EVENT,  5, "OLM",  "Other Load Miss" },
	{ EVENT,  6, "OSH",  "Other Store Hit" },
	{ EVENT,  7, "OSM",  "Other Store Miss" },
	{ EVENT,  8, "PFH",  "Prefetch Hit" },
	{ EVENT,  9, "PFM",  "Prefetch Miss" },
	{ EVENT, 10, "CCA",  "Cache Control by Address" },
	{ EVENT, 11, "CCE",  "Cache Control By Entry" },
	{ EVENT, 12, "CCS",  "Cache Control By Set" },
	{ EVENT, 13, "CBL",  "Copy-Back Line" },
	{ EVENT, 14, "HPM",  "Hit on Pending Miss" },
	{ CYCLE,  0, "TBC",  "Total Bus Cycles" },
	{ CYCLE,  1, "L32L", "32-byte Load Latency" },
	{ CYCLE,  2, "S32L", "32-byte Store Latency" },
	{ CYCLE,  3, "OLL",  "Other Load Latency" },
	{ CYCLE,  4, "OSL",  "Other Store Latency" },
	{ CYCLE,  5, "HPML", "Hit on Pending Miss Latency" },
};

static int stm_l2_perf_seq_printf_counter(struct seq_file *s,
		struct stm_l2_perf_counter *counter)
{
	void *address;
	long long unsigned int val64;

	switch (counter->type) {
	case EVENT:
		address = stm_l2_base + L2ECA(counter->index);
		return seq_printf(s, "%u", readl(address));
	case CYCLE:
		address = stm_l2_base + L2CCA(counter->index);
		val64 = readl(address + 4) & 0xffff;
		val64 = (val64 << 32) | readl(address);
		return seq_printf(s, "%llu", val64);
	}
	BUG();
	return -EFAULT;
}

static int stm_l2_perf_get_overflow(struct stm_l2_perf_counter *counter)
{
	void *address;

	switch (counter->type) {
	case EVENT:
		address = stm_l2_base + L2ECO;
		break;
	case CYCLE:
		address = stm_l2_base + L2CCO;
		break;
	default:
		BUG();
		return -EFAULT;
	}

	return !!(readl(address) & (1 << counter->index));
}



static ssize_t stm_l2_perf_enabled_read(struct file *file,
		char __user *buf, size_t count, loff_t *ppos)
{
	char status[] = " \n";

	if (readl(stm_l2_base + L2PMC) & 1)
		status[0] = 'Y';
	else
		status[0] = 'N';

	return simple_read_from_buffer(buf, count, ppos, status, 2);

}

static ssize_t stm_l2_perf_enabled_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	char value[] = " \n";

	if (copy_from_user(value, buf, min(sizeof(value), count)) != 0)
		return -EFAULT;

	if (count == 1 || (count == 2 && value[1] == '\n')) {
		switch (buf[0]) {
		case 'y':
		case 'Y':
		case '1':
			writel(1, stm_l2_base + L2PMC);
			break;
		case 'n':
		case 'N':
		case '0':
			writel(0, stm_l2_base + L2PMC);
			break;
		}
	}

	return count;
}

static const struct file_operations stm_l2_perf_enabled_fops = {
	.read = stm_l2_perf_enabled_read,
	.write = stm_l2_perf_enabled_write,
};



static ssize_t stm_l2_perf_clear_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	if (count) {
		unsigned int l2pmc;

		l2pmc = readl(stm_l2_base + L2PMC);
		l2pmc &= 1; /* only preserve enable/disable bit. */
		l2pmc |= (1<<1);
		writel(l2pmc, stm_l2_base + L2PMC);
	}
	return count;
}

static const struct file_operations stm_l2_perf_clear_fops = {
	.write = stm_l2_perf_clear_write,
};



enum stm_l2_perf_all_mode { VALUES, OVERFLOWS, VERBOSE };

static int stm_l2_perf_all_show(struct seq_file *s, void *v)
{
	enum stm_l2_perf_all_mode mode = (enum stm_l2_perf_all_mode)s->private;
	int i;

	for (i = 0; i < ARRAY_SIZE(stm_l2_perf_counters); i++) {
		struct stm_l2_perf_counter *counter = &stm_l2_perf_counters[i];

		switch (mode) {
		case VALUES:
			seq_printf(s, i ? " " : "");
			stm_l2_perf_seq_printf_counter(s, counter);
			break;
		case OVERFLOWS:
			seq_printf(s, "%s%d", i ? " " : "",
					stm_l2_perf_get_overflow(counter));
			break;
		case VERBOSE:
			seq_printf(s, i ? "\n" : "");
			seq_printf(s, "%s:\t", counter->name);
			stm_l2_perf_seq_printf_counter(s, counter);
			seq_printf(s, "%s\t(%s)",
					stm_l2_perf_get_overflow(counter) ?
					" <ovr>" : "", counter->description);
			break;
		default:
			BUG();
			return -EFAULT;
		};
	}
	seq_printf(s, "\n");

	return 0;
}

static int stm_l2_perf_all_open(struct inode *inode, struct file *file)
{
	return single_open(file, stm_l2_perf_all_show, inode->i_private);
}

static const struct file_operations stm_l2_perf_all_fops = {
	.open = stm_l2_perf_all_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};



static int stm_l2_perf_counter_show(struct seq_file *s, void *v)
{
	struct stm_l2_perf_counter *counter = s->private;

	stm_l2_perf_seq_printf_counter(s, counter);
	seq_printf(s, "\n");

	return 0;
}

static int stm_l2_perf_counter_open(struct inode *inode, struct file *file)
{
	return single_open(file, stm_l2_perf_counter_show, inode->i_private);
}

static const struct file_operations stm_l2_perf_counter_fops = {
	.open = stm_l2_perf_counter_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};



static int __init stm_l2_perf_counters_init(void)
{
	struct dentry *dir;
	int i;

	if (!stm_l2_base)
		return 0;

	dir = debugfs_create_dir("stm-l2-cache", NULL);
	if (!dir || IS_ERR(dir))
		return -ENOMEM;

	debugfs_create_file("enabled", S_IFREG | S_IRUGO | S_IWUSR,
			dir, NULL, &stm_l2_perf_enabled_fops);
	debugfs_create_file("clear", S_IFREG | S_IWUSR,
			dir, NULL, &stm_l2_perf_clear_fops);

	debugfs_create_file("all", S_IFREG | S_IRUGO,
			dir, (void *)VALUES, &stm_l2_perf_all_fops);
	debugfs_create_file("overflow", S_IFREG | S_IRUGO,
			dir, (void *)OVERFLOWS, &stm_l2_perf_all_fops);
	debugfs_create_file("verbose", S_IFREG | S_IRUGO,
			dir, (void *)VERBOSE, &stm_l2_perf_all_fops);

	for (i = 0; i < ARRAY_SIZE(stm_l2_perf_counters); i++) {
		struct stm_l2_perf_counter *counter = &stm_l2_perf_counters[i];

		debugfs_create_file(counter->name, S_IFREG | S_IRUGO,
				dir, counter, &stm_l2_perf_counter_fops);
	}

	return 0;
}
device_initcall(stm_l2_perf_counters_init);

#endif /* defined(CONFIG_DEBUG_FS) */



/* Wait for the cache to finalize all pending operations */

static void stm_l2_sync(void)
{
	writel(1, stm_l2_base + L2SYNC);
	while (readl(stm_l2_base + L2SYNC) & 1)
		cpu_relax();
}



/* Flushing interface */

static void stm_l2_flush_common(unsigned long start, int size, int is_phys,
		unsigned int l2reg)
{
	/* Any code trying to flush P4 address is definitely wrong... */
	BUG_ON(!is_phys && start >= P4SEG);

	/* Ensure L1 writeback is done before starting writeback on L2 */
	asm volatile("synco"
			: /* no output */
			: /* no input */
			: "memory");

	if (likely(is_phys || (start >= P1SEG && start < P3SEG))) {
		unsigned long phys_addr;
		unsigned long phys_end;

		if (is_phys) {
			/* Physical address given. Cool. */
			phys_addr = start;
		} else {
			/* We can assume that the memory pointed by a P1/P2
			 * virtual address is physically contiguous, as it is
			 * supposed to be kernel logical memory (not an
			 * ioremapped area) */
			BUG_ON(!virt_addr_valid(start));
			phys_addr = virt_to_phys(start);
		}
		phys_end = phys_addr + size;

		/* Round down to start of block (cache line). */
		phys_addr &= ~(stm_l2_block_size - 1);

		/* Do the honours! */
		while (phys_addr < phys_end) {
			writel(phys_addr, stm_l2_base + l2reg);
			phys_addr += stm_l2_block_size;
		}
	} else if ((start >= P3SEG && start < P4SEG) || (start < P1SEG)) {
		/* Round down to start of block (cache line). */
		unsigned long virt_addr = start & ~(stm_l2_block_size - 1);
		unsigned long virt_end = start + size;

		while (virt_addr < virt_end) {
			unsigned long phys_addr;
			unsigned long phys_end;
			pgd_t *pgd;
			pud_t *pud;
			pmd_t *pmd;
			pte_t *pte;

			/* When dealing with P1 or P3 memory, we have to go
			 * through the page directory... */
			if (start < P1SEG)
				pgd = pgd_offset(current->mm, virt_addr);
			else
				pgd = pgd_offset_k(virt_addr);
			BUG_ON(pgd_none(*pgd));
			pud = pud_offset(pgd, virt_addr);
			BUG_ON(pud_none(*pud));
			pmd = pmd_offset(pud, virt_addr);
			BUG_ON(pmd_none(*pmd));
			pte = pte_offset_kernel(pmd, virt_addr);
			BUG_ON(pte_not_present(*pte));

			/* Get the physical address */
			phys_addr = pte_val(*pte) & PTE_PHYS_MASK; /* Page */
			phys_addr += virt_addr & PAGE_MASK; /* Offset */

			/* Beginning of the next page */
			phys_end = PAGE_ALIGN(phys_addr + 1);

			while (phys_addr < phys_end && virt_addr < virt_end) {
				writel(phys_addr, stm_l2_base + l2reg);
				phys_addr += stm_l2_block_size;
				virt_addr += stm_l2_block_size;
			}
		}
	}
}

void stm_l2_flush_wback(unsigned long start, int size, int is_phys)
{
	if (!stm_l2_base)
		return;

	switch (stm_l2_current_mode) {
	case MODE_COPY_BACK:
		stm_l2_flush_common(start, size, is_phys, L2FA);
		/* Fall through */
	case MODE_WRITE_THROUGH:
		/* Since this is for the purposes of DMA, we have to
		 * guarantee that the data has all got out to memory
		 * before returning. */
		stm_l2_sync();
		break;
	case MODE_BYPASS:
		break;
	default:
		BUG();
		break;
	}

}
EXPORT_SYMBOL(stm_l2_flush_wback);

void stm_l2_flush_purge(unsigned long start, int size, int is_phys)
{
	if (!stm_l2_base)
		return;

	switch (stm_l2_current_mode) {
	case MODE_COPY_BACK:
		stm_l2_flush_common(start, size, is_phys, L2PA);
		/* Fall through */
	case MODE_WRITE_THROUGH:
		/* Since this is for the purposes of DMA, we have to
		 * guarantee that the data has all got out to memory
		 * before returning. */
		stm_l2_sync();
		break;
	case MODE_BYPASS:
		break;
	default:
		BUG();
		break;
	}
}
EXPORT_SYMBOL(stm_l2_flush_purge);

void stm_l2_flush_invalidate(unsigned long start, int size, int is_phys)
{
	if (!stm_l2_base)
		return;

	/* The L2 sync here is just belt-n-braces.  It's not required in the
	 * same way as for wback and purge, because the subsequent DMA is
	 * _from_ a device so isn't reliant on it to see the correct data.
	 * When the CPU gets to read the DMA'd-in data later, because the L2
	 * keeps the ops in-order, there is no hazard in terms of the L1 miss
	 * being serviced from the stale line in the L2.
	 *
	 * The reason I'm doing this is in case somehow a line in the L2 that's
	 * about to get invalidated gets evicted just before it in the L2 op
	 * queue and the DMA onto the same memory line has already begun.  This
	 * may actually be a non-issue (may be impossible in view of L2
	 * implementation), or is going to be at least very rare. */
	switch (stm_l2_current_mode) {
	case MODE_COPY_BACK:
	case MODE_WRITE_THROUGH:
		stm_l2_flush_common(start, size, is_phys, L2IA);
		stm_l2_sync();
		break;
	case MODE_BYPASS:
		break;
	default:
		BUG();
		break;
	}
}
EXPORT_SYMBOL(stm_l2_flush_invalidate);



/* Mode control */
static void stm_l2_invalidate(void)
{
	unsigned int i;
	unsigned long top = stm_l2_block_size * stm_l2_n_sets;

	for (i = 0; i < top; i += stm_l2_block_size)
		writel(i, stm_l2_base + L2IS);
	wmb();
	stm_l2_sync();
}

static void stm_l2_mode_write_through_to_bypass(void)
{
	/* As the cache is known to be clean, we can just shut it off then
	 * invalidate it afterwards.
	 *
	 * There is a potential risk if we have pre-empt on and we're fiddling
	 * with the cache mode from another process at the same time.   Gloss
	 * over that for now. */

	unsigned int l2ccr;
	unsigned int top, step;

	stm_l2_sync();
	l2ccr = readl(stm_l2_base + L2CCR);
	l2ccr &= ~3; /* discard CE and CBE bits */
	writel(l2ccr, stm_l2_base + L2CCR);
	stm_l2_sync();

	/* Invalidate the L2 */
	step = stm_l2_block_size;
	top = stm_l2_block_size * stm_l2_n_sets;

	stm_l2_invalidate();
}

static void stm_l2_mode_bypass_to_write_through(void)
{
	unsigned int l2ccr;

	stm_l2_sync();
	l2ccr = readl(stm_l2_base + L2CCR);
	l2ccr &= ~(1 << 1); /* discard CBE bit */
	l2ccr |= 1; /* CE */
	writel(l2ccr, stm_l2_base + L2CCR);
	wmb();
	stm_l2_sync();
}

/* stm-l2-helper.S */
void stm_l2_copy_back_to_write_through_helper(unsigned long top,
	void *l2ccr, void *l2fe, void *l2sync);

static void stm_l2_mode_copy_back_to_write_through(void)
{
	/* Have to purge with interrupts off. */
	unsigned int top;

	top = stm_l2_block_size * stm_l2_n_sets * stm_l2_n_ways;
	stm_l2_copy_back_to_write_through_helper(top, stm_l2_base + L2CCR,
			stm_l2_base + L2FE, stm_l2_base + L2SYNC);
}

static void stm_l2_mode_write_through_to_copy_back(void)
{
	unsigned int l2ccr;

	l2ccr = readl(stm_l2_base + L2CCR);
	l2ccr |= (1 << 1); /* CBE bit */
	writel(l2ccr, stm_l2_base + L2CCR);
	wmb();
}

static void stm_l2_set_mode(enum stm_l2_mode new_mode)
{
	spin_lock(&stm_l2_current_mode_lock);

	while (new_mode < stm_l2_current_mode) {
		switch (stm_l2_current_mode) {
		case MODE_WRITE_THROUGH:
			stm_l2_mode_write_through_to_bypass();
			break;
		case MODE_COPY_BACK:
			stm_l2_mode_copy_back_to_write_through();
			break;
		default:
			BUG();
			break;
		}
		stm_l2_current_mode--;
	}

	while (new_mode > stm_l2_current_mode) {
		switch (stm_l2_current_mode) {
		case MODE_BYPASS:
			stm_l2_mode_bypass_to_write_through();
			break;
		case MODE_WRITE_THROUGH:
			stm_l2_mode_write_through_to_copy_back();
			break;
		default:
			BUG();
			break;
		}
		stm_l2_current_mode++;
	}

	spin_unlock(&stm_l2_current_mode_lock);
}



/* sysfs interface */

static const char *l2_mode_name[] = {
	[MODE_BYPASS] = "bypass",
	[MODE_WRITE_THROUGH] = "write_through",
	[MODE_COPY_BACK] = "copy_back",
};

static ssize_t stm_l2_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	enum stm_l2_mode current_mode, mode;

	spin_lock(&stm_l2_current_mode_lock);
	current_mode = stm_l2_current_mode;
	spin_unlock(&stm_l2_current_mode_lock);

	for (mode = MODE_BYPASS; mode < MODE_LAST; mode++)
		len += sprintf(buf + len,
				mode == current_mode ? "[%s] " : "%s ",
				l2_mode_name[mode]);
	len += sprintf(buf + len, "\n");

	return len;
}

static ssize_t stm_l2_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	enum stm_l2_mode mode;

	for (mode = MODE_BYPASS; mode < MODE_LAST; mode++) {
		if (sysfs_streq(buf, l2_mode_name[mode])) {
			stm_l2_set_mode(mode);
			return count;
		}
	}

	return -EINVAL;
}

static struct device_attribute stm_l2_mode_attr =
	__ATTR(mode, S_IRUGO | S_IWUSR, stm_l2_mode_show, stm_l2_mode_store);

static struct attribute_group stm_l2_attr_group = {
	.name = "l2",
	.attrs = (struct attribute * []) {
		&stm_l2_mode_attr.attr,
		NULL
	},
};

static int __init stm_l2_sysfs_init(void)
{
	if (!stm_l2_base)
		return 0;

	return sysfs_create_group(mm_kobj, &stm_l2_attr_group);
}
late_initcall(stm_l2_sysfs_init);



/* Driver initialization */

static int __init stm_l2_probe(struct platform_device *pdev)
{
	struct resource *mem;
	unsigned long addr, size;
	void *base;
	unsigned int vcr;
	unsigned int cfg;
	unsigned int top, step;
	int blksz, setsz, nsets;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource!\n");
		return -EINVAL;
	}
	addr = mem->start;
	size = mem->end - mem->start + 1;
	mem = request_mem_region(addr, size, pdev->name);
	if (!mem) {
		dev_err(&pdev->dev, "Control registers already in use!");
		return -EBUSY;
	}
	base = ioremap(addr, size);
	if (!base) {
		dev_err(&pdev->dev, "Can't remap control registers!\n");
		release_mem_region(addr, size);
		return -EFAULT;
	}

	vcr = readl(base + L2VCR);
	cfg = readl(base + L2CFG);

	blksz = (cfg & 0xf);
	setsz = ((cfg >> 4) & 0xf);
	nsets = ((cfg >> 8) & 0xf);

	stm_l2_block_size = 1 << blksz;
	stm_l2_n_sets = 1 << nsets;
	stm_l2_n_ways = 1 << setsz;

	/* This is a reasonable test that the L2 is present.  We are never
	 * likely to do a L2 with a different line size. */
	if (stm_l2_block_size != 32) {
		dev_err(&pdev->dev, "Wrong line size detected, "
				"assuming no L2-Cache!\n");
		iounmap(base);
		release_mem_region(addr, size);
		return -ENODEV;
	}
	stm_l2_base = base;

	/* Invalidate the L2 */
	step = stm_l2_block_size;
	top = step << nsets;

	stm_l2_invalidate();

#if defined(CONFIG_STM_L2_CACHE_WRITETHROUGH)
	stm_l2_set_mode(MODE_WRITE_THROUGH);
#elif defined(CONFIG_STM_L2_CACHE_WRITEBACK)
	stm_l2_set_mode(MODE_COPY_BACK);
#endif

	return 0;
}

#ifdef CONFIG_HIBERNATION
static enum stm_l2_mode stm_l2_saved_mode;
static int stm_l2_freeze_noirq(struct device *dev)
{
	/*
	 * Disable the L2-cache
	 */
	stm_l2_saved_mode = stm_l2_current_mode;

	stm_l2_sync();
	stm_l2_set_mode(MODE_BYPASS);
	return 0;
}

static int stm_l2_restore_noirq(struct device *dev)
{
	stm_l2_invalidate();

	stm_l2_set_mode(stm_l2_saved_mode);
	return 0;
}

static struct dev_pm_ops stm_l2_pm = {
	.freeze_noirq = stm_l2_freeze_noirq,
	.restore_noirq = stm_l2_restore_noirq,
};

#else

static struct dev_pm_ops stm_l2_pm;

#endif

static struct platform_driver stm_l2_driver = {
	.driver.name = "stm-l2-cache",
	.driver.pm = &stm_l2_pm,
	.probe = stm_l2_probe,
};

static int __init stm_l2_init(void)
{
	return platform_driver_register(&stm_l2_driver);
}
postcore_initcall(stm_l2_init);
