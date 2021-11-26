/*
 * NetComm OMA-DM Client
 *
 * lua_sched.c
 * Lua event scheduler library.
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
#include <sys/types.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>

#include "lua_sched.h"
#include "util.h"

#define MODULE_NAME "schedule"
#define QUEUE_NAME  ("__" MODULE_NAME)

typedef struct event_t {
    struct event_t* next;
    struct timespec when;
    struct timespec period;
    int             my_ref;
    int             fn_ref;
} event_t;

static event_t* createEvent(lua_State* L, int fn_index, int period_ms)
{
    lua_pushvalue(L, fn_index);

    event_t* event = lua_newuserdata(L, sizeof(event_t));
    event->next = NULL;
    event->my_ref = luaR_ref(L);
    event->fn_ref = luaR_ref(L);
    time_from_ms(&event->period, period_ms);

    return event;
}

static void deleteEvent(lua_State* L, event_t* event)
{
    luaR_unref(L, event->my_ref);
    luaR_unref(L, event->fn_ref);
}

static bool runEvent(lua_State* L, event_t* event, bool* repeat)
{
    luaR_getref(L, event->fn_ref);
    if (lua_pcall(L, 0, 1, 0)) {
        *repeat = false;
        return false;
    }
    *repeat = luaL_popboolean(L, 0);
    return true;
}

static event_t* insertEvent(event_t* first, event_t* event)
{
    time_now(&event->when);
    time_add(&event->when, &event->period);

    if (!first) {
        event->next = NULL;
        return event;
    }
    if (time_diff_ms(&first->when, &event->when) < 0) {
        event->next = first;
        return event;
    }
    event_t* curr = first;
    while (curr->next && time_diff_ms(&curr->next->when, &event->when) >= 0) {
        curr = curr->next;
    }
    event->next = curr->next;
    curr->next = event;
    return first;
}

static void setFirstEvent(lua_State* L, event_t* event)
{
    if (event) {
        luaR_getref(L, event->my_ref);
    } else {
        lua_pushnil(L);
    }
    luaR_setfield(L, QUEUE_NAME);
}

static event_t* getFirstEvent(lua_State* L)
{
    luaR_getfield(L, QUEUE_NAME);
    return (event_t*)luaL_popuserdata(L, 0);
}

/* schedule.Event(period_ms, callback)
 *
 * Schedule a callback function to run after the specified number
 * of milliseconds have passed. Callback functions have the option
 * of returning a boolean value indicating whether or not the event
 * should be repeated.
 */
static int __schedule_event(lua_State* L)
{
    int period_ms = luaL_checkinteger(L, 1);
    luaL_argcheck(L, period_ms >= 0, 1, "period cannot be negative");
    luaL_checktype(L, 2, LUA_TFUNCTION);

    setFirstEvent(L, insertEvent(getFirstEvent(L), createEvent(L, 2, period_ms)));
    return 0;
}

/* Load library into Lua environment. */
void luaopen_schedule(lua_State* L)
{
    /* Library functions. */
    static const luaL_Reg funcs[] = {
        {"Event", __schedule_event},
        {NULL,    NULL}
    };

    /* Create library and install all functions in it. */
    luaL_register(L, MODULE_NAME, funcs);
    lua_pop(L, 1);
}

/* Perform one scheduler cycle, running any events due for execution.
 * Returns true if no errors occured, otherwise false with the error
 * message at the top of the stack. */
bool lua_schedule_run(lua_State* L)
{
    event_t* event = getFirstEvent(L);
    while (event && time_diff_now_ms(&event->when) < 1)
    {
        setFirstEvent(L, event->next);
        bool repeat;
        bool success = runEvent(L, event, &repeat);
        if (repeat) {
            setFirstEvent(L, insertEvent(getFirstEvent(L), event));
        } else {
            deleteEvent(L, event);
        }
        if (!success) {
            return false;
        }
        event = getFirstEvent(L);
    }
    return true;
}

/* Returns the number of milliseconds until the next event
 * is due to run, clamped at max_wait_ms milliseconds. If
 * no events are scheduled then max_wait_ms is returned. */
int lua_schedule_calculate_wait_ms(lua_State* L, int max_wait_ms)
{
    if (max_wait_ms < 0) {
        return 0;
    }
    event_t* event = getFirstEvent(L);
    if (event) {
        int diff = time_diff_now_ms(&event->when);
        return (diff < 0 ? 0 : (diff > max_wait_ms ? max_wait_ms : diff));
    }
    return max_wait_ms;
}

/* Schedule a C function to be called after the specified delay. */
void lua_schedule_event(lua_State* L, int period_ms, lua_CFunction fn)
{
    lua_pushcfunction(L, fn);
    setFirstEvent(L, insertEvent(getFirstEvent(L), createEvent(L, -1, period_ms)));
    lua_pop(L, 1);
}
