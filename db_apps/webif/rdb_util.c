#include <stdlib.h>
#include <string.h>
#include "rdb_util.h"
#include "rdb_ops.h"
#include "cdcs_base64.h"

char	name[MAX_NAME_LENGTH+1];
#define BUFFER_LEN	2048
char	value[BUFFER_LEN];
struct	rdb_session *s = NULL;

int rdb_start(void)
{
	return rdb_open(NULL, &s);
}

void rdb_end(void)
{
	rdb_close(&s);
}


char *get_single( char *myname )
{
	int len = sizeof(value);
	*value=0;
	if(rdb_get(s, myname, value, &len))
		;//printf("ioctl GETSINGLE returns %i ( %s )\n",-errno, strerror(errno) );
	if(*value==0)
		strcpy(value,"N/A");
	return value;
}

/* Read a RDB variable and returns base64-encoded value
 * @name: RDB variable name to read
 * Returns base64-encoded value; On error returns empty string.
 */
char *get_single_base64( char *name )
{
	char value_tmp[BUFFER_LEN];
	char len;
	if (!s || !name || rdb_get_string(s, name, value_tmp, sizeof(value_tmp))
		|| !(len = strlen(value_tmp))
		|| cdcs_base64_get_encoded_len(len) > sizeof(value)
		|| cdcs_base64encode(value_tmp, len, value, 0) <= 0) {
		value[0] = 0;
		return value;
	}
	return value;
}

char* get_single_raw(const char *myname) {
	static const char* empty="";
	int len = sizeof(value);
	*value=0;
	if(rdb_get(s, myname, value, &len))
		strcpy(value,empty);

	return value;
}

int set_single( char *myname, char *myvalue )
{
	return rdb_update_string(s, myname, myvalue, 0, DEFAULT_PERM);
}
