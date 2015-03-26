/*
 * arch/sh/kernel/cpu/sh4/stm_hom-core.S
 *
 *  Copyright (C) 2010  STMicroelectronics
 *  Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/thread_info.h>
#include <cpu/mmu_context.h>
#include <asm/mmu.h>
#include <asm/cache.h>
#include <asm/thread_info.h>

#include <linux/stm/poke_table.h>

/*
 * We need to ensure that this code is brought into the I and D-caches
 * before executing it, so that no external (to the CPU) memory accesses
 * occur other then the intended POKEs. To do this use the following macros:
 *    JUMPER_START and JUMPER_END around sequences which we want
 *      to be executed while loading up the cache
 *    JUMPER_EXEC embedded in the application code
 *    JUMPER_NOEXEC is a shorter sequence, where it is known the application
 *	will not be executing
 * one of these sequences needs to occur at least once per I-cache line.
 */
#define JUMPER_START		\
	8:

#define JUMPER_END		\
		bt 8f

#define JUMPER_NOEXEC		\
	JUMPER_START		\
	JUMPER_END

#define JUMPER_EXEC		\
		bra 81f;	\
		 nop;		\
	JUMPER_NOEXEC;		\
	81:

.text
ENTRY(stm_hom_exec_table)
	mov.l   r14, @-r15
	mov.l   r13, @-r15
	mov.l   r12, @-r15
	mov.l   r11, @-r15
	mov.l   r10, @-r15
	mov.l   r9,  @-r15
	mov.l   r8,  @-r15
	sts.l	pr,  @-r15	! save the pr (we can call other function)
	stc.l	sr,  @-r15

	stc.l	r6_bank, @-r15	! save the original r6_bank on the stack
	stc.l	r7_bank, @-r15	! save the original r7_bank on the stack

	mov.l	100f, r0	! save the stack value on
				! stm_hom_saved_stack_value
	mov.l	r15, @r0
	ocbp	@r0		! push the r15 value in main memory

	ocbp	@r15		! push the stack in main memory	(1/3)
	add	#32, r15
	ocbp    @r15		! push the stack in main memory	(2/3)
	add     #32, r15
	ocbp    @r15		! push the stack in main memory	(3/3)

	sett
	JUMPER_END		! start the jump sequence

	JUMPER_EXEC
really_stm_hom_exec_table:

	/*
	 * runs the iteration tables to turn-off the SOC
	 */
	bsr	DoPoking	! this DoPoking will __NOT__ return...
	 mov	r13, r0		! DoPoking wants in r0 the
				! SwitchJumpTable address

	sleep			! sleep with interrupt disabled
				! not really a problem becasue
				! the SOC will be turned-off

JUMPER_START
	/*
	 * load the instruction datas
	 */
	mov	r4, r0		/* start address I-table */
	mov	r5, r1		/* I-table size */
	tst	r1, r1
2:
	mov.l   @r0, r2			/* Load the I-table in cache */
	add	#32, r0
        bf/s	2b
         dt	r1

	sett				! set t bit to restart jump
	JUMPER_END			! sequence

.balign	32
JUMPER_NOEXEC
DoPoking:
	mov.l	@r4+, r5	/* opcode */
	mov.l	@r4+, r1	/* operand 1 */
	mov.l	@r4+, r2	/* operand 2 */

	mov.b	@(r0, r5), r5
	extu.b	r5, r5
	braf	r5
	  nop

SwitchJumpFrom:

/* END_MARKER */
DoRet:
	rts			/* Return point */
	 add #-8, r4

/* POKE8(A, VAL) */
DoPoke8:
	bra	DoPoking
	  mov.b	r2, @r1		/* *A = VAL */

/* POKE16(A, VAL) */
DoPoke16:
	bra	DoPoking
	  mov.w	r2, @r1		/* *A = VAL */

/* POKE32(A, VAL) */
DoPoke32:
	bra	DoPoking
	  mov.l	r2, @r1		/* *A = VAL */ ! 16

/* OR8(A, VAL) */
DoOr8:
	mov.b	@r1, r3		/* *A */
	or	r2, r3		/* *A | OR */
	bra	DoPoking
	  mov.b	r3, @r1		/* *A |= OR */
