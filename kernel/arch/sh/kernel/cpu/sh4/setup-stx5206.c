/*
 * STx5206 SH-4 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx5206.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



/* SH4-only resources ----------------------------------------------------- */

static struct platform_device stx5206_sh4_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 5,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd000000, 0x900),
		/* IRL0-3 are simply connected to ILC remote outputs 1-4 */
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x240), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x2a0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x300), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x360), -1),
	},
	.dev.platform_data = &(struct stm_plat_ilc3_data) {
		.inputs_num = ILC_NR_IRQS,
		.outputs_num = 16,
		.first_irq = ILC_FIRST_IRQ,
		.disable_wakeup = 1,
	},
};

static struct platform_device *stx5206_sh4_devices[] __initdata = {
	&stx5206_sh4_ilc3_device,
};

static int __init stx5206_sh4_devices_setup(void)
{
	return platform_add_devices(stx5206_sh4_devices,
			ARRAY_SIZE(stx5206_sh4_devices));
}
core_initcall(stx5206_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */

	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, WDT, HUDI,

	PCI_DEV0,					/* Group 0 */
	PCI_DEV1, PCI_DEV2, PCI_DEV3, MMC,
	SPI_SEQ, SPI_DATA, AUX_VDP_END_PROC, AUX_VDP_FIFO_EMPTY,
		COMPO_CAP_TF, COMPO_CAP_BF,
	NAND_SEQ,
	NAND_DATA,

	PIO3, PIO2, PIO1, PIO0,				/* Group 1 */
	SSC3, SSC2, SSC1, SSC0,				/* Group 2 */
	UART3, UART2, UART1, UART0,			/* Group 3 */
	IRB_WAKEUP, IRB, PWM, MAFE,			/* Group 4 */
	PCI_ERROR, FDMA_FLAG, DAA, TTXT,		/* Group 5 */
	PCI_DMA, GMAC, TS_MERGER, SBAG_HDMI_CEC_WAKEUP,	/* Group 6 */
	LX_DELTAMU, LX_AUD, DCXO, GMAC_PMT,		/* Group 7 */
	FDMA, BDISP_CQ2, BDISP_CQ1, HDMI_CEC,		/* Group 8 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,		/* Group 9 */
	TVO_DCS0, DELMU_PP, NAND, DELMU_MBE,		/* Group 10 */
	MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,	/* Group 11 */
		MAIN_VTG, AUX_VTG,
	BDISP_AQ4, BDISP_AQ3, BDISP_AQ2, BDISP_AQ1,	/* Group 12 */
	PTI, PDES_ESA, PDES, PDES_READ_CW,		/* Group 13 */
	TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,		/* Group 14 */
		CRIPTO_SIG_CHK,
	OHCI, EHCI, TVO_DCS1, FILTER,			/* Group 15 */
	ICAM3_KTE, ICAM3, KEY_SCANNER, MES,		/* Group 16 */

	/* interrupt groups */

	GROUP0_0, GROUP0_1,
	GROUP1, GROUP2, GROUP3,
	GROUP4, GROUP5, GROUP6, GROUP7,
	GROUP8, GROUP9, GROUP10, GROUP11,
	GROUP12, GROUP13, GROUP14, GROUP15,
	GROUP16
};

