#!/bin/bash 
#set -e
test -e /opt/opendatalogger/ncglue || exit 1

echo
date

SHRHOST=`hostname` 
SHRHOST=${SHRHOST//rpi-/} # gets hostname (if neccessary cut 'rpi-')

REMOTEHOSTFILE="/home/pi/scripts/remotehost.txt"

while true
do
    stdbuf -oL -eL /opt/opendatalogger/ncglue >> /opt/opendatalogger/log/ncglue.log
    case $? in
      0) # exit code for success
        echo "1 done"
        continue
        ;;
      1) # exit code for error! Manual action needed
        VAR="ncglue: Manual action needed"
        echo $VAR | wall
        exit 1
        ;;
      2) # exit code for finished job
        echo "job seems to be ready"
        if [ -s $REMOTEHOSTFILE ] ; then
            REMOTEHOST=`cat $REMOTEHOSTFILE`
            if [ -s `pgrep -n rsync` ] ; then
                echo "rsyncing to $REMOTEHOST:$SHRHOST-data/"
                OPWD=`pwd`
                cd /home/pi/data
                rsync -av --exclude tmp/ /home/pi/data/ $REMOTEHOST:$SHRHOST-data/
                if [ $? -eq 0 ]; then  #if successful, delete local files older than 1 hour
                    find /home/pi/data/ -mindepth 3 -mmin +63 -type f -delete
                    echo "files deleted"
                fi
                cd $OPWD
                exit 0
            else
                echo "rsync is already running. terminating"
                exit 0
            fi
        else
            echo "no remote host defined."
            exit 0
        fi
        ;;
      *) #
        echo "ncglue: undefined exit code"
        exit 1
        ;;
    esac
done
