/*
 * LIRC driver for Philips eHome USB Infrared Transceiver
 * and the Microsoft MCE 2005 Remote Control
 *
 * (C) by Martin A. Blatter <martin_a_blatter@yahoo.com>
 *
 * Transmitter support and reception code cleanup.
 * (C) by Daniel Melander <lirc@rajidae.se>
 *
 * Derived from ATI USB driver by Paul Miller and the original
 * MCE USB driver by Dan Corti
 *
 * This driver will only work reliably with kernel version 2.6.10
 * or higher, probably because of differences in USB device enumeration
 * in the kernel code. Device initialization fails most of the time
 * with earlier kernel versions.
 *
 **********************************************************************
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 5)
#error "*******************************************************"
#error "Sorry, this driver needs kernel version 2.6.5 or higher"
#error "*******************************************************"
#endif
#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/smp_lock.h>
#include <linux/completion.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <asm/uaccess.h>
#else
#include <linux/uaccess.h>
#endif
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/time.h>

#include <linux/lirc.h>
#include "kcompat.h"
#include "lirc_dev.h"

#define DRIVER_VERSION	"$Revision: 1.81 $"
#define DRIVER_AUTHOR	"Daniel Melander <lirc@rajidae.se>, " \
			"Martin Blatter <martin_a_blatter@yahoo.com>"
#define DRIVER_DESC	"Philips eHome USB IR Transceiver and Microsoft " \
			"MCE 2005 Remote Control driver for LIRC"
#define DRIVER_NAME	"lirc_mceusb2"

#define USB_BUFLEN	32	/* USB reception buffer length */
#define LIRCBUF_SIZE	256	/* LIRC work buffer length */

/* MCE constants */
#define MCE_CMDBUF_SIZE	384 /* MCE Command buffer length */
#define MCE_TIME_UNIT	50 /* Approx 50us resolution */
#define MCE_CODE_LENGTH	5 /* Normal length of packet (with header) */
#define MCE_PACKET_SIZE	4 /* Normal length of packet (without header) */
#define MCE_PACKET_HEADER 0x84 /* Actual header format is 0x80 + num_bytes */
#define MCE_CONTROL_HEADER 0x9F /* MCE status header */
#define MCE_TX_HEADER_LENGTH 3 /* # of bytes in the initializing tx header */
#define MCE_MAX_CHANNELS 2 /* Two transmitters, hardware dependent? */
#define MCE_DEFAULT_TX_MASK 0x03 /* Val opts: TX1=0x01, TX2=0x02, ALL=0x03 */
#define MCE_PULSE_BIT	0x80 /* Pulse bit, MSB set == PULSE else SPACE */
#define MCE_PULSE_MASK	0x7F /* Pulse mask */
#define MCE_MAX_PULSE_LENGTH 0x7F /* Longest transmittable pulse symbol */
#define MCE_PACKET_LENGTH_MASK  0x7F /* Pulse mask */


