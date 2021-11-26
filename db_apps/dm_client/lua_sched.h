/*
 * NetComm OMA-DM Client
 *
 * lua_sched.h
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

#ifndef __OMADM_LUA_SCHEDULE_H_20180402__
#define __OMADM_LUA_SCHEDULE_H_20180402__

    #include <lua.h>

    /* Install scheduler into Lua machine. */
    void luaopen_schedule(lua_State* L);

    /* Perform one scheduler cycle, running any events due for execution.
     * Returns true if no errors occured, otherwise false with the error
     * message at the top of the stack. */
    bool lua_schedule_run(lua_State* L);

    /* Returns the number of milliseconds until the next event
     * is due to run, clamped at max_wait_ms milliseconds. If
     * no events are scheduled then max_wait_ms is returned. */
    int  lua_schedule_calculate_wait_ms(lua_State* L, int max_wait_ms);

    /* Schedule a C function to be called after the specified delay. */
    void lua_schedule_event(lua_State* L, int period_ms, lua_CFunction fn);

#endif
