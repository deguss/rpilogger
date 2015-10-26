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
#include <string.h>
#include <libgen.h> //dirname
#include <dirent.h> 
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h> //mkdir, fsize

#include <netcdf.h> //binary file

#define logErrDate printf

#define MAX_FILES (1441)
#define MAX_FILENAME (14+1) //\0
//-------- arrays -------
char filenames[MAX_FILES][MAX_FILENAME];   // "235544.5137"
struct tm times[MAX_FILES];                // file name converted to tm_hour, tm_min, tm_sec ...
//-----------------------
struct params{
    struct timeval startEpoch;
    struct tm stime;
    float sps;
    int chnumber;
    int length;
};

int get_nc_params(const char *filename, struct params *par, int first);
void get_nc_data(const char *filename, const struct params *par, float *bufpt);
int cmpfunc( const void *a, const void *b);
int getSortedFileList(char *daystr);
long int date_difference(struct tm *tm1, struct tm *tm2);
void calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour );


//----------------------------------------------------------------------
int get_nc_params(const char *filename, struct params *par, int first){
//----------------------------------------------------------------------
    int status;
    int ncid;
    int dimid;
    static struct params parM;
    
    status = nc_open(filename, NC_NOWRITE, &ncid);
    if (status != NC_NOERR){ //possible errors: NC_NOMEM, NC_EHDFERR, NC_EDIMMETA, NC_ENOCOMPOIND
        logErrDate("%s: Could not open file %s to read! nc_open: %s\nExit\n",__func__,filename,nc_strerror(status));
        return -1;
    }
    
    status = nc_inq_nvars(ncid, &(par->chnumber));
    if (status != NC_NOERR){
        logErrDate("%s: nc_inq_nvars: %s\nExit\n",__func__,nc_strerror(status));
        return -1;
    }
    
    status = nc_inq_dimid(ncid, "ch1", &dimid);
    if (status != NC_NOERR){
        logErrDate("%s: nc_inq_dimid: %s\nExit\n",__func__,nc_strerror(status));
        return -1;
    }
    
    status = nc_inq_dimlen(ncid, dimid, &(par->length));
    if (status != NC_NOERR){
        logErrDate("%s: nc_inq_dimlen: %s\nExit\n",__func__,nc_strerror(status));
        return -1;
    }

    status = nc_get_att_float(ncid, NC_GLOBAL, "sps", &(par->sps));
    if (status != NC_NOERR){
        logErrDate("%s: nc_get_att_float: %s\nExit\n",__func__,nc_strerror(status));
        return -1;
    }
    
    
    char str[255];
    status = nc_get_att_text(ncid, NC_GLOBAL, "start", str);
    if (status != NC_NOERR){
        logErrDate("%s: nc_get_att_text: %s\nExit\n",__func__,nc_strerror(status));
        return -1;
    }

    char fracs[5];
    str[24]='\0'; //truncate string since get_att_text does not append \0
    sprintf(fracs,"%s",(str+20*sizeof(char)));
    par->startEpoch.tv_usec = strtol(fracs,NULL,10) * 100L;
    str[19]='\0'; //purge fractional seconds    
    strptime(str, "%Y-%m-%d %H:%M:%S", &(par->stime));
    par->startEpoch.tv_sec=(long int)mktime(&(par->stime));
        
    nc_close(ncid);
    
    //if output desired (first file, template)
    if (first){
        printf("%s: file %s read\n",__func__,filename);
        printf("\tstarts at: %04d/%02d/%02d %02d:%02d:%02d.%04ld\n", 
        par->stime.tm_year+1900, par->stime.tm_mon+1, par->stime.tm_mday, 
        par->stime.tm_hour, par->stime.tm_min, par->stime.tm_sec, par->startEpoch.tv_usec/100);
        printf("\tsps: %f\n",par->sps);
        printf("\tchannels: %d\n",par->chnumber);
        printf("\tlength: %d\n",par->length);    
        memcpy(&parM, par, sizeof(struct params));
    }
    else {
    //assert if parameters are equal through different files
        if (parM.sps - par->sps > FLT_EPSILON || parM.chnumber != par->chnumber 
        || parM.length != par->length){
            logErrDate("%s: severe error: parameter mismatch at file %s\n",__func__,filename);
            return -1;
        }
    }
    return 0;
}

//----------------------------------------------------------------------
void get_nc_data(const char *filename, const struct params *par, float *bufpt){
//----------------------------------------------------------------------    
    
}

//----------------------------------------------------------------------
int cmpfunc( const void *a, const void *b) {
//----------------------------------------------------------------------    
     char const *aa = (char const *)a;
     char const *bb = (char const *)b;
     return strcmp(aa, bb);
}

