#
# (C) COPYRIGHT 2010 STMicroelectronics R&D Ltd
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file COPYING in the main directory of this archive for
# more details.
#
MALI_DIR := $(srctree)/drivers/stm/mali

# Get ARM release version string
SVN_REV := $(shell [ -e $(MALI_DIR)/version ] && cat $(MALI_DIR)/version)

# Get API version of user-kernel interface by extracting it from the mali_kernel_ioctl.h file
API_VERSION := $(shell grep "\#define _MALI_API_VERSION" $(MALI_DIR)/common/mali_uk_types.h | cut -d' ' -f 3 )

# Driver include paths
ccflags-y := -I$(MALI_DIR) -I$(MALI_DIR)/common -I$(MALI_DIR)/linux -I$(MALI_DIR)/platform 
ccflags-y += -I$(MALI_DIR)/linux/license/gpl

# Source files which always are included in a build
OSK_OBJS := $(addprefix ./linux/, \
        mali_osk_atomics.o \
        mali_osk_irq.o \
        mali_osk_locks.o \
        mali_osk_low_level_mem.o \
        mali_osk_math.o \
        mali_osk_memory.o \
        mali_osk_misc.o \
        mali_osk_mali.o \
        mali_osk_notification.o \
        mali_osk_time.o \
        mali_osk_timers.o)

UKK_OBJS := $(addprefix ./linux/,\
        mali_ukk_mem.o \
        mali_ukk_gp.o \
        mali_ukk_pp.o \
        mali_ukk_core.o)

LINUX_OBJS := $(addprefix ./linux/, \
        mali_kernel_linux.o \
        mali_osk_indir_mmap.o \
        mali_ukk_vsync.o \
        mali_kernel_sysfs.o)

COMMON_OBJS := $(addprefix ./common/, \
        mali_kernel_core.o \
        mali_kernel_rendercore.o \
        mali_kernel_descriptor_mapping.o \
        mali_kernel_vsync.o )

PLATFORM_OBJS := $(addprefix ./platform/stm/, \
        mali_platform.o )

ifeq ($(CONFIG_CPU_SUBTYPE_STX7108)$(CONFIG_CPU_SUBTYPE_FLI7510),y)
SOC                      := stx7108
USING_MMU                := 1
USING_UMP                := 0
USING_MALI200            := 0
USING_MALI400            := 1
USING_MALI400_L2_CACHE   := 1
USING_GP2                := 1

# new for r2p1
ONLY_ZBT                 := 0
USING_ZBT                := 1
USING_OS_MEMORY          := 0
USING_PMM                := 1
USING_MALI_RUN_TIME_PM 	 := 0
USING_GPU_UTILIZATION  	 := 0
USING_PROFILING        	 := 0
TIMESTAMP              	 := default
USING_MALI_PMM_TESTSUITE := 0

# new for r2p2
OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB := 6

MALI_OBJS := $(OSK_OBJS) $(UKK_OBJS) $(COMMON_OBJS) $(LINUX_OBJS) $(PLATFORM_OBJS)
MALI_OBJS += $(addprefix ./common/, \
        mali_kernel_MALI200.o \
        mali_kernel_GP2.o \
        mali_kernel_mem_mmu.o \
        mali_kernel_memory_engine.o \
        mali_block_allocator.o \
        mali_kernel_mem_os.o \
        mali_kernel_l2_cache.o)
endif

ifeq ($(USING_PMM),1)
MALI_OBJS += $(addprefix ./common/pmm/, \
        mali_pmm.o \
        mali_pmm_policy.o \
        mali_pmm_policy_alwayson.o \
        mali_pmm_policy_jobcontrol.o \
        mali_pmm_state.o )
MALI_OBJS += $(addprefix ./linux/, \
        mali_kernel_pm.o \
        mali_osk_pm.o \
        mali_device_pause_resume.o)
ccflags-y += -I$(MALI_DIR)/common/pmm
endif

