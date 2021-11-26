#ifndef EVENT_RDB_UTIL_H_20140623_
#define EVENT_RDB_UTIL_H_20140623_

#include "rdb_ops.h"

#define RDB_VARIABLE_MAX_LEN	1024
#define RDB_VARIABLE_NAME_MAX_LEN	128
#define RDB_TRIGGER_NAME_BUF	2048

#define RDB_VAR_SIZE	256
#define RDB_VAL_SIZE	128

int rdb_setVal(const char* var,const char* val);
char* rdb_getVal(const char* var);
int rdb_init(void);
void rdb_fini(void);
const char* rdb_name(const char* path, const char* name);

extern struct rdb_session* rdb;

#endif	/* EVENT_RDB_UTIL_H_20140623_ */


