/*
 * lextras.h
 * A collection of helper functions.
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

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "lextras.h"

/* Load and run any files ending with '.lua' in the specified directory. */
int luaL_dodir(lua_State* L, const char* path)
{
    DIR *dir = opendir(path);
    if (!dir) {
        lua_pushfstring(L, "opendir(\"%s\") failed: %s", path, strerror(errno));
        return false;
    }
    struct dirent* entry = readdir(dir);
    for (; entry; entry = readdir(dir)) {
        int len = strlen(entry->d_name);
        if (len < 5 || strcmp(entry->d_name + len - 4, ".lua")) {
            continue;
        }
        char file[PATH_MAX];
        snprintf(file, sizeof(file), "%s/%s", path, entry->d_name);
        struct stat sbuf;
        if (stat(file, &sbuf) || !S_ISREG(sbuf.st_mode)) {
            continue;
        }
        if (luaL_dofile(L, file)) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return true;
}

/* Shallow copy one table into another. */
void luaL_copytable(lua_State* L, int dst_index, int src_index)
{
    dst_index = lua_absindex(L, dst_index);
    src_index = lua_absindex(L, src_index);
    for (lua_pushnil(L); lua_next(L, src_index);) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_rawset(L, dst_index);
    }
}

/* Create a shallow clone of a table. */
void luaL_clonetable(lua_State* L, int index, int narr, int nrec)
{
    index = lua_absindex(L, index);
    lua_createtable(L, narr, nrec);
    for (lua_pushnil(L); lua_next(L, index);) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_rawset(L, -4);
    }
}

/* Install integer constants into the table at the top of the stack. */
void luaL_setenums(lua_State* L, const luaL_Enum* enums)
{
    for (; enums->name; enums++) {
        lua_pushinteger(L, enums->value);
        lua_setfield(L, -2, enums->name);
    }
}

/* Alternate version of luaL_setenums that installs both forward and reverse mapping. */
void luaL_setenums_reversible(lua_State* L, const luaL_Enum* enums)
{
    for (; enums->name; enums++) {
        lua_pushinteger(L, enums->value);
        lua_setfield(L, -2, enums->name);
        lua_pushstring(L, enums->name);
        lua_rawseti(L, -2, enums->value);
    }
}

/* Create and register type metatable. */
void luaL_createuserdatatype(lua_State* L, const char* name, const luaL_Reg* methods)
{
    luaL_newmetatable(L, name);
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    luaL_register(L, NULL, methods);
}

/* Find and attach type metatable to a userdata object. */
void luaL_setuserdatatype(lua_State* L, int index, const char* name)
{
    index = lua_absindex(L, index);
    luaL_getmetatable(L, name);
    lua_setmetatable(L, index);
}
