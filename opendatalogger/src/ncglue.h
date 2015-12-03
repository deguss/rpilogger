/**
 *   @file    ncglue.h
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com 
 *   @brief   concatenating nc files.
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
 
#include <stdio.h> //to redirect stdout, stderr
#include <stdlib.h> //itoa
#include <stdint.h>
#include <stdarg.h> //logErrDate function (variable args)
#include <signal.h> //SIGINT
#define __USE_XOPEN   //to supress warning strptime
#define _GNU_SOURCE   //to supress warning strptime
#include <time.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <libgen.h> //dirname
#include <dirent.h> 
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h> //mkdir, fsize

#include <netcdf.h> //binary file

#define logErrDate printf
#define exit_all exit

#define MAX_FILES (65) 
#define MAX_FILENAME (13+1) // "1446396664.nc"

// -------------------- MACROS -----------------------------------------
#define CHECK_NC_ERR(strr) \
do { \
    if (status != NC_NOERR){ \
        logErrDate("%s: %s\n%s\nExit\n",__func__,strr,nc_strerror(status)); \
        return -1; \
    } \
} while(0);

#define CHECK_NC_ERR_FREE(strr) \
do { \
    if (status != NC_NOERR){ \
        logErrDate("%s: %s\n%s\nExit\n",__func__,strr,nc_strerror(status)); \
        FREE_AND_RETURN(-1); \
    } \
} while(0);

//before freeing we have to restore original address (which calloc gave)
#define FREE_AND_RETURN(retcode) \
do { \
    for (k=0; k<par.chnumber; k++){    \
        free(chp[k]);  \
    } \
    return retcode;    \
} \
while(0);
    
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

struct params{
    //no member struct timespec {.tv_sec, .tv_nsec} since it is stored in argument
    
    /* stuct tm (broken down time format) has following members
     * int tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec ... 
     */
    struct tm tmb;
    
    float sps;     //sampling rate (1/s)
    int chnumber;  //amount of channels (usually 2 4 or 8)
    size_t length; //amount of samples in the file (usually sps * file duration)
    
    char starts[26+1]; //start string in format "2015-11-08 01:39:27.499968"
    
};
#define MAX_CH_NUMBER (8)


float standard_deviation(float *datap, long n);

int get_nc_params(const char *file_in_dir, struct params *par, int first, 
	struct timespec *fnts_p);
	
	
int concat_nc_data(const char *file_in_dir, const int count, const char *file_out,
    const time_t tstart, long *pos_p, struct timespec (*fnts_p));
    
    
int cmpfunc_timespec(const void *a, const void *b);


int cmpfunc_time(const void *aa, const void *bb);


int getSortedFileList(const char *datadir, struct timespec *fnts_p);


int delete_file(char *to_delete_file, struct timespec *ts, int dryrun);


time_t calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour );


long arraysum( long *arr_p, long len);


int mkdir_filename(const char *dir_name);


long fsize(char *filename) ;


