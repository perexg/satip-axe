#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/hardirq.h>
#include <linux/irqflags.h>
#include <linux/rwsem.h>
#include <asm/uaccess.h>

#define STV6120_1 (0xc0 >> 1)
#define STV6120_2 (0xc6 >> 1)

extern int (*i2c_transfer_mangle)(struct i2c_adapter *adap, struct i2c_msg *msg, int num);
int i2c_transfer2(struct i2c_adapter *adap, struct i2c_msg *msg, int num);

static struct i2c_adapter *i2c_adapter0;
static int i2c_mangle_enable = 1;
static int i2c_mangle_debug = 0;
static int stv6120_gain = 8;

static void i2c_transfer_axe_dump(struct i2c_msg *msgs, int num)
{
	int ret;

	for (ret = 0; ret < num; ret++) {
		printk("i2c master_xfer[%d] %c, addr=0x%02x, "
			"len=%d%s, flags=0x%x\n", ret, (msgs[ret].flags & I2C_M_RD)
			? 'R' : 'W', msgs[ret].addr, msgs[ret].len,
			(msgs[ret].flags & I2C_M_RECV_LEN) ? "+" : "",
			msgs[ret].flags);
	}
}

static void mangle(u8 *dst, struct i2c_msg *m, int i, int val, int shift, int mask)
{
	memcpy(dst, m->buf, m->len);
	dst[i] &= ~(mask << shift);
	dst[i] |= (val & mask) << shift;
	if (i2c_mangle_debug & 2)
		printk("i2c mangle: i=%d val=0x%x shift=%i mask=0x%x (orig 0x%x new 0x%x)\n",
			i, val, shift, mask, m->buf[i], dst[i]);
	if (m->buf[i] != dst[i])
		m->buf = dst;
}

static void i2c_transfer_axe_mangle(struct i2c_msg *msgs, int num)
{
	static u8 mbuf[4][32];
	struct i2c_msg *m;
	int ret, r, i;

	for (ret = 0; ret < num && ret < ARRAY_SIZE(mbuf); ret++) {
		m = msgs + ret;
		if (m->len < 1 || (m->flags & I2C_M_RD) != 0)
			continue;
		if (m->addr == STV6120_1 || m->addr == STV6120_2)
			for (r = m->buf[0], i = 1; i < m->len; i++, r++)
				if (r == 0x01 || r == 0x0b)
					mangle(mbuf[ret], m, i, stv6120_gain, 0, 0x0f);
	}
}

static int i2c_transfer_axe(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	if (adap == i2c_adapter0) {
		if (i2c_mangle_debug & 1)
			i2c_transfer_axe_dump(msgs, num);
		if (i2c_mangle_enable)
			i2c_transfer_axe_mangle(msgs, num);
	}
	return i2c_transfer2(adap, msgs, num);
}

/*
 *
 */

static ssize_t i2c_mangle_enable_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%u\n", i2c_mangle_enable);
}

static ssize_t i2c_mangle_enable_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;
	i2c_mangle_enable = val;
	return count;
}

static DEVICE_ATTR(i2c_mangle_enable, 0644,
		   i2c_mangle_enable_show,
		   i2c_mangle_enable_store);

static ssize_t i2c_mangle_debug_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%u\n", i2c_mangle_debug);
}

static ssize_t i2c_mangle_debug_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;
	i2c_mangle_debug = val;
	return count;
}

static DEVICE_ATTR(i2c_mangle_debug, 0644,
		   i2c_mangle_debug_show,
		   i2c_mangle_debug_store);

static ssize_t stv6120_gain_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%u\n", stv6120_gain);
}

static ssize_t stv6120_gain_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;
	stv6120_gain = val;
	return count;
}

static DEVICE_ATTR(stv6120_gain, 0644,
		   stv6120_gain_show,
		   stv6120_gain_store);

static void sysfs_create_entries(void)
{
	struct kobject *kobj = &i2c_adapter0->dev.kobj;

	sysfs_create_file(kobj, &dev_attr_i2c_mangle_enable.attr);
	sysfs_create_file(kobj, &dev_attr_i2c_mangle_debug.attr);
	sysfs_create_file(kobj, &dev_attr_stv6120_gain.attr);
}

static void sysfs_remove_entries(void)
{
	struct kobject *kobj = &i2c_adapter0->dev.kobj;

	sysfs_remove_file(kobj, &dev_attr_i2c_mangle_enable.attr);
	sysfs_remove_file(kobj, &dev_attr_i2c_mangle_debug.attr);
	sysfs_remove_file(kobj, &dev_attr_stv6120_gain.attr);
}

/*
 *
 */

int init_module(void)
{
	i2c_adapter0 = i2c_get_adapter(0);
	if (i2c_adapter0 == NULL) {
		printk(KERN_ERR "i2c_mangle: unable to find adapter 0\n");
		return -ENODEV;
	}
	sysfs_create_entries();
	i2c_transfer_mangle = i2c_transfer_axe;
	printk(KERN_INFO "I2C-Bus AXE mangle module loaded\n");
	return 0;
}

void cleanup_module(void)
{
	sysfs_remove_entries();
	i2c_transfer_mangle = NULL;
	i2c_put_adapter(i2c_adapter0);
}

MODULE_AUTHOR("Jaroslav Kysela");
MODULE_DESCRIPTION("I2C-Bus AXE mangle module");
MODULE_LICENSE("GPL");
