/*
 * arch/sh/mm/consistent.c
 *
 * Copyright (C) 2004 - 2007  Paul Mundt
 *
 * Declared coherent memory functions based on arch/x86/kernel/pci-dma_32.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-debug.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/l2_cacheflush.h>
#include <asm/addrspace.h>

#ifdef CONFIG_PMB

/*
 * This is yet another copy of the ARM (and powerpc) VM region allocation
 * code (which is Copyright (C) 2000-2004 Russell King).
 *
 * We have to do this (rather than use get_vm_area()) because
 * dma_alloc_coherent() can be (and is) called from interrupt level.
 */

static DEFINE_SPINLOCK(consistent_lock);

/*
 * VM region handling support.
 *
 * This should become something generic, handling VM region allocations for
 * vmalloc and similar (ioremap, module space, etc).
 *
 * I envisage vmalloc()'s supporting vm_struct becoming:
 *
 *  struct vm_struct {
 *    struct vm_region	region;
 *    unsigned long	flags;
 *    struct page	**pages;
 *    unsigned int	nr_pages;
 *    unsigned long	phys_addr;
 *  };
 *
 * get_vm_area() would then call vm_region_alloc with an appropriate
 * struct vm_region head (eg):
 *
 *  struct vm_region vmalloc_head = {
 *	.vm_list	= LIST_HEAD_INIT(vmalloc_head.vm_list),
 *	.vm_start	= VMALLOC_START,
 *	.vm_end		= VMALLOC_END,
 *  };
 *
 * However, vmalloc_head.vm_start is variable (typically, it is dependent on
 * the amount of RAM found at boot time.)  I would imagine that get_vm_area()
 * would have to initialise this each time prior to calling vm_region_alloc().
 */
struct sh_vm_region {
	struct list_head	vm_list;
	unsigned long		vm_start;
	unsigned long		vm_end;
	struct page		*vm_pages;
};

static struct sh_vm_region consistent_head = {
	.vm_list	= LIST_HEAD_INIT(consistent_head.vm_list),
	.vm_start	= CONSISTENT_BASE,
	.vm_end		= CONSISTENT_END,
};

static struct sh_vm_region *
sh_vm_region_alloc(struct sh_vm_region *head, size_t size, gfp_t gfp)
{
	unsigned long addr = head->vm_start, end = head->vm_end - size;
	unsigned long flags;
	struct sh_vm_region *c, *new;

	new = kmalloc(sizeof(struct sh_vm_region), gfp);
	if (!new)
		goto out;

	spin_lock_irqsave(&consistent_lock, flags);

	list_for_each_entry(c, &head->vm_list, vm_list) {
		if ((addr + size) < addr)
			goto nospc;
		if ((addr + size) <= c->vm_start)
			goto found;
		addr = c->vm_end;
		if (addr > end)
			goto nospc;
	}

found:
	/*
	 * Insert this entry _before_ the one we found.
	 */
	list_add_tail(&new->vm_list, &c->vm_list);
	new->vm_start = addr;
	new->vm_end = addr + size;

	spin_unlock_irqrestore(&consistent_lock, flags);
	return new;

nospc:
	spin_unlock_irqrestore(&consistent_lock, flags);
	kfree(new);
out:
	return NULL;
}

static struct sh_vm_region *sh_vm_region_find(struct sh_vm_region *head,
					 unsigned long addr)
{
	struct sh_vm_region *c;

	list_for_each_entry(c, &head->vm_list, vm_list) {
		if (c->vm_start == addr)
			goto out;
	}
	c = NULL;
out:
	return c;
}

static void *__consistent_map(struct page *page, size_t size, gfp_t gfp)
{
	struct sh_vm_region *c;
	unsigned long vaddr;
	unsigned long paddr;

	c = sh_vm_region_alloc(&consistent_head, size,
			    gfp & ~(__GFP_DMA | __GFP_HIGHMEM));
	if (!c)
		return NULL;

	vaddr = c->vm_start;
	paddr = page_to_phys(page);
	if (ioremap_page_range(vaddr, vaddr+size, paddr, PAGE_KERNEL_NOCACHE)) {
		list_del(&c->vm_list);
		return NULL;
	}

	c->vm_pages = page;

	return (void *)vaddr;
}

static struct page *__consistent_unmap(void *vaddr, size_t size)
{
	unsigned long flags;
	struct sh_vm_region *c;
	struct page *page;

	spin_lock_irqsave(&consistent_lock, flags);
	c = sh_vm_region_find(&consistent_head, (unsigned long)vaddr);
	spin_unlock_irqrestore(&consistent_lock, flags);
	if (!c)
		goto no_area;

	if ((c->vm_end - c->vm_start) != size) {
		printk(KERN_ERR "%s: freeing wrong coherent size (%ld != %d)\n",
		       __func__, c->vm_end - c->vm_start, size);
		dump_stack();
		size = c->vm_end - c->vm_start;
	}

	page = c->vm_pages;

	unmap_kernel_range(c->vm_start, size);

	spin_lock_irqsave(&consistent_lock, flags);
	list_del(&c->vm_list);
	spin_unlock_irqrestore(&consistent_lock, flags);

	kfree(c);

