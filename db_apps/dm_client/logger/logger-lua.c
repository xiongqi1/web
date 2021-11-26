/*
 * Lua wrapper library for Logger.
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
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>
#include <logger.h>

#define MODULE_NAME "logger"

/* For installing convenience functions; see luaL_setloggers and luaopen_logger. */
typedef struct {
    const char *name;
    lua_CFunction func;
    int level;
} luaL_Logger;

static void formatArgument(lua_State* L, logger_buf* buf, int idx, char fmt);
static void formatMessage(lua_State* L, logger_buf* buf, const char* fmt, int nextArg);

static void dumpIndent(logger_buf* buf, int indent);
static void dumpTable(lua_State* L, logger_buf* buf, int idx, int depth, int indent, int allowMeta);
static void dumpValue(lua_State* L, logger_buf* buf, int idx, int depth, int indent, int allowMeta);

/* Retrieve the file name and line number for the given Lua stack frame. */
static void getSource(lua_State* L, int where, char* file, int* line)
{
    lua_Debug dbg;
    if (!lua_getstack(L, where, &dbg) || !lua_getinfo(L, "Sl", &dbg)) {
        *line = 0;
        strncpy(file, "[?]", LUA_IDSIZE);
    } else {
        *line = dbg.currentline;
        strncpy(file, dbg.short_src, LUA_IDSIZE);
    }
}

/* Retrieve a specified field from the options table if possible,
 * otherwise return a default value. */
