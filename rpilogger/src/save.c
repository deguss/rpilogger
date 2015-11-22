/**
 *   @file    save.c
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com
 *   @brief   temporary solution, will be included from main.c w/o header
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */

//--------------- MACROS -----------------------------------------------
#define CHECK_NC_ERR(strr) \
do { \
    if (status != NC_NOERR){ \
        logErrDate("%s: %s\n%s\nExit\n",__func__,strr,nc_strerror(status)); \
        return -1; \
    } \
} while(0);

 
//--------------- PROTOTYPES ------------------------------------------- 
int mkdir_filename(const char *dir_name);
void *thread_datastore(void * p);
int write_netcdf();
double difftime_hr(const struct timespec *t1, const struct timespec *t2);

pthread_mutex_t a_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t got_request = PTHREAD_COND_INITIALIZER;
pthread_t  p_thread;       // thread's structure                    
pthread_attr_t p_attr;



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
//--------------------------------------------------------------------------------------------------
void *thread_datastore(void * p){ 
//--------------------------------------------------------------------------------------------------
    int rc;
    long tpid = (long)syscall(SYS_gettid);
    printf("data-storage thread started with pid: %ld\n",tpid);
    
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGALRM);
    rc = pthread_sigmask(SIG_SETMASK, &set, NULL);
        PthreadCheck("pthread_sigmask", rc);

    uid_t uid;
    gid_t gid;
    get_uid_gid(file_user, &uid, &gid);
    setfsuid(uid);
    setfsgid(gid);

    

    pthread_mutex_lock(&a_mutex);
    while (done==0) {
        pthread_cond_wait(&got_request, &a_mutex);
        pthread_mutex_unlock(&a_mutex);
        rc = write_netcdf();
        if (rc < 0){
            logErrDate("%s: severe error while writing netcdf file\nExit\n",__func__);
            exit(EXIT_FAILURE);
        }
        
        if (auto_pga && pga_uf){    //write new pga in ini file 
            update_ini_file(ini_name);
            //printf("New PGA written in %s!\n",ini_name);
            pga_uf = 0; //clear update flag
        }
        pthread_mutex_lock(&a_mutex); // ????????

    }
    pthread_mutex_unlock(&a_mutex);
    pthread_exit(NULL);
}
#pragma GCC diagnostic pop

