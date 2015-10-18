#include "filelock.h"

//----------------------------------------------------------------------
void deletefile(const char *fl){
//----------------------------------------------------------------------
    if (unlink(fl)==-1){
        perror("failed to remove filelock");
        exit(1);                
    }
    printf("invalid filelock found, deleted!\n");    
}

//----------------------------------------------------------------------
void filelock(const char *filelock) {
//----------------------------------------------------------------------
    pid_t pid;
    int fd;
    //const char filelock[]="/var/run/ads.pid";
    struct stat sts;
    char chpid[10];
    char runpid[14];

    if (stat(filelock, &sts)==0){ // filelock already exists 
        printf("filelock already exists!\n");
        if ( (fd=open(filelock, O_RDONLY))==-1 ){
            perror("failed to open filelock for reading!");
            deletefile(filelock);
        }
       
        if (stat(filelock, &sts)==0){  // if filelock still exists 
            if (read(fd,chpid,sizeof(chpid))<=2){ // does not contain data 
                printf("filelock does not contain valid PID!\n");
                close(fd);
                deletefile(filelock);
            } else {
                close(fd);            
            
                sprintf(runpid,"/proc/%d/",atoi(chpid));
                if (stat(runpid, &sts)==0){ //todo: maybe add date check
                    printf("process %s seems to run already!\n",runpid);    
                    exit(1);
                } else  {//delete broken pid file
                    printf("deleting broken filelock (since process died)!\n");
                    deletefile(filelock);
                }
            }
        }
    }
    
    
    if ((fd = open(filelock, O_RDWR | O_EXCL | O_CREAT , S_IRUSR | S_IWUSR | S_IRGRP)) == -1) {
        close(fd);             
        perror("failed to open filelock for exclusive reading");
        exit(1);
    } else {
        pid = getpid();
        sprintf(chpid,"%ld",(long)(pid));
        printf("will acquire new filelock for PID=%s\n",chpid);
        if (write(fd,chpid,strlen(chpid)) != strlen(chpid)){
            close(fd);
            perror("failed to write to filelock");
            exit(1);
        } else {
            close(fd); 
            printf("OK\n");    
        }
    }
}


//----------------------------------------------------------------------
void get_uid_gid(const char *user, long *uid, long *gid){
//----------------------------------------------------------------------
    struct passwd pwd;
    struct passwd *result;
    char *buf;
    size_t bufsize;
    int s;


    bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1)          // Value was indeterminate
        bufsize = 20000;        //FIXME (this should be enough)

    buf = malloc(bufsize);
    if (buf == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    s = getpwnam_r(user, &pwd, buf, bufsize, &result);
    if (result == NULL) {
        if (s == 0)
            logErrDate("user %s not found\n",user);
        else {
            errno = s;
            perror("getpwnam_r");
        }
        exit(EXIT_FAILURE);
    }
    *uid = (long)pwd.pw_uid;
    *gid = (long)pwd.pw_gid;
}
