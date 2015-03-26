/*
 * stm_spi_fsm.c	Support for STM SPI Serial Flash Controller
 *
 * Author: Angus Clark <angus.clark@st.com>
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 *
 * JEDEC probe based on drivers/mtd/devices/m25p80.c
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/stm/platform.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include "stm_spi_fsm.h"

#define NAME		"stm-spi-fsm"

#define FLASH_DEFAULT_FREQ	10000000	/* Default and probe freq. */
#define FLASH_PAGESIZE		256
#define FLASH_MAX_BUSY_WAIT	(10 * HZ)	/* Maximum erase time */

/*
 * SPI FSM Controller data
 */
struct stm_spi_fsm {
	struct mtd_info	mtd;
	struct device	*dev;
	struct resource	*region;

	void __iomem	*base;
	struct mutex	lock;
	unsigned	partitioned;
	uint8_t		page_buf[FLASH_PAGESIZE]__attribute__((aligned(4)));

	uint32_t	read_mask;
	struct fsm_seq	*seq_read_data;
};

/*
 * SPI FLASH
 */

/* Commands */
#define FLASH_CMD_WREN		0x06
#define FLASH_CMD_WRDI		0x04
#define FLASH_CMD_RDID		0x9f
#define FLASH_CMD_RDSR		0x05
#define FLASH_CMD_WRSR		0x01
#define FLASH_CMD_READ		0x03
#define FLASH_CMD_READ_FAST	0x0b
#define FLASH_CMD_READ_DUAL	0x3b
#define FLASH_CMD_PAGEPROGRAM	0x02
#define FLASH_CMD_SE_4K		0x20
#define FLASH_CMD_SE_32K	0x52
#define FLASH_CMD_SE		0xd8
#define FLASH_CMD_CHIPERASE	0xc7

/* Status register */
#define FLASH_STATUS_BUSY	0x01
#define FLASH_STATUS_WEL	0x02
#define FLASH_STATUS_BP0	0x04
#define FLASH_STATUS_BP1	0x08
#define FLASH_STATUS_BP2	0x10
#define FLASH_STATUS_TB		0x20
#define FLASH_STATUS_SP		0x40
#define FLASH_STATUS_SRWP0	0x80

/* Device capabilities */
#define	SECT_4K			0x01	/* Supports FLASH_CMD_SE_4K	*/
#define	SECT_32K		0x02	/* Supports FLASH_CMD_SE_32K	*/
#define FAST_READ		0x04	/* Supports FLASH_CMD_FAST_READ	*/
#define DUAL_READ		0x08	/* Supports FLASH_CMD_DUAL_READ */
#define QUAD_READ		0x10	/* Supports FLASH_CMD_DUAL_READ */

/*
 * FSM template sequences
 * (Note, various fields are modified on-the-fly)
 */
struct fsm_seq {
	uint32_t data_size;
	uint32_t addr1;
	uint32_t addr2;
	uint32_t addr_cfg;
	uint32_t seq_opc[5];
	uint32_t mode;
	uint32_t dummy;
	uint32_t status;
	uint8_t  seq[16];
	uint32_t seq_cfg;
} __attribute__((__packed__, aligned(4)));
#define FSM_SEQ_SIZE			sizeof(struct fsm_seq)

