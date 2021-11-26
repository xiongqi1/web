/*
 * NetComm OMA-DM Client
 *
 * lua_dm_path.c
 * Lua object to represent DM tree paths.
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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>

#define TYPE "DmPath"

typedef struct {
    const char*  uri;
    int          uriLen;
    const char** segs;
    int          segCount;
    int          index;
} dm_path_t;

/* Returns the number of path segments in the given URI. */
static int countSegments(const char* uri)
{
    int segCount = 1;
    while (*uri) { 
        if (*uri++ == '/') {
            segCount++;
        }
    }
    return segCount;
}

/* Finds the start of each path segment in the given URI. */
static void findSegments(const char* uri, const char** segs)
{
    *segs++ = uri;
    while (*uri) {
        if (*uri++ == '/') {
            *segs++ = uri;
        }
    }
}

/* Calculate the length of a portion of the path. */
static int getLength(dm_path_t* path, int first, int last)
{
    const char* start = path->segs[first - 1];
    if (last < path->segCount) {
        return path->segs[last] - start - 1;
    }
    return path->uriLen - (start - path->uri);
}

/* Create and populate a new path object. */
static dm_path_t* createPath(lua_State* L, const char* uri)
{
    /* Allocate enough room for the path, segment list, and copy of the URI. */
    int uriLen = strlen(uri);
    int segCount = countSegments(uri);
    dm_path_t* path = lua_newuserdata(L, 
        sizeof(dm_path_t) + sizeof(const char*) * segCount + uriLen + 1
    );

    /* Doing everything in a single allocation means no need for a GC hook. */
    path->segs = (const char**)(path + 1);
    path->segCount = segCount;
    path->uri = strcpy((char*)(path->segs + segCount), uri);
    path->uriLen = uriLen;
    path->index = !strncmp(uri, "./", 2) ? 2 : 1;

    findSegments(path->uri, path->segs);
    luaL_setuserdatatype(L, -1, TYPE);
    return path;
}

/* Check if given stack position contains a path object. */
static dm_path_t* luaL_check_dm_path(lua_State* L, int index)
{
    return (dm_path_t*)luaL_checkudata(L, index, TYPE);
}

/* Create a new path object from within Lua. */
static int __dm_path_new(lua_State* L)
{
    createPath(L, luaL_checkstring(L, 1));
    return 1;
}

/* Returns the complete path. */
static int __dm_path_complete(lua_State* L)
{
    dm_path_t* path = luaL_check_dm_path(L, 1);
    lua_pushlstring(L, path->uri, path->uriLen);
    return 1;
}

/* Returns path up to and including current segment. */
static int __dm_path_current(lua_State* L)
{
    dm_path_t* path = luaL_check_dm_path(L, 1);  
    lua_pushlstring(L, path->uri, getLength(path, 1, path->index));
    return 1;
}

/* Returns the current segment. */
static int __dm_path_segment(lua_State* L)
{
    dm_path_t* path = luaL_check_dm_path(L, 1);
    lua_pushlstring(L, path->segs[path->index-1], getLength(path, path->index, path->index));
    return 1;
}

/* Get or set the current segment index. */
static int __dm_path_index(lua_State* L)
{
    dm_path_t* path = luaL_check_dm_path(L, 1);
    if (!lua_isnoneornil(L, 2)) {
        int index = luaL_checkinteger(L, 2);
        if (index < 1 || index > path->segCount) {
            luaL_error(L, "Segment index out of bounds.");
        }
        path->index = index;
        return 0;
    }
    lua_pushinteger(L, path->index);
    return 1;
}

/* Advance to next segment. */
static int __dm_path_next(lua_State* L)
{
    dm_path_t* path = luaL_check_dm_path(L, 1);
    if (path->index < path->segCount) {
        path->index++;
    } else {
        luaL_error(L, "Segment index out of bounds.");
    }
    lua_pushvalue(L, 1);
    return 1;
}

/* Advance to next segment. */
static int __dm_path_hasnext(lua_State* L)
{
    dm_path_t* path = luaL_check_dm_path(L, 1);
    lua_pushboolean(L, path->index < path->segCount);
    return 1;
}

/* Install extensions into the given Lua machine. */
void luaopen_dm_path(lua_State* L)
{
    static const luaL_Reg methods[] = {
        {"Complete", __dm_path_complete},
        {"Current",  __dm_path_current},
        {"Segment",  __dm_path_segment},
        {"Index",    __dm_path_index},
        {"Next",     __dm_path_next},
        {"HasNext",  __dm_path_hasnext},
        {NULL,       NULL}
    };

    luaL_createuserdatatype(L, TYPE, methods);
    lua_pushcfunction(L, __dm_path_new);
    lua_setglobal(L, "DmPath");
}

/* Push a new path object onto the stack. */
void lua_push_dm_path(lua_State* L, const char* uri)
{
    createPath(L, uri);
}

