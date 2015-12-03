void test_mkdir(char *dirname);
void* thread_datastore(void*);
void saveit(void);


pthread_mutex_t a_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t got_request = PTHREAD_COND_INITIALIZER;
int thr_id=0;         // thread ID for the newly created thread 
pthread_t  p_thread;       // thread's structure                    
pthread_attr_t p_attr;

//--------------------------------------------------------------------------------------------------
void* thread_datastore(void *void_p){ 
//--------------------------------------------------------------------------------------------------
	static int test;
	static struct tm t1;
	printf("data-storage thread started with pid: %ld\n",syscall(SYS_gettid));

	pthread_mutex_lock(&a_mutex);
	while (done==0) {
		if (test){
			printf("thread datastore probably not finished until new call!\n");
			test=0;
		}
		pthread_cond_wait(&got_request, &a_mutex);
		pthread_mutex_unlock(&a_mutex); // ??????????
		test=1;
		t1 = *gmtime(&dst.ts[piter].tv_sec);
		printf("%d-%02d-%02d %02d:%02d:%02d.%04ld\n",t1.tm_year+1900,t1.tm_mon+1, t1.tm_mday, t1.tm_hour,t1.tm_min,t1.tm_sec, dst.ts[piter].tv_nsec/100000L);
		saveit();
		if (auto_pga && pga_uf){	//write new pga in ini file 
			update_ini_file();
			printf("New PGA written in %s!\n",ini_name);
			pga_uf = 0; //clear update flag
		}
		test=0;
		pthread_mutex_lock(&a_mutex); // ????????

    }
	pthread_mutex_unlock(&a_mutex);
	pthread_exit(NULL);
}


