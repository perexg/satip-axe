/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct stm_miphy_device {
	struct mutex mutex;
	const struct miphy_if_ops *ops;
	enum miphy_if_type type;
};

struct miphy_if_ops {
	void (*reg_write)(int port, u8 addr, u8 data);
	u8 (*reg_read)(int port, u8 addr);
};

/* MiPHY register Read/Write interface registration */
int miphy_if_register(struct stm_miphy_device *miphy_dev,
		enum miphy_if_type type,
		int miphy_first, int miphy_count,
		enum miphy_mode *modes,
		struct device *parent,
		const struct miphy_if_ops *ops);

/* MiPHY register Read/Write interface un-registration */
int miphy_if_unregister(struct stm_miphy_device *dev);
