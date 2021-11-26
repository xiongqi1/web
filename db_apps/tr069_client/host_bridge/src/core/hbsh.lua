#!/usr/bin/lua
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
-- "Synthetic" Dimclient C-bindings
----
dimclient = {}
dimclient.log = function(level, msg)
	print('Log: ', level, msg)
end
dimclient.lock = function()
	print('Lock: Lock!')
end
dimclient.unlock = function()
	print('Lock: Unlock!')
end
dimclient.triggerNotification = function()
	print('Notification: Requested!')
end
dimclient.setParameter = function(name, value)
	print('Parameter: set', name, '"' .. value .. '"')
end
dimclient.addObject = function(name)
	print('Parameter: create', name)
end
dimclient.deleteObject = function(name)
	print('Parameter: delete', name)
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
	paramRoot:forAll(function(node)
		if node.type == 'default' then return true end -- we do not call the initilisers for default node subtrees
		local handlers = findHandler(node.handlerRules, node:getPath())
		if handlers and handlers['init'] then
			node:initValue(node:getPath(), node.default)
		end
	end)
	print(paramRoot)

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

function readline(prompt, continuePrompt)
	local tokens = {}
	local inQuote = nil
	local word = ''

	if prompt then io.write(prompt) end

	while true do
		local c = io.read(1)
		if not c then
			-- EOF
			if word == '' and #tokens < 1 then
				-- EOF with no pending input
				return nil
			end
			-- EOF with pending tokens
			if inQuote then
				print('\nSyntax error: Unterminated quoted string at EOF.')
				return nil
			end
			if #word > 0 then table.insert(tokens, word) end
			return tokens
		else
			if inQuote and c == inQuote then
				-- end of quoted string
				table.insert(tokens, word)
				inQuote = nil
				word = ''
			elseif not inQuote and (c == '"' or c == "'" or c == '`') then
				-- start of quoted string
				if #word > 0 then
					table.insert(tokens, word)
					word = ''
				end
				inQuote = c
			elseif c == ' ' and not inQuote then
				-- token gap
				if #word > 0 then table.insert(tokens, word) end
				word = ''
			elseif not c or (not inQuote and c == '\n') then
				-- EOL without continued quote (or EOF)
				if #word > 0 then table.insert(tokens, word) end
				return tokens
			else
				-- accumulate token
				word = word .. c
				if continuePrompt and c == '\n' then io.write(continuePrompt) end
			end
		end
	end
end

function findParameter(name)
	local node = findNode(paramRoot, name)
	if not node then error('Parameter not found: ' .. name) end
	return node
end

----
-- Shell Commands
----
commands = {
	dump = function(args)
		if #args > 1 then
			local node = findParameter(args[2])
			print(node)
		else
			print(paramRoot)
		end
	end,
	init = function(args)
		local node = findParameter(args[2])
		print(node:initValue(args[2], args[3]))
	end,
	get = function(args)
		local node = findParameter(args[2])
		print(node:getValue(args[2]))
	end,
	set = function(args)
		local node = findParameter(args[2])
		print(node:setValue(args[2], args[3]))
	end,
	unset = function(args)
		local node = findParameter(args[2])
		print(node:unsetValue(args[2]))
	end,
	create = function(args)
		local node = findParameter(args[2])
		print(node:createInstance(args[2], args[3]))
	end,
	delete = function(args)
		local node = findParameter(args[2])
		print(node:deleteInstance(args[2]))
	end,
}


----
-- Do It!
----
initInterface()
while true do
	args = readline('hbsh> ', '> ')
	if not args then break end
--	print(table.tostring(args))

	if args[1] and commands[args[1]] then
		local ret, msg = pcall(commands[args[1]], args)
		if not ret then print('Error: ' .. msg) end
	elseif args[1] then
		print('Unknown command: ' .. args[1])
	end
end
