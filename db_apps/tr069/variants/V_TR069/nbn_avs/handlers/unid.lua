----
-- UNI-D Object Handler
----
require('Logger')
require('Daemon')
require('CWMP.Error')
require('Parameter.Validator')
require('luardb')
require('rdbevent')

local function getRDBKey(node, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['Enable']		= 'enable',
		['Status']		= 'status',
		['MaxBitRate']		= 'bitrate',
		['DuplexMode']		= 'duplex',
		['TaggingMode']		= 'tagging',
		['MACs']		= 'swStats.macs'
	}
	if not fieldMapping[pathBits[#pathBits]] then
		error('UNI-D: unknown parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	return conf.wntd.unidPrefix .. '.' .. pathBits[#pathBits - 1] .. '.' .. fieldMapping[pathBits[#pathBits]]
end

local function getStatsRDBKey(node, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['BytesSent']		= 'TxBytes',
		['BytesReceived']	= 'RxBytes',
		['FramesSent']		= 'TxFrames',
		['FramesReceived']	= 'RxFrames',
		['DiscardsRX']		= 'RxDiscard',
		['ErrorsTX']		= 'TxFrameErr',
		['ErrorsRX']		= 'RxFrameErr',
		['RateTXAverage']	= 'TxAvgRate',
		['RateRXAverage']	= 'RxAvgRate',
		['RateTXMinimum']	= 'TxMinRate',
		['RateRXMinimum']	= 'RxMinRate',
		['RateTXMaximum']	= 'TxPeakRate',
		['RateRXMaximum']	= 'RxPeakRate',
		['Epoch']		= 'resetStamp',
		['Timestamp']		= 'stamp'
	}
	if not fieldMapping[pathBits[#pathBits]] then
		error('UNI-D: unknown stats parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	return conf.wntd.unidPrefix .. '.' .. pathBits[#pathBits - 2] .. '.swStats.' .. fieldMapping[pathBits[#pathBits]]
end

local changedUnids = {}

local function rdbChangeNotify()
	if #changedUnids > 0 then
		Logger.log('Parameter', 'info', 'UNI-D: delivering change notifications: ' .. table.concat(changedUnids, ','))
		luardb.lock()
		luardb.set(conf.wntd.unidPrefix .. '.changed', table.concat(changedUnids, ','))
		luardb.unlock()
		changedUnids = {}
	else
		Logger.log('Parameter', 'warning', 'UNI-D: no change notifications to deliver')
	end
end

local function queueChange(id)
	id = tostring(id)
	local when = client.inSession and 'postSession' or 'postInit'

	if table.contains(changedUnids, id) then
		Logger.log('Parameter', 'debug', 'UNI-D: change notification ' .. id .. ' already queued')
	else
		table.insert(changedUnids, id)
		Logger.log('Parameter', 'info', 'UNI-D: queued change notification ' .. id .. ' (' .. when .. ')')
	end
	if not client:isTaskQueued(when, rdbChangeNotify) then
		client:addTask(when, rdbChangeNotify)
		Logger.log('Parameter', 'info', 'UNI-D: installed RDB change notifier task (' .. when .. ')')
	else
		Logger.log('Parameter', 'debug', 'UNI-D: RDB change notifier task already installed (' .. when .. ')')
	end
end

local function validateParameterValue(name, value)
	local pathBits = name:explode('.')
	local paramName = pathBits[#pathBits]
	local rules = {
		['Enable']	= { '0', '1', 'true', 'false' },
		['Status']	= { 'Disabled', 'Error', 'NoLink', 'Up' },
		['MaxBitRate']	= { 'Auto', '10', '100', '1000' },
		['DuplexMode']	= { 'Auto', 'Half', 'Full' },
		['TaggingMode']	= { 'DefaultMapped', 'PriorityTagged', 'DSCPMapped', 'Tagged' },
		['MACs']	= function(value) end,
	}
	
	local validator = rules[paramName]
	if not validator then
		error('UNI-D: No validator rule for parameter name "' .. paramName .. '".')
	end
	if type(validator) == 'table' then
		if not table.contains(validator, value) then return CWMP.Error.InvalidParameterValue end
	elseif type(validator) == 'function' then
		if validator(value) then return CWMP.Error.InvalidParameterValue end
	else
		error('UNI-D: Not sure how to handle this kind of validator? ' .. type(validator))
	end
	return 0
end

return {
	----
	-- Container Collection
	----
	['**.UNID'] = {
		init = function(node, name, value)
			Logger.log('Parameter', 'info', 'UNI-D: model init')
			node:setAccess('readonly') -- no creation of additional instances
			-- create instances
			for i = 1,conf.wntd.unidCount do
				Logger.log('Parameter', 'info', 'UNI-D: creating instance ' .. i)
				local instance = node:createDefaultChild(i)
				queueChange(i)
			end
			return 0
		end,
	},

	----
	-- Stats
	----
	['**.UNID.*.Stats'] = {
		-- nothing
	},
	['**.UNID.*.Stats.*'] = {
		init = function(node, name, value)
			node.rdbKey = getStatsRDBKey(node, name)
			node.value = luardb.get(node.rdbKey) or node.default
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.rdbKey) or node.default
			return 0, node.value
		end,
	},

	----
	-- Instance Object
	----
	['**.UNID.*'] = {
		init = function(node, name, value)
			node:setAccess('readonly') -- no deletion of instances
			queueChange(node.name)
			return 0
		end,
	},

	----
	-- Instance Fields
	----
	['**.UNID.*.*'] = {
		init = function(node, name, value)
			-- check RDB state and default if required
			node.rdbKey = getRDBKey(node, name)
			node.value = luardb.get(node.rdbKey)
			if node.value == nil then
				Logger.log('Parameter', 'info', 'UNI-D: defaulting ' .. node.rdbKey)
				node.value = node.default
			end
			local ret = validateParameterValue(name, node.value)
			if ret > 0 then
				Logger.log('Parameter', 'error', 'UNI-D: value "' .. node.value .. '" for ' .. node.rdbKey .. ' is invalid.')
				node.value = node.default
			end
			luardb.set(node.rdbKey, node.value, 'p')
			
			-- install watcher for changes
			local watcher = function(key, value)
				if value then
					Logger.log('Parameter', 'debug', 'UNI-D: change notification for ' .. key)
					if value ~= node.value then
						Logger.log('Parameter', 'info', 'UNI-D: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						client:asyncParameterChange(node, node:getPath(), value)
						node.value = value
					end
				end
			end
			rdbevent.onChange(node.rdbKey, watcher)
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.rdbKey) or node.default
			return 0, node.value
		end,
		set = function(node, name, value)
			local ret = validateParameterValue(name, value)
			if ret > 0 then return ret end
			node.value = value
			luardb.set(node.rdbKey, value)
			queueChange(node.parent.name)
			return 0
		end
	},
	
	----
	-- MaxFrameSize
	----
	['**.Networking.MaxFrameSize'] = {
		init = function(node, name, value)
			node.rdbKey = conf.wntd.unidPrefix .. '.max_frame_size'
			node.value = luardb.get(node.rdbKey)
			if node.value == nil then
				Logger.log('Parameter', 'info', 'UNI-D: defaulting max frame size: ' .. node.default)
				node.value = node.default
			end
			luardb.set(node.rdbKey, node.value, 'p')
			
			-- install watcher for changes
			local watcher = function(key, value)
				if value then
					Logger.log('Parameter', 'debug', 'UNI-D: change notification for ' .. key)
					if value ~= node.value then
						Logger.log('Parameter', 'info', 'UNI-D: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						client:asyncParameterChange(node, node:getPath(), value)
						node.value = value
					end
				end
			end
			rdbevent.onChange(node.rdbKey, watcher)
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.rdbKey) or node.default
			return 0, node.value
		end,
		set = function(node, name, value)
			mfs = tonumber(value)
			if mfs < 590 or mfs > 2048 then return CWMP.Error.InvalidParameterValue, 'MaxFrameSize must be in the range [590:2048].' end
			node.value = value
			luardb.set(node.rdbKey, value)
			queueChange('g')
			return 0
		end
	}
	
}
