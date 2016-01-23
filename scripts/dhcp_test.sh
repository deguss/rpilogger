#!/bin/bash
#pgrep ^dhclient$ > /dev/null && exit 0
ret=$(pgrep -c ^dhclient$)
if (( "$ret" > "1" )); then
  echo "more than 1 instances of dhclient running!"
  killall dhclient
  dhclient -v -pf /run/dhclient.eth0.pid -lf /var/lib/dhcp/dhclient.eth0.leases eth0
fi
exit 0
