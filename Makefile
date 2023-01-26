BUILD=16
VERSION=$(shell date +%Y%m%d%H%M)-$(BUILD)
CPUS=4
CURDIR=$(shell pwd)
STLINUX=/opt/STM/STLinux-2.4
TOOLPATH=$(STLINUX)/host/bin
TOOLCHAIN=$(STLINUX)/devkit/sh4
TOOLCHAIN_KERNEL=$(CURDIR)/toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4
HOST_ARCH=$(shell uname -m)

EXTRA_AXE_MODULES_DIR=firmware/initramfs/root/modules_idl4k_7108_ST40HOST_LINUX_32BITS
EXTRA_AXE_MODULES=axe_dmx.ko axe_dmxts.ko axe_fp.ko axe_i2c.ko \
                  stapi_core_stripped.ko stapi_ioctl_stripped.ko stsys_ioctl.ko

EXTRA_AXE_LIBS_DIR=firmware/initramfs/usr/local/lib
EXTRA_AXE_LIBS=libboost_date_time.so libboost_date_time.so.1.53.0 \
               libboost_thread.so libboost_thread.so.1.53.0 \
               libboost_filesystem.so libboost_filesystem.so.1.53.0 \
               libboost_serialization.so libboost_serialization.so.1.53.0 \
               libboost_system.so libboost_system.so.1.53.0

ORIG_FILES=main_axe.out

KMODULES = drivers/usb/serial/cp210x.ko \
	   drivers/usb/serial/pl2303.ko \
	   drivers/usb/serial/spcp8x5.ko \
	   drivers/usb/serial/io_ti.ko \
	   drivers/usb/serial/ti_usb_3410_5052.ko \
	   drivers/usb/serial/io_edgeport.ko \
           drivers/usb/serial/ftdi_sio.ko \
	   drivers/usb/serial/oti6858.ko

MINISATIP_COMMIT=679b7cafc3d1dc9f710da91ed71a39b96cee7a8d
MINISATIP_VERSION=1.0.2-axe2
MINISATIP5_COMMIT=67e88c2d743d6df9c4a96aad772414169f61b764
MINISATIP7_COMMIT=d22ba0dfe3c706c3ab6ad86486d8a9e913080f7e
MINISATIP8_COMMIT=8e2362435cc8c5e0babc3e7ca67570c7f7dd03c5

BUSYBOX=busybox-1.26.2

DROPBEAR=dropbear-2022.83
DROPBEAR_SBIN_FILES=dropbear
DROPBEAR_BIN_FILES=dbclient dropbearconvert dropbearkey scp

OPENSSH=openssh-9.1p1

ETHTOOL=ethtool-3.18

MTD_UTILS_COMMIT=9f107132a6a073cce37434ca9cda6917dd8d866b # v1.5.1

LIBTIRPC_VERSION=0.2.5
LIBTIRPC=libtirpc-$(LIBTIRPC_VERSION)

RPCBIND_VERSION=0.2.3
RPCBIND=rpcbind-$(RPCBIND_VERSION)
RPCBIND_SBIN_FILES=rpcbind rpcinfo

NFSUTILS_VERSION=1.3.4
NFSUTILS=nfs-utils-$(NFSUTILS_VERSION)
NFSUTILS_SBIN_FILES=utils/showmount/showmount \
		    utils/exportfs/exportfs \
		    utils/nfsstat/nfsstat \
		    utils/mountd/mountd \
		    utils/statd/start-statd \
		    utils/statd/sm-notify \
		    utils/statd/statd \
		    utils/nfsd/nfsd

NANO_VERSION=2.8.1
NANO=nano-$(NANO_VERSION)
NANO_FILENAME=$(NANO).tar.gz
NANO_DOWNLOAD=http://www.nano-editor.org/dist/v2.8/$(NANO_FILENAME)

PYTHON3_VERSION0=3.5
PYTHON3_VERSION=$(PYTHON3_VERSION0).6
PYTHON3=Python-$(PYTHON3_VERSION)
PYTHON3_PACKAGE_NAME=$(PYTHON3)-1
PYTHON3_FILENAME=$(PYTHON3).tgz
PYTHON3_DOWNLOAD=https://www.python.org/ftp/python/$(PYTHON3_VERSION)/$(PYTHON3_FILENAME)

