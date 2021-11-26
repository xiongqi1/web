--[[
This script handles objects/parameters under Device.X_<VENDOR>.Services.MultiLevelFactoryReset

  FactoryResetLevel

  TR069 parameter details:
    * Device.X_<VENDOR>.Services.MultiLevelFactoryReset.FactoryResetLevel
      Factory reset level
      Available value: [factory|carrier|installer]

Copyright (C) 2021 Casa Systems. Inc.
--]]

require("CWMP.Error")

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. "." .. xVendorPrefix .. ".Services.MultiLevelFactoryReset."

local validLevelList = { factory = true, carrier = true, installer = true }

return {
    -- string:readwrite
    -- Available value: [factory|carrier|installer]
    [subRoot ..  "FactoryResetLevel"] = {
        get = function(node, name)
            return 0, luardb.get("service.system.factory.level") or node.default
        end,
        set = function(node, name, value)
            if not validLevelList[value] then
                return CWMP.Error.InvalidParameterValue
            end

            luardb.set("service.system.factory.level", value)
            return 0
        end
    };
}
