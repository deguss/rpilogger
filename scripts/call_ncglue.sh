#!/bin/bash 
#set -e
test -e /opt/opendatalogger/ncglue || exit 1

echo
date

SHRHOST=`hostname` 
SHRHOST=${SHRHOST//rpi-/} # gets hostname (if neccessary cuts 'rpi-')

REMOTEHOST="pid@aero.nck.ggki.hu"

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
        echo "rsyncing to $REMOTEHOST:$SHRHOST-data/"
        OPWD=`pwd`
        cd /home/pi/data
        rsync -av --exclude tmp/ /home/pi/data/ $REMOTEHOST:$SHRHOST-data/
        
        if [ $? -eq 0 ]; then  #if successful, delete local files
            find /home/pi/data/ -mindepth 3 -type f -exec rm {} \;
            echo "files deleted"
        fi
        
        cd $OPWD
        exit 0
        ;;
      *) #
        echo "ncglue: undefined exit code"
        exit 1
        ;;        
    esac
done
