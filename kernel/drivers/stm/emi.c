/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/sysdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/stm/emi.h>
#include <linux/stm/device.h>
#include <linux/stm/clk.h>


#define EMI_GEN_CFG			0x0028
#define EMI_BANKNUMBER			0x0860
#define EMI_BANK_ENABLE			0x0280
#define BANK_BASEADDRESS(b)		(0x800 + (0x10 * b))
#define BANK_EMICONFIGDATA(b, r)	(0x100 + (0x40 * b) + (8 * r))
#define EMI_COMMON_CFG(reg)		(0x10 + (0x8 * (reg)))


static struct platform_device *emi;
static struct clk *emi_clk;
#define emi_initialised			(emi != NULL)
static unsigned long emi_memory_base;
static void __iomem *emi_control;
static struct stm_device_state *emi_device_state;

static inline void emi_clk_xxable(int enable)
{
	if (!emi_clk || IS_ERR(emi_clk))
		return;

	if (enable)
		clk_enable(emi_clk);
	else
		clk_disable(emi_clk);
}

static inline void emi_clk_enable(void)
{
	emi_clk_xxable(1);
}

static inline void emi_clk_disable(void)
{
	emi_clk_xxable(0);
}


unsigned long emi_bank_base(int bank)
{
	unsigned long reg;

	BUG_ON(bank < 0 || bank >= EMI_BANKS);
	BUG_ON(!emi_initialised);

	reg = readl(emi_control + BANK_BASEADDRESS(bank));

	return emi_memory_base + (reg << 22);
}
EXPORT_SYMBOL_GPL(emi_bank_base);

void emi_bank_configure(int bank, unsigned long data[4])
{
	int i;

	BUG_ON(bank < 0 || bank >= EMI_BANKS);
	BUG_ON(!emi_initialised);

	for (i = 0; i < 4; i++)
		writel(data[i], emi_control + BANK_EMICONFIGDATA(bank, i));
}
EXPORT_SYMBOL_GPL(emi_bank_configure);

void emi_config_pcmode(int bank, int pc_mode)
{
	int mask;

	BUG_ON(!emi_initialised);

	switch (bank) {
	case 2:	/* Bank C */
		mask = 1<<3;
		break;
	case 3:	/* Bank D */
		mask = 1<<4;
		break;
	default:
		mask = 0;
		break;
	}

	if (mask) {
		u32 val = readl(emi_control + EMI_GEN_CFG);
		if (pc_mode)
			val |= mask;
		else
			val &= (~mask);
		writel(val, emi_control + EMI_GEN_CFG);
	}
}
EXPORT_SYMBOL_GPL(emi_config_pcmode);

/*
 *                ______________________________
 * EMIADDR    ___/                              \________
 *               \______________________________/
 *
 * (The cycle time specified in nano seconds)
 *
 *                |-----------------------------| cycle_time
 *                 ______________                ___________
 * CYCLE_TIME     /              \______________/
 *
 *
 * (IORD_start the number of nano seconds after the start of the cycle the
 * RD strobe is asserted IORD_end the number of nano seconds before the
 * end of the cycle the RD strobe is de-asserted.)
 *                   _______________________
 * IORD       ______/                       \________
 *
 *               |--|                       |---|
 *                 ^--- IORD_start            ^----- IORD_end
 *
 * (RD_latch the number of nano seconds at the end of the cycle the read
 * data is latched)
 *                                  __
 * RD_LATCH  ______________________/__\________
 *
 *                                 |------------|
 *                                      ^---------- RD_latch
 *
 * (IOWR_start the number of nano seconds after the start of the cycle the
 * WR strobe is asserted IOWR_end the number of nano seconds before the
 * end of the cycle the WR strobe is de-asserted.)
 *                   _______________________
 * IOWR       ______/                       \________
 *
 *               |--|                       |---|
 *                 ^--- IOWR_start            ^----- IOWR_end
 */



/* NOTE: these calculations assume a 100MHZ clock */



static void __init set_pata_read_timings(int bank, int cycle_time,
		int IORD_start, int IORD_end, int RD_latch)
{
	cycle_time = cycle_time / 10;
	IORD_start = IORD_start / 5;
	IORD_end = IORD_end / 5;
	RD_latch = RD_latch / 10;

	writel((cycle_time << 24) | (IORD_start << 8) | (IORD_end << 12),
			emi_control + BANK_EMICONFIGDATA(bank, 1));
	writel(0x791 | (RD_latch << 20),
			emi_control + BANK_EMICONFIGDATA(bank, 0));
}

