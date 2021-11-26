----
-- UNI-D Object Handler
----

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

local function rdbChangeNofity()
	if #changedUnids > 0 then
		dimclient.log('info', 'UNI-D: delivering change notifications: ' .. table.concat(changedUnids, ','))
		luardb.lock()
		luardb.set(conf.wntd.unidPrefix .. '.changed', table.concat(changedUnids, ','))
		luardb.unlock()
		changedUnids = {}
	else
		dimclient.log('info', 'UNI-D: no change notifications to deliver')
	end
end

local function queueChange(id, callback)
	callback = callback or 'postSession'

	if table.contains(changedUnids, id) then
--		dimclient.log('info', 'UNI-D: change notification ' .. id .. ' already queued')
	else
		table.insert(changedUnids, id)
		dimclient.callbacks.register(callback, rdbChangeNofity)
		dimclient.log('info', 'UNI-D: queued change notification "' .. id .. '"')
	end
end

local function queueInstanceChange(node, name, callback)
	local pathBits = name:explode('.')
	local id = pathBits[#pathBits - 1]
	callback = callback or 'postSession'
	queueChange(id, callback)
end


local function validateParameterValue(name, value)
	local pathBits = name:explode('.')
	local paramName = pathBits[#pathBits]
	local rules = {
		['Enable']	= { '0', '1', 'true', 'false' },
		['Status']	= { 'Disabled', 'Error', 'NoLink', 'Up' },
		['MaxBitRate']	= { 'Auto', '10', '100', '1000' },
		['DuplexMode']	= { 'Auto', 'Half', 'Full' },
		['TaggingMode']	= { 'DefaultMapped', 'PriorityTagged', 'DSCPMapped', 'Tagged', 'Transparent' },
		['MACs']	= function(value) end,
	}
	
	local validator = rules[paramName]
	if not validator then
		error('UNI-D: No validator rule for parameter name "' .. paramName .. '".')
	end
	if type(validator) == 'table' then
		if not table.contains(validator, value) then return cwmpError.InvalidParameterValue end
	elseif type(validator) == 'function' then
		if validator(value) then return cwmpError.InvalidParameterValue end
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
			dimclient.log('info', 'UNI-D: model init')
			node:setAccess('readonly') -- no creation of additional instances
			-- create instances
			for i = 1,conf.wntd.unidCount do
				dimclient.log('info', 'UNI-D: creating instance ' .. i)
				local instance = node:createDefaultChild(i)
			end
			return 0
		end,
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.ResourceExceeded,
		delete = cwmpError.funcs.InvalidArgument
	},

	----
	-- Stats
	----
	['**.UNID.*.Stats'] = {
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.InvalidArgument,
		delete = cwmpError.funcs.InvalidArgument
	},
	['**.UNID.*.Stats.*'] = {
		init = function(node, name, value)
			-- skip default node instance
			if node.type == 'default' then
				node.value = node.default
				return 0
			end
			
			-- check RDB state and default if required
			node.key = getStatsRDBKey(node, name)
			node.value = luardb.get(node.key) or node.default
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.key) or node.default
			return node.value
		end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly
	},

	----
	-- Instance Object
	----
	['**.UNID.*'] = {
		init = function(node, name, value)
			node:setAccess('readonly') -- no deletion of instances
		end,
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.InvalidArgument,
		delete = cwmpError.funcs.InvalidArgument
	},

	----
	-- Instance Fields
	----
	['**.UNID.*.*'] = {
		init = function(node, name, value)
			-- skip default node instance
			if node.type == 'default' then
				node.value = node.default
				return 0
			end
			
			-- check RDB state and default if required
			local key = getRDBKey(node, name)
			node.value = luardb.get(key)
			if node.value == nil then
				dimclient.log('info', 'UNI-D: defaulting ' .. key)
				node.value = node.default
			end
			local ret = validateParameterValue(name, node.value)
			if ret > 0 then
				dimclient.log('error', 'UNI-D: value "' .. node.value .. '" for ' .. key .. ' is invalid.')
				node.value = node.default
			end
			luardb.set(key, node.value, 'p')
			
			-- install watcher for changes
			local watcher = function(key, value)
				if value then
					dimclient.log('info', 'UNI-D: change notification for ' .. key)
					if value ~= node.value then
						dimclient.log('info', 'UNI-D: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						dimclient.setParameter(node:getPath(), value)
					end
				end
			end
			luardb.watch(key, watcher)
			queueInstanceChange(node, name, 'preSession')
			return 0
		end,
		get = function(node, name)
			if node.type ~= 'default' then
				node.value = luardb.get(getRDBKey(node, name))
			end
			return node.value
		end,
		set = function(node, name, value)
			local ret = validateParameterValue(name, value)
			if ret > 0 then return ret end

			if node.type ~= 'default' then
				node.value = value
				luardb.set(getRDBKey(node, name), value)
				queueInstanceChange(node, name)
			end
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
				dimclient.log('info', 'UNI-D: defaulting max frame size: ' .. node.default)
				node.value = node.default
			end
			luardb.set(node.rdbKey, node.value, 'p')
			
			-- install watcher for changes
			local watcher = function(key, value)
				if value then
					dimclient.log('info', 'UNI-D: change notification for ' .. key)
					if value ~= node.value then
						dimclient.log('info', 'UNI-D: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						dimclient.setParameter(node:getPath(), value)
					end
				end
			end
			luardb.watch(node.rdbKey, watcher)
			return 0
		end,
		get = function(node, name)
			return node.value
		end,
		set = function(node, name, value)
			mfs = tonumber(value)
			if mfs < 590 or mfs > 2048 then return cwmpError.InvalidArgument end
			node.value = value
			luardb.set(node.rdbKey, value)
			queueChange('g')
			return 0
		end
	}
	
}
