/*
 *   Core routines for STMicroelectronics' SoCs audio drivers
 *
 *   Copyright (c) 2005-2011 STMicroelectronics Limited
 *
 *   Author: Pawel Moll <pawel.moll@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/bpa2.h>
#include <linux/stm/stm-dma.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/info.h>
#include <sound/pcm_params.h>
#include <sound/asoundef.h>

#include "common.h"



int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);

static int snd_stm_card_index = -1; /* First available index */
module_param_named(index, snd_stm_card_index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STMicroelectronics audio subsystem "
		"card.");

static char *snd_stm_card_id;
module_param_named(id, snd_stm_card_id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STMicroelectronics audio subsystem card.");



/*
 * ALSA card management
 */

static struct snd_card *snd_stm_card;
static int snd_stm_card_registered;

int snd_stm_card_register(void)
{
	int result;
	const char *soc_type = get_cpu_subtype(cpu_data);

	BUG_ON(!soc_type);
	BUG_ON(!snd_stm_card);
	BUG_ON(snd_stm_card_registered);

	if (!snd_stm_card_id)
		strlcpy(snd_stm_card->id, soc_type, sizeof(snd_stm_card->id));

	strlcpy(snd_stm_card->driver, soc_type, sizeof(snd_stm_card->driver));
	snprintf(snd_stm_card->shortname, sizeof(snd_stm_card->shortname),
			"%s audio subsystem", soc_type);
	if (cpu_data->cut_minor < 0)
		snprintf(snd_stm_card->longname, sizeof(snd_stm_card->longname),
				"STMicroelectronics %s cut %d.x SOC audio "
				"subsystem", soc_type, cpu_data->cut_major);
	else
		snprintf(snd_stm_card->longname, sizeof(snd_stm_card->longname),
				"STMicroelectronics %s cut %d.%d SOC audio "
				"subsystem", soc_type, cpu_data->cut_major,
				cpu_data->cut_minor);

	result = snd_card_register(snd_stm_card);

	if (result == 0)
		snd_stm_card_registered = 1;

	return result;
}
EXPORT_SYMBOL(snd_stm_card_register);

int snd_stm_card_is_registered(void)
{
	BUG_ON(!snd_stm_card);

	return snd_stm_card_registered;
}
EXPORT_SYMBOL(snd_stm_card_is_registered);

struct snd_card *snd_stm_card_get(void)
{
	BUG_ON(!snd_stm_card);

	return snd_stm_card;
}
EXPORT_SYMBOL(snd_stm_card_get);


/*
 * Resources management
 */

int snd_stm_memory_request(struct platform_device *pdev,
		struct resource **mem_region, void **base_address)
{
	struct resource *resource;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		snd_stm_printe("Failed to"
				" platform_get_resource(IORESOURCE_MEM)!\n");
		return -ENODEV;
	}

	*mem_region = request_mem_region(resource->start,
			resource->end - resource->start + 1, pdev->name);
	if (!*mem_region) {
		snd_stm_printe("Failed request_mem_region(0x%08x,"
				" 0x%08x, '%s')!\n", resource->start,
				resource->end - resource->start + 1,
				pdev->name);
		return -EBUSY;
	}
	snd_stm_printd(0, "Memory region: 0x%08x-0x%08x\n",
			(*mem_region)->start, (*mem_region)->end);

	*base_address = ioremap(resource->start,
			resource->end - resource->start + 1);
	if (!*base_address) {
		release_resource(*mem_region);
		snd_stm_printe("Failed ioremap!\n");
		return -EINVAL;
	}

	snd_stm_printd(0, "Base address is 0x%p.\n", *base_address);

	return 0;
}
EXPORT_SYMBOL(snd_stm_memory_request);

void snd_stm_memory_release(struct resource *mem_region,
		void *base_address)
{
	iounmap(base_address);
	release_resource(mem_region);
}
EXPORT_SYMBOL(snd_stm_memory_release);

