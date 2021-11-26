#ifndef _LUACORE_H
#define _LUACORE_H

#include <lua.h>
#include "log.h"
#include "comms.h"


#define IF_DOWN_NONE  0
#define IF_DOWN_STOP  1
#define IF_DOWN_CLEAR 2


/// set http if up
///$ >=0 --if index
/// <0 	--error code
int lua_ifup(const char* ifup_script, TConnectSession* pSession,  unsigned int rdb_var_set_if);

/// set http if download
///$ >=0 --if index
/// <0 	--error code
int lua_ifdown(const char* ifdown_script, TConnectSession*pSession, int if_op);

void li_stackDump(lua_State *LUA);

#define li_die(LUA,...)  NTCLOG_ERR(##__VA_ARGS__);\
									li_stackDump(LUA);\
									exit(-1);

#endif

