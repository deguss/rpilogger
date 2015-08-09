void test_mkdir(char *);
void* thread_datastore(void*);
void saveit(double g);
off_t fsize(char *);

pthread_mutex_t a_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t got_request = PTHREAD_COND_INITIALIZER;
pthread_t  p_thread;       // thread's structure                    
pthread_attr_t p_attr;


//--------------------------------------------------------------------------------------------------
void* thread_datastore(void *void_p){ 
//--------------------------------------------------------------------------------------------------
	double g;
	int rc;
	sigset_t set;
	long tpid = (long)syscall(SYS_gettid);
	printf("data-storage thread started with pid: %ld\n",tpid);
	sigemptyset(&set);
	sigaddset(&set,SIGALRM);
	rc = pthread_sigmask(SIG_SETMASK, &set, NULL);
		PthreadCheck("pthread_sigmask", rc);


	struct passwd pwd;
	struct passwd *result;
	char *buf;
	size_t bufsize;
	int s;


	bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (bufsize == -1)          /* Value was indeterminate */
		bufsize = 16384;        /* Should be more than enough */

	buf = malloc(bufsize);
	if (buf == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	s = getpwnam_r("pi", &pwd, buf, bufsize, &result);
	if (result == NULL) {
		if (s == 0)
		 logErrDate("user pi not found\n");
		else {
		 errno = s;
		 perror("getpwnam_r");
		}
		exit(EXIT_FAILURE);
	}

	//printf("Name: %s UID: %ld\n", pwd.pw_name, (long) pwd.pw_uid);
	setfsuid((long)pwd.pw_uid);
	setfsgid((long)pwd.pw_gid);



	pthread_mutex_lock(&a_mutex);
	while (done==0) {
		pthread_cond_wait(&got_request, &a_mutex);
		pthread_mutex_unlock(&a_mutex); // ??????????
		g = (dst.t1.tv_sec*1E9+dst.t1.tv_nsec)-(dst.t2.tv_sec*1E9+dst.t2.tv_nsec);
		saveit(g);
		if (auto_pga && pga_uf){	//write new pga in ini file 
			update_ini_file();
			//printf("New PGA written in %s!\n",ini_name);
			pga_uf = 0; //clear update flag
		}
		pthread_mutex_lock(&a_mutex); // ????????

	}
	pthread_mutex_unlock(&a_mutex);
	pthread_exit(NULL);
}


//--------------------------------------------------------------------------------------------------
void saveit(double g){
//--------------------------------------------------------------------------------------------------
	static struct tm t1;
	int i;
	char str[500];

	/*
	if (websocket){ // --------------------------------------------------------------------------------------
		sprintf(str,"[");
		for(i=0;i<sampl;i++) 
			sprintf(str,"%s{\"x\":%ld%03ld, \"c\":%.9g, \"d\":%.9g},",str,dst.ts[piter+i].tv_sec,dst.ts[piter+i].tv_nsec/1000000, dst.data[piter+i][0],dst.data[piter+i][1]);
		*(str+strlen(str)-1)=']';
		if (!nopoll_conn_send_text(conn, str, strlen(str)))
			printf("ERROR sending data to websocket server!\n");

	} 
	if (savesql){ //-----------------------------------------------------------------------------------------
		if (PQstatus(pg_conn) != CONNECTION_OK){
		    pg_conn = PQconnectdb("dbname = postgres");
			printf("INFO: connected to database postgres\n");
			// Check to see that the backend pg_connection was successfully made 
			if (PQstatus(pg_conn) != CONNECTION_OK)    {
		        printf("ERROR: pg_connection to database failed: %s",PQerrorMessage(pg_conn));
			    PQfinish(pg_conn);
				exit(EXIT_FAILURE);
		    }
		}

		sprintf(&str[0],"INSERT INTO data VALUES ");
		for (i=0;i<sampl;i++){
			n=sprintf(str,"%s('%s',%.9g,%.9g),",str,tms[i], dst.data[piter+i][0], dst.data[piter+i][1]);
		}
		*(&str[0]+n-1)=';';
		//printf("(%i characters of reserved %i)\n",n,storsize);

		pg_res = PQexec(pg_conn, str);
		if (PQresultStatus(pg_res) != PGRES_COMMAND_OK) {
		    printf("ERROR: SQL command failed: %s", PQerrorMessage(pg_conn));
		    PQclear(pg_res);
			PQfinish(pg_conn);
			exit(EXIT_FAILURE);
		}
		PQclear(pg_res);

		sprintf(str,"insert into spsinfo (datetime, sps) select '%s', %g where not exists (select * from spsinfo where datetime = (select max(datetime) from spsinfo) and sps = %g);",tms[0],sps,sps);
		pg_res = PQexec(pg_conn, str);
		if (PQresultStatus(pg_res) != PGRES_COMMAND_OK) {
		    printf("ERROR: SQL command failed: %s", PQerrorMessage(pg_conn));
		    PQclear(pg_res);
			PQfinish(pg_conn);
			exit(EXIT_FAILURE);
		}
		PQclear(pg_res);
		//printf("\tSQL inserted %i records!\n",sampl);

	} */

	char dirn[100], fn[40], filen[150];

	if (saveascii || savebin){ //------------------------------------------------------------------------------
		t1 = *gmtime(&dst.t1.tv_sec); //first sample of batch
		sprintf(dirn,"%s/%d",savedirbase,t1.tm_year + 1900);
		test_mkdir(dirn);			//home/piter/data/2014
		sprintf(dirn,"%s/%02d", dirn, t1.tm_mon + 1);
		test_mkdir(dirn);			//home/piter/data/2014/12
		sprintf(dirn,"%s/%02d", dirn, t1.tm_mday); 
		test_mkdir(dirn);			//home/piter/data/2014/12/14
		sprintf(fn,"%02d%02d%02d.%04ld",t1.tm_hour,t1.tm_min,t1.tm_sec, dst.t1.tv_nsec/100000L); //file name only
	}	

	/*if (saveascii){ 	// generate ASCII file: *.cdl (netCDF)
	    FILE *nc=NULL;
		sprintf(filen,"%s/%s.cdl",dirn,fn);								//path to ASCII file (.cdl)
		nc = fopen(filen, "w");
		if (nc==NULL){
			printf("savefile: Could not open file %s to write!\n",filen);
			//printf("File will be omitted!\n");
			exit(EXIT_FAILURE);
		}
		fprintf(nc, 
			"netcdf %s{\n"
			"dimensions:\n"
			"	c = %i ;\n"
			"	d = %i ;\n"
			"variables:\n"
			"	float c(c) ;\n"
			"		c:units = \"mV\" ;\n"
			"	float d(d) ;\n"
			"		d:units = \"mV\" ;\n"
			"     :start=\"%s\";\n"
			"     :sps=%g.f;\n"
			"data:\n", fn, sampl, sampl,tms[0],sps);

		const char *s[2];
		s[0] = " c = ";
		s[1] = " d = ";
		for (i=0;i<2;i++){
			fprintf(nc,"%s",s[i]);
			for (j=0;j<sampl;j++)
				fprintf(nc,"%f,",dst.data[piter+j][i]);
			fseek(nc,-1,SEEK_CUR); //del last comma
			fprintf(nc,";\n");
		}
		fprintf(nc,	"}\n");
		if (!fclose(nc)){
#ifdef DEBUG
			printf("ASCII file %s written with %i samples each channel.\n",filen, sampl);
#else
			;
#endif
		}
		else {
			printf("savefile: Error while writing file %s!\n",filen);
			exit(EXIT_FAILURE);
		}
	}
*/ 

	if (savebin){ // generate BINARY file: *.nc (netCDF)	
		sprintf(filen,"%s/%s.nc",dirn,fn);								//path to BINARY file (.nc)
		int  ncid;  // netCDF id
		int c_dim, d_dim;     // dimension ids
		int c_id, d_id; // variable ids
		int status;
		size_t s1[1] = {0}, s2[1] = {sampl};
		float d[MAX_SAMPL];
		static long filesize_c;
		long filesize;

		status = nc_create(filen, NC_CLOBBER|NC_NETCDF4, &ncid);
		if (status != NC_NOERR){ //possible errors: NC_ENOMEM, NC_EHDFERR, NC_EFILEMETA
			logErrDate("savefile: Could not open file %s to write! nc_create: %s\nExit\n",filen,nc_strerror(status));
			exit(EXIT_FAILURE);
		}

		// define dimensions %s\n", strerror(errno)
		status = nc_def_dim(ncid, "ch3", sampl, &c_dim);
		if (status != NC_NOERR){
			logErrDate("savefile, nc_def_dim param ch3: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}
		status = nc_def_dim(ncid, "ch4", sampl, &d_dim);
		if (status != NC_NOERR){
			logErrDate("savefile, nc_def_dim param ch4: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}

		// define variables
		status = nc_def_var(ncid, "ch3", NC_FLOAT, 1, &c_dim, &c_id);
		if (status != NC_NOERR){ //  NC_BADID, NC_ENOTINDEFINE, NC_ESTRICTNC3, NC_MAX_VARS, NC_EBADTYPE, NC_EINVAL, NC_ENAMEINUSE, NC_EPERM
			logErrDate("savefile, nc_def_var param ch3: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}
		status = nc_def_var(ncid, "ch4", NC_FLOAT, 1, &d_dim, &d_id);
		if (status != NC_NOERR){
			logErrDate("savefile, nc_def_var param ch4: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}

		// assign global attributes
		sprintf(str,"%d-%02d-%02d %02d:%02d:%02d.%04ld",t1.tm_year+1900,t1.tm_mon+1, t1.tm_mday, t1.tm_hour,t1.tm_min,t1.tm_sec, dst.t1.tv_nsec/100000L);
		status = nc_put_att_text(ncid, NC_GLOBAL, "start", strlen(str), str);
		if (status != NC_NOERR){// NC_EINVAL, NC_ENOTVAR, NC_EBADTYPE, NC_ENOMEM, NC_EFILLVALUE
			logErrDate("savefile, nc_put_att_text param start: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}
		status = nc_put_att_float(ncid, NC_GLOBAL, "sps", NC_FLOAT, 1, &sps);
		if (status != NC_NOERR){
			logErrDate("savefile, nc_put_att_float param sps: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}


		// assign per-variable attributes
		status = nc_put_att_text(ncid, c_id, "units", 2, "mV");
		if (status != NC_NOERR){
			logErrDate("savefile, nc_put_att_text param units for ch3: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}
		status = nc_put_att_text(ncid, d_id, "units", 2, "mV");
		if (status != NC_NOERR){
			logErrDate("savefile, nc_put_att_text param units for ch4: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}
		status = nc_enddef (ncid);    // leave define mode
		if (status != NC_NOERR){
			logErrDate("savefile, nc_enddef: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}

		// assign variable data
		for (i=0;i<sampl;i++)
			d[i] = dst.data[piter+i][0];
		status = nc_put_vara_float(ncid, c_id, s1, s2, d);
		if (status != NC_NOERR){//NC_EHDFERR, NC_ENOTVAR, NC_EINVALCOORDS, NC_EEDGE, NC_ERANGE, NC_EINDEFINE, NC_EBADID, NC_ECHAR, NC_ENOMEM, NC_EBADTYPE
			logErrDate("savefile, nc_put_vara_float param ch3: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}
		for (i=0;i<sampl;i++)
			d[i] = dst.data[piter+i][1];
		status = nc_put_vara_float(ncid, d_id, s1, s2, d);
		if (status != NC_NOERR){
			logErrDate("savefile, nc_put_vara_float param ch4: %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}

		//close file
		status = nc_close(ncid);
		if (status != NC_NOERR){
			logErrDate("savefile, nc_close %s\nExit\n",nc_strerror(status));
			exit(EXIT_FAILURE);
		}
			//printf("%d-%02d-%02d %02d:%02d:%02d.%04ld  %.06g\n",t1.tm_year+1900,t1.tm_mon+1, t1.tm_mday, t1.tm_hour, t1.tm_min, t1.tm_sec, dst.ts[piter].tv_nsec/100000L, g/1E9);
		if (dst.t2.tv_sec == 0){
			filesize_c=(unsigned long)fsize(filen);
			printf("File written: %s. Samples: 2*%i.   (first file)      Size: %ldKiB\n",fn, sampl,(filesize_c)/1024);
		}
		else {
			filesize = (unsigned long)fsize(filen);
			printf("File written: %s. Samples: 2*%i. Ellapsed: %.4lfs. Size: %ldKiB\n",fn, sampl,g/1E9,(filesize)/1024);
			if (filesize != filesize_c){
				logErrDate("Error! Fize size not consistent!\nExit\n");
				exit(EXIT_FAILURE);
			}
		}

	}
}

//--------------------------------------------------------------------------------------------------
void test_mkdir(char *dirname){
//--------------------------------------------------------------------------------------------------
	struct stat st = {0};
	if (stat(dirname, &st) == -1) { //if dir does not exists, create it
	    if (mkdir(dirname, S_IRWXU | S_IRWXG | S_IRWXG | S_IXOTH)){
			logErrDate("Could not create directory %s!\n",dirname);
			exit(EXIT_FAILURE);
		}
	}
}

//--------------------------------------------------------------------------------------------------
off_t fsize(char *filename) {
//--------------------------------------------------------------------------------------------------
	struct stat st;

	if (stat(filename, &st) == 0)
		return st.st_size;

	logErrDate("Cannot determine size of %s\n", filename);

	return -1;
}

//--------------------------------------------------------------------------------------------------
void logErrDate(const char* format, ...) { 
//--------------------------------------------------------------------------------------------------
	char outstr[80];
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	tmp = localtime(&t);
	if (tmp == NULL) {
		perror("localtime");
		exit(EXIT_FAILURE);
	}
	strftime(outstr, 80, "%b %d %H:%M:%S: ", tmp);
	fprintf(stderr,outstr);

	va_list args;
	va_start (args, format);
	vfprintf(stderr, format, args);
	va_end (args);
}

