/**
 *   @file    system.c
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com 
 *   @brief   Provides miscellaneous helper functions.
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
 
#include "system.h"

static int latency_target_fd = -1;
static int32_t latency_target_value = 0;

/* exit function
 * will be called when receiving SIGINT
*/
void exit_all(int sig) 
{ //Ctrl+C

    sig=sig+1; //dummy operation, however needed to supress warn
    done=1;
    pthread_cancel(p_thread);
    printf("\n\n");
    logErrDate("Termination signal received! Will now exit.\n\n");
    fflush(stdout);
    fclose(fp_log);
    exit(EXIT_FAILURE);
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
                    //exit_all(-1);
                    return -1;
                }
            }       
        }
    } 
    return 0;
}


long fsize(char *filename) 
{

    struct stat st;

    if (stat(filename, &st) == 0)
        return (long)st.st_size;

    logErrDate("Cannot determine size of %s\n", filename);

    return -1;
}

void stack_prefault(void) 
{
    unsigned char dummy[MAX_SAFE_STACK];
    memset(dummy, 0, MAX_SAFE_STACK);
    return;
}

void print_logo(void)
{
 printf("\to===================================o\n"
        "\t|      open source data logger      |\n"
        "\t|       for the Raspberry PI        |\n"
        "\t|     http://opendatalogger.com     |\n"
        "\to===================================o\n\n");
    BUILDINFO();
}

void print_usage(void)
{
 printf("usage: \n"
        "    ads                 normal usage (daemonized)\n"
        "    ads -nodaemon       debug mode (not daemonizing, more output)\n"
        "    \n"
        "note:\n"
        "    currently you must invoke ads as root!\n\n");
} 

int listdir(const char *dir, char *element)
{
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

/* Latency trick
 * if the file /dev/cpu_dma_latency exists,
 * open it and write a zero into it. This will tell
 * the power management system not to transition to
 * a high cstate (in fact, the system acts like idle=poll)
 * When the fd to /dev/cpu_dma_latency is closed, the behavior
 * goes back to the system default.
 */
void set_latency_target(void)
{
    struct stat s;
    int err;

    err = stat("/dev/cpu_dma_latency", &s);
    if (err == -1) {
        logErrDate("%s: stat /dev/cpu_dma_latency failed!\n",__func__);
        return;
    }

    latency_target_fd = open("/dev/cpu_dma_latency", O_RDWR);
    if (latency_target_fd == -1) {
        logErrDate("%s: open /dev/cpu_dma_latency\n",__func__);
        return;
    }

    err = write(latency_target_fd, &latency_target_value, 4);
    if (err < 1) {
        logErrDate("%s: error setting cpu_dma_latency to %d!\n", 
            __func__,latency_target_value);
        close(latency_target_fd);
        return;
    }
    printf("# /dev/cpu_dma_latency set to %dus\n", latency_target_value);
}
 
