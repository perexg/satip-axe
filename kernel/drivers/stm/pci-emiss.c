/*
 * drivers/stm/pci-emiss.c
 *
 * PCI support for the STMicroelectronics EMISS PCI cell
 *
 * Copyright 2009 ST Microelectronics (R&D) Ltd.
 * Author: David J. McKay (david.mckay@st.com)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the top level directory for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/stm/platform.h>
#include <linux/stm/pci-glue.h>
#include <linux/stm/emi.h>
#include <linux/gpio.h>
#include <linux/cache.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <asm/clock.h>
#include "pci-emiss.h"


/* Due to the brain dead hardware we have to take the lock when we do inb(),
 * or config operations */
static DEFINE_SPINLOCK(stm_pci_io_lock);

/* I doubt we will ever have more than one EMI/PCI controller, so we may as
 * well make the register pointers global */
static void __iomem *emiss; /* pointer to emiss register area */
static void __iomem *ahb_pci; /* Ditto for AHB registers */

static unsigned pci_reset_pin = -EINVAL;	/* Global for PCI reset */

/* Static lookup table to precompute byte enables for various
 * transaction size. Stolen from Doug, as it is clearer than
 * computing it:-) */
struct byte_enables {
	unsigned char enables;
	signed   char shift;
};

static struct byte_enables be_table[3][4] = {
	{
		{ 0x0e,  0 }, /* BYTE read offset 0: legal */
		{ 0x0d,  8 }, /* BYTE read offset 1: legal */
		{ 0x0b, 16 }, /* BYTE read offset 2: legal */
		{ 0x07, 24 }  /* BYTE read offset 3: legal */
	}, {
		{ 0x0c,  0 }, /* WORD read offset 0: legal   */
		{ 0xff, -1 }, /* WORD read offset 1: illegal */
		{ 0x03, 16 }, /* WORD read offset 2: legal   */
		{ 0xff, -1 }  /* WORD read offset 3: illegal */
	}, {
		{ 0x00,  0 }, /* DWORD read offset 0: legal   */
		{ 0xff, -1 }, /* DWORD read offset 1: illegal */
		{ 0xff, -1 }, /* DWORD read offset 2: illegal */
		{ 0xff, -1 }  /* DWORD read offset 3: illegal */
	}
};

#define BYTE_ENABLE(addr, size) be_table[(size) / 2][(addr) & 3].enables
#define BYTE_ENABLE_SHIFT(addr, size) be_table[(size) / 2][(addr) & 3].shift
#define SIZE_MASK(size) ((size == 4) ? ~0 : (1 << (size << 3)) - 1)

#define pci_crp_readb(fn, addr) pci_crp_read(fn, addr, 1)
#define pci_crp_readw(fn, addr) pci_crp_read(fn, addr, 2)
#define pci_crp_readl(fn, addr) pci_crp_read(fn, addr, 4)

/* size is either 1, 2, or 4 */
static u32 pci_crp_read(unsigned fn, int addr, int size)
{
	unsigned char be = BYTE_ENABLE(addr, size);
	int shift = BYTE_ENABLE_SHIFT(addr, size);
	u32 ret;

	/* The spin lock is not needed as this are basically used only
	 * in the init function. But that may change */
	spin_lock(&stm_pci_io_lock);

	writel(PCI_CRP(addr, fn, PCI_CONFIG_READ, be), ahb_pci + PCI_CRP_ADDR);
	ret = readl(ahb_pci + PCI_CRP_RD_DATA);

	spin_unlock(&stm_pci_io_lock);

	return (ret >> shift) & SIZE_MASK(size);
}


#define pci_crp_writeb(fn, addr, val) pci_crp_write(fn, addr, 1, val)
#define pci_crp_writew(fn, addr, val) pci_crp_write(fn, addr, 2, val)
#define pci_crp_writel(fn, addr, val) pci_crp_write(fn, addr, 4, val)

