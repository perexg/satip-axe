/*
 * arch/sh/boards/st/common/mb705-epld.h
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Register offsets into the EPLD memory space.
 */

#ifndef __ARCH_SH_BOARDS_ST_COMMON_MB705_EPLD_H
#define __ARCH_SH_BOARDS_ST_COMMON_MB705_EPLD_H

#define EPLD_EMI_IDENT			0x000
#define EPLD_EMI_TEST			0x002
#define EPLD_EMI_SWITCH			0x004
#define EPLD_EMI_SWITCH_BOOTFROMNOR		(1<<8)
#define EPLD_EMI_RESET			0x006
#define EPLD_EMI_RESET_SW0			(1<<0)	/* mb680: MII */
#define EPLD_EMI_RESET_SW1			(1<<1)	/* mb680: PCI */
#define EPLD_EMI_RESET_SW2			(1<<2)	/* mb680: STEM */
#define EPLD_EMI_RESET_SW3			(1<<3)	/* mb680: n/c */
#define EPLD_EMI_RESET_FLASH			(1<<4)
#define EPLD_EMI_RESET_DVB			(1<<5)
#define EPLD_EMI_RESET_DISPLAY			(1<<6)
#define EPLD_EMI_RESET_MAFE			(1<<7)
#define EPLD_EMI_RESET_SPDIF			(1<<8)
#define EPLD_EMI_SMARTCARD		0x008
#define EPLD_EMI_MISC			0x00a
#define EPLD_EMI_MISC_NORFLASHVPPEN		(1<<2)
#define EPLD_EMI_MISC_NOTNANDFLASHWP		(1<<3)
#define EPLD_EMI_INT_STATUS		0x020
#define EPLD_EMI_INT_MASK		0x022
#define EPLD_EMI_INT_PRI(x)		(0x024+((x)*2))

#define EPLD_TS_DISPLAY_CTRL_REG	0x10c
#define EPLD_TS_DISPLAY0_BASE		0x140
#define EPLD_TS_DISPLAY1_BASE		0x180

