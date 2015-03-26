/*
 *  --------------------------------------------------------------------------
 *
 *  ssc.h
 *  define and struct for STMicroelectronics SSC device
 *
 *  --------------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *  --------------------------------------------------------------------------
 */

#ifndef STM_SSC_H
#define STM_SSC_H 1

/* SSC Baud Rate generator */
#define SSC_BRG                  0x0
/* SSC Transmitter Buffer  */
#define SSC_TBUF                 0x4
/* SSC Receive Buffer      */
#define SSC_RBUF                 0x8
/*SSC Control              */
#define SSC_CTL                  0xC
#define SSC_CTL_DATA_WIDTH_9     0x8
#define SSC_CTL_BM               0xf
#define SSC_CTL_HB               0x10
#define SSC_CTL_PH               0x20
#define SSC_CTL_PO		 0x40
#define SSC_CTL_SR		 0x80
#define SSC_CTL_MS		 0x100
#define SSC_CTL_EN		 0x200
#define SSC_CTL_LPB		 0x400
#define SSC_CTL_EN_TX_FIFO       0x800
#define SSC_CTL_EN_RX_FIFO       0x1000
#define SSC_CTL_EN_CLST_RX       0x2000

/* SSC Interrupt Enable */
#define SSC_IEN               	0x10
#define SSC_IEN_RIEN		0x1
#define SSC_IEN_TIEN		0x2
#define SSC_IEN_TEEN		0x4
#define SSC_IEN_REEN		0x8
#define SSC_IEN_PEEN		0x10
#define SSC_IEN_AASEN		0x40
#define SSC_IEN_STOPEN		0x80
#define SSC_IEN_ARBLEN		0x100
#define SSC_IEN_NACKEN		0x400
#define SSC_IEN_REPSTRTEN	0x800
#define SSC_IEN_TX_FIFO_HALF	0x1000
#define SSC_IEN_RX_FIFO_HALF_FULL	0x4000

/* SSC Status */
#define SSC_STA                   0x14
#define SSC_STA_RIR		  0x1
#define SSC_STA_TIR		  0x2
#define SSC_STA_TE		  0x4
#define SSC_STA_RE		  0x8
#define SSC_STA_PE		 0x10
#define SSC_STA_CLST		 0x20
#define SSC_STA_AAS		 0x40
#define SSC_STA_STOP		 0x80
#define SSC_STA_ARBL		0x100
#define SSC_STA_BUSY		0x200
#define SSC_STA_NACK		0x400
#define SSC_STA_REPSTRT		0x800
#define SSC_STA_TX_FIFO_HALF	0x1000
#define SSC_STA_TX_FIFO_FULL    0x2000
#define SSC_STA_RX_FIFO_HALF    0x4000

/*SSC I2C Control */
#define SSC_I2C               	0x18
#define SSC_I2C_I2CM		0x1
#define SSC_I2C_STRTG		0x2
#define SSC_I2C_STOPG		0x4
#define SSC_I2C_ACKG		0x8
#define SSC_I2C_AD10		0x10
#define SSC_I2C_TXENB		0x20
#define SSC_I2C_REPSTRTG	0x800
#define SSC_I2C_I2CFSMODE	0x1000
/* SSC Slave Address */
#define SSC_SLAD              	0x1C
/* SSC I2C bus repeated start hold time */
#define SSC_REP_START_HOLD    	0x20
/* SSC I2C bus start hold time */
#define SSC_START_HOLD        	0x24
/* SSC I2C bus repeated start setup time */
#define SSC_REP_START_SETUP   	 0x28
/* SSC I2C bus repeated stop setup time */
#define SSC_DATA_SETUP		0x2C
/* SSC I2C bus stop setup time */
#define SSC_STOP_SETUP		0x30
/* SSC I2C bus free time */
#define SSC_BUS_FREE		0x34

/* SSC Tx FIFO Status */
#define SSC_TX_FSTAT            0x38
#define SSC_TX_FSTAT_STATUS     0x07

/* SSC Rx FIFO Status */
#define SSC_RX_FSTAT            0x3C
#define SSC_RX_FSTAT_STATUS     0x07

/* SSC Prescaler value value for clock */
#define SSC_PRE_SCALER_BRG      0x40

/* SSC Clear bit operation */
#define SSC_CLR			0x80
#define SSC_CLR_SSCAAS 		0x40
#define SSC_CLR_SSCSTOP 	0x80
#define SSC_CLR_SSCARBL 	0x100
#define SSC_CLR_NACK    	0x400
#define SSC_CLR_REPSTRT     	0x800

/* SSC Noise suppression Width */
#define SSC_NOISE_SUPP_WIDTH	0x100
/* SSC Clock Prescaler */
#define SSC_PRSCALER		0x104
#define SSC_PRSC_VALUE          0x0f

/* SSC Noise suppression Width dataout */
#define SSC_NOISE_SUPP_WIDTH_DATAOUT	0x108

/* SSC Prescaler for delay in dataout */
#define SSC_PRSCALER_DATAOUT	0x10c

#define SSC_TXFIFO_SIZE         0x8
#define SSC_RXFIFO_SIZE         0x8

/* Use the following macros to access SSC I/O memory */
#define ssc_store32(ssc, offset, value)	iowrite32(value, ssc->base+offset)
#define ssc_store16(ssc, offset, value)	iowrite16(value, ssc->base+offset)
#define ssc_store8(ssc, offset, value)	iowrite8(value, ssc->base+offset)
#define ssc_load32(ssc, offset)		ioread32(ssc->base+offset)
#define ssc_load16(ssc, offset)		ioread16(ssc->base+offset)
#define ssc_load8(ssc, offset)		XSioread8(ssc->base+offset)

#endif				/* STM_SSC_H */
