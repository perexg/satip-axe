BUILD=4
VERSION=$(shell date +%Y%m%d%H%M)-$(BUILD)
CPUS=4
STLINUX=/opt/STM/STLinux-2.4
TOOLPATH=$(STLINUX)/host/bin
TOOLCHAIN=$(STLINUX)/devkit/sh4
TOOLCHAIN_KERNEL=$(shell pwd)/toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4
HOST_ARCH=$(shell uname -m)

EXTRA_AXE_MODULES_DIR=firmware/initramfs/root/modules_idl4k_7108_ST40HOST_LINUX_32BITS
EXTRA_AXE_MODULES=axe_dmx.ko axe_dmxts.ko axe_fe.ko axe_fp.ko axe_i2c.ko \
                  stapi_core_stripped.ko stapi_ioctl_stripped.ko stsys_ioctl.ko \
                  load_modules_list_32BITS.txt load_modules_list_axe_32BITS.txt \
                  load_modules.sh load_env.sh

ORIG_FILES=main_axe.out mknodes.out

BUSYBOX=busybox-1.23.2

DROPBEAR=dropbear-2015.67
DROPBEAR_SBIN_FILES=dropbear
DROPBEAR_BIN_FILES=dbclient dropbearconvert dropbearkey scp

define GIT_CLONE
	@mkdir -p apps/
	git clone $(1) apps/$(2)
endef

define WGET
	@mkdir -p apps/
	wget --no-verbose -O $(2) $(1)
endef

#
# all
#

.PHONY: all
all: kernel-axe-modules kernel

.PHONY: release
release: kernel-axe-modules out/idl4k.scr out/idl4k.recovery
	-ls -la out