/* module parameters */
#ifdef CONFIG_USB_DEBUG
static int debug = 1;
#else
static int debug;
#endif
#define dprintk(fmt, args...)					\
	do {							\
		if (debug)					\
			printk(KERN_DEBUG fmt, ## args);	\
	} while (0)

/* general constants */
#define SEND_FLAG_IN_PROGRESS	1
#define SEND_FLAG_COMPLETE	2
#define RECV_FLAG_IN_PROGRESS	3
#define RECV_FLAG_COMPLETE	4

#define PHILUSB_INBOUND		1
#define PHILUSB_OUTBOUND	2

#define VENDOR_PHILIPS		0x0471
#define VENDOR_SMK		0x0609
#define VENDOR_TATUNG		0x1460
#define VENDOR_GATEWAY		0x107b
#define VENDOR_SHUTTLE		0x1308
#define VENDOR_SHUTTLE2		0x051c
#define VENDOR_MITSUMI		0x03ee
#define VENDOR_TOPSEED		0x1784
#define VENDOR_RICAVISION	0x179d
#define VENDOR_ITRON		0x195d
#define VENDOR_FIC		0x1509
#define VENDOR_LG		0x043e
#define VENDOR_MICROSOFT	0x045e
#define VENDOR_FORMOSA		0x147a
#define VENDOR_FINTEK		0x1934
#define VENDOR_PINNACLE		0x2304
#define VENDOR_ECS		0x1019
#define VENDOR_WISTRON		0x0fb8
#define VENDOR_COMPRO		0x185b
#define VENDOR_NORTHSTAR	0x04eb

static struct usb_device_id mceusb_dev_table[] = {
	/* Philips Infrared Transceiver - Sahara branded */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0608) },
	/* Philips Infrared Transceiver - HP branded */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060c) },
	/* Philips SRM5100 */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060d) },
	/* Philips Infrared Transceiver - Omaura */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060f) },
	/* Philips Infrared Transceiver - Spinel plus */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0613) },
	/* Philips eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0815) },
	/* SMK/Toshiba G83C0004D410 */
	{ USB_DEVICE(VENDOR_SMK, 0x031d) },
	/* SMK eHome Infrared Transceiver (Sony VAIO) */
	{ USB_DEVICE(VENDOR_SMK, 0x0322) },
	/* bundled with Hauppauge PVR-150 */
	{ USB_DEVICE(VENDOR_SMK, 0x0334) },
	/* Tatung eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TATUNG, 0x9150) },
	/* Shuttle eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_SHUTTLE, 0xc001) },
	/* Shuttle eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_SHUTTLE2, 0xc001) },
	/* Gateway eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_GATEWAY, 0x3009) },
	/* Mitsumi */
	{ USB_DEVICE(VENDOR_MITSUMI, 0x2501) },
	/* Topseed eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0001) },
	/* Topseed HP eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0006) },
	/* Topseed eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0007) },
	/* Topseed eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0008) },
	/* Ricavision internal Infrared Transceiver */
	{ USB_DEVICE(VENDOR_RICAVISION, 0x0010) },
	/* Itron ione Libra Q-11 */
	{ USB_DEVICE(VENDOR_ITRON, 0x7002) },
	/* FIC eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_FIC, 0x9242) },
	/* LG eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_LG, 0x9803) },
	/* Microsoft MCE Infrared Transceiver */
	{ USB_DEVICE(VENDOR_MICROSOFT, 0x00a0) },
	/* Formosa eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe015) },
	/* Formosa21 / eHome Infrared Receiver */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe016) },
	/* Formosa aim / Trust MCE Infrared Receiver */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe017) },
	/* Formosa Industrial Computing / Beanbag Emulation Device */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe018) },
	/* Fintek eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_FINTEK, 0x0602) },
	/* Fintek eHome Infrared Transceiver (in the AOpen MP45) */
	{ USB_DEVICE(VENDOR_FINTEK, 0x0702) },
	/* Pinnacle Remote Kit */
	{ USB_DEVICE(VENDOR_PINNACLE, 0x0225) },
	/* Elitegroup Computer Systems IR */
	{ USB_DEVICE(VENDOR_ECS, 0x0f38) },
	/* Wistron Corp. eHome Infrared Receiver */
	{ USB_DEVICE(VENDOR_WISTRON, 0x0002) },
	/* Compro K100 */
	{ USB_DEVICE(VENDOR_COMPRO, 0x3020) },
	/* Northstar Systems eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_NORTHSTAR, 0xe004) },
	/* Terminating entry */
	{ }
};

static struct usb_device_id pinnacle_list[] = {
	{ USB_DEVICE(VENDOR_PINNACLE, 0x0225) },
	{}
};

static struct usb_device_id transmitter_mask_list[] = {
	{ USB_DEVICE(VENDOR_SMK, 0x031d) },
	{ USB_DEVICE(VENDOR_SMK, 0x0322) },
	{ USB_DEVICE(VENDOR_SMK, 0x0334) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0001) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0007) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0008) },
	{ USB_DEVICE(VENDOR_PINNACLE, 0x0225) },
	{}
};

/* data structure for each usb transceiver */
struct mceusb2_dev {

	/* usb */
	struct usb_device *usbdev;
	struct urb *urb_in;
	int devnum;
	struct usb_endpoint_descriptor *usb_ep_in;
	struct usb_endpoint_descriptor *usb_ep_out;

	/* buffers and dma */
	unsigned char *buf_in;
	unsigned int len_in;
	dma_addr_t dma_in;
	dma_addr_t dma_out;

	/* lirc */
	struct lirc_driver *d;
	lirc_t lircdata;
	unsigned char is_pulse;
	struct {
		u32 connected:1;
		u32 pinnacle:1;
		u32 transmitter_mask_inverted:1;
		u32 reserved:29;
	} flags;

	unsigned char transmitter_mask;
	unsigned int carrier_freq;

	/* handle sending (init strings) */
	int send_flags;
	wait_queue_head_t wait_out;

	struct mutex lock;
};

/* init strings */
static char init1[] = {0x00, 0xff, 0xaa, 0xff, 0x0b};
static char init2[] = {0xff, 0x18};

