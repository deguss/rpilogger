#pragma once
/**
 *   @file    i2c.h
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com 
 *   @brief   Implements a i2c interface for the Raspberry Pi.
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */

#include <stdio.h>
#include <sys/types.h> // open
#include <sys/stat.h>  // open
#include <fcntl.h>     // open
#include <sys/ioctl.h> 
#include <unistd.h>    // read/write usleep 
#include <stdlib.h>    // exit
#include <inttypes.h>  // uint8_t, etc
#include <linux/i2c-dev.h> // I2C bus definitions
#include <time.h>
#include <sys/time.h>


//--------------- DEFINES and VARIABLES --------------------------------

//-------------- FUNCTION PROTOTYPES -----------------------------------

/** opens the interface
 * \param device interface name e.g. "i2c-0" or "i2c-1"
 * \return file descriptor to the opened stream, or -1 in case of error
 */
int i2c_open(const char *device);


/** sets the slave address for which the following commands 
 * will operate on
 * \param fd file descriptor (of a previous i2c_open command)
 * \param addr i2c address usually in the range 1-127
 * \return 0 on success, -1 on error
 */
int i2c_select(int fd, int addr);


/** writes altogether 3 bytes to the i2c slave.
 * \param fd file descriptor (of a previous i2c_open command)
 * \param addr i2c address usually in the range 1-127
 * \param reg 1 byte of register address
 * \param data 2 bytes of data to be written
 * \return 0 on success, -1 on error
 */
int i2c_write(int fd, int addr, int reg, int data);


/** reads 2 bytes from the \a reg register of the i2c slave
 * \param fd file descriptor (of a previous i2c_open command)
 * \param addr i2c address usually in the range 1-127
 * \param reg 1 byte of register address to read from
 * \return 2 bytes of data read, or -1 on error
 */
int i2c_read(int fd, int addr, int reg);



//-------------- EXTERNAL FUNCTION PROTOTYPES --------------------------
extern void logErrDate(const char* format, ...);
