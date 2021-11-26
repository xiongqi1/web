-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------------------------------------------
-- Other required RGIF modules
-----------------------------------------------------------------------------------------------------------------------
local rgif_config = require("rgif_config")
local rgif_utils = require("rgif_utils")

local EventHandler = class("EventHandler", turbo.websocket.WebSocketHandler)

local payloadType = {} -- Payload type
payloadType.CONNECT = "Connect" -- Message sent right after handshake
payloadType.STATECHANGE = "StateChange" -- Message for state change event
payloadType.HEARTBEAT = "Heartbeat" -- Message for heartbeat event
payloadType.IMSSTATUS = "IMSRegStatusChange" -- Message for IMS registration status
payloadType.NETWORKSERVICESTATUSCHANGE = "NetworkServiceStatusChange" -- Message for network service status change event

EventHandler._protocols = {"v1.notification.turbontc"} -- protocols supported currently
EventHandler._protocol = "v1.notification.turbontc" -- selected protocol after negotiation. Default is v1.
EventHandler._heatbeatInterval = luardb.get("service.turbontc.event.interval") -- default heartbeat inteval in msec

local data_pdn_profile = "link.profile.1"

local function imsRegErrStatus()
    local reg = luardb.get("wwan.0.ims.register.reg_stat")
    local code = luardb.get("wwan.0.ims.register.reg_failure_error_code")
    if reg ~= "not registered" then
        return 0 -- Registered or Registering
    elseif code == nil or code == "0" or code == "" then
        return 1 -- Unregistered
    else
        return 2 -- Registration failure
    end
end

-- Watched events tables, organized by version.
EventHandler.watchedEvents = { ["v1.notification.turbontc"] =
    {
        ["Rebooting"] = {trigger = "service.system.is_rebooting"},
        ["Upgrading"] = {trigger = "service.system.upgrade"},
        ["Factory reset"] = {trigger = "service.system.factory_reset"},
        ["Establishing PDN"] = {trigger = data_pdn_profile..".connect_progress", expected = "establishing"},
        ["PDN Established"] = {trigger = data_pdn_profile..".connect_progress", expected = "established"},

        -- Voice service specific events
        ["End of VoLTE call"] = {trigger = "voicecall.statistics.end_of_call", voice=true},
        ["Registered"] = {trigger = "wwan.0.ims.register.reg_stat", payloadType=payloadType.IMSSTATUS, expected="registered", voice=true},
        ["Registering"] = {trigger = "wwan.0.ims.register.reg_stat", payloadType=payloadType.IMSSTATUS, expected="registering", voice=true},
        ["Registration failure"] = {trigger = "wwan.0.ims.register.reg_stat", payloadType=payloadType.IMSSTATUS, expected=function(_) return imsRegErrStatus() == 2 end, voice=true},
        ["Unregistered"] = {trigger = "wwan.0.ims.register.reg_stat", payloadType=payloadType.IMSSTATUS, expected=function(_) return imsRegErrStatus() == 1 end, voice=true},

        -- NetworkServiceStatusChange events

        ["No service"] = {
            trigger = "wwan.0.system_network_status.system_mode",
            payloadType=payloadType.NETWORKSERVICESTATUSCHANGE,
            expected="no service",
        },

        ["Limited service"] = {
            trigger = "wwan.0.system_network_status.system_mode",
            payloadType=payloadType.NETWORKSERVICESTATUSCHANGE,
            expected="Limited service",
        },

        ["Limited regional service"] = {
            trigger = "wwan.0.system_network_status.system_mode",
            payloadType=payloadType.NETWORKSERVICESTATUSCHANGE,
            expected="limited regional service",
        },

        ["Power save"] = {
            trigger = "wwan.0.system_network_status.system_mode",
            payloadType=payloadType.NETWORKSERVICESTATUSCHANGE,
            expected="power save",
        },

        ["LTE"] = {
            trigger = "wwan.0.system_network_status.system_mode",
            payloadType=payloadType.NETWORKSERVICESTATUSCHANGE,
            expected="lte",
        },

    },
}


function EventHandler:lastRebootReason()
    -- Currently "warm-restart", "cold-restart" and "upgrading" and "factory-reset" are supported
    -- "service.syslog.checklastreboot" is used to track the reboot reason in our system, which is set by syslogd.template when router is up.
    -- It contains two parts - reason of software, which is set through RDB "service.syslog.lastreboot", which would be cleared after reading
    -- and reason of hardware which is read from the reset controller.
    --
    -- For mdm9640, hw restart reason could not be fetched (or at least not known), so it is ignored.
    --
    -- Therefore, to make reboot reason shown corrcetly, the reboot reason has to be set in the RDB vairable - "service.syslog.lastreboot".
    -- This is consisted of the following two actions.
    -- 1) When system is reset via system reboot command directly or indirectly, rc.STOP script will
    -- set it as "Warm-resetart" if the value is empty.
    -- 2) The upgrading process will set it as "Upgrade" when upgrade is finished and before rebooting.
    --
    -- Therefore, if "service.syslog.lastreboot" value is empty, it means a cold restart. This is incorrect if the system is reset by pressing the
    -- reset button and this problem can only be resolved there is hardware coldreboot register support.
    if _G._lastRebootReason then -- If the reason has been acquired, just return it.
        return _G._lastRebootReason
    end

    _G._lastRebootReason = luardb.get("service.system.last_reboot_cause") -- Load reason from RDB if it has been resolved.
    if _G._lastRebootReason then
        return _G._lastRebootReason
    end

    lastreboot = luardb.get("service.syslog.checklastreboot") -- The value of this RDB is a string like "Previous reboot reason: sw() hw(powerup)".
    local _,_,swr,hwr = lastreboot:find("Previous reboot reason: sw%((.*)%) hw%((.*)%)")
    if swr ~= "" then
        _G._lastRebootReason = swr
    else
        factory = luardb.get("service.system.factory_reset")
        if factory == "2" then -- "2" is the initial status after factory reset
            luardb.set("service.system.factory_reset", "0")
            _G._lastRebootReason = "Factory-Reset"
        else -- If nothing specified in sw reboot reason, it is a cold restart
            _G._lastRebootReason = "Cold-restart"
        end
    end

    luardb.set("service.system.last_reboot_cause", _G._lastRebootReason)

    return _G._lastRebootReason
