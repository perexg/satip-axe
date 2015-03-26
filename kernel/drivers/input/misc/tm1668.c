/*
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Driver for Shenzen Titan Micro Electronics TM1668 LED/keyboard driver.
 *
 * See <include/linux/tm1668.h> file for platform data definitions.
 *
 * /sys/.../tm1668/ files:
 *   /text - displays written string (via platform characters table)
 *   /keys - returns keyboard state (to be used as mask in key definition)
 *   /raw - raw keyboard (read) and display (write) buffers access
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/tm1668.h>



#define DRIVER_NAME "tm1668"



#define TM1668_DISPLAY_BUFFER_SIZE 14
#define TM1668_DISPLAY_MAX_DIGITS 7
#define TM1668_KEYBOARD_BUFFER_SIZE 4

#define TM1668_CMD_DISPLAY_MODE(mode_6_12_not_7_11) \
		(0x00 | \
		(mode_6_12_not_7_11 ? 0x2 : 0x3))

#define TM1668_CMD_DATA(test_not_normal_mode, display_address_increment, \
			read_not_write) \
		(0x40 | \
		(test_not_normal_mode ? 0x4 : 0x0) | \
		(display_address_increment ? 0x0 : 0x1) | \
		(read_not_write ? 0x2 : 0x0))

#define TM1668_CMD_ADDRESS(address) \
		(0xc0 | \
		(address & 0xf))

#define TM1668_CMD_DISPLAY_CONTROL(on, pulse_width) \
		(0x80 | \
		(on ? 0x8 : 0x0) | \
		(pulse_width & 0x7))



struct tm1668_chip {
	spinlock_t lock; /* access lock */

	unsigned gpio_dio, gpio_sclk, gpio_stb;

	int brightness;
	int characters_num;
	struct tm1668_character *characters;

	struct input_dev *input;
	u32 keys_prev;
	int keys_num;
	struct tm1668_key *keys;
	struct delayed_work keys_work;
	unsigned long keys_poll_period;
};



/* Serial interface */

static void tm1668_writeb(struct tm1668_chip *chip, u8 byte)
{
	int i;

	for (i = 0; i < 8; i++) {
		gpio_set_value(chip->gpio_dio, byte & 0x1);
		gpio_set_value(chip->gpio_sclk, 0);
		udelay(1);
		gpio_set_value(chip->gpio_sclk, 1);
		udelay(1);

		byte >>= 1;
	}
}

static void tm1668_readb(struct tm1668_chip *chip, u8 *byte)
{
	int i;

	*byte = 0;
	for (i = 0; i < 8; i++) {
		gpio_set_value(chip->gpio_sclk, 0);
		udelay(1);
		*byte |= gpio_get_value(chip->gpio_dio);
		gpio_set_value(chip->gpio_sclk, 1);
		udelay(1);

		*byte <<= 1;
	}
}

static void tm1668_send(struct tm1668_chip *chip, u8 command,
		const void *buf, int len)
{
	const u8 *data = buf;
	int i;

	BUG_ON(len > TM1668_DISPLAY_BUFFER_SIZE);

	gpio_set_value(chip->gpio_stb, 0);
	udelay(1);
	tm1668_writeb(chip, command);
	for (i = 0; i < len; i++)
		tm1668_writeb(chip, *data++);
	udelay(1);
	gpio_set_value(chip->gpio_stb, 1);
	udelay(2);
}

static void tm1668_recv(struct tm1668_chip *chip, u8 command,
		void *buf, int len)
{
	u8 *data = buf;
	int i;

	gpio_set_value(chip->gpio_stb, 0);
	udelay(1);
	tm1668_writeb(chip, command);
	udelay(1);
	gpio_direction_input(chip->gpio_dio);
	for (i = 0; i < len; i++)
		tm1668_readb(chip, data++);
	gpio_direction_output(chip->gpio_dio, 1);
	udelay(1);
	gpio_set_value(chip->gpio_stb, 1);
	udelay(2);
}


/* Keyboard input */

static void tm1668_keys_poll(struct work_struct *work)
{
	struct tm1668_chip *chip = container_of(work, struct tm1668_chip,
			keys_work.work);
	u32 keys, diff;
	int i;

	spin_lock(&chip->lock);

	tm1668_recv(chip, TM1668_CMD_DATA(0, 1, 1), &keys, sizeof(keys));

	spin_unlock(&chip->lock);

	diff = keys ^ chip->keys_prev;

	for (i = 0; i < chip->keys_num; i++) {
		struct tm1668_key *key = &chip->keys[i];

		if (diff & key->mask) {
			input_event(chip->input, EV_KEY, key->code,
					!!(keys & key->mask));
			input_sync(chip->input);
		}
	}

	chip->keys_prev = keys;

	schedule_delayed_work(&chip->keys_work, chip->keys_poll_period);
}

