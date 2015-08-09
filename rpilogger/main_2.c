// make
// with websocket (nopoll client) 	gcc -o ads main_2.c -lrt -lnetcdf -lpq -lm -lpthread -lnopoll -I/usr/include/nopoll -Wall
// with postgres gcc -o ads main_2.c -lrt -lnetcdf -lpq -lm -lpthread -Wall
#include <stdio.h> //to redirect stdout, stderr
#include <stdlib.h> //itoa
#include <stdint.h>
#include <unistd.h>
#include <math.h> //min max fmin fmax
#include <float.h> //FLT_EPSILON
#include <signal.h> //SIGINT
#include <time.h>
#include <sys/resource.h> //priority
#include <sched.h>    //real time scheduling
#include <pwd.h>	//get user id for PI
#include <stdarg.h> //logErrDate function (variable args)
#include <unistd.h>
#include <sys/fsuid.h> //set thread uid/gid
#include <sys/time.h>
#include <sys/stat.h> //mkdir, fsize
#include <pthread.h>  //threading, link with -lpthread
#include <syscall.h> //pid of thread
#include <sys/utsname.h> //lock memory
#include <sys/mman.h>

#include <netcdf.h> //binary file
//#include </usr/include/postgresql/libpq-fe.h> //postgresql 
//#include </usr/include/nopoll/nopoll.h> //websocket
#include "main_2.h"
#include "iniparser/iniparser.c"
#include "iniparser/dictionary.c"
#include "i2c.c"
#include "ads1115.c"
#define NELEMS(x)  (sizeof(x) / sizeof(x[0]))
#define MAX_SAFE_STACK (8*1024) // The maximum stack size which is guaranteed safe to access without  faulting 
#define LOGFILE "rpi.log"
FILE *fp_log; 


//#define DEBUG   


// global variables
const char programloc[] = "/home/pi/rpilogger";
char ini_comments[]=
	"; configuration file\n"
	"; take care to set only valid parameters when editing this file by your own!\n" 
	"; usually the setup takes places when invoking setup.py\n"
	"; valid range for\n"
	";  - sampling rate sps in [0.003333,250] (any float representing frequency in Hz)\n"
	";  - spsadc can be 8,16,32,64,128,250,475,860 with the restriction spsadc > 2*sps\n"
	";  - programmable gain amplifier pga_x expressing voltage ranges symmetrical\\\n" 
	";     around 6.144,4.096,2.048,1.024,0.512,0.256 Volts as indices 0,1,2,3,4,5\n"
	";  - if auto_pga is set to 1 dynamic input range will be choosen, in that case\n"
	";  - after pga_updelay amount of seconds ADC range will be lowered.\n;\n"
	";  - saveascii and savebin control whether ASCII files (NETCDF .cdl) and\n"
	";     binary files (NETCDF .nc) will be produced.\n\n\n";
int SPSc=0, spsadc=8, sec_files=60, sampl, piter, saveascii=1, savebin=1, savesql=1, websocket=1, done=0, init=1;
float sps;
char savedirbase[200];
const int SPSv[8] = {8,16,32,64,128,250,475,860};
const uint16_t SPSa[8] = {ADS_CONF_DR_8, ADS_CONF_DR_16, ADS_CONF_DR_32, ADS_CONF_DR_64, ADS_CONF_DR_128, ADS_CONF_DR_250, ADS_CONF_DR_475, ADS_CONF_DR_860};

int pga_c=0, pga_d=0, auto_pga=1, pga_updelay=10, pga_uf=0,scale=1; //individual amplifier gain for each channel
const float PGAv[6] = {6144.0, 4096.0, 2048.0, 1024.0, 512.0, 256.0};
const uint16_t PGAa[6] = {ADS_CONF_PGA_6_144V, ADS_CONF_PGA_4_096V, ADS_CONF_PGA_2_048V, ADS_CONF_PGA_1_024V, ADS_CONF_PGA_0_512V, ADS_CONF_PGA_0_256V};
#define MAX_SAMPL 600*60
struct data_struct {
	float data[2*MAX_SAMPL][2];
	struct timespec t1;
	struct timespec t2;
	int it;
} dst;