//----------------------------------------------------------------------
int write_netcdf(){ // generate BINARY file: *.nc (netCDF)   
//----------------------------------------------------------------------
    static struct tm tm;
    int i;
    char str[500];
    char fn[255];
    int  ncid;  // netCDF id    
    int status;
    float d[MAX_SAMPL];
    static long filesize_c;
    long filesize;
    // tps indicated the time of the program start, the entirely first sample
    // after starting the program. (is used to determine file size check compare to)    
    static struct timespec tps; 
    
    tm = *gmtime(&dst.t2.tv_sec); 
    sprintf(fn,"%s/tmp/%ld.nc",datafiledir,dst.t2.tv_sec);
    mkdir_filename(fn);
     
    status = nc_create(fn, NC_CLOBBER|NC_NETCDF4, &ncid);
    if (status != NC_NOERR){ //possible errors: NC_ENOMEM, NC_EHDFERR, NC_EFILEMETA
        logErrDate("savefile: Could not open file %s to write! nc_create: %s\nExit\n",
        fn,nc_strerror(status));
        return -1;
    }
    
    int ch1_dim;       // dimension id
    int ch1_id;        // variable id
    size_t s1[1] = {0}, s2[1] = {sampl};
    
    // define dimensions 
    status = nc_def_dim(ncid, "ch1", sampl, &ch1_dim);
    CHECK_NC_ERR("nc_def_dim (ch1)");
    #if (CFG_NR_CH > 1)
        int ch2_dim, ch2_id;
        status = nc_def_dim(ncid, "ch2", sampl, &ch2_dim);
        CHECK_NC_ERR("nc_def_dim (ch2)");
    #endif
    #if (CFG_NR_CH > 2)
        int ch3_dim, ch3_id;
        status = nc_def_dim(ncid, "ch3", sampl, &ch3_dim);
        CHECK_NC_ERR("nc_def_dim (ch3)");
    #endif
    #if (CFG_NR_CH > 3)
        int ch4_dim, ch4_id;
        status = nc_def_dim(ncid, "ch4", sampl, &ch4_dim);
        CHECK_NC_ERR("nc_def_dim (ch4)");
    #endif

    // define variables
    status = nc_def_var(ncid, "ch1", NC_FLOAT, 1, &ch1_dim, &ch1_id);
    CHECK_NC_ERR("nc_def_var (ch1)");
    #if (CFG_NR_CH > 1)    
        status = nc_def_var(ncid, "ch2", NC_FLOAT, 1, &ch2_dim, &ch2_id);
        CHECK_NC_ERR("nc_def_var (ch2)");
    #endif
    #if (CFG_NR_CH > 2)    
        status = nc_def_var(ncid, "ch3", NC_FLOAT, 1, &ch3_dim, &ch3_id);
        CHECK_NC_ERR("nc_def_var (ch3)");
    #endif
    #if (CFG_NR_CH > 3)    
        status = nc_def_var(ncid, "ch4", NC_FLOAT, 1, &ch4_dim, &ch4_id);
        CHECK_NC_ERR("nc_def_var (ch4)");
    #endif
    


    // assign global attributes
    sprintf(str,"%d-%02d-%02d %02d:%02d:%02d.%06ld",tm.tm_year+1900,tm.tm_mon+1, tm.tm_mday, tm.tm_hour,tm.tm_min,tm.tm_sec, dst.t2.tv_nsec/1000L);
    status = nc_put_att_text(ncid, NC_GLOBAL, "start", strlen(str), str);
    CHECK_NC_ERR("nc_put_att_text (start)");
    
    status = nc_put_att_float(ncid, NC_GLOBAL, "sps", NC_FLOAT, 1, &sps);
    CHECK_NC_ERR("nc_put_att_float (sps)");


    // assign per-variable attributes
    status = nc_put_att_text(ncid, ch1_id, "units", 2, "mV");
    CHECK_NC_ERR("nc_put_att_text (ch1 units)");
    #if (CFG_NR_CH > 1)  
        status = nc_put_att_text(ncid, ch2_id, "units", 2, "mV");
        CHECK_NC_ERR("nc_put_att_text (ch2 units)");
    #endif
    #if (CFG_NR_CH > 2)
        status = nc_put_att_text(ncid, ch3_id, "units", 2, "mV");
        CHECK_NC_ERR("nc_put_att_text (ch3 units)");    
    #endif
    #if (CFG_NR_CH > 3)  
        status = nc_put_att_text(ncid, ch4_id, "units", 2, "mV");
        CHECK_NC_ERR("nc_put_att_text (ch4 units)");
    #endif
    
    status = nc_enddef (ncid);    // leave define mode
    CHECK_NC_ERR("nc_end_def");

    // assign variable data
    for (i=0;i<sampl;i++)
        d[i] = dst.data[piter+i][0];
    //TODO no copy
    status = nc_put_vara_float(ncid, ch1_id, s1, s2, d);
    CHECK_NC_ERR("nc_put_vara_float (ch1)");
    
    #if (CFG_NR_CH > 1)
        for (i=0;i<sampl;i++)
            d[i] = dst.data[piter+i][1];
        status = nc_put_vara_float(ncid, ch2_id, s1, s2, d);
        CHECK_NC_ERR("nc_put_vara_float (ch2)");
    #endif
    #if (CFG_NR_CH > 2)
        for (i=0;i<sampl;i++)
            d[i] = dst.data[piter+i][2];
        status = nc_put_vara_float(ncid, ch3_id, s1, s2, d);
        CHECK_NC_ERR("nc_put_vara_float (ch3)");
    #endif
    #if (CFG_NR_CH > 3)
        for (i=0;i<sampl;i++)
            d[i] = dst.data[piter+i][3];
        status = nc_put_vara_float(ncid, ch4_id, s1, s2, d);
        CHECK_NC_ERR("nc_put_vara_float (ch4)");
    #endif

    status = nc_sync(ncid); //ensure it is written to disk        
    CHECK_NC_ERR("nc_sync");
    
    status = nc_close(ncid); //close file
    CHECK_NC_ERR("nc_close");
        //printf("%d-%02d-%02d %02d:%02d:%02d.%04ld  %.06g\n",tm.tm_year+1900,tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, dst.ts[piter].tv_nsec/100000L, g/1E9);
        
    //check if this is the first file 
    if (tps.tv_sec==0){ 
        tps = dst.t2; //no memcpy needed
        filesize_c=fsize(fn);
        logErrDate("File written: %s, size=%ldKiB\n",
            fn, lroundf((float)filesize_c/1024.0));
    } 
    else {
        filesize = (unsigned long)fsize(fn);
        logErrDate("file=%ld.nc, last=%.6fs, size=%ldKiB\n",
            dst.t2.tv_sec, difftime_hr(&dst.t1, &dst.t2), lroundf((float)filesize/1024.0));
        if (filesize != filesize_c){
            logErrDate("Error! Fize size not consistent!\nExit\n");
            return -1;
        }

    }
    return 0;
} 

double difftime_hr(const struct timespec *t2, const struct timespec *t1)
{
    return ( ((double)t2->tv_sec*1E9 + (double)t2->tv_nsec)-
             ((double)t1->tv_sec*1E9 + (double)t1->tv_nsec) ) / 1E9;

}





