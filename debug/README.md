A notes for developers (might be outdated):
-------------------------------------------

Requirements:

  - various build tools (see Dockerfile in this repository))
  - STLinux 2.4 (all-sh4-glibc) - http://www.stlinux.com - installed to /opt/STM/STLinux-2.4/ (or use the Dockerfile in this repository))

Compilation:

  - just type 'make'
  - kernel with rootfs is in kernel/arch/sh/boot/uImage.gz

Compilation using Docker:

  ```
  docker build -t satip-axe-make .
  docker run --rm -v $(pwd):/build satip-axe-make all release
  ```

The release build will be in the `out/` directory. You will end up with a 
bunch of root-owned files in your working directory.

To perform a full rebuild, run `docker run --rm -v $(pwd):/build satip-axe-make clean`. You might also want to run 
`sudo git clean -xfd -f` to clean out untracked files from your working directory (sudo required since Docker generates 
files as root, `-f` required twice to clean out cloned git repositories from `apps/`).

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
