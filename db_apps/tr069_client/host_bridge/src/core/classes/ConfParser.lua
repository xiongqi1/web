require('Node')
require('Lexer')
require('objectlua.init')
local Object = objectlua.Object
local Lexer = Lexer
local Node = Node
local _G = _G
Object:subclass(...)
module(...)

function initialize(self, file)
	Lexer.null = ''
	self.root = Node:new('', 'root')
	self.lexer = Lexer:new(file)
	repeat
		ret = self:parseDefinition(self.root, true)
	until not ret
end

function _die(self, errorMessage)
	_G.error('Line ' .. self.lexer:currentLine() .. ': ' .. errorMessage, 0)
end

function _peek(self, acceptableType)
	local token, type = self.lexer:next()
	if _G.type(acceptableType) ~= 'table' then acceptableType = { acceptableType } end
	for _, accept in _G.ipairs(acceptableType) do
		if type == accept then
			return token, type
		end
	end
	self.lexer:push(token, type)
end

function _expect(self, expectedType, errorMessage)
	local token, type = self.lexer:next()
	if _G.type(expectedType) ~= 'table' then expectedType = { expectedType } end
	for _, expected in _G.ipairs(expectedType) do
		if type == expected then
			return token, type
		end
	end
	self:_die(errorMessage)
end

function parseObject(self, type)
	local name = self:_expect({ 'word', 'number', "value" }, 'Expected object name.')
	local obj = Node:new(name, type)
	if type == 'collection' then
		self:parseHandler(obj)
	end
	self:_expect('open-brace', 'Expected open-brace.')
	while self:parseDefinition(obj, false) do end
	self:_expect('semi-colon', 'Expected semi-colon.')
--	_G.print('object', type)
	return obj
end

function parseNotify(self, param)
	self:_expect('notify', 'Expected notification specification')
	self:_expect('open-paren', 'Expected open parenthesis')
	local current = self:_expect('number', 'Expected number')
	self:_expect('comma', 'Expected comma')
	local max = self:_expect('number', 'Expected number')
	self:_expect('close-paren', 'Expected close parenthesis')
	param:setNotify(current, max)
--	_G.print('notify')
end

function parseAccess(self, param)
	local access = self:_expect('accessType', 'Expected access type')
	param:setAccess(access)
--	_G.print('access')
end

function parseHandler(self, param)
	local tokenType
	local handler = self:_expect('handlerType', 'Expected handler type')
	self:_expect('open-paren', 'Expected open parenthesis')

	if self:_peek('close-paren') then return end
	
	while true do
		local arg, tokenType = self:_expect({ 'value', 'number', 'null', 'close-paren' }, 'Expected handler argument.')
		if tokenType == 'null' then arg = '' end
		param:addArgument(arg)
		local _, tokenType = self:_expect({ 'comma', 'close-paren' }, 'Expected comma or close parenthesis.')
		if tokenType == 'close-paren' then break end
	end
	local ret, msg = _G.pcall(param.setHandler, param, handler)
	if not ret then
		self:_die(msg)
	end
--	_G.print('handler')
end

function parseParam(self)
	local name = self:_expect({ 'word', 'number' }, 'Expected parameter name.')
	local paramType = self:_expect({ 'paramType' }, 'Expected parameter type.')
	local obj = Node:new(name, paramType)
	self:parseNotify(obj)
	self:parseAccess(obj)
	self:parseHandler(obj)
	self:_expect('semi-colon', 'Expected semi-colon')
--	_G.print('parameter')
	return obj
end

function parseDefault(self)
	self:_expect('open-brace', 'Expected open-brace.')
	local obj = Node:new('0', 'default')
	while self:parseDefinition(obj, false) do end
	self:_expect('semi-colon', 'Expected semi-colon.')
--	_G.print('default')
	return obj
end

function parseDefinition(self, parent, isRoot)
	local node
	local token, type = self.lexer:next()
	if token == nil then return false end

	if type == 'object' then
		node = self:parseObject('object')
	elseif type == 'collection' then
		node = self:parseObject('collection')
	elseif type == 'default' and not isRoot then
		node = self:parseDefault()
	elseif type == 'param' then
		node = self:parseParam()
	elseif type == 'close-brace' and not isRoot then
		return false
	else
		self:_die('Expected object or param, got ' .. type .. '; "' .. token .. '".')
	end
	
	parent:addChild(node)
	
--	_G.print('definition')
	return true
end
