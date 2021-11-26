----
-- UNI-D Object Handler
----

local function getRDBKey(node, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['Enable']	= 'enable',
		['Status']	= 'status',
		['MaxBitRate']	= 'bitrate',
		['DuplexMode']	= 'duplex',
		['TaggingMode']	= 'tagging'
	}
	return conf.wntd.unidPrefix .. '.' .. pathBits[#pathBits - 1] .. '.' .. fieldMapping[pathBits[#pathBits]]
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
	}
	
	local values = rules[paramName]
	if not values then
		error('No value rule for parameter name "' .. paramName .. '".')
	end
	if not table.contains(values, value) then
		return cwmpError.InvalidParameterValue
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
			node.value = node.default
			return 0
		end,
		get = function(node, name)
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
				dimclient.log('info', 'UNI-D: change notification for ' .. key)
				if value ~= node.value then
					dimclient.log('info', 'UNI-D: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
					dimclient.setParameter(node:getPath(), value)
				end
			end
			luardb.watch(key, watcher)
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
				luardb.set(getRDBKey(node, name))
			end
			return 0
		end
	},
}
