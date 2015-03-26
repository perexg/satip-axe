/*
 * Copyright (C) 2004-2009 STMicroelectronics
 *
 * Author Angelo Castello <angelo.castello@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, wrssc to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Jul 2009 : Ported for kernel 2.6.30. Removed any deprecated istances.
 *            angelo.castello@st.com
 */

#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/stm/pio.h>
#include <linux/delay.h>

/* General debugging */
#undef M41ST85Y_DEBUG
#ifdef  M41ST85Y_DEBUG
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define M41ST85Y_NAME		"m41st85y"
#define M41ST85Y_NREGMAP	0x14	/* no of RTC's registers */
#define M41ST85Y_ADDR		0x68	/* MY41ST85Y slave address */
#define M41ST85Y_ISOPEN		0x01	/* means /dev/rtc is in use */
#define M41ST85Y_RD		0x01	/* read flag for a i2c transfer */
#define M41ST85Y_WR		0x00	/* write flag for a i2c transfer */
#define M41ST85Y_INVALID	0xff	/* invalid value */
#define M41ST85Y_IRQ_LEVEL	0x01	/* default value. 1=High, 0=Low */
#define M41ST85Y_SQW_LEVEL	0x00	/* default value. 1=High, 0=Low */
#define M41ST85Y_NOBUS		0x03	/* number of I2C busses */

/*
 * Addressing compliante to SPI PIO address mechanism
 * Address = [7:Not used:7][6:PIO-Port:3][2:PIO-Pin:0]
 * Ex: PIO0[7] = 0x07, PIO2[5] = 0x15
 */
#define m41st85y_get_pioport(address)	((address >> 0x03) & 0x0f)
#define m41st85y_get_piopin(address)	(address & 0x07)

/* Addresses to scan: none. This chip cannot be detected. */
static unsigned short normal_i2c[] = { M41ST85Y_ADDR, I2C_CLIENT_END };

/* Insmod parameters */
I2C_CLIENT_INSMOD;

/* private data */
struct m41st85y_s {
	struct rtc_device *rtc;
	struct i2c_adapter *adapter;
	unsigned long status;
	unsigned long epoch;	/* default linux epoch 1900 */
	struct stpio_pin *irqpio;	/* PIO used as RTC-IRQ line */
	struct stpio_pin *sqwpio;	/* PIO used as RTC-SWQ line */
	unsigned int cmd;
} m41st85y;

static __u8 rbuf[M41ST85Y_NREGMAP];
static __u8 wbuf[M41ST85Y_NREGMAP];
static __u32 busid = M41ST85Y_NOBUS;
static __u32 irqpio = CONFIG_RTC_DRV_M41ST85Y_IRQPIO /* Ex: 0x07 */ ,
    sqwpio = CONFIG_RTC_DRV_M41ST85Y_SQWPIO /* Ex: 0x0F */ ;

static int m41st85y_transfer(struct m41st85y_s *instance,
			     __u8 * buf, __u8 len, __u8 oper, __u8 at_addr)
{
	struct i2c_msg msg[2];
	__u8 n_msg;
	int err = 0;

	if (oper == M41ST85Y_WR) {
		/* perform write request */
		msg[0].addr = M41ST85Y_ADDR;
		msg[0].flags = oper;
		msg[0].len = len;
		msg[0].buf = buf;
		n_msg = 1;
	} else {
		/* perform read request */
		rbuf[0] = at_addr;
		msg[0].addr = M41ST85Y_ADDR;
		msg[0].flags = M41ST85Y_WR;
		msg[0].len = 1;
		msg[0].buf = rbuf;

		msg[1].addr = M41ST85Y_ADDR;
		msg[1].flags = M41ST85Y_RD;
		msg[1].len = len;
		msg[1].buf = buf;
		n_msg = 2;
	}

	if ((err = i2c_transfer(instance->adapter, msg, n_msg)) != n_msg)
		(oper == M41ST85Y_WR) ?
		    printk(KERN_ERR "m41st85y: I2C write failed, err %d\n",
			   err) : printk(KERN_ERR
					 "m41st85y: I2C read  failed, err %d\n",
					 err);
	return err;
}

