------
-- Meta
------

local function setMeta(name, metadata)
	dimclient.log('info', 'parameter.setMeta(' .. name .. ', ' .. metadata .. ')')

	luardb.set('69m.' .. name, metadata)

	return 0
end

local function getMeta(name)
	dimclient.log('info', 'parameter.getMeta(' .. name .. ')')
	local ret = luardb.get('69m.' .. name)
	dimclient.log('info', 'parameter.getMeta(' .. name .. ') = ' .. ret)
	return ret
end

local function unsetMeta(name)
	dimclient.log('info', 'parameter.unsetMeta(' .. name .. ')')

	luardb.unset('69m.' .. name);

	return 0
end


----
-- Init a Parameter
----
local function init(name, value)
	value = value or ''
	dimclient.log('info', 'parameter.init(' .. name .. ', ' .. value .. ')')

	local node = findNode(paramRoot, name)
	if not node then
		dimclient.log('error', 'Parameter "' .. name .. '" not found in config file.')
		luasyslog.log('LOG_ERR', 'init: parameter not found: ' .. name)
		return cwmpError.InvalidParameterName
	end

	if node.type == 'bool' and value ~= '0' and value ~= '1' then
		dimclient.log('error', 'parameter.init(' .. name .. ', ' .. value .. ') expected 0 or 1 for boolean parameter')
	elseif (node.type == 'uint' or node.type == 'datetime') and not value:match('^(%d+)$') then
		dimclient.log('error', 'parameter.init(' .. name .. ', ' .. value .. ') expected number for ' .. node.type .. ' parameter')
	elseif (node.type == 'int' ) and not value:match('^(-*%d+)$') then
		dimclient.log('error', 'parameter.init(' .. name .. ', ' .. value .. ') expected number for ' .. node.type .. ' parameter')
	end

	local ret, msg = node:initValue(name, value)
	if ret ~= 0 then
		ret = ret or ''
		msg = msg or ''
		dimclient.log('error', 'Parameter init FAILED, ret = ' .. ret .. ', msg = ' .. msg)
		luasyslog.log('LOG_ERR', 'parameter init failed: ' .. ret .. ': ' .. msg)
		return ret
	end

	return 0
end


----
-- Set a Parameter
----
local function set(name, value)
	value = value or ''
	dimclient.log('info', 'parameter.set(' .. name .. ', ' .. value .. ')')

	local node = findNode(paramRoot, name)
	if not node then
		dimclient.log('error', 'Parameter "' .. name .. '" not found in config file.')
		luasyslog.log('LOG_ERR', 'set: parameter not found: ' .. name)
		return cwmpError.InvalidParameterName
	end

	if node.type == 'bool' and value ~= '0' and value ~= '1' then
		dimclient.log('error', 'parameter.set(' .. name .. ', ' .. value .. ') expected 0 or 1 for boolean parameter')
	elseif (node.type == 'uint' or node.type == 'datetime') and not value:match('^(%d+)$') then
		dimclient.log('error', 'parameter.set(' .. name .. ', ' .. value .. ') expected number for ' .. node.type .. ' parameter')
	elseif (node.type == 'int' ) and not value:match('^(-*%d+)$') then
		dimclient.log('error', 'parameter.init(' .. name .. ', ' .. value .. ') expected number for ' .. node.type .. ' parameter')

	end

	local ret, msg = node:setValue(name, value)
	if ret ~= 0 then
		ret = ret or ''
		msg = msg or ''
		dimclient.log('error', 'Parameter set FAILED, ret = ' .. ret .. ', msg = ' .. msg)
		luasyslog.log('LOG_ERR', 'parameter set failed: ' .. ret .. ': ' .. msg)
		return ret
	end

	return 0
end