static bool getBooleanOpt(lua_State* L, int idx, const char* name, bool def)
{
    lua_getfield(L, idx, name);
    if (!lua_isnil(L, -1)) {
        def = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
    return def;
}

/* Retrieve a specified field from the options table if possible,
 * otherwise return a default value. */
static int getIntegerOpt(lua_State* L, int idx, const char* name, int def)
{
    lua_getfield(L, idx, name);
    if (lua_isnumber(L, -1)) {
        def = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    return def;
}

/* Writes a string representation of the value at the given index,
 * honoring __tostring metamethods, returning values where possible
 * and type names everywhere else. */
static void formatArgument(lua_State* L, logger_buf* buf, int idx, char fmt)
{
    /* Missing arguments. */
    if (lua_isnone(L, idx)) {
        luaL_error(L, "no argument provided for specifier '%%%c'", fmt);
    }
    /* Generic behaviour, accepts anything and converts to string. */
    else if (fmt == 's') {
        /* Boolean values. */
        if (lua_isboolean(L, idx)) {
            logger_bzwrite(buf, lua_toboolean(L, idx) ? "true" : "false");
        }
        /* String and number values. */
        else if (lua_isstring(L, idx)) {
            logger_bzwrite(buf, lua_tostring(L, idx));
        }
        /* Anything with a __tostring metamethod. */
        else if (luaL_callmeta(L, idx, "__tostring")) {
            logger_bzwrite(buf, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        /* Everything else, type name only. */
        else {
            logger_bzwrite(buf, luaL_typename(L, idx));
        }
    }
    /* Interpret value as a boolean according to Lua conversion rules. */
    else if (fmt == 'b') {
        logger_bzwrite(buf, lua_toboolean(L, idx) ? "true" : "false");
    }
    /* Everything below this point requires a number. */
    else if (!lua_isnumber(L, idx)) {
        luaL_error(L, "expected number for specifier '%%%c', got %s", fmt, luaL_typename(L, idx));
    }
    /* Interpret value as a number according to Lua conversion rules. */
    else if (fmt == 'f') {
        logger_bfwrite(buf, "%f", lua_tonumber(L, idx));
    }
    /* Interpret value as an integer according to Lua conversion rules. */
    else if (fmt == 'i' || fmt == 'x') {
        logger_bfwrite(buf, fmt == 'i' ? "%i" : "%x", lua_tointeger(L, idx));
    }
}

/* Super-simple printf-style message formatting; currently supported format
 * specifiers are %s, %b, %f, %i and %x, with %s acting as a catch-all that
 * will accept all possible values. */
static void formatMessage(lua_State* L, logger_buf* buf, const char* fmt, int nextArg)
{
    for (; *fmt && logger_bcheck(buf, 1); fmt++) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's' || *fmt == 'b' || *fmt == 'f' || *fmt == 'i' || *fmt == 'x') {
                formatArgument(L, buf, nextArg++, *fmt);
            } else {
                logger_bcwrite(buf, '%');
                if (*fmt && *fmt != '%') {
                    logger_bcwrite(buf, *fmt);
                }
            }
        } else {
            logger_bcwrite(buf, *fmt);
        }
    }
}

/* Print indentation for table dumps. */
static void dumpIndent(logger_buf* buf, int indent)
{
    logger_bzwrite(buf, "  ");
    while (indent-- > 0) {
        logger_bzwrite(buf, "    ");
    }
}

/* Pretty-print a single Lua value for table dumps. */
static void dumpValue(lua_State* L, logger_buf* buf, int idx, int depth, int indent, int allowMeta)
{
    /* Print type name first. */
    logger_bfwrite(buf, "<%s>", luaL_typename(L, idx));

    /* Print value if possible/relevant. */
    int type = lua_type(L, idx);
    if (type == LUA_TBOOLEAN) {
        logger_bzwrite(buf, lua_toboolean(L, idx) ? " true" : " false");
    }
    else if (type == LUA_TNUMBER) {
        logger_bfwrite(buf, " %s", lua_tostring(L, idx));
    }
    else if (type == LUA_TSTRING) {
        logger_bfwrite(buf, " \"%s\"", lua_tostring(L, idx));
    }
    else if (allowMeta && luaL_callmeta(L, idx, "__tostring")) {
        logger_bfwrite(buf, " %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    else if (type == LUA_TTABLE) {
        if (depth != 0) {
            logger_bcwrite(buf, ' ');
            dumpTable(L, buf, idx, depth, indent, allowMeta);
        } else {
            logger_bzwrite(buf, " {...}");
        }
    }
}

/* Write a pretty-printed dump of a Lua table. */
static void dumpTable(lua_State* L, logger_buf* buf, int idx, int depth, int indent, int allowMeta)
{
    logger_bzwrite(buf, "{");
    idx = lua_absindex(L, idx);
    bool empty = true;
    for (lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1))
    {
        if (!logger_bcheck(buf, 1)) {
            lua_pop(L, 2);
            break;
        }
        empty = false;
        logger_bzwrite(buf, "\n");
        lua_pushvalue(L, -2);
        dumpIndent(buf, indent + 1);
        dumpValue(L, buf, -1, 0, 0, allowMeta);
        lua_pop(L, 1);
        logger_bzwrite(buf, " = ");
        dumpValue(L, buf, -1, depth - 1, indent + 1, allowMeta);
    }
    if (!empty) {
        logger_bzwrite(buf, "\n");
        dumpIndent(buf, indent);
    }
    logger_bzwrite(buf, "}");
}

/* logger.Initialise(opts)
 *
 * Initialise library with the specified options table. Fields permitted
 * in the options table match those in the logger_opts struct, with the
 * addition of "name" as a string field rather than a separate argument.
 */
static int __init(lua_State* L)
{
    if (lua_istable(L, 1)) {
        const char* name = NULL;
        logger_opts opts = {
            .use_syslog  = getBooleanOpt(L, 1, "use_syslog",  true),
            .use_console = getBooleanOpt(L, 1, "use_console", true),
            .no_colors   = getBooleanOpt(L, 1, "no_colors",   false),
            .no_date     = getBooleanOpt(L, 1, "no_date",     false),
            .no_source   = getBooleanOpt(L, 1, "no_source",   false),
            .min_level   = getIntegerOpt(L, 1, "min_level",   LOGGER_INFO)
        };
        lua_getfield(L, 1, "name");
        if (lua_isstring(L, -1)) {
            name = lua_tostring(L, -1);
        }
        logger_initialise(&opts, name);
        lua_pop(L, 1);
    } else {
        logger_initialise(NULL, NULL);
    }
    return 0;
}

/* logger.LogAt(where, level, message)
 *
 * Print a log message, manually specifying both the log
 * level and the stack frame to report as the origin.
 */
static int __logAt(lua_State* L)
{
    int where = luaL_checkinteger(L, 1) + 1;
    int level = luaL_checkinteger(L, 2);
    const char* format = luaL_checkstring(L, 3);

    if (logger_willprint(level, false))
    {
        int line;
        char file[LUA_IDSIZE];
        getSource(L, where, file, &line);

        char message[2048];
        logger_buf buf = LOGGER_BINIT(message, sizeof(message));
        formatMessage(L, &buf, format, 4);
        logger_bfinish(&buf);

        __logger(level, file, line, false, message);
    }
    return 0;
}

/* logger.TraceAt(where, message)
 * logger.DebugAt(where, message)
 * logger.InfoAt(where, message)
 * logger.NoticeAt(where, message)
 * logger.WarningAt(where, message)
 * logger.ErrorAt(where, message)
 *
 * Print a log message, manually specifying the stack frame to
 * report as the origin, with the log level taken from the
 * first upvalue. Intended to be installed as a C closure
 * using luaL_setloggers.
 */
static int __logAt_closure(lua_State*L)
{
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 2);
    return __logAt(L);
}

