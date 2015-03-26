/*
 * linux/arch/sh/boards/mb360/mach.c
 *
 * Copyright (C) 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the STMicroelectronics STB1 HARP and compatible boards
 */

#include <linux/init.h>

#include <linux/pci.h>
#include <linux/irq.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/machvec.h>
#include <asm/led.h>
#include <asm/io_generic.h>
#include <mach/epld.h>
#include "../../../drivers/pci/pci-st40.h"

unsigned long stb1eval_isa_port2addr(unsigned long offset)
{
#ifdef CONFIG_PCI
        /* This is something of a hack, to avoid problems with the IDE
         * driver trying to access non-existant memory. So we only
         * return valid addresses for PCI, and redirect everything else
         * to somewhere safe.
         */
        if ((offset >= PCIBIOS_MIN_IO) &&
            (offset < ((64 * 1024) - PCIBIOS_MIN_IO + 1))) {
                return offset + ST40PCI_IO_ADDRESS;
        }
#endif

        /* However picking somewhere safe isn't as easy as you might think.
         * I used to use external ROM, but that can cause problems if you are
         * in the middle of updating Flash. So I'm now using the processor core
         * version register, which is guaranted to be available, and non-writable.
         */
        return CCN_PVR;
}

static struct sh_machine_vector mv_stb1eval __initmv = {
        .mv_nr_irqs             = NR_IRQS,
        .mv_isa_port2addr       = stb1eval_isa_port2addr,

#ifdef CONFIG_PCI
        .mv_init_irq            = harp_init_irq,
#endif
#ifdef CONFIG_HEARTBEAT
        .mv_heartbeat           = heartbeat_heart,
#endif
};
