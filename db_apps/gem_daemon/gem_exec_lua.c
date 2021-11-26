// Part of GEM daemon
// Lua execute functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <unistd.h>
#include "gem_api.h"

//
// Execute Lua snippet, that can be in external file, or a memory
// buffer (depending on the last argument)
//
// The function forks and child process passes control to
// Lua, as far as the caller is concerned it never returns
//
// Return >0 if child forked successfully.
// -1 on error
//
// Note Lua 5.1 is required
//
static int execute_lua(char *exe_name, int num_bytes, int in_file)
{
    lua_State *L;
    int error;

    pid_t pID = fork();

    // if child
    if (pID == 0)
    {
        L = lua_open();   // opens Lua
        luaL_openlibs(L); // opens the basic library

        if (in_file)
        {
            error = luaL_loadfile(L, exe_name);
        }
        else
        {
            error = luaL_loadbuffer(L, exe_name, num_bytes, "GEM EE");
        }

        if (error)
        {
            gem_syslog(LOG_ERR, "%s", lua_tostring(L, -1));
            lua_pop(L, 1);  // pop error message from the stack
        }
        else
        {
            error = lua_pcall(L, 0, 0, 0);
            if (error)
            {
                gem_syslog(LOG_ERR, "%s", lua_tostring(L, -1));
                lua_pop(L, 1);  // pop error message from the stack
            }
        }
        lua_close(L);

        exit (error ? EXIT_FAILURE : EXIT_SUCCESS);
    }
    // if parent
    else if (pID > 0)
    {
        // child is executing!
        return pID;
    }
    else
    {
        gem_syslog(LOG_CRIT, "Fork failed in lua exec");
        return -1;
    }
}

// execute Lua snippet that resides in fileName
int gem_execute_lua_file(const char* fileName)
{
    return execute_lua((char *)fileName, 0, 1);
}

// script is stored in rdb variable (base64 encoded)
int gem_execute_lua_rdb_snippet(char* script, int num_bytes)
{
    return execute_lua(script, num_bytes, 0);
}

