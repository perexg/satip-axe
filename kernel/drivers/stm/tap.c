/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/kernel.h>

#include "tap.h"

enum stm_tap_state {
	TAP_TEST_LOGIC_RESET,
	TAP_RUN_TEST_IDLE,
	TAP_SELECT_DR_SCAN,
	TAP_SELECT_IR_SCAN,
	TAP_CAPTURE_DR,
	TAP_SHIFT_DR,
	TAP_EXIT1_DR,
	TAP_PAUSE_DR,
	TAP_EXIT2_DR,
	TAP_UPDATE_DR,
	TAP_CAPTURE_IR,
	TAP_SHIFT_IR,
	TAP_EXIT1_IR,
	TAP_PAUSE_IR,
	TAP_EXIT2_IR,
	TAP_UPDATE_IR,
	__TAP_LAST
};

static enum stm_tap_state stm_tap_fsm[__TAP_LAST][2] = {
	/*                         TMS = 0               TMS = 1 */
	[TAP_TEST_LOGIC_RESET] = { TAP_RUN_TEST_IDLE,    TAP_TEST_LOGIC_RESET },
	[TAP_RUN_TEST_IDLE] =    { TAP_RUN_TEST_IDLE,    TAP_SELECT_DR_SCAN },
	[TAP_SELECT_DR_SCAN] =	 { TAP_CAPTURE_DR,       TAP_SELECT_IR_SCAN },
	[TAP_SELECT_IR_SCAN] =   { TAP_CAPTURE_IR,       TAP_TEST_LOGIC_RESET },
	[TAP_CAPTURE_DR] =       { TAP_SHIFT_DR,         TAP_EXIT1_DR },
	[TAP_SHIFT_DR] =         { TAP_SHIFT_DR,         TAP_EXIT1_DR },
	[TAP_EXIT1_DR] =         { TAP_PAUSE_DR,         TAP_UPDATE_DR },
	[TAP_PAUSE_DR] =         { TAP_PAUSE_DR,         TAP_EXIT2_DR },
	[TAP_EXIT2_DR] =         { TAP_SHIFT_DR,         TAP_UPDATE_DR },
	[TAP_UPDATE_DR] =        { TAP_RUN_TEST_IDLE,    TAP_SELECT_DR_SCAN },
	[TAP_CAPTURE_IR] =       { TAP_SHIFT_IR,         TAP_EXIT1_IR },
	[TAP_SHIFT_IR] =         { TAP_SHIFT_IR,         TAP_EXIT1_IR },
	[TAP_EXIT1_IR] =         { TAP_PAUSE_IR,         TAP_UPDATE_IR },
	[TAP_PAUSE_IR] =         { TAP_PAUSE_IR,         TAP_EXIT2_IR },
	[TAP_EXIT2_IR] =         { TAP_SHIFT_IR,         TAP_UPDATE_IR },
	[TAP_UPDATE_IR] =        { TAP_RUN_TEST_IDLE,    TAP_SELECT_DR_SCAN }
};

struct stm_tap {
	enum stm_tap_state state;
	stm_tap_tick tick;
};

static struct stm_tap stm_tap;
static int stm_tap_in_use;

struct stm_tap *stm_tap_init(stm_tap_tick tick)
{
	struct stm_tap *tap;
	int i;

	if (stm_tap_in_use)
		return NULL;

	tap = &stm_tap;
	stm_tap_in_use = 1;

	tap->state = TAP_TEST_LOGIC_RESET;
	tap->tick = tick;

	/* Five transitions with TMS=1 should get us to Test-Logic/Reset
	 * state from any other state of the FSM... */
	for (i = 0; i < 5; i++)
		tick(1, 0);

	return tap;
}

void stm_tap_free(struct stm_tap *tap)
{
	if (tap != &stm_tap)
		return;

	BUG_ON(tap->state != TAP_TEST_LOGIC_RESET);

	tap->tick = NULL;

	stm_tap_in_use = 0;
}



