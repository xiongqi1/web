--[[
This script handles objects/parameters under Device.Celluar.X_<VENDOR>_BandLocking.

  CurrentBands
  LastReqResult
  LockingBands
  BandLockingType

  TR069 parameter details:
    * Device.Celluar.X_<VENDOR>_BandLocking.CurrentBands
      Current preferred band list on modem in integer list format.
      Ex) LTE:[1:2:3:4:5:8:66:71],NR5G:[77:78]

    * Device.Celluar.X_<VENDOR>_BandLocking.LastReqResult
      The result of the last Band Locking request.
      Ex) Result=[done],Type=[Force],LockingBandList=[{Applied band list}]

    * Device.Celluar.X_<VENDOR>_BandLocking.LockingBands
      Band list to lock in integer list format.
      Ex) LTE:[1:2:3:4:5:8:66:71],NR5G:[77:78]

    * Device.Celluar.X_<VENDOR>_BandLocking.BandLockingType ["Force"|"Normal"]
      This is a trigger to set modem preferred bands with
      Device.Celluar.X_<VENDOR>_BandLocking.LockingBands.

      - Force: The bands passed down are applied to the modem,
               the Power-Up check is set to No Change,
               effectively making the change immediately permanent.
      - Normal:(This is available for bellca V_CUSTOM_FEATURE_PACK only)
                The bands passed down are applied to the modem,
                and a 2 minute timer is started.
                If Full Service is achieved during the 2 minute period,
                the band setting becomes persistent (= the Last Good setting).
                If Full Service is not archived after the 2 minute period,
                the Last Good band setting should be restored.
                If the ODU is rebooted during the 2 minute period the Last Good band setting should be restored.
                There shall be a two minute dwell time where the device will look for service
                after the bands are changed in Normal activity.
                If Full Service is achieved, then the bands set are saved as the Last Good Setting,
                and the Power-Up check is set to No Change.

Copyright (C) 2021 Casa Systems. Inc.
--]]

require("CWMP.Error")

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. ".Cellular." .. xVendorPrefix .. "_BandLocking."

local reqLockingBand = ""
local lastPassedBand = ""
local lastReqResult = ""
local validBandListTable = {}

local validBandLockingTypes = { Force = true }

if conf.enableRevertBand == true then
    validBandLockingTypes.Normal = true
end

------------------local function prototype------------------
local setBandLocking
local hexStrToIntList
local cvtHexBandListTo
local positiveDivision
local intListToHexStr
local isValidBandListParameter
------------------------------------------------------------

-- This list represents
-- 1. the number of characters of each RAT hexmask
-- 2. available RAT names in this data model.
local hexStrLenList = { LTE = 32, NR5G = 64, NR5GNSA = 64, NR5GSA = 64 }

