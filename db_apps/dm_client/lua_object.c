/*
 * NetComm OMA-DM Client
 *
 * lua_object.h
 * Wrapper interface to allow libdmclient management objects to
 * be defined and implemented from within Lua scripts.
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

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>

#include <omadmclient.h>
#include <omadmtree_mo.h>
#include <syncml_error.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>
#include <logger.h>

#include "lua_dm_path.h"
#include "lua_object.h"

/* Userdata struct for scripted object definitions. */
typedef struct {
    lua_State*            L;
    char*                 uri;
    char*                 urn;
    int                   ref;
} lua_object_t;

static int strdup_rc(char** dst, const char* src)
{
    *dst = NULL;
    int rc = OMADM_SYNCML_ERROR_DEVICE_FULL;
    if (src) {
        *dst = strdup(src);
        if (*dst)
            return 0;
    }
    return rc;
}

static int lua_strdup_rc(lua_State* L, char** dst, int index, bool allow_nil, size_t* dstLen)
{
    if (lua_isnil(L, index)) {
        *dst = NULL;
        if (dstLen) {
            *dstLen = 0;
        }
        return allow_nil ? 0 : OMADM_SYNCML_ERROR_COMMAND_FAILED;
    }
    size_t srcLen;
    const char* src = lua_tolstring(L, index, &srcLen);
    if (!src) {
        return OMADM_SYNCML_ERROR_COMMAND_FAILED;
    }
    *dst = malloc(srcLen + 1);
    if (!*dst) {
        return OMADM_SYNCML_ERROR_DEVICE_FULL;
    }
    memcpy(*dst, src, srcLen + 1);
    if (dstLen) {
        *dstLen = srcLen;
    }
    return 0;
}

/* Retrieve the specified handler function for the given object, then
 * push the state table for that object as the first parameter to the
 * handler function. Log and unwind in case of error. */
static int lua_handler_prepare(lua_object_t* obj, const char* name)
{
    lua_pushstring(obj->L, name);
    luaR_getref(obj->L, obj->ref);
    lua_getfield(obj->L, -1, name);
    if (!lua_isfunction(obj->L, -1)) {
        log_error("%s: No '%s' handler installed.", obj->uri, name);
        lua_pop(obj->L, 3);
        return OMADM_SYNCML_ERROR_SESSION_INTERNAL;
    }
    lua_insert(obj->L, -2);
    return 0;
}

/* Call into handler function, then pop, check and return first
 * return value, assuming it to be a status code. Log and unwind
 * in case of error. */
static int lua_handler_pcall(lua_object_t* obj, int narg, int nret)
{
    narg += 1;
    nret += 1;
    if (lua_pcall(obj->L, narg, nret, 0)) {
        log_error(
            "%s: Error during '%s' handler: %s",
            obj->uri,
            lua_tostring(obj->L, -2),
            lua_tostring(obj->L, -1)
        );
        lua_pop(obj->L, 2);
        return OMADM_SYNCML_ERROR_COMMAND_FAILED;
    }
    if (!lua_isnumber(obj->L, -nret)) {
        log_error(
            "%s: Handler for '%s' handler returned %s, expected a status code.",
            obj->uri,
            lua_tostring(obj->L, -(nret + 1)),
            lua_typename(obj->L, lua_type(obj->L, -nret))
        );
        lua_pop(obj->L, nret + 1);
        return OMADM_SYNCML_ERROR_COMMAND_FAILED;
    }
    int rc = lua_tointeger(obj->L, -nret);
    if (rc) {
        lua_pop(obj->L, nret + 1);
    } else {
        lua_remove(obj->L, -nret);
        lua_remove(obj->L, -nret);
    }
    return rc;
}

/* libdmclient callback; called when object is attached to a new session.
 * SessionStart() -> status:int */
static int lua_object_session_start(void** obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)*obj_ptr;
    log_trace("%s: session_start", obj->uri);

    int rc = lua_handler_prepare(obj, "SessionStart");
    if (!rc) {
        rc = lua_handler_pcall(obj, 0, 0);
    }

    log_trace("%s: session_start: rc=%i", obj->uri, rc);
    return rc;
}

/* libdmclient callback; called when session closes.
 * SessionEnd() -> status:int */
