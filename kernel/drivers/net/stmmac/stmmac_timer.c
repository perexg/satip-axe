/*******************************************************************************
  STMMAC external timer support.

  Copyright (C) 2007-2009  STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/

#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include "stmmac_timer.h"

static void stmmac_timer_handler(void *data)
{
	struct net_device *dev = (struct net_device *)data;

	stmmac_schedule(dev);
}

#define STMMAC_TIMER_MSG(dev_name, timer, freq) \
printk(KERN_INFO "stmmac_timer: (%s) %s Timer (freq %dHz)\n", \
       dev_name, timer, freq);

#if defined(CONFIG_STMMAC_RTC_TIMER)
#include <linux/rtc.h>

static void stmmac_rtc_start(void *timer, unsigned int new_freq)
{
	struct rtc_device *rtc = timer;

	rtc_irq_set_freq(rtc, rtc->irq_task, new_freq);
	rtc_irq_set_state(rtc, rtc->irq_task, 1);
	return;
}

static void stmmac_rtc_stop(void *timer)
{
	struct rtc_device *rtc = timer;

	rtc_irq_set_state(rtc, rtc->irq_task, 0);
	return;
}

int stmmac_open_ext_timer(struct net_device *dev, struct stmmac_timer *tm)
{
	struct rtc_device *rtc;
	rtc_task_t rtc_task;

	rtc_task.private_data = dev;
	rtc_task.func = stmmac_timer_handler;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("open rtc device failed\n");
		return -ENODEV;
	}

	rtc_irq_register(rtc, &rtc_task);

	/* Periodic mode is not supported */
	if ((rtc_irq_set_freq(rtc, &rtc_task, tm->freq) < 0)) {
		pr_err("set periodic failed\n");
		rtc_irq_unregister(rtc, &rtc_task);
		rtc_class_close(rtc);
		return -1;
	}

	STMMAC_TIMER_MSG(dev->name, CONFIG_RTC_HCTOSYS_DEVICE, tm->freq);

	rtc->irq_task = &rtc_task;
	tm->timer_callb = rtc;
	tm->timer_start = stmmac_rtc_start;
	tm->timer_stop = stmmac_rtc_stop;

	return 0;
}

int stmmac_close_ext_timer(void *timer)
{
	struct rtc_device *rtc = timer;

	rtc_irq_set_state(rtc, rtc->irq_task, 0);
	rtc_irq_unregister(rtc, rtc->irq_task);
	rtc_class_close(rtc);

	return 0;
}

#elif defined(CONFIG_STMMAC_TMU_TIMER)
#include <linux/sh_timer.h>

/* Set rate and start the timer */
static void stmmac_tmu_set_rate(void *timer_callb, unsigned int new_freq)
{
	struct sh_timer_callb *timer = timer_callb;

	timer->timer_start(timer->tmu_priv);
	timer->set_rate(timer->tmu_priv, new_freq);
	return;
}

static void stmmac_tmu_stop(void *timer_callb)
{
	struct sh_timer_callb *timer = timer_callb;

	timer->timer_stop(timer->tmu_priv);
	return;
}

int stmmac_open_ext_timer(struct net_device *dev, struct stmmac_timer *tm)
{
	struct sh_timer_callb *timer;
	timer = sh_timer_register(stmmac_timer_handler,  (void *)dev);

	if (timer == NULL)
		return -1;

	STMMAC_TIMER_MSG(dev->name, "sh_tmu", tm->freq);

	tm->timer_callb = timer;
	tm->timer_start = stmmac_tmu_set_rate;
	tm->timer_stop = stmmac_tmu_stop;

	return 0;
}

int stmmac_close_ext_timer(void *timer_callb)
{
	struct sh_timer_callb *timer = timer_callb;

	stmmac_tmu_stop(timer_callb);
	sh_timer_unregister(timer->tmu_priv);
	return 0;
}
#endif
