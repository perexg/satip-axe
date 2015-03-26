/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/stm/device.h>

struct stm_device_state {
	struct device *dev;
	struct stm_device_config *config;
	enum stm_device_power_state power_state;
	struct stm_pad_state *pad_state;
	struct sysconf_field *sysconf_fields[0]; /* To be expanded */
};

static int __stm_device_init(struct stm_device_state *state,
		struct stm_device_config *config, struct device *dev)
{
	int i;

	state->dev = dev;
	state->config = config;
	state->power_state = stm_device_power_off;

	for (i = 0; i < config->sysconfs_num; i++) {
		struct stm_device_sysconf *sysconf = &config->sysconfs[i];

		state->sysconf_fields[i] = sysconf_claim(sysconf->regtype,
				sysconf->regnum, sysconf->lsb, sysconf->msb,
				dev_name(dev));
		if (!state->sysconf_fields[i])
			goto sysconf_error;
	}

	if (config->init && config->init(state))
		goto sysconf_error;

	if (config->pad_config &&
	    (state->pad_state = stm_pad_claim(config->pad_config,
					      dev_name(dev))) == NULL)
		goto pad_error;

	stm_device_power(state, stm_device_power_on);

	return 0;

pad_error:
	if (config->exit)
		config->exit(state);

sysconf_error:
	for (i--; i>=0; i--)
		sysconf_release(state->sysconf_fields[i]);

	return -EBUSY;
}

static void __stm_device_exit(struct stm_device_state *state)
{
	struct stm_device_config *config = state->config;
	int i;

	stm_device_power(state, stm_device_power_off);

	if (config->pad_config)
		stm_pad_release(state->pad_state);

	if (config->exit)
		config->exit(state);

	for (i = 0; i < config->sysconfs_num; i++)
		sysconf_release(state->sysconf_fields[i]);
}

struct stm_device_state *stm_device_init(struct stm_device_config *config,
		struct device *dev)
{
	struct stm_device_state *state = kzalloc(sizeof(*state) +
		sizeof(*state->sysconf_fields) * config->sysconfs_num,
		GFP_KERNEL);

	BUG_ON(!config);
	BUG_ON(!dev);

	if (state && __stm_device_init(state, config, dev) != 0)
		state = NULL;

	return state;
}
EXPORT_SYMBOL(stm_device_init);

void stm_device_exit(struct stm_device_state *state)
{
	BUG_ON(!state);

	__stm_device_exit(state);

	kfree(state);
}
EXPORT_SYMBOL(stm_device_exit);



static void stm_device_devres_exit(struct device *dev, void *res)
{
	struct stm_device_state *state = res;

	BUG_ON(!state);

	__stm_device_exit(state);
}

static int stm_device_devres_match(struct device *dev, void *res, void *data)
{
	struct stm_device_state *state = res, *match = data;

	return state == match;
}

struct stm_device_state *devm_stm_device_init(struct device *dev,
	struct stm_device_config *config)
{
	struct stm_device_state *state = devres_alloc(stm_device_devres_exit,
			sizeof(*state) + sizeof(*state->sysconf_fields) *
			config->sysconfs_num, GFP_KERNEL);

	BUG_ON(!dev);
	BUG_ON(!config);

	if (state) {
		if (__stm_device_init(state, config, dev) == 0) {
			devres_add(dev, state);
		} else {
			devres_free(state);
			state = NULL;
		}
	}

	return state;
}
EXPORT_SYMBOL(devm_stm_device_init);

void devm_stm_device_exit(struct device *dev, struct stm_device_state *state)
{
	int err;

	__stm_device_exit(state);

	err = devres_destroy(dev, stm_device_devres_exit,
			stm_device_devres_match, state);
	WARN_ON(err);
}
EXPORT_SYMBOL(devm_stm_device_exit);



static int stm_device_find_sysconf(struct stm_device_config *config,
		const char *name)
{
	int result = -1;
	int i;

	BUG_ON(!name);

	for (i = 0; i < config->sysconfs_num; i++) {
		struct stm_device_sysconf *pad_sysconf = &config->sysconfs[i];

		if (strcmp(name, pad_sysconf->name) == 0) {
			result = i;
			break;
		}
	}

	return result;
}

void stm_device_sysconf_write(struct stm_device_state *state,
		const char* name, unsigned long long value)
{
	int i;

	i = stm_device_find_sysconf(state->config, name);
	WARN_ON(i < 0);
	if (i >= 0)
		sysconf_write(state->sysconf_fields[i], value);
}
EXPORT_SYMBOL(stm_device_sysconf_write);

unsigned long long stm_device_sysconf_read(struct stm_device_state *state,
		const char* name)
{
	int i;
	unsigned long long result = -1;

	i = stm_device_find_sysconf(state->config, name);
	WARN_ON(i < 0);
	if (i >= 0)
		result = sysconf_read(state->sysconf_fields[i]);

	return result;
}
EXPORT_SYMBOL(stm_device_sysconf_read);



void stm_device_power(struct stm_device_state *device_state,
		enum stm_device_power_state power_state)
{
	if (device_state->power_state == power_state)
		return;

	if (device_state->config->power)
		device_state->config->power(device_state, power_state);

	device_state->power_state = power_state;
}
EXPORT_SYMBOL(stm_device_power);

struct stm_pad_state* stm_device_get_pad_state(struct stm_device_state *state)
{
	return state->pad_state;
}
EXPORT_SYMBOL(stm_device_get_pad_state);

struct stm_device_config* stm_device_get_config(struct stm_device_state *state)
{
	return state->config;
}
EXPORT_SYMBOL(stm_device_get_config);
