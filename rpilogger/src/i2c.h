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
#include <pthread.h>  //threading, link with -lpthread


//--------------- DEFINES and VARIABLES --------------------------------

//-------------- FUNCTION PROTOTYPES -----------------------------------
int i2c_open(const char *device);
int i2c_select(int fd, int addr);
int i2c_write(int fd, int addr, int reg, int data);
int i2c_read(int fd, int addr, int reg);

//-------------- EXTERNAL FUNCTION PROTOTYPES --------------------------
extern pthread_mutex_t a_mutex;
extern void logErrDate(const char* format, ...);
