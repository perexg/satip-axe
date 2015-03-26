/*
 * arch/sh/boards/st/mb411/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7100 MBoard board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/phy.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7100.h>
#include <sound/stm.h>
#include <asm/io.h>
#include <mach/epld.h>
#include <asm/irq.h>
#include <asm/irq-stx7100.h>
#include <mach/common.h>




static void __init mb411_setup(char** cmdline_p)
{
	printk("STMicroelectronics STb7100 MBoard board initialisation\n");

	stx7100_early_device_init();

	stx7100_configure_asc(2, &(struct stx7100_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7100_configure_asc(3, &(struct stx7100_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}



static struct resource mb411_smc91x_resources[] = {
	[0] = {
		.start	= 0x03e00300,
		.end	= 0x03e00300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 7,
		.end	= 7,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device mb411_smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(mb411_smc91x_resources),
	.resource	= mb411_smc91x_resources,
};



static void mb411_mtd_set_vpp(struct map_info *map, int vpp)
{
	/* Bit 0: VPP enable
	 * Bit 1: Reset (not used in later EPLD versions)
	 */

	if (vpp) {
		epld_write(3, EPLD_FLASH);
	} else {
		epld_write(2, EPLD_FLASH);
	}
}

static struct platform_device mb411_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 32*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= mb411_mtd_set_vpp,
	},
};



static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_mask = 0,
};

static struct platform_device mb411_epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= EPLD_BASE,
			.end	= EPLD_BASE + EPLD_SIZE - 1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 8,
	},
};



/* Unfortunately PDN and SMUTE can't be controlled using
 * default audio EPLD "firmware"...
 * That is why dummy converter is enough. */
static struct platform_device mb411_snd_ext_dacs = {
	.name = "snd_conv_dummy",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "External DACs",
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 9,
		.format = SND_STM_FORMAT__LEFT_JUSTIFIED |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
	},
};



static struct platform_device *mb411_devices[] __initdata = {
	&mb411_epld_device,
	&mb411_smc91x_device,
	&mb411_physmap_flash,
	&mb411_snd_ext_dacs,
};

static int __init mb411_device_init(void)
{
	stx7100_configure_sata();

	stx7100_configure_pwm(&(struct stx7100_pwm_config) {
			.out0_enabled = 1,
			.out1_enabled = 1, });

	stx7100_configure_ssc_i2c(0);
	stx7100_configure_ssc_spi(1, NULL);
	stx7100_configure_ssc_i2c(2);

	stx7100_configure_usb();

	stx7100_configure_lirc(&(struct stx7100_lirc_config) {
			.rx_mode = stx7100_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 1, });

	stx7100_configure_ethernet(&(struct stx7100_ethernet_config) {
			.mode = stx7100_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	/* Set the EPLD ATAPI register to 1, enabling the IDE interface.*/
	epld_write(1, EPLD_ATAPI);
	stx7100_configure_pata(&(struct stx7100_pata_config) {
			.emi_bank = 3,
			.pc_mode = 1,
			.irq = 8, });

	/* Initialize audio EPLD */
	epld_write(0x1, EPLD_DAC_PNOTS);
	epld_write(0x7, EPLD_DAC_SPMUX);

	return platform_add_devices(mb411_devices,
			ARRAY_SIZE(mb411_devices));
}
device_initcall(mb411_device_init);

static void __iomem *mb411_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb411_init_irq(void)
{
	unsigned long epldver;
	unsigned long pcbver;

	epld_early_init(&mb411_epld_device);

	epldver = epld_read(EPLD_EPLDVER);
	pcbver = epld_read(EPLD_PCBVER);
	printk("EPLD v%ldr%ld, PCB ver %lX\n",
	       epldver >> 4, epldver & 0xf, pcbver);

	/* Set the ILC to route external interrupts to the the INTC */
	/* Outputs 0-3 are the interrupt pins, 4-7 are routed to the INTC */
	ilc_route_external(ILC_EXT_IRQ0, 4, 0);
	ilc_route_external(ILC_EXT_IRQ1, 5, 0);
	ilc_route_external(ILC_EXT_IRQ2, 6, 0);

	/* Route e/net PHY interrupt to SH4 - only for STb7109 */
#ifdef CONFIG_STMMAC_ETH
	/* Note that we invert the signal - the ste101p is connected
	   to the mb411 as active low. The sh4 INTC expects active high */
	ilc_route_external(ILC_EXT_MDINT, 7, 1);
#else
	ilc_route_external(ILC_EXT_IRQ3, 7, 0);
#endif

	/* ...where they are handled as normal HARP style (encoded) interrpts */
	harp_init_irq();
}

static struct sh_machine_vector mv_mb411 __initmv = {
	.mv_name		= "mb411",
	.mv_setup		= mb411_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb411_init_irq,
	.mv_ioport_map		= mb411_ioport_map,
};
