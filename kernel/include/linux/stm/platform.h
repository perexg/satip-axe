/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_PLATFORM_H
#define __LINUX_STM_PLATFORM_H

#include <linux/gpio.h>
#include <linux/lirc.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/stm/pad.h>
#include <linux/stm/nand.h>
#include <linux/stmmac.h>
#include <linux/mmc/sdhci.h>
#include <linux/sdhci-pltfm.h>
#include <linux/stm/miphy.h>

/*** Platform definition helpers ***/

#define STM_PLAT_RESOURCE_MEM(_start, _size) \
		{ \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.flags = IORESOURCE_MEM, \
		}

#define STM_PLAT_RESOURCE_MEM_NAMED(_name, _start, _size) \
		{ \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.name = (_name), \
			.flags = IORESOURCE_MEM, \
		}

#if defined(CONFIG_CPU_SUBTYPE_ST40)

#define STM_PLAT_RESOURCE_IRQ(_st40, _st200) \
		{ \
			.start = (_st40), \
			.end = (_st40), \
			.flags = IORESOURCE_IRQ, \
		}

#define STM_PLAT_RESOURCE_IRQ_NAMED(_name, _st40, _st200) \
		{ \
			.start = (_st40), \
			.end = (_st40), \
			.name = (_name), \
			.flags = IORESOURCE_IRQ, \
		}

#else

#error Unknown architecture

#endif

#define STM_PLAT_RESOURCE_DMA(_req_line) \
		{ \
			.start = (_req_line), \
			.end = (_req_line), \
			.flags = IORESOURCE_DMA, \
		}

#define STM_PLAT_RESOURCE_DMA_NAMED(_name, _req_line) \
		{ \
			.start = (_req_line), \
			.end = (_req_line), \
			.name = (_name), \
			.flags = IORESOURCE_DMA, \
		}



/*** ASC platform data ***/

struct stm_plat_asc_data {
	int hw_flow_control:1;
	int txfifo_bug:1;
	struct stm_pad_config *pad_config;
};

extern int stm_asc_console_device;
extern unsigned int stm_asc_configured_devices_num;
extern struct platform_device *stm_asc_configured_devices[];

/*** LPC platform data ***/
struct stm_plat_rtc_lpc {
	unsigned int no_hw_req:1;	/* iomem in sys/serv 5197 */
	unsigned int need_wdt_reset:1;	/* W/A on 7141 */
	unsigned char irq_edge_level;
	char *clk_id;
};

/*** SSC platform data ***/

struct stm_plat_ssc_data {
	struct stm_pad_config *pad_config;
	void (*spi_chipselect)(struct spi_device *, int);
};



/*** LiRC platform data ***/

struct stm_plat_lirc_data {
	unsigned int irbclock;		/* IRB block clock
					 * (set to 0 for auto) */
	unsigned int irbclkdiv;		/* IRB block clock divison
					 * (set to 0 for auto) */
	unsigned int irbperiodmult;	/* manual setting period multiplier */
	unsigned int irbperioddiv;	/* manual setting period divisor */
	unsigned int irbontimemult;	/* manual setting pulse period
					 * multiplier */
	unsigned int irbontimediv;	/* manual setting pulse period
					 * divisor */
	unsigned int irbrxmaxperiod;	/* maximum rx period in uS */
	unsigned int irbversion;	/* IRB version type (1,2 or 3) */
	unsigned int sysclkdiv;		/* factor to divide system bus
					   clock by */
	unsigned int rxpolarity;	/* flag to set gpio rx polarity
					 * (usually set to 1) */
	unsigned int subcarrwidth;	/* Subcarrier width in percent - this
					 * is used to make the subcarrier
					 * waveform square after passing
					 * through the 555-based threshold
					 * detector on ST boards */
	struct stm_pad_config *pads;	/* pads to be claimed */
	unsigned int rxuhfmode:1;	/* RX UHF mode enabled */
	unsigned int txenabled:1;	/* TX operation is possible */
};



/*** PWM platform data ***/

/* Private data for the PWM driver */
struct stm_plat_pwm_data {
	int channel_enabled[2];
	struct stm_pad_config *channel_pad_config[2];
};



/*** Temperature sensor data ***/

struct plat_stm_temp_data {
	struct {
		int group, num, lsb, msb;
	} dcorrect, overflow, data;
	struct stm_device_config *device_config;
	int calibrated:1;
	int calibration_value;
	void (*custom_set_dcorrect)(void *priv);
	unsigned long (*custom_get_data)(void *priv);
	void *custom_priv;
};

/*** USB platform data ***/

