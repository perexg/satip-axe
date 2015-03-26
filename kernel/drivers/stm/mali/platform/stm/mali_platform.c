/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 * Copyright (C) 2011 STMicroelectronics R&D Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"

/* Use stubs for now */

_mali_osk_errcode_t mali_platform_init(_mali_osk_resource_t *resource)
{
	MALI_DEBUG_PRINT(3, ("[%s] Stub version, doing nothing\n", __FUNCTION__));
    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(_mali_osk_resource_type_t *type)
{
	MALI_DEBUG_PRINT(3, ("[%s] Stub version, doing nothing\n", __FUNCTION__));
    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_powerdown(u32 cores)
{
	MALI_DEBUG_PRINT(3, ("[%s] Stub version, doing nothing\n", __FUNCTION__));
    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_powerup(u32 cores)
{
	MALI_DEBUG_PRINT(3, ("[%s] Stub version, doing nothing\n", __FUNCTION__));
    MALI_SUCCESS;
}

void mali_gpu_utilization_handler(u32 utilization)
{
	MALI_DEBUG_PRINT(3, ("[%s] Stub version, doing nothing\n", __FUNCTION__));
}

#if MALI_POWER_MGMT_TEST_SUITE
u32 pmu_get_power_up_down_info(void)
{
	MALI_DEBUG_PRINT(3, ("[%s] Stub version, doing nothing, returning 4095\n", __FUNCTION__));
	return 4095;
}
#endif
