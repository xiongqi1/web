--[[
This script handles objects/parameters under Device.Users.

  User.{i}.Username

Copyright (C) 2018 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require("Daemon")
require("Logger")

local logSubsystem = 'User'
Logger.addSubsystem(logSubsystem)

local subRoot = conf.topRoot .. '.Users.'

------------------local variable----------------------------
------------------------------------------------------------

------------------local function----------------------------
------------------------------------------------------------

return {
    [subRoot .. 'User.1.Username'] = {
        get = function(node, name) return 0, 'admin' end,
        set = function(node, name, value)
            if value ~= 'admin' then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Value " .. value .. " is not valid"
            else
                return 0
            end
        end
    },
}

