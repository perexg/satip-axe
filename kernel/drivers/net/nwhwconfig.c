/*
 * Configuration of network device hardware from the kernel command line.
 *
 * Copyright (c) STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 */

#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/utsname.h>
#include <linux/in.h>
#include <linux/if.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/socket.h>
#include <linux/route.h>
#include <linux/udp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/major.h>
#include <linux/root_dev.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/ipconfig.h>

#include <asm/uaccess.h>
#include <net/checksum.h>
#include <asm/processor.h>

#undef NWHWDEBUG
#define NWHW_MAX_DEV NETDEV_BOOT_SETUP_MAX

static struct eth_dev {
	char user_dev_name[IFNAMSIZ];
	char user_hw_addr[18];
	int user_speed;
	int user_duplex;
} nwhwdev[NWHW_MAX_DEV];

static inline int hex_conv_nibble(char x)
{
	if ((x >= '0') && (x <= '9'))
		return x - '0';
	if ((x >= 'a') && (x <= 'f'))
		return x - 'a' + 10;
	if ((x >= 'A') && (x <= 'F'))
		return x - 'A' + 10;

	return -1;
}

static inline int parse_ether(const char *mac_addr_str, struct sockaddr *addr)
{
	int i, c1, c2;
	char *mac_addr = addr->sa_data;

	/*
	 * Pull out 6 two-digit hex chars
	 */
	for (i = 0; i < 6; i++) {

		c1 = hex_conv_nibble(*mac_addr_str++);
		c2 = hex_conv_nibble(*mac_addr_str++);

		if ((c1 == -1) || (c2 == -1))
			return 0;

		mac_addr[i] = (c1 << 4) | c2;

		if ((i != 5) && (*mac_addr_str++ != ':'))
			return 0;
	}

	addr->sa_family = ARPHRD_ETHER;

	return 1;
}

/**
 *  nwhw_config
 *  @dev : net device pointer
 *  Description:
 *	it sets the MAC address.
 *	Note that if the network device driver already uses a right
 *	address this function doesn't replace any value.
 */
static int __init nwhw_config(void)
{
	struct net_device *dev;
	struct sockaddr ether_addr;
	int valid_ether;
	int ndev = 0;

	while ((ndev < NWHW_MAX_DEV) &&
	       (dev = __dev_get_by_name(&init_net,
					nwhwdev[ndev].user_dev_name))) {

		if (!dev)
			break;

		if (!is_valid_ether_addr(dev->dev_addr)) {
			valid_ether = nwhwdev[ndev].user_hw_addr[0];

			if (valid_ether) {
				valid_ether =
				    parse_ether(nwhwdev[ndev].user_hw_addr,
						&ether_addr);
				if (!valid_ether) {
					printk("failed to parse addr: %s\n",
					       nwhwdev[ndev].user_hw_addr);
				}
			}
			printk(KERN_INFO "%s: (%s) setting mac address: %s\n",
			       __FUNCTION__, nwhwdev[ndev].user_dev_name,
			       nwhwdev[ndev].user_hw_addr);

			if (valid_ether) {
				rtnl_lock();
				if (dev_set_mac_address(dev, &ether_addr)) {
					printk(KERN_WARNING
					       "%s: not set MAC address...\n",
					       __FUNCTION__);
				}
				rtnl_unlock();
			}
		}

		if ((nwhwdev[ndev].user_speed != -1) ||
		    (nwhwdev[ndev].user_duplex != -1)) {
			struct ethtool_cmd cmd = { ETHTOOL_GSET };

			if (!dev->ethtool_ops->get_settings ||
			    (dev->ethtool_ops->get_settings(dev, &cmd) < 0)) {
				printk
				    ("Failed to read ether device settings\n");
			} else {
				cmd.cmd = ETHTOOL_SSET;
				cmd.autoneg = AUTONEG_DISABLE;
				if (nwhwdev[ndev].user_speed != -1)
					cmd.speed = nwhwdev[ndev].user_speed;
				if (nwhwdev[ndev].user_duplex != -1)
					cmd.duplex = nwhwdev[ndev].user_duplex;
				if (!dev->ethtool_ops->set_settings ||
				    (dev->ethtool_ops->set_settings(dev, &cmd) <
				     0)) {
					printk
					    ("Failed to set ether device settings\n");
				}
			}
		}
		ndev++;
	}
	return (0);
}

