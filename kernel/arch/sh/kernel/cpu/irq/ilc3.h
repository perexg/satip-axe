/*
 * Copyright (C) 2003-2010 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interrupt handling for STMicroelectronics Interrupt Level Controler (ILC).
 */

#ifndef __ARCH_SH_KERNEL_CPU_IRQ_ILC3_H
#define __ARCH_SH_KERNEL_CPU_IRQ_ILC3_H

#define _BIT(_int)		     (1 << (_int % 32))
#define _REG_OFF(_int)		     (sizeof(int) * (_int / 32))
#define _BANK(_irq)		     ((_irq) >> 5)

#define ILC_BASE_STATUS              0x200
#define ILC_BASE_ENABLE              0x400

#define ILC_INTERRUPT_REG(_int)      (0x080 + _REG_OFF(_int))
#define ILC_STATUS_REG(_int)         (0x200 + _REG_OFF(_int))
#define ILC_CLR_STATUS_REG(_int)     (0x280 + _REG_OFF(_int))
#define ILC_ENABLE_REG(_int)         (0x400 + _REG_OFF(_int))
#define ILC_CLR_ENABLE_REG(_int)     (0x480 + _REG_OFF(_int))
#define ILC_SET_ENABLE_REG(_int)     (0x500 + _REG_OFF(_int))

#define ILC_EXT_WAKEUP_EN(_int)	     (0x600 + _REG_OFF(_int))
#define ILC_EXT_WAKPOL_EN(_int)	     (0x680 + _REG_OFF(_int))

#define ILC_PRIORITY_REG(_int)       (0x800 + (8 * _int))
#define ILC_TRIGMODE_REG(_int)       (0x804 + (8 * _int))

/*
 * Macros to get/set/clear ILC registers
 */
#define ILC_SET_ENABLE(_base, _int) \
		writel(_BIT(_int), _base + ILC_SET_ENABLE_REG(_int))
#define ILC_CLR_ENABLE(_base, _int) \
		writel(_BIT(_int), _base + ILC_CLR_ENABLE_REG(_int))
#define ILC_GET_ENABLE(_base, _int) \
		(readl(_base + ILC_ENABLE_REG(_int)) & _BIT(_int))

#define ILC_CLR_STATUS(_base, _int) \
		writel(_BIT(_int), _base + ILC_CLR_STATUS_REG(_int))
#define ILC_GET_STATUS(_base, _int) \
		(readl(_base + ILC_STATUS_REG(_int)) & _BIT(_int))

#define ILC_SET_PRI(_base, _int, _pri) \
		writel((_pri), _base + ILC_PRIORITY_REG(_int))

#define ILC_SET_TRIGMODE(_base, _int, _mod) \
		writel((_mod), _base + ILC_TRIGMODE_REG(_int))

#define ILC_WAKEUP_ENABLE(_base, _int) \
		writel(readl(_base + ILC_EXT_WAKEUP_EN(_int)) |	\
				_BIT(_int), _base + ILC_EXT_WAKEUP_EN(_int))
#define ILC_WAKEUP_DISABLE(_base, _int) \
		writel(readl(_base + ILC_EXT_WAKEUP_EN(_int)) & \
				~_BIT(_int), _base + ILC_EXT_WAKEUP_EN(_int))
#define ILC_WAKEUP_HI(_base, _int) \
		writel(readl(_base + ILC_EXT_WAKPOL_EN(_int)) | \
				_BIT(_int), _base + ILC_EXT_WAKPOL_EN(_int))
#define ILC_WAKEUP_LOW(_base, _int) \
		writel(readl(_base + ILC_EXT_WAKPOL_EN(_int)) & \
				~_BIT(_int), _base + ILC_EXT_WAKPOL_EN(_int))
#define ILC_WAKEUP(_base, _int, _high) \
		((_high) ? (ILC_WAKEUP_HI(_base, _int)) : \
				(ILC_WAKEUP_LOW(_base, _int)))

#define ILC_TRIGGERMODE_NONE	0
#define ILC_TRIGGERMODE_HIGH	1
#define ILC_TRIGGERMODE_LOW	2
#define ILC_TRIGGERMODE_RISING	3
#define ILC_TRIGGERMODE_FALLING	4
#define ILC_TRIGGERMODE_ANY	5

#endif