static int m41st85y_power_up(void)
{
	__u8 RegsMap[M41ST85Y_NREGMAP];
	int err = 0;

	while (1) {
		m41st85y_transfer(&m41st85y, rbuf, 1, M41ST85Y_RD, 0x0F);
		if ((rbuf[0] & 0x40) == 0x00)
			break;
		printk(KERN_INFO
		       "m41st85y: There was an alarm during the back-up mode AF 0x%x\n",
		       rbuf[0]);
	}

	if ((err = m41st85y_transfer(&m41st85y,
				     RegsMap + 1, M41ST85Y_NREGMAP - 1,
				     M41ST85Y_RD, 0x01)) >= 0) {
		RegsMap[0x00] = 0x01;	/* address offset */
		RegsMap[0x01] &= ~0x80;	/* ST bit, wake up the oscillator */
		RegsMap[0x08] = 0x80;	/* IRQ/FT/OUT line is driven low */
		RegsMap[0x0A] &= ~0x40;	/* Swq disable */
		RegsMap[0x0C] &= ~0x40;	/* Update the TIMEKEEPER registers */
		RegsMap[0x13] &= 0x00;	/* Default Square wave output is 1Hz */
		/* also irq_freq should be setting up 1Hz at init fase */
		if ((err = m41st85y_transfer(&m41st85y,
					     RegsMap, M41ST85Y_NREGMAP,
					     M41ST85Y_WR,
					     M41ST85Y_INVALID)) >= 0) {
			/* waiting RTC hardware restart */
			ssleep(1);
			return 0;
		}
	}
	return err;
}

void m41st85y_handler(struct stpio_pin *pin, void *dev)
{
	struct m41st85y_s *instance = dev;
	__u8 skip = 0, events = 0;

	stpio_disable_irq(pin);

	if ((instance->cmd == RTC_PIE_ON) || (instance->cmd == RTC_UIE_ON)) {
		if (stpio_get_pin(pin) == M41ST85Y_IRQ_LEVEL) {
			skip = 1;
			stpio_enable_irq(pin, M41ST85Y_IRQ_LEVEL);
		} else
			stpio_enable_irq(pin, !M41ST85Y_IRQ_LEVEL);
	}

	if (!skip) {
		events |= RTC_IRQF;
		rtc_update_irq(instance->rtc, 1, events);
	}
}

static int m41st85y_read_time(struct device *dev, struct rtc_time *time_read)
{
	if (time_read == NULL)
		return -EIO;

	memset(time_read, 0, sizeof(struct rtc_time));

	if (m41st85y_transfer(&m41st85y, rbuf, 9, M41ST85Y_RD, 0x00) >= 0) {
		time_read->tm_sec = bcd2bin(rbuf[1] & 0x7f);
		time_read->tm_min = bcd2bin(rbuf[2] & 0x7f);
		time_read->tm_hour = bcd2bin(rbuf[3] & 0x3f);
		time_read->tm_wday = bcd2bin(rbuf[4] & 0x07);
		time_read->tm_mday = bcd2bin(rbuf[5] & 0x3f);
		time_read->tm_mon = bcd2bin(rbuf[6] & 0x1f);
		time_read->tm_year = bcd2bin(rbuf[7]);
		return 0;
	}

	return -EIO;
}