static void pci_crp_write(unsigned fn, int addr, int size, u32 val)
{
	unsigned char be = BYTE_ENABLE(addr, size);
	int shift = BYTE_ENABLE_SHIFT(addr, size);

	spin_lock(&stm_pci_io_lock);

	writel(PCI_CRP(addr, fn, PCI_CONFIG_WRITE, be), ahb_pci + PCI_CRP_ADDR);
	writel(val << shift , ahb_pci + PCI_CRP_WR_DATA);

	spin_unlock(&stm_pci_io_lock);
}

/* Generate a read cycle with the specified command type
 * and byte enables */
static inline u32 __pci_csr_read(u32 addr, u32 cmd)
{
	writel(addr, ahb_pci + PCI_CSR_ADDR);
	writel(cmd, ahb_pci + PCI_CSR_BE_CMD);

	return readl(ahb_pci + PCI_CSR_RD_DATA);
}

/* As above but take the lock. Has to be irqsave as can
 * be used in interrupt context */
static inline u32 pci_csr_read(u32 addr, u32 cmd)
{
	unsigned long flags;
	u32 val, err;

	spin_lock_irqsave(&stm_pci_io_lock, flags);

	val = __pci_csr_read(addr, cmd);

	/* When transaction fails (eg. no device case) the controller
	 * sets the error condition... Let's clear it here, before
	 * everything panics... */
	err = readl(ahb_pci + PCI_CSR_PCI_ERROR);
	if (err) {
		pr_debug("stm_pci: CSR reported PCI error 0x%x - "
				"cleared now...\n", err);
		writel(err, ahb_pci + PCI_CSR_PCI_ERROR);
		/* Read back... */
		err = readl(ahb_pci + PCI_CSR_PCI_ERROR);
		/* Should be zero now... */
		BUG_ON(err);
		/* The "read" value is 0xf..f */
		val = ~0;
	}

	spin_unlock_irqrestore(&stm_pci_io_lock, flags);

	return val;
}

/* Generate a write cycle with the specified command type
 * and byte enables */
static inline void __pci_csr_write(u32 addr, u32 cmd, u32 val)
{
	writel(addr, ahb_pci + PCI_CSR_ADDR);
	writel(cmd, ahb_pci + PCI_CSR_BE_CMD);
	writel(val, ahb_pci + PCI_CSR_WR_DATA);
}

static inline void pci_csr_write(u32 addr, u32 cmd, u32 val)
{
	unsigned long flags;

	spin_lock_irqsave(&stm_pci_io_lock, flags);

	__pci_csr_write(addr, cmd, val);

	spin_unlock_irqrestore(&stm_pci_io_lock, flags);
}

/* This is a bit ugly, but it means that pci slots will always start with 0.
 * Is there a way to get back from the bus to the device? */
static int idsel_lo, max_slot;

#define TYPE0_CONFIG_CYCLE(fn, where) \
	(((fn) << 8) | ((where) & ~3))
#define TYPE1_CONFIG_CYCLE(bus, devfn, where) \
	(((bus) << 16) | ((devfn) << 8) | ((where) & ~3) | 1)

static int pci_stm_config_read(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 *val)
{
	int slot = PCI_SLOT(devfn);
	int fn = PCI_FUNC(devfn);
	u32 addr, cmd, raw;

	if (pci_is_root_bus(bus)) {
		if (slot > max_slot) {
			*val = SIZE_MASK(size);
			return PCIBIOS_DEVICE_NOT_FOUND;
		}
		addr = TYPE0_CONFIG_CYCLE(fn, where) | (1<<(idsel_lo + slot));
	} else {
		addr = TYPE1_CONFIG_CYCLE(bus->number, devfn, where);
	}

	cmd = PCI_CSR_BE_CMD_VAL(PCI_CONFIG_READ, BYTE_ENABLE(where, size));

	/* I'm assuming we can use config read/write in interrupt context,
	 * safer */
	raw = pci_csr_read(addr, cmd);

	*val = (raw >> BYTE_ENABLE_SHIFT(where, size)) & SIZE_MASK(size);

	return PCIBIOS_SUCCESSFUL;
}

