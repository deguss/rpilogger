/**
 *   @file    inifile.h
 *   @author  Daniel Piri
 *   @brief   Manipulating config files.
 *   
 *   This software is licenced under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h> // variable args
#include <time.h>
#include <math.h> //fmin
#include <sys/stat.h>
#include "iniparser.h"
#include "dictionary.h"

//--------------- DEFINES and VARIABLES --------------------------------

const char ini_comments[]=
    "; configuration file\n"
    "; take care to set only valid parameters when editing this file by your own!\n" 
    "; valid range for\n"
    ";  - sampling rate sps in [0.003333,500] (any float representing frequency in Hz)\n"
    ";  - spsadc can be 8,16,32,64,128,250,475,860 with the restriction spsadc > 1.5*sps\n"
    ";  - programmable gain amplifier pga_x expressing voltage ranges symmetrical\\\n" 
    ";     around 6.144,4.096,2.048,1.024,0.512,0.256 Volts are indexed by 0,1,2,3,4,5\n"
    ";  - if auto_pga is set to 1 dynamic input range will be choosen, in that case\n"
    ";  - after pga_updelay amount of seconds ADC range will be lowered.\n;\n"
    ";  - output: binary files (NETCDF .nc) will be produced.\n\n\n";


//-------------- FUNCTION PROTOTYPES -----------------------------------

void create_ini_file(char *ini_name);
int parse_ini_file(char *ini_name);
void update_ini_file(char *ini_name);
void logErrDate(const char *format, ...);
int min(int x, int y);
int max(int x, int y);
int isValueInIntArr(int val, const int *arr, int size);

//--------------- MACROS -----------------------------------------------
#define NELEMS(x)  (sizeof(x) / sizeof(x[0]))

//-------------- EXTERNAL FUNCTION PROTOTYPES --------------------------

extern void exit_all(int);
extern int spsadc;
extern float sps;
extern int sampl;
extern int SPSc;
extern int pga[];
extern int auto_pga;
extern int pga_updelay;
extern char datafiledir[];
extern const float overvoltage;

extern const int SPSv[];
extern const size_t SPSv_count;

extern const uint16_t SPSa[];
extern const float PGAv[];
//extern const uint16_t PGAa[];

extern const char user[];


extern void get_uid_gid(const char *user, long *uid, long *gid);
