--[[
    NetComm OMA-DM Client

    event.lua
    Subscribed named events.

    Copyright Notice:
    Copyright (C) 2018 NetComm Wireless Limited.

    This file or portions thereof may not be copied or distributed in any form
    (including but not limited to printed or electronic forms and binary or object forms)
    without the expressed written consent of NetComm Wireless Limited.
    Copyright laws and International Treaties protect the contents of this file.
    Unauthorized use is prohibited.

    THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
    WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
]]

local __events = {}

event = {}

--[[
    Subscribe to a named event.
]]
function event.Subscribe(name, callback)

    local ev = __events[name]
    if not ev then
        ev = {}
        __events[name] = ev
    end
    ev[callback] = callback
    return callback

end

--[[
    Unsubscribe from a named event.
]]
function event.Unsubscribe(name, callback)

    local ev = __events[name]
    if ev then
        ev[callback] = nil
    end

end

--[[
    Fire a named event.
]]
function event.Fire(name, ...)

    logger.Debug("Firing '%s' event.", name)
    local ev = __events[name]
    if ev then
        for _, callback in pairs(ev) do
            local good, err = pcall(callback, ...)
            if not good then
                logger.Error("Error during '%s' event handler: %s", name, err)
            end
        end
    end

end

--[[
    Fire and then clear a named event so it can't be fired again.
]]
function event.FireOnceOnly(name, ...)

    event.Fire(name, ...)
    logger.Debug("Clearing subscriptions for '%s' event.", name)
    __events[name] = nil

end

--[[
    Install some named events.

    ClientStart:
        Fires once, after all scripts have been loaded and client has entered event loop.

    SystemStart:
        Fires once, after all system initscripts have completed.

    GotNetworkTime:
        Fires once, after network time first becomes available.

    GotIMEI:
        Fires once, after IMEI becomes available in RDB.
]]
local token_ss
local token_imei
local token_time
schedule.Event(0, function()
    event.FireOnceOnly("ClientStart")
    token_ss = rdb.Watch("system.router_ready", function(key, value)
        if value == "1" then
            schedule.Event(0, function()
                event.FireOnceOnly("SystemStart")
                rdb.Unwatch("system.router_ready", token_ss)
            end)
        end
    end, true)
    token_imei = rdb.Watch("wwan.0.imei", function(key, value)
        if value and #value > 0 then
            schedule.Event(0, function()
                event.FireOnceOnly("GotIMEI", value)
                rdb.Unwatch("wwan.0.imei", token_imei)
            end)
        end
    end, true)
    token_time = rdb.Watch("system.time.source", function(key, value)
        if value == "network" then
            schedule.Event(0, function()
                event.FireOnceOnly("GotNetworkTime")
                rdb.Unwatch("system.time.source", token_time)
            end)
        end
    end, true)
end)