static int pci_stm_config_write(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 val)
{
	int slot = PCI_SLOT(devfn);
	int fn = PCI_FUNC(devfn);
	u32 addr, cmd;

	if (pci_is_root_bus(bus)) {
		if (slot > max_slot)
			return PCIBIOS_DEVICE_NOT_FOUND;
		addr = TYPE0_CONFIG_CYCLE(fn, where) |  (1<<(idsel_lo + slot));
	} else {
		addr = TYPE1_CONFIG_CYCLE(bus->number, devfn, where);
	}

	cmd = PCI_CSR_BE_CMD_VAL(PCI_CONFIG_WRITE, BYTE_ENABLE(where, size));
	val <<= BYTE_ENABLE_SHIFT(where, size);

	pci_csr_write(addr, cmd, val);

	return PCIBIOS_SUCCESSFUL;
}

static inline u32 pci_emiss_in(unsigned long port, int size)
{
	unsigned be = BYTE_ENABLE(port, size);
	u32 raw;

	/* ad[1:0] must be passed through for IO port read/write */
	raw = pci_csr_read(port, PCI_CSR_BE_CMD_VAL(PCI_IO_READ, be));

	return (raw >> BYTE_ENABLE_SHIFT(port, size)) & SIZE_MASK(size);
}

static inline void pci_emiss_out(u32 val, unsigned long port, int size)
{
	unsigned be = BYTE_ENABLE(port, size);
	u32 cmd;

	cmd = PCI_CSR_BE_CMD_VAL(PCI_IO_WRITE, be);
	val <<=  BYTE_ENABLE_SHIFT(port, size);

	pci_csr_write(port, cmd, val);
}

/* As above, but does NOT take the lock. Beware */
static inline u32 __pci_emiss_in(unsigned long port, int size)
{
	unsigned be = BYTE_ENABLE(port, size);
	u32 raw;

	/* ad[1:0] must be passed through for IO port read/write */
	raw = __pci_csr_read(port, PCI_CSR_BE_CMD_VAL(PCI_IO_READ, be));

	return (raw >> BYTE_ENABLE_SHIFT(port, size)) & SIZE_MASK(size);
}

static inline void __pci_emiss_out(u32 val, unsigned long port, int size)
{
	unsigned be = BYTE_ENABLE(port, size);
	u32 cmd;

	cmd = PCI_CSR_BE_CMD_VAL(PCI_IO_WRITE, be);
	val <<=  BYTE_ENABLE_SHIFT(port, size);

	__pci_csr_write(port, cmd, val);
}

#define in_func(size, ext) \
	u##size pci_emiss_in##ext(unsigned long port) \
	{ \
		return (u##size) pci_emiss_in(port, size >> 3); \
	}
in_func(8, b)
in_func(16, w)
in_func(32, l)

/* Just does the same thing for now, I don't think we need a pause
 * here, as there is quite a bit of overhead anyway */
#define in_pause_func(size, ext) \
	u##size pci_emiss_in##ext##_p(unsigned long port) \
	{ \
		return pci_emiss_in##ext(port); \
	}
in_pause_func(8,  b)
in_pause_func(16, w)
in_pause_func(32, l)

/* For the string functions, in order to try to improve performance,
 * I've chunked the write together rather than taking/release the
 * lock for each transaction. This makes interrupt latency worse but
 * should improve performance. */
#define PCI_IO_CHUNK_SIZE 32 /* Correct size ? */

#define string_in_func(size, ext) \
	void pci_emiss_ins##ext(unsigned long port, void *dst, \
			unsigned long count) \
	{ \
		u##size *buf = (u##size *)dst; \
		unsigned long flags; \
		unsigned long chunk; \
	\
		while (count >= PCI_IO_CHUNK_SIZE) { \
			spin_lock_irqsave(&stm_pci_io_lock, flags); \
			for (chunk = count - PCI_IO_CHUNK_SIZE; \
					count > chunk; count--) \
				*buf++ = __pci_emiss_in(port, size >> 3); \
			spin_unlock_irqrestore(&stm_pci_io_lock, flags); \
		} \
	\
		/* Tail end  */ \
		spin_lock_irqsave(&stm_pci_io_lock, flags); \
		while (count--) \
			*buf++ =  __pci_emiss_in(port, size >> 3); \
		spin_unlock_irqrestore(&stm_pci_io_lock, flags); \
	}
string_in_func(8,  b)
string_in_func(16, w)
string_in_func(32, l)

