/*
 *  ---------------------------------------------------------------------------
 *  stm_nand_flex.c STMicroelectronics NAND Flash driver: H/W FLEX mode
 *  ---------------------------------------------------------------------------
 *
 *  Copyright (c) 2009 STMicroelectronics Limited
 *  Author: Angus Clark <Angus.Clark@st.com>
 *
 *  ---------------------------------------------------------------------------
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *  ---------------------------------------------------------------------------
 *
 *  Notes:
 *
 *   - Basic implementation initialises the NAND controller and overrides the
 *     MTD control layer (see flex_cmd_ctrl(), flex_read_byte(),
 *     flex_write_byte(), flex_rbn() and flex_select_chip()).
 *
 *   - FDMA and NAND Controller in FLEX mode has not been validated.  FDMA
 *     support diabled for the moment.
 *
 *   - Support for BOOT mode partitions has been added.  We maintain 2 sets of
 *     ECC-related parameters, and override the MTD read/write functions to
 *     switch ECC scheme depending on which partition we are in (see
 *     flex_setup_eccparam(), flex_select_eccparams(), nand_read(),
 *     nand_read_oob(), nand_write, nand_write_oob()).  Need to force
 *     chip.options NAND_USE_FLASH_BBT, since BOOT mode ECC layout is
 *     incompatible with factory-written bad-block markers.
 *
 *  TODO:
 *
 *    - Test board with multiple NAND devices
 *
 *    - Optimise nand_command and nand_command_lp to issue multibyte commands in
 *      one go.
 *
 *    - Add support for x16 devices.  Should just be a matter of configuring
 *      FLEX data register...
 *
 *  Changelog:
 *	2009-03-12 Angus Clark <angus.clark@st.com>
 *		- first version
 *
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <linux/clk.h>
#include <linux/stm/stm-dma.h>
#include <linux/stm/platform.h>
#include <linux/stm/nand.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "stm_nandc_regs.h"

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#define NAME	"stm-nand-flex"

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
#include "stm_nand_ecc.h"
/* NAND_BOOT uses a different ECC scheme to that of NAND_FLEX/NAND_AFM.  In
 * order to support the NAND_BOOT partition we need to maintain 2 sets of
 * ECC-related paramters, and switch depending which partition we wish to
 * access.
 */
struct ecc_params {
	/* nand_chip params */
	struct nand_ecc_ctrl	ecc_ctrl;
	int			subpagesize;

	/* mtd_info params */
	u_int32_t		subpage_sft;
};
#endif /* CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT */

/* NAND device connected to STM NAND Controller operatring in FLEX mode.  (There
 * may be several NAND device connected to the NAND controller.)
 */
struct stm_nand_flex_device {

	struct nand_chip	chip;
	struct mtd_info		mtd;
	int			csn;

	struct stm_nand_timing_data *timing_data;

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
	unsigned long		boot_start;
	unsigned long		boot_end;
	struct ecc_params	ecc_boot;
	struct ecc_params	ecc_flex;
#endif

#ifdef CONFIG_MTD_PARTITIONS
	/* Partition Table */
	int			nr_parts;
	struct mtd_partition	*parts;
#endif

};

/* STM NAND Controller operating in FLEX mode */
struct stm_nand_flex_controller {
	struct resource		*mem_region;
	void __iomem		*base_addr;

	int			current_csn;		/* Current chip	      */

	struct nand_hw_control	hwcontrol;		/* Aribitrate access  */
							/* to FLEX devices    */

	int			initialised;

	uint8_t			*buf;			/* Bounce buffer for  */
							/* non-aligned xfer   */
#ifdef CONFIG_STM_NAND_FLEX_CACHED
	unsigned long		data_phys;		/* FLEX data IO */
	void __iomem		*data_cached;
	spinlock_t		lock;
#endif

	struct stm_nand_flex_device *devices[0];
};

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
/* The command line passed to nboot_setup() */
__initdata static char *cmdline;
#endif

static struct stm_nand_flex_controller* mtd_to_flex(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_hw_control *controller = chip->controller;
	struct stm_nand_flex_controller *flex;

	flex = container_of(controller, struct stm_nand_flex_controller,
			    hwcontrol);
	return flex;
}

#define flex_writereg(val, reg)	iowrite32(val, flex->base_addr + (reg))
#define flex_readreg(reg)	ioread32(flex->base_addr + (reg))

#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif

/*** FLEX mode control functions (cf nand_base.c) ***/

/* Assumes EMINAND_DATAREAD has been configured for 1-byte reads. */
static uint8_t flex_read_byte(struct mtd_info *mtd)
{
	struct stm_nand_flex_controller *flex = mtd_to_flex(mtd);
	uint32_t reg;

	reg =  flex_readreg(EMINAND_FLEX_DATA);

	return (uint8_t)(reg & 0xff);
}

