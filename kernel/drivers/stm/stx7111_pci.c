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

/* This function assumes you are using the dedicated pins. Production boards
 * will more likely use the external interrupt pins and save the PIOs */
int stx7111_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin)
{
	int irq;
	int pin_type;

	if ((pin > 4) || (pin < 1))
		return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch(pin_type) {
	case PCI_PIN_DEFAULT:
		irq = evt2irq(0xa00 + ((pin - 1) * 0x20));
		break;
	case PCI_PIN_ALTERNATIVE:
		/* There is no alternative here ;-) */
		BUG();
		irq = -1;
		break;
	case PCI_PIN_UNUSED:
		irq = -1; /* Not used */
		break;
	default:
		/* Take whatever interrupt you are told */
		irq = pin_type;
		break;
	}

	return irq;
}

static struct platform_device stx7111_pci_device = {
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
		/* SERR interrupt set in stx7111_configure_pci() */
		STM_PLAT_RESOURCE_IRQ_NAMED("pci serr", -1, -1),
	},
};

#define STX7111_PIO_PCI_REQ(i) stm_gpio(0, ((3 - i) * 2) + 2)
#define STX7111_PIO_PCI_GNT(i) stm_gpio(2, (3 - i) + 5)
#define STX7111_PIO_PCI_INT(i) stm_gpio(3, i == 0 ? 7 : 3 - i)
#define STX7111_PIO_PCI_SERR   stm_gpio(5, 5)

void __init stx7111_configure_pci(struct stm_plat_pci_config *pci_conf)
{
	int i;
	struct sysconf_field *sc;

	/* LLA clocks have these horrible names... */
	pci_conf->clk_name = "CLKA_PCI";

	/* Fill in the default values for the 7111 */
	if(!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;
		pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0;
		pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1;
		pci_conf->ad_max_opcode = 5;
		pci_conf->ad_posted = 1;
	}

	/* Copy over platform specific data to driver */
	stx7111_pci_device.dev.platform_data = pci_conf;

#if defined(CONFIG_PM)
#warning TODO: PCI Power Management
#endif
	/* Claim and power up the PCI cell */
	sc = sysconf_claim(SYS_CFG, 32, 2, 2, "PCI Power");
	sysconf_write(sc, 0);
	sc = sysconf_claim(SYS_STA, 15, 2, 2, "PCI Power status");
	while (sysconf_read(sc))
		cpu_relax(); /* Loop until powered up */

	/* Claim and set pads into PCI mode */
	sc = sysconf_claim(SYS_CFG, 31, 20, 20, "PCI");
	sysconf_write(sc, 1);

	/* Configure the REQ/GNT[1..3], muxed with PIOs */
	for (i = 1; i < 4; i++) {
		static const char *req_name[] = {
			"PCI REQ 0",
			"PCI REQ 1",
			"PCI REQ 2",
			"PCI REQ 3"
		};
		static const char *gnt_name[] = {
			"PCI GNT 0",
			"PCI GNT 1",
			"PCI GNT 2",
			"PCI GNT 3"
		};

		switch (pci_conf->req_gnt[i]) {
		case PCI_PIN_DEFAULT:
			if (gpio_request(STX7111_PIO_PCI_REQ(i),
					req_name[i]) == 0)
				stm_gpio_direction(STX7111_PIO_PCI_REQ(i),
					STM_GPIO_DIRECTION_IN);
			else
				printk(KERN_ERR "Unable to configure PIO for "
						"%s\n", req_name[i]);

			if (gpio_request(STX7111_PIO_PCI_GNT(i),
					gnt_name[i]) == 0)
				stm_gpio_direction(STX7111_PIO_PCI_GNT(i),
						STM_GPIO_DIRECTION_ALT_OUT);
			else
				printk(KERN_ERR "Unable to configure PIO for "
						"%s\n", gnt_name[i]);

			sc = sysconf_claim(SYS_CFG, 5, (3 - i) + 2,
					(3 - i) + 2, req_name[i]);
			sysconf_write(sc, 1);
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
	for (i = 0; i < 4; i++) {
		static const char *int_name[] = {
			"PCI INT A",
			"PCI INT B",
			"PCI INT C",
			"PCI INT D"
		};

		switch (pci_conf->pci_irq[i]) {
		case PCI_PIN_ALTERNATIVE:
			/* No alternative here... */
			BUG();
			break;
		case PCI_PIN_DEFAULT:
			if (gpio_request(STX7111_PIO_PCI_INT(i),
					int_name[i]) != 0) {
				printk(KERN_ERR "Unable to claim PIO for "
						"%s\n", int_name[i]);
				break;
			}

			stm_gpio_direction(STX7111_PIO_PCI_INT(i),
					STM_GPIO_DIRECTION_IN);

			/* Set the alternate function correctly in sysconfig */
			sc = sysconf_claim(SYS_CFG, 5, 9 + i, 9 + i,
					int_name[i]);
			sysconf_write(sc, 1);
			break;
		default:
			/* Unused or interrupt number passed, nothing to do */
			break;
		}
        }

	/* Configure the SERR interrupt (if wired up) */
	switch (pci_conf->serr_irq) {
	case PCI_PIN_DEFAULT:
		if (gpio_request(STX7111_PIO_PCI_SERR, "PCI_SERR#") == 0) {
			stm_gpio_direction(STX7111_PIO_PCI_SERR,
					STM_GPIO_DIRECTION_IN);
			pci_conf->serr_irq = gpio_to_irq(STX7111_PIO_PCI_SERR);
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
		struct resource *res = platform_get_resource_byname(
				&stx7111_pci_device, IORESOURCE_IRQ,
				"pci serr");

		BUG_ON(!res);
		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

	stx7111_pci_device.dev.parent =
		bus_find_device_by_name(&platform_bus_type, NULL, "emi");
	platform_device_register(&stx7111_pci_device);
}
