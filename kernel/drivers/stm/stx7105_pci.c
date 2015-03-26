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
#include <linux/stm/stx7105.h>
#include <asm/irq-ilc.h>



/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when PIO pads
 * are used as interrupts or IRLx_IRQ when using external interrupts inputs */
int stx7105_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin)
{
	int irq;
	int pin_type;

	if ((pin > 4) || (pin < 1))
		return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch (pin_type) {
	case PCI_PIN_ALTERNATIVE:
		/* There is an alternative for the INTA only! */
		if (pin != 1) {
			BUG();
			irq = -1;
			break;
		}
		/* Fall through */
	case PCI_PIN_DEFAULT:
		/* There are only 3 dedicated interrupt lines! */
		if (pin == 4) {
			BUG();
			irq = -1;
			break;
		}
		irq = ILC_EXT_IRQ(pin + 25);
		break;
	case PCI_PIN_UNUSED:
		irq = -1; /* Not used */
		break;
	default:
		irq = pin_type; /* Take whatever interrupt you are told */
		break;
	}

	return irq;
}

static struct platform_device stx7105_pci_device = {
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
		STM_PLAT_RESOURCE_IRQ_NAMED("pci err", ILC_EXT_IRQ(25), -1),
		/* SERR interrupt set in stx7105_configure_pci() */
		STM_PLAT_RESOURCE_IRQ_NAMED("pci serr", -1, -1),
	},
};

static struct stm_pad_config pci_reqgnt_config[] = {
	/* REQ0/GNT0 have dedicated pins... */
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(6, 5, -1),	/* REQ1 */
			STM_PAD_PIO_OUT(7, 1, 4), 	/* GNT1 */
		},
	},
	[2] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(6, 6, -1),	/* REQ2 */
			STM_PAD_PIO_OUT(7, 2, 4), 	/* GNT2 */
		},
	},
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(6, 7, -1),	/* REQ3 */
			STM_PAD_PIO_OUT(7, 3, 4), 	/* GNT3 */
		},
	},
};

static struct stm_pad_config pci_int_config[] = {
	[0] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(6, 0, -1),	/* INTA */
		},
	},
	[1] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(6, 1, -1),	/* INTB */
		},
	},
	[2] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(6, 2, -1),	/* INTC */
		},
	},
	[3] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(6, 3, -1),	/* INTD */
		},
	},
};


#define STX7105_PIO_PCI_INTA_ALT stm_gpio(15, 3)
#define STX7105_PIO_PCI_SERR     stm_gpio(15, 4)

