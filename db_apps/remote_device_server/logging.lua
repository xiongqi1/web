-- Copyright (C) 2018 NetComm Wireless limited.
--
-- Log functions, requiring luasyslog module.

local log_module = {}

require "luasyslog"

luasyslog.open('rds', 'LOG_DAEMON')

local function doLog(priority, str)
    luasyslog.log(priority, str)
end

function log_module.logInfo(str)
    doLog('LOG_INFO', str)
end

function log_module.logNotice(str)
    doLog('LOG_NOTICE', str)
end

function log_module.logDbg(str)
    doLog('LOG_DEBUG', str)
end

function log_module.logErr(str)
    doLog('LOG_ERR', str)
end

return log_module
