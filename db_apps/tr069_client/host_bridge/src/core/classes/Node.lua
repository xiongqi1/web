require('objectlua.init')

local Object = objectlua.Object
local _G = _G
Object:subclass(...)
module(...)

function initialize(self, name, type)
	self.name = _G.tostring(name)
	self.type = type

	self.notify = 0
	self.maxNotify = 0
	self.instance = 0

	self.access = 'readonly'
	self.handler = 'none'
	self.handlerRules = {}
	self.arguments = {}
	self.unsetOnFactoryDefault = false

	self.default = ''
	self.value = ''

	self.parent = nil
	self.children = {}
end

function addChild(self, child)
	child.parent = self
	_G.table.insert(self.children, child)
end

function getChild(self, name)
	name = _G.tostring(name)
	for _, child in _G.ipairs(self.children) do
		if child.name == name then return child end
	end
end

function deleteChild(self, childToRemove)
	local newChildren = {}
	local pathToRemove = childToRemove:getPath()
	for _, child in _G.ipairs(self.children) do
		if child:getPath() ~= pathToRemove then
			_G.table.insert(newChildren, child)
		end
	end
	self.children = newChildren
	childToRemove.parent = nil
end

function deepCopy(self)
	local instance = _G.Node:new(self.name, self.type)
	instance:setNotify(self.notify, self.maxNotify)
	instance:setAccess(self.access)
	instance.arguments = self.arguments
	instance:setHandler(self.handler)
	instance:setDefault(self.default)
	instance.value = self.value
	for _, child in _G.ipairs(self.children) do
		instance:addChild(child:deepCopy())
	end
	return instance
end

function createDefaultChild(self, instanceId)
	local instance

	if self.type ~= 'collection' then
		_G.error('You may not createDefaultChild() of an node of type "' .. self.type .. '".')
	end

	local default = self:getChild('0')
	if default then
		-- deep-clone the default object if available
		instance = default:deepCopy()
		instance.type = 'object'
		instance.name = _G.tostring(instanceId)
	else
		-- otherwise create instance object manually
		instance = Node:new(instanceId, 'object')
	end

	instance.arguments = self.arguments
	instance:setHandler(self.handler)
	self:addChild(instance)
	return instance
end

function countInstanceChildren(self)
	local count = 0
	for _, child in _G.ipairs(self.children) do
		if child.type == 'object' then count = count + 1 end
	end
	return count
end

function maxInstanceChildId(self)
	local max = 0
	for _, child in _G.ipairs(self.children) do
		if child.type == 'object' and _G.tonumber(child.name) > max then max = _G.tonumber(child.name) end
	end
	return max
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
	-- none()
	['none'] = {
		['argCount'] = 0
	},
	-- const(default)
	['const'] = {
		['argCount'] = 1,
		['default'] = 1
	},
	-- transient(default)
	['transient'] = {
		['argCount'] = 1,
		['default'] = 1
	},
	-- persist(rdbKey, factoryReset, default)
	['persist'] = {
		['argCount'] = 3,
		['rdbKey'] = 1,
		['factoryReset'] = 2,
		['default'] = 3
	},
	-- dynamic(handler, default, ...)
	['dynamic'] = {
		['minArgCount'] = 2,
		['handler'] = 1,
		['default'] = 2
	},
	-- internal(readIdx, writeIdx, default)
	['internal'] = {
		['argCount'] = 3,
		['default'] = 3
	},
	-- rdb(rdbKey, min, max, validator, default)
	['rdb'] = {
		['argCount'] = 6,
		['rdbKey'] = 1,
		['rdbPersist'] = 2,
		['min'] = 3,
		['max'] = 4,
		['validator'] = 5,
		['default'] = 6
	},
	-- rdbobj(rdbKey, rdbField, default)
	['rdbobj'] = {
		['argCount'] = 3,
		['rdbKey'] = 1,
		['rdbField'] = 2,
		['default'] = 3
	},
}

