--[[
This script handles objects/parameters under Device.Ethernet.

  Interface.{i}.
  Interface.{i}.Stats.
  RMONStats.{i}.

Currently, only eth0_1 is supported

Copyright (C) 2016 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require("Daemon")
require("Logger")
require("Int64")

SWITCH_CLI_CMD='switch_cli'
PIPE_CMD='set -o pipefail'
INTF_ENABLE_MODE_DEFAULT='1'
MAX_BIT_RATE_DEFAULT_VALUE='-1'
DUPLEX_MODE_DEFAULT_VALUE='Auto'
RMON_STATS_CACHE_TIMEOUT_IN_SECS=5
INTF_EEE_ENABLE_DEFAULT_VALUE='1'

------------------global variables----------------------------
local logSubsystem = 'EthernetDevice'
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem(logSubsystem)

local subRoot = conf.topRoot .. '.Ethernet.'

local systemFS = '/sys/class/net/'
local iface = 'eth0_1'
local port = 2
local rdbVarIntfEnableMode = 'eth.intf.1.enable_mode'
local rdbVarIntfEnable = 'eth.intf.1.enable'
local rdbVarIntfLastChange= 'eth.intf.1.lastchange'
local rdbVarMaxBitRate = 'eth.intf.1.maxbitrate'
local rdbVarDuplexMode = 'eth.intf.1.duplexmode'
local rdbVarIntfEEEEnable = 'eth.intf.1.eeeenable'
local rmonLastUpdatedTime=nil
local rmonAttrs={}

------------------------------------------------------------

-- Data Model attribute name to system attribute mapping
-- key = data model attr name
-- value = table of {pattern to match, System attribute name}
local dmAttrToSysAttrMap = { ['Enable'] = { '(%d+)', 'eEnable' },
                    ['Status'] = { '(%d+)', 'eLink' },
                    ['CurrentBitRate'] = { '(%d+)', 'eSpeed' },
                    ['X_NETCOMM_CurrentDuplexMode'] = {'(%d+)', 'eDuplex' }
}
-- Data Model attribute to system attribute mapping for ethernet stats
-- key = data model attr name
-- value = table of system attribute names
local statsDMAttrToSysAttr = { ['BytesSent'] = {'nTxGoodBytes'},
        ['BytesReceived'] = {'nRxGoodBytes'},
        ['PacketsSent'] = {'nTxGoodPkts'},
        ['PacketsReceived'] = {'nRxGoodPkts'},
        ['ErrorsSent'] = {'nTxDroppedPkts'},
        ['ErrorsReceived'] = {'nRxFCSErrorPkts', 'nRxUnderSizeErrorPkts',
                              'nRxOversizeErrorPkts'},
        ['UnicastPacketsSent'] = {'nTxUnicastPkts'},
        ['UnicastPacketsReceived'] = {'nRxUnicastPkts'},
        ['DiscardPacketsSent'] = {'nTxDroppedPkts', 'nTxAcmDroppedPkts'},
        ['DiscardPacketsReceived'] = {'nRxDroppedPkts'},
        ['MulticastPacketsSent'] = {'nTxMulticastPkts'},
        ['MulticastPacketsReceived'] = {'nRxMulticastPkts'},
        ['BroadcastPacketsSent'] = {'nTxBroadcastPkts'},
        ['BroadcastPacketsReceived'] = {'nRxBroadcastPkts'},
        ['UnknownProtoPacketsReceived'] = {}
}

-- Data Model attribute to system attribute mapping for rmon ethernet stats
-- key = data model attr name
-- value = table of system attribute names
local rmonStatsDMAttrToSysAttr = { ['DropEvents'] = {'nTxAcmDroppedPkts'},
        ['Bytes'] = {'nRxGoodBytes', 'nRxBadBytes'},
        ['Packets'] = {'nRxGoodPkts'},
        ['BroadcastPackets'] = {'nRxBroadcastPkts'},
        ['MulticastPackets'] = {'nRxMulticastPkts'},
        ['CRCErroredPackets'] = {'nRxFCSErrorPkts'},
        ['UndersizePackets'] = {'nRxUnderSizeGoodPkts'},
        ['OversizePackets'] = {'nRxOversizeGoodPkts'},
        ['Packets64Bytes'] = {'nRx64BytePkts'},
        ['Packets65to127Bytes'] = {'nRx127BytePkts'},
        ['Packets128to255Bytes'] = {'nRx255BytePkts'},
        ['Packets256to511Bytes'] = {'nRx511BytePkts'},
        ['Packets512to1023Bytes'] = {'nRx1023BytePkts'},
        ['Packets1024to1518Bytes'] = {'nRxMaxBytePkts'}
}

-- Valid values for all the read write attributes
local boolValidValues = { ['0'] = true, ['1'] = true }
local duplexModeValidValues = { ['Auto'] = true, ['Half'] = true, ['Full'] = true }
local maxbitrateValidValues = { ['-1'] = true, ['10'] = true, ['100'] = true, ['1000'] = true }

-- Conversion tables between Data Model and System Values
local duplexModeDMToSysValues = { ['Full'] = 0, ['Half'] = 1 }
local duplexModeSysToDMValues = { ['0'] = 'Full', ['1'] = 'Half' }
local linkStatusSysToDMValues = { ['1'] = 'Up', ['0'] = 'Down' }

------------------local function----------------------------

-- @brief Interpret the statistics and return proper value for a
-- single data model attribute
-- @param dmAttr Data Model Attribute name
-- @param rmonStats RMON statistics table
-- @param mappingTable table with Data Model Attribute as key
--     and a list of System Attributes () as value
-- @return returns the proper value of the given attribute as a string
local interpretStats = function (dmAttr, rmonStats, mappingTable)
    sysAttrs = mappingTable[dmAttr]
    Logger.log(logSubsystem, 'debug', ' dmAttr: ' .. dmAttr)
    val = tobignumber(0)
    for _, sysAttr in pairs(sysAttrs) do
        val = val + tobignumber(rmonStats[sysAttr])
    end
    return tostring(val);
end

-- @brief Read RMON statistics for a given port.
-- This caches the output of the statistics and reads from system
-- only when the cache has expired.
-- @param port port number
-- @return returns the entire rmon stats as a table
function readRMONStatistics(port)
    local currTime = os.time()
    -- check if the request is received with in this time then
    -- return the cached variables
    if (rmonLastUpdatedTime ~= nil and
      os.difftime(currTime, rmonLastUpdatedTime) <= RMON_STATS_CACHE_TIMEOUT_IN_SECS) then
        Logger.log(logSubsystem, 'debug', 'Returning cached values')
        return rmonAttrs
    end

    rmonAttrs={}
    -- Device.Ethernet.RMONStats.1.
    local cmd = SWITCH_CLI_CMD .. ' GSW_RMON_PORT_GET nPortId=' .. port
    local f = io.popen(cmd, 'r') -- Daemon.readCommandOutput() merges the output because we do pipe:read(*a).
    for line in f:lines() do -- Read the pipe line-by-line instead.
        line = line:gsub("%s+", "") -- then destroy any white-spaces.
        if (line:len() > 3) then -- Skip line if it's empty. Yes, that happens too.
            n, v = line:match("(%w+):(%d+)") -- Break by colon. LHS is string, RHS is some digits.
            if n and v then
                rmonAttrs[n] = v
            end
        end
    end
    f:close()
    -- update the rmon attributes timestamp
    rmonLastUpdatedTime = os.time()
    Logger.log(logSubsystem, 'debug', 'updated cache at: ' .. rmonLastUpdatedTime)
    return rmonAttrs
end

-- @brief runs the 'Get' command for a given attribute and returns the value
-- @param cmd main command string to run
-- @param subCmd sub command string to run
-- @port port number
-- @dmAttr Data Model Attribute name to get
-- @return returns the value of the given attribute
local function runGetCommand(cmd, subCmd, port, dmAttr)
    Logger.log(logSubsystem, 'debug', 'subCmd:' .. ' '
        .. subCmd .. ' port: ' .. port .. ' attr: ' .. dmAttr)
    pattern, sysAttr = unpack(dmAttrToSysAttrMap[dmAttr])
    ret = Daemon.readCommandOutput(PIPE_CMD .. '; ' .. cmd .. ' ' .. subCmd
                .. ' nPortId=' .. port .. ' | grep ' .. sysAttr)
    return ret:match(pattern)
end

-- @brief runs the 'Set' command for a given attribute and returns the value
-- @param cmd main command string to run
-- @param subCmd sub command string to run
-- @port port number
-- @dmAttr Data Model Attribute name to set
-- @value value to set the given Data Model Attribute to
-- @return does not return anything, if the given command fails,
-- there is an assert inside readCommandOutput already.
local function runSetCommand(cmd, subCmd, port, dmAttr, value)
    Logger.log(logSubsystem, 'debug', 'subCmd:' .. ' '
        .. subCmd .. ' port: ' .. port .. ' attr: ' .. dmAttr .. ' value: ' .. value)
    _, sysAttr = unpack(dmAttrToSysAttrMap[dmAttr])
    -- we don't care about the output here actually, just run the command and
    -- verify the return value, which is done internally already
    Daemon.readCommandOutput(PIPE_CMD .. '; ' .. cmd .. ' ' .. subCmd
                .. ' nPortId=' .. port .. ' ' .. sysAttr .. '=' .. value)
end

-- @brief set functionality of bitrate used as part of both init and set
-- @param node cwmp node
-- @param name cwmp node name
-- @param value value to be set
-- @return returns 0 in case of success otherwise proper error value
local function setBitRate(node, name, value)
    if maxbitrateValidValues[value] then
        if value == '-1' then
            -- bSpeedForce will be set to 0 which means auto negotation so
            -- eSpeed will be set automatically
            cmd_ext = ' bSpeedForce=0'
        else
            -- bSpeedForce will be set to 1 and eSpeed will be set to
            -- value
            cmd_ext = ' bSpeedForce=1 eSpeed=' .. value
        end
        currDuplexMode = luardb.get(rdbVarDuplexMode)
        if currDuplexMode == 'Auto' or currDuplexMode == nil then
            cmd_ext = cmd_ext .. ' bDuplexForce=0'
        else
            cmd_ext = cmd_ext .. ' bDuplexForce=1 eDuplex='
                .. duplexModeDMToSysValues[currDuplexMode]
        end
        Logger.log(logSubsystem, 'debug', ' In setBitRate: cmd_ext:' .. cmd_ext)
        Daemon.readCommandOutput(PIPE_CMD .. '; ' .. SWITCH_CLI_CMD
                .. ' GSW_PORT_LINK_CFG_SET nPortId=' .. port .. cmd_ext)
        luardb.set(rdbVarMaxBitRate, value)
    else
        return CWMP.Error.InvalidParameterValue,
            "Error: Invalid parameter value " .. value .. " for node "
            .. name
        end
    return 0
end

-- @brief set functionality of duplexmode used as part of both init and set
-- @param node cwmp node
-- @param name cwmp node name
-- @param value value to be set
-- @return returns 0 in case of success otherwise proper error value
local function setDuplexMode(node, name, value)
    if duplexModeValidValues[value] then
        if value == 'Auto' then
            -- bDuplexForce will be set to 0 which means auto negotation so
            -- eDuplex will be set automatically
            cmd_ext = ' bDuplexForce=0'
        else
            -- bDuplexForce will be set to 1 and eDuplex will be set to
            -- the value given
            cmd_ext = ' bDuplexForce=1 eDuplex=' .. duplexModeDMToSysValues[value]
        end
        currBitRate = luardb.get(rdbVarMaxBitRate)
        if currBitRate == '-1' or currBitRate == nil then
            cmd_ext = cmd_ext .. ' bSpeedForce=0'
        else
            cmd_ext = cmd_ext .. ' bSpeedForce=1 eSpeed=' .. currBitRate
        end
        Logger.log(logSubsystem, 'debug', ' In setDuplexMode: cmd_ext:' .. cmd_ext)
        Daemon.readCommandOutput(PIPE_CMD .. '; ' .. SWITCH_CLI_CMD
                .. ' GSW_PORT_LINK_CFG_SET nPortId=' .. port .. cmd_ext)
        luardb.set(rdbVarDuplexMode, value)
    else
        return CWMP.Error.InvalidParameterValue,
            "Error: Invalid parameter value " .. value .. " for node "
            .. node.name
    end
    return 0
end

-- @brief set functionality of EEEEnable used as part of both init and set
-- @param node cwmp node
-- @param name cwmp node name
-- @param value value to be set
-- @return returns 0 in case of success otherwise proper error value
local function setEEEEnable(node, name, value)
    if boolValidValues[value] then
        cmd_ext = ' bLPI=' .. value
        currDuplexMode = luardb.get(rdbVarDuplexMode)
        if currDuplexMode == 'Auto' or currDuplexMode == nil then
            cmd_ext = cmd_ext .. ' bDuplexForce=0'
        else
            cmd_ext = cmd_ext .. ' bDuplexForce=1 eDuplex='
                .. duplexModeDMToSysValues[currDuplexMode]
        end
        currBitRate = luardb.get(rdbVarMaxBitRate)
        if currBitRate == '-1' or currBitRate == nil then
            cmd_ext = cmd_ext .. ' bSpeedForce=0'
        else
            cmd_ext = cmd_ext .. ' bSpeedForce=1 eSpeed=' .. currBitRate
        end
        Logger.log(logSubsystem, 'debug', ' In setEEEEnable: cmd_ext:' .. cmd_ext)
        -- set the value in system
        Daemon.readCommandOutput(PIPE_CMD .. '; ' .. SWITCH_CLI_CMD
            .. ' GSW_PORT_LINK_CFG_SET nPortId=' .. port .. cmd_ext)
        luardb.set(rdbVarIntfEEEEnable, value)
    else
        return CWMP.Error.InvalidParameterValue,
            "Error: Invalid parameter value " .. value .. " for node "
            .. name
    end
    return 0
end

------------------------------------------------------------

return {
    [subRoot .. 'Interface.1.Enable'] = {
        init = function(node, name)
            valInRdb = luardb.get(rdbVarIntfEnableMode)
            if valInRdb == nil then
                luardb.set(rdbVarIntfEnableMode, INTF_ENABLE_MODE_DEFAULT, 'p')
            end
            return 0
        end,
        get = function(node, name)
            return 0, luardb.get(rdbVarIntfEnableMode)
        end,
        set = function(node, name, value)
            if boolValidValues[value] then
                luardb.set(rdbVarIntfEnableMode, value)
                return 0
            else
                return CWMP.Error.InvalidParameterValue,
                    "Error: Invalid parameter value " .. value .. " for node "
                    .. name
            end
        end
    },

    [subRoot .. 'Interface.1.Status'] = {
        get = function(node, name)
            return 0, linkStatusSysToDMValues[luardb.get(rdbVarIntfEnable)]
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.Name'] = {
        get = function(node, name)
            return 0, iface
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.LastChange'] = {
        get = function(node, name)
            val = hdlerUtil.determineLastChange(rdbVarIntfLastChange)
            if val == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not read uptime for node " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.MACAddress'] = {
        get = function(node, name)
            local val = Daemon.readCommandOutput('pdb_get mac')
            if val == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not read mac address: " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.EEEEnable'] = {
        init = function(node, name)
            valInRdb = luardb.get(rdbVarIntfEEEEnable)
            if valInRdb == nil then
                luardb.set(rdbVarIntfEEEEnable, INTF_EEE_ENABLE_DEFAULT_VALUE, 'p')
            end
            -- set the value in system
            return setEEEEnable(node, name, luardb.get(rdbVarIntfEEEEnable))
        end,
        get = function(node, name)
            return 0, tostring(luardb.get(rdbVarIntfEEEEnable))
        end,
        set = function(node, name, value)
            return setEEEEnable(node, name, value)
        end,
    },

    [subRoot .. 'Interface.1.MaxBitRate'] = {
        init = function(node, name)
            valInRdb = luardb.get(rdbVarMaxBitRate)
            if valInRdb == nil then
                luardb.set(rdbVarMaxBitRate, MAX_BIT_RATE_DEFAULT_VALUE, 'p')
            end
            -- set the value in system
            return setBitRate(node, name, luardb.get(rdbVarMaxBitRate))
        end,
        get = function(node, name)
            return 0, tostring(luardb.get(rdbVarMaxBitRate))
        end,
        set = function(node, name, value)
            return setBitRate(node, name, value)
        end,
    },

    [subRoot .. 'Interface.1.CurrentBitRate'] = {
        get = function(node, name)
            ret = runGetCommand(SWITCH_CLI_CMD, 'GSW_PORT_LINK_CFG_GET', port, node.name)
            if ret == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not get " .. name
            end
            return 0, ret
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.DuplexMode'] = {
        init = function(node, name)
            valInRdb = luardb.get(rdbVarDuplexMode)
            if valInRdb == nil then
                luardb.set(rdbVarDuplexMode, DUPLEX_MODE_DEFAULT_VALUE, 'p')
            end
            -- set the value in system
            return setDuplexMode(node, name, luardb.get(rdbVarDuplexMode))
        end,
        get = function(node, name)
            return 0, tostring(luardb.get(rdbVarDuplexMode))
        end,
        set = function(node, name, value)
            return setDuplexMode(node, name, value)
        end
    },

    [subRoot .. 'Interface.1.X_NETCOMM_CurrentDuplexMode'] = {
        get = function(node, name)
            ret = runGetCommand(SWITCH_CLI_CMD, 'GSW_PORT_LINK_CFG_GET', port, node.name)
            if ret == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not get " .. name
            end
            return 0, duplexModeSysToDMValues[ret]
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.Stats.*'] = {
        get = function(node, name)
            rmonStats = readRMONStatistics(port)
            if ret == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not read stats for node" .. name
            end
            return 0, interpretStats(node.name, rmonStats, statsDMAttrToSysAttr)
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'RMONStats.1.*'] = {
        get = function(node, name)
            rmonStats = readRMONStatistics(port)
            if ret == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not read stats for node" .. name
            end
            return 0, interpretStats(node.name, rmonStats, rmonStatsDMAttrToSysAttr)
        end,
        set = CWMP.Error.funcs.ReadOnly
    },
}