static struct fsm_seq seq_dummy = {
	.data_size = TRANSFER_SIZE(0),
	.seq = {
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq seq_read_jedec = {
	.data_size = TRANSFER_SIZE(8),
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_RDID)),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq seq_read_status = {
	.data_size = TRANSFER_SIZE(4),
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_RDSR)),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq seq_read_data_default = {
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_READ)),
	.addr_cfg = ADR_CFG_PADS_1_ADD1 | ADR_CFG_CYCLES_ADD1(24),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_ADD1,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq seq_read_data_dual = {
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_READ_DUAL)),
	.addr_cfg = ADR_CFG_PADS_1_ADD1 | ADR_CFG_CYCLES_ADD1(24),
	.dummy = DUMMY_PADS_1 | DUMMY_CYCLES(8),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_ADD1,
		FSM_INST_DUMMY,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_2 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq seq_read_data_fast = {
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_READ_FAST)),
	.addr_cfg = ADR_CFG_PADS_1_ADD1 | ADR_CFG_CYCLES_ADD1(24),
	.dummy = DUMMY_PADS_1 | DUMMY_CYCLES(8),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_ADD1,
		FSM_INST_DUMMY,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq seq_write_data = {
	.addr_cfg = ADR_CFG_PADS_1_ADD1 | ADR_CFG_CYCLES_ADD1(24),
	.seq_opc = {
		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_WREN) | SEQ_OPC_CSDEASSERT),

		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_PAGEPROGRAM)),
	},
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_CMD2,
		FSM_INST_ADD1,
		FSM_INST_DATA_WRITE,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq seq_erase_sector = {
	.addr_cfg = (ADR_CFG_PADS_1_ADD1 |
		     ADR_CFG_CYCLES_ADD1(24) |
		     ADR_CFG_CSDEASSERT_ADD1),
	.seq_opc = {
		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_WREN) | SEQ_OPC_CSDEASSERT),

		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_SE)),
	},
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_CMD2,
		FSM_INST_ADD1,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq seq_erase_chip = {
	.seq_opc = {
		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_WREN) | SEQ_OPC_CSDEASSERT),

		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_CHIPERASE)),
	},
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_CMD2,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};


/*
 * FSM interface
 */
static inline int fsm_is_idle(struct stm_spi_fsm *fsm)
{
	return readl(fsm->base + SPI_FAST_SEQ_STA) & 0x10;
}

static inline uint32_t fsm_fifo_available(struct stm_spi_fsm *fsm)
{
	return (readl(fsm->base + SPI_FAST_SEQ_STA) >> 5) & 0x7f;
}

static inline void fsm_load_seq(struct stm_spi_fsm *fsm,
				const struct fsm_seq *const seq)
{
	BUG_ON(!fsm_is_idle(fsm));
	memcpy_toio(fsm->base + SPI_FAST_SEQ_TRANSFER_SIZE,
		    seq, FSM_SEQ_SIZE);
}

static int fsm_wait_seq(struct stm_spi_fsm *fsm)
{
	unsigned long timeo = jiffies + HZ;

	while (time_before(jiffies, timeo)) {
		if (fsm_is_idle(fsm))
			return 0;

		cond_resched();
	}

	dev_err(fsm->dev, "timeout on sequence completion\n");

	return 1;
}

static void fsm_clear_fifo(struct stm_spi_fsm *fsm)
{
	uint32_t avail;

	while ((avail = fsm_fifo_available(fsm)) > 0) {

		dev_dbg(fsm->dev, "clearing %d bytes from FIFO\n", avail*4);

		while (avail) {
			readl(fsm->base + SPI_FAST_SEQ_DATA_REG);
			avail--;
		}
	}
}

static int fsm_read_fifo(struct stm_spi_fsm *fsm,
			 uint32_t *buf, const uint32_t size)
{
	uint32_t avail;
	uint32_t remaining = size >> 2;
	uint32_t words;

	dev_dbg(fsm->dev, "reading %d bytes from FIFO\n", size);

	BUG_ON((((uint32_t)buf) & 0x3) || (size & 0x3));

	do {
		while (!(avail = fsm_fifo_available(fsm)))
			;
		words = min(avail, remaining);
		remaining -= words;

		readsl(fsm->base + SPI_FAST_SEQ_DATA_REG, buf, words);
		buf += words;

	} while (remaining);

	return size;
}

static int fsm_write_fifo(struct stm_spi_fsm *fsm,
			  const uint32_t *buf, const uint32_t size)
{
	uint32_t words = size >> 2;

	dev_dbg(fsm->dev, "writing %d bytes to FIFO\n", size);

	BUG_ON((((uint32_t)buf) & 0x3) || (size & 0x3));

	writesl(fsm->base + SPI_FAST_SEQ_DATA_REG, buf, words);

	return size;
}

/*
 * Serial Flash operations
 */

/* [AAC: The following contradicts validation GNBvd79303/GNBvd79597.  Awaiting
 * clarification from validation/design.]
 */
