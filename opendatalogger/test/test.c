/* compile using 
 * gcc -o test test.c -lm -lnetcdf -lrt -Wall
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
    
    

struct params{
    /* struct timeval has following members:
     * time_t tv_sec
     * long int tv_usec
     */
    struct timeval startEpoch;     
    
    /* stuct tm (broken down time format) has following members
     * int tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec ... 
     */
    struct tm stime;
    
    float sps;     //sampling rate (1/s)
    int chnumber;  //amount of channels (usually 2 4 or 8)
    size_t length; //amount of samples in the file (usually sps * file duration)
};
#define MAX_CH_NUMBER (8)


float standard_deviation(float *datap, long n);
int get_nc_params(const char *file_in_dir, struct params *par, int first, 
    char *startstr, struct timespec *fnts_p);
int concat_nc_data(const char *file_in_dir, const int count, const char *file_out,
    const struct tm *file_out_time, long *trunc_p, long *nana_p, struct timespec (*fnts_p));
int cmpfunc_timespec(const void *a, const void *b);
int cmpfunc_time(const void *aa, const void *bb);
int getSortedFileList(const char *datadir, struct timespec *fnts_p);
long date_difference(struct tm *tm1, struct tm *tm2);
int delete_file(char *file_in_dir, struct timespec *ts);
time_t calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour );
long arraysum( long *arr_p, long len);

int mkdir_filename(const char *dir_name);
long fsize(char *filename) ;


long fsize(char *filename) 
{
    struct stat st;

    if (stat(filename, &st) == 0)
        return (long)st.st_size;

    logErrDate("Cannot determine size of %s\n", filename);

    return -1;
}

float standard_deviation(float *datap, long n) 
{
    double mean=0.0, sum_deviation=0.0, stddev, min=__DBL_MAX__, max=__DBL_MIN__;
    double a;
    int i;
    for(i=0; i<n; i++){
        a=(double)(*(datap+i));
        mean+=a;
        if (a > max)
            max = a;
        else if(a < min)
            min = a;
    }
    mean=mean/(double)n;
    for(i=0; i<n;++i){
        sum_deviation+=((double)(*(datap+i))-mean)*((double)(*(datap+i))-mean);
    }
    stddev=sqrtl(sum_deviation/(double)n);
    printf("\tmean=%.4f, std_dev=%f, max=%.4f, min=%.4f\n",mean,stddev, max,min);
    return (float)stddev;
}


