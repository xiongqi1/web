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
require('CWMP')
require('CWMP.Array')
require('SOAP')
require('XML.StringBuffer')

-- __index() metamethod
local function get(self, key)
	local mt = getmetatable(self)
	assert(CWMP.memberExists(mt.type, key), 'CWMP type "' .. mt.type .. '" has no "' .. key .. '" member.')
	return mt.data[key]
end

-- __newindex() metamethod
local function set(self, key, value)
	local mt = getmetatable(self)
	assert(CWMP.memberExists(mt.type, key), 'CWMP type "' .. mt.type .. '" has no "' .. key .. '" member.')
	assert(CWMP.validMemberValue(mt.type, key, value), 'Invalid value for CWMP type "' .. mt.type .. '" member "' .. key .. '".')
	mt.data[key] = value
end

-- __tostring() metamethod
local function tostr(self, depth)
	depth = depth or 0
	local mt = getmetatable(self)
	local indent = string.rep('\t', depth)
	local ret = indent .. mt.name .. ' (object: ' .. mt.type .. '):\n'
	for idx, memberInfo in ipairs(mt.members) do
		local memberData = mt.data[memberInfo.name]
		if memberInfo.type.id == 'object' then
			-- object member
			if memberData then
				ret = ret .. CWMP.Object.tostring(memberData, depth + 1)
			else
				ret = ret .. indent .. '\t' .. memberInfo.name .. ' (' .. memberInfo.type.class .. '): nil\n'
			end
		elseif memberInfo.type.id == 'array' then
			-- array member
			if memberData then
				ret = ret .. CWMP.Array.tostring(memberData, depth + 1)
			else
				ret = ret .. indent .. '\t' .. memberInfo.name .. ' (array[]: ' .. memberInfo.type.type.id .. '): nil\n'
			end
		elseif memberInfo.type.id == 'anySimpleType' then
			-- anysimpleType member
			if mt.memberTypes[memberInfo.name] then
				ret = ret .. indent .. '\t' .. memberInfo.name .. ' (' .. mt.memberTypes[memberInfo.name] .. '): ' .. tostring(memberData) .. '\n'
			else
				ret = ret .. indent .. '\t' .. memberInfo.name .. ' (' .. memberInfo.type.id .. '): ' .. tostring(memberData) .. '\n'
			end
		else
			-- simpleType member
			ret = ret .. indent .. '\t' .. memberInfo.name .. ' (' .. memberInfo.type.id .. '): ' .. tostring(memberData) .. '\n'
		end
	end
	return ret
end

local function setMemberType(self, memberName, typeName)
	assert(CWMP.isSimpleType(typeName), 'Unknown CWMP simpleType "' .. typeName .. '".')
	local mt = getmetatable(self)
	local memberType = CWMP.getMemberType(mt.type, memberName)
	assert(memberType.id == 'anySimpleType', 'You may only setMemberType() object members of type "anySimpleType".')
	if mt.memberTypes[memberName] and typeName ~= mt.memberTypes[memberName] then error('Member type of anySimpleType member "' .. memberName .. '" of CWMP Object "' .. mt.name .. '" has already been set.') end
	mt.memberTypes[memberName] = typeName
end

local function getMemberType(self, memberName)
	local mt = getmetatable(self)
	if mt.memberTypes[memberName] then return mt.memberTypes[memberName] end
	return CWMP.getMemberType(mt.type, memberName).id
end

-- this is a hack to facilitate SetParameterValuesFault
local function addExtraChild(self, child)
	local mt = getmetatable(self)
	table.insert(mt.extraChildren, child)
end

local function toXML(self, depth, withNS, buffer)
	depth = depth or 0
	withNS = withNS or false
	buffer = buffer or XML.StringBuffer.new()
	local mt = getmetatable(self)
	local indent = string.rep('\t', depth);

	local ns = 'cwmp:'
	if mt.type == 'Fault' then ns = 'soap:' end

	if #mt.members < 1 then
		-- short form
		buffer:append(indent .. '<' .. (withNS and ns or '') .. mt.name .. '/>')
		return buffer
	end

	buffer:append(indent .. '<' .. (withNS and ns or '') .. mt.name .. '>')
	for _, member in ipairs(mt.members) do
		local name = member.name
		local value = self[name]
		local typeName = member.type.id
		local typeAttrib = ''
		if typeName == 'anySimpleType' then
			assert(mt.memberTypes[name], 'Member "' .. name .. '" of CWMP type "' .. mt.type .. '" is of type "anySimpleType" but has no type specified by a call to setMemberType().')
			typeName = mt.memberTypes[name]
			typeAttrib = ' xsi:type="xsd:' .. typeName .. '"'
		end
		if CWMP.isSimpleType(typeName) then
			assert(value ~= nil, 'Member "' .. name .. '" of CWMP type "' .. mt.type .. '" is nil.')
			buffer:append(indent .. '\t<' .. name .. typeAttrib .. '>' .. SOAP.serialiseSimpleType(typeName, value) .. '</' .. name .. '>')
		elseif typeName == 'array' then
			assert(value, 'Array member "' .. name .. '" of CWMP type "' .. mt.type .. '" is nil.')
			value:toXML(depth + 1, buffer)
		elseif typeName == 'object' then
			assert(value, 'Object member "' .. name .. '" of CWMP type "' .. mt.type .. '" is nil.')
			value:toXML(depth + 1, member.forceNS or false, buffer)
		else
			error('Unknown type "' .. typeName .. '" for member "' .. name .. '".')
		end
	end
	for _, extraChild in ipairs(mt.extraChildren) do
		extraChild:toXML(depth + 1, false, buffer)
	end
	buffer:append(indent .. '</' .. (withNS and ns or '') .. mt.name .. '>')

	return buffer
