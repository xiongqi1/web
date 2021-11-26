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
#include "parameter.h"
#include "callback.h"
#include "eventcode.h"
#include "debug.h"

extern unsigned int sendActiveNofi;

static void _find_dimclient_global(lua_State *L) {
	lua_getglobal(L, DIMCLIENT_GLOBAL);
	if(!lua_istable(L, -1)) {
		li_die(L, "Can't find dimclient global table.\n");
	}
}

static void _find_dimclient_container(lua_State *L, char *childName) {
	_find_dimclient_global(L);
	lua_getfield(L, -1, childName);
	lua_remove(L, -2);
	if(!lua_istable(L, -1)) {
		li_die(L, "Can't find dimclient.%s container table.\n", childName);
	}
}

void li_dimclient_find_function(lua_State *L, char *containerName, char *functionName) {
	_find_dimclient_container(L, containerName);
	lua_getfield(L, -1, functionName);
	lua_remove(L, -2);
	if(!lua_isfunction(L, -1)) {
		li_die(L, "Can't find dimclient.%s.%s function.\n", containerName, functionName);
	}
}

static int dimclient_lock(lua_State *L) {
	int argc = lua_gettop(L);
	if(argc != 0) return luaL_error(L, "lock expects no arguments.");
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_lock()\n");)
	li_lock();
	return 0;
}

static int dimclient_unlock(lua_State *L) {
	int argc = lua_gettop(L);
	if(argc != 0) return luaL_error(L, "unlock expects no arguments.");
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_unlock()\n");)
	li_unlock();
	return 0;
}

static int dimclient_log(lua_State *L) {
#ifdef _DEBUG
	int severity;
	int argc = lua_gettop(L);
	const char *level = luaL_checkstring(L, 1);
	const char *msg = luaL_checkstring(L, 2);
	if(argc != 2) return luaL_error(L, "log expects exactly 2 arguments.");
	if(!strcmp("debug", level)) {
		severity = SVR_DEBUG;
	} else if(!strcmp("info", level)) {
		severity = SVR_INFO;
	} else if(!strcmp("warn", level)) {
		severity = SVR_WARN;
	} else if(!strcmp("error", level)) {
		severity = SVR_ERROR;
	} else {
		return luaL_error(L, "invalid log type '%s' (debug, info, warn, error)", level);
	}
	DEBUG_OUTPUT(dbglog(severity, DBG_LUA, "log(): %s\n", msg);)
#endif
	return 0;
}

static int dimclient_triggerNotification(lua_State *L) {
	int argc = lua_gettop(L);
	if(argc != 0) return luaL_error(L, "triggerNotification expects no arguments.");
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_triggerNotification()\n");)
	requestInform();
	lua_pushnumber(L, 0);
	return 1;
}

static int dimclient_setParameter(lua_State *L) {
	int ret, notify, argc = lua_gettop(L);
	const char *name = luaL_checkstring(L, 1);
	const char *value = luaL_checkstring(L, 2);
	if(argc != 2) return luaL_error(L, "setParameter expects exactly two arguments.");
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_setParameter(%s, '%s')\n", name, value);)
	ret = setParameter2Host(name, (bool *) &notify, (char *) value);
	sendActiveNofi =true;
	lua_pushnumber(L, ret);
	lua_pushnumber(L, notify);
	return 2;
}

static int dimclient_addObject(lua_State *L) {
	int ret, instance, argc = lua_gettop(L);
	int suggestedInstance=-1;
	const char *name = luaL_checkstring(L, 1);
	if(argc < 1 && argc > 2) return luaL_error(L, "addObject expects one or two argument.");
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_addObject(%s)\n", name);)
	if ( argc ==2)
	{
		suggestedInstance = luaL_checkint(L,2);

	}
	ret = addObjectIntern(name, &instance, suggestedInstance);
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_addObject(%s) ret = %d, instance = %d\n", name, ret, instance);)
	lua_pushnumber(L, ret);
	lua_pushnumber(L, instance);
	return 2;
}

