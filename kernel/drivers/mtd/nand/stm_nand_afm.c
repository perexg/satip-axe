/*
 *  ------------------------------------------------------------------------
 *  stm_afm_nand.c STMicroelectronics Advanced Flex Mode NAND Flash driver
 *                 for SoC's with the Hamming NAND Controller.
 *  ------------------------------------------------------------------------
 *
 *  Copyright (c) 2010 STMicroelectronics Limited
 *  Author: Angus Clark <Angus.Clark@st.com>
 *
 *  ------------------------------------------------------------------------
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *  ------------------------------------------------------------------------
 *
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/clk.h>
#include <linux/stm/platform.h>
#include <linux/stm/nand.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "stm_nand_ecc.h"
#include "stm_nandc_regs.h"

#define NAME	"stm-nand-afm"


#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
/*
 * Support for STM boot-mode ECC: The STM NAND Boot Controller uses a different
 * ECC scheme to the AFM controller.  In order to support boot-mode ECC data, we
 * maintian two sets of ECC parameters and switch depending on which partition
 * we are about to read from.
 */
struct ecc_params {
	/* nand_chip params */
	struct nand_ecc_ctrl	ecc_ctrl;
	int			subpagesize;

	/* mtd_info params */
	u_int32_t		subpage_sft;
};

static void afm_select_eccparams(struct mtd_info *mtd, loff_t offs);
static void afm_write_page_boot(struct mtd_info *mtd,
				struct nand_chip *chip,
				const uint8_t *buf);
static int afm_read_page_boot(struct mtd_info *mtd,
			      struct nand_chip *chip,
			      uint8_t *buf, int page);

/* Module parameter for specifying name of boot partition */
static char *nbootpart;
module_param(nbootpart, charp, 0000);
MODULE_PARM_DESC(nbootpart, "MTD name of NAND boot-mode ECC partition");

#else
#define afm_select_eccparams(x, y) do {} while (0);
#endif /* CONFIG_STM_NAND_AFM_BOOTMODESUPPORT */


/* External functions */

/* AFM 32-byte program block */
struct afm_prog {
	uint8_t instr[16];
	uint32_t addr_reg;
	uint32_t extra_reg;
	uint8_t cmds[4];
	uint32_t seq_cfg;
} __attribute__((__packed__));

/* NAND device connected to STM NAND Controller */
struct stm_nand_afm_device {
	struct nand_chip	chip;
	struct mtd_info		mtd;

	int			csn;
	struct stm_nand_timing_data *timing_data;

	struct device		*dev;

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	unsigned long		boot_start;
	unsigned long		boot_end;
	struct ecc_params	ecc_boot;
	struct ecc_params	ecc_afm;
#endif

#ifdef CONFIG_MTD_PARTITIONS
	int			nr_parts;
	struct mtd_partition	*parts;
#endif

};

/* STM NAND AFM Controller  */
struct stm_nand_afm_controller {
	void __iomem		*base;
	void __iomem		*fifo_cached;
	unsigned long		fifo_phys;

	/* resources */
	struct device		*dev;
	unsigned int		map_base;
	unsigned int		map_size;
	unsigned int		irq;

	spinlock_t		lock; /* save/restore IRQ flags */

	int			current_csn;

	/* access control */
	struct nand_hw_control	hwcontrol;

	/* irq completions */
	struct completion	rbn_completed;
	struct completion	seq_completed;

	/* emulate MTD nand_base.c implementation */
	uint32_t		status;
	uint32_t		col;
	uint32_t		page;

	/* devices connected to controller */
	struct stm_nand_afm_device *devices[0];
};

#define afm_writereg(val, reg)	iowrite32(val, afm->base + (reg))
#define afm_readreg(reg)	ioread32(afm->base + (reg))

static struct stm_nand_afm_controller *mtd_to_afm(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_hw_control *controller = chip->controller;
	struct stm_nand_afm_controller *afm;

	afm = container_of(controller, struct stm_nand_afm_controller,
			    hwcontrol);
	return afm;
}


/*
 * Define some template AFM programs
 */
#define AFM_INSTR(cmd, op)	((cmd) | ((op) << 4))

/* AFM Prog: Block Erase */
struct afm_prog afm_prog_erase = {
	.cmds = {
		NAND_CMD_ERASE1,
		NAND_CMD_ERASE1,
		NAND_CMD_ERASE2,
		NAND_CMD_STATUS
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_ADDR,	0),
		AFM_INSTR(AFM_ADDR,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CMD,	3),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Block Erase, extra address cycle */
struct afm_prog afm_prog_erase_ext = {
	.cmds = {
		NAND_CMD_ERASE1,
		NAND_CMD_ERASE1,
		NAND_CMD_ERASE2,
		NAND_CMD_STATUS
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_ADDR,	0),
		AFM_INSTR(AFM_ADDR,	1),
		AFM_INSTR(AFM_ADDR,	2),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CMD,	3),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
		},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Read OOB [SmallPage] */
