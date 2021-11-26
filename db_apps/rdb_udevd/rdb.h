#ifndef __RDB_H__
#define __RDB_H__

#include "rdb_ops.h"

#define RDB_VARIABLE_MAX_LEN	1024
#define RDB_VARIABLE_NAME_MAX_LEN	256
#define RDB_ENUM_MAX_LEN	4096

int rdb_set_value(const char* var,const char* val, int persist);
const char* rdb_get_value(const char* var);
int rdb_del(const char* var);

char* rdb_get_printf(const char* fmt,...);
int rdb_set_printf(const char* rdb,const char* fmt,...);
int rdb_del_printf(const char* var,const char* fmt,...);

const char* rdb_var_printf(const char* fmt,...);

int rdb_init(void);
void rdb_fini(void);
char* rdb_enum(const char* name,int flags);

int rdb_get_handle();
struct rdb_session* rdb_get_struc();

typedef int (*rdb_regex_enum_callback)(const char* name,int ref);
int rdb_regex_enum(const char* name,const char* regex,rdb_regex_enum_callback cb,int ref);

#endif


