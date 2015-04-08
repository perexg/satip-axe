#!/bin/sh

logger -p local0.notice "Unmounting $MDEV"

for MOUNT_POINT in /media/* ; do
  if test -d "$MOUNT_POINT"; then
    d=$(mountpoint -n "$MOUNT_POINT" 2> /dev/null | cut -d ' ' -f 1)
    if test "$d" = "$MDEV" -o "$d" = "UNKNOWN"; then
      logger -p local0.notice "Unmounting $MDEV : $MOUNT_POINT"
      umount -f "$MOUNT_POINT"
      rmdir "$MOUNT_POINT"
    fi
  fi
done
