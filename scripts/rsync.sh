#!/bin/bash 
#set -e

echo
date
SHRHOST="lower"

REMOTEHOSTFILE="/home/pid/rpilogger/scripts/remotehost.txt"

if [ -s $REMOTEHOSTFILE ] ; then
    REMOTEHOST=`cat $REMOTEHOSTFILE`
    if [ -s `pgrep -n -x rsync` ] ; then
         echo "rsyncing to $REMOTEHOST:$SHRHOST-data/"
         OPWD=`pwd`
         cd /home/pid/$SHRHOST-data
         rsync -av --exclude tmp/ /home/pid/$SHRHOST-data/ $REMOTEHOST:$SHRHOST-data/
         if [ $? -eq 0 ]; then  #if successful, delete local files older than 3 days
              find /home/pid/$SHRHOST-data/ -mindepth 3 -mtime +3 -type f -exec rm {} \;
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

