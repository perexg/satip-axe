/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_STX7105_H
#define __LINUX_STM_STX7105_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>


/* Sysconfig groups */

#define SYS_DEV 0
#define SYS_STA 1
#define SYS_CFG 2


void stx7105_early_device_init(void);


struct stx7105_asc_config {
	union {
		enum { stx7105_asc0_pio0 } asc0;
		enum { stx7105_asc1_pio1 } asc1;
		enum { stx7105_asc2_pio4, stx7105_asc2_pio12 } asc2;
		enum { stx7105_asc3_pio5 } asc3;
	} routing;
	int hw_flow_control;
	int is_console;
};
void stx7105_configure_asc(int asc, struct stx7105_asc_config *config);


/* In I2C mode SCL = SCLK, SDA = MTSR,
 * in SPI mode SCK = SCLK, MOSI = MTSR, MISO = MRST */
struct stx7105_ssc_config {
	union {
		struct {
			enum {
				stx7105_ssc0_sclk_pio2_2,
			} sclk;
			enum {
				stx7105_ssc0_mtsr_pio2_3,
			} mtsr;
			enum {
				stx7105_ssc0_mrst_pio2_4,
			} mrst;
		} ssc0;
		struct {
			enum {
				stx7105_ssc1_sclk_pio2_5,
			} sclk;
			enum {
				stx7105_ssc1_mtsr_pio2_6,
			} mtsr;
			enum {
				stx7105_ssc1_mrst_pio2_7,
			} mrst;
		} ssc1;
		struct {
			enum {
				stx7105_ssc2_sclk_pio2_4, /* 7106 only! */
				stx7105_ssc2_sclk_pio3_4,
				stx7105_ssc2_sclk_pio12_0,
				stx7105_ssc2_sclk_pio13_4,
			} sclk;
			enum {
				stx7105_ssc2_mtsr_pio2_0,
				stx7105_ssc2_mtsr_pio3_5,
				stx7105_ssc2_mtsr_pio12_1,
				stx7105_ssc2_mtsr_pio13_5,
			} mtsr;
			enum {
				stx7105_ssc2_mrst_pio2_0,
				stx7105_ssc2_mrst_pio3_5,
				stx7105_ssc2_mrst_pio12_1,
				stx7105_ssc2_mrst_pio13_5,
			} mrst;
		} ssc2;
		struct {
			enum {
				stx7105_ssc3_sclk_pio2_7, /* 7106 only! */
				stx7105_ssc3_sclk_pio3_6,
				stx7105_ssc3_sclk_pio13_2,
				stx7105_ssc3_sclk_pio13_6,
			} sclk;
			enum {
				stx7105_ssc3_mtsr_pio2_1,
				stx7105_ssc3_mtsr_pio3_7,
				stx7105_ssc3_mtsr_pio13_3,
				stx7105_ssc3_mtsr_pio13_7,
			} mtsr;
			enum {
				stx7105_ssc3_mrst_pio2_1,
				stx7105_ssc3_mrst_pio3_7,
				stx7105_ssc3_mrst_pio13_3,
				stx7105_ssc3_mrst_pio13_7,
			} mrst;
		} ssc3;
	} routing;
	void (*spi_chipselect)(struct spi_device *spi, int is_on);
};
/* SSC configure functions return I2C/SPI bus number */
int stx7105_configure_ssc_i2c(int ssc, struct stx7105_ssc_config *config);
int stx7105_configure_ssc_spi(int ssc, struct stx7105_ssc_config *config);


struct stx7105_lirc_config {
	enum {
		stx7105_lirc_rx_disabled,
		stx7105_lirc_rx_mode_ir,
		stx7105_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
};
void stx7105_configure_lirc(struct stx7105_lirc_config *config);


struct stx7105_pwm_config {
	enum {
		stx7105_pwm_out0_disabled,
		stx7105_pwm_out0_pio4_4,
		stx7105_pwm_out0_pio13_0
	} out0;
	enum {
		stx7105_pwm_out1_disabled,
		stx7105_pwm_out1_pio4_5,
		stx7105_pwm_out1_pio13_1,
	} out1;
};
void stx7105_configure_pwm(struct stx7105_pwm_config *config);


struct stx7105_ethernet_config {
	enum {
		stx7105_ethernet_mode_mii,
		stx7105_ethernet_mode_gmii,
		stx7105_ethernet_mode_rgmii,
		stx7105_ethernet_mode_sgmii,
		stx7105_ethernet_mode_rmii,
		stx7105_ethernet_mode_reverse_mii,
	} mode;
	union {
		struct {
			enum {
				stx7105_ethernet_mii1_mdio_pio3_4,
				stx7105_ethernet_mii1_mdio_pio11_0
			} mdio;
			enum {
				stx7105_ethernet_mii1_mdc_pio3_5,
				stx7105_ethernet_mii1_mdc_pio11_1
			} mdc;
		} mii1;
	} routing;
	int mdint_workaround:1;
	int ext_clk:1;
	int phy_bus;
	int phy_addr;
	struct stmmac_mdio_bus_data *mdio_bus_data;
};
void stx7105_configure_ethernet(int port,
		struct stx7105_ethernet_config *config);


struct stx7105_usb_config {
	enum {
		stx7105_usb_ovrcur_disabled,
		stx7105_usb_ovrcur_active_high,
		stx7105_usb_ovrcur_active_low,
	} ovrcur_mode;
	int pwr_enabled;
	union {
		struct {
			enum {
				stx7105_usb0_ovrcur_pio4_4,
				stx7105_usb0_ovrcur_pio12_5,
				stx7105_usb0_ovrcur_pio14_4, /* 7106 only! */
			} ovrcur;
			enum {
				stx7105_usb0_pwr_pio4_5,
				stx7105_usb0_pwr_pio12_6,
				stx7105_usb0_pwr_pio14_5, /* 7106 only! */
			} pwr;
		} usb0;
		struct {
			enum {
				stx7105_usb1_ovrcur_pio4_6,
				stx7105_usb1_ovrcur_pio14_6
			} ovrcur;
			enum {
				stx7105_usb1_pwr_pio4_7,
				stx7105_usb1_pwr_pio14_7,
			} pwr;
		} usb1;
	} routing;
};
void stx7105_configure_usb(int port, struct stx7105_usb_config *config);


void stx7105_configure_sata(int port);


struct stx7105_pata_config {
	int emi_bank;
	int pc_mode;
	unsigned int irq;
};
void stx7105_configure_pata(struct stx7105_pata_config *config);


void stx7105_configure_nand(struct stm_nand_config *config);


void stx7106_configure_spifsm(struct stm_plat_spifsm_data *data);

struct stx7105_audio_config {
	enum {
		stx7105_pcm_player_0_output_disabled,
		stx7105_pcm_player_0_output_2_channels,
		stx7105_pcm_player_0_output_4_channels,
		stx7105_pcm_player_0_output_6_channels,
	} pcm_player_0_output;
	int pcm_player_1_enabled;
	int spdif_player_output_enabled;
	int pcm_reader_input_enabled;
};
void stx7105_configure_audio(struct stx7105_audio_config *config);

void stx7105_configure_pci(struct stm_plat_pci_config *pci_config);
int  stx7105_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin);

void stx7105_configure_mmc(void);

#endif