static struct intc_vect stx5206_intc_vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),

	INTC_VECT(PCI_DEV0, 0xa00),
	INTC_VECT(PCI_DEV1, 0xa20), INTC_VECT(PCI_DEV2, 0xa40),
		INTC_VECT(PCI_DEV3, 0xa60), INTC_VECT(MMC, 0xa80),
	INTC_VECT(SPI_SEQ, 0xb00), INTC_VECT(SPI_DATA, 0xb20),
		INTC_VECT(AUX_VDP_END_PROC, 0xb40),
		INTC_VECT(AUX_VDP_FIFO_EMPTY, 0xb60),
		INTC_VECT(COMPO_CAP_TF, 0xb80),
		INTC_VECT(COMPO_CAP_BF, 0xba0),
	INTC_VECT(NAND_SEQ, 0xc00),
	INTC_VECT(NAND_DATA, 0xc80),

	INTC_VECT(PIO3, 0x1000), INTC_VECT(PIO2, 0x1020),
		INTC_VECT(PIO1, 0x1040), INTC_VECT(PIO0, 0x1060),
	INTC_VECT(SSC3, 0x1080), INTC_VECT(SSC2, 0x10a0),
		INTC_VECT(SSC1, 0x10c0), INTC_VECT(SSC0, 0x10e0),
	INTC_VECT(UART3, 0x1100), INTC_VECT(UART2, 0x1120),
		INTC_VECT(UART1, 0x1140), INTC_VECT(UART0, 0x1160),
	INTC_VECT(IRB_WAKEUP, 0x1180), INTC_VECT(IRB, 0x11a0),
		INTC_VECT(PWM, 0x11c0), INTC_VECT(MAFE, 0x11e0),
	INTC_VECT(PCI_ERROR, 0x1200), INTC_VECT(FDMA_FLAG, 0x1220),
		INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(PCI_DMA, 0x1280), INTC_VECT(GMAC, 0x12a0),
		INTC_VECT(TS_MERGER, 0x12c0),
		INTC_VECT(SBAG_HDMI_CEC_WAKEUP, 0x12e0),
	INTC_VECT(LX_DELTAMU, 0x1300), INTC_VECT(LX_AUD, 0x1320),
		INTC_VECT(DCXO, 0x1340), INTC_VECT(GMAC_PMT, 0x1360),
	INTC_VECT(FDMA, 0x1380), INTC_VECT(BDISP_CQ2, 0x13a0),
		INTC_VECT(BDISP_CQ1, 0x13c0), INTC_VECT(HDMI_CEC, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
		INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(TVO_DCS0, 0x1480), INTC_VECT(DELMU_PP, 0x14a0),
		INTC_VECT(NAND, 0x14c0), INTC_VECT(DELMU_MBE, 0x14e0),
	INTC_VECT(MAIN_VDP_FIFO_EMPTY, 0x1500),
		INTC_VECT(MAIN_VDP_END_PROCESSING, 0x1520),
		INTC_VECT(MAIN_VTG, 0x1540),
		INTC_VECT(AUX_VTG, 0x1560),
	INTC_VECT(BDISP_AQ4, 0x1580), INTC_VECT(BDISP_AQ3, 0x15a0),
		INTC_VECT(BDISP_AQ2, 0x15c0), INTC_VECT(BDISP_AQ1, 0x15e0),
	INTC_VECT(PTI, 0x1600), INTC_VECT(PDES_ESA, 0x1620),
		INTC_VECT(PDES, 0x1640), INTC_VECT(PDES_READ_CW, 0x1660),
	INTC_VECT(TKDMA_TKD, 0x1680), INTC_VECT(TKDMA_DMA, 0x16a0),
		INTC_VECT(CRIPTO_SIGDMA, 0x16c0),
		INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI, 0x1700), INTC_VECT(EHCI, 0x1720),
		INTC_VECT(TVO_DCS1, 0x1740), INTC_VECT(FILTER, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
		INTC_VECT(KEY_SCANNER, 0x17c0), INTC_VECT(MES, 0x17e0),
};

static struct intc_group stx5206_intc_groups[] = {
	/* PCI_DEV0 is a single bit group member */
	INTC_GROUP(GROUP0_0, PCI_DEV1, PCI_DEV2, PCI_DEV3, MMC),
	INTC_GROUP(GROUP0_1, SPI_SEQ, SPI_DATA, AUX_VDP_END_PROC,
			AUX_VDP_FIFO_EMPTY, COMPO_CAP_TF, COMPO_CAP_BF),
	/* PIO0, PIO1, PIO2 are single bit groups members */

	INTC_GROUP(GROUP1, PIO3, PIO2, PIO1, PIO0),
	INTC_GROUP(GROUP2, SSC3, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP3, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP4, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP5, PCI_ERROR, FDMA_FLAG, DAA, TTXT),
	INTC_GROUP(GROUP6, PCI_DMA, GMAC, TS_MERGER, SBAG_HDMI_CEC_WAKEUP),
	INTC_GROUP(GROUP7, LX_DELTAMU, LX_AUD, DCXO, GMAC_PMT),
	INTC_GROUP(GROUP8, FDMA, BDISP_CQ2, BDISP_CQ1, HDMI_CEC),
	INTC_GROUP(GROUP9, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP10, TVO_DCS0, DELMU_PP, NAND, DELMU_MBE),
	INTC_GROUP(GROUP11, MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,
			MAIN_VTG, AUX_VTG),
	INTC_GROUP(GROUP12, BDISP_AQ4, BDISP_AQ3, BDISP_AQ2, BDISP_AQ1),
	INTC_GROUP(GROUP13, PTI, PDES_ESA, PDES, PDES_READ_CW),
	INTC_GROUP(GROUP14, TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,
			CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP15, OHCI, EHCI, TVO_DCS1, FILTER),
	INTC_GROUP(GROUP16, ICAM3_KTE, ICAM3, KEY_SCANNER, MES)
};