static int m41st85y_alarmset(struct device *dev,
			     unsigned int ioctl_cmd, struct rtc_time *ltime)
{
	/* to be sure that incoming ioctl request can be managed
	   by this */
	if ((ioctl_cmd != RTC_UIE_ON) && (ioctl_cmd != RTC_ALM_SET))
		return -1;

	if (ioctl_cmd == RTC_UIE_ON) {
		m41st85y_read_time(dev, ltime);

		/* alarm update */
		wbuf[0] = 0x0A;
		wbuf[1] = bin2bcd(ltime->tm_mon);
		wbuf[2] = 0xC0 | bin2bcd(ltime->tm_mday);
		wbuf[3] = 0x80 | bin2bcd(ltime->tm_hour);
		wbuf[4] = 0x80 | bin2bcd(ltime->tm_min);
		wbuf[5] = 0x80 | bin2bcd(ltime->tm_sec + 1);
		if (m41st85y_transfer(&m41st85y,
				      wbuf, 6, M41ST85Y_WR,
				      M41ST85Y_INVALID) >= 0) {
			wbuf[0] = 0x0A;
			wbuf[1] = (wbuf[1] | 0x80);
			DPRINTK("enable AFE writing %#x\n", wbuf[1]);
			if (m41st85y_transfer(&m41st85y,
					      wbuf, 2, M41ST85Y_WR,
					      M41ST85Y_INVALID) >= 0)
				return 0;
		}
	} else {
		if (m41st85y_transfer(&m41st85y,
				      rbuf, 6, M41ST85Y_RD, 0x0A) >= 0) {
			/* alarm update */
			wbuf[0] = 0x0A;
			wbuf[1] = (rbuf[0] & 0xE0) | bin2bcd(ltime->tm_mon);
			wbuf[2] = (rbuf[1] & 0xC0) | bin2bcd(ltime->tm_mday);
			wbuf[3] = (rbuf[2] & 0xC0) | bin2bcd(ltime->tm_hour);
			wbuf[4] = (rbuf[3] & 0x80) | bin2bcd(ltime->tm_min);
			wbuf[5] = (rbuf[4] & 0x80) | bin2bcd(ltime->tm_sec);
			DPRINTK("writing alarm date\n");
			if (m41st85y_transfer(&m41st85y,
					      wbuf, 6, M41ST85Y_WR,
					      M41ST85Y_INVALID) >= 0)
				return 0;
		}
	}
	return -EIO;
}

static int m41st85y_open(struct device *dev)
{
	/* locked at top level until the device will be 
	   release */
	if (m41st85y.status & M41ST85Y_ISOPEN) {
		return -EBUSY;
	}
	m41st85y.status |= M41ST85Y_ISOPEN;
	return 0;
}

static void m41st85y_release(struct device *dev)
{
	m41st85y.status &= ~M41ST85Y_ISOPEN;
	/* unlocked at top level before to be use it again */
}

static int m41st85y_set_time(struct device *dev, struct rtc_time *time_to_write)
{
	int err;

	/* this is already part of mutex area performed at top level */
	if ((err = m41st85y_transfer(&m41st85y,
				     rbuf, 8, M41ST85Y_RD, 0x00)) >= 0) {
		/* time update */
		wbuf[0] = 0x00;
		wbuf[1] = 0x00;
		wbuf[2] = (rbuf[1] & 0x80) | bin2bcd(time_to_write->tm_sec);
		wbuf[3] = (rbuf[2] & 0x80) | bin2bcd(time_to_write->tm_min);
		wbuf[4] = (rbuf[3] & 0xC0) | bin2bcd(time_to_write->tm_hour);
		memcpy(&wbuf[5], &rbuf[4], sizeof(char));
		wbuf[6] = (rbuf[5] & 0xC0) | bin2bcd(time_to_write->tm_mday);
		wbuf[7] = (rbuf[6] & 0xE0) | bin2bcd(time_to_write->tm_mon);
		wbuf[8] = bin2bcd((time_to_write->tm_year - m41st85y.epoch));

		err = m41st85y_transfer(&m41st85y,
					wbuf, 9, M41ST85Y_WR, M41ST85Y_INVALID);
	}
	return err;
}

static int m41st85y_read_alarm(struct device *dev,
			       struct rtc_wkalrm *alarm_read)
{
	int err = 0;

