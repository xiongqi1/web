
eventTbl={}

function table.topurestr( tbl )
  local result, done = {}, {}
  for k, v in ipairs( tbl ) do
    table.insert( result, v )
    done[ k ] = true
  end
  for k, v in pairs( tbl ) do
    if not done[ k ] then
      table.insert( result, k .. "=" .. v )
    end
  end
  return table.concat( result, "," )
end

----
-- Bootstrap Event Marker
---
local function bootstrapSet()
	dimclient.log('info', 'event.bootstrapSet()')
	luasyslog.log('LOG_INFO', 'setting the bootstrap flag')
	luardb.set(conf.rdb.eventPrefix .. '.bootstrap', 1, 'p')
	luanvramDB.set_with_commit(conf.rdb.eventPrefix .. '.bootstrap', '1')
	return 0
end

local function bootstrapGet()
	dimclient.log('info', 'event.bootstrapGet()')
	return luanvramDB.get(conf.rdb.eventPrefix .. '.bootstrap') or 0
end

local function bootstrapUnset()
	dimclient.log('info', 'event.bootstrapUnset()')
	luasyslog.log('LOG_INFO', 'removing the bootstrap flag')
	luardb.set(conf.rdb.eventPrefix .. '.bootstrap', 0, 'p')
	luanvramDB.set_with_commit(conf.rdb.eventPrefix .. '.bootstrap', '0')
	return 0
end


----
-- Event Store
----
local function deleteAll()
	local queuePrefix = conf.rdb.eventPrefix .. '.queue.'
	dimclient.log('info', 'event.deleteAll()')
	for _, key in ipairs(luardb.keys(queuePrefix)) do
		if key:sub(1, queuePrefix:len()) == queuePrefix then
--			print('matchedKeyForDelete', key)
			luardb.unset(key)
		end
	end

	eventTbl = {}
	luanvramDB.set("tr069.eventQ", "");
	return 0
end

local function getAll()
	local queuePrefix = conf.rdb.eventPrefix .. '.queue.'
	local queueIndex = conf.rdb.eventPrefix .. '.next'
	local events = {}
	local keys = {}
	dimclient.log('info', 'event.getAll()')

	local nvramQstring=luanvramDB.get("tr069.eventQ")
	local tArray = nvramQstring:explode(',')

	for i, event in ipairs(tArray) do
		local tbuf=string.gsub(event:gsub("^%s+", ""), "%s+$", "")
		if tbuf ~= '' then
			luardb.set(queueIndex, tonumber(i) + 1, 'p')
			luardb.set(queuePrefix .. i, tbuf, 'p')
		end
	end

	for _, key in ipairs(luardb.keys(queuePrefix)) do
		local prefix = key:sub(1, queuePrefix:len())
		if prefix == queuePrefix then
			local suffix = key:sub(queuePrefix:len() + 1)
--			print('matchedKeyForGet', key, prefix, suffix)
			table.insert(keys, tonumber(suffix))
		end
	end
	table.sort(keys)
	for _, key in ipairs(keys) do
		local value = luardb.get(queuePrefix .. key)
		dimclient.log('info', 'event.getAll() found = ' .. value)
		table.insert(events, value)
	end
	return events
end

local function add(event)
	local queueIndex = conf.rdb.eventPrefix .. '.next'
	local queuePrefix = conf.rdb.eventPrefix .. '.queue.'

	dimclient.log('info', 'event.add(' .. event .. ')')
	local next = tonumber(luardb.get(queueIndex) or 1)
	luardb.set(queueIndex, next + 1, 'p')
	luardb.set(queuePrefix .. next, event, 'p')


	eventTbl[#eventTbl+1] = event
	local retVal = table.topurestr( eventTbl )

	if retVal ~= nil then
		luanvramDB.set("tr069.eventQ", retVal);
	else
		luanvramDB.set("tr069.eventQ", "");
	end

	return 0
end

----
-- Inform Cycle
----
local function informComplete(result)
	local last_inform_time=readIntFromFile('/proc/uptime')

	-- If /proc/uptime has type error, rdb variable is not updated.
	if last_inform_time == nil then return 1 end

	if result then
		luardb.set('tr069.last_inform', last_inform_time)
	else
		luardb.set('tr069.last_inform_failure', last_inform_time)
	end
end

local function reboot()
	luardb.set('service.system.reset', 1)
	return 0
end

local function factoryReset()
	os.execute("defaults.sh -r")
	os.execute("ralink_init clear 2860")
	os.execute("ralink_init renew 2860 /etc_ro/Wireless/RT2860AP/RT2860_default_vlan")
	os.execute("ralink_init renew 2860 /etc/dynamic_default_vlan")
	luardb.set('service.system.reset', 1)
	return 0
end

----
-- Tick
----
local function tick()
	-- in order to service luardb change events dimclient calls into
	-- lua periodically which allows the debug hooks utilised by luardb to actually do something.
--	dimclient.log('debug', 'tick!')
	return 0
end

return {
	['bootstrapSet'] = bootstrapSet,
	['bootstrapGet'] = bootstrapGet,
	['bootstrapUnset'] = bootstrapUnset,
	['deleteAll'] = deleteAll,
	['getAll'] = getAll,
	['add'] = add,
	['informComplete'] = informComplete,
	['reboot'] = reboot,
	['factoryReset'] = factoryReset,
	['tick'] = tick
}
