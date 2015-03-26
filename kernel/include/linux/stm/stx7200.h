/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_STX7200_H
#define __LINUX_STM_STX7200_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>


/* Sysconfig groups */

#define SYS_DEV 0
#define SYS_STA 1
#define SYS_CFG 2


void stx7200_early_device_init(void);


struct stx7200_asc_config {
	int hw_flow_control;
	int is_console;
};
void stx7200_configure_asc(int asc, struct stx7200_asc_config *config);


struct stx7200_ssc_spi_config {
	void (*chipselect)(struct spi_device *spi, int is_on);
};
/* SSC configure functions return I2C/SPI bus number */
int stx7200_configure_ssc_i2c(int ssc);
int stx7200_configure_ssc_spi(int ssc, struct stx7200_ssc_spi_config *config);


struct stx7200_lirc_config {
	enum {
		stx7200_lirc_rx_disabled,
		stx7200_lirc_rx_mode_ir,
		stx7200_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
};
void stx7200_configure_lirc(struct stx7200_lirc_config *config);


struct stx7200_pwm_config {
	int out0_enabled;
	int out1_enabled;
};
void stx7200_configure_pwm(struct stx7200_pwm_config *config);


struct stx7200_ethernet_config {
	enum {
		stx7200_ethernet_mode_mii,
		stx7200_ethernet_mode_rmii,
	} mode;
	int ext_clk;
	int phy_bus;
	int phy_addr;
	struct stmmac_mdio_bus_data *mdio_bus_data;
};
void stx7200_configure_ethernet(int port,
		struct stx7200_ethernet_config *config);


void stx7200_configure_usb(int port);


void stx7200_configure_sata(void);


struct stx7200_pata_config {
	int emi_bank;
	int pc_mode;
	unsigned int irq;
};
void stx7200_configure_pata(struct stx7200_pata_config *config);

void stx7200_configure_nand(struct stm_nand_config *config);

struct stx7200_audio_config {
	enum {
		stx7200_pcm_player_1_disabled,
		stx7200_pcm_player_1_auddig0,
		stx7200_pcm_player_1_mii1
	} pcm_player_1_routing;
	enum {
		stx7200_pcm_player_2_disabled,
		stx7200_pcm_player_2_auddig1,
		stx7200_pcm_player_2_mii0
	} pcm_player_2_routing;
	enum {
		stx7200_pcm_player_3_disabled,
		stx7200_pcm_player_3_aiddig0_auddig1,
	} pcm_player_3_routing;

};
void stx7200_configure_audio(struct stx7200_audio_config *config);


#endif
