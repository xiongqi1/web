----
-- Handle Reverse Power Unit dynamic objects/parameters.
--
-- Copyright (C) 2017 NetComm Wireless Limited.
----

require("handlers.hdlerUtil")
require('Daemon')
require('luardb')

------------------global variables----------------------------
local subRoot = conf.topRoot .. '.X_NETCOMM.ReversePower.'

return {
    [subRoot .. 'LastChange'] = {
        get = function(node, name)
            val = hdlerUtil.determineLastChange('system.reversepower.lastchange')
            if val == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not read uptime for node " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },
    [subRoot .. 'Stats.Total.TotalEstablished'] = {
        get = function(node, name)
            val = hdlerUtil.determineTotalEstablishedTime()
            if val == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not read Total Established time for node " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },
}
