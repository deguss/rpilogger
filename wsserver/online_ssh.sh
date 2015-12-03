#!/bin/bash
REMOTEPORTFILE="/home/pi/wsserver/remoteport.txt"

if [ -s $REMOTEPORTFILE ]
then
	ODLPORT=3000
	echo $ODLPORT > $REMOTEPORTFILE
fi

URL="http://opendatalogger.com/update_client.php"
MAC=$(cat /sys/class/net/eth0/address)
HOST=$(hostname)
IP=$(hostname -I)
MAC=$(php -r "echo urlencode('$MAC');")
STR="mac=""$MAC""&host=""$HOST""&ip=""$IP"

echo $(date +"%Y/%m/%d %H:%M:%S"):
curl --silent --show-error --max-time 30 --data "$STR" $URL 

RES=$(netstat -tp 2>/dev/null | grep "s171.web-hosting.:21098")
case "$RES" in 
    *ESTABLISHED*)
        # do nothing
        ;;
    *)
        echo "[ERROR] no ssh tunnel established! Reconnect on port $ODLPORT"
        ssh -R $ODLPORT:localhost:22 openkhhr@opendatalogger.com -p 21098 -o ConnectTimeout=30 -f -N
        ;;
esac