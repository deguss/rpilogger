/**
 *   @file    ncglue.c
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com 
 *   @brief   concatenating nc files.
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
 
/* compile using 
 * gcc -o build/ncglue src/ncglue.c -lm -lnetcdf -lrt -Wall -Wextra -Wno-missing-field-initializers -Wunused
 */
#include "ncglue.h"

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


int get_nc_params(const char *file_in_dir, struct params *par, int first, struct timespec *fnts_p)
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
    
    status = nc_get_att_text(ncid, NC_GLOBAL, "start", par->starts); //"2015-11-08 01:39:27.499968"
    CHECK_NC_ERR("nc_get_att_text");
    //TODO: not nice
    par->starts[26]='\0'; //truncate string since get_att_text does not append \0  ???

    //convert startstring to fractional seconds (tv_usec)
    long tv_usec;
    if (sscanf((par->starts+20*sizeof(char)),"%ld",&tv_usec) != 1){
        logErrDate("%s: attribute \"start\" in %s not valid! (%s)\n",
        __func__, filename, par->starts);
        return -1;
    }
    //copy tv_usec to argument pointer
    fnts_p->tv_nsec = tv_usec * 1000;
    
    //purge fractional seconds
    char str[26+1];
    strncpy(str, par->starts, 27);
    str[19]='\0'; 
    char *ret;
    ret=strptime(str, "%Y-%m-%d %H:%M:%S", &(par->tmb));
    if (*ret!='\0' || ret==NULL){ //error
        logErrDate("%s: could not parse \"start\" in %s not valid! (%s)\n",
            __func__, filename, par->starts);
        return -1;
    }
   
    //file name check
    if (fnts_p->tv_sec != (long int)(timegm(&par->tmb))){
        logErrDate( "%s: corrupt data! file name does not match start date!\n"
                    "filename=%ld.nc,  start=%s\n",__func__, fnts_p->tv_sec,  str);
        return -1;
    }
      
    status = nc_close(ncid);
    CHECK_NC_ERR("nc_close");
    
    //if output desired (first file, template)
    if (first){
        printf("%s: first file %s read\n",__func__,filename);
        fprintf(stdout,"\tstarts at:                     %s\n",str);
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
    const time_t tstart, long *pos_p, struct timespec (*fnts_p))
{
    int status;
    int ncid_in, ncid_out;
    long i, j, k, m;
    char file_in[255];
    char varstr[3+1]; //channel name e.g. "ch1"
    if (count < 2 || file_in_dir == NULL || file_out == NULL || tstart == 0 ||
        pos_p == NULL || fnts_p == NULL){
        logErrDate("%s: parameter error!\nExit\n",__func__);
        return -1;
    }
    if (count < 60){
        printf("%s warn: too few files for a complete hour!\n"
               "   NaNs will be inserted where no data present!\n",__func__);
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
    
    /* determine when the first file begins (usually in the hour before the processed hour) */
    //sprintf(file_in,"%s%ld.nc", file_in_dir,fnts_p->tv_sec);
    struct params par;
    get_nc_params(file_in_dir, &par, 0, fnts_p);
    long totallen=par.length*60;    
    
    //allocate memory for 1 hour input files 
    float *chp[MAX_CH_NUMBER];
    long nnumb = totallen + 100000;
    for (k=0; k<par.chnumber; k++){
        chp[k] = calloc((size_t)nnumb, sizeof(float));
        if (chp[k] == NULL){
            logErrDate("%s: Could not allocate memory of size %ld kB!\nExit\n",
            __func__, (nnumb*sizeof(float)/1024L)+1);
            return -1;            
        }
        //and initialize it with NaN
        for (m=0; m<nnumb; m++)
            *(chp[k]+m)=NAN;
    }
    printf("info: memory allocated for %ld items\n",m);
    
    int out_dimid[MAX_CH_NUMBER];       // dimension id
    int out_varid[MAX_CH_NUMBER];        // variable id
    
    for (k=0; k<par.chnumber; k++){ //for as many channels as in file (1,2,4 or 8)
        sprintf(varstr,"%s%ld","ch",k+1); //ch1, ch2 ...
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

    //create start string format "YYYY-MM-DD hh:mm:ss.000000"
    struct tm tp=*(gmtime(&tstart));
    char tmpstr[26+1];
    strftime(tmpstr, sizeof(tmpstr), "%F %T.000000", &tp);
    status = nc_put_att_text(ncid_out, NC_GLOBAL, "start", strlen(tmpstr), tmpstr);
    CHECK_NC_ERR_FREE("nc_get_att_text");      
    
    //leaving define mode
    status = nc_enddef(ncid_out);
    CHECK_NC_ERR_FREE("nc_enddef");


    printf("%s:\n",__func__);
    
    long incl=0;
    float *memtr=NULL;
    //check if the first file stretches over the hour border
    if (*(pos_p) < 0){        
        incl = labs(*pos_p);
                                
        printf("from the first file %8ld samples (%.3fs) will be taken\n",incl,(float)(incl)/par.sps); 
        printf("      starts exactly at:       %s\n",par.starts);    
        
        memtr=calloc(par.length, sizeof(float));
        if (memtr == NULL){
            logErrDate("%s: Could not allocate memory of size %ld kB!\nExit\n",
            __func__, (par.length*sizeof(float)/1024L)+1);
            FREE_AND_RETURN(-1);
        }        
    }
    
    long ins_cnt=0;
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
        
        //for as many channels as in file (1,2,4 or 8)
        for (k=0; k<par.chnumber; k++){ 
            sprintf(varstr,"%s%ld","ch",k+1); //ch1, ch2 ...
            status = nc_inq_varid(ncid_in, varstr, &varid[k]);
            if (status != NC_NOERR){
                logErrDate("%s: Could not find variable %s in %s! nc_inq_varid: %s\nExit\n",
                __func__, varstr, file_in, nc_strerror(status));
                FREE_AND_RETURN(-1);
            }

            //check if the first file and neccessary to copy truncated data
            if (j==0 && *(pos_p) < 0){
                //read in all data to temporary memory: memtr
                status = nc_get_var_float(ncid_in, varid[k], memtr);
                if (status != NC_NOERR){
                    logErrDate("%s: Error while reading variable %s in %s! nc_get_var_float: %s\nExit\n",
                    __func__, varstr, file_in, nc_strerror(status));
                    FREE_AND_RETURN(-1);
                }
                //copy from temporary mem to output file array
                for (m=0, i=(par.length-labs(*pos_p));  i<(long)par.length && m<(long)par.length; m++, i++){
                    *(chp[k]+m) =  *(memtr+i);
                }
                //update ins_cnt
                ins_cnt=incl;
                   
            } 
            else { //insert values at position given by array pos
                status = nc_get_var_float(ncid_in, varid[k], chp[k]+*(pos_p+j));
                if (status != NC_NOERR){
                    logErrDate("%s: Error while reading variable %s in %s! nc_get_var_float: %s\nExit\n",
                    __func__, varstr, file_in, nc_strerror(status));
                    FREE_AND_RETURN(-1);
                }
            }
            
        } //end for as many channels
        
        //update inserted values counter 
        if (*(pos_p+j)+(long)par.length > totallen) //for the last file 
			ins_cnt+=(totallen - par.length);
        else
			ins_cnt+=par.length;
        
        status=nc_close(ncid_in);
        CHECK_NC_ERR_FREE("nc_close inputs");
        
    } // end for as many input files
    free(memtr);
    
    printf("\n\n");
    printf("info: file length:     %9ld\n",totallen);
    printf("      inserted values: %9ld\n",MIN(totallen,ins_cnt));
    printf("      NaN:             %9ld\n",totallen-MIN(totallen,ins_cnt));
    
     
    size_t s_start[] = {0};
    size_t s_count[] = {totallen};    
    
    // write hughe array to output file
    for (k=0; k<par.chnumber; k++){ //for as many channels
        status = nc_put_vara_float(ncid_out, out_varid[k], s_start, s_count, chp[k]);
        if (status != NC_NOERR){
            logErrDate("%s, nc_put_vara_float ch%ld: %s\nExit\n",__func__,k+1,nc_strerror(status));
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
        printf("ch%ld:",k+1);
        standard_deviation(chp[k],totallen);
    }
#endif
    //get filesize of output file
    long fsize_out=fsize((char *)file_out);
    printf("\nfile %s written. Size: %.2fMiB\n",file_out,fsize_out/1024.0/1024.0);

    //get the filesize of the last input file
    long fsize_in=fsize((char *)file_in); //last file size

    
    //file sanity check 
    if (count>58 && abs(j*fsize_in - fsize_out) > 512*1024){ //less than 0.5MiB deviation
        printf("file sanity check numb:%ld, fsize_in: %.2fMiB\n",j,fsize_in/1024.0/1024.0);        
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
    if (datadir==NULL || fnts_p==NULL){
        logErrDate("%s: parameter error!\n",__func__);
        return -1;
    }
    DIR *dir;
    struct dirent *ent;
    int ind=0, i;
    long l;
    printf("reading directory %s...\n",datadir);
    
    //count how many files to allocate buffer for filenames
    if ((dir = opendir(datadir)) != NULL) {
        while ((ent=readdir(dir)) != NULL) {  //will count "." and ".." too
            ind++;
        }
        closedir (dir);
    } else {
        logErrDate("%s: could not open directory %s\n",__func__,datadir);
        return -1;
    }    
    printf("   %d files counted\n",ind-2);
    
    //allocate buffer
    time_t *p;
    p = (time_t *)calloc(ind, sizeof(time_t));
    if (p == NULL){
        logErrDate("%s: could not allocate memory\n",__func__);
        return -1;
    }
    
    //read in files
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

    //sort files ascending (newest at the end)
    qsort(p, ind, sizeof(time_t), cmpfunc_time);
   
    
    //write the oldest 60 to the array pointer by parameter
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

int delete_file(char *file_in_dir, struct timespec *ts, int dryrun)
{
    static int cnt;
    char str[255];
    sprintf(str,"%s%ld.nc",file_in_dir,ts->tv_sec);    
    
    if (cnt++ < 60){
		if (dryrun){
            printf("%s would be deleted\n",str);
            return 0;            
		}
        if(unlink(str)==-1){
            logErrDate("%s: failed to delete file %s\n",__func__,str);
            return -1;
        } 
        else {
#ifdef DEBUG
            printf("%s deleted\n",str);
#endif
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
    struct timespec fnts[MAX_FILES];// array of timespecs = filenames
    long pos[MAX_FILES]={0};        // indicator for start index of files
    
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
    
    //convert the start time of the first file
    tp=*(gmtime(&fnts[0].tv_sec)); //convert to broken down time format

    //determine beginning of this hour (used for output file name)
    struct tm tp0h; 
    time_t t0h = calcNextFullHour(&tp, &tp0h, 0);
    //however when the first file starts within 60 seconds to the next hour, ceil
    if (tp.tm_min == 59){
        t0h = calcNextFullHour(&tp, &tp0h, 1);
    }
    strftime(tmpstr, sizeof(tmpstr), "%F %T %z", &tp0h);
    printf("beginning of the hour to be processed: %s\n",tmpstr); 
    
    
    //calculate next full hour (hour file limit)
    struct tm tp1h;
    time_t t1h = calcNextFullHour(&tp0h, &tp1h, 1);        
    strftime(tmpstr, sizeof(tmpstr), "%F %T %z", &tp1h);
    printf("end of the hour to be processed:       %s\n", tmpstr);   
    

    char file_in[255];
    char file_out[255];
    char daystr[255];
    
    /* open first file to check length, sps, number of channels ...
     * this will be saved as "template". parM .. Master
     * All further files must be identical in parameters except start date
     */
    struct params parM;
    sprintf(file_in,    "%s%ld.nc",file_in_dir,fnts[0].tv_sec);
    sprintf(daystr, "%04d/%02d/%02d", tp0h.tm_year+1900, tp0h.tm_mon+1, tp0h.tm_mday);

    if (get_nc_params(file_in_dir, &parM, 1, &fnts[0]) <0){
        logErrDate("Error while reading parameters in %s!\nExit\n",file_in);
        return -1;
    }   

    
    /* open all further files (including the first) within that hour,
     * and assert parameters are identical. 
     * Also count the predicted length of the file and print the file 
     * names in HH:MM:SS format (as in start string attribute).
     */
    printf("\nfile check for\n");
    printf(" # |    file name  =   start time   |      index | diff (us)|\n");
    printf("-------------------------------------------------------------");
    long nans;
    float dec_sec, diff;
    struct params parS;
    
    
    //for as many files as whose starting date is within hour limit
    for(ind=0; (t1h-fnts[ind].tv_sec)>0; ind++){ 
        
        //build file name
        sprintf(file_in,"%s%ld.nc",file_in_dir,fnts[ind].tv_sec);       
        
        if (get_nc_params(file_in_dir, &parS, 0, &fnts[ind]) < 0){
            logErrDate("%d. file (%s) parameter assert error!\n Skipping\n",ind,file_in);
            delete_file(file_in_dir, &fnts[ind], 0); //delete always, irrespect of -d flag
            return -1;
        }
        
        /* usually, the hour starts with the file beginning in the 59th 
         * minute of the previous hour. If yes, determine how much overlaps the 
         * tartget hour
         */

        //file begins before hour, need to find out when exactly
        if (fnts[ind].tv_sec < t0h){
            if (t0h - fnts[ind].tv_sec > 60){ //panic
                logErrDate("file %ld.nc (%s) out of time range!\n",fnts[ind].tv_sec, parS.starts);
                return -1;
            }
            //calculate decimal seconds (02:59:32.544123 -> 32.544123) 
            dec_sec=(float)parS.tmb.tm_sec + (float)(fnts[ind].tv_nsec*1E-9);
            
            //negativ value if file starts earlier than the beginning of the hour
            pos[ind]=0-lroundf(dec_sec*parS.sps);
            printf("\n*%2d: %ld.nc = %s\t%9ld",ind+1, fnts[ind].tv_sec, parS.starts+11,  pos[ind]);
            
        } else {
            /* in between we have to check for continousity. If a file is
             * missing insert as many NaN as needed.
             */
            
            // calculate relative time in decimal seconds
            dec_sec=(float)(fnts[ind].tv_sec-t0h)+(float)(fnts[ind].tv_nsec*1E-9);
            // convert to index
            pos[ind]=lroundf(dec_sec*parS.sps);
            
            //calculate jitter
            if (ind>0){ //difference make no sense at the first file
                diff=(float)(fnts[ind].tv_sec-fnts[ind-1].tv_sec-60)*1E9 + 
                     (float)(fnts[ind].tv_nsec-fnts[ind-1].tv_nsec);
                //printf("diff:  %+4.0f\t",diff/1E3);

                if (labs(diff) > 1E9/parS.sps) { //if missing more than 1 sample 
                    nans=lroundf(diff*parS.sps*1E-9);

                    printf("\n   : filling up with NaN                %+9ld = %8.2fs", nans,nans/parS.sps);
                }
           
                if (labs(diff) < 1E9) //display only if jitter < 1s
                    printf("\t%+4.0f",diff/1E3);
			}

            printf("\n %2d: %ld.nc = %s\t%9ld",ind+1, fnts[ind].tv_sec, parS.starts+11,  pos[ind]);
            
            

        } //end else
    }//end for 
    
    printf("\n------------------------------------------------------------------\n");
    
    //print the next file which IS NOT in the hour anymore
    sprintf(file_in,"%s%ld.nc",file_in_dir,fnts[ind].tv_sec);       
    if (get_nc_params(file_in_dir, &parS, 0, &fnts[ind]) < 0){
        logErrDate("further %d. file (%s) parameter assert error!\n",ind,file_in);
    }
    printf("(  : %ld.nc = %s) <- not included\n",fnts[ind].tv_sec, parS.starts);
    
    
    printf("\n%d files will be concatenated.\n",ind);

    sprintf(file_out, "%s%s/%04d-%02d-%02dT%02d.nc", file_out_dir,daystr,
        tp0h.tm_year+1900, tp0h.tm_mon+1, tp0h.tm_mday, tp0h.tm_hour); 
    printf("\noutfile: %s\n",file_out);

    /* concatenation:
     * concat_nc_data(const char *file_in_dir, const int count, const char *file_out,
     * const time_t tstart, long *pos_p, struct timespec (*fnts_p))
     */
    i=0;
    if (ind>1){
        if ((i=concat_nc_data(file_in_dir, ind, file_out, t0h, pos, fnts)) < 0){
            logErrDate("Error while concatenating!\n");
        }
    }     

	/* if the files are more than 3 days older, delete them (else the -d 
	 * flag /dryrun/ is set. */
	int dryrun=1; //default no delete
	time_t rightnow;
	time(&rightnow);
	if (rightnow - fnts[0].tv_sec > 3*24*60*60 )
		dryrun=0;
	if (argc>1 && !strcmp(argv[1],"-d"))
		dryrun=1;
	if (!dryrun)
		printf("info: deleting files since older than 3 days\n");

	
    /* after EVERY run the first file will be deleted */
    if(delete_file(file_in_dir, &fnts[0], dryrun)){
        logErrDate("failed to delete files.\nExit\n");
        return -1;
    }
    
    if (ind < 2){
        printf("nothing to do!\n Exit\n");
        return 0;
    }
    
    /* if exactly as many files as requested (or at highest one fewer) 
     * were concatenated, delete raw files, except the last one 
     * (it might contain data for the next hour)
     */
    if (i >= 2) {
        for (j=1; j<i-1 && j<60; j++){
            if(delete_file(file_in_dir, &fnts[j], dryrun)){
                logErrDate("failed to delete file %ld.nc.\nExit\n",fnts[j].tv_sec);
                return -1;
            }
        }
        //check last processed file
        tp=*(gmtime(&fnts[i-1].tv_sec)); //convert to broken down time format
        //printf("last file %02d:%02d:%02d\n",tp.tm_hour,tp.tm_min,tp.tm_sec);
        if (tp.tm_min != 59){
            if(delete_file(file_in_dir, &fnts[i-1], dryrun)){
                logErrDate("failed to delete files.\nExit\n");
                return -1;
            }
            j++; 
        }
        if (!dryrun)
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
