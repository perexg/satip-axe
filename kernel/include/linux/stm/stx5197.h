/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_STX5197_H
#define __LINUX_STM_STX5197_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>



/* Sysconfig groups & registers */

#define HS_CFG 			0
#define HD_CFG 			1

#define CFG_CTRL_A		HS_CFG, (0x00 / 4)
#define CFG_CTRL_B		HS_CFG, (0x04 / 4)

#define CFG_CTRL_C		HD_CFG, (0x00 / 4)
#define CFG_CTRL_D		HD_CFG, (0x04 / 4)
#define CFG_CTRL_E		HD_CFG, (0x08 / 4)
#define CFG_CTRL_F		HD_CFG, (0x0c / 4)
#define CFG_CTRL_G		HD_CFG, (0x10 / 4)
#define CFG_CTRL_H		HD_CFG, (0x14 / 4)
#define CFG_CTRL_I		HD_CFG, (0x18 / 4)
#define CFG_CTRL_J		HD_CFG, (0x1c / 4)

#define CFG_CTRL_K		HD_CFG, (0x40 / 4)
#define CFG_CTRL_L		HD_CFG, (0x44 / 4)
#define CFG_CTRL_M		HD_CFG, (0x48 / 4)
#define CFG_CTRL_N		HD_CFG, (0x4c / 4)
#define CFG_CTRL_O		HD_CFG, (0x50 / 4)
#define CFG_CTRL_P		HD_CFG, (0x54 / 4)
#define CFG_CTRL_Q		HD_CFG, (0x58 / 4)
#define CFG_CTRL_R		HD_CFG, (0x5c / 4)

#define CFG_MONITOR_A		HS_CFG, (0x08 / 4)
#define CFG_MONITOR_B		HS_CFG, (0x0c / 4)

#define CFG_MONITOR_C		HD_CFG, (0x20 / 4)
#define CFG_MONITOR_D		HD_CFG, (0x24 / 4)
#define CFG_MONITOR_E		HD_CFG, (0x28 / 4)
#define CFG_MONITOR_F		HD_CFG, (0x2c / 4)
#define CFG_MONITOR_G		HD_CFG, (0x30 / 4)
#define CFG_MONITOR_H		HD_CFG, (0x34 / 4)
#define CFG_MONITOR_I		HD_CFG, (0x38 / 4)
#define CFG_MONITOR_J		HD_CFG, (0x3c / 4)

#define CFG_MONITOR_K		HD_CFG, (0x60 / 4)
#define CFG_MONITOR_L		HD_CFG, (0x64 / 4)
#define CFG_MONITOR_M		HD_CFG, (0x68 / 4)
#define CFG_MONITOR_N		HD_CFG, (0x6c / 4)
#define CFG_MONITOR_O		HD_CFG, (0x70 / 4)
#define CFG_MONITOR_P		HD_CFG, (0x74 / 4)
#define CFG_MONITOR_Q		HD_CFG, (0x78 / 4)
#define CFG_MONITOR_R		HD_CFG, (0x7c / 4)



void stx5197_early_device_init(void);


struct stx5197_asc_config {
	int hw_flow_control;
	int is_console;
};
void stx5197_configure_asc(int asc, struct stx5197_asc_config *config);


struct stx5197_ssc_i2c_config {
	union {
		enum {
			/* SCL = PIO1.6, SDA = PIO1.7 */
			stx5197_ssc0_i2c_pio1,
			/* SCL = SPI_CLK, SDA = SPI_DATAIN */
			stx5197_ssc0_i2c_spi,

		} ssc0;
		enum {
			/* internal bus */
			stx5197_ssc1_i2c_qpsk,
			/* SCL = QAM_SCLT, SDA = QAM_SDAT */
			stx5197_ssc1_i2c_qam,
		} ssc1;
		enum {
			/* SCL = PIO3.3, SDA = PIO3.2 */
			stx5197_ssc2_i2c_pio3,
		} ssc2;
	} routing;
};
/* SSC configure functions return I2C/SPI bus number */
int stx5197_configure_ssc_i2c(int ssc, struct stx5197_ssc_i2c_config *config);
int stx5197_configure_ssc_spi(int ssc);


struct stx5197_lirc_config {
	enum {
		stx5197_lirc_rx_disabled,
		stx5197_lirc_rx_mode_ir,
		stx5197_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
};
void stx5197_configure_lirc(struct stx5197_lirc_config *config);


struct stx5197_pwm_config {
	int out0_enabled;
};
void stx5197_configure_pwm(struct stx5197_pwm_config *config);


struct stx5197_ethernet_config {
	enum {
		stx5197_ethernet_mode_mii,
		stx5197_ethernet_mode_rmii,
	} mode;
	int ext_clk;
	int phy_bus;
	int phy_addr;
	struct stmmac_mdio_bus_data *mdio_bus_data;
};
void stx5197_configure_ethernet(struct stx5197_ethernet_config *config);


void stx5197_configure_usb(void);


#endif

