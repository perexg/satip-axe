/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#include "stm_hom.h"
#include <linux/hom.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/preempt.h>

#include <asm/thread_info.h>
#include <asm/cacheflush.h>
#include <asm-generic/sections.h>
#include <asm-generic/bug.h>
#include <asm/fpu.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>

#undef  dbg_print

#ifdef CONFIG_HOM_DEBUG
#define dbg_print(fmt, args...)		pr_info("%s: " fmt, __func__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

#define SH_TMU_IOBASE		0xffd80000

#ifdef CONFIG_HOM_DEBUG
static int enable_hom_printk;
void hom_printk(char *buf, ...)
{
	static unsigned char *base_addr = (unsigned char *)empty_zero_page;
	int count, i;
	va_list args;

	if (!enable_hom_printk)
		return;

	va_start(args, buf);
	count = vsnprintf(base_addr, 4096, buf, args);
	va_end(args);

	for (i = 0; i < count; i += L1_CACHE_BYTES)
		asm volatile(
			"ocbp	@%0	\n"
			: : "r" (base_addr + i)
			: "memory");

	base_addr += count;
	if (base_addr > (empty_zero_page + 0x1000))
		base_addr = empty_zero_page;
}

/*
 * Nice function to debug the PMB setting
 */
static void stm_hom_check_pmb(void)
{
	int i;
	unsigned long pmb_addr = PMB_ADDR;
	unsigned long pmb_data = PMB_DATA;

	hom_printk("%s ", __func__);

	for (i = 0; i < PMB_ENTRY_MAX; ++i) {
		if ((readl(pmb_addr) & PMB_V) || (readl(pmb_data) & PMB_V)) {
			hom_printk("Valid entry in: %d - ", i);
			hom_printk("0x%x vs 0x%x ",
				readl(pmb_addr) >> 24,
				readl(pmb_data) >> 24);
			if (readl(pmb_data) & PMB_WT)
				hom_printk(" WT");
			else
				hom_printk(" CB");
			if (readl(pmb_data) & PMB_C)
				hom_printk(" Cached");
			if (readl(pmb_data) & PMB_UB)
				hom_printk(" UB ");
		}
		pmb_addr += (1 << PMB_E_SHIFT);
		pmb_data += (1 << PMB_E_SHIFT);
	}
	hom_printk("%s COMPLETE ", __func__);
}

#else
void hom_printk(char *buf, ...)
{
	return ;
}
#endif


static struct stm_mem_hibernation *platform_hom;
unsigned long stm_hom_saved_stack_value;
/*
 * stm_hom_boot_stack is a mini stack in uncached.data
 */
unsigned long stm_hom_boot_stack[THREAD_SIZE / 4]
		__attribute__ ((__section__ (".uncached.data")));

static long linux_marker[] = {	0x7a6f7266,	/* froz */
				0x6c5f6e65,	/* en_l */
				0x78756e69 };	/* inux */

int __weak stm_freeze_board(void *data)
{
	return 0;
}

int __weak stm_defrost_board(void *data)
{
	return 0;
}

/*
 * This function restarts the TMU1 in free running mode
 * This is required only for TMU1 because the TMU_0 is
 * managed via clock_event API while the TMU_1 has no initialization
 * via clock_source API therefore the initialization is done here.
 */
static void stm_hom_tmu1_restart(void)
{
	unsigned char tmp;
	iowrite32(0xffffffff, SH_TMU_IOBASE + 0x14); /* TMU1.TCOR */
	iowrite32(0xffffffff, SH_TMU_IOBASE + 0x18); /* TMU1.TCNT */
	tmp = ioread8(SH_TMU_IOBASE + 0x4);
	iowrite8(tmp | 0x2, SH_TMU_IOBASE + 0x4);
}

/*
 * Initialize the cache
 */
void __uses_jump_to_uncached stm_hom_cache_init(void)
{
	unsigned long ccr, flags;

	jump_to_uncached();
	ccr = ctrl_inl(CCR);
	/*
	 * Default CCR values .. enable the caches
	 * and invalidate them immediately..
	 */
	flags = CCR_CACHE_ENABLE | CCR_CACHE_INVALIDATE;

	if (ccr & CCR_CACHE_ENABLE) {
		unsigned long ways, waysize, addrstart;

		waysize = current_cpu_data.dcache.sets;
		waysize <<= current_cpu_data.dcache.entry_shift;

		ways = current_cpu_data.dcache.ways;

		addrstart = CACHE_OC_ADDRESS_ARRAY;

		do {
			unsigned long addr;

			for (addr = addrstart;
			     addr < addrstart + waysize;
			     addr += current_cpu_data.dcache.linesz)
				ctrl_outl(0, addr);

			addrstart += current_cpu_data.dcache.way_incr;
		} while (--ways);
	}

#if defined(CONFIG_CACHE_WRITETHROUGH)
	/* Write-through */
	flags |= CCR_CACHE_WT;
#elif defined(CONFIG_CACHE_WRITEBACK)
	/* Write-back */
	flags |= CCR_CACHE_CB;
#else
	/* Off */
	flags &= ~CCR_CACHE_ENABLE;
#endif

	ctrl_outl(flags, CCR);
	back_to_cached();
}

