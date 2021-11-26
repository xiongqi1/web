--[[
This script handles objects/parameters under Device.FAST.

  Line.{i}.

Copyright (C) 2017 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require('handlers.dslUtil')
require("Daemon")
require("Logger")
require('luardb')

------------------ global variables ----------------------------
local logSubsystem = 'FastDevice'
local subRoot = conf.topRoot .. '.FAST.'
local DSL_CMD_PATH = '/opt/intel/bin/dsl_pipe'
local DIR_US = 0
local DIR_DS = 1
local rdbVarDslLastChange='dsl.line.lastchange'
local reportedProfileList = ''
patternCurrProfile = 'awk \'{ print $2 }\' | awk -F "=" \'{ print $2 }\''
patternAllowedProfile = 'awk \'{ print $2 }\' | awk -F "=" \'{ print $2 }\''
local configuredProfile = {'106a', '106b', '106c', '212a', '212b'}
local reportedProfile = { '106a', '212a'}

------------------------------------------------------------
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem(logSubsystem)

----------------------------------------------------------------------------
-- @brief Function acts on one byte and determines the reported capabilities.
--        Appends to the list of standard profiles
-- @param val: Byte value
----------------------------------------------------------------------------
local function addProfile(val)
    local index = 1
    local supportedProfileList = ''

    -- Reset the value
    reportedProfileList = ''

    while val > 0 do
        -- test the rightmost bits
        if val % 2 == 1 then
            supportedProfileList = supportedProfileList .. ', ' .. configuredProfile[index]
        end
        val = math.floor(val/2) -- shift right
        index = index + 1
    end

    -- Filter out the used profile to determine what needs to be reported
    for index, word in ipairs(reportedProfile) do
        if string.find(supportedProfileList, word) ~= nil then
            if reportedProfileList == '' then
               reportedProfileList = word
            else
               reportedProfileList = reportedProfileList .. ', ' .. word
            end
        end
    end
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

	[subRoot .. 'Line.1.AllowedProfiles'] = {
        get = function (node, name)
            local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'FastProfileConfigGet', patternAllowedProfile)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
            addProfile(val)
            return 0, reportedProfileList
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

	[subRoot .. 'Line.1.CurrentProfile'] = {
        get = function (node, name)
            local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'FastProfileStatusGet', patternCurrProfile)
            -- When connected to G.fast DPU
            if val == 1 then
                return 0, '106a'
            elseif val == 8 then
                return 0, '212a'
            -- When connected to VDSL DPU
            elseif val == 0 then
                return 0, ''
            else
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            end
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

    [subRoot .. 'Line.1.UpstreamAttenuation'] = {
        get = function (node, name)
            local val = dslUtil.determineAttenuation(DIR_US)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'UpstreamAttenuation: val: ' .. val)
                return 0, val
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Line.1.DownstreamAttenuation'] = {
        get = function (node, name)
            local val = dslUtil.determineAttenuation(DIR_DS)
            if val == nil then
                return CWMP.Error.InternalError, "Error: Could not get " .. name
            else
                Logger.log(logSubsystem, 'debug', 'DownstreamAttenuation: val: ' .. val)
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

}