MULTICAST_RTP_PACKAGE_NAME=multicast-rtp-2
SENDDSQ_PACKAGE_NAME=senddsq

TVHEADEND_COMMIT=master

# 10663 10937 11234 11398
OSCAM_REV=11693

BINUTILS=binutils-2.39
BINUTILS_BIN_FILES=addr2line

define GIT_CLONE
	@mkdir -p apps/host
	git clone $(1) apps/$(2)
	cd apps/$(2) && git checkout -b axe $(3)
endef

define WGET
	@mkdir -p apps/host
	wget --no-verbose --no-check-certificate -O $(2) $(1)
endef

define PACKAGE
	-mkdir -p out/packages
	tar cvz -C $(1) -f out/packages/$(2).tar.gz $(3)
endef

#
# all
#

.PHONY: all
all: kernel-axe-modules kernel

.PHONY: release
release: kernel-axe-modules out/idl4k.scr out/idl4k.cfgreset out/idl4k.cfgresetusb out/idl4k.recovery
	-ls -la out

.PHONY: dist
dist:
	-mkdir -p dist
	cp out/*.fw out/*.usb out/*.flash dist

#
# create CPIO
#

CPIO_SRCS  = kernel-modules
CPIO_SRCS += busybox
CPIO_SRCS += dropbear
CPIO_SRCS += openssh
CPIO_SRCS += ethtool
CPIO_SRCS += minisatip
CPIO_SRCS += minisatip7
CPIO_SRCS += minisatip8
CPIO_SRCS += oscam
CPIO_SRCS += tools/axehelper
CPIO_SRCS += nfsutils
CPIO_SRCS += nano
CPIO_SRCS += mtd-utils
CPIO_SRCS += binutils

fs.cpio: $(CPIO_SRCS)
	fakeroot tools/do_min_fs.py \
	  -r "$(VERSION)" \
	  -b "bash strace openssl" \
	  -d "fs-add" \
	  $(foreach m,$(EXTRA_AXE_MODULES), -e "$(EXTRA_AXE_MODULES_DIR)/$(m):lib/modules/axe/$(m)") \
	  -e "patches/axe_dmxts_std.ko:lib/modules/axe/axe_dmxts_std.ko" \
	  -e "patches/axe_fe_157.ko:lib/modules/axe/axe_fe.ko" \
	  $(foreach m,$(ORIG_FILES), -e "$(EXTRA_AXE_MODULES_DIR)/../$(m):lib/modules/axe/$(m)") \
	  $(foreach m,$(EXTRA_AXE_LIBS), -e "$(EXTRA_AXE_LIBS_DIR)/$(m):lib/$(m)") \
	  -e "tools/i2c_mangle.ko:lib/modules/axe/i2c_mangle.ko" \
	  $(foreach m,$(KMODULES), -e "kernel/$(m):lib/modules/$(m)") \
	  -e "tools/axehelper:sbin/axehelper" \
	  -e "apps/$(BUSYBOX)/busybox:bin/busybox" \
	  $(foreach f,$(DROPBEAR_SBIN_FILES), -e "apps/$(DROPBEAR)/$(f):sbin/$(f)") \
	  $(foreach f,$(DROPBEAR_BIN_FILES), -e "apps/$(DROPBEAR)/$(f):usr/bin/$(f)") \
	  -e "apps/$(OPENSSH)/sftp-server:usr/libexec/sftp-server" \
	  -e "apps/$(ETHTOOL)/ethtool:sbin/ethtool" \
	  $(foreach f,$(RPCBIND_SBIN_FILES), -e "apps/$(RPCBIND)/$(f):usr/sbin/$(f)") \
	  $(foreach f,$(NFSUTILS_SBIN_FILES), -e "apps/$(NFSUTILS)/$(f):usr/sbin/$(notdir $(f))") \
	  -e "apps/minisatip/minisatip:sbin/minisatip" \
	  $(foreach f,$(notdir $(wildcard apps/minisatip/html/*)), -e "apps/minisatip/html/$f:usr/share/minisatip/html/$f") \
	  -e "apps/minisatip7/minisatip:sbin/minisatip7" \
	  $(foreach f,$(notdir $(wildcard apps/minisatip7/html/*)), -e "apps/minisatip7/html/$f:usr/share/minisatip7/html/$f") \
	  -e "apps/minisatip8/minisatip:sbin/minisatip8" \
	  $(foreach f,$(notdir $(wildcard apps/minisatip8/html/*)), -e "apps/minisatip8/html/$f:usr/share/minisatip8/html/$f") \
	  -e "apps/$(NANO)/src/nano:usr/bin/nano" \
	  -e "apps/mtd-utils/nandwrite:usr/sbin/nandwrite2" \
	  -e "apps/oscam-svn/Distribution/oscam-1.20_svn$(OSCAM_REV)-sh4-linux:sbin/oscamd" \
	  $(foreach f,$(BINUTILS_BIN_FILES), -e "apps/$(BINUTILS)/binutils/$(f):usr/bin/$(f)")

.PHONY: fs-list
fs-list:
	cpio -itv < kernel/rootfs-idl4k.cpio

#
# uboot
#

out/idl4k.cfgreset: patches/uboot-cfgreset.script
	$(TOOLPATH)/mkimage -T script -C none \
	  -n 'Reset satip-axe fw configuration' \
	  -d patches/uboot-cfgreset.script out/idl4k.cfgreset

out/idl4k.cfgresetusb: patches/uboot-cfgresetusb.script
	$(TOOLPATH)/mkimage -T script -C none \
	  -n 'Reset satip-axe fw configuration (USB)' \
	  -d patches/uboot-cfgresetusb.script out/idl4k.cfgresetusb

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

kernel/.config: patches/kernel.config toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3
	cp patches/kernel.config ./kernel/arch/sh/configs/idl4k_defconfig
	make -C kernel -j $(CPUS) ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- idl4k_defconfig

kernel/drivers/usb/serial/cp210x.ko: toolchain/4.5.3-99/opt/STM/STLinux-2.4/devkit/sh4/bin/sh4-linux-gcc-4.5.3 kernel/.config
	make -C kernel -j $(CPUS) ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- modules

kernel/arch/sh/boot/uImage.gz: kernel/drivers/usb/serial/cp210x.ko fs.cpio
	mv fs.cpio kernel/rootfs-idl4k.cpio
	make -C kernel -j ${CPUS} PATH="$(PATH):$(TOOLPATH)" \
	                          ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- uImage.gz

tools/i2c_mangle.ko: tools/i2c_mangle.c
	make -C tools ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- all

.PHONY: kernel-modules tools/i2c_mangle.ko
kernel-modules: kernel/drivers/usb/serial/cp210x.ko

.PHONY: kernel
kernel: kernel/arch/sh/boot/uImage.gz

.PHONY: kernel-mrproper
kernel-mrproper:
	make -C kernel -j ${CPUS} ARCH=sh CROSS_COMPILE=$(TOOLCHAIN_KERNEL)/bin/sh4-linux- mrproper

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

tools/axehelper: tools/axehelper.c
	$(TOOLCHAIN)/bin/sh4-linux-gcc -o tools/axehelper -Wall -lrt tools/axehelper.c

tools/axehelper.$(HOST_ARCH): tools/axehelper.c
	gcc -o tools/axehelper.$(HOST_ARCH) -Wall -lrt tools/axehelper.c

tools/syscall-dump.so: tools/syscall-dump.c
	$(TOOLCHAIN)/bin/sh4-linux-gcc -o tools/syscall-dump.o -c -fPIC -Wall tools/syscall-dump.c
	$(TOOLCHAIN)/bin/sh4-linux-gcc -o tools/syscall-dump.so -shared -rdynamic tools/syscall-dump.o -ldl

tools/syscall-dump.so.$(HOST_ARCH): tools/syscall-dump.c
	gcc -o tools/syscall-dump.o.$(HOST_ARCH) -c -fPIC -Wall tools/syscall-dump.c
	gcc -o tools/syscall-dump.so.$(HOST_ARCH) -shared -rdynamic tools/syscall-dump.o.$(HOST_ARCH) -ldl

.PHONY: s2i_dump
s2i_dump: tools/syscall-dump.so
	if test -z "$(SATIP_HOST)"; then echo "Define SATIP_HOST variable"; exit 1; fi
	cd firmware/initramfs && tar cvzf ../fw.tgz --owner=root --group=root *
	scp tools/syscall-dump.so tools/s2i-dump.sh firmware/fw.tgz \
	    root@$(SATIP_HOST):/root

#
# media_build
#

apps/media/build:
	$(call GIT_CLONE,git://linuxtv.org/media_build.git,media,master)
	$(call WGET,http://www.linuxtv.org/downloads/firmware/dvb-firmwares.tar.bz2,apps/media/dvb-firmwares.tar.bz2)
	make -C apps/media download untar

apps/media/v4l/dib0070.h: apps/media/build
	make -C apps/media SRCDIR=$(CURDIR)/kernel VER=2.6.32 allyesconfig
	make -C apps/media SRCDIR=$(CURDIR) VER=2.6.32

.PHONY: media
media: apps/media/v4l/dib0070.h

.PHONY: media-clean
media-clean:
	rm -rf apps/media

#
# minisatip 1.0+
#

apps/minisatip/src/axe.h: patches/minisatip-axe.patch
	rm -rf apps/minisatip
	$(call GIT_CLONE,https://github.com/catalinii/minisatip.git,minisatip,$(MINISATIP_COMMIT))
	cd apps/minisatip; patch -p1 < ../../patches/minisatip-axe.patch

apps/minisatip/minisatip: apps/minisatip/src/axe.h
	cd apps/minisatip && ./configure \
		--enable-axe \
		--disable-dvbca \
		--disable-dvbapi \
		--disable-dvbcsa \
		--disable-dvbaes \
		--disable-netcv
	make -C apps/minisatip \
	  VERSION=$(MINISATIP_VERSION) \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  EXTRA_CFLAGS="-O2 -I$(CURDIR)/kernel/include -O3 -m4-300 -funroll-loops -fomit-frame-pointer -fno-tree-vectorize -funsafe-loop-optimizations -funsafe-math-optimizations"

.PHONY: minisatip
minisatip: apps/minisatip/minisatip

.PHONY: minisatip-clean
minisatip-clean:
	rm -rf apps/minisatip

#
# minisatip7
#

apps/minisatip7/axe.h: patches/minisatip7-axe.patch
	rm -rf apps/minisatip7
	$(call GIT_CLONE,https://github.com/perexg/minisatip.git,minisatip7,$(MINISATIP7_COMMIT))
	cd apps/minisatip7; patch -p1 < ../../patches/minisatip7-axe.patch

apps/minisatip7/minisatip: apps/minisatip7/axe.h
	cd apps/minisatip7 && ./configure \
		--enable-axe \
		--disable-dvbca \
		--disable-dvbapi \
		--disable-dvbcsa \
		--disable-dvbaes \
		--disable-netceiver
	make -C apps/minisatip7 \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  EXTRA_CFLAGS="-O2 -I$(CURDIR)/kernel/include"

.PHONY: minisatip7
minisatip7: apps/minisatip7/minisatip

.PHONY: minisatip7-clean
minisatip7-clean:
	rm -rf apps/minisatip7

#
# minisatip8
#

apps/minisatip8/src/axe.h: patches/minisatip8-axe.patch
	rm -rf apps/minisatip8
	$(call GIT_CLONE,https://github.com/catalinii/minisatip.git,minisatip8,$(MINISATIP8_COMMIT))
	cd apps/minisatip8; patch -p1 < ../../patches/minisatip8-axe.patch

apps/minisatip8/minisatip: apps/minisatip8/src/axe.h
	cd apps/minisatip8 && ./configure \
		--enable-axe \
		--disable-dvbca \
		--disable-dvbapi \
		--disable-dvbcsa \
		--disable-dvbaes \
		--disable-netceiver
	make -C apps/minisatip8 \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  EXTRA_CFLAGS="-O2 -I$(CURDIR)/kernel/include"

.PHONY: minisatip8
minisatip8: apps/minisatip8/minisatip

.PHONY: minisatip8-clean
minisatip8-clean:
	rm -rf apps/minisatip8

#
# minisatip package
#

dist/packages/minisatip-$(VERSION).tar.gz: minisatip minisatip7 minisatip8
	rm -rf fs/usr/share/minisatip
	mkdir -p fs/usr/share/minisatip/icons/ \
		 fs/usr/share/minisatip/html/ \
		 fs/usr/share/minisatip7/html/ \
		 fs/usr/share/minisatip8/html/
	install -m 755 apps/minisatip/minisatip fs/sbin/minisatip
	install -m 644 apps/minisatip/html/* fs/usr/share/minisatip/html/
	install -m 755 apps/minisatip7/minisatip fs/sbin/minisatip7
	install -m 644 apps/minisatip7/html/* fs/usr/share/minisatip7/html/
	install -m 755 apps/minisatip8/minisatip fs/sbin/minisatip8
	install -m 644 apps/minisatip8/html/* fs/usr/share/minisatip7/html8
	tar cvz -C fs -f dist/packages/minisatip-$(VERSION).tar.gz \
		sbin/minisatip \
		sbin/minisatip7 \
		sbin/minisatip8 \
		usr/share/minisatip/html \
		usr/share/minisatip7/html \
		usr/share/minisatip8/html
	ls -la dist/packages/minisatip*

.PHONY: minisatip-package
minisatip-package: dist/packages/minisatip-$(VERSION).tar.gz

#
# busybox
#

apps/$(BUSYBOX)/Makefile:
	$(call WGET,http://busybox.net/downloads/$(BUSYBOX).tar.bz2,apps/$(BUSYBOX).tar.bz2)
	tar -C apps -xjf apps/$(BUSYBOX).tar.bz2
	patch -p0 < patches/busybox-1.26.2.patch

apps/$(BUSYBOX)/busybox: apps/$(BUSYBOX)/Makefile
	make -C apps/$(BUSYBOX) CROSS_COMPILE=$(TOOLCHAIN)/bin/sh4-linux- defconfig
	make -C apps/$(BUSYBOX) CROSS_COMPILE=$(TOOLCHAIN)/bin/sh4-linux-

.PHONY: busybox
busybox: apps/$(BUSYBOX)/busybox

#
# dropbear
#

apps/$(DROPBEAR)/configure:
	$(call WGET,https://matt.ucc.asn.au/dropbear/releases/$(DROPBEAR).tar.bz2,apps/$(DROPBEAR).tar.bz2)
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
# openssh (for sftp-server)
#

apps/$(OPENSSH)/configure:
	$(call WGET,https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/${OPENSSH}.tar.gz,apps/${OPENSSH}.tar.gz)
	tar -C apps -xzf apps/$(OPENSSH).tar.gz

apps/$(OPENSSH)/sftp-server: apps/$(OPENSSH)/configure
	cd apps/$(OPENSSH) && \
		CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	./configure \
		--host=sh4-linux \
		--prefix=/
	make -C apps/$(OPENSSH) -j $(CPUS) sftp-server

.PHONY: openssh
openssh: apps/$(OPENSSH)/sftp-server

#
# ethtool
#

apps/$(ETHTOOL)/configure:
	$(call WGET,https://www.kernel.org/pub/software/network/ethtool/$(ETHTOOL).tar.gz,apps/$(ETHTOOL).tar.gz)
	tar -C apps -xzf apps/$(ETHTOOL).tar.gz

apps/$(ETHTOOL)/ethtool: apps/$(ETHTOOL)/configure
	cd apps/$(ETHTOOL) && \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  CFLAGS="-O2" \
	./configure \
	  --host=sh4-linux \
	  --prefix=/
	make -C apps/$(ETHTOOL)

.PHONY: ethtool
ethtool: apps/$(ETHTOOL)/ethtool

#
# mtd-utils
#

apps/mtd-utils/Makefile:
	$(call GIT_CLONE,git://git.infradead.org/mtd-utils.git,mtd-utils,$(MTD_UTILS_COMMIT))

apps/mtd-utils/nanddump: apps/mtd-utils/Makefile
	make -C apps/mtd-utils \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  CFLAGS="-O2 -I$(CURDIR)/kernel/include"

.PHONY: mtd-utils
mtd-utils: apps/mtd-utils/nanddump

#
# libtirpc
#

apps/$(LIBTIRPC)/configure:
	$(call WGET,http://sourceforge.net/projects/libtirpc/files/libtirpc/$(LIBTIRPC_VERSION)/$(LIBTIRPC).tar.bz2,apps/$(LIBTIRPC).tar.bz2)
	tar -C apps -xjf apps/$(LIBTIRPC).tar.bz2

apps/$(LIBTIRPC)/src/.libs/libtirpc.a: apps/$(LIBTIRPC)/configure
	cd apps/$(LIBTIRPC) && \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  CFLAGS="-O2" \
	./configure \
	  --host=sh4-linux \
	  --prefix=/ \
	  --disable-shared \
	  --disable-gssapi \
	  --disable-ipv6
	make -C apps/$(LIBTIRPC)

.PHONY: libtirpc
libtirpc: apps/$(LIBTIRPC)/src/.libs/libtirpc.a

#
# rpcbind
#

apps/$(RPCBIND)/configure:
	$(call WGET,http://sourceforge.net/projects/rpcbind/files/rpcbind/$(RPCBIND_VERSION)/$(RPCBIND).tar.bz2,apps/$(RPCBIND).tar.bz2)
	tar -C apps -xjf apps/$(RPCBIND).tar.bz2

apps/$(RPCBIND)/rpcbind: apps/$(LIBTIRPC)/src/.libs/libtirpc.a apps/$(RPCBIND)/configure
	cd apps/$(RPCBIND) && \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  CFLAGS="-O2" \
	  TIRPC_CFLAGS="-I$(CURDIR)/apps/$(LIBTIRPC)/tirpc" \
	  TIRPC_LIBS="-L$(CURDIR)/apps/$(LIBTIRPC)/src/.libs -Wl,-Bstatic -ltirpc -Wl,-Bdynamic" \
	./configure \
	  --host=sh4-linux \
	  --prefix=/ \
	  --with-systemdsystemunitdir=no
	make -C apps/$(RPCBIND)

.PHONY: rpcbind
rpcbind: apps/$(RPCBIND)/rpcbind

#
# nfs-utils
#

apps/$(NFSUTILS)/configure:
	$(call WGET,http://sourceforge.net/projects/nfs/files/nfs-utils/$(NFSUTILS_VERSION)/$(NFSUTILS).tar.bz2,apps/$(NFSUTILS).tar.bz2)
	tar -C apps -xjf apps/$(NFSUTILS).tar.bz2

apps/$(NFSUTILS)/utils/exportfs/exportfs: apps/$(RPCBIND)/rpcbind apps/$(NFSUTILS)/configure
	cd apps/$(NFSUTILS) && \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  CFLAGS="-O2" \
	  TIRPC_CFLAGS="-I$(CURDIR)/apps/$(LIBTIRPC)/tirpc" \
	  TIRPC_LIBS="-L$(CURDIR)/apps/$(LIBTIRPC)/src/.libs -Wl,-Bstatic -ltirpc -Wl,-Bdynamic" \
	./configure \
	  --host=sh4-linux \
	  --prefix=/ \
	  --disable-mount \
	  --disable-nfsdcltrack \
	  --disable-nfsv4 \
	  --disable-gss \
	  --disable-ipv6 \
	  --disable-uuid \
	  --without-tcp-wrappers
	make -C apps/$(NFSUTILS)

.PHONY: nfsutils
nfsutils: apps/$(NFSUTILS)/utils/exportfs/exportfs

#
# binutils (mainly for addr2line)
#
apps/$(BINUTILS)/binutils/configure:
	$(call WGET,https://ftp.gnu.org/gnu/binutils/$(BINUTILS).tar.gz,apps/$(BINUTILS).tar.gz)
	tar -C apps -xf apps/$(BINUTILS).tar.gz

# disable as much as possible during configuring, since we only really want one binary...
apps/$(BINUTILS)/binutils/addr2line: apps/$(BINUTILS)/binutils/configure
	cd apps/$(BINUTILS) && \
		AR=$(TOOLCHAIN)/bin/sh4-linux-ar \
		CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
		CFLAGS="-O2" \
	./configure \
		--host=sh4-linux \
		--prefix=/ \
		--disable-gold \
		--disable-ld \
		--disable-gprofng \
		--disable-libquadmath \
		--disable-libada \
		--disable-libssp
	make -C apps/$(BINUTILS) -j $(CPUS)

.PHONY: binutils
binutils: apps/$(BINUTILS)/binutils/addr2line

#
# oscam
#

apps/oscam-svn/config.sh:
	cd apps && svn checkout http://www.streamboard.tv/svn/oscam/trunk oscam-svn -r $(OSCAM_REV)

apps/oscam-svn/Distribution/oscam-1.20-unstable_svn$(OSCAM_REV)-sh4-linux: apps/oscam-svn/config.sh
	make -C apps/oscam-svn CROSS_DIR=$(TOOLCHAIN)/bin/ CROSS=sh4-linux-

.PHONY: oscam
oscam: apps/oscam-svn/Distribution/oscam-1.20-unstable_svn$(OSCAM_REV)-sh4-linux

#
# nano
#

apps/$(NANO)/configure:
	$(call WGET,$(NANO_DOWNLOAD),apps/$(NANO_FILENAME))
	tar -C apps -xzf apps/$(NANO_FILENAME)

apps/$(NANO)/src/nano: apps/$(NANO)/configure
	cd apps/$(NANO) && \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  CFLAGS="-O2" \
	./configure \
	  --host=sh4-linux \
	  --disable-utf8 \
	  --prefix=/
	make -C apps/$(NANO)

.PHONY: nano
nano: apps/$(NANO)/src/nano

#
# python3-host
#

apps/host/$(PYTHON3)/pyconfig.h.in:
	$(call WGET,$(PYTHON3_DOWNLOAD),apps/host/$(PYTHON3_FILENAME))
	tar -C apps/host -xzf apps/host/$(PYTHON3_FILENAME)

apps/host/$(PYTHON3)/python: apps/host/$(PYTHON3)/pyconfig.h.in
	cd apps/host/$(PYTHON3) && \
	./configure
	make -C apps/host/$(PYTHON3)

.PHONY: python3-host
python3-host: apps/host/$(PYTHON3)/python

#
# python3
#

apps/$(PYTHON3)/pyconfig.h.in:
	$(call WGET,$(PYTHON3_DOWNLOAD),apps/$(PYTHON3_FILENAME))
	tar -C apps -xzf apps/$(PYTHON3_FILENAME)

apps/$(PYTHON3)/patch.stamp: apps/$(PYTHON3)/pyconfig.h.in apps/host/$(PYTHON3)/python
	cd apps/$(PYTHON3) && \
	  PKG_CONFIG_PATH=$(TOOLCHAIN)/target/usr/lib/pkgconfig \
	  PKG_CONFIG=$(TOOLPATH)/pkg-config \
	  ARCH=sh \
	  CPP=$(TOOLCHAIN)/bin/sh4-linux-cpp \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	  READELF=/usr/bin/readelf \
	  PYTHON_FOR_BUILD=$(CURDIR)/apps/host/$(PYTHON3)/python \
	  CONFIG_SITE=$(CURDIR)/patches/python3.config.site \
	./configure \
	  --host=sh4-linux \
	  --build=x86_64-redhat-linux \
	  --target=sh4-linux \
	  --sysconfdir=/etc \
	  --prefix=/usr \
	  --enable-ipv6=no
	cd apps/$(PYTHON3) && patch -b -p0 < ../../patches/python3-makefile.patch
	cd apps/$(PYTHON3) && patch -b -p0 < ../../patches/python3-setup.patch
	cd apps/$(PYTHON3) && patch -b -p0 < ../../patches/python3-ccompiler.patch
	cd apps/$(PYTHON3) && patch -b -p0 < ../../patches/python3-build-ext.patch
	cd apps/$(PYTHON3) && patch -b -p0 < ../../patches/python3-getpath.patch
	touch apps/$(PYTHON3)/patch.stamp

apps/$(PYTHON3)/compiled.stamp: apps/$(PYTHON3)/patch.stamp
	rm -rf $(CURDIR)/apps/$(PYTHON3)/dest
	PYTHONPATH=$(CURDIR)/apps/$(PYTHON3)/Lib \
        make -C apps/$(PYTHON3) \
	  PYINCDIRS="$(CURDIR)/apps/$(PYTHON3):$(CURDIR)/apps/$(PYTHON3)/Include" \
	  PYLIBS="." \
	  CROSS_COMPILE=yes \
	  ABIFLAGS="" \
	  PYTHON_FOR_BUILD=$(CURDIR)/apps/host/$(PYTHON3)/python \
	  PGEN=$(CURDIR)/apps/host/$(PYTHON3)/Parser/pgen \
	  FREEZE_IMPORTLIB=$(CURDIR)/apps/host/$(PYTHON3)/Programs/_freeze_importlib \
	  PYTHONPATH=$(CURDIR)/apps/$(PYTHON3)/Lib \
	  PYTHON_OPTIMIZE="" \
	  PYTHONDONTWRITEBYTECODE=1 \
	  _PYTHON_HOST_PLATFORM=linux-sh4 \
	  DESTDIR=$(CURDIR)/apps/$(PYTHON3)/dest \
	  sharedmods sharedinstall libinstall bininstall
	rm -f apps/$(PYTHON3)/dest/usr/bin/python*-config
	rm -f apps/$(PYTHON3)/dest/usr/bin/2to3*
	rm -f apps/$(PYTHON3)/dest/usr/bin/idle*
	$(TOOLCHAIN)/bin/sh4-linux-strip apps/$(PYTHON3)/dest/usr/bin/python3*
	rm -f apps/$(PYTHON3)/dest/usr/lib/*.a
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/test
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/ctypes/test
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/sqlite3
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/turtle*
	rm -f apps/$(PYTHON3)/dest/usr/lib/python3*/__pycache__/turtle*
	rm -f apps/$(PYTHON3)/dest/usr/lib/python3*/lib-dynload/*test*
	rm -f apps/$(PYTHON3)/dest/usr/lib/python3*/lib-dynload/*audio*
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/lib2to3
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/unittest
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/tkinter
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/idlelib
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/distutils
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/ensurepip
	rm -rf apps/$(PYTHON3)/dest/usr/lib/python3*/curses
	find $(CURDIR)/apps/$(PYTHON3)/dest/usr/lib/ -name "*.opt-[12].pyc" -exec rm {} \;
	find $(CURDIR)/apps/$(PYTHON3)/dest/usr/lib/ -name "test_*" -exec rm {} \;
	$(call PACKAGE,apps/$(PYTHON3)/dest,$(PYTHON3_PACKAGE_NAME),usr)
	touch apps/$(PYTHON3)/compiled.stamp

