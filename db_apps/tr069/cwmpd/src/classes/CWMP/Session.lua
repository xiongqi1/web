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

require('CWMP.RPC')
require('HTTP.Client')

CWMP.Session = {}

local function establish(self, tree, host)
	Logger.log('CWMP', 'debug', 'Session: Attempting ACS session initiation.')

	local ok, code, acsResponse = self.acs:inform(tree, host)
	if not ok or not acsResponse then
		Logger.log('CWMP', 'error', 'Session: Inform failed: ' .. code .. ': ' .. (acsResponse and tostring(acsResponse) or '*empty*'))
		self.acs:close()
		return false
	elseif acsResponse.type ~= 'InformResponse' then
		Logger.log('CWMP', 'error', 'Session: Unexpected "' .. acsResponse.type .. '" response from Inform.')
		self.acs:close()
		return false
	else
		Logger.log('CWMP', 'debug', 'Session: RX ' .. acsResponse.type)
		-- clear events delivered successfully and parameter changes notified
		host:clearEventsByResponse(acsResponse.type)
		tree:clearChanged()
	end
	-- connection requests must never be retried
	host:deleteEventsByCode('6 CONNECTION REQUEST')

	-- version negotiation
	if CWMP.versionCompare(self.acs.version, '1.4') >= 0 then
		-- we are a v1.4+ CPE
		if acsResponse.useCWMPVersion then
			-- this is a v1.4+ ACS
			Logger.log('CWMP', 'debug', 'ACS responds: useCWMPversion=' .. acsResponse.useCWMPVersion)
			if not CWMP.versionExists(acsResponse.useCWMPVersion) then
				Logger.log('CWMP', 'error', 'CWMP version ' .. acsResponse.useCWMPVersion .. ' is not supported')
				self.acs:close()
				return false
			end
			self.acs.version = acsResponse.useCWMPVersion
			Logger.log('CWMP', 'info', 'CWMP version ' .. self.acs.version .. ' is negotiated')
		else
			-- this is a v1.3- ACS, check acsResponse.version (inferred from NS)
			Logger.log('CWMP', 'debug', 'Inferred ACS version=' .. acsResponse.version)
			if not CWMP.versionExists(acsResponse.version) then
				Logger.log('CWMP', 'error', 'CWMP version ' .. acsResponse.version .. ' is not supported')
				self.acs:close()
                return false
			end
			self.acs.version = acsResponse.version
			Logger.log('CWMP', 'info', 'CWMP version ' .. self.acs.version .. ' is negotiated')
		end
	elseif CWMP.versionCompare(self.acs.version, '1.1') >= 0 then
		-- we are a v1.1-v1.3 CPE, check acsResponse.version (inferred from NS)
		Logger.log('CWMP', 'debug', 'Inferred ACS version=' .. acsResponse.version)
		if CWMP.versionCompare(self.acs.version, acsResponse.version) > 0 then
			-- use min(ACS_ver, CPE_ver)
			if not CWMP.versionExists(acsResponse.version) then
				Logger.log('CWMP', 'error', 'CWMP version ' .. acsResponse.version .. ' is not supported')
				self.acs:close()
                return false
			end
			self.acs.version = acsResponse.version
			Logger.log('CWMP', 'info', 'CWMP version ' .. self.acs.version .. ' is negotiated')
		end
	end
	-- v1.0 CPE will never change its version

	-- At the moment, this is for debugging purpose. It could be used to implement version specific functionality later.
	luardb.set('tr069.active_version', self.acs.version)

	if acsResponse.holdRequests then
		Logger.log('CWMP', 'debug', 'Session: HoldRequests == ' .. acsResponse.holdRequests)
	end

	-- until the ACS clears HoldRequests we dutifully answer its calls
	local acsRequest
	local cpeResponse
	if acsResponse.holdRequests == '1' then
		Logger.log('CWMP', 'info', 'Requests held by ACS.')
		ok, code, acsRequest = self.acs:noContent()
		if not ok then
			Logger.log('CWMP', 'error', 'Empty 204 send failed: ' .. code .. ': ' .. acsRequest or '*empty*')
			self.acs:close()
			return false
		end
		while ok and acsRequest and acsRequest.holdRequests do
			Logger.log('CWMP', 'info', 'Session: RX ' .. acsRequest.type)
			cpeResponse = CWMP.RPC.call(acsRequest)
			ok, code, acsRequest = self.acs:sendResponse(cpeResponse)
			if not ok then
				Logger.log('CWMP', 'error', cpeResponse.type .. ' failed: ' .. code .. ': ' .. (acsRequest and tostring(acsRequest) or '*empty*'))
				self.acs:close()
				return false
			end
		end
	end


	-- now we can send our own
	-- the standard doesn't say what should happen if HoldRequests gets asserted again?

	-- Don't know ACS RPC methods available?  Ask!
	if #self.acs.rpcMethods < 1 then
		ok, code, acsResponse = self.acs:getRPCMethods()
		if not ok then
			Logger.log('CWMP', 'error', 'GetRPCMethods failed: ' .. code .. ': ' .. (acsResponse and tostring(acsResponse) or '*empty*'))
			self.acs:close()
			return false
		else
			host:clearEventsByResponse(acsResponse.type)
		end
		Logger.log('CWMP', 'info', 'Session: RX ' .. acsResponse.type)
	end

	-- completed transfers
	for _, transfer in ipairs(Transfer.getByState('completed')) do
		ok, code, acsResponse = self.acs:transferComplete(transfer)
		if not ok then
			Logger.log('CWMP', 'error', 'TransferComplete failed: ' .. code .. ': ' .. (acsResponse and tostring(acsResponse) or '*empty*'))
			self.acs:close()
			return false
		else
			-- should probably validate acsResponse.type == 'TransferCompleteResponse'
--			transfer:setStatus('archived')
			transfer:delete();
			host:clearEventsByResponse(acsResponse.type)
		end
		Logger.log('CWMP', 'info', 'Session: RX ' .. acsResponse.type)
	end


	-- nothing left to do, send 204s again
	-- processing anything the ACS might have us do
	ok, code, acsRequest = self.acs:noContent()
	if not ok then
		Logger.log('CWMP', 'error', 'Empty 204 send failed: ' .. code .. ': ' .. (acsRequest and tostring(acsRequest) or '*empty*'))
		self.acs:close()
		return false
	end
	while ok and acsRequest do
		Logger.log('CWMP', 'info', 'Session: RX ' .. acsRequest.type)
		cpeResponse = CWMP.RPC.call(acsRequest)
		ok, code, acsRequest = self.acs:sendResponse(cpeResponse)
		if not ok then
			Logger.log('CWMP', 'error', cpeResponse.type .. ' failed: ' .. code .. ': ' .. (acsRequest and tostring(acsRequest) or '*empty*'))
			self.acs:close()
			return false
		end
	end

	if ok then
		Logger.log('CWMP', 'debug', 'Session: RX Empty')
		self.acs.retries = 0
	end

	self.acs:sessionEnd();
	--TT300 -- to reuse the session ID. do not reset curl SETTINGS(not socket connection).
	-- the curl socket connection will start and end by "ch.perform"
	--self.acs:close()
	return ok
end

local function terminate(self)
	self.acs:close()
end

function CWMP.Session.new(acs)
	local session = {
		acs = acs,



		establish = establish,
		terminate = terminate,
	}



	return session
end

return CWMP.Session