static void flex_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct stm_nand_flex_controller *flex = mtd_to_flex(mtd);
	static unsigned int flex_ctrl;
	uint32_t reg;

	if (ctrl & NAND_CTRL_CHANGE)
		flex_ctrl = ctrl;

	if (cmd != NAND_CMD_NONE) {
		if (flex_ctrl & NAND_CLE) {
			reg = (cmd & 0xff) | FLX_CMD_REG_BEAT_1 |
				FLX_CMD_REG_CSN_STATUS;
			flex_writereg(reg, EMINAND_FLEX_COMMAND_REG);
		} else if (flex_ctrl & NAND_ALE) {
			reg = (cmd & 0xff) | FLX_ADDR_REG_ADD8_VALID |
				FLX_ADDR_REG_BEAT_1 | FLX_ADDR_REG_CSN_STATUS;
			flex_writereg(reg, EMINAND_FLEX_ADDRESS_REG);
		} else {
			printk(KERN_ERR NAME "%s: unknown ctrl 0x%02x!\n",
			       __FUNCTION__, flex_ctrl);
		}
	}
}

/*
 * Override the default nand_wait() function.  If we have access to
 * RBn/dev_ready(), then we wait until RBn is asserted before issuing the
 * NAND_CMD_STATUS command.  If RBn is not available, then we revert to the
 * default behaviour and accept a bus stall until the NAND device becomes ready.
 */
static int flex_nand_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	unsigned long timeo = jiffies;
	int status, state = chip->state;

	if (state == FL_ERASING)
		timeo += (HZ * 400) / 1000;
	else
		timeo += (HZ * 20) / 1000;

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	ndelay(100);

	if (chip->dev_ready) {
		/* If we have access to RBn */
		while (time_before(jiffies, timeo)) {
			if (chip->dev_ready(mtd))
				break;
			cond_resched();
		}
		chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);
	} else {
		/* Else read NAND status register
		 * (which will stall bus until NAND device is ready) */
		chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);
		while (time_before(jiffies, timeo)) {
			if (chip->read_byte(mtd) & NAND_STATUS_READY)
				break;
			cond_resched();
		}
	}

	/* Get operation status */
	status = (int)chip->read_byte(mtd);
	return status;
}

static int flex_rbn(struct mtd_info *mtd)
{
	struct stm_nand_flex_controller *flex = mtd_to_flex(mtd);

	/* Apply a small delay before sampling RBn signal */
	ndelay(100);
	return (flex_readreg(EMINAND_RBN_STATUS) & (0x4)) ? 1 : 0;
}

/* FLEX mode ECC requires 4-byte read/writes.  To maintain compatibility with
 * MTD framework, the FLEX data register is, by default, configured for 1-byte
 * read/writes.  Therefore, we must switch temporarily to 4-byte read/writes.
 * In addition, readsl/writesl requires buf to be 4-byte aligned.  If necessary
 * we use flex.buf as bounce buffer.
 */
#ifdef CONFIG_STM_NAND_FLEX_CACHED
static void flex_read_buf_cached(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct stm_nand_flex_controller *flex = mtd_to_flex(mtd);
	unsigned long irq_flags;
	uint8_t *p;
	int notaligned;
	int lenaligned;

	notaligned = ((uint32_t)buf & 0x3) || (len & 0x3);

	/* Handle non-4-byte-aligned buffer */
	if (notaligned) {
		p = flex->buf;
		lenaligned = (len + 0x3) & ~0x3;
	} else {
		p = buf;
		lenaligned = len;
	}

	/* Switch to 4-byte reads */
	flex_writereg(FLX_DATA_CFG_BEAT_4 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAREAD_CONFIG);

	while (lenaligned > 0) {
		spin_lock_irqsave(&(flex->lock), irq_flags);
		invalidate_ioremap_region(flex->data_phys,
					  flex->data_cached, 0, L1_CACHE_BYTES);

		memcpy_fromio(p, flex->data_cached,
			      min(lenaligned, (int)L1_CACHE_BYTES));
		spin_unlock_irqrestore(&(flex->lock), irq_flags);

		p += L1_CACHE_BYTES;
		lenaligned -= L1_CACHE_BYTES;
	}

#ifdef CONFIG_STM_L2_CACHE
	/* If L2 cache is enabled, we must ensure the cacheline is evicted prior
	 * to non-cached writes to FLEX_DATA.
	 */
	invalidate_ioremap_region(flex->data_phys,
				  flex->data_cached, 0, L1_CACHE_BYTES);
#endif

	if (notaligned)
		memcpy(buf, flex->buf, len);

	/* Switch back to 1-byte reads */
	flex_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAREAD_CONFIG);
}
#else
static void flex_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct stm_nand_flex_controller *flex = mtd_to_flex(mtd);
	uint8_t *p;
	int notaligned;
	uint32_t lenaligned;

	notaligned = ((uint32_t)buf & 0x3) || (len & 0x3);

	/* Handle non-aligned buffer */
	if (notaligned) {
		p = flex->buf;
		lenaligned = (len + 0x3) & ~0x3;
	} else {
		p = buf;
		lenaligned = len;
	}

	/* Switch to 4-byte reads (required for ECC) */
	flex_writereg(FLX_DATA_CFG_BEAT_4 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAREAD_CONFIG);

	readsl(flex->base_addr + EMINAND_FLEX_DATA, p, lenaligned/4);

	if (notaligned)
		memcpy(buf, p, len);

	/* Switch back to 1-byte reads */
	flex_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAREAD_CONFIG);

}
#endif