.PHONY: python3
python3: apps/$(PYTHON3)/compiled.stamp

#
# multicast-rtp
#

apps/multicast-rtp/ok.stamp: tools/multicast-rtp
	mkdir -p apps/multicast-rtp/sbin apps/multicast-rtp/etc/init.extra
	cp tools/multicast-rtp apps/multicast-rtp/sbin
	ln -sf ../../sbin/multicast-rtp apps/multicast-rtp/etc/init.extra/multicast-rtp
	$(call PACKAGE,apps/multicast-rtp,$(MULTICAST_RTP_PACKAGE_NAME),etc sbin)
	touch apps/multicast-rtp/ok.stamp

.PHONY: multicast-rtp
multicast-rtp: apps/multicast-rtp/ok.stamp

#
# senddsq
#

tools/senddsq: tools/senddsq.c
	$(TOOLCHAIN)/bin/sh4-linux-gcc -o tools/senddsq -Wall -lrt tools/senddsq.c

apps/senddsq/ok.stamp: tools/senddsq
	cp tools/senddsq apps/senddsq/sbin
	$(call PACKAGE,apps/senddsq,$(SENDDSQ_PACKAGE_NAME),sbin)
	touch apps/senddsq/ok.stamp

.PHONY: senddsq
senddsq: tools/senddsq apps/senddsq/ok.stamp

#
# tvheadend
#

apps/tvheadend/Makefile:
	$(call GIT_CLONE,https://github.com/tvheadend/tvheadend.git,tvheadend,$(TVHEADEND_COMMIT))

apps/tvheadend/build.linux/tvheadend: apps/tvheadend/Makefile
	cd apps/tvheadend && \
	  PKG_CONFIG_PATH=$(TOOLCHAIN)/target/usr/lib/pkgconfig \
	  PKG_CONFIG=$(TOOLPATH)/pkg-config \
	  ARCH=sh \
	  CC=$(TOOLCHAIN)/bin/sh4-linux-gcc \
	./configure \
	  --disable-dbus_1 \
	  --disable-imagecache \
	  --disable-uriparser \
	  --enable-bundle
	$(MAKE) -j $(CPUS) -C apps/tvheadend

.PHONY: tvheadend
tvheadend: apps/tvheadend/build.linux/tvheadend

#
# clean all
#

.PHONY: clean
clean: kernel-mrproper
	rm -rf firmware/initramfs
	rm -rf toolchain/4.5.3-99
	rm -rf tools/syscall-dump.o* tools/syscall-dump.s*

testx:
	echo $(foreach f,$(notdir $(wildcard apps/minisatip5/html/*)), "'$f'")