end


function EventHandler:_make_payload(msgtype, msg)
    local payload = {Type = msgtype, }
    if msgtype == payloadType.HEARTBEAT then
        return payload
    elseif msgtype == payloadType.CONNECT then
        payload.RebootCause = msg
    elseif msgtype == payloadType.STATECHANGE then
        payload.State = msg
    elseif msgtype == payloadType.IMSSTATUS then
        payload.State = msg
    elseif msgtype == payloadType.NETWORKSERVICESTATUSCHANGE then
        payload.State = msg
    else
        error("No such payloadType: "..msgtype)
    end
    return payload
end

-- Return a selected subprotocol
function EventHandler:subprotocol(protocols)
    for _, p in pairs(protocols) do
        for _, s in pairs(self._protocols) do
            if p == s then
                self._protocol = s
                return s
            end
        end
    end
    return nil
end

function EventHandler:prepare(msg)
    if _G._currentConnection then -- Only one event server connection allowed
        logNotice("Closing the current open connection.")
        _G._prevConnection = _G._currentConnection -- Save the current connection to previous one
        _G._prevConnection:close()
        _G._currentConnection = nil
    end
end

-- Nothing to do here for our event server
function EventHandler:on_message(msg)
end

function EventHandler:_heartbeat()
    local payload = self:_make_payload(payloadType.HEARTBEAT)
    pcall(function() self:ping(turbo.escape.json_encode(payload), nil, nil) end)
end

function EventHandler:open()
    _G._currentConnection = self.stream -- Save the current stream
    local payload = self:_make_payload(payloadType.CONNECT, self:lastRebootReason())
    -- Send the first connect message firstly
    self:write_message(payload)
    -- Subscribe the RDBs to events
    local watchedEvents = self.watchedEvents[self._protocol]
    local function _send_event(rdbname, v)
        return function()
            val = luardb.get(rdbname)
            for evt, evtTable in pairs(v)
            do
                if (evtTable.expected == nil) or (type(evtTable.expected) == "string" and (evtTable.expected == val)) or -- Value is matched or there is no value required
                    (type(evtTable.expected) == "function" and evtTable.expected(val)) then -- A condition function returns true for the value
                    logDebug("Event [" .. evt .. "] detected")
                    local pt = evtTable.payloadType or payloadType.STATECHANGE -- If there is no payloadType define, the default is "STATECHAGNE"
                    pcall(function() self:write_message(self:_make_payload(pt, evt)) end)
                end
            end
        end
    end
    local watchedRdbs = {}
    -- Go through watchedEvents table to collect all RDBs for events
    -- The reason for doing this is that there might be different events for one subscribed RDB variable.
    for k,v in pairs(watchedEvents) do
        -- Only add voice service specific events if we are configured for them
        if not v.voice or rgif_config.has_voice then
            if watchedRdbs[v.trigger] == nil then
                watchedRdbs[v.trigger] = {}
            end
            watchedRdbs[v.trigger][k] = v
        end
    end
    for k,v in pairs(watchedRdbs) -- Now register the events
    do
        for e,_ in pairs(v)
        do
            logInfo("Adding watch to "..k.." for event ["..e.."]")
        end
        luardb.watch(k, _send_event(k,v))
    end
    -- Set up heartbeat callback
    self._intervalIndex = _G.io_loop_instance:set_interval(self._heatbeatInterval, self._heartbeat, self)
    -- Update client address and trigger any required actions
    rgif_utils.updateClientAddress(self.request.remote_ip or self.request.connection.address)
end

function EventHandler:on_close()
    if _G._prevConnection then
        _G._prevConnection = nil
    else
        _G._currentConnection = nil
    end
    _G.io_loop_instance:clear_interval(self._intervalIndex)
    -- Todo: unsubscribe all events. luardb needs to add extra APIs to support unwatch.
end

local module = {}

function module.init(_, handlers) -- This will be called when this module is loaded
    EventHandler:lastRebootReason() -- Get last reboot reason now so that it is ready when this module is loaded
    table.insert(handlers, {"^/notification$", EventHandler})
end

return module
