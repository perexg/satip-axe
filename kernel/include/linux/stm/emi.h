/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_EMI_H
#define __LINUX_STM_EMI_H

#define EMI_BANKS 5

struct emi_timing_data {
	int rd_cycle_time;
	int rd_oee_start;
	int rd_oee_end;
	int rd_latchpoint;
	int busreleasetime;

	int wr_cycle_time;
	int wr_oee_start;
	int wr_oee_end;

	int wait_active_low;
};

unsigned long emi_bank_base(int bank);
void emi_bank_configure(int bank, unsigned long data[4]);
void emi_config_pcmode(int bank, int pc_mode);

void emi_config_pata(int bank, int pc_mode);
void emi_config_nand(int bank, struct emi_timing_data *timing_data);
void emi_config_pci(void);

#endif
