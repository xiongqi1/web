--[[
This script handles common utility functions used by DSL and FAST objects
Copyright (C) 2017 NetComm Wireless limited.
--]]

local M = {}
dslUtil = M

require('lfs')
require('Logger')
require('Daemon')
require('luardb')

local LINE_NUM = 0
local CHANNEL_NUM = 0
local PIPE_CMD='set -o pipefail'
local MAX_CURR_RATE_DEFAULT_VALUE = 4294967295
local DSL_CMD_PATH = '/opt/intel/bin/dsl_pipe'

local logSubsystem = 'dslUtil'
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem(logSubsystem)

-- Mapping of Data Model Line stats attribute name to system attribute pattern
patternStatus = 'awk -F "=" \'{ print $3 }\''
patternLinkStatus = 'awk -F "=" \'{ print $3 }\''
patternMaxBitRate = 'awk \'{ print $5}\' | awk -F "=" \' {print $2}\''
patternNoiseMargin = 'awk \'{ print $4}\' | awk -F "=" \' {print $2}\''
patternAttenuation = 'awk \'{ print $2}\' | awk -F "=" \' {print $2}\''
-- Value of -32768 is returned when its not available by the DSL stack. We need to report them as special value of '1271' as per G997.1 Section 7.5.1.9 and 7.5.1.10
patternLineAttenuation = 'awk -F "=" \'{ print $5 " " $6 " " $7 " " $8 " " $9 }\' | awk \'{ print $1 "," $3 "," $5 "," $7 "," $9 }\' | sed \'s/-32768/1271/g\''
patternSignalAttenuation = 'awk -F "=" \'{ print $10 " " $11 " " $12 " " $13 " " $14 }\' | awk \'{ print $1 "," $3 "," $5 "," $7 "," $9 }\' | sed \'s/-32768/1271/g\''
patternPower = 'awk \'{ print $7}\' | awk -F "=" \' {print $2}\''

-- Mapping of Data Model Channel stats attribute name to system attribute pattern
patternCurrRate = 'awk \'{ print $6}\' | awk -F "=" \' {print $2}\''

-- Mapping of Line status values to TR069 expected strings
local linkStatusValues = { [0] = 'Disabled',
[256] = 'Disabled',
[512] = 'NoSignal',
[768] = 'EstablishingLink',
[896] = 'EstablishingLink',
[2049] = 'Up'
}

-- Mapping of Line initialization counters
patternFullInits = 'awk \'{ print $5}\' | awk -F "=" \' {print $2}\''
patternFailedFullInits = 'awk \'{ print $6}\' | awk -F "=" \' {print $2}\''
patternShortInits = 'awk \'{ print $7}\' | awk -F "=" \' {print $2}\''
patternFailedShortInits = 'awk \'{ print $8}\' | awk -F "=" \' {print $2}\''
patternReInits = 'awk \'{ print $9}\' | awk -F "=" \' {print $2}\''

-- Mapping of Line second counters
patternErroredSecs = 'awk \'{ print $6}\' | awk -F "=" \' {print $2}\''
patternSeverelyErroredSecs = 'awk \'{ print $7}\' | awk -F "=" \' {print $2}\''

----------------------------------------------------------------------------
-- @brief runs the 'Get' command for a given attribute and returns the value
-- @param cmd: main command string to run
-- @param subCmd: sub command string to run
-- @param pattern: Pattern corresponding to the Data Model Attribute name
-- @param dir: Direction (0 - US or 1 - DS or nil)
-- @param channel: Channel is 0 or nil
-- @return returns the value of the attribute
----------------------------------------------------------------------------
function M.runGetCommand(cmd, subCmd, pattern, dir, channel)
    local ret
    local finalCmd = PIPE_CMD .. '; ' .. cmd .. ' ' .. subCmd .. ' ' .. LINE_NUM
    if channel ~= nil then
        finalCmd = finalCmd .. ' ' .. channel
    end
    if dir ~= nil then
        finalCmd = finalCmd .. ' ' .. dir
    end
    finalCmd = finalCmd .. ' | ' .. pattern
    Logger.log(logSubsystem, 'debug', 'finalCmd: ' .. finalCmd)
    ret = Daemon.readCommandOutput(finalCmd)

    if subCmd == 'G997_LineStatusPerBandGet' then
        Logger.log(logSubsystem, 'debug', 'Command output: ret: ' .. ret)
        return (ret)
    else
        Logger.log(logSubsystem, 'debug', 'Command output: ret: ' .. tonumber(ret))
        return tonumber(ret)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the line/channel status of the xDSL line
-- @return line status as a string
----------------------------------------------------------------------------
function M.determineStatus()
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'LineStateGet', patternStatus)
    if val == nil then
        return nil
    elseif val == 2049 then
        return 'Up'
    else
        return 'Down'
    end
end

----------------------------------------------------------------------------
-- @brief Determine the link status of the xDSL line
-- @return link status as a string
----------------------------------------------------------------------------
function M.determineLinkStatus()
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'LineStateGet', patternLinkStatus)
    if linkStatusValues[val] == nil then
        return 'Error'
    end
    return linkStatusValues[val]
end

----------------------------------------------------------------------------
-- @brief Determine the attainable net data rate (ATTNDR) of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Attainable rate expressed in kbps (string)
----------------------------------------------------------------------------
function M.determineAttainableRate(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'G997_LineStatusGet', patternMaxBitRate, dir)
    if val == nil then
        return nil
    else
        -- Value returned is in bps, convert to kbps
        return tostring(math.floor((val/1000)+0.5))
    end
