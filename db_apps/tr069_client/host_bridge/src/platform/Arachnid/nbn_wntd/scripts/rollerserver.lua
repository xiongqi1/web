#!/usr/bin/env lua

require('luasyslog')
require "copas"

luasyslog.open('rollerserver', 'LOG_DAEMON')

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
  local f = io.popen('ls '..prefix..'* 2>/dev/null', 'r')
  if not f then return end
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

-- in case the directory part does not exist, create it
os.execute('mkdir -p `dirname '..fileRollBaseName..'` 2>/dev/null')

-- local itnum = 0

local function getTimePeriod()
  return math.floor(os.time() / fileRollPeriod)
  -- itnum = itnum + 1
  -- return math.floor(itnum/3)
end

local previousTime = -4

local function catFileName(n)
  return fileRollBaseName..string.format("%05d",n)
end

local currentName = catFileName(1)

function getStatsRollName()
  local thisTime = getTimePeriod()
  if thisTime ~= previousTime then
    previousTime = thisTime
    local filetab = scandir(fileRollBaseName)
    -- print ("Directory contents")
    local last = #filetab 
    -- print('number of file is '..last)
    if last >= fileRollCount then
      for n,v in ipairs(filetab) do
        -- print (n,v)
        if not os.rename(v, catFileName(n-1)) then
           luasyslog.log('LOG_ERR', 'mv '..v..' '..catFileName(n-1)..' failed')
        end
      end
      -- print('all gone well, rm '..catFileName(0))
      os.remove(catFileName(0))
    else
       last = last + 1
    end
    if last < 1 then last = 1 end
    currentName = catFileName(last)
    luasyslog.log('LOG_INFO', "create new log file "..currentName)
  end
  return currentName
end

copas.addserver(assert(socket.bind("127.0.0.1",fileRollPort)),
                function(c) return handler(copas.wrap(c), c:getpeername()) end
)
copas.loop()
