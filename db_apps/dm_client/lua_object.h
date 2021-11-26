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

#ifndef __OMADM_LUA_OBJECT_H_20180402__
#define __OMADM_LUA_OBJECT_H_20180402__

    #include <stdbool.h>
    #include <omadmclient.h>
    #include <lua.h>

    /* Install extensions into the given Lua machine. */
    void luaopen_object(lua_State* L);

    /* Give scripted objects a chance to initialise. */
    bool lua_object_initialise_all(lua_State* L);

    /* Install scripted objects into the given DM session. */
    int lua_object_install_all(lua_State* L, dmclt_session* session);

    /* Give scripted objects a chance to shut down. */
    void lua_object_shutdown_all(lua_State* L);

#endif