static char pin_init1[] = { 0x9f, 0x07};
static char pin_init2[] = { 0x9f, 0x13};
static char pin_init3[] = { 0x9f, 0x0d};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
static unsigned long usecs_to_jiffies(const unsigned int u)
{
	if (u > jiffies_to_usecs(MAX_JIFFY_OFFSET))
		return MAX_JIFFY_OFFSET;
#if HZ <= USEC_PER_SEC && !(USEC_PER_SEC % HZ)
	return (u + (USEC_PER_SEC / HZ) - 1) / (USEC_PER_SEC / HZ);
#elif HZ > USEC_PER_SEC && !(HZ % USEC_PER_SEC)
	return u * (HZ / USEC_PER_SEC);
#else
	return (u * HZ + USEC_PER_SEC - 1) / USEC_PER_SEC;
#endif
}
#endif
static void mceusb_dev_printdata(struct mceusb2_dev *ir, char *buf, int len)
{
	char codes[USB_BUFLEN*3 + 1];
	int i;

	if (len <= 0)
		return;

	for (i = 0; i < len && i < USB_BUFLEN; i++)
		snprintf(codes+i*3, 4, "%02x ", buf[i] & 0xFF);

	printk(KERN_INFO "" DRIVER_NAME "[%d]: data received %s (length=%d)\n",
		ir->devnum, codes, len);
}

static void usb_async_callback(struct urb *urb, struct pt_regs *regs)
{
	struct mceusb2_dev *ir;
	int len;

	if (!urb)
		return;

	ir = urb->context;
	if (ir) {
		len = urb->actual_length;

		dprintk(DRIVER_NAME
			"[%d]: callback called (status=%d len=%d)\n",
			ir->devnum, urb->status, len);

		if (debug)
			mceusb_dev_printdata(ir, urb->transfer_buffer, len);
	}

}


/* request incoming or send outgoing usb packet - used to initialize remote */
static void request_packet_async(struct mceusb2_dev *ir,
				 struct usb_endpoint_descriptor *ep,
				 unsigned char *data, int size, int urb_type)
{
	int res;
	struct urb *async_urb;
	unsigned char *async_buf;

	if (urb_type) {
		async_urb = usb_alloc_urb(0, GFP_KERNEL);
		if (likely(async_urb)) {
			/* alloc buffer */
			async_buf = kmalloc(size, GFP_KERNEL);
			if (async_buf) {
				if (urb_type == PHILUSB_OUTBOUND) {
					/* outbound data */
					usb_fill_int_urb(async_urb, ir->usbdev,
						usb_sndintpipe(ir->usbdev,
							ep->bEndpointAddress),
					async_buf,
					size,
					(usb_complete_t) usb_async_callback,
					ir, ep->bInterval);

					memcpy(async_buf, data, size);
				} else {
					/* inbound data */
					usb_fill_int_urb(async_urb, ir->usbdev,
						usb_rcvintpipe(ir->usbdev,
							ep->bEndpointAddress),
					async_buf, size,
					(usb_complete_t) usb_async_callback,
					ir, ep->bInterval);
				}
				async_urb->transfer_flags = URB_ASYNC_UNLINK;
			} else {
				usb_free_urb(async_urb);
				return;
			}
		}
	} else {
		/* standard request */
		async_urb = ir->urb_in;
		ir->send_flags = RECV_FLAG_IN_PROGRESS;
	}
	dprintk(DRIVER_NAME "[%d]: receive request called (size=%#x)\n",
		ir->devnum, size);

	async_urb->transfer_buffer_length = size;
	async_urb->dev = ir->usbdev;

	res = usb_submit_urb(async_urb, GFP_ATOMIC);
	if (res) {
		dprintk(DRIVER_NAME "[%d]: receive request FAILED! (res=%d)\n",
			ir->devnum, res);
		return;
	}
	dprintk(DRIVER_NAME "[%d]: receive request complete (res=%d)\n",
		ir->devnum, res);
}

static int unregister_from_lirc(struct mceusb2_dev *ir)
{
	struct lirc_driver *d = ir->d;
	int devnum;
	int rtn;

	devnum = ir->devnum;
	dprintk(DRIVER_NAME "[%d]: unregister from lirc called\n", devnum);

	rtn = lirc_unregister_driver(d->minor);
	if (rtn > 0) {
		printk(DRIVER_NAME "[%d]: error in lirc_unregister minor: %d\n"
			"Trying again...\n", devnum, d->minor);
		if (rtn == -EBUSY) {
			printk(DRIVER_NAME
				"[%d]: device is opened, will unregister"
				" on close\n", devnum);
			return -EAGAIN;
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);

		rtn = lirc_unregister_driver(d->minor);
		if (rtn > 0)
			printk(DRIVER_NAME "[%d]: lirc_unregister failed\n",
			devnum);
	}

	if (rtn) {
		printk(DRIVER_NAME "[%d]: didn't free resources\n", devnum);
		return -EAGAIN;
	}

	printk(DRIVER_NAME "[%d]: usb remote disconnected\n", devnum);

	lirc_buffer_free(d->rbuf);
	kfree(d->rbuf);
	kfree(d);
	kfree(ir);
	return 0;
}

