#!/bin/bash
#
# Author: Portisch
# https://github.com/Portisch/idl4k
#

UIMAGE=idl4k.bin
CPIONEW=rootfs-idl4k.cpio
INITRAMFSDIR=initramfs
KERNELDIR=../build/kernel/

case $1 in
extract)

	#////////////////////////////////////////////////////////////
	# Checking for uImage magic word
	# http://linux-arm.org/git?p=u-boot-armdev.git;a=blob;f=include/image.h
	#////////////////////////////////////////////////////////////
	#
	echo ">>>>> Checking for uImage magic word ( 27051956 ) :"
	MAGICWORD=`dd if=${UIMAGE} ibs=1 count=4 | hexdump -v -e '4/1 "%02X"'`
	if [ '27051956' != "$MAGICWORD" ]
		then
			echo "Not a uImage: $MAGICWORD != 27051956"
			exit 1
	else
			echo "$UIMAGE acknowledged!"
	fi

	#////////////////////////////////////////////////////////////
	# Remove header from uImage
	#////////////////////////////////////////////////////////////
	#
	echo ">>>>> Remove header from $UIMAGE"
	IMAGEOLDGZ='Image.old.gz'
	dd if=${UIMAGE} bs=1 skip=64 of=${IMAGEOLDGZ}

	#////////////////////////////////////////////////////////////
	# Extracting kernel from uImage
	#////////////////////////////////////////////////////////////
	#
	echo ">>>>> Extracting kernel from $UIMAGE"
	gunzip ${IMAGEOLDGZ}

	#////////////////////////////////////////////////////////////
	#Extracting initramfs
	# www.garykessler.net/library/file_sigs.html
	# The end of the cpio archive is recognized with an empty file named 'TRAILER!!!' = '54 52 41 49 4C 45 52 21 21 21' (hexadecimal)
	#////////////////////////////////////////////////////////////
	#
	echo ">>>>> Extracting initramfs from kernel"
	CPIOSTART=`grep -a -b -m 1 -o '0707010' Image.old | head -1 | cut -f 1 -d :`
	CPIOEND=`grep -a -b -m 1 -o -P '\x54\x52\x41\x49\x4C\x45\x52\x21\x21\x21\x00\x00\x00\x00' Image.old | head -1 | cut -f 1 -d :`
	CPIOEND=$((CPIOEND + 11 + 3))
	CPIOSIZE=$((CPIOEND - CPIOSTART))
	echo "START: ${CPIOSTART} END: ${CPIOEND} SIZE: ${CPIOSIZE}"
	OLDINITRAMFSDIR='initramfs'
	if [ "$CPIOSIZE" -le '0' ]
		then
			echo "initramfs.cpio not found"
			exit
	fi
	dd if=Image.old bs=1 skip=$CPIOSTART count=$CPIOSIZE > initramfs.old.cpio
	rm -rf $OLDINITRAMFSDIR
	mkdir -p $OLDINITRAMFSDIR
	cd $OLDINITRAMFSDIR
	cpio -v -i --no-absolute-filenames < ../initramfs.old.cpio
	cd ..
	rm -rf Image.old
	rm -rf initramfs.old.cpio
	echo ">>>>> CPIO arcive got extracted from binary"

	;;
repack)
	#////////////////////////////////////////////////////////////
	# Create new cpio archive
	#////////////////////////////////////////////////////////////
	#
	cd ${INITRAMFSDIR}
	echo ">>>>> Create new cpio archive: $CPIONEW"
	find . "(" -path './.git' -o -name '.gitkeep' -o -name 'README.md' ")" -prune -o -print | cpio -C 1 -R root:root -H newc -o > ${KERNELDIR}${CPIONEW}
	
	;;
	*)
	echo "unknown command"

esac