void __init stx7105_configure_pci(struct stm_plat_pci_config *pci_conf)
{
	struct sysconf_field *sc;
	int i;

	/* LLA clocks have these horrible names... */
	pci_conf->clk_name = "CLKA_PCI";

	/* 7105 cut 3 has req0 wired to req3 to work around NAND problems;
	 * the same story for 7106 */
	pci_conf->req0_to_req3 = (cpu_data->type == CPU_STX7106) ||
			(cpu_data->cut_major >= 3);

	/* Fill in the default values for the 7105 */
	if (!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;
		pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0;
		pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1;
		pci_conf->ad_max_opcode = 5;
		pci_conf->ad_posted = 1;
	}

	/* Copy over platform specific data to driver */
	stx7105_pci_device.dev.platform_data = pci_conf;

#if defined(CONFIG_PM)
#warning TODO: PCI Power Management
#endif
	/* Claim and power up the PCI cell */
	sc = sysconf_claim(SYS_CFG, 32, 2, 2, "PCI Power");
	sysconf_write(sc, 0); /* We will need to stash this somewhere
				 for power management. */
	sc = sysconf_claim(SYS_STA, 15, 2, 2, "PCI Power status");
	while (sysconf_read(sc))
		cpu_relax(); /* Loop until powered up */

	/* Claim and set pads into PCI mode */
	sc = sysconf_claim(SYS_CFG, 31, 20, 20, "PCI");
	sysconf_write(sc, 1);

	/* PCI_CLOCK_MASTER_NOT_SLAVE:
	 * 0: PCI clock is slave
	 * 1: PCI clock is master */
	sc = sysconf_claim(SYS_CFG, 5, 28, 28, "PCI");
	sysconf_write(sc, 1);

	/* Configure the REQ/GNT[1..3], muxed with PIOs */
	for (i = 1; i < 4; i++) {
		switch (pci_conf->req_gnt[i]) {
		case PCI_PIN_DEFAULT:
			/* REQ/GNT3 only exist pre cut 3 */
			BUG_ON(pci_conf->req0_to_req3 && i == 3);
			if (!stm_pad_claim(pci_reqgnt_config + i, "PCI")) {
				printk(KERN_ERR "Cannot claim REQ/GNT"
						"%d pads\n", i);
				BUG();
			}
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

	/* Configure interrupt PIOs */
	for (i = 0; i < 3; i++) {
		switch (pci_conf->pci_irq[i]) {
		case PCI_PIN_ALTERNATIVE:
			if (i != 0)
				BUG();
			if (gpio_request(STX7105_PIO_PCI_INTA_ALT,
						"PCI INTA") != 0) {
				printk(KERN_ERR "Unable to claim PIO for"
						"PCI INTA");
				BUG();
			}

			set_irq_type(ILC_EXT_IRQ(26), IRQ_TYPE_LEVEL_LOW);
			stm_gpio_direction(STX7105_PIO_PCI_INTA_ALT,
						STM_GPIO_DIRECTION_IN);

			/* PCI_INT0_SRC_SEL:
			 * 0: PCI_INT_FROM_DEVICE[0] is from PIO6[0]
			 * 1: PCI_INT_FROM_DEVICE[0] is from PIO15[3] */
			sc = sysconf_claim(SYS_CFG, 5, 27, 27, "PCI");
			sysconf_write(sc, 1);

			/* PCI_INT0_FROM_DEVICE:
			 * 0: Indicates disabled
			 * 1: Indicates PCI_INT_FROM_DEVICE[0] is enabled */
			sc = sysconf_claim(SYS_CFG, 5, 21, 21, "PCI");
			sysconf_write(sc, 1);
			break;
		case PCI_PIN_DEFAULT:
			if (!stm_pad_claim(pci_int_config + i, "PCI")) {
				printk(KERN_ERR "Failed to claim INT"
						"%c pad!\n", 'A' + i);
				BUG();
			}
			set_irq_type(ILC_EXT_IRQ(26 + i), IRQ_TYPE_LEVEL_LOW);

			if (i == 0) {
				/* PCI_INT0_SRC_SEL:
				 * 0: PCI_INT_FROM_DEVICE[0] is from PIO6[0]
				 * 1: PCI_INT_FROM_DEVICE[0] is from PIO15[3] */
				sc = sysconf_claim(SYS_CFG, 5, 27, 27, "PCI");
				sysconf_write(sc, 0);
			}

			/* PCI_INTn_FROM_DEVICE:
			 * 0: Indicates disabled
			 * 1: Indicates PCI_INT_FROM_DEVICE[n] is enabled */
			sc = sysconf_claim(SYS_CFG, 5, 21 - i, 21 - i, "PCI");
			sysconf_write(sc, 1);
			break;
		default:
			/* Unused or interrupt number passed, nothing to do */
			break;
		}
	}
	BUG_ON(pci_conf->pci_irq[3] != PCI_PIN_UNUSED);

	/* Configure the SERR interrupt (if wired up) */
	switch (pci_conf->serr_irq) {
	case PCI_PIN_DEFAULT:
		if (gpio_request(STX7105_PIO_PCI_SERR, "PCI_SERR#") == 0) {
			stm_gpio_direction(STX7105_PIO_PCI_SERR,
					STM_GPIO_DIRECTION_IN);
			pci_conf->serr_irq = gpio_to_irq(STX7105_PIO_PCI_SERR);
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
	if (pci_conf->serr_irq != PCI_PIN_UNUSED) {
		struct resource *res;

		res = platform_get_resource_byname(&stx7105_pci_device,
						IORESOURCE_IRQ, "pci serr");
		BUG_ON(!res);
		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

	/* LOCK is not claimed as is totally pointless, the SOCs do not
	 * support any form of coherency */

	stx7105_pci_device.dev.parent =
		bus_find_device_by_name(&platform_bus_type, NULL, "emi");
	platform_device_register(&stx7105_pci_device);
}