#define out_func(size, ext) \
	void pci_emiss_out##ext(u##size val, unsigned long port) \
	{ \
		pci_emiss_out(val, port, size >> 3); \
	}
out_func(8, b)
out_func(16, w)
out_func(32, l)

#define out_pause_func(size, ext) \
	void pci_emiss_out##ext##_p(u##size val, unsigned long port) \
	{ \
		pci_emiss_out##ext(val, port); \
	}
out_pause_func(8,  b)
out_pause_func(16, w)
out_pause_func(32, l)

#define string_out_func(size, ext) \
	void pci_emiss_outs##ext(unsigned long port, const void *src, \
			unsigned long count) \
	{ \
		u##size *buf = (u##size *)src; \
		unsigned long flags; \
		unsigned long chunk; \
	\
		while (count >= PCI_IO_CHUNK_SIZE) { \
			spin_lock_irqsave(&stm_pci_io_lock, flags); \
			for (chunk = count - PCI_IO_CHUNK_SIZE; count > chunk; \
					count--)	  \
				__pci_emiss_out(*buf++, port, size >> 3); \
			spin_unlock_irqrestore(&stm_pci_io_lock, flags); \
		} \
	\
		/* Tail end  */ \
		spin_lock_irqsave(&stm_pci_io_lock, flags); \
		while (count--) \
			__pci_emiss_out(*buf++, port, size >> 3); \
		spin_unlock_irqrestore(&stm_pci_io_lock, flags); \
	}
string_out_func(8, b)
string_out_func(16, w)
string_out_func(32, l)


static void __iomem __devinit *plat_ioremap_region(struct platform_device *pdev,
		char *name)
{
	struct resource *res;
	unsigned long size;
	void __iomem *p;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (res == NULL)
		return NULL;

	size = res->end - res->start + 1;

	if (!devm_request_mem_region(&pdev->dev, res->start, size, name))
		return NULL;

	p = devm_ioremap_nocache(&pdev->dev, res->start, size);

	if (p == NULL)
		printk(KERN_ERR "pci_stm: Failed to map address 0x%08x\n",
				res->start);

	return p;
}

static irqreturn_t pci_stm_dma_irq(int irq, void *data)
{
	printk(KERN_WARNING "pci_stm: PCI_BRIDGE_INT_DMA_STATUS = 0x%08x\n",
			readl(emiss + PCI_BRIDGE_INT_DMA_STATUS));
	printk(KERN_WARNING "pci_stm: Transaction with no associated "
			"function\n");

	writel(~0, emiss + PCI_BRIDGE_INT_DMA_CLEAR);

	return IRQ_HANDLED;
}

static irqreturn_t pci_stm_error_irq(int irq, void *data)
{
	printk(KERN_ERR "pci_stm: PCI_DEVICEINTMASK_INT_STATUS = 0x%08x\n",
			readl(emiss + PCI_DEVICEINTMASK_INT_STATUS));
	printk(KERN_ERR "pci_stm: PCI_PME_STATUSCHG_INT_STATUS = 0x%08x\n",
			readl(emiss + PCI_PME_STATUSCHG_INT_STATUS));
	printk(KERN_ERR "pci_stm: PCI_PME_STATECHG_INT_STATUS = 0x%08x\n",
			readl(emiss + PCI_PME_STATECHG_INT_STATUS));

	printk(KERN_ERR "pci_stm: PCI_CSR_PCI_ERROR = 0x%08x\n",
			readl(ahb_pci + PCI_CSR_PCI_ERROR));
	printk(KERN_ERR "pci_stm: PCI_CSR_PCI_ERROR_ADDR = 0x%08x\n",
			readl(ahb_pci + PCI_CSR_PCI_ERROR_ADDR));
	printk(KERN_ERR "pci_stm: PCI_CSR_AHB_ERROR = 0x%08x\n",
			readl(ahb_pci + PCI_CSR_AHB_ERROR));
	printk(KERN_ERR "pci_stm: PCI_CSR_AHB_ERROR_ADDR = 0x%08x\n",
			readl(ahb_pci + PCI_CSR_AHB_ERROR_ADDR));

	panic("pci_stm: PCI error interrupt raised\n");

	return IRQ_HANDLED;
}

