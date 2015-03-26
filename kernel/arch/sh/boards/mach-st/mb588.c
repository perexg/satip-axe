/*
 * arch/sh/boards/st/common/mb588.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics NAND Flash STEM board
 *
 * Jumper Settings:
 *     J1 "CS Routing"
 *         1-2:    STEM_notCS0
 *         2-3:    STEM_notCS1 [1]
 *
 *     J2 "RBn Routing"
 *         Not used [2]
 *
 *     J3 "Write Protect"
 *         open:   unprotected
 *         closed: protected
 *
 *     CN2
 *         Do Not Fit
 *
 * Notes:
 *     [1] Some host boards generate the STEM_notCSx signals by sub-decoding an
 *         EMI bank based on the EMI address accessed (eg mb705 EPLD).  The
 *         stm_nand_emi driver accesses NAND via the EMI buffer and will
 *         therefore work as expected with sub-decoded EMI banks.  However, if
 *         using the NAND controller (ie stm_nand_flex or stm_nand_afm), the EMI
 *         buffer is bi-passed and the EMI address pads will remain high.
 *         Consequently, is it only possible to use the NAND controller if the
 *         decoding logic expects the address pads to be high when asserting the
 *         CS (typically the 'upper' sub-bank).
 *
 *     [2] The STEM2 interface now accepts the NAND ready/not-busy signal (eg
 *         mb837).  To support this option, while remaining backwards compatible
 *         with previous boards, we use STEM_NAND_RDY, defaulting to 0 if not
 *         defined.
 *
 *     [3] The mb588 board is a generic NAND STEM module and may be populated
 *         with any compatible NAND device.  The timing paramters specified
 *         below have been chosen to work with a large number of NAND devices.
 *         Considerable improvement in performance can be gained by tuning the
 *         parameters to the particular NAND device being used.
 *
 *     [4] Some additional host board setup may be required to use proper CS
 *         signal signal - see "arch/sh/include/mach-<board>/mach/stem.h" for
 *         more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/platform.h>
#include <linux/stm/emi.h>
#include <mach/stem.h>

#if defined(CONFIG_CPU_SUBTYPE_STX7105)
#include <linux/stm/stx7105.h>
#elif defined(CONFIG_CPU_SUBTYPE_STX7111)
#include <linux/stm/stx7111.h>
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
#include <linux/stm/stx7200.h>
#elif defined(CONFIG_CPU_SUBTYPE_STX7108)
#include <linux/stm/stx7108.h>
#else
	error Unsupported SOC.
#endif


struct stm_nand_bank_data nand_bank_data = {
	.csn			= STEM_CS0_BANK,
	.emi_withinbankoffset	= STEM_CS0_OFFSET,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.nr_partitions	= 2,
	.partitions	= (struct mtd_partition []) {
		{
			.name	= "NAND Flash S1",
			.offset	= 0,
			.size 	= 0x00800000
		}, {
			.name	= "NAND Flash S2",
			.offset = MTDPART_OFS_NXTBLK,
			.size	= MTDPART_SIZ_FULL
		},
	},
	/* Timing parameters for generic NAND device.  For improved performance,
	 * tune according to device on mb588 (see note [3]) */
	.timing_data		= &(struct stm_nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 50,
		.wr_off		= 50,
		.rd_on		= 50,
		.rd_off		= 50,
		.chip_delay	= 50,		/* in us */
	},
};

/* NAND ready/not-busy signal (see note [2] above) */
#ifndef STEM_NAND_RDY
#define STEM_NAND_RDY		0
#endif

static int __init mb588_init(void)
{
#if defined(CONFIG_CPU_SUBTYPE_STX7105)
	stx7105_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &nand_bank_data,
			.rbn.flex_connected = STEM_NAND_RDY,});
#elif defined(CONFIG_CPU_SUBTYPE_STX7111)
	stx7111_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &nand_bank_data,
			.rbn.flex_connected = STEM_NAND_RDY,});
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
	stx7200_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &nand_bank_data,
			.rbn.flex_connected = STEM_NAND_RDY,});
#elif defined(CONFIG_CPU_SUBTYPE_STX7108)
	stx7108_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &nand_bank_data,
			.rbn.flex_connected = STEM_NAND_RDY,});
#endif
	return 0;

}
arch_initcall(mb588_init);
