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

#define MAX_FILES (1441+10) 
#define MAX_FILENAME (13+1) // "1446396664.nc"

#define CHECK_NC_ERR(strr) \
do { \
    if (status != NC_NOERR){ \
        logErrDate("%s: %s\n%s\nExit\n",__func__,strr,nc_strerror(status)); \
        return -1; \
    } \
} while(0);

struct params{
    struct timeval startEpoch;
    struct tm stime;
    float sps;
    int chnumber;
    size_t length;
};
#define MAX_CH_NUMBER (8)

int get_nc_params(const char *filename, struct params *par, int first);
int concat_nc_data(const char *file_in_dir, const int i_start, const int i_stop,
    const char *file_out, const struct params *par, 
    struct timespec (*fnts_p)[MAX_FILES]);
int cmpfunc_timespec(const void *a, const void *b);
int getSortedFileList(const char *datadir, struct timespec (*fnts_p)[MAX_FILES]);
long date_difference(struct tm *tm1, struct tm *tm2);
time_t calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour );
float standard_deviation(float *datap, int n);

int mkdir_filename(const char *dir_name);


int get_nc_params(const char *filename, struct params *par, int first)
{
    int status;
    int ncid;
    int dimid;
    static struct params parM;
    
    status = nc_open(filename, NC_NOWRITE, &ncid);
    if (status != NC_NOERR){ //possible errors: NC_NOMEM, NC_EHDFERR, NC_EDIMMETA, NC_ENOCOMPOIND
        logErrDate("%s: Could not open file %s to read!\n%s\nExit\n",__func__,filename,nc_strerror(status));
        return -1;
    }
    
    status = nc_inq_nvars(ncid, &(par->chnumber));
    CHECK_NC_ERR("nc_inq_nvars");
    
    status = nc_inq_dimid(ncid, "ch1", &dimid);
    CHECK_NC_ERR("nc_inq_dimid");
    
    status = nc_inq_dimlen(ncid, dimid, &(par->length));
    CHECK_NC_ERR("nc_inq_dimlen");

    status = nc_get_att_float(ncid, NC_GLOBAL, "sps", &(par->sps));
    CHECK_NC_ERR("nc_get_att_float");
    
    //get global attribute sampling start. String e.g. "2015-11-08 01:39:27.499968"
    char str[255];
    status = nc_get_att_text(ncid, NC_GLOBAL, "start", str);
    CHECK_NC_ERR("nc_get_att_text");

    str[26]='\0'; //truncate string since get_att_text does not append \0
    //sprintf(fracs,"%s",(str+20*sizeof(char)));
    if (sscanf((str+20*sizeof(char)),"%ld",&(par->startEpoch.tv_usec)) != 1){
        logErrDate("%s: attribute start in %s not valid! (%s)\n",
        __func__, filename, str);
    }
    str[19]='\0'; //purge fractional seconds    
    strptime(str, "%Y-%m-%d %H:%M:%S", &(par->stime));
    par->startEpoch.tv_sec=(long int)timegm(&par->stime);
      
    nc_close(ncid);
    
    //if output desired (first file, template)
    if (first){
        printf("%s: file %s read\n",__func__,filename);
        printf("\tstarts at: %04d/%02d/%02d %02d:%02d:%02d.%ld\n", 
        par->stime.tm_year+1900, par->stime.tm_mon+1, par->stime.tm_mday, 
        par->stime.tm_hour, par->stime.tm_min, par->stime.tm_sec, par->startEpoch.tv_usec);
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


int concat_nc_data(const char *file_in_dir, const int i_start, const int i_stop,
const char *file_out, const struct params *par, struct timespec (*fnts_p)[MAX_FILES]){

    int status;
    int ncid_in, ncid_out;
    int i, j, k;
    char file_in[255];
    char varstr[3+1]; //channel name e.g. "ch1"
    int number = (i_stop - i_start)+1;
    if (number < 1){
        logErrDate("%s: parameter error!\nExit\n",__func__);
        return -1;
    }
    
    if (mkdir_filename(file_out) < 0){
        
    }
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
        sprintf(file_in,"%s%ld.nc",file_in_dir,fnts_p[i]->tv_sec);
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


int cmpfunc_timespec( const void *aa, const void *bb) 
{
    struct timespec *a = (struct timespec *)aa;
    struct timespec *b = (struct timespec *)bb;
    if (a->tv_sec < b->tv_sec)
        return 0-(b->tv_sec < a->tv_sec);
    if (a->tv_sec > b->tv_sec)
        return (a->tv_sec - b->tv_sec);
    return 0;
}

int getSortedFileList(const char *datadir, struct timespec (*fnts_p)[MAX_FILES])
{
    DIR *dir;
    struct dirent *ent;
    int ind=0;
    long l;
    printf("reading directory %s...\n",datadir);
    if ((dir = opendir(datadir)) != NULL) {
        while ((ent=readdir(dir)) != NULL) { //print all the files and directories within directory 
            if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            if (sscanf(ent->d_name,"%ld.nc",&l) != 1){
                logErrDate("%s: not a valid file %s\n",__func__,ent->d_name); 
            }
            else {
                (*fnts_p)[ind].tv_sec = (time_t)l;
                ind++;
            }
        }
        closedir (dir);
    } else {
        logErrDate("%s: could not open directory %s\n",__func__,datadir);
        return -1;
    }
    qsort(fnts_p, ind, 0, cmpfunc_timespec);       
    return ind;
}

//----------------------------------------------------------------------
long date_difference(struct tm *tm1, struct tm *tm2){
//----------------------------------------------------------------------
    time_t time1;
    time_t time2;   
    time1 = mktime(tm1);
    time2 = mktime(tm2);
    return ( (long int) difftime(time2, time1));
}


int mkdir_filename(const char *dir_name)
{
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
                    return -1;
                }
            }       
        }
    } 
    return 0;
}