function setHandler(self, handler)
	local handlerInfo = handlerTypes[handler]
	if not handlerInfo then
		_G.error('Unknown handler type "' .. self.handler .. '".')
	end

	-- get handler rules, dynamic handlers find theirs by their 1st argument
	if handler == 'dynamic' then
		-- dynamic handlers
		self.handlerRules = dynamicHandlers[self.arguments[handlerInfo['handler']]]
	else
		-- "built-in" handlers
		self.handlerRules = dynamicHandlers['_' .. handler]
	end

	-- validate handler arguments
	if handlerInfo['argCount'] and #self.arguments ~= handlerInfo['argCount'] then
		_G.error('Handler "' .. handler .. '" takes ' .. handlerInfo['argCount'] .. ' arguments (got ' .. #self.arguments .. ').')
	end
	if handlerInfo['minArgCount'] and #self.arguments < handlerInfo['minArgCount'] then
		_G.error('Handler "' .. handler .. '" takes at least ' .. handlerInfo['minArgCount'] .. ' arguments.')
	end
	if handlerInfo['maxArgCount'] and #self.arguments > handlerInfo['maxArgCount'] then
		_G.error('Handler "' .. handler .. '" takes at most ' .. handlerInfo['maxArgCount'] .. ' arguments.')
	end

	if handlerInfo['factoryReset'] then
		if self.arguments[handlerInfo['factoryReset']] == '1' then
			self.unsetOnFactoryDefault = true
		end
	end

	local handlerInfoFields = { 'rdbKey', 'rdbField', 'rdbPersist', 'validator', 'min', 'max', 'default' }
	for _, name in _G.ipairs(handlerInfoFields) do
		if handlerInfo[name] then
			self[name] = self.arguments[handlerInfo[name]]
		end
	end

	self.value = self.default
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

function isParameter(self)
	return (self.type == 'string' or self.type == 'int' or self.type == 'uint' or self.type == 'bool' or self.type == 'datetime' or self.type == 'base64')
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
	local getIdx = 1
	local setIdx = -1

	if not self:isObject() then
		initIdx = 1
	end

	if self.handler == 'const' or self.handler == 'transient' then
		getIdx = 1
		setIdx = 1
	elseif self.handler == 'dynamic' or self.handler == 'persist' or self.handler == 'rdb' or self.handler == 'rdbobj' then
		getIdx = 1
		setIdx = 1
	elseif self.handler == 'internal' then
		getIdx = self.arguments[1]
		setIdx = self.arguments[2]
	elseif self.handler == 'none' then
		-- nothing
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
		self.instance,
		self.notify,
		self.maxNotify,
		1,
		initIdx,
		getIdx,
		setIdx,
		'',
		self.value
	}

	return _G.table.concat(data, ';')
end

function generateConf(self)
	if self.type == 'collection' then
		self.instance = self:maxInstanceChildId()
	end
	local ret = self:_getConfLine() or ''
	if ret ~= '' then ret = ret .. '\n' end
	for _, child in _G.ipairs(self.children) do
		ret = ret .. child:generateConf()
	end
	return ret
end

function forAll(self, callback)
	local ret = callback(self)
	if not ret then
		for _, child in _G.ipairs(self.children) do
			child:forAll(callback)
		end
	end
end

function recursiveInit(self)
	self:forAll(function(node)
		if node.type == 'default' then return true end -- we do not call the initilisers for default node subtrees
		local handlers = _G.findHandler(node.handlerRules, node:getPath())
		if handlers and handlers['init'] then
			node:initValue(node:getPath(), node.default)
		end
	end)
end


------
-- Main Parameter Value Calls
------
local function _handlerCall(self, method, name, ...)
	local handler = _G.findHandler(self.handlerRules, name)
	--_G.print("_handlerCall", self, method, name);
	if not handler then
		_G.error('No handler matching parameter "' .. name .. '".')
	elseif handler[method] then
		return handler[method](self, name, ...)
	elseif self.type == 'object'  then
		-- do not report error, if this is object
		return _G.cwmpError.OK
	elseif method ~= 'init' then
			-- missing anything but init method is probably an error
		_G.error('Not implemented ' .. method .. '(' .. name .. ').')
	else
		_G.dimclient.log('debug', 'No ' .. method .. ' for parameter "' .. name .. '" handler.')
		return _G.cwmpError.OK
	end
end

function initValue(self, name, value)		return _handlerCall(self, 'init', name, value) end
function pollValue(self, name)			return _handlerCall(self, 'poll', name) end
function getValue(self, name)			return _handlerCall(self, 'get', name) end
function setValue(self, name, value)		return _handlerCall(self, 'set', name, value) end
function unsetValue(self, name)			return _handlerCall(self, 'unset', name) end

function createInstance(self, name, instanceId)	return _handlerCall(self, 'create', name, instanceId) end
function deleteInstance(self, name)		return _handlerCall(self, 'delete', name) end


------
-- Debug Dumping
------
function __tostring(self, depth)
	depth = depth or 0
	local indent = _G.string.rep("\t", depth)
	local ret = indent .. self.type .. '("' .. self.name .. '"'
	if not self:isObject() then
		ret = ret .. ', ' .. self.notify .. ', ' .. self.maxNotify .. ', ' .. self.instance .. ', ' .. self.access .. ', ' .. self.handler .. ', "' .. _G.tostring(self.value) .. '"'
	elseif self.type == 'collection' then
		ret = ret .. ', ' .. self.instance
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
