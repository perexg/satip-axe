/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_PAD_H
#define __LINUX_STM_PAD_H

#include <linux/stm/sysconf.h>

/* The stm_pad_gpio_value structure describes PIOs that are to be claimed in
 * order to achieve I/O configuration required by a driver.
 *
 * "function" means the so-called "alternative PIO function",
 * usually described in SOCs datasheets. It just describes
 * which one of possible signals is to be multiplexed to
 * the actual pin. It is then used by SOC-specific "gpio_config"
 * callback provided when stm_pad_init() is called (see below).
 *
 * Function number meaning is absolutely up to the BSP author.
 * There is just a polite suggestion that 0 could mean "normal"
 * PIO functionality (as in: input/output, set high/low level).
 * Other numbers may be related to datasheet definitions (usually
 * 1 and more).
 *
 * "ignored" direction means that the PIO will not be claimed at,
 * so setting it can be used to "remove" a PIO from configuration
 * in runtime. */

enum stm_pad_gpio_direction {
	stm_pad_gpio_direction_unknown,
	stm_pad_gpio_direction_in,
	stm_pad_gpio_direction_out,
	stm_pad_gpio_direction_bidir,
	stm_pad_gpio_direction_custom,
	stm_pad_gpio_direction_ignored
};

struct stm_pad_gpio {
	unsigned gpio;
	enum stm_pad_gpio_direction direction;
	int out_value;
	int function;
	const char *name;
	void *priv;
};

#define STM_PAD_PIO_IN(_port, _pin, _function) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _function, \
	}

#define STM_PAD_PIO_IN_NAMED(_port, _pin, _function, _name) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _function, \
		.name = _name, \
	}

#define STM_PAD_PIO_OUT(_port, _pin, _function) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.out_value = -1, \
		.function = _function, \
	}

#define STM_PAD_PIO_OUT_NAMED(_port, _pin, _function, _name) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.out_value = -1, \
		.function = _function, \
		.name = _name, \
	}

#define STM_PAD_PIO_BIDIR(_port, _pin, _function) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_bidir, \
		.out_value = -1, \
		.function = _function, \
	}

#define STM_PAD_PIO_BIDIR_NAMED(_port, _pin, _function, _name) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_bidir, \
		.out_value = -1, \
		.function = _function, \
		.name = _name, \
	}

#define STM_PAD_PIO_STUB_NAMED(_port, _pin, _name) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_unknown, \
		.name = _name, \
	}



/* The bits that give us the most grief are "sysconf" values, and
 * they are the most likely the SOC-specific settings that must
 * be set while configuring the chip to some function, as required
 * by a driver.
 *
 * Notice that you are not supposed to define GPIO muxing (the
 * "alternative functions" mentioned above) related bits here.
 * They should be configured automagically via SOC-specific
 * muxing funtions (see stm_pad_init() below) */

struct stm_pad_sysconf {
	int regtype;
	int regnum;
	int lsb;
	int msb;
	int value;
};

#define STM_PAD_SYS_CFG(_regnum, _lsb, _msb, _value) \
	{ \
		.regtype = SYS_CFG, \
		.regnum = _regnum, \
		.lsb = _lsb, \
		.msb = _msb, \
		.value = _value, \
	}

#define STM_PAD_SYS_CFG_BANK(_bank, _regnum, _lsb, _msb, _value) \
	{ \
		.regtype = SYS_CFG_BANK##_bank, \
		.regnum = _regnum, \
		.lsb = _lsb, \
		.msb = _msb, \
		.value = _value, \
	}

/* We have to do this indirection to allow the first argument to
 * STM_PAD_SYSCONF to be a macro, as used by 5197 for example. */
#define ___STM_PAD_SYSCONF(_regtype, _regnum, _lsb, _msb, _value) \
        { \
                .regtype = _regtype, \
                .regnum = _regnum, \
                .lsb = _lsb, \
                .msb = _msb, \
                .value = _value, \
        }
#define STM_PAD_SYSCONF(_reg, _lsb, _msb, _value) \
	___STM_PAD_SYSCONF(_reg, _lsb, _msb, _value)



/* Pad state structure pointer is returned by the claim functions */
struct stm_pad_state;



/* All above bits and pieces are gathered together in the below
 * structure, known as "pad configuration".
 *
 * It contains lists of GPIOs used and sysconfig bits to be
 * configured on demand of a driver.
 *
 * In special cases one may wish to use custom claim function,
 * which is executed as the _last_ in order when claiming
 * and must return 0 or other value in case of error. */

struct stm_pad_config {
	int gpios_num;
	struct stm_pad_gpio *gpios;
	int sysconfs_num;
	struct stm_pad_sysconf *sysconfs;
	int (*custom_claim)(struct stm_pad_state *state, void *priv);
	void (*custom_release)(struct stm_pad_state *state, void *priv);
	void *custom_priv;
};



/* Pad manager initialization
 *
 * The gpio_config function should be provided by the SOC BSP
 * and configure given GPIO to requested direction & alternative function.
 *
 * gpios_num is a overall number of PIO lines provided by the SOC,
 * gpio_function_in and gpio_function_out are the numbers that should be passed
 * to the gpio_config function in order to select generic PIO functionality, for
 * input and output directions respectively.
 *
 * See also above (stm_pad_gpio_value definition). */

