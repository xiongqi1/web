#ifndef __RDB_UTIL_
#define __RDB_UTIL_

int rdb_start(void);

void rdb_end(void);

char *get_single( char *myname );

char *get_single_base64( char *name );

char* get_single_raw(const char *myname);

int set_single(char *myname, char *myvalue);

#endif