static ssize_t tm1668_keys_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tm1668_chip *chip = dev_get_drvdata(dev);
	u32 keys;

	spin_lock(&chip->lock);

	tm1668_recv(chip, TM1668_CMD_DATA(0, 1, 1), &keys, sizeof(keys));

	spin_unlock(&chip->lock);

	return sprintf(buf, "0x%08x\n", keys);
}

static DEVICE_ATTR(keys, S_IRUSR, tm1668_keys_show, NULL);


/* Display control & sysfs interface */

static void tm1668_clear(struct tm1668_chip *chip)
{
	u8 data[TM1668_DISPLAY_BUFFER_SIZE] = { 0, };

	tm1668_send(chip, TM1668_CMD_DATA(0, 1, 0), NULL, 0);
	tm1668_send(chip, TM1668_CMD_ADDRESS(0), data, sizeof(data));
}

static void tm1668_set_brightness(struct tm1668_chip *chip, int brightness)
{
	int enabled = brightness > 0;

	if (brightness < 0)
		brightness = 0;
	else if (brightness > 8)
		brightness = 8;
	chip->brightness = brightness;

	if (enabled)
		brightness--;

	tm1668_send(chip, TM1668_CMD_DISPLAY_CONTROL(enabled, brightness),
			NULL, 0);
}

static ssize_t tm1668_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result;
	struct tm1668_chip *chip = dev_get_drvdata(dev);

	spin_lock(&chip->lock);

	result = sprintf(buf, "%d\n", chip->brightness);

	spin_unlock(&chip->lock);

	return result;
}

static ssize_t tm1668_brightness_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	ssize_t result;
	struct tm1668_chip *chip = dev_get_drvdata(dev);
	unsigned long value;

	result = strict_strtoul(buf, 10, &value);
	if (result == 0) {
		spin_lock(&chip->lock);
		tm1668_set_brightness(chip, value);
		spin_unlock(&chip->lock);

		result = size;
	}

	return result;
}

static DEVICE_ATTR(brightness, S_IRUSR | S_IWUSR, tm1668_brightness_show,
		tm1668_brightness_store);

static void tm1668_print(struct tm1668_chip *chip, const char *text)
{
	u16 data[TM1668_DISPLAY_MAX_DIGITS] = { 0, };
	int i, j;

	BUG_ON(chip->characters_num <= 0 || !chip->characters);
	BUG_ON(!text);

	for (i = 0; i < TM1668_DISPLAY_MAX_DIGITS && *text; i++, text++)
		for (j = 0; j < chip->characters_num; j++)
			if (chip->characters[j].character == *text) {
				data[i] = chip->characters[j].value;
				break;
			}

	tm1668_send(chip, TM1668_CMD_DATA(0, 1, 0), NULL, 0);
	tm1668_send(chip, TM1668_CMD_ADDRESS(0), data, sizeof(data));
}

static ssize_t tm1668_text_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct tm1668_chip *chip = dev_get_drvdata(dev);

	spin_lock(&chip->lock);

	tm1668_print(chip, buf);

	spin_unlock(&chip->lock);

	return size;
}

static DEVICE_ATTR(text, S_IWUSR, NULL, tm1668_text_store);


/* raw sysfs interface */

static ssize_t tm1668_raw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tm1668_chip *chip = dev_get_drvdata(dev);

	spin_lock(&chip->lock);

	tm1668_recv(chip, TM1668_CMD_DATA(0, 1, 1), buf,
			TM1668_KEYBOARD_BUFFER_SIZE);

	spin_unlock(&chip->lock);

	return TM1668_KEYBOARD_BUFFER_SIZE;
}

static ssize_t tm1668_raw_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct tm1668_chip *chip = dev_get_drvdata(dev);
	u8 data[TM1668_DISPLAY_BUFFER_SIZE] = { 0, };

	strncpy(data, buf, TM1668_DISPLAY_BUFFER_SIZE);

	spin_lock(&chip->lock);

	tm1668_send(chip, TM1668_CMD_DATA(0, 1, 0), NULL, 0);
	tm1668_send(chip, TM1668_CMD_ADDRESS(0), data, sizeof(data));

	spin_unlock(&chip->lock);

	return size;
}

static DEVICE_ATTR(raw, S_IRUSR | S_IWUSR, tm1668_raw_show, tm1668_raw_store);


/* Driver routines */