/*
 * This function restores the orginal pgd value
 * based on the current running process
 */
static void stm_hom_pgd_setup(void)
{
	struct thread_info *ctf = current_thread_info();
	struct task_struct *task = ctf->task;
	struct mm_struct *current_mm = task->active_mm;

	set_TTB(current_mm->pgd);
}

static int stm_hom_enter(void)
{
	unsigned long *_ztext = (unsigned long *)
		(CONFIG_HOM_TAG_VIRTUAL_ADDRESS);
	long flag;
	unsigned long lpj =
		(cpu_data[raw_smp_processor_id()].loops_per_jiffy * HZ) / 1000;

	/* clear the temporary stack */
	memset(stm_hom_boot_stack, 0, THREAD_SIZE);

	/*
	 * Write the Linux Frozen Marker in the Main Memory
	 *
	 * Due to the __flush_dcache_segment_Xway implementation
	 * the marker in the empty_zero_page has to be moved in
	 * the main memory with an ocbp code
	 */
	asm volatile(
		"mov.l	%1, @(0,%0)	\n"	/* marker_0 */
		"mov.l	%2, @(4,%0)	\n"	/* marker_1 */
		"mov.l	%3, @(8,%0)	\n"	/* marker_2 */
		"mov.l	%4, @(12,%0)	\n"	/* entry_point */

		"mov.l	%5, @(16,%0)	\n"	/* only for degub */
		"mov.l  r15, @(20,%0)   \n"	/* only for debug */

		"ocbp	@%0		\n"
		: :	"r" (_ztext),
			"r" (linux_marker[0]),
			"r" (linux_marker[1]),
			"r" (linux_marker[2]),
			"r" (stm_defrost_kernel),
			"r" (current_thread_info())
		:	"memory");

	local_irq_save(flag);

	mdelay(100);
	BUG_ON(in_irq());

	/*
	 * Flush __all__ the caches to avoid some pending write operation
	 * on the memory is still in D-cache...
	 */
	flush_cache_all();

	stm_hom_exec_table(platform_hom->tbl_addr,
			   platform_hom->tbl_size, lpj, 0);

	BUG_ON(in_irq());

	local_irq_restore(flag);

	/*
	 * remove the marker in memory also if the bootloader already did that
	 */
	asm volatile(
		"mov	#-2, r0		\n"
		"mov.l	r0, @(0,%0)	\n"
		"mov.l	r0, @(4,%0)	\n"
		"mov.l	r0, @(8,%0)	\n"
		"mov.l	%1,  @(24,%0)	\n"	/* only for debug */
		"mov.l	r15,  @(28,%0)	\n"	/* onlu for debug */
		"ocbp	@%0		\n"
		: :	"r" (_ztext),
			"r" (current_thread_info())
		:	"memory", "r0");

	/*
	 * Here an __early__ console initialization to avoid
	 * blocking printk.
	 * This is required if the kernel boots with 'no_console_suspend'
	 */
	if (platform_hom->early_console)
		platform_hom->early_console();

	/*
	 * Restart the TMU1 in free running mode
	 */
	stm_hom_tmu1_restart();

	flush_cache_all();

	local_flush_tlb_all();

	dec_preempt_count();	/*
				 * Not clear why...
				 * but it works
				 */

	stm_hom_pgd_setup();

	disable_hlt();

	init_fpu(current);

#ifdef CONFIG_HOM_DEBUG
	enable_hom_printk = 1;
	stm_hom_check_pmb();
#endif

	memset(empty_zero_page, 0, 0x1000); /* clear the empty_zero_page */
	return 0;
}

int stm_hom_register(struct stm_mem_hibernation *data)
{
	if (!data || platform_hom)
		return -EINVAL;

	platform_hom = data;

	platform_hom->ops.enter = stm_hom_enter;

	if (hom_set_ops(&platform_hom->ops)) {
		platform_hom = NULL;
		return -EINVAL;
	}
	pr_info("[STM]: [PM]: HoM support registered\n");
	return 0;
}
EXPORT_SYMBOL_GPL(stm_hom_register);