struct afm_prog afm_prog_read_oob_sp = {
	.cmds = {
		NAND_CMD_READOOB
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Read OOB [LargePage] */
struct afm_prog afm_prog_read_oob_lp = {
	.cmds = {
		NAND_CMD_READ0,
		NAND_CMD_READSTART
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Read Raw Page and OOB Data [SmallPage]
 *
 * Note, "SPARE3" would normally be used to read the OOB data.  However, SPARE3
 * has been observed to cause problems with EMI Arbitration resulting in system
 * deadlock.  To avoid such problems, we use the DATA0 command here.  This will
 * have the effect of over-reading 48 bytes from the end of OOB area but should
 * be benign on current NAND devices.
 */
struct afm_prog afm_prog_read_raw_sp = {
	.cmds = {
		NAND_CMD_READ0
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Read Raw Page and OOB Data [LargePage] */
struct afm_prog afm_prog_read_raw_lp = {
	.cmds = {
		NAND_CMD_READ0,
		NAND_CMD_READSTART
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Write Raw Page [SmallPage]
 *           OOB written with FLEX mode.
 */
struct afm_prog afm_prog_write_raw_sp_a = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_READ0,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS,
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CMD,	3),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};

/* AFM Prog: Write Raw Page and OOB Data [LargePage] */
struct afm_prog afm_prog_write_raw_lp = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS,
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};

/* AFM Prog: Write Page and OOB Data, with ECC support [SmallPage]
 *
 * Note, the AFM command, "SPARE3", would normally be used to write the OOB
 * data, with ECC inserted on the fly by the NAND controller.  However, the
 * SPARE3 command has been observed to cause EMI Arbitration issues resulting in
 * a bus stall and system deadlock.  To avoid this problem, we switch to FLEX
 * mode to write the OOB data, extracting and inserting the ECC bytes from the
 * NAND ECC checkcode regiter (see afm_write_page_ecc_sp())
 */
struct afm_prog afm_prog_write_ecc_sp_a = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_READ0,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS,
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};

/* AFM Prog: Write Page and OOB Data, with ECC support [LargePage] */
struct afm_prog afm_prog_write_ecc_lp = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS,
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	2),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};

/* AFM Prog: Write OOB Data [LargePage] */
struct afm_prog afm_prog_write_oob_lp = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};


/*
 * STMicroeclectronics H/W ECC layouts
 *
 * AFM4 ECC:      512-byte data records, 3 bytes H/W ECC, 1 byte S/W ECC, 3
 *                bytes marker ({'A', 'F', 'M'})
 * Boot-mode ECC: 128-byte data records, 3 bytes ECC + 1 byte marker ('B')
 *
 */
static struct nand_ecclayout afm_oob_16 = {
	.eccbytes = 7,
	.eccpos = {
		/* { HW_ECC0, HW_ECC1, HW_ECC2, 'A', 'F', 'M', SW_ECC } */
		0, 1, 2, 3, 4, 5, 6},
	.oobfree = {
		{.offset = 7,
		 . length = 9},
	}
};

static struct nand_ecclayout afm_oob_64 = {
	.eccbytes = 28,
	.eccpos = {
		/* { HW_ECC0, HW_ECC1, HW_ECC2, 'A', 'F', 'M', SW_ECC } */
		0,   1,  2,  3,  4,  5,  6,	/* Record 0 */
		16, 17, 18, 19, 20, 21, 22,	/* Record 1 */
		32, 33, 34, 35, 36, 37, 38,	/* Record 2 */
		48, 49, 50, 51, 52, 53, 54,	/* Record 3 */
	},
	.oobfree = {
		{.offset = 7,
		 .length = 9
		},
		{.offset = 23,
		 .length = 9
		},
		{.offset = 39,
		 .length = 9
		},
		{.offset = 55,
		 .length = 9
		},
	}
};

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
static struct nand_ecclayout boot_oob_16 = {
	.eccbytes = 16,
	.eccpos = {
		0, 1, 2, 3,	/* Record 0: ECC0, ECC1, ECC2, 'B' */
		4, 5, 6, 7,	/* Record 1: ECC0, ECC1, ECC2, 'B' */
		8, 9, 10, 11,	/*                  ...            */
		12, 13, 14, 15
	},
	.oobfree = {{0, 0} },	/* No free space in OOB */
};

static struct nand_ecclayout boot_oob_64 = {
	.eccbytes = 64,
	.eccpos = {
		0, 1, 2, 3,	/* Record 0: ECC0, ECC1, ECC2, B */
		4, 5, 6, 7,	/* Record 1: ECC0, ECC1, ECC2, B */
		8,  9, 10, 11,	/*                 ...           */
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
#endif

/* Pattern descriptors for scanning bad-block scanning - add support for AFM ECC
 * scheme */
static uint8_t scan_ff_pattern[] = { 0xff, 0xff };
static struct nand_bbt_descr bbt_scan_sp = {

	.options = (NAND_BBT_SCAN2NDPAGE | NAND_BBT_SCANSTMAFMECC),
	.offs = 5,
	.len = 1,
	.pattern = scan_ff_pattern
};
static struct nand_bbt_descr bbt_scan_lp = {
	.options = (NAND_BBT_SCAN2NDPAGE | NAND_BBT_SCANSTMAFMECC),
	.offs = 0,
	.len = 2,
	.pattern = scan_ff_pattern
};


/*
 * AFM Interrupts
 */

#define AFM_IRQ			(1 << 0)
#define AFM_IRQ_RBN		(1 << 2)
#define AFM_IRQ_DATA_DREQ	(1 << 3)
#define AFM_IRQ_SEQ_DREQ	(1 << 4)
#define AFM_IRQ_CHECK		(1 << 5)
#define AFM_IRQ_ECC_FIX		(1 << 6)

static void afm_enable_interrupts(struct stm_nand_afm_controller *afm,
				  uint32_t irqs)
{
	uint32_t reg;

	reg = afm_readreg(EMINAND_INTERRUPT_ENABLE);
	reg |= irqs;
	afm_writereg(reg, EMINAND_INTERRUPT_ENABLE);

}

static void afm_disable_interrupts(struct stm_nand_afm_controller *afm,
				   uint32_t irqs)
{
	uint32_t reg;

	reg = afm_readreg(EMINAND_INTERRUPT_ENABLE);
	reg &= ~irqs;
	afm_writereg(reg, EMINAND_INTERRUPT_ENABLE);
}

static irqreturn_t afm_irq_handler(int irq, void *dev)
{
	struct stm_nand_afm_controller *afm =
		(struct stm_nand_afm_controller *)dev;
	unsigned int irq_status;

	irq_status = afm_readreg(EMINAND_INTERRUPT_STATUS);

	/* RBn */
	if (irq_status & 0x4) {
		afm_writereg(0x04, EMINAND_INTERRUPT_CLEAR);
		complete(&afm->rbn_completed);
	}

	/* ECC_fix */
	if (irq_status & 0x40)
		afm_writereg(0x10, EMINAND_INTERRUPT_CLEAR);

	/* Seq_req */
	if (irq_status & AFM_IRQ_SEQ_DREQ) {
		afm_writereg(0x40, EMINAND_INTERRUPT_CLEAR);
		complete(&afm->seq_completed);
	}

	return IRQ_HANDLED;
}

/*
 * AFM Initialisation
 */

/* AFM set generic config register */
static void afm_generic_config(struct stm_nand_afm_controller *afm,
			       uint32_t busw, uint32_t page_size,
			       uint32_t chip_size)
{
	uint32_t reg;

	reg = 0x00;
	if ((busw & NAND_BUSWIDTH_16) == 0)
		reg |= 0x1 << 16;		/* data_8_not_16      */

	if (page_size > 512) {
		/* large page */
		reg |= 0x1 << 17;		/* page size	      */
		if (chip_size > (128 << 20))
			reg |= 0x1 << 18;	/* extra addr cycle   */
	} else if (chip_size > (32 << 20)) {
		/* small page */
		reg |= 0x1 << 18;		/* extra addr cycle   */

	}

	dev_dbg(afm->dev, "setting AFM_GENERIC_CONFIG_REG = 0x%08x\n", reg);

	afm_writereg(reg, EMINAND_AFM_GENERIC_CONFIG_REG);
}

/* AFM configure timing parameters */
static void afm_set_timings(struct stm_nand_afm_controller *afm,
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

	dev_dbg(afm->dev, "setting CONTROL_TIMING = 0x%08x\n", reg);
	afm_writereg(reg, EMINAND_CONTROL_TIMING);

	/* WEN_TIMING */
	n = (tm->wr_on + emi_t_ns - 1)/emi_t_ns;
	reg = (n & 0xff) << 0;

	n = (tm->wr_off + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 8;

	dev_dbg(afm->dev, "setting WEN_TIMING = 0x%08x\n", reg);
	afm_writereg(reg, EMINAND_WEN_TIMING);

	/* REN_TIMING */
	n = (tm->rd_on + emi_t_ns - 1)/emi_t_ns;
	reg = (n & 0xff) << 0;

	n = (tm->rd_off + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 8;

	dev_dbg(afm->dev, "setting REN_TIMING = 0x%08x\n", reg);
	afm_writereg(reg, EMINAND_REN_TIMING);
}

/* Initialise the AFM NAND controller */
static struct stm_nand_afm_controller * __init
afm_init_controller(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata = pdev->dev.platform_data;
	struct stm_nand_afm_controller *afm;
	struct resource *resource;
	int err = 0;

	afm = kzalloc(sizeof(struct stm_nand_afm_controller) +
		      (sizeof(struct stm_nand_afm_device *) * pdata->nr_banks),
		       GFP_KERNEL);
	if (!afm) {
		dev_err(&pdev->dev, "failed to allocate afm controller data\n");
		err = -ENOMEM;
		goto err0;
	}

	afm->dev = &pdev->dev;

	/* Request IO Memory */
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		dev_err(&pdev->dev, "failed to find IOMEM resource\n");
		err = -ENOENT;
		goto err1;
	}
	afm->map_base = resource->start;
	afm->map_size = resource->end - resource->start + 1;

	if (!request_mem_region(afm->map_base, afm->map_size, pdev->name)) {
		dev_err(&pdev->dev, "request memory region failed [0x%08x]\n",
			afm->map_base);
		err = -EBUSY;
		goto err1;
	}

	/* Map base address */
	afm->base = ioremap_nocache(afm->map_base, afm->map_size);
	if (!afm->base) {
		dev_err(&pdev->dev, "base ioremap failed [0x%08x]\n",
			afm->map_base);
		err = -ENXIO;
		goto err2;
	}

#ifdef CONFIG_STM_NAND_AFM_CACHED
	/* Setup cached mapping to the AFM data FIFO */
	afm->fifo_phys = (afm->map_base + EMINAND_AFM_DATA_FIFO);
#ifndef CONFIG_32BIT
	/* 29-bit uses 'Area 7' address.  [Should this be done in ioremap?] */
	afm->fifo_phys &= 0x1fffffff;
#endif
	afm->fifo_cached = ioremap_cache(afm->fifo_phys, 512);
	if (!afm->fifo_cached) {
		dev_err(&pdev->dev, "fifo ioremap failed [0x%08x]\n",
			afm->map_base + EMINAND_AFM_DATA_FIFO);
		err = -ENXIO;
		goto err3;
	}

	spin_lock_init(&afm->lock);
#endif

	/* Setup IRQ handler */
	resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!resource) {
		dev_err(&pdev->dev, "failed to find IRQ resource\n");
		err = -ENOENT;
		goto err4;
	}

	afm->irq = resource->start;
	if (request_irq(afm->irq, afm_irq_handler, IRQF_DISABLED,
			dev_name(&pdev->dev), afm)) {
		dev_err(&pdev->dev, "irq request failed\n");
		err = -EBUSY;
		goto err4;
	}

	/* Initialise 'controller' structure */
	spin_lock_init(&afm->hwcontrol.lock);
	init_waitqueue_head(&afm->hwcontrol.wq);

	init_completion(&afm->rbn_completed);
	init_completion(&afm->seq_completed);
	afm->current_csn = -1;

	/* Stop AFM Controller, in case it's still running! */
	afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
	memset(afm->base + EMINAND_AFM_SEQUENCE_REG_1, 0, 32);

	/* Reset AFM Controller */
	afm_writereg((0x1 << 3), EMINAND_FLEXMODE_CONFIG);
	udelay(1);
	afm_writereg(0x00, EMINAND_FLEXMODE_CONFIG);

	/* Disable boot_not_flex */
	afm_writereg(0x00000000, EMINAND_BOOTBANK_CONFIG);

	/* Set Controller to AFM */
	afm_writereg(0x00000002, EMINAND_FLEXMODE_CONFIG);

	/* Enable Interrupts: individual interrupts enabled when needed! */
	afm_writereg(0x0000007C, EMINAND_INTERRUPT_CLEAR);
	afm_writereg(0x00000001, EMINAND_INTERRUPT_EDGECONFIG);
	afm_writereg(AFM_IRQ, EMINAND_INTERRUPT_ENABLE);

	/* Success :-) */
	return afm;


	/* Error path */
 err4:
#ifdef CONFIG_STM_NAND_AFM_CACHED
	iounmap(afm->fifo_cached);
 err3:
#endif
	iounmap(afm->base);
 err2:
	release_mem_region(afm->map_base, afm->map_size);
 err1:
	kfree(afm);
 err0:
	return ERR_PTR(err);

}

static void __devexit afm_exit_controller(struct platform_device *pdev)
{
	struct stm_nand_afm_controller *afm = platform_get_drvdata(pdev);

	free_irq(afm->irq, afm);
#ifdef CONFIG_STM_NAND_AFM_CACHED
	iounmap(afm->fifo_cached);
#endif
	iounmap(afm->base);
	release_mem_region(afm->map_base, afm->map_size);
	kfree(afm);
}

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
static void afm_setup_eccparams(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;

	/* Take a copy of ECC AFM params, as set up during nand_scan() */
	data->ecc_afm.ecc_ctrl = chip->ecc;
	data->ecc_afm.subpagesize = chip->subpagesize;
	data->ecc_afm.subpage_sft = mtd->subpage_sft;

	/* Set ECC BOOT params */
	data->ecc_boot.ecc_ctrl = data->ecc_afm.ecc_ctrl;
	data->ecc_boot.ecc_ctrl.write_page = afm_write_page_boot;
	data->ecc_boot.ecc_ctrl.read_page = afm_read_page_boot;
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
		dev_err(data->dev, "invalid ECC parameters\n");
		BUG();
	}
	data->ecc_boot.ecc_ctrl.total = data->ecc_boot.ecc_ctrl.steps *
		data->ecc_boot.ecc_ctrl.bytes;

	/* Disable subpage writes */
	data->ecc_boot.subpage_sft = 0;
	data->ecc_boot.subpagesize = mtd->writesize;
}

/* Switch ECC parameters */
static void afm_set_eccparams(struct mtd_info *mtd, struct ecc_params *params)
{
	struct nand_chip *chip = mtd->priv;

	chip->ecc = params->ecc_ctrl;
	chip->subpagesize = params->subpagesize;

	mtd->oobavail = params->ecc_ctrl.layout->oobavail;
	mtd->subpage_sft = params->subpage_sft;
}

static void afm_select_eccparams(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;

	if (offs >= data->boot_start &&
	    offs < data->boot_end) {
		if (chip->ecc.layout != data->ecc_boot.ecc_ctrl.layout) {
			dev_dbg(data->dev, "switching to boot-mode ECC\n");
			afm_set_eccparams(mtd, &data->ecc_boot);
		}
	} else {
		if (chip->ecc.layout != data->ecc_afm.ecc_ctrl.layout) {
			dev_dbg(data->dev, "switching to AFM4 ECC\n");
			afm_set_eccparams(mtd, &data->ecc_afm);
		}
	}
}

/* Replicated from ../mtdpart.c: required here to get slave MTD offsets and
 * determine which ECC mode to use.
 */
struct mtd_part {
	struct mtd_info mtd;
	struct mtd_info *master;
	u_int32_t offset;
	int index;
	struct list_head list;
	int registered;
};

#define PART(x)  ((struct mtd_part *)(x))
#endif



/*
 * Internal helper-functions for MTD Interface callbacks
 */
static uint8_t *afm_transfer_oob(struct nand_chip *chip, uint8_t *oob,
				 struct mtd_oob_ops *ops, size_t len)
{
	switch (ops->mode) {

	case MTD_OOB_PLACE:
	case MTD_OOB_RAW:
		memcpy(oob, chip->oob_poi + ops->ooboffs, len);
		return oob + len;

	case MTD_OOB_AUTO: {
		struct nand_oobfree *free = chip->ecc.layout->oobfree;
		uint32_t boffs = 0, roffs = ops->ooboffs;
		size_t bytes = 0;

		for (; free->length && len; free++, len -= bytes) {
			/* Read request not from offset 0 ? */
			if (unlikely(roffs)) {
				if (roffs >= free->length) {
					roffs -= free->length;
					continue;
				}
				boffs = free->offset + roffs;
				bytes = min_t(size_t, len,
					      (free->length - roffs));
				roffs = 0;
			} else {
				bytes = min_t(size_t, len, free->length);
				boffs = free->offset;
			}
			memcpy(oob, chip->oob_poi + boffs, bytes);
			oob += bytes;
		}

		return oob;
	}
	default:
		BUG();
	}
	return NULL;
}

static int afm_do_read_oob(struct mtd_info *mtd, loff_t from,
			   struct mtd_oob_ops *ops)
{
	int page, realpage, chipnr, sndcmd = 1;
	struct nand_chip *chip = mtd->priv;
	int blkcheck = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;
	int readlen = ops->ooblen;
	int len;
	uint8_t *buf = ops->oobbuf;

