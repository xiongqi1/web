#ifndef _LUACORE_H
#define _LUACORE_H

#include <lua.h>

#define LUA_INTERFACE_FILE "/usr/lib/tr-069/init.lua"
#define LUA_INTERFACE_INIT_FUNCTION "initInterface"

int li_init();
int li_fini();

void li_lock();
void li_unlock();

lua_State *li_thread_new();
void li_thread_close(lua_State *L);

void li_die(lua_State *thread, char *msg, ...);
void li_stackDump(lua_State *thread);

#endif