static int fsm_wait_busy(struct stm_spi_fsm *fsm)
{
	const struct fsm_seq *seq = &seq_read_status;
	unsigned long deadline;
	uint8_t status[4] = {0x00, 0x00, 0x00, 0x00};

	/* Load read_status sequence */
	fsm_load_seq(fsm, seq);

	/* Repeat until busy bit is deasserted, or timeout */
	deadline = jiffies + FLASH_MAX_BUSY_WAIT;
	do {
		fsm_read_fifo(fsm, (uint32_t *)status, 4);

		if ((status[3] & FLASH_STATUS_BUSY) == 0) {
			fsm_wait_seq(fsm);
			return 0;
		}

		cond_resched();

		/* Wait for sequence to end, then restart */
		fsm_wait_seq(fsm);

		writel(seq->seq_cfg, fsm->base + SPI_FAST_SEQ_CFG);
	} while (!time_after_eq(jiffies, deadline));

	fsm_wait_seq(fsm);

	dev_err(fsm->dev, "timeout on wait_busy\n");

	return 1;
}

static int fsm_read_jedec(struct stm_spi_fsm *fsm, uint8_t *const jedec)
{
	const struct fsm_seq *seq = &seq_read_jedec;
	uint32_t tmp[2];

	fsm_load_seq(fsm, seq);

	fsm_read_fifo(fsm, tmp, 8);

	memcpy(jedec, tmp, 5);

	return 0;
}

static int fsm_erase_sector(struct stm_spi_fsm *fsm, const uint32_t offset)
{
	struct fsm_seq *seq = &seq_erase_sector;

	dev_dbg(fsm->dev, "erasing sector at 0x%08x\n", offset);

	seq->addr1 = offset;

	fsm_load_seq(fsm, seq);

	fsm_wait_seq(fsm);

	fsm_wait_busy(fsm);

	return 0;
}

static int fsm_erase_chip(struct stm_spi_fsm *fsm)
{
	const struct fsm_seq *seq = &seq_erase_chip;

	dev_dbg(fsm->dev, "erasing chip\n");

	fsm_load_seq(fsm, seq);

	fsm_wait_seq(fsm);

	fsm_wait_busy(fsm);

	return 0;
}

static int fsm_read(struct stm_spi_fsm *fsm, uint8_t *const buf,
		    const uint32_t size, const uint32_t offset)
{
	struct fsm_seq *seq = fsm->seq_read_data;
	uint32_t read_mask = fsm->read_mask;
	uint8_t *page_buf = fsm->page_buf;
	uint32_t size_ub;
	uint32_t size_lb;
	uint32_t size_mop;
	uint32_t tmp[2];
	uint8_t *p;

	dev_dbg(fsm->dev, "reading %d bytes from 0x%08x\n", size, offset);

	/* Handle non-aligned buf */
	p = ((uint32_t)buf & 0x3) ? page_buf : buf;

	/* Handle non-aligned size */
	size_ub = (size + read_mask) & ~read_mask;
	size_lb = size & ~read_mask;
	size_mop = size & read_mask;

	seq->data_size = TRANSFER_SIZE(size_ub);
	seq->addr1 = offset;

	fsm_load_seq(fsm, seq);

	fsm_read_fifo(fsm, (uint32_t *)p, size_lb);

	if (size_mop) {
		fsm_read_fifo(fsm, tmp, read_mask + 1);
		memcpy(p + size_lb, &tmp, size_mop);
	}

	/* Handle non-aligned buf */
	if ((uint32_t)buf & 0x3)
		memcpy(buf, page_buf, size);

	/* Wait for sequence to finish */
	fsm_wait_seq(fsm);

	return 0;
}

