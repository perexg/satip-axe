/*
 * arch/sh/boards/st/common/mb562.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics BD-DVD peripherals board support.
 */

#include <linux/init.h>
#include <linux/stm/stx7200.h>
#include <linux/bug.h>
#include <asm/processor.h>

static int __init device_init(void)
{
	/* So far valid only for 7200 processor board! */
	BUG_ON(cpu_data->type != CPU_STX7200);

	/* Set up "scenario 1" of audio outputs */
	stx7200_configure_audio(&(struct stx7200_audio_config) {
			.pcm_player_1_routing = stx7200_pcm_player_1_mii1,
			.pcm_player_3_routing =
					stx7200_pcm_player_3_aiddig0_auddig1,
			});

	return 0;
}
arch_initcall(device_init);
