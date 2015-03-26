#
# Makefile for the STMicroelectronics device drivers.
#

obj-y					+= emi.o
obj-y					+= gpio.o
obj-y					+= pad.o
obj-y					+= sysconf.o
obj-y					+= device.o
obj-y					+= clk.o
obj-y					+= clocks/

obj-$(CONFIG_STM_DMA)			+= fdma.o fdma-xbar.o
obj-$(CONFIG_STM_MIPHY)			+= miphy.o
obj-$(CONFIG_STM_MIPHY_TAP)		+= miphy_tap.o tap.o
obj-$(CONFIG_STM_MIPHY_PCIE_MP)		+= miphy_pcie_mp.o
obj-$(CONFIG_STM_MIPHY_DUMMY)		+= miphy_dummy.o
obj-$(CONFIG_STM_COPROCESSOR_SUPPORT)	+= copro-st_socs.o
obj-$(CONFIG_STM_COPROCESSOR)		+= stm-coprocessor.o
obj-$(CONFIG_STM_COPROCESSOR_FRMW)	+= stm-coprocessor_frmw.o
obj-$(CONFIG_STM_RNG)			+= rng.o
obj-$(CONFIG_STM_MALI)			+= mali/

obj-$(CONFIG_PM)			+= wakeup_devices.o
obj-$(CONFIG_PM)			+= pm_notify.o
obj-$(CONFIG_PMS)			+= pms.o 
obj-$(CONFIG_PM_RUNTIME)		+= pm_runtime.o
obj-$(CONFIG_STM_LPC)			+= lpc.o

ifdef CONFIG_STM_LPM
obj-$(CONFIG_STM_LPM)                  += lpm.o
endif

obj-$(CONFIG_STM_PCI_EMISS)		+= pci-emiss.o
obj-$(CONFIG_STM_PCIE)			+= pcie.o

obj-$(CONFIG_CPU_SUBTYPE_FLI7510)	+= fli7510.o fli7510_clock.o fli7510_audio.o fli7510_comms.o fli7510_hispeed.o fli7510_pci.o
obj-$(CONFIG_CPU_SUBTYPE_STX5197)	+= stx5197.o stx5197_clock.o
obj-$(CONFIG_CPU_SUBTYPE_STX5206)	+= stx5206.o stx5206_clock.o stx5206_comms.o stx5206_hispeed.o stx5206_pci.o
obj-$(CONFIG_CPU_SUBTYPE_STX7100)	+= stx7100.o stx7100_clock.o stx7100_audio.o stx7100_comms.o stx7100_hispeed.o
obj-$(CONFIG_CPU_SUBTYPE_STX7105)	+= stx7105.o stx7105_clock.o stx7105_audio.o stx7105_comms.o stx7105_hispeed.o stx7105_pci.o
obj-$(CONFIG_CPU_SUBTYPE_STX7108)	+= stx7108.o stx7108_clock.o stx7108_audio.o stx7108_comms.o stx7108_hispeed.o stx7108_pci.o
obj-$(CONFIG_CPU_SUBTYPE_STX7111)	+= stx7111.o stx7111_clock.o stx7111_audio.o stx7111_comms.o stx7111_hispeed.o stx7111_pci.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141)	+= stx7141.o stx7141_clock.o stx7141_audio.o stx7141_comms.o stx7141_hispeed.o
obj-$(CONFIG_CPU_SUBTYPE_STX7200)	+= stx7200.o stx7200_clock.o stx7200_audio.o stx7200_comms.o stx7200_hispeed.o
