#
# Makefile for the STMicroelectronics clocks.
#

clock-y					:= clock-common.o
clock-$(CONFIG_CPU_SUBTYPE_FLI7510)	+= clock-fli7510.o
clock-$(CONFIG_CPU_SUBTYPE_STX5197)	+= clock-stx5197.o
clock-$(CONFIG_CPU_SUBTYPE_STX5206)	+= clock-stx5206.o
clock-$(CONFIG_CPU_SUBTYPE_STX7100)	+= clock-stx7100.o
clock-$(CONFIG_CPU_SUBTYPE_STX7105)	+= clock-stx7105.o
clock-$(CONFIG_CPU_SUBTYPE_STX7108)	+= clock-stx7108.o
clock-$(CONFIG_CPU_SUBTYPE_STX7111)	+= clock-stx7111.o
clock-$(CONFIG_CPU_SUBTYPE_STX7141)	+= clock-stx7141.o
clock-$(CONFIG_CPU_SUBTYPE_STX7200)	+= clock-stx7200.o

obj-y	+= $(clock-y)