time_t calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour)
{
    *(tp1h) = *((struct tm *)now);
    tp1h->tm_hour = now->tm_hour+hour;
    tp1h->tm_min = 0;
    tp1h->tm_sec = 0;
    time_t retval = timegm(tp1h); //normalize (using UTC)
    
    char tmpstr[255];
    strftime(tmpstr, sizeof(tmpstr), "%F %T %z", tp1h);        
    if (hour==1)
        printf("next full hour at:       %s\n", tmpstr);
    if (hour==0)
        printf("this hour started at:    %s\n", tmpstr);
                
    return retval;
}

int main(int argc, char * argv[])
{
    int i,ind;
    char file_in_dir[255];
    char file_out_dir[255];
    sprintf(file_in_dir, "/home/pi/data/tmp/");
    sprintf(file_out_dir,"/home/pi/data/");
    
    struct timespec fnts[MAX_FILES];   // array of timespecs = filenames
    // struct timespec has members:
    //    time_t  tv_sec    #seconds
    //    long    tv_nsec   #nanoseconds        
    
    i = getSortedFileList(file_in_dir, &fnts);
    printf("%d files (>%i hours) found.\n",i,i/60);
    if (i<=0){
        logErrDate("No files to process!?\n");
        exit_all(-1);
    }
    
    struct tm tp=*(gmtime(&fnts[0].tv_sec));
    char tmpstr[255];
    strftime(tmpstr, sizeof(tmpstr), "%F %T %z", &tp);
    printf("first file (%ld.nc) starts at: %s\n",fnts[0].tv_sec,tmpstr); 
    
    //calculate next full hour (hour file limit)
    struct tm tp1h;
    time_t t1h = calcNextFullHour(&tp, &tp1h, 1);    
    printf("in total %ld seconds to next full hour\n",(long)(t1h-fnts[0].tv_sec));
    
    //beginning of this hour (output file name)    
    struct tm tp0h; 
    time_t t0h = calcNextFullHour(&tp, &tp0h, 0);


    char file_in[255];
    char file_out[255];
    char daystr[255];
    //open first file to check length, sps, number of channels ...
    struct params parM, parS;
    
    sprintf(file_in,    "%s%ld.nc",file_in_dir,fnts[0].tv_sec);
    sprintf(daystr, "%04d/%02d/%02d", tp0h.tm_year+1900, tp0h.tm_mon+1, tp0h.tm_mday);
    sprintf(file_out, "%s%s/%04d-%02d-%02dT%02d.nc", file_out_dir,daystr,
        tp0h.tm_year+1900, tp0h.tm_mon+1, tp0h.tm_mday, tp0h.tm_hour);
    if (get_nc_params(file_in, &parM, 1) <0){
        logErrDate("Error while reading parameters!\nExit\n");
        exit_all(-1);        
    }

    printf("\n");
    printf("outfile: %s\n",file_out);
    //open all further files (including the first), assert parameters are identical
    for(ind=0; (t1h-fnts[ind].tv_sec)>0; ind++){
        sprintf(file_in,"%s%ld.nc",file_in_dir,fnts[ind].tv_sec);
        if (get_nc_params(file_in, &parS, 0) < 0){
            logErrDate("%d. file (%s) parameter assert error!\n Skipping\n",ind,file_in);
            //TODO: delete file
        }
    }
    ind=ind-1; //to recover index to last element
    printf("%d files will be concatenated.\n",ind);
    
    concat_nc_data(file_in_dir, 0, ind, file_out, &parS);

    return 0;
}


