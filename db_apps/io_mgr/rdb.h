#ifndef __RDB_H__
#define __RDB_H__

#include "rdb_ops.h"

#define RDB_VARIABLE_MAX_LEN	1024
#define RDB_VARIABLE_NAME_MAX_LEN	128
#define RDB_ENUM_MAX_LEN	4096

int rdb_setVal(const char* var,const char* val, int persist);
const char* rdb_getVal(const char* var);
int rdb_init(void);
void rdb_fini(void);
char* rdb_enum(int flags);

int rdb_get_handle();
struct rdb_session* rdb_get_struc();

#endif


