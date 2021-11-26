#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>

#include "rdb_ops.h"

#include "rdb.h"

static char val[RDB_VARIABLE_MAX_LEN];
struct rdb_session* rdb=NULL;

int rdb_setVal(const char* var,const char* val)
{
	int rc;

	if( (rc=rdb_update_string(rdb, var, val, 0, 0))<0 ) {
		syslog(LOG_ERR,"failed to write to rdb(%s) - %s",var,strerror(errno));
	}

	return rc;
}

const char* rdb_getVal(const char* var)
{
	int len;
	
	len=sizeof(val);
	
	if(rdb_get(rdb,var,val,&len)<0) {
		return NULL;
	}
	
	return val;
}


int rdb_init(void)
{
	// open rdb database
	if( rdb_open(NULL,&rdb)<0 ) {
		syslog(LOG_ERR,"failed to open rdb driver - %s",strerror(errno));
		return -1;
	}
	
	return 0;
}


void rdb_fini(void)
{		
	if(rdb)
		rdb_close(&rdb);
}

#ifdef MODULE_TEST
int main(int argc,char* argv[])
{
	const char* p;
	rdb_init();
	
	if( rdb_setVal("rdb_test","abc")<0 ) {
		printf("rdb_set() failed\n");
		goto fini;
	}
	
	if(!(p=rdb_getVal("rdb_test"))) {
		printf("rdb_get() failed\n");
		goto fini;
	}
	
	if(strcmp(p,"abc")) {
		printf("rdb value not matching\n");
		goto fini;
	}
	
	printf("module test ok\n");
	
fini:	
	rdb_fini();
	
	return 0;
}
		
		
#endif
