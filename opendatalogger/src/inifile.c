/**
 *   @file    inifile.c
 *   @author  Daniel Piri
 *   @link    http://opendatalogger.com
 *   @brief   Manipulating config files.
 *   
 *   This software is licensed under the GNU General Public License.
 *   (CC-BY-NC-SA) You are free to adapt, share but non-commercial.
 */
 
#include "inifile.h"


void create_ini_file(char *ini_name)
{
    uid_t uid;
    gid_t gid;

    FILE *ini=NULL;
    ini = fopen(ini_name, "w");
    if (ini==NULL){
        logErrDate("create_ini_file: Could not open file %s to write!\n",
                ini_name);
        exit_all(-1);
    }
    
    //set unbuffered mode
    if (setvbuf(ini, NULL, _IONBF, 0) != 0){
        logErrDate("%s: failed to set unbuffered file IO!\n",__func__);
    }
    
    fprintf(ini,"%s\n"
    "[default]\n"
    "sps = 500\n"
    "spsadc = 860\n"
    "pga_ch1 = 0\n"
    "pga_ch2 = 0\n"
    "pga_ch3 = 0\n"
    "pga_ch4 = 0\n"    
    "auto_pga = 1\n"
    "pga_updelay = 300\n\n"
    "[output]\n"
    "datafile_dir = /home/pi/data\n"
    "file_user = pi\n"
    "\n",ini_comments);
    fclose(ini);

    /* since sampling process runs as file_user (pi) modify permissions */
    get_uid_gid(file_user, &uid, &gid);
    if (chown(ini_name, uid, gid) || chmod(ini_name, S_IRUSR | S_IWUSR | 
    S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)){
        logErrDate("%s: chown, chmod failed for %s!",__func__,ini_name);
        exit_all(-1);
    }
}


int parse_ini_file(char *ini_name) 
{
    //printf("%s %s\n",__func__,ini_name);
    if (ini_name == NULL){
        logErrDate("%s: no ini name provided?!\n",__func__);
        exit_all(-1);
    }

    struct stat sts;
    if (stat(ini_name, &sts) != 0){ //file does not exist
        create_ini_file(ini_name);
        printf("No config file present. %s created.\n",ini_name);
    }
    
    dictionary  *ini ;
    ini = iniparser_load(ini_name);
    
    if (ini==NULL) {
        create_ini_file(ini_name);
        printf("Config file %s created.\n",ini_name);
        ini = iniparser_load(ini_name);
    }

    spsadc = iniparser_getint(ini, "default:spsadc", 860);
    sps = (float)iniparser_getdouble(ini, "default:sps", 500);
    pga[0] = iniparser_getint(ini, "default:pga_ch1", 0);
    pga[1] = iniparser_getint(ini, "default:pga_ch2", 0);
    pga[2] = iniparser_getint(ini, "default:pga_ch3", 0);
    pga[3] = iniparser_getint(ini, "default:pga_ch4", 0);
    auto_pga = iniparser_getint(ini, "default:auto_pga", 1);
    pga_updelay = max(iniparser_getint(ini, "default:pga_updelay", 300),1);
    
    char *s; //pointer to statically allocated char string
    s = iniparser_getstring(ini, "output:datafile_dir", "/home/pi/data");
    strcpy(datafiledir,s);
    s = iniparser_getstring(ini, "output:file_user", "pi");
    strcpy(file_user,s);
    
    //validity check
    if (isValueInIntArr(spsadc,SPSv,SPSv_count) && pga[0]>-1 && pga[0]<6 &&
    pga[1]>-1 && pga[1]<6 && pga[2]>-1 && pga[2]<6 && pga[3]>-1 && pga[3]<6){
        iniparser_freedict(ini);
        //verify spsadc for feasibility
        for (SPSc=0; SPSc<SPSv_count; SPSc++)
            if (SPSv[SPSc] == spsadc)
                break;
                
        //if not found
        if (SPSc >= SPSv_count){
            SPSc = SPSv_count-1;
            printf("Invalid spsadc specified (%d)! Will use %d instead!\n",
            spsadc,SPSv[SPSc]);
            spsadc=SPSv[SPSc];
        }
        
        sampl = (int)((float)60*sps);                
        printf("Config file %s read. sps=%gHz, spsadc: %iHz, (1 min contains "
        "%d samples)\n",ini_name,sps,SPSv[SPSc],sampl);

        if (auto_pga) 
            printf("Dynamic input voltage range selection (in 5 steps)\n");
        else
            printf("Input voltage ranges ch1:+/-%g, ch2: +/-%g, ch3: +/-%g, ch4: +/-%g\n",
                fmin(overvoltage,PGAv[pga[0]]),fmin(overvoltage,PGAv[pga[1]]),
                fmin(overvoltage,PGAv[pga[2]]),fmin(overvoltage,PGAv[pga[3]]));
        

        printf("Data location: %s, file owner: %s\n",datafiledir,file_user);
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

void update_ini_file(char *ini_name)
{
    dictionary  *ini ;
    char bstr[33];
    FILE *fini ;

    ini = iniparser_load(ini_name);
    if (ini==NULL){
        logErrDate("update_ini_file: cannot parse ini-file: %s!\n Exit!\n", ini_name);
        exit_all(-1);
    }
    sprintf(bstr,"%i",pga[0]);
    iniparser_set(ini, "default:pga_ch1", bstr);    
    
    sprintf(bstr,"%i",pga[1]);
    iniparser_set(ini, "default:pga_ch2", bstr);

    sprintf(bstr,"%i",pga[2]);
    iniparser_set(ini, "default:pga_ch3", bstr);

    sprintf(bstr,"%i",pga[3]);
    iniparser_set(ini, "default:pga_ch4", bstr);

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



void logErrDate(const char *format, ...) 
{ 
    char outstr[80];
    time_t t = time(NULL);
    struct tm *tmp;

    if ((tmp = localtime(&t)) == NULL) {
        printf("localtime");
        perror("localtime");
        exit_all(-1);
    }
    strftime(outstr, 80, "%b %e %T: ", tmp); 
    fprintf(stderr,outstr);

    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
}


int min(int x, int y)
{
    if (x<y)
        return x;
    else 
        return y;
}
 
int max(int x, int y)
{
    if (x>y)
        return x;
    else 
        return y;
}

int isValueInIntArr(int val, const int *arr, int size)
{
    int i;
    for(i=0;i<size;i++)
        if (*(arr+i)==val)
            return 1;
    return 0;
}