char ini_name[100];
struct timespec ts, ts1, ts2;
struct tm *pt1;
//PGconn     *pg_conn;
//PGresult   *pg_res;


//noPollConn * conn;
//noPollCtx * ctx;

/***************************************************************************/
/* our macro for errors checking                                           */
// thx for http://stackoverflow.com/questions/6202762/pthread-create-as-detached
/***************************************************************************/
#define COND_CHECK(func, cond, retv, errv) \
if ( (cond) ) \
{ \
   fprintf(stderr, "\n[CHECK FAILED at %s:%d]\n| %s(...)=%d (%s)\n\n",\
              __FILE__,__LINE__,func,retv,strerror(errv)); \
   exit(EXIT_FAILURE); \
}

#define ErrnoCheck(func,cond,retv)  COND_CHECK(func, cond, retv, errno)
#define PthreadCheck(func,rc) COND_CHECK(func,(rc!=0), rc, rc)



#include "save_2.c"
// TODO prevent multiple instances 
 //TODO if sampling rate 1/300 what is in 1-minute file???
//--------------------------------------------------------------------------------------------------
void exit_all(int sig) { //Ctrl+C
//--------------------------------------------------------------------------------------------------
	pthread_cancel(p_thread);


//	PQfinish(pg_conn);
//	nopoll_ctx_unref(ctx);
	printf("\nInterrupt?! Will exit.\n");
	fflush(stdout);
	fclose (fp_log);
	exit(EXIT_FAILURE);
}

