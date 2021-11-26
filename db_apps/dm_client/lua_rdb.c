/*
 * NetComm OMA-DM Client
 *
 * lua_rdb.c
 * Lua wrapper library for RDB.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>
#include <logger.h>

#include <rdb_ops.h>

#define MODULE_NAME "rdb"
#define SESSION_NAME  ("__" MODULE_NAME)

/* Userdata to represent an RDB subscription. */
typedef struct {
    int watchRef;
    int watchCount;
} sub_t;

/* Userdata to represent an RDB session. */
typedef struct {
    struct rdb_session* session;
    int                 subsRef;
} rdb_t;

/* Open a new RDB session and create a userdata object
 * to represent it, which is stored in the registry. */
static rdb_t* create_rdb(lua_State* L)
{
    log_debug("Creating RDB session.");

    rdb_t* rdb = lua_newuserdata(L, sizeof(rdb_t));
    int rc = rdb_open(NULL, &rdb->session);
    if (rc) {
        luaL_error(L, "rdb_open() failed: %s.", strerror(-rc));
    }

    lua_newtable(L);
    rdb->subsRef = luaR_ref(L);

    luaL_getmetatable(L, MODULE_NAME);
    lua_setmetatable(L, -2);
    luaR_setfield(L, SESSION_NAME);
    return rdb;
}

/* Return the RDB object associated with this Lua machine, creating one if it doesn't exist. */
static rdb_t* require_rdb(lua_State* L)
{
    luaR_getfield(L, SESSION_NAME);
    rdb_t* rdb = lua_touserdata(L, -1);
    if (!rdb) {
        rdb = create_rdb(L);
    }

    lua_pop(L, 1);
    return rdb;
}

/* Subscribe to an RDB key and create a userdata object to represent that subscription. */
static sub_t* create_sub(lua_State* L, rdb_t* rdb, const char* key)
{
    log_trace("Creating subscription for '%s'.", key);

    int rc = rdb_subscribe(rdb->session, key);
    if (rc && rc != -ENOENT) {
        luaL_error(L, "rdb_subscribe failed('%s'): %s.", key, strerror(-rc));
    }

    luaR_getref(L, rdb->subsRef);
    sub_t* sub = lua_newuserdata(L, sizeof(sub_t));

    lua_newtable(L);
    sub->watchRef = luaR_ref(L);
    sub->watchCount = 0;

    lua_setfield(L, -2, key);
    lua_pop(L, 1);
    return sub;
}

/* Unsubscribe from an RDB key and delete the associated subscription object. */
static void delete_sub(lua_State* L, rdb_t* rdb, sub_t* sub, const char* key)
{
    log_trace("Deleting subscription for '%s'.", key);

    int rc = rdb_unsubscribe(rdb->session, key);
    if (rc) {
        luaL_error(L, "rdb_unsubscribe failed('%s'): %s.", key, strerror(-rc));
    }

    luaR_unref(L, sub->watchRef);
    luaR_getref(L, rdb->subsRef);
    lua_pushnil(L);
    lua_setfield(L, -2, key);
    lua_pop(L, 1);
}

/* Return the subscription object for an RDB key. */
static sub_t* get_sub(lua_State* L, rdb_t* rdb, const char* key)
{
    luaR_getref(L, rdb->subsRef);
    lua_getfield(L, -1, key);
    sub_t* sub = luaL_popuserdata(L, 1);
    return sub;
}

/* Return the subscription object for an RDB key, creating it if it doesn't exist. */
static sub_t* require_sub(lua_State* L, rdb_t* rdb, const char* key)
{
    sub_t* sub = get_sub(L, rdb, key);
    if (!sub) {
        sub = create_sub(L, rdb, key);
    }
    return sub;
}

