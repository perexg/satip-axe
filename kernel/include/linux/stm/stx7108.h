/*
 * (c) 2010-2011 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_STX7108_H
#define __LINUX_STM_STX7108_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>


/* Returns: 1 if being executed on the "HOST" (L2-cached) ST40,
 *          0 if executed on the "RT" ST40 */
#define STX7108_HOST_CORE ((ctrl_inl(CCN_PRR) & (1 << 7)) == 0)


/* Sysconfig groups */

#define SYS_STA_BANK0		0
#define SYS_CFG_BANK0		1
#define SYS_STA_BANK1		2
#define SYS_CFG_BANK1		3
#define SYS_STA_BANK2		4
#define SYS_CFG_BANK2		5
#define SYS_STA_BANK3		6
#define SYS_CFG_BANK3		7
#define SYS_STA_BANK4		8
#define SYS_CFG_BANK4		9


struct stx7108_pio_mode_config {
	int oe:1;
	int pu:1;
	int od:1;
};

/* Structure aligned to the "STi7108 Generic Retime Padlogic
 * Application Note" SPEC */
struct stx7108_pio_retime_config {
	int retime:2;
	int clk1notclk0:2;
	int clknotdata:2;
	int double_edge:2;
	int invertclk:2;
	int delay_input:2;
};

struct stx7108_pio_config {
	struct stx7108_pio_mode_config *mode;
	struct stx7108_pio_retime_config *retime;
};


void stx7108_early_device_init(void);


struct stx7108_asc_config {
	union {
		struct {
			enum {
				stx7108_asc3_txd_pio21_0,
				stx7108_asc3_txd_pio24_4,
			} txd;
			enum {
				stx7108_asc3_rxd_pio21_1,
				stx7108_asc3_rxd_pio24_5,
			} rxd;
			enum {
				stx7108_asc3_cts_pio21_4,
				stx7108_asc3_cts_pio25_0,
			} cts;
			enum {
				stx7108_asc3_rts_pio21_3,
				stx7108_asc3_rts_pio24_7,
			} rts;
		} asc3;
	} routing;
	int hw_flow_control:1;
	int is_console:1;
};
void stx7108_configure_asc(int asc, struct stx7108_asc_config *config);


struct stx7108_ssc_config {
	union {
		struct {
			enum {
				stx7108_ssc2_sclk_pio1_3,
				stx7108_ssc2_sclk_pio14_4
			} sclk;
			enum {
				stx7108_ssc2_mtsr_pio1_4,
				stx7108_ssc2_mtsr_pio14_5
			} mtsr;
			enum {
				stx7108_ssc2_mrst_pio1_5,
				stx7108_ssc2_mrst_pio14_6
			} mrst;
		} ssc2;
	} routing;
	void (*spi_chipselect)(struct spi_device *spi, int is_on);
};
int stx7108_configure_ssc_i2c(int ssc, struct stx7108_ssc_config *config);
int stx7108_configure_ssc_spi(int ssc, struct stx7108_ssc_config *config);


struct stx7108_lirc_config {
	enum {
		stx7108_lirc_rx_disabled,
		stx7108_lirc_rx_mode_ir,
		stx7108_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
};
void stx7108_configure_lirc(struct stx7108_lirc_config *config);


struct stx7108_pwm_config {
	int out0_enabled;
	int out1_enabled;
};
void stx7108_configure_pwm(struct stx7108_pwm_config *config);


struct stx7108_ethernet_config {
	enum {
		stx7108_ethernet_mode_mii,
		stx7108_ethernet_mode_gmii,
		stx7108_ethernet_mode_gmii_gtx,
		stx7108_ethernet_mode_rmii,
		stx7108_ethernet_mode_rgmii_gtx,
		stx7108_ethernet_mode_reverse_mii
	} mode;
	int ext_clk;
	int phy_bus;
	int phy_addr;
	void (*txclk_select)(int txclk_250_not_25_mhz);
	struct stmmac_mdio_bus_data *mdio_bus_data;
};
void stx7108_configure_ethernet(int port,
		struct stx7108_ethernet_config *config);


void stx7108_configure_usb(int port);

void stx7108_configure_mmc(int emmc);

struct stx7108_miphy_config {
	int force_jtag;		/* Option available for CUT2.0 */
	enum miphy_mode *modes;
};
void stx7108_configure_miphy(struct stx7108_miphy_config *config);

struct stx7108_sata_config {
};
void stx7108_configure_sata(int port, struct stx7108_sata_config *config);


struct stx7108_pata_config {
	int emi_bank;
	int pc_mode;
	unsigned int irq;
};
void stx7108_configure_pata(struct stx7108_pata_config *config);


struct stx7108_audio_config {
	enum {
		stx7108_pcm_player_2_output_disabled,
		stx7108_pcm_player_2_output_2_channels,
		stx7108_pcm_player_2_output_4_channels,
		stx7108_pcm_player_2_output_6_channels,
		stx7108_pcm_player_2_output_8_channels,
	} pcm_player_2_output;
	int spdif_player_output_enabled;
	int pcm_reader_input_enabled;
};
void stx7108_configure_audio(struct stx7108_audio_config *config);


void stx7108_configure_pci(struct stm_plat_pci_config *pci_config);
int  stx7108_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin);

void stx7108_configure_nand(struct stm_nand_config *config);

struct stx7108_pcie_config {
	unsigned reset_gpio; /* Which (if any) gpio for PCIe reset */
	void (*reset)(void); /* Do something else on reset if needed */
};

void stx7108_configure_pcie(struct stx7108_pcie_config *config);

void stx7108_configure_mali(struct stm_mali_config *config);

void stx7108_configure_spifsm(struct stm_plat_spifsm_data *data);

#endif