JUMPER_NOEXEC

/* OR16(A, VAL) */
DoOr16:
	mov.w	@r1, r3		/* *A */
	or	r2, r3		/* *A | OR */
	bra	DoPoking
	  mov.w	r3, @r1		/* *A |= OR */

/* OR32(A, VAL) */
DoOr32:
	mov.l	@r1, r3		/* *A */
	or	r2, r3		/* *A | OR */
	bra	DoPoking
	 mov.l	r3, @r1		/* *A |= OR */

/* UPDATE8(A, AND, OR) */
DoUpdate8:
	mov.b	@r1, r3		/* *A */
	and	r2, r3		/* *A & AND */
	mov.b	@r4+, r2	/* read OR */ !16
	add	#3, r4
	or	r2, r3		/* (*A & AND) | OR */
	bra	DoPoking
	  mov.b	r3, @r1		/* *A = ((*A & AND) | OR) */

JUMPER_NOEXEC
/* UPDATE16(A, AND, OR) */
DoUpdate16:
	mov.w	@r1, r3		/* *A */
	and	r2, r3		/* *A & AND */
	mov.w	@r4+, r2	/* read OR */
	add	#2, r4
	or	r2, r3		/* (*A & AND) | OR */
	bra	DoPoking
	  mov.w	r3, @r1		/* *A = ((*A & AND) | OR) */

/* UPDATE32(A, AND, OR) */
DoUpdate32:
	mov.l	@r1, r3		/* *A */
	and	r2, r3		/* *A & AND */
	mov.l	@r4+, r2	/* read OR */
	or	r2, r3		/* (*A & AND) | OR */
	bra	DoPoking
	 mov.l	r3, @r1		/* *A = ((*A & AND) | OR) */

JUMPER_NOEXEC
/* POKE_UPDATE32(A1, A2, AND, SHIFT, OR) */
DoPokeUpdate32:
	mov.l	@r2, r3		/* *A2 */
	mov.l	@r4+, r2	/* read AND */
	and	r2, r3		/* *A2 & AND */
	mov.l	@r4+, r2	/* read SHIFT */
	shld	r2, r3		/* (*A2 & AND) << SHIFT */
	mov.l	@r4+, r2	/* read OR */
	or	r2, r3		/* ((*A2 & AND) << SHIFT) | OR */
	bra	DoPoking
	 mov.l	r3, @r1		/* *A1 = (((*A2 & AND) << SHIFT) | OR) */

/* WHILE_NE8(A, AND, VAL) */
DoWhileNe8:
	mov.l	@r4+, r5	/* read VAL */
1:	mov.b	@r1, r3		/* *A */
	extu.b	r3, r3		/* 32bit(*A) */
	and	r2, r3		/* *A & AND */
	cmp/eq	r3, r5		/* if ((*A & AND) == VAL) */
	bf	1b		/* loop if false */
	bt	DoPoking

JUMPER_NOEXEC
/* WHILE_NE16(A, AND, VAL) */
DoWhileNe16:
	mov.l	@r4+, r5	/* read VAL */
1:	mov.w	@r1, r3		/* *A */
	extu.w	r3, r3		/* 32bit(*A) */
	and	r2, r3		/* *A & AND */
	cmp/eq	r3, r5		/* if ((*A & AND) == VAL) */
	bf	1b		/* loop if false */
	bt	DoPoking

/* WHILE_NE32(A, AND, VAL) */
DoWhileNe32:
	mov.l	@r4+, r5	/* read VAL */
1:	mov.l	@r1, r3		/* *A */
	and	r2, r3		/* *A & AND */
	cmp/eq	r3, r5		/* if ((*A & AND) == VAL) */
	bf	1b		/* loop if false */
	bt	DoPoking
JUMPER_NOEXEC

/* IF_EQ32(NESTLEVEL, A, AND, VAL)
   Note that NESTLEVEL is not in the actual table, but there is a distance
   field following VAL.
 */