static void flex_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct stm_nand_flex_controller *flex = mtd_to_flex(mtd);
	const uint8_t *p;

	BUG_ON(len & 0x3);

	/* Handle non-aligned buffer */
	if ((uint32_t)buf & 0x3) {
		p = flex->buf;
		memcpy(flex->buf, buf, len);
	} else {
		p = buf;
	}

	/* Switch to 4-byte reads (required for ECC) */
	flex_writereg(FLX_DATA_CFG_BEAT_4 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAWRITE_CONFIG);

	writesl(flex->base_addr + EMINAND_FLEX_DATA, p, len/4);

	/* Switch back to 1-byte writes  */
	flex_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAWRITE_CONFIG);

}

static int flex_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct stm_nand_flex_controller *flex = mtd_to_flex(mtd);
	uint32_t *p = (uint32_t *)buf;
	uint32_t d;
	int ret = 0;
	int i;

	/* Switch to 4-byte reads */
	flex_writereg(FLX_DATA_CFG_BEAT_4 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAREAD_CONFIG);

	for (i = 0; i < len/4; i++) {
		d = readl(flex->base_addr + EMINAND_FLEX_DATA);
		if (d != *p++) {
			ret = -EFAULT;
			goto out1;
		}
	}

 out1:
	/* Switch back to 1-byte reads */
	flex_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAREAD_CONFIG);

	return ret;
}

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
/* The STMicroelectronics NAND boot-controller uses 3 bytes ECC per 128-byte
 * data record.  However, the ECC layout clashes with the factory-set bad-block
 * markers.  To help distinguish between boot-mode ECC blocks and factory-set
 * bad-blocks, we use the 4th byte of each OOB record to store a boot-mode ECC
 * marker.
 */
static struct nand_ecclayout boot_oob_16 = {
	.eccbytes = 16,
	.eccpos = {
		0, 1, 2, 3,	/* 1st 128-byte record: ECC0, ECC1, ECC2, B */
		4, 5, 6, 7,	/* 2st 128-byte record: ECC0, ECC1, ECC2, B */
		8, 9, 10, 11,	/*                ...                       */
		12, 13, 14, 15
	},
	.oobfree = {{0, 0} },	/* No free space in OOB */
};

static struct nand_ecclayout boot_oob_64 = {
	.eccbytes = 64,
	.eccpos = {
		0, 1, 2, 3,	/* 1st 128-byte record: ECC0, ECC1, ECC2, B */
		4, 5, 6, 7,	/* 2st 128-byte record: ECC0, ECC1, ECC2, B */
		8,  9, 10, 11,	/*                ...                       */
		12, 13, 14, 15,
		16, 17, 18, 19,
		20, 21, 22, 23,
		24, 25, 26, 27,
		28, 29, 30, 31,
		32, 33, 34, 35,
		36, 37, 38, 39,
		40, 41, 42, 43,
		44, 45, 46, 47,
		48, 49, 50, 51,
		52, 53, 54, 55,
		56, 57, 58, 59,
		60, 61, 62, 63
	},
	.oobfree = {{0, 0} },	/* No free OOB bytes */
};

static uint8_t scan_ff_pattern[] = { 0xff, 0xff };
static struct nand_bbt_descr bbt_scan_sp = {
	.options = NAND_BBT_SCAN2NDPAGE | NAND_BBT_SCANSTMBOOTECC,
	.offs = 5,
	.len = 1,
	.pattern = scan_ff_pattern
};

static struct nand_bbt_descr bbt_scan_lp = {
	.options = NAND_BBT_SCAN2NDPAGE | NAND_BBT_SCANSTMBOOTECC,
	.offs = 0,
	.len = 2,
	.pattern = scan_ff_pattern
};

/* Update 'badblock_pattern' to handle STM boot-mode ECC prior to bad-block
 * scanning */
static int scan_bbt_stmecc(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	chip->badblock_pattern = (mtd->writesize > 512) ?
		&bbt_scan_lp : &bbt_scan_sp;

	return nand_default_bbt(mtd);
}


/* Replicated from ../mtdpart.c: required here to get slave MTD offsets and
 * determine which ECC mode to use.
 */
struct mtd_part {
	struct mtd_info mtd;
	struct mtd_info *master;
	u_int32_t offset;
	struct list_head list;
};

#define PART(x)  ((struct mtd_part *)(x))

/* Boot mode ECC calc/correct function */
int boot_calc_ecc(struct mtd_info *mtd, const unsigned char *buf,
		  unsigned char *ecc)
{
	stm_ecc_gen(buf, ecc, ECC_128);
	ecc[3] = 'B';

	return 0;
}

