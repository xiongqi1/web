--[[
This script handles Reserve Factory Reset parameters under

  Device.X_<VENDOR>_ReserveFactoryReset

The purpose of this script is to reserve factory reset so the device
restores its config to factory default status on next booting. It is
useful to provide backward compatibility where there are many difference
in config between old and new f/w revisions.

Copyright (C) 2021 Casa Systems. Inc.
--]]

require('CWMP.Error')

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. "."

return {
    -- boolean:readwrite
    -- Reserve factory reset by setting an environment variable.
    -- This is useful when upgrading or downgrading f/w of which
    -- version is not compatible with current version so factory
    -- reset is needed after flashing f/w.
    [subRoot .. xVendorPrefix .. "_ReserveFactoryReset"] = {
        get = function(node, name)
            local val = Daemon.readCommandOutput("environment -r FACTORY_RESET") or ""
            return 0, (val == 'NEEDED') and '1' or '0'
        end,
        set = function(node, name, value)
            if not value then
                return CWMP.Error.InvalidParameterValue
            end
            local cmd
            if value == '1' then
                cmd = 'environment -w FACTORY_RESET NEEDED'
            else
                cmd = "environment -d FACTORY_RESET"
            end
            Logger.log('Parameter', 'error', 'cmd = '..cmd)
            local status = os.execute(cmd);
            if status == 0 then
                return 0;
            end
            Logger.log('Parameter', 'error', 'failed to run ' ..cmd)
            return CWMP.Error.InternalError
        end
   }
}
