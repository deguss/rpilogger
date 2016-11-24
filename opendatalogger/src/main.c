/**
 *   @file    main.c
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com
 *   @brief   main file
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
#include "main.h"
#include "save.c"

// TODO if sampling rate lower than 1/60 what is in 1-minute file???

//--------------- PROTOTYPES -------------------------------------------
static void take_a_sample(struct timespec *tabs);
static inline long calcdiff_us(struct timespec t1, struct timespec t2);
static inline void tsnorm(struct timespec *ts);
static void adjust_pga(int it);
static void getADC_ADS1115(int it);
static float twocompl2float(int value, float pga);


/* interrupt service routine
 * performs the sampling each 1/sps
 */
static void take_a_sample(struct timespec *tabs1) 
{ 
    int err = -1;
    static int ov[4]; 
    static int notfirst;
    int i;
    
    //check if we are at the first sample of the lower or eq. of the upper buffer
    if ((dst.it == 0) || (dst.it == sampl)){
        dst.t2=dst.t1; //store timestamp for processing in t2!
        dst.t1=*(tabs1);
    }    
    
    //sampling
    getADC_ADS1115(dst.it);

    //overvoltage check 
    for (i=0;i<CFG_NR_CH;i++){
        if (fabs(dst.data[dst.it][i]) > overvoltage){
            ov[i]++;
            if (ov[i] > 10){ 
                err = i;
                ov[i] = 0;
            }
        }
    }
    
    //if there is an overvoltage, print
    if (err > -1){
        printf("WARN: Overvoltage on ch%i! \tch%i=%.0f", err+1, err+1, dst.data[dst.it][err]);
    }
    
    //if auto gain is selected (from config file), calculate it    
    if (auto_pga){
        adjust_pga(dst.it);
    }
    
    //if started in debug mode (flag --debug) additional output will be printed    
    //  regardless of the sampling rate, once a second
    if (debug){  
        static int cnt;
        if (++cnt == (int)sps){
            printf("debug: ch1=%+5.0f",dst.data[dst.it][0]);
            #if (CFG_NR_CH > 1)
                printf(", ch2=%+5.0f" , dst.data[dst.it][1]); 
            #endif
            #if (CFG_NR_CH > 2)
                printf(", ch3=%+5.0f" , dst.data[dst.it][2]); 
            #endif                
            #if (CFG_NR_CH > 3)
                printf(", ch4=%+5.0f" , dst.data[dst.it][3]); 
            #endif                            
            printf("\n");
            cnt=0;
        }
        
    }
    
    //lower half ready for processing if sampl+1 measurements taken
    //at this time already the upper half of the buffer gets filled.
    if (dst.it == sampl){   
        piter = 0;
        pthread_mutex_lock(&a_mutex);
        pthread_cond_signal(&got_request);
        pthread_mutex_unlock(&a_mutex);
    }
    else if (dst.it == 1 && notfirst){ //upper half ready for processing
        piter = sampl;
        pthread_mutex_lock(&a_mutex);
        pthread_cond_signal(&got_request);
        pthread_mutex_unlock(&a_mutex);
    }
    
    //increment buffer index
    dst.it++;
    if (dst.it == 2*sampl){ //flip over
        notfirst = 1; //set static variable
        dst.it=0;
    }
}

static inline void tsnorm(struct timespec *ts)
{
    while (ts->tv_nsec >= 1E9L) {
        ts->tv_nsec -= 1E9L;
        ts->tv_sec++;
    }
}

static inline long calcdiff_us(struct timespec t1, struct timespec t2)
{
    int64_t diff;
    diff = 1000000 * (int64_t)((int)t1.tv_sec - (int)t2.tv_sec);
    diff += ((int)t1.tv_nsec - (int)t2.tv_nsec)/1000;
    if (diff > (int64_t)__LONG_MAX__){
        fprintf(stderr,"overflow in %s\n",__func__);
        printf("diff: %" PRId64 "\n",diff); //this is the way to print int64
        exit_all(-1);
    }
    return (long)diff;
}


