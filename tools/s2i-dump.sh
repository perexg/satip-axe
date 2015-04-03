#!/bin/sh

if ! test -d /mnt/s2i-log; then
  mkdir -p /mnt/s2i-log
  mount -t tmpfs -o size=300M,mode=0755 tmpfs /mnt/s2i-log
fi
mkdir -p /usr/local/bin
ln -sf /usr/bin/mdnsd /usr/local/bin/mdnsd
rm -f /mnt/s2i-log/s2i.log
LD_PRELOAD=/usr/lib/syscall-dump.so \
SYSCALL_DUMP_LOG=/mnt/s2i-log/s2i.log \
/usr/lib/s2i.bin
