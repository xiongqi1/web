

----
-- Bootstrap Event Marker
---
local function bootstrapSet()
	dimclient.log('info', 'event.bootstrapSet()')
	luasyslog.log('LOG_INFO', 'setting the bootstrap flag')
	luardb.set(conf.rdb.eventPrefix .. '.bootstrap', 1, 'p')
	return 0
end

local function bootstrapGet()
	dimclient.log('info', 'event.bootstrapGet()')
	return luardb.get(conf.rdb.eventPrefix .. '.bootstrap') or 0
end

local function bootstrapUnset()
	dimclient.log('info', 'event.bootstrapUnset()')
	luasyslog.log('LOG_INFO', 'removing the bootstrap flag')
	luardb.set(conf.rdb.eventPrefix .. '.bootstrap', 0, 'p')
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
	return 0
end

local function getAll()
	local queuePrefix = conf.rdb.eventPrefix .. '.queue.'
	local events = {}
	local keys = {}
	dimclient.log('info', 'event.getAll()')
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
		luardb.set('tr069.last_inform', os.date('%s'))
	else
		luardb.set('tr069.last_inform_failure', os.date('%s'))
	end
end

----
-- Reboot
----
local function reboot()
	luardb.set('service.system.reset', 1)
	return 0
end


----
-- Factory Reset
----
local function factoryReset()
	luardb.set('service.system.factory', 1)
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