	DEBUG(MTD_DEBUG_LEVEL3, "afm_do_read_oob: from = 0x%08Lx, len = %i\n",
	      (unsigned long long)from, readlen);


	if (ops->mode == MTD_OOB_AUTO)
		len = chip->ecc.layout->oobavail;
	else
		len = mtd->oobsize;

	if (unlikely(ops->ooboffs >= len)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
			"Attempt to start read outside oob\n");
		return -EINVAL;
	}

	/* Do not allow reads past end of device */
	if (unlikely(from >= mtd->size ||
		     ops->ooboffs + readlen >
		     ((mtd->size >> chip->page_shift) -
		      (from >> chip->page_shift)) * len)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	chipnr = (int)(from >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/* Shift to get page */
	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	while (1) {
		sndcmd = chip->ecc.read_oob(mtd, chip, page, sndcmd);

		len = min(len, readlen);
		buf = afm_transfer_oob(chip, buf, ops, len);

		if (!(chip->options & NAND_NO_READRDY)) {
			/*
			 * Apply delay or wait for ready/busy pin. Do this
			 * before the AUTOINCR check, so no problems arise if a
			 * chip which does auto increment is marked as
			 * NOAUTOINCR by the board driver.
			 */
			if (!chip->dev_ready)
				udelay(chip->chip_delay);
			else
				nand_wait_ready(mtd);
		}

		readlen -= len;
		if (!readlen)
			break;

		/* Increment page address */
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}

		/* Check, if the chip supports auto page increment
		 * or if we have hit a block boundary.
		 */
		if (!NAND_CANAUTOINCR(chip) || !(page & blkcheck))
			sndcmd = 1;
	}

	ops->oobretlen = ops->ooblen;
	return 0;
}

static int afm_do_read_ops(struct mtd_info *mtd, loff_t from,
			   struct mtd_oob_ops *ops)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int chipnr, page, realpage, col, bytes, aligned;
	struct nand_chip *chip = mtd->priv;
	struct mtd_ecc_stats stats;
	int blkcheck = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;
	int sndcmd = 1;
	int ret = 0;
	uint32_t readlen = ops->len;
	uint32_t oobreadlen = ops->ooblen;
	uint8_t *bufpoi, *oob, *buf;

	DEBUG(MTD_DEBUG_LEVEL3, "afm_do_read_ops: from = 0x%08Lx, len = %i\n",
	      (unsigned long long)from, readlen);

	stats = mtd->ecc_stats;

	chipnr = (int)(from >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	realpage = (int)(from >> chip->page_shift);	/* actual page no.  */
	page = realpage & chip->pagemask;		/* within-chip page */

	col = (int)(from & (mtd->writesize - 1));	/* col within page  */

	buf = ops->datbuf;			/* pointer to data buf */
	oob = ops->oobbuf;			/* pointer to oob buf	*/

	while (1) {
		/* #bytes in next transfer */
		bytes = min(mtd->writesize - col, readlen);
		/* tranfer aligned to page? */
		aligned = (bytes == mtd->writesize);

		/* Add test for for alignment */
		if ((uint32_t)buf & 0x3)
			aligned = 0;

		/* Is the current page in the buffer ? */
		if (realpage != chip->pagebuf || oob) {
			bufpoi = aligned ? buf : chip->buffers->databuf;

			afm->page = page;
			afm->col = 0;
			/* Now read the page into the buffer, without ECC if
			 * MTD_OOB_RAW */
			if (unlikely(ops->mode == MTD_OOB_RAW))
				ret = chip->ecc.read_page_raw(mtd, chip,
							      bufpoi, page);
			else
				ret = chip->ecc.read_page(mtd, chip,
							  bufpoi, page);
			if (ret < 0)
				break;

			/* Transfer not aligned data */
			if (!aligned) {
				chip->pagebuf = realpage;
				memcpy(buf, chip->buffers->databuf + col,
				       bytes);
			}

			buf += bytes;

			/* Done page data, now look at OOB... */
			if (unlikely(oob)) {
				/* Raw mode does data:oob:data:oob */
				if (ops->mode != MTD_OOB_RAW) {
					int toread = min(oobreadlen,
						chip->ecc.layout->oobavail);
					if (toread) {
						oob = afm_transfer_oob(chip,
								       oob,
								       ops,
								       toread);
						oobreadlen -= toread;
					}
				} else
					buf = afm_transfer_oob(chip,
							       buf, ops,
							       mtd->oobsize);
			}
		} else {
			memcpy(buf, chip->buffers->databuf + col, bytes);
			buf += bytes;
		}

		readlen -= bytes;

		if (!readlen)
			break;

		/* For subsequent reads align to page boundary. */
		col = 0;
		/* Increment page address */
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}

		/* Check, if the chip supports auto page increment
		 * or if we have hit a block boundary.
		 */
		if (!NAND_CANAUTOINCR(chip) || !(page & blkcheck))
			sndcmd = 1;
	}

	ops->retlen = ops->len - (size_t) readlen;
	if (oob)
		ops->oobretlen = ops->ooblen - oobreadlen;

	if (ret)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return  mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;

}

static int afm_check_wp(struct mtd_info *mtd)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	uint32_t status;

	/* Switch to Flex Mode */
	afm_writereg(0x00000001, EMINAND_FLEXMODE_CONFIG);

	/* Read status register */
	afm_writereg(FLX_CMD(NAND_CMD_STATUS), EMINAND_FLEX_COMMAND_REG);
	status = afm_readreg(EMINAND_FLEX_DATA);

	/* Switch back to AFM */
	afm_writereg(0x00000002, EMINAND_FLEXMODE_CONFIG);

	return (status & NAND_STATUS_WP) ? 0 : 1;
}

static uint8_t *afm_fill_oob(struct nand_chip *chip, uint8_t *oob,
			     struct mtd_oob_ops *ops)
{
	size_t len = ops->ooblen;

	switch (ops->mode) {

	case MTD_OOB_PLACE:
	case MTD_OOB_RAW:
		memcpy(chip->oob_poi + ops->ooboffs, oob, len);
		return oob + len;

	case MTD_OOB_AUTO: {
		struct nand_oobfree *free = chip->ecc.layout->oobfree;
		uint32_t boffs = 0, woffs = ops->ooboffs;
		size_t bytes = 0;

		for (; free->length && len; free++, len -= bytes) {
			/* Write request not from offset 0 ? */
			if (unlikely(woffs)) {
				if (woffs >= free->length) {
					woffs -= free->length;
					continue;
				}
				boffs = free->offset + woffs;
				bytes = min_t(size_t, len,
					      (free->length - woffs));
				woffs = 0;
			} else {
				bytes = min_t(size_t, len, free->length);
				boffs = free->offset;
			}
			memcpy(chip->oob_poi + boffs, oob, bytes);
			oob += bytes;
		}
		return oob;
	}
	default:
		BUG();
	}
	return NULL;
}

static int afm_do_write_oob(struct mtd_info *mtd, loff_t to,
			    struct mtd_oob_ops *ops)
{
	int chipnr, page, status, len;
	struct nand_chip *chip = mtd->priv;

	DEBUG(MTD_DEBUG_LEVEL3, "nand_write_oob: to = 0x%08x, len = %i\n",
	      (unsigned int)to, (int)ops->ooblen);

	if (ops->mode == MTD_OOB_AUTO)
		len = chip->ecc.layout->oobavail;
	else
		len = mtd->oobsize;

	/* Do not allow write past end of page */
	if ((ops->ooboffs + ops->ooblen) > len) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_write_oob: "
		      "Attempt to write past end of page\n");
		return -EINVAL;
	}

	if (unlikely(ops->ooboffs >= len)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
			"Attempt to start write outside oob\n");
		return -EINVAL;
	}

	/* Do not allow reads past end of device */
	if (unlikely(to >= mtd->size ||
		     ops->ooboffs + ops->ooblen >
			((mtd->size >> chip->page_shift) -
			 (to >> chip->page_shift)) * len)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
			"Attempt write beyond end of device\n");
		return -EINVAL;
	}

	chipnr = (int)(to >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/* Shift to get page */
	page = (int)(to >> chip->page_shift);

	/* Check, if it is write protected */
	if (afm_check_wp(mtd))
		return -EROFS;

	/* Invalidate the page cache, if we write to the cached page */
	if (page == chip->pagebuf)
		chip->pagebuf = -1;

	memset(chip->oob_poi, 0xff, mtd->oobsize);
	afm_fill_oob(chip, ops->oobbuf, ops);
	status = chip->ecc.write_oob(mtd, chip, page & chip->pagemask);
	memset(chip->oob_poi, 0xff, mtd->oobsize);

	if (status)
		return status;

	ops->oobretlen = ops->ooblen;

	return 0;
}

static int afm_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			  const uint8_t *buf, int page, int cached, int raw)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	afm->page = page;
	afm->col = 0;

	if (unlikely(raw))
		chip->ecc.write_page_raw(mtd, chip, buf);
	else
		chip->ecc.write_page(mtd, chip, buf);

	/* Check status information */
	if (afm->status & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
}