end

local function fromXML(self, node, ns)
	local mt = getmetatable(self)
	for _, member in ipairs(mt.members) do
		local name = member.name
		local value = node:findTag(name)
		assert(value, 'Member "' .. name .. '" of CWMP type "' .. mt.type .. '" not found.')
		local typeName = member.type.id
		local typeAttrib = ''
		if typeName == 'anySimpleType' then
			typeAttrib = value:getAttributeNS('type', SOAP.ns['xsi'])
			--[[ CWMP spec says anySimpleType must have a type attribute.
			     However, some ACS does not include the type attribute.
			     We infer the type from the data model if it is absent.
			--]]
			local prefix, typeId
			if not typeAttrib then
				Logger.log('CWMP', 'warning', 'Member "' .. name .. '" of CWMP type "' .. mt.type .. '" is of type "anySimpleType" but has no XSchema-instance type attribute.')
				assert(self['Name'], 'The member "Name" of CWMP type "' .. mt.type .. '" is missing. Failed to infer type')
				local param = paramTree:find(self['Name'])
				assert(param, 'Failed to find "' .. self['Name'] .. '" in the data model')
				typeId = param:getCWMPType()
				Logger.log('CWMP', 'info', 'Inferred type of "' .. self['Name'] .. '" is "' .. typeId .. '"')
			else
				prefix, typeId = typeAttrib:match('^([^:]+):(.+)$')
				if not prefix then
					-- if for some crazy reason the XSchema namespace is default
					-- or there is something otherwise wacky in the type attribute
					typeId = typeAttrib
				else
					-- validate the XSchema type attribute prefix
					local xsdPrefix = value:getNSPrefix(SOAP.ns['xsd'])
					assert(prefix == xsdPrefix, 'Expected XSchema namespace type attribute value.')
				end
			end
			assert(CWMP.isSimpleType(typeId), 'Unknown simple type "' .. typeId .. '" in XSchema-instance type attibute of member "' .. name .. '"')
			typeName = typeId
			self:setMemberType(name, typeName)
		end
		if CWMP.isSimpleType(typeName) then
			local valueText = value:innerText()
			local val = SOAP.deserialiseSimpleType(typeName, valueText)
			Logger.log('CWMP', 'debug', name .. '(' .. typeName .. ') := "' .. valueText .. '" ' .. type(val))
			self[name] = val
		elseif typeName == 'array' then
			local arr = self[name]
			arr:fromXML(value, ns)
		elseif typeName == 'object' then
			local obj = self[name]
			obj:fromXML(value, ns)
		else
			error('Unknown type "' .. typeName .. '" for member "' .. name .. '".')
		end
	end
end

CWMP.Object = {}
CWMP.Object.tostring = tostr

function CWMP.Object.new(name, typeName, populate)
	populate = populate or false
	typeName = typeName or name
	assert(CWMP.typeExists(typeName), 'Unknown CWMP type "' .. typeName .. '".')

	local mt = {}
	mt.name = name
	mt.type = typeName
	mt.data = {}
	mt.members = CWMP.types[typeName]
	mt.memberTypes = {}
	mt.extraChildren = {}
	mt.__index = get
	mt.__newindex = set
	mt.__tostring = tostr

	local obj = {
		setMemberType = setMemberType,
		getMemberType = getMemberType,
		toXML = toXML,
		fromXML = fromXML,
		addExtraChild = addExtraChild,
	}
	setmetatable(obj, mt)

	if populate then
		for idx, memberInfo in ipairs(mt.members) do
			if memberInfo.type.id == 'object' then
				-- object child
				mt.data[memberInfo.name] = CWMP.Object.new(memberInfo.name, memberInfo.type.class, true)
			elseif memberInfo.type.id == 'array' then
				-- array child
				mt.data[memberInfo.name] = CWMP.Array.new(memberInfo.name, memberInfo.type)
			else
				-- simpleType child
				-- nothing!
			end
		end
	end

	return obj
end

return CWMP.Object