static int set_use_inc(void *data)
{
	struct mceusb2_dev *ir = data;

	if (!ir) {
		printk(DRIVER_NAME "[?]: set_use_inc called with no context\n");
		return -EIO;
	}
	dprintk(DRIVER_NAME "[%d]: set use inc\n", ir->devnum);

	MOD_INC_USE_COUNT;
	if (!ir->flags.connected) {
		if (!ir->usbdev)
			return -ENOENT;
		ir->flags.connected = 1;
	}

	return 0;
}

static void set_use_dec(void *data)
{
	struct mceusb2_dev *ir = data;

	if (!ir) {
		printk(DRIVER_NAME "[?]: set_use_dec called with no context\n");
		return;
	}
	dprintk(DRIVER_NAME "[%d]: set use dec\n", ir->devnum);

	if (ir->flags.connected) {
		mutex_lock(&ir->lock);
		ir->flags.connected = 0;
		mutex_unlock(&ir->lock);
	}
	MOD_DEC_USE_COUNT;
}

static void send_packet_to_lirc(struct mceusb2_dev *ir)
{
	if (ir->lircdata != 0) {
		lirc_buffer_write(ir->d->rbuf,
				  (unsigned char *) &ir->lircdata);
		wake_up(&ir->d->rbuf->wait_poll);
		ir->lircdata = 0;
	}
}

static void mceusb_dev_recv(struct urb *urb, struct pt_regs *regs)
{
	struct mceusb2_dev *ir;
	int buf_len, packet_len;
	int i, j;

	if (!urb)
		return;

	ir = urb->context;
	if (!ir) {
		urb->transfer_flags |= URB_ASYNC_UNLINK;
		usb_unlink_urb(urb);
		return;
	}

	buf_len = urb->actual_length;
	packet_len = 0;

	if (debug)
		mceusb_dev_printdata(ir, urb->transfer_buffer, buf_len);

	if (ir->send_flags == RECV_FLAG_IN_PROGRESS) {
		ir->send_flags = SEND_FLAG_COMPLETE;
		dprintk(DRIVER_NAME "[%d]: setup answer received %d bytes\n",
			ir->devnum, buf_len);
	}

	switch (urb->status) {
	/* success */
	case 0:
		for (i = 0; i < buf_len; i++) {
			/* decode mce packets of the form (84),AA,BB,CC,DD */
			if (ir->buf_in[i] >= 0x80 && ir->buf_in[i] <= 0x9e) {
				/* data headers */
				/* decode packet data */
				packet_len =
					ir->buf_in[i] & MCE_PACKET_LENGTH_MASK;
				for (j = 1;
				     j <= packet_len && (i+j < buf_len);
				     j++) {
					/* rising/falling flank */
					if (ir->is_pulse !=
					    (ir->buf_in[i + j] &
					     MCE_PULSE_BIT)) {
						send_packet_to_lirc(ir);
						ir->is_pulse =
							ir->buf_in[i + j] &
								MCE_PULSE_BIT;
					}

					/* accumulate mce pulse/space values */
					ir->lircdata +=
						(ir->buf_in[i + j] &
						 MCE_PULSE_MASK)*MCE_TIME_UNIT;
					ir->lircdata |=
						(ir->is_pulse ? PULSE_BIT : 0);
				}

				i += packet_len;
			} else if (ir->buf_in[i] == MCE_CONTROL_HEADER) {
				/* status header (0x9F) */
				/*
				 * A transmission containing one or
				 * more consecutive ir commands always
				 * ends with a GAP of 100ms followed by the
				 * sequence 0x9F 0x01 0x01 0x9F 0x15
				 * 0x00 0x00 0x80
				 */

		/*
		Uncomment this if the last 100ms
		"infinity"-space should be transmitted
		to lirc directly instead of at the beginning
		of the next transmission. Changes pulse/space order.

				if (++i < buf_len && ir->buf_in[i]==0x01)
					send_packet_to_lirc(ir);

		*/

				/* end decode loop */
				i = buf_len;
			}
		}

		break;

	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		urb->transfer_flags |= URB_ASYNC_UNLINK;
		usb_unlink_urb(urb);
		return;

	case -EPIPE:
	default:
		break;
	}

	usb_submit_urb(urb, GFP_ATOMIC);
}


