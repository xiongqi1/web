#!/usr/bin/env lua

-- setup for our local includes
local us = debug.getinfo(1, "S").source:match[[^@?(.*[\/])[^\/]-$]]
if us then
  package.path = us.."?.lua;"..package.path 
else
  us = ''
end

-- our config file is shared with the TR-069 hostbridge
conf = dofile('/usr/lib/tr-069/config.lua')

local fileRollBaseName = conf.stats.fileRollBaseName or "/tmp/file-roll-stats."
local fileRollPeriod = conf.stats.fileRollPeriod or 60 * 60
local fileRollCount  = conf.stats.fileRollCount or 25
local fileRollPort  = conf.stats.fileRollPort or 2220

server = assert(loadfile(us..'rollerserver.lua'))

server('prefix', fileRollBaseName, 'interval', fileRollPeriod, 'nfiles', fileRollCount, 'port', fileRollPort)
