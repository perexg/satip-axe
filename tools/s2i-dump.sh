#!/bin/sh

if test -r /root/fw.tgz; then
  if ! test -d /1; then
    mkdir /1
  fi
  cd /1
  tar xvzf /root/fw.tgz
  mount --bind /dev /1/dev
  mount --bind /proc /1/proc
  cp -av /root/syscall-dump.so /1/usr/lib
  cp -av /root/s2i-dump.sh /1
  cp -av /usr/bin/strace /1/usr/bin
  exit 0
fi
if ! test -d /mnt/s2i-log; then
  mkdir -p /mnt/s2i-log
  mount -t tmpfs -o size=300M,mode=0755 tmpfs /mnt/s2i-log
fi
if ! test -d /mnt/ramdisk; then
  mkdir -p /mnt/ramdisk
  mount -t tmpfs -o size=1024k,mode=0755 tmpfs /mnt/ramdisk
fi
killall -9 mdnsd
rm -f /mnt/s2i-log/*
#TRACE="strace -r -ff -o /mnt/s2i-log/trace"
LD_PRELOAD=/usr/lib/syscall-dump.so \
SYSCALL_DUMP_LOG=/mnt/s2i-log/s2i.log \
$TRACE /root/s2i.bin
