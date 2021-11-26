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

require('SOAP')
require('XML.StringBuffer')

local function assertIndex(self, idx)
	assert(type(idx) == 'number', 'CWMP Array may only be indexed by integers.')
	assert(idx > 0, 'CWMP Array may only be indexed by purely positive integers.')
	local mt = getmetatable(self)
	if mt.type.max then
		assert(idx < mt.type.max, 'CWMP Array "' .. mt.name .. '" may contain at most ' .. mt.type.max .. ' elements.')
	end
end

local function get(self, idx)
	assertIndex(self, idx)
	local mt = getmetatable(self)
	return mt.elements[idx]
end

local function set(self, idx, value)
	assertIndex(self, idx)
	local mt = getmetatable(self)
	assert(CWMP.validArrayValue(mt.type, idx, value), 'Invalid value for CWMP array "' .. mt.name .. '" element ' .. idx .. '.')
	mt.elements[idx] = value
end

local function add(self, value)
	local mt = getmetatable(self)
	idx = #(mt.elements) + 1
	set(self, idx, value)
end

local function count(self)
	local mt = getmetatable(self)
	return #(mt.elements)
end

local function tostr(self, depth)
	depth = depth or 0
	local mt = getmetatable(self)
	local indent = string.rep('\t', depth)
	local ret = indent .. mt.name .. ' (array[' .. (#mt.elements) .. ']: ' .. mt.type.type.id .. '):\n'
	local typeId = mt.type.type.id
	for idx, element in ipairs(mt.elements) do
		if CWMP.isSimpleType(typeId) then
			-- simpleType member
			ret = ret .. indent .. '\t' .. mt.type.type.id .. ' (' .. mt.type.type.id .. '): ' .. tostring(element) .. '\n'
		elseif typeId == 'object' then
			-- object member
			if element then
				ret = ret .. CWMP.Object.tostring(element, depth + 1)
			else
				ret = ret .. indent .. '\t' .. mt.type.type.class .. ' (' .. mt.type.type.id .. '): nil\n'
			end
--		elseif mt.type.type.id == 'array' then
			-- array member
			-- should we implement array of arrays? CWMP doesn't need it, but SOAP specifies it.
		else
			error('Unimplemented array member type "' .. typeId .. '".')
		end
	end
	return ret

end

local function toXML(self, depth, buffer)
	depth = depth or 0
	buffer = buffer or XML.StringBuffer.new()
	local mt = getmetatable(self)
	local indent = string.rep('\t', depth)

	if mt.type.min then
		assert(#mt.elements >= mt.type.min, 'Array "' .. mt.name .. '" must contain at least ' .. mt.type.min .. ' elements.')
	end
	
	local arrayType
	local typeId = mt.type.type.id
	if CWMP.isSimpleType(typeId) then
		arrayType = 'xsd:' .. typeId .. '[' .. #mt.elements .. ']'
	elseif typeId == 'object' then
		arrayType = 'cwmp:' .. mt.type.type.class .. '[' .. #mt.elements .. ']'
	else
		error('Unimplemented array element serialise from type "' .. typeId .. '".')
	end

	buffer:append(indent .. '<' .. mt.name .. ' xsi:type="soap-enc:Array" soap-enc:arrayType="' .. arrayType .. '">')
	for idx = 1, #mt.elements do
		local value = mt.elements[idx]
		assert(value, 'Element ' .. idx .. '" of CWMP array "' .. mt.name .. '" is nil.')
		if CWMP.isSimpleType(typeId) then
			buffer:append(indent .. '\t<' .. typeId .. '>' .. SOAP.serialiseSimpleType(typeId, value) .. '</' .. typeId .. '>')
		elseif typeId == 'object' then
			value:toXML(depth + 1, false, buffer)
--		elseif typeId == 'array' then
--			value:toXML(depth + 1, buffer)
		else
			error('Unimplemented array element serialise from type "' .. typeId .. '".')
		end
	end
	buffer:append(indent .. '</' .. mt.name .. '>')

	return buffer
end

local function fromXML(self, node, ns)
	local mt = getmetatable(self)
	local typeId = mt.type.type.id
	
--	print('array:', node:tagName())

	-- we should validate the XSchema-instance type and SOAP-encoding arrayType attributes here
	-- FIXME!

	local children = node:getChildrenByTagName(typeId ~= 'object' and typeId or mt.type.type.class)
	for _, child in ipairs(children) do
--		print('child:', child:tagName())
		if CWMP.isSimpleType(typeId) then
			assert(child:tagName() == typeId, 'Expected simpleType "' .. typeId .. '" array member element, got "' .. child:tagName() .. '".')
			local value = SOAP.deserialiseSimpleType(typeId, child:innerText())
--			print(node:tagName() .. '[] := "' .. value .. '"')
			self:add(value)
		elseif typeId == 'object' then
			local obj = CWMP.Object.new(mt.type.type.class, mt.type.type.class, true)
			obj:fromXML(child, ns)
			self:add(obj)
		else
			error('Unimplemented array element deserialise as type "' .. typeId .. '".')
		end
	end

	-- we should validate the count matched the SOAP-encoding size indicated
	-- the max size has already been constrained by the add() method, but the min has not
end

CWMP.Array = {}
CWMP.Array.tostring = tostr

function CWMP.Array.new(name, typeInfo)
	if typeInfo.type.id == 'object' then
		assert(CWMP.typeExists(typeInfo.type.class), 'Unknown CWMP type "' .. typeInfo.type.class .. '".')
	end

	local mt = {}
	mt.name = name
	mt.type = typeInfo
	mt.elements = {}
	mt.__index = get
	mt.__newindex = set
	mt.__tostring = tostr

	local arr = {
		add = add,
		count = count,
		toXML = toXML,
		fromXML = fromXML
	}
	setmetatable(arr, mt)
	return arr
end

return CWMP.Array
