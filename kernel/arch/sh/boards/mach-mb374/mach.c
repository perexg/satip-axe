/*
 * linux/arch/sh/boards/mb374/mach.c
 *
 * Copyright (C) 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the STMicroelectronics STB40RA Starter board.
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <mach/epld.h>
#include "../../../drivers/pci/pci-st40.h"

static void __iomem *mb374_ioport_map(unsigned long port, unsigned int size)
{
#ifdef CONFIG_PCI
        /* This is something of a hack, to avoid problems with the IDE
         * driver trying to access non-existant memory. So we only
         * return valid addresses for PCI, and redirect everything else
         * to somewhere safe.
         */
        if ((port >= PCIBIOS_MIN_IO) &&
            (port < ((64 * 1024) - PCIBIOS_MIN_IO + 1))) {
                return port + ST40PCI_IO_ADDRESS;
        }
#endif

        /* However picking somewhere safe isn't as easy as you might think.
         * I used to use external ROM, but that can cause problems if you are
         * in the middle of updating Flash. So I'm now using the processor core
         * version register, which is guaranted to be available, and non-writable.
         */
        return (void __iomem *)CCN_PVR;
}

void __init mb374_setup(char **cmdline_p);

static struct sh_machine_vector mv_mb374 __initmv = {
	.mv_name		= "ST40RA/ST40STB1 Starter",
	.mv_setup		= mb374_setup,
        .mv_nr_irqs             = NR_IRQS,
        .mv_ioport_map		= mb374_ioport_map,

#ifdef CONFIG_PCI
        .mv_init_irq            = harp_init_irq,
#endif
};