static void __init set_pata_write_timings(int bank, int cycle_time,
		int IOWR_start, int IOWR_end)
{
	cycle_time = cycle_time / 10;
	IOWR_start = IOWR_start / 5;
	IOWR_end = IOWR_end / 5;

	writel((cycle_time << 24) | (IOWR_start << 8) | (IOWR_end << 12),
			emi_control + BANK_EMICONFIGDATA(bank, 2));
}

void __init emi_config_pata(int bank, int pc_mode)
{
	BUG_ON(bank < 0 || bank >= EMI_BANKS);
	BUG_ON(!emi_initialised);

	/* Set timings for PIO4 */
	set_pata_read_timings(bank, 120, 35, 30, 20);
	set_pata_write_timings(bank, 120, 35, 30);

	emi_config_pcmode(bank, pc_mode);

}

static void set_nand_read_timings(int bank, int cycle_time,
		int IORD_start, int IORD_end,
		int RD_latch, int busreleasetime,
		int wait_active_low )
{
	cycle_time = cycle_time / 10;		/* cycles */
	IORD_start = IORD_start / 5;		/* phases */
	IORD_end = IORD_end / 5;		/* phases */
	RD_latch = RD_latch / 10;		/* cycles */
	busreleasetime = busreleasetime / 10;   /* cycles */

	writel(0x04000699 | (busreleasetime << 11) | (RD_latch << 20) | (wait_active_low << 25),
			emi_control + BANK_EMICONFIGDATA(bank, 0));

	writel((cycle_time << 24) | (IORD_start << 12) | (IORD_end << 8),
			emi_control + BANK_EMICONFIGDATA(bank, 1));
}

static void set_nand_write_timings(int bank, int cycle_time,
		int IOWR_start, int IOWR_end)
{
	cycle_time = cycle_time / 10;		/* cycles */
	IOWR_start = IOWR_start / 5;		/* phases */
	IOWR_end   = IOWR_end / 5;		/* phases */

	writel((cycle_time << 24) | (IOWR_start << 12) | (IOWR_end << 8),
			emi_control + BANK_EMICONFIGDATA(bank, 2));
}

void emi_config_nand(int bank, struct emi_timing_data *timing_data)
{
	BUG_ON(bank < 0 || bank >= EMI_BANKS);
	BUG_ON(!emi_initialised);

	set_nand_read_timings(bank,
			timing_data->rd_cycle_time,
			timing_data->rd_oee_start,
			timing_data->rd_oee_end,
			timing_data->rd_latchpoint,
			timing_data->busreleasetime,
			timing_data->wait_active_low);

	set_nand_write_timings(bank,
			timing_data->wr_cycle_time,
			timing_data->wr_oee_start,
			timing_data->wr_oee_end);

	/* Disable PC mode */
	emi_config_pcmode(bank, 0);
}
EXPORT_SYMBOL_GPL(emi_config_nand);

void emi_config_pci(void)
{
	u32 tmp;

	BUG_ON(!emi_initialised);

	tmp = readl(emi_control + EMI_GEN_CFG);
	/* bit 16 is undocumented but enables extra pullups on the bus which
	 * is needed for correction operation if the EMI is accessed
	 * simultaneously with PCI
	 */
	writel(tmp | (1 << 16), emi_control + EMI_GEN_CFG);
}
EXPORT_SYMBOL_GPL(emi_config_pci);

#ifdef CONFIG_PM
/*
 * Note on Power Management of EMI device
 * ======================================
 * The EMI is registered twice on different view:
 * 1. as platform_device to acquire the platform specific
 *    capability (via sysconf)
 * 2. as sysdev_device to really manage the suspend/resume
 *    operation on standby and hibernation
 */

/*
 * emi_num_common_cfg = 12 common config	+
 * 			emi_bank_enable(0x280)	+
 *			emi_bank_number(0x860)
 */
#define emi_num_common_cfg	(12 + 2)
#define emi_num_bank		5
#define emi_num_bank_cfg	4

struct emi_pm_bank {
	unsigned long cfg[emi_num_bank_cfg];
	unsigned long base_address;
};

struct emi_pm {
	unsigned long common_cfg[emi_num_common_cfg];
	struct emi_pm_bank bank[emi_num_bank];
};