int boot_correct_ecc(struct mtd_info *mtd, unsigned char *buf,
		     unsigned char *read_ecc, unsigned char *calc_ecc)
{
	int status;

	status = stm_ecc_correct(buf, read_ecc, calc_ecc, ECC_128);

	/* convert to MTD-compatible status */
	if (status == E_NO_CHK)
		return 0;
	if (status == E_D1_CHK || status == E_C1_CHK)
		return 1;

	return -1;
}

/* Setup ECC params for NAND_BOOT and NAND_FLEX.  The intial 'nand_scan()' sets
 * up ECC parameters for NAND_FLEX mode.  Here, we take a copy of the exisiting
 * NAND_FLEX paramters, and derive a set of NAND_BOOT paramters.
 */
static void flex_setup_eccparams(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_flex_device *data = chip->priv;

	/* Take a copy of ECC FLEX params, as set up during nand_scan() */
	data->ecc_flex.ecc_ctrl = chip->ecc;
	data->ecc_flex.subpagesize = chip->subpagesize;
	data->ecc_flex.subpage_sft = mtd->subpage_sft;

	/* Set ECC BOOT params */
	data->ecc_boot.ecc_ctrl = data->ecc_flex.ecc_ctrl;
	data->ecc_boot.ecc_ctrl.calculate = boot_calc_ecc;
	data->ecc_boot.ecc_ctrl.correct = boot_correct_ecc;
	data->ecc_boot.ecc_ctrl.size = 128;
	data->ecc_boot.ecc_ctrl.bytes = 4;

	if (mtd->oobsize == 16)
		data->ecc_boot.ecc_ctrl.layout = &boot_oob_16;
	else
		data->ecc_boot.ecc_ctrl.layout = &boot_oob_64;

	data->ecc_boot.ecc_ctrl.layout->oobavail = 0;
	data->ecc_boot.ecc_ctrl.steps = mtd->writesize /
		data->ecc_boot.ecc_ctrl.size;
	if (data->ecc_boot.ecc_ctrl.steps * data->ecc_boot.ecc_ctrl.size !=
	    mtd->writesize) {
		printk(KERN_WARNING "Invalid ECC parameters\n");
		BUG();
	}
	data->ecc_boot.ecc_ctrl.total = data->ecc_boot.ecc_ctrl.steps *
		data->ecc_boot.ecc_ctrl.bytes;

	if (!(chip->options & NAND_NO_SUBPAGE_WRITE) &&
	    !(chip->cellinfo & NAND_CI_CELLTYPE_MSK)) {
		switch (data->ecc_boot.ecc_ctrl.steps) {
		case 2:
			data->ecc_boot.subpage_sft = 1;
			break;
		case 4:
		case 8:
		case 16:
			data->ecc_boot.subpage_sft = 2;
			break;
		}
	}
	data->ecc_boot.subpagesize = mtd->writesize >>
		data->ecc_boot.subpage_sft;
}

/* Set MTD to use ECC params */
static void flex_set_eccparams(struct mtd_info *mtd, struct ecc_params *params)
{
	struct nand_chip *chip = mtd->priv;

	chip->ecc = params->ecc_ctrl;
	chip->subpagesize = params->subpagesize;

	mtd->oobavail = params->ecc_ctrl.layout->oobavail;
	mtd->subpage_sft = params->subpage_sft;
}

static void flex_select_eccparams(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_flex_device *data = chip->priv;

	if (offs >= data->boot_start &&
	    offs < data->boot_end) {
		if (chip->ecc.layout != data->ecc_boot.ecc_ctrl.layout) {
			DEBUG(MTD_DEBUG_LEVEL0, NAME
			      ": Switching to BOOT mode ECC\n");
			flex_set_eccparams(mtd, &data->ecc_boot);
		}
	} else {
		if (chip->ecc.layout != data->ecc_flex.ecc_ctrl.layout) {
			DEBUG(MTD_DEBUG_LEVEL0, NAME
			      ": Switching to FLEX mode ECC\n");
			flex_set_eccparams(mtd, &data->ecc_flex);
		}
	}
}

/* nand_base.c functions required by MTD interface functions defined below */
int nand_get_device(struct nand_chip *chip, struct mtd_info *mtd,
		    int new_state);
void nand_release_device(struct mtd_info *mtd);
int nand_do_read_oob(struct mtd_info *mtd, loff_t from,
		     struct mtd_oob_ops *ops);
int nand_do_read_ops(struct mtd_info *mtd, loff_t from,
		     struct mtd_oob_ops *ops);
int nand_do_write_oob(struct mtd_info *mtd, loff_t to,
		      struct mtd_oob_ops *ops);
int nand_do_write_ops(struct mtd_info *mtd, loff_t to,
		      struct mtd_oob_ops *ops);

/* Override the following functions (see nand_base.c), so that we can switch ECC
 * scheme before starting xfer sequence.
 */

/**
 * nand_read - [MTD Interface] MTD compability function for nand_do_read_ecc
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @len:	number of bytes to read
 * @retlen:	pointer to variable to store the number of read bytes
 * @buf:	the databuffer to put data
 *
 * Get hold of the chip and call nand_do_read
 */
