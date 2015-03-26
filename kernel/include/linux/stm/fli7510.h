/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_FLI7510_H
#define __LINUX_STM_FLI7510_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>



/* Returns: 1 if being executed on the "HOST" ST40,
 *          0 if executed on the "RT" ST40 */
#define FLI7510_ST40HOST_CORE ((ctrl_inl(CCN_PRR) & (1 << 7)) != 0)



/* Sysconfig groups & registers */

#define PRB_PU_CFG_1			0
#define PRB_PU_CFG_2			1
#define TRS_PU_CFG_0			2 /* 7510 spec */
#define TRS_SPARE_REGS_0		2 /* 75[234] spec */
#define TRS_PU_CFG_1			3 /* 7510 spec */
#define TRS_SPARE_REGS_1		3 /* 75[234] spec */
#define VDEC_PU_CFG_0			4
#define VDEC_PU_CFG_1			5
#define VOUT_PU_CFG_1			6 /* 7510 spec */
#define VOUT_SPARE_REGS			6 /* 75[234] spec */
#define CKG_DDR				7
#define PCIE_SPARE_REGS			8

#define CFG_RESET_CTL			PRB_PU_CFG_1, (0x00 / 4)
#define CFG_BOOT_CTL			PRB_PU_CFG_1, (0x04 / 4)
#define CFG_SYS1			PRB_PU_CFG_1, (0x08 / 4)
#define CFG_MPX_CTL			PRB_PU_CFG_1, (0x0c / 4)
#define CFG_PWR_DWN_CTL			PRB_PU_CFG_1, (0x10 / 4)
#define CFG_SYS2			PRB_PU_CFG_1, (0x14 / 4)
#define CFG_MODE_PIN_STATUS		PRB_PU_CFG_1, (0x18 / 4)
#define CFG_PCI_ROPC_STATUS		PRB_PU_CFG_1, (0x1c / 4)

#define CFG_ST40_HOST_BOOT_ADDR		PRB_PU_CFG_2, (0x00 / 4)
#define CFG_ST40_CTL_BOOT_ADDR		PRB_PU_CFG_2, (0x04 / 4)
#define CFG_SYS10			PRB_PU_CFG_2, (0x08 / 4)
#define CFG_RNG_BIST_CTL		PRB_PU_CFG_2, (0x0c / 4)
#define CFG_SYS12			PRB_PU_CFG_2, (0x10 / 4)
#define CFG_SYS13			PRB_PU_CFG_2, (0x14 / 4)
#define CFG_SYS14			PRB_PU_CFG_2, (0x18 / 4)
#define CFG_EMI_ROPC_STATUS		PRB_PU_CFG_2, (0x1c / 4)

#define CFG_COMMS_CONFIG_1		TRS_SPARE_REGS_0, (0x00 / 4)
#define CFG_TRS_CONFIG			TRS_SPARE_REGS_0, (0x04 / 4)
#define CFG_COMMS_CONFIG_2		TRS_SPARE_REGS_0, (0x08 / 4)
#define CFG_USB_SOFT_JTAG		TRS_SPARE_REGS_0, (0x0c / 4)
#define CFG_TRS_SPARE_REG5_NOTUSED_0	TRS_SPARE_REGS_0, (0x10 / 4)
#define CFG_TRS_CONFIG_2		TRS_SPARE_REGS_0, (0x14 / 4)
#define CFG_COMMS_TRS_STATUS		TRS_SPARE_REGS_0, (0x18 / 4)
#define CFG_EXTRA_ID1_LSB		TRS_SPARE_REGS_0, (0x1c / 4)

#define CFG_SPARE_1			TRS_SPARE_REGS_1, (0x00 / 4)
#define CFG_SPARE_2			TRS_SPARE_REGS_1, (0x04 / 4)
#define CFG_SPARE_3			TRS_SPARE_REGS_1, (0x08 / 4)
#define CFG_TRS_SPARE_REG4_NOTUSED	TRS_SPARE_REGS_1, (0x0c / 4)
#define CFG_TRS_SPARE_REG5_NOTUSED_1	TRS_SPARE_REGS_1, (0x10 / 4)
#define CFG_TRS_SPARE_REG6_NOTUSED	TRS_SPARE_REGS_1, (0x14 / 4)
#define CFG_DEVICE_ID			TRS_SPARE_REGS_1, (0x18 / 4)
#define CFG_EXTRA_ID1_MSB		TRS_SPARE_REGS_1, (0x1c / 4)

#define CFG_TOP_SPARE_REG1		VDEC_PU_CFG_0, (0x00 / 4)
#define CFG_TOP_SPARE_REG2		VDEC_PU_CFG_0, (0x04 / 4)
#define CFG_TOP_SPARE_REG3		VDEC_PU_CFG_0, (0x08 / 4)
#define CFG_ST231_DRA2_DEBUG		VDEC_PU_CFG_0, (0x0c / 4)
#define CFG_ST231_AUD1_DEBUG		VDEC_PU_CFG_0, (0x10 / 4)
#define CFG_ST231_AUD2_DEBUG		VDEC_PU_CFG_0, (0x14 / 4)
#define CFG_REG7_0			VDEC_PU_CFG_0, (0x18 / 4)
#define CFG_INTERRUPT			VDEC_PU_CFG_0, (0x1c / 4)

