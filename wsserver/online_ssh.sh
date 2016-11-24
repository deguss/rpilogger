#!/bin/bash
REMOTEPORTFILE="/home/pid/rpilogger/wsserver/remoteport.txt"

if [ -s $REMOTEPORTFILE ] #file exists and has size greater than 0
then
    ODLPORT=`cat $REMOTEPORTFILE`
else
    ODLPORT=3000
    echo $ODLPORT > $REMOTEPORTFILE
fi

URL="http://tesla.ggki.hu/update_client.php"
MAC=$(cat /sys/class/net/eth0/address)
HOST=$(hostname)
IP=$(hostname -i)
STR="mac=""$MAC""&host=""$HOST""&ip=""$IP"

echo $(date +"%Y/%m/%d %H:%M:%S"):
curl --silent --show-error --max-time 30 --data "$STR" $URL

RES=$(netstat -tp 2>/dev/null | grep "tesla.ggki.hu:telnet")
case "$RES" in 
    *ESTABLISHED*)
        # do nothing
        ;;
    *)
        echo "[ERROR] no ssh tunnel established! Reconnect on port $ODLPORT"
        ssh -R $ODLPORT:localhost:22 pi@tesla.ggki.hu -p 23 -o ConnectTimeout=30 -f -N
		ssh -R 3001:192.168.1.15:22 pi@tesla.ggki.hu -p 23 -o ConnectTimeout=30 -f -N
        ;;
esac
