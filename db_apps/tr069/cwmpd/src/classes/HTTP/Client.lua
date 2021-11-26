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
require('HTTP')
require('Logger')
require('luacurl')
local urlModule = require('socket.url')

HTTP.Client = {}

local function init(self)
	assert(self.ch == nil, 'Client already initilised.')

	self.ch = curl.new()
	if self.debug then
		self.ch:setopt(curl.OPT_VERBOSE, self.debug)
	end

	if self.username or self.password then
		self.ch:setopt(curl.OPT_USERPWD, self.username .. ':' .. self.password)
--		self.ch:setopt(curl.OPT_USERNAME, self.username)
--		self.ch:setopt(curl.OPT_PASSWORD, self.password)
	end
	if self.timeout then
		self.ch:setopt(curl.OPT_TIMEOUT, self.timeout)
	end
	if self.connectionTimeout then
		self.ch:setopt(curl.OPT_CONNECTTIMEOUT, self.connectionTimeout)
	end
	if self.maxRedirects then
		self.ch:setopt(curl.OPT_MAXREDIRS, self.maxRedirects)
	end

	if self.cookies then
		self.ch:setopt(curl.OPT_COOKIEFILE, self.cookies)
	end
	self.ch:setopt(curl.OPT_HTTPAUTH, curl.AUTH_BASIC + curl.AUTH_DIGEST)

	--[[ For HTTPS connection, both VERIFYPEER and VERIFYHOST are true
	     by default. curl will try the highest available SSLVERSION and
	     all supported CIPHERs. So we do not need to set them in general.
	     For HTTP connection, these settings have no effects.
	--]]

	-- verify host for https connection
	-- 0 -- no host verify
	-- 2 -- strict host verify. the CN must match the domain (default)
	-- if verify host is required, set conf.net.ssl_verify_host to true
	if not conf.net.ssl_verify_host then
		self.ch:setopt(curl.OPT_SSL_VERIFYHOST, 0)
	end

	-- verify peer (server certificate) for https connection
	-- true -- verify peer (default)
	-- false -- accept any certificate
	-- if we want to skip verification, set conf.net.ssl_verify_peer to false
	if conf.net.ssl_verify_peer == false then
		self.ch:setopt(curl.OPT_SSL_VERIFYPEER, false)
	end

	-- SSL version to use for https connection
	-- 0 -- auto version negotiation (default)
	-- 1 -- TLSv1 (all TLSv1.*)
	-- 2 -- SSLv2
	-- 3 -- SSLv3
	-- 4 -- TLSv1_0 (added in curl 7.34.0)
	-- 5 -- TLSv1_1 (added in curl 7.34.0)
	-- 6 -- TLSv1_2 (added in curl 7.34.0)
	-- 7 -- TLSv1_3 (added in curl 7.52.0)
	if type(conf.net.ssl_version) == 'number' then
		self.ch:setopt(curl.OPT_SSLVERSION, conf.net.ssl_version)
	end

	-- SSL cipher list for https connection
	-- This should be a colon separated string of ciphers
	-- By default, all supported ciphers will be available during handshake.
	if type(conf.net.ssl_cipher_list) == 'string' then
		self.ch:setopt(curl.OPT_SSL_CIPHER_LIST, conf.net.ssl_cipher_list)
	end

	if type(conf.net.ca_cert) == 'string' then
		self.ch:setopt(curl.OPT_CAINFO, conf.net.ca_cert)
	end

	if type(conf.net.ca_path) == 'string' then
		self.ch:setopt(curl.OPT_CAPATH, conf.net.ca_path)
	end

	if type(conf.net.client_cert) == 'string' then
		self.ch:setopt(curl.OPT_SSLCERT, conf.net.client_cert)
        end

	if type(conf.net.client_key) == 'string' then
		self.ch:setopt(curl.OPT_SSLKEY, conf.net.client_key)
        end
end

local function close(self)
	assert(self.ch, 'Client not initialised.')
	self.ch:close()
end



local function getResponseCode(self)
	local code, msg, errCode = self.ch:getinfo(curl.INFO_RESPONSE_CODE)
	if not code then
		Logger.log('HTTP', 'error', 'getinfo(INFO_RESPONSE_CODE) failed: ' .. msg .. ' (' .. errCode .. ')')
		error('getinfo(INFO_RESPONSE_CODE) failed: ' .. msg .. ' (' .. errCode .. ')')
	end
	return code