static int nand_read(struct mtd_info *mtd, loff_t from, size_t len,
		     size_t *retlen, uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

	/* Do not allow reads past end of device */
	if ((from + len) > mtd->size)
		return -EINVAL;
	if (!len)
		return 0;

	nand_get_device(chip, mtd, FL_READING);

	/** Added to support switching ECC format **/
	flex_select_eccparams(mtd, from);

	chip->ops.len = len;
	chip->ops.datbuf = buf;
	chip->ops.oobbuf = NULL;

	ret = nand_do_read_ops(mtd, from, &chip->ops);

	*retlen = chip->ops.retlen;

	nand_release_device(mtd);

	return ret;
}

/**
 * nand_read_oob - [MTD Interface] NAND read data and/or out-of-band
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob operation description structure
 *
 * NAND read data and/or out-of-band data
 */
static int nand_read_oob(struct mtd_info *mtd, loff_t from,
			 struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;
	int ret = -ENOTSUPP;

	ops->retlen = 0;

	/* Do not allow reads past end of device */
	if (ops->datbuf && (from + ops->len) > mtd->size) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	nand_get_device(chip, mtd, FL_READING);

	/** Added to support switching ECC format **/
	flex_select_eccparams(mtd, from);

	switch (ops->mode) {
	case MTD_OOB_PLACE:
	case MTD_OOB_AUTO:
	case MTD_OOB_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = nand_do_read_oob(mtd, from, ops);
	else
		ret = nand_do_read_ops(mtd, from, ops);

 out:
	nand_release_device(mtd);
	return ret;
}

/**
 * nand_write - [MTD Interface] NAND write with ECC
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @len:	number of bytes to write
 * @retlen:	pointer to variable to store the number of written bytes
 * @buf:	the data to write
 *
 * NAND write with ECC
 */
static int nand_write(struct mtd_info *mtd, loff_t to, size_t len,
			  size_t *retlen, const uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

	/* Do not allow reads past end of device */
	if ((to + len) > mtd->size)
		return -EINVAL;
	if (!len)
		return 0;

	nand_get_device(chip, mtd, FL_WRITING);

	/** Added to support switching ECC format **/
	flex_select_eccparams(mtd, to);

	chip->ops.len = len;
	chip->ops.datbuf = (uint8_t *)buf;
	chip->ops.oobbuf = NULL;

	ret = nand_do_write_ops(mtd, to, &chip->ops);

	*retlen = chip->ops.retlen;

	nand_release_device(mtd);

	return ret;
}

/**
 * nand_write_oob - [MTD Interface] NAND write data and/or out-of-band
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operation description structure
 */
static int nand_write_oob(struct mtd_info *mtd, loff_t to,
			  struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;
	int ret = -ENOTSUPP;

	ops->retlen = 0;

	/* Do not allow writes past end of device */
	if (ops->datbuf && (to + ops->len) > mtd->size) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	nand_get_device(chip, mtd, FL_WRITING);

	flex_select_eccparams(mtd, to);

	switch (ops->mode) {
	case MTD_OOB_PLACE:
	case MTD_OOB_AUTO:
	case MTD_OOB_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = nand_do_write_oob(mtd, to, ops);
	else
		ret = nand_do_write_ops(mtd, to, ops);

 out:
	nand_release_device(mtd);
	return ret;
}

#endif /* CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT */


/* Configure NAND controller timing registers */
static void flex_set_timings(struct stm_nand_flex_controller *flex,
			     struct stm_nand_timing_data *tm)
{
	uint32_t n;
	uint32_t reg;

	struct clk *emi_clk;
	uint32_t emi_t_ns;

	/* Timings set in terms of EMI clock... */
	emi_clk = clk_get(NULL, "emi_clk");

	if (!emi_clk || IS_ERR(emi_clk)) {
		printk(KERN_WARNING NAME ": Failed to find EMI clock. "
		       "Using default 100MHz.\n");
		emi_t_ns = 10;
	} else {
		emi_t_ns = 1000000000UL / clk_get_rate(emi_clk);
	}

	/* CONTROL_TIMING */
	n = (tm->sig_setup + emi_t_ns - 1)/emi_t_ns;
	reg = (n & 0xff) << 0;

	n = (tm->sig_hold + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 8;

	n = (tm->CE_deassert + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 16;

	n = (tm->WE_to_RBn + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 24;

	DEBUG(MTD_DEBUG_LEVEL0, "%s: CONTROL_TIMING = 0x%08x\n", NAME, reg);
	flex_writereg(reg, EMINAND_CONTROL_TIMING);

	/* WEN_TIMING */
	n = (tm->wr_on + emi_t_ns - 1)/emi_t_ns;
	reg = (n & 0xff) << 0;

	n = (tm->wr_off + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 8;

	DEBUG(MTD_DEBUG_LEVEL0, "%s: WEN_TIMING = 0x%08x\n", NAME, reg);
	flex_writereg(reg, EMINAND_WEN_TIMING);

	/* REN_TIMING */
	n = (tm->rd_on + emi_t_ns - 1)/emi_t_ns;
	reg = (n & 0xff) << 0;

	n = (tm->rd_off + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 8;

	DEBUG(MTD_DEBUG_LEVEL0, "%s: REN_TIMING = 0x%08x\n", NAME, reg);
	flex_writereg(reg, EMINAND_REN_TIMING);
}

/* FLEX mode chip select: For now we only support 1 chip per
 * 'stm_nand_flex_device' so chipnr will be 0 for select, -1 for deselect.
 *
 * So, if we change device:
 *   - Set bank in mux_control_reg to data->csn
 *   - Update read/write timings
 */
static void flex_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct stm_nand_flex_controller *flex = mtd_to_flex(mtd);
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_flex_device *data = chip->priv;

	/* Deselect, do nothing */
	if (chipnr == -1) {
		return;

	} else if (chipnr == 0) {
		/* If same chip as last time, no need to change anything */
		if (data->csn == flex->current_csn)
			return;

		/* Set CSn on FLEX controller */
		flex->current_csn = data->csn;
		flex_writereg(0x1 << data->csn, EMINAND_MUXCONTROL_REG);

		/* Set up timing parameters */
		flex_set_timings(flex, data->timing_data);

	} else {
		printk(KERN_ERR NAME ": attempt to select chipnr = %d\n",
		       chipnr);
	}

	return;
}

#ifdef CONFIG_MTD_DEBUG
static void flex_print_regs(struct stm_nand_flex_controller *flex)
{
	printk(NAME ": FLEX Registers:\n");
	printk(KERN_INFO "\tbootbank_config = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_BOOTBANK_CONFIG));
	printk(KERN_INFO "\trbn_status = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_RBN_STATUS));
	printk(KERN_INFO "\tinterrupt_enable = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_INTERRUPT_ENABLE));
	printk(KERN_INFO "\tinterrupt_status = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_INTERRUPT_STATUS));
	printk(KERN_INFO "\tinterrupt_clear = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_INTERRUPT_CLEAR));
	printk(KERN_INFO "\tinterrupt_edgeconfig = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_INTERRUPT_EDGECONFIG));
	printk(KERN_INFO "\tcontrol_timing = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_CONTROL_TIMING));
	printk(KERN_INFO "\twen_timing = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_WEN_TIMING));
	printk(KERN_INFO "\tren_timing = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_REN_TIMING));
	printk(KERN_INFO "\tflexmode_config = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_FLEXMODE_CONFIG));
	printk(KERN_INFO "\tmuxcontrol_reg = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_MUXCONTROL_REG));
	printk(KERN_INFO "\tcsn_alternate_reg = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_CSN_ALTERNATE));
	printk(KERN_INFO "\tmulti_cs_config_reg = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_MULTI_CS_CONFIG_REG));
	printk(KERN_INFO "\tversion_reg = 0x%08x\n",
	       (unsigned int)flex_readreg(EMINAND_VERSION_REG));
}
#endif /* CONFIG_MTD_DEBUG */

static struct stm_nand_flex_controller * __init
flex_init_controller(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata = pdev->dev.platform_data;
	struct resource *resource;
	int res;
	struct stm_nand_flex_controller *flex;

	flex = kzalloc(sizeof(struct stm_nand_flex_controller) +
		       (sizeof(struct stm_nand_flex_device *) * pdata->nr_banks),
		       GFP_KERNEL);
	if (!flex)
		return ERR_PTR(-ENOMEM);

	/* Request IO Memory */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"flex_mem");
	if (!resource) {
		printk(KERN_ERR NAME ": Failed to get FLEX IORESOURCE_MEM.\n");
		res = -ENODEV;
		goto out1;
	}
	flex->mem_region = request_mem_region(resource->start,
					     resource->end -
					     resource->start + 1,
					     pdev->name);
	if (!flex->mem_region) {
		printk(KERN_ERR NAME ": Failed request memory region 0x%08x.\n",
		       pdev->resource[0].start);
		res = -EBUSY;
		goto out1;
	}

	/* Map base address */
	flex->base_addr = ioremap_nocache(resource->start,
					  resource->end - resource->start + 1);
	if (!flex->base_addr) {
		printk(KERN_ERR NAME " Failed tp map base address  0x%08x\n",
		       resource->start);
		res = -EINVAL;
		goto out2;
	}

#ifdef CONFIG_STM_NAND_FLEX_CACHED
	flex->data_phys = resource->start + EMINAND_FLEX_DATA;
	flex->data_cached = ioremap_cache(flex->data_phys, L1_CACHE_BYTES);
	if (!flex->data_cached) {
		printk(KERN_ERR NAME " Failed to map data reg 0x%08x\n",
		       resource->start + EMINAND_FLEX_DATA);
		res = -EINVAL;
		goto out3;
	}
	spin_lock_init(&flex->lock);
#endif

	flex->buf = kmalloc(NAND_MAX_PAGESIZE +  NAND_MAX_OOBSIZE,
			   GFP_KERNEL | __GFP_DMA);
	if (!flex->buf) {
		printk(KERN_ERR NAME " Failed allocate bounce buffer\n");
		res = -ENOMEM;
		goto out4;
	}

	flex->current_csn = -1;

	/* Initialise 'controller' structure */
	spin_lock_init(&flex->hwcontrol.lock);
	init_waitqueue_head(&flex->hwcontrol.wq);

	/* Disable boot_not_flex */
	flex_writereg(0x00000000, EMINAND_BOOTBANK_CONFIG);

	/* Reset FLEX Controller */
	flex_writereg((0x1 << 3), EMINAND_FLEXMODE_CONFIG);
	udelay(1);
	flex_writereg(0x00, EMINAND_FLEXMODE_CONFIG);

	/* Set Controller to FLEX mode */
	flex_writereg(0x00000001, EMINAND_FLEXMODE_CONFIG);

	/* Not using interrupts in FLEX mode */
	flex_writereg(0x00, EMINAND_INTERRUPT_ENABLE);

	/* To fit with MTD framework, configure FLEX_DATA reg for 1-byte
	 * read/writes, and deassert CSn
	 */
	flex_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAWRITE_CONFIG);
	flex_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAREAD_CONFIG);

#ifdef CONFIG_MTD_DEBUG
	flex_print_regs(flex);
#endif

	return flex;
 out4:
#ifdef CONFIG_STM_NAND_FLEX_CACHED
	iounmap(flex->data_cached);
 out3:
#endif
	iounmap(flex->base_addr);
 out2:
	release_resource(flex->mem_region);
 out1:
	return ERR_PTR(res);
}

static void __devexit flex_exit_controller(struct platform_device *pdev)
{
	struct stm_nand_flex_controller *flex = platform_get_drvdata(pdev);

	kfree(flex->buf);
	iounmap(flex->base_addr);
#ifdef CONFIG_STM_NAND_FLEX_CACHED
	iounmap(flex->data_cached);
#endif
	release_resource(flex->mem_region);
}

static struct stm_nand_flex_device * __init
flex_init_bank(struct stm_nand_flex_controller *flex,
	       struct stm_nand_bank_data *bank,
	       int rbn_connected, const char *name)
{
	struct stm_nand_flex_device *data;
	int res;

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
	struct mtd_info *slave;
	struct mtd_part *part;
	char *boot_part_name;
#endif

	/* Allocate memory for the device structure (and zero it) */
	data = kzalloc(sizeof(struct stm_nand_flex_device), GFP_KERNEL);
	if (!data) {
		printk(KERN_ERR NAME
		       ": Failed to allocate device structure.\n");
		res = -ENOMEM;
		goto out1;
	}

	/* House keeping :-) */
	data->chip.priv = data;
	data->mtd.priv = &data->chip;
	data->mtd.owner = THIS_MODULE;

	/* Assign more sensible name (default is string from nand_ids.c!) */
	data->mtd.name = name;
	data->csn = bank->csn;

	/* Use hwcontrol structure to manage access to FLEX Controller */
	data->chip.controller = &flex->hwcontrol;
	data->chip.state = FL_READY;

	/* Get chip's timing data */
	data->timing_data = bank->timing_data;

	/* Copy over chip specific platform data */
	data->chip.chip_delay = data->timing_data->chip_delay;
	data->chip.options = bank->options;
	data->chip.options |= NAND_NO_AUTOINCR;      /* Not tested, disable */

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
	/* Handle STM H/W ECC layouts when performing initial scan for
	 * bad-blocks */
	data->chip.scan_bbt = scan_bbt_stmecc;
#endif
	/* Callbacks for FLEX mode operation */
	data->chip.cmd_ctrl = flex_cmd_ctrl;
	data->chip.select_chip = flex_select_chip;
	data->chip.read_byte = flex_read_byte;
#ifdef CONFIG_STM_NAND_FLEX_CACHED
	data->chip.read_buf = flex_read_buf_cached;
#else
	data->chip.read_buf = flex_read_buf;
#endif
	data->chip.write_buf = flex_write_buf;
	data->chip.verify_buf = flex_verify_buf;
	data->chip.waitfunc = flex_nand_wait;
	if (rbn_connected)
		data->chip.dev_ready = flex_rbn;

	/* For now, use NAND_ECC_SOFT. Callbacks filled in during scan() */
	data->chip.ecc.mode = NAND_ECC_SOFT;

	/* Data IO */
	data->chip.IO_ADDR_R = flex->base_addr + EMINAND_FLEX_DATA;
	data->chip.IO_ADDR_W = flex->base_addr + EMINAND_FLEX_DATA;

#if defined(CONFIG_CPU_SUBTYPE_STX7200)
	/* Reset AFM program. Why!?! */
	flex_readreg(EMINAND_AFM_SEQUENCE_STATUS_REG);
	memset(flex->base_addr + EMINAND_AFM_SEQUENCE_REG_1, 0, 32);
#endif

	/* Scan to find existance of the device */
	if (nand_scan(&data->mtd, 1)) {
		printk(KERN_ERR NAME ":nand_scan failed\n");
		res = -ENXIO;
		goto out2;
	}

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
	if (data->chip.ecc.mode == NAND_ECC_4BITONDIE) {
		printk(KERN_ERR NAME ": boot-mode ECC not supported on "
		       "4-bit on-die ECC devices\n");
		res = -ENXIO;
		goto out2;
	}

	/* Setup ECC params, for NORMAL and BOOT operation */
	flex_setup_eccparams(&data->mtd);
	/* Override MTD NAND interface, to allow us to provide BOOT SUPPORT */
	data->mtd.read = nand_read;
	data->mtd.write = nand_write;
	data->mtd.read_oob = nand_read_oob;
	data->mtd.write_oob = nand_write_oob;

	/* Set name of boot partition */
	boot_part_name = cmdline ? cmdline : CONFIG_STM_NAND_FLEX_BOOTPARTITION;
	printk(KERN_INFO NAME ": Using boot partition name [%s] (from %s)\n",
	       boot_part_name, cmdline ? "command line" : "kernel config");

#endif

#ifdef CONFIG_MTD_PARTITIONS
	/* Try parsing commandline paritions */
	data->nr_parts = parse_mtd_partitions(&data->mtd, part_probes,
					      &data->parts, 0);

	/* Try platfotm data */
	if (!data->nr_parts && bank->partitions) {
		data->parts = bank->partitions;
		data->nr_parts = bank->nr_partitions;
	}

	/* Add any partitions that were found */
	if (data->nr_parts) {
		res = add_mtd_partitions(&data->mtd,
					 data->parts, data->nr_parts);
		if (res)
			goto out2;

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
		/* Update boot-mode slave partition */
		slave = get_mtd_partition_slave(&data->mtd, boot_part_name);
		if (slave) {
			printk(KERN_INFO NAME ": Found BOOT parition"
			       "[%s], updating ECC paramters\n",
			       slave->name);

			part = PART(slave);
			data->boot_start = part->offset;
			data->boot_end = part->offset + slave->size;

			slave->oobavail =
				data->ecc_boot.ecc_ctrl.layout->oobavail;
			slave->subpage_sft =
				data->ecc_boot.subpage_sft;
			slave->ecclayout =
				data->ecc_boot.ecc_ctrl.layout;

			printk("boot-ECC 0x%08x->0x%08x\n",
			       (unsigned int)data->boot_start,
			       (unsigned int)data->boot_end);

		} else {
			printk(KERN_WARNING NAME ": Failed to find boot "
			       "partition [%s]\n", boot_part_name);
		}
#endif
	} else
#endif
		res = add_mtd_device(&data->mtd);

	if (!res)
		return data;

 out2:
#ifdef CONFIG_MTD_PARTITIONS
	if (data->parts && data->parts != bank->partitions)
		kfree(data->parts);
#endif

	kfree(data);
 out1:
	return ERR_PTR(res);
}

static int __init stm_nand_flex_probe(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata = pdev->dev.platform_data;
	int res;
	int n;
	struct stm_nand_bank_data *bank;
	struct stm_nand_flex_controller *flex;

	flex = flex_init_controller(pdev);
	if (IS_ERR(flex)) {
		dev_err(&pdev->dev, "Failed to initialise NAND Controller.\n");
		res = PTR_ERR(flex);
		return res;
	}

	bank = pdata->banks;
	for (n=0; n<pdata->nr_banks; n++) {
		flex->devices[n] = flex_init_bank(flex, bank,
						  pdata->flex_rbn_connected,
						  dev_name(&pdev->dev));
		bank++;
	}

	platform_set_drvdata(pdev, flex);

	return 0;
}

static int __devexit stm_nand_flex_remove(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata = pdev->dev.platform_data;
	struct stm_nand_flex_controller *flex = platform_get_drvdata(pdev);
	int n;

	for (n=0; n<pdata->nr_banks; n++) {
		struct stm_nand_flex_device *data = flex->devices[n];
		nand_release(&data->mtd);

#ifdef CONFIG_MTD_PARTITIONS
		if (data->parts && data->parts != pdata->banks[n].partitions)
			kfree(data->parts);
#endif

		kfree(data);
	}

	flex_exit_controller(pdev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver stm_nand_flex_driver = {
	.probe		= stm_nand_flex_probe,
	.remove		= stm_nand_flex_remove,
	.driver		= {
		.name	= NAME,
		.owner	= THIS_MODULE,
	},
};

#ifdef CONFIG_STM_NAND_FLEX_BOOTMODESUPPORT
static int __init bootpart_setup(char *s)
{
	cmdline = s;
	return 1;
}

__setup("nbootpart=", bootpart_setup);
#endif

static int __init stm_nand_flex_init(void)
{
	return platform_driver_register(&stm_nand_flex_driver);
}

static void __exit stm_nand_flex_exit(void)
{
	platform_driver_unregister(&stm_nand_flex_driver);
}

module_init(stm_nand_flex_init);
module_exit(stm_nand_flex_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Angus Clark");
MODULE_DESCRIPTION("STMicroelectronics NAND driver: H/W FLEX mode");
