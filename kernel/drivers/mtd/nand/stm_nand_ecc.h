/*
 * Synopsis : Error Correction Codes (ECC) Algorithms.
 *
 * Copyright (c) 2008 STMicroelectronics Limited.  All right reserved.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License Version 2.0 only.  See linux/COPYING for more information.
 *
 * An ECC is a 3-byte code which can be used to detect and correct 1-bit
 * errors (perhaps introduced by NAND flash defects) in a 128, 256, or
 * 512-byte block of data.
 *
 * Features:
 *   - Correction of any 1-bit error in the data.
 *   - Detection of any 1-bit error in the ECC and whether the data is OK.
 *   - Detection of any 2-bit error in the data.
 *
 * Limitations:
 *   - CANNOT correct 2-bit errors.
 *   - Results for 3-bit errors (or worse) are UNDEFINED.
 *
 * This algorithm is only intended for use with data corrupted by a NAND
 * flash - in which anything more than a 1-bit error is highly unlikely.
 *
 * DO NOT attempt to use it where the data may be more seriously corrupted.
 * The algorithm WILL NOT always correct serious defects and may even report
 * that the data is good.
 *
 * Usage
 * =====
 *
 * Writing flash:
 *
 *   1) Prepare the data.
 *   2) Generate an ECC for each 128, 256, or 512 bytes.
 *        stm_ecc_gen (data, ecc, ECC_256)
 *   3) Write both to flash.
 *
 * Reading flash:
 *
 *   1) Read both data and ECC from flash.
 *   2) Generate a fresh ECC for the read data.
 *        ecc_gen (read_data, new_ecc, ECC_256)
 *   3) Compare the two ECCs and correct the data, if necessary.
 *        ecc_correct (read_data, read_ecc, new_ecc, ECC_256)
 *   4) Check the return code:
 *        E_UN_CHK
 *            Data cannot be used - too badly corrupted.
 *        E_NO_CHK
 *            All is well.
 *        E_D1_CHK
 *            Data has been corrected, but the flash block contains an error.
 *        E_C1_CHK
 *            Data is OK, but the flash block contains an error in the ECC.
 *
 * If a read error occurs then you may want to consider moving the data to
 * another flash block. If the read error is in the ECC then don't forget
 * to generate a correct ECC before rewriting.
 *
 * ECC Format
 * ==========
 *
 * Basic Format:
 *
 *      		byte 0		byte 1		byte2
 *
 *      bit 0		LP0		LP8		LP16
 *      bit 1		LP1		LP9		LP17
 *      bit 2		LP2		LP10	`	CP0
 *      bit 3		LP3		LP11		CP1
 *      bit 4		LP4		LP12		CP2
 *      bit 5		LP5		LP13		CP3
 *      bit 6		LP6		LP14		CP4
 *      bit 7		LP7		LP15		CP5
 *
 *      CP = Column parity
 *      LP = Line parity
 *
 * ECC_128:
 *
 *      LP14-17 are not used. The unused bits are set to zero.
 *
 *      This format is designed to match the format used by the error
 *      correcting EMI NAND Controller. It may NOT be compatible with other
 *      128 byte ECCs.
 *
 * ECC_256:
 *
 *      LP16-17 are not used. The unused bits are set to zero.
 *
 * ECC_512:
 *
 *      All bits are used.
 *
 *      This format is NOT compatible with the 512 byte ECC used by OSPlus.
 *      (At the time of writing, this can be resolved merely by inverting each
 *      bit in the ECC, but this may not be the case in future.)
 *
 * Changelog:
 *    2009-02-25 Angus Clark <angus.clark@st.com>
 *        - Renamed, formatted, and edited for linux compatibility
 *
 */

#ifndef STM_NAND_ECC_H
#define STM_NAND_ECC_H

#include <linux/types.h>

/* The ECC algorithms support three different data sizes. */
enum ecc_size
{
  ECC_128 = 128,
  ECC_256 = 256,
  ECC_512 = 512
};

/* Return values from check function */
enum ecc_check
{
  E_UN_CHK = -1, /* uncorrectable error. */
  E_NO_CHK = 0,  /* No Errors. */
  E_D1_CHK = 1,  /* 1-bit data error. */
  E_C1_CHK = 2   /* 1-bit code error. */
};

/* Generate 3 byte ECC code for ecc_size block p_data.
   "p_data" is a pointer to the date and must be 4-byte aligned.
   "size" gives length of "p_data" - one of enum ecc_size.
 */
void stm_ecc_gen(const uint8_t *p_data, uint8_t *ecc,
		 enum ecc_size size);

/* Detect and correct a 1 bit error in a 128, 256 or 512 byte block.
   "p_data" is a pointer to the data.
   "old_ecc" is the proper ECC for the data.
   "new_ecc" is the ECC generated from the (possibly) corrupted data.
   The size of the block is given in "size".

   Returns whether the data needed correcting, or was not correctable.
   If the result code is E_D1_CHK, then the data will have been modified.
 */
enum ecc_check stm_ecc_correct(uint8_t *p_data,
			       uint8_t *old_ecc,
			       uint8_t *new_ecc,
			       enum ecc_size size);

unsigned char stm_afm_lp1617(const unsigned char *buf);

#endif /* ifndef STM_NAND_ECC_H */

