#
# this is script executed from /root/main_axe.out binary
# after the AXE initialization routine
# it is respawned immediately on exit
#

while [ 1 ]; do
  if [ -x /etc/init.d/satip ]; then
    . /etc/init.d/satip
  else
    sleep 33554432
  fi
done
