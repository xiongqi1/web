#!/usr/bin/env lua
require('luardb')
require('luasyslog')
require('stringutil')
require('tableutil')

local turbo = require('turbo')

-- setup for our local includes
local us = debug.getinfo(1, "S").source:match[[^@?(.*[\/])[^\/]-$]]
if us then
  package.path = us.."?.lua;"..package.path 
else
  us = ''
end

-- setup syslog facility
luasyslog.open('mib-vars', 'LOG_DAEMON')

-- our config file is shared with the TR-069 hostbridge
conf = dofile('/usr/lib/tr-069/config.lua')
require('rdbtable')

assert(conf.stats.mibListener)
assert(conf.stats.unidSwitchStatsPrefix)
assert(conf.wntd.unidPrefix)

------------------------------------------------
local function dinfo(v)
  if conf.wntd.debug > 1 then
    luasyslog.log('LOG_INFO', v)
  end
end


local collect

local function resetValues(prefix)
  if not prefix then
    return
  end
  tab = rdbtable(prefix)
  if conf.wntd.debug > 2 then
    print('initial stats tab : '..table.tostring(tab))
  end
  collect.start()
  collect.set(prefix..'.resetStamp', os.time())
  for k,v in pairs(tab) do
    -- print('collect.set('..prefix..'.'..k..' , 0)')
    collect.set(prefix..'.'..k, 0)
    --if string.match(k, "^%d+$") then
  end
  collect.commit()
end

local calcs = {
   ['TxFcsErr'] = { 'TxOverSize', 
		    'TxUnderRun',
		    'TxLateCol' },
       ['TxOK'] = { 'Tx64Byte', 
		    'Tx128Byte',
		    'Tx256Byte',
		    'Tx512Byte',
		    'Tx1024Byte',
		    'Tx1518Byte',
		    'TxMaxByte', '-',
		    'TxLateCol',
		    'TxAbortCol',
		    'TxUnderRun',
		    'TxExcDefer' },
  ['TxUnicast'] = { 'TxOK', '-',
		    'TxBroad', 
		    'TxPause',
		    'TxMulti' },
      ['RxBad'] = { 'RxFragment',
		    'RxUndersize',
		    'RxFcsErr',
		    'RxAllignErr' },
       ['RxOK'] = { 'Rx64Byte',
		    'Rx128Byte',
		    'Rx256Byte',
		    'Rx512Byte',
		    'Rx1024Byte',
		    'Rx1518Byte',
		    'RxMaxByte', '-',
		    'Filtered',
		    'RxFcsErr',
		    'RxAllignErr' },
  ['RxUnicast'] = { 'RxOK', '-',
		    'RxBroad',
		    'RxPause',
		    'RxMulti' },
}

function deepcopy(object)
  local lookup_table = {}
  local function _copy(object)
    if type(object) ~= "table" then
      return object
    elseif lookup_table[object] then
      return lookup_table[object]
    end
    local new_table = {}
    lookup_table[object] = new_table
    for index, value in pairs(object) do
      new_table[_copy(index)] = _copy(value)
    end
    return setmetatable(new_table, getmetatable(object))
  end
  return _copy(object)
end

local function ops(tab, names)
  local val = 0
  local nextop = '+'
  for index, value in ipairs(names) do
    if value == '+' or value == '-' then
      nextop = value
    else
      local v = tab[value]
      -- print('val '..value..' is '..tostring(v))
      if v then
        v = tonumber(v)
        -- print('op '..val..nextop..v)
        if nextop == '+' then
          val = val + v
        else
          val = val - v
        end
      end
    end
  end
  return val
end

local function derive(tab)
  for k, v in pairs(calcs) do
    local use = ops(tab, v)
    -- print('port '..tab.port..' '..k..' is '..use)
    if use > 0 then
      tab[k] = ops(tab, v)
    end
  end
end

local avg_init = {  ['peak'] = 0,
                    ['min']  = 0,
                    ['avg']  = 0,
                    ['count'] = 0,
                    ['startval'] = 0,
                    ['startstamp'] = 0,
                    ['lastval'] = 0,
                    ['laststamp'] = 0,
                }

local avg = {}

avg['TxByte'] = deepcopy(avg_init)
avg['RxGoodByte'] = deepcopy(avg_init)

-- tuples of reporting name, 
local reportable = { 
		     ['stamp']      =  'stamp',
                     ['TxFrames']   =  'TxOK',
		     ['RxFrames']   =  'RxOK',
		     ['RxDiscard']  =  'Filtered',
		     ['TxBytes']    =  'TxByte',
		     ['RxBytes']    =  'RxGoodByte',
		     ['TxFrameErr'] =  'TxFcsErr',
		     ['RxFrameErr'] =  'RxBad' ,
		     ['TxPeakRate'] =  { 'TxByte',     'peak'},
		     ['RxPeakRate'] =  { 'RxGoodByte', 'peak'},
		     ['TxMinRate']  =  { 'TxByte',     'min'},
		     ['RxMinRate']  =  { 'RxGoodByte', 'min'},
		     ['TxAvgRate']  =  { 'TxByte',     'avg'},
		     ['RxAvgRate']  =  { 'RxGoodByte', 'avg'},
	}