	if (alarm_read != NULL) {
		if ((err = m41st85y_transfer(&m41st85y,
					     rbuf, 6, M41ST85Y_RD, 0x0A)) >= 0)
		{
			alarm_read->time.tm_mon = bcd2bin(rbuf[0] & 0x1f);
			alarm_read->time.tm_mday = bcd2bin(rbuf[1] & 0x3f);
			alarm_read->time.tm_hour = bcd2bin(rbuf[2] & 0x3f);
			alarm_read->time.tm_min = bcd2bin(rbuf[3] & 0x7f);
			alarm_read->time.tm_sec = bcd2bin(rbuf[4] & 0x7f);
		}
	} else
		err = -EIO;

	return err;
}

static int m41st85y_set_alarm(struct device *dev,
			      struct rtc_wkalrm *alarm_to_write)
{
	return m41st85y_alarmset(dev, RTC_ALM_SET, &alarm_to_write->time);
}

static int m41st85y_ioctl(struct device *dev, unsigned int cmd,
			  unsigned long arg)
{
	struct rtc_time ltime;
	int err = 0;

	memset(&ltime, 0, sizeof(struct rtc_time));
	m41st85y.cmd = cmd;

	switch (cmd) {
	case RTC_UIE_OFF:	/* Mask ints from RTC updates.  */
	case RTC_AIE_OFF:	/* Mask alarm int. enab. bit    */
	case RTC_AIE_ON:	/* Allow alarm interrupts.      */
		{
			/* reading AFE bits */
			if (m41st85y_transfer(&m41st85y,
					      rbuf, 1, M41ST85Y_RD, 0x0A) >= 0)
			{
				char n_data = 2;

				wbuf[0] = 0x0A;
				if (cmd == RTC_AIE_ON) {
					stpio_enable_irq(m41st85y.irqpio,
							 M41ST85Y_IRQ_LEVEL);
					wbuf[1] = (rbuf[0] | 0x80);
				} else {
					stpio_disable_irq(m41st85y.irqpio);
					m41st85y.cmd = 0;	/* disable status */
					n_data = 6;
					wbuf[1] = (rbuf[0] & ~0xA0);	/* disabling AFE flag bit */
					wbuf[2] = wbuf[3] = wbuf[4] = wbuf[5] = 0x00;	/* disabling RPT5-RPT1 */
				}

				DPRINTK("writing AFE %#x\n", wbuf[1]);
				if (m41st85y_transfer(&m41st85y,
						      wbuf, n_data, M41ST85Y_WR,
						      M41ST85Y_INVALID) >= 0) {
					return 0;
				}
			}
			return -EIO;
		}
	case RTC_PIE_OFF:	/* Mask periodic int. enab. bit */
	case RTC_PIE_ON:	/* Allow periodic ints          */
		{
			if (m41st85y_transfer(&m41st85y,
					      rbuf, 1, M41ST85Y_RD, 0x0A) >= 0)
			{
				wbuf[0] = 0x0A;
				if (cmd == RTC_PIE_OFF) {
					stpio_disable_irq(m41st85y.sqwpio);
					wbuf[1] = (rbuf[0] & ~0x40);
				} else {
					stpio_enable_irq(m41st85y.sqwpio,
							 M41ST85Y_SQW_LEVEL);
					wbuf[1] = (rbuf[0] | 0x40);
				}

				DPRINTK("writing on SWQE %#x\n", wbuf[1]);
				if (m41st85y_transfer(&m41st85y,
						      wbuf, 2, M41ST85Y_WR,
						      M41ST85Y_INVALID) >= 0)
					return 0;
			}
			return -EIO;
		}
	case RTC_UIE_ON:	/* Allow ints for RTC updates. (one per second) */
		{
			stpio_enable_irq(m41st85y.irqpio, M41ST85Y_IRQ_LEVEL);
			return m41st85y_alarmset(dev, cmd, &ltime);
		}
	case RTC_ALM_READ:	/* Read the present alarm time */
		{
			struct rtc_wkalrm alarm_read;

			if ((err = m41st85y_read_alarm(NULL, &alarm_read)) >= 0)
				return copy_to_user((void __user *)arg,
						    &alarm_read.time,
						    sizeof alarm_read.time) ?
				    -EFAULT : 0;
			return err;
		}
	case RTC_ALM_SET:	/* Store a time into the alarm */
		{
			if (copy_from_user
			    (&ltime, (struct rtc_time __user *)arg,
			     sizeof ltime))
				return -EFAULT;
			return m41st85y_alarmset(dev, cmd, &ltime);
		}
	case RTC_RD_TIME:	/* Read the time/date from RTC  */
		{
			m41st85y_read_time(dev, &ltime);
			return copy_to_user((void __user *)arg,
					    &ltime, sizeof ltime) ? -EFAULT : 0;
		}
	case RTC_SET_TIME:	/* Set the RTC */
		{
			if (copy_from_user
			    (&ltime, (struct rtc_time __user *)arg,
			     sizeof ltime))
				return -EFAULT;

			return m41st85y_set_time(dev, &ltime);
		}
	case RTC_IRQP_READ:	/* Read the periodic IRQ rate.  */
		{
			return put_user(m41st85y.rtc->irq_freq,
					(unsigned long __user *)arg);
		}
	case RTC_IRQP_SET:	/* Set periodic IRQ rate.       */
		{
			wbuf[0] = 0x13;
			switch (arg) {
			case 0:
				wbuf[1] = 0x00;
				break;
			case 1:
				wbuf[1] = 0xF0;
				break;
			case 2:
				wbuf[1] = 0xE0;
				break;
			case 4:
				wbuf[1] = 0xD0;
				break;
			case 8:
				wbuf[1] = 0xC0;
				break;
			case 16:
				wbuf[1] = 0xB0;
				break;
			case 32:
				wbuf[1] = 0xA0;
				break;
			case 64:
				wbuf[1] = 0x90;
				break;
			case 128:
				wbuf[1] = 0x80;
				break;
			case 256:
				wbuf[1] = 0x70;
				break;
			case 512:
				wbuf[1] = 0x60;
				break;
			case 1024:
				wbuf[1] = 0x50;
				break;
			case 2048:
				wbuf[1] = 0x40;
				break;
			case 4096:
				wbuf[1] = 0x30;
				break;
			case 8192:
				wbuf[1] = 0x20;
				break;
			default:
				return -ENOTSUPP;
			}

			if ((err = m41st85y_transfer(&m41st85y,
						     wbuf, 2, M41ST85Y_WR,
						     M41ST85Y_INVALID)) >= 0) {
				m41st85y.rtc->irq_freq = arg;
				return 0;
			}

			return err;
		}
	case RTC_EPOCH_READ:	/* Read the epoch.      */
		{
			return put_user(m41st85y.epoch,
					(unsigned long __user *)arg);
		}
	case RTC_EPOCH_SET:	/* Set the epoch.       */
		{
			copy_from_user(&m41st85y.epoch, (void *)arg,
				       sizeof(long));
			return 0;
		}
	default:
		return -ENOTTY;
	}
}

