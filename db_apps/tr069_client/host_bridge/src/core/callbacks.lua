conf = dofile('/usr/lib/tr-069/config.lua')

local _functions = {
	preSession = {},
	postSession = {},
	cleanup = {}
}

local function register(type, func)
	local functions = _functions[type]
	if not functions then error('No such callback type "' .. type .. '".') end

	for _, cbfunc in ipairs(functions) do
		if cbfunc == func then return 0 end
	end

	table.insert(functions, func)
end

local function runRegisteredCallbacks(type)
	local functions = _functions[type]
	if not functions then error('No such callback type "' .. type .. '".') end
	for _, func in ipairs(functions) do
		dimclient.log('info', 'doing callback...')
		local ret, msg = pcall(func)
		if not ret then
			dimclient.log('error', 'callback threw error: ' .. msg)
		end
	end
	_functions[type] = {}
end

local function threadDump()
	for id, _ in pairs(dimclient._threads) do
		dimclient.log('debug', 'thread id = ' .. id)
	end
end

local function gc()
	local beforeUsage = collectgarbage('count')
	dimclient.log('info', 'memoryUsageBefore = ' .. beforeUsage .. ' k');
	collectgarbage('collect');
	local afterUsage = collectgarbage('count')
	dimclient.log('info', 'memoryUsageAfter = ' .. afterUsage .. ' k');
	dimclient.log('info', 'recovered = ' .. (beforeUsage - afterUsage) .. ' k');
end

local function poll()
	-- call initilisers
	paramRoot:forAll(function(node)
		if node.type == 'default' then return true end -- we do not call the initilisers for default node subtrees
		local handlers = findHandler(node.handlerRules, node:getPath())
		if handlers and handlers['poll'] then
			node:pollValue(node:getPath())
		end
	end)

end

local function platypus_poll()
	local nameTbl = { "InternetGatewayDevice.LANDevice.1.Hosts.Host",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration.1.AssociatedDevice",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration.2.AssociatedDevice",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration.3.AssociatedDevice",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration.4.AssociatedDevice",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration.5.AssociatedDevice",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration.6.AssociatedDevice",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration.7.AssociatedDevice",
			"InternetGatewayDevice.LANDevice.1.WLANConfiguration.8.AssociatedDevice"
			}

	for i, name in ipairs(nameTbl) do
		local node = findNode(paramRoot, name)
		if node then
			node:pollValue(name)
		end
	end
end

local function initDoneCallback()
	dimclient.log('info', 'initDoneCallback()')
	luasyslog.log('LOG_INFO', 'init: complete')
	luardb.set('service.tr069.status', 'running')

	if not conf.platypus then
		poll()
	end

	threadDump()
	gc()
end

local function preSessionCallback()
	dimclient.log('info', 'preSessionCallback()')

	if conf.nvram then
		luanvramDB.init()
	end

	runRegisteredCallbacks('preSession')

	luardb.set('service.tr069.status', 'inform')

	luasyslog.log('LOG_INFO', 'inform: begin')

	threadDump()

end

local function postSessionCallback()
	dimclient.log('info', 'postSessionCallback()')

	if conf.nvram then
		luanvramDB.commit()
	end

	runRegisteredCallbacks('postSession')

	luardb.set('service.tr069.status', 'running')

	luasyslog.log('LOG_INFO', 'inform: end')

	if conf.platypus then
		platypus_poll()
	else
		poll()
	end

	threadDump()

end

local function cleanupCallback()

	luardb.set('service.tr069.status', 'running')

	dimclient.log('info', 'cleanupCallback()')

	runRegisteredCallbacks('cleanup')

	threadDump()

	gc()

	if conf.debug then
		print(paramRoot)
	end

end

local callbacks = {
	['register'] = register,
	['initDone']	= initDoneCallback,
	['preSession']	= preSessionCallback,
	['postSession']	= postSessionCallback,
	['cleanup']	= cleanupCallback
}

return callbacks
