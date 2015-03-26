/*
 * Copyright (C) 2007-09 STMicroelectronics Limited
 * Author: Angelo Castello <angelo.castello@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

/*
 * start code detect (SCD) support
 */
struct lirc_scd_s {
	unsigned int code;		/* code symbols to be detect. */
	unsigned int alt_code;		/* alternative SCD to be detected */
	unsigned int nomtime;		/* nominal symbol time in us */
	unsigned int noiserecov;	/* noise recovery configuration */
};

#define LIRC_SCD_CONFIGURE             _IOW('i', 0x00000021, struct lirc_scd_s)
#define LIRC_SCD_ENABLE                _IO('i', 0x00000022)
#define LIRC_SCD_DISABLE               _IO('i', 0x00000023)
#define LIRC_SCD_STATUS                _IOR('i', 0x00000024, unsigned long)
#define LIRC_SCD_GET_VALUE             _IOR('i', 0x00000025, struct lirc_scd_s)