end

local function getCookie(self, url)
	if not url then return "" end

	local urlTbl = urlModule.parse(url)

	local cookieList = self.ch:getinfo(curl.INFO_COOKIELIST)
	if not cookieList then return "" end

	for i, cookie in ipairs(cookieList) do
		local domain, _, _, _, _, name, value = cookie:match("(%S+)%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")

		if domain and domain == urlTbl.host and name and value then
			return name .. '=' .. value
		end
	end

	return ""
end

local function doRawPOST(self, url, contentType, data, interface)
	assert(self.ch, 'Client not initialised.')

	local cookie = getCookie(self, url)

	self.ch:setopt(curl.OPT_POST, true)
	self.ch:setopt(curl.OPT_URL, url)
	self.ch:setopt(curl.OPT_POSTFIELDS, data)

	self.ch:setopt(curl.OPT_HTTPHEADER,
		'Content-Type: ' .. contentType,
		'SOAPAction: ""',
		'Expect:',
		'Accept:'
	)

	if cookie and cookie ~= "" then
		self.ch:setopt(curl.OPT_COOKIE, cookie)
	end

	self.buf = ''
	self.ch:setopt(curl.OPT_WRITEDATA, self)
	self.ch:setopt(curl.OPT_WRITEFUNCTION, function(client, buf)
		client.buf = client.buf .. buf
		return #buf
	end)

	if interface and interface ~= '' then
		self.ch:setopt(curl.OPT_INTERFACE, interface)
	end

	local ret, msg, errCode = self.ch:perform()
	if not ret then
		Logger.log('HTTP', 'error', 'doRawPOST(): perform() failed: ' .. msg .. ' (' .. errCode .. ')' .. ' url:"' .. url .. '" interface:"' .. tostring(interface) .. '"')
		Logger.log('HTTP', 'debug', 'doRawPOST(): RX buffer "' .. self.buf .. '"')
		error('doRawPOST(): perform() failed: ' .. msg .. ' (' .. errCode .. ')')
	end
	local code = self:getResponseCode()

	return code, self.buf
end

local function doEmptyPOST(self, url, interface)
	assert(self.ch, 'Client not initialised.')

	local cookie = getCookie(self, url)

	self.ch:setopt(curl.OPT_POST, true)
	self.ch:setopt(curl.OPT_URL, url)
	self.ch:setopt(curl.OPT_POSTFIELDS, '')
	self.ch:setopt(curl.OPT_HTTPHEADER,
		'Content-Type:',
		'Expect:'
	)

	if cookie and cookie ~= "" then
		self.ch:setopt(curl.OPT_COOKIE, cookie)
	end

	self.buf = ''
	self.ch:setopt(curl.OPT_WRITEDATA, self)
	self.ch:setopt(curl.OPT_WRITEFUNCTION, function(client, buf)
		client.buf = client.buf .. buf
		return #buf
	end)

	if interface and interface ~= '' then
		self.ch:setopt(curl.OPT_INTERFACE, interface)
	end

	local ret, msg, errCode = self.ch:perform()
	if not ret then
		Logger.log('HTTP', 'error', 'doEmptyPOST(): perform() failed: ' .. msg .. ' (' .. errCode .. ')' .. ' url:"' .. url .. '" interface:"' .. tostring(interface) .. '"')
		error('doEmptyPOST(): perform() failed: ' .. msg .. ' (' .. errCode .. ')')
	end
	local code = self:getResponseCode()

	return code, self.buf

end


local function close_connection(self, url, interface)
	assert(self.ch, 'Client not initialised.')

	self.ch:setopt(curl.OPT_FORBID_REUSE,true)
	self.ch:setopt(curl.OPT_CONNECT_ONLY,true)

	self.ch:setopt(curl.OPT_POST, true)
	self.ch:setopt(curl.OPT_URL, url)
	self.ch:setopt(curl.OPT_POSTFIELDS, '')

	self.ch:setopt(curl.OPT_WRITEFUNCTION, function(client, buf)
		return 0
	end)

	if interface and interface ~= '' then
		self.ch:setopt(curl.OPT_INTERFACE, interface)
	end

	local ret, msg, errCode = self.ch:perform()
	if not ret then
		-- always failed, because this perform is for CONNECT_ONLY
	end

	self.ch:setopt(curl.OPT_FORBID_REUSE,false)
	self.ch:setopt(curl.OPT_CONNECT_ONLY,false)

