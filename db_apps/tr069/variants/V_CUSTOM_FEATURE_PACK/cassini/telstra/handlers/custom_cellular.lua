--[[
This script handles objects/parameters under Device.Cellular.X_TELSTRA_CELLULAR.

  TAC
  TransmitPower
  RRCState
  5GConnected

  Device.Cellular.X_TELSTRA_CELLULAR.PrimaryCarriers/SecondaryCarriers.#.(Index 1 alias: "LTE", Index 2 alias: "5GNR")
    RSRP
    RSRQ
    SINR
    Band
    Bandwidth
    ChannelRate
    EARFCN
    NR-ARFCN
    ECI
    PCI
    ULEnabled

Copyright (C) 2021 Casa Systems. Inc.
--]]


local subRoot = conf.topRoot .. ".Cellular.X_TELSTRA_CELLULAR."

local pattPriCarriers = string.format("%sPrimaryCarriers%%.(%%d+)%%.%%S*", subRoot)
local pattSecCarriers = string.format("%sSecondaryCarriers%%.(%%d+)%%.%%S*", subRoot)
------------------local function prototype------------------
local is5GNsaConnected
local is5GSaConnected
local is5GConnected
local is5GAvailable
local getLteCellStrength
------------------------------------------------------------


------------------local variable definition ----------------
local g_numOfPriCarrierInst = 2 -- the number of instances of PrimaryCarriers object(1:LTE, 2:NR5G)
local g_numOfSecCarrierInst = 11 -- the number of instances of SecondaryCarriers object(1-4:LTE, 5-11:NR5G)

local lteScellRdbObjConf = {persist = false, idSelection= "smallestUnused"}
local lteScellRdbObj = rdbobject.getClass("wwan.0.system_network_status.lte_ca_scell.list", lteScellRdbObjConf)
------------------------------------------------------------


------------------local function definition ----------------

-- Query the modem is connected with 5G NSA.
is5GNsaConnected = function()
    local curr_so = luardb.get("wwan.0.system_network_status.current_system_so") or ""
    if string.match(curr_so, "5G NSA") then
        return true
    end
    return false
end

-- Query the modem is connected with 5G SA.
is5GSaConnected = function()
    local curr_so = luardb.get("wwan.0.system_network_status.current_system_so") or ""
    if string.match(curr_so, "5G SA") then
        return true
    end
    return false
end

-- Query the modem is connected with 5G network.
is5GConnected = function()
    local curr_so = luardb.get("wwan.0.system_network_status.current_system_so") or ""
    if string.match(curr_so, "5G [N]?SA") then
        return true
    end
    return false
end

-- Query 5G service is available in the current network.
is5GAvailable = function()
    local curr_so = luardb.get("wwan.0.system_network_status.current_system_so") or ""

    -- To synchronize value with Cellular.Interface.CurrentAccessTechnology paramter,
    -- check endc_avail to determin NR5G is up.(refer to cellularDevice.lua)
    local endc_avail = luardb.get("wwan.0.system_network_status.endc_avail") or ""

    if endc_avail == "1" or string.match(curr_so, "5G [N]?SA") then
        return true
    end
    return false
end

-- get LTE rsrp, rsrq of CA Primary or Secondary serving cell
--
-- pci: Physical Cell Id
-- freq: Frequency number
-- return tuple of rsrp, rsrq. If invalid, returns empty strings.
getLteCellStrength = function(pci, freq)
    local rsrp = ""
    local rsrq = ""
    local pci = string.trim(pci)
    local freq = string.trim(freq)

    if pci == "" or freq == "" then
        return rsrp, rsrq
    end

    local prefixPattern = string.format("E,%s,%s,([-.%%d]+),([-.%%d]+)", freq, pci)
    for i, value in hdlerUtil.traverseRdbVariable{prefix="wwan.0.cell_measurement.", suffix="" , startIdx=0} do
        value = string.trim(value)
        rsrp, rsrq = string.match(value, prefixPattern)
        if rsrp then
            break
        end
    end
    return rsrp and rsrp or "" , rsrq and rsrq or ""
end
------------------------------------------------------------

