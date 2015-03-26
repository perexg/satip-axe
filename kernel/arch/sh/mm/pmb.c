/*
 * arch/sh/mm/pmb.c
 *
 * Privileged Space Mapping Buffer (PMB) Support.
 *
 * Copyright (C) 2005, 2006, 2007 Paul Mundt
 *
 * P1/P2 Section mapping definitions from map32.h, which was:
 *
 *	Copyright 2003 (c) Lineo Solutions,Inc.
 *
 * Large changes to support dynamic mappings using PMB
 * Copyright (c) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/pm.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>

#if 0
#define DPRINTK(fmt, args...) printk(KERN_ERR "%s: " fmt, __FUNCTION__, ## args)
#else
#define DPRINTK(fmt, args...) do { ; } while (0)
#endif


#define NR_PMB_ENTRIES	16
#define MIN_PMB_MAPPING_SIZE	(8*1024*1024)

#ifdef CONFIG_PMB_64M_TILES
#define PMB_FIXED_SHIFT 26
#define PMB_VIRT2POS(virt) (((virt) >> PMB_FIXED_SHIFT) & (NR_PMB_ENTRIES - 1))
#define PMB_POS2VIRT(pos) (((pos) << PMB_FIXED_SHIFT) + P1SEG)
#endif

struct pmb_entry {
	unsigned long vpn;
	unsigned long ppn;
	unsigned long flags;	/* Only size */
	struct pmb_entry *next;
	unsigned long size;
	int pos;
};

struct pmb_mapping {
	unsigned long phys;
	unsigned long virt;
	unsigned long size;
	unsigned long flags;	/* Only cache etc */
	struct pmb_entry *entries;
	struct pmb_mapping *next;
	int usage;
};

static DEFINE_RWLOCK(pmb_lock);
static unsigned long pmb_map;
static struct pmb_entry   pmbe[NR_PMB_ENTRIES] __attribute__ ((__section__ (".uncached.data")));
static struct pmb_mapping pmbm[NR_PMB_ENTRIES];
static struct pmb_mapping *pmb_mappings, *pmb_mappings_free;

static __always_inline unsigned long mk_pmb_entry(unsigned int entry)
{
	return (entry & PMB_E_MASK) << PMB_E_SHIFT;
}

static __always_inline unsigned long mk_pmb_addr(unsigned int entry)
{
	return mk_pmb_entry(entry) | PMB_ADDR;
}

static __always_inline unsigned long mk_pmb_data(unsigned int entry)
{
	return mk_pmb_entry(entry) | PMB_DATA;
}

static __always_inline void __set_pmb_entry(unsigned long vpn,
	unsigned long ppn, unsigned long flags, int pos)
{
#ifdef CONFIG_CACHE_WRITETHROUGH
	/*
	 * When we are in 32-bit address extended mode, CCR.CB becomes
	 * invalid, so care must be taken to manually adjust cacheable
	 * translations.
	 */
	if (likely(flags & PMB_C))
		flags |= PMB_WT;
#endif
#ifdef CONFIG_PMB_64M_TILES
	BUG_ON(pos != PMB_VIRT2POS(vpn));
#endif
	ctrl_outl(0, mk_pmb_addr(pos));
	ctrl_outl(vpn, mk_pmb_addr(pos));
	ctrl_outl(ppn | flags | PMB_V, mk_pmb_data(pos));

	/*
	 * Read back the value just written. This shouldn't be necessary,
	 * but when resuming from hibernation it appears to fix a problem.
	 */
	ctrl_inl(mk_pmb_addr(pos));
}

static void __uses_jump_to_uncached set_pmb_entry(unsigned long vpn,
	unsigned long ppn, unsigned long flags, int pos)
{
	jump_to_uncached();
	__set_pmb_entry(vpn, ppn, flags, pos);
	back_to_cached();
}

static __always_inline void __clear_pmb_entry(int pos)
{
#ifdef CONFIG_PMB_64M_TILES
	ctrl_outl(0, mk_pmb_addr(pos));
	ctrl_outl(PMB_POS2VIRT(pos), mk_pmb_addr(pos));
	ctrl_outl((CONFIG_PMB_64M_TILES_PHYS & ~((1 << PMB_FIXED_SHIFT)-1)) |
		  PMB_SZ_64M | PMB_WT | PMB_UB | PMB_V, mk_pmb_data(pos));
#else
	ctrl_outl(0, mk_pmb_addr(pos));
#endif
}

