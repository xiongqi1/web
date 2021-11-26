#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "globals.h"
#include "debug.h"

#include "luaCore.h"
#include "luaDimclient.h"
#include "luaEvent.h"

int _call_void_function(char *from, char *name) {
	int ret;
	lua_State *L = li_thread_new();

// 	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "%s() begin\n", from);)

	li_dimclient_find_function(L, DIMCLIENT_EVENT, name);
	ret = lua_pcall(L, 0, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "%s(): %s function threw an error: %s\n", from, name, msg);)
		li_stackDump(L);
	} else {
		ret = lua_tointeger(L, -1);
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
// 	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "%s() end %d\n", from, ret);)

	return ret;
}

int li_event_bootstrap_set() {
	return _call_void_function("li_event_bootstrap_set", "bootstrapSet");
}

int li_event_bootstrap_unset() {
	return _call_void_function("li_event_bootstrap_unset", "bootstrapUnset");
}

int  li_event_bootstrap_get() {
	return _call_void_function("li_event_bootstrap_get", "bootstrapGet");
}

int li_event_deleteAll() {
	return _call_void_function("li_event_deleteAll", "deleteAll");
}

int li_event_add(const char *event) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_event_add(%s) begin\n", event);)

	li_dimclient_find_function(L, DIMCLIENT_EVENT, "add");
	lua_pushstring(L, event);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_event_add(%s): function threw an error: %s\n", event, msg);)
		li_stackDump(L);
	} else {
		ret = lua_tointeger(L, -1);
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_event_add(%s) end %d\n", event, ret);)
	return ret;
}

int li_event_getAll(int (*callback)(const char *event)) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_event_getAll(%p) begin\n", callback);)

	li_dimclient_find_function(L, DIMCLIENT_EVENT, "getAll");
	ret = lua_pcall(L, 0, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_event_getAll(): function threw an error: %s\n", msg);)
		li_stackDump(L);
	} else if(lua_istable(L, -1)) {
		int i, len;
		char *event;
		len = lua_objlen(L, -1);
		for(i = 1; i <= len; i++) {
			lua_pushnumber(L, i);
			lua_gettable(L, -2);
			if(lua_isstring(L, -1)) {
				event = (char *)luaL_checkstring(L, -1);
				event = strdup(event); // we need to do this because the callback mangles the buffer and lua strings are singleton global.
				DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "li_event_getAll(): found event '%s'.\n", event);)
				ret = callback(event);
				if(ret) {
					DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_event_getAll(): event addition callback failed %d.\n", ret);)
				}
				free(event);
			} else {
				DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_event_getAll(): table member is not a string?\n");)
				li_stackDump(L);
			}
			lua_pop(L, 1);
		}
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_event_getAll(): function did not return a table?\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_event_getAll() end %d\n", ret);)
	return ret;
}

int li_event_informComplete(int result) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_event_informComplete() begin\n");)

	li_dimclient_find_function(L, DIMCLIENT_EVENT, "informComplete");
	lua_pushboolean(L, result);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_event_informComplete(): informComplete function threw an error: %s\n", msg);)
		li_stackDump(L);
	} else {
		ret = lua_tointeger(L, -1);
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_event_informComplete() end %d\n", ret);)

	return ret;
}

int li_event_reboot() {
	return _call_void_function("li_event_reboot", "reboot");
}

int li_event_factoryReset() {
	return _call_void_function("li_event_factoryReset", "factoryReset");
}

int li_event_tick() {
	return _call_void_function("li_event_tick", "tick");
}
