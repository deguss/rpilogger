#pragma once
/**
 *   @file    ads1115.c
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com
 *   @brief   ADS1115 interface
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */

// -------------------- config ----------------------------------------

const float overvoltage=5000.0;

//------- These defines should come with compiler flag -D --------------
#ifndef CFG_ADC1
 #warning "No i2c address defined for ADC1. Will try 0x49"
 #define CFG_ADC1 (0x49)
#endif 

#ifndef CFG_SEQ1_ADC1
 #warning "No sampling sequence defined for ADC1. Will use differential 01"
 #define CFG_SEQ1_ADC1 ADS_CONF_MUX_D_01
#endif

#if defined(CFG_ADC2)
 #ifndef CFG_SEQ2_ADC2
  #error "ADC sampling sequence not defined however multiple ADC in use"
 #endif
 #if defined(CFG_SEQ1_ADC1) && defined(CFG_SEQ2_ADC2) && defined(CFG_SEQ3_ADC1) && defined(CFG_SEQ4_ADC2)
  #define CFG_NR_CH (4)
 #endif
 #if defined(CFG_SEQ1_ADC1) && defined(CFG_SEQ2_ADC2) && !defined(CFG_SEQ3_ADC1) && !defined(CFG_SEQ4_ADC2)
  #define CFG_NR_CH (2)
 #endif 
 #if defined(CFG_SEQ1_ADC1) && defined(CFG_SEQ2_ADC2) && defined(CFG_SEQ3_ADC1) && !defined(CFG_SEQ4_ADC2)
  #error "Not possible to sample 3 channels using 2 ADC-s. Define 2 or 4!"
 #endif  
 
#else //one ADC in use
 #if defined(CFG_SEQ1_ADC1) && !defined(CFG_SEQ2_ADC2) && !defined(CFG_SEQ3_ADC1) && !defined(CFG_SEQ4_ADC2)
  #define CFG_NR_CH (1)
 #endif
 #if defined(CFG_SEQ2_ADC2) || defined(CFG_SEQ3_ADC1) || defined(CFG_SEQ4_ADC2) 
  #error "Too many ADC sampling sequences defined."
 #endif
#endif


#define CFG_SEQ_PRINTER(n)  do{ \
    switch(n){                                                \
        case ADS_CONF_MUX_D_01: printf("(DIFF 0-1)"); break;  \
        case ADS_CONF_MUX_D_23: printf("(DIFF 2-3)"); break;  \
        case ADS_CONF_MUX_S_0:  printf("(SINGLE 0)"); break;  \
        case ADS_CONF_MUX_S_1:  printf("(SINGLE 1)"); break;  \
        case ADS_CONF_MUX_S_2:  printf("(SINGLE 2)"); break;  \
        case ADS_CONF_MUX_S_3:  printf("(SINGLE 3)"); break;  \
        default: printf("(NOT VALID CHANNEL)");        break;  \
   }                                                          \
   printf("\n");                                              \
}                                                             \
while(0)

#define NELEMS(x)  (sizeof(x) / sizeof(x[0]))

//------------- ADS1115 register defines -------------------------------

// Register pointers
#define ADS_PTR_MASK    (0x03)
#define ADS_PTR_CNV     (0x00)
#define ADS_PTR_CONF    (0x01)

// Config Register
#define ADS_CONF_OS_SNG     (0x8000)  //Write: Set to start a single-conversion
#define ADS_CONF_OS_BUSY    (0x8000)  //Read: Bit = 0 when conversion is in progress
                                              //Read: Bit = 1 when device is not performing a conversion
#define ADS_CONF_MUX_MASK   (0x7000)
#define ADS_CONF_MUX_D_01   (0x0000)  // Differential P = AIN0, N = AIN1 (default)
#define ADS_CONF_MUX_D_03   (0x1000)  // Differential P = AIN0, N = AIN3
#define ADS_CONF_MUX_D_13   (0x2000)  // Differential P = AIN1, N = AIN3
#define ADS_CONF_MUX_D_23   (0x3000)  // Differential P = AIN2, N = AIN3
#define ADS_CONF_MUX_S_0    (0x4000)  // Single-ended AIN0
#define ADS_CONF_MUX_S_1    (0x5000)  // Single-ended AIN1
#define ADS_CONF_MUX_S_2    (0x6000)  // Single-ended AIN2
#define ADS_CONF_MUX_S_3    (0x7000)  // Single-ended AIN3

#define ADS_CONF_PGA_MASK     (0x0E00)
#define ADS_CONF_PGA_6_144V   (0x0000)  // +/-6.144V range
#define ADS_CONF_PGA_4_096V   (0x0200)  // +/-4.096V range
#define ADS_CONF_PGA_2_048V   (0x0400)  // +/-2.048V range (default)
#define ADS_CONF_PGA_1_024V   (0x0600)  // +/-1.024V range
#define ADS_CONF_PGA_0_512V   (0x0800)  // +/-0.512V range
#define ADS_CONF_PGA_0_256V   (0x0A00)  // +/-0.256V range

#define ADS_CONF_MODE_SNG     (0x0100)  // Power-down single-shot mode (default)
#define ADS_CONF_CQUE_NONE    (0x0003)  // Disable the comparator and put ALERT/RDY in high state (default)

#define ADS_CONF_DR_MASK   (0x00E0)  
#define ADS_CONF_DR_8      (0x0000)  // 8 samples per second
#define ADS_CONF_DR_16     (0x0020)  // 16 samples per second
#define ADS_CONF_DR_32     (0x0040)  // 32 samples per second
#define ADS_CONF_DR_64     (0x0060)  // 64 samples per second
#define ADS_CONF_DR_128    (0x0080)  // 128 samples per second
#define ADS_CONF_DR_250    (0x00A0)  // 250 samples per second (default)
#define ADS_CONF_DR_475    (0x00C0)  // 475 samples per second
#define ADS_CONF_DR_860    (0x00E0)  // 860 samples per second


//--------------- DEFINES and VARIABLES --------------------------------

//sampling rates
const int SPSv[8] = {8,16,32,64,128,250,475,860};
const uint16_t SPSa[8] = { ADS_CONF_DR_8, ADS_CONF_DR_16, ADS_CONF_DR_32,
       ADS_CONF_DR_64, ADS_CONF_DR_128, ADS_CONF_DR_250, ADS_CONF_DR_475,
       ADS_CONF_DR_860};
const size_t SPSv_count = NELEMS(SPSv);

//pga rates       
const float PGAv[6] = {6144.0, 4096.0, 2048.0, 1024.0, 512.0, 256.0};
const uint16_t PGAa[6] = {ADS_CONF_PGA_6_144V, ADS_CONF_PGA_4_096V,
     ADS_CONF_PGA_2_048V, ADS_CONF_PGA_1_024V, ADS_CONF_PGA_0_512V, 
     ADS_CONF_PGA_0_256V};
const size_t PGAv_count = NELEMS(PGAv);






