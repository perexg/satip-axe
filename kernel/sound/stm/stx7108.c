/*
 *   STMicrolectronics STx7108 SoC audio glue driver
 *
 *   Copyright (c) 2010-2011 STMicroelectronics Limited
 *
 *   Author: Pawel Moll <pawel.moll@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <asm/irq-ilc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stx7108.h>
#include <sound/core.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx7108"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7108 audio subsystem "
		"card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7108 audio subsystem card.");



/*
 * Audio initialization
 */

static struct sysconf_field *snd_stm_stx7108_pcmp_valid_sel;

static int __init snd_stm_stx7108_init(void)
{
	int result;

	snd_stm_printd(0, "snd_stm_stx7108_init()\n");

	if (cpu_data->type != CPU_STX7108) {
		snd_stm_printe("Not supported (other than STx7108) SOC "
				"detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	/* Route PCM Player #2 to the digital outputs (PIOs) */
	snd_stm_stx7108_pcmp_valid_sel = sysconf_claim(SYS_CFG_BANK4,
			43, 0, 1, "PCMP_VALID_SEL");
	if (!snd_stm_stx7108_pcmp_valid_sel) {
		snd_stm_printe("Can't claim required sysconf field!\n");
		result = -EBUSY;
		goto error_sysconf_claim;
	}
	sysconf_write(snd_stm_stx7108_pcmp_valid_sel, 2);

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards!\n");
		goto error_card_register;
	}

	return 0;

error_card_register:
	sysconf_release(snd_stm_stx7108_pcmp_valid_sel);
error_sysconf_claim:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7108_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx7108_exit()\n");

	sysconf_release(snd_stm_stx7108_pcmp_valid_sel);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7108 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7108_init);
module_exit(snd_stm_stx7108_exit);