DoIfEq32:
	mov.l	@r1, r1		/* *A */
	and	r2, r1		/* *A & AND */
	mov.l	@r4+, r2	/* read VAL */
	mov.l	@r4+, r3	/* read distance to ELSE/ENDIF */
	cmp/eq	r2, r1		/* if ((*A & AND) == VAL) */
	bt	DoPoking	/* go ahead with these pokes */
	add	r3, r4		/* skip forward through pokes to ELSE or ENDIF*/
	bf	DoPoking

/* IF_GT32(NESTLEVEL, A, AND, VAL)
   Note that NESTLEVEL is not in the actual table, but there is a distance
   field following VAL.
 */
JUMPER_NOEXEC
DoIfGT32:
	mov.l	@r1, r1		/* *A */
	and	r2, r1		/* *A & AND */
	mov.l	@r4+, r2	/* read VAL */
	mov.l	@r4+, r3	/* read distance to ELSE/ENDIF */
	cmp/hi	r2, r1		/* if ((*A & AND) > VAL) */
	bt	DoPoking	/* go ahead with these pokes if true*/
	add	r3, r4		/* skip forward through pokes to ELSE or ENDIF*/
	bf	DoPoking

/* ELSE(NESTLEVEL)
   Note that NESTLEVEL is not in the actual table, but there is a distance
   field following the opcode.
 */
DoElse:
	add	#-4, r4		/* We took 1 arg too many from r4 for a delay */
	bra	DoPoking
	  add	r1, r4		/* skip through to ENDIF */

JUMPER_NOEXEC
/* DELAY(ITERATIONS) */
DoDelay:
	mov	r6, r2		/* _1_ms_lpj */
	tst	r2, r2
2:	bf/s	2b
	 dt	r2
	tst	r1, r1
	bf/s	DoDelay
	 dt	r1
	bra	DoPoking
	 add	#-4, r4		/* We took 1 arg too many from r4 for a delay */

/* IF_DEVID_GE(NESTLEVEL, VAL)
   Note that NESTLEVEL is not in the actual table, but there is a distance
   field following VAL.
 */
DoIfDevIDGE:
	cmp/hs	r1, r5		/* if (device ID >= VAL) */
	bt	DoPoking	/* go ahead with these pokes if true */
	bra	DoPoking
	  add	r2, r4

JUMPER_NOEXEC
/* IF_DEVID_LE(NESTLEVEL, VAL)
   Note that NESTLEVEL is not in the actual table, but there is a distance
   field following VAL.
 */
DoIfDevIDLE:
	cmp/hi	r5, r1		/* if (device ID <= VAL) */
	bt	1f		/* go ahead with these pokes if true */
	add	r2, r4
1:	bra	DoPoking
	  nop

JUMPER_START
/*
 *	preload the instruction datas
 */
	mov	r4, r0			/* start address I-table */
	tst	r5, r5
2:
	mov.l   @r0, r2			/* Load the I-tables in cache */
	add	#32, r0
        bf/s	2b
         dt	r5

	/* Ensure the jump table is in the data cache */
	mova	SwitchJumpTable, r0	/* Keep this in r0 for use in DoPoking*/
	mov	r0, r13
	bra	really_stm_hom_exec_table
	 mov.l	@r0, r0

/* The SwitchJumpTable must be in increasing numeric order of opcode (with
 * padding for any missing entries).  Distance between SwitchJumpTable and any
 * of the operations must be less than 255 bytes (the assembler should point it
 * out if we ever break that condition and have to switch to 16 bit values).
 */
	.balign 32
