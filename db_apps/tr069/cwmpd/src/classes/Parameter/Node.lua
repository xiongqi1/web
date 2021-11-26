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
require('Parameter')
require('CWMP')
require('Int64')

----
-- Parameter Tree Node
----

local function addChild(self, child)
	child.parent = self
	table.insert(self.children, child)
end

local function getChild(self, name)
	name = tostring(name)
	for _, child in ipairs(self.children) do
		if child.name == name then return child end
	end
end

local function recursiveNotification(self)
	self:forAll(function(node)
		if node.type ~= 'root' then
			if client then
				if node.maxNotify == 3 then
					local path = node:getPath(true) or node.name
					client.tree:removeforcedInformParameter(path)
				end

				if node.handler == 'dynamic' then
					luardb.unset(node:getPath(true) .. '.@notify')
					client:removeNotification(node:getPath(true))
				end
			end
		end
	end, true)  -- we do not call the initilisers for default node subtrees
end

local function deleteChild(self, childToRemove)
	local newChildren = {}
	local pathToRemove = childToRemove:getPath()
	for _, child in ipairs(self.children) do
		if child:getPath() ~= pathToRemove then
			table.insert(newChildren, child)
		else
			recursiveNotification(child)
		end
	end
	self.children = newChildren
	childToRemove.parent = nil
end

local function deepCopy(self)
	local instance = Parameter.Node.new(self.name, self.type)
	instance:setNotify(self.notify, self.minNotify, self.maxNotify, self.defaultNotify)
	instance:setAccess(self.access)
	instance.arguments = self.arguments
	instance:setHandler(self.handler)
	instance:setDefault(self.default)
	instance.value = self.value
	for _, child in pairs(self.children) do
		instance:addChild(child:deepCopy())
	end
	return instance
end

local function createDefaultChild(self, instanceId)
	local instance

	if self.type ~= 'collection' then
		error('You may not createDefaultChild() of an node of type "' .. self.type .. '".')
	end

	local default = self:getChild('0')
	if default then
		-- deep-clone the default object if available
		instance = default:deepCopy()
		instance.type = 'object'
		instance.name = tostring(instanceId)
	else
		-- otherwise create instance object manually
		instance = Parameter.Node:new(instanceId, 'object')
	end

	-- the new clone of the default (or brand-new object)
	-- gets the handler of the collection it was spawned from
	-- this lets the collection handler manage instance lifecycle
	instance.arguments = self.arguments
	instance:setHandler(self.handler)
	self:addChild(instance)
	instance:addDynamicNotification()
	return instance
end

local function countInstanceChildren(self)
	local count = 0
	for _, child in ipairs(self.children) do
		if child.type == 'object' then count = count + 1 end
	end
	return count
end

local function maxInstanceChildId(self)
	local max = 0
	for _, child in ipairs(self.children) do
		if child.type == 'object' and tonumber(child.name) > max then max = tonumber(child.name) end
	end
	return max
end

local function setNotify(self, current, min, max, default)
	default = default or current
	assert(current >= min, 'Current notification ' .. current .. ' of "' .. (self:getPath(true) or self.name) .. '" is less than the minimum ' .. min .. '.')
	assert(current <= max, 'Current notification ' .. current .. ' of "' .. (self:getPath(true)  or self.name) .. '" is greater than the maximum ' .. max .. '.')
	assert(default >= min, 'Default notification ' .. default .. ' of "' .. (self:getPath(true) or self.name) .. '" is less than the minimum ' .. min .. '.')
	assert(default <= max, 'Default notification ' .. default .. ' of "' .. (self:getPath(true)  or self.name) .. '" is greater than the maximum ' .. max .. '.')
	self.notify = current
	self.defaultNotify = default
	self.minNotify = min
	self.maxNotify = max
end

local function setAccessList(self, accessList)
	assert(type(accessList) == 'table', 'Expected array table of access list strings.')
	self.accessList = accessList
end

local function setAccess(self, access)
	self.access = access
end

local function dynamicPersistNotify(self)
	local rdbKey=self:getPath(true)
	if not rdbKey then return end

	if self.notify ~= self.defaultNotify then
		luardb.set(rdbKey .. '.@notify', self.notify)
		luardb.setFlags(rdbKey .. '.@notify', Daemon.flagOR(luardb.getFlags(rdbKey .. '.@notify'), 'p'))
	else
		luardb.unset(rdbKey .. '.@notify')
	end
