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
#include "luaParameter.h"
#include "luaDimclient.h"
#include "parameter.h"
#include "globals.h"
#include "debug.h"

int li_param_meta_store(const char *name, const char *meta) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_meta_store(%s, %s): begin\n", name, meta);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "setMeta");
	lua_pushstring(L, name);
	lua_pushstring(L, meta);
	ret = lua_pcall(L, 2, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_meta_store(): setMeta function threw an error: %s\n", msg);)
		li_stackDump(L);
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_meta_store(): setMeta function returned a non-integer.\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_meta_store(%s): end %d\n", name, ret);)

	return ret;
}

int li_param_meta_retrieve(const char *name, char **meta, size_t *len) {
	int ret;
	const char *str;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_meta_retrieve(%s): begin\n", name);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "getMeta");
	lua_pushstring(L, name);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_meta_retrieve(): getMeta function threw an error: %s\n", msg);)
		li_stackDump(L);
	} else if(lua_isstring(L, -1)) {
		str = luaL_checklstring(L, -1, len);
		*meta = strndup(str, *len);
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
		*meta = "";
		*len = 0;
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_meta_retrieve(): getMeta function returned a non-string.\n");)
		li_stackDump(L);
		*meta = "";
		*len = 0;
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_meta_retrieve(%s): end %d\n", name, ret);)

	return ret;
}

int li_param_meta_delete(const char *name) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_meta_delete(%s): begin\n", name);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "unsetMeta");
	lua_pushstring(L, name);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_meta_delete(): unsetMeta function threw an error: %s\n", msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_meta_delete(): unsetMeta function returned a non-integer.\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_meta_delete(%s): end %d\n", name, ret);)

	return ret;
}

int li_param_value_init(const char *name, const char *value) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_value_init(%s, %s): begin\n", name, value);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "init");
	lua_pushstring(L, name);
	lua_pushstring(L, value);
	ret = lua_pcall(L, 2, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_value_init(): init function threw an error: %s\n", msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_value_init(): init function returned a non-integer.\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_value_init(%s): end %d\n", name, ret);)

	return ret;
}

int li_param_value_store(const char *name, const char *value) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_value_store(%s, %s): begin\n", name, value);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "set");
	lua_pushstring(L, name);
	lua_pushstring(L, value);
	ret = lua_pcall(L, 2, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_value_store(): set function threw an error: %s\n", msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_value_store(): set function returned a non-integer.\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_value_store(%s): end %d\n", name, ret);)

	return ret;
}

int li_param_value_retrieve(const char *name, char **value, size_t *len) {
	int ret;
	const char *str;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_value_retrieve(%s): begin\n", name);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "get");
	lua_pushstring(L, name);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_value_retrieve(): get function threw an error: %s\n", msg);)
		li_stackDump(L);
	} else if(lua_isstring(L, -1)) {
		str = luaL_checklstring(L, -1, len);
		*value = strndup(str, *len);
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
		*value = "";
		*len = 0;
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_value_retrieve(): get function returned a non-string.\n");)
		li_stackDump(L);
		*value = "";
		*len = 0;
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_value_retrieve(%s): end %d\n", name, ret);)

	return ret;
}

int li_param_value_delete(const char *name) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_value_delete(%s): begin\n", name);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "unset");
	lua_pushstring(L, name);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_value_delete(): unset function threw an error: %s\n", msg);)
		li_stackDump(L);
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_value_delete(): unset function returned a non-integer.\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);
	
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_value_delete(%s): end %d\n", name, ret);)

	return ret;
}

int li_param_object_create(const char *name, int instanceId) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_object_create(%s, %d): begin\n", name, instanceId);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "create");
	lua_pushstring(L, name);
	lua_pushinteger(L, instanceId);
	ret = lua_pcall(L, 2, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_object_create(): create function threw an error: %s\n", msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_object_create(): create function returned a non-integer.\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);
	
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_object_create(%s, %d): end %d\n", name, instanceId, ret);)

	return ret;
}

int li_param_object_delete(const char *name) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_object_delete(%s): begin\n", name);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "delete");
	lua_pushstring(L, name);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_object_delete(): delete function threw an error: %s\n", msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_object_delete(): delete function returned a non-integer.\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);
	
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_object_delete(%s): end %d\n", name, ret);)

	return ret;
}

int li_param_delete() {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_delete(): begin\n");)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "unsetAll");
	ret = lua_pcall(L, 0, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_delete(): unsetAll function threw an error: %s\n", msg);)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_param_delete(): unsetAll function returned a non-integer.\n");)
		li_stackDump(L);
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);
	
	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_param_delete(): end %d\n", ret);)

	return ret;
}


#ifdef PLATFORM_PLATYPUS
int li_nvram_value_retrieve(const char *name, char **value, size_t *len) {
	int ret;
	const char *str;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_nvram_value_retrieve(%s): begin\n", name);)

	li_dimclient_find_function(L, DIMCLIENT_PARAMETER, "getnvram");
	lua_pushstring(L, name);
	ret = lua_pcall(L, 1, 1, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_nvram_value_retrieve(): get function threw an error: %s\n", msg);)
		li_stackDump(L);
	} else if(lua_isstring(L, -1)) {
		str = luaL_checklstring(L, -1, len);
		*value = strndup(str, *len);
	} else if(lua_isnumber(L, -1)) {
		ret = lua_tointeger(L, -1);
		*value = "";
		*len = 0;
	} else {
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_nvram_value_retrieve(): get function returned a non-string.\n");)
		li_stackDump(L);
		*value = "";
		*len = 0;
		ret = ERR_INTERNAL_ERROR;
	}
	lua_pop(L, 1);
	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_nvram_value_retrieve(%s): end %d\n", name, ret);)

	return ret;
}
#endif