----
-- Get a Parameter
----
local function get(name)
	dimclient.log('info', 'parameter.get(' .. name .. ')')
	local node = findNode(paramRoot, name)
	if not node then
		dimclient.log('error', 'Parameter "' .. name .. '" not found in config file.')
		luasyslog.log('LOG_ERR', 'get: parameter not found: ' .. name)
		return cwmpError.InvalidParameterName
	end
	local ret = node:getValue(name)

	if type(ret) == 'number' then
		-- numeric returns are errors
		dimclient.log('error', 'parameter.get(' .. name .. '), error code returned = ' .. tostring(ret))
	elseif type(ret) == 'string' then
		-- string returns are normal values
		dimclient.log('info', 'parameter.get(' .. name .. '), ret = "' .. ret .. '"')

		-- validate in context of node type
		if node.type == 'bool'  then
			if ret =='' then ret ='0' end
			if ret ~= '0' and ret ~= '1' then
				dimclient.log('error', 'parameter.get(' .. name .. ') expected 0 or 1 for boolean parameter, got "' .. ret .. '"')
			end
		elseif (node.type == 'uint'  or node.type == 'datetime') and not ret:match('^(%d+)$') then
			dimclient.log('error', 'parameter.get(' .. name .. ') expected number for ' .. node.type .. ' parameter, got "' .. ret .. '"')
		elseif (node.type == 'int' ) and not ret:match('^(-*%d+)$') then
			dimclient.log('error', 'parameter.get(' .. name .. ') expected number for ' .. node.type .. ' parameter, got "' .. ret .. '"')

		end
	else
		-- anything else is unexpected
		dimclient.log('error', 'parameter.get(' .. name .. '), unexpected return type ' .. type(ret) .. ', ret = ' .. tostring(ret))
	end

	return ret
end

----
-- Unset a Parameter
----
local function unset(name)
	dimclient.log('info', 'parameter.unset(' .. name .. ')')

	local node = findNode(paramRoot, name)
	if not node then
		dimclient.log('error', 'Parameter "' .. name .. '" not found in config file.')
		luasyslog.log('LOG_ERR', 'unset: parameter not found: ' .. name)
		return cwmpError.InvalidParameterName
	end

	local ret, msg = node:unsetValue(name)
	if ret ~= 0 then
		msg = msg or ''
		dimclient.log('error', 'Parameter delete FAILED, ret = ' .. ret .. ', msg = ' .. msg)
		luasyslog.log('LOG_ERR', 'parameter delete failed: ' .. ret .. ': ' .. msg)
		return ret
	end

	return 0
end


----
-- New Collection Member Instance
----
local function create(name, id)
	dimclient.log('info', 'parameter.create(' .. name .. ', ' .. id .. ')')

	local node = findNode(paramRoot, name)
	if not node then
		dimclient.log('error', 'Collection "' .. name .. '" not found in config file.')
		luasyslog.log('LOG_ERR', 'create: parameter not found: ' .. name)
		return cwmpError.InvalidParameterName
	end

	local ret, msg = node:createInstance(name, id)
	if ret ~= 0 then
		ret = ret or ''
		msg = msg or ''
		dimclient.log('error', 'Collection instance creation FAILED, ret = ' .. ret .. ', msg = ' .. msg)
		luasyslog.log('LOG_ERR', 'object create failed: ' .. ret .. ': ' .. msg)
		return ret
	end

	return 0
end


----
-- Delete Collection Member Instance
----
local function delete(name)
	dimclient.log('info', 'parameter.delete(' .. name .. ')')

	local node = findNode(paramRoot, name)
	if not node then
		dimclient.log('error', 'Collection "' .. name .. '" not found in config file.')
		luasyslog.log('LOG_ERR', 'delete: parameter not found: ' .. name)
		return cwmpError.InvalidParameterName
	end

	local ret, msg = node:deleteInstance(name)
	if ret ~= 0 then
		msg = msg or ''
		dimclient.log('error', 'Collection instance deletion FAILED, ret = ' .. ret .. ', msg = ' .. msg)
		luasyslog.log('LOG_ERR', 'object delete failed: ' .. ret .. ': ' .. msg)
		return ret
	end

	return 0
end

----
-- Factory Reset
----
local function unsetAll()
	dimclient.log('info', 'parameter.unsetAll()')
	luasyslog.log('LOG_INFO', 'factory reset parameter clear')
	local action = function(node)
		if node.handler == 'persist' and node.unsetOnFactoryDefault then
			dimclient.log('debug', 'Deleting parameter "' .. node:getPath() .. '".')
			node:unsetValue(node:getPath())
		end
	end
	paramRoot:forAll(action)
	return 0
end

local function getnvram(name)
	dimclient.log('info', 'parameter.getnvram(' .. name .. ')')
	local ret = ''

	if conf.platypus then
		ret=luanvram.get(name)

		if ret == nil then return '' end
	end

	return ret
end

local function setrdb(name, value)
	dimclient.log('info', 'parameter.setrdb(' .. name .. ', ' .. value ..')')

	luardb.set(name, value)

	return 0
end


return {
	['getMeta'] = getMeta,
	['setMeta'] = setMeta,
	['unsetMeta'] = unsetMeta,
	['init'] = init,
	['get'] = get,
	['set'] = set,
	['unset'] = unset,
	['create'] = create,
	['delete'] = delete,
	['unsetAll'] = unsetAll,
	['getnvram'] = getnvram,
	['setrdb'] = setrdb
}