static void check_ADS1115(int fd, int addr, int cfg)
{
    // Wait a tiny bit to start sampling (10us)
    static struct timespec tim1 = {.tv_sec=0, .tv_nsec=10e3};
    static struct timespec rem;
    nanosleep(&tim1, &rem);

    // Check if the ADC with addr got the right configuration
    int reg = i2c_read(fd, addr, ADS_PTR_CONF) | ADS_CONF_OS_BUSY;
    
    if (cfg != reg){
        printf("adc with addr: %02X cfg-write: %04X, cfg-read: %04X\n",addr, cfg, reg);
        if (cfg != (i2c_read(fd, addr, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
            logErrDate("ADS1115 with addr: %02X returned not the specified configuration. Data transfer from ADC to RPi corrupt!\n", addr);
            exit_all(-1);
        }
    }
}

static float ADS1115_readresult(int fd, int cfg, int pga)
{
    struct timespec tim2={.tv_sec=0, .tv_nsec=10e3};
    struct timespec rem;
    int ret;
    // Wait for conversion to complete 
    while ((i2c_read(fd, cfg, ADS_PTR_CONF) & ADS_CONF_OS_BUSY) == 0) {  
        nanosleep(&tim2, &rem); //wait 10us
    }

    // read conversion register adc1_addr and include sign bit
    ret = i2c_read(fd, cfg, ADS_PTR_CNV);
    return twocompl2float(ret, pga);
}


static void getADC_ADS1115(int it) 
{
    static struct timespec tim1 = {.tv_sec=0, .tv_nsec=500e3};
    static struct timespec rem;
    int cfg1;
    float pga1;
    const int cfg = ADS_CONF_OS_SNG | ADS_CONF_MODE_SNG | ADS_CONF_CQUE_NONE |
                    (ADS_CONF_DR_MASK & SPSa[SPSc]); //single shot mode
    
    static int fd;   //file descriptor (i2c)
    if (init){ //only once at the beginning
        fd = i2c_open(i2c_interface);
        init=0;
    }

    // Trigger sampling ADC1
    cfg1 = cfg | (ADS_CONF_MUX_MASK & CFG_SEQ1_ADC1) | (ADS_CONF_PGA_MASK & PGAa[pga[0]]);
    pga1 = PGAv[pga[0]];
    i2c_write(fd, CFG_ADC1, ADS_PTR_CONF, cfg1);     // set config register and start conversion ADS1115       
            
        #ifdef CFG_ADC2
        int cfg2;
        float pga2;
        cfg2 = cfg | (ADS_CONF_MUX_MASK & CFG_SEQ2_ADC2) | (ADS_CONF_PGA_MASK & PGAa[pga[1]]);
        pga2 = PGAv[pga[1]];
        i2c_write(fd, CFG_ADC2, ADS_PTR_CONF, cfg2);
        #endif

    check_ADS1115(fd, CFG_ADC1, cfg1);

        #ifdef CFG_ADC2
        check_ADS1115(fd, CFG_ADC2, cfg2);
        #endif

    nanosleep(&tim1, &rem);
    dst.data[it][0] = ADS1115_readresult(fd, CFG_ADC1, pga1);
    
        #ifdef CFG_ADC2  
        dst.data[it][1] = ADS1115_readresult(fd, CFG_ADC2, pga2);
        #endif



    #ifdef CFG_SEQ2_ADC1 //single sequence
        cfg1 = cfg | (ADS_CONF_MUX_MASK & CFG_SEQ2_ADC1) | (ADS_CONF_PGA_MASK & PGAa[pga[1]]);
        pga1 = PGAv[pga[1]];
        i2c_write(fd, CFG_ADC1, ADS_PTR_CONF, cfg1);
        check_ADS1115(fd, CFG_ADC1, cfg1);
        nanosleep(&tim1, &rem);
        dst.data[it][1] = ADS1115_readresult(fd, CFG_ADC1, pga1);
    #endif

    #ifdef CFG_SEQ3_ADC1 //single sequence
        cfg1 = cfg | (ADS_CONF_MUX_MASK & CFG_SEQ3_ADC1) | (ADS_CONF_PGA_MASK & PGAa[pga[2]]);
        pga1 = PGAv[pga[2]];
        i2c_write(fd, CFG_ADC1, ADS_PTR_CONF, cfg1);
        check_ADS1115(fd, CFG_ADC1, cfg1);
        nanosleep(&tim1, &rem);
        dst.data[it][2] = ADS1115_readresult(fd, CFG_ADC1, pga1);
    #endif

    #ifdef CFG_SEQ4_ADC1 //single sequence
        cfg1 = cfg | (ADS_CONF_MUX_MASK & CFG_SEQ4_ADC1) | (ADS_CONF_PGA_MASK & PGAa[pga[3]]);
        pga1 = PGAv[pga[3]];
        i2c_write(fd, CFG_ADC1, ADS_PTR_CONF, cfg1);
        check_ADS1115(fd, CFG_ADC1, cfg1);
        nanosleep(&tim1, &rem);
        dst.data[it][3] = ADS1115_readresult(fd, CFG_ADC1, pga1);
    #endif
    
}

static float twocompl2float(int value, float pga)
{
    float ret;
    ret = (value > 0x7FFF) ? (float)(value-0xFFFF) : (float)(value); 
    return (ret * pga/32768.0); //mV    
}

static void adjust_pga(int it)
{
    static int sec_rem[4];
    int i;
    
    for (i=0;i<4;i++){ //initialization: load default values (count down)
        if (sec_rem[i] <= 0)
            sec_rem[i] = pga_updelay*sps;
            
        //range up immediatelly if not already the biggest range
        if ((fabs(dst.data[it][i]) > 0.95*PGAv[pga[i]]) && pga[i]>0){ 
            pga[i] = max(pga[i]-1,0);
            printf("Voltage range increased to |%6.3f|V on channel %i.\n",
                    PGAv[pga[i]]/1000.0,i);
            pga_uf = 1; //set update flag
        }
        
        //think over to range down if not already the lowest range
        if ((fabs(dst.data[it][i]) < 0.8*PGAv[min(pga[i]+1,5)]) && pga[i]<5 ){ 
            sec_rem[i] -= 1;
            if (sec_rem[i] <=0){   //range down only after pga_updelay seconds
                pga[i] = min(pga[i]+1,5);
                printf("Voltage range decreased to |%6.3f|V on channel %i. after %is.\n",
                        PGAv[pga[i]]/1000.0,i,pga_updelay);                
                pga_uf = 1; //set update flag
            }
        }
        else 
            sec_rem[i] = pga_updelay*sps;            
    }
}


int main(int argc, char * argv[])
{
    char *ppath=NULL;
    char logfile[255];
    char histfile[255];
    dst.t1.tv_sec=0;
    dst.t2.tv_sec=0;
    uid_t uid;
    gid_t gid;
    
    //determine executable location based on process ID (pid)
    pid_t pid = getpid();
    char path[255], dest[255];
    sprintf(path, "/proc/%d/exe", pid);
    if (readlink(path, dest, sizeof(path)) == -1){
        perror("readlink");
        exit_all(-1);
    }
    ppath=dirname(dest);    
    //printf("Executable's location: %s\n", ppath);
    
    /* ------------ load configuration from ini file --------------- */
    // (if it returns -1 -> no valid file)
    sprintf(ini_name,"%s/%s",ppath,CONFIGFILE);
    if (parse_ini_file(ini_name)) 
        parse_ini_file(ini_name); //create one
         
    sprintf(logfile,"%s/%s",ppath,LOGDIR);
    mkdir_filename(logfile);
    get_uid_gid(file_user, &uid, &gid);
    chown(logfile, uid, gid);
    sprintf(logfile,"%s/%s",logfile,LOGFILE);        
    sprintf(histfile,"%s/%s/%s",ppath,LOGDIR,HISTFILE);
         
     
    if (argc>1){
        if (!strcmp(argv[1],"daemon") || !strcmp(argv[1],"-daemon") ||
            !strcmp(argv[1],"--daemon")){
            daemon_f=1;
        }
        if (!strcmp(argv[1],"debug") || !strcmp(argv[1],"-debug") ||
            !strcmp(argv[1],"--debug")){
            debug=1;
        }        
        if (!strcmp(argv[1],"help") || !strcmp(argv[1],"-help") || 
            !strcmp(argv[1],"--help")){
            print_logo();
            print_usage();
            exit(EXIT_SUCCESS);
        }
    }
    
    set_latency_target();
    
    if (daemon_f){ // --------- Daemonize ---------------
        
        pid_t process_id = 0;
        pid_t sid = 0;
        
        process_id = fork(); // Create child process
        if (process_id < 0){ // Indication of fork() failure
            logErrDate("fork failed!\n");
            exit(EXIT_FAILURE); // Return 1 (failure) in exit status
        }
        if (process_id > 0) { // PARENT PROCESS. Need to kill it.
            printf("Will now detach and go into background!\n");
            printf("redirecting stdout and stderr to: %s\n",logfile);
            logErrDate("Main daemon started with pid: %ld\n",(long)process_id);
            exit(EXIT_SUCCESS); // Return (0) success in exit status
        }
        
        umask(0); //unmask the file mode
        
        sid = setsid(); //set new session
        if (sid < 0){
            exit(EXIT_FAILURE); // Return (1) failure
        }
        
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
            exit(EXIT_FAILURE); // Log the failure
        }
        
        // Close stdin. stdout and stderr
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
   

        printf("redirecting stdout and stderr to: %s\n",logfile);

        fp_log = freopen(logfile,"w",stdout); //redirect stdout to file
        setvbuf(fp_log, NULL, _IOLBF, 1024); // line buffering
        dup2(fileno(fp_log), fileno(stderr)); //redirect stderr to file
    }
    get_uid_gid(file_user, &uid, &gid);
    chown(logfile, uid, gid);
    chmod(logfile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    
    filelock(FILELOCK);    
    
    pid = getpid();
    printf("Executable: %s\n", dest);
    logErrDate("Main thread started with pid: %ld\n",(long)pid);
    
    print_logo();
    
    //register signal handlers
    register_signals();

    /* ----------- find out i2c interface name ------------------- */
    // /dev/i2c-0 (Raspberry PI Model A)
    // /dev/i2c-1 (Raspberry PI Model B, PI 2)

    strcpy(i2c_interface,"/dev/");
    if(listdir("/sys/class/i2c-dev/", i2c_interface) != 1){
        printf("No or multiple i2c interfaces found! Please clarify in %s:%d!\n",__FILE__, __LINE__);
        printf("%s???\n",i2c_interface);    
        exit_all(-1);
    }

    // CHECK connection to ads1115 whose ADDR=0x49 as i2c slave
    int fd; 
    fd = i2c_open(i2c_interface);
    i2c_select(fd, CFG_ADC1);   
#ifdef CFG_ADC2
    i2c_select(fd, CFG_ADC2);
#endif
    close(fd);

    
    int rc;
    rc=pthread_attr_init(&p_attr);
        PthreadCheck("pthread_attr_init", rc);
    rc=pthread_attr_setdetachstate(&p_attr, PTHREAD_CREATE_DETACHED);
        PthreadCheck("pthread_attr_setdetachstate", rc);
    rc = pthread_create(&p_thread, &p_attr, thread_datastore, NULL);
        PthreadCheck("pthread_create", rc);
    
    //set nice value
    int which = PRIO_PROCESS;
    if (setpriority(which, pid, -15) != 0){
        perror("set_priority");
        exit_all(-1);
    }

    //set FIFO policy and RT priority (highest)
    /* FIFO policy implies different priority levels than usual [-20,19]
     * the highest priority = RT (real time) equals 99
     */
    struct sched_param param;
    param.sched_priority = 99;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        perror("sched_setscheduler");
        exit_all(-1);  
    }

    // Lock memory 
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
        perror("mlockall failed");
        exit_all(-1);
    }

    // Pre-fault stack 
    stack_prefault();

    printf("Channel assignment:\n");
        printf("\tch1=[ADC:%#04x]:",CFG_ADC1);
        CFG_SEQ_PRINTER(CFG_SEQ1_ADC1);
    #if (CFG_NR_CH > 1)
        #if defined(CFG_SEQ2_ADC1) //single
            printf("\tch2=[ADC:%#04x]:",CFG_ADC1);
            CFG_SEQ_PRINTER(CFG_SEQ2_ADC1);
        #else //alternating
            printf("\tch2=[ADC:%#04x]:",CFG_ADC2);
            CFG_SEQ_PRINTER(CFG_SEQ2_ADC2);
        #endif
    #endif
    #if (CFG_NR_CH > 2)
        printf("\tch3=[ADC:%#04x]:",CFG_ADC1);
        CFG_SEQ_PRINTER(CFG_SEQ3_ADC1);
    #endif                
    #if (CFG_NR_CH > 3)
        #if defined(CFG_SEQ4_ADC1) //single
            printf("\tch4=[ADC:%#04x]:",CFG_ADC1);
            CFG_SEQ_PRINTER(CFG_SEQ4_ADC1);
        #else //alternating
            printf("\tch4=[ADC:%#04x]:",CFG_ADC2);
            CFG_SEQ_PRINTER(CFG_SEQ4_ADC2);
        #endif
    #endif  

    FILE *fp;
    fp=fopen(histfile,"w");

    
    /* 
     * sps is expected to be [1/60,5000] Hz, therefore
     * period=[60,2E-4] sec
     */
    float per = 1.0/(float)sps;

    /* struct timespec has members:
     *    time_t  tv_sec    //seconds
     *    long    tv_nsec   //nanoseconds     
     */
    struct timespec tper={0,0};
    tper.tv_sec = (long)(per);
    tper.tv_nsec = (long)((per-((float)tper.tv_sec * 1E9)) * 1E9);
    printf("sampling period %ld.%06lds determined\n",tper.tv_sec,tper.tv_nsec/1000);
    
    //some timers (monothonic, absolute and measurement timer)
    struct timespec t1, tm1, tm2, tabs, tabs2;
    long long cnt=0;
    long diff;
    //align to full second
    clock_gettime(CLOCK_REALTIME, &t1);
    tm2.tv_sec=0;
    tm2.tv_nsec=max(1E9L - t1.tv_nsec - 200,0);
    nanosleep(&tm2, NULL); 

    clock_gettime(CLOCK_MONOTONIC, &t1);
    while(!done){
        //increase absolute time by the value of period
        t1.tv_sec+=tper.tv_sec; //TODO: possible overflow        
        t1.tv_nsec+=tper.tv_nsec;
        tsnorm(&t1);

        //save measurement tm1 to tm2 
        tm2 = tm1;
        //sleep and get immediatelly the timestamp
        //TODO: check return value of nanosleep (0 if successful)
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t1, NULL);
        clock_gettime(CLOCK_MONOTONIC, &tm1);
        
        tabs2=tabs;
        clock_gettime(CLOCK_REALTIME, &tabs);
        
        take_a_sample(&tabs);
        
        if (debug){
            //statistics makes no sense for the first loop
            if (cnt++==0)
                continue;
            //handle overflow
            if (cnt == __LONG_LONG_MAX__)
                cnt = 1;
                
            //calculate jitter (difference to nominal value)
            diff = tper.tv_sec*1E6 + tper.tv_nsec/1E3 - calcdiff_us(tm1,tm2);
            fprintf(fp,"%d,",(int)diff);
        
            //termination 1 minute
            if (cnt >= 130*sps){
                done = 1;
                printf("TERMINATION (debug mode). Check histogram file %s\n",HISTFILE);
            }
        }
    
    }
    fseek(fp,-1,SEEK_CUR); //remove last comma
    fputs(" ",fp);
    fclose(fp);   
    
    
    printf("\n");
        
   
    return 0;
}