static int fsm_write(struct stm_spi_fsm *fsm, const uint8_t *const buf,
		     const uint32_t size, const uint32_t offset)
{
	struct fsm_seq *seq = &seq_write_data;
	uint8_t *page_buf = fsm->page_buf;
	uint32_t size_ub;
	uint32_t size_lb;
	uint32_t size_mop;
	uint32_t tmp;
	uint8_t *t = (uint8_t *)&tmp;
	int i;
	const uint8_t *p;

	dev_dbg(fsm->dev, "writing %d bytes to 0x%08x\n", size, offset);

	/* Handle non-aligned buf */
	if ((uint32_t)buf & 0x3) {
		memcpy(page_buf, buf, size);
		p = page_buf;
	} else {
		p = buf;
	}

	/* Handle non-aligned size */
	size_ub = (size + 0x3) & ~0x3;
	size_lb = size & ~0x3;
	size_mop = size & 0x3;

	seq->data_size = TRANSFER_SIZE(size_ub);
	seq->addr1 = offset;

	if (cpu_data->type == CPU_STX7106) {
		/* Requires starting "dummy" sequence on stx7106 (seems vaguely
		 * related to GNBvd79464) [AAC: check with validation/design:
		 * Not down to erase sequence, setting FIFO-write bit only seems
		 * to work properly if dummy sequence has been run.]
		 */
		fsm_load_seq(fsm, &seq_dummy);
		readl(fsm->base + SPI_FAST_SEQ_CFG);
	}

	/* Need to set FIFO to write mode, before writing data to FIFO (see
	 * GNBvb79594)
	 */
	writel(0x00040000, fsm->base + SPI_FAST_SEQ_CFG);
	readl(fsm->base + SPI_FAST_SEQ_CFG);

	/* Write data to FIFO, before starting sequence (see GNBvd79593) */
	fsm_write_fifo(fsm, (uint32_t *)p, size_lb);

	p += size_lb;

	/* Handle non-aligned size */
	if (size_mop) {
		tmp = ~0ul;	/* fill with 0xff's */
		for (i = 0; i < size_mop; i++)
			t[i] = *p++;

		fsm_write_fifo(fsm, &tmp, 4);
	}

	/* Start sequence */
	fsm_load_seq(fsm, seq);

	/* Wait for sequence to finish */
	fsm_wait_seq(fsm);

	/* Wait for completion */
	fsm_wait_busy(fsm);

	return 0;
}

/*
 * FSM Configuration
 */
static int fsm_set_mode(struct stm_spi_fsm *fsm, uint32_t mode)
{
	/* Wait for controller to accept mode change */
	if (!cpu_data->type == CPU_STX7108) {
		/* Does not work correctly on stx7108 */
		while (!(readl(fsm->base + SPI_STA_MODE_CHANGE) & 0x1))
			;
	}
	writel(mode, fsm->base + SPI_MODESELECT);

	return 0;
}

static int fsm_set_freq(struct stm_spi_fsm *fsm, uint32_t freq)
{
	struct clk *emi_clk;
	uint32_t emi_freq;
	uint32_t clk_div;

	/* Timings set in terms of EMI clock... */
	emi_clk = clk_get(NULL, "emi_clk");

	if (IS_ERR(emi_clk)) {
		dev_warn(fsm->dev, "Failed to find EMI clock. "
			 "Using default 100MHz.\n");
		emi_freq = 100000000UL;
	} else {
		emi_freq = clk_get_rate(emi_clk);
	}

	/* Calculate clk_div: multiple of 2, round up, 2 -> 128. Note, clk_div =
	 * 4 is not supported on current SoCs, use 6 instead (RnDHV00020914) */
	clk_div = 2*((emi_freq + (2*freq - 1))/(2*freq));

	if (clk_div < 2)
		clk_div = 2;
	else if (clk_div == 4)
		clk_div = 6;
	else if (clk_div > 128)
		clk_div = 128;

	dev_dbg(fsm->dev, "emi_clk = %uHZ, spi_freq = %uHZ, clock_div = %u\n",
		emi_freq, freq, clk_div);

	/* Set SPI_CLOCKDIV */
	writel(clk_div, fsm->base + SPI_CLOCKDIV);

	return 0;
}

