/*
 * arch/sh/kernel/cpu/sh4/entry.S
 *
 *  Copyright (C) 1999, 2000, 2002  Niibe Yutaka
 *  Copyright (C) 2003 - 2006  Paul Mundt
 *  Copyright (c) 2008 STMicroelectronics R&D Ltd. <carl.shaw@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * entry.S contains the exception vector handling code
 * This includes:
 *     the TRAPA general exception handler used for system calls
 *     the interrupt exception entry point
 *     the memory fault handlers
 *
 * When the SH takes an exception, the hardware jumps to a hard offset from
 * the address stored in the VBR register.  VBR is set to be <vbr_base> in
 * this file.  Each offset then contains code to handle the specific
 * exception and do the context switch if necessary by pushing the current
 * register set to the appropriate stack.
 * The PR is then set to return to a function that restores the original
 * context, so that a simple "rts" can be used to return from the handler
 * code.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call.
 *
 * NOTE: This code uses a convention that instructions in the delay slot
 * of a transfer-control instruction are indented by an extra space, thus:
 *
 *    jmp	@k0	    ! control-transfer instruction
 *     ldc	k1, ssr     ! delay slot
 *
 * Stack layout in 'ret_from_syscall':
 * 	ptrace needs to have all regs on the stack.
 *	if the order here is changed, it needs to be
 *	updated in ptrace.c and ptrace.h
 *
 *	r0
 *      ...
 *	r15 = stack pointer
 *	spc
 *	pr
 *	ssr
 *	gbr
 *	mach
 *	macl
 *	syscall #
 *
 *  As this is critical low level code which is potentially called thousands
 *  of times each second, effort has been spent to optimise it.  It has been
 *  optimised for real-world situations where it is not likely to be all
 *  resident in instruction and data caches.
 */

#include <linux/sys.h>
#include <linux/errno.h>

/* Override the ENTRY macro - all global functions in this file */
/* will be aligned to an i-cache boundary for performance       */
#define ENTRY(name) \
  .globl name; \
  .balign 32; \
  name:

#include <linux/linkage.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>
#include <asm/unistd.h>
#include <cpu/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/page.h>

#define k0	r0
#define k1	r1
#define k2	r2
#define k3	r3
#define k4	r4

!
!  Macro definitions
!

! Macro for pushing user registers
! k0 contains the stack
! k1, k4 scratch
! k3 contains the return address
!
! 998f: must contain 0x80f0
! 999f: must contain 0xcfffffff
!
.macro PUSH_USER_REGS
	mov	#-1, k4
	mov.l	k4, @-r15	! set TRA (default: -1)
	!
	sts.l	macl, @-r15
	sts.l	mach, @-r15
	stc.l	gbr, @-r15
	stc.l	ssr, @-r15
	sts.l	pr, @-r15
	stc.l	spc, @-r15
	!
	lds	k3, pr		! Set the return address to pr
	!
	mov.l	k0, @-r15	! save original stack
	mov.l	r14, @-r15
	mov.l	r13, @-r15
	mov.l	r12, @-r15
	mov.l	r11, @-r15
	mov.l	r10, @-r15
	mov.l	r9, @-r15
	mov.l	r8, @-r15
	!
	mov.l	998f, k1	! (latency=2) k1 = 0x80f0
	stc	sr, r8		! Back to normal register bank, and
	or	k1, r8		! Block all interrupts
	mov.l	999f, k1	! (latency=2) k1 = 0xcfffffff
	and	k1, r8		! ...
	ldc	r8, sr		! ...changed here.
	!
	!
	mov.l	r7, @-r15
	mov.l	r6, @-r15
	mov.l	r5, @-r15
	mov.l	r4, @-r15
	mov.l	r3, @-r15
	mov.l	r2, @-r15
	mov.l	r1, @-r15
	mov.l	r0, @-r15
.endm

!
!  Macro for checking GUSA atomic rollback
!
!  k0, k1 used as scratch registers
!
.macro GUSA_CHECK
#ifdef CONFIG_GUSA
	! Check for roll back gRB (User and Kernel)
	mov	r15, k0
	shll	k0
	bf	1f
	shll	k0
	bf	1f
	stc	spc, k1
	stc	r0_bank, k0
	cmp/hs	k0, k1		! test k1 (saved PC) >= k0 (saved r0)
	bt/s	2f
	 stc	r1_bank, k1

	add	#-2, k0
	add	r15, k0
	ldc	k0, spc		! PC = saved r0 + r15 - 2
2:	mov	k1, r15		! SP = r1
1:
#endif
.endm

#if defined(CONFIG_KGDB)
NMI_VEC = 0x1c0			! Must catch early for debounce
#endif

/* Offsets to the stack */
OFF_R0  =  0		/* Return value. New ABI also arg4 */
OFF_R1  =  4     	/* New ABI: arg5 */
OFF_R2  =  8     	/* New ABI: arg6 */
OFF_R3  =  12     	/* New ABI: syscall_nr */
OFF_R4  =  16     	/* New ABI: arg0 */
OFF_R5  =  20     	/* New ABI: arg1 */
OFF_R6  =  24     	/* New ABI: arg2 */
OFF_R7  =  28     	/* New ABI: arg3 */
OFF_SP	=  (15*4)
OFF_PC  =  (16*4)
OFF_PR  =  (17*4)
OFF_SR	=  (16*4+8)
OFF_TRA	=  (16*4+6*4)