#define CFG_ST231_DRA2_PERIPH_REG1	VDEC_PU_CFG_1, (0x00 / 4)
#define CFG_ST231_DRA2_BOOT_REG2	VDEC_PU_CFG_1, (0x04 / 4)
#define CFG_ST231_AUD1_PERIPH_REG3	VDEC_PU_CFG_1, (0x08 / 4)
#define CFG_ST231_AUD1_BOOT_REG4	VDEC_PU_CFG_1, (0x0c / 4)
#define CFG_ST231_AUD2_PERIPH_REG5	VDEC_PU_CFG_1, (0x10 / 4)
#define CFG_ST231_AUD2_BOOT_REG6	VDEC_PU_CFG_1, (0x14 / 4)
#define CFG_REG7_1			VDEC_PU_CFG_1, (0x18 / 4)
#define CFG_INTERRUPT_REG8		VDEC_PU_CFG_1, (0x1c / 4)

#define CFG_REG1_VOUT_PIO_ALT_SEL	VOUT_SPARE_REGS, (0x00 / 4)
#define CFG_REG2_VOUT_PIO_ALT_SEL	VOUT_SPARE_REGS, (0x04 / 4)
#define CFG_VOUT_SPARE_REG3		VOUT_SPARE_REGS, (0x08 / 4)
#define CFG_REG4_DAC_CTRL		VOUT_SPARE_REGS, (0x0c / 4)
#define CFG_REG5_VOUT_DEBUG_PAD_CTL	VOUT_SPARE_REGS, (0x10 / 4)
#define CFG_REG6_TVOUT_DEBUG_CTL	VOUT_SPARE_REGS, (0x14 / 4)
#define CFG_REG7_UNUSED			VOUT_SPARE_REGS, (0x18 / 4)

#define CKG_DDR_CTL_PLL_DDR_FREQ	CKG_DDR, (0x04 / 4)
#define CKG_DDR_STATUS_PLL_DDR		CKG_DDR, (0x0c / 4)

#define CFG_REG1_PCIE_CORE_MIPHY_INIT		PCIE_SPARE_REGS, (0x00 / 4)
#define CFG_REG2_SPARE_OUTPUT_REG		PCIE_SPARE_REGS, (0x04 / 4)
#define CFG_REG3_MIPHY_INIT			PCIE_SPARE_REGS, (0x08 / 4)
#define CFG_REG4_PCIE_CORE_LINK_CTRL		PCIE_SPARE_REGS, (0x0c / 4)
#define CFG_REG5_PCIE_SPARE_OUTPUT_REG		PCIE_SPARE_REGS, (0x10 / 4)
#define CFG_REG6_PCIE_CORE_MIPHY_PCS_CTRL	PCIE_SPARE_REGS, (0x14 / 4)
#define CFG_REG7_PCIE_CORE_PCS_MIPHY_STATUS	PCIE_SPARE_REGS, (0x18 / 4)
#define CFG_REG8_PCIE_SYS_ERR_INTERRUPT		PCIE_SPARE_REGS, (0x1c / 4)


void fli7510_early_device_init(void);


struct fli7510_asc_config {
	int hw_flow_control;
	int is_console;
};
void fli7510_configure_asc(int asc, struct fli7510_asc_config *config);


struct fli7510_ssc_spi_config {
	void (*chipselect)(struct spi_device *spi, int is_on);
};
/* SSC configure functions return I2C/SPI bus number */
int fli7510_configure_ssc_i2c(int ssc);
int fli7510_configure_ssc_spi(int ssc, struct fli7510_ssc_spi_config *config);


/* IR input only */
void fli7510_configure_lirc(void);


struct fli7510_pwm_config {
	int out0_enabled:1;
	int out1_enabled:1;
	int out2_enabled:1;
	int out3_enabled:1;
};
void fli7510_configure_pwm(struct fli7510_pwm_config *config);


struct fli7510_ethernet_config {
	enum {
		fli7510_ethernet_mode_mii,
		fli7510_ethernet_mode_gmii,
		fli7510_ethernet_mode_rmii,
		fli7510_ethernet_mode_reverse_mii,
	} mode;
	int ext_clk;
	int phy_bus;
	int phy_addr;
	struct stmmac_mdio_bus_data *mdio_bus_data;
};
void fli7510_configure_ethernet(struct fli7510_ethernet_config *config);


struct fli7510_usb_config {
	enum {
		fli7510_usb_ovrcur_disabled,
		fli7510_usb_ovrcur_active_high,
		fli7510_usb_ovrcur_active_low,
	} ovrcur_mode;
};
void fli7510_configure_usb(int port, struct fli7510_usb_config *config);

void fli7510_configure_mmc(void);

struct fli7510_audio_config {
	enum {
		fli7510_pcm_player_0_output_disabled,
		fli7510_pcm_player_0_output_2_channels,
		fli7510_pcm_player_0_output_4_channels,
		fli7510_pcm_player_0_output_6_channels,
		fli7510_pcm_player_0_output_8_channels,
	} pcm_player_0_output_mode; /* a.k.a. I2SA_OUT, a.k.a. MAIN */
	int pcm_player_1_output_enabled; /* a.k.a. I2SC_OUT, a.k.a. SEC */
	int spdif_player_output_enabled;
};
void fli7510_configure_audio(struct fli7510_audio_config *config);


void fli7510_configure_pci(struct stm_plat_pci_config *pci_config);
int  fli7510_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin);

void fli7510_configure_nand(struct stm_nand_config *config);

void fli7510_configure_spifsm(struct stm_plat_spifsm_data *data);

struct fli7540_pcie_config {
	unsigned reset_gpio;
	void (*reset)(void);
};
/* The 7540 has PCI express rather than PCI */
void fli7540_configure_pcie(struct fli7540_pcie_config *config);

#endif
