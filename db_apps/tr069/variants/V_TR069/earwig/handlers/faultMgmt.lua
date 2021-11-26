--[[
This script handles objects/parameters under Device.FaultMgmt.

  SupportedAlarm.{i}.
  CurrentAlarm.{i}.
  HistoryEvent.{i}.
  ExpeditedEvent.{i}.
  QueuedEvent.{i}.

Copyright (C) 2018 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require("Daemon")
require("Logger")

------------------global variables----------------------------
local logSubsystem = 'FaultMgmt'
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem(logSubsystem)

local subRoot = conf.topRoot .. '.FaultMgmt.'

local tr069Root = 'tr069.FaultMgmt.'
local tr069EventRoot = tr069Root .. 'Event.'
local tr069SupportedAlarmRoot = tr069Root .. 'SupportedAlarm.'

------------------------------------------------------------
------------------------------------------------------------

function get_event_key(node, event_key)
    -- Read event ID value
    local actual_event_id = luardb.get(node.parent.rdbKey .. '.' .. node.parent.name .. '._event_id')
    if actual_event_id == nil then
        return nil
    end

    -- Read key value
    local actual_key = tr069EventRoot .. actual_event_id .. '.' .. event_key
    Logger.log('FaultMgmt', 'debug', node.name .. " => " .. (actual_key or "(nil)"))
    return luardb.get(actual_key)
end

------------------------------------------------------------
------------------------------------------------------------

return {
    -- Ref to Event
    [subRoot .. 'CurrentAlarm|HistoryEvent|ExpeditedEvent|QueuedEvent.*.AlarmIdentifier|ManagedObjectInstance|NotificationType|PerceivedSeverity|AdditionalText|AdditionalInformation|EventTime|AlarmChangedTime'] = {
        get = function(node, name)
            -- The node is not statically associated with a particular rdbKey as _event_id may change
            local val
            if node.name == 'AlarmChangedTime' then
                val = get_event_key(node, 'EventTime')
            else
                val = get_event_key(node, node.name)
            end
            Logger.log('FaultMgmt', 'debug', name .. ": " .. tostring(val))
            return 0, val or ""
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    -- Ref to SupportedAlarm
    [subRoot .. 'CurrentAlarm|HistoryEvent|ExpeditedEvent|QueuedEvent.*.EventType|ProbableCause|SpecificProblem'] = {
        get = function(node, name)
            local supported_alarm_id = get_event_key(node, '_event_type_id')
            Logger.log('FaultMgmt', 'debug', "supported_alarm_id: " .. (supported_alarm_id or "(nil)"))
            if supported_alarm_id == nil then
                return 0, ""
            end

            local rdb_key = tr069SupportedAlarmRoot .. supported_alarm_id .. '.' .. node.name
            Logger.log('FaultMgmt', 'debug', name .. " => " .. (rdb_key or "(nil)"))
            local val = luardb.get(rdb_key)
            Logger.log('FaultMgmt', 'debug', name .. ": " .. tostring(val))
            return 0, val or ""
        end,
        set = CWMP.Error.funcs.ReadOnly
    },
}
