// prototypes
void exit_all(int);
void ISRSamplingTimer(int);
int adjust_pga(char ch, float *p, int pga_x, float voltage_div);
void getADC(float*, char*);
void update_ini_file(void);
void create_ini_file(char*);
int parse_ini_file(char*);
int isValueInIntArr(int val, const int *arr, int size);
int min(int x, int y);
int max(int x, int y);
void stack_prefault(void);
void logErrDate(const char* format, ...);
