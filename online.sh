#!/bin/bash
URL="http://opendatalogger.com/update_client.php"
MAC=$(cat /sys/class/net/eth0/address)
HOST=$(hostname)
IP=$(hostname -I)
MAC=$(php -r "echo urlencode('$MAC');")
STR="mac=""$MAC""&host=""$HOST""&ip=""$IP"
#curl "$STR" 
curl -m 5 --data "$STR" $URL
#echo "$STR"