static int fsm_configure_reads(struct stm_spi_fsm *fsm, uint32_t capabilities)
{
	/* Disable DUAL mode on stx7106 (see GNBvd78843) */
	if (cpu_data->type == CPU_STX7106) {
		dev_info(fsm->dev, "disabling DUAL mode reads on stx7106\n");
		capabilities &= ~DUAL_READ;
	}

	if (capabilities & DUAL_READ) {
		/* Do DUAL mode read */
		fsm->seq_read_data = &seq_read_data_dual;
		fsm->read_mask = 0x7;	/* Dual Mode seems to require multiple
					 * of 8-byte transfers!  [ACC: check
					 * with validation/design: attempts with
					 * 4-byte transfers -> subsequent
					 * non-dual reads shift in 2 dummy
					 * bytes.] */
		dev_dbg(fsm->dev, "setting DUAL mode reads\n");
	} else if (capabilities & FAST_READ) {
		/* Do FAST read */
		fsm->seq_read_data = &seq_read_data_fast;
		fsm->read_mask = 0x3;
		dev_dbg(fsm->dev, "setting FAST reads\n");
	} else {
		/* Fall-back to normal read */
		fsm->seq_read_data = &seq_read_data_default;
		fsm->read_mask = 0x3;
	}

	return 0;
}

static int fsm_init(struct stm_spi_fsm *fsm)
{
	/* Perform a soft reset of the FSM controller */
	if (!cpu_data->type == CPU_STX7106) {
		/* This fails on stx7106 [AAC: check with validation.  Reset
		 * results in non-clearable data in the FIFO.]
		 */
		writel(SEQ_CFG_SWRESET, fsm->base + SPI_FAST_SEQ_CFG);
		udelay(1);
		writel(0, fsm->base + SPI_FAST_SEQ_CFG);
	}

	/* Set clock frequency to safe default */
	fsm_set_freq(fsm, FLASH_DEFAULT_FREQ);

	/* Switch to FSM */
	fsm_set_mode(fsm, SPI_MODESELECT_FSM);

	/* Set timing parameters (use reset values for now (awaiting information
	 * from Validation Team)
	 */
	writel(SPI_CFG_DEVICE_ST |
	       SPI_CFG_MIN_CS_HIGH(0x0AA) |
	       SPI_CFG_CS_SETUPHOLD(0xa0) |
	       SPI_CFG_DATA_HOLD(0x00), fsm->base + SPI_CONFIGDATA);

	/* Clear FIFO, just in case */
	fsm_clear_fifo(fsm);

	return 0;
}

static void fsm_exit(struct stm_spi_fsm *fsm)
{

}


/*
 * MTD interface
 */

/*
 * Read an address range from the flash chip.  The address range
 * may be any size provided it is within the physical boundaries.
 */
static int mtd_read(struct mtd_info *mtd, loff_t from, size_t len,
		    size_t *retlen, u_char *buf)
{
	struct stm_spi_fsm *fsm = mtd->priv;
	uint32_t bytes;

	dev_dbg(fsm->dev, "%s %s 0x%08x, len %zd\n",  __func__,
		"from", (u32)from, len);

	/* Byte count starts at zero. */
	if (retlen)
		*retlen = 0;

	if (!len)
		return 0;

	if (from + len > mtd->size)
		return -EINVAL;

	mutex_lock(&fsm->lock);

	while (len > 0) {
		bytes = min(len, (size_t)FLASH_PAGESIZE);

		fsm_read(fsm, buf, bytes, from);

		buf += bytes;
		from += bytes;
		len -= bytes;

		if (retlen)
			*retlen += bytes;
	}

	mutex_unlock(&fsm->lock);

	return 0;
}

/*
 * Write an address range to the flash chip.  Data must be written in
 * FLASH_PAGESIZE chunks.  The address range may be any size provided
 * it is within the physical boundaries.
 */
static int mtd_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	struct stm_spi_fsm *fsm = mtd->priv;

	u32 page_offs;
	u32 bytes;
	uint8_t *b = (uint8_t *)buf;

	dev_dbg(fsm->dev, "%s %s 0x%08x, len %zd\n", __func__,
		"to", (u32)to, len);

	if (retlen)
		*retlen = 0;

	if (!len)
		return 0;

	if (to + len > mtd->size)
		return -EINVAL;

	/* offset within page */
	page_offs = to % FLASH_PAGESIZE;

	mutex_lock(&fsm->lock);

	while (len) {

		/* write up to page boundary */
		bytes = min(FLASH_PAGESIZE - page_offs, len);

		fsm_write(fsm, b, bytes, to);

		b += bytes;
		len -= bytes;
		to += bytes;

		/* We are now page-aligned */
		page_offs = 0;

		if (retlen)
			*retlen += bytes;

	}

	mutex_unlock(&fsm->lock);

	return 0;
}

