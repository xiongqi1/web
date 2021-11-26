#ifndef _LRT_H
#define _LRT_H

#include <lua.h>
#include <stdio.h>

int lrt_init(lua_State *global_state);

void lrt_lock();
void lrt_unlock();

lua_State *lrt_thread_new();
void lrt_thread_close(lua_State *L);

void lrt_die(lua_State *thread, char *msg, ...);
void lrt_stackDump(lua_State *thread, FILE *file, char *title);

#endif
