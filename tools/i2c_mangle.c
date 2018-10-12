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
#define STV0900_1 (0xd0 >> 1)
#define STV0900_2 (0xd2 >> 1)

extern int (*i2c_transfer_mangle)(struct i2c_adapter *adap, struct i2c_msg *msg, int num);
int i2c_transfer2(struct i2c_adapter *adap, struct i2c_msg *msg, int num);

static struct i2c_adapter *i2c_adapter0;
static int i2c_mangle_enable = 1;
static int i2c_mangle_debug = 0;
static int stv6120_gain = 8;
static int stv0900_mis[4] = { -1, -1, -1, -1 };
static int stv0900_pls[4] = { -1, -1, -1, -1 };

static void i2c_transfer_axe_dump(struct i2c_msg *msgs, int num)
{
	struct i2c_msg *m;
	int ret;
	u8 *b;

	for (ret = 0; ret < num; ret++) {
		m = msgs + ret;
		b = m->buf;
		printk("i2c master_xfer[%d] %c, addr=0x%02x, "
			"len=%d%s, flags=0x%04x, "
			"data[%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x]\n",
			ret,
			(m->flags & I2C_M_RD) ? 'R' : 'W',
			m->addr, m->len,
			(m->flags & I2C_M_RECV_LEN) ? "+" : "",
			m->flags,
			m->len > 0 ? b[0] : 0,
			m->len > 1 ? b[1] : 0,
			m->len > 2 ? b[2] : 0,
			m->len > 3 ? b[3] : 0,
			m->len > 4 ? b[4] : 0,
			m->len > 5 ? b[5] : 0,
			m->len > 6 ? b[6] : 0,
			m->len > 7 ? b[7] : 0);
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

#define REG_SET3(b, b1, b2, b3) \
	b[0] = b1, b[1] = b2, b[2] = b3

static void demod_set_pls_and_mis(struct i2c_adapter *adap, struct i2c_msg *src, int p)
{
	struct i2c_msg m[6];
	u8 buf[6][3];
	int num, r, mis, idx = p ? 1 : 0;
	u32 pls;
	u8 iaddr = p ? 0xf3 : 0xf5;

	switch (src->addr) {
	case STV0900_1: idx += 0; break;
	case STV0900_2: idx += 2; break;
	default: return;
	}

	mis = stv0900_mis[idx];
	if (mis >= 0 && mis <= 255) {
		/* PDELCTRL1 - enable filter */
		REG_SET3(buf[0], iaddr, 0x50, 0x20);
		/* ISIENTRY */
		REG_SET3(buf[1], iaddr, 0x5e, mis);
		/* ISIBITENA */
		REG_SET3(buf[2], iaddr, 0x5f, 0xff);
		/* set PLS code and mode (upper three bits) */
		pls = stv0900_pls[idx];
		REG_SET3(buf[3], iaddr-1, 0xae, pls); /* PLROOT0 */
		REG_SET3(buf[4], iaddr-1, 0xad, pls >> 8); /* PLROOT1 */
		REG_SET3(buf[5], iaddr-1, 0xac, (pls >> 16) & 0x0f); /* PLROOT3 */
		num = 6;
		if (i2c_mangle_debug & 4)
			printk("i2c idx=%d: pls=%d mis=%d\n", idx, pls, mis);
	} else {
		/* SWRST */
		REG_SET3(buf[0], iaddr, 0x72, 0xd1);
		/* PDELCTRL1 - disable filter */
		REG_SET3(buf[1], iaddr, 0x50, 0x00);
		/* set PLS code to 1 and mode to ROOT */
		REG_SET3(buf[2], iaddr-1, 0xae, 1); /* PLROOT0 */
		REG_SET3(buf[3], iaddr-1, 0xad, 0); /* PLROOT1 */
		REG_SET3(buf[4], iaddr-1, 0xac, 0); /* PLROOT3 */

		num = 5;
		if (i2c_mangle_debug & 4)
			printk("i2c idx=%d: disable mis filter\n", idx);
	}
	for (r = 0; r < num; r++) {
		m[r] = *src;
		m[r].len = 3;
		m[r].buf = buf[r];
	}
	if (i2c_mangle_debug & 1)
		i2c_transfer_axe_dump(m, num);
	r = i2c_transfer2(adap, m, num);
	if (r < 0)
		printk("i2c mangle demod pls and mis error! (%d)\n", r);
}

static void i2c_transfer_axe_mangle(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	static u8 mbuf[4][32];
	struct i2c_msg *m;
	int ret, r, i;

	for (ret = 0; ret < num && ret < ARRAY_SIZE(mbuf); ret++) {
		m = msgs + ret;
		if (m->len < 1 || (m->flags & I2C_M_RD) != 0)
			continue;
		if (m->addr == STV6120_1 || m->addr == STV6120_2) {
			for (r = m->buf[0], i = 1; i < m->len; i++, r++)
				if (r == 0x01 || r == 0x0b)
					mangle(mbuf[ret], m, i, stv6120_gain, 0, 0x0f);
		} else if (m->addr == STV0900_1 || m->addr == STV0900_2) {
			/* inject pls/mis settings before TSCFGH path merger reset */
			if (m->flags == 0 && m->len == 3 &&
			    (m->buf[0] == 0xf3 || m->buf[0] == 0xf5) &&
			    m->buf[1] == 0x72 && m->buf[2] == 0xd1)
				demod_set_pls_and_mis(adap, m, m->buf[0] == 0xf3);
		}
	}
}

static int i2c_transfer_axe(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	if (adap == i2c_adapter0) {
		if (i2c_mangle_enable)
			i2c_transfer_axe_mangle(adap, msgs, num);
		if (i2c_mangle_debug & 1)
			i2c_transfer_axe_dump(msgs, num);
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

static ssize_t stv0900_mis1_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%i\n", stv0900_mis[0]);
}

static ssize_t stv0900_mis1_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	stv0900_mis[0] = val;
	return count;
}

static DEVICE_ATTR(stv0900_mis1, 0644,
		   stv0900_mis1_show,
		   stv0900_mis1_store);

static ssize_t stv0900_mis2_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%i\n", stv0900_mis[1]);
}

static ssize_t stv0900_mis2_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	stv0900_mis[1] = val;
	return count;
}

