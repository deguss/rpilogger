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

static long nodelete;


float standard_deviation(float *datap, long n);
int get_nc_params(const char *file_in_dir, struct params *par, int first, 
    char *startstr, struct timespec *fnts_p);
int concat_nc_data(const char *file_in_dir, const int count, const char *file_out, 
        struct tm *file_out_time, const struct params *par, struct timespec (*fnts_p));
int cmpfunc_timespec(const void *a, const void *b);
int getSortedFileList(const char *datadir, struct timespec *fnts_p);
long date_difference(struct tm *tm1, struct tm *tm2);
time_t calcNextFullHour(const struct tm *now, struct tm *tp1h, int hour );


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
    
    //if 4th argument startstr != NULL, we will copy the string there
    if (startstr != NULL)
        strcpy(startstr,str);

    str[26]='\0'; //truncate string since get_att_text does not append \0
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
    //diagnostit output only
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
    struct tm *file_out_time, const struct params *par, struct timespec (*fnts_p)){

    int status;
    int ncid_in, ncid_out;
    int j, k, m;
    char file_in[255];
    char varstr[3+1]; //channel name e.g. "ch1"
    if (count < 2 || file_in_dir == NULL || file_out == NULL || par == NULL){
        logErrDate("%s: parameter error!\nExit\n",__func__);
        return -1;
    }
    if (count < 60){
        printf("warn %s: it seems to have too few files for concatenation. "
               "start time will be adjusted, and the file filled up with "
               "NaN values to the next full hour.\n",__func__);
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
    
    //allocate memory for 1 hour input files 
    float *chp[MAX_CH_NUMBER];
    size_t nnumb = par->length*(60+1);
    for (k=0; k<par->chnumber; k++){
        chp[k] = calloc(nnumb, sizeof(float));
        if (chp[k] == NULL){
            logErrDate("%s: Could not allocate memory of size %d kB!\nExit\n",
            __func__, (nnumb*sizeof(float)/1024)+1);
            return -1;            
        }     
    }
    
    int out_dimid[MAX_CH_NUMBER];       // dimension id
    int out_varid[MAX_CH_NUMBER];        // variable id
    
    for (k=0; k<par->chnumber; k++){ //for as many channels as in file (1,2,4 or 8)
        sprintf(varstr,"%s%d","ch",k+1); //ch1, ch2 ...
        // define dimensions 
        status = nc_def_dim(ncid_out, varstr, par->length*60, &out_dimid[k]);
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
            logErrDate("%s: param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            return -1;
        }
        status = nc_put_att_text(ncid_out, out_varid[k], "_FillValue", 3, "NaN");
        if (status != NC_NOERR){
            logErrDate("%s: param %s: %s\nExit\n",__func__,varstr,nc_strerror(status));
            return -1;
        }        
    }
    
    /* determine when the first file begins (usually in the hour before the processed hour) */
    
    //filename of the first file
    sprintf(file_in,"%s%ld.nc", file_in_dir,fnts_p->tv_sec);
    char startstr[255], tmpstr[255];
    struct params upar;
    if ((status=get_nc_params(file_in_dir, &upar, 0, &startstr[0], fnts_p)) < 0){
        logErrDate("%s: parsing \"start\" string in %s caused error: %d\nExit\n",
        __func__,file_in,status);
        return -1;
    }
    long first_file_samp=0;    
    //if file starts within 60 seconds to the next hour, ceil
    if (upar.stime.tm_min == 59){ //TODO assuming no leap second!
        float newtime=(float)upar.stime.tm_sec + (float)(upar.startEpoch.tv_usec*1E-6);
        first_file_samp=round(newtime*upar.sps);
        
        //updating start string to format "2015-11-08 14:00:00.000000"
        strftime(startstr, sizeof(startstr), "%F %T.000000", file_out_time);
        //just for displaying the date of the first file
        strftime(tmpstr, sizeof(tmpstr), "%F %T %z", gmtime(&fnts_p->tv_sec));
        
        status = nc_put_att_text(ncid_out, NC_GLOBAL, "start", strlen(startstr), startstr);
        CHECK_NC_ERR("nc_get_att_text");        
        
        //adding samples
        printf("taking the last %ld samples (%.6fs) from the first file (%s)\n",first_file_samp, newtime, tmpstr);
        // variable first_file_samp will be evaluated later
               
   
    } else {
        /* write in the output file the "start" string from the
         * very first input file, and give a warning
         */
        printf( "warn: the first file does not contain the start of the hour!\n"
                "              starts exactly at:       %s\n",startstr);
        status = nc_put_att_text(ncid_out, NC_GLOBAL, "start", strlen(startstr), startstr);
        CHECK_NC_ERR("nc_get_att_text");
    }
    
    //leaving define mode
    status = nc_enddef(ncid_out);
    CHECK_NC_ERR("nc_enddef");
    

    float diff, diff2;
    float nan=NAN;
    long ins_nan=0, ins_allnan=0, ins_cnt=0;
    
    // for as many input files as given -------------
    int varid[MAX_CH_NUMBER];
    for (j=0; j<count && j*par->length+ins_allnan < par->length*count; j++){
        sprintf(file_in,"%s%ld.nc",      file_in_dir,(fnts_p+j)->tv_sec);

        status = nc_open(file_in, NC_NOWRITE, &ncid_in);
        if (status != NC_NOERR){ 
            logErrDate("%s: Could not open file %s to read! nc_open: %s\nExit\n",
            __func__,file_in,nc_strerror(status));
            return -1;
        }
    
        //print filename
        printf("\n%2d. %ld.%06ld\t",j+1,(fnts_p+j)->tv_sec,(fnts_p+j)->tv_nsec/1000);
        
        //calculate jitter
        if (j>0){ //difference make no sense at the first file
            diff=(float)((fnts_p+j)->tv_sec-(fnts_p+j-1)->tv_sec-60)*1E9 + 
                 (float)((fnts_p+j)->tv_nsec-(fnts_p+j-1)->tv_nsec);
            printf("diff:  %+4.0f\t",diff/1E3);
#ifdef DEBUG_DIFF       
            if (j>1){ //difference of differences make no sense at the first 2 files
                printf("ddiff: %+4.0f | %+4.0f",(diff-diff2)/1E3, (diff+diff2)/1E3);
            }
            diff2=diff;
#endif
            //continousity check 
            if (diff > 1E9/par->sps) { //if missing more than 1 sample 
                ins_nan=(long)round(diff*par->sps*1E-9);
                printf("inserting %ld NaN (%lds)",ins_nan,(long)round(ins_nan/(float)par->sps));
                nodelete=1;
                //update inserted values counter
                ins_cnt+=ins_nan;
            }            
            
        }
        
        //update inserted values counter
        ins_cnt+=par->length;
        
        for (k=0; k<par->chnumber; k++){ //for as many channels as in file (1,2,4 or 8)
            sprintf(varstr,"%s%d","ch",k+1); //ch1, ch2 ...
            status = nc_inq_varid(ncid_in, varstr, &varid[k]);
            if (status != NC_NOERR){
                logErrDate("%s: Could not find variable %s in %s! nc_inq_varid: %s\nExit\n",
                __func__, varstr, file_in, nc_strerror(status));
                return -1;
            }
            
            //inserting NaN in case of missing files or time gaps
            for (m=0; m<ins_nan; m++){
                *(chp[k]+ins_allnan+m+j*par->length)=nan;
            }
            // update variable indicating the inserted nans (ins_allnan)
            ins_allnan+=ins_nan;
            //clear the indicator to insert nans (since it is the same for all channels)
            ins_nan=0;
            
            status = nc_get_var_float(ncid_in, varid[k], (chp[k]+j*par->length + ins_allnan));
            if (status != NC_NOERR){
                logErrDate("%s: Error while reading variable %s in %s! nc_get_var_float: %s\nExit\n",
                __func__, varstr, file_in, nc_strerror(status));
                return -1;
            }
        }

        
#ifdef DEBUG        
        printf("%2d. input file read (name: %s, ch: %d)\n",j,file_in,k);
#endif
        status=nc_close(ncid_in);
        CHECK_NC_ERR("nc_close inputs");
    } // end for as many input files
    printf("\n");
    //TODO correct by start moved
    printf("info: inserted values: %ld/%d\n ",ins_cnt,par->length*60);
    
    /* Correction of the beginning
     * for the first file it might be neccessary to add only the last 'first_file_samp' amount
     * so the pointer will be moved by (par->length-first_file_samp)
     */
    if (first_file_samp){
        for (k=0; k<par->chnumber; k++)
            chp[k]=chp[k]+(par->length-first_file_samp);
        printf("info: start index moved by %ld\n",par->length-first_file_samp);
    }
    
    /* Correction for the ending 
     * lets simply truncate to  count*par->length;
     */
     
    size_t s_start[] = {0};
    size_t s_count[] = {par->length*count + ins_allnan};    
        
    // assign variable data 
    for (k=0; k<par->chnumber; k++){ //for as many channels
        status = nc_put_vara_float(ncid_out, out_varid[k], s_start, s_count, chp[k]);
        if (status != NC_NOERR){
            logErrDate("%s, nc_put_vara_float ch%d: %s\nExit\n",__func__,k+1,nc_strerror(status));
            return -1;
        }
        status = nc_sync(ncid_out); //ensure it is written to disk        
        CHECK_NC_ERR("nc_sync");
    }
        
    status = nc_close(ncid_out); //close file
    CHECK_NC_ERR("nc_close output");
    
    //get filesize of output file
    long fsize_out=fsize((char *)file_out);
    printf("\nfile %s written. Size: %.2fMiB\n",file_out,fsize_out/1024.0/1024.0);

    //get the filesize of the last input file
    long fsize_in=fsize((char *)file_in); //last file size

    //file sanity check 
    if (abs(j*fsize_in - fsize_out) > 512*1024){ //less than 0.5MiB deviation
        logErrDate("%s: file sizes seems not to be ok!\nNo deletion! Exit\n",__func__);
        return -1;
    }
    printf("============================== stats ==============================\n");
    for (k=0; k<par->chnumber; k++){
        printf("ch%i:",k+1);
        standard_deviation(chp[k],count*par->length);
    }

    
    for (k=0; k<par->chnumber; k++){
        //before freeing we have to restore original address (which calloc gave)
        if (first_file_samp)
            chp[k]=chp[k]-(par->length-first_file_samp);
        free(chp[k]);  
    }
        
    return j;
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

int getSortedFileList(const char *datadir, struct timespec *fnts_p)
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
                continue;
            }
        
            (fnts_p+ind)->tv_sec = (time_t)l;
            ind++;
            if (ind > MAX_FILES){
                ind = MAX_FILES;
                break;
            }
        }
        closedir (dir);
    } else {
        logErrDate("%s: could not open directory %s\n",__func__,datadir);
        return -1;
    }
    qsort(fnts_p, ind, sizeof(struct timespec), cmpfunc_timespec);       
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
    int i,ind;
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
    
    /* struct tm is broken-down time */
    struct tm tp;   
    
    i = getSortedFileList(file_in_dir, &fnts[0]);
    //check for overrun
    if (i==MAX_FILES)
        printf("more than ");
    printf("%d files (>%i hours) found.\n",i,i/60);
    if (i<=0){
        logErrDate("No files to process!?\n");
        exit_all(-1);
    }