static int emi_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	int idx;
	int bank, data;
	static struct emi_pm *emi_saved_data;

	switch (state.event) {
	case PM_EVENT_ON:
		if (emi_saved_data) {
			/* restore the previous common value */
			for (idx = 0; idx < emi_num_common_cfg-2; ++idx)
			writel(emi_saved_data->common_cfg[idx],
				emi_control+EMI_COMMON_CFG(idx));
			writel(emi_saved_data->common_cfg[12], emi_control
					+ EMI_BANK_ENABLE);
			writel(emi_saved_data->common_cfg[13], emi_control
					+ EMI_BANKNUMBER);
			/* restore the previous bank values */
			for (bank = 0; bank < emi_num_bank; ++bank) {
			  writel(emi_saved_data->bank[bank].base_address,
				emi_control + BANK_BASEADDRESS(bank));
			  for (data = 0; data < emi_num_bank_cfg; ++data)
				emi_bank_configure(bank, emi_saved_data->bank[bank].cfg);
			}
			kfree(emi_saved_data);
			emi_saved_data = NULL;
		} else
			emi_clk_enable();
		stm_device_power(emi_device_state, stm_device_power_on);
		break;
	case PM_EVENT_SUSPEND:
		stm_device_power(emi_device_state, stm_device_power_off);
		emi_clk_disable();
		break;
	case PM_EVENT_FREEZE:
		emi_saved_data = kmalloc(sizeof(struct emi_pm), GFP_NOWAIT);
		if (!emi_saved_data) {
			printk(KERN_ERR "Unable to freeze the emi registers\n");
			return -ENOMEM;
		}
		/* save the emi common values */
		for (idx = 0; idx < emi_num_common_cfg-2; ++idx)
			emi_saved_data->common_cfg[idx] =
				readl(emi_control + EMI_COMMON_CFG(idx));
		emi_saved_data->common_cfg[12] =
				readl(emi_control + EMI_BANK_ENABLE);
		emi_saved_data->common_cfg[13] =
				readl(emi_control + EMI_BANKNUMBER);
		/* save the emi bank value */
		for (bank  = 0; bank < emi_num_bank; ++bank) {
		  emi_saved_data->bank[bank].base_address =
			readl(emi_control + BANK_BASEADDRESS(bank));
		  for (data = 0; data < emi_num_bank_cfg; ++data)
			emi_saved_data->bank[bank].cfg[data] =
			   readl(emi_control + BANK_EMICONFIGDATA(bank, data));
		}
		/* on hibernation don't turn-off emi for harddisk issue */
		break;
	}
	return 0;
}

static int emi_sysdev_resume(struct sys_device *dev)
{
	return emi_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_class emi_sysdev_class = {
	.name = "emi",
	.suspend = emi_sysdev_suspend,
	.resume = emi_sysdev_resume,
};

struct sys_device emi_sysdev_dev = {
	.id = 0,
	.cls = &emi_sysdev_class,
};

static int __init emi_sysdev_register(void)
{
	int ret;

	ret = sysdev_class_register(&emi_sysdev_class);
	if (ret)
		return ret;

	ret = sysdev_register(&emi_sysdev_dev);
	if (ret)
		return ret;

	return 0;
}
#else
#define emi_sysdev_register()
#endif

static int __init emi_driver_probe(struct platform_device *pdev)
{
	struct resource *res;

	BUG_ON(emi_initialised);

	emi_device_state = devm_stm_device_init(&pdev->dev,
		(struct stm_device_config *)pdev->dev.platform_data);

	if (!emi_device_state)
		return -EBUSY;

	/* acquires control base resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (!request_mem_region(res->start, res->end - res->start, "EMI"))
		return -EBUSY;

	emi_control = ioremap(res->start, res->end - res->start);
	if (emi_control == NULL)
		return -ENOMEM;

	/* acquires mem base resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	emi_memory_base = res->start;

	emi_clk = clk_get(&pdev->dev, "emi_clk");
	if (!emi_clk)
		pr_warning("emi_clk not found!\n");

	emi_clk_enable();
	emi = pdev; /* to say the EMI is initialised */
	return 0;
}

static struct platform_driver emi_driver = {
	.driver.name = "emi",
	.driver.owner = THIS_MODULE,
	.probe = emi_driver_probe,
};

static int __init stm_emi_driver_init(void)
{
	emi_sysdev_register();
	return platform_driver_register(&emi_driver);
}

postcore_initcall(stm_emi_driver_init);