static void __uses_jump_to_uncached clear_pmb_entry(int pos)
{
	jump_to_uncached();
	__clear_pmb_entry(pos);
	back_to_cached();
}

static int pmb_alloc(int pos)
{
	if (likely(pos == PMB_NO_ENTRY))
		pos = find_first_zero_bit(&pmb_map, NR_PMB_ENTRIES);

repeat:
	if (unlikely(pos >= NR_PMB_ENTRIES))
		return PMB_NO_ENTRY;

	if (test_and_set_bit(pos, &pmb_map)) {
		pos = find_first_zero_bit(&pmb_map, NR_PMB_ENTRIES);
		goto repeat;
	}

	return pos;
}

static void pmb_free(int entry)
{
	clear_bit(entry, &pmb_map);
}

static struct pmb_mapping* pmb_mapping_alloc(void)
{
	struct pmb_mapping *mapping;

	if (pmb_mappings_free == NULL)
		return NULL;

	mapping = pmb_mappings_free;
	pmb_mappings_free = mapping->next;

	memset(mapping, 0, sizeof(*mapping));

	return mapping;
}

static void pmb_mapping_free(struct pmb_mapping* mapping)
{
	mapping->next = pmb_mappings_free;
	pmb_mappings_free = mapping;
}

static __always_inline void __pmb_mapping_set(struct pmb_mapping *mapping)
{
	struct pmb_entry *entry = mapping->entries;

	do {
		__set_pmb_entry(entry->vpn, entry->ppn,
			      entry->flags | mapping->flags, entry->pos);
		entry = entry->next;
	} while (entry);
}

static void pmb_mapping_set(struct pmb_mapping *mapping)
{
	struct pmb_entry *entry = mapping->entries;

	do {
		set_pmb_entry(entry->vpn, entry->ppn,
			      entry->flags | mapping->flags, entry->pos);
		entry = entry->next;
	} while (entry);
}

static void pmb_mapping_clear_and_free(struct pmb_mapping *mapping)
{
	struct pmb_entry *entry = mapping->entries;

	do {
		clear_pmb_entry(entry->pos);
		pmb_free(entry->pos);
		entry = entry->next;
	} while (entry);
}

#ifdef CONFIG_PMB_64M_TILES

static struct {
	unsigned long size;
	int flag;
} pmb_sizes[] = {
	{ .size = 1 << PMB_FIXED_SHIFT, .flag = PMB_SZ_64M,  },
};

/*
 * Different algorithm when we tile the entire P1/P2 region with
 * 64M PMB entries. This means the PMB entry is tied to the virtual
 * address it covers, so we only need to search for the virtual
 * address which accomodates the mapping we're interested in.
 */
static struct pmb_mapping* pmb_calc(unsigned long phys, unsigned long size,
				    unsigned long req_virt, int *req_pos,
				    unsigned long pmb_flags)
{
	struct pmb_mapping *new_mapping;
	struct pmb_mapping **prev_ptr;
	unsigned long prev_end, next_start;
	struct pmb_mapping *next_mapping;
	unsigned long new_start, new_end;
	const unsigned long pmb_size = pmb_sizes[0].size;
	struct pmb_entry *entry;
	struct pmb_entry **prev_entry_ptr;

	if (size == 0)
		return NULL;

	new_mapping = pmb_mapping_alloc();
	if (!new_mapping)
		return NULL;

	DPRINTK("request: phys %08lx, size %08lx\n", phys, size);

	prev_end = P1SEG;
	next_mapping = pmb_mappings;
	prev_ptr = &pmb_mappings;
	for (;;) {
		if (next_mapping == NULL)
			next_start = P3SEG;
		else
			next_start = next_mapping->virt;

		DPRINTK("checking space between %08lx and %08lx\n",
			prev_end, next_start);

		if (req_virt) {
			if ((req_virt < prev_end) || (req_virt > next_start))
				goto next;
			new_start = req_virt;
		} else {
			new_start = prev_end + (phys & (pmb_size-1));
		}

		new_end = new_start + size;

		if (new_end <= next_start)
			break;

next:
		if (next_mapping == NULL) {
			DPRINTK("failed, give up\n");
			return NULL;
		}

		prev_ptr = &next_mapping->next;
		prev_end = next_mapping->virt + next_mapping->size;
		next_mapping = next_mapping->next;
	}

	DPRINTK("found space at %08lx to %08lx\n", new_start, new_end);

	BUG_ON(req_pos && (*req_pos != PMB_VIRT2POS(new_start)));

	phys &= ~(pmb_size - 1);
	new_start &= ~(pmb_size - 1);

	new_mapping->phys = phys;
	new_mapping->virt = new_start;
	new_mapping->size = 0;
	new_mapping->flags = pmb_flags;
	new_mapping->entries = NULL;
	new_mapping->usage = 1;
	new_mapping->next = *prev_ptr;
	*prev_ptr = new_mapping;

	prev_entry_ptr = &new_mapping->entries;
	while (new_start < new_end) {
		int pos = PMB_VIRT2POS(new_start);

		pos = pmb_alloc(pos);
		BUG_ON(pos == PMB_NO_ENTRY);
		DPRINTK("using PMB entry %d\n", pos);

		entry = &pmbe[pos];
		entry->vpn = new_start;
		entry->ppn = phys;
		entry->flags = pmb_sizes[0].flag;
		entry->next = NULL;
		entry->size = pmb_size;
		*prev_entry_ptr = entry;
		prev_entry_ptr = &entry->next;

		new_start += pmb_size;
		phys += pmb_size;
		new_mapping->size += pmb_size;
	}

	return new_mapping;
}

