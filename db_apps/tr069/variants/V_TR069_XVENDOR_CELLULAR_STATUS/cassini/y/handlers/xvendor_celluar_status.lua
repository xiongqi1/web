--[[
This script handles objects/parameters under Device.Cellular.X_<VENDOR>_Status.

  IMEISV
  UpperLayerIndicator
  CurrentBand
  5GConnected

  Device.Cellular.X_<VENDOR>_Status.LTE.
    ECGI
    eNodeBID
    CellID
    PCI
    EARFCN
    RSRP
    RSRQ
    SNR
    CQI
    TAC
    TransmitPower
    RRCState

  Device.Cellular.X_<VENDOR>_Status.NR5G.
    NCGI
    gNodeBID
    gNBCellID
    gNBPCI
    NRARFCN
    RSRP
    RSRQ
    SNR
    NRCQI
    TAC
    TransmitPower
    gNBSSBIndex

Copyright (C) 2021 Casa Systems. Inc.
--]]

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"
local subRoot = conf.topRoot .. ".Cellular." .. xVendorPrefix .. "_Status."

------------------local function prototype------------------
local is5GSaConnected
local isNot5GSaConnected
local is5GConnected
local is4GConnected
local isNot4GConnected
------------------------------------------------------------

------------------local function definition ----------------
-- Query the modem is connected in 5G SA.
is5GSaConnected = function()
    if string.match(luardb.get("wwan.0.system_network_status.current_system_so") or "", "5G SA") then
        return true
    end
    return false
end

-- Query the modem is not connected in 5G SA.
isNot5GSaConnected = function()
    if not is5GSaConnected() then
        return true
    end
    return false
end

-- Query the modem is connected in 5G network.
is5GConnected = function()
    if string.match(luardb.get("wwan.0.system_network_status.current_system_so") or "", "5G [N]?SA") then
        return true
    end
    return false
end

-- Query the modem is connected in 4G network.
is4GConnected = function()
    if string.match(luardb.get("wwan.0.system_network_status.current_system_so") or "", "LTE") then
        return true
    end
    return false
end

-- Query the modem is not connected in 4G network.
isNot4GConnected = function()
    if not is4GConnected() then
        return true
    end
    return false
end
------------------------------------------------------------

------------------local variable definition ----------------
-- rdbName: corresponding rdb variable name
-- dispCond: When the callback function returns true, the value is displayed.
--           Otherwise, node.default is displayed. (Based on WEBUI implementations)
local paramNameAttrPair = {
    LTE = {
        ECGI = {rdbName = "wwan.0.system_network_status.ECGI", dispCond = isNot5GSaConnected},
        eNodeBID = {rdbName = "wwan.0.system_network_status.eNB_ID", dispCond = isNot5GSaConnected},
        CellID = {rdbName = "wwan.0.system_network_status.CellID", dispCond = isNot5GSaConnected},
        PCI = {rdbName = "wwan.0.system_network_status.PCID", dispCond = isNot5GSaConnected},
        EARFCN = {rdbName = "wwan.0.system_network_status.channel", dispCond = isNot5GSaConnected},
        RSRP = {rdbName = "wwan.0.signal.0.rsrp", dispCond = isNot5GSaConnected, prefix = " dBm"},
        RSRQ = {rdbName = "wwan.0.signal.rsrq", dispCond = isNot5GSaConnected, prefix = " dB"},
        SNR = {rdbName = "wwan.0.signal.snr", dispCond = isNot5GSaConnected, prefix = " dB"},
        CQI = {rdbName = "wwan.0.servcell_info.avg_wide_band_cqi", dispCond = isNot5GSaConnected},
        TAC = {rdbName = "wwan.0.radio.information.tac", dispCond = isNot5GSaConnected},
        TransmitPower = {rdbName = "wwan.0.system_network_status.tx_pwr", dispCond = isNot5GSaConnected, prefix = " dBm"},
        RRCState = {rdbName = "wwan.0.radio_stack.rrc_stat.rrc_stat",dispCond = isNot5GSaConnected},
    },
    NR5G = {
        NCGI = {rdbName = "wwan.0.radio_stack.nr5g.cgi", dispCond = is5GSaConnected},
        gNodeBID = {rdbName = "wwan.0.radio_stack.nr5g.gNB_ID", dispCond = is5GSaConnected},
        gNBCellID = {rdbName = "wwan.0.radio_stack.nr5g.CellID", dispCond = is5GSaConnected},
        gNBPCI = {rdbName = "wwan.0.radio_stack.nr5g.pci", dispCond = isNot4GConnected},
        NRARFCN = {rdbName = "wwan.0.radio_stack.nr5g.arfcn", dispCond = isNot4GConnected},
        RSRP = {rdbName = "wwan.0.radio_stack.nr5g.rsrp", dispCond = isNot4GConnected, prefix = " dBm"},
        RSRQ = {rdbName = "wwan.0.radio_stack.nr5g.rsrq", dispCond = isNot4GConnected, prefix = " dB"},
        SNR = {rdbName = "wwan.0.radio_stack.nr5g.snr", dispCond = isNot4GConnected, prefix = " dB"},
        NRCQI = {rdbName = "wwan.0.radio_stack.nr5g.cqi", dispCond = isNot4GConnected},
        TAC = {rdbName = "wwan.0.radio_stack.nr5g.tac", dispCond = is5GSaConnected},
        TransmitPower = {rdbName = "wwan.0.radio_stack.nr5g.tx_pwr", dispCond = is5GSaConnected, prefix = " dBm"},
        gNBSSBIndex = {rdbName = "wwan.0.radio_stack.nr5g.ssb_index", dispCond = isNot4GConnected},
    }
}
------------------------------------------------------------

return {
    [subRoot ..  "5GConnected"] = {
        get = function(node, name)
            return 0, is5GConnected() and "1" or "0"
        end,
    },
    [subRoot ..  "LTE|NR5G.*"] = {
        get = function(node, name)
            local rat, paramName = string.match(name, ".*%.([^.]+)%.([^.]+)$")
            if paramName and paramNameAttrPair[rat] and paramNameAttrPair[rat][paramName] then
                local paramAttr = paramNameAttrPair[rat][paramName]
                if (type(paramAttr.dispCond) == "function" and paramAttr.dispCond())
                    or type(paramAttr.dispCond) ~= "function" then
                    return 0, (luardb.get(paramAttr.rdbName) or node.default) .. (paramAttr.prefix or "")
                end
            end
            return 0, node.default
        end,
    },
}