static int m41st85y_read_callback(struct device *dev, int data)
{
	if (data & RTC_IRQF) {
		if ((m41st85y.cmd == RTC_AIE_ON)
		    || (m41st85y.cmd == RTC_UIE_ON)) {
			while (1) {
				m41st85y_transfer(&m41st85y,
						  rbuf, 1, M41ST85Y_RD, 0x0F);

				DPRINTK("AF 0x%x\n", rbuf[0]);
				if ((rbuf[0] & 0x40) == 0x00)
					break;
			}
		}
	}
	return data;
}

static int m41st85y_irq_set_state(struct device *dev, int enabled)
{
	if (enabled)
		return m41st85y_ioctl(dev, RTC_PIE_ON, 0);
	else
		return m41st85y_ioctl(dev, RTC_PIE_OFF, 0);
}

static int m41st85y_irq_set_freq(struct device *dev, int freq)
{
	return m41st85y_ioctl(dev, RTC_IRQP_SET, freq);
}

static struct rtc_class_ops m41st85y_rtc_ops = {
	.open = m41st85y_open,
	.release = m41st85y_release,
	.ioctl = m41st85y_ioctl,
	.read_time = m41st85y_read_time,
	.set_time = m41st85y_set_time,
	.read_alarm = m41st85y_read_alarm,
	.set_alarm = m41st85y_set_alarm,
	.irq_set_state = m41st85y_irq_set_state,
	.irq_set_freq = m41st85y_irq_set_freq,
	.read_callback = m41st85y_read_callback,

};