static int __init tm1668_probe(struct platform_device *pdev)
{
	int err;
	struct tm1668_chip *chip;
	struct tm1668_platform_data *plat_data = pdev->dev.platform_data;
	int i;

	BUG_ON(!plat_data);

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto error_kzalloc;
	}
	spin_lock_init(&chip->lock);
	platform_set_drvdata(pdev, chip);

	/* Allocate GPIO lines... */

	chip->gpio_dio = plat_data->gpio_dio;
	err = gpio_request(chip->gpio_dio, dev_name(&pdev->dev));
	if (err != 0)
		goto error_request_gpio_dio;
	gpio_direction_output(chip->gpio_dio, 1);

	chip->gpio_sclk = plat_data->gpio_sclk;
	err = gpio_request(chip->gpio_sclk, dev_name(&pdev->dev));
	if (err != 0)
		goto error_request_gpio_sclk;
	gpio_direction_output(chip->gpio_sclk, 1);

	chip->gpio_stb = plat_data->gpio_stb;
	err = gpio_request(chip->gpio_stb, dev_name(&pdev->dev));
	if (err != 0)
		goto error_request_gpio_stb;
	gpio_direction_output(chip->gpio_stb, 1);

	/* Initialize the chip */

	tm1668_send(chip, TM1668_CMD_DISPLAY_MODE(plat_data->config ==
			tm1668_config_6_digits_12_segments), NULL, 0);

	/* Initialize keyboard interface */

	chip->input = input_allocate_device();
	if (!chip->input) {
		err = -ENOMEM;
		goto error_input_allocate;
	}
	chip->input->evbit[0] = BIT(EV_KEY);
	chip->input->name = pdev->name;
	chip->input->phys = DRIVER_NAME "/input0";
	chip->input->dev.parent = &pdev->dev;
	chip->input->id.bustype = BUS_HOST;
	chip->input->id.vendor = 0x0001;
	chip->input->id.product = 0x1668;
	chip->input->id.version = 0x0100;

	tm1668_recv(chip, TM1668_CMD_DATA(0, 1, 1), &chip->keys_prev,
			sizeof(chip->keys_prev));
	chip->keys_num = plat_data->keys_num;
	chip->keys = plat_data->keys;
	BUG_ON(chip->keys_num > 0 && !chip->keys);

	for (i = 0; i < chip->keys_num; i++)
		input_set_capability(chip->input, EV_KEY, chip->keys[i].code);

	err = input_register_device(chip->input);
	if (err) {
		input_free_device(chip->input);
		goto error_input_register;
	}

	chip->keys_poll_period = plat_data->keys_poll_period;
	BUG_ON(chip->keys_poll_period < 1); /* pick your number ;-) */
	INIT_DELAYED_WORK(&chip->keys_work, tm1668_keys_poll);
	schedule_delayed_work(&chip->keys_work, chip->keys_poll_period);

	err = device_create_file(&pdev->dev, &dev_attr_keys);
	if (err != 0)
		goto error_create_attr_keys;

	/* Initialize display interface */

	tm1668_clear(chip);
	tm1668_set_brightness(chip, plat_data->brightness);
	chip->characters_num = plat_data->characters_num;
	chip->characters = plat_data->characters;
	if (chip->characters_num > 0)
		tm1668_print(chip, plat_data->text);

	err = device_create_file(&pdev->dev, &dev_attr_brightness);
	if (err != 0)
		goto error_create_attr_brightness;

	if (chip->characters_num > 0) {
		err = device_create_file(&pdev->dev, &dev_attr_text);
		if (err != 0)
			goto error_create_attr_text;
	}

	/* Initialize raw interface */

	err = device_create_file(&pdev->dev, &dev_attr_raw);
	if (err != 0)
		goto error_create_attr_raw;

	return 0;

error_create_attr_raw:
	if (chip->characters_num > 0)
		device_remove_file(&pdev->dev, &dev_attr_text);
error_create_attr_text:
	device_remove_file(&pdev->dev, &dev_attr_brightness);
error_create_attr_brightness:
	device_remove_file(&pdev->dev, &dev_attr_keys);
error_create_attr_keys:
	cancel_delayed_work_sync(&chip->keys_work);
	input_unregister_device(chip->input);
error_input_register:
error_input_allocate:
	gpio_free(chip->gpio_stb);
error_request_gpio_stb:
	gpio_free(chip->gpio_sclk);
error_request_gpio_sclk:
	gpio_free(chip->gpio_dio);
error_request_gpio_dio:
	kfree(chip);
error_kzalloc:
	return err;

}

static int __exit tm1668_remove(struct platform_device *pdev)
{
	struct tm1668_chip *chip = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_raw);

	if (chip->characters_num > 0)
		device_remove_file(&pdev->dev, &dev_attr_text);
	device_remove_file(&pdev->dev, &dev_attr_brightness);
	tm1668_set_brightness(chip, 0);

	device_remove_file(&pdev->dev, &dev_attr_keys);
	cancel_delayed_work_sync(&chip->keys_work);

	input_unregister_device(chip->input);

	kfree(chip);

	return 0;
}

static struct platform_driver tm1668_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.remove		= __exit_p(tm1668_remove),
};

static int __init tm1668_init(void)
{
	return platform_driver_probe(&tm1668_driver, tm1668_probe);
}
module_init(tm1668_init);

static void __exit tm1668_exit(void)
{
	platform_driver_unregister(&tm1668_driver);
}
module_exit(tm1668_exit);

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("TM1668 LED/keyboard chip driver");
MODULE_LICENSE("GPL");

