require('objectlua.init')
local Object = objectlua.Object
local _G = _G
Object:subclass(...)
module(...)

function initialize(self, filename)
	self.file = _G.io.open(filename, 'r')
	self.lineNumber = 0
	self.buf = ''
	self.stack = {}
end

function _more(self)
	self.buf = _G.string.gsub(self.buf, '^%s+(.-)%s*$', '%1')
	while self.buf == '' do
		local line = self.file:read('*l')
		self.lineNumber = self.lineNumber + 1
		if line == nil then return nil end
		line = _G.string.gsub(line, '^%s+(.-)%s*$', '%1')
		line = _G.string.gsub(line, '^(.-)%#.*$', '%1')
		self.buf = line
	end
	return self.buf
end

function currentLine(self)
	return self.lineNumber
end

function _less(self, text)
	self.buf = text
end

function tokens(self)
	return function() return self:next() end
end

function push(self, token, type)
	_G.table.insert(self.stack, { ['token'] = token, ['type'] = type })
end

function next(self)
	if #self.stack > 0 then
		local token = _G.table.remove(self.stack)
		return token.token, token.type
	end

	local token, ret, rest
	local text = self:_more()

	if text == nil then return nil end

	local tokenTypes = {
		{ 'open-brace',		'^({)' },
		{ 'close-brace',	'^(})' },
		{ 'open-paren',		'^(%()' },
		{ 'close-paren',	'^(%))' },
		{ 'comma',		'^(,)' },
		{ 'semi-colon',		'^(;)' },

		{ 'value',		'^"([^"]*)"' },
		{ 'value',		"^'([^']*)'" },
		{ 'number',		'^(-?[%d]+)' },

		{ 'object',		'^(object)' },
		{ 'collection',		'^(collection)' },
		{ 'default',		'^(default)' },
		{ 'param',		'^(param)' },

		{ 'notify',		'^(notify)' },
		{ 'access',		'^(access)' },

		{ 'accessType',		'^(readonly)' },
		{ 'accessType',		'^(readwrite)' },
		{ 'accessType',		'^(writeonly)' },

		{ 'handlerType',	'^(none)' },
		{ 'handlerType',	'^(const)' },
		{ 'handlerType',	'^(transient)' },
		{ 'handlerType',	'^(persist)' },
		{ 'handlerType',	'^(dynamic)' },
		{ 'handlerType',	'^(internal)' },
		{ 'handlerType',	'^(rdbobj)' },
		{ 'handlerType',	'^(rdb)' },

		{ 'null',		'^(null)' },

		{ 'paramType',		'^(string)' },
		{ 'paramType',		'^(int)' },
		{ 'paramType',		'^(uint)' },
		{ 'paramType',		'^(bool)' },
		{ 'paramType',		'^(datetime)' },
		{ 'paramType',		'^(base64)' },

		{ 'word',		'^([_%a][-_%w]+)' }
	}

	for _, rule in _G.ipairs(tokenTypes) do
		local type = rule[1]
		local pattern = rule[2]
		pattern = pattern .. '(.*)$'
		token, rest = text:match(pattern)
--		_G.print('match:', pattern, text, token)
		if(token ~= nil) then
			self:_less(rest)
--			_G.print('token(' .. type .. ', "' .. token .. '")')
			return token, type
		end
	end

	_G.error('Line ' .. self.lineNumber .. ': Lexer error: Can not parse into tokens "' .. text .. '".')
end