int stm_pad_init(int gpios_num, int gpio_function_in, int gpio_function_out,
			int (*gpio_config)(unsigned gpio,
			enum stm_pad_gpio_direction direction,
			int function, void *priv));



/* Driver interface */

struct stm_pad_state *stm_pad_claim(struct stm_pad_config *config,
		const char *owner);
void stm_pad_release(struct stm_pad_state *state);

struct stm_pad_state *devm_stm_pad_claim(struct device *dev,
		struct stm_pad_config *config, const char *owner);
void devm_stm_pad_release(struct device *dev, struct stm_pad_state *state);

/* Functions below are private methods, for the GPIO driver use only! */
int stm_pad_claim_gpio(unsigned gpio);
void stm_pad_configure_gpio(unsigned gpio, unsigned direction);
void stm_pad_release_gpio(unsigned gpio);
const char *stm_pad_get_gpio_owner(unsigned gpio);



/* GPIO interface */

/* "name" is the GPIO name as defined in "struct stm_pad_gpio".
 * Returns gpio number or STM_GPIO_INVALID in case of error */
unsigned stm_pad_gpio_request_input(struct stm_pad_state *state,
		const char *name);
unsigned stm_pad_gpio_request_output(struct stm_pad_state *state,
		const char *name, int value);
void stm_pad_gpio_free(struct stm_pad_state *state, unsigned gpio);



/* GPIO definition helpers
 *
 * If a GPIO on the list in pad configuration is defined with a name,
 * it is possible to perform some operations on it in easy way... */

int stm_pad_set_gpio(struct stm_pad_config *config, const char *name,
		unsigned gpio);

#define stm_pad_set_pio(config, name, port, pin) \
	stm_pad_set_gpio(config, name, stm_gpio(port, pin))

int stm_pad_set_direction_function(struct stm_pad_config *config,
		const char *name, enum stm_pad_gpio_direction direction,
		int out_value, int function);

#define stm_pad_set_pio_in(config, name, function) \
	stm_pad_set_direction_function(config, name, \
			stm_pad_gpio_direction_in, -1, function)

#define stm_pad_set_pio_out(config, name, function) \
	stm_pad_set_direction_function(config, name, \
			stm_pad_gpio_direction_out, -1, function)

#define stm_pad_set_pio_bidir(config, name, function) \
	stm_pad_set_direction_function(config, name, \
			stm_pad_gpio_direction_bidir, -1, function)

#define stm_pad_set_pio_ignored(config, name) \
	stm_pad_set_direction_function(config, name, \
			stm_pad_gpio_direction_ignored, -1, -1)

int stm_pad_set_priv(struct stm_pad_config *config, const char *name,
		void *priv);



/* Dynamic pad configuration allocation
 *
 * In some cases it's easier to create a pad configuration in runtime,
 * rather then to prepare 2^16 different static blobs (or to alter
 * 90% of pre-defined one). The API below helps in this... */

struct stm_pad_config *stm_pad_config_alloc(int gpio_values_num,
		int sysconf_values_num);

int stm_pad_config_add_sysconf(struct stm_pad_config *config,
		int regtype, int regnum, int lsb, int msb, int value);

#define stm_pad_config_add_sys_cfg(config, regnum, lsb, msb, value) \
	stm_pad_config_add_sysconf(config, SYS_CFG, regnum, lsb, msb, value)

int stm_pad_config_add_gpio_named(struct stm_pad_config *config,
		unsigned gpio, enum stm_pad_gpio_direction direction,
		int out_value, int function, const char *name);

#define stm_pad_config_add_pio(config, port, pin, \
			direction, out_value, function) \
	stm_pad_config_add_gpio_named(config, stm_gpio(port, pin), \
			direction, out_value, function, NULL)

#define stm_pad_config_add_pio_named(config, port, pin, \
			direction, out_value, function, name) \
	stm_pad_config_add_gpio_named(config, stm_gpio(port, pin), \
			direction, out_value, function, name)

#define stm_pad_config_add_pio_in(config, port, pin, function) \
	stm_pad_config_add_pio(config, port, pin, \
			stm_pad_gpio_direction_in, -1, function)

#define stm_pad_config_add_pio_in_named(config, port, pin, function, name) \
	stm_pad_config_add_pio_named(config, port, pin, \
			stm_pad_gpio_direction_in, -1, function, name)

#define stm_pad_config_add_pio_out(config, port, pin, function) \
	stm_pad_config_add_pio(config, port, pin, \
			stm_pad_gpio_direction_out, -1, function)

#define stm_pad_config_add_pio_out_named(config, port, pin, \
			function, name) \
	stm_pad_config_add_pio_named(config, port, pin, \
			stm_pad_gpio_direction_out, -1, function, name)

#define stm_pad_config_add_pio_bidir(config, port, pin, function) \
	stm_pad_config_add_pio(config, port, pin, \
			stm_pad_gpio_direction_bidir, -1, function)

#define stm_pad_config_add_pio_bidir_named(config, port, pin, \
			function, name) \
	stm_pad_config_add_pio_named(config, port, pin, \
			stm_pad_gpio_direction_bidir, -1, function, name)

#endif
