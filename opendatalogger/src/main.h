#pragma once
/**
 *   @file    main.h
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com
 *   @brief   main header file 
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */

#include <stdio.h> //to redirect stdout, stderr
#include <stdlib.h> //itoa
#include <stdint.h>
#include <unistd.h>
#include <math.h> //min max fmin fmax
#include <float.h> //FLT_EPSILON
#include <signal.h> //SIGINT
#include <time.h>
#include <limits.h>
#include <string.h>
#include <libgen.h> //dirname
#include <dirent.h> 
#include <sys/resource.h> //priority
#include <sched.h>    //real time scheduling
#include <stdarg.h> //logErrDate function (variable args)
#include <unistd.h>

#include <sys/time.h>
#include <sys/stat.h> //mkdir, fsize
#include <pthread.h>  //threading, link with -lpthread
#include <syscall.h> //pid of thread
#include <sys/utsname.h> //lock memory
#include <sys/mman.h>
#include <sys/fsuid.h> //set thread uid/gid
#include <pwd.h>    //get user id 

#include <netcdf.h> //binary file
//#include </usr/include/postgresql/libpq-fe.h> //postgresql 
//#include </usr/include/nopoll/nopoll.h> //websocket

#include "system.h"
#include "i2c.h"
#include "ads1115.h"
#include "filelock.h"

//--------------- DEFINES and VARIABLES --------------------------------

// --- global variables
//TODO consider using volatile atomic_t for flags
int done=0, init=1, pga_uf=0, daemon_f=0, debug=0;
int SPSc, spsadc, sampl, piter;
float sps;
char datafiledir[255];

/* FIXME. it is neccessary to initialize file_user somehow, else there 
 * is no way to set owner at creation.
 */
char file_user[50]="pi"; 

int pga[4];
int auto_pga;
int pga_updelay; 
char i2c_interface[100];
#define MAX_SAMPL 600*60

struct data_struct {
    float data[2*MAX_SAMPL][CFG_NR_CH];
    struct timespec t1;
    struct timespec t2;
    int it;
} dst;
/* The sampling process fills the buffer 'data' in exactly 2 minutes.
 * However when reaching it's half (after 1min) the storage thread is triggered,
 * to write out the data (from either the lower half or upper half) to disk.
 * 
 * t1 always keeps the start time of the first sample in the active half,
 * regardless if it is the lower or the upper.
 * 
 * t2 always indicates the start time of the first sample of that half, 
 * which is read for processing (=writing to disk).
 * 
 */
 
char ini_name[50];
FILE *fp_log; 


#define LOGFILE "ads.log"
#define HISTFILE "histog.json"
#define LOGDIR "log"
#define CONFIGFILE "ads.conf"
#define FILELOCK "/var/run/ads.pid"
#define GROUP "pi"


//--------------- MACROS -----------------------------------------------




#define ErrnoCheck(func,cond,retv)  COND_CHECK(func, cond, retv, errno)
#define PthreadCheck(func,rc) COND_CHECK(func,(rc!=0), rc, rc)

#define COND_CHECK(func, cond, retv, errv)                            \
do {                                                                  \
    if ( (cond) ){                                                    \
        fprintf(stderr, "\n[CHECK FAILED at %s:%d]\n| %s(...)=%d (%s)\n\n",\
              __FILE__,__LINE__,func,retv,strerror(errv));            \
        exit(EXIT_FAILURE);                                           \
    }                                                                 \
} while (0)

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)


//-------------- EXTERNAL FUNCTIONS AND VARIABLES ----------------------

extern void create_ini_file(char *ini_name);
extern int parse_ini_file(char *ini_name);
extern void update_ini_file(char *ini_name);
extern void logErrDate(const char *format, ...);
extern int min(int x, int y);
extern int max(int x, int y);
extern int isValueInIntArr(int val, const int *arr, int size);

extern void exit_all(int);
extern void stack_prefault(void);
extern void print_logo(void);
extern void print_usage(void);
extern int listdir(const char *dir, char *element);
extern void set_latency_target(void);

extern long fsize(char *filename);

extern void sighandler(int signum, siginfo_t *info, void *ptr);
extern void register_signals(void);
