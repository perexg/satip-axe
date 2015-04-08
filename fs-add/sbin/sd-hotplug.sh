#!/bin/sh

add() {

BLKID=$(/sbin/blkid /dev/$MDEV)
eval ${BLKID#*:}
if [ -n "$LABEL" ]; then
  MOUNT_POINT=/media/$LABEL
else
  MOUNT_POINT=/media/$MDEV
fi

logger -p local0.notice "Mounting $MDEV : $MOUNT_POINT"

mkdir -p "$MOUNT_POINT"

for fs_type in vfat msdos ; do
    if $(mount -t $fs_type -o umask=0 /dev/$MDEV "$MOUNT_POINT" 2>/dev/null) ; then
	logger -p local0.notice "... $MDEV mounted using $fs_type filesystem"
        exit 0
    fi
done
for fs_type in ext4 ext3 ext2 ; do
    if $(mount -t $fs_type /dev/$MDEV "$MOUNT_POINT" 2>/dev/null) ; then
	logger -p local0.notice "... $MDEV mounted using $fs_type filesystem"
        exit 0
    fi
done

}

remove() {

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

}

case "$ACTION" in
add) add ;;
remove) remove ;;
esac