#define NOTALIGNED(x)	((x & (chip->subpagesize - 1)) != 0)
static int afm_do_write_ops(struct mtd_info *mtd, loff_t to,
			     struct mtd_oob_ops *ops)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	int chipnr, realpage, page, blockmask, column;
	struct nand_chip *chip = mtd->priv;
	uint32_t writelen = ops->len;
	uint8_t *oob = ops->oobbuf;
	uint8_t *buf = ops->datbuf;
	int ret, subpage;

	ops->retlen = 0;
	if (!writelen)
		return 0;

	/* reject writes, which are not page aligned */
	if (NOTALIGNED(to) || NOTALIGNED(ops->len)) {
		dev_err(afm->dev, "attempt to write not page aligned data\n");
		return -EINVAL;
	}

	column = to & (mtd->writesize - 1);
	subpage = column || (writelen & (mtd->writesize - 1));

	if (subpage && oob)
		return -EINVAL;

	chipnr = (int)(to >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/* Check, if it is write protected */
	if (afm_check_wp(mtd))
		return -EIO;

	realpage = (int)(to >> chip->page_shift);
	page = realpage & chip->pagemask;
	blockmask = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;

	/* Invalidate the page cache, when we write to the cached page */
	if (to <= (chip->pagebuf << chip->page_shift) &&
	    (chip->pagebuf << chip->page_shift) < (to + ops->len))
		chip->pagebuf = -1;

	/* If we're not given explicit OOB data, let it be 0xFF */
	if (likely(!oob))
		memset(chip->oob_poi, 0xff, mtd->oobsize);

	while (1) {
		int bytes = mtd->writesize;
		int cached = writelen > bytes && page != blockmask;
		uint8_t *wbuf = buf;

		/* Partial page write ? */
		/* TODO: change alignment constraints for DMA transfer! */
		if (unlikely(column || writelen < (mtd->writesize - 1) ||
			     ((uint32_t)wbuf & 31))) {
			cached = 0;
			bytes = min_t(int, bytes - column, (int) writelen);
			chip->pagebuf = -1;
			memset(chip->buffers->databuf, 0xff, mtd->writesize);
			memcpy(&chip->buffers->databuf[column], buf, bytes);
			wbuf = chip->buffers->databuf;
		}

		if (unlikely(oob))
			oob = afm_fill_oob(chip, oob, ops);

		ret = chip->write_page(mtd, chip, wbuf, page, cached,
				       (ops->mode == MTD_OOB_RAW));
		if (ret)
			break;

		writelen -= bytes;
		if (!writelen)
			break;

		column = 0;
		buf += bytes;
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}
	}

	ops->retlen = ops->len - writelen;
	if (unlikely(oob))
		ops->oobretlen = ops->ooblen;
	return ret;
}


/*
 * MTD Interface callbacks (replacing equivalent functions in nand_base.c)
 */


/* MTD Interface - erase block(s) */
static int afm_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	return nand_erase_nand(mtd, instr, 0);
}

/* MTD Interface - Check if block at offset is bad */
static int afm_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *chip = mtd->priv;

	/* Check for invalid offset */
	if (offs > mtd->size)
		return -EINVAL;

	/* We should always have a memory-resident BBT */
	BUG_ON(!chip->bbt);

	return nand_isbad_bbt(mtd, offs, 0);
}

/* MTD Interface - Mark block at the given offset as bad */
static int afm_block_markbad(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *chip = mtd->priv;
	uint8_t buf[16];
	int block, ret;

	/* Is is already marked bad? */
	ret = afm_block_isbad(mtd, offs);
	if (ret)
		return (ret > 0) ? 0 : ret;

	/* Update memory-resident BBT */
	BUG_ON(!chip->bbt);
	block = (int)(offs >> chip->bbt_erase_shift);
	chip->bbt[block >> 2] |= 0x01 << ((block & 0x03) << 1);

	/* Update non-volatile marker... */
	if (chip->options & NAND_USE_FLASH_BBT) {
		/* Update flash-resident BBT */
		ret = nand_update_bbt(mtd, offs);
	} else {
		/*
		 * Write the bad-block marker to OOB.  We also need to wipe the
		 * first 'AFM' marker (just in case it survived the preceding
		 * failed ERASE) to prevent subsequent on-boot scans from
		 * recognising an AFM block, instead of a marked-bad block.
		 */
		memset(buf, 0, 16);
		nand_get_device(chip, mtd, FL_WRITING);
		offs += mtd->oobsize;
		chip->ops.mode = MTD_OOB_PLACE;
		chip->ops.len = 16;
		chip->ops.ooblen = 16;
		chip->ops.datbuf = NULL;
		chip->ops.oobbuf = buf;
		chip->ops.ooboffs = chip->badblockpos & ~0x01;

		ret = afm_do_write_oob(mtd, offs, &chip->ops);
		nand_release_device(mtd);
	}
	if (ret == 0)
		mtd->ecc_stats.badblocks++;

	return ret;
}

/* MTD Interface - Read chunk of page data */
static int afm_read(struct mtd_info *mtd, loff_t from, size_t len,
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
	afm_select_eccparams(mtd, from);

	chip->ops.len = len;
	chip->ops.datbuf = buf;
	chip->ops.oobbuf = NULL;

	ret = afm_do_read_ops(mtd, from, &chip->ops);

	*retlen = chip->ops.retlen;

	nand_release_device(mtd);

	return ret;
}

/* MTD Interface - read page data and/or OOB */
static int afm_read_oob(struct mtd_info *mtd, loff_t from,
			struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;
	int ret = -ENOTSUPP;

	/* Do not allow reads past end of device */
	if (ops->datbuf && (from + ops->len) > mtd->size) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	nand_get_device(chip, mtd, FL_READING);
	afm_select_eccparams(mtd, from);

	switch (ops->mode) {
	case MTD_OOB_PLACE:
	case MTD_OOB_AUTO:
	case MTD_OOB_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = afm_do_read_oob(mtd, from, ops);
	else
		ret = afm_do_read_ops(mtd, from, ops);

 out:
	nand_release_device(mtd);
	return ret;
}

/* MTD Interface - write page data */
static int afm_write(struct mtd_info *mtd, loff_t to, size_t len,
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
	afm_select_eccparams(mtd, to);

	chip->ops.len = len;
	chip->ops.datbuf = (uint8_t *)buf;
	chip->ops.oobbuf = NULL;

	ret = afm_do_write_ops(mtd, to, &chip->ops);

	*retlen = chip->ops.retlen;

	nand_release_device(mtd);

	return ret;
}

/* MTD Interface - write page data and/or OOB */
static int afm_write_oob(struct mtd_info *mtd, loff_t to,
			  struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;
	int ret = -ENOTSUPP;

	/* Do not allow writes past end of device */
	if (ops->datbuf && (to + ops->len) > mtd->size) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	nand_get_device(chip, mtd, FL_WRITING);
	afm_select_eccparams(mtd, to);

	switch (ops->mode) {
	case MTD_OOB_PLACE:
	case MTD_OOB_AUTO:
	case MTD_OOB_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = afm_do_write_oob(mtd, to, ops);
	else
		ret = afm_do_write_ops(mtd, to, ops);

 out:
	nand_release_device(mtd);
	return ret;
}

/* MTD Interface - Sync (just wait for chip ready, then release) */
static void afm_sync(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	DEBUG(MTD_DEBUG_LEVEL3, "nand_sync: called\n");

	/* Grab the lock and see if the device is available */
	nand_get_device(chip, mtd, FL_SYNCING);
	/* Release it and go back */
	nand_release_device(mtd);
}

/* MTD Interface - Suspend (wait for chip ready, then suspend) */
static int afm_suspend(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	return nand_get_device(chip, mtd, FL_PM_SUSPENDED);
}

/* MTD Interface - Resume (release device) */
static void afm_resume(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	if (chip->state == FL_PM_SUSPENDED)
		nand_release_device(mtd);
	else
		printk(KERN_ERR "afm_resume() called for a chip which is not "
		       "in suspended state\n");
}

/*
 * AFM data transfer routines
 */

#ifdef CONFIG_STM_NAND_AFM_CACHED
static void afm_read_buf_cached(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	unsigned long irq_flags;

	int lenaligned = len & ~(L1_CACHE_BYTES-1);
	int lenleft = len & (L1_CACHE_BYTES-1);

	while (lenaligned > 0) {
		spin_lock_irqsave(&(afm->lock), irq_flags);
		invalidate_ioremap_region(afm->fifo_phys, afm->fifo_cached,
					  0, L1_CACHE_BYTES);
		memcpy_fromio(buf, afm->fifo_cached, L1_CACHE_BYTES);
		spin_unlock_irqrestore(&(afm->lock), irq_flags);

		buf += L1_CACHE_BYTES;
		lenaligned -= L1_CACHE_BYTES;
	}

#ifdef CONFIG_STM_L2_CACHE
	/* If L2 cache is enabled, we must ensure the cacheline is evicted prior
	 * to non-cached accesses..
	 */
	invalidate_ioremap_region(afm->fifo_phys, afm->fifo_cached,
				  0, L1_CACHE_BYTES);
#endif
	/* Mop up any remaining bytes */
	while (lenleft > 0) {
		*buf++ = readb(afm->base + EMINAND_AFM_DATA_FIFO);
		lenleft--;
	}
}
#else
/* Default read buffer command */
static void afm_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	readsl(afm->base + EMINAND_AFM_DATA_FIFO, buf, len/4);
}
#endif

/* Default write buffer command */
static void afm_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	writesl(afm->base + EMINAND_AFM_DATA_FIFO, buf, len/4);
}


/*
 * AFM low-level chip operations
 */

/* AFM: Block Erase */
static void afm_erase_cmd(struct mtd_info *mtd, int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	struct afm_prog	*prog;
	struct nand_chip *chip = mtd->priv;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* Initialise Seq interrupts */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	/* Select AFM program */
	if ((mtd->writesize == 512 && chip->chipsize > (32 << 20)) ||
	    (mtd->writesize == 2048 && chip->chipsize > (128 << 20)))
		/* For 'large' chips, we need an extra address cycle */
		prog = &afm_prog_erase_ext;
	else
		prog = &afm_prog_erase;

	/* Set page address */
	prog->extra_reg	= page;

	/* Copy program to controller, and start sequence */
	memcpy_toio(afm->base + EMINAND_AFM_SEQUENCE_REG_1, prog, 32);

	/* Wait for sequence to finish */
	ret = wait_for_completion_timeout(&afm->seq_completed, 2*HZ);
	if (!ret) {
		dev_warn(afm->dev, "sequence timeout, force exit!\n");
		afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
		return;
	}

	/* Get status */
	reg = afm_readreg(EMINAND_AFM_SEQUENCE_STATUS_REG);
	afm->status =  NAND_STATUS_READY | ((reg & 0x60) >> 5);

	/* Disable interrupts */
	afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
}

static int afm_correct_ecc(struct mtd_info *mtd, unsigned char *buf,
			   unsigned char *read_ecc, unsigned char *calc_ecc)
{
	/* No error */
	if ((read_ecc[0] ^ calc_ecc[0]) == 0 &&
	    (read_ecc[1] ^ calc_ecc[1]) == 0 &&
	    (read_ecc[2] ^ calc_ecc[2]) == 0)
		return 0;

	/* Special test for freshly erased page */
	if (read_ecc[0] == 0xff && calc_ecc[0] == 0x00 &&
	    read_ecc[1] == 0xff && calc_ecc[1] == 0x00 &&
	    read_ecc[2] == 0xff && calc_ecc[2] == 0x00)
		return 0;

	/* Use nand_ecc.c:nand_correct_data() function */
	return nand_correct_data(mtd, buf, read_ecc, calc_ecc);

}

