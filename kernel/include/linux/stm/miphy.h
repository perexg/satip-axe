/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_MIPHY_H
#define __LINUX_STM_MIPHY_H

enum miphy_if_type { TAP_IF, UPORT_IF, DUMMY_IF };
enum miphy_mode { UNUSED_MODE, SATA_MODE, PCIE_MODE };

struct stm_miphy;

/*
 * MiPHY API for SATA and PCIe
 */

struct stm_miphy *stm_miphy_claim(int port, enum miphy_mode mode,
	struct device *dev);
void stm_miphy_release(struct stm_miphy *miphy);

void stm_miphy_force_interface(int port, enum miphy_if_type interface);

int stm_miphy_start(struct stm_miphy *miphy);

int stm_miphy_sata_status(struct stm_miphy *miphy);

void stm_miphy_assert_deserializer(struct stm_miphy *miphy, int assert);

void stm_miphy_freeze(struct stm_miphy *miphy);
void stm_miphy_thaw(struct stm_miphy *miphy);

#endif