static void lua_object_session_end(void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: session_end", obj->uri);

    int rc = lua_handler_prepare(obj, "SessionEnd");
    if (!rc) {
        rc = lua_handler_pcall(obj, 0, 0);
    }

    log_trace("%s: session_end: rc=%i", obj->uri, rc);
}

/* libdmclient callback; return list of all URIs associated with a URN.
 * FindURN(urn:string) -> status:int, uris:table */
static int lua_object_find_urn(const char* urn, char*** uris, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: find_urn: urn=%s", obj->uri, urn);

    int count = 0;
    *uris = NULL;

    int rc = lua_handler_prepare(obj, "FindURN");
    if (!rc) {
        lua_pushstring(obj->L, urn);
        rc = lua_handler_pcall(obj, 1, 1);
        if (!rc) {
            count = lua_objlen(obj->L, -1);
            *uris = malloc(sizeof(char*) * (count + 1));
            if (*uris) {
                for (int i = 0; i < count; i++) {
                    lua_rawgeti(obj->L, -1, i + 1);
                    char* uri = strdup(lua_tostring(obj->L, -1));
                    lua_pop(obj->L, 1);
                    if (!uri) {
                        for (int j = 0; j < count; j++) {
                            free((*uris)[j]);
                        }
                        free(*uris);
                        *uris = NULL;
                        break;
                    }
                    (*uris)[i] = uri;
                }
                (*uris)[count] = NULL;
            }
            if (!*uris) {
                rc = OMADM_SYNCML_ERROR_DEVICE_FULL;
            }
            lua_pop(obj->L, 1);
        }
    }

    log_trace("%s: find_urn: rc=%i, count=%i", obj->uri, rc, count);
    return rc;
}

/* libdmclient callback; return the node type at a given URI.
 * GetType(uri:dmpath) -> status:int, type:int */
static int lua_object_get_type(const char* uri, omadmtree_node_kind_t* type, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: get_type: uri=%s", obj->uri, uri);

    *type = OMADM_NODE_NOT_EXIST;

    int rc = lua_handler_prepare(obj, "GetType");
    if (!rc) {
        lua_push_dm_path(obj->L, uri);
        rc = lua_handler_pcall(obj, 1, 1);
        if (!rc) {
            *type = lua_tointeger(obj->L, -1);
            lua_pop(obj->L, 1);
        }
    }

    log_trace("%s: get_type: rc=%i, type=%i", obj->uri, rc, (int)*type);
    return rc;
}

/* libdmclient callback; return the node contents at a given URI.
 * GetValue(uri:dmpath) -> status:int, format:string, type:string, value:string */
static int lua_object_get_value(dmtree_node_t* node, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: get_value: uri=%s", obj->uri, node->uri);

    int rc = lua_handler_prepare(obj, "GetValue");
    if (!rc) {
        lua_push_dm_path(obj->L, node->uri);
        rc = lua_handler_pcall(obj, 1, 3);
        if (!rc) {
            rc = lua_strdup_rc(obj->L, &node->format, -3, false, NULL);
            if (!rc) {
                rc = lua_strdup_rc(obj->L, &node->type, -2, true, NULL);
                if (rc) {
                    free(node->format);
                } else {
                    size_t len;
                    rc = lua_strdup_rc(obj->L, &node->data_buffer, -1, true, &len);
                    if (rc) {
                        free(node->format);
                        free(node->type);
                    } else {
                        node->data_size = len;
                    }
                }
            }
            lua_pop(obj->L, 3);
        }
    }

    log_trace("%s: get_value: rc=%i, fmt=%s, type=%s, len=%i", obj->uri, rc,
        !rc && node->format ? node->format    : "(null)",
        !rc && node->type   ? node->type      : "(null)",
        !rc                 ? node->data_size : 0
    );
    return rc;
}

/* libdmclient callback; create or replace the node contents at a given URI.
 * SetValue(uri:dmpath, format:string, type:string, value:string) -> status:int */
