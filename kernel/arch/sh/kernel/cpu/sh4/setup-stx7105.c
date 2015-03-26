/*
 * STx7105 SH-4 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



/* SH4-only resources ----------------------------------------------------- */

static struct platform_device stx7105_sh4_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 16,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd000000, 0x900),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x200), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x220), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x240), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x260), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x280), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x2a0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x2c0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x2e0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x300), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x320), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x340), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x360), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x380), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x3a0), -1),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x3c0), -1),
	},
	.dev.platform_data = &(struct stm_plat_ilc3_data) {
		.inputs_num = ILC_NR_IRQS,
		.outputs_num = 16,
		.first_irq = ILC_FIRST_IRQ,
		.disable_wakeup = 1,
	},
};

static struct platform_device *stx7105_sh4_devices[] __initdata = {
	&stx7105_sh4_ilc3_device,
};

static int __init stx7105_sh4_devices_setup(void)
{
	return platform_add_devices(stx7105_sh4_devices,
			ARRAY_SIZE(stx7105_sh4_devices));
}
core_initcall(stx7105_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, WDT, HUDI,

	I2S2SPDIF0,					/* Group 0 */
	I2S2SPDIF1, I2S2SPDIF2, I2S2SPDIF3, SATA_DMAC,
	SATA_HOSTC, DVP, STANDALONE_PIO, AUX_VDP_END_PROC,
	AUX_VDP_FIFO_EMPTY, COMPO_CAP_BF, COMPO_CAP_TF,
	PIO0,
	PIO1,
	PIO2,

	PIO6, PIO5, PIO4, PIO3,				/* Group 1 */
	SSC3, SSC2, SSC1, SSC0,				/* Group 2 */
	UART3, UART2, UART1, UART0,			/* Group 3 */
	IRB_WAKEUP, IRB, PWM, MAFE,			/* Group 4 */
	SBAG, BDISP_AQ, DAA, TTXT,			/* Group 5 */
	EMPI_PCI, GMAC_PMT, GMAC, TS_MERGER,		/* Group 6 */
	LX_DELTAMU, LX_AUD, DCXO, PTI1,			/* Group 7 */
	FDMA0, FDMA1, OHCI1, EHCI1,			/* Group 8 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,		/* Group 9 */
	TVO_DCS0, NAND, DELMU_PP, DELMU_MBE,		/* Group 10 */
	MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,	/* Group 11 */
	MAIN_VTG, AUX_VTG,
	HDMI_CEC_WAKEUP, HDMI_CEC, HDMI, HDCP,		/* Group 12 */
	PTI0, PDES_ESA, PDES, PDES_READ_CW,		/* Group 13 */
	TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,		/* Group 14 */
	CRIPTO_SIG_CHK,
	OHCI0, EHCI0, TVO_DCS1, BDISP_CQ,		/* Group 15 */
	ICAM3_KTE, ICAM3, KEY_SCANNER, MES,		/* Group 16 */

	/* interrupt groups */
	GROUP0_0, GROUP0_1,
	GROUP1, GROUP2, GROUP3,
	GROUP4, GROUP5, GROUP6, GROUP7,
	GROUP8, GROUP9, GROUP10, GROUP11,
	GROUP12, GROUP13, GROUP14, GROUP15,
	GROUP16
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),

	INTC_VECT(I2S2SPDIF0, 0xa00),
	INTC_VECT(I2S2SPDIF1, 0xa20), INTC_VECT(I2S2SPDIF2, 0xa40),
	INTC_VECT(I2S2SPDIF3, 0xa60), INTC_VECT(SATA_DMAC, 0xa80),
	INTC_VECT(SATA_HOSTC, 0xb00), INTC_VECT(DVP, 0xb20),
	INTC_VECT(STANDALONE_PIO, 0xb40), INTC_VECT(AUX_VDP_END_PROC, 0xb60),
	INTC_VECT(AUX_VDP_FIFO_EMPTY, 0xb80), INTC_VECT(COMPO_CAP_BF, 0xba0),
	INTC_VECT(COMPO_CAP_TF, 0xbc0),
	INTC_VECT(PIO0, 0xc00),
	INTC_VECT(PIO1, 0xc80),
	INTC_VECT(PIO2, 0xd00),

	INTC_VECT(PIO6, 0x1000), INTC_VECT(PIO5, 0x1020),
	INTC_VECT(PIO4, 0x1040), INTC_VECT(PIO3, 0x1060),
	INTC_VECT(SSC3, 0x1080), INTC_VECT(SSC2, 0x10a0),
	INTC_VECT(SSC1, 0x10c0), INTC_VECT(SSC0, 0x10e0),
	INTC_VECT(UART3, 0x1100), INTC_VECT(UART2, 0x1120),
	INTC_VECT(UART1, 0x1140), INTC_VECT(UART0, 0x1160),
	INTC_VECT(IRB_WAKEUP, 0x1180), INTC_VECT(IRB, 0x11a0),
	INTC_VECT(PWM, 0x11c0), INTC_VECT(MAFE, 0x11e0),
	INTC_VECT(SBAG, 0x1200), INTC_VECT(BDISP_AQ, 0x1220),
	INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(EMPI_PCI, 0x1280), INTC_VECT(GMAC_PMT, 0x12a0),
	INTC_VECT(GMAC, 0x12c0), INTC_VECT(TS_MERGER, 0x12e0),
	INTC_VECT(LX_DELTAMU, 0x1300), INTC_VECT(LX_AUD, 0x1320),
	INTC_VECT(DCXO, 0x1340), INTC_VECT(PTI1, 0x1360),
	INTC_VECT(FDMA0, 0x1380), INTC_VECT(FDMA1, 0x13a0),
	INTC_VECT(OHCI1, 0x13c0), INTC_VECT(EHCI1, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
	INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(TVO_DCS0, 0x1480), INTC_VECT(NAND, 0x14a0),
	INTC_VECT(DELMU_PP, 0x14c0), INTC_VECT(DELMU_MBE, 0x14e0),
	INTC_VECT(MAIN_VDP_FIFO_EMPTY, 0x1500), INTC_VECT(MAIN_VDP_END_PROCESSING, 0x1520),
	INTC_VECT(MAIN_VTG, 0x1540), INTC_VECT(AUX_VTG, 0x1560),
	INTC_VECT(HDMI_CEC_WAKEUP, 0x1580), INTC_VECT(HDMI_CEC, 0x15a0),
	INTC_VECT(HDMI, 0x15c0), INTC_VECT(HDCP, 0x15e0),
	INTC_VECT(PTI0, 0x1600), INTC_VECT(PDES_ESA, 0x1620),
	INTC_VECT(PDES, 0x1640), INTC_VECT(PDES_READ_CW, 0x1660),
	INTC_VECT(TKDMA_TKD, 0x1680), INTC_VECT(TKDMA_DMA, 0x16a0),
	INTC_VECT(CRIPTO_SIGDMA, 0x16c0), INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI0, 0x1700), INTC_VECT(EHCI0, 0x1720),
	INTC_VECT(TVO_DCS1, 0x1740), INTC_VECT(BDISP_CQ, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
	INTC_VECT(KEY_SCANNER, 0x17c0), INTC_VECT(MES, 0x17e0),
};

