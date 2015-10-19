#!/bin/bash

if (( $UID ))
then 
    echo "not root!"
    exit -1
fi
cd /home/pi/rpilogger
mkdir -p log
chown pi:pi log
touch log/checker.log
chown pi:pi log/checker.log
pid=$(pgrep ^ads$ -o)
if [ "$pid" ]
then
    uw=$(ps -p $pid -o %cpu | tail -n1 | awk '{printf int($1)}') #35 (cpu percentage as number)

    lllog=$(tail -n10 log/ads.log | grep "File written" | tail -n1) #File written: 192736.4995. Samples: 2*30000. Ellapsed: 59.9999s. Size: 235KiB
    ltime=$(echo $lllog | awk -F': ' '{print $2}' | awk -F'.' '{print $1}') #192736
    ldate=$(date +"%H%M%S") #193240
    lell=$(echo $lllog | awk -F'Ellapsed: ' '{print $2}' | head -c5) # 59.99 or 60.00
    lsize=$(echo $lllog | awk -F'Size: ' '{print $2}')

    echo -n $(date +"%Y/%m/%d %H:%M:%S.%4N") "pid: $pid, cpu: $uw%, file: $ltime, ell: $lell, size: $lsize"

    if [ 5 -gt ${uw:0:2} ]; then
        echo " [ERROR:PCPU]"
        pkill ^ads$
        sleep 1
    else
        if !([ "$lell" = "59.99" ] || [ "$lell" = "60.00" ] || [ "$lell" = "" ]);   then
            echo " [ERROR:ELL]"
            if [ "$(pgrep shutdown)" = "" ]; then
                echo " action: scheduling reboot"
                /sbin/shutdown -r +5 "checker.sh: reboot in 5min" &
                exit -1
            fi
        else
            if [ $(pgrep shutdown) ]; then
                /sbin/shutdown -c
                echo " shutdown cancelled"
                exit 0
            else
                if [ "$lsize" != "235KiB" ]; then
                    echo " [ERROR:SIZE]"
                    echo " action: restarting ads"
                    pkill ^ads$
                    sleep 1
                else
                    echo " [OK]"
                    exit 0
                fi
            fi
        fi
        
    fi  
else
    echo $(date +"%Y/%m/%d %H:%M:%S.%4N") "   not running!        ERROR!!!"
    echo " action: restarting ads"
fi

curr=$(date +"until_%Y-%m-%dT%H%M%S.txt")

cp log/ads.log log/$curr
chown pi:pi log/$curr
build/ads &

sleep 1

echo $(date +"%Y/%m/%d %H:%M:%S.%4N") " (re)started with pid: $$ > log/$curr"

exit 0
