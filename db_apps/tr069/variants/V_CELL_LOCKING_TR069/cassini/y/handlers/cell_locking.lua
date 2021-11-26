--[[
This script handles objects/parameters under Device.Celluar.X_<VENDOR>_CellLocking.

  CellLockingLTE
  CellLockingNR5G

  TR069 parameter details:
    * Device.Celluar.X_<VENDOR>_CellLocking.CellLockingLTE
      A list of pci/earfcn pairs delimited with semicolon(;)
      To clear all, set empty string.
      Ex) pci:1,earfcn:11;pci:2,earfcn:22;pci:3,earfcn:33

    * Device.Celluar.X_<VENDOR>_CellLocking.CellLockingNR5G
      Single pci/arfcn/scs(NR5G subcarrier spacing[15|30|60|120|240])/band(NR5G SA Band Number) pair
      To clear all, set empty string.
      Ex) pci:100,arfcn:631584,scs:15,band:77

    * Device.Celluar.X_<VENDOR>_CellLocking.AllowedNR5GSABandList
      Allowed NR5G SA band list in string delimited with comma(,)
      Ex) 77,78 or NA

Copyright (C) 2021 Casa Systems. Inc.
--]]

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. ".Cellular." .. xVendorPrefix .. "_CellLocking."

-- Allowed NR5G SA band list.
--  * associative table form -> allowedNR5GSaBandList
--    Ex) {["77"]=true, ["78"]=true}
--  * list in string form -> allowedNR5GSaBandStr
--    Ex) "77,78"
local allowedNR5GSaBandList = nil
local allowedNR5GSaBandStr = nil

------------------local function prototype------------------
local cellLockingRdbRpc
local setCellLockingLTE
local setCellLockingNR5G
local setValidatorLTE
local saBandValidator
local setValidatorNR5G
local buildAllowedNR5GSaBandList
------------------------------------------------------------

-- validataion check for LTE cell lock list
--
-- RETURN: true or false
-- Valid format: pci:PCI1,earfcn:EARFCN1;pci:PCI2,earfcn:EARFCN2;...;pci:PCI3,earfcn:EARFCN3
setValidatorLTE = function(input)
    if not input then return false end

    for _, v in ipairs(string.explode(input, ";")) do
        local pci, earfcn = string.match(v, "pci:(%d+),earfcn:(%d+)")
        if not pci and v ~= "" then
            return false
        end
    end

    return true
end

local validScs = { ["15"] = true, ["30"] = true, ["60"] = true, ["120"] = true, ["240"] = true }

saBandValidator = function(bandNum)
    if not allowedNR5GSaBandList then
        buildAllowedNR5GSaBandList()
    end
    return type(allowedNR5GSaBandList) == 'table'and allowedNR5GSaBandList[tostring(bandNum)]
end

-- validataion check for NR5G SA cell lock list
--
-- RETURN: true or false
-- Valid format: pci:PCI,arfcn:ARFCN,scs:SCS,band:BandNumber
setValidatorNR5G = function(input)
    if not input then return false end
    if input == "" then return true end

    local pci, arfcn, scs, band = string.match(input, "pci:(%d+),arfcn:(%d+),scs:(%d+),band:(%d+)")
    if not pci or not validScs[tostring(scs)] or not saBandValidator(band) then
        return false
    end
    return true
end

-- build a table of allowed NR5G SA band list and string list on cell locking.
--
-- If success to build, allowedNR5GSaBandList and allowedNR5GSaBandStr will be updated with corresponding result.
buildAllowedNR5GSaBandList = function()
    local rdbAllowedBand = luardb.get("wwan.0.fixed_module_band_list") or luardb.get("wwan.0.module_band_list") or ""
    if string.trim(rdbAllowedBand) == "" then -- it does not have allowed band list and wmmd does not come up, yet.
        return
    end

    local retTbl = {}
    local retArray = {}
    for _, v in ipairs(string.explode(rdbAllowedBand, "&")) do
        local bandNum = string.match(v, ".*,NR5G SA BAND (%d+)")
        if bandNum then
            retTbl[bandNum] = true
            table.insert(retArray, bandNum)
        end
    end
    table.sort(retArray, function(a,b) return (tonumber(a) < tonumber(b)) end)
    allowedNR5GSaBandList = retTbl
    allowedNR5GSaBandStr = table.concat(retArray, ",")
