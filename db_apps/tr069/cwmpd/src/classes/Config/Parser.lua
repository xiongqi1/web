----
-- Copyright (C) 2012 NetComm Wireless Limited.
--
-- This file or portions thereof may not be copied or distributed in any form
-- (including but not limited to printed or electronic forms and binary or object forms)
-- without the expressed written consent of NetComm Wireless Limited.
-- Copyright laws and International Treaties protect the contents of this file.
-- Unauthorized use is prohibited.
--
-- THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
-- FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
-- NETCOMM WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
-- INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
-- BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
-- OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
-- AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
-- THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
-- SUCH DAMAGE.
----
require('Config.Lexer')
require('Parameter.Node')

require('tableutil')

local function _die(self, errorMessage)
	local msg = 'Parse error: Line ' .. self.lexer.lineNumber .. ': ' .. errorMessage
	Logger.log('Config', 'error', msg)
	error(msg, 0)
end

local function _peek(self, acceptableTypes)
	local token, tokenType = self.lexer:next()
	if type(acceptableTypes) ~= 'table' then acceptableTypes = { acceptableTypes } end
	if table.contains(acceptableTypes, tokenType) then
		return token, tokenType
	end
	self.lexer:push(token, tokenType)
end

local function _expect(self, expectedTypes, errorMessage)
	local token, tokenType = self.lexer:next()
	if type(expectedTypes) ~= 'table' then expectedTypes = { expectedTypes } end
	if table.contains(expectedTypes, tokenType) then
		return token, tokenType
	end
	_die(self, errorMessage .. ' (got ' .. tokenType .. ' "' .. token .. '")')
end

local function parseObject(self, type)
	local name = _expect(self, { 'word', 'number', "value" }, 'Expected object name.')
	local obj = Parameter.Node.new(name, type)
	if type == 'collection' then
		self:parseHandler(obj)
	elseif type == 'object' then
		obj:addArgument('')
		obj:setHandler('const')
	end
	_expect(self, 'open-brace', 'Expected open-brace.')
	while self:parseDefinition(obj, false) do end
	_expect(self, 'semi-colon', 'Expected semi-colon.')
--	print('object', type)
	return obj
end

local function parseNotify(self, param)
	_expect(self, 'notify', 'Expected notification specification')
	_expect(self, 'open-paren', 'Expected open parenthesis')
	local current = 0 + _expect(self, 'number', 'Expected number')
	_expect(self, 'comma', 'Expected comma')
	local min = 0 + _expect(self, 'number', 'Expected number')
	_expect(self, 'comma', 'Expected comma')
	local max = 0 + _expect(self, 'number', 'Expected number')
	_expect(self, 'close-paren', 'Expected close parenthesis')
	param:setNotify(current, min, max)
--	print('notify')
end

local function parseAccess(self, param)
	local access = _expect(self, 'accessType', 'Expected access type')
	param:setAccess(access)
--	print('access')
end

local function parseHandler(self, param)
	local tokenType
	local handler = _expect(self, 'handlerType', 'Expected handler type')
	_expect(self, 'open-paren', 'Expected open parenthesis')

	if _peek(self, 'close-paren') then return end
	
	while true do
		local arg, tokenType = _expect(self, { 'value', 'number', 'null', 'close-paren' }, 'Expected handler argument.')
		if tokenType == 'null' then arg = '' end
		param:addArgument(arg)
		local _, tokenType = _expect(self, { 'comma', 'close-paren' }, 'Expected comma or close parenthesis.')
		if tokenType == 'close-paren' then break end
	end
	local ret, msg = pcall(param.setHandler, param, handler)
	if not ret then
		_die(self, 'Parameter setHandler() error: ' .. msg)
	end
--	print('handler')
end

local function parseParam(self)
	local name = _expect(self, { 'word', 'number', 'num-word' }, 'Expected parameter name.') -- allows for names starting with a digit
	local paramType = _expect(self, 'paramType', 'Expected parameter type.')
	local obj = Parameter.Node.new(name, paramType)
	self:parseNotify(obj)
	self:parseAccess(obj)
	self:parseHandler(obj)
	_expect(self, 'semi-colon', 'Expected semi-colon')
--	print('parameter')
	return obj
end

local function parseDefault(self)
	_expect(self, 'open-brace', 'Expected open-brace.')
	local obj = Parameter.Node.new('0', 'default')
	while self:parseDefinition(obj, false) do end
	_expect(self, 'semi-colon', 'Expected semi-colon.')
	obj:setAccess('readwrite') -- default can be deleted
--	print('default')
	return obj
end

local function parseDefinition(self, parent, isRoot)
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
		_die(self, 'Expected object or param, got ' .. type .. '; "' .. token .. '".')
	end
	
	parent:addChild(node)
	
--	print('definition')
	return true
end


Config.Parser = {}

function Config.Parser.parse(filename)
	local parser = {
		['parseObject'] = parseObject,
		['parseNotify'] = parseNotify,
		['parseAccess'] = parseAccess,
		['parseHandler'] = parseHandler,
		['parseParam'] = parseParam,
		['parseDefault'] = parseDefault,
		['parseDefinition'] = parseDefinition,
	}
	parser.lexer = Config.Lexer.new(filename)
	parser.root = Parameter.Node.new('', 'root')
	repeat
		ret = parser:parseDefinition(parser.root, true)
	until not ret
	return parser.root
end

return Config.Parser
