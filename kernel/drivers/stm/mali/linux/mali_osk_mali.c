/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 * Copyright (C) 2011 STMicroelectronics R&D Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_osk_mali.c
 * Implementation of the OS abstraction layer which is specific for the Mali kernel device driver
 */
#include <linux/kernel.h>       
#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/ioport.h>
#include <linux/string.h>

#include <asm/uaccess.h>

#include "mali_kernel_common.h" /* MALI_xxx macros */
#include "mali_osk.h"           /* kernel side OS functions */
#include "mali_uk_types.h"
#include "mali_kernel_linux.h"  /* exports initialize/terminate_kernel_device() */

extern struct platform_device *mali_platform_device;

/* Help to convert platform device resources into mali resources */
struct mali_resource_properties
{
	const char *name;
	_mali_osk_resource_type_t type;
	u32 has_irq;
	u32 mmu_id;
};

struct mali_resource_properties mali_resource_table[] = {
	{ "MALI400GP", MALI400GP, 1, 1 },
	{ "MALI400PP-0", MALI400PP, 1, 2 },
	{ "MALI400PP-1", MALI400PP, 1, 3 },
	{ "MALI400PP-2", MALI400PP, 1, 4 },
	{ "MALI400PP-3", MALI400PP, 1, 5 },
	{ "MMU-1", MMU, 1, 1 },
	{ "MMU-2", MMU, 1, 2 },
	{ "MMU-3", MMU, 1, 3 },
	{ "MMU-4", MMU, 1, 4 },
	{ "MMU-5", MMU, 1, 5 },
	{ "MALI400L2", MALI400L2, 0, 0 },
	{ "OS_MEMORY", OS_MEMORY, 0, 0 },
	{ "MEMORY", MEMORY, 0, 0 },
	{ "EXTERNAL_MEMORY_RANGE", MEM_VALIDATION, 0, 0 }
};

/* is called from mali_kernel_constructor in common code */
_mali_osk_errcode_t _mali_osk_init( void )
{
	if (0 != initialize_kernel_device()) MALI_ERROR(_MALI_OSK_ERR_FAULT);

	mali_osk_low_level_mem_init();
	
	MALI_SUCCESS;
}

/* is called from mali_kernel_deconstructor in common code */
void _mali_osk_term( void )
{
	mali_osk_low_level_mem_term();
	terminate_kernel_device();
}

static struct mali_resource_properties *get_resource_properties(const char *name)
{
	int i;
	for(i=0;i<ARRAY_SIZE(mali_resource_table);i++)
	{
		if(!strcmp(mali_resource_table[i].name,name))
			return &mali_resource_table[i];
	}

	MALI_PRINT(("Cannot find resource name %s\n", name));

	return NULL;
}

static int mali_get_mem_resources(_mali_osk_resource_t *resources, struct stm_mali_resource *mem_resources, int num_resources, int *index)
{
	int n = 0;
	while(n < num_resources)
	{
		struct mali_resource_properties *properties;
		struct stm_mali_resource *mem_resource;
		_mali_osk_resource_t *mali_resource = &resources[*index];
		mem_resource = &mem_resources[n];
		if(!(properties = get_resource_properties(mem_resource->name)))
			return _MALI_OSK_ERR_FAULT;
	
		mali_resource->type = properties->type;
		mali_resource->base = mem_resource->start;
		mali_resource->size = mem_resource->end - mem_resource->start + 1;
	
		switch(properties->type)
		{
			case MEMORY:
			case OS_MEMORY:
				mali_resource->flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_READABLE | _MALI_PP_WRITEABLE |_MALI_GP_READABLE | _MALI_GP_WRITEABLE;
				mali_resource->alloc_order = n;
				break;
			case MEM_VALIDATION:
				mali_resource->flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_WRITEABLE | _MALI_PP_READABLE;
				break;
			default:
				MALI_PRINT(("Resource %s should not be passed as mem_resources\n",mem_resource->name));
				return _MALI_OSK_ERR_FAULT;
		}
	
		MALI_DEBUG_PRINT(3, ("Constructed resources for %s\n",mem_resource->name));
		mali_resource->description = mem_resource->name;
		(*index)++;
		n++;
	}
	return _MALI_OSK_ERR_OK;
}

_mali_osk_errcode_t _mali_osk_resources_init( _mali_osk_resource_t **arch_config, u32 *num_resources )
{
	struct resource *platform_resource;
	_mali_osk_resource_t *resources;
	int n,resource_count;
	struct stm_mali_config *mali_config = (struct stm_mali_config *)
							mali_platform_device->dev.platform_data;

	resource_count = mali_platform_device->num_resources +
			 mali_config->num_mem_resources +
			 mali_config->num_ext_resources; 

	resources = kzalloc((resource_count * sizeof(_mali_osk_resource_t)),GFP_KERNEL);
	if(!resources)
		MALI_ERROR(_MALI_OSK_ERR_NOMEM);

        resource_count = 0;
        n = 0;
	while((platform_resource = platform_get_resource(mali_platform_device,IORESOURCE_MEM,n)) != NULL)
	{
		_mali_osk_resource_t *mali_resource = &resources[resource_count];
		struct mali_resource_properties *properties;
		if(!(properties = get_resource_properties(platform_resource->name)))
			goto error;
	
		mali_resource->type = properties->type;
		mali_resource->base = platform_resource->start;
		mali_resource->size = 0;
	
		switch(properties->type)
		{
			case MEMORY:
			case OS_MEMORY:
			case MEM_VALIDATION:
				MALI_PRINT(("Mali Memory resource %s should be passed in platfrom priv data not IORESOURCE_MEM\n",platform_resource->name));
				goto error;
			default:
				break;
		}
	
		mali_resource->mmu_id = properties->mmu_id;
		if(properties->has_irq)
		{
			int irq;
			MALI_DEBUG_PRINT(3, ("finding IRQ for %s\n",platform_resource->name));
			if((irq = platform_get_irq_byname(mali_platform_device,platform_resource->name)) < 0)
			{
				MALI_PRINT(("Unable to find IRQ resource for %s\n",platform_resource->name));
				goto error;
			}
			MALI_DEBUG_PRINT(3, ("found IRQ %d for %s\n",irq,platform_resource->name));
			mali_resource->irq = irq;
		}
	
		MALI_DEBUG_PRINT(3, ("Constructed resources for %s\n",platform_resource->name));
		mali_resource->description = platform_resource->name;
		resource_count++;
		n++;
	}

	/* Memory resources */
	/* Memory resources allocated by Linux kernel and Memory managed by 
	 * mali driver memory allocator */
	if (mali_config->mem)
		mali_get_mem_resources(resources, mali_config->mem,
				mali_config->num_mem_resources, &resource_count);

	/* External memory regions for mali to directly render */
	if (mali_config->ext_mem)
		mali_get_mem_resources(resources, mali_config->ext_mem,
				mali_config->num_ext_resources, &resource_count);
 
	*num_resources = resource_count;
	*arch_config = resources;
	return _MALI_OSK_ERR_OK;

error:
	kfree(resources);
	*arch_config = NULL;
        MALI_ERROR(_MALI_OSK_ERR_FAULT);
}

void _mali_osk_resources_term( _mali_osk_resource_t **arch_config, u32 num_resources )
{
	kfree(*arch_config);
}