int get_nc_params(const char *file_in_dir, struct params *par, int first, 
    char *startstr, struct timespec *fnts_p)
{
    int status;
    int ncid;
    int dimid;
    static struct params parM;
    char filename[255];
    
    if (file_in_dir == NULL || par == NULL || fnts_p == NULL){
        logErrDate("%s: parameter error\nExit\n",__func__);
        return -1;
    }
    sprintf(filename,"%s%ld.nc",file_in_dir,fnts_p->tv_sec);
    
    status = nc_open(filename, NC_NOWRITE, &ncid);
    if (status != NC_NOERR){
        logErrDate("%s: Could not open file %s to read!\n%s\nExit\n",
        __func__,filename,nc_strerror(status));
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

    //TODO: not nice
    str[26]='\0'; //truncate string since get_att_text does not append \0  ???
        
    //if 4th argument startstr != NULL, we will copy the string there
    if (startstr != NULL)
        strcpy(startstr,str);

    //sprintf(fracs,"%s",(str+20*sizeof(char)));
    if (sscanf((str+20*sizeof(char)),"%ld",&(par->startEpoch.tv_usec)) != 1){
        logErrDate("%s: attribute \"start\" in %s not valid! (%s)\n",
        __func__, filename, str);
    }
    
    //copy nsec to array pointer given by argument 
    fnts_p->tv_nsec = par->startEpoch.tv_usec * 1000;
    
    //purge fractional seconds    
    str[19]='\0'; 
    strptime(str, "%Y-%m-%d %H:%M:%S", &(par->stime));
    par->startEpoch.tv_sec=(long int)(timegm(&par->stime));
    
    //file name check
    if (par->startEpoch.tv_sec != fnts_p->tv_sec){
        logErrDate( "%s: corrupt data! file name does not match start date!\n"
                    "filename=%ld.nc,  start=%ld (%s)\n",__func__,
                    fnts_p->tv_sec, par->startEpoch.tv_sec, str);
        return -1;
    }
      
    status = nc_close(ncid);
    CHECK_NC_ERR("nc_close");
    
#ifdef DEBUG    
    //diagnostic output only
    printf("str: %s | startEpoch: %ld.%ld\n",str,par->startEpoch.tv_sec,par->startEpoch.tv_usec);    
#endif    
    //if output desired (first file, template)
    if (first){
        printf("%s: first file %s read\n",__func__,filename);
        fprintf(stdout,"\tstarts at:                     %s.%ld\n",str,par->startEpoch.tv_usec);
        printf("\tsps: %f Hz\n",par->sps);
        printf("\tchannels: %d\n",par->chnumber);
        printf("\tlength: %d samples (%gs)\n",par->length, roundf((float)par->length/par->sps));
        memcpy(&parM, par, sizeof(struct params));
    }
    else {
        //assert if parameters are equal through different files
        if (parM.sps - par->sps > FLT_EPSILON || parM.chnumber != par->chnumber 
        || parM.length != par->length ) {
            logErrDate("%s: severe error: parameter mismatch at file %s\n",__func__,filename);
            return -1;
        }
        
    }
    return 0;
}


int concat_nc_data(const char *file_in_dir, const int count, const char *file_out,
    const struct tm *file_out_time, long *trunc_p, long *nana_p, struct timespec (*fnts_p))
{
    int status;
    int ncid_in, ncid_out;
    int j, k, m;
    char file_in[255];
    char varstr[3+1]; //channel name e.g. "ch1"
    if (count < 2 || file_in_dir == NULL || file_out == NULL || 
        trunc_p == NULL || nana_p == NULL || fnts_p == NULL){
        logErrDate("%s: parameter error!\nExit\n",__func__);
        return -1;
    }
    if (count < 60){
        printf("%s warn: too few files for a complete hour!\n"
               "   info: start time will be adjusted, and the file filled up with NaN\n",__func__);
    }
    
    if (mkdir_filename(file_out) < 0){
        logErrDate("%s: Could not create output directory!\nExit\n",__func__);
        return -1;
    }

    // open file to write
    status = nc_create(file_out, NC_CLOBBER|NC_NETCDF4, &ncid_out);
    if (status != NC_NOERR){ //possible errors: NC_ENOMEM, NC_EHDFERR, NC_EFILEMETA
        logErrDate("%s: Could not open file %s to write! nc_create: %s\nExit\n",
        __func__,file_out,nc_strerror(status));
        return -1;
    }
    
    long totallen=arraysum(nana_p,MAX_FILES)+arraysum(trunc_p,MAX_FILES);
    
    /* determine when the first file begins (usually in the hour before the processed hour) */
    sprintf(file_in,"%s%ld.nc", file_in_dir,fnts_p->tv_sec);
    char startstr[255], tmpstr[255];
    struct params par;
    if ((status=get_nc_params(file_in_dir, &par, 0, startstr, fnts_p)) < 0){
        logErrDate("%s: parsing \"start\" string in %s caused error: %s\nExit\n",
        __func__,file_in,nc_strerror(status));
        return -1;
    }
    
    
    //allocate memory for 1 hour input files 
    float *chp[MAX_CH_NUMBER];
    size_t nnumb = totallen + 100000;
    for (k=0; k<par.chnumber; k++){
        chp[k] = calloc(nnumb, sizeof(float));
        if (chp[k] == NULL){
            logErrDate("%s: Could not allocate memory of size %d kB!\nExit\n",
            __func__, (nnumb*sizeof(float)/1024)+1);
            return -1;            
        }
        //and initialize it with NaN
        for (m=0; m<nnumb; m++)
            *(chp[k]+m)=NAN;
    }
    long first_file_samp=0;
    int out_dimid[MAX_CH_NUMBER];       // dimension id
    int out_varid[MAX_CH_NUMBER];        // variable id
    
    for (k=0; k<par.chnumber; k++){ //for as many channels as in file (1,2,4 or 8)
        sprintf(varstr,"%s%d","ch",k+1); //ch1, ch2 ...
        // define dimensions 
        status = nc_def_dim(ncid_out, varstr, totallen, &out_dimid[k]);
        if (status != NC_NOERR){
            logErrDate("%s: nc_def_dim param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            FREE_AND_RETURN(-1);
        }
        // define variables
        status = nc_def_var(ncid_out, varstr, NC_FLOAT, 1, &out_dimid[k], &out_varid[k]);
        if (status != NC_NOERR){
            logErrDate("%s: nc_def_var param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            FREE_AND_RETURN(-1);
        } 
        // assign per-variable attributes
        status = nc_put_att_text(ncid_out, out_varid[k], "units", 2, "mV");
        if (status != NC_NOERR){
            logErrDate("%s: param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            FREE_AND_RETURN(-1);
        }
        //define fill variable
        float nan=NAN;
        status = nc_def_var_fill(ncid_out, out_varid[k], 0, &nan);
        if (status != NC_NOERR){
            logErrDate("%s: param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            FREE_AND_RETURN(-1);
        }        
    }
    status = nc_put_att_float(ncid_out, NC_GLOBAL, "sps", NC_FLOAT, 1, &(par.sps));
    CHECK_NC_ERR_FREE("nc_put_att_float (sps)");    
    

    //usual way, the first file stretches over the hour border
    printf("%s:\n",__func__);
    if (*(trunc_p) < par.length){
        
        first_file_samp = *trunc_p; // variable first_file_samp will be evaluated later
        
        printf("from the first file %8ld samples (%.2fs) will be taken\n",
                first_file_samp,(float)(first_file_samp)/par.sps); 
        printf("      starts exactly at:       %s\n",startstr);
        
        //updating start string to format "2015-11-08 14:00:00.000000"
        strftime(tmpstr, sizeof(tmpstr), "%F %T.000000", file_out_time);
        status = nc_put_att_text(ncid_out, NC_GLOBAL, "start", strlen(tmpstr), tmpstr);
        CHECK_NC_ERR_FREE("nc_get_att_text");  
   
    } else { //any other case e.g. first file starts at 02:12:34 (HH:MM:SS)
        //copy the "start" string from the very first input file
        printf( "warn: the first file does not contain the start of the hour!\n");
        
        status = nc_put_att_text(ncid_out, NC_GLOBAL, "start", 26, startstr);
        CHECK_NC_ERR_FREE("nc_get_att_text");
    }
    
    //leaving define mode
    status = nc_enddef(ncid_out);
    CHECK_NC_ERR_FREE("nc_enddef");
    
    long ins_cnt=0, ins_nan;
    
    // for as many input files as given -------------
    int varid[MAX_CH_NUMBER];
    for (j=0; j<count; j++){
        sprintf(file_in,"%s%ld.nc",      file_in_dir,(fnts_p+j)->tv_sec);

        status = nc_open(file_in, NC_NOWRITE, &ncid_in);
        if (status != NC_NOERR){ 
            logErrDate("%s: Could not open file %s to read! nc_open: %s\nExit\n",
            __func__,file_in,nc_strerror(status));
            FREE_AND_RETURN(-1);
        }
        
        //read from the input files
        for (k=0; k<par.chnumber; k++){ //for as many channels as in file (1,2,4 or 8)
            sprintf(varstr,"%s%d","ch",k+1); //ch1, ch2 ...
            status = nc_inq_varid(ncid_in, varstr, &varid[k]);
            if (status != NC_NOERR){
                logErrDate("%s: Could not find variable %s in %s! nc_inq_varid: %s\nExit\n",
                __func__, varstr, file_in, nc_strerror(status));
                FREE_AND_RETURN(-1);
            }
            
            //insert NaN if neccessary
            ins_nan=arraysum(nana_p,j+1); //cumulative sum
            //insert values
            status = nc_get_var_float(ncid_in, varid[k], (chp[k]+j*par.length + ins_nan));
            if (status != NC_NOERR){
                logErrDate("%s: Error while reading variable %s in %s! nc_get_var_float: %s\nExit\n",
                __func__, varstr, file_in, nc_strerror(status));
                FREE_AND_RETURN(-1);
            }
            printf("|");
            fflush(stdout);
            
        }
        //update inserted values counter 
        ins_cnt+=*(trunc_p+j)+*(nana_p+j); //values + NaNs        
        
        status=nc_close(ncid_in);
        CHECK_NC_ERR_FREE("nc_close inputs");
        
    } // end for as many input files
    
    //add the NaN count at the end (if some)
    ins_cnt+=*(nana_p+j); //values + NaNs 
    
    printf("\n\n");
    //TODO correct by start moved
    printf("info: nominal file length:    %8ld\n",(long)par.length*60);
    printf("      inserted NaN:           %8ld\n",arraysum(nana_p,MAX_FILES));
    printf("      file length (incl. NaN):%8ld == %8ld\n",totallen,ins_cnt);
    
    
    /* Correction of the beginning
     * for the first file it might be neccessary to add only the last 'first_file_samp' amount
     * so the pointer will be moved by (par.length-first_file_samp)
     */
    int offs=0;
    if (first_file_samp){
        offs=(par.length-first_file_samp);
        printf("info: start index moved by %d\n",offs);
    }
     
    size_t s_start[] = {0};
    size_t s_count[] = {totallen};    
    
    // write hughe array to output file
    for (k=0; k<par.chnumber; k++){ //for as many channels
        status = nc_put_vara_float(ncid_out, out_varid[k], s_start, s_count, chp[k]+offs);
        if (status != NC_NOERR){
            logErrDate("%s, nc_put_vara_float ch%d: %s\nExit\n",__func__,k+1,nc_strerror(status));
            FREE_AND_RETURN(-1);
        }
        status = nc_sync(ncid_out); //ensure it is written to disk        
        CHECK_NC_ERR_FREE("nc_sync");
    }
        
    status = nc_close(ncid_out); //close file
    CHECK_NC_ERR_FREE("nc_close output");

#ifdef DEBUG_STAT
    printf("============================== stats ==============================\n");
    for (k=0; k<par.chnumber; k++){
        printf("ch%i:",k+1);
        standard_deviation(chp[k],count*par.length);
    }
#endif
    //get filesize of output file
    long fsize_out=fsize((char *)file_out);
    printf("\nfile %s written. Size: %.2fMiB\n",file_out,fsize_out/1024.0/1024.0);

    //get the filesize of the last input file
    long fsize_in=fsize((char *)file_in); //last file size

    
    //file sanity check 
    if (count>58 && abs(j*fsize_in - fsize_out) > 512*1024){ //less than 0.5MiB deviation
        printf("file sanity check numb:%d, fsize_in: %.2fMiB\n",j,fsize_in/1024.0/1024.0);        
        logErrDate("%s: file sizes seems not to be ok!\nNo deletion! Exit\n",__func__);
        FREE_AND_RETURN(-1);
    }

    
    FREE_AND_RETURN(j);
}


int cmpfunc_timespec(const void *aa, const void *bb) 
{
    struct timespec *a = (struct timespec *)aa;
    struct timespec *b = (struct timespec *)bb;
    if (a->tv_sec < b->tv_sec)
        return 0-(b->tv_sec - a->tv_sec);
    if (a->tv_sec > b->tv_sec)
        return (a->tv_sec - b->tv_sec);
    return 0;
}

int cmpfunc_time(const void *aa, const void *bb) 
{
    time_t *a = (time_t *)aa;
    time_t *b = (time_t *)bb;
    if ( *a < *b)
        return 0-(*b - *a);
    if ( *a > *b)
        return (*a - *b);
    return 0;
}

int getSortedFileList(const char *datadir, struct timespec *fnts_p)
{
    DIR *dir;
    struct dirent *ent;
    int ind=0;
    long l;
    printf("reading directory %s...\n",datadir);
    if ((dir = opendir(datadir)) != NULL) {
        while ((ent=readdir(dir)) != NULL) { 
            if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            ind++;
        }
        closedir (dir);
    } else {
        logErrDate("%s: could not open directory %s\n",__func__,datadir);
        return -1;
    }    
    printf("   %d files counted\n",ind);
    
    time_t *p;
    p = (time_t *)calloc(ind, sizeof(time_t));
    if (p == NULL){
        logErrDate("%s: could not allocate memory\n",__func__);
        return -1;
    }
    
    ind=0;    
    if ((dir = opendir(datadir)) != NULL) {
        while ((ent=readdir(dir)) != NULL) { //print all the files and directories within directory 
            if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            if (sscanf(ent->d_name,"%ld.nc",&l) != 1){
                logErrDate("%s: not a valid file %s\n",__func__,ent->d_name); 
                continue;
            }
        
            *(p+ind) = (time_t)l;
            ind++;
        }
        closedir (dir);
    }
    qsort(p, ind, sizeof(time_t), cmpfunc_time);

    int i;
    for (i=0; i<MAX_FILES && i<ind; i++){
        (fnts_p+i)->tv_sec = *(p+i);
    }
    
    free(p);
    return ind;
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

int delete_file(char *file_in_dir, struct timespec *ts)
{
    static int cnt;
    char str[255];
    sprintf(str,"%s%ld.nc",file_in_dir,ts->tv_sec);    
    
    if (cnt++ < 60){        //TODO remove
        if(unlink(str)==-1){
            logErrDate("%s: failed to delete file %s\n",__func__,str);
            return -1;
        } 
        else {
            printf("%s deleted\n",str);
            return 0;
        }
    }
    else {
        printf("%s: You really want to delete so many files??? %s\n",__func__,str);
        return -1;
    }
}

time_t calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour)
{
    *(tp1h) = *((struct tm *)now);
    tp1h->tm_hour = now->tm_hour+hour;
    tp1h->tm_min = 0;
    tp1h->tm_sec = 0;
    return timegm(tp1h); //normalize (using UTC)
}

int main(int argc, char * argv[])
{
    int i,j,ind;
    char file_in_dir[255];
    char file_out_dir[255];
    sprintf(file_in_dir, "/home/pi/data/tmp/");
    sprintf(file_out_dir,"/home/pi/data/");
    
    char tmpstr[255];

    /* struct timespec has members:
     *    time_t  tv_sec    //seconds
     *    long    tv_nsec   //nanoseconds     
     */
    struct timespec fnts[MAX_FILES];   // array of timespecs = filenames
    long nana[MAX_FILES]={0};   // indicator of NaN to be inserted when missing files
    long trunc[MAX_FILES]={0};   // indicator for truncating files
    
    /* struct tm is broken-down time */
    struct tm tp;   
    
    i = getSortedFileList(file_in_dir, &fnts[0]);
    printf("%d files (>%i hours) found.\n",i,i/60);
    if (i<=0){
        logErrDate("No files to process!?\n");
        return -1;
    }
#ifdef DEBUG_LIST  
    //diagnostic output: list all files (sorted)
    for (ind=0; ind<i && ind<MAX_FILES; ind++){
        tp=*(gmtime(&fnts[ind].tv_sec));
        strftime(tmpstr, sizeof(tmpstr), "%F %T", &tp);
        printf("%4d. %ld.nc (starts at:  %s)\n",ind,fnts[ind].tv_sec,tmpstr);          
    }
#endif        
    
    //display the starting time of the first file
    /*struct tm*/
    tp=*(gmtime(&fnts[0].tv_sec)); //convert to broken down time format
    strftime(tmpstr, sizeof(tmpstr), "%F %T %z", &tp);
//printf("first file (%ld.nc) starts at:  %s\n",fnts[0].tv_sec,tmpstr); 

    //determine beginning of this hour (used for output file name)
    struct tm tp0h; 
    calcNextFullHour(&tp, &tp0h, 0);
    //however when file starts within 60 seconds to the next hour, ceil
    if (tp.tm_min == 59){
        calcNextFullHour(&tp, &tp0h, 1);
    }
    strftime(tmpstr, sizeof(tmpstr), "%F %T %z", &tp0h);
    printf("beginning of the hour to be processed: %s\n",tmpstr); 
    
    
    //calculate next full hour (hour file limit)
    struct tm tp1h;
    time_t t1h = calcNextFullHour(&tp0h, &tp1h, 1);        
    strftime(tmpstr, sizeof(tmpstr), "%F %T %z", &tp1h);
    //printf("end of the hour to be processed:       %s\n", tmpstr);
    printf("in total %ld seconds (%.2f min) to   %s\n",
    (long)(t1h-fnts[0].tv_sec),(t1h-fnts[0].tv_sec)/60.0,tmpstr);
    
    

    char file_in[255];
    char file_out[255];
    char daystr[255];
    char startstr[255];
    //open first file to check length, sps, number of channels ...
    struct params parM, parS;
    
    sprintf(file_in,    "%s%ld.nc",file_in_dir,fnts[0].tv_sec);
    sprintf(daystr, "%04d/%02d/%02d", tp0h.tm_year+1900, tp0h.tm_mon+1, tp0h.tm_mday);

    if (get_nc_params(file_in_dir, &parM, 1, NULL, &fnts[0]) <0){
        logErrDate("Error while reading parameters in %s!\nExit\n",file_in);
        exit_all(-1);        
    }

    sprintf(file_out, "%s%s/%04d-%02d-%02dT%02d.nc", file_out_dir,daystr,
        tp0h.tm_year+1900, tp0h.tm_mon+1, tp0h.tm_mday, tp0h.tm_hour);    
    printf("\noutfile: %s\n",file_out);

    
    /* open all further files (including the first) within that hour,
     * and assert parameters are identical. 
     * Also count the predicted length of the file and print the file 
     * names in HH:MM:SS format (as in start string attribute).
     */
    printf("\nfile check for\n");
    printf(" # |        start date         | samples |    diff (us)| inserted s\n");
    printf("------------------------------------------------------------------\n");
    long totallen=0;
    float dec_sec, diff, diff2;
    for(ind=0; (t1h-fnts[ind].tv_sec)>0; ind++){
        sprintf(file_in,"%s%ld.nc",file_in_dir,fnts[ind].tv_sec);       
        if (get_nc_params(file_in_dir, &parS, 0, startstr, &fnts[ind]) < 0){
            logErrDate("%d. file (%s) parameter assert error!\n Skipping\n",ind,file_in);
            delete_file(file_in_dir, &fnts[ind]);
            return -1;
        }
        tp=*(gmtime(&fnts[ind].tv_sec)); //convert to broken down time format
        
        /* Beginning or End
         * usually, the hour starts with the file beginning in the 59th 
         * second of the previous hour, or the last file starts in 59th sec.
         */
        //calculate decimal seconds (02:59:32.544123 -> 32.544123) 
        dec_sec=(float)parS.stime.tm_sec + (float)(parS.startEpoch.tv_usec*1E-6);
        if (tp.tm_min == 59){
            //calculate amount of NaNs to be inserted   
            if (ind==0) //at the beginning
                trunc[ind]=lroundf(dec_sec*parS.sps);
            else
                trunc[ind]=lroundf((60.0-dec_sec)*parS.sps);
            totallen+= trunc[ind];
            printf("*%2d: %s\t%8ld\n",ind+1,startstr,  trunc[ind]);
            
        } else {
            /* in between we have to check for continousity. If a file is
             * missing insert as many NaN as needed.
             */
            trunc[ind]=(long)parS.length;
            if (ind>0){ //difference make no sense at the first file
                diff=(float)(fnts[ind].tv_sec-fnts[ind-1].tv_sec-60)*1E9 + 
                     (float)(fnts[ind].tv_nsec-fnts[ind-1].tv_nsec);
                //printf("diff:  %+4.0f\t",diff/1E3);

                if (diff > 1E9/parS.sps) { //if missing more than 1 sample 
                    nana[ind]=lroundf(diff*parS.sps*1E-9);
 
                    printf("   : filling up with NaN:\t%8ld\t\t%8.2f\n",nana[ind],nana[ind]/parS.sps);
                    //update inserted values counter
                    totallen+=nana[ind];
                }
            }              
            totallen+=parS.length;
            
            printf(" %2d: %s\t%8ld",ind+1,startstr,(long)parS.length);
            //calculate jitter
            if (ind>0){ //difference make no sense at the first file
                diff=(float)(fnts[ind].tv_sec-fnts[ind-1].tv_sec-60)*1E9 + 
                     (float)(fnts[ind].tv_nsec-fnts[ind-1].tv_nsec);
#ifdef DEBUG_DIFF       
                    if (ind>1){ //difference of differences make no sense at the first 2 files
                        printf("\tddiff: %+4.0f | %+4.0f",(diff-diff2)/1E3, (diff+diff2)/1E3);
                    }
                    diff2=diff;
#endif              
                if (diff < 1E9) //display only if jitter < 1s
                    printf("\t%+4.0f",diff/1E3);
            }
            printf("\n");

        }
    }
    //check if completed, else fill up with NaN
    float dec_min=60.0*(float)parS.stime.tm_min + dec_sec + 60.0; //add 1 minute, since end of the file
    nana[ind]=lroundf((3600.0-dec_min)*parS.sps)+1;
    if (nana[ind] > 0){ //if the last file does not stretch over hour border
        totallen+=nana[ind];
        printf("   : filling up with NaN:\t%8ld\t\t%8.2f\n",nana[ind], nana[ind]/parS.sps);
    }    
    printf("------------------------------------------------------------------\n");
    printf("end: %s %9ld\n",tmpstr,totallen);
    
    //print the next file which IS NOT in the hour anymore
    struct params upar;
    sprintf(file_in,"%s%ld.nc",file_in_dir,fnts[ind].tv_sec);       
    if (get_nc_params(file_in_dir, &upar, 0, startstr, &fnts[ind]) < 0){
        logErrDate("further %d. file (%s) parameter assert error!\n",ind,file_in);
    }
    printf("(  : %s) <- not included\n",startstr);
    
    
    printf("\n%d files will be concatenated.\n\n",ind);
    
#ifdef DEBUG_PARRAY    
    for (i=0; i<ind+1 && i<MAX_FILES; i++){
        printf("\t%2d tr:%8ld  NaN:%8ld\n",i+1,trunc[i],nana[i]);
    }
    printf("arraysum(trunc):  %8ld\n",arraysum(trunc, MAX_FILES));
    printf("arraysum(nana):   %8ld\n",arraysum(nana, MAX_FILES));
    printf("arraysum(tr+nana):%8ld\n",arraysum(trunc, MAX_FILES)+arraysum(nana, MAX_FILES));
#endif

    /* concatenation:
     * int concat_nc_data(const char *file_in_dir, const int count, const char *file_out,
     *      const struct tm *file_out_time, long *trunc_p, long *nana_p, struct timespec (*fnts_p)){
     */
    i=0;
    if (ind>1){
        if ((i=concat_nc_data(file_in_dir, ind, file_out, &tp1h, trunc, nana, fnts)) < 0){
            logErrDate("Error while concatenating!\n");
        }
    }     
    /* after EVERY run the first file will be deleted */
    if(delete_file(file_in_dir, &fnts[0])){
        logErrDate("failed to delete files.\nExit\n");
        return -1;
    }
    
    if (ind < 2){
        printf("nothing to do!\n Exit\n");
        return 0;
    }
    
// dry run when 
// return -1;
    
    /* if exactly as many files as requested (or at highest one fewer) 
     * were concatenated, delete raw files, except the last one 
     * (it might contain data for the next hour)
     */
    if (i >= 2) {
        for (j=1; j<i-1 && j<60; j++){
            if(delete_file(file_in_dir, &fnts[j])){
                logErrDate("failed to delete file %ld.nc.\nExit\n",fnts[j].tv_sec);
                return -1;
            }
        }
        //check last processed file
        tp=*(gmtime(&fnts[i-1].tv_sec)); //convert to broken down time format
        printf("last file %02d:%02d:%02d\n",tp.tm_hour,tp.tm_min,tp.tm_sec);
        if (tp.tm_min != 59){
            if(delete_file(file_in_dir, &fnts[i-1])){
                logErrDate("failed to delete files.\nExit\n");
                return -1;
            }
            j++; 
        }
        printf("%d files deleted from %s\n",j,file_in_dir);
    }
    else {
        printf("Requested %d files to concatenate, but only %d performed.\n",ind,i);
        fprintf(stderr,"Manual action needed!\n");
    }
    return 0;
}

long arraysum( long *arr_p, long len)
{
    long sum=0,i;
    for (i=0; i<len; i++)
        sum+=*(arr_p+i);
    return sum;
}
