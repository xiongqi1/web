--[[
    Service Map overwrite for NIT sensors API

    Copyright (C) 2021 Casa Systems.
--]]

local module = {}

-- Initialisation function.
--
-- @param serviceMap The base serviceMap table to be overwritten.
--     See serviceMap in common.lua for detailed definition.
function module.init(serviceMap)
    assert(serviceMap and type(serviceMap) == 'table',
           'serviceMap is not passed in properly')
    logInfo('add serviceMap.sensors')
    serviceMap['sensors'] = { roles = {'install', 'support'}, func = nil }
end

return module
