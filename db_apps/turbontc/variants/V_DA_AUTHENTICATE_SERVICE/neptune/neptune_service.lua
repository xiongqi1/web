--[[
    Service Map overwrite for Neptune

    Copyright (C) 2020 Casa Systems.
--]]

require 'luardb'

-- set RDB if the value has changed
local function rdbSetIfChg(rdbKey, rdbVal)
    if luardb.get(rdbKey) ~= rdbVal then
        logInfo(string.format('set RDB %s = %s', rdbKey, rdbVal))
        luardb.set(rdbKey, rdbVal)
    end
end

-- Activate debug service. For Neptune, debug service means local ssh
local function activateDebug(enable)
    logNotice((enable and '' or 'de') .. 'activating Neptune debug')
    rdbSetIfChg('service.ssh.enable', enable and '1' or '0')
    rdbSetIfChg('admin.local.ssh_enable', enable and '1' or '0')
    return true
end

local module = {}

-- Initialisation function.
--
-- @param serviceMap The base serviceMap table to be overwritten.
--     See serviceMap in common.lua for detailed definition.
function module.init(serviceMap)
    assert(serviceMap and type(serviceMap) == 'table',
           'serviceMap is not passed in properly')
    logInfo('overwrite serviceMap.debug')
    serviceMap.debug = {
        roles = {'support', 'develop'}, func = activateDebug
    }
end

return module
