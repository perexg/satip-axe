/*
 * ST40STB1/ST40RA Setup
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/io.h>
#include <asm/sci.h>

static struct resource rtc_resources[] = {
	[0] = {
		.start	= 0xffc80000,
		.end	= 0xffc80000 + 0x58 - 1,
		.flags	= IORESOURCE_IO,
	},
	[1] = {
		/* Period IRQ */
		.start	= 21,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		/* Carry IRQ */
		.start	= 22,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		/* Alarm IRQ */
		.start	= 20,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device rtc_device = {
	.name		= "sh-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rtc_resources),
	.resource	= rtc_resources,
};

static struct plat_sci_port sci_platform_data[] = {
	{
		.mapbase	= 0xffe00000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 23, 24, 26, 25 },
	}, {
		.mapbase	= 0xffe80000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 40, 41, 43, 42 },
	}, {
		.flags = 0,
	}
};

static struct platform_device sci_device = {
	.name		= "sh-sci",
	.id		= -1,
	.dev		= {
		.platform_data	= sci_platform_data,
	},
};

static struct platform_device *st40ra_devices[] __initdata = {
	&rtc_device,
	&sci_device,
};

static int __init st40ra_devices_setup(void)
{
	return platform_add_devices(st40ra_devices,
				    ARRAY_SIZE(st40ra_devices));
}
__initcall(st40ra_devices_setup);

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	HUDI,
	TMU0, TMU1, TMU2_TUNI, TMU2_TICPI,
	RTC_ATI, RTC_PRI, RTC_CUI,
	SCIF1_ERI, SCIF1_RXI, SCIF1_BRI, SCIF1_TXI,
	SCIF2_ERI, SCIF2_RXI, SCIF2_BRI, SCIF2_TXI,
	WDT,
	PCI_SERR, PCI_ERR, PCI_AD, PCI_PWR_DWN,
	DMA0, DMA1, DMA2, DMA3, DMA4, DMA_ERR,
	PIO0, PIO1, PIO2,

	/* interrupt groups */
	TMU2, RTC, SCIF1, SCIF2, PCI, DMAC, PIO,
};

static struct intc_vect vectors[] = {
	INTC_VECT(HUDI, 0x600),
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2_TUNI, 0x440), INTC_VECT(TMU2_TICPI, 0x460),
	INTC_VECT(RTC_ATI, 0x480), INTC_VECT(RTC_PRI, 0x4a0),
	INTC_VECT(RTC_CUI, 0x4c0),
	INTC_VECT(SCIF1_ERI, 0x4e0), INTC_VECT(SCIF1_RXI, 0x500),
	INTC_VECT(SCIF1_BRI, 0x520), INTC_VECT(SCIF1_TXI, 0x540),
	INTC_VECT(SCIF2_ERI, 0x700), INTC_VECT(SCIF2_RXI, 0x720),
	INTC_VECT(SCIF2_BRI, 0x740), INTC_VECT(SCIF2_TXI, 0x760),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(PCI_SERR, 0xa00), INTC_VECT(PCI_ERR, 0xa20),
	INTC_VECT(PCI_AD, 0xa40), INTC_VECT(PCI_PWR_DWN, 0xa60),
	INTC_VECT(DMA0, 0xb00), INTC_VECT(DMA1, 0xb20),
	INTC_VECT(DMA1, 0xb40), INTC_VECT(DMA2, 0xb60),
	INTC_VECT(DMA4, 0xb80), INTC_VECT(DMA_ERR, 0xbc0),
	INTC_VECT(PIO0, 0xc00), INTC_VECT(PIO1, 0xc80),
	INTC_VECT(PIO2, 0xd00),

};

static struct intc_group groups[] = {
	INTC_GROUP(TMU2, TMU2_TUNI, TMU2_TICPI),
	INTC_GROUP(RTC, RTC_ATI, RTC_PRI, RTC_CUI),
	INTC_GROUP(SCIF1, SCIF1_ERI, SCIF1_RXI, SCIF1_BRI, SCIF1_TXI),
	INTC_GROUP(SCIF2, SCIF2_ERI, SCIF2_RXI, SCIF2_BRI, SCIF2_TXI),
	INTC_GROUP(PCI, PCI_ERR, PCI_AD, PCI_PWR_DWN),
	INTC_GROUP(DMAC, DMA0, DMA1, DMA1, DMA2, DMA4, DMA_ERR),
	INTC_GROUP(PIO, PIO0, PIO1, PIO2),
};

static struct intc_prio_reg prio_registers[] = {
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,   RTC } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0, SCIF1,    0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0, SCIF2, HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
	{ 0xfe080000, 0, 32, 4, /* INTPRI00 */ {    0,    0,  PIO2, PIO1,
						 PIO0, DMAC,   PCI, PCI_SERR } },
};

static struct intc_mask_reg mask_registers[] = {
	{ 0xfe080040, 0xfe080060, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, PIO2, PIO1, PIO0,			/* 15..12 */
	    DMA_ERR, 0, DMA4, DMA3,			/* 11...8 */
	    DMA2, DMA1, DMA0, 0,			/*  7...4 */
	    PCI_PWR_DWN, PCI_AD, PCI_ERR, PCI_SERR } }	/*  3...0 */
};

static DECLARE_INTC_DESC(intc_desc, "st40ra", vectors, groups,
			 mask_registers, prio_registers, NULL);

static struct intc_vect vectors_irlm[] = {
	INTC_VECT(IRL0, 0x240), INTC_VECT(IRL1, 0x2a0),
	INTC_VECT(IRL2, 0x300), INTC_VECT(IRL3, 0x360),
};

static DECLARE_INTC_DESC(intc_desc_irlm, "st40ra_irlm", vectors_irlm, NULL,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	register_intc_controller(&intc_desc);
}

#define INTC_ICR	0xffd00000UL
#define INTC_ICR_IRLM   (1<<7)

/* enable individual interrupt mode for external interupts */
void __init plat_irq_setup_pins(int mode)
{
	switch (mode) {
	case IRQ_MODE_IRQ: /* individual interrupt mode for IRL3-0 */
		register_intc_controller(&intc_desc_irlm);
		ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);
		break;
	default:
		BUG();
	}
}
