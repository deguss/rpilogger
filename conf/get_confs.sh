# /boot/
cp /boot/config.txt .

# /etc/
cp /etc/ntp.conf .
cp /etc/resolv.conf .
cp /etc/vsftpd.conf .
cp /etc/watchdog.conf .
cp /etc/default/watchdog .
cp /etc/modprobe.d/watchdog.conf .
cp /etc/modprobe.d/i2c.conf .
cp /etc/fail2ban/fail2ban.conf .
cp /etc/fail2ban/jail.conf .
cp /etc/lighttpd/lighttpd.conf .
cp /etc/php5/cgi/php.ini .
cp /etc/modules .
cp /etc/highlight/filetypes.conf .
cp /etc/environment .
cp /etc/ssh/sshd_config .
cp /etc/ssh/ssh_config .
cp /etc/issue.net .
cp /etc/profile . 

# usr
cp /usr/local/bin/dynmotd .

# crontabs
crontab -l > ~/conf/crontab_pi
sudo crontab -l > ~/conf/crontab_root






