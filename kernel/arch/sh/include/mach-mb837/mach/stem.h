/*
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: Angus Clark <angus.clark@st.com>
 *
 * The mb837 STEM interface can be configured in a bewildering number of
 * ways. Two possible configurations are defined here: one for the mb837
 * standalone; and one for the mb837 mated with the mb705 peripheral board.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __ASM_SH_MB837_STEM_H
#define __ASM_SH_MB837_STEM_H

#include <asm/irq-ilc.h>

#ifdef CONFIG_SH_ST_MB705
/* mb837 + mb705 (signals routed via mb705 ELPD)
 *	STEM_CS0 = BANK2 + (1<<24) (J9-A = DNF; J12-B = fit; J9-B = DNF)
 *	STEM_CS1 = BANK3 (J12-H = DNF; J12-F = fit; J12-G = DNF)
 *	STEM_NAND_RDY = 0 (J11 = 2-3 mb705)
 *	STEM_INTR1 = SysIRQ2 (J4-B = DNF)
 *	STEM_INTR0 = SysIRQ1 (J4-A = DNF)
 */
#define STEM_CS0_BANK		2
#define STEM_CS0_OFFSET		(1<<24)
#define STEM_CS1_BANK		3
#define STEM_CS1_OFFSET		0
#define STEM_NAND_RDY		0
#define STEM_INTR0_IRQ		ILC_EXT_IRQ(2)
#define STEM_INTR1_IRQ		ILC_EXT_IRQ(1)
#else
/*
 * mb837 standalone
 *	STEM CS0 = BANK0 (J9-A = fit; J12-B = DNF; J9-B = DNF)
 *	STEM_CS1 = BANK3 (J12-H = DNF; J12-F = DNF; J12-G = fit)
 *	STEM_NAND_RDY = 1 (J11 = 1-2)
 *	STEM_INTR0 = SysIRQ1 (J4-A = fit)
 *	STEM_INTR1 = SysIRQ2 (J4-B = fit)
 */
#define STEM_CS0_BANK		0
#define STEM_CS0_OFFSET		0
#define STEM_CS1_BANK		3
#define STEM_CS1_OFFSET		0
#define STEM_NAND_RDY		1
#define STEM_INTR0_IRQ		ILC_EXT_IRQ(2)
#define STEM_INTR1_IRQ		ILC_EXT_IRQ(1)
#endif

#endif /* __ASM_SH_MB837_STEM_H */