/* Dispatch notification to all watchers of an RDB key. */
static void notify_one(lua_State* L, rdb_t* rdb, sub_t* sub, const char* key)
{
    /* Get updated variable contents. */
    int len = 0;
    char* buf = NULL;
    int rc = rdb_get_alloc(rdb->session, key, &buf, &len);
    if (rc && rc != -ENOENT) {
        luaL_error(L, "rdb_get_alloc('%s') failed: %s.", key, strerror(-rc));
    }

    /* Deliver notification to all associated watchers. */
    if (sub->watchCount > 0) {
        log_trace("Reporting change to '%s'.", key);
        if (rc == -ENOENT) {
            lua_pushnil(L);
        } else {
            if (len > 0) {
                len--;
            }
            lua_pushlstring(L, buf, len);
        }
        luaR_getref(L, sub->watchRef);
        for (lua_pushnil(L); lua_next(L, -2);) {
            lua_pushstring(L, key);
            lua_pushvalue(L, -5);
            if (lua_pcall(L, 2, 0, 0)) {
                log_error("Error during '%s' watcher: %s", key, lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 2);
    }

    if (!rc) {
        free(buf);
    }
}

/* Dispatch pending notifications for all subscribed RDB keys. */
static int notify_all(lua_State* L)
{
    rdb_t* rdb = require_rdb(L);

    /* Get list of triggered variables. */
    int len = 0;
    char* buf = NULL;
    int rc = rdb_getnames_alloc(rdb->session, "", &buf, &len, TRIGGERED);
    if (rc && rc != -ENOENT) {
        luaL_error(L, "rdb_getnames_alloc() failed: %s.", strerror(-rc));
    }

    /* Deliver notifications for each variable. */
    if (!rc) {
        char* tmp = NULL;
        char* key = strtok_r(buf, RDB_GETNAMES_DELIMITER_STR, &tmp);
        for (; key; key = strtok_r(NULL, RDB_GETNAMES_DELIMITER_STR, &tmp)) {
            sub_t* sub = get_sub(L, rdb, key);
            if (sub) {
                notify_one(L, rdb, sub, key);
            }
        }
        free(buf);
    }
    return 0;
}

/* Get an RDB value and push it onto the stack. */
static void get_and_push(lua_State* L, const char* key)
{
    rdb_t* rdb = require_rdb(L);

    int len = 0;
    char* buf = NULL;
    int rc = rdb_get_alloc(rdb->session, key, &buf, &len);
    if (rc && rc != -ENOENT) {
        luaL_error(L, "rdb_get_alloc('%s') failed: %s.", key, strerror(-rc));
    }

    if (rc == -ENOENT) {
        lua_pushnil(L);
    } else {
        if (len > 0) {
            len--;
        }
        lua_pushlstring(L, buf, len);
        free(buf);
    }
}

/* rdb.Watch(key, callback, [immediate])
 *
 * Install a watcher callback for the specified RDB variable that will
 * receive notifications when the key is created/modified/deleted.
 * Returns a token that can be used to remove the callback later.
 */
static int __watch(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    log_trace("Adding watcher for '%s'.", key);

    rdb_t* rdb = require_rdb(L);
    sub_t* sub = require_sub(L, rdb, key);

    /* Store watcher. */
    luaR_getref(L, sub->watchRef);
    lua_pushvalue(L, 2);
    int token = luaL_ref(L, -2);
    lua_pop(L, 1);

    sub->watchCount++;

    /* Run the callback immediately with the current value if requested. */
    if (lua_toboolean(L, 3)) {
        lua_settop(L, 2);
        lua_pushstring(L, key);
        get_and_push(L, key);
        if (lua_pcall(L, 2, 0, 0)) {
            log_error("Error during '%s' watcher: %s", key, lua_tostring(L, -1));
        }
    }

    /* The caller can use this token to remove the watcher later. */
    lua_pushinteger(L, token);
    return 1;
}

/* rdb.WatchTrigger(key, callback, [immediate])
 *
 * Alternate version of rdb.Watch that only executes the callback
 * if the watched RDB variable contains a non-empty string, and
 * automatically erases the contents afterwards.
 */
static int __watchTrigger_callback(lua_State* L)
{
    if (lua_isstring(L, 2) && lua_objlen(L, 2) > 0)
    {
        const char* key = lua_tostring(L, 1);
        rdb_t* rdb = require_rdb(L);
        int rc = rdb_update(rdb->session, key, "", 1, 0, DEFAULT_PERM);
        if (rc) {
            luaL_error(L, "rdb_update('%s') failed: %s.", key, strerror(-rc));
        }
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_call(L, 2, 0);
    }
    return 0;
}
static int __watchTrigger(lua_State* L)
{

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    lua_pushcclosure(L, __watchTrigger_callback, 1);
    lua_replace(L, 2);

    return __watch(L);
}

/* rdb.Unwatch(key, token)
 *
 * Remove a previously installed watcher callback from the specified RDB variable.
 * If the number of watchers for a key drops to zero then the RDB subscription for
 * that key will also be deleted.
 */
static int __unwatch(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    int token = luaL_checkinteger(L, 2);

    log_trace("Removing watcher for '%s'.", key);

    /* Find subscription record. */
    rdb_t* rdb = require_rdb(L);
    sub_t* sub = get_sub(L, rdb, key);
    if (!sub) {
        return luaL_error(L, "no subscription exists for '%s'", key);
    }

    /* Find watcher associated with token. */
    luaR_getref(L, sub->watchRef);
    lua_rawgeti(L, -1, token);
    if (lua_isnil(L, -1)) {
        return luaL_error(L, "no watcher with token %d for '%s'", token, key);
    }

    /* Delete watcher. */
    lua_pushnil(L);
    lua_rawseti(L, -3, token);
    lua_pop(L, 2);

    sub->watchCount--;
    if (sub->watchCount == 0) {
        delete_sub(L, rdb, sub, key);
    }
    return 0;
}

/* rdb.Get(key)
 *
 * Return the value of the specified RDB variable or nil if it does not exist.
 */
static int __get(lua_State* L)
{
    get_and_push(L, luaL_checkstring(L, 1));
    return 1;
}

/* rdb.GetNumber(key, [default])
 *
 * Return the value of the specified RDB variable converted to a number,
 * or the value of 'default' if the conversion fails.
 */
static int __getNumber(lua_State* L)
{
    get_and_push(L, luaL_checkstring(L, 1));
    if (lua_isnumber(L, -1)) {
        lua_pushnumber(L, lua_tonumber(L, -1));
    } else {
        lua_settop(L, 2);
    }
    return 1;
}

/* rdb.Set(key, value, [flags])
 *
 * Set the value of the specified RDB variable, creating it if necessary.
 */
static int __set(lua_State* L)
{
    size_t len;
    const char* key = luaL_checkstring(L, 1);
    const char* val = luaL_checklstring(L, 2, &len);
    int flags = luaL_optinteger(L, 3, 0);
    rdb_t* rdb = require_rdb(L);

    int rc = rdb_update(rdb->session, key, val, len + 1, flags, DEFAULT_PERM);
    if (rc) {
        luaL_error(L, "rdb_update('%s') failed: %s.", key, strerror(-rc));
    }
    return 0;
}

/* rdb.Unset(key)
 *
 * Delete the specified RDB variable.
 */
static int __unset(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    rdb_t* rdb = require_rdb(L);

    int rc = rdb_delete(rdb->session, key);
    if (rc) {
        luaL_error(L, "rdb_delete('%s') failed: %s.", key, strerror(-rc));
    }
    return 0;
}

/* rdb.GetFlags(key)
 *
 * Get the flags of an existing RDB variable.
 */
static int __getFlags(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    rdb_t* rdb = require_rdb(L);

    int flags;
    int rc = rdb_getflags(rdb->session, key, &flags);
    if (rc) {
        luaL_error(L, "rdb_getflags('%s') failed: %s.", key, strerror(-rc));
    }

    lua_pushinteger(L, flags);
    return 1;
}

/* rdb.SetFlags(key, flags)
 *
 * Set the flags of an existing RDB variable.
 */
static int __setFlags(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    int flags = luaL_checkinteger(L, 2);
    rdb_t* rdb = require_rdb(L);

    int rc = rdb_setflags(rdb->session, key, flags);
    if (rc) {
        luaL_error(L, "rdb_setflags('%s') failed: %s.", key, strerror(-rc));
    }

    return 0;
}

/* rdb.Find(prefix)
 *
 * Returns a list of all RDB variables starting with the
 * specified prefix. Returns all variables if no prefix
 * is specified.
 */
static int __find(lua_State* L)
{
    const char* prefix = luaL_optstring(L, 1, "");
    rdb_t* rdb = require_rdb(L);

    int len = 0;
    char* buf = NULL;
    int rc = rdb_getnames_alloc(rdb->session, prefix, &buf, &len, 0);
    if (rc && rc != -ENOENT) {
        luaL_error(L, "rdb_getnames_alloc() failed: %s.", strerror(-rc));
    }

    lua_newtable(L);
    if (!rc) {
        int idx = 1;
        char* tmp = NULL;
        char* key = strtok_r(buf, RDB_GETNAMES_DELIMITER_STR, &tmp);
        for (; key; key = strtok_r(NULL, RDB_GETNAMES_DELIMITER_STR, &tmp)) {
            lua_pushstring(L, key);
            lua_rawseti(L, -2, idx++);
        }
        free(buf);
    }
    return 1;
}

/* rdb.Keys(prefix)
 *
 * Returns a function that will iterate over all RDB
 * variables starting with the specified prefix, or
 * all variables if no prefix is specified.
 */
static int __keys_iterator(lua_State* L)
{
    lua_pushvalue(L, lua_upvalueindex(2));
    if (lua_next(L, lua_upvalueindex(1))) {
        lua_insert(L, -2);
        lua_replace(L, lua_upvalueindex(2));
    } else {
        lua_pushnil(L);
    }
    return 1;
}
static int __keys(lua_State* L)
{
    __find(L);

    lua_pushnil(L);
    lua_pushcclosure(L, __keys_iterator, 2);
    return 1;
}

/* rdb.Pairs(prefix)
 *
 * Similar to rdb.Keys, but the function returned
 * here will produce both variable names and values
 * on each iteration.
 */
static int __pairs_iterator(lua_State* L)
{
    lua_pushvalue(L, lua_upvalueindex(2));
    if (lua_next(L, lua_upvalueindex(1))) {
        lua_insert(L, -2);
        lua_replace(L, lua_upvalueindex(2));
        get_and_push(L, lua_tostring(L, -1));
        return 2;
    }
    lua_pushnil(L);
    return 1;
}
static int __pairs(lua_State* L)
{
    __find(L);

    lua_pushnil(L);
    lua_pushcclosure(L, __pairs_iterator, 2);
    return 1;
}

/* rdb.Atomic(callback, ...)
 *
 * Executes the supplied callback with RDB locked, catching
 * any Lua errors and unlocking RDB before rethrowing them.
 * Any additional parameters are passed to the callback, and
 * any values returned by the callback will be forwarded to
 * the calling function.
 */
static int __atomic(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    rdb_t* rdb = require_rdb(L);

    int rc = rdb_lock(rdb->session, 0);
    if (rc) {
        luaL_error(L, "rdb_lock() failed: %s.", strerror(-rc));
    }
    int thrown = lua_pcall(L, lua_gettop(L) - 1, LUA_MULTRET, 0);
    rc = rdb_unlock(rdb->session);
    if (rc) {
        luaL_error(L, "rdb_unlock() failed: %s.", strerror(-rc));
    }
    if (thrown) {
        lua_error(L);
    }
    return lua_gettop(L);
}

/* GC metamethod that will be called to clean up and rdb_t objects. */
static int __gc(lua_State* L)
{
    rdb_t* rdb = luaL_checkudata(L, 1, MODULE_NAME);
    rdb_close(&rdb->session);
    return 0;
}

/* Load library into Lua environment. */
void luaopen_rdb(lua_State* L)
{
    /* Flags and other constants. */
    static const luaL_Enum enums[] = {
        {"PERSIST",    PERSIST},
        {"CRYPT",      CRYPT},
        {"HASH",       HASH},
        {"FIFO",       FIFO},
        {"STATISTICS", STATISTICS},
        {"FLAG400",    FLAG400},
        {"FLAG800",    FLAG800},
        {"CUSTOM",     CUSTOM},
        {"LARGE",      LARGE},
        {"BINARY",     BINARY},
        {NULL, 0}
    };

    /* Library functions. */
    static const luaL_Reg funcs[] = {
        {"Watch",        __watch},
        {"WatchTrigger", __watchTrigger},
        {"Unwatch",      __unwatch},
        {"Get",          __get},
        {"GetNumber",    __getNumber},
        {"Set",          __set},
        {"Unset",        __unset},
        {"GetFlags",     __getFlags},
        {"SetFlags",     __setFlags},
        {"Find",         __find},
        {"Keys",         __keys},
        {"Pairs",        __pairs},
        {"Atomic",       __atomic},
        {NULL, NULL}
    };

    /* Create a metatable so we can attach a GC metamethod to clean up RDB sessions. */
    luaL_newmetatable(L, MODULE_NAME);
    lua_pushcfunction(L, __gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    /* Create library and install all functions in it. */
    luaL_register(L, MODULE_NAME, funcs);
    luaL_setenums(L, enums);

    lua_pop(L, 1);
}

/* Return the RDB file handle for the given Lua machine. */
int lua_rdb_getfd(lua_State* L)
{
    rdb_t* rdb = require_rdb(L);
    if (!rdb) {
        return -1;
    }
    return rdb_fd(rdb->session);
}

/* Process notifications for any subscribed RDB variables.
 * Returns true if no errors occured, otherwise false with
 * the error message at the top of the stack. */
int lua_rdb_notify(lua_State* L)
{
    lua_pushcfunction(L, notify_all);
    if (lua_pcall(L, 0, 0, 0)) {
        return false;
    }
    return true;
}
