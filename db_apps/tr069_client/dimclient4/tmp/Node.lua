require('objectlua.init')
require('luardb')
local Object = objectlua.Object
local _G = _G
local luardb = luardb
Object:subclass(...)
module(...)

function initialize(self, name, type)
	self.name = name
	self.type = type

	self.notify = 0
	self.maxNotify = 0

	self.access = 'readonly'
	self.handler = 'transient'
	self.arguments = { }
	
	self.default = ''

	self.parent = nil
	self.children = {}
end

function addChild(self, child)
	child.parent = self
	_G.table.insert(self.children, child)
end

function getChild(self, name)
	for _, child in _G.ipairs(self.children) do
		if child.name == name then return child end
	end
end

function setNotify(self, current, max)
	self.notify = current
	self.maxNotify = max
end

function setAccess(self, access)
	self.access = access
end

----
-- Dynamic Handler Mechanism
----

local dynamicHandlers = {}
_G.setmetatable(dynamicHandlers, { __index = function(t, name)
	local handler = _G.require('handlers.' .. name)
	t[name] = handler
	return handler
end })

local handlerTypes = {
	['const'] = {
		['argCount'] = 1,
		['defaultArg'] = 1,
		['getValue'] = function(this, name) return this.value end,
		['setValue'] = function(this, name, value) 
			_G.print('LUA', 'Assigning a constant parameter?', name, value, '<-', this.value)
			this.value = value
		end,
		['unsetValue'] = function(this, name) 
			_G.print('LUA', 'Deleting a constant parameter?', name)
			this.value = this.default
		end
	},
	['transient'] = {
		['argCount'] = 1,
		['defaultArg'] = 1,
		['getValue'] = function(this, name) return this.value end,
		['setValue'] = function(this, name, value) this.value = value end,
		['unsetValue'] = function(this, name) this.value = this.default end
	},
	['persist'] = {
		['argCount'] = 2,
		['defaultArg'] = 2,
		['rdbKey'] = 1,
		['getValue'] = function(this, name)
			this.value = luardb.get(this.rdbKey)
			return this.value
		end,
		['setValue'] = function(this, name, value)
			this.value = value
			luardb.set(this.rdbKey, this.value)
		end,
		['unsetValue'] = function(this, name) 
			this.value = this.default
			luardb.unset(this.rdbKey)
		end
	},
	['dynamic'] = {
		['argCount'] = 2,
		['defaultArg'] = 2,
		['handlerArg'] = 1
	},
	['internal'] = {
		['argCount'] = 3,
		['defaultArg'] = 3,
		['getValue'] = function(this, name)
			_G.print('LUA', 'Fetching an internal parameter?', name, this.value)
			return this.value
		end,
		['setValue'] = function(this, name, value)
			_G.print('LUA', 'Assigning an internal parameter?', name, value, '<-', this.value)
			this.value = value
		end,
		['unsetValue'] = function(this, name)
			_G.print('LUA', 'Deleting an internal parameter?', name)
			this.value = this.default
		end
	}
}

function setHandler(self, handler)

	local handlerInfo = handlerTypes[handler]

	if handler == 'dynamic' then
		self.getValue = dynamicHandlers[self.arguments[handlerInfo['handlerArg']]].getValue
		self.setValue = dynamicHandlers[self.arguments[handlerInfo['handlerArg']]].setValue
		self.unsetValue = dynamicHandlers[self.arguments[handlerInfo['handlerArg']]].unsetValue
	else
		self.getValue = handlerInfo['getValue']
		self.setValue = handlerInfo['setValue']
		self.unsetValue = handlerInfo['unsetValue']
	end

	if not handlerInfo then
		_G.error('Unknown handler type "' .. self.handler .. '".')
	end

	if #self.arguments ~= handlerInfo['argCount'] then
		_G.error('Handler "' .. handler .. '" takes ' .. handlerInfo['argCount'] .. ' arguments.')
	end

	self.default = self.arguments[handlerInfo['defaultArg']]

	if handlerInfo['rdbKey'] then
		self.rdbKey = self.arguments[handlerInfo['rdbKey']]
		_G.print('get', self.rdbKey)
		self.value = luardb.get(self.rdbKey)
		if self.value == nil then
			_G.print('defaulting', self.rdbKey, self.default)
			luardb.set(self.rdbKey, self.default)
			self.value = self.default
		end
		local watcher = function(key, value)
			_G.print('changed', key, value)
		end
		_G.print('watch', self.rdbKey, watcher)
