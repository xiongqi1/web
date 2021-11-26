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
require('Logger')
Logger.addSubsystem('XML')

require('LuaXml')
require('stringutil')
require('tableutil')

XML = {}

function XML.encode(str)
	assert(type(str) == 'string', 'Not a string: ' .. type(str) .. '.')
	str = str:gsub('&', '&amp;')
	str = str:gsub('<', '&lt;')
	str = str:gsub('>', '&gt;')
	str = str:gsub('"', '&quot;')
	str = str:gsub("'", '&apos;')
	-- similar to encode this isn't really fully compliant for unicode
	-- we assume UTF-8 encoding is in use and the decoder will behave
	-- mechanically, treating each byte individually
   	str = str:gsub('([^%w%&%;%p%\t% ])',
		function(chr) return string.format('&#x%X;', string.byte(chr)) end
	)
	return str
end

function XML.decode(str)
	assert(type(str) == 'string', 'Not a string: ' .. type(str) .. '.')
	-- clearly this isn't fully compliant for all codepoints, UTF-8 or otherwise
	-- it makes some assumptions about encoder behaviour being as above (byte-wise)
	-- the semantics of string.char aren't well defined, what happens for n > 255?
	-- if n is large we should manually encode it as several bytes of UTF-8
	-- as lua's multibyte string support is minimal anyway this will do for now
	str = str:gsub('&#x([%x]+)%;',
		function(hex) return string.char(tonumber(hex, 16)) end
	)
	str = str:gsub('&#([0-9]+)%;',
		function(dec) return string.char(tonumber(dec, 10)) end
	)
	str = str:gsub('&apos;', "'")
	str = str:gsub('&quot;', '"')
	str = str:gsub('&gt;', '>')
	str = str:gsub('&lt;', '<');
	str = str:gsub('&amp;', '&');
	return str
end

-- the xml table is the __index metatable member for all nodes it manipulates
-- this is handy as we can extend it easily, adding additional methods
-- we do this to add something like real namespace support to it
-- that said, the entire NS support is pretty broken
-- we really need to write our own NS-aware parser layer to replace this crap
-- another sin in the name of expediency

function xml.nsDeclarations(node)
	local nses = {}
	for attr, val in pairs(node) do
		if type(attr) == 'string' and attr:startsWith('xmlns:') then
			nses[attr:sub(7)] = val
		elseif type(attr) == 'string' and attr == 'xmlns' then
			nses[''] = val
		end
	end
	return nses
end

-- recursively populate the metatable of each XML element node with an "ns" table of prefix -> URI
-- fortunately each element node already has its own private metatable so we can just add a key to it
local function addNSInformation(node, parentNSes)
	parentNSes = parentNSes or {}
	local mt = getmetatable(node)
	assert(type(mt) == 'table', 'No metatable on XML node.')
	local localNSes = node:nsDeclarations()
--	print('local NS', node[0], table.tostring(localNSes))
	for prefix, uri in pairs(parentNSes) do
		if not localNSes[prefix] then
			localNSes[prefix] = uri
		end
	end
	mt.ns = localNSes

	-- recurse children
	for _, elem in ipairs(node) do
		if type(elem) == 'table' then
			addNSInformation(elem, localNSes)
		end
	end
end

function xml.getNSPrefix(node, nsURI)
--	print(node[0])
	local mt = getmetatable(node)
	assert(type(mt) == 'table', 'No metatable on XML node.')
	for prefix, uri in pairs(mt.ns) do
		if uri == nsURI then
--			print(uri, '=', prefix)
			return prefix
		end
	end
end

function xml.getPrefixNS(node, prefix)
	local mt = getmetatable(node)
	return mt.ns[prefix]
end

function xml.getNS(node)
	local name = node[0]
	local prefix, tag = name:match('^([^:]+):(.+)$')
	if prefix then
		return node:getPrefixNS(prefix)
	else
		return node:getPrefixNS('')
	end
end

function xml.findNS(node, tag, uri, matchSelf)
--	print('findNS', node[0], tag, uri, matchSelf)
	matchSelf = matchSelf or false
	local name = tag
	local prefix = node:getNSPrefix(uri)
	if prefix and prefix ~= '' then
		name = prefix .. ':' .. tag
	end
	if matchSelf and node[0] == name then return node end
	for _, child in ipairs(node) do
		if type(child) == 'table' then
			if child[0] == name then
				return child
			elseif not matchSelf then
				-- ask child to match itself directly
				-- as NS may be defined in the tag itself
				-- it makes the code a little neater
				local cnode = child:findNS(tag, uri, true)
				if cnode then
					return cnode
				end
			end
		end
	end
end

function xml.findTag(node, tag)
	for _, child in ipairs(node) do
		if type(child) == 'table' and child:tagName() == tag then
			return child
		end
	end
end

function xml.getAttributeNS(node, attr, uri)
	local prefix = node:getNSPrefix(uri)
	if prefix and prefix ~= '' then
		attr = prefix .. ':' .. attr
	end
	return node[attr]
end

function xml.getChildrenByNS(node, uri)
	local children = {}
	for _, child in ipairs(node) do
		if type(child) == 'table' and child:getNS() == uri then
			table.insert(children, child)
		end
	end
	return children
end

function xml.childNS(node, idx, uri)
	local nsChildren = node:getChildrenByNS(uri)
	return nsChildren[idx]
end

function xml.tagName(node)
	local name = node[0]
	local prefix, tag = name:match('^([^:]+):(.+)$')
	if tag then return tag end
	return name
end

function xml.getChildrenByTagName(node, tag)
	local children = {}
	for _, child in ipairs(node) do
		if type(child) == 'table' and child:tagName() == tag then
			table.insert(children, child)
		end
	end
	return children
end

function xml.innerText(node)
	local text = ''
	for _, child in ipairs(node) do
		if type(child) == 'table' then
			text = text .. child:innerText()
		elseif type(child) == 'string' then
			text = text .. child
		else
			error('Unexpected child type: ' .. type(child) .. '.')
		end
	end
	return text
end

-- get all NS declarations in tree
-- returns an array of objects each having the prefix, uri and element node of declaration
function XML.getAllNSDeclarations(node, nses)
	nses = nses or {}
	for attr, val in pairs(node) do
		if type(attr) == 'string' and attr:startsWith('xmlns:') or attr == 'xmlns' then
			local ns = {
				id = attr:sub(7),
				ns = val,
				root = node
			}
			table.insert(nses, ns)
		end
	end
	for _, elem in ipairs(node) do
		if type(elem) == 'table' then
			XML.getAllNSDeclarations(elem, nses)
		end
	end
	return nses
end

-- shallowest 1st seen NS prefix by URI
-- we'll use this for now to locate sub-tree pieces we care about
-- like the SOAP envelope and the CWMP payload
-- eventually this will be replaced by real NS support
function XML.getNSPrefix(node, uri)
	local nses = XML.getAllNSDeclarations(node)
	for _, ns in ipairs(nses) do
		if ns.ns == uri then
			return ns.id
		end
	end
end

function XML.declaresNS(node, uri)
	return (XML.getNSPrefix(node, uri) ~= nil)
end


function XML.parse(str)
	local tree = xml.eval(str)
	if not tree then return end
	addNSInformation(tree)
	return tree
end

return XML
