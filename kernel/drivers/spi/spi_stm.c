/*
 *  ------------------------------------------------------------------------
 *  spi_stm.c SPI/SSC driver for STMicroelectronics platforms
 *  ------------------------------------------------------------------------
 *
 *  Copyright (c) 2008 STMicroelectronics Limited
 *  Author: Angus Clark <Angus.Clark@st.com>
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *
 *  ------------------------------------------------------------------------
 *  Changelog:
 *  2008-01-24 (angus.clark@st.com)
 *    - Initial version
 *  2008-08-28 (angus.clark@st.com)
 *    - Updates to fit with changes to 'ssc_pio_t'
 *    - SSC accesses now all 32-bit, for compatibility with 7141 Comms block
 *    - Updated to handle 7141 PIO ALT configuration
 *    - Support for user-defined, per-bus, chip_select function.  Specified
 *      in board setup
 *    - Bug fix for rx_bytes_pending updates
 *
 *  ------------------------------------------------------------------------
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/uaccess.h>
#include <linux/param.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/stm/platform.h>
#include <linux/stm/ssc.h>

#define NAME "spi-stm"

struct spi_stm {
	/* SSC SPI Controller */
	struct spi_bitbang	bitbang;
	void __iomem		*base;
	struct clk		*clk;
	struct platform_device  *pdev;

	/* Resources */
	struct resource r_mem;
	struct resource r_irq;
	struct stm_pad_state *pad_state;

	/* SSC SPI current transaction */
	const u8		*tx_ptr;
	u8			*rx_ptr;
	u16			bits_per_word;
	u16			bytes_per_word;
	unsigned int		baud;
	unsigned int		words_remaining;
	struct completion	done;
};

static void spi_stm_gpio_chipselect(struct spi_device *spi, int value)
{
	unsigned int out;

	if (spi->chip_select == (typeof(spi->chip_select))(STM_GPIO_INVALID))
		return;

	if (!spi->controller_data) {
		if (gpio_request(spi->chip_select, "spi_stm cs")) {
			dev_err(&spi->dev, "failed to allocate CS pin\n");
			return;
		}
		spi->controller_data = (void *)1;
		gpio_direction_output(spi->chip_select,
				      spi->mode & SPI_CS_HIGH);
	}

	if (value == BITBANG_CS_ACTIVE)
		out = spi->mode & SPI_CS_HIGH ? 1 : 0;
	else
		out = spi->mode & SPI_CS_HIGH ? 0 : 1;

	gpio_set_value(spi->chip_select, out);

	dev_dbg(&spi->dev, "%s PIO%d[%d] -> %d \n",
		value == BITBANG_CS_ACTIVE ? "select" : "deselect",
		stm_gpio_port(spi->chip_select),
		stm_gpio_pin(spi->chip_select), out);

	return;
}

static int spi_stm_setup_transfer(struct spi_device *spi,
				     struct spi_transfer *t)
{
	struct spi_stm *spi_stm;
	u32 hz;
	u8 bits_per_word;
	u32 reg;
	u32 sscbrg;

	spi_stm = spi_master_get_devdata(spi->master);
	bits_per_word = (t) ? t->bits_per_word : 0;
	hz = (t) ? t->speed_hz : 0;

	/* If not specified, use defaults */
	if (!bits_per_word)
		bits_per_word = spi->bits_per_word;
	if (!hz)
		hz = spi->max_speed_hz;

	/* Actually, can probably support 2-16 without any other change!!! */
	if (bits_per_word != 8 && bits_per_word != 16) {
		dev_err(&spi->dev, "unsupported bits_per_word=%d\n",
			bits_per_word);
		return -EINVAL;
	}
	spi_stm->bits_per_word = bits_per_word;

	/* Set SSC_BRF */
	sscbrg = clk_get_rate(spi_stm->clk) / (2*hz);
	if (sscbrg < 0x07 || sscbrg > (0x1 << 16)) {
		dev_err(&spi->dev, "baudrate outside valid range"
			" %d (sscbrg = %d)\n", hz, sscbrg);
		return -EINVAL;
	}
	spi_stm->baud = clk_get_rate(spi_stm->clk) / (2 * sscbrg);
	if (sscbrg == (0x1 << 16)) /* 16-bit counter wraps */
		sscbrg = 0x0;
	ssc_store32(spi_stm, SSC_BRG, sscbrg);