static struct intc_group groups[] = {
	/* I2S2SPDIF0 is a single bit group */
	INTC_GROUP(GROUP0_0, I2S2SPDIF1, I2S2SPDIF2, I2S2SPDIF3, SATA_DMAC),
	INTC_GROUP(GROUP0_1, SATA_HOSTC, DVP, STANDALONE_PIO,
			AUX_VDP_END_PROC, AUX_VDP_FIFO_EMPTY,
			COMPO_CAP_BF, COMPO_CAP_TF),
	/* PIO0, PIO1, PIO2 are single bit groups */

	INTC_GROUP(GROUP1, PIO6, PIO5, PIO4, PIO3),
	INTC_GROUP(GROUP2, SSC3, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP3, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP4, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP5, SBAG, BDISP_AQ, DAA, TTXT),
	INTC_GROUP(GROUP6, EMPI_PCI, GMAC_PMT, GMAC, TS_MERGER),
	INTC_GROUP(GROUP7, LX_DELTAMU, LX_AUD, DCXO, PTI1),
	INTC_GROUP(GROUP8, FDMA0, FDMA1, OHCI1, EHCI1),
	INTC_GROUP(GROUP9, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP10, TVO_DCS0, NAND, DELMU_PP, DELMU_MBE),
	INTC_GROUP(GROUP11, MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,
		   MAIN_VTG, AUX_VTG),
	INTC_GROUP(GROUP12, HDMI_CEC_WAKEUP, HDMI_CEC, HDMI, HDCP),
	INTC_GROUP(GROUP13, PTI0, PDES_ESA, PDES, PDES_READ_CW),
	INTC_GROUP(GROUP14, TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,
		   CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP15, OHCI0, EHCI0, TVO_DCS1, BDISP_CQ),
	INTC_GROUP(GROUP16, ICAM3_KTE, ICAM3, KEY_SCANNER, MES),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,       } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },

				/* 31-28,    27-24,    23-20,      19-16, */
				/* 15-12,     11-8,      7-4,        3-0  */
	{ 0x00000300, 0, 32, 4,
		/* INTPRI00 */ {       0,        0,     PIO2,       PIO1,
				    PIO0, GROUP0_1, GROUP0_0, I2S2SPDIF0 } },
	{ 0x00000304, 0, 32, 4,
		/* INTPRI04 */ {  GROUP8,   GROUP7,   GROUP6,     GROUP5,
				  GROUP4,   GROUP3,   GROUP2,     GROUP1 } },
	{ 0x00000308, 0, 32, 4,
		/* INTPRI08 */ { GROUP16,  GROUP15,  GROUP14,    GROUP13,
				 GROUP12,  GROUP11,  GROUP10,     GROUP9 } },
};

