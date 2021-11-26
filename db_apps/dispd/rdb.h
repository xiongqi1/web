#ifndef __RDB_H__
#define __RDB_H__

#include "rdb_ops.h"

#define RDB_VARIABLE_MAX_LEN	1024
#define RDB_VARIABLE_NAME_MAX_LEN	128

int rdb_setVal(const char* var,const char* val);
const char* rdb_getVal(const char* var);
int rdb_init(void);
void rdb_fini(void);

extern struct rdb_session* rdb;

#endif


