/*
 * arch/sh/boards/mach-adi7108/setup.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: Srinivas Kandagatla (srinivas.kandagatla@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>


#define ADI7108_PIO_POWER_ON_ETHERNET0 stm_gpio(19, 7)
#define ADI7108_PIO_POWER_ON_ETHERNET1 stm_gpio(15, 4)
#define ADI7108_GPIO_MII1_SPEED_SEL stm_gpio(21, 7)


static void __init adi7108_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STi7108-ADI Board "
			"initialisation\n");

	stx7108_early_device_init();

	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio24_4,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio24_5,
			.hw_flow_control = 0,
			.is_console = 1, });

	stx7108_configure_asc(1, &(struct stx7108_asc_config) {
			.hw_flow_control = 1, });
}


static int adi7108_phy1_reset(void *bus)
{
	static int done;
	if (!done) {
		gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET1, 0);
		udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
		gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET1, 1);
		done = 1;
	}

	return 1;
}


static void adi7108_mii_txclk_select(int txclk_250_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO21[7]. */
	if (txclk_250_not_25_mhz)
		gpio_set_value(ADI7108_GPIO_MII1_SPEED_SEL, 1);
	else
		gpio_set_value(ADI7108_GPIO_MII1_SPEED_SEL, 0);
}

#ifdef CONFIG_SH_ST_ADI7108_STMMAC0
static int adi7108_phy0_reset(void *bus)
{
	static int done;

	if (!done) {
		gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET0, 0);
		udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
		gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET0, 1);
		done = 1;
	}

	return 1;
}

static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	.bus_id = 0,
	.phy_reset = adi7108_phy0_reset,
	.phy_mask = 0,
};
#endif /* CONFIG_SH_ST_ADI7108_STMMAC0 */

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.bus_id = 1,
	.phy_reset = adi7108_phy1_reset,
	.phy_mask = 0,
};

/* NOR FLASH */
static struct platform_device adi7108_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 256*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= NULL,
		.nr_parts	= 3,
		.parts		= (struct mtd_partition []) {
			{
				.name = "NOR Flash 1",
				.size = 0x00080000,
				.offset = 0x00000000,
			}, {
				.name = "NOR Flash 2",
				.size = 0x00200000,
				.offset = MTDPART_OFS_NXTBLK,
			}, {
				.name = "NOR Flash 3",
				.size = MTDPART_SIZ_FULL,
				.offset = MTDPART_OFS_NXTBLK,
			}
		},
	},
};

/* Serial FLASH */
static struct stm_plat_spifsm_data adi7108_serial_flash =  {
	.name		= "m25p128",
	.nr_parts	= 2,
	.parts = (struct mtd_partition []) {
		{
			.name = "Serial Flash 1",
			.size = 0x00200000,
			.offset = 0,
		}, {
			.name = "Serial Flash 2",
			.size = MTDPART_SIZ_FULL,
			.offset = MTDPART_OFS_NXTBLK,
		},
	},
};

/* NAND FLASH */
static struct stm_nand_bank_data adi7108_nand_flash = {
	.csn		= 1,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.nr_partitions	= 2,
	.partitions	= (struct mtd_partition []) {
		{
			.name	= "NAND Flash 1",
			.offset	= 0,
			.size 	= 0x00800000
		}, {
			.name	= "NAND Flash 2",
			.offset = MTDPART_OFS_NXTBLK,
			.size	= MTDPART_SIZ_FULL
		},
	},
	.timing_data = &(struct stm_nand_timing_data) {
		.sig_setup      = 10,           /* times in ns */
		.sig_hold       = 10,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on          = 10,
		.wr_off         = 30,
		.rd_on          = 10,
		.rd_off         = 30,
		.chip_delay     = 30,           /* in us */
	},
};

static struct platform_device *adi7108_devices[] __initdata = {
	&adi7108_nor_flash,
};

