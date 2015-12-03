/**
 *   @file    nanosleep.c
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com 
 *   @brief   timing performance testing
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
/* compile 
 * gcc -o build/nanosleep src/nanosleep.c -lm -lnetcdf -lrt -Wall -Wextra -Wno-missing-field-initializers -Wunused
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <float.h>
#include <inttypes.h>

#include <linux/unistd.h>

#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/mman.h>


#define HISTFILE "/home/pi/rpilogger/test/nanosleep.json"


#define MAX_SAFE_STACK (8*1024) 
void setSchedPrio_noerrcheck(void)
{
    struct sched_param param;
    
    /* FIFO policy implies different priority levels than usual [-20,19]
     * the highest priority = RT (real time) equals 99
     */
    param.sched_priority = 99;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        perror("sched_setscheduler");
    }

    // Lock memory 
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
        perror("mlockall failed");
    }

    // Pre-fault stack 
    unsigned char dummy[MAX_SAFE_STACK];
    memset(dummy, 0, MAX_SAFE_STACK);
    
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
        fprintf(stderr,"overflow in calcdiff\n");
        printf("diff: %" PRId64 "\n",diff); //this is the way to print int64
        exit(-1);
    }
    static int c;
    if (c++ < 0)
        printf("%ld\n",(long)diff);
    return (long)diff;
}

int main(int argc, char * argv[])
{
    struct timespec t1, tm1, tm2, tabs, tabs2;
    long period_us, duration;
    if (argc>2){
        period_us=atol(argv[1]);
        duration=atol(argv[2]);
    }
    else {
        period_us=500;
        duration=5;
    }
    printf("period: %ldus, total test time: %ld\n",period_us,duration);
    setSchedPrio_noerrcheck();
    
    
    double i, i_max, i_min, sum=0;
    long min=1E9L, max=0, diff;
    double lim = (1E6F/(double)period_us) * duration;
    int *jit;
    printf("testing for %ld seconds\n ...\n",duration);
    
    //allocate memory to store jitter array (for creating histograms)
    if (lim < __INT32_MAX__ && __SIZEOF_INT__ == 4){
        jit=calloc((int)lim, sizeof(int));
        if (jit == NULL){
            printf("not enough memory\n");
            return -1;    
        }            
    } else {
        printf("too much steps\n");
        return -1;
    }
    //align to full second
    clock_gettime(CLOCK_REALTIME, &t1);
    tm2.tv_sec=0;
    tm2.tv_nsec=1E9L - t1.tv_nsec - 300;
    nanosleep(&tm2, NULL); 


    clock_gettime(CLOCK_MONOTONIC, &t1);
    for(i=0; i<lim; i++){        
        //increase absolute time by the value of period_us
        t1.tv_nsec+=period_us*1E3; //TODO: possible overflow
        tsnorm(&t1);

        //save measurement tm1 to tm2 
        tm2 = tm1;
        //sleep and get immediatelly the timestamp
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t1, NULL);
        clock_gettime(CLOCK_MONOTONIC, &tm1);
        
        tabs2=tabs;
        clock_gettime(CLOCK_REALTIME, &tabs);
        //printf("abs time: %ld.%09ld",tabs.tv_sec, tabs.tv_nsec);        
        
        if (i==0){ //statistics makes no sense for the first loop
            continue;
        }
        
        *(jit+(int)i)=(int)(calcdiff_us(tabs,tabs2)-period_us);
            
        //calculate jitter (difference to nominal value)
        diff = period_us - calcdiff_us(tm1,tm2);
        
        //record maximum
        if (diff > max){
            max = diff;
            i_max = i;
        }
            
        //record minimum
        if (diff < min){
            min = diff;
            i_min = i;
        }
            
        //record moving average
        sum+=(double)diff;
        //avoid overflow of sum
        if (sum > __DBL_MAX__/10)
            break;         
        
    }
    
    printf("loops: [%.0f], avg: %lfus, min: %ldus @ %.3fs, max: %ldus @ %.3fs\n",
        i,sum/i, min,i_min/i*(double)duration, max,i_max/i*(double)duration);
    
    //calculate bin frequency for the histogram
    int bins = (int)(max-min+1);
    int *hist=calloc(bins, sizeof(int));
    if (hist == NULL){
        printf("not enough memory\n");
        return -1;    
    }
    int j,in;
    for (j=0; j<(int)lim; j++){
        //calculate index number [0,|min|+max+1]
        in=*(jit+j)-(int)min+1; 
        *(hist+in)=*(hist+in)+1;
    }
    FILE *fp;
    fp=fopen(HISTFILE,"w");

#if 0 //bin:value array
    fprintf(fp,"[");
    for (in=0;in<bins; in++) fprintf(fp,"\"%d\":%d,\n",(int)min+in,*(hist+in));
    fprintf(fp,"]\n");
#endif    
#if 0  //bin and value arrays
    fprintf(fp,"[[");
    for (in=0;in<bins; in++) fprintf(fp,"\'%d\',",(int)min+in);
    fprintf(fp,"],\n[");
    for (in=0;in<bins; in++) fprintf(fp,"%d,",*(hist+in));
    fprintf(fp,"]]\n");
#endif
#if 0  //list
    fprintf(fp,"[\n  [\"jitter\"],\n  ");
    for (in=0;in<lim-1; in++)
        fprintf(fp,"[%d],",*(jit+in));
    fprintf(fp,"[%d]\n",*(jit+in));   
    fprintf(fp,"]\n");
#endif    
#if 1  //octave, simple array
    fprintf(fp,"");
    for (in=0;in<lim-1; in++)
        fprintf(fp,"%d,",*(jit+in));
    fprintf(fp,"%d\n",*(jit+in));   
#endif 

    
    fclose(fp);
    free(jit);
    printf("histogram saved in %s\n",HISTFILE);
    
    return 0;
}
