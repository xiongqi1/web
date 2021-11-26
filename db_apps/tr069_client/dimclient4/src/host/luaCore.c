#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "luaCore.h"
#include "luaDimclient.h"
#include "debug.h"

#define MINSTACK 16

static lua_State *LUA = NULL;
static pthread_mutex_t li_mutex = PTHREAD_MUTEX_INITIALIZER;


void li_stackDump (lua_State *L) {
	int i;
	int top = lua_gettop(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "Stack %p (%d): begin\n", L, top);)
	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(L, i);
		switch (t) {
			case LUA_TSTRING:  /* strings */
				DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "\t%d string:'%s'\n", i, lua_tostring(L, i));)
				break;
			case LUA_TBOOLEAN:  /* booleans */
				DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, ((lua_toboolean(L, i))?("\t%d bool:true\n"):("\t%d bool:false\n")), i);)
				break;
			case LUA_TNUMBER:  /* numbers */
				DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "\t%d number:%g\n", i, lua_tonumber(L, i));)
				break;
			default:  /* other values */
				DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "\t%d %s\n", i, lua_typename(L, t));)
				break;
		}
	}
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "Stack %p (%d): end\n", L, top);)
}

void li_die(lua_State *L, char *fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	DEBUG_OUTPUT(vdbglog(SVR_ERROR, DBG_LUA, fmt, argp);)
	va_end(argp);
	li_stackDump(L);
	lua_close(L);
	exit(EXIT_FAILURE);
}


void li_lock() {
//	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_lock(): begin\n");)
	pthread_mutex_lock(&li_mutex);
//	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_lock(): end\n");)
}

void li_unlock() {
	pthread_mutex_unlock(&li_mutex);
//	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_unlock()\n");)
}

static void li_stack() {
//	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_stack(): top = %d\n", lua_gettop(LUA));)
	if(!lua_checkstack(LUA, MINSTACK)) {
		li_die(LUA, "Error allocating stack space: %s\n", strerror(errno));
	}
}

static void _find_thread_container(lua_State *L) {
	lua_getglobal(L, DIMCLIENT_GLOBAL);
	if(!lua_istable(L, -1)) {
		li_die(L, "Can't find dimclient global table.\n");
	}
	lua_getfield(L, -1, DIMCLIENT_THREADS);
	lua_remove(L, -2);
	if(!lua_istable(L, -1)) {
		li_die(L, "Can't find dimclient thread container table.\n");
	}
}

lua_State *li_thread_new() {
	char buf[16];
	lua_State *L;

//	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_thread_new(): begin\n");)

	li_lock();
	li_stack();
//	DEBUG_OUTPUT(li_stackDump(LUA);)
	_find_thread_container(LUA);
	L = lua_newthread(LUA);
	sprintf(buf, "%p", L);
	lua_setfield(LUA, -2, buf);
	lua_pop(LUA, 1);
//	DEBUG_OUTPUT(li_stackDump(LUA);)
	li_unlock();

//	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_thread_new(): end newThread = %p\n", L);)

	return L;
}

void li_thread_close(lua_State *L) {
	char buf[16];

//	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_thread_close(%p): begin\n", L);)

	li_lock();
	li_stack();
//	DEBUG_OUTPUT(li_stackDump(LUA);)
	_find_thread_container(LUA);
	sprintf(buf, "%p", L);
	lua_pushnil(LUA);
	lua_setfield(LUA, -2, buf);
	lua_pop(LUA, 1);
//	DEBUG_OUTPUT(li_stackDump(LUA);)
	li_unlock();

//	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_thread_close(%p): end\n", L);)
}

int li_init() {
	int ret;
	char *luaFile = LUA_INTERFACE_FILE;

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_init(): begin\n");)

	// setup lua environment
	li_lock();
	LUA = lua_open();
	luaL_openlibs(LUA);
	if(luaL_loadfile(LUA, luaFile) || lua_pcall(LUA, 0, 0, 0)) {
		li_die(LUA, "Error running lua interface startup file '%s'.\n", luaFile);
	}

	li_stack();
	li_stackDump(LUA);

	// setup dimclient bindings
	if(li_dimclient_init(LUA)) {
		li_die(LUA, "Error initalising dimclient bindings.\n");
	}

	// call init function
	lua_getglobal(LUA, LUA_INTERFACE_INIT_FUNCTION);
	if(!lua_isfunction(LUA, -1)) {
		li_die(LUA, "Can't find lua interface init function '%s'.\n", LUA_INTERFACE_INIT_FUNCTION);
	}
	ret = lua_pcall(LUA, 0, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(LUA, -1);
		li_die(LUA, "The lua interface init function threw an error: %s\n", msg);
	}
	lua_pop(LUA, 1);
//	DEBUG_OUTPUT(li_stackDump(LUA);)
	li_unlock();
	
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_init(): end\n");)
	return 0;
}

int li_fini() {
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_fini(): begin\n");)
	li_lock();
	lua_close(LUA);
	LUA = NULL;
	li_unlock();
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_fini(): end\n");)

	return 0;
}
