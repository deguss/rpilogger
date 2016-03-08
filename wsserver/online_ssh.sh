#!/bin/bash
REMOTEPORTFILE="/home/pi/wsserver/remoteport.txt"

if [ -s $REMOTEPORTFILE ] #file exists and has size greater than 0
then
    ODLPORT=`cat $REMOTEPORTFILE`
else
    ODLPORT=3000
    echo $ODLPORT > $REMOTEPORTFILE
fi

URL="http://tesla.ggki.hu/update_client.php"
URL2="http://opendatalogger.com/update_client.php"
MAC=$(cat /sys/class/net/eth0/address)
HOST=$(hostname)
IP=$(hostname -I)
MAC=$(php -r "echo urlencode('$MAC');")
STR="mac=""$MAC""&host=""$HOST""&ip=""$IP"

echo $(date +"%Y/%m/%d %H:%M:%S"):
curl --silent --show-error --max-time 30 --data "$STR" $URL
curl --silent --show-error --max-time 30 --data "$STR" $URL2

#RES=$(netstat -tp 2>/dev/null | grep "s171.web-hosting.:21098")
RES=$(netstat -tp 2>/dev/null | grep "tesla.ggki.hu:telnet")
case "$RES" in 
    *ESTABLISHED*)
        # do nothing
        ;;
    *)
        echo "[ERROR] no ssh tunnel established! Reconnect on port $ODLPORT"
        #ssh -R $ODLPORT:localhost:22 openkhhr@opendatalogger.com -p 21098 -o ConnectTimeout=30 -f -N
        ssh -R $ODLPORT:localhost:22 pi@tesla.ggki.hu -p 23 -o ConnectTimeout=30 -f -N

        ;;
esac
