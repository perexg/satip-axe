/*
 * arch/sh/mm/ioremap.c
 *
 * Re-map IO memory to kernel address space so that we can access it.
 * This is needed for high PCI addresses that aren't mapped in the
 * 640k-1MB IO memory area on PC's
 *
 * (C) Copyright 1995 1996 Linus Torvalds
 * (C) Copyright 2005, 2006 Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 */
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/addrspace.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/mmu.h>

/*
 * Remap an arbitrary physical address space into the kernel virtual
 * address space.
 *
 * NOTE! We need to allow non-page-aligned mappings too: we will obviously
 * have to convert them into an offset in a page-aligned mapping, but the
 * caller shouldn't need to know that small detail.
 */
static void __iomem *__ioremap_prot(unsigned long phys_addr, unsigned long size,
				    pgprot_t pgprot)
{
	struct vm_struct * area;
	unsigned long offset, last_addr, addr;
	int simple = (pgprot_val(pgprot) == pgprot_val(PAGE_KERNEL)) ||
		(pgprot_val(pgprot) == pgprot_val(PAGE_KERNEL_NOCACHE));
	int cached = pgprot_val(pgprot) & _PAGE_CACHABLE;

	/* Don't allow wraparound or zero size */
	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

	/*
	 * If we're in the fixed PCI memory range, mapping through page
	 * tables is not only pointless, but also fundamentally broken.
	 * Just return the physical address instead.
	 *
	 * For boards that map a small PCI memory aperture somewhere in
	 * P1/P2 space, ioremap() will already do the right thing,
	 * and we'll never get this far.
	 */
	if (is_pci_memory_fixed_range(phys_addr, size))
		return (void __iomem *)phys_addr;

	/*
	 * Don't allow anybody to remap normal RAM that we're using..
	 */
	if ((phys_addr >= __pa(memory_start)) && (last_addr < __pa(memory_end))) {
		char *t_addr, *t_end;
		struct page *page;

		t_addr = __va(phys_addr);
		t_end = t_addr + (size - 1);

		for(page = virt_to_page(t_addr); page <= virt_to_page(t_end); page++)
			if(!PageReserved(page))
				return NULL;
	}

	/* P4 uncached addresses are permanently mapped */
	if ((PXSEG(phys_addr) == P4SEG) && simple && !cached)
		return (void __iomem *)phys_addr;

	/*
	 * Mappings have to be page-aligned
	 */
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr+1) - phys_addr;

#ifdef CONFIG_PMB
	addr = pmb_remap(phys_addr, size, cached ? _PAGE_CACHABLE : 0);
	if (addr)
		return (void __iomem *)(offset + (char *)addr);
#endif

	area = get_vm_area(size, VM_IOREMAP);
	if (!area)
		return NULL;
	area->phys_addr = phys_addr;
	addr = (unsigned long)area->addr;

	if (ioremap_page_range(addr, addr + size, phys_addr, pgprot)) {
		vunmap((void *)addr);
		return NULL;
	}

	return (void __iomem *)(offset + (char *)addr);
}

void __iomem *__ioremap(unsigned long phys_addr, unsigned long size,
			unsigned long flags)
{
	pgprot_t pgprot;

	if (unlikely(flags & _PAGE_CACHABLE))
		pgprot = PAGE_KERNEL;
	else
		pgprot = PAGE_KERNEL_NOCACHE;

	return __ioremap_prot(phys_addr, size, pgprot);
}
EXPORT_SYMBOL(__ioremap);

void __iounmap(void __iomem *addr)
{
	unsigned long vaddr = (unsigned long __force)addr;
	unsigned long seg = PXSEG(vaddr);
	struct vm_struct *p;

	if (seg == P4SEG || is_pci_memory_fixed_range(vaddr, 0))
		return;

#ifdef CONFIG_29BIT
	if (seg < P3SEG)
		return;
#endif

#ifdef CONFIG_PMB
	if (pmb_unmap(vaddr))
		return;
#endif

	p = remove_vm_area((void *)(vaddr & PAGE_MASK));
	if (!p) {
		printk(KERN_ERR "%s: bad address %p\n", __func__, addr);
		return;
	}

	kfree(p);
}
EXPORT_SYMBOL(__iounmap);
