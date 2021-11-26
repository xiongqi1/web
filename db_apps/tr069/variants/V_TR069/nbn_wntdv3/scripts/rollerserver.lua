#!/usr/bin/env lua

require('luasyslog')
require "copas"

local turbo = require('turbo')

luasyslog.open('rollerserver', 'LOG_DAEMON')

local rollerDebug = conf.wntd.debug
--local rollerDebug = 2

local function dinfo(v)
  if rollerDebug > 1 then
    luasyslog.log('LOG_INFO', v)
  end
end

local scandir
local getStatsRollName


local outputCoroutine

local function addRollerLines()
  local function perform(lines)
    local f = io.open (getStatsRollName(), 'a+')
    if not f then return end
    f:write(lines)
    f:close()
  end
  while true do
      perform(coroutine.yield())
  end
end

outputCoroutine = coroutine.create(addRollerLines)

coroutine.resume(outputCoroutine)

local function forcenumber(n)
  return tonumber(n) or 0
end

local function handler(c, host, port)
  local peer = host .. ":" .. port
  luasyslog.log('LOG_INFO', "connection from "..peer)
  while true do
    local line = c:receive"*l"
    if not line then break end
    local cmd, len = string.match(line, '^(%S+)%s+(.*)')
    len = forcenumber(len)
    -- print('saw cmd '..cmd..' len:'..len)
    if not cmd or cmd ~= 'tdata' or len < 1 then
      c:send('NAK\n')
    else
      c:send('OK\n')
      line = c:receive(len)
      if not line then break end
      coroutine.resume(outputCoroutine, line)
      c:send('OK\n')
    end
  end
  luasyslog.log('LOG_INFO', "connection closed "..peer)
end

local opts = {
  ['prefix'] = { ['allow'] = 's', ['val'] = '', ['desc'] = 'File path prefix. Sequence numbers will be concatenated'},
  ['port'] = { ['allow'] = 'u', ['val'] = 0, ['desc'] = 'Port number to listen to.' },
  ['size'] = { ['allow'] = 'u', ['val'] = 0, ['desc'] = 'Maximum log file size.' },
  ['nfiles'] = { ['allow'] = 'u', ['val'] = 0, ['desc'] = 'Number of files or periods.'},
  ['interval'] = { ['allow'] = 'u', ['val'] = 0, ['desc'] = 'Time in seconds for each file.'},
}

local name = arg[0] or 'rollerserver'

local function usage()
   print(name..': Server to collect data and write to roll over file(s)')
   for k,v in pairs(opts) do
     print('     '..k..' <val> # '..v.desc)
   end
end

local function optbad(bad)
    luasyslog.log('LOG_ERR', bad)
    print(bad)
    os.exit(2)
end

local arg = { ... }  -- magic trick to get args in loadfile
local argc = 1
while arg[argc] do
  local k = arg[argc]
  argc = argc + 1
  local v = arg[argc]
  argc = argc + 1
  if not v then
    usage()
    optbad('option '..k..'has no value')
  end
  if opts[k] then
    opts[k]['val'] = v
  else
    usage()
    optbad('option '..k..'not valid')
  end
end

local valid = true
for k,v in pairs(opts) do
   local a = v.allow
   if a == 's' then
     if string.len(v.val) < 1 then
       print('parameter '..k..' not valid')
       valid = false
     end
   elseif a == 'u' then
     v.val = forcenumber(v.val)
     if v.val < 1 then
       print('parameter '..k..' not number > 0')
       valid = false
     end
   end
end

if not valid then
    usage()
    optbad('options not set or invalid: see stdout')
end

function scandir(prefix)
  local f = io.popen('ls -r '..prefix..'* 2>/dev/null', 'r')
  if not f then 
    luasyslog.log('LOG_ERR', "scandir is not working for - "..prefix)
    return 
  end
  local rv = f:read("*a")
  f:close()
  tabby = {}
  local from = 1
  local delim_from, delim_to = string.find( rv, "\n", from )
  while delim_from do
          table.insert( tabby, string.sub( rv, from , delim_from-1 ) )
          from = delim_to + 1
          delim_from, delim_to = string.find( rv, "\n", from )
  end
  -- table.insert( tabby, string.sub( rv, from ) )
  -- Comment out eliminates blank line on end!
  return tabby
end

local fileRollBaseName = opts.prefix.val
local fileRollPeriod   = opts.interval.val
local fileRollCount    = opts.nfiles.val
local fileRollPort     = opts.port.val
local fileRollSize     = opts.size.val

-- in case the directory part does not exist, create it
os.execute('mkdir -p `dirname '..fileRollBaseName..'` 2>/dev/null')

-- local itnum = 0

local logTime = 0

local baselen = string.len(fileRollBaseName)

local function catFileName(n)
  return fileRollBaseName..string.format("%05d",n)
end

-- check whether given file exists
-- @param name file name
-- @return true if the file exists; false otherwise
local function fileExists(name)
  local f = io.open(name,"r")
  if f~=nil then
    io.close(f)
    return true
  else
    return false
  end
end

-- roll stats files
local function rollStatsFiles()
  if fileRollCount <= 1 then
    -- In this (unexpected) case there will be only 1 stats file anyway.
    -- Erase that only file.
    local f = io.open(catFileName(1), 'w+')
    f.close()
    return
  end
  for i=fileRollCount-1,1,-1 do
    local fromFile = catFileName(i)
    if fileExists(fromFile) then
      local toFile = catFileName(i+1)
      if not os.rename(fromFile, toFile) then
        luasyslog.log('LOG_ERR', 'mv '..fromFile..' '..toFile..' failed')
      else
        dinfo('mv '..fromFile..' '..toFile..' done')
      end
    end
  end
end

local currentName = catFileName(1)

function getStatsRollName()
  local logFileSize = 0
  local file = io.open(currentName,'r')
  if file then
    logFileSize = file:seek("end")
    file:close()
  end
  local timeDiff = (turbo.util.gettimemonotonic() - logTime)/1000

  -- roll if log file size is more than 500K or if time interval is more than fileRollPeriod 
  if logFileSize >= fileRollSize or timeDiff >= fileRollPeriod then
    dinfo('log filesize = '..logFileSize..' log time diff = '..timeDiff)
    rollStatsFiles()
    logTime = turbo.util.gettimemonotonic()
    dinfo("created new log file "..currentName)
  end
  dinfo("writing on the same log file "..currentName)
  return currentName
end

function checkStatDir()
  local statfolder = scandir(fileRollBaseName)
  local filecount = #statfolder
  if filecount > fileRollCount then
	local delfilecount = filecount - fileRollCount
	dinfo("deleting "..delfilecount.." extra log files ")
  	for _,v in ipairs(statfolder) do
      		if (os.execute('rm '..v) ~= 0) then
        		luasyslog.log('LOG_ERR', 'Error deleting extra file '..v)
      		else
      			dinfo("deleted the extra log file "..v)
			delfilecount = delfilecount - 1
      		end
		if delfilecount <= 0 then
			break
		end
  	end
  end
	
end

checkStatDir()
copas.addserver(assert(socket.bind("127.0.0.1",fileRollPort)),
                function(c) return handler(copas.wrap(c), c:getpeername()) end
)
copas.loop()
