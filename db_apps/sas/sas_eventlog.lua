--
-- Copyright (C) 2019 NetComm Wireless Limited.
--

local eventlog = {}

-- rotate log files every hour or when log size exceeds limit
-- @param logfile full path to sas event log file
local function log_rotate(logfile)
  local lfs = require("lfs")
  local attributes = lfs.attributes(logfile)
  if attributes then
    local last_log = os.date("*t", attributes.modification)
    local current = os.date("*t")
    local maxrotate = luardb.get("sas.eventlog.maxrotate") or 24
    local maxsize = (luardb.get("sas.eventlog.sizekb") or 128) / maxrotate
    local logsize = attributes.size / 1024 -- in KiB
    if last_log.hour ~= current.hour or logsize >= maxsize then
      os.remove(logfile .. '.'..maxrotate) -- remove oldest log
      for i=maxrotate-1,1,-1 do
        os.rename(logfile..'.'..i, logfile..'.'..i+1)
      end
      os.rename(logfile, logfile..'.1')
    end
  end
end

-- log an event to a log file, rotate log files if neccessary
-- @param args, array of values to be logged
local function log(args)
  local logfile = luardb.get("sas.eventlog.filename") or "/tmp/sas_event.log"
  log_rotate(logfile)

  local file, err = io.open(logfile, "a")
  if not file then
    local syslog = require('luasyslog')
    syslog.log('error', string.format('Failed to open sas event log, error=%s', err))
    return
  end

  local event = args[1]
  for i = 2, #args do event = event .. ',' .. (args[i] or '') end
  file:write(string.format("%s,%s\n", os.date("!%Y-%m-%dT%TZ"), event))
  file:close()
end

-- log SAS event based on request and response data
-- @param request, the request to SAS server
-- @param res, response data
function eventlog.log(request, res)
  local responseCode = res['responseCode'] or -1
  local grantExpireTime = res['grantExpireTime'] or ''
  local transmitExpireTime = res['transmitExpireTime'] or ''
  local args = {
    ['registration'] =    {1,res['cbsdId'],responseCode},
    ['deregistration'] =  {2,res['cbsdId'],responseCode},
    ['grant'] =           {3,res['grantId'],responseCode,grantExpireTime},
    ['relinquishment'] =  {4,res['grantId'],responseCode,grantExpireTime},
    ['heartbeat'] =       {5,res['grantId'],responseCode,grantExpireTime,transmitExpireTime},
  }
  if args[request] then log(args[request]) end
end

return eventlog
