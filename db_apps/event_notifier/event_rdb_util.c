#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>

#include "rdb_ops.h"

#include "event_rdb_util.h"

static char val[RDB_VARIABLE_MAX_LEN];
struct rdb_session* rdb=NULL;

/* Returns a pointer to a static buffer with a string like "<path>.<name>".
 * Note: it has 10 buffers, so each return value is valid for up to 10 calls.
 */
const char* rdb_name(const char* path, const char* name)
{
	enum { size = 10 };
	static unsigned int i = 0;
	static char names[size][256];
	const char* second_dot = name && *name ? "." : "";
	char* n = names[i];
	sprintf(n, "%s%s%s", path, second_dot, name);
	if (++i == size)
	{
		i = 0;
	}
	return n;
}

int rdb_setVal(const char* var,const char* val)
{
	int rc;

	if( (rc=rdb_update_string(rdb, var, val, 0, 0))<0 ) {
		syslog(LOG_ERR,"failed to write to rdb(%s) - %s",var,strerror(errno));
	}

	return rc;
}

/* This function returns a pointer to a global buffer so caller function
 * should copy to other buffer before calling this function again within same function. */
char* rdb_getVal(const char* var)
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
	/* open rdb database */
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

