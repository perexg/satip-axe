/*
 * arch/sh/boards/st/common/db679.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics Parallel ATA STEM board
 *
 * This code assumes that STEM_notCS0 and STEM_notINTR0 lines are used,
 * so jumpers J1 and J2 shall be set to 1-2 positions.
 *
 * Some additional main board setup may be required to use proper CS signal
 * signal - see "arch/sh/include/mach-<board>/mach/stem.h" for more
 * information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7200.h>
#include <mach/stem.h>



static int __init db679_init(void)
{
#if defined(CONFIG_CPU_SUBTYPE_STX7100)
	stx7100_configure_pata(&(struct stx7100_pata_config) {
			.emi_bank = STEM_CS0_BANK,
			.pcm_mode = 0,
			.irq = STEM_INTR0_IRQ });
#elif defined(CONFIG_CPU_SUBTYPE_STX7105)
	/* Need to use STEM bank 1 as bank 0 isn't big enough */
	stx7105_configure_pata(&(struct stx7105_pata_config) {
			.emi_bank = STEM_CS1_BANK,
			.pc_mode = 0,
			.irq = STEM_INTR1_IRQ });
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
	stx7200_configure_pata(&(struct stx7200_pata_config) {
			.emi_bank = STEM_CS0_BANK,
			.pc_mode = 0,
			.irq = STEM_INTR0_IRQ });
#else
#	error Unsupported SOC.
#endif
	return 0;
}
arch_initcall(db679_init);