static irqreturn_t pci_stm_serr_irq(int irq, void *data)
{
	panic("pci_stm: SERR interrupt raised\n");

	return IRQ_HANDLED;
}

/* The configuration read/write functions */
static struct pci_ops pci_config_ops = {
	.read = pci_stm_config_read,
	.write = pci_stm_config_write,
};

static struct pci_channel stm_pci_controller = {
	.pci_ops = &pci_config_ops
};

void pci_stm_pio_reset(void)
{
	/* Active low for PCI signals */
	if (gpio_direction_output(pci_reset_pin, 0)) {
		printk(KERN_ERR "pci_stm: cannot set PCI RST (gpio %u)"
				"to output\n", pci_reset_pin);
		return;
	}

	mdelay(1); /* From PCI spec */

	/* Change to input, assumes pullup . This will work for boards like the
	 * PDK7105 which do a power on reset as well via a diode. If you drive
	 * this as an output it prevents the reset switch (and the JTAG
	 * reset!) from working correctly */
	gpio_direction_input(pci_reset_pin);

	/* PCI spec says there should be a one second delay here. This seems a
	 * tad excessive to me! If you really have something that needs a huge
	 * reset time then you should supply your own reset function */
	mdelay(10);
}

static void __devinit pci_stm_setup(struct stm_plat_pci_config *pci_config,
		unsigned long pci_window_start,
		unsigned long pci_window_size)
{
	unsigned long lmi_base, lmi_end, mbar_size;
	int fn;
	unsigned v;
	unsigned long req_gnt_mask;
	int i, req;

	/* Set up the EMI to use PCI */
	emi_config_pci();

	/* You HAVE to have either wrap or ping-pong enabled, even though they
	 * are different bits. Very strange */
	writel(PCI_BRIDGE_CONFIG_RESET | PCI_BRIDGE_CONFIG_HOST_NOT_DEVICE |
			PCI_BRIDGE_CONFIG_WRAP_ENABLE_ALL,
			emiss + PCI_BRIDGE_CONFIG);

	v = readl(emiss + EMISS_CONFIG);
	writel((v & ~EMISS_CONFIG_CLOCK_SELECT_MASK) |
			EMISS_CONFIG_PCI_CLOCK_MASTER |
			EMISS_CONFIG_CLOCK_SELECT_PCI |
			EMISS_CONFIG_PCI_HOST_NOT_DEVICE,
			emiss + EMISS_CONFIG);

	/* It doesn't make any sense to try to use req/gnt3 when the chip has
	 * the req0_to_req3 workaround. Effectively req3 is disconnected, so
	 * it only supports 3 external masters
	 */
	BUG_ON(pci_config->req0_to_req3 &&
	       pci_config->req_gnt[3] != PCI_PIN_UNUSED);

	req_gnt_mask = EMISS_ARBITER_CONFIG_BUS_REQ_ALL_MASKED;
	/* Figure out what req/gnt lines we are using */
	for (i = 0; i < 4; i++) {
		if (pci_config->req_gnt[i] != PCI_PIN_UNUSED) {
			req = ((i == 0) && pci_config->req0_to_req3) ? 3 : i;
			req_gnt_mask &= ~EMISS_ARBITER_CONFIG_MASK_BUS_REQ(req);
		}
	}
	/* The PCI_NOT_EMI bit really controls MPX or PCI. It also must be set
	 * to allow the req0_to_req3 logic to be enabled. GRANT_RETRACTION is
	 * not available on MPX, so should be set for PCI
	 */
	writel(EMISS_ARBITER_CONFIG_PCI_NOT_EMI |
	       EMISS_ARBITER_CONFIG_GRANT_RETRACTION |
	       req_gnt_mask, emiss + EMISS_ARBITER_CONFIG);

	/* This field will need to be parameterised by the soc layer for sure,
	 * all silicon will likely be different */
	v = PCI_AD_CONFIG_READ_AHEAD(pci_config->ad_read_ahead);
	v |= PCI_AD_CONFIG_CHUNKS_IN_MSG(pci_config->ad_chunks_in_msg);
	v |= PCI_AD_CONFIG_PCKS_IN_CHUNK(pci_config->ad_pcks_in_chunk);
	v |= PCI_AD_CONFIG_TRIGGER_MODE(pci_config->ad_trigger_mode);
	v |= PCI_AD_CONFIG_MAX_OPCODE(pci_config->ad_max_opcode);
	v |= PCI_AD_CONFIG_POSTED(pci_config->ad_posted);
	v |= PCI_AD_CONFIG_THRESHOLD(pci_config->ad_threshold);
	writel(v, emiss +  PCI_AD_CONFIG);

	/* Now we can start to program up the BARs and probe the bus */

	/* Set up the window from the STBUS space to PCI space
	 * We want a one to one physical mapping, anything else is far too
	 * complicated (and pointless) */
	writel(pci_window_start, emiss + PCI_FRAME_ADDR);
	/* Largest PCI address will form the mask in effect.
	 * Assumes pci_window_size is ^2 */
	writel(pci_window_size  - 1, emiss + PCI_FRAMEADDR_MASK);

	/* Now setup the reverse mapping, using as many functions as we have
	 * to. Each function maps 256Megs */

	/* This does not have to be on a 256 Meg boundary */
	lmi_base = CONFIG_MEMORY_START;
	/* Rarely a multiple of 256 Megs */
	lmi_end = lmi_base + CONFIG_MEMORY_SIZE - 1;

	/* Attempt to size the MBARS */
	pci_crp_writel(0, PCI_BASE_ADDRESS_0, ~0);
	/* The mbar size should be 256Megs, but doing it this way means it
	 * can change */
	mbar_size = ~(pci_crp_readl(0, PCI_BASE_ADDRESS_0) &
			PCI_BASE_ADDRESS_MEM_MASK) + 1;

	/* Drop lmi base so that it is a multiple of the mbar size */
	lmi_base = lmi_base & ~(mbar_size - 1);

	for (fn = 0; fn < 8 && lmi_base < lmi_end; fn++) {
		pci_crp_writel(fn, PCI_BASE_ADDRESS_0, lmi_base);
		pci_crp_writeb(fn, PCI_CACHE_LINE_SIZE, L1_CACHE_BYTES >> 2);
		pci_crp_writeb(fn, PCI_LATENCY_TIMER, 0xff);
		pci_crp_writew(fn, PCI_COMMAND,
				PCI_COMMAND_SERR | PCI_COMMAND_PARITY |
				PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY);

		/* Update the emiss registers, we just have a one to one
		 * translation */
		writel(lmi_base, emiss + PCI_BUFFADDR_FUNC(fn, 0));
		writel(lmi_base, emiss + PCI_BUFFADDR_FUNC(fn, 1));

		writel(mbar_size, emiss + PCI_FUNC_BUFFDEPTH(fn));
		writel(PCI_FUNC_BUFF_CONFIG_ENABLE |
				PCI_FUNC_BUFF_CONFIG_FUNC_ID(fn),
				emiss + PCI_FUNC_BUFF_CONFIG(fn));

		lmi_base += mbar_size;
	}

	/* Generate an error if we get a transaction that isn't
	 * claimed by anybody */
	writel(PCI_BRIDGE_INT_DMA_ENABLE_INT_ENABLE |
			PCI_BRIDGE_INT_DMA_ENABLE_INT_UNDEF_FN_ENABLE,
			emiss + PCI_BRIDGE_INT_DMA_ENABLE);

	/* Reset any pci peripherals that are connected to the board */
	if (pci_config->pci_reset)
		pci_config->pci_reset();
}

