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
require('Daemon')

require('Parameter')
require('HTTP.Client')
require('CWMP.Message')
require('CWMP.Object')
require('SOAP')
require('tableutil')
require('variants')

Daemon.ACS = {}

local function nextId(self)
	local id = self.nextId
	self.nextId = self.nextId + 1
	return tostring(id)
end

local function init(self, tree)
	assert(not self.http, 'ACS connection already initilised.')

	self.url = tree:getValue('$ROOT.ManagementServer.URL')
	local username = tree:getValue('$ROOT.ManagementServer.Username')
	local password = tree:getValue('$ROOT.ManagementServer.Password')

	local http = HTTP.Client.new()
	http.debug = conf.net.httpDebug
	http:setAuth(username, password)
	if conf.net.ioTimeout then
		http:setTimeout(conf.net.ioTimeout)
	end
	if conf.net.connectionTimeout then
		http:setConnectionTimeout(conf.net.connectionTimeout)
	end
	http:init()
	self.http = http

	Logger.log('Daemon', 'debug', 'ACS: init.')
end

local function inform(self, tree, host)
	if not self.http then self:init(tree) end

	Logger.log('Daemon', 'debug', 'ACS: TX: Inform')

	local request = CWMP.Message.new('Inform')
	request.version = self.version
	request.id = nextId(self)
	Logger.log('Daemon', 'debug', 'ACS: version=' .. self.version)
	-- only CWMP v1.4+ supports SupportedCWMPVersions
	if CWMP.versionCompare(self.version, '1.4') >= 0 then
		local supportedVersions = {}
		for ver, ns in pairs(CWMP.versions) do
			table.insert(supportedVersions, ver)
		end
		request.supportedCWMPVersions = table.concat(supportedVersions, ',')
		Logger.log('Daemon', 'debug', 'SupportedCWMPVersions=' .. request.supportedCWMPVersions)
	end
	local inform = request.message

	inform.DeviceId.Manufacturer = tree:getValue('$ROOT.DeviceInfo.Manufacturer')
	inform.DeviceId.OUI = tree:getValue('$ROOT.DeviceInfo.ManufacturerOUI')
	inform.DeviceId.ProductClass = tree:getValue('$ROOT.DeviceInfo.ProductClass')
	inform.DeviceId.SerialNumber = tree:getValue('$ROOT.DeviceInfo.SerialNumber')

	for _, event in ipairs(host:getEvents()) do
		local evt = CWMP.Object.new('EventStruct')
		if event.code then
			evt.EventCode = event.code
			evt.CommandKey = event.key or ''
			inform.Event:add(evt)

			-- Report reboot reason with the first "1 BOOT" inform after system bootup
			if conf.enableRebootReasonReport == true and event.code == "1 BOOT" then
				local lastreboot = luardb.get("service.tr069.lastreboot")
				if lastreboot and lastreboot ~= "" then
					local evt1 = CWMP.Object.new('EventStruct')
					if conf.rebootReasonCodeList then -- With reboot reason code.
						evt1.EventCode = string.format("X OUI RG Boot %s", conf.rebootReasonCodeList[lastreboot] or conf.rebootReasonCodeList._unknown or "")
					else
						evt1.EventCode = string.format("Reboot reason: %s", lastreboot)
					end
					evt1.CommandKey = ''
					inform.Event:add(evt1)
				end
			end
		else
			-- this should never happen, but it appears rdb_manager drops the ball now and then
			Logger.log('Daemon', 'error', 'ACS: event with no event code.')
			host:deleteEvent(event)
		end
	end
	if inform.Event:count() < 1 then
		Logger.log('Daemon', 'error', 'ACS: no events for inform!')
		-- FIXME: what to do when this happens? (never should, but it does!)
		Logger.log('Daemon', 'warning', 'ACS: synthesising an value change to avoid problems...')
		local evt = CWMP.Object.new('EventStruct')
		evt.EventCode = '4 VALUE CHANGE'
		evt.CommandKey = ''
		inform.Event:add(evt)
	end

	for idx = 1, inform.Event:count() do
		Logger.log('Daemon', 'info', 'Event=' .. inform.Event[idx].EventCode .. ', Key=' .. inform.Event[idx].CommandKey)
	end

	inform.MaxEnvelopes = 1
	inform.CurrentTime = os.date('%FT%T')
	inform.RetryCount = self.retries
	self.retries = self.retries + 1

	-- forced and notification parameters
	for _, node in ipairs(tree:getInformParameterNodes()) do
		local path = node:getPath()
		local ret, val = node:getValue(path)
		if ret == 0 then
			local param = CWMP.Object.new('ParameterValueStruct')
			param.Name = path
			param:setMemberType('Value', node:getCWMPType())
			param.Value = node:nodeToCWMP(val)
			inform.ParameterList:add(param)

			-- node.value shold be updated with reported value.
			-- Without this, client could trigger value change event two times.
			node.value = val
		else
			Logger.log('Daemon', 'error', 'ACS: Error fetching inform parameter value "' .. node:getPath() .. '": ' .. ret .. (val and (': ' .. val) or '.'))
		end
	end

	return self.http:sendCWMP(self.url, request, self.current_interface)
end

