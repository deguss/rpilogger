#pragma once
/**
 *   @file    system.h
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com 
 *   @brief   Provides miscellaneous helper functions.
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
#include <sys/types.h> // open
#include <sys/stat.h>  // open
#include <fcntl.h>     // open

#include <sys/time.h>
#include <sys/stat.h> //mkdir, fsize
#include <pthread.h>  //threading, link with -lpthread
#include <syscall.h> //pid of thread
#include <sys/utsname.h> //lock memory
#include <sys/mman.h>
#include <sys/fsuid.h> //set thread uid/gid
#include <pwd.h>    //get user id 

//--------------- DEFINES and VARIABLES --------------------------------

// The maximum stack size which is guaranteed safe to access without  faulting 
#define MAX_SAFE_STACK (8*1024) 

//--------------- MACROS -----------------------------------------------

#ifndef _TIMESTAMP_
 #define STR_ADD " local time"
 #define _TIMESTAMP_ __DATE__ __TIME__ STR_ADD
#endif
#ifndef _USER_AND_HOST
 #define _USER_AND_HOST "?"
#endif 

#define BUILDINFO() \
 do { \
 printf("INFO: build with gcc %d.%d.%d at %s.\n"\
        "      %s\n", \
 __GNUC__,__GNUC_MINOR__,__GNUC_PATCHLEVEL__, _TIMESTAMP_, _USER_AND_HOST); \
 } while(0) 

// #pragma message( BUILDINFO(); )


//--------------- PROTOTYPES -------------------------------------------
void exit_all(int sig);
int mkdir_filename(const char *dir_name);
long fsize(char *filename);
void stack_prefault(void);
void print_logo(void);
void print_usage(void);
int listdir(const char *dir, char *element);
void set_latency_target(void);

//-------------- EXTERNAL FUNCTIONS AND VARIABLES ----------------------
extern void logErrDate(const char* format, ...);

extern int done;
extern pthread_t  p_thread;
extern FILE *fp_log; 