/*
 * Erase an address range on the flash chip.  The address range may extend
 * one or more erase sectors.  Return an error is there is a problem erasing.
 */
static int mtd_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct stm_spi_fsm *fsm = mtd->priv;
	u32 addr, len;

	dev_dbg(fsm->dev, "%s %s 0x%llx, len %lld\n", __func__,
		"at", (long long)instr->addr, (long long)instr->len);

	if (instr->addr + instr->len > mtd->size)
		return -EINVAL;

	if (instr->len & (mtd->erasesize - 1))
		return -EINVAL;

	addr = instr->addr;
	len = instr->len;

	mutex_lock(&fsm->lock);

	/* whole-chip erase? */
	if (len == mtd->size) {
		if (fsm_erase_chip(fsm)) {
			instr->state = MTD_ERASE_FAILED;
			mutex_unlock(&fsm->lock);
			return -EIO;
		}
	} else {
		while (len) {
			if (fsm_erase_sector(fsm, addr)) {
				instr->state = MTD_ERASE_FAILED;
				mutex_unlock(&fsm->lock);
				return -EIO;
			}

			addr += mtd->erasesize;
			len -= mtd->erasesize;
		}
	}

	mutex_unlock(&fsm->lock);

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}

/*
 * SPI FLASH devices
 */
struct flash_info {
	char		*name;

	/* JEDEC id zero means "no ID" (most older chips); otherwise it has
	 * a high byte of zero plus three data bytes: the manufacturer id,
	 * then a two byte device id.
	 */
	u32		jedec_id;
	u16             ext_id;

	/* The size listed here is what works with FLASH_CMD_SE, which isn't
	 * necessarily called a "sector" by the vendor.
	 */
	unsigned	sector_size;
	u16		n_sectors;

	/* FLASH device capabilities */
	u16		capabilities;

	/* Maximum operating frequency.  Note, where FAST_READ is supported,
	 * freq_max specifies the FAST_READ frequency, not the READ frequency.
	 */
	 u32		max_freq;
};

/* Device table adapted from drivers/mtd/devices/m25p80.c
 *
 * In order to configure features supported by SPI FSM controller, we have added
 * FAST_READ, DUAL_READ, and QUAD_READ as device capabilities, and added a field
 * specifying the device operating frequency.
 */
static struct flash_info __devinitdata flash_types[] = {

	/* ST Microelectronics/Numonyx --
	 * (newer production versions may have feature updates (eg faster
	 * operating frequency) */
	{ "m25p40",  0x202013, 0,  64 * 1024,   8, FAST_READ, 25000000},
	{ "m25p80",  0x202014, 0,  64 * 1024,  16, FAST_READ, 25000000},
	{ "m25p16",  0x202015, 0,  64 * 1024,  32, FAST_READ, 25000000},
	{ "m25p32",  0x202016, 0,  64 * 1024,  64, FAST_READ, 50000000},
	{ "m25p64",  0x202017, 0,  64 * 1024, 128, FAST_READ, 50000000},
	{ "m25p128", 0x202018, 0, 256 * 1024,  64, FAST_READ, 50000000},

#define M25PX_CAPS (SECT_4K | FAST_READ | DUAL_READ)
	{ "m25px32", 0x207116, 0,  64 * 1024,  64, M25PX_CAPS, 75000000},
	{ "m25px64", 0x207117, 0,  64 * 1024, 128, M25PX_CAPS, 75000000},

	/* Winbond -- w25x "blocks" are 64K, "sectors" are 4KiB */
#define W25X_CAPS (SECT_4K | FAST_READ | DUAL_READ)
	{ "w25x40",  0xef3013, 0,  64 * 1024,   8, W25X_CAPS, 75000000},
	{ "w25x80",  0xef3014, 0,  64 * 1024,  16, W25X_CAPS, 75000000},
	{ "w25x16",  0xef3015, 0,  64 * 1024,  32, W25X_CAPS, 75000000},
	{ "w25x32",  0xef3016, 0,  64 * 1024,  64, W25X_CAPS, 75000000},
	{ "w25x64",  0xef3017, 0,  64 * 1024, 128, W25X_CAPS, 75000000},

#define N25Q_CAPS (FAST_READ | DUAL_READ)
	{ "n25q128", 0x20ba18, 0, 64 * 1024,  256, N25Q_CAPS, 108000000},