ifeq ($(USING_PROFILING),1)
MALI_OBJS += $(addprefix ./common/, \
        mali_kernel_profiling.o) \
        timestamp-$(TIMESTAMP)/mali_timestamp.o
ccflags-y += -I$(MALI_DIR)/timestamp-$(TIMESTAMP)
endif

ifeq ($(USING_GPU_UTILIZATION),1)
MALI_OBJS += $(addprefix ./common/, \
        mali_kernel_utilization.o
endif

MAIL_OBJS += malidrv_build_info.o

# Mali driver defines
ccflags-y += 	-DUSING_MMU=$(USING_MMU) \
		-DMALI_USE_UNIFIED_MEMORY_PROVIDER=0 \
		-D_MALI_OSK_SPECIFIC_INDIRECT_MMAP \
		-DMALI_UKK_HAS_IMPLICIT_MMAP_CLEANUP \
		-DUSING_MALI400=$(USING_MALI400) \
		-DUSING_MALI400_L2_CACHE=$(USING_MALI400_L2_CACHE) \
		-DSVN_REV=$(SVN_REV) \
		-DSVN_REV_STRING=\"$(SVN_REV)\"

# Mali driver defines new for r2p1
ccflags-y += 	-DUSING_ZBT=$(USING_ZBT) \
		-DUSING_OS_MEMORY=$(USING_OS_MEMORY) \
		-DONLY_ZBT=$(ONLY_ZBT) \
		-DUSING_MALI_PMM=$(USING_PMM) \
		-DMALI_PMM_RUNTIME_JOB_CONTROL_ON=$(USING_MALI_RUN_TIME_PM) \
		-DMALI_GPU_UTILIZATION=$(USING_GPU_UTILIZATION) \
		-DMALI_TIMELINE_PROFILING_ENABLED=$(USING_PROFILING) \
		-DMALI_POWER_MGMT_TEST_SUITE=$(USING_MALI_PMM_TESTSUITE)

# Mali driver defines new for r2p2
ccflags-y += 	-DMALI_OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB=$(OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB) 
ifeq ($(shell test $(SUBLEVEL) -gt 32 && echo "OK"),OK)
# MALI_STATE_TRACKING is only supported on Linux kernels from version 2.6.32.
ccflags-y += -DMALI_STATE_TRACKING=1
else
ccflags-y += -DMALI_STATE_TRACKING=0
endif

ifeq ($(CONFIG_STM_MALI_DEBUG),y)
ccflags-y += -DDEBUG
endif

BUILD_DATE := $(shell LANG=C /bin/date "+%a %b %d %Y %H:%m %Z")

CFLAGS_malidrv_build_info.o := -DMALIDRV_SOC="KBUILD_STR($(SOC))"     \
		-DMALIDRV_KERNELDIR="KBUILD_STR($(KERNELDIR))" \
		-DMALIDRV_USING_MMU="KBUILD_STR($(USING_MMU))" \
		-DMALIDRV_USING_UMP="KBUILD_STR($(USING_UMP))" \
		-DMALIDRV_MMAP_CACHED_PAGES="KBUILD_STR($(MMAP_CACHED_PAGES))" \
		-DMALIDRV_USING_MALI200="KBUILD_STR($(USING_MALI200))" \
		-DMALIDRV_USING_MALI400="KBUILD_STR($(USING_MALI400))" \
		-DMALIDRV_USING_MALI400L2="KBUILD_STR($(USING_MALI400L2))" \
		-DMALIDRV_USING_MALIGP2="KBUILD_STR($(USING_MALIGP2))" \
		-DMALIDRV_API_VERSION="KBUILD_STR($(API_VERSION))" \
		-DMALIDRV_SVN_REV="KBUILD_STR($(SVN_REV))" \
		-DMALIDRV_BUILD_DATE="KBUILD_STR($(BUILD_DATE))"

obj-$(CONFIG_STM_MALI) += mali.o
mali-objs := $(MALI_OBJS)

# last line
