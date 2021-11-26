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
require('Config')

----
-- Config File Tokeniser
----

-- whitespace and comment skipping fetch of line from file
local function _more(self)
	self.buffer = string.gsub(self.buffer, '^%s+(.-)%s*$', '%1')
	while self.buffer == '' do
		local line = self.file:read('*l')
		self.lineNumber = self.lineNumber + 1
		if line == nil then return nil end
		line = string.gsub(line, '^%s+(.-)%s*$', '%1')
		line = string.gsub(line, '^(.-)%#.*$', '%1')
		self.buffer = line
	end
	return self.buffer
end

-- return unconsumed part of line back to buffer
local function _less(self, text)
	self.buffer = text
end

-- token iterator
local function tokens(self)
	return function() return self:next() end
end

-- unget a token
local function push(self, token, tokenType)
	table.insert(self.stack, { ['token'] = token, ['type'] = tokenType })
end

-- get next token
local function next(self)
	-- anything on stack? return it first
	if #self.stack > 0 then
		local token = table.remove(self.stack)
		return token.token, token.type
	end

	local token, ret, rest
	local text = _more(self)

	-- check for EOF
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
		{ 'num-word',		'^([%d]+[-_%a][-_%w]*)' }, --word starting with number, this must be placed before number
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
		{ 'handlerType',	'^(dynamic)' },
		{ 'handlerType',	'^(rdbobj)' },
		{ 'handlerType',	'^(rdbmem)' },
		{ 'handlerType',	'^(rdb)' },

		{ 'null',		'^(null)' },

		{ 'paramType',		'^(string)' },
		{ 'paramType',		'^(int)' },
		{ 'paramType',		'^(uint)' },
		{ 'paramType',		'^(long)' },
		{ 'paramType',		'^(ulong)' },
		{ 'paramType',		'^(bool)' },
		{ 'paramType',		'^(datetime)' },
		{ 'paramType',		'^(base64)' },

		{ 'word',		'^([_%a][-_%w]+)' }
	}

	-- match token
	for _, rule in ipairs(tokenTypes) do
		local tokenType = rule[1]
		local pattern = rule[2]
		pattern = pattern .. '(.*)$'
		token, rest = text:match(pattern)
--		print('match:', pattern, text, token)
		if(token ~= nil) then
			_less(self, rest)
--			print('token(' .. tokenType .. ', "' .. token .. '")')
			return token, tokenType
		end
	end

	-- lexing failure
	error('Lexer error: Line ' .. self.lineNumber .. ': Can not parse "' .. text .. '" into tokens.')
end

Config.Lexer = {}

function Config.Lexer.new(filename)
	local file, msg = io.open(filename, 'r')
	if not file then error('Lexer error: Can not open "' .. filename .. '": ' .. msg) end
	local lexer = {
		file = file,
		lineNumber = 0,
		buffer = '',
		stack = {},
		tokens = tokens,
		push = push,
		next = next,
	}
	return lexer
end

return Config.Lexer
