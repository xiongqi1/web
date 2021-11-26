conf = dofile('/usr/lib/tr-069/config.lua')

package.path = conf.fs.code .. '/?.lua;' .. conf.fs.code .. '/classes/?.lua;' .. package.path
package.cpath = conf.fs.code .. '/?.so;' .. package.cpath

require('stringutil')
require('tableutil')
require('luardb')
require('rdbobject')
require('luasyslog')

dofile(conf.fs.code .. '/utils.lua')
dofile(conf.fs.code .. '/errors.lua')

require('ConfParser')

-- nvram conf for Platypus
if conf.nvram then
	require('luanvram')
	dofile(conf.fs.code .. '/luanvramDB.lua')
end

----
-- Initialise LUA Environment
--
-- At this point we have a global dimclient table with a few C functions in it
-- We have to populate this with a table of LUA callback functions
-- We then load our config file and generate the dimclient config from it
-- Upon returning to dimclient it will parse the generated file and start up
----
function initInterface()
	dimclient.log('info', 'initInterface()')

	luasyslog.open('tr-069', 'LOG_DAEMON')
	luasyslog.log('LOG_INFO', 'client start-up')

	-- setup dimclient children, callbacks, functions, etc
	dimclient.callbacks = require('callbacks')
	dimclient.event = require('event')
	dimclient.transfer = require('transfer')
	dimclient.parameter = require('parameter')

	-- nvram session commit system
-- 	if conf.nvram then
-- 		dimclient.callbacks.register("preSession", luanvramDB.init)
-- 		dimclient.callbacks.register("postSession", luanvramDB.commit)
-- 	end

	-- load config file
	-- parsing the config file builds param tree and sets up listeners for RDB changes
	local ret, configOrError = pcall(ConfParser.new, ConfParser, conf.fs.config)
	if not ret then
		dimclient.log('error', 'Error parsing config file: ' .. configOrError)
		os.exit(1)
	end
	paramRoot = configOrError.root

	-- call initilisers
	paramRoot:recursiveInit()

	if not conf.platypus then
		print(paramRoot)
	end

	-- add inform trigger
	luardb.set(conf.rdb.informTrigger, 0)
	luardb.watch(conf.rdb.informTrigger, function(k, v)
		if v then
			if v == '1' then
				dimclient.log('info', 'inform triggered')
				luasyslog.log('LOG_INFO', 'inform requested')
				local ret = dimclient.triggerNotification()
				dimclient.log('info', 'inform trigger, ret = ' .. tostring(ret))
				luardb.set(conf.rdb.informTrigger, 0)
			else
				dimclient.log('info', 'inform trigger change: ' .. v)
			end
		end
	end)

	-- generate config
	local file = io.open(conf.fs.generatedConfig, 'w')
	file:write(paramRoot:generateConf())
	file:close()
end
