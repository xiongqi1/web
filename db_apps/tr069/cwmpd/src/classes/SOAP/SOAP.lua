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
Logger.addSubsystem('SOAP')

require('XML')
require('XML.StringBuffer')

require('stringutil')
require('tableutil')
require('Int64')

SOAP = {}

SOAP.ns = {
	['soap']	= 'http://schemas.xmlsoap.org/soap/envelope/',
	['soap-enc']	= 'http://schemas.xmlsoap.org/soap/encoding/',
	['xsd']		= 'http://www.w3.org/2001/XMLSchema',
	['xsi']		= 'http://www.w3.org/2001/XMLSchema-instance',
}

local converters = {
	['string'] = {
		serialise = XML.encode,
		deserialise = function(str) return str end
	},
	['base64'] = {
		serialise = XML.encode,
		deserialise = function(str) return str end
	},
	['dateTime'] = {
		serialise = XML.encode,
		deserialise = function(str) return str end
	},
	['int'] = {
		serialise = function(num) return tostring(num) end,
		deserialise = function(str) return tonumber(str) end
	},
	['unsignedInt'] = {
		serialise = function(num) return tostring(num) end,
		deserialise = function(str) return tonumber(str) end
	},
	['long'] = {
		serialise = function(num) return tostring(num) end,
		deserialise = function(str) return tobignumber(str) end
	},
	['unsignedLong'] = {
		serialise = function(num) return tostring(num) end,
		deserialise = function(str) return tobignumber(str) end
	},
	['boolean'] = {
		serialise = function(bool) return bool and 'true' or 'false' end,
		deserialise = function(str) return (str:lower() == 'true' or str == '1') end
	},
}

function SOAP.serialiseSimpleType(typeName, value)
	assert(CWMP.isSimpleType(typeName), '"' .. typeName .. '" is not a valid simpleType.')
	local converter = converters[typeName]
	assert(converter.serialise, 'No serialiser for simple type "' .. typeName .. '".')
	return converter.serialise(value)
end

function SOAP.deserialiseSimpleType(typeName, str)
	assert(CWMP.isSimpleType(typeName), '"' .. typeName .. '" is not a valid simpleType.')
	local converter = converters[typeName]
	assert(converter.deserialise, 'No deserialiser for simple type "' .. typeName .. '".')
	return converter.deserialise(str)
end

function SOAP.serialise(cwmpMessage)
	local nsdefs = {}
	local buffer = XML.StringBuffer.new()
	for id, uri in pairs(SOAP.ns) do
		table.insert(nsdefs, 'xmlns:' .. id .. '="' .. XML.encode(uri) .. '"')
	end
	table.insert(nsdefs, 'xmlns:cwmp="' .. XML.encode(CWMP.versions[cwmpMessage.version]) .. '"')

--	local xmlStr = '<?xml version="1.0" encoding="UTF-8" ?>\n'
	buffer:append('<soap:Envelope ' .. table.concat(nsdefs, ' ') .. '>')
	if cwmpMessage.id or cwmpMessage.holdRequests ~= nil or cwmpMessage.supportedCWMPVersions then
		buffer:append('\t<soap:Header>')
		if cwmpMessage.id then
			buffer:append('\t\t<cwmp:ID soap:mustUnderstand="1">' .. XML.encode(cwmpMessage.id) .. '</cwmp:ID>')
		end
		if cwmpMessage.holdRequests ~= nil then
			buffer:append('\t\t<cwmp:HoldRequests soap:mustUnderstand="1">' .. (cwmpMessage.holdRequests and '1' or '0') .. '</cwmp:HoldRequests>')
		end
		if cwmpMessage.supportedCWMPVersions then
			buffer:append('\t\t<cwmp:SupportedCWMPVersions soap:mustUnderstand="0">' .. XML.encode(cwmpMessage.supportedCWMPVersions) .. '</cwmp:SupportedCWMPVersions>')
		end
		buffer:append('\t</soap:Header>')
	end
	buffer:append('\t<soap:Body>')
	cwmpMessage.message:toXML(2, true, buffer)
	buffer:append('\t</soap:Body>')
	buffer:append('</soap:Envelope>')

	return buffer:concat('\n')
end

function SOAP.deserialise(xmlString)
	local xmlObj = XML.parse(xmlString)
	assert(xmlObj, 'SOAP parsing error.')

	-- Actually a SOAP message?
	assert(XML.declaresNS(xmlObj, SOAP.ns['soap']), 'No SOAP namespace definition present.')

	-- SOAP Envelope.
	local envelope = xmlObj:findNS('Envelope', SOAP.ns['soap'], true)
	assert(envelope, 'No SOAP envelope.')

	-- Determine CWMP version.
	local cwmpVersion = nil
	local cwmpNS = nil
	for ver, ns in pairs(CWMP.versions) do
		if XML.declaresNS(xmlObj, ns) then
			assert(not cwmpVersion, 'Message has multiple CWMP versions.')
			cwmpVersion = ver
			cwmpNS = ns
			if cwmpVersion == '1.2' then
				-- v1.2-v1.4 share the same NS cwmp-1-2
				break
			end
		end
	end
	-- I guess it is possible that the CWMP namespace might be absent for a Fault which has been canonicalised?
	-- Unlikely as that might be, as there will generally be at least an ID in the header.
	-- But it is remotely possible the SOAP fault occurred at a level above CWMP...
	-- If this happens in practice change this to be deferred beyond fault/message determination?
	assert(cwmpVersion, 'Message has no CWMP namespace definition.')

	-- SOAP Header.
	local id = nil
	local holdRequests = nil
	local useCWMPVersion = nil
	local header = envelope:findNS('Header', SOAP.ns['soap'])
	if header then
		id = header:findNS('ID', cwmpNS)
		holdRequests = header:findNS('HoldRequests', cwmpNS)
		useCWMPVersion = header:findNS('UseCWMPVersion', cwmpNS)
		-- we should really look for other mustUnderstand elements here and throw...
		-- but full SOAP compliance is rather difficult for a client such as this
	end

	-- SOAP Body.
	local body = envelope:findNS('Body', SOAP.ns['soap'])
	assert(body, 'No SOAP Body.')

	-- Parse body.
	local cwmpRoot = body:childNS(1, cwmpNS)
	if cwmpRoot then
		-- The CWMP message.
		local msgType = cwmpRoot:tagName()
		local cwmp = CWMP.Message.new(msgType)
		cwmp.version = cwmpVersion
		if id then cwmp.id = id:innerText() end
		if holdRequests then cwmp.holdRequests = holdRequests:innerText() end
		if useCWMPVersion then cwmp.useCWMPVersion = useCWMPVersion:innerText() end
		cwmp.message:fromXML(cwmpRoot, cwmpNS)
		return cwmp
	else
		-- A Fault?
		local fault = body:findNS('Fault', SOAP.ns['soap'])
		assert(fault, 'No CWMP element or SOAP Fault.')
		local msg = CWMP.Message.new('Fault')
		msg.version = cwmpVersion
		if id then msg.id = id:innerText() end
		if holdRequests then msg.holdRequests = holdRequests:innerText() end
		if useCWMPVersion then cwmp.useCWMPVersion = useCWMPVersion:innerText() end
		msg.message:fromXML(fault, SOAP.ns['soap'])
		-- FIXME: other Fault parsing...
		return msg
	end
end

return SOAP