static int stm_tap_step(struct stm_tap *tap, enum stm_tap_state state, int tdi)
{
	int tms;

	pr_debug("%s(tap->state=%d, state=%d, tdi=%d)\n",
			__func__, tap->state, state, tdi);

	if (stm_tap_fsm[tap->state][0] == state) {
		tms = 0;
	} else if (stm_tap_fsm[tap->state][1] == state) {
		tms = 1;
	} else {
		BUG();
		return -1;
	}

	tap->state = state;

	return tap->tick(tms, tdi);
}

static int stm_tap_goto(struct stm_tap *tap, const enum stm_tap_state *path)
{
	while (*path != __TAP_LAST) {
		int tdo = stm_tap_step(tap, *path, 0);

		if (tdo < 0)
			return tdo;

		path++;
	}

	return 0;
}



int stm_tap_enable(struct stm_tap *tap)
{
	static const enum stm_tap_state const path[] = {
		TAP_RUN_TEST_IDLE,
		__TAP_LAST
	};

	BUG_ON(tap->state != TAP_TEST_LOGIC_RESET);

	return stm_tap_goto(tap, path);
}

int stm_tap_disable(struct stm_tap *tap)
{
	static const enum stm_tap_state const path[] = {
		TAP_SELECT_DR_SCAN,
		TAP_SELECT_IR_SCAN,
		TAP_TEST_LOGIC_RESET,
		__TAP_LAST
	};

	BUG_ON(tap->state != TAP_RUN_TEST_IDLE);

	return stm_tap_goto(tap, path);
}

int stm_tap_shift(struct stm_tap *tap, int ir_not_dr,
		unsigned int *in, unsigned int *out, int bits)
{
	int result;
	static const enum stm_tap_state const dr_path_there[] = {
		TAP_SELECT_DR_SCAN,
		TAP_CAPTURE_DR,
		TAP_SHIFT_DR,
		__TAP_LAST
	};
	static const enum stm_tap_state const dr_path_back[] = {
		TAP_UPDATE_DR,
		TAP_RUN_TEST_IDLE,
		__TAP_LAST
	};
	static const enum stm_tap_state const ir_path_there[] = {
		TAP_SELECT_DR_SCAN,
		TAP_SELECT_IR_SCAN,
		TAP_CAPTURE_IR,
		TAP_SHIFT_IR,
		__TAP_LAST
	};
	static const enum stm_tap_state const ir_path_back[] = {
		TAP_UPDATE_IR,
		TAP_RUN_TEST_IDLE,
		__TAP_LAST
	};
	unsigned int tdi = 0;
	unsigned int tdo = 0;
	unsigned int tdo_mask = 1;
	enum stm_tap_state next_state;

	pr_debug("%s(ir_not_dr=%d, in=%p, out=%p, bits=%d)\n",
			__func__, ir_not_dr, in, out, bits);

	BUG_ON(tap->state != TAP_RUN_TEST_IDLE);

	/* This code handles up to 32 bits data blocks in this moment.
	 * If more is needed, it shouldn't be hard to implement... */
	if (bits < 1 || bits > sizeof(*in) * 8)
		return -1;

	result = stm_tap_goto(tap, ir_not_dr ? ir_path_there : dr_path_there);
	if (result < 0)
		return result;

	if (in)
		tdi = *in;

	pr_debug("%s(): tdi=0x%x\n", __func__, tdi);

	/* We are in Shift-IR or Shift-DR state here. Time to start with
	 * the data. Least significant bit enters goes in & out first... */
	next_state = ir_not_dr ? TAP_SHIFT_IR : TAP_SHIFT_DR;
	while (bits) {
		/* Last bit is shifted in during Shift-xR to Exit1-xR
		 * transition */
		if (bits == 1)
			next_state = ir_not_dr ? TAP_EXIT1_IR : TAP_EXIT1_DR;

		result = stm_tap_step(tap, next_state, tdi & 0x1);
		if (result < 0)
			return result;
		BUG_ON(result > 1);

		tdi >>= 1;

		tdo |= result ? tdo_mask : 0;
		tdo_mask <<= 1;

		bits--;
	}

	if (out)
		*out = tdo;

	pr_debug("%s(): tdo=0x%x\n", __func__, tdo);

	return stm_tap_goto(tap, ir_not_dr ? ir_path_back : dr_path_back);
}