#define g_imask		r6	/* r6_bank1 */
#define k_g_imask	r6_bank	/* r6_bank1 */
#define current		r7	/* r7_bank1 */

#include <asm/entry-macros.S>

/*
 * Kernel mode register usage:
 *	k0	scratch
 *	k1	scratch
 *	k2	scratch (Exception code)
 *	k3	scratch (Return address)
 *	k4	scratch
 *	k5	reserved
 *	k6	Global Interrupt Mask (0--15 << 4)
 *	k7	CURRENT_THREAD_INFO (pointer to current thread info)
 */

!
!
! Exception Vector Base
!
!	Must be aligned to a page boundary.
!

	.balign 	4096,0,4096
ENTRY(vbr_base)
	! Reset vector - unused as hardware automatically jumps to reset address
	.long	0

	!
	! General exception vector
	!

	.balign 	256,0,256

general_exception:
	! Using k0, k1 for scratch registers (r0_bank1, r1_bank),

	GUSA_CHECK

	stc	ssr, k0		! Is it from kernel space?
	shll	k0		! Check MD bit (bit30) by shifting it into...
	shll	k0		!       ...the T bit
	bt/s	1f		! It's a kernel to kernel transition.
	 mov	r15, k0		! save original stack to k0
	/* User space to kernel */
	mov	#(THREAD_SIZE >> 10), k1
	shll8	k1		! k1 := THREAD_SIZE
	shll2	k1
	add	current, k1
	mov	k1, r15		! change to kernel stack
	!
1:
	!
	! Save the user registers on the stack.
	!

	! Get EXPEVT
	mov.l   1f, k2
	mov.l	@k2, k2

	mov.l	2f, k3		! ret_from_exception address
	PUSH_USER_REGS

	! Get exception code
	stc	r2_bank, r8
	shlr2	r8
	shlr	r8

#ifdef COUNT_EXCEPTIONS
	mov.l	3f, r9
	add	r8, r9
	mov.l	@r9, r10
	add	#1, r10
	mov.l	r10, @r9
#endif
	mov.l	4f, r9
	add	r8, r9
	mov.l	@r9, r9
	jmp	@r9
	 nop

	rts
	 nop

	.balign	32 /* d-cache align to only one line */
1:	.long	EXPEVT
2:	.long	ret_from_exception
#ifdef COUNT_EXCEPTIONS
3:	.long	exception_count_table
#endif
998: 	.long	0x000080f0
999: 	.long	0xcfffffff
4:	.long	exception_handling_table


/* gas doesn't flag impossible values for mov #immediate as an error */
#if (_PAGE_PRESENT >> 2) > 0x7f
#error cannot load PAGE_PRESENT as an immediate
#endif
#if _PAGE_DIRTY > 0x7f
#error cannot load PAGE_DIRTY as an immediate
#endif
#if (_PAGE_PRESENT << 2) != _PAGE_ACCESSED
#error cannot derive PAGE_ACCESSED from PAGE_PRESENT
#endif

#define ldmmupteh(r)	mov.l	8f, r

	.balign 	1024,0,1024
tlb_miss:
#ifdef COUNT_EXCEPTIONS
	! Increment the counts
	mov.l	9f, k1
	mov.l	@k1, k2
	add	#1, k2
	mov.l	k2, @k1
#endif

	! k0 scratch
	! k1 pgd and pte pointers
	! k2 faulting address
	! k3 pgd and pte index masks
	! k4 shift

	! Load up the pgd entry (k1)

	ldmmupteh(k0)		!  9 LS (latency=2)	MMU_PTEH

	mov.w	4f, k3		!  8 LS (latency=2)	(PTRS_PER_PGD-1) << 2
	mov	#-(PGDIR_SHIFT-2), k4	!  6 EX

	mov.l	@(MMU_TEA-MMU_PTEH,k0), k2	! 18 LS (latency=2)

	mov.l	@(MMU_TTB-MMU_PTEH,k0), k1	! 18 LS (latency=2)

	mov	k2, k0			!   5 MT (latency=0)
	shld	k4, k0			!  99 EX

	and	k3, k0			!  78 EX

	mov.l	@(k0, k1), k1		!  21 LS (latency=2)
	mov	#-(PAGE_SHIFT-2), k4	!   6 EX

	! Load up the pte entry (k2)

	mov	k2, k0			!   5 MT (latency=0)
	shld	k4, k0			!  99 EX

	tst	k1, k1			!  86 MT

	bt	20f			! 110 BR

	mov.w	3f, k3			!  8 LS (latency=2)
					!  (PTRS_PER_PTE-1) << 2
	and	k3, k0			!  78 EX
	mov.w	5f, k4			!   8 LS (latency=2)	_PAGE_PRESENT

	mov.l	@(k0, k1), k2		!  21 LS (latency=2)
	add	k0, k1			!  49 EX