--		luardb.watch(self.rdbKey, watcher)
	else
		self.value = self.default
	end

	self.handler = handler
end

function addArgument(self, argument)
	_G.table.insert(self.arguments, argument)
end

function setDefault(self, default)
	self.default = default
end

function isObject(self)
	return (self.type == 'object' or self.type == 'collection' or self.type == 'default')
end

function getPath(self, leafObjExtend)
	leafObjExtend = leafObjExtend or false
	if not self.parent then return nil end;
	local prefix = self.parent:getPath(false)
	local suffix = ''
	if self:isObject() and leafObjExtend then
		suffix = '.'
	end
	if prefix then
		return prefix .. '.' .. self.name .. suffix
	else
		return self.name .. suffix
	end
end

function _getTypeId(self)
	local types = {
		[0] = 'root',
		[6] = 'string',
		[7] = 'int',
		[9] = 'uint',
		[18] = 'bool',
		[11] = 'datetime',
		[12] = 'base64',
		[98] = 'object',
		[99] = 'collection',
		[97] = 'default'
	}
	for id, type in _G.pairs(types) do
		if(type == self.type) then return id end
	end
	_G.error('No such paramType = "' .. self.type .. '"')
end

function _getConfLine(self)
	if self.type == 'root' then return nil end

	local initIdx = -1
	local getIdx = -1
	local setIdx = -1

	if not self:isObject() then
		initIdx = 1
	end

	if self.handler == 'const' or self.handler == 'transient' then
		getIdx = 1
		setIdx = 1
	elseif self.handler == 'dynamic' or self.handler == 'persist' then
		getIdx = 1
		setIdx = 1
	elseif self.handler == 'internal' then
		getIdx = self.arguments[1]
		setIdx = self.arguments[2]
	else
		_G.error('Unknown handler type "' .. self.handler .. '" at ' .. self.name .. '.')
	end

	if self.access == 'readonly' then
		setIdx = -1
	elseif self.access == 'readwrite' then
		-- get/set as above
	elseif self.access == 'writeonly' then
		getIdx = -1
	else
		_G.error('Unknown access type "' .. self.access .. '".')
	end

	local data = {
		self:getPath(true) or '',
		self:_getTypeId(),
		0,
		self.notify,
		self.maxNotify,
		1,
		initIdx,
		getIdx,
		setIdx,
		'',
		self.default
	}

	return _G.table.concat(data, ';')
end

function generateConf(self)
	local ret = self:_getConfLine() or ''
	if ret ~= '' then ret = ret .. '\n' end
	for _, child in _G.ipairs(self.children) do
		ret = ret .. child:generateConf()
	end
	return ret
end

function forAllByHandlerType(self, handlerType, callback)
	if self.handler == handlerType then
		callback(self)
	end
	for _, child in _G.ipairs(self.children) do
		child:forAllByHandlerType(handlerType, callback)
	end
end

function __tostring(self, depth)
	depth = depth or 0
	local indent = _G.string.rep("\t", depth)
	local ret = indent .. self.type .. '("' .. self.name .. '"'
	if not self:isObject() then
		ret = ret .. ', ' .. self.notify .. ', ' .. self.maxNotify .. ', "' .. self.access .. '", "' .. self.handler .. '"'
	end
	ret = ret .. ')'

	if #self.children > 0 then
		ret = ret .. ' {\n'
		for _, child in _G.ipairs(self.children) do
			ret = ret .. child:__tostring(depth+1)
		end
		ret = ret .. indent .. '};\n';
	else
		ret = ret .. ';\n'
	end

	return ret
end