/* logger.Log(level, message)
 *
 * Print a log message from the current stack frame,
 * manually specifying the log level.
 */
static int __log(lua_State* L)
{
    lua_pushinteger(L, 0);
    lua_insert(L, 1);
    return __logAt(L);
}

/* logger.Trace(message)
 * logger.Debug(message)
 * logger.Info(message)
 * logger.Notice(message)
 * logger.Warning(message)
 * logger.Error(message)
 *
 * Print a log message from the current stack frame, with the
 * log level taken from the first upvalue. Intended to be
 * installed as a C closure using luaL_setloggers.
 */
static int __log_closure(lua_State*L)
{
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    return __log(L);
}

/* logger.Hex(level, description, buffer)
 *
 * Print a hex dump from the current stack frame,
 * manually specifying the log level.
 */
static int __logHex(lua_State* L)
{
    size_t length;
    int level = luaL_checkinteger(L, 1);
    const char* desc = luaL_checkstring(L, 2);
    const char* buffer = luaL_checklstring(L, 3, &length);

    if (logger_willprint(level, true))
    {
        int line;
        char file[LUA_IDSIZE];
        getSource(L, 1, file, &line);

        __loggerh(level, file, line, desc, buffer, length);
    }
    return 0;
}

/* logger.HexTrace(description, buffer)
 * logger.HexDebug(description, buffer)
 * logger.HexInfo(description, buffer)
 * logger.HexNotice(description, buffer)
 * logger.HexWarning(description, buffer)
 * logger.HexError(description, buffer)
 *
 * Print a hex dump from the current stack frame, with the
 * log level taken from the first upvalue. Intended to be
 * installed as a C closure using luaL_setloggers.
 */
static int __logHex_closure(lua_State* L)
{
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    return __logHex(L);
}

/* logger.Table(level, description, table, [depth], [ignoreMeta])
 *
 * Print a table dump from the current stack frame,
 * manually specifying the log level.
 */
static int __logTable(lua_State* L)
{
    int level = luaL_checkinteger(L, 1);
    const char* desc = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    int depth = luaL_optinteger(L, 4, 1);
    int allowMeta = !lua_toboolean(L, 5);

    if (logger_willprint(level, true))
    {
        int line;
        char file[LUA_IDSIZE];
        getSource(L, 1, file, &line);

        char message[8192];
        logger_buf buf = LOGGER_BINIT(message, sizeof(message));
        logger_bfwrite(&buf, "[TBL] %s, table contents:\n  ", desc);
        dumpTable(L, &buf, 3, depth, 0, allowMeta);
        logger_bfinish(&buf);

        __logger(level, file, line, true, message);
    }
    return 0;
}