//----------------------------------------------------------------------
int getSortedFileList(char *daystr){
//----------------------------------------------------------------------    
    
    struct tm *tp=&times[0];
    char afilename[255];
    char timestr[255];
    char fulldir[255];    
    
    DIR *dir;
    struct dirent *ent;
    int i,ind=0;
    strcpy(fulldir,"/home/pi/data/tmp/");
    strcat(fulldir,daystr);
    printf("reading directory %s...\n",fulldir);
    if ((dir = opendir (fulldir)) != NULL) {
        while ((ent=readdir(dir)) != NULL) { //print all the files and directories within directory 
            if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            strcpy(timestr,ent->d_name);
            timestr[11]='\0';   //cut ending ".nc"
            strcpy(filenames[ind], timestr);    //copy HHMMSS.FFFF to global array
            timestr[6]='\0';    //cut fractional seconds and dot -> HHMMSS
            strcpy(afilename, daystr);
            strcat(afilename, timestr);
            tp=&(times[ind]);
            if (strptime(afilename, "%Y/%m/%d/%H%M%S", tp) != NULL){
                ind++;
            }
            else
                printf("%s: not a valid file %s\n",__func__,ent->d_name); 
        }
        closedir (dir);
    } else {
        perror("opendir"); // could not open directory 
        return EXIT_FAILURE;
    }
    qsort(filenames, ind, MAX_FILENAME, cmpfunc);  
    for(i=0;i<ind;i++){ //recalculate time struct
        tp=&(times[i]);
        strcpy(afilename, filenames[i]);
        afilename[6]='\0'; //cut fractional seconds and dot
        strptime(afilename, "%H%M%S", tp);
    }
     
    return ind;
}

//----------------------------------------------------------------------
long int date_difference(struct tm *tm1, struct tm *tm2){
//----------------------------------------------------------------------
    time_t time1;
    time_t time2;   
    time1 = mktime(tm1);
    time2 = mktime(tm2);
    return ( (long int) difftime(time2, time1));
}

//----------------------------------------------------------------------
void calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour){
//----------------------------------------------------------------------
    if (hour==0){ //no argument was given
        logErrDate("%s: argument +0 hours was given!\n",__func__);
    }

    tp1h->tm_year = now->tm_year; 
    tp1h->tm_mon  = now->tm_mon;
    tp1h->tm_mday = now->tm_mday;
    tp1h->tm_hour = now->tm_hour+hour;
    tp1h->tm_min = 0;
    tp1h->tm_sec = 0;
    mktime(tp1h); //normalize
    if (hour==1)
        printf("%s: next full hour at: %04d/%02d/%02d %02d:%02d:%02d\n", __func__,
    tp1h->tm_year+1900, tp1h->tm_mon+1, tp1h->tm_mday, tp1h->tm_hour, tp1h->tm_min, 
    tp1h->tm_sec);
}

//----------------------------------------------------------------------
int main(int argc, char * argv[]){
//----------------------------------------------------------------------    
    char daystr[11+1];
    int i,ind;
    strcpy(daystr,"2015/10/23/");
    i=getSortedFileList(daystr);

    printf("%d files found.\n",i); //indicate number of elements
    
    struct tm *tp=&times[0];
    printf("first file starts at: %04d/%02d/%02d %02d:%02d:%02d.%s\n", 
    tp->tm_year+1900, tp->tm_mon+1, tp->tm_mday, tp->tm_hour, tp->tm_min, 
    tp->tm_sec, (filenames[0]+7*sizeof(char)));   //inclusive fractional sec
    
    struct tm tp1h;
    calcNextFullHour(tp, &tp1h, 1);
    printf("in total %ld seconds to next full hour\n",date_difference(tp, &tp1h));

    char fullpath[255];
    //open first file to check length, sps, number of channels ...
    struct params parM, parS;
    sprintf(fullpath,"/home/pi/data/tmp/%s%s.nc",daystr,filenames[0]);
    get_nc_params(fullpath, &parM, 1);

    //open all further files, assert parameters are identical
    for(ind=1; date_difference(tp, &tp1h)>0; ind++){
        tp=&times[ind];
        sprintf(fullpath,"/home/pi/data/tmp/%s%s.nc",daystr,filenames[ind]);
        get_nc_params(fullpath, &parS, 0);
        printf("%2d %s\n",ind,fullpath);
    }

    //get_nc_data(const int howmanyfiles, const struct params *par, float *bufpt);
    
    
    
    //tp->tm_hour+=1;
    //mktime(tp); // normalize result
    
    return 0;
}

//----------------------------------------------------------------------
int main2(int argc, char * argv[]){
//----------------------------------------------------------------------
    int status;
    int ncid;
    char filen[500];
    int nvarsp;
    int dimid;
    size_t lenp;
    
    sprintf(filen,"%s","/home/pi/data/tmp/2015/10/23/235844.5137.nc");
    
    status = nc_open(filen, NC_NOWRITE, &ncid);
    if (status != NC_NOERR){ //possible errors: NC_NOMEM, NC_EHDFERR, NC_EDIMMETA, NC_ENOCOMPOIND
        logErrDate("savefile: Could not open file %s to write! nc_create: %s\nExit\n",filen,nc_strerror(status));
        exit(EXIT_FAILURE);
    }
    printf("%s\nfile open %s (id: %i)\n",nc_strerror(status),filen,ncid);
    
    status = nc_inq_nvars(ncid, &nvarsp);
    if (status != NC_NOERR){
        logErrDate("savefile: nc_inq_nvars: %s\nExit\n",nc_strerror(status));
        exit(EXIT_FAILURE);
    }
    printf("%d vars\n",nvarsp);
    
    status = nc_inq_dimid(ncid, "ch1", &dimid);
    if (status != NC_NOERR){
        logErrDate("savefile: nc_inq_dim: %s\nExit\n",nc_strerror(status));
        exit(EXIT_FAILURE);
    }
    
    status = nc_inq_dimlen(ncid, dimid, &lenp);
    if (status != NC_NOERR){
        logErrDate("savefile: nc_inq_dimlen: %s\nExit\n",nc_strerror(status));
        exit(EXIT_FAILURE);
    }
    printf("%d elements each\n",lenp);
    
    
    nc_close(ncid);
     
    return 0;
}
