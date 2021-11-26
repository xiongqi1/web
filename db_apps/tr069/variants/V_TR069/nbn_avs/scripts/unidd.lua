#!/usr/bin/env lua
require('luardb')
require('luasyslog')
require('stringutil')
require('rdbqueue')
-- dofile('/usr/lib/tr-069/scripts/unid-class.lua')
dofile('unid-class.lua')

-- setup syslog facility
luasyslog.open('unidd', 'LOG_DAEMON')

-- our config file is shared with the TR-069 hostbridge
conf = dofile('/usr/lib/tr-069/config.lua')

local function _dbLock(doLock)
-- we already hold rdb.lock
end

local vlanQ = {}

local function addVlanQ(data)
  luasyslog.log('LOG_INFO', 'UNI-D add vlanQ : ' .. table.concat(data, ','))
  vlanQ:insert(data)
end

UNID.addQ = addVlanQ

-- create instances
unids = {}
for id = 1,conf.wntd.unidCount do
  -- pcall( UNID.getById(id) )
  local ret, msg = pcall(UNID.getById, id)
  if ret then
    unids[id] = msg
    luasyslog.log('LOG_INFO', 'UNI-D ' .. id .. ': initialised OK.')
  else
    luasyslog.log('LOG_ERR', 'UNI-D ' .. id .. ': initialisation failed: ' .. msg)
    os.exit(1) -- we exit at this point, not much else we can do...
  end
end

local action = { }

function action.CODES(v)
  luasyslog.log('LOG_INFO', 'UNID-Q codes :' .. v)
  os.execute('ar_codes -f ' ..v.. '|' .. conf.wntd.switchConfig)
end

function action.MODE(v)
  luasyslog.log('LOG_INFO', 'UNID-Q setunid :' .. v)
  os.execute(conf.wntd.set_unid..' do '..conf.wntd.switchConfig..' '..v)
end

function action.SHELL(v)
  luasyslog.log('LOG_INFO', 'UNID-Q shell :' .. v)
  os.execute('sh ' ..v.. '|'.. conf.wntd.switchConfig)
end

local function changedQueueProc()
  -- wait for changes
  while true do
    data = vlanQ.remove(1)
    if not data then
      break
    end
    for k, v in pairs(data) do
      luasyslog.log('LOG_INFO', 'UNID-Q :' .. k .. ' -> ' .. v)
      if not action[k] then
        error('Do not know how to do action "' .. k .. '".')
      end
      action[k](v)
    end
  end
end
local function collect_tag_changed()
	local tag_changed =0;
	local port_map = "";
	for _, unid in ipairs(unids) do
		if unid.tag_changed ~= 0 then tag_changed = tag_changed +1 end
		unid.tag_changed =0;
	end
	if tag_changed >0 then
		for id, unid in ipairs(unids) do
			if unid.tagging =='PriorityTagged' or unid.tagging == 'TaggedCos' then
				port_map = port_map .. " " ..id;
			end
		end
		luasyslog.log('LOG_INFO', 'pfilter port map'..port_map)
		local ccode = { ['CODES'] = 'pfilter ' .. port_map .." p 0 5" }
		addVlanQ(ccode)
	end
end
local function unidChangedCB(k, ids)
  if ids and ids ~= '' then
    luardb.lock()
    for _, id in ipairs(ids:explode(',')) do
      id = tonumber(id) or 0
      local unid = unids[id]
      if unid then
        -- pcall( unid:pollChange() )
        local ret, msg = pcall(unid.pollChange, unid)
        if ret then
          luasyslog.log('LOG_INFO', 'UNI-D ' .. unid.id .. ': changed OK.')
        else
          luasyslog.log('LOG_ERR', 'UNI-D ' .. unid.id .. ': change poll failed: ' .. msg)
        end
      else
        luasyslog.log('LOG_ERR', 'Change notification for unknown UNI-D ID: ' .. id)
      end
    end
    luardb.set(conf.wntd.unidPrefix .. '.changed', '')
    collect_tag_changed();
    luardb.unlock()
  end
  changedQueueProc()
end

-- we watch the "<unidPrefix>.changed" variable
-- the TR-069 client will put a comma seperated list
-- of UNI-D IDs which have been changed into this variable
luardb.watch(conf.wntd.unidPrefix .. '.changed', unidChangedCB)

-- wait for changes
while true do
  luardb.wait(conf.wntd.unidStatusPollInterval)
  for _, unid in ipairs(unids) do
    -- pcall( unid:pollStatus() )
    local ret, msg = pcall(unid.pollStatus, unid)
    if not ret then
      luasyslog.log('LOG_ERR', 'UNI-D ' .. unid.id .. ': status poll failed: ' .. msg)
    end
  end
end

