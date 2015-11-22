/**
 *   @file    i2c.c
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com
 *   @brief   Implements a i2c interface for the Raspberry Pi.
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
#include "i2c.h"


int i2c_open(const char *device) 
{
    int fd;
    if ((fd = open (device, O_RDWR)) < 0){
        logErrDate("%s: Couldn't open %s!"
            "Check whether it exists and if you have permisson!\n",__func__,device);
        return -1;
    }
    return fd;
}

int i2c_select(int fd, int addr)
{
    if (ioctl (fd, I2C_SLAVE, addr) < 0){ // ? I2C_SLAVE_FORCE
        logErrDate("%s: Couldn't find device %02x on address!\n",__func__, addr);
        return -1;
    }
    return 0;
}

int i2c_write(int fd, int addr, int reg, int data)
{
    uint8_t buf[3];

    i2c_select(fd, addr);
 
    buf[0] = reg & 0xFF;
    buf[1] = (data & 0xFF00) >> 8;
    buf[2] = data & 0x00FF;
    if (write(fd, buf, 3) != 3) {
        logErrDate("%s: failed to write to register %02X!\n",__func__,reg);
        return -1;
    }
    return 0;
}

int i2c_read(int fd, int addr, int reg)
{
    uint8_t buf[2];

    i2c_select(fd, addr);

    buf[0] = reg & 0xFF;
    if (write(fd, buf, 1) != 1) {
        logErrDate("%s: failed to write to register %02X!\n",__func__,reg);
        return -1;
    }
    if (read(fd, buf, 2) != 2) {
        logErrDate("%s: failed to read from register %02X!\n",__func__,reg);
        return -1;
    }
    return (int16_t)buf[0]*256 + (uint16_t)buf[1];
}