static DEVICE_ATTR(stv0900_mis2, 0644,
		   stv0900_mis2_show,
		   stv0900_mis2_store);

static ssize_t stv0900_mis3_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%i\n", stv0900_mis[2]);
}

static ssize_t stv0900_mis3_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	stv0900_mis[2] = val;
	return count;
}

static DEVICE_ATTR(stv0900_mis3, 0644,
		   stv0900_mis3_show,
		   stv0900_mis3_store);

static ssize_t stv0900_mis4_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%i\n", stv0900_mis[3]);
}

static ssize_t stv0900_mis4_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	stv0900_mis[3] = val;
	return count;
}

static DEVICE_ATTR(stv0900_mis4, 0644,
		   stv0900_mis4_show,
		   stv0900_mis4_store);

static ssize_t stv0900_pls1_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%i\n", stv0900_pls[0]);
}

static ssize_t stv0900_pls1_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	stv0900_pls[0] = val;
	return count;
}

static DEVICE_ATTR(stv0900_pls1, 0644,
		   stv0900_pls1_show,
		   stv0900_pls1_store);

static ssize_t stv0900_pls2_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%i\n", stv0900_pls[1]);
}

static ssize_t stv0900_pls2_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	stv0900_pls[1] = val;
	return count;
}

static DEVICE_ATTR(stv0900_pls2, 0644,
		   stv0900_pls2_show,
		   stv0900_pls2_store);

static ssize_t stv0900_pls3_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%i\n", stv0900_pls[2]);
}

static ssize_t stv0900_pls3_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	stv0900_pls[2] = val;
	return count;
}

static DEVICE_ATTR(stv0900_pls3, 0644,
		   stv0900_pls3_show,
		   stv0900_pls3_store);

static ssize_t stv0900_pls4_show
  (struct device *dev, struct device_attribute *attr, char *page)
{
	return sprintf(page, "%i\n", stv0900_pls[3]);
}

static ssize_t stv0900_pls4_store
  (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	stv0900_pls[3] = val;
	return count;
}

static DEVICE_ATTR(stv0900_pls4, 0644,
		   stv0900_pls4_show,
		   stv0900_pls4_store);

static void sysfs_create_entries(void)
{
	struct kobject *kobj = &i2c_adapter0->dev.kobj;

	sysfs_create_file(kobj, &dev_attr_i2c_mangle_enable.attr);
	sysfs_create_file(kobj, &dev_attr_i2c_mangle_debug.attr);
	sysfs_create_file(kobj, &dev_attr_stv6120_gain.attr);
	sysfs_create_file(kobj, &dev_attr_stv0900_mis1.attr);
	sysfs_create_file(kobj, &dev_attr_stv0900_mis2.attr);
	sysfs_create_file(kobj, &dev_attr_stv0900_mis3.attr);
	sysfs_create_file(kobj, &dev_attr_stv0900_mis4.attr);
	sysfs_create_file(kobj, &dev_attr_stv0900_pls1.attr);
	sysfs_create_file(kobj, &dev_attr_stv0900_pls2.attr);
	sysfs_create_file(kobj, &dev_attr_stv0900_pls3.attr);
	sysfs_create_file(kobj, &dev_attr_stv0900_pls4.attr);
}

static void sysfs_remove_entries(void)
{
	struct kobject *kobj = &i2c_adapter0->dev.kobj;

	sysfs_remove_file(kobj, &dev_attr_i2c_mangle_enable.attr);
	sysfs_remove_file(kobj, &dev_attr_i2c_mangle_debug.attr);
	sysfs_remove_file(kobj, &dev_attr_stv6120_gain.attr);
	sysfs_remove_file(kobj, &dev_attr_stv0900_mis1.attr);
	sysfs_remove_file(kobj, &dev_attr_stv0900_mis2.attr);
	sysfs_remove_file(kobj, &dev_attr_stv0900_mis3.attr);
	sysfs_remove_file(kobj, &dev_attr_stv0900_mis4.attr);
	sysfs_remove_file(kobj, &dev_attr_stv0900_pls1.attr);
	sysfs_remove_file(kobj, &dev_attr_stv0900_pls2.attr);
	sysfs_remove_file(kobj, &dev_attr_stv0900_pls3.attr);
	sysfs_remove_file(kobj, &dev_attr_stv0900_pls4.attr);
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