local function getRPCMethods(self)
	if not self.http then self:init(paramTree) end

	Logger.log('Daemon', 'info', 'ACS: TX: GetRPCMethods')

	local request = CWMP.Message.new('GetRPCMethods')
	request.version = self.version
	request.id = nextId(self)

	local ok, code, response = self.http:sendCWMP(self.url, request, self.current_interface)
	if ok then
		self.rpcMethods = {}
		local methods = response.message.MethodList
		for i = 1,methods:count() do
			table.insert(self.rpcMethods, methods[i])
		end
		Logger.log('Daemon', 'info', 'ACS: RX: Implemented RPCs: ' .. table.concat(self.rpcMethods, ', '))
	end

	return ok, code, response
end

local function transferComplete(self, transfer)
	if not self.http then self:init(paramTree) end
	Logger.log('Daemon', 'info', 'ACS: TX: TransferComplete')

	local request = CWMP.Message.new('TransferComplete')
	request.version = self.version

	request.message.CommandKey = transfer.key
	request.message.FaultStruct.FaultCode = (transfer.result and transfer.result or 9002)
	request.message.FaultStruct.FaultString = (transfer.message and transfer.message or 'Internal error: No result code and message available in completed transfer.')
	request.message.StartTime = transfer.started and CWMP.formatEpocTime(transfer.started) or CWMP.unknownTime
	request.message.CompleteTime = transfer.completed and CWMP.formatEpocTime(transfer.completed) or CWMP.unknownTime

	return self.http:sendCWMP(self.url, request, self.current_interface)
end

local function sendResponse(self, response)
	if not self.http then self:init(paramTree) end
	Logger.log('Daemon', 'info', 'ACS: TX: ' .. response.type)
	return self.http:sendCWMP(self.url, response, self.current_interface)
end

local function noContent(self)
	if not self.http then self:init(paramTree) end
	Logger.log('Daemon', 'debug', 'ACS: TX: Empty')
	return self.http:sendEmptyCWMP(self.url, self.current_interface)
end
local function sessionEnd(self)
	if self.http  then
		if conf.net.keep_ssl_session then
			-- Close TCP connection only, SSL session cache is kept
			self.http:close_connection(self.url)
		else
			-- destry SSL session
			self:close();
		end
	end
end



local function close(self)
	assert(self.http, 'ACS connection not initilised.')
	self.http:close()
	self.http = nil
	Logger.log('Daemon', 'debug', 'ACS: closed.')
end

local function seedRandomizer()
	-- seed the randomizer with a combination of system time + imei + signal rsrp
	-- system time
	local time_val = os.time() or 1
        local len = string.len(time_val)
	if len and len > 8 then
		time_val = tonumber(string.sub(time_val,-8) or 1)
        end
	-- imei - last 8 digits of 15 digit number
	local seed = time_val
	local imei = luardb.get('wwan.0.imei')
	if imei ~= nil then
		seed = seed + (tonumber(string.sub(imei,-8)) or 0)
	end
	-- rsrp - values after decimal point
	-- (rsrp is a valid rdb variable only on Antelope and Arachnid but this should not cause any error on other platforms)
	local rsrp = luardb.get('wwan.0.signal.0.rsrp')
	if rsrp ~= nil then
		local dot = '%.'
		local dotpos = string.find(rsrp,dot)
		if dotpos ~= nil then
			seed = seed + (tonumber(string.sub(rsrp,(dotpos+1))) or 0)
		end
	end

        math.randomseed(seed)
end

local function getRetryDelay(self, tree)
	local tries = self.retries <= 10 and self.retries or 10
	-- tree.getValue assert fails if parameter does not exist, so use pcall to catch
	local ret, m, k
	ret, m = pcall(tree.getValue, tree, '$ROOT.ManagementServer.CWMPRetryMinimumWaitInterval')
	if not ret then
		m = conf.cwmp.defaultBackoffM
	end
	ret, k = pcall(tree.getValue, tree, '$ROOT.ManagementServer.CWMPRetryIntervalMultiplier')
	if not ret then
		k = conf.cwmp.defaultBackoffK
	end
	local min = m * (k/1000)^(tries - 1)
	local max = m * (k/1000)^tries
	Logger.log('Daemon', 'debug', 'Retry interval: (m = ' .. m .. ', k = ' .. k .. '): [' .. min .. '-' .. max .. '].')
	return math.random(min, max)
end

function Daemon.ACS.new()
	-- initialize for Santos only, skip otherwise
	if variants.V_CUSTOM_FEATURE_PACK == "Santos" then
		local iface = luardb.get("tr069.server.interface") or 'wwan0'
		local fbiface = luardb.get("tr069.server.fallback_interface") or 'wwan1'
		luardb.set('tr069.server.current_interface', iface)
	end

	seedRandomizer()

	local acs = {
		fallback_sessions = 0,
		isFallback = false,
		version = '1.1',
		nextId = 1,
		retries = 0,
		rpcMethods = {},

		interface = iface,
		fallbackInterface = fbiface,
		current_interface = iface,

		init = init,
		inform = inform,
		getRPCMethods = getRPCMethods,
		transferComplete = transferComplete,
		sendResponse = sendResponse,
		noContent = noContent,
		close = close,
		sessionEnd=sessionEnd,

		getRetryDelay = getRetryDelay,
	}
	return acs
end

return Daemon.ACS