/* AFM: Read Page and OOB Data with ECC */
static int afm_read_page_ecc(struct mtd_info *mtd, struct nand_chip *chip,
			     uint8_t *buf, int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int i, j;
	int eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;

	uint8_t *p = buf;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	uint8_t *e1, *e2, t;

	/* Read raw page+oob data */
	chip->ecc.read_page_raw(mtd, chip, buf, page);

	/* Recover ECC from OOB: H/W ECC + S/W LP16,17 from device */
	e1 = ecc_code;
	for (i = 0; i < chip->ecc.total; i++)
		e1[i] = chip->oob_poi[eccpos[i]];

	/* Generate linux-style 'ecc_code' and 'ecc_calc' */
	e1 = ecc_code;
	e2 = ecc_calc;
	p = buf;
	for (i = 0; i < eccsteps; i++) {
		uint32_t ecc_afm;
		uint8_t lp1617;
		uint32_t chkcode_offs;

		/* Get H/W ECC */
		chkcode_offs = (eccsteps == 1) ? (3 << 2) : (i << 2);
		ecc_afm = afm_readreg(EMINAND_AFM_ECC_CHECKCODE_REG_0 +
				      chkcode_offs);

		/* Extract ecc_code and ecc_calc */
		for (j = 0; j < 3; j++) {
			e2[j] = (unsigned char)(ecc_afm & 0xff);
			ecc_afm >>= 8;
		}

		/* Swap e[0] and e[1] */
		t = e1[0]; e1[0] = e1[1]; e1[1] = t;
		t = e2[0]; e2[0] = e2[1]; e2[1] = t;

		/* Generate S/W LP1617 ecc_calc */
		lp1617 = stm_afm_lp1617(p);

		/* Copy S/W LP1617 bits to standard location */
		e1[2] = (e1[2] & 0xfc) | (e1[6] & 0x3);
		e2[2] = (e2[2] & 0xfc) | (lp1617 & 0x3);

		/* Move on to next ECC block */
		e1 += eccbytes;
		e2 += eccbytes;
		p += eccsize;
	}

	/* Detect/Correct ECC errors */
	p = buf;
	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = afm_correct_ecc(mtd, p, &ecc_code[i], &ecc_calc[i]);

		if (stat == -1) {
			mtd->ecc_stats.failed++;
			printk(KERN_CONT "step %d, page %d]\n",
			       i / eccbytes, page);
		} else if (stat == 1) {
			printk(KERN_CONT "step %d, page = %d]\n",
			       i / eccbytes, page);
			mtd->ecc_stats.corrected++;
		}
	}

	return  0;
}

/* AFM: Read Raw Page and OOB Data */
static int afm_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
			     uint8_t *buf, int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	struct afm_prog	*prog;
	uint32_t reg;
	int ret;

	/* Select AFM program */
	prog = (mtd->writesize == 512) ?
		&afm_prog_read_raw_sp :
		&afm_prog_read_raw_lp;

	/* Initialise RBn interrupt */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, AFM_IRQ_RBN);

	if (mtd->writesize == 512) {
		/* SmallPage: Reset ECC counters */
		reg = afm_readreg(EMINAND_FLEXMODE_CONFIG);
		reg |= (1 << 6);
		afm_writereg(reg, EMINAND_FLEXMODE_CONFIG);
	}

	/* Set page address */
	prog->addr_reg = afm->page << 8;

	/* Copy program to controller, and start sequence */
	memcpy_toio(afm->base + EMINAND_AFM_SEQUENCE_REG_1, prog, 32);

	/* Wait for data to become available */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "RBn timeout, force exit\n");
		afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
		afm_disable_interrupts(afm, AFM_IRQ_RBN);
		return 1;
	}
	/* Read page data and OOB (SmallPage: +48 bytes dummy data) */
	chip->read_buf(mtd, buf, mtd->writesize);
	chip->read_buf(mtd, chip->oob_poi, 64);

	/* Disable RBn interrupts */
	afm_disable_interrupts(afm, AFM_IRQ_RBN);

	return 0;
}

/* AFM: Read OOB data */
static int afm_read_oob_chip(struct mtd_info *mtd, struct nand_chip *chip,
			     int page, int sndcmd)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog	*prog;
	uint32_t reg;
	int ret;

	/* Select AFM program */
	prog = (mtd->writesize == 512) ?
		&afm_prog_read_oob_sp :
		&afm_prog_read_oob_lp;

	/* Initialise RBn interrupt */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, AFM_IRQ_RBN);

	if (mtd->writesize == 512) {
		/* SmallPage: Reset ECC counters and set page address */
		reg = afm_readreg(EMINAND_FLEXMODE_CONFIG);
		reg |= (1 << 6);
		afm_writereg(reg, EMINAND_FLEXMODE_CONFIG);

		prog->addr_reg	= page << 8;
	} else {
		/* LargePage: Set page address */
		prog->addr_reg	= (page << 8) | (2048 >> 8);
	}

	/* Copy program to controller, and start sequence */
	memcpy_toio(afm->base + EMINAND_AFM_SEQUENCE_REG_1, prog, 32);

	/* Wait for data to become available */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "RBn timeout, force exit\n");
		afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
		afm_disable_interrupts(afm, AFM_IRQ_RBN);
		return 1;
	}

	/* Read OOB data to chip->oob_poi buffer */
	chip->read_buf(mtd, chip->oob_poi, 64);

	/* Disable RBn Interrupts */
	afm_disable_interrupts(afm, AFM_IRQ_RBN);

	return 0;
}

/* AFM: Write Page and OOB Data, with ECC support [SmallPage] */
static void afm_write_page_ecc_sp(struct mtd_info *mtd,
				  struct nand_chip *chip,
				  const uint8_t *buf)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog *prog = &afm_prog_write_ecc_sp_a;
	uint32_t ecc_afm;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* 1. Write page data to chip's page buffer */

	/*    Initialise Seq interrupt */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	/*    Reset ECC counters */
	reg = afm_readreg(EMINAND_FLEXMODE_CONFIG);
	reg |= (1 << 6);
	afm_writereg(reg, EMINAND_FLEXMODE_CONFIG);

	/*    Set page address */
	prog->addr_reg	= afm->page << 8;

	/*    Copy program to controller, and start sequence */
	memcpy_toio(afm->base + EMINAND_AFM_SEQUENCE_REG_1, prog, 32);

	/*    Write page data */
	chip->write_buf(mtd, buf, mtd->writesize);

	/*    Wait for the sequence to terminate */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "seq timeout, force exit\n");
		afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
		return;
	}

	/*    Disable Seq interrupt */
	afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	/* 2. Write OOB data */
	/*    Populate OOB with ECC data  */
	ecc_afm = afm_readreg(EMINAND_AFM_ECC_CHECKCODE_REG_3);
	chip->oob_poi[0] = ecc_afm & 0xff; ecc_afm >>= 8;
	chip->oob_poi[1] = ecc_afm & 0xff; ecc_afm >>= 8;
	chip->oob_poi[2] = ecc_afm & 0xff;
	chip->oob_poi[3] = 'A';
	chip->oob_poi[4] = 'F';
	chip->oob_poi[5] = 'M';
	chip->oob_poi[6] = stm_afm_lp1617(buf);

	/*    Switch to FLEX mode for writing OOB */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, AFM_IRQ_RBN);
	afm_writereg(0x00000001, EMINAND_FLEXMODE_CONFIG);

	/*    Write OOB data */
	afm_writereg(FLX_DATA_CFG_BEAT_4 | FLX_DATA_CFG_CSN_STATUS,
		     EMINAND_FLEX_DATAWRITE_CONFIG);
	writesl(afm->base + EMINAND_FLEX_DATA, chip->oob_poi, 4);
	afm_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		     EMINAND_FLEX_DATAWRITE_CONFIG);

	/*    Issue page program command */
	afm_writereg(FLX_CMD(NAND_CMD_PAGEPROG), EMINAND_FLEX_COMMAND_REG);

	/*    Wait for page program operation to complete */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret)
		dev_err(afm->dev, "RBn timeout, force exit\n");

	/*     Get status */
	afm_writereg(FLX_CMD(NAND_CMD_STATUS), EMINAND_FLEX_COMMAND_REG);
	afm->status = afm_readreg(EMINAND_FLEX_DATA);

	afm_disable_interrupts(afm, AFM_IRQ_RBN);

	/* 3. Switch back to AFM */
	afm_writereg(0x00000002, EMINAND_FLEXMODE_CONFIG);
}


/* AFM: Write Page and OOB Data, with ECC support [LargePage]  */
static void afm_write_page_ecc_lp(struct mtd_info *mtd,
				  struct nand_chip *chip,
				  const uint8_t *buf)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	const uint8_t *p;

	struct afm_prog *prog = &afm_prog_write_ecc_lp;

	uint32_t reg;
	int ret;
	int i;

	afm->status = 0;

	/* Calc S/W ECC LP1617, insert into OOB area with 'AFM' signature */
	p = buf;
	for (i = 0; i < eccsteps; i++) {
		chip->oob_poi[eccpos[3+i*eccbytes]] = 'A';
		chip->oob_poi[eccpos[4+i*eccbytes]] = 'F';
		chip->oob_poi[eccpos[5+i*eccbytes]] = 'M';
		chip->oob_poi[eccpos[6+i*eccbytes]] = stm_afm_lp1617(p);
		p += eccsize;
	}

	/* Initialise Seq interrupt */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	/* Reset ECC counters */
	reg = afm_readreg(EMINAND_FLEXMODE_CONFIG);
	reg |= (1 << 6);
	afm_writereg(reg, EMINAND_FLEXMODE_CONFIG);

	/* Set page address */
	prog->addr_reg	= afm->page << 8;

	/* Copy program to controller, and start sequence */
	memcpy_toio(afm->base + EMINAND_AFM_SEQUENCE_REG_1, prog, 32);

	/* Write page and oob data */
	chip->write_buf(mtd, buf, mtd->writesize);
	chip->write_buf(mtd, chip->oob_poi, 64);

	/* Wait for sequence to complete */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "seq timeout, force exit\n");
		afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
		return;
	}

	/* Get status */
	reg = afm_readreg(EMINAND_AFM_SEQUENCE_STATUS_REG);
	afm->status =  NAND_STATUS_READY | ((reg & 0x60) >> 5);

	/* Disable Seq interrupt */
	afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
}

