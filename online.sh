#!/bin/bash
URL="http://opendatalogger.com/update_client.php"
MAC=$(cat /sys/class/net/eth0/address)
HOST=$(hostname)
IP=$(hostname -I)
MAC=$(php -r "echo urlencode('$MAC');")
STR="mac=""$MAC""&host=""$HOST""&ip=""$IP"

echo $(date +"%Y/%m/%d %H:%M:%S"):
curl -s --show-error -m 5 --data "$STR" $URL 

RES=$(netstat -tp 2>/dev/null | grep "s171.web-hosting.:21098")
# usual answer: 
# tcp        0      0 pc222.lan.example.com:33955 s171.web-hosting.:21098 ESTABLISHED 2413/ssh 
case "$RES" in 
    *ESTABLISHED*)
        # do nothing
        ;;
    *)
        echo "[ERROR] no ssh tunnel established! Reconnect"
        ssh -R 3000:localhost:22 openkhhr@opendatalogger.com -p 21098 -o ConnectTimeout=10 -f -N
        ;;
esac
