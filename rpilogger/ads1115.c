/* ads1115.c library written by PiD
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

// ------------ ADS1115 register defines --------------
// Register pointers
#define ADS_PTR_MASK        0x03
#define ADS_PTR_CNV     0x00
#define ADS_PTR_CONF      0x01

// Config Register
#define ADS_CONF_OS_SNG	0x8000  //Write: Set to start a single-conversion
#define ADS_CONF_OS_BUSY	0x8000  //Read: Bit = 0 when conversion is in progress
											  //Read: Bit = 1 when device is not performing a conversion
#define ADS_CONF_MUX_MASK	0x7000
#define ADS_CONF_MUX_D_01	0x0000  // Differential P = AIN0, N = AIN1 (default)
#define ADS_CONF_MUX_D_03	0x1000  // Differential P = AIN0, N = AIN3
#define ADS_CONF_MUX_D_13	0x2000  // Differential P = AIN1, N = AIN3
#define ADS_CONF_MUX_D_23	0x3000  // Differential P = AIN2, N = AIN3
#define ADS_CONF_MUX_S_0	0x4000  // Single-ended AIN0
#define ADS_CONF_MUX_S_1	0x5000  // Single-ended AIN1
#define ADS_CONF_MUX_S_2	0x6000  // Single-ended AIN2
#define ADS_CONF_MUX_S_3	0x7000  // Single-ended AIN3

#define ADS_CONF_PGA_MASK     0x0E00
#define ADS_CONF_PGA_6_144V   0x0000  // +/-6.144V range
#define ADS_CONF_PGA_4_096V   0x0200  // +/-4.096V range
#define ADS_CONF_PGA_2_048V   0x0400  // +/-2.048V range (default)
#define ADS_CONF_PGA_1_024V   0x0600  // +/-1.024V range
#define ADS_CONF_PGA_0_512V   0x0800  // +/-0.512V range
#define ADS_CONF_PGA_0_256V   0x0A00  // +/-0.256V range

#define ADS_CONF_MODE_SNG	  0x0100  // Power-down single-shot mode (default)
#define ADS_CONF_CQUE_NONE	  0x0003  // Disable the comparator and put ALERT/RDY in high state (default)

#define ADS_CONF_DR_MASK   0x00E0  
#define ADS_CONF_DR_8      0x0000  // 8 samples per second
#define ADS_CONF_DR_16     0x0020  // 16 samples per second
#define ADS_CONF_DR_32     0x0040  // 32 samples per second
#define ADS_CONF_DR_64     0x0060  // 64 samples per second
#define ADS_CONF_DR_128    0x0080  // 128 samples per second
#define ADS_CONF_DR_250    0x00A0  // 250 samples per second (default)
#define ADS_CONF_DR_475    0x00C0  // 475 samples per second
#define ADS_CONF_DR_860    0x00E0  // 860 samples per second