/* AFM: Write Raw Page and OOB Data [LargePage] */
static void afm_write_page_raw_lp(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog *prog = &afm_prog_write_raw_lp;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* Initialise Seq Interrupts */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	/* Set page address */
	prog->addr_reg	= afm->page << 8;

	/* Copy program to controller, and start sequence */
	memcpy_toio(afm->base + EMINAND_AFM_SEQUENCE_REG_1, prog, 32);

	/* Write page and OOB data */
	chip->write_buf(mtd, buf, 2048);
	chip->write_buf(mtd, chip->oob_poi, 64);

	/* Wait for sequence to complete */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "seq timeout, force exit\n");
		afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
		return;
	}

	/* Get status */
	reg = afm_readreg(EMINAND_AFM_SEQUENCE_STATUS_REG);
	afm->status =  NAND_STATUS_READY | ((reg & 0x60) >> 5);

	/* Disable Seq Interrupts */
	afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
}

/* AFM: Write Raw Page and OOB Data [SmallPage] */
static void afm_write_page_raw_sp(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog *prog = &afm_prog_write_raw_sp_a;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* Enable sequence interrupts */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	/* 1. Write page data to chip's page buffer */
	prog->addr_reg	= afm->page << 8;
	memcpy_toio(afm->base + EMINAND_AFM_SEQUENCE_REG_1, prog, 32);

	chip->write_buf(mtd, buf, mtd->writesize);

	ret = wait_for_completion_timeout(&afm->seq_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "seq timeout, force exit\n");
		afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
		return;
	}
	afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	reg = afm_readreg(EMINAND_AFM_SEQUENCE_STATUS_REG);
	if ((reg & 0x60) != 0) {
		dev_err(afm->dev, "error programming page data\n");
		afm->status = NAND_STATUS_FAIL;
		return;
	}

	/* 2. Switch to FLEX mode and write OOB data */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, AFM_IRQ_RBN);
	afm_writereg(0x00000001, EMINAND_FLEXMODE_CONFIG);

	/*    Send OOB pointer operation */
	afm_writereg(FLX_CMD(NAND_CMD_READOOB),  EMINAND_FLEX_COMMAND_REG);

	/*    Send SEQIN command */
	afm_writereg(FLX_CMD(NAND_CMD_SEQIN), EMINAND_FLEX_COMMAND_REG);

	/*    Send page address */
	reg = afm->page << 8 | FLX_ADDR_REG_ADD8_VALID |
		FLX_ADDR_REG_CSN_STATUS;
	if (chip->chipsize > (32 << 20))
		reg |= FLX_ADDR_REG_BEAT_4;
	else
		reg |= FLX_ADDR_REG_BEAT_3;
	afm_writereg(reg, EMINAND_FLEX_ADDRESS_REG);

	/*    Send OOB data */
	afm_writereg(FLX_DATA_CFG_BEAT_4 | FLX_DATA_CFG_CSN_STATUS,
		     EMINAND_FLEX_DATAWRITE_CONFIG);
	writesl(afm->base + EMINAND_FLEX_DATA, chip->oob_poi, 4);
	afm_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		     EMINAND_FLEX_DATAWRITE_CONFIG);

	/*    Send page program command */
	afm_writereg(FLX_CMD(NAND_CMD_PAGEPROG),  EMINAND_FLEX_COMMAND_REG);

	/* Wait for page program operation to complete */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret)
		dev_err(afm->dev, "RBn timeout!\n");

	/*     Get status */
	afm_writereg(FLX_CMD(NAND_CMD_STATUS), EMINAND_FLEX_COMMAND_REG);
	afm->status = afm_readreg(EMINAND_FLEX_DATA);

	afm_disable_interrupts(afm, AFM_IRQ_RBN);

	/* 3. Switch back to AFM */
	afm_writereg(0x00000002, EMINAND_FLEXMODE_CONFIG);
}

/* AFM: Write OOB Data [LargePage] */
static int afm_write_oob_chip_lp(struct mtd_info *mtd, struct nand_chip *chip,
				 int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog *prog = &afm_prog_write_oob_lp;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* Enable sequence interrupts */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	prog->addr_reg	= (page << 8) | (2048 >> 8);

	memcpy_toio(afm->base + EMINAND_AFM_SEQUENCE_REG_1, prog, 32);

	/* Write OOB */
	chip->write_buf(mtd, chip->oob_poi, 64);

	/* Wait for sequence to complete */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ);
	if (!ret) {
		dev_err(afm->dev, "seq timeout, force exit\n");
		afm_writereg(0x00000000, EMINAND_AFM_SEQUENCE_CONFIG_REG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);
		return 1;
	}

	/* Get status */
	reg = afm_readreg(EMINAND_AFM_SEQUENCE_STATUS_REG);
	afm->status =  NAND_STATUS_READY | ((reg & 0x60) >> 5);

	/* Disable sequence interrupts */
	afm_disable_interrupts(afm, AFM_IRQ_SEQ_DREQ);

	return afm->status & NAND_STATUS_FAIL ? -EIO : 0;
}

/* AFM: Write OOB Data [SmallPage] */
static int afm_write_oob_chip_sp(struct mtd_info *mtd, struct nand_chip *chip,
				 int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int ret;
	uint32_t status;
	uint32_t reg;

	afm->status = 0;

	/* Initialise interrupts */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, AFM_IRQ_RBN);

	/* Switch to Flex Mode */
	afm_writereg(0x00000001, EMINAND_FLEXMODE_CONFIG);

	/* Pointer Operation */
	afm_writereg(FLX_CMD(NAND_CMD_READOOB), EMINAND_FLEX_COMMAND_REG);

	/* Send SEQIN command */
	afm_writereg(FLX_CMD(NAND_CMD_SEQIN), EMINAND_FLEX_COMMAND_REG);

	/* Send Col/Page Addr */
	reg = (page << 8 |
	       FLX_ADDR_REG_RBN |
	       FLX_ADDR_REG_ADD8_VALID |
	       FLX_ADDR_REG_CSN_STATUS);

	if (chip->chipsize > (32 << 20))
		/* Need extra address cycle for 'large' devices */
		reg |= FLX_ADDR_REG_BEAT_4;
	else
		reg |= FLX_ADDR_REG_BEAT_3;
	afm_writereg(reg, EMINAND_FLEX_ADDRESS_REG);

	/* Write OOB data */
	afm_writereg(FLX_DATA_CFG_BEAT_4 | FLX_DATA_CFG_CSN_STATUS,
		     EMINAND_FLEX_DATAWRITE_CONFIG);
	writesl(afm->base + EMINAND_FLEX_DATA, chip->oob_poi, 4);
	afm_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		     EMINAND_FLEX_DATAWRITE_CONFIG);

	/* Issue Page Program command */
	afm_writereg(FLX_CMD(NAND_CMD_PAGEPROG), EMINAND_FLEX_COMMAND_REG);

	/* Wait for page program operation to complete */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret)
		dev_err(afm->dev, "RBn timeout\n");

	/* Get status */
	afm_writereg(FLX_CMD(NAND_CMD_STATUS), EMINAND_FLEX_COMMAND_REG);
	status = afm_readreg(EMINAND_FLEX_DATA);

	afm->status = 0xff & status;

	/* Switch back to AFM */
	afm_writereg(0x00000002, EMINAND_FLEXMODE_CONFIG);

	/* Disable RBn interrupts */
	afm_disable_interrupts(afm, AFM_IRQ_RBN);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

/* Read the device electronic signature */
static int afm_read_sig(struct mtd_info *mtd,
		       int *maf_id, int *dev_id,
		       uint8_t *cellinfo, uint8_t *extid)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	uint32_t ret, reg;

	/* Enable RBn interrupts */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, AFM_IRQ_RBN);

	/* Switch to Flex Mode */
	afm_writereg(0x00000001, EMINAND_FLEXMODE_CONFIG);

	/* Issue NAND reset */
	afm_writereg(FLX_CMD(NAND_CMD_RESET), EMINAND_FLEX_COMMAND_REG);

	/* Wait for reset to complete */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret)
		dev_err(afm->dev, "RBn timeout\n");

	/* Read electronic signature */
	afm_writereg(FLX_CMD(NAND_CMD_READID), EMINAND_FLEX_COMMAND_REG);

	reg = (0x00 |
	       FLX_ADDR_REG_BEAT_1 |
	       FLX_ADDR_REG_RBN |
	       FLX_ADDR_REG_CSN_STATUS);
	afm_writereg(reg, EMINAND_FLEX_ADDRESS_REG);

	afm_writereg(FLX_DATA_CFG_BEAT_4 | FLX_DATA_CFG_CSN_STATUS,
		     EMINAND_FLEX_DATAREAD_CONFIG);
	reg = afm_readreg(EMINAND_FLEX_DATA);
	afm_writereg(FLX_DATA_CFG_BEAT_1 | FLX_DATA_CFG_CSN_STATUS,
		      EMINAND_FLEX_DATAREAD_CONFIG);

	/* Extract manufacturer and device ID */
	*maf_id = reg & 0xff;
	*dev_id = (reg >> 8) & 0xff;

	/* Newer devices have all the information in additional id bytes */
	*cellinfo = (reg >> 16) & 0xff;
	*extid = (reg >> 24) & 0xff;

	/* Switch back to AFM Mode */
	afm_writereg(0x00000002, EMINAND_FLEXMODE_CONFIG);

	/* Disable RBn interrupts */
	afm_disable_interrupts(afm, AFM_IRQ_RBN);

	return 0;
}

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
/* For boot-mode, we use software ECC with AFM raw read/write commands */
static int boot_calc_ecc(struct mtd_info *mtd, const unsigned char *buf,
			 unsigned char *ecc)
{
	stm_ecc_gen(buf, ecc, ECC_128);
	ecc[3] = 'B';

	return 0;
}

static int boot_correct_ecc(struct mtd_info *mtd, unsigned char *buf,
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

static int afm_read_page_boot(struct mtd_info *mtd, struct nand_chip *chip,
			      uint8_t *buf, int page)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	chip->ecc.read_page_raw(mtd, chip, buf, page);

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		boot_calc_ecc(mtd, p, &ecc_calc[i]);

	for (i = 0; i < chip->ecc.total; i++)
		ecc_code[i] = chip->oob_poi[eccpos[i]];

	eccsteps = chip->ecc.steps;
	p = buf;

	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = boot_correct_ecc(mtd, p, &ecc_code[i], &ecc_calc[i]);
		if (stat == -1) {
			printk(KERN_CONT "step %d, page %d]\n",
			       i / eccbytes, page);
			mtd->ecc_stats.failed++;
		} else if (stat == 1) {
			printk(KERN_CONT "step %d, page = %d]\n",
			       i / eccbytes, page);
			mtd->ecc_stats.corrected += stat;
		}
	}
	return 0;
}

static void afm_write_page_boot(struct mtd_info *mtd,
				struct nand_chip *chip,
				const uint8_t *buf)

{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	const uint8_t *p = buf;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	/* Software ecc calculation */
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		boot_calc_ecc(mtd, p, &ecc_calc[i]);

	for (i = 0; i < chip->ecc.total; i++)
		chip->oob_poi[eccpos[i]] = ecc_calc[i];

	chip->ecc.write_page_raw(mtd, chip, buf);

}
#endif /* CONFIG_STM_NAND_AFM_BOOTMODESUPPORT */


