/*
 * linux/arch/sh/boards/mb360/led.c
 *
 * Copyright (C) 2000 Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * This file contains LED code for the ST40RA/ST40STB1 Eval board.
 */

#include <linux/stm/pio.h>
#include <asm/io.h>
#include <asm/led.h>
#include <mach/epld.h>

/* ST40 Eval: Flash LD9 (PIO LED) connected to PIO1 bit 3 */
void mach_led(int position, int value)
{
	static struct stpio_pin *led = NULL;

	if (led == NULL) {
		led = stpio_request_pin(1, 3, "LED", STPIO_OUT);
	}

	stpio_set_pin(led, value);
}