int snd_stm_irq_request(struct platform_device *pdev,
		unsigned int *irq, irq_handler_t handler, void *dev_id)
{
	struct resource *resource;
	int result;

	resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!resource) {
		snd_stm_printe("Failed to "
				"platform_get_resource(IORESOURCE_IRQ)!\n");
		return -ENODEV;
	}
	snd_stm_printd(0, "IRQ: %u\n", resource->start);

	*irq = resource->start;

	result = request_irq(*irq, handler, IRQF_DISABLED, pdev->name, dev_id);
	if (result != 0) {
		snd_stm_printe("Failed request_irq!\n");
		return -EINVAL;
	}

	/* request_irq() enables the interrupt immediately; as it is
	 * lethal in concurrent audio environment, we want to have
	 * it disabled for most of the time... */
	disable_irq(*irq);

	return 0;
}

int snd_stm_fdma_request(struct platform_device *pdev, int *channel)
{
	static const char *fdmac_id[] = { STM_DMAC_ID, NULL };
	static const char *fdma_cap_lb[] = { STM_DMA_CAP_LOW_BW, NULL };
	static const char *fdma_cap_hb[] = { STM_DMA_CAP_HIGH_BW, NULL };

	*channel = request_dma_bycap(fdmac_id, fdma_cap_lb, pdev->name);
	if (*channel < 0) {
		*channel = request_dma_bycap(fdmac_id, fdma_cap_hb, pdev->name);
		if (*channel < 0) {
			snd_stm_printe("Failed to request_dma_bycap()==%d!\n",
					*channel);
			return -ENODEV;
		}
	}
	snd_stm_printd(0, "FDMA channel: %d\n", *channel);

	return 0;
}



/*
 * ALSA procfs additional entries
 */

static struct snd_info_entry *snd_stm_info_root;

int snd_stm_info_create(void)
{
	int result = 0;

	snd_stm_info_root = snd_info_create_module_entry(THIS_MODULE,
			"stm", NULL);
	if (snd_stm_info_root) {
		snd_stm_info_root->mode = S_IFDIR | S_IRUGO | S_IXUGO;
		if (snd_info_register(snd_stm_info_root) < 0) {
			result = -EINVAL;
			snd_info_free_entry(snd_stm_info_root);
		}
	} else {
		result = -ENOMEM;
	}

	return result;
}

void snd_stm_info_dispose(void)
{
	if (snd_stm_info_root)
		snd_info_free_entry(snd_stm_info_root);
}

int snd_stm_info_register(struct snd_info_entry **entry,
		const char *name,
		void (read)(struct snd_info_entry *, struct snd_info_buffer *),
		void *private_data)
{
	int result = 0;

	/* Skip the "snd_" prefix, if bus_id has been simply given */
	if (strncmp(name, "snd_", 4) == 0)
		name += 4;

	*entry = snd_info_create_module_entry(THIS_MODULE, name,
			snd_stm_info_root);
	if (*entry) {
		(*entry)->c.text.read = read;
		(*entry)->private_data = private_data;
		if (snd_info_register(*entry) < 0) {
			result = -EINVAL;
			snd_info_free_entry(*entry);
		}
	} else {
		result = -EINVAL;
	}
	return result;
}
EXPORT_SYMBOL(snd_stm_info_register);

void snd_stm_info_unregister(struct snd_info_entry *entry)
{
	if (entry)
		snd_info_free_entry(entry);
}
EXPORT_SYMBOL(snd_stm_info_unregister);



/*
 * PCM buffer memory management
 */

struct snd_stm_buffer {
	struct snd_pcm *pcm;

	struct bpa2_part *bpa2_part;

	int allocated;
	struct snd_pcm_substream *substream;

	snd_stm_magic_field;
};

#if defined(CONFIG_BPA2)
static char *bpa2_part = CONFIG_SND_STM_BPA2_PARTITION_NAME;
#else
static char *bpa2_part = "";
#endif
module_param(bpa2_part, charp, S_IRUGO);

struct snd_stm_buffer *snd_stm_buffer_create(struct snd_pcm *pcm,
		struct device *device, int prealloc_size)
{
	struct snd_stm_buffer *buffer;