.PHONY: dist
dist:
	-mkdir -p dist
	cp out/*.fw out/*.usb out/*.flash dist

#
# create CPIO
#

fs.cpio: minisatip
	fakeroot tools/do_min_fs.py \
	  -r "$(VERSION)" \
	  -b "bash strace" \
	  $(foreach m,$(EXTRA_AXE_MODULES), -e "$(EXTRA_AXE_MODULES_DIR)/$(m):lib/modules/$(m)") \
	  $(foreach m,$(ORIG_FILES), -e "$(EXTRA_AXE_MODULES_DIR)/../$(m):root") \
	  -e "apps/$(BUSYBOX)/busybox:bin/busybox" \
	  $(foreach f,$(DROPBEAR_SBIN_FILES), -e "apps/$(DROPBEAR)/$(f):sbin/$(f)") \
	  $(foreach f,$(DROPBEAR_BIN_FILES), -e "apps/$(DROPBEAR)/$(f):usr/bin/$(f)") \
	  -e "apps/minisatip/minisatip:sbin/minisatip" \
	  -e "apps/minisatip/icons/lr.jpg:usr/share/minisatip/icons/lr.jpg" \
	  -e "apps/minisatip/icons/lr.png:usr/share/minisatip/icons/lr.png" \
	  -e "apps/minisatip/icons/sm.jpg:usr/share/minisatip/icons/sm.jpg" \
	  -e "apps/minisatip/icons/sm.png:usr/share/minisatip/icons/sm.png"

.PHONY: fs-list
fs-list:
	cpio -itv < kernel/rootfs-idl4k.cpio

#
# uboot
#

out/idl4k.recovery: patches/uboot-recovery.script
	$(TOOLPATH)/mkimage -T script -C none \
	  -n 'Restore original idl4k fw' \
	  -d patches/uboot-recovery.script out/idl4k.recovery

out/idl4k.scr: patches/uboot.script patches/uboot-flash.script out/satip-axe-$(VERSION).fw
	rm -f out/*.scr out/*.usb out/*.flash out/*.recovery
	sed -e 's/@VERSION@/$(VERSION)/g' \
	  < patches/uboot.script > out/uboot.script
	sed -e 's/@VERSION@/$(VERSION)/g' \
	  < patches/uboot-flash.script > out/uboot-flash.script
	$(TOOLPATH)/mkimage -T script -C none \
	  -n 'SAT>IP AXE fw v$(VERSION)' \
	  -d out/uboot.script out/satip-axe-$(VERSION).usb
	$(TOOLPATH)/mkimage -T script -C none \
	  -n 'SAT>IP AXE fw v$(VERSION)' \
	  -d out/uboot-flash.script out/satip-axe-$(VERSION).flash
	ln -sf satip-axe-$(VERSION).usb out/idl4k.scr
	rm out/uboot.script out/uboot-flash.script

out/satip-axe-$(VERSION).fw: kernel/arch/sh/boot/uImage.gz
	mkdir -p out
	rm -f out/*.fw
	cp -av kernel/arch/sh/boot/uImage.gz out/satip-axe-$(VERSION).fw

#
# kernel
#

kernel/.config: toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3
	make -C kernel -j $(CPUS) ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- idl4k_defconfig

kernel/arch/sh/boot/uImage.gz: toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3 kernel/.config fs.cpio
	mv fs.cpio kernel/rootfs-idl4k.cpio
	make -C kernel -j $(CPUS) ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- modules
	make -C kernel -j ${CPUS} PATH="$(PATH):$(TOOLPATH)" \
	                          ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- uImage.gz

.PHONY: kernel
kernel: kernel/arch/sh/boot/uImage.gz

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
# syscall dump
#

tools/syscall-dump.so: tools/syscall-dump.c
	$(TOOLCHAIN)/bin/sh4-linux-gcc -o tools/syscall-dump.o -c -fPIC -Wall tools/syscall-dump.c
	$(TOOLCHAIN)/bin/sh4-linux-gcc -o tools/syscall-dump.so -shared -rdynamic tools/syscall-dump.o -ldl

tools/syscall-dump.so.$(HOST_ARCH): tools/syscall-dump.c
	gcc -o tools/syscall-dump.o.$(HOST_ARCH) -c -fPIC -Wall tools/syscall-dump.c
	gcc -o tools/syscall-dump.so.$(HOST_ARCH) -shared -rdynamic tools/syscall-dump.o.$(HOST_ARCH) -ldl

.PHONY: s2i_dump
s2i_dump: tools/syscall-dump.so
	if test -z "$(SATIP_HOST)"; then echo "Define SATIP_HOST variable"; exit 1; fi
	scp tools/syscall-dump.so \
	    tools/s2i-dump.sh \
	    firmware/initramfs/root/s2i.bin \
	    firmware/initramfs/usr/lib/libuuid.so.1 \
	    firmware/initramfs/usr/lib/libcurl.so.4 \
	    firmware/initramfs/usr/lib/liboauth.so.0 \
	    firmware/initramfs/usr/lib/libsoup-2.4.so.1 \
	    firmware/initramfs/usr/lib/libgio-2.0.so.0 \
	    firmware/initramfs/usr/lib/libgobject-2.0.so.0 \
	    firmware/initramfs/usr/lib/libgmodule-2.0.so.0 \
	    firmware/initramfs/usr/lib/libgthread-2.0.so.0 \
	    firmware/initramfs/usr/lib/libssl.so.1.0.0 \
	    firmware/initramfs/usr/lib/libcrypto.so.1.0.0 \
	    firmware/initramfs/usr/lib/libxml2.so.2 \
	    root@$(SATIP_HOST):/usr/lib
	scp firmware/initramfs/root/default_sl.json \
	    firmware/initramfs/root/*.txt \
	    firmware/initramfs/root/*.m3u \
	    root@$(SATIP_HOST):/root
	scp firmware/initramfs/usr/local/bin/mdnsd root@$(SATIP_HOST):/usr/bin
	scp $(TOOLCHAIN)/target/bin/netstat root@$(SATIP_HOST):/bin

#
# minisatip
#

apps/minisatip/axe.h:
	$(call GIT_CLONE,https://github.com/catalinii/minisatip.git,minisatip)
	cd apps/minisatip; patch -p1 < ../../patches/minisatip-axe.patch

apps/minisatip/minisatip: apps/minisatip/axe.h
	make -C apps/minisatip \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  CFLAGS="-O2 -DAXE=1 -DSYS_DVBT2=16"

.PHONY: minisatip
minisatip: apps/minisatip/minisatip

.PHONY: minisatip-clean
minisatip-clean:
	rm -rf apps/minisatip

#
# busybox
#

apps/$(BUSYBOX)/Makefile:
	$(call WGET,http://busybox.net/downloads/$(BUSYBOX).tar.bz2,apps/$(BUSYBOX).tar.bz2)
	tar -C apps -xjf apps/$(BUSYBOX).tar.bz2

apps/$(BUSYBOX)/busybox: apps/$(BUSYBOX)/Makefile
	make -C apps/$(BUSYBOX) CROSS_COMPILE=$(TOOLCHAIN)/bin/sh4-linux- defconfig
	make -C apps/$(BUSYBOX) CROSS_COMPILE=$(TOOLCHAIN)/bin/sh4-linux-
	#make -C apps/$(DROPBEAR) PROGRAMS="dropbear dbclient dropbearkey dropbearconvert scp"

.PHONY: busybox
busybox: apps/$(BUSYBOX)/busybox

#
# dropbear
#

apps/$(DROPBEAR)/configure:
	$(call WGET,https://matt.ucc.asn.au/dropbear/$(DROPBEAR).tar.bz2,apps/$(DROPBEAR).tar.bz2)
	tar -C apps -xjf apps/$(DROPBEAR).tar.bz2

apps/$(DROPBEAR)/dropbear: apps/$(DROPBEAR)/configure
	cd apps/$(DROPBEAR) && \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	./configure \
	  --host=sh4-linux \
	  --prefix=/ \
          --disable-lastlog \
          --disable-utmp \
          --disable-utmpx \
          --disable-wtmp \
          --disable-wtmpx
	sed -e 's/DEFAULT_PATH \"\/usr\/bin:\/bin\"/DEFAULT_PATH \"\/sbin:\/usr\/sbin:\/bin:\/usr\/bin:\/usr\/local\/bin\"/g' \
	  < apps/$(DROPBEAR)/options.h > apps/$(DROPBEAR)/options.h.2
	mv apps/$(DROPBEAR)/options.h.2 apps/$(DROPBEAR)/options.h
	make -C apps/$(DROPBEAR) PROGRAMS="dropbear dbclient dropbearkey dropbearconvert scp"

.PHONY: dropbear
dropbear: apps/$(DROPBEAR)/dropbear

#
# clean all
#

.PHONY: clean
clean: kernel-mrproper
	rm -rf firmware/initramfs
	rm -rf toolchain/4.5.3-99
	rm -rf tools/syscall-dump.o* tools/syscall-dump.s*
