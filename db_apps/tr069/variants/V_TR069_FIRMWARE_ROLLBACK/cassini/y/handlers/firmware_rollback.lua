--[[
This script handles objects/parameters under Device.Services.X_<VENDOR>_NetworkConfig.

  FirmwareRollback
  FirmwareRollbackMinVersion

  Requirement description:
    The firmware downgrade or the firmware rollback solution is
    to bring the capability of replacing the operational software
    to the one that has been previously installed.
    The principal design is to replace the operational software volume
    with the rescue software volume that (usually) has an older version.

  TR069 parameter details:
    * Device.Services.X_<VENDOR>_NetworkConfig.FirmwareRollback
      If set to true, start the firmware rollback procedure with min_version validation.

      Default value: false
      Possible values: true, false

    * Device.Services.X_<VENDOR>_NetworkConfig.FirmwareRollbackMinVersion
      Defines the minimum software version, which supports the partial restore.

Copyright (C) 2020 Casa Systems. Inc.
--]]

require("Daemon")
require("handlers.hdlerUtil")
require("Logger")

local logSubsystem = "Firmware_rollback"
Logger.addSubsystem(logSubsystem, 'debug') -- TODO: change level to 'notice' once debugged

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. '.Services.' .. xVendorPrefix .. '_NetworkConfig.'

local function start_rollback()
    local ret = os.execute("abctl --swap_active")
    if ret == 0 then -- rollback success, reboot device.
        luardb.set("service.system.reset", "1")
    end
end

-- Validation check for version number
-- Valid format: blank or A.B.C.D(A, B, C, D is unsignd integer)
-- Return: true or false
local function is_valid_min_version(val)
    if type(val) == "string" and (val == "" or string.match(val, "%d+%.%d+%.%d+%.%d+")) then
        return true
    end
    return false
end

-- Check between min_version and firmware version on inactive slot.
-- * min_version      * version of inactive_slot
--   blank              valid/invalid           ==> Allowed rollback always.
--   valid(A.B.C.D)     valid(E.F.G.H)          ==> Rollback when "E.F.G.H" > "A.B.C.B"
--   valid              invalid                 ==> Not allowed rollback because inactive slot seems not have rollback functionality.
--
-- Return:
--   true: rollback allowed.
--   false: otherwise
local function is_rollback_valid ()
    local ver_min = luardb.get("tr069.firmware_rollback.min_version") or ""
    if ver_min == "" then
        return true
    end

    local ver_inactive = Daemon.readCommandOutput("abctl --get_fw_version inactive") or ""
    local m1, m2, m3, m4 = string.match(ver_min, "(%d+)%.(%d+)%.(%d+)%.(%d+)")
    local i1, i2, i3, i4 = string.match(ver_inactive, "(%d+)%.(%d+)%.(%d+)%.(%d+)")

    if i1 and m1 then
        m1, m2, m3, m4 = tonumber(m1), tonumber(m2), tonumber(m3), tonumber(m4)
        i1, i2, i3, i4 = tonumber(i1), tonumber(i2), tonumber(i3), tonumber(i4)

        if i1 > m1 then return true end
        if i1 == m1 and i2 > m2 then return true end
        if i1 == m1 and i2 == m2 and i3 > m3 then return true end
        if i1 == m1 and i2 == m2 and i3 == m3 and i4 >= m4 then return true end
    end
    return false
end

return {
    -- This is a rollback prodecure trigger.
    -- If set to 1, must start the firmware rollback procedure.
    --
    -- bool : readwrite
    -- Default Value: 0
    -- Available Value: [1 | 0]
    [subRoot .. 'FirmwareRollback'] = {
        get = function(node, name)
            return 0, "0"
        end,
        set = function(node, name, value)
            if value == "1" then
                if  is_rollback_valid() and
                    client:isTaskQueued('sessionDeferred', start_rollback) ~= true then
                    client:addTask('sessionDeferred', start_rollback, false)
                else
                    return CWMP.Error.InvalidArguments
                end
            end
            return 0
        end
    },

      -- Defines the minimum software version, which supports the firmware rollback.
      --
      -- string : readwrite
      -- Default Value: ""
      -- Available Value: ["A.B.C.D" | ""]
      --  * "A.B.C.D": A is most significant number and D is least significant number.
      --  * "": It allows an unconditional rollback.
      -- Involved RDB variable: "tr069.firmware_rollback.min_version"
    [subRoot .. 'FirmwareRollbackMinVersion'] = {
        get = function(node, name)
            return 0, luardb.get("tr069.firmware_rollback.min_version") or ""
        end,
        set = function(node, name, value)
            if not is_valid_min_version(value) then
                return CWMP.Error.InvalidParameterValue
            end
            luardb.set("tr069.firmware_rollback.min_version", value)
            return 0
        end
    },
}
