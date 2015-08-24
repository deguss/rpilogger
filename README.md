RPilogger
========
**embedded data logger for a Raspberry Pi**

### Description
tbd

### User Guide
tdb

### Installation instructions 
The configuration needs system-wide changes in different config files and system modules.
The most easy way is to download the entire image. Download link soon.
However the instructions below provide a guide to port the RPilogger to your existing OS.

Login as user pi. Execute the commands below in strict order.

* change user password, extend file system using ```sudo raspi-config```
* set root password! ```sudo passwd```
* Install packages
```
sudo apt-get update
sudo apt-get install ssh sshfs ntp ntpdate php5-dev netcdf-bin libnetcdf-dev libpam0g-dev linux-headers-rpi python-pip python-rpi.gpio python-pip vsftpd lighttpd php5-common php5-cgi php5 php-pear libssh2-php  fail2ban apt-file  daemontools-run mailutils ssmtp git mc nmap
```
in case of errors, re-execute these
```sudo apt-get install -f```, ```sudo apt-get upgrade```
* install python stuff 
```
sudo pip install twisted configparser numpy netcdf
sudo pecl install PAM
```

* delete home folder of pi, and pull the content of this repository
```
sudo rm -r /home/pi
git clone http://github.com/deguss/rpilogger pi
```

* configure web
```
sudo rm -r /var/www
sudo ln -s /home/pi/www /var/www
sudo chmod 0775 /home/pi/www
sudo chown -h www-data:www-data /var/www
sudo ln -s /etc/lighttpd/conf-available/10-fastcgi.conf /etc/lighttpd/conf-enabled/
sudo ln -s /etc/lighttpd/conf-available/15-fastcgi-php.conf /etc/lighttpd/conf-enabled/
sudo usermod -a -G i2c,spi,gpio,www-data pi
sudo usermod -a -G shadow www-data
```



[visit project's website](http://geodata.ggki.hu/rpilogger)
