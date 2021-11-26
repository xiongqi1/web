#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "global.h"
#include "lrt.h"

#define MINSTACK 16		/*< minimum lua global_state stack size */
static lua_State *global_state = NULL;

#define REGISTRY_KEY "lrt"	/*< registry key for thread state */

/* mutex to protect core function global state */
static pthread_mutex_t lrt_mutex = PTHREAD_MUTEX_INITIALIZER;

/* print a stack dump of a lua state */
void lrt_stackDump (lua_State *L, FILE *where, char *title) {
	int i;
	int top = lua_gettop(L);
	fprintf(where, "%s: %p, stack(%d): ", title, (void*)L, top);
	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(L, i);
		switch (t) {
			case LUA_TSTRING:  /* strings */
				fprintf(where, "string:'%s'", lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:  /* booleans */
				fprintf(where, ((lua_toboolean(L, i))?("bool:true"):("bool:false")));
				break;
			case LUA_TNUMBER:  /* numbers */
				fprintf(where, "number:%g", lua_tonumber(L, i));
				break;
			default:  /* other values */
				fprintf(where, "%s", lua_typename(L, t));
				break;
		}
		fprintf(where, ", ");  /* put a separator */
	}
	fprintf(where, "\n");  /* end the listing */
}

/* printing stack on thread death, exit process if thread was global_state */
void lrt_die(lua_State *L, char *fmt, ...) {
	va_list argp;
	va_start(argp, fmt);

	vfprintf(stderr, fmt, argp);
	va_end(argp);

	lrt_stackDump(L, stderr, "lrt_die()");

	if(L == global_state) {
		lua_close(L);
		exit(EXIT_FAILURE);
	} else {
		lrt_thread_close(L);
	}
}

/* lock mutex protecting global_state access */
void lrt_lock() {
	DEBUG(fprintf(stderr, "lrt_lock(): try\n"));
	pthread_mutex_lock(&lrt_mutex);
	DEBUG(fprintf(stderr, "lrt_lock(): locked\n"));
}

/* unlock mutex protecting global_state access */
void lrt_unlock() {
	pthread_mutex_unlock(&lrt_mutex);
	DEBUG(fprintf(stderr, "lrt_unlock(): unlocked\n"));
}

/* ensure MINSTACK stack space in global_state */
static void lrt_stack() {
	DEBUG(fprintf(stderr, "lrt_stack(): top = %d\n", lua_gettop(global_state)));
	if(!lua_checkstack(global_state, MINSTACK)) {
		lrt_die(global_state, "Error allocating stack space: %s\n", strerror(errno));
	}
}


static void _find_thread_container(lua_State *L) {
	lua_pushstring(L, REGISTRY_KEY);
	lua_gettable(L, LUA_REGISTRYINDEX);
	if(!lua_istable(L, -1)) {
		lrt_die(L, "Can't find '%s' registry key.\n", REGISTRY_KEY);
	}
}

/* create a new thread sharing global_state global variables but with own stack */
lua_State *lrt_thread_new() {
	char buf[16];
	lua_State *L;

	DEBUG(fprintf(stderr, "lrt_thread_new(): begin\n"));

	lrt_lock();
	lrt_stack();
	DEBUG(lrt_stackDump(global_state, stderr, "lrt_thread_new(): before"));
	_find_thread_container(global_state);
	L = lua_newthread(global_state);
	sprintf(buf, "%p", L);
	lua_setfield(global_state, -2, buf);
	lua_pop(global_state, 1);
	DEBUG(lrt_stackDump(global_state, stderr, "lrt_thread_new(): after"));
	lrt_unlock();

	DEBUG(fprintf(stderr, "lrt_thread_new(): end newThread = %p\n", L));

	return L;
}

/* release thread */
void lrt_thread_close(lua_State *L) {
	char buf[16];

	DEBUG(fprintf(stderr, "lrt_thread_close(%p): begin\n", L));

	lrt_lock();
	lrt_stack();
	DEBUG(lrt_stackDump(global_state, stderr, "lrt_thread_close(): before"));
	_find_thread_container(global_state);
	sprintf(buf, "%p", L);
	lua_pushnil(global_state);
	lua_setfield(global_state, -2, buf);
	lua_pop(global_state, 1);
	DEBUG(lrt_stackDump(global_state, stderr, "lrt_thread_close(): after"));
	lrt_unlock();

	DEBUG(fprintf(stderr, "lrt_thread_close(%p): end\n", L));
}

/* init thread state system */
int lrt_init(lua_State *L) {
	lrt_lock();
	
	// keep pointer to global state
	global_state = L;
	
	// create a registry table for thread data
	lua_pushstring(L, REGISTRY_KEY); 
	lua_newtable(L);
	lua_settable(L, LUA_REGISTRYINDEX);

	DEBUG(lrt_stackDump(L, stderr, "lrt_init()"));
	
	lrt_unlock();
	
	return 0;
}
