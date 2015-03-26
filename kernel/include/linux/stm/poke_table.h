/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#ifndef __STM_POKE_TABLE_H__
#define __STM_POKE_TABLE_H__

/* Opcode values */
#define OP_END_POKES					0
#define OP_POKE8					1
#define OP_POKE16					2
#define OP_POKE32					3
#define OP_OR8						4
#define OP_OR16						5
#define OP_OR32						6
#define OP_UPDATE8					7
#define OP_UPDATE16					8
#define OP_UPDATE32					9
#define OP_POKE_UPDATE32				10
#define OP_WHILE_NE8					11
#define OP_WHILE_NE16					12
#define OP_WHILE_NE32					13
#define OP_IF_EQ32					14
#define OP_IF_GT32					15
#define OP_ELSE						16
#define OP_DELAY					17
#define OP_IF_DEVID_GE					18
#define OP_IF_DEVID_LE					19

#ifdef __ASSEMBLER__

/* Poke table commands */
#define POKE8(A, VAL)				.long OP_POKE8, A, VAL
#define POKE16(A, VAL)				.long OP_POKE16, A, VAL
#define POKE32(A, VAL)				.long OP_POKE32, A, VAL
#define OR8(A, VAL)				.long OP_OR8, A, VAL
#define OR16(A, VAL)				.long OP_OR16, A, VAL
#define OR32(A, VAL)				.long OP_OR32, A, VAL
#define UPDATE8(A, AND, OR)			.long OP_UPDATE8, A, AND, OR
#define UPDATE16(A, AND, OR)			.long OP_UPDATE16, A, AND, OR
#define UPDATE32(A, AND, OR)			.long OP_UPDATE32, A, AND, OR
#define POKE_UPDATE32(A1, A2, AND, SHIFT, OR)	.long OP_POKE_UPDATE32,\
			 A1, A2, AND, SHIFT, OR
#define WHILE_NE8(A, AND, VAL)						\
	.long OP_WHILE_NE8, A, AND, VAL; .if (VAL > 0xFF);		\
	 ASM_ERROR("Value VAL in WHILE_NE8 should fit in 8 bits"); .endif
#define WHILE_NE16(A, AND, VAL)				.long OP_WHILE_NE16,\
		 A, AND, VAL; .if (VAL > 0xFFFF);\
		 ASM_ERROR("Value VAL in WHILE_NE16 should fit in 16 bits");\
		 .endif
#define WHILE_NE32(A, AND, VAL)				.long OP_WHILE_NE32,\
		 A, AND, VAL
#define IF_EQ32(NESTLEVEL, A, AND, VAL)					\
	.long OP_IF_EQ32, A, AND, VAL, (NESTLEVEL ## f - .)
#define IF_GT32(NESTLEVEL, A, AND, VAL)					\
	.long OP_IF_GT32, A, AND, VAL, (NESTLEVEL ## f - .)
/* An explicit ELSE will skip the OP_ELSE embedded in the ENDIF
 * to make things faster
 */
#define ELSE(NESTLEVEL)							\
	.long OP_ELSE; NESTLEVEL: ; .long (NESTLEVEL ## f - .)
/* ENDIF includes an OP_ELSE so that we end up at the correct position
 * regardless of whether there is an explcit ELSE in the IF construct
 */
#define ENDIF(NESTLEVEL)						\
	.long OP_ELSE; NESTLEVEL: ; .long 0
#define DELAY(ITERATIONS)						\
	.long OP_DELAY, ITERATIONS
/* The 2nd argument to the poke loop code (in R5 for ST40, or $r0.17 for ST200)
 * must be the device ID to compare against for these operations to work - the
 * poke loop code does not try to retrieve the device ID itself.
 */
#define IF_DEVID_GE(NESTLEVEL, VAL)					\
	.long OP_IF_DEVID_GE, VAL, (NESTLEVEL ## f - .)
#define IF_DEVID_LE(NESTLEVEL, VAL)					\
	.long OP_IF_DEVID_LE, VAL, (NESTLEVEL ## f - .)
/* The end marker needs two extra entries which get read by the code, but are
   never used.
 */
#define END_MARKER							\
	.long OP_END_POKES

#else
/* Poke table commands */
#define POKE8(A, VAL)				OP_POKE8, A, VAL
#define POKE16(A, VAL)				OP_POKE16, A, VAL
#define POKE32(A, VAL)				OP_POKE32, A, VAL
#define OR8(A, VAL)				OP_OR8, A, VAL
#define OR16(A, VAL)				OP_OR16, A, VAL
#define OR32(A, VAL)				OP_OR32, A, VAL
#define UPDATE8(A, AND, OR)			OP_UPDATE8, A, AND, OR
#define UPDATE16(A, AND, OR)			OP_UPDATE16, A, AND, OR
#define UPDATE32(A, AND, OR)			OP_UPDATE32, A, AND, OR
#define POKE_UPDATE32(A1, A2, AND, SHIFT, OR)	\
		OP_POKE_UPDATE32, A1, A2, AND, SHIFT, OR
#define WHILE_NE8(A, AND, VAL)			OP_WHILE_NE8, A, AND, VAL
#define WHILE_NE16(A, AND, VAL)			OP_WHILE_NE16, A, AND, VAL
#define WHILE_NE32(A, AND, VAL)			OP_WHILE_NE32, A, AND, VAL
#define IF_EQ32(NESTLEVEL, A, AND, VAL)
#define IF_GT32(NESTLEVEL, A, AND, VAL)
#define ELSE(NESTLEVEL)
#define ENDIF(NESTLEVEL)
#define DELAY(ITERATIONS)			OP_DELAY, ITERATIONS
#define IF_DEVID_GE(NESTLEVEL, VAL)
#define IF_DEVID_LE(NESTLEVEL, VAL)
#define END_MARKER				OP_END_POKES

#endif

#endif
