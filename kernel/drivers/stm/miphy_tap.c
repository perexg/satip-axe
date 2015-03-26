#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/pio.h>
#include <linux/stm/platform.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/miphy.h>
#include "miphy.h"
#include "tap.h"

#define NAME		"stm-miphy-tap"

struct stm_miphy_tap_device {
	struct stm_miphy_device miphy_dev;
	int c_port;
	int ports;
	struct stm_tap	*tap;
	struct sysconf_field *tms;
	struct sysconf_field *tck;
	struct sysconf_field *tdi;
	struct sysconf_field *tdo;
	struct sysconf_field *tap_en;
	struct sysconf_field *trstn;
};
static struct stm_miphy_tap_device *tap_dev;

static int stm_miphy_tap_tick(int tms, int tdi)
{
	sysconf_write(tap_dev->tck, 0);

	sysconf_write(tap_dev->tms, tms);
	sysconf_write(tap_dev->tdi, tdi);

	sysconf_write(tap_dev->tck, 1);

	return sysconf_read(tap_dev->tdo);
}


#define IR_SIZE				3
#define IR_MACROCELL_RESET_ACCESS	0x4	/* 100 */
#define IR_MACRO_MICRO_BUS_ACCESS	0x5	/* 101 */
#define IR_BYPASS			0x7	/* 111 */
static void stm_miphy_select(int port)
{
	unsigned int value = 0;
	int chain_size;
	int i;

	pr_debug("%s(port=%d)\n", __func__, port);

	if (tap_dev->c_port == port)
		return;

	/* Create instructions chain - MACROMICROBUS_ACCESS for selected
	 * port, BYPASS for all other ones */
	for (i = 0; i < tap_dev->ports; i++) {
		int shift = (tap_dev->ports - 1 - i) * IR_SIZE;

		if (i == port)
			value |= IR_MACRO_MICRO_BUS_ACCESS << shift;
		else
			value |= IR_BYPASS << shift;
	}

	/* Every port has IR_SIZE bits for instruction */
	chain_size = tap_dev->ports * IR_SIZE;

	/* Set the instructions */
	stm_tap_shift_ir(tap_dev->tap, &value, NULL, chain_size);

	tap_dev->c_port = port;
}

#define DR_SIZE		18
#define DR_WR		0x1	/* DR[1..0] = 01 */
#define DR_RD		0x2	/* DR[1..0] = 10 */
#define DR_ADDR_SHIFT	2	/* DR[9..2] */
#define DR_ADDR_MASK	0xff
#define DR_DATA_SHIFT	10	/* DR[17..10] */
#define DR_DATA_MASK	0xff

/* TAP based register read/write functions */
static u8 stm_miphy_tap_reg_read(int port, u8 addr)
{
	unsigned int value = 0;
	int chain_size;

	pr_debug("%s(port=%d, addr=0x%x)\n", __func__, port, addr);

	BUG_ON(addr & ~DR_ADDR_MASK);

	stm_miphy_select(port);

	/* Create "read access" value */
	value = DR_RD | (addr << DR_ADDR_SHIFT);

	/* Shift value to add BYPASS bits for ports farther in chain */
	value <<= tap_dev->ports - 1 - port;

	/* Overall chain length is the DR_SIZE for meaningful
	 * command plus 1 bit per every BYPASSed port */
	chain_size = DR_SIZE + (tap_dev->ports - 1);

	/* Set "read access" command */
	stm_tap_shift_dr(tap_dev->tap, &value, NULL, chain_size);

	/* Now read back value */
	stm_tap_shift_dr(tap_dev->tap, NULL, &value, chain_size);

	/* Remove "BYPASSed" bits */
	value >>= tap_dev->ports - 1 - port;

	/* The RD bit should be cleared by now... */
	BUG_ON(value & DR_RD);

	/* Extract the result */
	value >>= DR_DATA_SHIFT;
	BUG_ON(value & ~DR_DATA_MASK);

	pr_debug("%s()=0x%x\n", __func__, value);

	return value;
}
static void stm_miphy_tap_reg_write(int port, u8 addr, u8 data)
{
	unsigned int value = 0;

	pr_debug("%s(port=%d, addr=0x%x, data=0x%x)\n",
			__func__, port, addr, data);

	BUG_ON(addr & ~DR_ADDR_MASK);
	BUG_ON(data & ~DR_DATA_MASK);

	stm_miphy_select(port);

	/* Create "write access" value */
	value = DR_WR | (addr << DR_ADDR_SHIFT) | (data << DR_DATA_SHIFT);

	/* Shift value to add BYPASS bits for ports farther in chain */
	value <<= tap_dev->ports - 1 - port;

	/* Overall chain length is the DR_SIZE for meaningful
	 * command plus 1 bit per every BYPASSed port */
	stm_tap_shift_dr(tap_dev->tap, &value, NULL,
			DR_SIZE + (tap_dev->ports - 1));
}