#define STM_PLAT_USB_FLAGS_STRAP_8BIT			(1<<0)
#define STM_PLAT_USB_FLAGS_STRAP_16BIT			(2<<0)
#define STM_PLAT_USB_FLAGS_STRAP_PLL			(1<<2)
#define STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE	(1<<3)
#define STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128	(1<<4)
#define STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256	(2<<4)

struct stm_plat_usb_data {
	unsigned long flags;
	struct stm_device_config *device_config;
};



/*** TAP platform data ***/

struct tap_sysconf_field {
	u8 group, num;
	u8 lsb, msb;
	enum {POL_NORMAL, POL_INVERTED} pol;
};

struct stm_tap_sysconf {
	struct tap_sysconf_field tms;
	struct tap_sysconf_field tck;
	struct tap_sysconf_field tdi;
	struct tap_sysconf_field tdo;
	struct tap_sysconf_field tap_en;
	struct tap_sysconf_field trstn;
	int tap_en_pol;
	int trstn_pol;
};

struct stm_plat_tap_data {
	int miphy_first, miphy_count;
	enum miphy_mode *miphy_modes;
	struct stm_tap_sysconf *tap_sysconf;
};


/*** PCIE-MP platform data ***/

struct stm_plat_pcie_mp_data {
	int miphy_first, miphy_count;
	enum miphy_mode *miphy_modes;
	void (*mp_select)(int port);
};



/*** MiPHY dummy platform data ***/

struct stm_plat_miphy_dummy_data {
	int miphy_first, miphy_count;
	enum miphy_mode *miphy_modes;
};



/*** SATA platform data ***/

struct stm_plat_sata_data {
	unsigned long phy_init;
	unsigned long pc_glue_logic_init;
	unsigned int only_32bit;
	unsigned int oob_wa;
	struct stm_device_config *device_config;
	void (*host_restart)(int port);
	int port_num;
	int miphy_num;
};

/*** PIO platform data ***/

struct stm_plat_pio_irqmux_data {
	int port_first;
	int ports_num;
};



/*** Sysconf block platform data ***/

#define PLAT_SYSCONF_GROUP(_id, _offset) \
	{ \
		.group = _id, \
		.offset = _offset, \
		.name = #_id \
	}

struct stm_plat_sysconf_group {
	int group;
	unsigned long offset;
	const char *name;
	const char *(*reg_name)(int num);
};

struct stm_plat_sysconf_data {
	int groups_num;
	struct stm_plat_sysconf_group *groups;
};



/*** NAND flash platform data ***/

struct stm_plat_nand_flex_data {
	unsigned int nr_banks;
	struct stm_nand_bank_data *banks;
	unsigned int flex_rbn_connected:1;
};

struct stm_plat_nand_emi_data {
	unsigned int nr_banks;
	struct stm_nand_bank_data *banks;
	int emi_rbn_gpio;
};

struct stm_nand_config {
	enum {
		stm_nand_emi,
		stm_nand_flex,
		stm_nand_afm
	} driver;
	int nr_banks;
	struct stm_nand_bank_data *banks;
	union {
		int emi_gpio;
		int flex_connected;
	} rbn;
};


/*** STM SPI FSM Serial Flash data ***/

struct stm_plat_spifsm_data {
	char			*name;
	struct mtd_partition	*parts;
	unsigned int		nr_parts;
	unsigned int		max_freq;
};


/*** FDMA platform data ***/

struct stm_plat_fdma_slim_regs {
	unsigned long id;
	unsigned long ver;
	unsigned long en;
	unsigned long clk_gate;
};

struct stm_plat_fdma_periph_regs {
	unsigned long sync_reg;
	unsigned long cmd_sta;
	unsigned long cmd_set;
	unsigned long cmd_clr;
	unsigned long cmd_mask;
	unsigned long int_sta;
	unsigned long int_set;
	unsigned long int_clr;
	unsigned long int_mask;
};

struct stm_plat_fdma_ram {
	unsigned long offset;
	unsigned long size;
};

struct stm_plat_fdma_hw {
	struct stm_plat_fdma_slim_regs slim_regs;
	struct stm_plat_fdma_ram dmem;
	struct stm_plat_fdma_periph_regs periph_regs;
	struct stm_plat_fdma_ram imem;
};

struct stm_plat_fdma_fw_regs {
	unsigned long rev_id;
	unsigned long cmd_statn;
	unsigned long req_ctln;
	unsigned long ptrn;
	unsigned long cntn;
	unsigned long saddrn;
	unsigned long daddrn;
};

struct stm_plat_fdma_data {
	struct stm_plat_fdma_hw *hw;
	struct stm_plat_fdma_fw_regs *fw;
};



/*** PCI platform data ***/

