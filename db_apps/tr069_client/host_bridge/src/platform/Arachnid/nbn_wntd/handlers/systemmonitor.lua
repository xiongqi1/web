----
-- Periodic Ping Daemon Config Parameters
----

local function getRDBKey(node, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['Enable']		= 'enable',
		['PingHost1']		= 'ping_address1',
		['PingHost2']		= 'ping_address2',
		['PingInterval']	= 'ping_period',
		['PingErrorInterval']	= 'ping_failure_period',
		['PingErrorsToReset']	= 'ping_failure_count',
		['ForceReboot']		= 'reboot_period'
	}
	if not fieldMapping[pathBits[#pathBits]] then
		error('SystemMonitor: unknown parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	return 'service.systemmonitor.' .. fieldMapping[pathBits[#pathBits]]
end

local function validateParameterValue(name, value)
	local pathBits = name:explode('.')
	local paramName = pathBits[#pathBits]
	local rules = {
		['Enable'] = { '0', '1', 'true', 'false' },
		['PingHost1'] = function(ip)
			if ip ~= '' and not isValidIP4(ip) then return true end
		end,
		['PingHost2'] = function(ip)
			if ip ~= '' and not isValidIP4(ip) then return true end
		end,
		['PingInterval'] = function(val)
			val = tonumber(val)
			if not val or val < 1 then return true end
		end,
		['PingErrorInterval'] = function(val)
			val = tonumber(val)
			if not val or val < 1 then return true end
		end,
		['PingErrorsToReset'] = function(val)
			val = tonumber(val)
			if not val or val < 1 then return true end
		end,
		['ForceReboot'] = function(val)
			val = tonumber(val)
			if not val or val < 0 then return true end
		end
	}
	
	local validator = rules[paramName]
	if not validator then
		error('SystemMonitor: No validator rule for parameter name "' .. paramName .. '".')
	end
	if type(validator) == 'table' then
		if not table.contains(validator, value) then return cwmpError.InvalidParameterValue end
	elseif type(validator) == 'function' then
		if validator(value) then return cwmpError.InvalidParameterValue end
	else
		error('SystemMonitor: Not sure how to handle this kind of validator? ' .. type(validator))
	end
	return 0
end

return {
	----
	-- Instance Fields
	----
	['**.SystemMonitor.*'] = {
		init = function(node, name, value)
			local key = getRDBKey(node, name)
			node.value = luardb.get(key)
			if node.value == nil then
				dimclient.log('info', 'SystemMonitor: defaulting ' .. key)
				node.value = node.default
			end
			local ret = validateParameterValue(name, node.value)
			if ret > 0 then
				dimclient.log('error', 'SystemMonitor: value "' .. node.value .. '" for ' .. key .. ' is invalid.')
				node.value = node.default
			end
			luardb.set(key, node.value, 'p')
			
			-- install watcher for changes
			local watcher = function(key, value)
				if value then
					dimclient.log('info', 'SystemMonitor: change notification for ' .. key)
					if value ~= node.value then
						dimclient.log('info', 'SystemMonitor: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						dimclient.setParameter(node:getPath(), value)
					end
				end
			end
			luardb.watch(key, watcher)
			return 0
		end,
		get = function(node, name)
			local key = getRDBKey(node, name)
			node.value = luardb.get(key)
			return node.value
		end,
		set = function(node, name, value)
			local ret = validateParameterValue(name, value)
			if ret > 0 then return ret end
			if node.type == 'bool' then
				if value == '1' or value == 'true' then value = '1' else value = '0' end
			end
			local key = getRDBKey(node, name)
			node.value = value
			luardb.set(key, value)
			return 0
		end,
		unset = cwmpError.funcs.InvalidArgument,
		create = cwmpError.funcs.InvalidArgument,
		delete = cwmpError.funcs.InvalidArgument
	},
}
