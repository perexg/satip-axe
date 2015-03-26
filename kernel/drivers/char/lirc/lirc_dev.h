/*
 * LIRC base driver
 *
 * (L) by Artur Lipowski <alipowski@interia.pl>
 *        This code is licensed under GNU GPL
 *
 * $Id: lirc_dev.h,v 1.37 2009/03/15 09:34:00 lirc Exp $
 *
 */

#ifndef _LINUX_LIRC_DEV_H
#define _LINUX_LIRC_DEV_H

#include <linux/version.h>
#define LIRC_REMOVE_DURING_EXPORT
#ifndef LIRC_REMOVE_DURING_EXPORT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
/* when was it really introduced? */
#define LIRC_HAVE_KFIFO
#endif
#endif
#define MAX_IRCTL_DEVICES 4
#define BUFLEN            16

#define mod(n, div) ((n) % (div))

#include <linux/slab.h>
#include <linux/fs.h>
#ifdef LIRC_HAVE_KFIFO
#include <linux/kfifo.h>
#endif

struct lirc_buffer {
	wait_queue_head_t wait_poll;
	spinlock_t lock;
	unsigned int chunk_size;
	unsigned int size; /* in chunks */
	/* Using chunks instead of bytes pretends to simplify boundary checking
	 * And should allow for some performance fine tunning later */
#ifdef LIRC_HAVE_KFIFO
	struct kfifo *fifo;
#else
	unsigned int fill; /* in chunks */
	int head, tail;    /* in chunks */
	unsigned char *data;
#endif
};
#ifndef LIRC_HAVE_KFIFO
static inline void lirc_buffer_lock(struct lirc_buffer *buf,
				    unsigned long *flags)
{
	spin_lock_irqsave(&buf->lock, *flags);
}
static inline void lirc_buffer_unlock(struct lirc_buffer *buf,
				      unsigned long *flags)
{
	spin_unlock_irqrestore(&buf->lock, *flags);
}
static inline void _lirc_buffer_clear(struct lirc_buffer *buf)
{
	buf->head = 0;
	buf->tail = 0;
	buf->fill = 0;
}
#endif
static inline void lirc_buffer_clear(struct lirc_buffer *buf)
{
#ifdef LIRC_HAVE_KFIFO
	if (buf->fifo)
		kfifo_reset(buf->fifo);
#else
	unsigned long flags;
	lirc_buffer_lock(buf, &flags);
	_lirc_buffer_clear(buf);
	lirc_buffer_unlock(buf, &flags);
#endif
}
static inline int lirc_buffer_init(struct lirc_buffer *buf,
				    unsigned int chunk_size,
				    unsigned int size)
{
	init_waitqueue_head(&buf->wait_poll);
	spin_lock_init(&buf->lock);
#ifndef LIRC_HAVE_KFIFO
	_lirc_buffer_clear(buf);
#endif
	buf->chunk_size = chunk_size;
	buf->size = size;
#ifdef LIRC_HAVE_KFIFO
	buf->fifo = kfifo_alloc(size*chunk_size, GFP_KERNEL, &buf->lock);
	if (!buf->fifo)
		return -ENOMEM;
#else
	buf->data = kmalloc(size*chunk_size, GFP_KERNEL);
	if (buf->data == NULL)
		return -ENOMEM;
	memset(buf->data, 0, size*chunk_size);
#endif
	return 0;
}
static inline void lirc_buffer_free(struct lirc_buffer *buf)
{
#ifdef LIRC_HAVE_KFIFO
	if (buf->fifo)
		kfifo_free(buf->fifo);
#else
	kfree(buf->data);
	buf->data = NULL;
	buf->head = 0;
	buf->tail = 0;
	buf->fill = 0;
	buf->chunk_size = 0;
	buf->size = 0;
#endif
}
#ifndef LIRC_HAVE_KFIFO
static inline int  _lirc_buffer_full(struct lirc_buffer *buf)
{
	return (buf->fill >= buf->size);
}
#endif
static inline int  lirc_buffer_full(struct lirc_buffer *buf)
{
#ifdef LIRC_HAVE_KFIFO
	return kfifo_len(buf->fifo) == buf->size * buf->chunk_size;
#else
	unsigned long flags;
	int ret;
	lirc_buffer_lock(buf, &flags);
	ret = _lirc_buffer_full(buf);
	lirc_buffer_unlock(buf, &flags);
	return ret;
#endif
}
#ifndef LIRC_HAVE_KFIFO
static inline int  _lirc_buffer_empty(struct lirc_buffer *buf)
{
	return !(buf->fill);
}
#endif
static inline int  lirc_buffer_empty(struct lirc_buffer *buf)
{
#ifdef LIRC_HAVE_KFIFO
	return !kfifo_len(buf->fifo);
#else
	unsigned long flags;
	int ret;
	lirc_buffer_lock(buf, &flags);
	ret = _lirc_buffer_empty(buf);
	lirc_buffer_unlock(buf, &flags);
	return ret;
#endif
}
#ifndef LIRC_HAVE_KFIFO
static inline int  _lirc_buffer_available(struct lirc_buffer *buf)
{
	return (buf->size - buf->fill);
}
#endif
static inline int  lirc_buffer_available(struct lirc_buffer *buf)
{
#ifdef LIRC_HAVE_KFIFO
	return buf->size - (kfifo_len(buf->fifo) / buf->chunk_size);
#else
	unsigned long flags;
	int ret;
	lirc_buffer_lock(buf, &flags);
	ret = _lirc_buffer_available(buf);
	lirc_buffer_unlock(buf, &flags);
	return ret;
#endif
}
#ifndef LIRC_HAVE_KFIFO
static inline void _lirc_buffer_read_1(struct lirc_buffer *buf,
				       unsigned char *dest)
{
	memcpy(dest, &buf->data[buf->head*buf->chunk_size], buf->chunk_size);
	buf->head = mod(buf->head+1, buf->size);
	buf->fill -= 1;
}
#endif
static inline void lirc_buffer_read(struct lirc_buffer *buf,
				    unsigned char *dest)
{
#ifdef LIRC_HAVE_KFIFO
	if (kfifo_len(buf->fifo) >= buf->chunk_size)
		kfifo_get(buf->fifo, dest, buf->chunk_size);
#else
	unsigned long flags;
	lirc_buffer_lock(buf, &flags);
	_lirc_buffer_read_1(buf, dest);
	lirc_buffer_unlock(buf, &flags);
#endif
}
#ifndef LIRC_HAVE_KFIFO
static inline void _lirc_buffer_write_1(struct lirc_buffer *buf,
				      unsigned char *orig)
{
	memcpy(&buf->data[buf->tail*buf->chunk_size], orig, buf->chunk_size);
	buf->tail = mod(buf->tail+1, buf->size);
	buf->fill++;
}
#endif
static inline void lirc_buffer_write(struct lirc_buffer *buf,
				     unsigned char *orig)
{
#ifdef LIRC_HAVE_KFIFO
	kfifo_put(buf->fifo, orig, buf->chunk_size);
#else
	unsigned long flags;
	lirc_buffer_lock(buf, &flags);
	_lirc_buffer_write_1(buf, orig);
	lirc_buffer_unlock(buf, &flags);
#endif
}
#ifndef LIRC_HAVE_KFIFO
static inline void _lirc_buffer_write_n(struct lirc_buffer *buf,
					unsigned char *orig, int count)
{
	int space1;
	if (buf->head > buf->tail)
		space1 = buf->head - buf->tail;
	else
		space1 = buf->size - buf->tail;

	if (count > space1) {
		memcpy(&buf->data[buf->tail * buf->chunk_size], orig,
		       space1 * buf->chunk_size);
		memcpy(&buf->data[0], orig + (space1 * buf->chunk_size),
		       (count - space1) * buf->chunk_size);
	} else {
		memcpy(&buf->data[buf->tail * buf->chunk_size], orig,
		       count * buf->chunk_size);
	}
	buf->tail = mod(buf->tail + count, buf->size);
	buf->fill += count;
}
#endif
static inline void lirc_buffer_write_n(struct lirc_buffer *buf,
				       unsigned char *orig, int count)
{
#ifdef LIRC_HAVE_KFIFO
	kfifo_put(buf->fifo, orig, count * buf->chunk_size);
#else
	unsigned long flags;
	lirc_buffer_lock(buf, &flags);
	_lirc_buffer_write_n(buf, orig, count);
	lirc_buffer_unlock(buf, &flags);
#endif
}