static struct intc_mask_reg mask_registers[] = {
	{ 0x00000340, 0x00000360, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, PIO2, PIO1, PIO0,				/* 15..12 */
	    COMPO_CAP_TF, COMPO_CAP_BF, AUX_VDP_FIFO_EMPTY,	/* 11...8 */
		AUX_VDP_END_PROC,
	    STANDALONE_PIO, DVP, SATA_HOSTC, SATA_DMAC,		/*  7...4 */
	    I2S2SPDIF3, I2S2SPDIF2, I2S2SPDIF1, I2S2SPDIF0 } },	/*  3...0 */
	{ 0x00000344, 0x00000364, 32, /* INTMSK04 / INTMSKCLR04 */
	  { EHCI1, OHCI1, FDMA1, FDMA0,				/* 31..28 */
	    PTI1, DCXO, LX_AUD, LX_DELTAMU,			/* 27..24 */
	    TS_MERGER, GMAC, GMAC_PMT, EMPI_PCI,		/* 23..20 */
	    TTXT, DAA, BDISP_AQ, SBAG,				/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP,				/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, SSC3, 				/*  7...4 */
	    PIO3, PIO4, PIO5,  PIO6  } },			/*  3...0 */
	{ 0x00000348, 0x00000368, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES, KEY_SCANNER, ICAM3, ICAM3_KTE,			/* 31..28 */
	    BDISP_CQ, TVO_DCS1, EHCI0, OHCI0,			/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIGDMA, TKDMA_DMA, TKDMA_TKD,/* 23..20 */
	    PDES_READ_CW, PDES, PDES_ESA, PTI0,			/* 19..16 */
	    HDCP, HDMI, HDMI_CEC, HDMI_CEC_WAKEUP,		/* 15..12 */
	    AUX_VTG, MAIN_VTG, MAIN_VDP_END_PROCESSING,		/* 11...8 */
		 MAIN_VDP_FIFO_EMPTY,
	    DELMU_MBE, DELMU_PP, NAND, TVO_DCS0,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(intc_desc, "stx7105", vectors, groups,
			 mask_registers, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	struct sysconf_field *sc;
	unsigned long intc2_base = (unsigned long)ioremap(0xfe001000, 0x400);
	int i;

	for (i = 4; i <= 6; i++)
		prio_registers[i].set_reg += intc2_base;
	for (i = 0; i <= 2; i++) {
		mask_registers[i].set_reg += intc2_base;
		mask_registers[i].clr_reg += intc2_base;
	}

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&intc_desc);
}
