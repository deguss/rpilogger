#!/bin/bash
#URL="http://opendatalogger.com/update_client.php"
URL="http://tesla.ggki.hu/update_client.php"
MAC=$(cat /sys/class/net/eth0/address)
HOST=$(hostname)
IP=$(hostname -I)
MAC=$(php -r "echo urlencode('$MAC');")
STR="mac=""$MAC""&host=""$HOST""&ip=""$IP"

echo $(date +"%Y/%m/%d %H:%M:%S"):
curl --silent --show-error --max-time 30 --data "$STR" $URL 