	snd_stm_printd(1, "snd_stm_buffer_init(pcm=%p, prealloc_size=%d)\n",
			pcm, prealloc_size);

	BUG_ON(!pcm);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		snd_stm_printe("Can't allocate memory for a buffer "
				"description!\n");
		return NULL;
	}
	snd_stm_magic_set(buffer);
	buffer->pcm = pcm;

#if defined(CONFIG_BPA2)
	buffer->bpa2_part = bpa2_find_part(bpa2_part);
	if (buffer->bpa2_part) {
		snd_stm_printd(0, "Using BPA2 partition '%s'...\n", bpa2_part);
		return buffer;
	}

	buffer->bpa2_part = bpa2_find_part("bigphysarea");
	if (buffer->bpa2_part) {
		snd_stm_printd(0, "Using legacy 'bigphysarea' BPA2 "
				"partition...\n");
		return buffer;
	}
#endif

	if (snd_pcm_lib_preallocate_pages_for_all(pcm,
			SNDRV_DMA_TYPE_DEV, device,
			prealloc_size, prealloc_size) == 0) {
		snd_stm_printd(0, "Using pcm_lib's preallocated buffer "
				"(%d bytes)...\n", prealloc_size);
		return buffer;
	}

	snd_stm_printe("Can't provide any memory for buffers!\n");
	kfree(buffer);
	return NULL;
}

void snd_stm_buffer_dispose(struct snd_stm_buffer *buffer)
{
	snd_stm_printd(1, "snd_stm_buffer_dispose(buffer=%p)\n", buffer);

	BUG_ON(!buffer);
	BUG_ON(!snd_stm_magic_valid(buffer));
	BUG_ON(buffer->allocated);

	/* snd_pcm_lib__preallocate*-ed buffer is freed automagically */

	snd_stm_magic_clear(buffer);
	kfree(buffer);
}

int snd_stm_buffer_is_allocated(struct snd_stm_buffer *buffer)
{
	snd_stm_printd(1, "snd_stm_buffer_is_allocated(buffer=%p)\n",
			buffer);

	BUG_ON(!buffer);
	BUG_ON(!snd_stm_magic_valid(buffer));

	return buffer->allocated;
}

int snd_stm_buffer_alloc(struct snd_stm_buffer *buffer,
		struct snd_pcm_substream *substream, int size)
{
	snd_stm_printd(1, "snd_stm_buffer_alloc(buffer=%p, substream=%p, "
			"size=%d)\n", buffer, substream, size);

	BUG_ON(!buffer);
	BUG_ON(!snd_stm_magic_valid(buffer));
	BUG_ON(buffer->allocated);
	BUG_ON(size <= 0);

	if (buffer->bpa2_part) {
#if defined(CONFIG_BPA2)
		struct snd_pcm_runtime *runtime = substream->runtime;
		int pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

		runtime->dma_addr = bpa2_alloc_pages(buffer->bpa2_part, pages,
				0, GFP_KERNEL);
		if (runtime->dma_addr == 0) {
			snd_stm_printe("Can't get %d pages from BPA2!\n",
					pages);
			return -ENOMEM;
		}
		runtime->dma_bytes = size;
		runtime->dma_area = ioremap_nocache(runtime->dma_addr, size);
#else
		snd_BUG();
#endif
	} else {
		if (snd_pcm_lib_malloc_pages(substream, size) < 0) {
			snd_stm_printe("Can't allocate pages using pcm_lib!\n");
			return -ENOMEM;
		}
		/* runtime->dma_* are set by snd_pcm_lib_malloc_pages()
		 * (by snd_pcm_set_runtime_buffer() to be more specific.) */
	}

	snd_stm_printd(1, "Allocated memory: dma_addr=0x%08x, dma_area=0x%p, "
			"dma_bytes=%u\n", substream->runtime->dma_addr,
			substream->runtime->dma_area,
			substream->runtime->dma_bytes);

	buffer->substream = substream;
	buffer->allocated = 1;