#else

static struct {
	unsigned long size;
	int flag;
} pmb_sizes[] = {
	{ .size = 0x01000000, .flag = PMB_SZ_16M,  },
	{ .size = 0x04000000, .flag = PMB_SZ_64M,  },
	{ .size = 0x08000000, .flag = PMB_SZ_128M, },
	{ .size	= 0x20000000, .flag = PMB_SZ_512M, },
};

static struct pmb_mapping* pmb_calc(unsigned long phys, unsigned long size,
				    unsigned long req_virt, int *req_pos,
				    unsigned long pmb_flags)
{
	unsigned long orig_phys = phys;
	unsigned long orig_size = size;
	int max_i = ARRAY_SIZE(pmb_sizes)-1;
	struct pmb_mapping *new_mapping;
	unsigned long alignment;
	unsigned long virt_offset;
	struct pmb_entry **prev_entry_ptr;
	unsigned long prev_end, next_start;
	struct pmb_mapping *next_mapping;
	struct pmb_mapping **prev_ptr;
	struct pmb_entry *entry;
	unsigned long start;

	if (size == 0)
		return NULL;

	new_mapping = pmb_mapping_alloc();
	if (!new_mapping)
		return NULL;

	DPRINTK("request: phys %08lx, size %08lx\n", phys, size);

	/*
	 * First work out the PMB entries to tile the physical region.
	 *
	 * Fill in new_mapping and its list of entries, all fields
	 * except those related to virtual addresses.
	 *
	 * alignment is the maximum alignment of all of the entries which
	 * make up the mapping.
	 * virt_offset will be non-zero in case some of the entries leading
	 * upto those which force the maximal alignment are smaller than
	 * those largest ones, and in this case virt_offset must be added
	 * to the eventual virtual address (which is aligned to alignment),
	 * to get the virtual address of the first entry.
	 */
 retry:
	phys = orig_phys;
	size = orig_size;
	alignment = 0;
	virt_offset = 0;
	prev_entry_ptr = &new_mapping->entries;
	new_mapping->size = 0;
	while (size > 0) {
		unsigned long best_size;	/* bytes of size covered by tile */
		int best_i;
		unsigned long entry_phys;
		unsigned long entry_size;	/* total size of tile */
		int i;

		entry = *prev_entry_ptr;
		if (entry == NULL) {
			int pos;

			pos = pmb_alloc(req_pos ? *req_pos++ : PMB_NO_ENTRY);
			if (pos == PMB_NO_ENTRY)
				goto failed_give_up;
			entry = &pmbe[pos];
			entry->next = NULL;
			*prev_entry_ptr = entry;
		}
		prev_entry_ptr = &entry->next;

		/*
		 * Calculate the 'best' PMB entry size. This is the
		 * one which covers the largest amount of the physical
		 * address range we are trying to map, but if
		 * increasing the size wouldn't increase the amount we
		 * would be able to map, don't bother. Similarly, if
		 * increasing the size would result in a mapping where
		 * half or more of the coverage is wasted, don't bother.
		 */
		best_size = best_i = 0;
		for (i = 0; i <= max_i; i++) {
			unsigned long pmb_size = pmb_sizes[i].size;
			unsigned long tmp_start, tmp_end, tmp_size;
			tmp_start = phys & ~(pmb_size-1);
			tmp_end = tmp_start + pmb_size;
			tmp_size = min(phys+size, tmp_end)-max(phys, tmp_start);
			if (tmp_size <= best_size)
				continue;

			if (best_size) {
				unsigned long wasted_size;
				wasted_size = pmb_size - tmp_size;
				if (wasted_size >= (pmb_size / 2))
					continue;
			}

			best_i = i;
			best_size = tmp_size;
		}

		BUG_ON(best_size == 0);

		entry_size = pmb_sizes[best_i].size;
		entry_phys = phys & ~(entry_size-1);
		DPRINTK("using PMB %d: phys %08lx, size %08lx\n",
			entry->pos, entry_phys, entry_size);

		entry->ppn   = entry_phys;
		entry->size  = entry_size;
		entry->flags = pmb_sizes[best_i].flag;

		if (pmb_sizes[best_i].size > alignment) {
			alignment = entry_size;
			if (new_mapping->size)
				virt_offset = alignment - new_mapping->size;
		}

		new_mapping->size += entry_size;
		size -= best_size;
		phys += best_size;
	}

	new_mapping->phys = new_mapping->entries->ppn;

	DPRINTK("mapping: phys %08lx, size %08lx\n", new_mapping->phys, new_mapping->size);
	DPRINTK("virtual alignment %08lx, offset %08lx\n", alignment, virt_offset);

	/* Each iteration should use at least as many entries previous ones */
	BUG_ON(entry->next);

	/* Do we have a conflict with the requested maping? */
	BUG_ON(req_virt && ((req_virt & (alignment-1)) != virt_offset));

	/* Next try and find a virtual address to map this */
	prev_end = P1SEG;
	next_mapping = pmb_mappings;
	prev_ptr = &pmb_mappings;
	do {
		if (next_mapping == NULL)
			next_start = P3SEG;
		else
			next_start = next_mapping->virt;

		if (req_virt)
			start = req_virt;
		else
			start = ALIGN(prev_end, alignment) + virt_offset;

		DPRINTK("checking for virt %08lx between %08lx and %08lx\n",
			start, prev_end, next_start);

		if ((start >= prev_end) &&
		    (start + new_mapping->size <= next_start))
			break;

		if (next_mapping == NULL)
			goto failed;

		prev_ptr = &next_mapping->next;
		prev_end = next_mapping->virt + next_mapping->size;
		next_mapping = next_mapping->next;
	} while (1);

	DPRINTK("success, using %08lx\n", start);
	new_mapping->virt = start;
	new_mapping->flags = pmb_flags;
	new_mapping->usage = 1;
	new_mapping->next = *prev_ptr;
	*prev_ptr = new_mapping;

	/* Finally fill in the vpn's */
	for (entry = new_mapping->entries; entry; entry=entry->next) {
		entry->vpn = start;
		start += entry->size;
	}

	return new_mapping;

failed:
	if (--max_i >= 0) {
		DPRINTK("failed, try again with max_i %d\n", max_i);
		goto retry;
	}

failed_give_up:
	DPRINTK("failed, give up\n");
	for (entry = new_mapping->entries; entry; entry = entry->next)
		pmb_free(entry->pos);
	pmb_mapping_free(new_mapping);
	return NULL;
}
#endif

