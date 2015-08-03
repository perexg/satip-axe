#!/bin/sh

if test -r /root/fw.tgz; then
  if ! test -d /1; then
    mkdir /1
  fi
  cd /1
  tar xvzf /root/fw.tgz
  mkdir -p /dev/shm
  mount -t tmpfs none /dev/shm -o size=1m,mode=330,uid=0,gid=0
  mount --bind /dev /1/dev
  mkdir /1/dev/shm
  mount --bind /dev/shm /1/dev/shm
  mount --bind /proc /1/proc
  mount --bind /sys /1/sys
  cp -av /root/syscall-dump.so /1/usr/lib
  cp -av /root/s2i-dump.sh /1
  cp -av /usr/bin/strace /1/usr/bin
  rm /root/fw.tgz
  exit 0
fi
if ! test -d /.rwfs; then
  mkdir -p /.rwfs
  mount -t tmpfs -o size=30M,mode=0755 tmpfs /.rwfs
  mkdir -p /.rwfs/etc
fi
if ! test -d /mnt/s2i-log; then
  mkdir -p /mnt/s2i-log
  mount -t tmpfs -o size=300M,mode=0755 tmpfs /mnt/s2i-log
fi
if ! test -d /mnt/ramdisk; then
  mkdir -p /mnt/ramdisk
  mount -t tmpfs -o size=1024k,mode=0755 tmpfs /mnt/ramdisk
fi
if ! test -d /mnt/data; then
  mkdir -p /mnt/data
  mount -t tmpfs -o size=30M,mode=0755 tmpfs /mnt/data
fi
mkdir -p /media
killall -9 mdnsd
rm -f /mnt/s2i-log/*
#TRACE="strace -r -ff -o /mnt/s2i-log/trace"
LD_PRELOAD=/usr/lib/syscall-dump.so \
SYSCALL_DUMP_LOG=/mnt/s2i-log/s2i.log \
LD_LIBRARY_PATH=/usr/local/lib \
$TRACE /root/s2i.bin
