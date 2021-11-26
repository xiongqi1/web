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
#include "luaTransfer.h"

static int _call_void_function(char *from, char *name) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "%s() begin\n", from);)

	li_dimclient_find_function(L, DIMCLIENT_TRANSFER, name);
	ret = lua_pcall(L, 0, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "%s(): %s function threw an error: %s\n", from, name, msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else {
		ret = lua_tointeger(L, -1);
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "%s() end %d\n", from, ret);)

	return ret;
}

static int _call_twostr_function(char *from, char *name, char *arg1, char *arg2) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "%s(%s, %s) begin\n", from, arg1, arg2);)

	li_dimclient_find_function(L, DIMCLIENT_TRANSFER, name);
	lua_pushstring(L, arg1);
	lua_pushstring(L, arg2);
	ret = lua_pcall(L, 2, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "%s(%s, %s): %s function threw an error: %s\n", from, arg1, arg2, name, msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else {
		ret = lua_tointeger(L, -1);
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "%s(%s, %s) end %d\n", from, arg1, arg2, ret);)

	return ret;
}

static int _call_onestr_function(char *from, char *name, char *arg1) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "%s(%s) begin\n", from, arg1);)

	li_dimclient_find_function(L, DIMCLIENT_TRANSFER, name);
	lua_pushstring(L, arg1);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "%s(%s): %s function threw an error: %s\n", from, arg1, name, msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else {
		ret = lua_tointeger(L, -1);
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "%s(%s) end %d\n", from, arg1, ret);)

	return ret;
}


int li_transfer_deleteAll() {
	return _call_void_function("li_transfer_deleteAll", "deleteAll");
}

int li_transfer_delete(char *name) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_transfer_delete(%s) begin\n", name);)

	li_dimclient_find_function(L, DIMCLIENT_TRANSFER, "delete");
	lua_pushstring(L, name);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_transfer_delete(%s): function threw an error: %s\n", name, msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else {
		ret = lua_tointeger(L, -1);
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_transfer_delete(%s) end %d\n", name, ret);)
	return ret;
}


int li_transfer_add(char *name, char *value) {
	return _call_twostr_function("li_transfer_add", "add", name, value);
}

int li_transfer_getAll(int (*callback)(char *name, char *value)) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_transfer_getAll(%p) begin\n", callback);)

	li_dimclient_find_function(L, DIMCLIENT_TRANSFER, "getAll");
	ret = lua_pcall(L, 0, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_transfer_getAll(): function threw an error: %s\n", msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else if(lua_istable(L, -1)) {
		char *name, *value;
		lua_pushnil(L);
		while(lua_next(L, -2)) {
			if(!lua_isstring(L, -1)) {
				DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_transfer_getAll(): table member is not a string?\n");)
				li_stackDump(L);
				ret = ERR_INTERNAL_ERROR;
			} else if(!lua_isstring(L, -2)) {
				DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_transfer_getAll(): table key is not a string?\n");)
				li_stackDump(L);
				ret = ERR_INTERNAL_ERROR;
			} else {
				name = (char *)luaL_checkstring(L, -2);
				value = (char *)luaL_checkstring(L, -1);
				name = strdup(name);
				value = strdup(value);
				DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "li_transfer_getAll(): found transfer '%s': %s\n", name, value);)
				ret = callback(name, value);
				if(ret) {
					DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_transfer_getAll(): transfer '%s' addition failed: %d.\n", name, ret);)
				} else {
					DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "li_transfer_getAll(): transfer '%s' addition OK.\n", name);)
				}
				free(name);
				free(value);
			}
			lua_pop(L, 1);
		}
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_transfer_getAll(): function did not return a table?\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	
	li_thread_close(L);
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_transfer_getAll() end %d\n", ret);)
	return ret;
}

int li_transfer_download_before(const char *name, const char *value) {
	return _call_twostr_function("li_transfer_download_before", "downloadBefore", name, value);
}

int li_transfer_download_after(const char *name, const char *value) {
	return _call_twostr_function("li_transfer_download_after", "downloadAfter", name, value);
}

int li_transfer_upload_before(const char* targetFileName, const char* targetType ) {
	return _call_twostr_function("li_transfer_upload_before", "uploadBefore", targetFileName, targetType);
}

int li_transfer_upload_after(const char* targetFileName, const char* targetType ) {
	return _call_twostr_function("li_transfer_upload_after", "uploadAfter", targetFileName, targetType);
}

#if defined(PLATFORM_PLATYPUS)
int li_transfer_get_configfilename(char **value, size_t *len) {
	int ret;
	const char *str;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_transfer_get_configfilename(): begin\n");)

	li_dimclient_find_function(L, DIMCLIENT_TRANSFER, "getConfigfileName");
	ret = lua_pcall(L, 0, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_transfer_get_configfilename(): get function threw an error: %s\n", msg);)
		li_stackDump(L);
	} else if(lua_isstring(L, -1)) {
		str = luaL_checklstring(L, -1, len);
		*value = strndup(str, *len);
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
		*value = "";
		*len = 0;
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_transfer_get_configfilename(): get function returned a non-string.\n");)
		li_stackDump(L);
		*value = "";
		*len = 0;
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_transfer_get_configfilename(): end %d\n", ret);)

	return ret;
}
#endif