static int m41st85y_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "m41st85y: functionality I2C unsupported\n");
		return -ENODEV;
	}
	if (m41st85y_power_up() >= 0) {

		m41st85y.rtc =
		    rtc_device_register(M41ST85Y_NAME,
					&client->dev, &m41st85y_rtc_ops,
					THIS_MODULE);
		if (IS_ERR(m41st85y.rtc)) {
			printk(KERN_ERR
			       "m41st85y: RTC dev register failure.\n");
			return PTR_ERR(m41st85y.rtc);
		}

		m41st85y.adapter = client->adapter;
		m41st85y.cmd = 0;	/* none */
		m41st85y.epoch = 1900;	/* default value on Linux */
		m41st85y.rtc->irq_freq = 1;	/* default square ware 1Hz */
		printk(KERN_INFO
		       "m41st85y: IRQ line plugged on PIO%d[%d]\n",
		       m41st85y_get_pioport(irqpio),
		       m41st85y_get_piopin(irqpio));
		printk(KERN_INFO
		       "m41st85y: SQW line plugged on PIO%d[%d]\n",
		       m41st85y_get_pioport(sqwpio),
		       m41st85y_get_piopin(sqwpio));

		if ((m41st85y.irqpio =
		     stpio_request_pin(m41st85y_get_pioport(irqpio),
				       m41st85y_get_piopin(irqpio),
				       M41ST85Y_NAME,
				       STPIO_BIDIR_Z1)) != NULL) {
			if ((m41st85y.sqwpio =
			     stpio_request_pin(m41st85y_get_pioport(sqwpio),
					       m41st85y_get_piopin(sqwpio),
					       M41ST85Y_NAME,
					       STPIO_IN)) != NULL) {
				stpio_flagged_request_irq(m41st85y.irqpio,
							  M41ST85Y_IRQ_LEVEL,
							  m41st85y_handler,
							  (void *)&m41st85y, 0);
				stpio_flagged_request_irq(m41st85y.sqwpio,
							  M41ST85Y_SQW_LEVEL,
							  m41st85y_handler,
							  (void *)&m41st85y, 0);
				i2c_set_clientdata(client, &m41st85y);
				printk(KERN_INFO
				       "m41st85y: STMicroelectronics M41ST85Y RTC Driver registered\n");
				return 0;
			} else
				stpio_free_pin(m41st85y.irqpio);
		}
		printk(KERN_ERR
		       "m41st85y: STMicroelectronics M41ST85Y RTC Driver unregistered\n");
	}
	return -EIO;
}

static int m41st85y_remove(struct i2c_client *client)
{
	stpio_free_irq(m41st85y.irqpio);
	stpio_free_irq(m41st85y.sqwpio);
	rtc_device_unregister(m41st85y.rtc);
	return 0;
}

static struct i2c_device_id m41st85y_id[] = {
	{M41ST85Y_NAME, 0},
	{}
};

