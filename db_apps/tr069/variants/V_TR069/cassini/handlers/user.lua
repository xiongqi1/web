--[[
This script handles objects/parameters under Device.Users.

  User.{i}.Username
  User.{i}.Password

Copyright (C) 2019 Casa Systems. Inc.
--]]

require("handlers.hdlerUtil")
require("Daemon")
require("Logger")

local logSubsystem = 'User'
Logger.addSubsystem(logSubsystem)

local subRoot = conf.topRoot .. '.Users.'

local userAccounts = luardb.get("admin.user.accounts") or 'root'
local fixedUsername = userAccounts:explode(',')[1]
------------------local variable----------------------------
------------------------------------------------------------

------------------local function----------------------------
------------------------------------------------------------

return {
    [subRoot .. 'User.1.Username'] = {
        get = function(node, name) return 0, fixedUsername end,
        set = function(node, name, value)
            if value ~= fixedUsername then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Value " .. value .. " is not valid"
            else
                return 0
            end
        end
    },

    [subRoot .. 'User.1.Password'] = {
        set = function(node, name, value)
            local mime = require("mime")
            local b64pw = mime.b64(value)
            local cmd = string.format("echo -n '%s' | base64 -d | openssl passwd -stdin -6", b64pw)
            local handle = io.popen(cmd, 'r')
            if not handle then
                return CWMP.Error.InternalError, "Error: Unable to generate hash for the password"
            end
            local enc_pw = handle:read('*line')
            handle:close()
            luardb.set("admin.user."..fixedUsername..".encrypted", enc_pw)
            return 0
        end
    },
}

