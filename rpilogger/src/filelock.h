#pragma once
/**
 *   @file    filelock.h
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com
 *   @brief   Prevents application running multiple instances.
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <sys/fsuid.h> //set thread uid/gid
#include <pwd.h>    //get user id 

//-------------- FUNCTION PROTOTYPES -----------------------------------

void deletefile(const char *fl);
void filelock(const char *filelock);
void get_uid_gid(const char *user, uid_t *uid, gid_t *gid);

//-------------- EXTERNAL FUNCTION PROTOTYPES --------------------------

extern void logErrDate(const char* format, ...);