#ifdef DEBUG_LIST  
    //diagnostic output: list all files (sorted)
    for (ind=0;ind<i;ind++){
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
    //open all further files (including the first) within that hour,
    // and assert parameters are identical.
    for(ind=0; (t1h-fnts[ind].tv_sec)>0; ind++){
        sprintf(file_in,"%s%ld.nc",file_in_dir,fnts[ind].tv_sec);       
        if (get_nc_params(file_in_dir, &parS, 0, NULL, &fnts[ind]) < 0){
            logErrDate("%d. file (%s) parameter assert error!\n Skipping\n",ind,file_in);
            //TODO: delete file
        }
    }
    printf("%d files will be concatenated.\n",ind);
    ind=ind-1; //to recover index to last element
    
    if ((i=concat_nc_data(file_in_dir, ind, file_out, &tp0h, &parS, &fnts[0])) < 0){
        logErrDate("Error while concatenating!\n Skipping\n");
        //TODO: delete files
    }
    
// dry run when 
//return -1;    
    
    /* if exactly as many files as requested were concatenated, 
     * delete raw files, except the last one (it might contain data for 
     * the next hour)
     */
    if (i==ind && !nodelete) {
        for (i=0; i<ind-1; i++){
            sprintf(file_in,"%s%ld.nc",file_in_dir,fnts[i].tv_sec);
            if(unlink(file_in)==-1){
                logErrDate("failed to delete input file %s\n",file_in);
                return -1;
            } 
        }
        //check last processed file
        tp=*(gmtime(&fnts[ind-1].tv_sec)); //convert to broken down time format
        printf("last file minute=%d\n",tp.tm_min);
        if (tp.tm_min != 59){
            sprintf(file_in,"%s%ld.nc",file_in_dir,fnts[ind-1].tv_sec);
            if(unlink(file_in)==-1){
                logErrDate("failed to delete input file %s\n",file_in);
                return -1;
            }
            i++; 
        }
        printf("%i files deleted from %s\n",i,file_in_dir);
    }
    return 0;
}