return {
    -- string:readonly
    -- Related Rdb: LTE - wwan.0.radio.information.tac, NR5G - wwan.0.radio_stack.nr5g.tac
    [subRoot ..  "TAC"] = {
        get = function(node, name)
            if is5GSaConnected() then
                return 0, luardb.get("wwan.0.radio_stack.nr5g.tac") or ""
            else
                return 0, luardb.get("wwan.0.radio.information.tac") or ""
            end
        end,
    },

    -- string:readonly
    -- Related Rdb: LTE - wwan.0.system_network_status.tx_pwr, NR5G - wwan.0.radio_stack.nr5g.tx_pwr
    [subRoot ..  "TransmitPower"] = {
        get = function(node, name)
            local retVal = ""
            if is5GSaConnected() then
                retVal = luardb.get("wwan.0.radio_stack.nr5g.tx_pwr") or ""
            else
                retVal = luardb.get("wwan.0.system_network_status.tx_pwr") or ""
            end
            return 0, retVal ~= "" and retVal .. " dbm" or ""
        end,
    },

    -- string:readonly
    -- Related Rdb: LTE - wwan.0.radio_stack.rrc_stat.rrc_stat, NR5G - Not available
    [subRoot ..  "RRCState"] = {
        get = function(node, name)
            if is5GSaConnected() then
                return 0, "" -- TODO:: Need NR5G rdb variable.
            else
                return 0, luardb.get("wwan.0.radio_stack.rrc_stat.rrc_stat") or ""
            end
        end,
    },

    -- bool:readonly
    [subRoot ..  "5GConnected"] = {
        get = function(node, name)
            return 0, is5GConnected() and "1" or "0"
        end,
    },

    -- PrimaryCarriers root object
    [subRoot ..  "PrimaryCarriers"] = {
        init = function(node, name, value)
            for idx=1, g_numOfPriCarrierInst do
                node:createDefaultChild(idx)
            end
            return 0
        end,
    },

    [subRoot .. "PrimaryCarriers.*"] = {
        init = function(node, name, value)
            return 0
        end,
    },

    [subRoot ..  "PrimaryCarriers.*.*"] = {
        get = function(node, name)
            local dataModelIdx = tonumber(string.match(name, pattPriCarriers)) or 0  -- 1: "LTE", 2: "NR5G"
            local paramName = string.match(name, ".+%.([^%.]+)$")

            if dataModelIdx < 1 then
                return CWMP.Error.InvalidParameterName
            end

            local retVal = "N/A"

            -- Not Available, yet.
            if paramName == "SINR" or paramName == "ECI" or paramName == "ChannelRate" then
                return 0, retVal
            end

            if dataModelIdx == 1 and not is5GConnected() then -- LTE
                -- Field formation for wwan.0.cell_measurement.0 => E,earfcn,pci,rsrp,rsrq
                -- string.match pattern for wwan.0.cell_measurement.0 => E,%d+,%d+,%-[%.%d]+,%-[%.%d]+
                local servingCellInfo = luardb.get("wwan.0.cell_measurement.0") or ""
                local earfcn, pci, rsrp, rsrq = string.match(servingCellInfo, "E,(%d+),(%d+),(%-[%.%d]+),(%-[%.%d]+)")
                if paramName == "RSRP" then
                    retVal = rsrp or ""
                elseif paramName == "RSRQ" then
                    retVal = rsrq or ""
                elseif paramName == "Band" then
                    retVal = luardb.get("wwan.0.system_network_status.current_band") or ""
                elseif paramName == "Bandwidth" then
                    -- Ex) wwan.0.system_network_status.current_rf_bandwidth => LTE 1.4MHz, LTE 10MHz, LTE 20MHz
                    local lteBandwidth = luardb.get("wwan.0.system_network_status.current_rf_bandwidth") or ""
                    retVal = string.match(lteBandwidth, "LTE ([%.%d]+MHz)") or ""
                elseif paramName == "EARFCN" then
                    retVal = earfcn or ""
                elseif paramName == "PCI" then
                    retVal = pci or ""
                end
            elseif dataModelIdx == 2 and is5GConnected() then -- NR5G
                if paramName == "RSRP" then
                    retVal = luardb.get("wwan.0.radio_stack.nr5g.rsrp") or ""
                elseif paramName == "RSRQ" then
                    retVal = luardb.get("wwan.0.radio_stack.nr5g.rsrq") or ""
                elseif paramName == "Band" then
                    retVal = "N/A"
                elseif paramName == "Bandwidth" then
                    retVal = luardb.get("wwan.0.radio_stack.nr5g.bw") or ""
                elseif paramName == "NR-ARFCN" then
                    retVal = luardb.get("wwan.0.radio_stack.nr5g.arfcn") or ""
                elseif paramName == "PCI" then
                    retVal = luardb.get("wwan.0.radio_stack.nr5g.pci") or ""
                end
            end

            return 0, retVal
        end,
    },

    -- SecondaryCarriers root object
    [subRoot ..  "SecondaryCarriers"] = {
        init = function(node, name, value)
            for idx=1, g_numOfSecCarrierInst do
                node:createDefaultChild(idx)
            end
            return 0
        end,
    },

    [subRoot .. "SecondaryCarriers.*"] = {
        init = function(node, name, value)
            return 0
        end,
    },

    [subRoot ..  "SecondaryCarriers.*.*"] = {
        get = function(node, name)
            local dataModelIdx = tonumber(string.match(name, pattSecCarriers)) or 0  -- 1-4:LTE, 5-11:NR5G
            local paramName = string.match(name, ".+%.([^%.]+)$")

            if dataModelIdx < 1 then
                return CWMP.Error.InvalidParameterName
            end

            local retVal = "N/A"

            -- Not Available, yet.
            if paramName == "SINR" or paramName == "ECI" or paramName == "ChannelRate" then
                return 0, retVal
            end

            if dataModelIdx > 0 and dataModelIdx <= 4 and not is5GConnected() then -- LTE
                local ret, inst = pcall(lteScellRdbObj.getById, lteScellRdbObj, dataModelIdx)
                if ret and inst then
                    if paramName == "RSRP" then
                        local rsrp, rsrq = getLteCellStrength(inst.pci, inst.freq)
                        retVal = rsrp
                    elseif paramName == "RSRQ" then
                        local rsrp, rsrq = getLteCellStrength(inst.pci, inst.freq)
                        retVal = rsrq
                    elseif paramName == "Band" then
                        retVal = inst.band or ""
                    elseif paramName == "Bandwidth" then
                        retVal = inst.bandwidth or ""
                    elseif paramName == "EARFCN" then
                        retVal = inst.freq or ""
                    elseif paramName == "PCI" then
                        retVal = inst.pci or ""
                    elseif paramName == "ULEnabled" then
                        retVal = inst.ul_configured == "1" and "1" or "0"
                    end
                end
            elseif dataModelIdx == "2" and is5GConnected() then -- NR5G
                --TODO:: Back-end is not available, yet.
            end

            return 0, retVal
        end,
    },

}