setBandLocking = function (task)
    local lockingType = task.data

    -- requested locking band is not valid.
    if reqLockingBand == "" then
        return
    end

    local lockingBandHexmask = ""
    local isValidBandList = true
    local bandList = string.explode(reqLockingBand, ',')
    for _, v in pairs(bandList) do
        local bandName, bandIntList = string.match(v, "([%a%d]+):%[([:%d]*)%]")
        if not bandName then
            isValidBandList = false
            break
        end

        if hexStrLenList[bandName] then
            local hexMaskStr = intListToHexStr(bandIntList:explode(':'), hexStrLenList[bandName])
            if hexMaskStr then
                lockingBandHexmask = string.format("%s%s:%s", (#lockingBandHexmask == 0 and '' or lockingBandHexmask .. ','),  bandName, hexMaskStr)
            else
                isValidBandList = false
                break
            end
        end
    end

    local status = "[error]"
    luardb.set("wwan.0.currentband.revert_selband.last_reverted_band", "") -- clear last reverted band status.

    if isValidBandList then
        if lockingType == "Normal" then
            -- Backup last good band
            local last_good_band = luardb.get("wwan.0.currentband.revert_selband.last_good_band")
            luardb.set("wwan.0.currentband.revert_selband.last_good_setting", last_good_band)
            luardb.set("wwan.0.currentband.revert_selband.mode", "last_good_setting")
        else
            luardb.set("wwan.0.currentband.revert_selband.last_good_setting", "")
            luardb.set("wwan.0.currentband.revert_selband.mode", "no_change")
        end

        luardb.set("wwan.0.currentband.cmd.status", "")
        luardb.set("wwan.0.currentband.cmd.param.band.hexmask", lockingBandHexmask)
        luardb.set("wwan.0.currentband.cmd.command", "set_hexmask")

        local timeout = 5
        status = luardb.get("wwan.0.currentband.cmd.status")
        while timeout > 0 and status == "" do
            luardb.wait(1)
            timeout = timeout - 1
            status = luardb.get("wwan.0.currentband.cmd.status")
        end

        if lockingType == "Normal" and status == "[done]" then
            os.execute("systemctl restart band_validation_check.service")
        end

        -- Update "wwan.0.currentband.current_selband.hexmask" with current perferred band list
        luardb.set("wwan.0.currentband.cmd.command", "get")
    end

    lastReqResult = string.format("Result=%s,Type=[%s],LockingBandList=[%s]", status, lockingType, reqLockingBand)
    lastPassedBand = reqLockingBand
    reqLockingBand = ""
end

-- Convert hexmask in string to integer list that has bitmask
--
-- hexStr: Hexmask in string type
--
-- return integer list which represents bitmask of hexStr
--        or empty table if the input hexmask is not valid.
hexStrToIntList = function (hexStr)
    assert(type(hexStr) == 'string', 'Invalid data type, required string type')
    local retT = {}
    local shift = 0
    hexStr = string.gsub(string.trim(hexStr), "^0[xX]", "")
    for w in string.gmatch (string.reverse(hexStr), ".") do
        local n =  tonumber(w, 16)
        if not n then
            return {}
        end
        for i= 1, 4, 1 do
            if (n % 2) == 1 then
                table.insert(retT, i+shift*4)
            end
            n = math.floor(n / 2)
        end
        shift = shift + 1
    end
    return retT
end

-- Convert band list from Hexmask format to integer list format
--
-- hexBandList: Band list in hexmask format
--           Ex)
--            Input: GSM:0x00000000000000000000000000000000,
--                   WCDMA:0x00000000000000000000000000000000,
--                   LTE:0x00000000000000020000060000000842,
--                   NR5G:0x0000000000000000000000000000000000000000000020000000000000000000
-- cvtToTbl: * If true, convert to a table.
--            Ex)
--            Output: {
--                      "LTE" = {"2" = true, "7" = true, "12" = true, "42" = true, "43" = true, "66" = true},
--                      "NR5G" = {"78" = true}
--                    }
--           * Otherwise, convert to a string(default)
--            Ex)
--            Output: LTE:[1:2:3:4:5:8:66:71],NR5G:[77:78]
--
-- return band list string or table with integer list format
--
cvtHexBandListTo = function(hexBandList, cvtToTbl)
    local result = cvtToTbl and {} or ""
    local curBandTbl = string.explode(hexBandList or "", ",")
    for _, v in pairs(curBandTbl) do
        local bandName, bandHexmask = string.match(v, "([%a%d]+):([xX%x]+)")
        if bandName and hexStrLenList[bandName] then
            local intList = hexStrToIntList(bandHexmask)
            if intList then
                if cvtToTbl then
                    result[bandName] = {}
                    table.foreachi(intList, function(_,v) result[bandName][tostring(v)] = true end)
                else
                    result = string.format("%s%s:[%s]", (#result == 0 and '' or result .. ','),  bandName, table.concat(intList, ':'))
                end
            end
        end
    end

    ---[=[ NR5G and NR5GNSA/NR5GSA fields are mutual exclusive. Below is to take out "NR5G" when "NR5GNSA" and "NR5GSA" exist.
    if cvtToTbl then -- result in table
        if result.NR5GNSA and result.NR5GSA then
            result.NR5G = nil
        end
    else -- result in string
        if string.match(result, "NR5GNSA:%[[%d:]*]") and string.match(result, "NR5GSA:%[[%d:]*]") then
            result = result:gsub("NR5G:%[[%d:]*],?", ""):gsub(",$", "")
        end
    end
    --]=]
    return result
end

positiveDivision = function (a, b)
    assert(a>=0 and b >0, "Invalid dividend or divisor")
    local quotient = math.floor(a / b)
    return quotient, a - quotient * b
end

-- Convert integer list to hexmask in string
--
-- intList: integer list that has bitmask of Band hexmask.
-- hexStrLenInChar: the number of characters of output (default: 64)
--
-- return hexmask in string type
--        or nil, if bitmask is out of hexStrLenInChar range.
intListToHexStr = function (intList, hexStrLenInChar)
    assert(type(intList) == "table", "Invalid data type, required table type")
    hexStrLenInChar = tonumber(hexStrLenInChar) or 64
    local hexstr = "0123456789abcdef"
    local retT = {}
    for i = 1, hexStrLenInChar do
        retT[i] = 0
    end
    for _, v in ipairs(intList) do
        local num = tonumber(v)
        if num and num > 0 then
            local quoti, remain = positiveDivision(num-1, 4)
            if retT[quoti+1] ~= nil then
                retT[quoti+1] = retT[quoti+1] + 2^(remain)
            else
                return
            end
        end
    end

    -- Convert decimal to hexadecimal in string.
    for k, v in ipairs(retT) do
        retT[k] = string.sub(hexstr, v+1, v+1)
    end

    return "0x" .. table.concat(retT):reverse()
end

-- Validation check for band list in integer list format
--
-- intBandList: band list in integer list format(string type)
--              Ex) "LTE:[2:07:12:42:43:66],NR5G:[78]"
--
-- return: true/false
isValidBandListParameter = function(intBandList)
    local bandTbl = string.explode(intBandList or "", ",")
    for _, v in pairs(bandTbl) do
        local bandName, bandIntList = string.match(v, "([%a%d]+):%[([:%d]*)%]")
        if bandName and hexStrLenList[bandName] then
            for _, bandNum in pairs(string.explode( bandIntList or "", ":")) do
                -- invalid, if validBandListTable has bandName sub-table but sub-table does not include band number.
                -- tostring(tonumber(bandNum)) conversion is to allow zero appended bandNum.(Ex LTE:[02:07:12:42:43:66],NR5G:[78])
                if tonumber(bandNum) and validBandListTable[bandName] and not validBandListTable[bandName][tostring(tonumber(bandNum))] then
                    return false
                end
            end
        end
    end
    return true
end

return {
    -- string:readonly
    -- Current preferred band list in integer list format.
    -- dynamic handler is used on purpose to avoid setting default value to the rdb variable.
    [subRoot ..  "CurrentBands"] = {
        get = function(node, name)
            return 0, cvtHexBandListTo(luardb.get("wwan.0.currentband.current_selband.hexmask"))
        end,
    };

    -- string:readonly
    -- The result of the last Band Locking request.
    -- Result=[done],Type=[Normal],LockingBandList=[...]
    [subRoot ..  "LastReqResult"] = {
        get = function(node, name)
            local result = ""

            if lastReqResult ~= "" then
                local lastRevertedBand = luardb.get("wwan.0.currentband.revert_selband.last_reverted_band") or ""
                if lastRevertedBand == "" then
                    result = lastReqResult
                else
                    result = string.format("Result=%s,Type=[%s],LockingBandList=[%s]", "[reverted]", "Normal", lastPassedBand)
                end
            end

            return 0, result
        end,
    };

    -- string:readwrite
    -- Band list to lock in integer list format.
    [subRoot ..  "LockingBands"] = {
        get = function(node, name)
            return 0, ""
        end,
        set = function(node, name, value)
            if luardb.get("nit.connected") == "1" then
                return CWMP.Error.InternalError, "The device is in installation procedure: band configuration rejected"
            end

            -- With svn commit rev 101156, the allowed band list changes from "wwan.0.currentband.revert_selband.factory_setting"
            -- to wwan.0.fixed_module_band_list.hexmask/wwan.0.module_band_list.hexmask.
            -- With the change, validBandListTable could not be set in init() function,
            -- if factory default value "wwan.0.fixed_module_band_list.hexmask" does not exist and wmmd does not come up, yet.
            -- So the cvtHexBandListTo() is moved from init() handler to set() handler, which "wwan.0.module_band_list.hexmask" is available in.
            if not next(validBandListTable) then
                validBandListTable = cvtHexBandListTo(luardb.get("wwan.0.fixed_module_band_list.hexmask") or luardb.get("wwan.0.module_band_list.hexmask") or "", true)
            end

            value = string.trim(value)

            if not isValidBandListParameter(value) then
                return CWMP.Error.InvalidParameterValue
            end

            -- To allow a value format extending , validation check is not be done here.
            reqLockingBand = value
            return 0
        end
    };

    -- string:readwrite
    -- Available Value: ["Force"|"Normal"]
    [subRoot ..  "BandLockingType"] = {
        get = function(node, name)
            return 0, ""
        end,
        set = function(node, name, value)
            if luardb.get("nit.connected") == "1" then
                return CWMP.Error.InternalError, "The device is in installation procedure: band configuration rejected"
            end

            if not validBandLockingTypes[value] then
                return CWMP.Error.InvalidParameterValue
            end

            -- Removed queued task in the same session to apply the last setParameterValues value.
            if client:isTaskQueued('postSession', setBandLocking) == true then
                client:removeTask('postSession', setBandLocking)
            end

            -- Band setting should be done after session is fully terminated (postSession),
            -- otherwise current session could be terminated by force.
            client:addTask('postSession', setBandLocking, false, value)
            return 0
        end
    };
}
