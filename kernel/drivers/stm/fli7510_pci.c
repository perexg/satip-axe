/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/device.h>
#include <linux/stm/fli7510.h>
#include <linux/pci.h>
#include <asm/irq-ilc.h>



/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when PIO pads
 * are used as interrupts or IRLx_IRQ when using external interrupts inputs */
int fli7510_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin)
{
	int result;
	int type;

	BUG_ON(cpu_data->type != CPU_FLI7510);

	if (pin < 1 || pin > 4)
		return -1;

	type = pci_config->pci_irq[pin - 1];

	switch (type) {
	case PCI_PIN_ALTERNATIVE:
		/* Actually there are no alternative pins... */
		BUG();
		result = -1;
		break;
	case PCI_PIN_DEFAULT:
		/* Only INTA/INTB are described as "dedicated" PCI
		 * interrupts and even if these two are described as that,
		 * they are actually just "normal" external interrupt
		 * inputs (INT2 & INT3)... Additionally, depending
		 * on the spec version, the number below may seem wrong,
		 * but believe me - they are correct :-) */
		switch (pin) {
		case 1 ... 2:
			result = ILC_IRQ(119 - (pin - 1));
			/* Configure the ILC input to be active low,
			 * which is the PCI way... */
			set_irq_type(result, IRQ_TYPE_LEVEL_LOW);
			break;
		default:
			/* Other should be passed just as interrupt number
			 * (eg. result of the ILC_IRQ() macro) */
			BUG();
			result = -1;
			break;
		}
		break;
	case PCI_PIN_UNUSED:
		result = -1; /* Not used */
		break;
	default:
		/* Take whatever interrupt you are told */
		result = type;
		break;
	}

	return result;
}

static void fli7510_pci_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, "PCI_PWR", value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, "PCI_ACK")
			== value)
			break;
		mdelay(10);
	}

	return;
}

static struct platform_device fli7510_pci_device = {
	.name = "pci_stm",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		/* 512 MB */
		STM_PLAT_RESOURCE_MEM_NAMED("pci memory",
					    0xc0000000, 0x20000000),
		{
			.name = "pci io",
			.start = 0x0400,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		},
		STM_PLAT_RESOURCE_MEM_NAMED("pci emiss", 0xfd200000, 0x17fc),
		STM_PLAT_RESOURCE_MEM_NAMED("pci ahb", 0xfd080000, 0xff),
		STM_PLAT_RESOURCE_IRQ_NAMED("pci dma", ILC_IRQ(47), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("pci err", ILC_IRQ(48), -1),
		/* SERR interrupt set in fli7510_configure_pci() */
		STM_PLAT_RESOURCE_IRQ_NAMED("pci serr", -1, -1),
	},
	.dev.platform_data = &(struct stm_device_config){
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYSCONF(CFG_PWR_DWN_CTL, 2, 2,
				"PCI_PWR"),
			STM_DEVICE_SYSCONF(CFG_PCI_ROPC_STATUS,
				18, 18, "PCI_ACK"),
		},
		.power = fli7510_pci_power,

	}
};

#define FLI7510_PIO_PCI_REQ(i)   stm_gpio(15, (i - 1) * 2)
#define FLI7510_PIO_PCI_GNT(i)   stm_gpio(15, ((i - 1) * 2) + 1)
#define FLI7510_PIO_PCI_INT(i)   stm_gpio(25, i)
#define FLI7510_PIO_PCI_SERR     stm_gpio(16, 6)

