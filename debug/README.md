A notes for developers (might be outdated):
-------------------------------------------

Requirements:

  - git, python
  - distro with yum
  - STLinux 2.4 (all-sh4-glibc) - http://www.stlinux.com - installed to /opt/STM/STLinux-2.4/
  - fakeroot package/tools

Preparing build env:
  - Install a clean distro with yum package manager (CentOS 7 confirmed to work)
  - Install STLinux toolchain:
    - download http://archive.stlinux.com/stlinux/2.4/iso/STLinux-2.4-sh4-20141119.iso
    - mount it somewhere and cd to that dir
    - ./install all-sh4-glibc
    - this will create /opt/STM/STLinux-2.4/
  - yum install epel-release
  - yum install gcc git python wget tar bzip2 uboot-tools patch fakeroot svn file
  - cd ~ ; git clone https://github.com/perexg/satip-axe ; cd satip-axe

Compilation:

  - just type 'make'
  - kernel with rootfs is in kernel/arch/sh/boot/uImage.gz

Booting uImage.gz on Inverto IDL-400S/Grundig GSS.BOX/Telestar Digibit R1 from USB:

  - connect TTL USB serial adapter to J3 connector (4 pins at the power supply)
    - pin 1 = 3.6V (do not use), pin 2 = GND, pin 3 = RXD (STM CPU), pin 4 = TXD (STM CPU)
    - parameters: 115200,8N1
  - press 'Enter' when you turn on the box (you have only one second) to get 'idl4k> ' prompt
    (it seems that 'Enter' is required also to enable the serial console)
  - modify bootargs (optional - for original firmware)

  set bootargs=console=ttyAS0,115200

  - set debugfw environment variable

  set debugfw "debugfw=usb start;usb start;fatload usb 0 0x84000000 uimage.gz;set bootargs console=ttyAS0,115200 bigphysarea=20000;bootm 0x84000000"

  - you may type 'save' to store this settings permanently
  - to execute the debugfw type 'run debugfw'
  - note that the USB stick should be in the first (upper) USB connector

Configuration

  - dhcp client, telnetd, dropbear (ssh daemon) and minisatip are active by default
  - configuration is stored in /etc/sysconfig/config