device_initcall(nwhw_config);

#if defined (CONFIG_NETPOLL)
void nwhw_uconfig(struct net_device *dev)
{
	struct sockaddr ether_addr;
	int ndev = 0;
	int valid_ether;

	valid_ether = nwhwdev[ndev].user_hw_addr[0];

	printk(KERN_DEBUG "%s\n", __FUNCTION__);
	while (ndev < NWHW_MAX_DEV) {
		if (valid_ether) {
			valid_ether = parse_ether(nwhwdev[ndev].user_hw_addr,
						  &ether_addr);
			if (!valid_ether) {
				printk(KERN_WARNING
				       "\tfailed to parse ether addr\n");
			}
		}
		if (valid_ether) {
			if (dev_set_mac_address(dev, &ether_addr))
				printk(KERN_WARNING "\tnot set MAC address\n");
		}
		ndev++;
	}
	return;
}
#endif

static void nwhw_print_args(void)
{
#ifdef NWHWDEBUG
	int i;
	printk("%s\n", __FUNCTION__);
	for (i = 0; i < NWHW_MAX_DEV; i++) {
		printk("\t%d) %s, addr %s, speed %d, duplex %s\n", i,
		       nwhwdev[i].user_dev_name, nwhwdev[i].user_hw_addr,
		       nwhwdev[i].user_speed,
		       (nwhwdev[i].user_duplex) ? "Full" : "Half");
	}
#endif
	return;
}

/**
 *  nwhw_config_setup - parse the nwhwconfig parameters
 *  @str : pointer to the nwhwconfig parameter
 *  Description:
 *  This function parses the nwhwconfig command line argumets.
 *  Command line syntax:
 *	nwhwconf=device:eth0,hwaddr:<mac0>[,speed:<speed0>][,duplex:<duplex0>];
		 device:eth1,hwaddr:<mac1>[,speed:<speed1>][,duplex:<duplex1>];
		...
 */
static int __init nwhw_config_setup(char *str)
{
	char *opt;
	int j = 0;

	if (!str || !*str)
		return 0;
	while (((opt = strsep(&str, ";")) != NULL) && (j < NWHW_MAX_DEV)) {
		char *p;
		nwhwdev[j].user_speed = -1;
		nwhwdev[j].user_duplex = -1;
		while ((p = strsep(&opt, ",")) != NULL) {
			if (!strncmp(p, "device:", 7)) {
				strlcpy(nwhwdev[j].user_dev_name, p + 7,
					sizeof(nwhwdev[j].user_dev_name));
			} else if (!strncmp(p, "hwaddr:", 7)) {
				strlcpy(nwhwdev[j].user_hw_addr, p + 7,
					sizeof(nwhwdev[j].user_hw_addr));
			} else if (!strncmp(p, "speed:", 6)) {
				switch (simple_strtoul(p + 6, NULL, 0)) {
				case 10:
					nwhwdev[j].user_speed = SPEED_10;
					break;
				case 100:
					nwhwdev[j].user_speed = SPEED_100;
					break;
				}
			} else if (!strcmp(p, "duplex:full")) {
				nwhwdev[j].user_duplex = DUPLEX_FULL;
			} else if (!strcmp(p, "duplex:half")) {
				nwhwdev[j].user_duplex = DUPLEX_HALF;
			}
		}
		j++;
	}

	nwhw_print_args();

	return 1;
}

__setup("nwhwconf=", nwhw_config_setup);
