/*
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_WAKEUP_DEVICES__
#define __LINUX_STM_WAKEUP_DEVICES__

/*
 * the stm_wakeup_devices tracks __ONLY__ the special devices which have
 * a constraint to wakeup; i.e.:
 *
 * - irb.scd needs at least 1 MHz
 *
 * - hdmi_cec needs 100 MHz.
 *
 * - eth_phy needs 25 MHz (125 MHz in gphy)
 *
 * In some SOC these IPs share the same clock therefore a fine tuning
 * has to be done to set the clock according
 * the currently enabled wakeup devices.
 *
 * Currently the other IPs (i.e.: ASC, PIO) didn't raise any particular issues.
 */

struct stm_wakeup_devices {
	int lirc_can_wakeup:1;		/* lirc_scd_clk >= 1 MHz	*/
	int hdmi_can_wakeup:1;		/* hdmi_clk == 100 MHz		*/
	int eth_phy_can_wakeup:1;	/* eth_phy_clk ~= 25 MHz	*/
	int eth1_phy_can_wakeup:1;
};

int stm_check_wakeup_devices(struct stm_wakeup_devices *dev_wk);
#endif
