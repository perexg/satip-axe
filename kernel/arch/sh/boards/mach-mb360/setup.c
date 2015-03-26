/*
 * arch/sh/boards/mb360/setup.c
 *
 * Copyright (C) 2001 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics ST40RA/ST40STB1 Eval support.
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
void __init platform_setup(void)
{
        unsigned board=ctrl_inl(EPLD_REVID_PLD);
        unsigned pld=ctrl_inl(EPLD_REVID_BOARD);

        printk("STMicroelectronics ST40RA/ST40STB1 Eval initialisaton\n");
        printk("Board version %c EPLD version: %d.%02d\n",
               'A'+(board&0xf), (pld >> 4) & 0xf, pld & 0xf);

        /* Currently all STB1 chips have problems with the sleep instruction,
         * so disable it here.
         */
	disable_hlt();
}

#ifdef CONFIG_PCI

int __init pcibios_map_platform_irq(u8 slot, u8 pin)
{
        int irq=-1;


        switch (slot) {
        case 2:
                irq=0;
                break;
        case 9 ... 12:
                irq = 12-slot+1;
                break;
        }

        /* Are we asking for a known slot ? */
        if(irq==-1) return -1;

	pr_debug("Asking for slot %d pin %d - given %d\n",slot,pin,irq);

        if(pin==1) return irq;
        /* An INTB,INTC,INTD - these are commoned up */
        return pin+3;
}

#endif
