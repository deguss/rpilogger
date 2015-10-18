#! /bin/bash

echo -e "Searching for the process listening on port 22...\n"
PORT22_PID=$(netstat -lptun | grep -E ":22\s" | awk '{print $7}' | awk -F/ '{print $1}')
if [ ! -n "$PORT22_PID" ]; then
        echo "Error: Was not able to find any process listening on port 22"
        exit 1
fi
echo -e "Found the following PID: $PORT22_PID\n"
echo -e "Command line for PID $PORT22_PID: $(cat /proc/$PORT22_PID/cmdline)\n"
echo -e "Listing process(es) relating to PID $PORT22_PID:\n"
echo "UID        PID  PPID  C STIME TTY          TIME CMD"
ps -ef | grep -E "\s$PORT22_PID\s"