local mibReportInterval
local checkReportInterval = false

local function forcenumber(n)
  return tonumber(n) or 0
end
 
local function newreport(unid)
  unid['avg'] = deepcopy(avg)
  for k,v in pairs(unid.avg) do
    local val
    val = 0
    if unid[k] then
        val = tonumber(unid[k])
    end
    v.lastval = val
    v.laststamp = unid.monotonic_timestamp or 0
  end
  unid.startReportStamp = unid.monotonic_timestamp or 0
end

local function calcavg(unid)
  if not unid.avg then
    newreport(unid)
  end
  for k,v in pairs(unid.avg) do
    local val, dval
    val = 0
    dval = 0
    if unid[k] then
        val = tonumber(unid[k])
    end
    if v.lastval then
      dval = val - v.lastval
    end
    if dval < 0 then
      dval = 0
    end
    local tval = unid.monotonic_timestamp or 0
    tval = tval - v.laststamp or 0
    tval = tval/1000
    if tval > 0 then 
      dval = dval / tval
    end
    if v.count == 0 then
      v.count = 1
      v.peak = dval
      v.min  = dval
      v.avg  = dval
      v.startval = dval
      v.startstamp = unid.monotonic_timestamp or 0
    else
      v.count = v.count + 1
      if dval > v.peak then
        v.peak = dval
      end
      if dval < v.min then
        v.min = dval
      end
      v.avg  = v.avg + (dval - v.avg) / v.count
    end
    v.lastval = val
    v.laststamp = unid.monotonic_timestamp or 0
  end
end

local function report(prefix, unid)
  collect.start()
  -- collect.set(prefix .. 'transition', 1)
  for k,v in pairs(reportable) do
    local val = 0
    if type(v) == 'string' then
      if unid[v] then
        val = unid[v]
      end
    elseif type(v) == 'table' then
      local name = v[1]
      local ent  = v[2]
      if unid.avg[name][ent] then
        val = math.floor(unid.avg[name][ent] + 0.5)
      end
    end
    collect.set(prefix .. k, val)
  end
  collect.commit()
end

local function stats(unid)
  if not unid.port or unid.port > 5  then
    return
  end
  derive(unid)
  dinfo('UNID port ' .. unid.port .. ' at '..os.date('%Y/%m/%d %H:%M:%S'))
  if conf.wntd.debug > 2 then
     for k,v in pairs(unid) do
        dinfo(k..' -> '..v)
     end
  end

  local prefix
  if unid.port == 0 then
    prefix = conf.stats.cpuStatsPrefix
  elseif unid.port == 1 then
    prefix = conf.stats.trunkStatsPrefix
  else
    prefix = conf.wntd.unidPrefix..'.'..tostring(unid.port - 1)..'.'..conf.stats.unidSwitchStatsPrefix
  end
  if not prefix then
    return
  end
  calcavg(unid)
  prefix = prefix .. '.'
  local start = forcenumber(unid.startReportStamp)
  if (unid.monotonic_timestamp - start)/1000 >= mibReportInterval - 1 then
    report(prefix, unid)
    newreport(unid)
  end
end

local function capture (cmd)
  local f = assert(io.popen(cmd, 'r'))
  local unids = {['port'] = {}}
  local pinfo
  while true do
    if checkReportInterval then
      checkReportInterval = false
      luardb.wait(0.01)
    end
    local s = f:read('*l')
    if not s then break end
    -- print('read ' .. s)
    local w,n = string.match(s, '^(%S+)%s+(%d+)')
    if w and n then
      if w == 'Port' then
        local port = tonumber(n)
        if unids.port and unids.port[port] then
          pinfo = unids.port[port]
        else
          pinfo = {}
        end
        pinfo.port = port
	-- unid.date = os.date('%Y/%m/%d %H:%M:%S')
        pinfo.stamp = os.time()
        pinfo.monotonic_timestamp = turbo.util.gettimemonotonic()
        unids.port[port] = pinfo
      elseif w == 'EndPort' then
        stats(pinfo)
        if pinfo.port == 6 then
          checkReportInterval = true
        end
        pinfo = nil
        -- unid = {}
      elseif pinfo then
        pinfo[w] = n
      end
    end
  end
  f:close()
end

local function resetAll()
  resetValues(conf.stats.cpuStatsPrefix)
  resetValues(conf.stats.trunkStatsPrefix)
  for i = 1,4 do
    resetValues(conf.wntd.unidPrefix..'.'..i..'.'..conf.stats.unidSwitchStatsPrefix)
  end
end

local function changedMibReportInterval()
  local interval
  interval = forcenumber(luardb.get('service.mibd.reportinterval'))
  if interval < 5 then
    interval = forcenumber(conf.stats.unidMibReportInterval)
    if interval < 5 then
      interval = 30
    end
  end
  mibReportInterval = interval
end

changedMibReportInterval()

local statsclient = assert(loadfile(us..'statsclient.lua'))
collect = statsclient()
collect.init(conf.stats.fileRollPort)

luardb.watch('service.mibd.reportinterval', changedMibReportInterval)

resetAll()

capture(conf.stats.mibListener)