	dev_dbg(&spi->dev, "setting baudrate: target = %u hz, "
		"actual = %u hz, sscbrg = %u\n",
		hz, spi_stm->baud, sscbrg);

	 /* Set SSC_CTL and enable SSC */
	 reg = ssc_load32(spi_stm, SSC_CTL);
	 reg |= SSC_CTL_MS;

	 if (spi->mode & SPI_CPOL)
		 reg |= SSC_CTL_PO;
	 else
		 reg &= ~SSC_CTL_PO;

	 if (spi->mode & SPI_CPHA)
		 reg |= SSC_CTL_PH;
	 else
		 reg &= ~SSC_CTL_PH;

	 if ((spi->mode & SPI_LSB_FIRST) == 0)
		 reg |= SSC_CTL_HB;
	 else
		 reg &= ~SSC_CTL_HB;

	 if (spi->mode & SPI_LOOP)
		 reg |= SSC_CTL_LPB;
	 else
		 reg &= ~SSC_CTL_LPB;

	 reg &= 0xfffffff0;
	 reg |= (bits_per_word - 1);

	 reg |= SSC_CTL_EN_TX_FIFO | SSC_CTL_EN_RX_FIFO;
	 reg |= SSC_CTL_EN;

	 dev_dbg(&spi->dev, "ssc_ctl = 0x%04x\n", reg);
	 ssc_store32(spi_stm, SSC_CTL, reg);

	 /* Clear the status register */
	 ssc_load32(spi_stm, SSC_RBUF);

	 return 0;
}

static void spi_stm_cleanup(struct spi_device *spi)
{
	if (spi->controller_data) {
		gpio_free(spi->chip_select);
		spi->controller_data = (void *)0;
	}
}

/* the spi->mode bits understood by this driver: */
#define MODEBITS  (SPI_CPOL | SPI_CPHA | SPI_LSB_FIRST | SPI_LOOP | SPI_CS_HIGH)
static int spi_stm_setup(struct spi_device *spi)
{
	struct spi_stm *spi_stm;

	spi_stm = spi_master_get_devdata(spi->master);

	if (spi->mode & ~MODEBITS) {
		dev_err(&spi->dev, "unsupported mode bits %x\n",
			spi->mode & ~MODEBITS);
		return -EINVAL;
	}

	if (!spi->max_speed_hz)  {
		dev_err(&spi->dev, "max_speed_hz unspecified\n");
		return -EINVAL;
	}

	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	return spi_stm_setup_transfer(spi, NULL);
}

/* Load the TX FIFO */
static void ssc_write_tx_fifo(struct spi_stm *spi_stm)
{

	uint32_t count;
	uint32_t word = 0;
	int i;

	if (spi_stm->words_remaining > 8)
		count = 8;
	else
		count = spi_stm->words_remaining;

	for (i = 0; i < count; i++) {
		if (spi_stm->tx_ptr) {
			if (spi_stm->bytes_per_word == 1) {
				word = *spi_stm->tx_ptr++;
			} else {
				word = *spi_stm->tx_ptr++;
				word = *spi_stm->tx_ptr++ | (word << 8);
			}
		}
		ssc_store32(spi_stm, SSC_TBUF, word);
	}
}

/* Read the RX FIFO */
static void ssc_read_rx_fifo(struct spi_stm *spi_stm)
{

	uint32_t count;
	uint32_t word = 0;
	int i;

	if (spi_stm->words_remaining > 8)
		count = 8;
	else
		count = spi_stm->words_remaining;

	for (i = 0; i < count; i++) {
		word = ssc_load32(spi_stm, SSC_RBUF);
		if (spi_stm->rx_ptr) {
			if (spi_stm->bytes_per_word == 1) {
				*spi_stm->rx_ptr++ = (uint8_t)word;
			} else {
				*spi_stm->rx_ptr++ = (word >> 8);
				*spi_stm->rx_ptr++ = word & 0xff;
			}
		}
	}

	spi_stm->words_remaining -= count;
}

/* Interrupt fired when TX shift register becomes empty */
static irqreturn_t spi_stm_irq(int irq, void *dev_id)
{
	struct spi_stm *spi_stm = (struct spi_stm *)dev_id;

	/* Read RX FIFO */
	ssc_read_rx_fifo(spi_stm);

	/* Fill TX FIFO */
	if (spi_stm->words_remaining) {
		ssc_write_tx_fifo(spi_stm);
	} else {
		/* TX/RX complete */
		ssc_store32(spi_stm, SSC_IEN, 0x0);
		complete(&spi_stm->done);
	}

	return IRQ_HANDLED;
}


