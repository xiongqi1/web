--[[
This script handles parameters under Device.Services.X_<VENDOR>_NetworkConfig.IPHandover.
Copyright (C) 2021 Casa Systems. Inc.
--]]

require("Daemon")
require("handlers.hdlerUtil")

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"
local subRoot = conf.topRoot .. '.Services.' .. xVendorPrefix .. '_NetworkConfig.IPHandover.'

return {
    -- Enable or disable IP handover
    [subRoot .. 'Enable'] = {
        get = function(node, name)
            return 0, luardb.get("sas.config.ip_handover") or "0"
        end,
        set = function(node, name, value)
            if value == "1" or value == "0" then
                luardb.set("sas.config.ip_handover", value)
                if value == "0" then
                    luardb.set("service.ip_handover.enable", value)
                end
                return 0
            end
            return CWMP.Error.InvalidArguments
        end
    },
}