SwitchJumpTable:
        .byte   DoRet - SwitchJumpFrom
        .byte   DoPoke8 - SwitchJumpFrom
        .byte   DoPoke16 - SwitchJumpFrom
        .byte   DoPoke32 - SwitchJumpFrom
        .byte   DoOr8 - SwitchJumpFrom
        .byte   DoOr16 - SwitchJumpFrom
        .byte   DoOr32 - SwitchJumpFrom
        .byte   DoUpdate8 - SwitchJumpFrom
        .byte   DoUpdate16 - SwitchJumpFrom
        .byte   DoUpdate32 - SwitchJumpFrom
        .byte   DoPokeUpdate32 - SwitchJumpFrom
        .byte   DoWhileNe8 - SwitchJumpFrom
        .byte   DoWhileNe16 - SwitchJumpFrom
        .byte   DoWhileNe32 - SwitchJumpFrom
        .byte   DoIfEq32 - SwitchJumpFrom
        .byte   DoIfGT32 - SwitchJumpFrom
        .byte   DoElse - SwitchJumpFrom
        .byte   DoDelay - SwitchJumpFrom
        .byte   DoIfDevIDGE - SwitchJumpFrom
        .byte   DoIfDevIDLE - SwitchJumpFrom

#ifdef CONFIG_HOM_DEBUG
.macro MARK_STEP	x
	mova	hom_mark,	r0
	mov	#(\x),	r1
	mov.l	r1,	@r0
	ocbp	@r0
.endm
#else
.macro MARK_STEP	x
.endm
#endif

/*
 * Condition at the entry of defrost_kernel:
 *
 *   BSC has already been initialized.
 *   INTC may or may not be initialized.
 *   VBR may or may not be initialized.
 *   MMU may or may not be initialized.
 *   Cache may or may not be initialized.
 *   Hardware (including on-chip modules) may or may not be initialized.
 *
 */
ENTRY(stm_defrost_kernel)	! This function returns
				! to the caller of stm_hom_exec_table

	!			Initialize Status Register
	mov.l	1f, r0		! MD=1, RB=0, BL=0, IMASK=0xF
	ldc	r0, sr
	!			Initialize global interrupt mask
	mov	#0, r0
	ldc	r0, r6_bank

	! First of all restore the original
	! Linux VBR ...
	mov.l	4f, r0
	ldc	r0, vbr


	! mark the step 1
	MARK_STEP 	1

	! use a temporary stack in uncached-data
	! this is required because the final stack
	! can be every where in the memory (also in
	! memory not mapped in the boot-loader)
	mov.l	101f, r0
	mov.l	102f, r15
	ldc	r0, r7_bank
	add	r0, r15

	! mark the step 2
	MARK_STEP 	2

					! to restore the PMB
	mov.l   6f, r0
	jsr	@r0
         nop

	! mark the step 3
	MARK_STEP	3

	mov.l	2f, r0			! Turn-on the Cache
	jsr	@r0
	 nop

	! mark the step 4
	MARK_STEP	4
					! Turn-on the MMU
	mov.l	3f, r0
	mov.l	5f, r1
	mov.l	r1, @r0

	! mark the step 5
	MARK_STEP	5

	! restore the final frozen stack
	mov.l	100f, r0
	mov.l	@r0, r15

	! Restore the r6/r7 bank
	! to be sure the code runs on the right
	! process_stack
	ldc.l	@r15+, r7_bank
	ldc.l	@r15+, r6_bank

	! Restore the remaining registers
	ldc.l	@r15+, sr
	lds.l	@r15+, pr
	mov.l	@r15+, r8
	mov.l	@r15+, r9
	mov.l	@r15+, r10
	mov.l	@r15+, r11
	mov.l	@r15+, r12
	mov.l	@r15+, r13
	mov.l	@r15+, r14
	rts			! Returns where the 'stm_hom_exec_table'
				! was called
	 nop

	.balign 4
1:	.long	0x400080F0	! MD=1, RB=0, BL=0, FD=1, IMASK=0xF
2:	.long	stm_hom_cache_init
3:	.long	MMUCR
4:	.long	vbr_base
5:	.long	MMU_CONTROL_INIT | MMUCR_SE
/*
 * Not so nice the pmb_sysdev_resume is called twice but
 * I want restore the pmb as soon as possible....
 * and at the end it should not be a big issue...
 */
6:	.long	stm_hom_pmb_init
100:	.long	stm_hom_saved_stack_value	! the final stack
101:	.long	stm_hom_boot_stack		! a temporary stack
102:	.long	THREAD_SIZE
#ifdef CONFIG_HOM_DEBUG
hom_mark:	.long	0			! just for debug
#endif
