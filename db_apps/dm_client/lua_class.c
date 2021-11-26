/*
 * NetComm OMA-DM Client
 *
 * lua_class.c
 * Basic Lua class creation with inheritance.
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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>

/*
 * For reference, the code below should be roughly equivalent to
 * the following Lua snippet.
 *
 * function Class(base)
 *   local class = {}
 *   local methods = {}
 *   if base then
 *     for k, v in pairs(base)
 *       class[k] = v
 *     end
 *     for k, v in pairs(base.__methods)
 *       methods[k] = v
 *     end
 *     class.__base = base
 *     methods.__base = base.__methods
 *   end
 *   methods.__index = methods
 *   class.__methods = methods
 *   return setmetatable(class, {
 *     __call = function (_, ...)
 *         local object = setmetatable({}, {
 *           __index = methods
 *         })
 *         if object.__init then
 *           object:__init(...)
 *         end
 *         return object
 *     end
 *   })
 * end
 */

static int __construct(lua_State* L)
{
    /* Create new object and set its metatable. */
    lua_newtable(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L, -2);

    /* Check if this class has an __init method. */
    lua_getfield(L, -1, "__init");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
    }
    else {
        /* Permute stack; [class, args, obj, __init] --> [class, obj, __init, obj, args] */
        lua_pushvalue(L, -2);
        lua_insert(L, 2);
        lua_insert(L, 3);
        lua_insert(L, 4);
        /* Call __init. */
        lua_call(L, lua_gettop(L) - 3, 0);
    }

    /* Return new object. */
    return 1;
}

static int __instanceof(lua_State* L)
{
    /* Is it a table? */
    if (lua_type(L, 1) == LUA_TTABLE) {
        /* Does it have a metatable? */
        if (lua_getmetatable(L, 1)) {
            /* Is it our metatable? */
            lua_pushvalue(L, lua_upvalueindex(1));
            if (lua_equal(L, -1, -2)) {
                /* One of us! */
                lua_pop(L, 2);
                lua_pushboolean(L, true);
                return 1;
            }
            lua_pop(L, 2);
        }
    }

    /* Not one of us. */
    lua_pushboolean(L, false);
    return 1;
}

static int __class(lua_State* L)
{
    int type = lua_type(L, 1);
    if (type != LUA_TNIL && type != LUA_TNONE)
    {
        /* Clone base class. */
        if (type != LUA_TTABLE) {
            return luaL_argerror(L, 1, "expected base class or nil");
        }
        luaL_clonetable(L, 1, 0, 3);

        /* Save reference to base class. */
        lua_pushvalue(L, 1);
        lua_setfield(L, -2, "__base");

        /* Clone base class __methods. */
        lua_getfield(L, 1, "__methods");
        if (lua_isnil(L, -1)) {
            return luaL_argerror(L, 1, "base class has no '__methods' field");
        }
        luaL_clonetable(L, -1, 0, 2);

        /* Save reference to base class __methods. */
        lua_insert(L, -2);
        lua_setfield(L, -2, "__base");
    }
    else
    {
        /* No base class; create new tables for class and methods. */
        lua_createtable(L, 0, 3);
        lua_createtable(L, 0, 1);
    }

    /* Set __methods as its own index. */
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    /* Create class metatable. */
    lua_createtable(L, 0, 1);

    /* Install constructor as __call metamethod. */
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, __construct, 1);
    lua_setfield(L, -2, "__call");

    /* Set class metatable. */
    lua_setmetatable(L, -3);

    /* Install InstanceOf as static method. */
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, __instanceof, 1);
    lua_setfield(L, -3, "InstanceOf");

    /* Save reference to __methods. */
    lua_setfield(L, -2, "__methods");

    /* Return new class. */
    return 1;
}

void luaopen_class(lua_State* L)
{
    lua_pushcfunction(L, __class);
    lua_setglobal(L, "Class");
}
