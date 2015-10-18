#include "main_2.h"
#include "save_2.c"


// TODO prevent multiple instances 
// check /run/ads.pid ???
// TODO if sampling rate 1/300 what is in 1-minute file???
 
//--------------------------------------------------------------------------------------------------
void exit_all(int sig) { //Ctrl+C
//--------------------------------------------------------------------------------------------------
    done=1;
    pthread_cancel(p_thread);
//  PQfinish(pg_conn);
//  nopoll_ctx_unref(ctx);
    printf("\n\n");
    logErrDate("Termination signal received! Will now exit.\n\n");
    fflush(stdout);
    fclose (fp_log);
    exit(EXIT_FAILURE);
}

//----------------------------------------------------------------------
void ISRSamplingTimer(int sig) { //each 1/sps
//----------------------------------------------------------------------
    int err = -1;
    static int ov[4]; 
    int i;
    getADC(dst.it);

    if ((dst.it == sampl-1) || (dst.it == 2*sampl-1)){
        dst.t2=dst.t1;
        clock_gettime(CLOCK_REALTIME, &dst.t1);
    }

    for (i=0;i<4;i++){
        if (fabs(dst.data[dst.it][i]) > overvoltage){
            ov[i]++;
            if (ov[i] > 10){ 
                err = i;
                ov[i] = 0;
            }
        }
    }
    if (auto_pga)
        adjust_pga(dst.it);

    if (err > -1)
        printf("WARN: Overvoltage on ch%i! /ch1=%.0f, ch2=%.0f, ch3=%.0f ch4=%.0f/\n",err,dst.data[dst.it][0],dst.data[dst.it][1],dst.data[dst.it][2],dst.data[dst.it][3]);


    dst.it++;
    
    if (dst.it == sampl){   //FIRST batch ready for processing
        piter = 0;
        pthread_mutex_lock(&a_mutex);
        pthread_cond_signal(&got_request);
        pthread_mutex_unlock(&a_mutex);
    }
    else if (dst.it == 2*sampl){ //SECOND batch -"-
        piter = sampl;
        pthread_mutex_lock(&a_mutex);
        pthread_cond_signal(&got_request);
        pthread_mutex_unlock(&a_mutex);
        dst.it=0;
    }
        
    signal (sig, ISRSamplingTimer);
}


