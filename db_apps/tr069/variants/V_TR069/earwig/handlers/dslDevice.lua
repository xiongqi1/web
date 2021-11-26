--[[
This script handles objects/parameters under Device.DSL.

  Line.{i}.
  Channel.{i}.

Copyright (C) 2017 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require('handlers.dslUtil')
require("Daemon")
require("Logger")
require('luardb')

------------------ global variables ----------------------------
local logSubsystem = 'DslDevice'
local PIPE_CMD='set -o pipefail'
local DSL_CMD_PATH = '/opt/intel/bin/dsl_pipe'
local subRoot = conf.topRoot .. '.DSL.'
local LINE_NUM = 0
local DIR_US = 0
local DIR_DS = 1
local g997CapabilityList = ''
local reportedCapabilityList = ''
local rdbVarDslLastChange='dsl.line.lastchange'
------------------------------------------------------------
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem(logSubsystem)

-- Mapping of Data Model Line stats attribute name to system attribute pattern
patternStandardUsed = 'awk \'{ print$9 }\' | awk -F "=" \'{ print $2}\''

-- Table describing the reported G.997 capabilities.
-- See G.997.1 chapter 7.3.1.1.1 or lib_dsl_api_g997.h for reference.
-- For example, the G.997 capabilities is reported as ATSECapabilities=(00,00,00,00,00,00,00,07,80)
-- Each row in the table below represents a byte reported.
-- The eight strings in each row correspond to bit_0, bit_1 .. bit_7 of an byte
-- If a bit is set, it means that corresponding standard is used.
local g997Capabilities = {
 {'T1.413', 'ETSI_101_388', 'G.992.1_Annex_A_NO_POTS', 'G.992.1_Annex_A_O_POTS', 'G.992.1_Annex_B_NO_ISDN', 'G.992.1_Annex_B_O_ISDN', 'G.992.1_Annex_C_NO_TCM_ISDN', 'G.992.1_Annex_C_O_TCM_ISDN'},
 {'G.992.2_NO_POTS', 'G.992.2_O_POTS', 'G.992.2_NO_TCM_ISDN', 'G.992.2_O_TCM_ISDN', '', '', '', ''},
 {'', '', 'G.992.3_Annex_A_NO_POTS', 'G.992.3_Annex_A_O_POTS', 'G.992.3_Annex_B_NO_ISDN', 'G.992.3_Annex_B_O_ISDN', '', ''},
 {'G.992.4_NO_POTS','G.992.4_O_POTS','','','G.992.3_Annex_I_NO','G.992.3_Annex_I_O','G.992.3_Annex_J_NO','G.992.3_Annex_J_O'},
 {'G.992.4_Annex_I_NO','G.992.4_Annex_I_O','G.992.3_Annex_L_1_NO','G.992.3_Annex_L_2_NO','G.992.3_Annex_L_3_O','G.992.3_Annex_L_4_O','G.992.3_Annex_M_NO_POTS','G.992.3_Annex_M_O_POTS'},
 {'G.992.5_Annex_A_NO_POTS','G.992.5_Annex_A_O_POTS','G.992.5_Annex_B_NO_ISDN','G.992.5_Annex_B_O_ISDN','','','G.992.5_Annex_I_NO','G.992.5_Annex_I_O'},
 {'G.992.5_Annex_J_NO','G.992.5_Annex_J_O','G.992.5_Annex_M_NO_POTS','G.992.5_Annex_M_O_POTS','','','',''},
 {'G.993.2_Annex_A','G.993.2_Annex_B','G.993.2_Annex_C','','','','','G.993.1'},
 {'G.993.5_Annex_A','G.993.5_Annex_B','G.993.5_Annex_C','G.993.5_Annex_A_F','G.993.5_Annex_B_F','G.993.5_Annex_C_F','','G.9701'}
}

-- Standards list to be reported in the 'StandardsUsed' field
local reportedCapabilities = { 'G.992.1_Annex_A', 'G.992.1_Annex_B', 'G.992.1_Annex_C', 'T1.413', 'T1.413i2', 'ETSI_101_388', 'G.992.2', 'G.992.3_Annex_A', 'G.992.3_Annex_B', 'G.992.3_Annex_C', 'G.992.3_Annex_I', 'G.992.3_Annex_J', 'G.992.3_Annex_L', 'G.992.3_Annex_M', 'G.992.4', 'G.992.5_Annex_A', 'G.992.5_Annex_B', 'G.992.5_Annex_C', 'G.992.5_Annex_I', 'G.992.5_Annex_J', 'G.992.5_Annex_M', 'G.993.1', 'G.993.1_Annex_A', 'G.993.2_Annex_A', 'G.993.2_Annex_B', 'G.993.2_Annex_C', 'G.9701' }

-- @brief Function acts on one byte and determines the reported capabilities.
--        Appends to the list of G997 standards
-- @param row: Byte index (1-9)
-- @param val: Byte value
local function addCapability(row, val)
    local col = 1
    while val > 0 do
        -- test the rightmost bits
        if val % 2 == 1 then
            g997CapabilityList = g997CapabilityList .. ', ' .. g997Capabilities[row][col]
        end
        val = math.floor(val/2) -- shift right
        col = col + 1
    end
end

-- @brief Determine the list of G997 standards used in the xDSL connection
-- @param cmd: main command string to run
-- @param subCmd: sub command string to run
-- @param pattern: Pattern corresponding to the Data Model Attribute name
-- @param dir: Direction (0 - US or 1 - DS)
local function determineStandardsUsed(cmd, subCmd, pattern, dir)
    local byteNum=1
    local finalCmd = PIPE_CMD .. '; ' .. cmd .. ' ' .. subCmd .. ' ' .. LINE_NUM .. ' ' .. dir .. ' | ' .. pattern
    Logger.log(logSubsystem, 'debug', 'Only line and dir: finalCmd: ' .. finalCmd)

    -- This will be in a format (00,00,00,00,00,00,00,07,80)
    local rx_cap = Daemon.readCommandOutput(finalCmd)
    Logger.log(logSubsystem, 'debug', 'Command output: ret: ' .. rx_cap)

    -- Reset the value
    reportedCapabilityList = ''

    -- Get the G997 Standards used
    for i in string.gmatch(rx_cap, "%x+") do
        -- Process each byte in the received byte list
        local num=tonumber(i, 16)
        addCapability(byteNum, num)
        byteNum=byteNum+1
    end
    Logger.log(logSubsystem, 'debug', 'g997CapabilityList: ' .. g997CapabilityList)

    -- Filter out the used G997 Standards to determine what needs to be reported
    for index, word in ipairs(reportedCapabilities) do
        if string.find(g997CapabilityList, word) ~= nil then
            if reportedCapabilityList == '' then
               reportedCapabilityList = word
            else
               reportedCapabilityList = reportedCapabilityList .. ', ' .. word
            end
        end
    end

    Logger.log(logSubsystem, 'debug', 'reportedCapabilityList: ' .. reportedCapabilityList)

end

------------------------------------------------------------
return {
	[subRoot .. 'Line.1.Status'] = {
        get = function (node, name)
            local val = dslUtil.determineStatus()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

	[subRoot .. 'Line.1.LastChange'] = {
        get = function (node, name)
            val = hdlerUtil.determineLastChange(rdbVarDslLastChange)
            if val == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not read uptime for node " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

	[subRoot .. 'Line.1.LinkStatus'] = {
        get = function (node, name)
            local val = dslUtil.determineLinkStatus()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

	[subRoot .. 'Line.1.StandardsSupported'] = {
        get = function (node, name)
            local supportedCapabilityList = ''
            for index, word in ipairs(reportedCapabilities) do
                if supportedCapabilityList == '' then
                   supportedCapabilityList = word
                else
                   supportedCapabilityList = supportedCapabilityList .. ', ' .. word
                end
            end
            return 0, supportedCapabilityList
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

	[subRoot .. 'Line.1.StandardUsed'] = {
        get = function (node, name)
            -- Get the Near-End capabilities for the NCD
            determineStandardsUsed(DSL_CMD_PATH, 'G997_LineInventoryGet', patternStandardUsed, DIR_US)
            return 0, reportedCapabilityList
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

	[subRoot .. 'Line.1.UpstreamMaxBitRate'] = {
        get = function (node, name)
            local val = dslUtil.determineAttainableRate(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'UpstreamMaxBitRate: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

	[subRoot .. 'Line.1.DownstreamMaxBitRate'] = {
        get = function (node, name)
            local val = dslUtil.determineAttainableRate(DIR_DS)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'DownstreamMaxBitRate: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

	[subRoot .. 'Line.1.UpstreamNoiseMargin'] = {
        get = function (node, name)
            local val = dslUtil.determineNoiseMargin(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'UpstreamNoiseMargin: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.DownstreamNoiseMargin'] = {
        get = function (node, name)
            local val = dslUtil.determineNoiseMargin(DIR_DS)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'DownstreamNoiseMargin: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.UpstreamPower'] = {
        get = function (node, name)
            local val = dslUtil.determinePower(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'UpstreamPower: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.DownstreamPower'] = {
        get = function (node, name)
            local val = dslUtil.determinePower(DIR_DS)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'DownstreamPower: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.TestParams.LATNds'] = {
        get = function (node, name)
            local val = dslUtil.determineLineAttenuation(DIR_DS)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'DownstreamLineAttenuation: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.TestParams.LATNus'] = {
        get = function (node, name)
            local val = dslUtil.determineLineAttenuation(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'UpstreamLineAttenuation: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.TestParams.SATNds'] = {
        get = function (node, name)
            local val = dslUtil.determineSignalAttenuation(DIR_DS)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'DownstreamSignalAttenuation: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.TestParams.SATNus'] = {
        get = function (node, name)
            local val = dslUtil.determineSignalAttenuation(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'UpstreamSignalAttenuation: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.ErroredSecs'] = {
        get = function (node, name)
            local val = dslUtil.determineErroredSecs(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'ErroredSecs: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.SeverelyErroredSecs'] = {
        get = function (node, name)
            local val = dslUtil.determineSeverelyErroredSecs(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'SeverelyErroredSecs: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.X_NETCOMM.FullInits'] = {
        get = function (node, name)
            local val = dslUtil.determineFullInits()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'FullInits: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.X_NETCOMM.FailedFullInits'] = {
        get = function (node, name)
            local val = dslUtil.determineFailedFullInits()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'FailedFullInits: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.X_NETCOMM.ShortInits'] = {
        get = function (node, name)
            local val = dslUtil.determineShortInits()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'ShortInits: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.X_NETCOMM.FailedShortInits'] = {
        get = function (node, name)
            local val = dslUtil.determineFailedShortInits()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'FailedShortInits: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.X_NETCOMM.ReInits'] = {
        get = function (node, name)
            local val = dslUtil.determineReInits()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'ReInits: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.X_NETCOMM.Showtimes'] = {
        get = function (node, name)
            local val = dslUtil.determineShowtimeCount()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'Showtime Count: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.Stats.Total.X_NETCOMM.TotalShowtime'] = {
        get = function (node, name)
            local val = dslUtil.determineTotalShowtime()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            Logger.log(logSubsystem, 'debug', 'Total time in Showtime: val: ' .. val)
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Channel.1.Status'] = {
        get = function (node, name)
            local val = dslUtil.determineStatus()
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Channel.1.LastChange'] = {
        get = function (node, name)
            val = hdlerUtil.determineLastChange(rdbVarDslLastChange)
            if val == nil then
                return CWMP.Error.InternalError,
                    "Error: Could not read uptime for node " .. name
            end
            return 0, val
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Channel.1.LowerLayers'] = {
        get = function (node, name)
                return 0, 'Device.DSL.Line.1'
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Channel.1.UpstreamCurrRate'] = {
        get = function (node, name)
            local val = dslUtil.determineNetDataRate(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'UpstreamCurrRate: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Channel.1.DownstreamCurrRate'] = {
        get = function (node, name)
            local val = dslUtil.determineNetDataRate(DIR_DS)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'DownstreamCurrRate: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },
}