	return page;

no_area:
	printk(KERN_ERR "%s: trying to free invalid coherent area: %p\n",
	       __func__, vaddr);
	dump_stack();

	return NULL;
}

#else

static void *__consistent_map(struct page *page, size_t size, gfp_t gfp)
{
	return P2SEGADDR(page_address(page));
}

static struct page *__consistent_unmap(void *vaddr, size_t size)
{
	unsigned long addr;

	addr = P1SEGADDR((unsigned long)vaddr);
	BUG_ON(!virt_addr_valid(addr));
	return virt_to_page(addr);
}

#endif

#define PREALLOC_DMA_DEBUG_ENTRIES	4096

static int __init dma_init(void)
{
	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	return 0;
}
fs_initcall(dma_init);

void *dma_alloc_coherent(struct device *dev, size_t size,
			   dma_addr_t *dma_handle, gfp_t gfp)
{
	void *ret;
	int order;
	struct page *page;
	unsigned long phys_addr;
	void* kernel_addr;
	size_t orig_size;

	if (dma_alloc_from_coherent(dev, size, dma_handle, &ret))
		return ret;

	/* ignore region specifiers */
	gfp &= ~(__GFP_DMA | __GFP_HIGHMEM);

	orig_size = size;
	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = alloc_pages(gfp, order);
	if (!page)
		return NULL;

	kernel_addr = page_address(page);
	phys_addr = virt_to_phys(kernel_addr);
	ret = __consistent_map(page, size, gfp);
	if (!ret) {
		__free_pages(page, order);
		return NULL;
	}

	memset(kernel_addr, 0, orig_size);
	/*
	 * Pages from the page allocator may have data present in
	 * cache. So flush the cache before using uncached memory.
	 */
	dma_cache_sync(dev, kernel_addr, orig_size, DMA_BIDIRECTIONAL);

	/*
	 * Free the otherwise unused pages, unless got compound page
	 */
	if (!PageCompound(page)) {
		struct page *end = page + (1 << order);

		split_page(page, order);

		for (page += size >> PAGE_SHIFT; page < end; page++)
			__free_page(page);
	}

	*dma_handle = phys_addr;

	debug_dma_alloc_coherent(dev, orig_size, *dma_handle, ret);

	return ret;
}
EXPORT_SYMBOL(dma_alloc_coherent);

void dma_free_coherent(struct device *dev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	int order = get_order(size);
	struct page *page;

	if (dma_release_from_coherent(dev, order, vaddr))
		return;

	debug_dma_free_coherent(dev, size, vaddr, dma_handle);

	size = PAGE_ALIGN(size);
	page = __consistent_unmap(vaddr, size);
	if (page) {
		if (PageCompound(page)) {
			__free_pages(page, get_order(size));
		} else {
			int i;

			for (i = 0; i < (size >> PAGE_SHIFT); i++)
				__free_page(page + i);
		}
	}
}
EXPORT_SYMBOL(dma_free_coherent);

void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
		    enum dma_data_direction direction)
{
	switch (direction) {
	case DMA_FROM_DEVICE:		/* invalidate only */
		__flush_invalidate_region(vaddr, size);
		__l2_flush_invalidate_region(vaddr, size);
		break;
	case DMA_TO_DEVICE:		/* writeback only */
		__flush_wback_region(vaddr, size);
		__l2_flush_wback_region(vaddr, size);
		break;
	case DMA_BIDIRECTIONAL:		/* writeback and invalidate */
		__flush_purge_region(vaddr, size);
		__l2_flush_purge_region(vaddr, size);
		break;
	default:
		BUG();
	}
}
EXPORT_SYMBOL(dma_cache_sync);

static int __init memchunk_setup(char *str)
{
	return 1; /* accept anything that begins with "memchunk." */
}
__setup("memchunk.", memchunk_setup);

static void __init memchunk_cmdline_override(char *name, unsigned long *sizep)
{
	char *p = boot_command_line;
	int k = strlen(name);

	while ((p = strstr(p, "memchunk."))) {
		p += 9; /* strlen("memchunk.") */
		if (!strncmp(name, p, k) && p[k] == '=') {
			p += k + 1;
			*sizep = memparse(p, NULL);
			pr_info("%s: forcing memory chunk size to 0x%08lx\n",
				name, *sizep);
			break;
		}
	}
}

int __init platform_resource_setup_memory(struct platform_device *pdev,
					  char *name, unsigned long memsize)
{
	struct resource *r;
	dma_addr_t dma_handle;
	void *buf;

	r = pdev->resource + pdev->num_resources - 1;
	if (r->flags) {
		pr_warning("%s: unable to find empty space for resource\n",
			name);
		return -EINVAL;
	}

	memchunk_cmdline_override(name, &memsize);
	if (!memsize)
		return 0;

	buf = dma_alloc_coherent(NULL, memsize, &dma_handle, GFP_KERNEL);
	if (!buf) {
		pr_warning("%s: unable to allocate memory\n", name);
		return -ENOMEM;
	}

	memset(buf, 0, memsize);

	r->flags = IORESOURCE_MEM;
	r->start = dma_handle;
	r->end = r->start + memsize - 1;
	r->name = name;
	return 0;
}
