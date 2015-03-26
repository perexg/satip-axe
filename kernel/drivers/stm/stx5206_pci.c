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
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx5206.h>
#include <asm/irq-ilc.h>



/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when PIO pads
 * are used as interrupts or IRLx_IRQ when using external interrupts inputs */
int stx5206_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin)
{
	int result;
	int dedicated_irqs[] = {
		evt2irq(0xa00), /* PCI_INT_FROM_DEVICE[0] */
		evt2irq(0xa20), /* PCI_INT_FROM_DEVICE[1] */
		evt2irq(0xa40), /* PCI_INT_FROM_DEVICE[2] */
		evt2irq(0xa60), /* PCI_INT_FROM_DEVICE[3] */
	};
	int type;

	if (pin < 1 || pin > 4)
		return -1;

	type = pci_config->pci_irq[pin - 1];

	switch (type) {
	case PCI_PIN_ALTERNATIVE:
		/* Actually, there are no alternative pins */
		BUG();
		result = -1;
		break;
	case PCI_PIN_DEFAULT:
		result = dedicated_irqs[pin - 1];
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

static struct platform_device stx5206_pci_device = {
	.name = "pci_stm",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
#ifdef CONFIG_32BIT
		/* 512 MB */
		STM_PLAT_RESOURCE_MEM_NAMED("pci memory",
					    0xc0000000, 0x20000000),
#else
		/* 64 MB */
		STM_PLAT_RESOURCE_MEM_NAMED("pci memory",
					    0x08000000, 0x04000000),
#endif
		{
			.name = "pci io",
			.start = 0x0400,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		},
		STM_PLAT_RESOURCE_MEM_NAMED("pci emiss", 0xfe400000, 0x17fc),
		STM_PLAT_RESOURCE_MEM_NAMED("pci ahb", 0xfe560000, 0xff),
		STM_PLAT_RESOURCE_IRQ_NAMED("pci dma", evt2irq(0x1280), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("pci err", evt2irq(0x1200), -1),
		/* SERR interrupt set in stx5206_configure_pci() */
		STM_PLAT_RESOURCE_IRQ_NAMED("pci serr", -1, -1),
	},
};

void __init stx5206_configure_pci(struct stm_plat_pci_config *pci_conf)
{
	struct sysconf_field *sc;

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

	/* The EMI_BUSREQ[0] pin (also know just as EmiBusReq) is
	 * internally wired to the arbiter's PCI request 3 line.
	 * And the answer to the obvious question is: That's right,
	 * the EMI_BUSREQ[3] is not wired at all... */
	pci_conf->req0_to_req3 = 1;
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

	/* Copy over platform specific data to driver */
	stx5206_pci_device.dev.platform_data = pci_conf;

	/* pci_master_not_slave: Controls PCI padlogic.
	 * 1: NO DLY cells in EMI Datain/Out paths to/from pads.
	 * 0: DLY cells in EMI Datain/Out paths to/from pads */
	sc = sysconf_claim(SYS_CFG, 4, 11, 11, "PCI");
	sysconf_write(sc, 1);

	/* cfg_pci_enable: PCI select
	 * 1: PCI functionality and pad type enabled on all the shared pads
	 *    for PCI. */
	sc = sysconf_claim(SYS_CFG, 10, 30, 30, "PCI");
	sysconf_write(sc, 1);

	/* cfg_pci_int_1_from_tsout7_or_from_spi_hold: PCI_INT_FROM_DEVICE[1]
	 *    select
	 * 0: PCI_INT_FROM_DEVICE[1] is mapped on TSOUTDATA[7]
	 * 1: PCI_INT_FROM_DEVICE[1] is mapped on SPI_HOLD */
	sc = sysconf_claim(SYS_CFG, 10, 29, 29, "PCI");
	switch (pci_conf->pci_irq[1]) {
	case PCI_PIN_DEFAULT:
		sysconf_write(sc, 0); /* TSOUTDATA[7] */
		break;
	case PCI_PIN_ALTERNATIVE:
		sysconf_write(sc, 1); /* SPI_HOLD */
		break;
	default:
		/* User provided interrupt, do nothing */
		break;
	}

	/* pci_int_from_device_pci_int_to_host: Used to control the pad
	 *    direction.
	 * 0: PCI being device/sattelite (pci_int_to_host)
	 * 1: PCI being host (pci_int_from_device[0]). */
	sc = sysconf_claim(SYS_CFG, 10, 23, 23, "PCI");
	sysconf_write(sc, 1);

	/* emi_pads_mode: defines mode for emi/pci shared pads
	 * 0: bidir TTL
	 * 1: PCI */
	sc = sysconf_claim(SYS_CFG, 31, 20, 20, "PCI");
	sysconf_write(sc, 1);

	/* Configure the SERR interrupt (there is no pin dedicated
	 * as input here, have to use PIO or external interrupt) */
	switch (pci_conf->serr_irq) {
	case PCI_PIN_DEFAULT:
	case PCI_PIN_ALTERNATIVE:
		BUG();
		pci_conf->serr_irq = PCI_PIN_UNUSED;
		break;
	}
	if (pci_conf->serr_irq == PCI_PIN_UNUSED) {
		/* "Disable" the SERR IRQ resource (it's last on the list) */
		stx5206_pci_device.num_resources--;
	} else {
		struct resource *res = platform_get_resource_byname(
				&stx5206_pci_device, IORESOURCE_IRQ,
				"pci serr");

		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

#if defined(CONFIG_PM)
#warning TODO: PCI Power Managament
#endif
	/* conf_pci_pme_out: Used as enable for SPI_WRITE_PROTECT_PAD when
	 *    used for PCI.
	 * 1: Pad Input
	 * 0: Pad Output. */
	sc = sysconf_claim(SYS_CFG, 31, 22, 22, "PCI");
	sysconf_write(sc, 1);

	/* pci_power_down_req: power down request for PCI module */
	sc = sysconf_claim(SYS_CFG, 32, 2, 2, "PCI");
	sysconf_write(sc, 0);

	/* power_down_ack_pci: PCI power down acknowledge */
	sc = sysconf_claim(SYS_STA, 15, 2, 2, "PCI");
	while (sysconf_read(sc))
		cpu_relax();

	stx5206_pci_device.dev.parent =
		bus_find_device_by_name(&platform_bus_type, NULL, "emi");
	platform_device_register(&stx5206_pci_device);
}

