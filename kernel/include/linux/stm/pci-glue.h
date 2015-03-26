/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: David Mckay <david.mckay@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Arch specific glue to join up the main stm pci driver in drivers/stm/
 * to the specific PCI arch code.
 *
 */

#ifndef __LINUX_STM_PCI_GLUE_H
#define __LINUX_STM_PCI_GLUE_H

#include <linux/pci.h>

/*
 * This function takes the information filled in by the arch independant code
 * and hooks it up to the arch specific stuff. The arch independant code lives
 * in drivers/stm/pcie.c or drivers/stm/pci.c
 *
 * If this function needs to allocate memory it should use devm_kzalloc()
 */

enum stm_pci_type {STM_PCI_EMISS, STM_PCI_EXPRESS};

int __devinit stm_pci_register_controller(struct platform_device *pdev,
					  struct pci_ops *config_ops,
					  enum stm_pci_type type);

/*
 * Given a pci bus, return the corresponding platform device that created it
 * in the first place. Must be called with a bus that has as it's root
 * controller an interface registered via above mechanism
 */
struct platform_device *stm_pci_bus_to_platform(struct pci_bus *bus);

/* Returns what type of bus this bus is ultimately connected to */
enum stm_pci_type stm_pci_controller_type(struct pci_bus *bus);

/* For a given pci express device, return the legacy interrupt number ie INTA.
 * Must be called on a device that has it's root bus registered via the above
 * function. Will return -EINVAL if called on a non-express bus which doesn't
 * have the concept of a legacy interrupt
 */
int stm_pci_legacy_irq(struct pci_dev *dev);


#ifdef CONFIG_STM_PCI_EMISS

/* IO functions for the EMISS PCI controller. Unfortunately
 * you have to actually run code to do an IO transaction,
 * rather than just dereference it, this complicates it
 * considerably
 */
u8 pci_emiss_inb(unsigned long port);
u16 pci_emiss_inw(unsigned long port);
u32 pci_emiss_inl(unsigned long port);

u8 pci_emiss_inb_p(unsigned long port);
u16 pci_emiss_inw_p(unsigned long port);
u32 pci_emiss_inl_p(unsigned long port);

void pci_emiss_insb(unsigned long port, void *dst, unsigned long count);
void pci_emiss_insw(unsigned long port, void *dst, unsigned long count);
void pci_emiss_insl(unsigned long port, void *dst, unsigned long count);

void pci_emiss_outb(u8 val, unsigned long port);
void pci_emiss_outw(u16 val, unsigned long port);
void pci_emiss_outl(u32 val, unsigned long port);

void pci_emiss_outb_p(u8 val, unsigned long port);
void pci_emiss_outw_p(u16 val, unsigned long port);
void pci_emiss_outl_p(u32 val, unsigned long port);

void pci_emiss_outsb(unsigned long port, const void *src,
		     unsigned long count);
void pci_emiss_outsw(unsigned long port, const void *src,
		     unsigned long count);
void pci_emiss_outsl(unsigned long port, const void *src,
		     unsigned long count);
#endif


#ifdef CONFIG_SUPERH
/*
 * Defines macros to plug in the EMISS IO functions into the
 * SH4 machine vector
 *
 * We have to hook all the in/out functions as they cannot be memory
 * mapped with the emiss PCI IP
 *
 * Also, for PCI we use the generic iomap implementation, and so do
 * not need the ioport_map function, instead using the generic cookie
 * based implementation.
 *
 */
#ifdef CONFIG_STM_PCI_EMISS

#define STM_PCI_IO_MACHINE_VEC			\
	.mv_inb = pci_emiss_inb,		\
	.mv_inw = pci_emiss_inw,		\
	.mv_inl = pci_emiss_inl,		\
	.mv_outb = pci_emiss_outb,		\
	.mv_outw = pci_emiss_outw,		\
	.mv_outl = pci_emiss_outl,		\
	.mv_inb_p = pci_emiss_inb_p,		\
	.mv_inw_p = pci_emiss_inw,		\
	.mv_inl_p = pci_emiss_inl,		\
	.mv_outb_p = pci_emiss_outb_p,		\
	.mv_outw_p = pci_emiss_outw,		\
	.mv_outl_p = pci_emiss_outl,		\
	.mv_insb = pci_emiss_insb,		\
	.mv_insw = pci_emiss_insw,		\
	.mv_insl = pci_emiss_insl,		\
	.mv_outsb = pci_emiss_outsb,		\
	.mv_outsw = pci_emiss_outsw,		\
	.mv_outsl = pci_emiss_outsl,
#else
#define STM_PCI_IO_MACHINE_VEC
#endif /* CONFIG_STM_PCI_EMISS */

#endif /* CONFIG_SUPERH */

#endif /* __LINUX_STM_PCI_GLUE_H */