//--------------------------------------------------------------------------------------------------
void saveit(void){
//--------------------------------------------------------------------------------------------------
	struct tm t1;
	char tms[430*60][25]; //worst case 1.2MB
	int i,n;
	char *sql;
	//float 8 byte: (7 digits+comma), datetime 24 byte (2015-02-07, 15:22:21.1234) + sample times 3bytes (),;
	int storsize=(sampl*(4*16+26+4) + 200) *sizeof(char);
	char str[430*60*100];
	for (i=0;i<sampl;i++){ //convert timespec to string
		t1 = *gmtime(&dst.ts[piter+i].tv_sec);
		sprintf(tms[i],"%d-%02d-%02d %02d:%02d:%02d.%04ld",t1.tm_year+1900,t1.tm_mon+1, t1.tm_mday, t1.tm_hour,t1.tm_min,t1.tm_sec, dst.ts[piter+i].tv_nsec/100000L);
	}

	if (websocket){ // --------------------------------------------------------------------------------------
		//printf("{\"x\":%ld.%09ld, \"y\":%.9g}\n",dst.ts[piter].tv_sec,dst.ts[piter].tv_nsec,dst.data[piter][0]);
		sprintf(str,"[");
		for(i=0;i<sampl;i++)
			sprintf(str,"%s{\"x\":%ld%03ld, \"a\":%.9g, \"b\":%.9g, \"c\":%.9g, \"d\":%.9g},",str,dst.ts[piter+i].tv_sec,dst.ts[piter+i].tv_nsec/1000000, dst.data[piter+i][0],dst.data[piter+i][1],dst.data[piter+i][2],dst.data[piter+i][3]);
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
				exit(-1);
		    }
		}
		sql = (char *) malloc(storsize); 
		sprintf(sql,"INSERT INTO data VALUES ");
		for (i=0;i<sampl;i++){
			n=sprintf(sql,"%s('%s',%.9g,%.9g,%.9g,%.9g),",sql,tms[i], dst.data[piter+i][0],dst.data[piter+i][1],dst.data[piter+i][2], dst.data[piter+i][3]);
		}
		*(sql+n-1)=';';
		//printf("(%i characters of reserved %i)\n",n,storsize);

		pg_res = PQexec(pg_conn, sql);
		if (PQresultStatus(pg_res) != PGRES_COMMAND_OK) {
		    printf("ERROR: SQL command failed: %s", PQerrorMessage(pg_conn));
		    PQclear(pg_res);
			PQfinish(pg_conn);
			exit(-1);
		}
		PQclear(pg_res);

		sprintf(sql,"insert into spsinfo (datetime, sps) select '%s', %g where not exists (select * from spsinfo where datetime = (select max(datetime) from spsinfo) and sps = %g);",tms[0],sps,sps);
		pg_res = PQexec(pg_conn, sql);
		if (PQresultStatus(pg_res) != PGRES_COMMAND_OK) {
		    printf("ERROR: SQL command failed: %s", PQerrorMessage(pg_conn));
		    PQclear(pg_res);
			PQfinish(pg_conn);
			exit(-1);
		}
		PQclear(pg_res);

		free(sql);
		//printf("\tSQL inserted %i records!\n",sampl);

	}

	char dirn[100], fn[40], filen[150];
    FILE *nc=NULL;
	int j;

	if (saveascii || savebin){ //------------------------------------------------------------------------------
		t1 = *gmtime(&dst.ts[piter].tv_sec); //first sample of batch
		test_mkdir(savedirbase);	//home/piter/data
		sprintf(dirn,"%s/%d",savedirbase,t1.tm_year + 1900);
		test_mkdir(dirn);			//home/piter/data/2014
		sprintf(dirn,"%s/%02d", dirn, t1.tm_mon + 1);
		test_mkdir(dirn);			//home/piter/data/2014/12
		sprintf(dirn,"%s/%02d", dirn, t1.tm_mday); 
		test_mkdir(dirn);			//home/piter/data/2014/12/14
		sprintf(fn,"%02d%02d%02d.%04ld",t1.tm_hour,t1.tm_min,t1.tm_sec, dst.ts[piter].tv_nsec/100000L); //file name only
	}	

	if (saveascii){ 	// generate ASCII file: *.cdl (netCDF)
		sprintf(filen,"%s/%s.cdl",dirn,fn);								//path to ASCII file (.cdl)
		nc = fopen(filen, "w");
		if (nc==NULL){
			printf("savefile: Could not open file %s to write!\n",filen);
			//printf("File will be omitted!\n");
			exit(-1);
		}
		fprintf(nc,
			"netcdf %s{\n"
			"dimensions:\n"
			"	a = %i ;\n"
			"	b = %i ;\n"
			"	c = %i ;\n"
			"	d = %i ;\n"
			"variables:\n"
			"	float a(a) ;\n"
			"		a:units = \"mV\" ;\n"
			"	float b(b) ;\n"
			"		b:units = \"mV\" ;\n"
			"	float c(c) ;\n"
			"		c:units = \"mV\" ;\n"
			"	float d(d) ;\n"
			"		d:units = \"mV\" ;\n"
			"     :start=\"%s\";\n"
			"     :sps=%g.f;\n"
			"data:\n", fn,sampl, sampl, sampl, sampl,tms[0],sps);

		const char *s[4];
		s[0] = " a = ";
		s[1] = " b = ";
		s[2] = " c = ";
		s[3] = " d = ";
		for (i=0;i<4;i++){
			fprintf(nc,"%s",s[i]);
			for (j=0;j<sampl;j++)
				fprintf(nc,"%f,",dst.data[piter+j][i]);
			fseek(nc,-1,SEEK_CUR); //del last comma
			fprintf(nc,";\n");
		}
		fprintf(nc,	"}\n");
		if (!fclose(nc))
#ifdef DEBUG
			printf("ASCII file %s written with %i samples each channel.\n",filen, sampl);
#else
			;
#endif
		else
			printf("savefile: Error while writing file %s!\n",filen);
	}


	if (savebin){ // generate BINARY file: *.nc (netCDF)	
		sprintf(filen,"%s/%s.nc",dirn,fn);								//path to BINARY file (.nc)
		int  ncid;  // netCDF id
		int a_dim, b_dim, c_dim, d_dim;     // dimension ids
		int a_id, b_id, c_id, d_id; // variable ids

		if (nc_create(filen, NC_CLOBBER, &ncid)){
			printf("savefile: Could not open file %s to write!\n",filen);
			//printf("File will be omitted!\n");
			exit(-1);
		}
		// define dimensions
		nc_def_dim(ncid, "a", sampl, &a_dim);
		nc_def_dim(ncid, "b", sampl, &b_dim);
		nc_def_dim(ncid, "c", sampl, &c_dim);
		nc_def_dim(ncid, "d", sampl, &d_dim);
		// define variables
		nc_def_var(ncid, "a", NC_FLOAT, 1, NULL, &a_id);
		nc_def_var(ncid, "b", NC_FLOAT, 1, NULL, &b_id);
		nc_def_var(ncid, "c", NC_FLOAT, 1, NULL, &c_id);
		nc_def_var(ncid, "d", NC_FLOAT, 1, NULL, &d_id);
		// assign global attributes
		nc_put_att_text(ncid, NC_GLOBAL, "start", strlen(tms[0]), tms[0]);
		nc_put_att_float(ncid, NC_GLOBAL, "sps", NC_FLOAT, 1, &sps);
		// assign per-variable attributes
		nc_put_att_text(ncid, a_id, "units", 2, "mV");
		nc_put_att_text(ncid, b_id, "units", 2, "mV");
		nc_put_att_text(ncid, c_id, "units", 2, "mV");
		nc_put_att_text(ncid, d_id, "units", 2, "mV");
		nc_enddef (ncid);    // leave define mode

		// assign variable data
		size_t s1[1] = {0}, s2[1] = {sampl};
		float d[430*60];
		for (i=0;i<sampl;i++)
			d[i] = dst.data[piter+i][0];
		nc_put_vara(ncid, a_id, s1, s2, d);

		for (i=0;i<sampl;i++)
			d[i] = dst.data[piter+i][1];
		nc_put_vara(ncid, b_id, s1, s2, d);

		for (i=0;i<sampl;i++)
			d[i] = dst.data[piter+i][2];
		nc_put_vara(ncid, c_id, s1, s2, d);		

		for (i=0;i<sampl;i++)
			d[i] = dst.data[piter+i][3];
		nc_put_vara(ncid, d_id, s1, s2, d);

		if (!nc_close(ncid))
#ifdef DEBUG
			printf("Binary file %s written with %i samples each channel.\n",filen, sampl);
#else
			;
#endif
		else
			printf("savefile: Error while writing file %s!\n",filen);
	}
}

//--------------------------------------------------------------------------------------------------
void test_mkdir(char *dirname){
//--------------------------------------------------------------------------------------------------
	struct stat st = {0};
	if (stat(dirname, &st) == -1) { //if dir does not exists, create it
	    if (mkdir(dirname, S_IRWXU | S_IRWXG | S_IRWXG | S_IXOTH)){
			printf("Could not create directory %s!\n",dirname);
			exit(-1);
		}
	}
}


