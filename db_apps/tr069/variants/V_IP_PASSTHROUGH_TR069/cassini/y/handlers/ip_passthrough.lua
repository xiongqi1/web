--[[
This script handles objects/parameters for IP Passthrough service per APN profile

  TR069 parameter details:
    * Device.Cellular.AccessPoint.{i}.X_<VENDOR>.IPPassthrough.Enable
      Boolean to enable/disable IP passthrough per APN profile

Copyright (C) 2021 Casa Systems. Inc.
--]]


-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. ".Cellular.AccessPoint.*." .. xVendorPrefix .. ".IPPassthrough."
local regexIdx = "Device%.Cellular%.AccessPoint%.(%d+)%.[_%a][-_%w]+%.IPPassthrough.Enable"

return {
    -- bool:readwrite
    -- rdb variable: link.profile.#.ip_handover.enable
    [subRoot ..  "Enable"] = {
        get = function(node, name)
            local instIdx = tonumber(string.match(name, regexIdx))
            if not instIdx then
                return CWMP.Error.InvalidParameterName
            end
            local en = luardb.get(string.format("link.profile.%d.ip_handover.enable", instIdx))

            return 0, en == "1" and "1" or "0"
        end,
        set = function(node, name, value)
            if value ~= "0" and value ~= "1" then
                return CWMP.Error.InvalidParameterValue
            end

            local instIdx = tonumber(string.match(name, regexIdx))
            if not instIdx then
                return CWMP.Error.InvalidParameterName
            end

            luardb.set(string.format("link.profile.%d.ip_handover.enable", instIdx), value)
            return 0
        end
    };
}