long pmb_remap(unsigned long phys,
	       unsigned long size, unsigned long flags)
{
	struct pmb_mapping *mapping;
	int pmb_flags;
	unsigned long offset;

	/* Convert typical pgprot value to the PMB equivalent */
	if (flags & _PAGE_CACHABLE) {
		if (flags & _PAGE_WT)
			pmb_flags = PMB_WT;
		else
			pmb_flags = PMB_C;
	} else
		pmb_flags = PMB_WT | PMB_UB;

	DPRINTK("phys: %08lx, size %08lx, flags %08lx->%08x\n",
		phys, size, flags, pmb_flags);

	write_lock(&pmb_lock);

	for (mapping = pmb_mappings; mapping; mapping=mapping->next) {
		DPRINTK("check against phys %08lx size %08lx flags %08lx\n",
			mapping->phys, mapping->size, mapping->flags);
		if ((phys >= mapping->phys) &&
		    (phys+size <= mapping->phys+mapping->size) &&
		    (pmb_flags == mapping->flags))
			break;
	}

	if (mapping) {
		/* If we hit an existing mapping, use it */
		mapping->usage++;
		DPRINTK("found, usage now %d\n", mapping->usage);
	} else if (size < MIN_PMB_MAPPING_SIZE) {
		/* We spit upon small mappings */
		write_unlock(&pmb_lock);
		return 0;
	} else {
		mapping = pmb_calc(phys, size, 0, NULL, pmb_flags);
		if (!mapping) {
			write_unlock(&pmb_lock);
			return 0;
		}
		pmb_mapping_set(mapping);
	}

	write_unlock(&pmb_lock);

	offset = phys - mapping->phys;
	return mapping->virt + offset;
}

