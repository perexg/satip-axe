/*
 * arch/sh/boards/mb374/setup.c
 *
 * Copyright (C) 2001 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics ST40RA/ST40STB1 Starter support.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/pci.h>

#include <asm/io.h>
#include <mach/epld.h>
#include "../../../drivers/pci/pci-st40.h"

/*
 * Initialize the board
 */
void __init mb374_setup(char **cmdline_p)
{
	unsigned char version;

	version = ctrl_inl(EPLD_REVID) & 0xff;

	printk("STMicroelectronics ST40 Starter initialisation\n");
	printk("EPLD version: %d.%02d\n",(version >> 4) & 0xf, version & 0xf);

	/* Currently all STB1 chips have problems with the sleep instruction,
	 * so disable it here.
	 */
	disable_hlt();
}

#ifdef CONFIG_PCI
int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	int irq=-1;

	pr_debug("%s: slot %d, pin %d\n", __FUNCTION__, slot, pin);

	switch(slot) {
	case 1:
		irq=0;
		break;
	case 2:
		irq=3;
		break;
	case 3:
		irq=1;
		break;
	case 4:
		irq=2;
		break;
	}

	/* Are we asking for a known slot ? */
	if(irq==-1) return -1;

	if(pin==1) return irq;

	/* if INTB/C/D h then this can only come from the PCI add in slot */
	if(slot!=1) return -1;

	/* An INTB,INTC,INTD - these are commoned up */
	switch(pin) {
	case 2:
		irq=4;
		break;
	case 3:
		irq=5;
		break;
	case 4:
		irq=7;
		break;
	default:
		irq=-1;
		break;
	}

	return irq;
}
#endif