static int dimclient_delObject(lua_State *L) {
	int ret, status, argc = lua_gettop(L);
	const char *name = luaL_checkstring(L, 1);
	if(argc != 1) return luaL_error(L, "delObject expects exactly one argument.");
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_delObject(%s)\n", name);)
	ret = deleteObject(name, &status);
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_delObject(%s) ret = %d, status = %d\n", name, ret, status);)
	lua_pushnumber(L, ret);
	lua_pushnumber(L, status);
	return 2;
}

static int dimclient_addEventSingle(lua_State *L) {
	int ret;
	const char *eventCode = luaL_checkstring(L, 1);
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_addEventSingle(%s)\n", eventCode);)
	ret = addEventCodeSingle(eventCode);
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_addEventSingle(%s) ret = %d\n", eventCode, ret);)
	lua_pushnumber(L, ret);
	return 1;
}

static int dimclient_asyncInform(lua_State *L) {
	DEBUG_OUTPUT(dbglog(SVR_INFO, DBG_LUA, "dimclient_asyncInform()\n");)
	setAsyncInform(true);
	return 0;
}


static int dimclient_callback(char *handler) {
	int ret;
	lua_State *L = li_thread_new();

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_callback(%s) begin\n", handler);)

	li_dimclient_find_function(L, DIMCLIENT_CALLBACKS, handler);
	ret = lua_pcall(L, 0, 0, 0);
	if(ret) {
		const char *msg = lua_tostring(L, -1);
		DEBUG_OUTPUT(dbglog(SVR_ERROR, DBG_LUA, "li_callback(%s): callback function threw an error: %s\n", handler, msg);)
		li_stackDump(L);
	}

	li_thread_close(L);

	DEBUG_OUTPUT(dbglog(SVR_DEBUG, DBG_LUA, "li_callback(%s) end\n", handler);)

	return CALLBACK_REPEAT;
}

static int dimclient_callback_init()	{ return dimclient_callback("initDone"); }
static int dimclient_callback_pre()	{ return dimclient_callback("preSession"); }
static int dimclient_callback_post()	{ return dimclient_callback("postSession"); }
static int dimclient_callback_cleanup()	{ return dimclient_callback("cleanup"); }

int li_dimclient_init(lua_State *L) {
	lua_newtable(L); // _G.dimclient - eventually

	// thread container
	lua_newtable(L);
	lua_setfield(L, -2, DIMCLIENT_THREADS);

	// lock & unlock
	lua_pushcfunction(L, dimclient_lock);
	lua_setfield(L, -2, "lock");
	lua_pushcfunction(L, dimclient_unlock);
	lua_setfield(L, -2, "unlock");

	// log
	lua_pushcfunction(L, dimclient_log);
	lua_setfield(L, -2, "log");

	// calls into dimclient
	lua_pushcfunction(L, dimclient_triggerNotification);
	lua_setfield(L, -2, "triggerNotification");
	lua_pushcfunction(L, dimclient_setParameter);
	lua_setfield(L, -2, "setParameter");
	lua_pushcfunction(L, dimclient_addObject);
	lua_setfield(L, -2, "addObject");
	lua_pushcfunction(L, dimclient_delObject);
	lua_setfield(L, -2, "delObject");
	lua_pushcfunction(L, dimclient_addEventSingle);
	lua_setfield(L, -2, "addEventSingle");
	lua_pushcfunction(L, dimclient_asyncInform);
	lua_setfield(L, -2, "asyncInform");

	// register callbacks
	addCallback(dimclient_callback_init, initParametersDoneCbList);
	addCallback(dimclient_callback_pre, preSessionCbList);
	addCallback(dimclient_callback_post, postSessionCbList);
	addCallback(dimclient_callback_cleanup, cleanUpCbList);

	lua_setglobal(L, DIMCLIENT_GLOBAL);

	return 0;
}