static int __init device_init(void)
{
	struct sysconf_field *sc;
	unsigned long nor_bank_base = 0;
	unsigned long nor_bank_size = 0;

	/*
	 *
	 * BootUp	RE32 & RE35	RE33 & RE34
	 * NOR		    0R		    NC
	 * NAND/SPI	    NC		    0R
	 *
	 */

	/* Configure Flash according to boot-device
	 *
	 * [Only tested on boot-from-NOR, board-mod required for
	 * boot-from-NAND/SPI]
	 */
	sc = sysconf_claim(SYS_STA_BANK1, 3, 2, 6, "boot_device");
	switch (sysconf_read(sc)) {
	case 0x15:
		pr_info("Configuring FLASH for boot-from-NOR\n");
		nor_bank_base = emi_bank_base(0);
		nor_bank_size = emi_bank_base(1) - nor_bank_base;
		adi7108_nand_flash.csn = 1;
		break;
	case 0x14:
		pr_info("Configuring FLASH for boot-from-NAND\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		adi7108_nand_flash.csn = 0;
		break;
	case 0x1a:
		pr_info("Configuring FLASH for boot-from-SPI\n");
		nor_bank_base = emi_bank_base(1);
		nor_bank_size = emi_bank_base(2) - nor_bank_base;
		adi7108_nand_flash.csn = 0;
		break;
	default:
		BUG();
		break;
	}
	sysconf_release(sc);

	/* Update NOR Flash base address and size: */
	/*     - limit bank size to 64MB (some targetpacks define 128MB!) */
	if (nor_bank_size > 64*1024*1024)
		nor_bank_size = 64*1024*1024;
	/*     - reduce visibility of NOR flash to EMI bank size */
	if (adi7108_nor_flash.resource[0].end > nor_bank_size - 1)
		adi7108_nor_flash.resource[0].end = nor_bank_size - 1;
	/*     - update resource parameters */
	adi7108_nor_flash.resource[0].start += nor_bank_base;
	adi7108_nor_flash.resource[0].end += nor_bank_base;


	/* NIM */
	stx7108_configure_ssc_i2c(1, NULL);

	/* AV */
	stx7108_configure_ssc_i2c(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio14_4,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio14_5, });

	/* EEPROM */
	stx7108_configure_ssc_i2c(5, NULL);

	/* HDMI */
	stx7108_configure_ssc_i2c(6, NULL);

	stx7108_configure_lirc(&(struct stx7108_lirc_config) {
			.rx_mode = stx7108_lirc_rx_mode_ir, });

	stx7108_configure_usb(0);
	stx7108_configure_usb(1);
	stx7108_configure_usb(2);

/*-------------------------------------------------------------
 * 		      |  Ver1.0    VerB    VerC      VerD
 * SATA 0/1 connector | E-sata0+1 E-sata0 E-sata0 Sata on board
 * PCI-e Connector    |		   √	   √
 *-------------------------------------------------------------*/
#if defined(CONFIG_SH_ST_ADI7108_VER_B_BOARD) || \
	defined(CONFIG_SH_ST_ADI7108_VER_C_BOARD)

	stx7108_configure_miphy(&(struct stx7108_miphy_config) {
			.modes = (enum miphy_mode[2]) {
				SATA_MODE, PCIE_MODE },
			});

	stx7108_configure_sata(0, &(struct stx7108_sata_config) { });

#elif defined(CONFIG_SH_ST_ADI7108_VER_D_BOARD) || \
	defined(CONFIG_SH_ST_ADI7108_VER_1_0_BOARD)
	stx7108_configure_miphy(&(struct stx7108_miphy_config) {
			.modes = (enum miphy_mode[2]) {
				SATA_MODE, SATA_MODE },
			});

	stx7108_configure_sata(0, &(struct stx7108_sata_config) { });
	stx7108_configure_sata(1, &(struct stx7108_sata_config) { });
#endif

#ifdef CONFIG_SH_ST_ADI7108_STMMAC0
	/* By default the RJ45 connector is removed on this board. */

	gpio_request(ADI7108_PIO_POWER_ON_ETHERNET0, "POWER_ON_ETHERNET");
	gpio_direction_output(ADI7108_PIO_POWER_ON_ETHERNET0, 0);


	stx7108_configure_ethernet(0, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac0_mdio_bus,
		});
#endif /* CONFIG_SH_ST_ADI7108_STMMAC0 */
	/* To use the MII/GMII mode.
	 *
	 *		RP1 	MII1_EN
	 *	GMII    NC      1
	 *	MII     51R     0
	 *
	 */
	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(ADI7108_PIO_POWER_ON_ETHERNET1, "POWER_ON_ETHERNET");
	gpio_direction_output(ADI7108_PIO_POWER_ON_ETHERNET1, 0);

	gpio_request(ADI7108_GPIO_MII1_SPEED_SEL, "stmmac");
	gpio_direction_output(ADI7108_GPIO_MII1_SPEED_SEL, 0);

	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_gmii_gtx,
			.ext_clk = 0,
			.phy_bus = 1,
			.txclk_select = adi7108_mii_txclk_select,
			.phy_addr = 1,
			.mdio_bus_data = &stmmac1_mdio_bus,
		});

	stx7108_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &adi7108_nand_flash,
			.rbn.flex_connected = 1,});

	stx7108_configure_spifsm(&adi7108_serial_flash);

	stx7108_configure_mmc(0);

	return platform_add_devices(adi7108_devices,
			ARRAY_SIZE(adi7108_devices));
}
arch_initcall(device_init);


static void __iomem *adi7108_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_adi7108 __initmv = {
	.mv_name = "adi7108",
	.mv_setup = adi7108_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = adi7108_ioport_map,
};

#ifdef CONFIG_HIBERNATION_ON_MEMORY
int stm_defrost_board(void *data)
{
	gpio_direction_output(ADI7108_PIO_POWER_ON_ETHERNET1, 0);

	gpio_direction_output(ADI7108_GPIO_MII1_SPEED_SEL, 0);

	/*
	 * adi7108_phy_reset(...);
	 */
	gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET1, 0);
	udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET1, 1);

	return 0;
}
#endif
