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

#ifndef __LEXTRAS_H__
#define __LEXTRAS_H__

    #include <lua.h>

    /* For use with luaL_setenums. */
    typedef struct luaL_Enum {
        const char *name;
        lua_Integer value;
    } luaL_Enum;

    /* Aliases for interacting with Lua registry. */
    #define luaR_ref(L)           luaL_ref(L, LUA_REGISTRYINDEX)
    #define luaR_unref(L, ref)    luaL_unref(L, LUA_REGISTRYINDEX, ref)
    #define luaR_getref(L, ref)   lua_rawgeti(L, LUA_REGISTRYINDEX, ref)
    #define luaR_setref(L, ref)   lua_rawseti(L, LUA_REGISTRYINDEX, ref)
    #define luaR_getfield(L, key) lua_getfield(L, LUA_REGISTRYINDEX, key)
    #define luaR_setfield(L, key) lua_setfield(L, LUA_REGISTRYINDEX, key)

    /* Push a boolean value, using nil in place of false; useful when implementing sets. */
    #define luaL_pushtrueornil(L, val) \
        ({int _val = (val); _val ? lua_pushboolean(L, 1) : lua_pushnil(L);})

    /* Pop the value at the top of the stack before returning it as a boolean. */
    #define luaL_popboolean(L, pop_extra) \
        ({int _val = lua_toboolean(L, -1); lua_pop(L, (pop_extra) + 1); _val;})

    /* Pop the value at the top of the stack before returning it as an integer. */
    #define luaL_popinteger(L, pop_extra) \
        ({int _val = lua_tointeger(L, -1); lua_pop(L, (pop_extra) + 1); _val;})

    /* Pop the value at the top of the stack before returning it as a userdata pointer. */
    #define luaL_popuserdata(L, pop_extra) \
        ({void* _val = lua_touserdata(L, -1); lua_pop(L, (pop_extra) + 1); _val;})

    /* Check the type of an optional function argument. */
    #define luaL_opttype(L, arg, type) \
        ({if (!lua_isnoneornil(L, arg)) luaL_checktype(L, arg, type);})

    /* Convert relative stack index to absolute. */
    #define lua_absindex(L, idx) \
        ({int _idx = (idx); _idx < 0 ? lua_gettop(L) + _idx + 1 : _idx;})

    /* Load and run any files ending with '.lua' in the specified directory. */
    LUA_API int luaL_dodir(lua_State* L, const char* path);

    /* Shallow copy one table into another. */
    LUA_API void luaL_copytable(lua_State* L, int dst_index, int src_index);

    /* Create a shallow clone of a table. */
    LUA_API void luaL_clonetable(lua_State* L, int index, int narr, int nrec);

    /* Install integer constants into the table at the top of the stack. */
    LUA_API void luaL_setenums(lua_State* L, const luaL_Enum* enums);

    /* Alternate version of luaL_setenums that installs both forward and reverse mapping. */
    LUA_API void luaL_setenums_reversible(lua_State* L, const luaL_Enum* enums);

    /* Create and register type metatable. */
    LUA_API void luaL_createuserdatatype(lua_State* L, const char* name, const luaL_Reg* methods);

    /* Find and attach type metatable to a userdata object. */
    LUA_API void luaL_setuserdatatype(lua_State* L, int index, const char* name);

#endif
