/*
 * STx7100/STx7109 SH-4 Setup
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/stx7100.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



/* SH4-only resources ----------------------------------------------------- */

struct platform_device stx7100_sh4_wdt_device = {
	.name = "wdt",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xffc00008, 8),
	},
};

static struct platform_device stx7100_sh4_ilc3_device = {
	.name = "ilc3",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0x18000000, 0x900),
	},
};

static struct platform_device *stx7100_sh4_devices[] __initdata = {
	&stx7100_sh4_wdt_device,
	&stx7100_sh4_ilc3_device,
};

static int __init stx7100_sh4_devices_setup(void)
{
	return platform_add_devices(stx7100_sh4_devices,
			ARRAY_SIZE(stx7100_sh4_devices));
}
core_initcall(stx7100_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, RTC, SCIF, WDT, HUDI,

	SATA_DMAC, SATA_HOSTC,
	PIO0, PIO1, PIO2,
	PIO5, PIO4, PIO3, MTP,			/* Group 0 */
	SSC2, SSC1, SSC0,			/* Group 1 */
	UART3, UART2, UART1, UART0,		/* Group 2 */
	IRB_WAKEUP, IRB, PWM, MAFE,		/* Group 3 */
	DISEQC, DAA, TTXT,			/* Group 4 */
	EMPI, ETH_MAC, TS_MERGER,		/* Group 5 */
	ST231_DELTA, ST231_AUD, DCXO, PTI1,	/* Group 6 */
	FDMA_MBOX, FDMA_GP0, I2S2SPDIF, CPXM,	/* Group 7 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,	/* Group 8 */
	MPEG2, DELTA_PRE0, DELTA_PRE1, DELTA_MBE,	/* Group 9 */
	VDP_FIFO_EMPTY, VDP_END_PROC, VTG1, VTG2,	/* Group 10 */
	BDISP_AQ1, DVP, HDMI, HDCP,			/* Group 11 */
	PTI, PDES_ESA0, PDES, PRES_READ_CW,		/* Group 12 */
	SIG_CHK, TKDMA, CRIPTO_SIG_DMA, CRIPTO_SIG_CHK,	/* Group 13 */
	OHCI, EHCI, SATA, BDISP_CQ1,			/* Group 14 */
	ICAM3_KTE, ICAM3, MES_LMI_VID, MES_LMI_SYS,	/* Group 15 */

	/* interrupt groups */
	SATA_SPLIT,
	GROUP0, GROUP1, GROUP2, GROUP3,
	GROUP4, GROUP5, GROUP6, GROUP7,
	GROUP8, GROUP9, GROUP10, GROUP11,
	GROUP12, GROUP13, GROUP14, GROUP15,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(RTC, 0x480), INTC_VECT(RTC, 0x4a0), INTC_VECT(RTC, 0x4c0),
	INTC_VECT(SCIF, 0x4e0), INTC_VECT(SCIF, 0x500),
		INTC_VECT(SCIF, 0x520), INTC_VECT(SCIF, 0x540),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),

	INTC_VECT(SATA_DMAC, 0xa20), INTC_VECT(SATA_HOSTC, 0xa40),
	INTC_VECT(PIO0, 0xc00), INTC_VECT(PIO1, 0xc80), INTC_VECT(PIO2, 0xd00),
	INTC_VECT(MTP, 0x1000), INTC_VECT(PIO5, 0x1020),
	INTC_VECT(PIO4, 0x1040), INTC_VECT(PIO3, 0x1060),
	INTC_VECT(SSC2, 0x10a0),
	INTC_VECT(SSC1, 0x10c0), INTC_VECT(SSC0, 0x10e0),
	INTC_VECT(UART3, 0x1100), INTC_VECT(UART2, 0x1120),
	INTC_VECT(UART1, 0x1140), INTC_VECT(UART0, 0x1160),
	INTC_VECT(IRB_WAKEUP, 0x1180), INTC_VECT(IRB, 0x11a0),
	INTC_VECT(PWM, 0x11c0), INTC_VECT(MAFE, 0x11e0),
	INTC_VECT(DISEQC, 0x1220),
	INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(EMPI, 0x1280), INTC_VECT(ETH_MAC, 0x12a0),
	INTC_VECT(TS_MERGER, 0x12c0),
	INTC_VECT(ST231_DELTA, 0x1300), INTC_VECT(ST231_AUD, 0x1320),
	INTC_VECT(DCXO, 0x1340), INTC_VECT(PTI1, 0x1360),
	INTC_VECT(FDMA_MBOX, 0x1380), INTC_VECT(FDMA_GP0, 0x13a0),
	INTC_VECT(I2S2SPDIF, 0x13c0), INTC_VECT(CPXM, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
	INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(MPEG2, 0x1480), INTC_VECT(DELTA_PRE0, 0x14a0),
	INTC_VECT(DELTA_PRE1, 0x14c0), INTC_VECT(DELTA_MBE, 0x14e0),
	INTC_VECT(VDP_FIFO_EMPTY, 0x1500), INTC_VECT(VDP_END_PROC, 0x1520),
	INTC_VECT(VTG1, 0x1540), INTC_VECT(VTG2, 0x1560),
	INTC_VECT(BDISP_AQ1, 0x1580), INTC_VECT(DVP, 0x15a0),
	INTC_VECT(HDMI, 0x15c0), INTC_VECT(HDCP, 0x15e0),
	INTC_VECT(PTI, 0x1600), INTC_VECT(PDES_ESA0, 0x1620),
	INTC_VECT(PDES, 0x1640), INTC_VECT(PRES_READ_CW, 0x1660),
	INTC_VECT(SIG_CHK, 0x1680), INTC_VECT(TKDMA, 0x16a0),
	INTC_VECT(CRIPTO_SIG_DMA, 0x16c0), INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI, 0x1700), INTC_VECT(EHCI, 0x1720),
	INTC_VECT(SATA, 0x1740), INTC_VECT(BDISP_CQ1, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
	INTC_VECT(MES_LMI_VID, 0x17c0), INTC_VECT(MES_LMI_SYS, 0x17e0)
};