end

-- execute and wait result on RDB RPC command to set cell lock
--
-- lockRat: lte or 5g
--
-- RETURN: true or false
cellLockingRdbRpc = function(lockRat, rdbRpcArg)
    luardb.set("wwan.0.cell_lock.cmd.status", "")
    luardb.set("wwan.0.cell_lock.cmd.param.rat", lockRat) -- cell locking type
    luardb.set("wwan.0.cell_lock.cmd.param.lock_list", rdbRpcArg)
    luardb.set("wwan.0.cell_lock.cmd.command", "set")

    local timeout = 5
    status = luardb.get("wwan.0.cell_lock.cmd.status")
    while timeout > 0 and status == "" do
        luardb.wait(1)
        timeout = timeout - 1
        status = luardb.get("wwan.0.cell_lock.cmd.status")
    end

    if status == "[done]" then
        return true
    end
    return false
end

-- callback for LTE cell locking
setCellLockingLTE = function(task)
    cellLockingRdbRpc("lte", task.data)
end

-- callback for NR5G cell locking
setCellLockingNR5G = function(task)
    cellLockingRdbRpc("5g", task.data)
end

return {
    -- string:readonly
    [subRoot ..  "AllowedNR5GSABandList"] = {
        get = function(node, name)
            if not allowedNR5GSaBandStr then
                buildAllowedNR5GSaBandList()
            end
            return 0, allowedNR5GSaBandStr or "NA"
        end,
    };
    -- string:readwrite
    -- Available value format: pci:PCI1,earfcn:EARFCN1;pci:PCI2,earfcn:EARFCN2;...;pci:PCI3,earfcn:EARFCN3
    [subRoot ..  "CellLockingLTE"] = {
        get = function(node, name)
            return 0, luardb.get("wwan.0.modem_pci_lock_list") or "NA"
        end,
        set = function(node, name, value)
            -- Removed queued task in the same session to apply the last setParameterValues value.
            if client:isTaskQueued('postSession', setCellLockingLTE) == true then
                client:removeTask('postSession', setCellLockingLTE)
            end

            if not setValidatorLTE(value) then
                return CWMP.Error.InvalidParameterValue
            end

            -- Cell locking should be done after session is fully terminated (postSession),
            -- Otherwise current session could be terminated by force.
            client:addTask('postSession', setCellLockingLTE, false, value)
            return 0
        end
    };
    -- string:readwrite
    -- Available value format: {"pci":"PCI","arfcn":"ARFCN","scs":"SCS","band":"NR5G_SA_BAND_NUMBER"}
    [subRoot ..  "CellLockingNR5G"] = {
        get = function(node, name)
            -- if wwan.0.modem_pci_lock_list_5g does not exist, then returns "NA"
            -- if wwan.0.modem_pci_lock_list_5g is empty sting, then returns ""
            local retVal = "NA"
            local cellLock = luardb.get("wwan.0.modem_pci_lock_list_5g")
            if cellLock then
                local pci, arfcn, scs, band = string.match(cellLock, "pci:(%d+),arfcn:(%d+),scs:(%d+),band:NR5G SA BAND (%d+)")
                if pci then
                    retVal = string.format("pci:%s,arfcn:%s,scs:%s,band:%s", pci, arfcn, scs, band)
                else
                    retVal = ""
                end
            end
            return 0, retVal
        end,
        set = function(node, name, value)
            -- Removed queued task in the same session to apply the last setParameterValues value.
            if client:isTaskQueued('postSession', setCellLockingNR5G) == true then
                client:removeTask('postSession', setCellLockingNR5G)
            end

            if not setValidatorNR5G(value) then
                return CWMP.Error.InvalidParameterValue
            end

            if value ~= "" then
                local pci, arfcn, scs, band = string.match(value, "pci:(%d+),arfcn:(%d+),scs:(%d+),band:(%d+)")
                value = string.format("pci:%s,arfcn:%s,scs:%s,band:NR5G SA BAND %s", pci, arfcn, scs, band)
            end
            -- Cell locking should be done after session is fully terminated (postSession),
            -- Otherwise current session could be terminated by force.
            client:addTask('postSession', setCellLockingNR5G, false, value)
            return 0
        end
    };
}