static struct intc_prio_reg stx5206_intc_prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,     0 } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },

	/* offset against          31-28,    27-24,    23-20,     19-16,
	   intc2_base below        15-12,     11-8,      7-4,       3-0  */
	{ 0x00000000, 0, 32, 4,
		/* INTPRI00 */ {       0,        0, NAND_SEQ, NAND_DATA,
				    PIO0, GROUP0_1, GROUP0_0,  PCI_DEV0} },
	{ 0x00000004, 0, 32, 4,
		/* INTPRI04 */ {  GROUP8,   GROUP7,   GROUP6,    GROUP5,
				  GROUP4,   GROUP3,   GROUP2,    GROUP1 } },
	{ 0x00000008, 0, 32, 4,
		/* INTPRI08 */ { GROUP16,  GROUP15,  GROUP14,   GROUP13,
				 GROUP12,  GROUP11,  GROUP10,    GROUP9 } },
};

static struct intc_mask_reg stx5206_intc_mask_registers[] = {
	/* offsets against
	 * intc2_base below */
	{ 0x00000040, 0x00000060, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, 0, NAND_DATA, NAND_SEQ,				/* 15..12 */
	    0, COMPO_CAP_BF, COMPO_CAP_TF, AUX_VDP_FIFO_EMPTY,	/* 11...8 */
	    AUX_VDP_END_PROC, SPI_DATA, SPI_SEQ, MMC,		/*  7...4 */
	    PCI_DEV3, PCI_DEV2, PCI_DEV1, PCI_DEV0 } },		/*  3...0 */
	{ 0x00000044, 0x00000064, 32, /* INTMSK04 / INTMSKCLR04 */
	  { HDMI_CEC, BDISP_CQ1, BDISP_CQ2, FDMA,		/* 31..28 */
	    GMAC_PMT, DCXO, LX_AUD, LX_DELTAMU,			/* 27..24 */
	    SBAG_HDMI_CEC_WAKEUP, TS_MERGER, GMAC, PCI_DMA,	/* 23..20 */
	    TTXT, DAA, FDMA_FLAG, PCI_ERROR,			/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP,				/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, SSC3, 				/*  7...4 */
	    PIO0, PIO1, PIO2, PIO3 } },				/*  3...0 */
	{ 0x00000048, 0x00000068, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES, KEY_SCANNER, ICAM3, ICAM3_KTE,			/* 31..28 */
	    FILTER, TVO_DCS1, EHCI, OHCI,			/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIGDMA, TKDMA_DMA, TKDMA_TKD,/* 23..20 */
	    PDES_READ_CW, PDES, PDES_ESA, PTI,			/* 19..16 */
	    BDISP_AQ1, BDISP_AQ2, BDISP_AQ3, BDISP_AQ4,		/* 15..12 */
	    AUX_VTG, MAIN_VTG, MAIN_VDP_END_PROCESSING,		/* 11...8 */
		MAIN_VDP_FIFO_EMPTY,
	    DELMU_MBE, NAND, DELMU_PP, TVO_DCS0,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(stx5206_intc_desc, "stx5206", stx5206_intc_vectors,
		stx5206_intc_groups, stx5206_intc_mask_registers,
		stx5206_intc_prio_registers, NULL);

#define INTC_ICR	0xffd00000UL
#define INTC_ICR_IRLM   (1 << 7)

void __init plat_irq_setup(void)
{
	unsigned long intc2_base = (unsigned long)ioremap(0xfe001300, 0x100);
	int i;

	for (i = 4; i < ARRAY_SIZE(stx5206_intc_prio_registers); i++)
		stx5206_intc_prio_registers[i].set_reg += intc2_base;

	for (i = 0; i < ARRAY_SIZE(stx5206_intc_mask_registers); i++) {
		stx5206_intc_mask_registers[i].set_reg += intc2_base;
		stx5206_intc_mask_registers[i].clr_reg += intc2_base;
	}

	register_intc_controller(&stx5206_intc_desc);

	/* IRL0-3 are simply connected to ILC remote outputs 1-4 */
	/* Disable encoded interrupts */
	ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);
}
