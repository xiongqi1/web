/*
 * NetComm OMA-DM Client
 *
 * lua_enum.c
 * Utilities for generating reversible enumerations in Lua, as well
 * as definitions of any standard enumerations to be installed.
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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>

#include <curl/curl.h>
#include <omadmtree_mo.h>
#include <syncml_error.h>

/* SyncML status codes; adopted as generic status code within scripted functions. */
static const luaL_Enum enumStatus[] = {
    {"NoError",                OMADM_SYNCML_ERROR_NONE},
    {"InProgress",             OMADM_SYNCML_ERROR_IN_PROGRESS},
    {"Success",                OMADM_SYNCML_ERROR_SUCCESS},
    {"AcceptedForProcessing",  OMADM_SYNCML_ERROR_ACCEPTED_FOR_PROCESSING},
    {"AuthenticationAccepted", OMADM_SYNCML_ERROR_AUTHENTICATION_ACCEPTED},
    {"OperationCancelled",     OMADM_SYNCML_ERROR_OPERATION_CANCELED},
    {"NotExecuted",            OMADM_SYNCML_ERROR_NOT_EXECUTED},
    {"NotModified",            OMADM_SYNCML_ERROR_NOT_MODIFIED},
    {"InvalidCredentials",     OMADM_SYNCML_ERROR_INVALID_CREDENTIALS},
    {"Forbidden",              OMADM_SYNCML_ERROR_FORBIDDEN},
    {"NotFound",               OMADM_SYNCML_ERROR_NOT_FOUND},
    {"NotAllowed",             OMADM_SYNCML_ERROR_NOT_ALLOWED},
    {"NotSupported",           OMADM_SYNCML_ERROR_OPTIONAL_FEATURE_NOT_SUPPORTED},
    {"MissingCredentials",     OMADM_SYNCML_ERROR_MISSING_CREDENTIALS},
    {"IncompleteCommand",      OMADM_SYNCML_ERROR_INCOMPLETE_COMMAND},
    {"PathTooLong",            OMADM_SYNCML_ERROR_URI_TOO_LONG},
    {"AlreadyExists",          OMADM_SYNCML_ERROR_ALREADY_EXISTS},
    {"DeviceFull",             OMADM_SYNCML_ERROR_DEVICE_FULL},
    {"PermissionDenied",       OMADM_SYNCML_ERROR_PERMISSION_DENIED},
    {"CommandFailed",          OMADM_SYNCML_ERROR_COMMAND_FAILED},
    {"CommandNotImplemented",  OMADM_SYNCML_ERROR_COMMAND_NOT_IMPLEMENTED},
    {"InternalError",          OMADM_SYNCML_ERROR_SESSION_INTERNAL},
    {"AtomicFailed",           OMADM_SYNCML_ERROR_ATOMIC_FAILED},
    {NULL, 0}
};

/* Node types. */
static const luaL_Enum enumType[] = {
    {"NotFound", OMADM_NODE_NOT_EXIST},
    {"Interior", OMADM_NODE_IS_INTERIOR},
    {"Leaf",     OMADM_NODE_IS_LEAF},
    {NULL, 0}
};

/* cUrl error codes. */
static const luaL_Enum enumCurl[] = {
    {"Success",            CURLE_OK},
    {"CouldntResolveHost", CURLE_COULDNT_RESOLVE_HOST},
    {"CouldntConnect",     CURLE_COULDNT_CONNECT},
    {"WriteError",         CURLE_WRITE_ERROR},
    {"OutOfMemory",        CURLE_OUT_OF_MEMORY},
    {"OperationTimedOut",  CURLE_OPERATION_TIMEDOUT},
    {NULL, 0}
};

/* Enum(values)
 *
 * Returns a table both forward and reverse mappings
 * of every entry in the table 'values', i.e. both
 * key->value and value<-key.
 *
 * Example:
 *   Enum({
 *     "FIRST",
 *     "SECOND",
 *     "THIRD"
 *   })
 *
 * Returns:
 *   {
 *      ["FIRST"] = 1,
 *      ["SECOND"] = 2,
 *      ["THIRD"] = 3,
 *      [1] = "FIRST",
 *      [2] = "SECOND",
 *      [3] = "THIRD"
 *   }
 */
static int __enum(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_newtable(L);
    for (lua_pushnil(L); lua_next(L, 1);) {
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        lua_settable(L, -5);
        lua_pushvalue(L, -2);
        lua_settable(L, -4);
    }
    return 1;
}

/* IsSuccess(status)
 *
 * Returns true if status is a SyncML status
 * code representing some form of success.
 */
static int __isSuccess(lua_State* L)
{
    int rc = luaL_checkinteger(L, 1);
    lua_pushboolean(L,
        rc == OMADM_SYNCML_ERROR_NONE ||
        rc == OMADM_SYNCML_ERROR_SUCCESS
    );
    return 1;
}

void luaopen_enum(lua_State* L)
{
    lua_newtable(L);
    luaL_setenums_reversible(L, enumStatus);
    lua_setglobal(L, "STATUS");

    lua_newtable(L);
    luaL_setenums_reversible(L, enumType);
    lua_setglobal(L, "TYPE");

    lua_newtable(L);
    luaL_setenums_reversible(L, enumCurl);
    lua_setglobal(L, "CURL");

    lua_register(L, "Enum",      __enum);
    lua_register(L, "IsSuccess", __isSuccess);
}
