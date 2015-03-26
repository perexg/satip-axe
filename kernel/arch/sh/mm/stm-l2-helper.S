/*
 * arch/sh/mm/stm-l2-helper.S
 *
 * Copyright (C) 2008 STMicroelectronics
 * Written by Richard P. Curnow
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

	.text
	.little

	.macro L2SYNC l2syncreg, scratch
	mov	#1, \scratch
	mov.l	\scratch, @\l2syncreg
1:
	mov.l	@\l2syncreg, \scratch
	shlr	\scratch
	bt	1b
	.endm

	/* The CHAIN_* macros are used to build a chain of branches,
	 * interleaved (carefully) into the mainline code,
	 * which can be used to prefetch all the lines into the
	 * instruction cache before we start the real work.
	 */
	.macro CHAIN_HEAD
	sett
	bt	999f
	777:
	.endm

	/* There MUST be one of these per cache line's worth of code.
	 * Otherwise the prefetch will be incomplete. */
	.macro CHAIN_MID
	bra	888f
	nop
	.balign 32
	999:
	bt	999f
	888:
	.endm

	.macro CHAIN_TAIL
	.balign 32
	999:
	bra	777b
	nop
	.endm

	.balign 32
	.global stm_l2_copy_back_to_write_through_helper
stm_l2_copy_back_to_write_through_helper:

	/* args
	   r4 = top of range
	   r5 = L2CCR
	   r6 = L2FE
	   r7 = L2SYNC
	   */

	mov	#0, r2
	sts.l	pr, @-r15
	mov	#0x10, r3
	shll16	r3
	shll8	r3	! r3 = 1<<28

	! irq off
	stc	sr, r0
	or	r0, r3
	ldc	r3, sr	! block on

	CHAIN_HEAD
	synco
	bsr	do_l2_sync
	  nop

	CHAIN_MID
1:
	mov.l	r2, @r6
	add	#32, r2	! assumes L2 line size is 32; will not change
	cmp/hs	r4, r2
	bf	1b

	bsr	do_l2_sync
	  nop

	! cache now flushed
	mov.l	@r5, r1
	mov	#-3, r2	! 0xfffffffd aka ~2 aka ~(1<<1)
	and	r2, r1

	bsr do_l2_sync
	  mov.l	r1, @r5	! clear L2CCR.CBE

	ldc	r0, sr	! restore SR : block off

	CHAIN_MID

	lds.l	@r15+, pr
	rts
	  nop

	CHAIN_MID
do_l2_sync:
	L2SYNC	r7, r1
	rts
	  nop

	CHAIN_TAIL