/* logger.TableTrace(description, table, [depth], [ignoreMeta])
 * logger.TableDebug(description, table, [depth], [ignoreMeta])
 * logger.TableInfo(description, table, [depth], [ignoreMeta])
 * logger.TableNotice(description, table, [depth], [ignoreMeta])
 * logger.TableWarning(description, table, [depth], [ignoreMeta])
 * logger.TableError(description, table, [depth], [ignoreMeta])
 *
 * Print a table dump from the current stack frame, with the
 * log level taken from the first upvalue. Intended to be
 * installed as a C closure using luaL_setloggers.
 */
static int __logTable_closure(lua_State* L)
{
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    return __logTable(L);
}

/* Used to install functions as closures that take a single integer upvalue. */
static void luaL_setloggers(lua_State* L, const luaL_Logger* loggers)
{
    for (; loggers->name; loggers++) {
        lua_pushinteger(L, loggers->level);
        lua_pushcclosure(L, loggers->func, 1);
        lua_setfield(L, -2, loggers->name);
    }
}

/* Load library into Lua environment. */
int luaopen_logger(lua_State* L)
{
    /* Log levels and other constants. */
    const luaL_Enum enums[] = {
        {"ALL",     LOGGER_ALL},
        {"TRACE",   LOGGER_TRACE},
        {"DEBUG",   LOGGER_DEBUG},
        {"INFO",    LOGGER_INFO},
        {"NOTICE",  LOGGER_NOTICE},
        {"WARNING", LOGGER_WARNING},
        {"ERROR",   LOGGER_ERROR},
        {"NONE",    LOGGER_NONE},
        {NULL, 0}
    };

    /* Basic library functions. */
    const luaL_Reg funcs[] = {
        {"Initialise", __init},
        {"Log",        __log},
        {"LogAt",      __logAt},
        {"Hex",        __logHex},
        {"Table",      __logTable},
        {NULL, NULL}
    };

    /* Convenience functions, implemented as closures. */
    const luaL_Logger loggers[] = {
        /* Log message. */
        {"Trace",   __log_closure, LOGGER_TRACE},
        {"Debug",   __log_closure, LOGGER_DEBUG},
        {"Info",    __log_closure, LOGGER_INFO},
        {"Notice",  __log_closure, LOGGER_NOTICE},
        {"Warning", __log_closure, LOGGER_WARNING},
        {"Error",   __log_closure, LOGGER_ERROR},
        /* Log message against specified stack frame. */
        {"TraceAt",   __logAt_closure, LOGGER_TRACE},
        {"DebugAt",   __logAt_closure, LOGGER_DEBUG},
        {"InfoAt",    __logAt_closure, LOGGER_INFO},
        {"NoticeAt",  __logAt_closure, LOGGER_NOTICE},
        {"WarningAt", __logAt_closure, LOGGER_WARNING},
        {"ErrorAt",   __logAt_closure, LOGGER_ERROR},
        /* Log hex dump. */
        {"HexTrace",   __logHex_closure, LOGGER_TRACE},
        {"HexDebug",   __logHex_closure, LOGGER_DEBUG},
        {"HexInfo",    __logHex_closure, LOGGER_INFO},
        {"HexNotice",  __logHex_closure, LOGGER_NOTICE},
        {"HexWarning", __logHex_closure, LOGGER_WARNING},
        {"HexError",   __logHex_closure, LOGGER_ERROR},
        /* Log table dump. */
        {"TableTrace",   __logTable_closure, LOGGER_TRACE},
        {"TableDebug",   __logTable_closure, LOGGER_DEBUG},
        {"TableInfo",    __logTable_closure, LOGGER_INFO},
        {"TableNotice",  __logTable_closure, LOGGER_NOTICE},
        {"TableWarning", __logTable_closure, LOGGER_WARNING},
        {"TableError",   __logTable_closure, LOGGER_ERROR},
        {NULL, NULL, 0}
    };

    /* Create library and install all functions in it. */
    luaL_register(L, MODULE_NAME, funcs);
    luaL_setenums(L, enums);
    luaL_setloggers(L, loggers);

    return 1;
}

/* Load library into Lua environment; use this when installing via C API. */
void luaopen_logger_C(lua_State* L)
{
    luaopen_logger(L);
    lua_pop(L, 1);
}