static const struct miphy_if_ops stm_miphy_tap_ops = {
	.reg_write = stm_miphy_tap_reg_write,
	.reg_read = stm_miphy_tap_reg_read,
};

static int stm_miphy_tap_probe(struct platform_device *pdev)
{
	struct stm_plat_tap_data *data =
			(struct stm_plat_tap_data *)pdev->dev.platform_data;
	struct tap_sysconf_field *tck = &data->tap_sysconf->tck;
	struct tap_sysconf_field *tms = &data->tap_sysconf->tms;
	struct tap_sysconf_field *tdi = &data->tap_sysconf->tdi;
	struct tap_sysconf_field *tdo = &data->tap_sysconf->tdo;
	struct tap_sysconf_field *tap_en = &data->tap_sysconf->tap_en;
	struct tap_sysconf_field *trstn = &data->tap_sysconf->trstn;
	int result;

	tap_dev = kzalloc(sizeof(struct stm_miphy_tap_device), GFP_KERNEL);
	if (!tap_dev)
		return -ENOMEM;

	tap_dev->tck = sysconf_claim(tck->group, tck->num,
				tck->lsb, tck->msb, NAME);
	BUG_ON(!tap_dev->tck);

	tap_dev->tms = sysconf_claim(tms->group, tms->num,
				tms->lsb, tms->msb, NAME);
	BUG_ON(!tap_dev->tms);

	tap_dev->tdi = sysconf_claim(tdi->group, tdi->num,
				tdi->lsb, tdi->msb, NAME);
	BUG_ON(!tap_dev->tdi);

	tap_dev->tdo = sysconf_claim(tdo->group, tdo->num,
				tdo->lsb, tdo->msb, NAME);
	BUG_ON(!tap_dev->tdo);

	tap_dev->tap_en = sysconf_claim(tap_en->group, tap_en->num,
				tap_en->lsb, tap_en->msb, NAME);
	BUG_ON(!tap_dev->tap_en);
	sysconf_write(tap_dev->tap_en,
			(tap_en->pol == POL_INVERTED) ? 0 : 1);
	/* TMS should be set to 1 when taking the TAP
	 * machine out of reset... */
	sysconf_write(tap_dev->tms, 1);

	/* trstn_sata */
	tap_dev->trstn = sysconf_claim(trstn->group, trstn->num,
			trstn->lsb, trstn->msb, NAME);
	BUG_ON(!tap_dev->trstn);
	sysconf_write(tap_dev->trstn,
			(trstn->pol == POL_INVERTED) ? 0 : 1);

	udelay(100);

	tap_dev->ports = data->miphy_count;
	tap_dev->c_port = -1;
	tap_dev->tap = stm_tap_init(stm_miphy_tap_tick);

	stm_tap_enable(tap_dev->tap);

	result = miphy_if_register(&tap_dev->miphy_dev, TAP_IF,
			data->miphy_first, data->miphy_count, data->miphy_modes,
			&pdev->dev, &stm_miphy_tap_ops);

	if (result) {
		printk(KERN_ERR "Unable to Register TAP MiPHY device\n");
		return result;
	}

	return 0;
}

static int stm_miphy_tap_remove(struct platform_device *pdev)
{

	stm_tap_disable(tap_dev->tap);
	stm_tap_free(tap_dev->tap);

	miphy_if_unregister(&tap_dev->miphy_dev);
	/* free the memory and sysconf */
	sysconf_release(tap_dev->tck);
	sysconf_release(tap_dev->tms);
	sysconf_release(tap_dev->tdi);
	sysconf_release(tap_dev->tdo);

	kfree(tap_dev);
	tap_dev = NULL;

	return 0;
}

static struct platform_driver stm_miphy_tap_driver = {
	.driver.name = NAME,
	.driver.owner = THIS_MODULE,
	.probe = stm_miphy_tap_probe,
	.remove = stm_miphy_tap_remove,
};

static int __init stm_miphy_tap_init(void)
{
	return platform_driver_register(&stm_miphy_tap_driver);
}

postcore_initcall(stm_miphy_tap_init);

MODULE_AUTHOR("STMicroelectronics @st.com");
MODULE_DESCRIPTION("STM MiPHY TAP driver");
MODULE_LICENSE("GPL");