	/* Winbond -- w25q "blocks" are 64K, "sectors" are 4KiB */
#define W25Q_CAPS (SECT_4K | SECT_32K | FAST_READ | DUAL_READ | QUAD_READ)
	{ "w25q80",  0xef4014, 0,  64 * 1024,  16, W25Q_CAPS, 80000000},
	{ "w25q16",  0xef4015, 0,  64 * 1024,  32, W25Q_CAPS, 80000000},
	{ "w25q32",  0xef4016, 0,  64 * 1024,  64, W25Q_CAPS, 80000000},
	{ "w25q64",  0xef4017, 0,  64 * 1024, 128, W25Q_CAPS, 80000000},
	{     NULL, 0x000000, 0,         0,   0,       0, },
};

static struct flash_info *__devinit fsm_jedec_probe(struct stm_spi_fsm *fsm)
{
	u8			id[5];
	u32			jedec;
	u16                     ext_jedec;
	struct flash_info	*info;

	/* JEDEC also defines an optional "extended device information"
	 * string for after vendor-specific data, after the three bytes
	 * we use here.  Supporting some chips might require using it.
	 */

	if (fsm_read_jedec(fsm, id) != 0) {
		dev_info(fsm->dev, "error reading JEDEC ID\n");
		return NULL;
	}

	jedec = id[0];
	jedec = jedec << 8;
	jedec |= id[1];
	jedec = jedec << 8;
	jedec |= id[2];

	dev_dbg(fsm->dev, "JEDEC =  0x%08x [%02x %02x %02x %02x %02x]\n",
		jedec, id[0], id[1], id[2], id[3], id[4]);

	ext_jedec = id[3] << 8 | id[4];
	for (info = flash_types; info->name; info++) {
		if (info->jedec_id == jedec) {
			if (info->ext_id != 0 && info->ext_id != ext_jedec)
				continue;
			return info;
		}
	}

	dev_err(fsm->dev, "unrecognized JEDEC id %06x\n", jedec);

	return NULL;
}

/*
 * STM SPI FSM driver setup
 */
