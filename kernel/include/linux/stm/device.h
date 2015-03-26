/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_DEVICE_H
#define __LINUX_STM_DEVICE_H

#include <linux/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pad.h>

struct stm_device_state;

struct stm_device_sysconf {
	int regtype;
	int regnum;
	int lsb;
	int msb;
	const char *name;
};

enum stm_device_power_state {
	stm_device_power_off,
	stm_device_power_on
};

struct stm_device_config {
	int sysconfs_num;
	struct stm_device_sysconf *sysconfs;
	struct stm_pad_config *pad_config;
	int (*init)(struct stm_device_state *state);
	int (*exit)(struct stm_device_state *state);
	void (*power)(struct stm_device_state *state,
		enum stm_device_power_state power_state);
};

#define STM_DEVICE_SYS_CFG(_regnum, _lsb, _msb, _name) \
	{ \
		.regtype = SYS_CFG, \
		.regnum = _regnum, \
		.lsb = _lsb, \
		.msb = _msb, \
		.name = _name, \
	}

#define STM_DEVICE_SYS_STA(_regnum, _lsb, _msb, _name) \
	{ \
		.regtype = SYS_STA, \
		.regnum = _regnum, \
		.lsb = _lsb, \
		.msb = _msb, \
		.name = _name, \
	}

#define STM_DEVICE_SYS_CFG_BANK(_bank, _regnum, _lsb, _msb, _name) \
	{ \
		.regtype = SYS_CFG_BANK##_bank, \
		.regnum = _regnum, \
		.lsb = _lsb, \
		.msb = _msb, \
		.name = _name, \
	}

#define STM_DEVICE_SYS_STA_BANK(_bank, _regnum, _lsb, _msb, _name) \
	{ \
		.regtype = SYS_STA_BANK##_bank, \
		.regnum = _regnum, \
		.lsb = _lsb, \
		.msb = _msb, \
		.name = _name, \
	}

/* We have to do this indirection to allow the first argument to
 * STM_DEVICE_SYSCONF to be a macro, as used by 5197 for example. */
#define ___STM_DEVICE_SYSCONF(_regtype, _regnum, _lsb, _msb, _name) \
        { \
                .regtype = _regtype, \
                .regnum = _regnum, \
                .lsb = _lsb, \
                .msb = _msb, \
                .name = _name, \
        }
#define STM_DEVICE_SYSCONF(_reg, _lsb, _msb, _name) \
	___STM_DEVICE_SYSCONF(_reg, _lsb, _msb, _name)

struct stm_device_state *stm_device_init(struct stm_device_config *config,
		struct device *dev);
void stm_device_exit(struct stm_device_state *state);

void stm_device_sysconf_write(struct stm_device_state *state,
		const char* name, unsigned long long value);
unsigned long long stm_device_sysconf_read(struct stm_device_state *state,
		const char* name);

struct stm_device_state *devm_stm_device_init(struct device *dev,
		struct stm_device_config *config);
void devm_stm_device_exit(struct device *dev,
		struct stm_device_state *state);

void stm_device_power(struct stm_device_state *state,
		enum stm_device_power_state);

struct stm_pad_state* stm_device_get_pad_state(struct stm_device_state *state);
struct stm_device_config* stm_device_get_config(struct stm_device_state *state);

#endif