end


local function setTimeout(self, timeout)
	self.timeout = timeout
end

local function setConnectionTimeout(self, timeout)
	self.connectionTimeout = timeout
end

local function setAuth(self, username, password)
	self.username = username
	self.password = password
end

local function sendCWMP(self, url, request, interface)
	local requestXML = SOAP.serialise(request)
	Logger.log('HTTP', 'debug', 'Client TX: ' .. requestXML)
	local ret, code, responseXML = pcall(self.doRawPOST, self, url, 'text/xml', requestXML, interface)
	if not ret then
		Logger.log('HTTP', 'error', 'Client TX: Error: ' .. tostring(code) .. ': ' .. tostring(responseXML))
		return false, 500, nil
	else
		if code == 204 then
			Logger.log('HTTP', 'debug', 'Client RX: 204 No Content')
			return true, code, nil
		elseif code ~= 200 then
			Logger.log('HTTP', 'error', 'Client RX: HTTP error: ' .. tostring(code) .. ': ' .. tostring(responseXML))
			return false, code, responseXML
		end
	end
	Logger.log('HTTP', 'debug', 'Client RX: ' .. code .. ': ' .. responseXML)
	local ret, response = xpcall(function() return SOAP.deserialise(responseXML) end, debug.traceback)
	if not ret then
		Logger.log('HTTP', 'error', 'Client RX: Reply parsing error: ' .. response)
		Logger.log('HTTP', 'info', 'Client RX: XML received: ' .. responseXML)
		return false, code, response
	elseif response:isFault() then
		Logger.log('HTTP', 'error', 'Client RX: SOAP Fault: ' .. response.message.detail.Fault.FaultCode .. ': ' .. response.message.detail.Fault.FaultString)
		return false, code, response
	end
	return true, code, response
end

local function sendEmptyCWMP(self, url, interface)
	Logger.log('HTTP', 'debug', 'Client TX: Empty 204')
	local ret, code, responseXML = pcall(self.doEmptyPOST, self, url, interface)
	if not ret then
		Logger.log('HTTP', 'error', 'Client TX: Error: ' .. tostring(code) .. ': ' .. tostring(responseXML))
		return false, 500, nil
	else
		if code == 204 then
			Logger.log('HTTP', 'debug', 'Client RX: 204 No Content')
			return true, code, nil
		elseif code ~= 200 then
			Logger.log('HTTP', 'error', 'Client RX: HTTP error: ' .. tostring(code) .. ': ' .. tostring(responseXML))
			return false, code, responseXML
		end
	end
	Logger.log('HTTP', 'debug', 'Client RX: ' .. code .. ': ' .. responseXML)
	local ret, response = pcall(SOAP.deserialise, responseXML)
	if not ret then
		Logger.log('HTTP', 'error', 'Client RX: Reply parsing error: ' .. response)
		return false, code, response

	elseif response:isFault() then
		Logger.log('HTTP', 'error', 'Client RX: SOAP Fault: ' .. response.message.detail.Fault.FaultCode .. ': ' .. response.message.detail.Fault.FaultString)
		return false, code, response
	end
	return true, code, response
end


function HTTP.Client.new()
	local instance = {
		timeout = 60,
		connectionTimeout = 60,
		maxRedirects = 5,
		cookies = '',
		debug = false,

		ch = nil,

		init = init,
		close = close,
		setTimeout = setTimeout,
		setConnectionTimeout = setConnectionTimeout,
		setAuth = setAuth,

		getResponseCode = getResponseCode,
		doRawPOST = doRawPOST,
		doEmptyPOST = doEmptyPOST,
		close_connection= close_connection,
		sendCWMP = sendCWMP,
		sendEmptyCWMP = sendEmptyCWMP,
	}
	return instance
end

return HTTP.Client
