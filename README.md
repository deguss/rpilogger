OpenSourceLogger
================
Open source data logger for a Raspberry Pi

## Description
* Analog-to-Digital converters are connected through the I2C interface to the GPIO header of the Raspberry Pi. 
* The real-time kernel Linux embedded software samples analogue voltages with up to 500Hz frequency (2 channels simultaneously, each in differential mode).
* The data is stored in high compression NETCDF4 format (binary) which can be easily processed with any Computer algebra program (Matlab, Octave, Scilab and of course with python numpy).
* Optionally the Raspberry runs a lightweight web server with diagnostic tools. The reports and log files are updated in real-time (websocket connection).


## Contents
- [Minimal installation](#minimal-installation)
- [Download image](#download-image)

## Minimal installation
It is possible to configure everything on your running OS to use the OpenDataLogger. However it is designed to operate on a PREEMPT kernel Linux (quasi real time OS) and there are several system-wide config files and permissions to set up. Therefore you can download an minimal image with everything set up for operation.  [Download image](#download-image)


The instructions below provide a guide to port the OpenDataLogger to your existing OS.
Currently, there are some restrictions. 
* Login as user pi

Execute the commands below. Watch for errors. 
```
sudo apt-get update
sudo apt-get install fail2ban daemontools-run git mc nmap highlight
sudo apt-get install netcdf-bin libnetcdf-dev libpam0g-dev linux-headers-rpi
sudo apt-get install python-pip python-rpi.gpio php5-common php5-cgi php-cli php-pear libssh2-php 
```
in case of errors, re-execute these
```sudo apt-get install -f```, ```sudo apt-get upgrade```

at least install python stuff (numpy might take several hours, but it is neccessary)
```
sudo pip install twisted configparser numpy netcdf
sudo pecl install PAM
```

delete home folder of pi, and pull the content of this repository
```
cd /home
sudo rm -r /home/pi
sudo mkdir pi
sudo chown pi:pi pi
git clone http://github.com/deguss/rpilogger pi
bash
```

configure web-server and i2c permissions
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


#### i2c addresses
After connecting the ADC modules to the I2C interface, it is adviced to check if they are recognized. Each ADC module (and any other device on the I2C bus) has a unique address. 
* When shorting the ADDR pin of the ADS1115 to GND -> `ADDR=0x48`
* When shorting the ADDR pin of the ADS1115 to 3.3V -> `ADDR=0x49`

(There are 2 more configurations)

```
sudo i2cdetect -y 1
```

![alt text](http://opendatalogger.com/photos/i2cdetect.png "i2c detect result")

As it can be seen, only 1 ADC is present. [Another i2c config guide](https://learn.adafruit.com/adafruits-raspberry-pi-lesson-4-gpio-setup/configuring-i2c).

#### i2c bus frequency
I set the i2c clock frequency to 2MHz (2000000 Hz) which is quite high. Usually, there is no need to reduce the frequency, however if you are experimenting or having corrupt data, you can display the actual clock frequency by
```
sudo cat /sys/module/i2c_bcm2708/parameters/baudrate
```
and temporarly also set by writing as root to this file. The permanent setting for the clock frequency (also called baud rate) is done at 2 locations (depending on your kernel version):

Most recently in the ```/etc/modprobe.d/i2c.conf``` file. It should contain these lines:
``` options i2c_bcm2708 baudrate=2000000 ```


## Download image
The configuration needs system-wide changes in different config files and system modules.
The easiest way is to download the entire image and flash to an SD-card. 

Download link soon.


The instructions below provide a guide to port the RPilogger to your existing OS.

Login as user pi. Execute the commands below in strict order.

* change user password, extend file system using ```sudo raspi-config```
* set root password! ```sudo passwd```




[visit project's website](http://geodata.ggki.hu/rpilogger)