	return 0;
}

void snd_stm_buffer_free(struct snd_stm_buffer *buffer)
{
	struct snd_pcm_runtime *runtime;

	snd_stm_printd(1, "snd_stm_buffer_free(buffer=%p)\n", buffer);

	BUG_ON(!buffer);
	BUG_ON(!snd_stm_magic_valid(buffer));
	BUG_ON(!buffer->allocated);

	runtime = buffer->substream->runtime;

	snd_stm_printd(1, "Freeing dma_addr=0x%08x, dma_area=0x%p, "
			"dma_bytes=%u\n", runtime->dma_addr,
			runtime->dma_area, runtime->dma_bytes);

	if (buffer->bpa2_part) {
#if defined(CONFIG_BPA2)
		iounmap(runtime->dma_area);

		bpa2_free_pages(buffer->bpa2_part, runtime->dma_addr);
		runtime->dma_area = NULL;
		runtime->dma_addr = 0;
		runtime->dma_bytes = 0;
#else
		snd_BUG();
#endif
	} else {
		snd_pcm_lib_free_pages(buffer->substream);
		/* runtime->dma_* are cleared by snd_pcm_lib_free_pages()
		 * (by snd_pcm_set_runtime_buffer() to be more specific.) */
	}

	buffer->allocated = 0;
	buffer->substream = NULL;
}

static int snd_stm_buffer_mmap_fault(struct vm_area_struct *area,
				     struct vm_fault *vmf)
{
	/* No VMA expanding here! */
	return VM_FAULT_SIGBUS;
}

static struct vm_operations_struct snd_stm_buffer_mmap_vm_ops = {
	.open  = snd_pcm_mmap_data_open,
	.close = snd_pcm_mmap_data_close,
	.fault = snd_stm_buffer_mmap_fault,
};

int snd_stm_buffer_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *area)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long map_offset = area->vm_pgoff << PAGE_SHIFT;
	unsigned long phys_addr = runtime->dma_addr + map_offset;
	unsigned long map_size = area->vm_end - area->vm_start;
	unsigned long phys_size = runtime->dma_bytes + PAGE_SIZE -
			runtime->dma_bytes % PAGE_SIZE;

	snd_stm_printd(1, "snd_stm_buffer_mmap(substream=%p, area=%p)\n",
			substream, area);

	snd_stm_printd(1, "Mmaping %lu bytes starting from 0x%08lx "
			"(dma_addr=0x%08x, dma_size=%u, vm_pgoff=%lu, "
			"vm_start=0x%lx, vm_end=0x%lx)...\n", map_size,
			phys_addr, runtime->dma_addr, runtime->dma_bytes,
			area->vm_pgoff, area->vm_start, area->vm_end);

	if (map_size > phys_size) {
		snd_stm_printe("Trying to perform mmap larger than buffer!\n");
		return -EINVAL;
	}

	area->vm_ops = &snd_stm_buffer_mmap_vm_ops;
	area->vm_private_data = substream;
	area->vm_flags |= VM_RESERVED;
	area->vm_page_prot = pgprot_noncached(area->vm_page_prot);

	if (remap_pfn_range(area, area->vm_start, phys_addr >> PAGE_SHIFT,
			map_size, area->vm_page_prot) != 0) {
		snd_stm_printe("Can't remap buffer!\n");
		return -EAGAIN;
	}

	/* Must be called implicitly here... */
	snd_pcm_mmap_data_open(area);

	return 0;
}



/*
 * Common ALSA parameters constraints
 */

/*
#define FIXED_TRANSFER_BYTES max_transfer_bytes > 16 ? 16 : max_transfer_bytes
#define FIXED_TRANSFER_BYTES max_transfer_bytes
*/

#if defined(FIXED_TRANSFER_BYTES)

