#!/bin/bash
echo "restarting tailf-server and reboot-server"

if [ $UID -ne 0 ]; then
  echo "You must be root" 2>&1
  exit 1
fi

PIDP=`pgrep tailf-server`
    if [ $? -eq 0 ]; then  
    kill -15 $PIDP
    echo "tailf-server killed. Will be restared soon"
fi

PIDP=`pgrep reboot-server`
if [ $? -eq 0 ]; then  
    kill -15 $PIDP
    echo "reboot-server killed. Will be restared soon"
fi

