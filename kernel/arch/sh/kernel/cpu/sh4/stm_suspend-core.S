/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/linkage.h>
#include <cpu/mmu_context.h>
#include <linux/stm/poke_table.h>

/*
 * We need to ensure that this code is brought into the I and D-caches
 * before executing it, so that no external (to the CPU) memory accesses
 * occur other then the intended POKEs. To do this use the following macros:
 *    JUMPER_START and JUMPER_END around sequences which we want
 *	to be executed while loading up the cache
 *    JUMPER_EXEC embedded in the application code
 *    JUMPER_NOEXEC is a shorter sequence, where it is known the application
 *       will not be executing
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

ENTRY(stm_exec_table)
JUMPER_START
	mov.l   r13,  @-r15
	sett				! JUMPER sequences assume t=1
	sts.l	pr,  @-r15
JUMPER_END

.balign 32
JUMPER_NOEXEC
stm_really_exec_table:
	! runs the suspend instruction table
	bsr	DoPoking
	 mov	r13, r0

	mov	r7, r0
	tst	#1, r0			! checks NO_SLEEP flag
	bf	cpu_resume

	mov.l	13f, r0
	ldc	r0, vbr			! installs the wakeup_interrupt

	mov	#0x3c, r1
	shll2	r1
	not	r1,   r1

JUMPER_EXEC
	stc	sr,   r0
	and	r1,   r0		! 16
	ldc	r0,   sr		! enables the interrupts

	sleep				! SLEEP!!!

JUMPER_EXEC

.balign		32
wakeup_interrupt:
JUMPER_EXEC
	! Reset SR.RB bit
	mov.l	11f, r0
	stc	sr, r1
	and	r0, r1
	ldc	r1, sr		! on the main bank

	! Restore the Linux vbr
	mov.l	10f, r0
	ldc	r0,  vbr

	! check for early action with SR.BL bit equal 1
	mov	r7, r0
	tst	#2, r0		! check EARLY_ACTION flag
	bt	1f

	! runs the early_action instruction table
	bsr	DoPoking
	 mov	r13, r0
1:
	! Set SR.IMASK and reset SR.BL
	stc	sr, r0
	or	#0xf0,r0	! 16
	mov.l	12f, r1
	and	r1, r0
	ldc	r0, sr
	bra	cpu_resume
	 nop
JUMPER_EXEC

.balign		4
JUMPER_EXEC
	nop
cpu_resume:
	! runs the resume instruction table
	bsr     DoPoking
	 mov	r13, r0

	lds.l	@r15+, pr
	rts
	 mov.l   @r15+, r13

JUMPER_START
	mov.l	10f, r0		! from 10f to 13f are in dcache
JUMPER_END

.balign 32
10:	.long vbr_base
11:	.long ~(1 << 29)	! to reset the SR.RB bit
12:	.long ~(1 << 28)	! to reset the SR.BL bit
13:	.long wakeup_interrupt - 0x600

/*
 * The poke table is a series of long words, in the format:
 *
 *	opcode, operand, operand (, operand)*
 *
 * An opcode of 0 marks the table end.
 *
 * The DoPoking comes direclty from PBL.
 *
 */

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

/*
 * IF_EQ32(NESTLEVEL, A, AND, VAL)
 * Note that NESTLEVEL is not in the actual table, but there is a distance
 * field following VAL.
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

/*
 * IF_GT32(NESTLEVEL, A, AND, VAL)
 * Note that NESTLEVEL is not in the actual table, but there is a distance
 * field following VAL.
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

/*
 * ELSE(NESTLEVEL)
 * Note that NESTLEVEL is not in the actual table, but there is a distance
 * field following the opcode.
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

/*
 * IF_DEVID_GE(NESTLEVEL, VAL)
 * Note that NESTLEVEL is not in the actual table, but there is a distance
 * field following VAL.
 */
DoIfDevIDGE:
	cmp/hs	r1, r5		/* if (device ID >= VAL) */
	bt	DoPoking	/* go ahead with these pokes if true */
	bra	DoPoking
	 add	r2, r4

JUMPER_NOEXEC
/*
 * IF_DEVID_LE(NESTLEVEL, VAL)
 * Note that NESTLEVEL is not in the actual table, but there is a distance
 * field following VAL.
 */
DoIfDevIDLE:
	cmp/hi	r5, r1		/* if (device ID <= VAL) */
	bt	1f		/* go ahead with these pokes if true */
	add	r2, r4
1:	bra	DoPoking
	 nop

JUMPER_START
/*
 *	preload the instruction data
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
	bra	stm_really_exec_table
	 mov.l	@r0, r0

/*
 * The SwitchJumpTable must be in increasing numeric order of opcode (with
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