#ifdef CONFIG_CPU_HAS_PTEA
	! Test the entry for present and _PAGE_ACCESSED

	mov	#-28, k3		!   6 EX
	mov	k2, k0			!   5 MT (latency=0)

	tst	k4, k2			!  68 MT
	shld	k3, k0			!  99 EX

	bt	20f			! 110 BR

	! Set PTEA register
	! MMU_PTEA = ((pteval >> 28) & 0xe) | (pteval & 0x1)
	!
	! k0=pte>>28, k1=pte*, k2=pte, k3=<unused>, k4=_PAGE_PRESENT

	and	#0xe, k0		!  79 EX

	mov	k0, k3			!   5 MT (latency=0)
	mov	k2, k0			!   5 MT (latency=0)

	and	#1, k0			!  79 EX

	or	k0, k3			!  82 EX

	ldmmupteh(k0)			!   9 LS (latency=2)
	shll2	k4			! 101 EX		_PAGE_ACCESSED

	tst	k4, k2			!  68 MT

	mov.l	k3, @(MMU_PTEA-MMU_PTEH,k0)	! 27 LS

	mov.l	7f, k3			!   9 LS (latency=2)
					!  _PAGE_FLAGS_HARDWARE_MASK

	! k0=MMU_PTEH, k1=pte*, k2=pte, k3=_PAGE_FLAGS_HARDWARE,
	! k4=_PAGE_ACCESSED
#else

	! Test the entry for present and _PAGE_ACCESSED

	mov.l	7f, k3			!   9 LS (latency=2)
					!  _PAGE_FLAGS_HARDWARE_MASK
	tst	k4, k2			!  68 MT

	shll2	k4			! 101 EX		_PAGE_ACCESSED
	ldmmupteh(k0)			!   9 LS (latency=2)

	bt	20f			! 110 BR
	tst	k4, k2			!  68 MT

	! k0=MMU_PTEH, k1=pte*, k2=pte, k3=_PAGE_FLAGS_HARDWARE,
	! k4=_PAGE_ACCESSED

#endif

	! Set up the entry

	and	k2, k3			!  78 EX
	bt/s	10f			! 108 BR

	 mov.l	k3, @(MMU_PTEL-MMU_PTEH,k0)	! 27 LS

	ldtlb				! 128 CO

	! At least one instruction between ldtlb and rte
	nop				! 119 NOP

	rte				! 126 CO

	 nop				! 119 NOP


10:	or	k4, k2			!  82 EX

	ldtlb				! 128 CO

	! At least one instruction between ldtlb and rte
	mov.l	k2, @k1			!  27 LS

	rte				! 126 CO

	! Note we cannot execute mov here, because it is executed after
	! restoring SSR, so would be executed in user space.
	 nop				! 119 NOP

	! Either pgd or pte not present - bounce to general exception
	! handler
20:
	bra	general_exception
	 nop

	.align 5
	! One cache line if possible...
1:	.long	swapper_pg_dir
3:	.short	(PTRS_PER_PTE-1) << 2
4:	.short	(_PTRS_PER_PGD-1) << 2
5:	.long	_PAGE_PRESENT
7:	.long	_PAGE_FLAGS_HARDWARE_MASK
8:	.long	MMU_PTEH
#ifdef COUNT_EXCEPTIONS
9:	.long	exception_count_miss
#endif

	.balign 	512,0,512
interrupt:
	!  Interrupt vector

#if defined(CONFIG_KGDB)
	! Debounce (filter nested NMI)
	mov.l	2f, k2
	mov.l	@k2, k0
	mov.l	5f, k1
	cmp/eq	k1, k0
	bf	0f
	mov.l	6f, k1
	tas.b	@k1
	bt	0f
	rte
	 nop
	.align	2
2:	.long	INTEVT
5:	.long	NMI_VEC
6:	.long	in_nmi
0:
#endif /* defined(CONFIG_KGDB) */

	! Using k0, k1 for scratch registers (r0_bank1, r1_bank),
	! save all registers onto stack.
	!

	GUSA_CHECK

	stc	ssr, k0		! Is it from kernel space?
	shll	k0		! Check MD bit (bit30) by shifting it into...
	shll	k0		!       ...the T bit
	bt/s	1f		! It's a kernel to kernel transition.
	 mov	r15, k0		! save original stack to k0
	/* User space to kernel */
	mov	#(THREAD_SIZE >> 10), k1
	shll8	k1		! k1 := THREAD_SIZE
	shll2	k1
	add	current, k1
	mov	k1, r15		! change to kernel stack
	!
1:
	! Save the user registers on the stack.
	mov.l	6f, k3			! return address
	PUSH_USER_REGS

	mov.l	1f, r9
	mov.l	2f, r4
	mov.l	@r4, r4
	shlr2	r4
	shlr	r4
	mov	r4, r0		! save vector->jmp table offset for later
	shlr2	r4		! vector to IRQ# conversion
	add	#-0x10, r4

	cmp/pz	r4		! is it a valid IRQ?
	bf	10f

	jmp	@r9
	 mov	r15, r5

10:
	/*
	 * We got here as a result of taking the INTEVT path for something
	 * that isn't a valid hard IRQ, therefore we bypass the do_IRQ()
	 * path and special case the event dispatch instead.  This is the
	 * expected path for the NMI (and any other brilliantly implemented
	 * exception), which effectively wants regular exception dispatch
	 * but is unfortunately reported through INTEVT rather than
	 * EXPEVT.  Grr.
	 */
	mov.l	7f, r9
	mov.l	@(r0, r9), r9
	jmp	@r9
	 mov	r15, r8		! trap handlers take saved regs in r8

	.balign	32 /* d-cache align so only one line pulled */