static int lua_object_set_value(const dmtree_node_t* node, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: set_value: uri=%s, fmt=%s, type=%s, len=%i",
        obj->uri, node->uri, node->format, node->type, node->data_size
    );

    int rc = lua_handler_prepare(obj, "SetValue");
    if (!rc) {
        lua_push_dm_path(obj->L, node->uri);
        lua_pushstring(obj->L, node->format);
        lua_pushstring(obj->L, node->type);
        lua_pushlstring(obj->L, node->data_buffer, node->data_size);
        rc = lua_handler_pcall(obj, 4, 0);
    }

    log_trace("%s: set_value: rc=%i", obj->uri, rc);
    return rc;
}

/* libdmclient callback; return the node permissions at a given URI.
 * GetACL(uri:dmpath) -> status:int, acl:string */
static int lua_object_get_acl(const char* uri, char** acl, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: get_acl: uri=%s", obj->uri, uri);

    int rc = lua_handler_prepare(obj, "GetACL");
    if (!rc) {
        lua_push_dm_path(obj->L, uri);
        rc = lua_handler_pcall(obj, 1, 1);
        if (!rc) {
            rc = lua_strdup_rc(obj->L, acl, -1, true, NULL);
            lua_pop(obj->L, 1);
        }
    }

    log_trace("%s: get_acl: rc=%i, acl=%s", obj->uri, rc, !rc && *acl ? *acl : "(null)");
    return rc;
}

/* libdmclient callback; set the node permissions at a given URI.
 * SetACL(uri:dmpath, acl:string) -> status:int */
static int lua_object_set_acl(const char* uri, const char* acl, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: set_acl: uri=%s, acl=%s", obj->uri, uri, acl);

    int rc = lua_handler_prepare(obj, "SetACL");
    if (!rc) {
        lua_push_dm_path(obj->L, uri);
        lua_pushstring(obj->L, acl);
        rc = lua_handler_pcall(obj, 2, 0);
    }

    log_trace("%s: set_acl: rc=%i", obj->uri, rc);
    return rc;
}

/* libdmclient callback; rename the node at a given URI.
 * Rename(from:dmpath, to:string) -> status:int */
static int lua_object_rename(const char* from, const char* to, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: rename: from=%s, to=%s", obj->uri, from, to);

    int rc = lua_handler_prepare(obj, "Rename");
    if (!rc) {
        lua_push_dm_path(obj->L, from);
        lua_pushstring(obj->L, to);
        rc = lua_handler_pcall(obj, 2, 0);
    }

    log_trace("%s: rename: rc=%i", obj->uri, rc);
    return rc;
}

/* libdmclient callback; delete the node at a given URI.
 * Delete(uri:dmpath) -> status:int */
static int lua_object_delete(const char* uri, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: delete: uri=%s", obj->uri, uri);

    int rc = lua_handler_prepare(obj, "Delete");
    if (!rc) {
        lua_push_dm_path(obj->L, uri);
        rc = lua_handler_pcall(obj, 1, 0);
    }

    log_trace("%s: delete: rc=%i", obj->uri, rc);
    return rc;
}

/* libdmclient callback; execute the node at a given URI.
 * Execute(uri:dmpath, arg:string, correlator:string) -> status:int */
static int lua_object_execute(const char* uri, const char* arg, const char* correlator, void* obj_ptr)
{
    lua_object_t* obj = (lua_object_t*)obj_ptr;
    log_trace("%s: execute: uri=%s, correlator=%s", obj->uri, uri, correlator);

    int rc = lua_handler_prepare(obj, "Execute");
    if (!rc) {
        lua_push_dm_path(obj->L, uri);
        lua_pushstring(obj->L, arg);
        lua_pushstring(obj->L, correlator);
        rc = lua_handler_pcall(obj, 3, 0);
    }

    log_trace("%s: execute: rc=%i", obj->uri, rc);
    return rc;
}

