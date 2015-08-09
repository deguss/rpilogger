/* i2c library written by PiD
*/
#include <stdio.h>
#include <sys/types.h> // open
#include <sys/stat.h>  // open
#include <fcntl.h>     // open
#include <unistd.h>    // read/write usleep 
#include <stdlib.h>    // exit
#include <inttypes.h>  // uint8_t, etc
#include <linux/i2c-dev.h> // I2C bus definitions
#include <time.h>
#include <sys/time.h>


int i2c_open(const char *device);
int i2c_select(int fd, int addr);
int i2c_write(int fd, int addr, int reg, int data);
int i2c_read(int fd, int addr, int reg);

extern pthread_mutex_t a_mutex;

//--------------------------------------------------------------------------------------------------------
int i2c_open(const char *device) {
//--------------------------------------------------------------------------------------------------------
	int fd;
	if ((fd = open (device, O_RDWR)) < 0){
		logErrDate("Error: Couldn't open %s! Check whether it exists and if you have permisson!\n",device);
		return -1;
	}
	return fd;
}

//--------------------------------------------------------------------------------------------------------
int i2c_select(int fd, int addr){
//--------------------------------------------------------------------------------------------------------
	if (ioctl (fd, I2C_SLAVE, addr) < 0){ // ? I2C_SLAVE_FORCE
		logErrDate("Error: Couldn't find device %02x on address!\n", addr);
		return -1;
	}
    return 0;
}

//--------------------------------------------------------------------------------------------------------
int i2c_write(int fd, int addr, int reg, int data){
//--------------------------------------------------------------------------------------------------------
	uint8_t buf[3];

	pthread_mutex_lock(&a_mutex);
	i2c_select(fd, addr);
 
	buf[0] = reg & 0xFF;
	buf[1] = (data & 0xFF00) >> 8;
	buf[2] = data & 0x00FF;
	if (write(fd, buf, 3) != 3) {
		logErrDate("Error: i2c_write failed to write to register %02X!\n",reg);
		pthread_mutex_unlock(&a_mutex);
		return -1;
	}
	pthread_mutex_unlock(&a_mutex);
	return 0;
}

//--------------------------------------------------------------------------------------------------------
int i2c_read(int fd, int addr, int reg){
//--------------------------------------------------------------------------------------------------------
	uint8_t buf[2];

	pthread_mutex_lock(&a_mutex);
	i2c_select(fd, addr);

	buf[0] = reg & 0xFF;
	if (write(fd, buf, 1) != 1) {
		logErrDate("Error: i2c_read failed to write to register %02X!\n",reg);
		pthread_mutex_unlock(&a_mutex);
		return -1;
	}
	if (read(fd, buf, 2) != 2) {
		logErrDate("Error: failed to read from to register %02X!\n",reg);
		pthread_mutex_unlock(&a_mutex);
		return -1;
	}
	pthread_mutex_unlock(&a_mutex);
	return (int16_t)buf[0]*256 + (uint16_t)buf[1];
}

