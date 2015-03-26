CPUS=4
TOOLCHAIN_KERNEL=$(shell pwd)/toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4

#
# kernel
#

kernel/.config: toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3
	make -C kernel -j $(CPUS) ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- idl4k_defconfig

.PHONY: kernel
kernel: toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3 kernel/.config
	make -C kernel -j $(CPUS) ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- modules
	make -C kernel -j ${CPUS} ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- vmlinux

.PHONY: kernel-mrproper
kernel-mrproper:
	make -C kernel ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- mrproper

define RPM_UNPACK
	@mkdir -p $(1)
	cd $(1) ; rpm2cpio ../$(2) | cpio -idv
endef

toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3:
	$(call RPM_UNPACK,toolchain/4.5.3-99,stlinux24-cross-sh4-binutils-2.24.51.0.3-76.i386.rpm)
	$(call RPM_UNPACK,toolchain/4.5.3-99,stlinux24-cross-sh4-cpp-4.5.3-99.i386.rpm)
	$(call RPM_UNPACK,toolchain/4.5.3-99,stlinux24-cross-sh4-gcc-4.5.3-99.i386.rpm)

#
# extract kernel modules from firmware
#

.PHONY: kernel-axe-modules
kernel-axe-modules: firmware/initramfs/root/modules_idl4k_7108_ST40HOST_LINUX_32BITS/axe_dmx.ko

firmware/initramfs/root/modules_idl4k_7108_ST40HOST_LINUX_32BITS/axe_dmx.ko:
	cd firmware ; ../tools/cpio-idl4k-bin.sh extract
	chmod -R u+rw firmware/initramfs

#
# clean all
#
.PHONY: clean
clean: kernel-mrproper
	rm -rf firmware/initramfs
	rm -rf toolchain/4.5.3-99