end

----------------------------------------------------------------------------
-- @brief Determine the noise margin (SNRm) of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Noise margin expressed in units of 0.1dB (string)
----------------------------------------------------------------------------
function M.determineNoiseMargin(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'G997_LineStatusGet', patternNoiseMargin, dir)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the attenuation of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Line attenuation expressed in units of 0.1dB (string)
----------------------------------------------------------------------------
function M.determineAttenuation(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'G997_LineStatusGet', patternAttenuation, dir)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the per band line attenuation of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Comma-separated list of Per Band Line attenuation expressed in units of 0.1dB (string)
----------------------------------------------------------------------------
function M.determineLineAttenuation(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'G997_LineStatusPerBandGet', patternLineAttenuation, dir)
    return (val)
end

----------------------------------------------------------------------------
-- @brief Determine the per band signal attenuation of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Comma-separated list of Per Band Signal attenuation expressed in units of 0.1dB (string)
----------------------------------------------------------------------------
function M.determineSignalAttenuation(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'G997_LineStatusPerBandGet', patternSignalAttenuation, dir)
    return (val)
end

----------------------------------------------------------------------------
-- @brief Determine the transmit power of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Transmit power expressed in units of 0.1dBmV (string)
----------------------------------------------------------------------------
function M.determinePower(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'G997_LineStatusGet', patternPower, dir)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the actual net data rate (ANDR) of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Actual net data rate expressed in kbps (string)
----------------------------------------------------------------------------
function M.determineNetDataRate(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'G997_ChannelStatusGet', patternCurrRate, dir, CHANNEL_NUM)
    if val == nil then
        return nil
    elseif val == 0 then
        -- Note: If the parameter is implemented but no value is available, it MUST have the value
        -- 4294967295 (the maximum for its data type).
        val = MAX_CURR_RATE_DEFAULT_VALUE
        return tostring(val)
    else
        -- Value returned is in bps, convert to kbps
        return tostring(val/1000)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total number of Full initializations of the xDSL line
-- @return Count of the Full initializations of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineFullInits()
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'PM_LineInitCountersTotalGet', patternFullInits)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total number of Failed full initializations of the xDSL line
-- @return Count of the Failed full initializations of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineFailedFullInits()
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'PM_LineInitCountersTotalGet', patternFailedFullInits)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total number of Short initializations of the xDSL line
-- @return Count of the Short initializations of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineShortInits()
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'PM_LineInitCountersTotalGet', patternShortInits)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total number of Failed short initializations of the xDSL line
-- @return Count of the Failed short initializations of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineFailedShortInits()
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'PM_LineInitCountersTotalGet', patternFailedShortInits)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total number of reinitializations of the xDSL line
-- @return Count of the reinitializations of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineReInits()
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'PM_LineInitCountersTotalGet', patternReInits)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total number of Errored seconds of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Errored seconds of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineErroredSecs(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'PM_LineSecCountersTotalGet', patternErroredSecs, dir)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total number of Severely Errored seconds of the xDSL line
-- @param dir: Direction (0 - US or 1 - DS)
-- @return Severely Errored seconds of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineSeverelyErroredSecs(dir)
    local val = dslUtil.runGetCommand(DSL_CMD_PATH, 'PM_LineSecCountersTotalGet', patternSeverelyErroredSecs, dir)
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total time the line has been in Showtime state
-- @return Total time in Showtime of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineTotalShowtime()
    local current_time = Daemon.readIntFromFile('/proc/uptime') or nil
    local sync_time = Daemon.readIntFromFile('/tmp/userifd/line_synced_time_0') or nil
    local drop_time = Daemon.readIntFromFile('/tmp/userifd/line_dropped_time_0') or nil
    local cumulative_time = Daemon.readIntFromFile('/tmp/userifd/line_cumulative_showtime_0') or nil

    if current_time == nil or sync_time == nil or drop_time == nil or cumulative_time == nil then
        return nil
    end

    -- Example: The line synced @ t=200, so line_synced_time = 200.
    -- line_dropped_time = 0 and line_cumulative_showtime = 0
    -- Line lost sync @ t=500, then line_dropped_time = 500 and
    -- line_cumulative_showtime = 500-200=300.
    -- Say the line regained sync @ t=700 and then dropped out @ t=900
    -- So if we read the values @ t=300 when the line was synced,
    -- we would get TotalShowtime=300-200=100
    -- If we read the values @ t=600 when the line was out of sync,
    -- we would get TotalShowtime=300
    -- If we read the values @ t=800 when the line was synced again,
    -- we would get TotalShowtime=300+(800-700)=400
    if sync_time > drop_time then
        -- The line is currently in sync
        return tostring(cumulative_time + (current_time - sync_time))
    else
        -- The line is currently in not in sync
        return tostring(cumulative_time)
    end
end

----------------------------------------------------------------------------
-- @brief Determine the total number of times the line went to Showtime state
--        after losing sync
-- @return Total count of Showtime events of the xDSL line since device bootup
----------------------------------------------------------------------------
function M.determineShowtimeCount()
    local val = Daemon.readIntFromFile('/tmp/userifd/line_showtime_count_0') or nil
    if val == nil then
        return nil
    else
        return tostring(val)
    end
end

return dslUtil