int snd_stm_pcm_transfer_bytes(unsigned int bytes_per_frame,
		unsigned int max_transfer_bytes)
{
	int transfer_bytes = FIXED_TRANSFER_BYTES;

	snd_stm_printd(1, "snd_stm_pcm_transfer_bytes(bytes_per_frame=%u, "
			"max_transfer_bytes=%u) = %u (FIXED)\n",
			bytes_per_frame, max_transfer_bytes, transfer_bytes);

	return transfer_bytes;
}

int snd_stm_pcm_hw_constraint_transfer_bytes(struct snd_pcm_runtime *runtime,
		unsigned int max_transfer_bytes)
{
	return snd_pcm_hw_constraint_step(runtime, 0,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
			snd_stm_pcm_transfer_bytes(0, max_transfer_bytes));
}

#else

int snd_stm_pcm_transfer_bytes(unsigned int bytes_per_frame,
		unsigned int max_transfer_bytes)
{
	unsigned int transfer_bytes;

	for (transfer_bytes = bytes_per_frame;
			transfer_bytes * 2 < max_transfer_bytes;
			transfer_bytes *= 2)
		;

	snd_stm_printd(2, "snd_stm_pcm_transfer_bytes(bytes_per_frame=%u, "
			"max_transfer_bytes=%u) = %u\n", bytes_per_frame,
			max_transfer_bytes, transfer_bytes);

	return transfer_bytes;
}

static int snd_stm_pcm_hw_rule_transfer_bytes(struct snd_pcm_hw_params *params,
		struct snd_pcm_hw_rule *rule)
{
	int changed = 0;
	unsigned int max_transfer_bytes = (unsigned int)rule->private;
	struct snd_interval *period_bytes = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES);
	struct snd_interval *frame_bits = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_FRAME_BITS);
	unsigned int transfer_bytes, n;

	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bits->min / 8,
			max_transfer_bytes);
	n = period_bytes->min % transfer_bytes;
	if (n != 0 || period_bytes->openmin) {
		period_bytes->min += transfer_bytes - n;
		changed = 1;
	}

	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bits->max / 8,
			max_transfer_bytes);
	n = period_bytes->max % transfer_bytes;
	if (n != 0 || period_bytes->openmax) {
		period_bytes->max -= n;
		changed = 1;
	}

	if (snd_interval_checkempty(period_bytes)) {
		period_bytes->empty = 1;
		return -EINVAL;
	}

	return changed;
}

int snd_stm_pcm_hw_constraint_transfer_bytes(struct snd_pcm_runtime *runtime,
		unsigned int max_transfer_bytes)
{
	return snd_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
			snd_stm_pcm_hw_rule_transfer_bytes,
			(void *)max_transfer_bytes,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
			SNDRV_PCM_HW_PARAM_FRAME_BITS, -1);
}

#endif



/*
 * Common ALSA controls routines
 */

int snd_stm_ctl_boolean_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}
EXPORT_SYMBOL(snd_stm_ctl_boolean_info);

int snd_stm_ctl_iec958_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;

	return 0;
}


int snd_stm_ctl_iec958_mask_get_con(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_PROFESSIONAL |
			IEC958_AES0_NONAUDIO |
			IEC958_AES0_CON_NOT_COPYRIGHT |
			IEC958_AES0_CON_EMPHASIS |
			IEC958_AES0_CON_MODE;
	ucontrol->value.iec958.status[1] = IEC958_AES1_CON_CATEGORY |
			IEC958_AES1_CON_ORIGINAL;
	ucontrol->value.iec958.status[2] = IEC958_AES2_CON_SOURCE |
			IEC958_AES2_CON_CHANNEL;
	ucontrol->value.iec958.status[3] = IEC958_AES3_CON_FS |
			IEC958_AES3_CON_CLOCK;
	ucontrol->value.iec958.status[4] = IEC958_AES4_CON_MAX_WORDLEN_24 |
			IEC958_AES4_CON_WORDLEN;

	return 0;
}

