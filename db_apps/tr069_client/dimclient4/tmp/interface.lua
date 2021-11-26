local basepath = '/home/alany/work/tr-069/Dimark_Client_4.0/tmp'
package.path = basepath .. '/?.lua;' .. package.path
package.cpath = basepath .. '/?.so;' .. package.cpath

dofile(basepath .. '/utils.lua')
dofile(basepath .. '/errors.lua')

require('luardb')
require('ConfParser')

local function findNode(node, path)
	local pathBits = path:explode('.')
	for _, bit in ipairs(pathBits) do
		if bit == '' then
			return node.children
		else
			node = node:getChild(bit)
			if not node then return nil end
		end
	end
	return node
end

local function log(...)
	print('LUA', ...)
end

local rdbChange = function(name, value)
	log('rdbChange', name, value)
	setParameter('InternetGatewayDevice.ManagementServer.PeriodicInformInterval', value)
end

local config

function initInterface()
	log('initInterface')

	-- load config
	ret, config = pcall(ConfParser.new, ConfParser, './lua/tr-069.conf')
	if not ret then
		log('Error parsing config file: ', config)
		os.exit(1)
	end
	
	-- generate config
	local file = io.open('./tmp/tmp.param', 'w')
	file:write(config.root:generateConf())
	file:close()
end

function storeParameterMeta(name, metadata)
	log('storeParameterMeta', name, metadata)

	luardb.set('69m.' .. name, metadata)

	return 0
end

function retrieveParameterMeta(name)
	log('retrieveParameterMeta', name)
	local ret = luardb.get('69m.' .. name)
	log('retrieveParameterMeta', ret)
	return ret
end

function removeParameterMeta(name)
	log('removeParameterMeta', name)

	luardb.unset('69m.' .. name);

	return 0
end



function storeParameterData(name, value)
	log('storeParameterData', name, value)

	local node = findNode(config.root, name)
	if not node then
		log('Parameter not found in config file.', name)
		return cwmpError.InvalidParameterName
	end

	local ret, msg = node:setValue(name, value)
	if ret then
		log('Parameter set FAILED', ret, msg)
		return ret
	else
		log('storeParameterData', value)
	end

	return 0
end

function retrieveParameterData(name)
	log('retrieveParameterData', name)
	local node = findNode(config.root, name)
	if not node then
		log('Parameter not found in config file.', name)
		return cwmpError.InvalidParameterName
	end
	local ret = node:getValue(name)
	log('retrieveParameterData', ret)

	return ret
end

function removeParameterData(name)
	log('removeParameter', name)

	local node = findNode(config.root, name)
	if not node then
		log('Parameter not found in config file.', name)
		return cwmpError.InvalidParameterName
	end

	local ret, msg = node:unsetValue(name)
	if ret then
		log('Parameter delete FAILED', ret, msg)
		return ret
	end

	return 0
end



function removeAllParameters()
	log('removeAllParameters')
	local action = function(node)
		node:unsetValue(node:getPath())
	end
	config.root:forAllByHandlerType('persist', action)
	return 0
end