#define EPLD_AUDIO_IDENT		0x200
#define EPLD_AUDIO_TEST			0x202
#define EPLD_AUDIO_RESET		0x204
#define EPLD_AUDIO_RESET_AUDDAC0		(1<<0)
#define EPLD_AUDIO_RESET_AUDDAC1		(1<<1)
#define EPLD_AUDIO_RESET_AUDDAC2		(1<<2)
#define EPLD_AUDIO_RESET_SPDIFIN		(1<<3)
#define EPLD_AUDIO_RESET_SPDIFOUT		(1<<4)
#define EPLD_AUDIO_SWITCH0		0x206
#define EPLD_AUDIO_SWITCH0_SW61			(1<<0)
#define EPLD_AUDIO_SWITCH0_SW62			(1<<1)
#define EPLD_AUDIO_SWITCH0_SW63			(1<<2)
#define EPLD_AUDIO_SWITCH0_SW64			(1<<3)
#define EPLD_AUDIO_SWITCH0_SW41			(1<<4)
#define EPLD_AUDIO_SWITCH0_SW42			(1<<5)
#define EPLD_AUDIO_SWITCH0_SW43			(1<<6)
#define EPLD_AUDIO_SWITCH0_SW44			(1<<7)
#define EPLD_AUDIO_SWITCH1		0x208
#define EPLD_AUDIO_SWITCH1_SW31			(1<<0)
#define EPLD_AUDIO_SWITCH1_SW32			(1<<1)
#define EPLD_AUDIO_SWITCH1_SW33			(1<<2)
#define EPLD_AUDIO_SWITCH1_SW34			(1<<3)
#define EPLD_AUDIO_SWITCH1_SW51			(1<<4)
#define EPLD_AUDIO_SWITCH1_SW52			(1<<5)
#define EPLD_AUDIO_SWITCH1_SW53			(1<<6)
#define EPLD_AUDIO_SWITCH1_SW54			(1<<7)
#define EPLD_AUDIO_SWITCH2		0x20a
#define EPLD_AUDIO_SWITCH1_SW11			(1<<0)
#define EPLD_AUDIO_SWITCH1_SW12			(1<<1)
#define EPLD_AUDIO_SWITCH1_SW13			(1<<2)
#define EPLD_AUDIO_SWITCH1_SW14			(1<<3)
#define EPLD_AUDIO_SWITCH1_SW21			(1<<4)
#define EPLD_AUDIO_SWITCH1_SW22			(1<<5)
#define EPLD_AUDIO_SWITCH1_SW23			(1<<6)
#define EPLD_AUDIO_SWITCH1_SW24			(1<<7)
#define EPLD_AUDIO_SPDIFIN		0x20c
#define EPLD_AUDIO_SPDIFIN_I2CEN		(1<<0)
#define EPLD_AUDIO_SPDIFIN_SNOTH		(1<<1)
#define EPLD_AUDIO_SPDIFIN_AD0NVRERR		(1<<2)
#define EPLD_AUDIO_SPDIFIN_AD1NOTAUDIO		(1<<3)
#define EPLD_AUDIO_SPDIFIN_AD2U			(1<<4)
#define EPLD_AUDIO_SPDIFIN_C			(1<<5)
#define EPLD_AUDIO_SPDIFIN_96KHZ		(1<<6)
#define EPLD_AUDIO_SPDIFIN_RCBL			(1<<7)
#define EPLD_AUDIO_SPDIFOUT0		0x20e
#define EPLD_AUDIO_SPDIFOUT0_I2CEN		(1<<0)
#define EPLD_AUDIO_SPDIFOUT0_HNOTS		(1<<1)
#define EPLD_AUDIO_SPDIFOUT0_AD0		(1<<2)
#define EPLD_AUDIO_SPDIFOUT0_AD1		(1<<3)
#define EPLD_AUDIO_SPDIFOUT0_AD2		(1<<4)
#define EPLD_AUDIO_SPDIFOUT0_CEN		(1<<5)
#define EPLD_AUDIO_SPDIFOUT0_APMS		(1<<6)
#define EPLD_AUDIO_SPDIFOUT0_TRCBLD		(1<<7)
#define EPLD_AUDIO_SPDIFOUT1		0x210
#define EPLD_AUDIO_SPDIFOUT1_SFMT0		(1<<0)
#define EPLD_AUDIO_SPDIFOUT1_SFMT1		(1<<1)
#define EPLD_AUDIO_DAC0			0x212
#define EPLD_AUDIO_DAC0_DIF0			(1<<0)
#define EPLD_AUDIO_DAC0_DIF1			(1<<1)
#define EPLD_AUDIO_DAC0_SMUTE			(1<<2)
#define EPLD_AUDIO_DAC0_ACKS			(1<<3)
#define EPLD_AUDIO_DAC0_DEM			(1<<4)
#define EPLD_AUDIO_DAC1			0x214
#define EPLD_AUDIO_DAC1_DIF0			(1<<0)
#define EPLD_AUDIO_DAC1_DIF1			(1<<1)
#define EPLD_AUDIO_DAC1_SMUTE			(1<<2)
#define EPLD_AUDIO_DAC1_ACKS			(1<<3)
#define EPLD_AUDIO_DAC1_DEM			(1<<4)
#define EPLD_AUDIO_DAC2			0x216
#define EPLD_AUDIO_DAC2_DIF0			(1<<0)
#define EPLD_AUDIO_DAC2_DIF1			(1<<1)
#define EPLD_AUDIO_DAC2_SMUTE			(1<<2)
#define EPLD_AUDIO_DAC2_ACKS			(1<<3)
#define EPLD_AUDIO_DAC2_DEM0			(1<<4)
#define EPLD_AUDIO_DAC2_DEM1			(1<<5)
#define EPLD_AUDIO_DAC2_I2CEN			(1<<6)
#define EPLD_AUDIO_DAC2_PNOTS			(1<<7)
#define EPLD_AUDIO_USERLED		0x218
#define EPLD_AUDIO_USERLED_LD11B		(1<<0)
#define EPLD_AUDIO_USERLED_LD11T		(1<<1)
#define EPLD_AUDIO_USERLED_LD10B		(1<<2)
#define EPLD_AUDIO_USERLED_LD10T		(1<<3)

void mb705_reset(int bit, unsigned long usdelay);
extern char mb705_rev;

#endif