int snd_stm_ctl_iec958_mask_get_pro(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_PROFESSIONAL |
			IEC958_AES0_NONAUDIO |
			IEC958_AES0_PRO_EMPHASIS |
			IEC958_AES0_PRO_FREQ_UNLOCKED |
			IEC958_AES0_PRO_FS;
	ucontrol->value.iec958.status[1] = IEC958_AES1_PRO_MODE |
			IEC958_AES1_PRO_USERBITS;
	ucontrol->value.iec958.status[2] = IEC958_AES2_PRO_SBITS |
			IEC958_AES2_PRO_WORDLEN;

	return 0;
}

int snd_stm_iec958_cmp(const struct snd_aes_iec958 *a,
		const struct snd_aes_iec958 *b)
{
	int result;

	BUG_ON(!a);
	BUG_ON(!b);

	result = memcmp(a->status, b->status, sizeof(a->status));
	if (result == 0)
		result = memcmp(a->subcode, b->subcode, sizeof(a->subcode));
	if (result == 0)
		result = memcmp(a->dig_subframe, b->dig_subframe,
				sizeof(a->dig_subframe));

	return result;
}


/*
 * Debug features
 */

/* Memory dump function */

void snd_stm_hex_dump(void *data, int size)
{
	unsigned char *buffer = data;
	char line[57];
	int i;

	for (i = 0; i < size; i++) {
		if (i % 16 == 0)
			sprintf(line, "%p", data + i);
		sprintf(line + 8 + ((i % 16) * 3), " %02x", *buffer++);
		if (i % 16 == 15 || i == size - 1)
			printk(KERN_DEBUG "%s\n", line);
	}
}

/* IEC958 structure dump */
void snd_stm_iec958_dump(const struct snd_aes_iec958 *vuc)
{
	int i;
	char line[54];
	const unsigned char *data;

	printk(KERN_DEBUG "                        "
			"0  1  2  3  4  5  6  7  8  9\n");
	data = vuc->status;
	for (i = 0; i < 24; i++) {
		if (i % 10 == 0)
			sprintf(line, "%p status    %02d:",
					(unsigned char *)vuc + i, i);
		sprintf(line + 22 + ((i % 10) * 3), " %02x", *data++);
		if (i % 10 == 9 || i == 23)
			printk(KERN_DEBUG "%s\n", line);
	}

	data = vuc->subcode;
	for (i = 0; i < 147; i++) {
		if (i % 10 == 0)
			sprintf(line, "%p subcode  %03d:",
					(unsigned char *)vuc +
					offsetof(struct snd_aes_iec958,
					dig_subframe) + i, i);
		sprintf(line + 22 + ((i % 10) * 3), " %02x", *data++);
		if (i % 10 == 9 || i == 146)
			printk(KERN_DEBUG "%s\n", line);
	}

	printk(KERN_DEBUG "%p dig_subframe: %02x %02x %02x %02x\n",
			(unsigned char *)vuc +
			offsetof(struct snd_aes_iec958, dig_subframe),
			vuc->dig_subframe[0], vuc->dig_subframe[1],
			vuc->dig_subframe[2], vuc->dig_subframe[3]);
}



/*
 * Core initialization
 */

static int __init snd_stm_core_init(void)
{
	int result;

	snd_stm_printd(0, "%s()\n", __func__);

	result = snd_card_create(snd_stm_card_index, snd_stm_card_id,
			THIS_MODULE, 0, &snd_stm_card);
	if (result != 0) {
		snd_stm_printe("Failed to create ALSA card!\n");
		goto error_card_create;
	}

	result = snd_stm_info_create();
	if (result != 0) {
		snd_stm_printe("Procfs info creation failed!\n");
		goto error_info;
	}
	result = snd_stm_conv_init();
	if (result != 0) {
		snd_stm_printe("Converters infrastructure initialization"
				" failed!\n");
		goto error_conv;
	}

	return result;

error_conv:
	snd_stm_info_dispose();
error_info:
	snd_card_free(snd_stm_card);
error_card_create:
	return result;
}

static void __exit snd_stm_core_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	snd_stm_conv_exit();
	snd_stm_info_dispose();

	snd_card_free(snd_stm_card);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics System-on-Chips' audio core driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_core_init);
module_exit(snd_stm_core_exit);