//----------------------------------------------------------------------
void getADC(int it) {
//----------------------------------------------------------------------
    static int adc1_addr=CFG_ADC1;
    struct timespec tim1, tim2, rem;
    int ret, cfg1;
    float pga1;
    const int cfg = ADS_CONF_OS_SNG | ADS_CONF_MODE_SNG | ADS_CONF_CQUE_NONE |
                    (ADS_CONF_DR_MASK & SPSa[SPSc]); //single shot mode
    
    static int fd;   //file descriptor (i2c)
    if (init){ //only once at the beginning
        fd = i2c_open(i2c_interface);
        init=0;
    }

    cfg1 = cfg | (ADS_CONF_MUX_MASK & CFG_SEQ1_ADC1) | (ADS_CONF_PGA_MASK & PGAa[pga[0]]);
    pga1 = PGAv[pga[0]];
    i2c_write(fd, adc1_addr, ADS_PTR_CONF, cfg1);     // set config register and start conversion ADS1115       
            
#if defined(CFG_ADC2) && defined(CFG_SEQ2_ADC2)
    static int adc2_addr=CFG_ADC2;
    int cfg2;
    float pga2;
    cfg2 = cfg | (ADS_CONF_MUX_MASK & CFG_SEQ2_ADC2) | (ADS_CONF_PGA_MASK & PGAa[pga[1]]);
    pga2 = PGAv[pga[1]];
    i2c_write(fd, adc2_addr, ADS_PTR_CONF, cfg2);
#endif
   
    tim1.tv_sec=0;
    tim1.tv_nsec=10000;
    nanosleep(&tim1, &rem);


    //check if A|C got correct config
    if (cfg1 != (i2c_read(fd, adc1_addr, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
        printf("adc1: cfg-w: %04X, read: %04X\n",cfg1, i2c_read(fd, adc1_addr, ADS_PTR_CONF));
        if (cfg1 != (i2c_read(fd, adc1_addr, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
            logErrDate("Slave 1 returned not the specified configuration. Data transfer from ADC to RPi corrupt!\n");
            exit_all(-1);
        }
    }

#ifdef CFG_ADC2  
    //check if B|D got correct config
    if (cfg2 != (i2c_read(fd, adc2_addr, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
        printf("adc2: cfg-w: %04X, read: %04X\n",cfg2, i2c_read(fd, adc2_addr, ADS_PTR_CONF));
        if (cfg2 != (i2c_read(fd, adc2_addr, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
            logErrDate("Slave 2 returned not the specified configuration. Data transfer from ADC to RPi corrupt!\n");
            exit_all(-1);
        }
    }
#endif

    // wait for conversion to complete on adc1_addr
    tim1.tv_sec=0;
    tim1.tv_nsec=500000; //1/860 /2   //(1000000000.0/SPSv[SPSc])*1.1;
    tim2.tv_sec=0;
    tim2.tv_nsec=10000;
    nanosleep(&tim1, &rem);
    while ((i2c_read(fd, adc1_addr, ADS_PTR_CONF) & ADS_CONF_OS_BUSY) == 0) {  
        nanosleep(&tim2, &rem); //wait 10us
    }

    // read conversion register adc1_addr and include sign bit
    ret = i2c_read(fd, adc1_addr, ADS_PTR_CNV);
    dst.data[it][0] = twocompl2float(ret,pga1);
    
#ifdef CFG_ADC2  
    //check if compleated also on adc2_addr
    while ((i2c_read(fd, adc2_addr, ADS_PTR_CONF) & ADS_CONF_OS_BUSY) == 0) {
        nanosleep(&tim2, &rem);
    }
    // read conversion register adc2_addr
    ret = i2c_read(fd, adc2_addr, ADS_PTR_CNV);
    dst.data[it][1] = twocompl2float(ret,pga2);
#endif        

}

//----------------------------------------------------------------------
float twocompl2float(int value, float pga){
//----------------------------------------------------------------------
    float ret;
    ret = (value > 0x7FFF) ? (float)(value-0xFFFF) : (float)(value); 
    return (ret * pga/32768.0); //mV    
}

//----------------------------------------------------------------------
void adjust_pga(int it){
//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
void stack_prefault(void) {
//----------------------------------------------------------------------
    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
    return;
}

//----------------------------------------------------------------------
void print_logo(void){
//----------------------------------------------------------------------
 printf("\to===================================o\n"
        "\t|      open source data logger      |\n"
        "\t|       for the Raspberry PI        |\n"
        "\t|     http://opendatalogger.com     |\n"
        "\to===================================o\n\n");
 printf(BUILDINFO);
}

//----------------------------------------------------------------------
int listdir(const char *dir, char *element){
//----------------------------------------------------------------------    
    DIR *d;
    struct dirent *direntry;
    int i=0;
    d = opendir(dir);
    if (d){
        while ((direntry=readdir(d)) != NULL){
            if (!strcmp(direntry->d_name,".") || !strcmp(direntry->d_name,".."))
                continue;
            if (i)
                strcat(element, "\n");                
            strcat(element, direntry->d_name);
            i++;
        }
    closedir(d);
    }
    return i;
}

//======================================================================
int main(int argc, char * argv[]){
//======================================================================
    struct itimerval new;
    float tv;
    int rc;
    int fd; 
    int which = PRIO_PROCESS;
    id_t pid;
    char path[200];
    char *ppath;
    char dest[200];
    char logfile[200];
    dst.t1.tv_sec=0;
    dst.t2.tv_sec=0;
    int nodaemon=0;
    
    pid = getpid();
    sprintf(path, "/proc/%d/exe", pid);
    if (readlink(path, dest, 200) == -1){
        perror("readlink");
        exit_all(-1);
    }
    else {
        ppath=dirname(dest);
        //printf("Executable's location: %s\n", ppath);
        sprintf(logfile,"%s/%s",ppath,LOGFILE);        
    }     
     
    if (argc>1){
        if (!strcmp(argv[1],"nodaemon") || !strcmp(argv[1],"-nodaemon") ||
            !strcmp(argv[1],"--nodaemon")){
            nodaemon=1;
        }
        if (!strcmp(argv[1],"help") || !strcmp(argv[1],"-help") || !strcmp(argv[1],"--help")){
            print_logo();
            printf("usage: \n");
            printf("    ads                 normal usage (daemonized)\n");
            printf("    ads -nodaemon       debug mode (not daemonizing)\n");
            printf("\n");
            printf("note:\n");
            printf("    currently you must invoke ads as root!\n\n");
            exit(0);
        }
    }

    if (!nodaemon){ // --------- Daemonize --------------- (default)
        
        pid_t process_id = 0;
        pid_t sid = 0;
        
        process_id = fork(); // Create child process
        if (process_id < 0){ // Indication of fork() failure
            logErrDate("fork failed!\n");
            exit(1); // Return failure in exit status
        }
        if (process_id > 0) { // PARENT PROCESS. Need to kill it.
            printf("Will now detach and go into background!\n");
            printf("redirecting stdout and stderr to: %s\n",logfile);
            logErrDate("Main daemon started with pid: %ld\n",(long)process_id);
            exit(0); // return success in exit status
        }
        umask(0); //unmask the file mode
        sid = setsid(); //set new session
        if(sid < 0){
            exit(1); // Return failure
        }
        // Change the current working directory to root.
        //chdir("/");
        // Close stdin. stdout and stderr
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
   

        printf("redirecting stdout and stderr to: %s\n",logfile);

        fp_log = freopen(logfile,"w",stdout); //redirect stdout to file
        setvbuf(fp_log, NULL, _IOLBF, 1024); // line buffering
        dup2(fileno(fp_log), fileno(stderr)); //redirect stderr to file
    }
    long li=1;
    long *uid=&li, *gid=&li;
    get_uid_gid(user, uid, gid);
    chown(logfile, *uid, *gid);
    chmod(logfile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    
    filelock(FILELOCK);    
    
    pid = getpid();
    printf("Executable: %s\n", dest);
    logErrDate("Main thread started with pid: %ld\n",(long)pid);
    
    print_logo();

    signal(SIGINT, exit_all); //e.g. Ctrl+c
    signal(SIGTERM, exit_all); //e.g. pkill

    // find out interface name
    // /dev/i2c-0 (Raspberry PI Model A)
    // /dev/i2c-1 (Raspberry PI Model B, PI 2)

    strcpy(i2c_interface,"/dev/");
    if(listdir("/sys/class/i2c-dev/", i2c_interface) != 1){
        printf("No or multiple i2c interfaces found! Please clarify in %s:%d!\n",__FILE__, __LINE__);
        printf("%s???\n",i2c_interface);    
        exit_all(-1);
    }

    fd = i2c_open(i2c_interface);
    i2c_select(fd, CFG_ADC1);               // CHECK connection to ads1115 whose ADDR=0x49 as i2c slave
#ifdef CFG_ADC2
    i2c_select(fd, CFG_ADC2);
#endif
    close(fd);

    
    sprintf(ini_name,"%s/%s",ppath,CONFIGFILE);

    if (parse_ini_file(ini_name)) //if it returns -1 -> no valid file
        parse_ini_file(ini_name);
    
    
   
    
    
    
    rc=pthread_attr_init(&p_attr);
        PthreadCheck("pthread_attr_init", rc);
    rc=pthread_attr_setdetachstate(&p_attr, PTHREAD_CREATE_DETACHED);
        PthreadCheck("pthread_attr_setdetachstate", rc);
    rc = pthread_create(&p_thread, &p_attr, thread_datastore, NULL);
        PthreadCheck("pthread_create", rc);
    

    if (setpriority(which, pid, -15) != 0){
        perror("set_priority");
        exit_all(-1);
    }

    struct sched_param param;
    param.sched_priority = 50;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        perror("sched_setscheduler");
        exit_all(-1);  
    }

    // Lock memory 
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
        perror("mlockall failed");
        exit(-2);
    }

    // Pre-fault stack 
    stack_prefault();

    //configure timer
    signal (SIGALRM, ISRSamplingTimer);
    //TODO  Instead, install your signal handlers with sigaction() instead of signal() , and set the SA_RESTART flag, which will cause system calls to automatically restart in case it got aborted by a signal.

    tv = 1.0/sps;
    
    /* man settimer
       Timers  will  never  expire  before the requested time, but may expire some (short) time
       afterward, which depends on the system timer resolution and  on  the  system  load;  see
       time(7) */
    new.it_interval.tv_sec = (int)tv;
    new.it_interval.tv_usec = (int)((tv-new.it_interval.tv_sec)*1000000);// TODO - 20; 
    new.it_value.tv_sec = 0;
    clock_gettime(CLOCK_REALTIME, &ts);
    new.it_value.tv_usec = 1000000 - ts.tv_nsec/1000 - 150 + 500000;
    if (new.it_value.tv_usec > 1000000)
        new.it_value.tv_usec -= 1000000;

    if (setitimer (ITIMER_REAL, &new, NULL) < 0){
        logErrDate("Timer init failed!!!\n");
        exit_all(-1);
    }
    else
        printf("Timer init succeeded. Interval %ld.%06lds set!\n",new.it_interval.tv_sec, new.it_interval.tv_usec);

    /* nopoll client init
    ctx = nopoll_ctx_new ();
    if (! ctx) {
        printf("Error creating nopoll context!\n"); 
        return nopoll_false; 
    }

    if (websocket){
        conn = nopoll_conn_new (ctx, "geodata.ggki.hu", "50500", NULL, NULL, NULL, NULL);
        if (! nopoll_conn_is_ok (conn)) {
            printf("Error connecting to websocket server!\n"); 
            exit_all(-1);
        }
    }
    */
    printf("\n");
    
        
    while(1)
        pause();    
    return 0;
}
