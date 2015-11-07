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

#define MAX_FILES (1441+10) 
#define MAX_FILENAME (13+1) // "1446396664.nc"

struct params{
    struct timeval startEpoch;
    struct tm stime;
    float sps;
    int chnumber;
    int length;
};
#define MAX_CH_NUMBER (8)

int get_nc_params(const char *filename, struct params *par, int first);
int concat_nc_data(const char *file_in_dir, const int i_start, const int i_stop,
                const char *file_out, const struct params *par,
                char *filenames[][MAX_FILENAME], struct timespec (*times)[]);
int cmpfunc(const void *a, const void *b);
int getSortedFileList(const char *datadir, char *filenames[][MAX_FILENAME], 
                struct timespec (*times)[MAX_FILES]);
long int date_difference(struct tm *tm1, struct tm *tm2);
void calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour );
float standard_deviation(float *datap, int n);

int mkdir_filename(const char *dir_name);
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
int concat_nc_data(const char *file_in_dir, const int i_start, const int i_stop,
                const char *file_out, const struct params *par,
                char *filenames[][MAX_FILENAME], struct timespec (*times)[]){
//----------------------------------------------------------------------    
    int status;
    int ncid_in, ncid_out;
    int i, j, k;
    char file_in[255];
    char varstr[3+1]; //channel name e.g. "ch1"
    int number = (i_stop - i_start)+1;
    if (number < 1){
        logErrDate("%s: parameter error!\nExit\n",__func__);
    }
    
    mkdir_filename(file_out);
    // open file to write
    status = nc_create(file_out, NC_CLOBBER|NC_NETCDF4, &ncid_out);
    if (status != NC_NOERR){ //possible errors: NC_ENOMEM, NC_EHDFERR, NC_EFILEMETA
        logErrDate("%s: Could not open file %s to write! nc_create: %s\nExit\n",
        __func__,file_out,nc_strerror(status));
        return -1;
    }
    
    int out_dimid[MAX_CH_NUMBER];       // dimension id
    int out_varid[MAX_CH_NUMBER];        // variable id
    size_t s1[1] = {0};
    size_t s2[] = {par->length*number};
    
    for (k=0; k<par->chnumber; k++){ //for as many channels as in file (1,2,4 or 8)
        sprintf(varstr,"%s%d","ch",k+1); //ch1, ch2 ...
        // define dimensions 
        status = nc_def_dim(ncid_out, varstr, par->length*number, &out_dimid[k]);
        if (status != NC_NOERR){
            logErrDate("%s: nc_def_dim param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            return -1;
        }
        // define variables
        status = nc_def_var(ncid_out, varstr, NC_FLOAT, 1, &out_dimid[k], &out_varid[k]);
        if (status != NC_NOERR){
            logErrDate("%s: nc_def_var param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            return -1;
        } 
        // assign per-variable attributes
        status = nc_put_att_text(ncid_out, out_varid[k], "units", 2, "mV");
        if (status != NC_NOERR){
            logErrDate("%s: nc_put_att_text param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            return -1;
        }
    }
    status = nc_enddef (ncid_out);    // leave define mode
    
    //allocate memory for 1 hour input files 
    float *chp[MAX_CH_NUMBER];
    size_t nnumb = par->length*(number+1);
    for (i=0; i<par->chnumber; i++){
        chp[i] = calloc(nnumb, sizeof(float));
        if (chp[i] == NULL){
            logErrDate("%s: Could not allocate memory of size %d kB!\nExit\n",
            __func__, (nnumb*sizeof(float)/1024)+1);
            return -1;            
        }     
    }
    
    // for as many input files as given -------------
    int varid[MAX_CH_NUMBER];
    for (j=i_start, i=0; j<=i_stop; i++, j++){
        sprintf(file_in,"%s%s.nc",file_in_dir,*filenames[j]);
        printf("input file: %s\n",file_in);

        status = nc_open(file_in, NC_NOWRITE, &ncid_in);
        if (status != NC_NOERR){ //possible errors: NC_NOMEM, NC_EHDFERR, NC_EDIMMETA, NC_ENOCOMPOIND
            logErrDate("%s: Could not open file %s to read! nc_open: %s\nExit\n",
            __func__,file_in,nc_strerror(status));
            return -1;
        }
    
        for (k=0; k<par->chnumber; k++){ //for as many channels as in file (1,2,4 or 8)
            sprintf(varstr,"%s%d","ch",k+1); //ch1, ch2 ...
            status = nc_inq_varid(ncid_in, varstr, &varid[k]);
            if (status != NC_NOERR){
                logErrDate("%s: Could not find variable %s in %s! nc_inq_varid: %s\nExit\n",
                __func__, varstr, file_in, nc_strerror(status));
                return -1;
            }
        
            status = nc_get_var_float(ncid_in, varid[k], (chp[k]+i*par->length));
            if (status != NC_NOERR){
                logErrDate("%s: Error while reading variable %s in %s! nc_get_var_float: %s\nExit\n",
                __func__, varstr, file_in, nc_strerror(status));
                return -1;
            }
        }
        printf("input file %s read ch: %d\n",file_in,k);
        nc_close(ncid_in);
    } // end for as many input files
        
    // assign variable data 
    for (k=0; k<par->chnumber; k++){ //for as many channels
        status = nc_put_vara_float(ncid_out, out_varid[k], s1, s2, chp[k]);
        if (status != NC_NOERR){
            logErrDate("%s, nc_put_vara_float param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            return -1;
        }
        status = nc_sync(ncid_out); //ensure it is written to disk        
        if (status != NC_NOERR){
            logErrDate("%s, nc_sync %s\nExit\n",__func__,nc_strerror(status));
            return -1;
        }        
    }
        
    status = nc_close(ncid_out); //close file
    if (status != NC_NOERR){
        logErrDate("%s, nc_close %s\nExit\n",__func__,nc_strerror(status));
        return -1;
    }
    
    //printf("||%f, %f, %f, %f||\n",*(chp1), *(chp1+1), *(chp1+par->length-2), *(chp1+par->length-1));
    //standard_deviation(chp1,par->length);


    
    for (k=0; k<par->chnumber; k++)    
        free(chp[k]);
    
    
    
    return 0;
}

//----------------------------------------------------------------------
float standard_deviation(float *datap, int n) {
//----------------------------------------------------------------------
    float mean=0.0, sum_deviation=0.0;
    int i;
    for(i=0; i<n; i++){
        mean+=*(datap+i);
    }
    mean=mean/(long double)n;
    for(i=0; i<n;++i){
        sum_deviation+=(*(datap+i)-mean)*(*(datap+i)-mean);
    }
    printf("\tmean=%f, std_dev=%f\n",mean,sqrtf(sum_deviation/n));
    return 0;
}

//----------------------------------------------------------------------
int cmpfunc( const void *a, const void *b) {
//----------------------------------------------------------------------    
     char const *aa = (char const *)a;
     char const *bb = (char const *)b;
     return strcmp(aa, bb);
}

//----------------------------------------------------------------------
int getSortedFileList(const char *datadir, char *filenames[][MAX_FILENAME], 
        struct timespec (*times)[MAX_FILES]){
//----------------------------------------------------------------------    
    char afilename[255];
    char timestr[MAX_FILENAME];
    
    DIR *dir;
    struct dirent *ent;
    int i,ind=0;
    struct tm tm1;
    printf("reading directory %s...\n",datadir);
    if ((dir = opendir(datadir)) != NULL) {
        while ((ent=readdir(dir)) != NULL) { //print all the files and directories within directory 
            if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            strncpy(timestr,ent->d_name, MAX_FILENAME-1); //e.g. 1446396664.4998.nc
            timestr[MAX_FILENAME-1]='\0'; //truncate length
            if (sscanf(timestr,"%ld.%ld.nc",&times[ind]->tv_sec, &times[ind]->tv_nsec) != 2)
                logErrDate("%s: not a valid file %s\n",__func__,ent->d_name); 
            else {
                times[ind]->tv_nsec*=1E5; //convert to real ns (from 1/10000 sec)
                ind++;
            }
        }
        closedir (dir);
    } else {
        logErrDate("%s: could not open directory %s\n",__func__,datadir);
        return -1;
    }
    qsort(filenames, ind, MAX_FILENAME, cmpfunc);  
    for(i=0;i<ind;i++){ //recalculate time struct
        sscanf(filenames[i][0],"%ld.%ld.nc",&times[i]->tv_sec, &times[i]->tv_nsec);
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

//--------------------------------------------------------------------------------------------------
int mkdir_filename(const char *dir_name){
//--------------------------------------------------------------------------------------------------
    struct stat st = {0};
    char dirname[255];
    strcpy(dirname,dir_name);
    
    uint h=1,i,j=0;
    char tmp[255];
    for(i=1; i<=strlen(dirname); i++){
        if(dirname[i]=='.')
            j=i;
        if(dirname[i]=='/' || (i==strlen(dirname) && j<h) ){
            h=i;
            snprintf(tmp, (size_t)(i+1), dirname);
            if (stat(tmp, &st) == -1) { //if dir does not exists, try to create it
                printf("Creating directory %s\n",tmp);
                if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXG | S_IXOTH)){
                    logErrDate("%s: could not create directory %s!\n",__func__,tmp);
                    //exit_all(-1);
                    return -1;
                }
            }       
        }
    } 
    return 0;
}

//----------------------------------------------------------------------
int main(int argc, char * argv[]){
//----------------------------------------------------------------------    
    char datadir[255];
    int i,ind;
    strcpy(datadir,"/home/pi/data/tmp/");
    
    // static array of file names e.g. ["1446396664.nc"]
    char filenames[MAX_FILES][MAX_FILENAME];   
    // pointer to first 
    struct timespec ts1[MAX_FILES];   // array of timespecs
    // struct timespec has members:
    //    time_t  tv_sec    seconds
    //    long    tv_nsec   nanoseconds        
    
    printf("%d files found.\n",getSortedFileList(datadir, &(filenames[0]), &ts1));
    
    struct tm tp=*(gmtime(&ts1[0]));
    printf("first file starts at: %04d/%02d/%02d %02d:%02d:%02d.%09ld\n", 
    tp.tm_year+1900, tp.tm_mon+1, tp.tm_mday, tp.tm_hour, tp.tm_min, 
    tp.tm_sec, ts1[0].tv_nsec);   //inclusive fractional sec
    
    struct tm tp1h; //next full hour (hour file limit)
    calcNextFullHour(&tp, &tp1h, 1);
    printf("in total %ld seconds to next full hour\n",date_difference(&tp, &tp1h));
    struct tm tp0h; //beginning of this hour (output file name)
    calcNextFullHour(&tp, &tp0h, 0);    

    char file_in_dir[255];
    char file_in[255];
    char file_out[255];
    //open first file to check length, sps, number of channels ...
    struct params parM, parS;
    sprintf(file_in_dir,"/home/pi/data/tmp/");
    sprintf(file_in,    "/home/pi/data/tmp/%s",filenames[0]);
    sprintf(file_out,   "/home/pi/data/%04d/%02d/%02d/%04d-%02d-%02dT%02d.nc",
    tp0h.tm_year+1900, tp0h.tm_mon+1, tp0h.tm_mday, 
    tp0h.tm_year+1900, tp0h.tm_mon+1, tp0h.tm_mday, tp0h.tm_hour);
    get_nc_params(file_in, &parM, 1);

    printf("outfile: %s\n",file_out);
    //open all further files (including the first), assert parameters are identical
    for(ind=0; date_difference(&tp, &tp1h)>0; ind++){
        tp=&times[ind];
        sprintf(file_in,"/home/pi/data/tmp/%s%s.nc",daystr,filenames[ind]);
        get_nc_params(file_in, &parS, 0);
            printf("%2d %s\n",ind,file_in);
    }
    ind=ind-1; //to recover index to last element
    
    //concat_nc_data(file_in_dir, 0, ind, file_out, &parS);

    
    
    
    
    //tp->tm_hour+=1;
    //mktime(tp); // normalize result
    
    return 0;
}