static int __init stm_spi_fsm_probe(struct platform_device *pdev)
{
	struct stm_plat_spifsm_data *data = pdev->dev.platform_data;
	struct stm_spi_fsm *fsm;
	struct resource *resource;
	int ret = 0;
	struct flash_info *info;
	unsigned i;
	uint32_t freq;

	/* Allocate memory for the driver structure (and zero it) */
	fsm = kzalloc(sizeof(struct stm_spi_fsm), GFP_KERNEL);
	if (!fsm) {
		dev_err(&pdev->dev, "failed to allocate fsm controller data\n");
		return -ENOMEM;
	}

	fsm->dev = &pdev->dev;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		dev_err(&pdev->dev, "failed to find IORESOURCE_MEM\n");
		ret = -ENODEV;
		goto out1;
	}

	fsm->region = request_mem_region(resource->start,
					 resource_size(resource), pdev->name);
	if (!fsm->region) {
		dev_err(&pdev->dev, "failed to reserve memory region "
			"[0x%08x-0x%08x]\n", resource->start, resource->end);
		ret = -EBUSY;
		goto out1;
	}

	fsm->base = ioremap_nocache(resource->start, resource_size(resource));

	if (!fsm->base) {
		dev_err(&pdev->dev, "failed to ioremap [0x%08x]\n",
			resource->start);
		ret = -EINVAL;
		goto out2;
	}

	mutex_init(&fsm->lock);

	/* Initialise FSM */

	if (fsm_init(fsm) != 0) {
		dev_err(&pdev->dev, "failed to initialise SPI FSM "
			"Controller\n");
		ret = -EINVAL;
		goto out3;
	}

	/* Detect SPI FLASH device */
	info = fsm_jedec_probe(fsm);
	if (!info) {
		ret = -ENODEV;
		goto out4;
	}

	platform_set_drvdata(pdev, fsm);

	/* Set operating frequency, from table or overridden by platform data */
	if (data->max_freq)
		freq = data->max_freq;
	else if (info->max_freq)
		freq = info->max_freq;
	else
		freq = FLASH_DEFAULT_FREQ;
	fsm_set_freq(fsm, freq);

	/* Configure read operations based on device capabilities */
	fsm_configure_reads(fsm, info->capabilities);

	/* Set up MTD parameters */
	fsm->mtd.priv = fsm;
	if (data && data->name)
		fsm->mtd.name = data->name;
	else
		fsm->mtd.name = NAME;

	fsm->mtd.type = MTD_NORFLASH;
	fsm->mtd.writesize = 4;
	fsm->mtd.flags = MTD_CAP_NORFLASH;
	fsm->mtd.size = info->sector_size * info->n_sectors;
	fsm->mtd.erasesize = info->sector_size;

	fsm->mtd.read = mtd_read;
	fsm->mtd.write = mtd_write;
	fsm->mtd.erase = mtd_erase;

	dev_info(&pdev->dev, "found device: %s, size = %llx (%lldMiB) "
		 "erasesize = 0x%08x (%uKiB)\n",
		 info->name,
		 (long long)fsm->mtd.size, (long long)(fsm->mtd.size >> 20),
		 fsm->mtd.erasesize, (fsm->mtd.erasesize >> 10));

	/* Add partitions */
	if (mtd_has_partitions()) {
		struct mtd_partition	*parts = NULL;
		int			nr_parts = 0;

		if (mtd_has_cmdlinepart()) {
			static const char *part_probes[]
					= { "cmdlinepart", NULL, };

			nr_parts = parse_mtd_partitions(&fsm->mtd,
							part_probes, &parts, 0);
		}

		if (nr_parts <= 0 && data && data->parts) {
			parts = data->parts;
			nr_parts = data->nr_parts;
		}

		if (nr_parts > 0) {
			for (i = 0; i < nr_parts; i++) {
				dev_dbg(fsm->dev, "partitions[%d] = "
					"{.name = %s, .offset = 0x%llx, "
					".size = 0x%llx (%lldKiB) }\n",
					i, parts[i].name,
					(long long)parts[i].offset,
					(long long)parts[i].size,
					(long long)(parts[i].size >> 10));
			}
			fsm->partitioned = 1;

			if (add_mtd_partitions(&fsm->mtd, parts, nr_parts)) {
				ret = -ENODEV;
				goto out4;
			}

			/* Success :-) */
			return 0;
		}

	} else if (data->nr_parts) {
		dev_info(&pdev->dev, " ignoring %d default partitions on %s\n",
			 data->nr_parts, data->name);
	}

	if (add_mtd_device(&fsm->mtd)) {
		ret = -ENODEV;
		goto out4;
	}

	/* Success :-) */
	return 0;

 out4:
	fsm_exit(fsm);
	platform_set_drvdata(pdev, NULL);
 out3:
	iounmap(fsm->base);
 out2:
	release_resource(fsm->region);
 out1:
	kfree(fsm);

	return ret;
}

static int __devexit stm_spi_fsm_remove(struct platform_device *pdev)
{
	struct stm_spi_fsm *fsm = platform_get_drvdata(pdev);

	if (mtd_has_partitions() && fsm->partitioned)
		del_mtd_partitions(&fsm->mtd);
	else
		del_mtd_device(&fsm->mtd);

	fsm_exit(fsm);
	iounmap(fsm->base);
	release_resource(fsm->region);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver stm_spi_fsm_driver = {
	.probe		= stm_spi_fsm_probe,
	.remove		= stm_spi_fsm_remove,
	.driver		= {
		.name	= NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init stm_spi_fsm_init(void)
{
	return platform_driver_register(&stm_spi_fsm_driver);
}

static void __exit stm_spi_fsm_exit(void)
{
	platform_driver_unregister(&stm_spi_fsm_driver);
}

module_init(stm_spi_fsm_init);
module_exit(stm_spi_fsm_exit);

MODULE_AUTHOR("Angus Clark <Angus.Clark@st.com>");
MODULE_DESCRIPTION("STM SPI FSM driver");
MODULE_LICENSE("GPL");