static struct i2c_driver m41st85y_driver = {
	.driver.name = M41ST85Y_NAME,
	.probe = m41st85y_probe,
	.remove = m41st85y_remove,
	.id_table = m41st85y_id
};

static __init int m41st85y_init(void)
{
	return i2c_add_driver(&m41st85y_driver);
}

static __exit void m41st85y_exit(void)
{
	i2c_del_driver(&m41st85y_driver);
}

EXPORT_SYMBOL(rtc_register);
EXPORT_SYMBOL(rtc_unregister);
EXPORT_SYMBOL(rtc_control);

int rtc_register(rtc_task_t * task)
{
	if (task == NULL || task->func == NULL)
		return -EINVAL;
	spin_lock_irq(&m41st85y.rtc->irq_lock);
	if (m41st85y.status & M41ST85Y_ISOPEN) {
		spin_unlock_irq(&m41st85y.rtc->irq_lock);
		return -EBUSY;
	}
	spin_lock(&m41st85y.rtc->irq_task_lock);
	if (m41st85y.rtc->irq_task) {
		spin_unlock(&m41st85y.rtc->irq_task_lock);
		spin_unlock_irq(&m41st85y.rtc->irq_lock);
		return -EBUSY;
	}

	m41st85y.status |= M41ST85Y_ISOPEN;
	m41st85y.rtc->irq_task = task;
	spin_unlock(&m41st85y.rtc->irq_task_lock);
	spin_unlock_irq(&m41st85y.rtc->irq_lock);
	return 0;
}

int rtc_control(rtc_task_t * task, unsigned int cmd, unsigned long arg)
{
	spin_lock_irq(&m41st85y.rtc->irq_task_lock);
	if (m41st85y.rtc->irq_task != task) {
		spin_unlock_irq(&m41st85y.rtc->irq_task_lock);
		return -ENXIO;
	}
	spin_unlock_irq(&m41st85y.rtc->irq_task_lock);
	return m41st85y_ioctl(NULL, cmd, arg);
}

int rtc_unregister(rtc_task_t * task)
{
	spin_lock_irq(&m41st85y.rtc->irq_lock);
	spin_lock(&m41st85y.rtc->irq_task_lock);

	if (m41st85y.rtc->irq_task != task) {
		spin_unlock(&m41st85y.rtc->irq_task_lock);
		spin_unlock_irq(&m41st85y.rtc->irq_lock);
		return -ENXIO;
	}
	m41st85y.rtc->irq_task = NULL;

	/* diasbilng the RTC's AIE, UIE and PIE control */
	if (m41st85y_transfer(&m41st85y, rbuf, 1, M41ST85Y_RD, 0x0A) >= 0) {
		wbuf[0] = 0x0A;
		wbuf[1] = rbuf[0] & ~0xC0;
		if (m41st85y_transfer(&m41st85y,
				      wbuf, 2, M41ST85Y_WR,
				      M41ST85Y_INVALID) >= 0) {
			m41st85y.status &= ~M41ST85Y_ISOPEN;
			spin_unlock(&m41st85y.rtc->irq_task_lock);
			spin_unlock_irq(&m41st85y.rtc->irq_lock);
			return 0;
		}
	}

	spin_unlock(&m41st85y.rtc->irq_task_lock);
	spin_unlock_irq(&m41st85y.rtc->irq_lock);
	return -EIO;
}

module_param(busid, uint, 0644);
module_param(irqpio, uint, 0644);
module_param(sqwpio, uint, 0644);
module_init(m41st85y_init);
module_exit(m41st85y_exit);
MODULE_AUTHOR("angelo castello <angelo.castello@st.com>");
MODULE_PARM_DESC(busid, "I2C bus ID");
MODULE_PARM_DESC(irqpio, "PIO port/pin for RTC-IRQ line");
MODULE_PARM_DESC(busid, "PIO port/pin for RTC-SWQ line");
MODULE_DESCRIPTION("External RTC upon I2C");
MODULE_LICENSE("GPL");
