#!/usr/bin/env lua
require('ConfParser')

if #arg < 1 then
	print('usage: event-machine.lua <config file>')
	os.exit(-1)
end

client = Dimark:new()

ret, config = pcall(ConfParser.new, ConfParser, arg[1])
if not ret then
	print('Parse error: ' .. conf)
	os.exit(1)
end

function tokenise(str)
	local i = 0
	local tokens = {}
	for w in string.gmatch(str, '[%w%.]+') do
		tokens[i] = w
		i = i + 1
	end
	return tokens
end

function find(node, path)
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

function cmd_error(args)
	print('Unknown command "' .. args[0] .. '"')
	for i = 0,#args do
		print(i, args[i])
	end
end

function cmd_init(args)
	print(config.root:generateConf())
end

function cmd_get(args)
	local node = find(config.root, args[1])
	print(args[1], node)
end

function cmd_set(args)
	local node = find(config.root, args[1])
	print(args[1], node)
end

local cmds = {
	['init'] = cmd_init,
	['get'] = cmd_get,
	['set'] = cmd_set,
}

io.write('% ')
for line in io.lines() do
	if line ~= '' then
		args = tokenise(line)
		if cmds[args[0]] then
			cmds[args[0]](args)
		else
			cmd_error(args)
		end
	end	
	io.write('% ')
end
print()