1:	.long	do_IRQ
2:	.long	INTEVT
6:	.long	ret_from_irq
7:	.long   exception_handling_table
998: 	.long	0x000080f0
999: 	.long	0xcfffffff

!
! TLB Miss / Initial Page write exception handling
!			_and_
! TLB hits, but the access violate the protection.
! It can be valid access, such as stack grow and/or C-O-W.
!
!
! Find the pmd/pte entry and loadtlb
! If it's not found, cause address error (SEGV)
!
! Although this could be written in assembly language (and it'd be faster),
! this first version depends *much* on C implementation.
!

	.align	2
ENTRY(exception_none)
	rts
	 nop

#if defined(CONFIG_MMU)
ENTRY(tlb_miss_load)
	bra	call_dpf
	 mov	#0, r5

	/* we use global here in case we move the exception vector table */
	/* to another file (as in ex.S for SH3).  We don't use ENTRY or  */
	/* else we would be forcing each function into different cache   */
	/* lines - it is OK as is.                                       */
	.globl tlb_miss_store
tlb_miss_store:
	bra	call_dpf
	 mov	#1, r5

	.globl initial_page_write
initial_page_write:
	bra	call_dpf
	 mov	#1, r5

	.globl tlb_protection_violation_load
tlb_protection_violation_load:
	bra	call_dpf
	 mov	#0, r5

	.globl tlb_protection_violation_store
tlb_protection_violation_store:
	bra	call_dpf
	 mov	#1, r5

call_dpf:
	mov.l	1f, r0
	mov.l	@r0, r6		! address
	mov.l	3f, r0
	jmp	@r0
	 mov	r15, r4		! regs

	.balign 32	/* put in same d-cache line */
1:	.long	MMU_TEA
3:	.long	do_page_fault

ENTRY(address_error_load)
	bra	call_dae
	 mov	#0,r5		! writeaccess = 0

	.globl address_error_store
address_error_store:
	bra	call_dae
	 mov	#1,r5		! writeaccess = 1

call_dae:
	mov.l	1f, r0
	mov.l	@r0, r6		! address
	mov.l	2f, r0
	jmp	@r0
	 mov	r15, r4		! regs

	.align 2	/* does not span d-cache line due to ENTRY above */
1:	.long	MMU_TEA
2:	.long   do_address_error
#endif /* CONFIG_MMU */

#if defined(CONFIG_SH_STANDARD_BIOS)
	/* Unwind the stack and jmp to the debug entry */
ENTRY(sh_bios_handler)
	mov.l	@r15+, r0
	mov.l	@r15+, r1
	mov.l	@r15+, r2
	mov.l	@r15+, r3
	mov.l	@r15+, r4
	mov.l	@r15+, r5
	mov.l	@r15+, r6
	mov.l	@r15+, r7
	stc	sr, r8
	mov.l	1f, r9			! BL =1, RB=1, IMASK=0x0F
	or	r9, r8
	ldc	r8, sr			! here, change the register bank
	mov.l	@r15+, r8
	mov.l	@r15+, r9
	mov.l	@r15+, r10
	mov.l	@r15+, r11
	mov.l	@r15+, r12
	mov.l	@r15+, r13
	mov.l	@r15+, r14
	mov.l	@r15+, k0
	ldc.l	@r15+, spc
	lds.l	@r15+, pr
	mov.l	@r15+, k1
	ldc.l	@r15+, gbr
	lds.l	@r15+, mach
	lds.l	@r15+, macl
	mov	k0, r15
	!
	mov.l	2f, k0
	mov.l	@k0, k0
	jmp	@k0
	 ldc	k1, ssr
	.align	2
1:	.long	0x300000f0
2:	.long	gdb_vbr_vector
#endif /* CONFIG_SH_STANDARD_BIOS */


!
!  Undefined exception
!

ENTRY(exception_error)
	!
	TRACE_IRQS_ON
	sti
	mov.l	1f, r0
	jmp	@r0
	 nop

	.align	2
1:	.long	do_exception_error

!
!  Exit function after general exception and IRQ
!

ENTRY(ret_from_exception)
#if defined(CONFIG_PREEMPT)
	cli
	TRACE_IRQS_OFF
#endif
ret_from_irq:
	!
	mov	#OFF_SR, r0
	mov.l	@(r0,r15), r0	! get status register
	shll	r0
	shll	r0		! kernel space?
	get_current_thread_info r8, r0
#ifndef CONFIG_PREEMPT
	bf	resume_userspace
#else
	bt	resume_kernel	! Yes, it's from kernel, go back soon
	bra	resume_userspace
	 nop
ENTRY(resume_kernel)
	cli
	mov.l	@(TI_PRE_COUNT,r8), r0	! current_thread_info->preempt_count
	tst	r0, r0
	bf	noresched
need_resched:
	mov.l	@(TI_FLAGS,r8), r0	! current_thread_info->flags
	tst	#_TIF_NEED_RESCHED, r0	! need_resched set?
	bt	noresched

	mov	#OFF_SR, r0
	mov.l	@(r0,r15), r0		! get status register
	shlr	r0
	and	#(0xf0>>1), r0
	cmp/eq	#(0xf0>>1), r0		! interrupts off (exception path)?
	bt	noresched
	mov.l	3f, r0
	jsr	@r0			! call preempt_schedule_irq
	 nop
	bra	need_resched
	 nop

noresched:
	bra	restore_all
	 nop

	.balign 32 /* align to d-cache boundary */
1:	.long	PREEMPT_ACTIVE
2:	.long	schedule
3:	.long	preempt_schedule_irq
#endif

!
!  Restore user registers and return
!

restore_all:
#ifdef CONFIG_TRACE_IRQFLAGS
	mov	#OFF_SR, r0
	mov.l	@(r0,r15), r0		! get status register
	shlr2	r0
	and	#0x3c, r0
	cmp/eq	#0x3c, r0
	bt	6f
	TRACE_IRQS_ON
6:
#endif
	mov.l	@r15+, r0
	mov.l	@r15+, r1
	mov.l	@r15+, r2
	mov.l	@r15+, r3
	mov.l	@r15+, r4
	mov.l	@r15+, r5
	mov.l	@r15+, r6
	mov.l	@r15+, r7
	!
	stc	sr, r8
	mov.l	7f, r9
	or	r9, r8			! BL =1, RB=1
	ldc	r8, sr			! here, change the register bank
	!
	mov.l	@r15+, r8
	mov.l	@r15+, r9
	mov.l	@r15+, r10
	mov.l	@r15+, r11
	mov.l	@r15+, r12
	mov.l	@r15+, r13
	mov.l	@r15+, r14
	mov.l	@r15+, k4		! original stack pointer
	ldc.l	@r15+, spc
	lds.l	@r15+, pr
	mov.l	@r15+, k3		! original SR
	ldc.l	@r15+, gbr
	lds.l	@r15+, mach
	lds.l	@r15+, macl
	add	#4, r15			! Skip syscall number
	!
	! Calculate new SR value
	mov	k3, k2			! original SR value
	mov	#0xfffffff0, k1
	extu.b	k1, k1
	not	k1, k1
	and	k1, k2			! Mask original SR value
	!
	mov	k3, k0			! Calculate IMASK-bits
	shlr2	k0
	and	#0x3c, k0
	cmp/eq	#0x3c, k0
	bt/s	6f
	 shll2	k0
	mov	g_imask, k0
	!
6:	or	k0, k2			! Set the IMASK-bits
	ldc	k2, ssr
	!
#if defined(CONFIG_KGDB)
	! Clear in_nmi
	mov.l	6f, k0
	mov	#0, k1
	mov.b	k1, @k0
#endif
	mov	k4, r15
	rte
	 nop

	.balign	32 /* align to d-cache boundary */
#ifdef CONFIG_KGDB
6:	.long	in_nmi
#endif
7:	.long	0x30000000

!
!  Code to run before returning to user-space
!

ENTRY(resume_userspace)
	! r8: current_thread_info
	cli
	TRACE_IRQS_OFF
	mov.l	@(TI_FLAGS,r8), r0		! current_thread_info->flags
	tst	#(_TIF_WORK_MASK & 0xff), r0
	bt/s	restore_all
	 tst	#_TIF_NEED_RESCHED, r0

work_pending:
	! r0: current_thread_info->flags
	! r8: current_thread_info
	! t:  result of "tst	#_TIF_NEED_RESCHED, r0"
	bf/s	work_resched
	 tst	#(_TIF_SIGPENDING | _TIF_RESTORE_SIGMASK), r0
work_notifysig:
	bt/s	restore_all
	 mov	r15, r4		! [MT]
	mov.l	2f, r1		! [LS]
	mov	r0, r6		! [MT]
	mov	r12, r5		! [MT] set arg1(save_r0)
	mov.l	3f, r0		! [LS]
	jmp	@r1		! [CO]
	 lds	r0, pr		! [CO]
work_resched:
	mov.l	1f, r1
	jsr	@r1				! schedule
	 nop
	cli
	TRACE_IRQS_OFF
	!
	mov.l	@(TI_FLAGS,r8), r0		! current_thread_info->flags
	tst	#(_TIF_WORK_MASK & 0xff), r0
	bt	restore_all
	bra	work_pending
	 tst	#_TIF_NEED_RESCHED, r0

	.balign	32 /* align to d-cache boundary */
1:	.long	schedule
2:	.long	do_notify_resume
3:	.long	resume_userspace

/*
 * Syscall interface:
 *
 *	Syscall #: R3
 *	Arguments #0 to #3: R4--R7
 *	Arguments #4 to #6: R0, R1, R2
 *	TRA: (number of arguments + ABI revision) x 4
 *        (but see Note below)
 *
 * This code also handles delegating other traps to the BIOS/gdb stub
 * according to:
 *
 * Trap number
 * (TRA>>2)	Purpose
 * --------	-------
 * 0x00-0x0f	original SH-3/4 syscall ABI (unused).
 * 0x10-0x1f	general SH-3/4 syscall ABI.
 * 0x20-0x2f	aliased to 0x30 to 0x3f if KGDB or SH BIOS
 * 0x30-0x3f	debug traps used by the kernel (KDGB and SH BIOS)
 * 0x40-0xff	aliased to 0x30 to 0x3f if KGDB or SH BIOS
 *
 * Note: When we're first called, the TRA value must be shifted
 * right 2 bits in order to get the value that was used as the "trapa"
 * argument.  This argument is ONLY used when KGDB and/or SH BIOS are
 * configured into the kernel and in this case, any value > 0x1f will
 * call the function in the debug table entry (trapa value &0xf)
 *
 * If KGDB or SH BIOS are not configured, then any trapa value can be
 * used - they are unused anyway (even if the ABI contains the notion
 * that the number of parameters should for the basis for the trap
 * number, in practice this is not used).
 */

ENTRY(ret_from_fork)
	mov.l	7f, r8
	jsr	@r8
	 mov	r0, r4
	bra	syscall_exit
	 nop

/*
 * The poorly named main trapa decode and dispatch routine, for
 * system calls and debug traps through their respective jump tables.
 */
ENTRY(system_call)
	mov.l	0f, r11		!   9 LS (l2)	1 r11=INV_MASK
	mov     #OFF_TRA, r12   !   6 EX        1 r12=OFF_TRA

	mov.l   1f, r9          !   9 LS (l2)   1 r9=&TRA
	mov     #((0x20<<2)-1), r8     !   6 EX  1 r12=0x7f

	stc     sr, r10         ! 153 CO (l2i2) 2 r10=SR

	mov.l   @r9, r9         !   9 LS (l2)   1 r9=TRA
	and     r11, r10        !  78 EX        2 r10=SR&INV_MASK

	cmp/hi  r8, r9         !  57 MT        1 t=TRA>0x7f

	bt      debug_trap	! 110 BR        1

#ifdef CONFIG_TRACE_IRQFLAGS
	/* Need to preserve r3 */
	mov	r3, r8
	TRACE_IRQS_ON
	mov	r8, r3
#endif

/* Expanded sti  (also included in code above ) */
	stc     k_g_imask, r11

	or      r11, r10

        ldc     r10, sr

	get_current_thread_info r8, xx  ! 152 CO (l2i2) 3

	mov.l   @(TI_FLAGS,r8), r8      ! 18 LS (l2)    3
	add     r15, r12                !  49 EX        1 r12=SP+OFF_TRA

	shll2	r3		!     EX	syscall num x4
	mov.l   r9, @r12        !  27 LS        1 SP[OFF_TRA]=TRA

	mov     #(_TIF_WORK_SYSCALL_MASK & 0xff), r10	! 6 EX    3
	mov.l   2f, r12         !  9 LS (l2)    4 R12=number of syscallsx4

	tst     r10, r8         !  86 MT        3 T=tracing not enabled
	mov     #(_TIF_WORK_SYSCALL_MASK >> 8), r9	! 6 EX    3

	shll8   r9
	bf      syscall_trace_entry     ! 108 BR        3

	tst     r9, r8
	bf      syscall_trace_entry
	
	cmp/hs  r12, r3                 !  58 MT        4

	bt      syscall_badsys  ! 110 BR        4
syscall_call:
	mov.l	3f, r8		!     MT	Load address of sys_call_table

	add	r8, r3		!     EX

	mov.l	@r3, r8		!     MT

	jsr	@r8	    	!     CO	jump to specific syscall handler
	 nop

	mov.l	@(OFF_R0,r15), r12		! save r0
	mov.l	r0, @(OFF_R0,r15)		! save the return value
	!
syscall_exit:
	cli
	TRACE_IRQS_OFF
	!
	get_current_thread_info r8, r0
	mov.l	@(TI_FLAGS,r8), r0		! current_thread_info->flags
	tst	#(_TIF_ALLWORK_MASK & 0xff), r0
	mov	#(_TIF_ALLWORK_MASK >> 8), r1
	bf	syscall_exit_work
	shlr8	r0
	tst	r0, r1
	bf	syscall_exit_work
	bra	restore_all
	 nop
ret_from_exception_end:

!
!  Syscall exit function
!

syscall_exit_work:
	! r0: current_thread_info->flags
	! r8: current_thread_info
	tst	#(_TIF_WORK_SYSCALL_MASK & 0xff), r0
	bt/s	work_pending
	 tst	#_TIF_NEED_RESCHED, r0
	TRACE_IRQS_ON
	sti
	mov.l	9f, r0			! [LS] do_syscall_trace_leave
	mov	r15,r4			! [MT] pass stacked regs as arg
	jsr	@r0			! [CO]
	 mov	#0, r5			! [EX] trace entry [0]
	bra	resume_userspace
	 nop

	.align	2
syscall_trace_entry:
	!                     	Yes it is traced.
	mov.l	4f, r11		! [LS] Call do_syscall_trace_enter which notifies
	mov	r15,r4		! [MT] pass stacked regs as arg
	jsr	@r11	    	! [CO] superior (will chomp R[0-7])
	 mov	#1, r5		! [EX] trace entry [1]

	!			Reload R0-R4 from kernel stack, where the
	!   	    	    	parent may have modified them using
	!   	    	    	ptrace(POKEUSR).  (Note that R0-R2 are
	!   	    	    	used by the system call handler directly
	!   	    	    	from the kernel stack anyway, so don't need
	!   	    	    	to be reloaded here.)  This allows the parent
	!   	    	    	to rewrite system calls and args on the fly.
	mov.l	@(OFF_R4,r15), r4   ! arg0
	mov.l	@(OFF_R5,r15), r5
	mov.l	@(OFF_R6,r15), r6
	mov.l	@(OFF_R7,r15), r7   ! arg3
	mov.l	@(OFF_R3,r15), r3   ! syscall_nr
	!
	mov.l	2f, r10			! LS   Number of syscalls x4
	shll2	r3			! EX	syscall num x4

	cmp/hs	r10, r3
	bf	syscall_call
	mov	#-ENOSYS, r0
	bra	syscall_exit
	 mov.l	r0, @(OFF_R0,r15)	! Return value

debug_trap:
/*
 * The main debug trap handler.
 *
 * r9=TRA (not the trap number!)
 *
 * Note: This assumes that the trapa value is left in its original
 * form (without the shlr2 shift) so the calculation for the jump
 * call table offset remains a simple in place mask.
 *
 */
	mov	#(0xf << 2), r10

	mov.l	8f, r8
	and	r10, r9

	add	r9, r8

	mov.l	@r8, r8

	jmp	@r8
	 mov	r15, r4 /* Jump to kgdb, pass stacked regs as arg */

syscall_badsys:			! Bad syscall number
	get_current_thread_info r8, r0
	mov	#-ENOSYS, r0
	bra	resume_userspace
	 mov.l	r0, @(OFF_R0,r15)	! Return value

	.balign	32 /* d-cache line align */
0:	.long   0xffffff0f
1:	.long	TRA
2:	.long	(NR_syscalls<<2)
3:	.long	sys_call_table
4:	.long	do_syscall_trace_enter
7:	.long	schedule_tail	/* used by ret_from_fork */
8:	.long	debug_trap_table
9:	.long	do_syscall_trace_leave

!
!  Debug information
!
	.section	.debug_frame,"",@progbits
.LCIE:
	.ualong	.LCIE_end - .LCIE_start
.LCIE_start:
	.ualong	0xffffffff	/* CIE ID */
	.byte	0x1		/* Version number */
	.string	""		/* NUL-terminated augmentation string */
	.uleb128 0x1		/* Code alignment factor */
	.sleb128 -4		/* Data alignment factor */
	.byte	0x11		/* Return address register column */
				/* Augmentation length and data (none) */
	.byte	0xc		/* DW_CFA_def_cfa */
	.uleb128 0xf		/* r15 */
	.uleb128 0x0		/* offset 0 */

	.align 2
.LCIE_end:

	.ualong	.LFDE_end-.LFDE_start	/* Length FDE */
.LFDE_start:
	.ualong	.LCIE			/* CIE pointer */
	.ualong	ret_from_exception-4	/* absolute start address */
	.ualong	ret_from_exception_end-(ret_from_exception-4)
					/* length */
	.byte	0x4			/* DW_CFA_advance_loc4 */
	.ualong	0			/*   by */
	.byte	0xe			/* DW_CFA_def_cfa_offset */
	.uleb128 OFF_TRA+4		/*   advance CFA by sizeof(ptregs)+4 */

	.byte	0x80+0			/* DW_CFA_offset r0 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(0*4)))>>2
	.byte	0x80+1			/* DW_CFA_offset r1 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(1*4)))>>2
	.byte	0x80+2			/* DW_CFA_offset r2 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(2*4)))>>2
	.byte	0x80+3			/* DW_CFA_offset r3 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(3*4)))>>2
	.byte	0x80+4			/* DW_CFA_offset r4 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(4*4)))>>2
	.byte	0x80+5			/* DW_CFA_offset r5 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(5*4)))>>2
	.byte	0x80+6			/* DW_CFA_offset r6 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(6*4)))>>2
	.byte	0x80+7			/* DW_CFA_offset r7 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(7*4)))>>2
	.byte	0x80+8			/* DW_CFA_offset r8 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(8*4)))>>2
	.byte	0x80+9			/* DW_CFA_offset r9 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(9*4)))>>2
	.byte	0x80+10			/* DW_CFA_offset r10 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(10*4)))>>2
	.byte	0x80+11			/* DW_CFA_offset r11 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(11*4)))>>2
	.byte	0x80+12			/* DW_CFA_offset r12 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(12*4)))>>2
	.byte	0x80+13			/* DW_CFA_offset r13 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(13*4)))>>2
	.byte	0x80+14			/* DW_CFA_offset r14 */
	.uleb128 ((OFF_TRA+4)-(OFF_R0+(14*4)))>>2
	.byte	0x80+15			/* DW_CFA_offset r15 */
	.uleb128 ((OFF_TRA+4)-OFF_SP)>>2
	.byte	0x80+16			/* DW_CFA_offset r16=pc */
	.uleb128 ((OFF_TRA+4)-OFF_PC)>>2
	.byte	0x80+17			/* DW_CFA_offset r17=pr */
	.uleb128 ((OFF_TRA+4)-OFF_PR)>>2
	.align 2
.LFDE_end:

	.previous

	.data

	.globl	exception_handling_table
exception_handling_table:
	.long	exception_error		/* 000 */
	.long	exception_error
#if defined(CONFIG_MMU)
	.long	tlb_miss_load		/* 040 */
	.long	tlb_miss_store
	.long	initial_page_write
	.long	tlb_protection_violation_load
	.long	tlb_protection_violation_store
	.long	address_error_load
	.long	address_error_store	/* 100 */
#else
	.long	exception_error	! tlb miss load		/* 040 */
	.long	exception_error	! tlb miss store
	.long	exception_error	! initial page write
	.long	exception_error	! tlb prot violation load
	.long	exception_error	! tlb prot violation store
	.long	exception_error	! address error load
	.long	exception_error	! address error store	/* 100 */
#endif
#if defined(CONFIG_SH_FPU)
	.long	fpu_error_trap_handler	/* 120 */
#else
	.long	exception_error		/* 120 */
#endif
	.long	exception_error		/* 140 */
	.long	system_call	! Unconditional Trap	 /* 160 */
	.long	exception_error
		! reserved_instruction (filled by trap_init) /* 180 */
	.long	exception_error
		! illegal_slot_instruction (filled by trap_init) /*1A0*/

	! NOTE: don't use ENTRY() macros for data tables! ENTRY() contains alignment
	! operations which could break a linear table!!
	.globl  nmi_slot
	nmi_slot:
#if defined (CONFIG_KGDB)
	.long	debug_trap_table	/* 1C0 */	! Allow trap to debugger
#else
	.long	exception_none	/* 1C0 */	! Not implemented yet
#endif
	.globl	user_break_point_trap
	user_break_point_trap:
	.long	break_point_trap	/* 1E0 */

	.rept (0x820-0x1e0)/0x20	/* highest entry we access is at evt 0x820 in traps.c */
	.long exception_error
	.endr

#ifdef CONFIG_HIBERNATION

! save_regs()
! - save default tra, macl, mach, gbr, ssr, pr* and spc on the stack
! - save r15*, r14, r13, r12, r11, r10, r9, r8 on the stack
! - switch bank
! - save r7, r6, r5, r4, r3, r2, r1, r0 on the stack
! k0 contains original stack pointer*
! k1 trashed
! k3 passes original pr*
! k4 passes SR bitmask
! BL=1 on entry, on exit BL=0.

ENTRY(save_regs)
	mov	#-1, r1
	mov.l	k1, @-r15	! set TRA (default: -1)
	sts.l	macl, @-r15
	sts.l	mach, @-r15
	stc.l	gbr, @-r15
	stc.l	ssr, @-r15
	mov.l	k3, @-r15	! original pr in k3
	stc.l	spc, @-r15

	mov.l	k0, @-r15	! original stack pointer in k0
	mov.l	r14, @-r15
	mov.l	r13, @-r15
	mov.l	r12, @-r15
	mov.l	r11, @-r15
	mov.l	r10, @-r15
	mov.l	r9, @-r15
	mov.l	r8, @-r15

	mov.l	0f, k3		! SR bits to set in k3

! save_low_regs()
! - modify SR for bank switch
! - save r7, r6, r5, r4, r3, r2, r1, r0 on the stack
! k3 passes bits to set in SR
! k4 passes bits to clear in SR

ENTRY(save_low_regs)
	stc	sr, r8
	or	k3, r8
	and	k4, r8
	ldc	r8, sr

	mov.l	r7, @-r15
	mov.l	r6, @-r15
	mov.l	r5, @-r15
	mov.l	r4, @-r15
	mov.l	r3, @-r15
	mov.l	r2, @-r15
	mov.l	r1, @-r15
	rts
	 mov.l	r0, @-r15

! restore_regs()
! - restore r0, r1, r2, r3, r4, r5, r6, r7 from the stack
! - switch bank
! - restore r8, r9, r10, r11, r12, r13, r14, r15 from the stack
! - restore spc, pr*, ssr, gbr, mach, macl, skip default tra
! k2 returns original pr
! k3 returns original sr
! k4 returns original stack pointer
! r8 passes SR bitmask, overwritten with restored data on return
! r9 trashed
! BL=0 on entry, on exit BL=1 (depending on r8).

ENTRY(restore_regs)
	mov.l	@r15+, r0
	mov.l	@r15+, r1
	mov.l	@r15+, r2
	mov.l	@r15+, r3
	mov.l	@r15+, r4
	mov.l	@r15+, r5
	mov.l	@r15+, r6
	mov.l	@r15+, r7
	!
	stc	sr, r9
	or	r8, r9
	ldc	r9, sr
	!
	mov.l	@r15+, r8
	mov.l	@r15+, r9
	mov.l	@r15+, r10
	mov.l	@r15+, r11
	mov.l	@r15+, r12
	mov.l	@r15+, r13
	mov.l	@r15+, r14
	mov.l	@r15+, k4		! original stack pointer
	ldc.l	@r15+, spc
	mov.l	@r15+, k2		! original PR
	mov.l	@r15+, k3		! original SR
	ldc.l	@r15+, gbr
	lds.l	@r15+, mach
	lds.l	@r15+, macl
	rts
	 add	#4, r15			! Skip syscall numbe

.balign 4
0:	.long	0x000080f0		! FD=1, IMASK=15
#endif
