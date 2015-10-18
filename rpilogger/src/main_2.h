#pragma once

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

#include "i2c.h"
#include "ads1115.h"
#include "filelock.h"

//--------------- DEFINES and VARIABLES --------------------------------

// --- global variables
int done=0, init=1, pga_uf=0;
int SPSc, spsadc, sampl, piter;
float sps;
char datafiledir[200];
int pga[4];
int auto_pga;
int pga_updelay; 
char i2c_interface[100];
#define MAX_SAMPL 600*60

struct data_struct {
    float data[2*MAX_SAMPL][4];
    struct timespec t1;
    struct timespec t2;
    int it;
} dst;

char ini_name[100];
struct timespec ts, ts1, ts2;
struct tm *pt1;

FILE *fp_log; 

#define LOGFILE "ads.log"
#define CONFIGFILE "daq.cfg"
#define FILELOCK "/var/run/ads.pid"
#define USER "pi"
const char user[]=USER;
#define GROUP "pi"


//--------------- MACROS -----------------------------------------------

#define NELEMS(x)  (sizeof(x) / sizeof(x[0]))
#define MAX_SAFE_STACK (8*1024) // The maximum stack size which is guaranteed safe to access without  faulting 


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
#define BUILDINFO "INFO: build with gcc " STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__) " on " __DATE__ " at " __TIME__ " UTC\n"
//#pragma message( BUILDINFO )


//--------------- PROTOTYPES -------------------------------------------
void exit_all(int);
void ISRSamplingTimer(int);
void adjust_pga(int it);
void getADC(int it);
float twocompl2float(int value, float pga);
int isValueInIntArr(int val, const int *arr, int size);
void stack_prefault(void);
void print_logo(void);
int listdir(const char *dir, char *element);

//-------------- EXTERNAL FUNCTION PROTOTYPES --------------------------

extern void create_ini_file(char *ini_name);
extern int parse_ini_file(char *ini_name);
extern void update_ini_file(char *ini_name);
extern void logErrDate(const char *format, ...);
extern int min(int x, int y);
extern int max(int x, int y);
extern int isValueInIntArr(int val, const int *arr, int size);
