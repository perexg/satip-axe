/*
 * LIRC base driver
 *
 * (L) by Artur Lipowski <alipowski@interia.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: lirc_dev.c,v 1.93 2009/03/29 10:55:31 lirc Exp $
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 18)
#error "**********************************************************"
#error " Sorry, this driver needs kernel version 2.2.18 or higher "
#error "**********************************************************"
#endif

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/smp_lock.h>
#include <linux/completion.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <asm/uaccess.h>
#include <asm/errno.h>
#else
#include <linux/uaccess.h>
#include <linux/errno.h>
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#include <linux/wrapper.h>
#endif
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#include <linux/kthread.h>
#endif
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "kcompat.h"

/* SysFS header */
#if defined(LIRC_HAVE_SYSFS)
#include <linux/device.h>
#endif

#include <linux/lirc.h>
#include "lirc_dev.h"

static int debug;
#define dprintk(fmt, args...)					\
	do {							\
		if (debug)					\
			printk(KERN_DEBUG fmt, ## args);	\
	} while (0)

#define IRCTL_DEV_NAME    "lirc"
#define IRCTL_DEV_MAJOR   61
#define NOPLUG            -1
#define LOGHEAD           "lirc_dev (%s[%d]): "

struct irctl {
	struct lirc_driver d;
	int attached;
	int open;

	struct mutex buffer_lock;
	struct lirc_buffer *buf;
	unsigned int chunk_size;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	int tpid;
	struct completion *t_notify;
	struct completion *t_notify2;
	int shutdown;
#else
	struct task_struct *task;
#endif
	long jiffies_to_wait;

#ifdef LIRC_HAVE_DEVFS_24
	devfs_handle_t devfs_handle;
#endif
};

static DEFINE_MUTEX(driver_lock);

static struct irctl *irctls[MAX_IRCTL_DEVICES];
static struct file_operations fops;

/* Only used for sysfs but defined to void otherwise */
static lirc_class_t *lirc_class;

/*  helper function
 *  initializes the irctl structure
 */
static void init_irctl(struct irctl *ir)
{
	mutex_init(&ir->buffer_lock);
	ir->d.minor = NOPLUG;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	ir->tpid = -1;
#endif
}

static void cleanup(struct irctl *ir)
{
	dprintk(LOGHEAD "cleaning up\n", ir->d.name, ir->d.minor);

	if (ir->buf != ir->d.rbuf) {
		lirc_buffer_free(ir->buf);
		kfree(ir->buf);
	}
	ir->buf = NULL;
}

/*  helper function
 *  reads key codes from driver and puts them into buffer
 *  returns 0 on success
 */
static int add_to_buf(struct irctl *ir)
{
	if (ir->d.add_to_buf) {
		int res = -ENODATA;
		int got_data = 0;

		/*
		 * service the device as long as it is returning
		 * data and we have space
		 */
		while ((res = ir->d.add_to_buf(ir->d.data, ir->buf)) == 0) {
			got_data++;
		}

		if (res == -ENODEV)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
			ir->shutdown = 1;
#else
			kthread_stop(ir->task);
#endif

		return got_data ? 0 : res;
	}

	return 0;
}

/* main function of the polling thread */
static int lirc_thread(void *irctl)
{
	struct irctl *ir = irctl;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	/*
	 * This thread doesn't need any user-level access, so get rid
	 * of all our resources
	 */
	daemonize("lirc_dev");

	if (ir->t_notify != NULL)
		complete(ir->t_notify);

#endif
	dprintk(LOGHEAD "poll thread started\n", ir->d.name, ir->d.minor);

	do {
		if (ir->open) {
			if (ir->jiffies_to_wait) {
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(ir->jiffies_to_wait);
#ifndef LIRC_REMOVE_DURING_EXPORT
			} else {
				interruptible_sleep_on(
					ir->d.get_queue(ir->d.data));
#endif
			}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
			if (ir->shutdown)
#else
			if (kthread_should_stop())
#endif
				break;
			if (!add_to_buf(ir))
				wake_up_interruptible(&ir->buf->wait_poll);
		} else {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	} while (!ir->shutdown);

	if (ir->t_notify2 != NULL)
		wait_for_completion(ir->t_notify2);

	ir->tpid = -1;
	if (ir->t_notify != NULL)
		complete(ir->t_notify);
#else
	} while (!kthread_should_stop());
#endif

	dprintk(LOGHEAD "poll thread ended\n", ir->d.name, ir->d.minor);

	return 0;
}

int lirc_register_driver(struct lirc_driver *d)
{
	struct irctl *ir;
	int minor;
	int bytes_in_key;
	unsigned int buffer_size;
	int err;
#ifdef LIRC_HAVE_DEVFS_24
	char name[16];
#endif
	DECLARE_COMPLETION(tn);

	if (!d) {
		printk(KERN_ERR "lirc_dev: lirc_register_driver: "
		       "driver pointer must be not NULL!\n");
		err = -EBADRQC;
		goto out;
	}

	if (MAX_IRCTL_DEVICES <= d->minor) {
		printk(KERN_ERR "lirc_dev: lirc_register_driver: "
		       "\"minor\" must be between 0 and %d (%d)!\n",
		       MAX_IRCTL_DEVICES-1, d->minor);
		err = -EBADRQC;
		goto out;
	}

	if (1 > d->code_length || (BUFLEN * 8) < d->code_length) {
		printk(KERN_ERR "lirc_dev: lirc_register_driver: "
		       "code length in bits for minor (%d) "
		       "must be less than %d!\n",
		       d->minor, BUFLEN * 8);
		err = -EBADRQC;
		goto out;
	}

	printk(KERN_INFO "lirc_dev: lirc_register_driver: sample_rate: %d\n",
		d->sample_rate);
	if (d->sample_rate) {
		if (2 > d->sample_rate || HZ < d->sample_rate) {
			printk(KERN_ERR "lirc_dev: lirc_register_driver: "
			       "sample_rate must be between 2 and %d!\n", HZ);
			err = -EBADRQC;
			goto out;
		}
		if (!d->add_to_buf) {
			printk(KERN_ERR "lirc_dev: lirc_register_driver: "
			       "add_to_buf cannot be NULL when "
			       "sample_rate is set\n");
			err = -EBADRQC;
			goto out;
		}
#ifndef LIRC_REMOVE_DURING_EXPORT
	} else if (!(d->fops && d->fops->read) && !d->get_queue && !d->rbuf) {
#else
	} else if (!(d->fops && d->fops->read) && !d->rbuf) {
#endif
		printk(KERN_ERR "lirc_dev: lirc_register_driver: "
#ifndef LIRC_REMOVE_DURING_EXPORT
		       "fops->read, get_queue and rbuf "
#else
		       "fops->read and rbuf "
#endif
		       "cannot all be NULL!\n");
		err = -EBADRQC;
		goto out;
#ifndef LIRC_REMOVE_DURING_EXPORT
	} else if (!d->get_queue && !d->rbuf) {
#else
	} else if (!d->rbuf) {
#endif
		if (!(d->fops && d->fops->read && d->fops->poll &&
		      d->fops->ioctl)) {
			printk(KERN_ERR "lirc_dev: lirc_register_driver: "
			       "neither read, poll nor ioctl can be NULL!\n");
			err = -EBADRQC;
			goto out;
		}
	}
#ifdef MODULE
	if (d->owner == NULL) {
		printk(KERN_ERR "lirc_dev: lirc_register_driver: "
				    "no module owner registered\n");
		err = -EBADRQC;
		goto out;
	}
#endif
	mutex_lock(&driver_lock);

	minor = d->minor;

	if (minor < 0) {
		/* find first free slot for driver */
		for (minor = 0; minor < MAX_IRCTL_DEVICES; minor++)
			if (!irctls[minor])
				break;
		if (MAX_IRCTL_DEVICES == minor) {
			printk(KERN_ERR "lirc_dev: lirc_register_driver: "
			       "no free slots for drivers!\n");
			err = -ENOMEM;
			goto out_lock;
		}
	} else if (irctls[minor]) {
		printk(KERN_ERR "lirc_dev: lirc_register_driver: "
		       "minor (%d) just registered!\n", minor);
		err = -EBUSY;
		goto out_lock;
	}

	ir = kzalloc(sizeof(struct irctl), GFP_KERNEL);
	if (!ir) {
		err = -ENOMEM;
		goto out_lock;
	}
	init_irctl(ir);
	irctls[minor] = ir;

	if (d->sample_rate) {
		ir->jiffies_to_wait = HZ / d->sample_rate;
	} else {
		/* it means - wait for external event in task queue */
		ir->jiffies_to_wait = 0;
	}

	/* some safety check 8-) */
	d->name[sizeof(d->name)-1] = '\0';

	bytes_in_key = d->code_length/8 + (d->code_length%8 ? 1 : 0);
	buffer_size = d->buffer_size ? d->buffer_size : BUFLEN / bytes_in_key;

	if (d->rbuf) {
		ir->buf = d->rbuf;
	} else {
		ir->buf = kmalloc(sizeof(struct lirc_buffer), GFP_KERNEL);
		if (!ir->buf) {
			err = -ENOMEM;
			goto out_lock;
		}
		err = lirc_buffer_init(ir->buf, bytes_in_key, buffer_size);
		if (err) {
			kfree(ir->buf);
			goto out_lock;
		}
	}
	ir->chunk_size = ir->buf->chunk_size;

	if (d->features == 0)
		d->features = (d->code_length > 8) ?
			LIRC_CAN_REC_LIRCCODE : LIRC_CAN_REC_CODE;

	ir->d = *d;
	ir->d.minor = minor;

#if defined(LIRC_HAVE_DEVFS_24)
	sprintf(name, DEV_LIRC "/%d", ir->d.minor);
	ir->devfs_handle = devfs_register(NULL, name, DEVFS_FL_DEFAULT,
					  IRCTL_DEV_MAJOR, ir->d.minor,
					  S_IFCHR | S_IRUSR | S_IWUSR,
					  &fops, NULL);
#elif defined(LIRC_HAVE_DEVFS_26)
	devfs_mk_cdev(MKDEV(IRCTL_DEV_MAJOR, ir->d.minor),
			S_IFCHR|S_IRUSR|S_IWUSR,
			DEV_LIRC "/%u", ir->d.minor);
#endif
	(void) lirc_device_create(lirc_class, ir->d.dev,
				  MKDEV(IRCTL_DEV_MAJOR, ir->d.minor), NULL,
				  "lirc%u", ir->d.minor);

#ifndef LIRC_REMOVE_DURING_EXPORT
	if (d->sample_rate || d->get_queue) {
#else
	if (d->sample_rate) {
#endif
		/* try to fire up polling thread */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
		ir->t_notify = &tn;
		ir->tpid = kernel_thread(lirc_thread, (void *)ir, 0);
		if (ir->tpid < 0) {
#else
		ir->task = kthread_run(lirc_thread, (void *)ir, "lirc_dev");
		if (IS_ERR(ir->task)) {
#endif
			printk(KERN_ERR "lirc_dev: lirc_register_driver: "
			       "cannot run poll thread for minor = %d\n",
			       d->minor);
			err = -ECHILD;
			goto out_sysfs;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
		wait_for_completion(&tn);
		ir->t_notify = NULL;
#endif
	}
	ir->attached = 1;
	mutex_unlock(&driver_lock);

/*
 * Recent kernels should handle this autmatically by increasing/decreasing
 * use count when a dependant module is loaded/unloaded.
 */
#ifndef KERNEL_2_5
	MOD_INC_USE_COUNT;
#endif
	dprintk("lirc_dev: driver %s registered at minor number = %d\n",
		ir->d.name, ir->d.minor);
	d->minor = minor;
	return minor;

out_sysfs:
	lirc_device_destroy(lirc_class,
			    MKDEV(IRCTL_DEV_MAJOR, ir->d.minor));
#ifdef LIRC_HAVE_DEVFS_24
	devfs_unregister(ir->devfs_handle);
#endif
#ifdef LIRC_HAVE_DEVFS_26
	devfs_remove(DEV_LIRC "/%i", ir->d.minor);
#endif
out_lock:
	mutex_unlock(&driver_lock);
out:
	return err;
}
EXPORT_SYMBOL(lirc_register_driver);

int lirc_unregister_driver(int minor)
{
	struct irctl *ir;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	DECLARE_COMPLETION(tn);
	DECLARE_COMPLETION(tn2);
#endif

	if (minor < 0 || minor >= MAX_IRCTL_DEVICES) {
		printk(KERN_ERR "lirc_dev: lirc_unregister_driver: "
		       "\"minor\" must be between 0 and %d!\n",
		       MAX_IRCTL_DEVICES-1);
		return -EBADRQC;
	}

	ir = irctls[minor];

	mutex_lock(&driver_lock);

	if (ir->d.minor != minor) {
		printk(KERN_ERR "lirc_dev: lirc_unregister_driver: "
		       "minor (%d) device not registered!", minor);
		mutex_unlock(&driver_lock);
		return -ENOENT;
	}

	/* end up polling thread */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	if (ir->tpid >= 0) {
		ir->t_notify = &tn;
		ir->t_notify2 = &tn2;
		ir->shutdown = 1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
		{
			struct task_struct *p;

			p = find_task_by_pid(ir->tpid);
			wake_up_process(p);
		}
#else
		/* 2.2.x does not export wake_up_process() */
		wake_up_interruptible(ir->d.get_queue(ir->d.data));
#endif
		complete(&tn2);
		wait_for_completion(&tn);
		ir->t_notify = NULL;
		ir->t_notify2 = NULL;
	}
#else /* kernel >= 2.6.23 */
	if (ir->task)
		kthread_stop(ir->task);
#endif

	dprintk("lirc_dev: driver %s unregistered from minor number = %d\n",
		ir->d.name, ir->d.minor);

	ir->attached = 0;
	if (ir->open) {
		dprintk(LOGHEAD "releasing opened driver\n",
			ir->d.name, ir->d.minor);
		wake_up_interruptible(&ir->buf->wait_poll);
		mutex_lock(&ir->buffer_lock);
		ir->d.set_use_dec(ir->d.data);
		module_put(ir->d.owner);
		mutex_unlock(&ir->buffer_lock);
	} else {
		cleanup(ir);
		irctls[minor] = NULL;
		kfree(ir);
	}

#ifdef LIRC_HAVE_DEVFS_24
	devfs_unregister(ir->devfs_handle);
#endif
#ifdef LIRC_HAVE_DEVFS_26
	devfs_remove(DEV_LIRC "/%u", ir->d.minor);
#endif
	lirc_device_destroy(lirc_class,
			    MKDEV(IRCTL_DEV_MAJOR, ir->d.minor));

	mutex_unlock(&driver_lock);

/*
 * Recent kernels should handle this autmatically by increasing/decreasing
 * use count when a dependant module is loaded/unloaded.
 */
#ifndef KERNEL_2_5
	MOD_DEC_USE_COUNT;
#endif

	return 0;
}
EXPORT_SYMBOL(lirc_unregister_driver);

static int irctl_open(struct inode *inode, struct file *file)
{
	struct irctl *ir;
	int retval;

	if (iminor(inode) >= MAX_IRCTL_DEVICES || !irctls[iminor(inode)]) {
		dprintk("lirc_dev [%d]: open result = -ENODEV\n",
			iminor(inode));
		return -ENODEV;
	}

	ir = irctls[iminor(inode)];

	dprintk(LOGHEAD "open called\n", ir->d.name, ir->d.minor);

	/* if the driver has an open function use it instead */
	if (ir->d.fops && ir->d.fops->open)
		return ir->d.fops->open(inode, file);

	if (mutex_lock_interruptible(&driver_lock))
		return -ERESTARTSYS;

	if (ir->d.minor == NOPLUG) {
		retval = -ENODEV;
		goto error;
	}

	if (ir->open) {
		retval = -EBUSY;
		goto error;
	}
#ifdef MODULE
	if (ir->d.owner != NULL && try_module_get(ir->d.owner)) {
#endif
		++ir->open;
		retval = ir->d.set_use_inc(ir->d.data);

		if (retval) {
			module_put(ir->d.owner);
			--ir->open;
		} else {
			lirc_buffer_clear(ir->buf);
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
		if (ir->tpid >= 0) {
			struct task_struct *p;

			p = find_task_by_pid(ir->tpid);
			wake_up_process(p);
		}
#else
		if (ir->task)
			wake_up_process(ir->task);
#endif
#ifdef MODULE
	} else {
		if (ir->d.owner == NULL)
			dprintk(LOGHEAD "no module owner!!!\n",
				ir->d.name, ir->d.minor);

		retval = -ENODEV;
	}
#endif
 error:
	if (ir)
		dprintk(LOGHEAD "open result = %d\n", ir->d.name, ir->d.minor,
			retval);

	mutex_unlock(&driver_lock);

	return retval;
}

static int irctl_close(struct inode *inode, struct file *file)
{
	struct irctl *ir = irctls[iminor(inode)];

	dprintk(LOGHEAD "close called\n", ir->d.name, ir->d.minor);

	/* if the driver has a close function use it instead */
	if (ir->d.fops && ir->d.fops->release)
		return ir->d.fops->release(inode, file);

	if (mutex_lock_interruptible(&driver_lock))
		return -ERESTARTSYS;

	--ir->open;
	if (ir->attached) {
		ir->d.set_use_dec(ir->d.data);
		module_put(ir->d.owner);
	} else {
		cleanup(ir);
		irctls[ir->d.minor] = NULL;
		kfree(ir);
	}

	mutex_unlock(&driver_lock);

	return 0;
}

static unsigned int irctl_poll(struct file *file, poll_table *wait)
{
	struct irctl *ir = irctls[iminor(file->f_dentry->d_inode)];
	unsigned int ret;

	dprintk(LOGHEAD "poll called\n", ir->d.name, ir->d.minor);

	/* if the driver has a poll function use it instead */
	if (ir->d.fops && ir->d.fops->poll)
		return ir->d.fops->poll(file, wait);

	mutex_lock(&ir->buffer_lock);
	if (!ir->attached) {
		mutex_unlock(&ir->buffer_lock);
		return POLLERR;
	}

	poll_wait(file, &ir->buf->wait_poll, wait);

	dprintk(LOGHEAD "poll result = %s\n",
		ir->d.name, ir->d.minor,
		lirc_buffer_empty(ir->buf) ? "0" : "POLLIN|POLLRDNORM");

	ret = lirc_buffer_empty(ir->buf) ? 0 : (POLLIN|POLLRDNORM);

	mutex_unlock(&ir->buffer_lock);
	return ret;
}

/*
 *
 */
static int irctl_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	unsigned long mode;
	int result;
	struct irctl *ir = irctls[iminor(inode)];

	dprintk(LOGHEAD "ioctl called (0x%x)\n",
		ir->d.name, ir->d.minor, cmd);

	/* if the driver has a ioctl function use it instead */
	if (ir->d.fops && ir->d.fops->ioctl) {
		result = ir->d.fops->ioctl(inode, file, cmd, arg);
		if (result != -ENOIOCTLCMD)
			return result;
	}

	if (ir->d.minor == NOPLUG || !ir->attached) {
		dprintk(LOGHEAD "ioctl result = -ENODEV\n",
			ir->d.name, ir->d.minor);
		return -ENODEV;
	}

	/* The driver can't handle cmd */
	result = 0;

	switch (cmd) {
	case LIRC_GET_FEATURES:
		result = put_user(ir->d.features, (unsigned long *)arg);
		break;
	case LIRC_GET_REC_MODE:
		if (!(ir->d.features & LIRC_CAN_REC_MASK))
			return -ENOSYS;

		result = put_user(LIRC_REC2MODE
				  (ir->d.features & LIRC_CAN_REC_MASK),
				  (unsigned long *)arg);
		break;
	case LIRC_SET_REC_MODE:
		if (!(ir->d.features & LIRC_CAN_REC_MASK))
			return -ENOSYS;

		result = get_user(mode, (unsigned long *)arg);
		if (!result && !(LIRC_MODE2REC(mode) & ir->d.features))
			result = -EINVAL;
		/*
		 * FIXME: We should actually set the mode somehow but
		 * for now, lirc_serial doesn't support mode changing either
		 */
		break;
	case LIRC_GET_LENGTH:
		result = put_user(ir->d.code_length, (unsigned long *) arg);
		break;
	default:
		result = -EINVAL;
	}

	dprintk(LOGHEAD "ioctl result = %d\n",
		ir->d.name, ir->d.minor, result);

	return result;
}

#ifdef CONFIG_COMPAT
#define LIRC_GET_FEATURES_COMPAT32     _IOR('i', 0x00000000, __u32)

#define LIRC_GET_SEND_MODE_COMPAT32    _IOR('i', 0x00000001, __u32)
#define LIRC_GET_REC_MODE_COMPAT32     _IOR('i', 0x00000002, __u32)

#define LIRC_GET_LENGTH_COMPAT32       _IOR('i', 0x0000000f, __u32)

#define LIRC_SET_SEND_MODE_COMPAT32    _IOW('i', 0x00000011, __u32)
#define LIRC_SET_REC_MODE_COMPAT32     _IOW('i', 0x00000012, __u32)

static long irctl_compat_ioctl(struct file *file,
			       unsigned int cmd32,
			       unsigned long arg)
{
	mm_segment_t old_fs;
	int ret;
	unsigned long val;
	unsigned int cmd;

	switch (cmd32) {
	case LIRC_GET_FEATURES_COMPAT32:
	case LIRC_GET_SEND_MODE_COMPAT32:
	case LIRC_GET_REC_MODE_COMPAT32:
	case LIRC_GET_LENGTH_COMPAT32:
	case LIRC_SET_SEND_MODE_COMPAT32:
	case LIRC_SET_REC_MODE_COMPAT32:
		/*
		 * These commands expect (unsigned long *) arg
		 * but the 32-bit app supplied (__u32 *).
		 * Conversion is required.
		 */
		if (get_user(val, (__u32 *)compat_ptr(arg)))
			return -EFAULT;
		lock_kernel();
		/*
		 * tell irctl_ioctl that it's safe to use the pointer
		 * to val which is in kernel address space and not in
		 * user address space
		 */
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		cmd = _IOC(_IOC_DIR(cmd32), _IOC_TYPE(cmd32), _IOC_NR(cmd32),
			   (_IOC_TYPECHECK(unsigned long)));
		ret = irctl_ioctl(file->f_path.dentry->d_inode, file,
				  cmd, (unsigned long)(&val));

		set_fs(old_fs);
		unlock_kernel();
		switch (cmd) {
		case LIRC_GET_FEATURES:
		case LIRC_GET_SEND_MODE:
		case LIRC_GET_REC_MODE:
		case LIRC_GET_LENGTH:
			if (!ret && put_user(val, (__u32 *)compat_ptr(arg)))
				return -EFAULT;
			break;
		}
		return ret;

	case LIRC_GET_SEND_CARRIER:
	case LIRC_GET_REC_CARRIER:
	case LIRC_GET_SEND_DUTY_CYCLE:
	case LIRC_GET_REC_DUTY_CYCLE:
	case LIRC_GET_REC_RESOLUTION:
	case LIRC_SET_SEND_CARRIER:
	case LIRC_SET_REC_CARRIER:
	case LIRC_SET_SEND_DUTY_CYCLE:
	case LIRC_SET_REC_DUTY_CYCLE:
	case LIRC_SET_TRANSMITTER_MASK:
	case LIRC_SET_REC_DUTY_CYCLE_RANGE:
	case LIRC_SET_REC_CARRIER_RANGE:
		/*
		 * These commands expect (unsigned int *)arg
		 * so no problems here. Just handle the locking.
		 */
		lock_kernel();
		cmd = cmd32;
		ret = irctl_ioctl(file->f_path.dentry->d_inode,
				  file, cmd, arg);
		unlock_kernel();
		return ret;
	default:
		/* unknown */
		printk(KERN_ERR "lirc_dev: %s(%s:%d): Unknown cmd %08x\n",
		       __func__, current->comm, current->pid, cmd32);
		return -ENOIOCTLCMD;
	}
}
#endif

static ssize_t irctl_read(struct file *file,
			  char *buffer,
			  size_t length,
			  loff_t *ppos)
{
	struct irctl *ir = irctls[iminor(file->f_dentry->d_inode)];
	unsigned char buf[ir->chunk_size];
	int ret = 0, written = 0;
	int unlock = 1;
	DECLARE_WAITQUEUE(wait, current);

	dprintk(LOGHEAD "read called\n", ir->d.name, ir->d.minor);

	/* if the driver has a specific read function use it instead */
	if (ir->d.fops && ir->d.fops->read)
		return ir->d.fops->read(file, buffer, length, ppos);

	if (mutex_lock_interruptible(&ir->buffer_lock))
		return -ERESTARTSYS;
	if (!ir->attached) {
		mutex_unlock(&ir->buffer_lock);
		return -ENODEV;
	}

	if (length % ir->buf->chunk_size) {
		dprintk(LOGHEAD "read result = -EINVAL\n",
			ir->d.name, ir->d.minor);
		mutex_unlock(&ir->buffer_lock);
		return -EINVAL;
	}

	/*
	 * we add ourselves to the task queue before buffer check
	 * to avoid losing scan code (in case when queue is awaken somewhere
	 * between while condition checking and scheduling)
	 */
	add_wait_queue(&ir->buf->wait_poll, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	/*
	 * while we didn't provide 'length' bytes, device is opened in blocking
	 * mode and 'copy_to_user' is happy, wait for data.
	 */
	while (written < length && ret == 0) {
		if (lirc_buffer_empty(ir->buf)) {
			/* According to the read(2) man page, 'written' can be
			 * returned as less than 'length', instead of blocking
			 * again, returning -EWOULDBLOCK, or returning
			 * -ERESTARTSYS */
			if (written)
				break;
			if (file->f_flags & O_NONBLOCK) {
				ret = -EWOULDBLOCK;
				break;
			}
			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}

			mutex_unlock(&ir->buffer_lock);
			schedule();
			set_current_state(TASK_INTERRUPTIBLE);

			if (mutex_lock_interruptible(&ir->buffer_lock)) {
				unlock = 0;
				ret = -ERESTARTSYS;
				break;
			}

			if (!ir->attached) {
				ret = -ENODEV;
				break;
			}
		} else {
			lirc_buffer_read(ir->buf, buf);
			ret = copy_to_user((void *)buffer+written, buf,
					   ir->buf->chunk_size);
			written += ir->buf->chunk_size;
		}
	}

	remove_wait_queue(&ir->buf->wait_poll, &wait);
	set_current_state(TASK_RUNNING);
	if(unlock) mutex_unlock(&ir->buffer_lock);

	dprintk(LOGHEAD "read result = %s (%d)\n",
		ir->d.name, ir->d.minor, ret ? "-EFAULT" : "OK", ret);

	return ret ? ret : written;
}


void *lirc_get_pdata(struct file *file)
{
	void *data = NULL;

	if (file && file->f_dentry && file->f_dentry->d_inode &&
	    file->f_dentry->d_inode->i_rdev) {
		struct irctl *ir;
		ir = irctls[iminor(file->f_dentry->d_inode)];
		data = ir->d.data;
	}

	return data;
}
EXPORT_SYMBOL(lirc_get_pdata);


static ssize_t irctl_write(struct file *file, const char *buffer,
			   size_t length, loff_t *ppos)
{
	struct irctl *ir = irctls[iminor(file->f_dentry->d_inode)];

	dprintk(LOGHEAD "write called\n", ir->d.name, ir->d.minor);

	/* if the driver has a specific read function use it instead */
	if (ir->d.fops && ir->d.fops->write)
		return ir->d.fops->write(file, buffer, length, ppos);

	if (!ir->attached)
		return -ENODEV;

	return -EINVAL;
}


static struct file_operations fops = {
	.owner		= THIS_MODULE,
	.read		= irctl_read,
	.write		= irctl_write,
	.poll		= irctl_poll,
	.ioctl		= irctl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= irctl_compat_ioctl,
#endif
	.open		= irctl_open,
	.release	= irctl_close
};

static int __init lirc_dev_init(void)
{
	if (register_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME, &fops)) {
		printk(KERN_ERR "lirc_dev: register_chrdev failed\n");
		goto out;
	}

	lirc_class = class_create(THIS_MODULE, IRCTL_DEV_NAME);
	if (IS_ERR(lirc_class)) {
		printk(KERN_ERR "lirc_dev: class_create failed\n");
		goto out_unregister;
	}

	printk(KERN_INFO "lirc_dev: IR Remote Control driver registered, "
	       "major %d \n", IRCTL_DEV_MAJOR);

	return 0;

out_unregister:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	if (unregister_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME))
		printk(KERN_ERR "lirc_dev: unregister_chrdev failed!\n");
#else
	/* unregister_chrdev returns void now */
	unregister_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME);
#endif
out:
	return -1;
}

static void __exit lirc_dev_exit(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	int ret;

	ret = unregister_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME);
	class_destroy(lirc_class);

	if (ret)
		printk(KERN_ERR "lirc_dev: error in "
		       "module_unregister_chrdev: %d\n", ret);
	else
		dprintk("lirc_dev: module successfully unloaded\n");
#else
	/* unregister_chrdev returns void now */
	unregister_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME);
	class_destroy(lirc_class);
	dprintk("lirc_dev: module unloaded\n");
#endif
}

module_init(lirc_dev_init);
module_exit(lirc_dev_exit);

MODULE_DESCRIPTION("LIRC base driver module");
MODULE_AUTHOR("Artur Lipowski");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(IRCTL_DEV_MAJOR);

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable debugging messages");
