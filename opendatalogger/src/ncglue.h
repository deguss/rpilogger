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

#define MAX_FILES (65) 
#define MAX_FILENAME (13+1) // "1446396664.nc"

// -------------------- MACROS -----------------------------------------
#define CHECK_NC_ERR(strr) \
do { \
    if (status != NC_NOERR){ \
        fprintf(stderr,"%s: %s\n%s\nExit\n",__func__,strr,nc_strerror(status)); \
        return -1; \
    } \
} while(0);

#define CHECK_NC_ERR_FREE(strr) \
do { \
    if (status != NC_NOERR){ \
        fprintf(stderr,"%s: %s\n%s\nExit\n",__func__,strr,nc_strerror(status)); \
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

/**
 * calculates standard deviation, mean, min and max of the array
 * \param datap pointer to the float array
 * \param n length of the array
 * \return the standard deviation
 */
float standard_deviation(float *datap, long n);

/**
 * stores the recording parameters in the struct par for the file whoose file-
 * name will be created by file_in_dir/%ld.nc, with fnts_p->tv_sec.
 * \param file_in_dir the constant string for the location of the files
 *        usually /home/pi/data/tmp
 * \param par pointer to an allocated instance of type struct params
 * \param first flag to indicate that this is the "Master" which serves as compa-
 *        rison base for the parameters. first==1 -> Master, else not. 
 * \param fnts_p pointer to a struct timespec, from where member tv_sec is used
 *        to determine the file name.
 * \return 0 if success, -1 on any error
 */
int get_nc_params(const char *file_in_dir, struct params *par, int first, 
    struct timespec *fnts_p);
    
    
/** 
 * concatenates \a count files located in \a file_in_dir into a \a file_out by
 * preserving insert indices defined in \a pos_p and using array \a fnts_p for 
 * filenames to read in from.
 * \param file_in_dir the constant string for the location of the files
 *        usually /home/pi/data/tmp
 * \param count this is the amount of files to process
 * \param file_out the output file name
 * \param tstart start time of the hour. Usually the first file begins in the 
 *        hour before tstart (usually somewhere in the 59. minute of that hour)
 *        but if not, this marks the start of the hour. 
 * \param pos_s pointer to the array where the indices will be stored where the 
 *        input files have to be inserted into the output array/file. Usually 
 *        this is a monothonic array with constant increase and some offset
 * \param fnts_p pointer to a struct timespec, from where member tv_sec is used
 *        to determine the file name.
 * \return the number of concatenated files on success (equals to \a count) or
 *        -1 on any error
 */
int concat_nc_data(const char *file_in_dir, const int count, const char *file_out,
    const time_t tstart, long *pos_p, struct timespec (*fnts_p));
    
/** 
 * compare function for timespec types.
 * used for qsort (sorting)
 */
int cmpfunc_timespec(const void *a, const void *b);

/** 
 * compare function for time_t types.
 * used for qsort (sorting)
 */
int cmpfunc_time(const void *aa, const void *bb);


/** 
 * counts the files in the \a datadir directory. Allocates memory to store all
 * of them and then sorts by name increasingly. To the output array at address
 * \a fnts_p only the first MAX_FILES will be copied.
 * \param datadir the directory to get the filenames from
 * \param fnts_p points to the array, which holds the filenames in the tv_sec
 *        member as integer.
 * \param returns the total number of files in the directory or
 *        -1 on error
 */
int getSortedFileList(const char *datadir, struct timespec *fnts_p);

/** 
 * deletes the file in the \a file_in_dir given by "%ld.nc",ts->tv_sec
 * \param file_in_dir the location of the file to be deleted.
 *        usually /home/pi/data/tmp
 * \param ts pointer to a timespec, whoose member tv_sec will be used to 
 *        determine the filename, according to the implementation:
 *        sprintf(filename, "%s/%ld.nc", file_in_dir, fnts_p->tv_sec);
 * \param dryrun if set, no files will be deleted only the indication
 *        is printed "filename would be deleted"
 */
int delete_file(char *file_in_dir, struct timespec *ts, int dryrun);


time_t calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour );

/**
 * cumulative sum
 * \param arr_p pointer to the array of long elements
 * \param len length of the array
 * \return the sum of the first \a len entries
 * \note no care is taken for overflow checking!
 */
long arraysum( long *arr_p, long len);

/**
 * creates directories recursively
 * \param dir_name full path (can also include a filename, no problem)
 *        all neccessary directories up to the last '/' in the \a dir_name will
 *        be created
 * \return 0 on success
 *         -1 in case of error
 */
int mkdir_filename(const char *dir_name);

/** 
 * \param filename the file name
 * \return file size in bytes
 *         or -1 on any error
 */
long fsize(char *filename) ;


