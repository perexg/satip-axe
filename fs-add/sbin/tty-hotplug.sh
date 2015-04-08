#!/bin/sh

add() {

test -r /etc/sysconfig/config && . /etc/sysconfig/config

DEVID1=${DEVPATH%:*}
DEVID2=${DEVPATH##*:}
DEVID=${DEVID1##*/}:${DEVID2%%/*}

logger -p local0.notice "$MDEV device attached '$DEVID'"

if test -n "$DEVID"; then
  ln -sf "$MDEV" "/dev/ttyUSB${DEVID}"
fi
if test "$DEVID" = "$TTYUSB1_DEV" -a -n "$TTYUSB1_NAME"; then
  ln -sf "$MDEV" "/dev/${TTYUSB1_NAME}"
fi
if test "$DEVID" = "$TTYUSB2_DEV" -a -n "$TTYUSB2_NAME"; then
  ln -sf "$MDEV" "/dev/${TTYUSB2_NAME}"
fi
if test "$DEVID" = "$TTYUSB3_DEV" -a -n "$TTYUSB3_NAME"; then
  ln -sf "$MDEV" "/dev/${TTYUSB3_NAME}"
fi
if test "$DEVID" = "$TTYUSB4_DEV" -a -n "$TTYUSB4_NAME"; then
  ln -sf "$MDEV" "/dev/${TTYUSB4_NAME}"
fi

}

remove() {

for d in /dev/ttyUSB*; do
  s=$(readlink "$d")
  if test "$s" = "$MDEV"; then
    rm "$d"
  fi
done

}

case "$ACTION" in
add) add ;;
remove) remove ;;
esac