static int spi_stm_txrx_bufs(struct spi_device *spi, struct spi_transfer *t)
{
	struct spi_stm *spi_stm;
	uint32_t ctl = 0;

	spi_stm = spi_master_get_devdata(spi->master);

	/* Setup transfer */
	spi_stm->tx_ptr = t->tx_buf;
	spi_stm->rx_ptr = t->rx_buf;

	if (spi_stm->bits_per_word > 8) {
		/* Anything greater than 8 bits-per-word requires 2
		 * bytes-per-word in the RX/TX buffers */
		spi_stm->bytes_per_word = 2;
		spi_stm->words_remaining = t->len/2;
	} else if (spi_stm->bits_per_word == 8 &&
		   ((t->len & 0x1) == 0)) {
		/* If transfer is even-length, and 8 bits-per-word, then
		 * implement as half-length 16 bits-per-word transfer */
		spi_stm->bytes_per_word = 2;
		spi_stm->words_remaining = t->len/2;

		/* Set SSC_CTL to 16 bits-per-word */
		ctl = ssc_load32(spi_stm, SSC_CTL);
		ssc_store32(spi_stm, SSC_CTL, (ctl | 0xf));

		ssc_load32(spi_stm, SSC_RBUF);

	} else {
		spi_stm->bytes_per_word = 1;
		spi_stm->words_remaining = t->len;
	}

	INIT_COMPLETION(spi_stm->done);

	/* Start transfer by writing to the TX FIFO */
	ssc_write_tx_fifo(spi_stm);
	ssc_store32(spi_stm, SSC_IEN, SSC_IEN_TEEN);

	/* Wait for transfer to complete */
	wait_for_completion(&spi_stm->done);

	/* Restore SSC_CTL if necessary */
	if (ctl)
		ssc_store32(spi_stm, SSC_CTL, ctl);

	return t->len;

}

static int __init spi_stm_probe(struct platform_device *pdev)
{
	struct stm_plat_ssc_data *plat_data = pdev->dev.platform_data;
	struct spi_master *master;
	struct resource *res;
	struct spi_stm *spi_stm;
	u32 reg;
	int status = 0;

	master = spi_alloc_master(&pdev->dev, sizeof(struct spi_stm));
	if (!master) {
		dev_err(&pdev->dev, "failed to allocate spi master\n");
		status = -ENOMEM;
		goto err0;
	}

	platform_set_drvdata(pdev, master);

	spi_stm = spi_master_get_devdata(master);
	spi_stm->bitbang.master = spi_master_get(master);
	spi_stm->bitbang.setup_transfer = spi_stm_setup_transfer;
	spi_stm->bitbang.txrx_bufs = spi_stm_txrx_bufs;
	spi_stm->bitbang.master->setup = spi_stm_setup;
	spi_stm->bitbang.master->cleanup = spi_stm_cleanup;

	if (plat_data->spi_chipselect)
		spi_stm->bitbang.chipselect = plat_data->spi_chipselect;
	else
		spi_stm->bitbang.chipselect = spi_stm_gpio_chipselect;

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = MODEBITS;

	/* chip_select field of spi_device is declared as u8 and therefore
	 * limits number of GPIOs that can be used as a CS line. Sorry. */
	master->num_chipselect =
		sizeof(((struct spi_device *)0)->chip_select) * 256;
	master->bus_num = pdev->id;
	init_completion(&spi_stm->done);

	/* Get resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to find IOMEM resource\n");
		status = -ENOENT;
		goto err1;
	}
	spi_stm->r_mem = *res;

	if (!request_mem_region(res->start,
				res->end - res->start + 1, NAME)) {
		dev_err(&pdev->dev, "request memory region failed [0x%x]\n",
			res->start);
		status = -EBUSY;
		goto err1;
	}

	spi_stm->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!spi_stm->base) {
		dev_err(&pdev->dev, "ioremap memory failed [0x%x]\n",
			res->start);
		status = -ENXIO;
		goto err2;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to find IRQ resource\n");
		status = -ENOENT;
		goto err3;
	}
	spi_stm->r_irq = *res;

	if (request_irq(res->start, spi_stm_irq,
			IRQF_DISABLED, dev_name(&pdev->dev), spi_stm)) {
		dev_err(&pdev->dev, "irq request failed\n");
		status = -EBUSY;
		goto err3;
	}

	spi_stm->pad_state = stm_pad_claim(plat_data->pad_config,
					   dev_name(&pdev->dev));
	if (!spi_stm->pad_state) {
		dev_err(&pdev->dev, "pads request failed\n");
		status = -EBUSY;
		goto err4;
	}

	/* Disable I2C and Reset SSC */
	ssc_store32(spi_stm, SSC_I2C, 0x0);
	reg = ssc_load16(spi_stm, SSC_CTL);
	reg |= SSC_CTL_SR;
	ssc_store32(spi_stm, SSC_CTL, reg);

	udelay(1);
	reg = ssc_load32(spi_stm, SSC_CTL);
	reg &= ~SSC_CTL_SR;
	ssc_store32(spi_stm, SSC_CTL, reg);

	/* Set SSC into slave mode before reconfiguring PIO pins */
	reg = ssc_load32(spi_stm, SSC_CTL);
	reg &= ~SSC_CTL_MS;
	ssc_store32(spi_stm, SSC_CTL, reg);

	spi_stm->clk = clk_get(&pdev->dev, "comms_clk");
	if (!spi_stm->clk) {
		dev_err(&pdev->dev, "Comms clock not found!\n");
		goto err5;
	}

	clk_enable(spi_stm->clk);
	/* Start "bitbang" worker */
	status = spi_bitbang_start(&spi_stm->bitbang);
	if (status) {
		dev_err(&pdev->dev, "bitbang start failed [%d]\n", status);
		goto err5;
	}

	dev_info(&pdev->dev, "registered SPI Bus %d\n", master->bus_num);

	return status;

 err5:
	stm_pad_release(spi_stm->pad_state);
 err4:
	free_irq(spi_stm->r_irq.start, spi_stm);
 err3:
	iounmap(spi_stm->base);
 err2:
	release_mem_region(spi_stm->r_mem.start,
			   resource_size(&spi_stm->r_mem));
 err1:
	spi_master_put(spi_stm->bitbang.master);
	platform_set_drvdata(pdev, NULL);
 err0:
	return status;
}