/* Probe function for PCI data
 * When we get here, we can assume that the PCI block is powered up and ready
 * to rock, and that all sysconfigs have been set correctly. All mangling
 * of emiss arbiter registers is done here */
static int __devinit pci_stm_probe(struct platform_device *pdev)
{
	struct pci_channel *chan = &stm_pci_controller;
	struct resource *res;
	int ret, irq;
	unsigned long pci_window_start, pci_window_size;
	struct stm_plat_pci_config *pci_config = pdev->dev.platform_data;
	struct clk *pci_clk;

	emiss = plat_ioremap_region(pdev, "pci emiss");
	if (emiss == NULL)
		return -ENOMEM;

	ahb_pci = plat_ioremap_region(pdev, "pci ahb");
	if (ahb_pci == NULL)
		return -ENOMEM;

	/* We don't use any of the ping-pong weirdness,
	 * but sometimes the errors are useful */
	irq = platform_get_irq_byname(pdev, "pci dma");
	ret = devm_request_irq(&pdev->dev, irq, pci_stm_dma_irq, 0,
			"pci dma", NULL);
	if (ret)
		printk(KERN_ERR "pci_stm: Cannot request DMA irq %d\n", irq);

	irq = platform_get_irq_byname(pdev, "pci err");
	ret = devm_request_irq(&pdev->dev, irq, pci_stm_error_irq, 0,
			"pci error", NULL);
	if (ret)
		printk(KERN_ERR "pci_stm: Cannot request error irq "
				"%d\n", irq);

	irq = platform_get_irq_byname(pdev, "pci serr");
	/* Don't hook the error interrupt if we are told not to */
	if (irq > 0) {
		ret = devm_request_irq(&pdev->dev, irq, pci_stm_serr_irq, 0,
				"pci serr", NULL);
		if (ret)
			printk(KERN_ERR "pci_stm: Cannot request SERR irq "
					"%d\n", irq);
	}

	if (!pci_config->clk_name)
		pci_config->clk_name = "pci";
	pci_clk = clk_get(&pdev->dev, pci_config->clk_name);
	if (pci_clk && !IS_ERR(pci_clk)) {
		unsigned long pci_clk_rate = (pci_config->pci_clk == 0) ?
				33333333 : pci_config->pci_clk;
		unsigned long pci_clk_mhz = pci_clk_rate / 1000000;

		printk(KERN_INFO "pci_stm: Setting PCI clock to %luMHz\n",
				pci_clk_mhz);
		if (clk_set_rate(pci_clk, pci_clk_rate))
			printk(KERN_ERR "pci_stm: Unable to set PCI clock to "
					"%luMHz\n", pci_clk_mhz);
	} else {
		printk(KERN_ERR "pci_stm: Unable to find pci clock\n");
	}

	if (!pci_config->pci_reset && pci_config->pci_reset_gpio != -EINVAL) {
		/* We have not been given a reset function by the board layer,
		 * and the PIO is valid.  Assume it is done via PIO. Claim pins
		 * specified in config and use default PIO reset function.  */
		if (!gpio_request(pci_config->pci_reset_gpio, "pci reset")) {
			pci_reset_pin = pci_config->pci_reset_gpio;
			pci_config->pci_reset = pci_stm_pio_reset;
		} else {
			printk(KERN_ERR "pci_stm: PIO pin %d specified "
					"for reset, cannot request\n",
					pci_config->pci_reset_gpio);
		}
	}

	/* Extract where the PCI memory window is supposed to be */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pci memory");
	if (res == NULL)
		return -ENXIO;

	pci_window_start = res->start;
	pci_window_size = res->end - res->start + 1 ;

	/* Set up the sh board channel stuff to point at the platform data
	 * we have passed in */
	chan->mem_resource = res;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO, "pci io");
	if (res == NULL)
		return -ENXIO;
	chan->io_resource = res;

	/* Copy over into globals used by config read/write code */
	idsel_lo = pci_config->idsel_lo;
	max_slot = pci_config->idsel_hi - idsel_lo;

	/* Now do all the register poking */
	pci_stm_setup(pci_config, pci_window_start, pci_window_size);

	ret = stm_pci_register_controller(pdev, &pci_config_ops, STM_PCI_EMISS);

	return ret;
}

/* There is no power management for PCI at the moment
 * should be relatively straightforward to add I think
 * once the main driver is stable. */
static struct platform_driver pci_stm_driver = {
	.driver.name = "pci_stm",
	.driver.owner = THIS_MODULE,
	.probe = pci_stm_probe,
};


static int __init pci_stm_init(void)
{
	return	platform_driver_register(&pci_stm_driver);
}

/* Has to be pretty early */
arch_initcall(pci_stm_init);