static ssize_t lirc_write(struct file *file, const char *buf,
			  size_t n, loff_t *ppos)
{
	int i, count = 0, cmdcount = 0;
	struct mceusb2_dev *ir = NULL;
	lirc_t wbuf[LIRCBUF_SIZE]; /* Workbuffer with values from lirc */
	unsigned char cmdbuf[MCE_CMDBUF_SIZE]; /* MCE command buffer */
	unsigned long signal_duration = 0; /* Singnal length in us */
	struct timeval start_time, end_time;

	do_gettimeofday(&start_time);

	/* Retrieve lirc_driver data for the device */
	ir = lirc_get_pdata(file);
	if (!ir || !ir->usb_ep_out)
		return -EFAULT;

	if (n % sizeof(lirc_t))
		return -EINVAL;
	count = n / sizeof(lirc_t);

	/* Check if command is within limits */
	if (count > LIRCBUF_SIZE || count%2 == 0)
		return -EINVAL;
	if (copy_from_user(wbuf, buf, n))
		return -EFAULT;

	/* MCE tx init header */
	cmdbuf[cmdcount++] = MCE_CONTROL_HEADER;
	cmdbuf[cmdcount++] = 0x08;
	cmdbuf[cmdcount++] = ir->transmitter_mask;

	/* Generate mce packet data */
	for (i = 0; (i < count) && (cmdcount < MCE_CMDBUF_SIZE); i++) {
		signal_duration += wbuf[i];
		wbuf[i] = wbuf[i] / MCE_TIME_UNIT;

		do { /* loop to support long pulses/spaces > 127*50us=6.35ms */

			/* Insert mce packet header every 4th entry */
			if ((cmdcount < MCE_CMDBUF_SIZE) &&
			    (cmdcount - MCE_TX_HEADER_LENGTH) %
			     MCE_CODE_LENGTH == 0)
				cmdbuf[cmdcount++] = MCE_PACKET_HEADER;

			/* Insert mce packet data */
			if (cmdcount < MCE_CMDBUF_SIZE)
				cmdbuf[cmdcount++] =
					(wbuf[i] < MCE_PULSE_BIT ?
					 wbuf[i] : MCE_MAX_PULSE_LENGTH) |
					 (i & 1 ? 0x00 : MCE_PULSE_BIT);
			else
				return -EINVAL;
		} while ((wbuf[i] > MCE_MAX_PULSE_LENGTH) &&
			 (wbuf[i] -= MCE_MAX_PULSE_LENGTH));
	}

	/* Fix packet length in last header */
	cmdbuf[cmdcount - (cmdcount - MCE_TX_HEADER_LENGTH) % MCE_CODE_LENGTH] =
		0x80 + (cmdcount - MCE_TX_HEADER_LENGTH) % MCE_CODE_LENGTH - 1;

	/* Check if we have room for the empty packet at the end */
	if (cmdcount >= MCE_CMDBUF_SIZE)
		return -EINVAL;

	/* All mce commands end with an empty packet (0x80) */
	cmdbuf[cmdcount++] = 0x80;

	/* Transmit the command to the mce device */
	request_packet_async(ir, ir->usb_ep_out, cmdbuf,
			     cmdcount, PHILUSB_OUTBOUND);

	/*
	 * The lircd gap calculation expects the write function to
	 * wait the time it takes for the ircommand to be sent before
	 * it returns.
	 */
	do_gettimeofday(&end_time);
	signal_duration -= (end_time.tv_usec - start_time.tv_usec) +
			   (end_time.tv_sec - start_time.tv_sec) * 1000000;

	/* delay with the closest number of ticks */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(usecs_to_jiffies(signal_duration));

	return n;
}

static void set_transmitter_mask(struct mceusb2_dev *ir, unsigned int mask)
{
	if (ir->flags.transmitter_mask_inverted)
		/*
		 * The mask begins at 0x02 and has an inverted
		 * numbering scheme
		 */
		ir->transmitter_mask =
			(mask != 0x03 ? mask ^ 0x03 : mask) << 1;
	else
		ir->transmitter_mask = mask;
}


/* Sets the send carrier frequency */
static int set_send_carrier(struct mceusb2_dev *ir, int carrier)
{
	int clk = 10000000;
	int prescaler = 0, divisor = 0;
	unsigned char cmdbuf[] = { 0x9F, 0x06, 0x01, 0x80 };

	/* Carrier is changed */
	if (ir->carrier_freq != carrier) {

		if (carrier <= 0) {
			ir->carrier_freq = carrier;
			dprintk(DRIVER_NAME "[%d]: SET_CARRIER disabling "
				"carrier modulation\n", ir->devnum);
			request_packet_async(ir, ir->usb_ep_out,
					     cmdbuf, sizeof(cmdbuf),
					     PHILUSB_OUTBOUND);
			return carrier;
		}

		for (prescaler = 0; prescaler < 4; ++prescaler) {
			divisor = (clk >> (2 * prescaler)) / carrier;
			if (divisor <= 0xFF) {
				ir->carrier_freq = carrier;
				cmdbuf[2] = prescaler;
				cmdbuf[3] = divisor;
				dprintk(DRIVER_NAME "[%d]: SET_CARRIER "
					"requesting %d Hz\n",
					ir->devnum, carrier);

				/* Transmit new carrier to mce device */
				request_packet_async(ir, ir->usb_ep_out,
						     cmdbuf, sizeof(cmdbuf),
						     PHILUSB_OUTBOUND);
				return carrier;
			}
		}

		return -EINVAL;

	}

	return carrier;
}


