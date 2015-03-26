/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __STM_TAP_H
#define __STM_TAP_H

struct stm_tap;

/**
 * Execute a clock cycle - drives TCK signal low, then high.
 *
 * @param tms  Value of TMS signal (0/1) during transition.
 * @param tdi  Value of TDI signal (0/1) during transition.
 * @param priv Private value defined on initialisation.
 *
 * @return Value of TDO signal (0/1) after transition or
 *         negative value on error.
 */
typedef int (*stm_tap_tick)(int tms, int tdi);

/**
 * Initialise TAP object.
 *
 * @param tick JTAG operation function to be used by the TAP instance.
 * @param priv Private value passed to tick function.
 *
 * @return Pointer to TAP object or NULL on error.
 */
struct stm_tap *stm_tap_init(stm_tap_tick tick);

/**
 * Free TAP object.
 *
 * @params tap Pointer to TAP object.
 */
void stm_tap_free(struct stm_tap *tap);



/**
 * Enable TAP controller (enters Run-Test/Idle state).
 *
 * @params tap Pointer to TAP object.
 *
 * @return 0 on success, negative value on error
 */
int stm_tap_enable(struct stm_tap *tap);

/**
 * Disable TAP controller (enters Test-Logic-Reset).
 *
 * @params tap Pointer to TAP object.
 *
 * @return 0 on success, negative value on error
 */
int stm_tap_disable(struct stm_tap *tap);

/**
 * Shift (LSB first) in/out data or instruction registers value (going to
 * Shift-DR or Shift-IR state and then back to Run-Test/Idle).
 *
 * @param ir_not_dr 1 to access instruction registers,
 *                  0 to access data registers.
 * @param in        If not NULL points to data to be shifted into registers.
 * @param out       If not NULL points to buffer where registers data
 *                  will be shifted into.
 * @param bits      Number of bits to be shifted.
 * @param tap       Pointer to TAP object.
 *
 * @return 0 on success, negative value on error
 */
int stm_tap_shift(struct stm_tap *tap, int ir_not_dr,
		unsigned int *in, unsigned int *out, int bits);

#define stm_tap_shift_dr(tap, in, out, bits) \
		stm_tap_shift(tap, 0, in, out, bits)

#define stm_tap_shift_ir(tap, in, out, bits) \
		stm_tap_shift(tap, 1, in, out, bits)

#endif