static int lua_register_object(lua_State* L)
{
    const char* uri = luaL_checkstring(L, 1);
    if (!lua_isnoneornil(L, 2)) {
        luaL_argcheck(L, lua_istable(L, 2), 2, "expected table or nil");
    }

    luaR_getfield(L, "__objects");
    lua_getfield(L, -1, uri);
    if (!lua_isnil(L, -1)) {
        lua_pop(L, 2);
        luaL_error(L, "An object already exists for '%s'.", uri);
    }
    lua_pop(L, 1);

    lua_object_t* obj = lua_newuserdata(L, sizeof(lua_object_t));
    luaL_getmetatable(L, "lua_object_t");
    lua_setmetatable(L, -2);

    obj->L = L;
    obj->uri = strdup(uri);

    if (!obj->uri) {
        lua_pop(L, 2);
        free(obj->uri);
        luaL_error(L, "Out of memory.");
    }

    lua_setfield(L, -2, uri);
    lua_pop(L, 1);

    if (!lua_isnoneornil(L, 2)) {
        lua_pushvalue(L, 2);
    } else {
        lua_newtable(L);
    }
    lua_pushvalue(L, -1);
    obj->ref = luaR_ref(L);

    return 1;
}

static int lua_object_gc(lua_State* L)
{
    lua_object_t* obj = luaL_checkudata(L, 1, "lua_object_t");
    log_trace("%s: gc", obj->uri);

    free(obj->uri);
    return 0;
}

void luaopen_object(lua_State* L)
{
    luaL_newmetatable(L, "lua_object_t");
    lua_pushcfunction(L, lua_object_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    lua_pushcfunction(L, lua_register_object);
    lua_setglobal(L, "RegisterObject");

    lua_newtable(L);
    luaR_setfield(L, "__objects");
}

bool lua_object_initialise_all(lua_State* L)
{
    luaR_getfield(L, "__objects");
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
    {
        lua_object_t* obj = (lua_object_t*)lua_touserdata(L, -1);
        log_trace("%s: initialise", obj->uri);

        int rc = lua_handler_prepare(obj, "Initialise");
        if (!rc) {
            rc = lua_handler_pcall(obj, 0, 0);
        }

        log_trace("%s: initialise: rc=%i", obj->uri, rc);
        if (rc) {
            lua_pop(L, 3);
            return false;
        }
    }
    lua_pop(L, 1);
    return true;
}

int lua_object_install_all(lua_State* L, dmclt_session* session)
{
    static const omadm_mo_interface_t __interface = {
        .initFunc    = lua_object_session_start,
        .closeFunc   = lua_object_session_end,
        .findURNFunc = lua_object_find_urn,
        .isNodeFunc  = lua_object_get_type,
        .getFunc     = lua_object_get_value,
        .setFunc     = lua_object_set_value,
        .getACLFunc  = lua_object_get_acl,
        .setACLFunc  = lua_object_set_acl,
        .renameFunc  = lua_object_rename,
        .deleteFunc  = lua_object_delete,
        .execFunc    = lua_object_execute
    };

    luaR_getfield(L, "__objects");
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
    {
        lua_object_t* obj = (lua_object_t*)lua_touserdata(L, -1);
        log_trace("%s: install", obj->uri);

        char *uri;
        int rc = strdup_rc(&uri, obj->uri);
        if (rc) {
            log_error("%s: Out of memory.", obj->uri);
        }
        else {
            omadm_mo_interface_t* interface = malloc(sizeof(omadm_mo_interface_t));
            if (!interface) {
                log_error("%s: Out of memory.", obj->uri);
                free(uri);
                rc = OMADM_SYNCML_ERROR_DEVICE_FULL;
            }
            else {
                memcpy(interface, &__interface, sizeof(omadm_mo_interface_t));
                interface->base_uri = uri;
                rc = omadmclient_session_add_mo(session, interface, obj);
                if (rc) {
                    free(uri);
                    free(interface);
                    log_error("%s: omadmclient_session_add_mo() failed with code %d.", obj->uri, rc);
                }
            }
        }

        log_trace("%s: install: rc=%i", obj->uri, rc);
        if (rc) {
            lua_pop(L, 3);
            return false;
        }
    }
    lua_pop(L, 1);
    return true;
}

void lua_object_shutdown_all(lua_State* L)
{
    luaR_getfield(L, "__objects");
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
    {
        lua_object_t* obj = (lua_object_t*)lua_touserdata(L, -1);
        log_trace("%s: shutdown", obj->uri);

        int rc = lua_handler_prepare(obj, "Shutdown");
        if (!rc) {
            rc = lua_handler_pcall(obj, 0, 0);
        }

        log_trace("%s: shutdown: rc=%i", obj->uri, rc);
    }
    lua_pop(L, 1);
}