//--------------------------------------------------------------------------------------------------
void ISRSamplingTimer(int sig) { //each 1/sps
//--------------------------------------------------------------------------------------------------
	char err = ' ';
	static int ov_c,ov_d;
	getADC(&dst.data[dst.it][0],NULL);

	if ((dst.it == sampl-1) || (dst.it == 2*sampl-1)){
		dst.t2=dst.t1;
		clock_gettime(CLOCK_REALTIME, &dst.t1);
	}


	if (fabs(dst.data[dst.it][0]) > 5000.0){
		ov_c++;
		if (ov_c > 10){ 
			err = 'C';
			ov_c = 0;
		}
	}
	if (fabs(dst.data[dst.it][1]) > 5000.0){
		ov_d++;
		if (ov_d > 10){ 
			err = 'D';
			ov_d = 0;
		}
	}
	if (err != ' ')
		printf("WARN: Overvoltage on %c! /C:%.0f\tD:%.0f/\n",err,dst.data[dst.it][0],dst.data[dst.it][1]);

	if (auto_pga){
		//channel A: adc value in *dataptr, pga gain in pga_a, voltage_divider is 2
		pga_c = adjust_pga('C', &dst.data[dst.it][0], pga_c, 1.0);
		pga_d = adjust_pga('D', &dst.data[dst.it][1], pga_d, 1.0);
	}	

	dst.it++;
	
	if (dst.it == sampl){	//FIRST batch ready for processing
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


//--------------------------------------------------------------------------------------------------
void getADC(float *p, char *sel) {
//--------------------------------------------------------------------------------------------------
	static int adc1=0x49, adc2=0x48;
	struct timespec tim1, tim2, rem;
	int ret, cfg1, cfg2;
	const int cfg = ADS_CONF_OS_SNG | ADS_CONF_MODE_SNG | ADS_CONF_CQUE_NONE | (ADS_CONF_DR_MASK & SPSa[SPSc]); //single shot mode
	float voltage_div=1,pga1,pga2;

	static int fd;	 //file descriptor (i2c)
	if (init){ //only once at the beginning
		fd = i2c_open("/dev/i2c-1");
		init=0;
	}

	cfg1 = cfg | (ADS_CONF_MUX_MASK & ADS_CONF_MUX_D_23) | (ADS_CONF_PGA_MASK & PGAa[pga_c]);
	cfg2 = cfg | (ADS_CONF_MUX_MASK & ADS_CONF_MUX_D_23) | (ADS_CONF_PGA_MASK & PGAa[pga_d]);
	pga1 = PGAv[pga_c];
	pga2 = PGAv[pga_d];

	// set config register and start conversion ADS1115 
	i2c_write(fd, adc1, ADS_PTR_CONF, cfg1); //for channel A and C
	i2c_write(fd, adc2, ADS_PTR_CONF, cfg2); //for channel B and D
	
	tim1.tv_sec=0;
	tim1.tv_nsec=10000;
	nanosleep(&tim1, &rem);


	//check if A|C got correct config
	if (cfg1 != (i2c_read(fd, adc1, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
		printf("adc1: cfg-w: %04X, read: %04X\n",cfg1, i2c_read(fd, adc1, ADS_PTR_CONF));
		if (cfg1 != (i2c_read(fd, adc1, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
			logErrDate("Slave 1 returned not the specified configuration. Data transfer from ADC to RPi corrupt!\n");
			exit_all(-1);
		}
	}

	//check if B|D got correct config
	if (cfg2 != (i2c_read(fd, adc2, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
		printf("adc2: cfg-w: %04X, read: %04X\n",cfg2, i2c_read(fd, adc2, ADS_PTR_CONF));
		if (cfg2 != (i2c_read(fd, adc2, ADS_PTR_CONF) | ADS_CONF_OS_BUSY)){
			logErrDate("Slave 2 returned not the specified configuration. Data transfer from ADC to RPi corrupt!\n");
			exit_all(-1);
		}
	}


	// wait for conversion to complete on adc1
	tim1.tv_sec=0;
	tim1.tv_nsec=500000; //1/860 /2   //(1000000000.0/SPSv[SPSc])*1.1;
	tim2.tv_sec=0;
	tim2.tv_nsec=10000;
	nanosleep(&tim1, &rem);
	while ((i2c_read(fd, adc1, ADS_PTR_CONF) & ADS_CONF_OS_BUSY) == 0) {  
		nanosleep(&tim2, &rem); //wait 10us
	}

	// read conversion register adc1 and include sign bit
	ret = i2c_read(fd, adc1, ADS_PTR_CNV);
	*p = ret > 0x7FFF ? (float)(ret-0xFFFF) : (float)(ret); 

	//check if compleated also on adc2
	while ((i2c_read(fd, adc2, ADS_PTR_CONF) & ADS_CONF_OS_BUSY) == 0) {
		nanosleep(&tim2, &rem);
	}
	// read conversion register adc2
	ret = i2c_read(fd, adc2, ADS_PTR_CNV);
	*(p+1) = ret > 0x7FFF ? (float)(ret-0xFFFF) : (float)(ret);

	if (scale){
		*p *= pga1/32768.0*voltage_div; //mV
		*(p+1) *= pga2/32768.0*voltage_div; //mV
	}
}

//--------------------------------------------------------------------------------------------------
int adjust_pga(char ch, float *p, int pga_x, float voltage_div){
//--------------------------------------------------------------------------------------------------
// call e.g. adjust_pga('A', (dataptr), pga_a, 2.0);
// pga_a = 0..5 representing ranges 6144.0, 4096.0, 2048.0, 1024.0, 512.0, 256.0
	static int sec_remC, sec_remD;
	int sec_rem;
	//initialization
	if (sec_remC <= 0)
		sec_remC = pga_updelay*sps;
	if (sec_remD <= 0)
		sec_remD = pga_updelay*sps;

	//select the proper one
	switch (ch){
		case 'C': sec_rem = sec_remC; break;
		case 'D': sec_rem = sec_remD; break;
		default: 
			logErrDate("adjust_pga: invalid channel specified!\n"); 
			exit_all(-1);
	}
	//printf("\t|%c|=%g, (%g)",ch,fabs(*p),0.95*voltage_div*PGAv[pga_x]);
	if ((fabs(*p) > 0.95*voltage_div*PGAv[pga_x]) && pga_x>0){ //range up immediatelly if not already the biggest range
		pga_x = max(pga_x-1,0);
		printf("Voltage range increased to +/-%6.3fV on channel %c. [%i/6]\n",voltage_div*PGAv[pga_x]/1000.0,ch,pga_x);
		pga_uf = 1; //set update flag
	}
	if ((fabs(*p) < 0.8*voltage_div*PGAv[min(pga_x+1,5)]) && pga_x<5 ){ //think over to range down if not already the lowest range
		sec_rem -= 1;
		if (sec_rem <=0){	//range down only after pga_updelay seconds
			pga_x = min(pga_x+1,5);
			printf("Voltage range decreased to +/-%6.3fV on channel %c after |%c|<80%% of once smaller range for %is. [%i/6]\n",voltage_div*PGAv[pga_x]/1000.0,ch,ch,pga_updelay,pga_x);				
			pga_uf = 1; //set update flag
		}
	}
	else 
		sec_rem = pga_updelay*sps;
	//store back
	switch (ch){
		case 'C': sec_remC = sec_rem; break;
		case 'D': sec_remD = sec_rem; break;
	}
	return pga_x;
}

//--------------------------------------------------------------------------------------------------------
void create_ini_file(char * ini_name){
//--------------------------------------------------------------------------------------------------------
	FILE *ini=NULL;
	ini = fopen(ini_name, "w");
	if (ini==NULL){
		logErrDate("create_ini_file: Could not open file %s to write!\n",ini_name);
		exit_all(-1);
	}
	fprintf(ini,"%s\n"
	"[default]\n"
	"sps = 1\n"
	"spsadc = 8\n"
	"pga_c = 0\n"
	"pga_d = 0\n"
	"auto_pga = 1\n"
	"pga_updelay = 10\n"
	"scale = 1\n\n"
	"[output]\n"
	"savedirbase = /home/pi/data\n"
	"sec_files = 60\n"
	"saveascii = 1\n"
	"savebin = 1\n"
	"savesql = 1\n"
	"websocket = 1\n"
	"\n",ini_comments);
	fclose(ini);
}

//--------------------------------------------------------------------------------------------------------
int parse_ini_file(char * ini_name) {
//--------------------------------------------------------------------------------------------------------
	dictionary  * ini ;
	ini = iniparser_load(ini_name);
	char * s;
	if (ini==NULL) {
		create_ini_file(ini_name);
		printf("Config file %s created.\n",ini_name);
		ini = iniparser_load(ini_name);
	}

	spsadc = iniparser_getint(ini, "default:spsadc", 8);
	sps = (float)iniparser_getdouble(ini, "default:sps", 1);
	pga_c = iniparser_getint(ini, "default:pga_c", 0);
	pga_d = iniparser_getint(ini, "default:pga_d", 0);
	auto_pga = iniparser_getint(ini, "default:auto_pga", 1);
	scale = iniparser_getint(ini, "default:scale", 1);
	pga_updelay = max(iniparser_getint(ini, "default:pga_updelay", 10),1);
	s = iniparser_getstring(ini, "output:savedirbase", "/home/pi/data");
	strcpy(&savedirbase[0],s);
	sec_files = min(max(iniparser_getint(ini, "output:sec_files", 60),1),60);
	saveascii = iniparser_getboolean(ini, "output:saveascii", 1);
	savebin = iniparser_getboolean(ini, "output:savebin", 1);
	savesql = iniparser_getboolean(ini, "output:savesql", 1);
	websocket = iniparser_getboolean(ini, "output:websocket", 1);

	//validity check
	if (isValueInIntArr(spsadc,&SPSv[0],NELEMS(SPSv)) && pga_c>-1 && pga_c<6 && pga_d>-1 && pga_d<6){
		iniparser_freedict(ini);
		for (SPSc=0;SPSc<8;SPSc++)
			if (SPSv[SPSc] == spsadc)
				break;
		printf("Config file %s read. sps: %gHz, spsadc: %iHz\n",ini_name,sps,SPSv[SPSc]);

		if (auto_pga) 
			printf("Dynamic input voltage range selection (in 5 steps)\n");
		else
			printf("Input voltage ranges C: +/-%g\tD: +/-%g\n",fmin(5000.0,PGAv[pga_c]),fmin(5000.0,PGAv[pga_d]));
		
		sampl = (int)((float)sec_files*sps);
		printf("%i seconds (%i samples) / file each to %s\n",sec_files,sampl,savedirbase);
		printf("ASCII: %s \tbinary: %s \tSQL: %s \tWEBSOCKET: %s\n",saveascii ? "YES" : "NO",savebin ? "YES" : "NO", savesql ? "YES" : "NO", websocket ? "YES" : "NO");
		return 0;
	}
	else {	
		logErrDate("Invalid config file!\n");
		create_ini_file(ini_name);
		printf("New config file %s created.\n",ini_name);
		iniparser_freedict(ini);
		return -1;
	}
}

//--------------------------------------------------------------------------------------------------------	
void update_ini_file(void){
//--------------------------------------------------------------------------------------------------------
	dictionary  *ini ;
	char bstr[33];
    FILE *fini ;

	ini = iniparser_load(ini_name);
	if (ini==NULL){
        logErrDate("update_ini_file: cannot parse ini-file: %s!\n Exit!\n", ini_name);
		exit_all(-1);
	}

	sprintf(bstr,"%i",pga_c);
	iniparser_set(ini, "default:pga_c", bstr);

	sprintf(bstr,"%i",pga_d);
	iniparser_set(ini, "default:pga_d", bstr);

	fini = fopen(ini_name, "w");
	if (fini==NULL){
        logErrDate("update_ini_file: cannot open ini-file %s to update!\nExit!\n", ini_name);
		exit_all(-1);
	}
	fprintf(fini,"%s\n",ini_comments);
	iniparser_dumpsection_ini(ini, "default", fini);
	iniparser_dumpsection_ini(ini, "output", fini);
	fclose(fini);

	iniparser_freedict(ini);
}

//--------------------------------------------------------------------------------------------------------
int isValueInIntArr(int val, const int *arr, int size){
//--------------------------------------------------------------------------------------------------------
	int i;
	for(i=0;i<size;i++)
		if (*(arr+i)==val)
			return 1;
	return 0;
}

//--------------------------------------------------------------------------------------------------------
int min(int x, int y){
//--------------------------------------------------------------------------------------------------------
	return y ^ ((x ^ y) & -(x < y));
}
 
//--------------------------------------------------------------------------------------------------------
int max(int x, int y){
//--------------------------------------------------------------------------------------------------------
	return x ^ ((x ^ y) & -(x < y));
}


//--------------------------------------------------------------------------------------------------------
void stack_prefault(void) {
//--------------------------------------------------------------------------------------------------------
	unsigned char dummy[MAX_SAFE_STACK];

	memset(dummy, 0, MAX_SAFE_STACK);
	return;
}

//========================================================================================================
int main(int argc, char * argv[]){
//========================================================================================================
	struct itimerval new;
	float tv;
	int rc;
	int fd; 
	int which = PRIO_PROCESS;
	id_t pid;
	dst.t1.tv_sec=0;
	dst.t2.tv_sec=0;







	pid = getpid();
	logErrDate("Main thread started with pid: %ld\n",(long)pid);
	printf("redirecting stdout and stderr to file: %s\n",LOGFILE);

	fp_log = freopen(LOGFILE,"w",stdout); //redirect stdout to file
	setvbuf(fp_log, NULL, _IOLBF, 1024); // line buffering
	dup2(fileno(fp_log), fileno(stderr)); //redirect stderr to file

	logErrDate("Main thread started with pid: %ld\n",(long)pid);



	printf(	"\t =================================== \n"
			"\t| Raspberry Pi logger - RPilogger01 |\n"
			"\t =================================== \n\n");


	signal(SIGINT, exit_all); //e.g. Ctrl+c
	signal(SIGTERM, exit_all); //e.g. pkill

	// open device on /dev/i2c-0 the default on Raspberry Pi A
	fd = i2c_open("/dev/i2c-1");
	i2c_select(fd, 0x49);				// CHECK connection to ads1115 whose ADDR=0x49 as i2c slave
	i2c_select(fd, 0x48);				// 0x48
	close(fd);
	
	// check if called with parameters as e.g. configfile.ini
	// $ sudo ./ads configfile.ini
	if (argc>1) 
		strcpy(ini_name,argv[1]);
	else 	// default file name
		sprintf(ini_name,"%s/daq.ini",programloc);

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

	/* Lock memory */

	if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
		perror("mlockall failed");
		exit(-2);
	}

	/* Pre-fault our stack */
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
		return -1;
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