/* AFM : Select Chip
 * For now we only support 1 chip per 'stm_nand_afm_device' so chipnr will be 0
 * for select, -1 for deselect.
 */
static void afm_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;

	/* Deselect, do nothing */
	if (chipnr == -1) {
		return;
	} else if (chipnr == 0) {
		/* If same chip as last time, no need to change anything */
		if (data->csn == afm->current_csn)
			return;

		/* Set CSn on AFM controller */
		afm->current_csn = data->csn;
		afm_writereg(0x1 << data->csn, EMINAND_MUXCONTROL_REG);

		/* Set up timing parameters */
		afm_set_timings(afm, data->timing_data);
	} else {
		dev_err(afm->dev, "attempt to select chipnr = %d\n", chipnr);
	}

	return;
}

/* The low-level AFM chip operations wait internally, and set the status field.
 * All that is left to do here is return the status */
static int afm_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	return afm->status;
}

/*
 * afm_command() and afm_read_byte() are treated as special cases here.  We need
 * to support nand_base.c:nand_erase_nand() because this is called directly by
 * nand_bbt.c (why is it not a callback?).  However, nand_erase_nand() calls
 * nand_check_wp() which in turn calls chip->cmdfunc() and chip->read_byte() in
 * order to check the status register.  Since nand_check_wp() is the only
 * function that uses these callbacks, we can implement specialised versions
 * such that afm_command() is empty, and afm_read_byte() queries and returns the
 * status register.
 *
 * Need to track updates to nand_base.c to ensure these assumptions remain valid
 * in the future!
 */
static void afm_command(struct mtd_info *mtd, unsigned int command,
			 int column, int page_addr)
{

}

static uint8_t afm_read_byte(struct mtd_info *mtd)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	uint32_t reg;

	/* Switch to Flex Mode */
	afm_writereg(0x00000001, EMINAND_FLEXMODE_CONFIG);

	/* Write STATUS command (do not set FLX_CMD_REG_RBN!) */
	reg = (NAND_CMD_STATUS | FLX_CMD_REG_BEAT_1 | FLX_CMD_REG_CSN_STATUS);
	afm_writereg(reg, EMINAND_FLEX_COMMAND_REG);

	reg = afm_readreg(EMINAND_FLEX_DATA);

	/* Switch back to AFM */
	afm_writereg(0x00000002, EMINAND_FLEXMODE_CONFIG);

	return reg & 0xff;
}

static int afm_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	printk(KERN_ERR NAME ": %s Unsupported callback", __func__);
	BUG();

	return 0;
}

static u16 afm_read_word(struct mtd_info *mtd)
{
	printk(KERN_ERR NAME ": %s Unsupported callback", __func__);
	BUG();

	return 0xff;
}

/*
 * AFM scan/probe NAND routines
 */

/* Set AFM generic call-backs (not chip-specific) */
static void afm_set_defaults(struct nand_chip *chip, int busw)
{
	chip->chip_delay = 0;
	chip->cmdfunc = afm_command;
	chip->waitfunc = afm_wait;
	chip->select_chip = afm_select_chip;
	chip->read_byte = afm_read_byte;
	chip->read_word = afm_read_word;
	chip->block_bad = NULL;
	chip->block_markbad = NULL;
	chip->write_buf = afm_write_buf;
	chip->verify_buf = afm_verify_buf;
	chip->scan_bbt = nand_default_bbt;
#ifdef CONFIG_STM_NAND_AFM_CACHED
	chip->read_buf = afm_read_buf_cached;
#else
	chip->read_buf = afm_read_buf;
#endif
}


/* Determine the NAND device paramters
 * [cf nand_base.c:nand_get_flash_type()] */
static struct nand_flash_dev *afm_get_flash_type(struct mtd_info *mtd,
						 struct nand_chip *chip,
						 int busw, int *maf_id)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct nand_flash_dev *type = NULL;
	int i, dev_id, maf_idx;
	uint8_t cellinfo;
	uint8_t extid;

	/* Select the device */
	chip->select_chip(mtd, 0);

	/* Read the electronic signature */
	afm_read_sig(mtd, maf_id, &dev_id, &cellinfo, &extid);

	/* Lookup device */
	for (i = 0; nand_flash_ids[i].name != NULL; i++) {
		if (dev_id == nand_flash_ids[i].id) {
			type =  &nand_flash_ids[i];
			break;
		}
	}

	if (!type)
		return ERR_PTR(-ENODEV);

	if (!mtd->name)
		mtd->name = type->name;

	chip->chipsize = type->chipsize << 20;

	if (!type->pagesize) {
		/* New devices use the extended chip info... */
		chip->cellinfo = cellinfo;

		/* Calc pagesize */
		mtd->writesize = 1024 << (extid & 0x3);
		extid >>= 2;

		/* Calc oobsize */
		mtd->oobsize = (8 << (extid & 0x01)) * (mtd->writesize >> 9);
		extid >>= 2;

		/* Calc blocksize. Blocksize is multiples of 64KiB */
		mtd->erasesize = (64 * 1024) << (extid & 0x03);
		extid >>= 2;

		/* Get buswidth information */
		busw = (extid & 0x01) ? NAND_BUSWIDTH_16 : 0;
	} else {
		/* Old devices have chip data hardcoded in device id table */
		mtd->erasesize = type->erasesize;
		mtd->writesize = type->pagesize;
		mtd->oobsize = mtd->writesize / 32;
		busw = type->options & NAND_BUSWIDTH_16;
	}

	afm_generic_config(afm, chip->options & NAND_BUSWIDTH_16,
			   mtd->writesize, chip->chipsize);

	/* Try to identify manufacturer */
	for (maf_idx = 0; nand_manuf_ids[maf_idx].id != 0x0; maf_idx++) {
		if (nand_manuf_ids[maf_idx].id == *maf_id)
			break;
	}

	/*
	 * Check, if buswidth is correct. Hardware drivers should set
	 * chip correct !
	 */
	if (busw != (chip->options & NAND_BUSWIDTH_16)) {
		dev_info(afm->dev, "NAND device: Manufacturer ID:"
			 " 0x%02x, Chip ID: 0x%02x (%s %s)\n", *maf_id,
			 dev_id, nand_manuf_ids[maf_idx].name, mtd->name);
		dev_info(afm->dev, "NAND bus width %d instead %d bit\n",
			 (chip->options & NAND_BUSWIDTH_16) ? 16 : 8,
			 busw ? 16 : 8);
		return ERR_PTR(-EINVAL);
	}

	/* Calculate the address shift from the page size */
	chip->page_shift = ffs(mtd->writesize) - 1;

	/* Convert chipsize to number of pages per chip -1. */
	chip->pagemask = (chip->chipsize >> chip->page_shift) - 1;

	chip->bbt_erase_shift = ffs(mtd->erasesize) - 1;
	chip->phys_erase_shift = chip->bbt_erase_shift;

	chip->chip_shift = ffs(chip->chipsize) - 1;

	/* Set the bad block position [AAC: Now obsolete?] */
	chip->badblockpos = mtd->writesize > 512 ?
		NAND_LARGE_BADBLOCK_POS : NAND_SMALL_BADBLOCK_POS;

	/* Get chip options, preserve non chip based options */
	chip->options &= ~NAND_CHIPOPTIONS_MSK;
	chip->options |= type->options & NAND_CHIPOPTIONS_MSK;

	/*
	 * Set chip as a default. Board drivers can override it, if necessary
	 */
	chip->options |= NAND_NO_AUTOINCR;

	/* Check if chip is a not a samsung device. Do not clear the
	 * options for chips which are not having an extended id.
	 */
	if (*maf_id != NAND_MFR_SAMSUNG && !type->pagesize)
		chip->options &= ~NAND_SAMSUNG_LP_OPTIONS;


	chip->erase_cmd = afm_erase_cmd;

	dev_info(afm->dev, "NAND device: Manufacturer ID:"
		 " 0x%02x, Chip ID: 0x%02x (%s %s)\n", *maf_id, dev_id,
		 nand_manuf_ids[maf_idx].name, type->name);

	return type;
}


/* Scan device, part 1 : Determine device paramters */
int afm_scan_ident(struct mtd_info *mtd, int maxchips)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int busw, maf_id;
	struct nand_chip *chip = mtd->priv;
	struct nand_flash_dev *type = NULL;

	if (maxchips != 1) {
		dev_err(afm->dev, "Only chip per MTD device allowed\n");
		return 1;
	}

	busw = chip->options & NAND_BUSWIDTH_16;
	afm_set_defaults(chip, busw);

	type = afm_get_flash_type(mtd, chip, busw, &maf_id);

	if (IS_ERR(type)) {
		dev_err(afm->dev, "No NAND device found\n");
		chip->select_chip(mtd, -1);
		return PTR_ERR(type);
	}

	/* Not dealing with multichip support just yet! */
	chip->numchips = 1;
	mtd->size = chip->chipsize;

	return 0;

}

/* Scan device, part 2: Configure AFM according to device paramters */
static int afm_scan_tail(struct mtd_info *mtd)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int i;
	struct nand_chip *chip = mtd->priv;

	int ret;

	if (!(chip->options & NAND_OWN_BUFFERS))
		chip->buffers = kmalloc(sizeof(*chip->buffers), GFP_KERNEL);
	if (!chip->buffers)
		return -ENOMEM;

	/* Set the internal oob buffer location, just after the page data */
	chip->oob_poi = chip->buffers->databuf + mtd->writesize;

	if (mtd->writesize == 512 && mtd->oobsize == 16) {
		chip->ecc.layout = &afm_oob_16;
		chip->badblock_pattern = &bbt_scan_sp;
	} else if (mtd->writesize == 2048 && mtd->oobsize == 64) {
		chip->ecc.layout = &afm_oob_64;
		chip->badblock_pattern = &bbt_scan_lp;
	} else {
		dev_err(afm->dev, "Unsupported chip type "
			"[pagesize = %d, oobsize = %d]\n",
			mtd->writesize, mtd->oobsize);
		return 1;
	}

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	/* Handle boot-mode ECC when scanning for bad blocks */
	chip->badblock_pattern->options |= NAND_BBT_SCANSTMBOOTECC;