static struct pmb_mapping *pmb_mapping_find(unsigned long addr,
					    struct pmb_mapping ***prev)
{
	struct pmb_mapping *mapping;
	struct pmb_mapping **prev_mapping = &pmb_mappings;

	for (mapping = pmb_mappings; mapping; mapping=mapping->next) {
		if ((addr >= mapping->virt) &&
		    (addr < mapping->virt + mapping->size))
			break;
		prev_mapping = &mapping->next;
	}

	if (prev != NULL)
		*prev = prev_mapping;

	return mapping;
}

int pmb_unmap(unsigned long addr)
{
	struct pmb_mapping *mapping;
	struct pmb_mapping **prev_mapping;

	write_lock(&pmb_lock);

	mapping = pmb_mapping_find(addr, &prev_mapping);

	if (unlikely(!mapping)) {
		write_unlock(&pmb_lock);
		return 0;
	}

	DPRINTK("mapping: phys %08lx, size %08lx, count %d\n",
		mapping->phys, mapping->size, mapping->usage);

	if (--mapping->usage == 0) {
		pmb_mapping_clear_and_free(mapping);
		*prev_mapping = mapping->next;
		pmb_mapping_free(mapping);
	}

	write_unlock(&pmb_lock);

	return 1;
}

static void noinline __uses_jump_to_uncached
apply_boot_mappings(struct pmb_mapping *uc_mapping, struct pmb_mapping *ram_mapping)
{
	register int i __asm__("r1");
	register unsigned long c2uc __asm__("r2");
	register struct pmb_entry *entry __asm__("r3");
	register unsigned long flags __asm__("r4");

	/* We can execute this directly, as the current PMB is uncached */
	__pmb_mapping_set(uc_mapping);

	cached_to_uncached = uc_mapping->virt -
		(((unsigned long)&__uncached_start) & ~(uc_mapping->entries->size-1));

	jump_to_uncached();

	/*
	 * We have to be cautious here, as we will temporarily lose access to
	 * the PMB entry which is mapping main RAM, and so loose access to
	 * data. So make sure all data is going to be in registers or the
	 * uncached region.
	 */

	c2uc = cached_to_uncached;
	entry = ram_mapping->entries;
	flags = ram_mapping->flags;

	for (i=0; i<NR_PMB_ENTRIES-1; i++)
		__clear_pmb_entry(i);

	do {
		entry = (struct pmb_entry*)(((unsigned long)entry) + c2uc);
		__set_pmb_entry(entry->vpn, entry->ppn,
				entry->flags | flags, entry->pos);
		entry = entry->next;
	} while (entry);

	/* Flush out the TLB */
	i =  ctrl_inl(MMUCR);
	i |= MMUCR_TI;
	ctrl_outl(i, MMUCR);

	back_to_cached();
}

struct pmb_mapping *uc_mapping, *ram_mapping
	__attribute__ ((__section__ (".uncached.data")));

void __init pmb_init(void)
{
	int i;
	int entry;

	/* Create the free list of mappings */
	pmb_mappings_free = &pmbm[0];
	for (i=0; i<NR_PMB_ENTRIES-1; i++)
		pmbm[i].next = &pmbm[i+1];
	pmbm[NR_PMB_ENTRIES-1].next = NULL;

	/* Initialise the PMB entrie's pos */
	for (i=0; i<NR_PMB_ENTRIES; i++)
		pmbe[i].pos = i;

	/* Create the initial mappings */
	entry = NR_PMB_ENTRIES-1;
	uc_mapping = pmb_calc(__pa(&__uncached_start), &__uncached_end - &__uncached_start,
		 P3SEG-pmb_sizes[0].size, &entry, PMB_WT | PMB_UB);
	ram_mapping = pmb_calc(__MEMORY_START, __MEMORY_SIZE, P1SEG, 0, PMB_C);
	apply_boot_mappings(uc_mapping, ram_mapping);
}