#define PCI_PIN_ALTERNATIVE -3 /* Use alternative PIO rather than default */
#define PCI_PIN_DEFAULT     -2 /* Use whatever the default is for that pin */
#define PCI_PIN_UNUSED	    -1 /* Pin not in use */

/* In the board setup, you can pass in the external interrupt numbers instead
 * if you have wired up your board that way. It has the advantage that the PIO
 * pins freed up can then be used for something else. */
struct stm_plat_pci_config {
	/* PCI_PIN_DEFAULT/PCI_PIN_UNUSED. Other IRQ can be passed in */
	int pci_irq[4];
	/* As above for SERR */
	int serr_irq;
	/* Lowest address line connected to an idsel  - slot 0 */
	char idsel_lo;
	/* Highest address line connected to an idsel - slot n */
	char idsel_hi;
	/* Set to PCI_PIN_DEFAULT if the corresponding req/gnt lines are
	 * in use */
	char req_gnt[4];
	/* PCI clock in Hz. If zero default to 33MHz */
	unsigned long pci_clk;

	/* If you supply a pci_reset() function, that will be used to reset
	 * the PCI bus.  Otherwise it is assumed that the reset is done via
	 * PIO, the number is specified here. Specify -EINVAL if no PIO reset
	 * is required either, for example if the PCI reset is done as part
	 * of power on reset. */
	unsigned pci_reset_gpio;
	void (*pci_reset)(void);

	/* You may define a PCI clock name. If NULL it will fall
	 * back to "pci" */
	const char *clk_name;

	/* Various PCI tuning parameters. Set by SOC layer. You don't have
	 * to specify these as the defaults are usually fine. However, if
	 * you need to change them, you can set ad_override_default and
	 * plug in your own values. */
	unsigned ad_threshold:4;
	unsigned ad_chunks_in_msg:5;
	unsigned ad_pcks_in_chunk:5;
	unsigned ad_trigger_mode:1;
	unsigned ad_posted:1;
	unsigned ad_max_opcode:4;
	unsigned ad_read_ahead:1;
	/* Set to override default values for your board */
	unsigned ad_override_default:1;

	/* Some SOCs have req0 pin connected to req3 signal to work around
	 * some problems with NAND.  These bits will be set by the chip layer,
	 * the board layer should NOT touch this.
	 */
	unsigned req0_to_req3:1;
};



/* How these are done vary considerable from SOC to SOC. Sometimes
 * they are wired up to sysconfig bits, other times they are simply
 * memory mapped registers.
 */

struct stm_plat_pcie_ops {
	void (*init)(void *handle);
	void (*enable_ltssm)(void *handle);
	void (*disable_ltssm)(void *handle);
};

/* PCIe platform data */
struct stm_plat_pcie_config {
	/* Which PIO the PERST# signal is on.
	 * If it is not connected, and you rely on the autonomous reset,
	 * then specifiy -EINVAL here
	 */
	unsigned reset_gpio;
	/* If you have a really wierd way of wanging PERST# (unlikely),
	 * then do it here. Given PCI express is defined in such a way
	 * that autonomous reset should work it is OK to not connect it at
	 * all.
	 */
	void (*reset)(void);
	/* Magic number to shove into the amba bus bridge. The AHB driver will
	 * be commoned up at some point in the future so this will change
	 */
	unsigned long ahb_val;
	/* Which miphy this pcie is using */
	int miphy_num;
	/* Magic handle to pass through to the ops */
	void *ops_handle;
	struct stm_plat_pcie_ops *ops;
};

/*** ILC platform data ***/

struct stm_plat_ilc3_data {
	unsigned short inputs_num;
	unsigned short outputs_num;
	unsigned short first_irq;

	/*
	 * The ILC supports the wakeup capability but on some chip when enabled
	 * the system is unstable during the resume from suspend, so disable
	 * it.
	 */
	int disable_wakeup:1;
};

/*** To claim Ethernet PAD resources from thr platform ***/

static inline int stmmac_claim_resource(struct platform_device *pdev)
{
	int ret = 0;
	struct plat_stmmacenet_data *plat_dat = pdev->dev.platform_data;

	if (!(devm_stm_pad_claim(&pdev->dev,
				(struct stm_pad_config *) plat_dat->custom_cfg,
				dev_name(&pdev->dev)))) {
		pr_err("%s: failed to request pads!\n", __func__);

		ret = -ENODEV;
	}

	return ret;
}

/* Mali specific */
struct stm_mali_resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
};
struct stm_mali_config {
	/* Memory allocated by Linux kernel and
	 Memory regions managed by mali driver */
	int num_mem_resources;
	struct stm_mali_resource *mem;
	/* Access to other regions of memory to directly render */
	int num_ext_resources;
	struct stm_mali_resource *ext_mem;
};

#endif /* __LINUX_STM_PLATFORM_H */
