require('luardb')
require('luasyslog')

rdbevent = {}
rdbevent.debug = false

local listenerId = 0
local watches = {}

local function _rdbVarChange(key, value)
	if rdbevent.debug then luasyslog.log('debug', 'No watcher for key "' .. key .. '".') end

	-- find associated watcher
	local watch = watches[key]
	if not watch then
		if rdbevent.debug then luasyslog.log('error', 'No watcher for key "' .. key .. '".') end
		return
	end
	
	-- work out event type
	local eventType
	if watch.value == nil then
		-- created (previously non-existant)
		-- well created is a bit of a hack as the current RDB implementation poorly handles this case
		eventType = 'create'
	elseif watch.value == value then
		-- touched (but unchanged)
		eventType = 'touch'
	elseif value == nil then
		-- deleted
		eventType = 'delete'
	else
		-- changed
		eventType = 'change'
	end
	
	-- deliver the notifications
	for _, listener in ipairs(watch.listeners) do
		if listener.event == eventType or listener.event == 'event' then
			ret, msg = pcall(listener.callback, key, value, watch.value, eventType)
			if not ret then
				luasyslog.log('error', 'Listener ' .. listener.id .. ' error: ' .. msg)
				luasyslog.log('error', 'Watcher "' .. key .. '" event "' .. eventType .. '".')
			end
		end
	end
	
	watch.value = value
end

local function _addWatch(key, event, callback)
	local watch = watches[key]
	if not watch then
		watch = {
			key = key,
			value = luardb.get(key),
			listeners = {}
		}
		watches[key] = watch
		luardb.watch(key, _rdbVarChange)
	end
	listenerId = listenerId + 1
	local lid = listenerId
	watch.listeners[lid] = {
		id = lid,
		event = event,
		callback = callback
	}
	
	return lid
end

function rdbevent.removeWatch(id)
	for _, watch in pairs(watches) do
		for lid, listener in pairs(watch.listeners) do
			if lid == id then
				watch.listeners[lid] = nil
				return
			end
		end
	end
	error('No such watch, ID = ' .. id)
end

function rdbevent.onEvent(key, callback) return _addWatch(key, 'event', callback) end
function rdbevent.onCreate(key, callback) return _addWatch(key, 'create', callback) end
function rdbevent.onDelete(key, callback) return _addWatch(key, 'delete', callback) end
function rdbevent.onChange(key, callback) return _addWatch(key, 'change', callback) end
function rdbevent.onTouch(key, callback) return _addWatch(key, 'touch', callback) end

function rdbevent.poll()
	for _, watch in pairs(watches) do
		local val = luardb.get(watch.key)
		if val ~= watch.value then
			_rdbVarChange(watch.key, val)
		end
	end
end

return rdbevent
