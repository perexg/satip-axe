CPUS=4
STLINUX=/opt/STM/STLinux-2.4
TOOLPATH=$(STLINUX)/host/bin
TOOLCHAIN=$(STLINUX)/devkit/sh4
TOOLCHAIN_KERNEL=$(shell pwd)/toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4

define GIT_CLONE
	@mkdir -p apps/
	git clone $(1) apps/$(2)
endef


#
# all
#

.PHONY: all
all: kernel-axe-modules kernel

#
# create CPIO
#

fs.cpio: minisatip
	fakeroot tools/do_min_fs.py \
	  -b "bash" \
	  -e "apps/minisatip/minisatip:sbin/minisatip"

.PHONY: fs-list
fs-list:
	cpio -itv < kernel/rootfs-idl4k.cpio

#
# kernel
#

kernel/.config: toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3
	make -C kernel -j $(CPUS) ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- idl4k_defconfig

.PHONY: kernel
kernel: toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3 kernel/.config fs.cpio
	mv fs.cpio kernel/rootfs-idl4k.cpio
	make -C kernel -j $(CPUS) ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- modules
	make -C kernel -j ${CPUS} PATH="$(PATH):$(TOOLPATH)" \
	                          ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- uImage.gz

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
# minisatip
#

apps/minisatip/minisatip.c:
	$(call GIT_CLONE,https://github.com/catalinii/minisatip.git,minisatip)

.PHONY: minisatip
minisatip: apps/minisatip/minisatip.c
	make -C apps/minisatip \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  CFLAGS="-O2 -DSYS_DVBT2=16"

#
# clean all
#

.PHONY: clean
clean: kernel-mrproper
	rm -rf firmware/initramfs
	rm -rf toolchain/4.5.3-99
