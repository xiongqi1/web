----
-- Validated RDB Variable <--> TR-069 Parameter Binding
----
local validators = {
	----
	-- Types
	----
	['string'] = function(node, value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)
		if max and string.len(value) > max then
			return cwmpError.InvalidParameterValue, 'Parameter length may not exceed ' .. max .. '.'
		end
		if min and string.len(value) < min then
			return cwmpError.InvalidParameterValue, 'Parameter length must be at least ' .. min .. '.'
		end
	end,
	['int'] = function(node, value)
		local num = tonumber(value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)
		if not num then
			return cwmpError.InvalidParameterType, 'Parameter is an integer, got a non-number.'
		end
		if max and num > max then
			return cwmpError.InvalidParameterValue, 'Parameter may not exceed ' .. max .. '.'
		end
		if min and num < min then
			return cwmpError.InvalidParameterValue, 'Parameter may not be less than ' .. min .. '.'
		end
	end,
	['uint'] = function(node, value)
		local num = tonumber(value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)
		if not num then
			return cwmpError.InvalidParameterType, 'Parameter is an unsigned integer, got a non-number.'
		end
		if num < 0 then
			return cwmpError.InvalidParameterType, 'Parameter is an unsigned integer, got a negative.'
		end
		if max and num > max then
			return cwmpError.InvalidParameterValue, 'Parameter may not exceed ' .. max .. '.'
		end
		if min and num < min then
			return cwmpError.InvalidParameterValue, 'Parameter may not be less than ' .. min .. '.'
		end
	end,
	['bool'] = function(node, value)
		local validValues = { '0', '1' }
		if not table.contains(validValues, value) then
			return cwmpError.InvalidParameterType, 'Parameter is an boolean, got a non-boolean.'
		end
	end,
	['datetime'] = function(node, value)
		-- FIXME: implement something here!
	end,
	['base64'] = function(node, value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)
		if max and string.len(value) > max then
			return cwmpError.InvalidParameterValue, 'Parameter length may not exceed ' .. max .. '.'
		end
		if min and string.len(value) < min then
			return cwmpError.InvalidParameterValue, 'Parameter length must be at least ' .. min .. '.'
		end
	end,

	----
	-- Custom Validators
	----
	['IPv4'] = function(node, value)
		local min = tonumber(node.min)
		if min and min == 0 and value == '' then return end
		if not isValidIP4(value) then return cwmpError.InvalidParameterValue end
	end,
	['IPv4Mask'] = function(node, value)
		local min = tonumber(node.min)
		if min and min == 0 and value == '' then return end
		if not isValidIP4Netmask(value) then return cwmpError.InvalidParameterValue end
	end,
	['URL'] = function(node, value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)
		if max and string.len(value) > max then
			return cwmpError.InvalidParameterValue, 'Parameter length may not exceed ' .. max .. '.'
		end
		if min and string.len(value) < min then
			return cwmpError.InvalidParameterValue, 'Parameter length must be at least ' .. min .. '.'
		end
		-- FIXME: add some actual URL validation
	end,
}

local function validate(node, value)
	local validatorType = node.type
	if node.validator ~= '' then validatorType = node.validator end
	local validator = validators[validatorType]
	if not validator then error('No validator of type "' .. validatorType .. '".') end
	if type(validator) ~= 'function' then error('Validator of type "' .. validatorType .. '" is not a function!') end
	return validator(node, value)
end

return {
	['**'] = {
		init = function(node, name, value)
			node.value = luardb.get(node.rdbKey)
			if node.value == nil then
				dimclient.log('info', 'rdb.init(' .. name .. '): defaulting rdb variable ' .. node.rdbKey .. ' = "' .. node.default .. '"')
				luardb.set(node.rdbKey, node.default)
				node.value = node.default
			else
				dimclient.log('debug', 'rdb.init(' .. name .. '): rdb value "' .. node.value .. '"')
			end
			if node.rdbPersist == '1' then
				luardb.setFlags(node.rdbKey, luardb.getFlags(node.rdbKey) .. 'p')
			end
			
			-- we only install a watcher if the variable supports active notification
			-- this allows stats vars to be read-thru and not keep dimclient busy
			if tonumber(node.maxNotify) > 1 then
				-- creating a function per var is perhaps a bit expensive?
				-- one might implement this using a table look-up or walk of config to map RDB key to TR-069 parameter path
				local watcher = function(key, value)
					if value then
						dimclient.log('info', 'rdb watcher: change notification for ' .. key)
						if value ~= node.value then
							dimclient.log('info', 'rdb watcher: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
							dimclient.setParameter(node:getPath(), value)
						else
							dimclient.log('info', 'rdb watcher: ' .. key .. ' is unchanged from "' .. node.value .. '"')
						end
					end
				end
				dimclient.log('info', 'rdb.init(' .. name .. '): installed watcher for rdb variable '.. node.rdbKey)
				luardb.watch(node.rdbKey, watcher)
			else
				dimclient.log('info', 'rdb.init(' .. name .. '): max passive notification, no watcher')
			end
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.rdbKey)
			dimclient.log('info', 'rdb.get(' .. name .. ') = luardb.get("' ..  node.rdbKey .. '") = "' .. node.value .. '"')
			return node.value
		end,
		set = function(node, name, value)
			dimclient.log('info', 'rdb.set(' .. name .. ', "' .. value .. '") = luardb.set("' ..  node.rdbKey .. '", ...)')
			local ret, msg = validate(node, value)
			if ret then
				dimclient.log('info', 'rdb.set(' .. name .. ', "' .. value .. '") error: ' .. msg)
				return ret
			end
			node.value = value
			luardb.set(node.rdbKey, node.value)
			if node.rdbPersist == '1' then
				luardb.setFlags(node.rdbKey, luardb.getFlags(node.rdbKey) .. 'p')
			end
			return 0
		end,
		unset = function(node, name) 
			dimclient.log('info', 'rdb.unset(' .. name .. ', "' .. value .. '") = luardb.unset("' ..  node.rdbKey .. '", ...)')
			node.value = node.default
			luardb.unset(node.rdbKey)
			return 0
		end,
		create = cwmpError.funcs.InvalidArgument,
		delete = cwmpError.funcs.InvalidArgument
	}
}