static int spi_stm_remove(struct platform_device *pdev)
{
	struct spi_stm *spi_stm;
	struct spi_master *master;

	master = platform_get_drvdata(pdev);
	spi_stm = spi_master_get_devdata(master);

	spi_bitbang_stop(&spi_stm->bitbang);

	clk_disable(spi_stm->clk);

	stm_pad_release(spi_stm->pad_state);
	free_irq(spi_stm->r_irq.start, spi_stm);
	iounmap(spi_stm->base);
	release_mem_region(spi_stm->r_mem.start,
			   resource_size(&spi_stm->r_mem));

	spi_master_put(spi_stm->bitbang.master);
	platform_set_drvdata(pdev, NULL);

	return 0;
}
#ifdef CONFIG_PM
static int spi_stm_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct spi_stm *spi_stm;

	spi_stm = spi_master_get_devdata(master);

	ssc_store32(spi_stm, SSC_IEN, 0);

	clk_disable(spi_stm->clk);
	return 0;
}

static int spi_stm_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct spi_stm *spi_stm;

	spi_stm = spi_master_get_devdata(master);

	clk_enable(spi_stm->clk);
	return 0;
}

static struct dev_pm_ops spi_stm_pm = {
	.suspend = spi_stm_suspend,
	.resume = spi_stm_resume,
	.freeze = spi_stm_suspend,
	.restore = spi_stm_resume,
	.runtime_suspend = spi_stm_suspend,
	.runtime_resume = spi_stm_resume,
};
#else
static struct dev_pm_ops spi_stm_pm;
#endif

static struct platform_driver spi_stm_driver = {
	.driver.name = NAME,
	.driver.owner = THIS_MODULE,
	.driver.pm = &spi_stm_pm,
	.probe = spi_stm_probe,
	.remove = spi_stm_remove,
};


static int __init spi_stm_init(void)
{
	return platform_driver_register(&spi_stm_driver);
}

static void __exit spi_stm_exit(void)
{
	platform_driver_unregister(&spi_stm_driver);
}

module_init(spi_stm_init);
module_exit(spi_stm_exit);

MODULE_AUTHOR("STMicroelectronics <www.st.com>");
MODULE_DESCRIPTION("STM SSC SPI driver");
MODULE_LICENSE("GPL");