#endif

	/* Set ECC parameters and call-backs */
	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.size = 512;
	chip->ecc.bytes = 7;
	chip->write_page = afm_write_page;
	chip->ecc.read_page = afm_read_page_ecc;
	chip->ecc.read_page_raw = afm_read_page_raw;
	chip->ecc.read_oob = afm_read_oob_chip;
	if (mtd->writesize == 512) {
		chip->ecc.write_page = afm_write_page_ecc_sp;
		chip->ecc.write_page_raw = afm_write_page_raw_sp;
		chip->ecc.write_oob = afm_write_oob_chip_sp;
	} else {
		chip->ecc.write_page = afm_write_page_ecc_lp;
		chip->ecc.write_page_raw = afm_write_page_raw_lp;
		chip->ecc.write_oob = afm_write_oob_chip_lp;
	}
	chip->ecc.steps = mtd->writesize / chip->ecc.size;
	if (chip->ecc.steps * chip->ecc.size != mtd->writesize) {
		dev_err(afm->dev, "Invalid ecc parameters\n");
		return 1;
	}
	chip->ecc.total = chip->ecc.steps * chip->ecc.bytes;
	chip->ecc.calculate = NULL;	/* Hard-coded in call-backs */
	chip->ecc.correct = NULL;

	/* Count the number of OOB bytes available for client data */
	chip->ecc.layout->oobavail = 0;
	for (i = 0; chip->ecc.layout->oobfree[i].length
		     && i < MTD_MAX_OOBFREE_ENTRIES; i++)
		chip->ecc.layout->oobavail +=
			chip->ecc.layout->oobfree[i].length;
	mtd->oobavail = chip->ecc.layout->oobavail;

	/* Disable subpage writes for now */
	mtd->subpage_sft = 0;
	chip->subpagesize = mtd->writesize >> mtd->subpage_sft;

	/* Initialize state */
	chip->state = FL_READY;

	/* Invalidate the pagebuffer reference */
	chip->pagebuf = -1;

	/* Fill in remaining MTD driver data */
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->erase = afm_erase;  /* uses chip->erase_cmd */
	mtd->point = NULL;
	mtd->unpoint = NULL;
	mtd->read = afm_read;
	mtd->write = afm_write;
	mtd->read_oob = afm_read_oob;
	mtd->write_oob = afm_write_oob;
	mtd->sync = afm_sync;
	mtd->lock = NULL;
	mtd->unlock = NULL;
	mtd->suspend = afm_suspend;
	mtd->resume = afm_resume;
	mtd->block_isbad = afm_block_isbad;
	mtd->block_markbad = afm_block_markbad;

	/* Propagate ecc.layout to mtd_info */
	mtd->ecclayout = chip->ecc.layout;

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	/* Setup ECC params, for AFM and BOOT operation */
	afm_setup_eccparams(mtd);
#endif

	/* Check, if we should skip the bad block table scan */
	if (chip->options & NAND_SKIP_BBTSCAN)
		return 0;

	/* Build bad block table */
	ret = chip->scan_bbt(mtd);

	return ret;
}

/* Scan for the NAND device */
int afm_scan(struct mtd_info *mtd, int maxchips)
{
	int ret;

	ret = afm_scan_ident(mtd, maxchips);
	if (!ret)
		ret = afm_scan_tail(mtd);


	return ret;
}

#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY
/* The NAND_BLOCK_ZERO_REMAP_REG is not implemented on current versions of the
 * NAND controller.  We must therefore scan for the logical block 0 pattern.
 */
static int check_block_zero_pattern(uint8_t *buf)
{
	uint8_t i;
	uint8_t *b;

	for (i = 0, b = buf + 128; i < 64; i++, b++)
		if (*b != i)
			return 1;

	return 0;
}

static int pbl_boot_boundary(struct mtd_info *mtd, uint32_t *_boundary)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;
	uint8_t *buf = chip->buffers->databuf;
	int block;
	int block_zero_phys = -1;
	int pages_per_block;
	uint32_t boundary_log = 0;
	uint32_t boundary_phys;

	/* Select boot-mode ECC */
	afm_select_eccparams(mtd, data->boot_start);

	/* Find logical block zero */
	pages_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);
	afm->col = 0;
	afm->page = 0;
	for (block = 0; block < 512; block++) {
		if (nand_isbad_bbt(mtd, block << chip->phys_erase_shift, 0)
		    == 0) {
			afm_read_page_boot(mtd, chip, buf, afm->page);

			if (check_block_zero_pattern(buf) == 0) {
				block_zero_phys = block;
				boundary_log = *((uint32_t *)(buf + 0x0034));
				break;
			}
		}
		afm->page += pages_per_block;
	}

	/* Return if we didn't find logical block zero */
	if (block_zero_phys == -1)
		return 1;

	/* For bootmode_boundary, map logical offset to physical offset */
	boundary_phys = block_zero_phys << chip->phys_erase_shift;
	while (boundary_log >= mtd->erasesize) {
		boundary_phys += mtd->erasesize;
		if (!afm_block_isbad(mtd, boundary_phys))
			boundary_log -= mtd->erasesize;
	}
	boundary_phys += boundary_log;		/* Add residual offset */

	dev_info(afm->dev, "boot-mode boundary = 0x%08x (physical)\n",
		 boundary_phys);

	*_boundary = boundary_phys;

	return 0;
}
#endif


static struct stm_nand_afm_device * __init
afm_init_bank(struct stm_nand_afm_controller *afm,
	      struct stm_nand_bank_data *bank,
	      const char *name)
{
	struct stm_nand_afm_device *data;
	int err;

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	struct mtd_info *slave;
	struct mtd_part *part;
	char *boot_part_name;
#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY
	uint32_t boundary;
#endif
#endif

	/* Allocate memory for the driver structure (and zero it) */
	data = kzalloc(sizeof(struct stm_nand_afm_device), GFP_KERNEL);
	if (!data) {
		dev_err(afm->dev, "failed to allocate device structure\n");
		err = -ENOMEM;
		goto err1;
	}

	data->chip.priv = data;
	data->mtd.priv = &data->chip;
	data->mtd.owner = THIS_MODULE;
	data->dev = afm->dev;

	/* Use hwcontrol structure to manage access to AFM Controller */
	data->chip.controller = &afm->hwcontrol;
	data->chip.state = FL_READY;

	/* Assign more sensible name (default is string from nand_ids.c!) */
	data->mtd.name = name;
	data->csn = bank->csn;

	data->timing_data = bank->timing_data;

	data->chip.options = bank->options;
	data->chip.options |= NAND_NO_AUTOINCR;

	/* Scan to find existance of device */
	if (afm_scan(&data->mtd, 1)) {
		dev_err(afm->dev, "device scan failed\n");
		err = -ENXIO;
		goto err2;
	}

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	/* Set name of boot partition */
	boot_part_name = nbootpart ? nbootpart :
		CONFIG_STM_NAND_AFM_BOOTPARTITION;
	dev_info(afm->dev, "using boot partition name [%s] (from %s)\n",
		 boot_part_name, nbootpart ? "command line" : "kernel config");
#endif

#ifdef CONFIG_MTD_PARTITIONS
	/* Try probing for MTD partitions */
	data->nr_parts = parse_mtd_partitions(&data->mtd,
					      part_probes,
					      &data->parts, 0);

	/* Try platform data */
	if (!data->nr_parts && bank->partitions) {
		data->parts = bank->partitions;
		data->nr_parts = bank->nr_partitions;
	}

	/* Add any partitions that were found */
	if (data->nr_parts) {
		err = add_mtd_partitions(&data->mtd, data->parts,
					 data->nr_parts);
		if (err)
			goto err3;

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
		/* Update boot-mode slave partition */
		slave = get_mtd_partition_slave(&data->mtd, boot_part_name);
		if (slave) {
			dev_info(afm->dev, "found boot-mode partition "
				 "updating ECC paramters\n");

			part = PART(slave);
			data->boot_start = part->offset;
			data->boot_end = part->offset + slave->size;

#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY
			/* Update 'boot_end' with value in PBL image */
			if (pbl_boot_boundary(&data->mtd,
					      &boundary) != 0) {
				dev_info(afm->dev, "failed to get boot-mode "
					 "boundary from PBL\n");
			} else {
				if (boundary < data->boot_start ||
				    boundary > data->boot_end) {
					dev_info(afm->dev,
						 "PBL boot-mode "
						 "boundary lies outside "
						 "boot partition\n");
				} else {
					dev_info(afm->dev,
						 "Updating boot-mode "
						 "ECC boundary from PBL");
					data->boot_end = boundary;
				}
			}
#endif
			slave->oobavail =
				data->ecc_boot.ecc_ctrl.layout->oobavail;
			slave->subpage_sft =
				data->ecc_boot.subpage_sft;
			slave->ecclayout =
				data->ecc_boot.ecc_ctrl.layout;

			dev_info(afm->dev, "boot-mode ECC: 0x%08x->0x%08x\n",
				 (unsigned int)data->boot_start,
				 (unsigned int)data->boot_end);

		} else {
			dev_info(afm->dev, "failed to find boot "
				 "partition [%s]\n", boot_part_name);
		}
#endif
	} else
#endif

		err = add_mtd_device(&data->mtd);

	/* Success! */
	if (!err)
		return data;

#ifdef CONFIG_MTD_PARTITIONS
 err3:
	if (data->parts && data->parts != bank->partitions)
		kfree(data->parts);
#endif
 err2:
	kfree(data);
 err1:
	return ERR_PTR(err);
}

/*
 * stm-nand-afm device probe
 */
static int __init stm_afm_probe(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata = pdev->dev.platform_data;
	struct stm_nand_bank_data *bank;
	struct stm_nand_afm_controller *afm;
	int n;
	int res;

	afm = afm_init_controller(pdev);
	if (IS_ERR(afm)) {
		dev_err(&pdev->dev, "failed to initialise NAND Controller.\n");
		res = PTR_ERR(afm);
		return res;
	}

	bank = pdata->banks;
	for (n = 0; n < pdata->nr_banks; n++) {
		afm->devices[n] = afm_init_bank(afm, bank,
						dev_name(&pdev->dev));
		bank++;
	}

	platform_set_drvdata(pdev, afm);

	return 0;
}


static int __devexit stm_afm_remove(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata = pdev->dev.platform_data;
	struct stm_nand_afm_controller *afm = platform_get_drvdata(pdev);
	int n;

	for (n = 0; n < pdata->nr_banks; n++) {
		struct stm_nand_afm_device *data = afm->devices[n];
		nand_release(&data->mtd);

#ifdef CONFIG_MTD_PARTITIONS
		if (data->parts && data->parts != pdata->banks[n].partitions)
			kfree(data->parts);
#endif
		kfree(data);
	}

	afm_exit_controller(pdev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver stm_afm_nand_driver = {
	.probe		= stm_afm_probe,
	.remove		= stm_afm_remove,
	.driver		= {
		.name	= NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init stm_afm_nand_init(void)
{
	return platform_driver_register(&stm_afm_nand_driver);
}

static void __exit stm_afm_nand_exit(void)
{
	platform_driver_unregister(&stm_afm_nand_driver);
}

module_init(stm_afm_nand_init);
module_exit(stm_afm_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Angus Clark");
MODULE_DESCRIPTION("STM AFM-based NAND Flash driver");