struct lirc_driver {
	char name[40];
	int minor;
	unsigned long code_length;
	unsigned int buffer_size; /* in chunks holding one code each */
	int sample_rate;
	unsigned long features;
	void *data;
	int (*add_to_buf) (void *data, struct lirc_buffer *buf);
#ifndef LIRC_REMOVE_DURING_EXPORT
	wait_queue_head_t* (*get_queue) (void *data);
#endif
	struct lirc_buffer *rbuf;
	int (*set_use_inc) (void *data);
	void (*set_use_dec) (void *data);
	struct file_operations *fops;
	struct device *dev;
	struct module *owner;
};
/* name:
 * this string will be used for logs
 *
 * minor:
 * indicates minor device (/dev/lirc) number for registered driver
 * if caller fills it with negative value, then the first free minor
 * number will be used (if available)
 *
 * code_length:
 * length of the remote control key code expressed in bits
 *
 * sample_rate:
 * sample_rate equal to 0 means that no polling will be performed and
 * add_to_buf will be triggered by external events (through task queue
 * returned by get_queue)
 *
 * data:
 * it may point to any driver data and this pointer will be passed to
 * all callback functions
 *
 * add_to_buf:
 * add_to_buf will be called after specified period of the time or
 * triggered by the external event, this behavior depends on value of
 * the sample_rate this function will be called in user context. This
 * routine should return 0 if data was added to the buffer and
 * -ENODATA if none was available. This should add some number of bits
 * evenly divisible by code_length to the buffer
 *
 * get_queue:
 * this callback should return a pointer to the task queue which will
 * be used for external event waiting
 *
 * rbuf:
 * if not NULL, it will be used as a read buffer, you will have to
 * write to the buffer by other means, like irq's (see also
 * lirc_serial.c).
 *
 * set_use_inc:
 * set_use_inc will be called after device is opened
 *
 * set_use_dec:
 * set_use_dec will be called after device is closed
 *
 * fops:
 * file_operations for drivers which don't fit the current driver model.
 *
 * Some ioctl's can be directly handled by lirc_dev if the driver's
 * ioctl function is NULL or if it returns -ENOIOCTLCMD (see also
 * lirc_serial.c).
 *
 * owner:
 * the module owning this struct
 *
 */


/* following functions can be called ONLY from user context
 *
 * returns negative value on error or minor number
 * of the registered device if success
 * contents of the structure pointed by d is copied
 */
extern int lirc_register_driver(struct lirc_driver *d);

/* returns negative value on error or 0 if success
*/
extern int lirc_unregister_driver(int minor);

/* Returns the private data stored in the lirc_driver
 * associated with the given device file pointer.
 */
void *lirc_get_pdata(struct file *file);

#endif
