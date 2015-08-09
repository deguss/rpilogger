#!/bin/bash
name=$(date +%Y-%m-%d)
cd /home/pi
tar --exclude='data' --exclude='apt-install' --exclude='dp' -zcf "/tmp/"$name"_home.tgz" /home/pi

# ssh public key stored in remote authorized_keys file (no passphrase)
scp "/tmp/"$name"_home.tgz" pid@aero.nck.ggki.hu:/home/pid/lemi-bck/
retvalue=$?

if [ $retvalue = 0 ]; then
	echo "$name"" done"
else
	echo "Could not copy!"
	echo "Return value: $retvalue"
fi
