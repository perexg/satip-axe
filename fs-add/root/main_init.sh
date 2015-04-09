#
# this is script executed from /root/main_axe.out binary
# after the AXE initialization routine
# it is respawned immediately on exit
#

echo | nc 127.0.0.1 1001
while test 1; do sleep 999999999; done