end
----
-- Dynamic Handler Mechanism
----

local dynamicHandlers = {}
setmetatable(dynamicHandlers, { __index = function(t, name)
	local handler = require('handlers.' .. name)
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
	-- dynamic(handler, default, ...)
	['dynamic'] = {
		['minArgCount'] = 2,
		['handler'] = 1,
		['default'] = 2
	},
	-- rdb(rdbKey, persist, min, max, validator, default)
	['rdb'] = {
		['argCount'] = 6,
		['rdbKey'] = 1,
		['rdbPersist'] = 2,
		['min'] = 3,
		['max'] = 4,
		['validator'] = 5,
		['default'] = 6
	},
	-- rdbobj(className, persist, idSelection, deletable)
	['rdbobj'] = {
		['argCount'] = 4,
		['rdbKey'] = 1,
		['rdbPersist'] = 2,
		['rdbIdSelection'] = 3,
		['deletable'] = 4
	},
	-- rdbmem(memberName, min, max, validator, default)
	['rdbmem'] = {
		['argCount'] = 5,
		['rdbField'] = 1,
		['min'] = 2,
		['max'] = 3,
		['validator'] = 4,
		['default'] = 5
	},
}

local function setHandler(self, handler)
	local handlerInfo = handlerTypes[handler]
	if not handlerInfo then
		error('Unknown handler type "' .. self.handler .. '".')
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
		error('Handler "' .. handler .. '" takes ' .. handlerInfo['argCount'] .. ' arguments (got ' .. #self.arguments .. ').')
	end
	if handlerInfo['minArgCount'] and #self.arguments < handlerInfo['minArgCount'] then
		error('Handler "' .. handler .. '" takes at least ' .. handlerInfo['minArgCount'] .. ' arguments.')
	end
	if handlerInfo['maxArgCount'] and #self.arguments > handlerInfo['maxArgCount'] then
		error('Handler "' .. handler .. '" takes at most ' .. handlerInfo['maxArgCount'] .. ' arguments.')
	end

	if handlerInfo['factoryReset'] then
		if self.arguments[handlerInfo['factoryReset']] == '1' then
			self.unsetOnFactoryDefault = true
		end
	end

	local handlerInfoFields = { 'rdbKey', 'rdbField', 'rdbPersist', 'rdbIdSelection', 'deletable', 'validator', 'min', 'max', 'default' }
	for _, name in ipairs(handlerInfoFields) do
		if handlerInfo[name] then
			self[name] = self.arguments[handlerInfo[name]]
		end
	end

	self.value = self.default
	self.handler = handler
end

local function addDynamicNotification(self)
	if client and client.tree then
		if self.maxNotify == 3 then
			local path = self:getPath(true) or self.name
			client.tree:addforcedInformParameter(path)
		end
	end

	if client and self.handler == 'dynamic' then
		local path = self:getPath(true) or self.name

		if self.notify == 1 then
			client:addNotification('passive', path)
		elseif self.notify == 2 then
			client:addNotification('active', path)
		end
	end
end

local function addArgument(self, argument)
	table.insert(self.arguments, argument)
end

local function setDefault(self, default)
	self.default = default
end

local function isObject(self)
	return (self.type == 'object' or self.type == 'collection' or self.type == 'default' or self.type == 'root')
end

local function isParameter(self)
	return (self.type == 'string'
		or self.type == 'int'
		or self.type == 'uint'
		or self.type == 'long'
		or self.type == 'ulong'
		or self.type == 'bool'
		or self.type == 'datetime'
		or self.type == 'base64')
end

local function isWritable(self)
	return (self.access ~= 'readonly')
end

local function getPath(self, leafObjExtend)
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

-- get alternative names of node.
--
-- This is for TR-157(TR-157_Amendment-10 : A.3.2, A.3.2.1).
-- Conversion rules are described in TR-157.
--
-- alterName: alternative name for given reference
-- reference: reference to get alternative name
--
-- return: converted alternative name of node.
--         Or, nil, if node is not corresponding to reference.
local function getAlternativeName(self, alterName, reference)
	assert(type(reference) == "string" and reference ~= "" , "Invalid reference")
	local pathName = self:getPath(true)

	local isObjRef = string.match(reference, "%.$") and "." or ""
	local pathPattern = string.gsub(reference, "*", "(%%d+)") -- Wildcards is only for instance identifiers, so "*" is replaced with %d.

	if isObjRef == "" then -- If reference is parameter.
		pathPattern = pathPattern .. "$"
	end

	if not string.match(pathName, pathPattern) then -- Validation check between pathName and reference.
		return nil -- target path is not corresponding to reference
	end

	if not alterName or alterName == "" then -- Alternative name is empty. return pathName as it is.
		return pathName
	end

	if not string.match(reference, "([^%*]+)%*.*") then -- Path name does not include wildcards
		return alterName .. isObjRef .. string.gsub(pathName, pathPattern, "")
	end

	local instIdentity = table.concat({string.match(pathName, pathPattern)}, ".") or ""
	if instIdentity ~= "" then
		instIdentity = "." .. instIdentity
	end

	return alterName .. instIdentity .. isObjRef .. string.gsub(pathName, pathPattern, "")
end

local parameterTypeCWMPTypes = {
	['string']	= 'string',
	['int']		= 'int',
	['uint']	= 'unsignedInt',
	['long']	= 'long',
	['ulong']	= 'unsignedLong',
	['bool']	= 'boolean',
	['datetime']	= 'dateTime',
	['base64']	= 'base64'
}

local parameterTypeCWMPConverters = {
	['string']	= {
		nodeToCWMP = function(val) return val end,
		cwmpToNode = function(val) return val end,
	},
	['base64']	= {
		nodeToCWMP = function(val) return val end,
		cwmpToNode = function(val) return val end,
	},
	['int']	= {
		nodeToCWMP = function(val) return math.max(math.min((tonumber(val) or 0), 2147483647), -2147483648) end,
		cwmpToNode = function(val) return tostring(val) end,
	},
	['uint']	= {
		nodeToCWMP = function(val) return math.max(math.min((tonumber(val) or 0), 4294967295), 0) end,
		cwmpToNode = function(val) return tostring(val) end,
	},
	['long']	= {
		nodeToCWMP = function(val) return tobignumber(val) end,
		cwmpToNode = function(val) return tostring(val) end,
	},
	['ulong']	= {
		nodeToCWMP = function(val) return tobignumber(val) end,
		cwmpToNode = function(val) return tostring(val) end,
	},
	['bool']	= {
		nodeToCWMP = function(val) return (val == '1' or val == 'true') end,
		cwmpToNode = function(val) return (val and '1' or '0') end,
	},
	['datetime']	= {
		nodeToCWMP = function(val)
			--[[
			TR-069 specifies all times MUST be expressed in UTC unless
			explicitly stated otherwise. One exception is the CurrentLocalTime
			parameter which must be a local time.
			Internally, we represent a datetime as a string of epoc seconds
			optionally followed by a suffix 'L' which indicates it is a local
			time rather than UTC.
			--]]
			if not val or val == '' then return CWMP.unknownTime end
			local suffix = val:sub(-1, -1)
			if suffix:upper() == 'L' then
				-- we should present this as a local time
				return CWMP.formatEpocTime(val:sub(1,-2), true)
			end
			-- UTC time
			return CWMP.formatEpocTime(val)
		end,
		cwmpToNode = function(val)
			-- we only deal with UTC time
			local Y, M, D, h, m, s = val:match('^(%d+)-(%d+)-(%d+)T(%d+):(%d+):(%d+)')
			assert(Y, 'Date parse error "' .. tostring(val) .. '" is not a valid ISO-8601 datetime string.')
			return Daemon.utcTimeToEpoch(Y, M, D, h, m, s)
		end,
	},
}

-- convert a node value to CWMP value
local function nodeToCWMP(self, value)
	local converter = parameterTypeCWMPConverters[self.type]
	assert(converter, 'No parameter string value to CWMP type converter for node type "' .. self.type .. '".')
	assert(type(converter.nodeToCWMP) == 'function', 'Parameter string converter "' .. self.type .. '" is not a function.')
	return converter.nodeToCWMP(value)
end

-- convert a CWMP value to node value
local function cwmpToNode(self, value)
	local converter = parameterTypeCWMPConverters[self.type]
	assert(converter, 'No parameter CWMP type to string value converter for node type "' .. self.type .. '".')
	assert(type(converter.cwmpToNode) == 'function', 'Parameter string converter "' .. self.type .. '" is not a function.')
	return converter.cwmpToNode(value)
end

local function getCWMPType(self)
	if self:isParameter() then
		return parameterTypeCWMPTypes[self.type]
	end
end

local function getCWMPValue(self)
	local ret, value = self:getValue(self:getPath(true))
	assert(type(ret) == 'number', 'Expected integer return value from node:getValue() handler for ' .. self:getPath(true) .. ' ret=' .. ret .. ', got ' .. type(ret) .. '.')
	assert(type(value) == 'string', 'Error occurred on ' .. self:getPath(true) .. ' value="' .. ((value == nil) and "(nil)" or value) .. '".')

	if ret == 0 then
		value = self:nodeToCWMP(value)
	end
	return ret, value
end

local function setCWMPValue(self, value)
	local value = self:cwmpToNode(value)

	local ret, msg = self:setValue(self:getPath(true), value)
	assert(type(ret) == 'number', 'Expected integer return value from node setValue() handler, got ' .. type(ret) .. '.')

	if self.handler == 'dynamic' then
		self.value = value
	end
	return ret, msg
end

local function setCWMPAttributes(self, notify, accessList)
	assert(type(notify) == 'nil' or type(notify) == 'number', 'Expected notify as nil or a integer.')
	assert(type(accessList) == 'nil' or type(accessList) == 'table', 'Expected accessList as nil or a table.')

	if notify then
		self.notify = notify
	end

	if accessList then
		self:setAccessList(accessList)
	end

	local ret, msg
	if self.handler == 'dynamic' then
		self:dynamicPersistNotify()
		if self.notify == 1 then
			ret, msg = client:addNotification('passive', self:getPath(true))
		elseif self.notify == 2 then
			ret, msg = client:addNotification('active', self:getPath(true))
		else
			ret, msg = client:removeNotification(self:getPath(true))
		end
	else
		ret, msg = self:attributeChange(self:getPath(true))
		assert(type(ret) == 'number', 'Expected integer return value from node:attributeChange() handler, got ' .. type(ret) .. '.')
	end
	return ret, msg
end

local function forAll(self, callback, skipDefaultNodes)
	skipDefaultNodes = skipDefaultNodes or false
	if self.type == 'default' and skipDefaultNodes then return end
	local ret = callback(self)
	if not ret then
		for _, child in ipairs(self.children) do
			child:forAll(callback, skipDefaultNodes)
		end
	end
end

local function recursiveInit(self)
	self:forAll(function(node)
		if node.type ~= 'root' then
			local handlers = Parameter.findHandler(node.handlerRules, node:getPath())
			if handlers and handlers['init'] then
				local ret, msg = node:initValue(node:getPath())
				if ret ~= 0 then
					Logger.log('Parameter', 'error', 'node:initValue(' .. node:getPath() .. ') error ' .. tostring(ret) .. (msg and (': ' .. msg) or '.'))
				end
			end

			if handlers then
				if client and client.tree then
					if node.maxNotify == 3 then
						local path = node:getPath(true) or node.name
						client.tree:addforcedInformParameter(path)
					end
				end

				if node.handler == 'dynamic' then

					local notifyRdbKey=node:getPath(true) .. '.@notify'
					local rdbNotifyValue=luardb.get(notifyRdbKey)
					if rdbNotifyValue and rdbNotifyValue ~= '' then
						local persistedNotify = tonumber(rdbNotifyValue)
						if persistedNotify > node.maxNotify then
							Logger.log('Parameter', 'warning', 'persisted notify exceeds maximum.')
							persistedNotify = node.maxNotify
						elseif persistedNotify < node.minNotify then
							Logger.log('Parameter', 'warning', 'persisted notify is less than minimum.')
							persistedNotify = node.minNotify
						end
						node.notify=persistedNotify
					end

					if node.notify == 1 then
						client:addNotification('passive', node:getPath())
					elseif node.notify == 2 then
						client:addNotification('active', node:getPath())
					end
				end
			end
		end
	end, true)  -- we do not call the initilisers for default node subtrees
end

------
-- Main Parameter Value Calls
------
local function _handlerCall(self, method, name, ...)
	local handler = Parameter.findHandler(self.handlerRules, name)
	if not handler then
		error('No handler matching parameter "' .. name .. '".')
	elseif handler[method] then
		return handler[method](self, name, ...)
	elseif method ~= 'init' and method ~= 'attrib' then
		-- missing anything but the init and attrib methods is probably an error
		error('Not implemented ' .. method .. '(' .. name .. ').')
	else
		Logger.log('Parameter', 'warning', 'No ' .. method .. ' for parameter "' .. name .. '" handler.')
		return 0
	end
end

local function initValue(self, name)		return _handlerCall(self, 'init', name) end
local function getValue(self, name)		return _handlerCall(self, 'get', name) end
local function setValue(self, name, value)	return _handlerCall(self, 'set', name, value) end
local function attributeChange(self, name)	return _handlerCall(self, 'attrib', name) end
local function createInstance(self, name)	return _handlerCall(self, 'create', name) end
local function deleteInstance(self, name)	return _handlerCall(self, 'delete', name) end

local function validateValue(self, value)
	return Parameter.Validator.validateNodeValue(self, value)
end


------
-- Debug Dumping
------
local function __tostring(self, depth)
	depth = depth or 0
	local indent = string.rep("\t", depth)
	local ret = indent .. self.type .. '("' .. self.name .. '"'
	if not self:isObject() then
		ret = ret .. ', ' .. self.notify .. ', ' .. self.maxNotify .. ', ' .. self.access .. ', ' .. self.handler .. ', "' .. _G.tostring(self.value) .. '"'
	end
	ret = ret .. ')'

	if #self.children > 0 then
		ret = ret .. ' {\n'
		for _, child in ipairs(self.children) do
			ret = ret .. child:__tostring(depth+1)
		end
		ret = ret .. indent .. '};\n';
	else
		ret = ret .. ';\n'
	end

	return ret
end


Parameter.Node = {}

function Parameter.Node.new(name, type)
	local node = {
		name = tostring(name),
		type = type,

		notify = 0,
		defaultNotify = 0,
		minNotify = 0,
		maxNotify = 2,

		accessList = {},

		access = 'readonly',
		handler = 'none',
		handlerRules = {},
		arguments = {},
		unsetOnFactoryDefault = false,

		default = '',
		value = '',

		parent = nil,
		children = {},

		['addChild'] = addChild,
		['getChild'] = getChild,
		['deleteChild'] = deleteChild,

		['deepCopy'] = deepCopy,
		['createDefaultChild'] = createDefaultChild,
		['countInstanceChildren'] = countInstanceChildren,
		['maxInstanceChildId'] = maxInstanceChildId,

		['setNotify'] = setNotify,
		['setAccessList'] = setAccessList,
		['setAccess'] = setAccess,
		['setHandler'] = setHandler,
		['setDefault'] = setDefault,
		['addArgument'] = addArgument,
		['addDynamicNotification'] = addDynamicNotification,

		['isObject'] = isObject,
		['isParameter'] = isParameter,
		['isWritable'] = isWritable,
		['getPath'] = getPath,
		['getAlternativeName'] = getAlternativeName,
		['forAll'] = forAll,
		['recursiveInit'] = recursiveInit,
		['dynamicPersistNotify'] = dynamicPersistNotify,

		['initValue'] = initValue,
		['getValue'] = getValue,
		['setValue'] = setValue,
		['attributeChange'] = attributeChange,
		['createInstance'] = createInstance,
		['deleteInstance'] = deleteInstance,

		['validateValue'] = validateValue,

		['getCWMPType'] = getCWMPType,
		['getCWMPValue'] = getCWMPValue,
		['setCWMPValue'] = setCWMPValue,
		['setCWMPAttributes'] = setCWMPAttributes,

		['nodeToCWMP'] = nodeToCWMP,
		['cwmpToNode'] = cwmpToNode,

		['__tostring'] = __tostring
	}

	return node
end

return Parameter.Node
