#!/bin/bash
set -e
pwds=$PWD

cd /var/log/

echo "total log fize size: " $(du -h -s)
echo 

FILES=$(ls -1 *.log)
echo "$FILES" > $pwds/logfiles.txt

for f in $FILES
do
 cat /dev/null > $f
done
rm -f *.gz

echo "total log fize size: " $(du -h -s)

cd $pwds
cat logfiles.txt
echo "[ok]"
