#
# Makefile for the Linux/SuperH SH-4 backends.
#

obj-y	:= probe.o common.o
common-y	+= entry.o

swsusp-y				:= $(addprefix ../sh3/, swsusp.o)
obj-$(CONFIG_HIBERNATION)		+= swsusp.o
obj-$(CONFIG_SH_FPU)			+= fpu.o softfloat.o
obj-$(CONFIG_SH_STORE_QUEUES)		+= sq.o

# CPU subtype setup
obj-$(CONFIG_CPU_SUBTYPE_SH7750)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7750R)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7750S)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7091)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751R)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7760)	+= setup-sh7760.o
obj-$(CONFIG_CPU_SUBTYPE_SH4_202)	+= setup-sh4-202.o
obj-$(CONFIG_CPU_SUBTYPE_FLI7510)	+= setup-fli7510.o
obj-$(CONFIG_CPU_SUBTYPE_ST40STB1)	+= setup-st40ra.o
obj-$(CONFIG_CPU_SUBTYPE_STX5197)	+= setup-stx5197.o
obj-$(CONFIG_CPU_SUBTYPE_STX5206)	+= setup-stx5206.o
obj-$(CONFIG_CPU_SUBTYPE_STX7100)	+= setup-stx7100.o
obj-$(CONFIG_CPU_SUBTYPE_STX7105)	+= setup-stx7105.o
obj-$(CONFIG_CPU_SUBTYPE_STX7108)	+= setup-stx7108.o
obj-$(CONFIG_CPU_SUBTYPE_STX7111)	+= setup-stx7111.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141)	+= setup-stx7141.o
obj-$(CONFIG_CPU_SUBTYPE_STX7200)	+= setup-stx7200.o

# Primary on-chip clocks (common)
ifndef CONFIG_CPU_SH4A
clock-$(CONFIG_CPU_SH4)			:= clock-sh4.o
clock-$(CONFIG_CPU_SUBTYPE_FLI7510)	:= clock-fli7510.o
clock-$(CONFIG_CPU_SUBTYPE_STX5197)	:= clock-stx5197.o
clock-$(CONFIG_CPU_SUBTYPE_STX5206)	:= clock-stx5206.o
clock-$(CONFIG_CPU_SUBTYPE_STX7100)	:= clock-stx7100.o
clock-$(CONFIG_CPU_SUBTYPE_STX7105)	:= clock-stx7105.o
clock-$(CONFIG_CPU_SUBTYPE_STX7108)	:= clock-stx7108.o
clock-$(CONFIG_CPU_SUBTYPE_STX7111)	:= clock-stx7111.o
clock-$(CONFIG_CPU_SUBTYPE_STX7141)	:= clock-stx7141.o
clock-$(CONFIG_CPU_SUBTYPE_STX7200)	:= clock-stx7200.o
endif

# Additional clocks by subtype
clock-$(CONFIG_CPU_SUBTYPE_SH4_202)	+= clock-sh4-202.o

obj-y	+= $(clock-y)

obj-$(CONFIG_CPU_SUBTYPE_ST40)		+= stm-tmu.o

cpufreq-y				:= cpufreq-stm.o
cpufreq-$(CONFIG_CPU_SUBTYPE_STX5197)	+= cpufreq-stx5197.o
cpufreq-$(CONFIG_CPU_SUBTYPE_STX5206)	+= cpufreq-stx5206.o
cpufreq-$(CONFIG_CPU_SUBTYPE_STX7105)	+= cpufreq-stx7105.o
cpufreq-$(CONFIG_CPU_SUBTYPE_STX7108)	+= cpufreq-stx7108.o
cpufreq-$(CONFIG_CPU_SUBTYPE_STX7111)	+= cpufreq-stx7111.o
cpufreq-$(CONFIG_CPU_SUBTYPE_STX7141)	+= cpufreq-stx7141.o
cpufreq-$(CONFIG_CPU_SUBTYPE_STX7200)	+= cpufreq-stx7200.o
obj-$(CONFIG_STM_CPU_FREQ)		+= $(cpufreq-y)

suspend-y				:= stm_suspend.o stm_suspend-core.o
suspend-$(CONFIG_CPU_SUBTYPE_STX5197)	+= suspend-stx5197.o
suspend-$(CONFIG_CPU_SUBTYPE_STX5206)	+= suspend-stx5206.o
suspend-$(CONFIG_CPU_SUBTYPE_STX7100)	+= suspend-stx7100.o
suspend-$(CONFIG_CPU_SUBTYPE_STX7105)	+= suspend-stx7105.o
suspend-$(CONFIG_CPU_SUBTYPE_STX7108)	+= suspend-stx7108.o
suspend-$(CONFIG_CPU_SUBTYPE_STX7111)	+= suspend-stx7111.o
suspend-$(CONFIG_CPU_SUBTYPE_STX7141)	+= suspend-stx7141.o
suspend-$(CONFIG_CPU_SUBTYPE_STX7200)	+= suspend-stx7200.o
suspend-$(CONFIG_CPU_SUBTYPE_FLI7510)	+= suspend-fli7510.o
obj-$(CONFIG_SUSPEND)			+= $(suspend-y)

hom-y					:= stm_hom.o stm_hom-core.o
hom-$(CONFIG_CPU_SUBTYPE_STX7108)	+= hom-stx7108.o
obj-$(CONFIG_HIBERNATION_ON_MEMORY)	+= $(hom-y)