int pmb_virt_to_phys(void *addr, unsigned long *phys, unsigned long *flags)
{
	struct pmb_mapping *mapping;
	unsigned long vaddr = (unsigned long __force)addr;

	read_lock(&pmb_lock);

	mapping = pmb_mapping_find(vaddr, NULL);
	if (!mapping) {
		read_unlock(&pmb_lock);
		return EFAULT;
	}

	if (phys)
		*phys = mapping->phys + (vaddr - mapping->virt);
	if (flags)
		*flags = mapping->flags;

	read_unlock(&pmb_lock);

	return 0;
}
EXPORT_SYMBOL(pmb_virt_to_phys);

bool __in_29bit_mode(void)
{
#ifdef CONFIG_CPU_SUBTYPE_STX7100
	/* ST40-200 used a different mechanism to control SE mode */
	return (__raw_readl(MMUCR) & MMUCR_SE) == 0;
#else
	return (__raw_readl(PMB_PASCR) & PASCR_SE) == 0;
#endif
}

static int pmb_seq_show(struct seq_file *file, void *iter)
{
	int i;

	seq_printf(file, "V: Valid, C: Cacheable, WT: Write-Through\n"
			 "CB: Copy-Back, B: Buffered, UB: Unbuffered\n");
	seq_printf(file, "ety   vpn  ppn  size   flags\n");

	for (i = 0; i < NR_PMB_ENTRIES; i++) {
		unsigned long addr, data;
		unsigned int size;
		char *sz_str = NULL;

		addr = ctrl_inl(mk_pmb_addr(i));
		data = ctrl_inl(mk_pmb_data(i));

		size = data & PMB_SZ_MASK;
		sz_str = (size == PMB_SZ_16M)  ? " 16MB":
			 (size == PMB_SZ_64M)  ? " 64MB":
			 (size == PMB_SZ_128M) ? "128MB":
					         "512MB";

		/* 02: V 0x88 0x08 128MB C CB  B */
		seq_printf(file, "%02d: %c 0x%02lx 0x%02lx %s %c %s %s\n",
			   i, ((addr & PMB_V) && (data & PMB_V)) ? 'V' : ' ',
			   (addr >> 24) & 0xff, (data >> 24) & 0xff,
			   sz_str, (data & PMB_C) ? 'C' : ' ',
			   (data & PMB_WT) ? "WT" : "CB",
			   (data & PMB_UB) ? "UB" : " B");
	}

	return 0;
}

static int pmb_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmb_seq_show, NULL);
}

static const struct file_operations pmb_debugfs_fops = {
	.owner		= THIS_MODULE,
	.open		= pmb_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init pmb_debugfs_init(void)
{
	struct dentry *dentry;

	dentry = debugfs_create_file("pmb", S_IFREG | S_IRUGO,
				     sh_debugfs_root, NULL, &pmb_debugfs_fops);
	if (!dentry)
		return -ENOMEM;
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	return 0;
}
subsys_initcall(pmb_debugfs_init);

#ifdef CONFIG_PM
static __uses_jump_to_uncached
int pmb_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	static pm_message_t prev_state;
	int idx;
	switch (state.event) {
	case PM_EVENT_ON:
		/* Resumeing from hibernation */
		if (prev_state.event == PM_EVENT_FREEZE) {
			for (idx = 1; idx < NR_PMB_ENTRIES; ++idx)
				if (pmbm[idx].usage && pmbm[idx].virt != 0xbf)
					pmb_mapping_set(&pmbm[idx]);
			flush_cache_all();
		}
	  break;
	case PM_EVENT_SUSPEND:
	  break;
	case PM_EVENT_FREEZE:
	  break;
	}
	prev_state = state;
	return 0;
}

static int pmb_sysdev_resume(struct sys_device *dev)
{
	return pmb_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_driver pmb_sysdev_driver = {
	.suspend = pmb_sysdev_suspend,
	.resume = pmb_sysdev_resume,
};

static int __init pmb_sysdev_init(void)
{
	return sysdev_driver_register(&cpu_sysdev_class, &pmb_sysdev_driver);
}

subsys_initcall(pmb_sysdev_init);

#ifdef CONFIG_HIBERNATION_ON_MEMORY

void __uses_jump_to_uncached stm_hom_pmb_init(void)
{
	apply_boot_mappings(uc_mapping, ram_mapping);

	/* Now I can call the pmb_sysdev_resume */
	pmb_sysdev_suspend(NULL, PMSG_ON);
}
#endif
#endif