static int lirc_ioctl(struct inode *node, struct file *filep,
		      unsigned int cmd, unsigned long arg)
{
	int result;
	unsigned int ivalue;
	unsigned long lvalue;
	struct mceusb2_dev *ir = NULL;

	/* Retrieve lirc_driver data for the device */
	ir = lirc_get_pdata(filep);
	if (!ir || !ir->usb_ep_out)
		return -EFAULT;


	switch (cmd) {
	case LIRC_SET_TRANSMITTER_MASK:

		result = get_user(ivalue, (unsigned int *) arg);
		if (result)
			return result;
		switch (ivalue) {
		case 0x01: /* Transmitter 1     => 0x04 */
		case 0x02: /* Transmitter 2     => 0x02 */
		case 0x03: /* Transmitter 1 & 2 => 0x06 */
			set_transmitter_mask(ir, ivalue);
			break;

		default: /* Unsupported transmitter mask */
			return MCE_MAX_CHANNELS;
		}

		dprintk(DRIVER_NAME ": SET_TRANSMITTERS mask=%d\n", ivalue);
		break;

	case LIRC_GET_SEND_MODE:

		result = put_user(LIRC_SEND2MODE(LIRC_CAN_SEND_PULSE &
						 LIRC_CAN_SEND_MASK),
				  (unsigned long *) arg);

		if (result)
			return result;
		break;

	case LIRC_SET_SEND_MODE:

		result = get_user(lvalue, (unsigned long *) arg);

		if (result)
			return result;
		if (lvalue != (LIRC_MODE_PULSE&LIRC_CAN_SEND_MASK))
			return -EINVAL;
		break;

	case LIRC_SET_SEND_CARRIER:

		result = get_user(ivalue, (unsigned int *) arg);
		if (result)
			return result;

		set_send_carrier(ir, ivalue);
		break;

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static struct file_operations lirc_fops = {
	.owner	= THIS_MODULE,
	.write	= lirc_write,
	.ioctl	= lirc_ioctl,
};


static int mceusb_dev_probe(struct usb_interface *intf,
				const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *idesc;
	struct usb_endpoint_descriptor *ep = NULL;
	struct usb_endpoint_descriptor *ep_in = NULL;
	struct usb_endpoint_descriptor *ep_out = NULL;
	struct usb_host_config *config;
	struct mceusb2_dev *ir = NULL;
	struct lirc_driver *driver = NULL;
	struct lirc_buffer *rbuf = NULL;
	int devnum, pipe, maxp;
	int minor = 0;
	int i;
	char buf[63], name[128] = "";
	int mem_failure = 0;
	int is_pinnacle;

	dprintk(DRIVER_NAME ": usb probe called\n");

	usb_reset_device(dev);

	config = dev->actconfig;

	idesc = intf->cur_altsetting;

	is_pinnacle = usb_match_id(intf, pinnacle_list) ? 1 : 0;

	/* step through the endpoints to find first bulk in and out endpoint */
	for (i = 0; i < idesc->desc.bNumEndpoints; ++i) {
		ep = &idesc->endpoint[i].desc;

		if ((ep_in == NULL)
			&& ((ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
			    == USB_DIR_IN)
			&& (((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
			    == USB_ENDPOINT_XFER_BULK)
			|| ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
			    == USB_ENDPOINT_XFER_INT))) {

			dprintk(DRIVER_NAME ": acceptable inbound endpoint "
				"found\n");
			ep_in = ep;
			ep_in->bmAttributes = USB_ENDPOINT_XFER_INT;
			if (is_pinnacle)
				/*
				 * setting seems to 1 seem to cause issues with
				 * Pinnacle timing out on transfer.
				 */
				ep_in->bInterval = ep->bInterval;
			else
				ep_in->bInterval = 1;
		}

		if ((ep_out == NULL)
			&& ((ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
			    == USB_DIR_OUT)
			&& (((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
			    == USB_ENDPOINT_XFER_BULK)
			|| ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
			    == USB_ENDPOINT_XFER_INT))) {

			dprintk(DRIVER_NAME ": acceptable outbound endpoint "
				"found\n");
			ep_out = ep;
			ep_out->bmAttributes = USB_ENDPOINT_XFER_INT;
			if (is_pinnacle)
				/*
				 * setting seems to 1 seem to cause issues with
				 * Pinnacle timing out on transfer.
				 */
				ep_out->bInterval = ep->bInterval;
			else
				ep_out->bInterval = 1;
		}
	}
	if (ep_in == NULL || ep_out == NULL) {
		dprintk(DRIVER_NAME ": inbound and/or "
			"outbound endpoint not found\n");
		return -ENODEV;
	}

	devnum = dev->devnum;
	pipe = usb_rcvintpipe(dev, ep_in->bEndpointAddress);
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

	mem_failure = 0;
	ir = kzalloc(sizeof(struct mceusb2_dev), GFP_KERNEL);
	if (!ir) {
		mem_failure = 1;
		goto mem_failure_switch;
	}

	driver = kzalloc(sizeof(struct lirc_driver), GFP_KERNEL);
	if (!driver) {
		mem_failure = 2;
		goto mem_failure_switch;
	}

	rbuf = kmalloc(sizeof(struct lirc_buffer), GFP_KERNEL);
	if (!rbuf) {
		mem_failure = 3;
		goto mem_failure_switch;
	}

	if (lirc_buffer_init(rbuf, sizeof(lirc_t), LIRCBUF_SIZE)) {
		mem_failure = 4;
		goto mem_failure_switch;
	}

	ir->buf_in = usb_buffer_alloc(dev, maxp, GFP_ATOMIC, &ir->dma_in);
	if (!ir->buf_in) {
		mem_failure = 5;
		goto mem_failure_switch;
	}

	ir->urb_in = usb_alloc_urb(0, GFP_KERNEL);
	if (!ir->urb_in) {
		mem_failure = 7;
		goto mem_failure_switch;
	}

	strcpy(driver->name, DRIVER_NAME " ");
	driver->minor = -1;
	driver->features = LIRC_CAN_SEND_PULSE |
		LIRC_CAN_SET_TRANSMITTER_MASK |
		LIRC_CAN_REC_MODE2 |
		LIRC_CAN_SET_SEND_CARRIER;
	driver->data = ir;
	driver->rbuf = rbuf;
	driver->set_use_inc = &set_use_inc;
	driver->set_use_dec = &set_use_dec;
	driver->code_length = sizeof(lirc_t) * 8;
	driver->fops  = &lirc_fops;
	driver->dev   = &intf->dev;
	driver->owner = THIS_MODULE;

	mutex_init(&ir->lock);
	init_waitqueue_head(&ir->wait_out);

	minor = lirc_register_driver(driver);
	if (minor < 0)
		mem_failure = 9;

mem_failure_switch:

	switch (mem_failure) {
	case 9:
		usb_free_urb(ir->urb_in);
	case 7:
		usb_buffer_free(dev, maxp, ir->buf_in, ir->dma_in);
	case 5:
		lirc_buffer_free(rbuf);
	case 4:
		kfree(rbuf);
	case 3:
		kfree(driver);
	case 2:
		kfree(ir);
	case 1:
		printk(DRIVER_NAME "[%d]: out of memory (code=%d)\n",
			devnum, mem_failure);
		return -ENOMEM;
	}

	driver->minor = minor;
	ir->d = driver;
	ir->devnum = devnum;
	ir->usbdev = dev;
	ir->len_in = maxp;
	ir->flags.connected = 0;
	ir->flags.pinnacle = is_pinnacle;
	ir->flags.transmitter_mask_inverted =
		usb_match_id(intf, transmitter_mask_list) ? 0 : 1;

	ir->lircdata = PULSE_MASK;
	ir->is_pulse = 0;

	/* ir->flags.transmitter_mask_inverted must be set */
	set_transmitter_mask(ir, MCE_DEFAULT_TX_MASK);
	/* Saving usb interface data for use by the transmitter routine */
	ir->usb_ep_in = ep_in;
	ir->usb_ep_out = ep_out;

	if (dev->descriptor.iManufacturer
	    && usb_string(dev, dev->descriptor.iManufacturer,
			  buf, sizeof(buf)) > 0)
		strlcpy(name, buf, sizeof(name));
	if (dev->descriptor.iProduct
	    && usb_string(dev, dev->descriptor.iProduct,
			  buf, sizeof(buf)) > 0)
		snprintf(name + strlen(name), sizeof(name) - strlen(name),
			 " %s", buf);
	printk(DRIVER_NAME "[%d]: %s on usb%d:%d\n", devnum, name,
	       dev->bus->busnum, devnum);

	/* inbound data */
	usb_fill_int_urb(ir->urb_in, dev, pipe, ir->buf_in,
		maxp, (usb_complete_t) mceusb_dev_recv, ir, ep_in->bInterval);
	ir->urb_in->transfer_dma = ir->dma_in;
	ir->urb_in->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* initialize device */
	if (ir->flags.pinnacle) {
		int usbret;

		/*
		 * I have no idea why but this reset seems to be crucial to
		 * getting the device to do outbound IO correctly - without
		 * this the device seems to hang, ignoring all input - although
		 * IR signals are correctly sent from the device, no input is
		 * interpreted by the device and the host never does the
		 * completion routine
		 */

		usbret = usb_reset_configuration(dev);
		printk(DRIVER_NAME "[%d]: usb reset config ret %x\n",
		       devnum, usbret);

		/*
		 * its possible we really should wait for a return
		 * for each of these...
		 */
		request_packet_async(ir, ep_in, NULL, maxp, PHILUSB_INBOUND);
		request_packet_async(ir, ep_out, pin_init1, sizeof(pin_init1),
				     PHILUSB_OUTBOUND);
		request_packet_async(ir, ep_in, NULL, maxp, PHILUSB_INBOUND);
		request_packet_async(ir, ep_out, pin_init2, sizeof(pin_init2),
				     PHILUSB_OUTBOUND);
		request_packet_async(ir, ep_in, NULL, maxp, PHILUSB_INBOUND);
		request_packet_async(ir, ep_out, pin_init3, sizeof(pin_init3),
				     PHILUSB_OUTBOUND);
		/* if we don't issue the correct number of receives
		 * (PHILUSB_INBOUND) for each outbound, then the first few ir
		 * pulses will be interpreted by the usb_async_callback routine
		 * - we should ensure we have the right amount OR less - as the
		 * mceusb_dev_recv routine will handle the control packets OK -
		 * they start with 0x9f - but the async callback doesn't handle
		 * ir pulse packets
		 */
		request_packet_async(ir, ep_in, NULL, maxp, 0);
	} else {
		request_packet_async(ir, ep_in, NULL, maxp, PHILUSB_INBOUND);
		request_packet_async(ir, ep_in, NULL, maxp, PHILUSB_INBOUND);
		request_packet_async(ir, ep_out, init1,
				     sizeof(init1), PHILUSB_OUTBOUND);
		request_packet_async(ir, ep_in, NULL, maxp, PHILUSB_INBOUND);
		request_packet_async(ir, ep_out, init2,
				     sizeof(init2), PHILUSB_OUTBOUND);
		request_packet_async(ir, ep_in, NULL, maxp, 0);
	}

	usb_set_intfdata(intf, ir);

	return 0;
}


static void mceusb_dev_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct mceusb2_dev *ir = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);

	if (!ir || !ir->d)
		return;

	ir->usbdev = NULL;
	wake_up_all(&ir->wait_out);

	mutex_lock(&ir->lock);
	usb_kill_urb(ir->urb_in);
	usb_free_urb(ir->urb_in);
	usb_buffer_free(dev, ir->len_in, ir->buf_in, ir->dma_in);
	mutex_unlock(&ir->lock);

	unregister_from_lirc(ir);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static int mceusb_dev_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct mceusb2_dev *ir = usb_get_intfdata(intf);
	printk(DRIVER_NAME "[%d]: suspend\n", ir->devnum);
	usb_kill_urb(ir->urb_in);
	return 0;
}

static int mceusb_dev_resume(struct usb_interface *intf)
{
	struct mceusb2_dev *ir = usb_get_intfdata(intf);
	printk(DRIVER_NAME "[%d]: resume\n", ir->devnum);
	if (usb_submit_urb(ir->urb_in, GFP_ATOMIC))
		return -EIO;
	return 0;
}
#endif

static struct usb_driver mceusb_dev_driver = {
	LIRC_THIS_MODULE(.owner = THIS_MODULE)
	.name =		DRIVER_NAME,
	.probe =	mceusb_dev_probe,
	.disconnect =	mceusb_dev_disconnect,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	.suspend =	mceusb_dev_suspend,
	.resume =	mceusb_dev_resume,
#endif
	.id_table =	mceusb_dev_table
};

static int __init mceusb_dev_init(void)
{
	int i;

	printk(KERN_INFO "\n");
	printk(KERN_INFO DRIVER_NAME ": " DRIVER_DESC " " DRIVER_VERSION "\n");
	printk(KERN_INFO DRIVER_NAME ": " DRIVER_AUTHOR "\n");
	dprintk(DRIVER_NAME ": debug mode enabled\n");

	i = usb_register(&mceusb_dev_driver);
	if (i < 0) {
		printk(DRIVER_NAME ": usb register failed, result = %d\n", i);
		return -ENODEV;
	}

	return 0;
}

static void __exit mceusb_dev_exit(void)
{
	usb_deregister(&mceusb_dev_driver);
}

module_init(mceusb_dev_init);
module_exit(mceusb_dev_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(usb, mceusb_dev_table);

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");

EXPORT_NO_SYMBOLS;