static struct intc_group groups[] = {
	INTC_GROUP(SATA_SPLIT, SATA_DMAC, SATA_HOSTC),
	INTC_GROUP(GROUP0, PIO5, PIO4, PIO3, MTP),
	INTC_GROUP(GROUP1, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP2, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP3, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP4, DISEQC, DAA, TTXT),
	INTC_GROUP(GROUP5, EMPI, ETH_MAC, TS_MERGER),
	INTC_GROUP(GROUP6, ST231_DELTA, ST231_AUD, DCXO, PTI1),
	INTC_GROUP(GROUP7, FDMA_MBOX, FDMA_GP0, I2S2SPDIF, CPXM),
	INTC_GROUP(GROUP8, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP9, MPEG2, DELTA_PRE0, DELTA_PRE1, DELTA_MBE),
	INTC_GROUP(GROUP10, VDP_FIFO_EMPTY, VDP_END_PROC, VTG1, VTG2),
	INTC_GROUP(GROUP11, BDISP_AQ1, DVP, HDMI, HDCP),
	INTC_GROUP(GROUP12, PTI, PDES_ESA0, PDES, PRES_READ_CW),
	INTC_GROUP(GROUP13, SIG_CHK, TKDMA, CRIPTO_SIG_DMA, CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP14, OHCI, EHCI, SATA, BDISP_CQ1),
	INTC_GROUP(GROUP15, ICAM3_KTE, ICAM3, MES_LMI_VID, MES_LMI_SYS),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,   RTC } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0, SCIF,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
				/* 31-28,   27-24,   23-20,   19-16 */
				/* 15-12,    11-8,     7-4,     3-0 */
	{ 0x00000300, 0, 32, 4,
		/* INTPRI00 */ {       0,       0,    PIO2,    PIO1,
				    PIO0,       0, SATA_SPLIT,    0 } },
	{ 0x00000304, 0, 32, 4,
		/* INTPRI04 */ {  GROUP7,  GROUP6,  GROUP5,  GROUP4,
				  GROUP3,  GROUP2,  GROUP1,  GROUP0 } },
	{ 0x00000308, 0, 32, 4,
		/* INTPRI08 */ { GROUP15, GROUP14, GROUP13, GROUP12,
				 GROUP11, GROUP10,  GROUP9,  GROUP8 } },
};

static struct intc_mask_reg mask_registers[] = {
	{ 0x00000340, 0x00000360, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, PIO2, PIO1, PIO0,				/* 15..12 */
	    0, 0, 0, 0,						/* 11...8 */
	    0, 0, 0, 0,						/*  7...4 */
	    0, SATA_HOSTC, SATA_DMAC, 0 } },			/*  3...0 */
	{ 0x00000344, 0x00000364, 32, /* INTMSK04 / INTMSKCLR04 */
	  { CPXM, I2S2SPDIF, FDMA_GP0, FDMA_MBOX,		/* 31..28 */
	    PTI1, DCXO, ST231_AUD, ST231_DELTA,			/* 27..24 */
	    0, TS_MERGER, ETH_MAC, EMPI,			/* 23..20 */
	    TTXT, DAA, DISEQC, 0,				/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP, 			/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, 0,				/*  7...4 */
	    PIO3, PIO4, PIO5, MTP } },				/*  3...0 */
	{ 0x00000348, 0x00000368, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES_LMI_SYS, MES_LMI_VID, ICAM3, ICAM3_KTE, 	/* 31..28 */
	    BDISP_CQ1, SATA, EHCI, OHCI,			/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIG_DMA, TKDMA, SIG_CHK,	/* 23..20 */
	    PRES_READ_CW, PDES, PDES_ESA0, PTI,			/* 19..16 */
	    HDCP, HDMI, DVP, BDISP_AQ1,				/* 15..12 */
	    VTG2, VTG1, VDP_END_PROC, VDP_FIFO_EMPTY,		/* 11...8 */
	    DELTA_MBE, DELTA_PRE1, DELTA_PRE0, MPEG2,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(intc_desc, "stx7100", vectors, groups,
			 mask_registers, prio_registers, NULL);

static struct intc_vect vectors_irlm[] = {
	INTC_VECT(IRL0, 0x240), INTC_VECT(IRL1, 0x2a0),
	INTC_VECT(IRL2, 0x300), INTC_VECT(IRL3, 0x360),
};

static DECLARE_INTC_DESC(intc_desc_irlm, "stx7100_irlm", vectors_irlm, NULL,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	struct sysconf_field *sc;
	void __iomem *intc2_base = ioremap(0x19001000, 0x400);
	int i;

	ilc_early_init(&stx7100_sh4_ilc3_device);

	for (i = 4; i <= 6; i++)
		prio_registers[i].set_reg += (unsigned long) intc2_base;
	for (i = 0; i <= 2; i++) {
		mask_registers[i].set_reg += (unsigned long) intc2_base;
		mask_registers[i].clr_reg += (unsigned long) intc2_base;
	}

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&intc_desc);
}

#define INTC_ICR	0xffd00000UL
#define INTC_ICR_IRLM   (1<<7)

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