void __init fli7510_configure_pci(struct stm_plat_pci_config *pci_conf)
{
	struct sysconf_field *sc;
	int i;

	/* PCI is available in FLI7510 only! */
	if (cpu_data->type != CPU_FLI7510) {
		BUG();
		return;
	}

	/* Fill in the default values */
	if (!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;
		pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0;
		pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1;
		pci_conf->ad_max_opcode = 5;
		pci_conf->ad_posted = 1;
	}

	/* The EMI_BUS_REQ[0] pin (also know just as EmiBusReq) is
	 * internally wired to the arbiter's PCI request 3 line.
	 * And the answer to the obvious question is: That's right,
	 * the EMI_BUSREQ[3] is not wired at all... */
	pci_conf->req0_to_req3 = 1;
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

	/* Copy over platform specific data to driver */
	fli7510_pci_device.dev.platform_data = pci_conf;

	/* REQ/GNT[1..2] PIOs setup */
	for (i = 1; i <= 2; i++) {
		switch (pci_conf->req_gnt[i]) {
		case PCI_PIN_DEFAULT:
			/* emiss_bus_req_enable */
			sc = sysconf_claim(CFG_COMMS_CONFIG_1,
					24 + (i - 1), 24 + (i - 1), "PCI");
			sysconf_write(sc, 1);
			if (gpio_request(FLI7510_PIO_PCI_REQ(i),
					"PCI_REQ") == 0)
				stm_gpio_direction(FLI7510_PIO_PCI_REQ(i),
						STM_GPIO_DIRECTION_IN);
			else
				printk(KERN_ERR "Unable to configure PIO for "
						"PCI_REQ%d\n", i);
			if (gpio_request(FLI7510_PIO_PCI_GNT(i),
					"PCI_GNT") == 0)
				stm_gpio_direction(FLI7510_PIO_PCI_GNT(i),
						STM_GPIO_DIRECTION_ALT_OUT);
			else
				printk(KERN_ERR "Unable to configure PIO for "
						"PCI_GNT%d\n", i);
			break;
		case PCI_PIN_UNUSED:
			/* Unused is unused - nothing to do */
			break;
		default:
			/* No alternative here... */
			BUG();
			break;
		}
	}

	/* Claim "dedicated" interrupt pins... */
	for (i = 0; i < 4; i++) {
		static const char *int_name[] = {
			"PCI INTA",
			"PCI INTB",
		};

		switch (pci_conf->pci_irq[i]) {
		case PCI_PIN_DEFAULT:
			if (i > 1) {
				BUG();
				break;
			}
			if (gpio_request(FLI7510_PIO_PCI_INT(i),
					int_name[i]) != 0)
				printk(KERN_ERR "Unable to claim PIO for "
						"%s\n", int_name[0]);
			break;
		case PCI_PIN_ALTERNATIVE:
			/* No alternative here... */
			BUG();
			break;
		default:
			/* Unused or interrupt number passed, nothing to do */
			break;
		}
	}

	/* Configure the SERR interrupt (if wired up) */
	switch (pci_conf->serr_irq) {
	case PCI_PIN_DEFAULT:
		if (gpio_request(FLI7510_PIO_PCI_SERR, "PCI SERR#") == 0) {
			stm_gpio_direction(FLI7510_PIO_PCI_SERR,
					STM_GPIO_DIRECTION_IN);
			pci_conf->serr_irq = gpio_to_irq(FLI7510_PIO_PCI_SERR);
			set_irq_type(pci_conf->serr_irq, IRQ_TYPE_LEVEL_LOW);
		} else {
			printk(KERN_WARNING "%s(): Failed to claim PCI SERR# "
					"PIO!\n", __func__);
			pci_conf->serr_irq = PCI_PIN_UNUSED;
		}
		break;
	case PCI_PIN_ALTERNATIVE:
		/* No alternative here */
		BUG();
		pci_conf->serr_irq = PCI_PIN_UNUSED;
		break;
	}
	if (pci_conf->serr_irq == PCI_PIN_UNUSED) {
		/* "Disable" the SERR IRQ resource (it's last on the list) */
		fli7510_pci_device.num_resources--;
	} else {
		struct resource *res = platform_get_resource_byname(
				&fli7510_pci_device, IORESOURCE_IRQ,
				"pci serr");

		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

	fli7510_pci_device.dev.parent =
		bus_find_device_by_name(&platform_bus_type, NULL, "emi");

	platform_device_register(&fli7510_pci_device);
}


static struct pcie_sysconfs {
	struct sysconf_field *reg1;
	struct sysconf_field *reg3;
	struct sysconf_field *reg4;
} fli7540_pcie_sysconfs;

/* REG1_PCIE_CORE_MIPHY_INIT */
#define MIPHY_OSC_MODE_GM               (1<<0)
#define MIPHY_OSC_MODE_MILLER           (1<<1)
#define MIPHY_PLL_REF_DIV_BY_2          (1<<2)
#define MIPHY_P1_TX_LSPD                (1<<3)
#define MIPHY_P1_RX_LSPD                (1<<4)
#define MIPHY_P1_SSC_EN                 (1<<5)
#define MIPHY_P1_NEARAFELB              (1<<6)
#define MIPHY_P1_FARAFELB               (1<<7)
#define MIPHY_PCIE_SOFT_RST_N           (1<<8)
#define MIPHY_P1_SERDES_IDDQ            (1<<9)
#define DFT_DEBUG_SEL                   (1<<10)
#define PCIE_PREFETCH_BRIDGE_EN         (1<<11)

/* REG3_MIPHY_INIT */
#define MIPHY_P1_OSC_BYPASS             (1<<0)
#define PCIE_DEVICE_TYPE_RC             (1<<1)
#define MIPHY_P1_OSC_FORCE_EXT          (1<<2)
#define MIPHY_OSC_EXT_SEL               (1<<3)

/* REG4_PCIE_CORE_LINK_CTRL */
#define PCIE_APP_LTSSM_ENABLE           (1<<0)
#define PCIE_APP_REQ_RETRY_EN           (1<<1)
#define PCIE_CFG_LANE_EN                (1<<2)
#define PCIE_DEBUG_ADD_MASK             0x7f8
#define PCIE_DEBUG_ADD(x)               (((x) << 3) & PCIE_DEBUG_ADD_MASK)
#define PCIE_DEBUG_MODE                 (1<<11)
#define PCIE_APP_INIT_RST               (1<<12)
#define PCIE_SYS_INT                    (1<<13)


static void fli7540_pcie_init(void *handle)
{
	struct pcie_sysconfs *sys = handle;
	u32 data;

	/* Force external clock to be used */
	sysconf_write(sys->reg3, MIPHY_P1_OSC_BYPASS |
				 PCIE_DEVICE_TYPE_RC);
	/* Soft reset the PCIE core. This then selects if you
	 * are in root complex or endpoint mode
	 */
	data = sysconf_read(sys->reg1);
	sysconf_write(sys->reg1, data | MIPHY_PCIE_SOFT_RST_N);
}


static void fli7540_pcie_ltssm_disable(void *handle)
{
	struct pcie_sysconfs *sys = handle;

	sysconf_write(sys->reg4, 0);
}

static void fli7540_pcie_ltssm_enable(void *handle)
{
	struct pcie_sysconfs *sys = handle;

	sysconf_write(sys->reg4, PCIE_APP_LTSSM_ENABLE);
}

/* Ops to drive the platform specific bits of the interface */
static struct stm_plat_pcie_ops fli7540_pcie_ops = {
	.init          = fli7540_pcie_init,
	.enable_ltssm  = fli7540_pcie_ltssm_enable,
	.disable_ltssm = fli7540_pcie_ltssm_disable,
};

static struct stm_plat_pcie_config fli7540_plat_pcie_config = {
	.ahb_val = 0x264207,
	.ops_handle = &fli7540_pcie_sysconfs,
	.ops = &fli7540_pcie_ops,
};

#define PCIE_MEM_START 0xc0000000
#define PCIE_MEM_SIZE  0x3c00000
#define PCIE_CNTRL_REG_SIZE 0x1000
#define PCIE_AHB_REG_SIZE 0x8
#define MSI_FIRST_IRQ 	(NR_IRQS - 33)
#define MSI_LAST_IRQ 	(NR_IRQS - 1)

static struct platform_device fli7540_pcie_device = {
	.name = "pcie_stm",
	.id = -1,
	.num_resources = 8,
	.resource = (struct resource[]) {
		/* Main PCI window, 960 MB */
		STM_PLAT_RESOURCE_MEM_NAMED("pcie memory",
					    PCIE_MEM_START, PCIE_MEM_SIZE),
		/* There actually is no IO for the PCI express cell, so this
		 * is a dummy really. Must be disjoint from PCI
		 */
		{
			.name = "pcie io",
			.start = 0x10000,
			.end = 0x1ffff,
			.flags = IORESOURCE_IO,
		},
		STM_PLAT_RESOURCE_MEM_NAMED("pcie cntrl", 0xfe100000,
					    PCIE_CNTRL_REG_SIZE),
		/* Cut 1 moves where this is, probed dynamically */
		STM_PLAT_RESOURCE_MEM_NAMED("pcie ahb", 0xfe180000,
					    PCIE_AHB_REG_SIZE),
		STM_PLAT_RESOURCE_IRQ_NAMED("pcie inta", ILC_IRQ(133), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("pcie syserr", ILC_IRQ(135), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("msi mux", ILC_IRQ(134), -1),
		{
			.start = MSI_FIRST_IRQ,
			.end  = MSI_LAST_IRQ,
			.name = "msi range",
			.flags = IORESOURCE_IRQ,
		}
	},
	.dev.platform_data = &fli7540_plat_pcie_config,
};


void __init fli7540_configure_pcie(struct fli7540_pcie_config *config)
{
	struct resource *res;

	/* Cut1 of the Freeman ultra moves where the spare regs and the ahb
	 * bridge are. We adjust them accordingly here. Strangely the spare
	 * registers appear to be replicated at the old address as well.
	 */
	if (cpu_data->cut_major >= 1) {
		res = platform_get_resource_byname(&fli7540_pcie_device,
						   IORESOURCE_MEM, "pcie ahb");
		BUG_ON(!res);
		res->start = 0xfe108000;
		res->end = res->start + PCIE_AHB_REG_SIZE - 1;
	}

	/* Claim the sysconfs in the PCIE spare block */
	fli7540_pcie_sysconfs.reg1 = sysconf_claim(
			CFG_REG1_PCIE_CORE_MIPHY_INIT, 0, 11, "pcie");
	fli7540_pcie_sysconfs.reg3 = sysconf_claim(
			CFG_REG3_MIPHY_INIT, 0, 3, "pcie");
	fli7540_pcie_sysconfs.reg4 = sysconf_claim(
			CFG_REG4_PCIE_CORE_LINK_CTRL, 0, 13, "pcie");

	fli7540_plat_pcie_config.reset_gpio = config->reset_gpio;
	fli7540_plat_pcie_config.reset = config->reset;
	/* Freeman only has one miphy */
	fli7540_plat_pcie_config.miphy_num = 0;
	platform_device_register(&fli7540_pcie_device);
}
