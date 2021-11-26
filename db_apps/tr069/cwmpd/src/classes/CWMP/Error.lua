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
require('CWMP.Message')

local cwmpErrors = {
	-- ACS
	[8000] = { type = 'ACS', name = 'MethodNotSupported', description = 'Method not supported' },
	[8001] = { type = 'ACS', name = 'RequestDenied', description = 'Request denied' },
	[8002] = { type = 'ACS', name = 'InternalError', description = 'Internal error' },
	[8003] = { type = 'ACS', name = 'InvalidArguments', description = 'Invalid arguments' },
	[8004] = { type = 'ACS', name = 'ResourcesExceeded', description = 'Resources exceeded' },
	[8005] = { type = 'ACS', name = 'RetryRequest', description = 'Retry request' },

	-- CPE
	[9000] = { type = 'CPE', name = 'MethodNotSupported', description = 'Method not supported' },
	[9001] = { type = 'CPE', name = 'RequestDenied', description = 'Request denied' },
	[9002] = { type = 'CPE', name = 'InternalError', description = 'Internal error' },
	[9003] = { type = 'CPE', name = 'InvalidArguments', description = 'Invalid arguments' },
	[9004] = { type = 'CPE', name = 'ResourcesExceeded', description = 'Resources exceeded' },
	[9005] = { type = 'CPE', name = 'InvalidParameterName', description = 'Invalid parameter name' },
	[9006] = { type = 'CPE', name = 'InvalidParameterType', description = 'Invalid parameter type' },
	[9007] = { type = 'CPE', name = 'InvalidParameterValue', description = 'Invalid parameter value' },
	[9008] = { type = 'CPE', name = 'ReadOnly', description = 'Attempt to set non-writable parameter' },
	[9009] = { type = 'CPE', name = 'NotificationRequestRejected', description = 'Notification request rejected' },
	[9010] = { type = 'CPE', name = 'TransferDownloadFailure', description = 'File transfer failure' },
	[9011] = { type = 'CPE', name = 'TransferUploadFailure', description = 'Upload failure' },
	[9012] = { type = 'CPE', name = 'TransferServerAuthFailure', description = 'File transfer server authentication failure' },
	[9013] = { type = 'CPE', name = 'TransferUnsupportedProtocol', description = 'Unsupported protocol for file transfer' },
	[9014] = { type = 'CPE', name = 'TransferMulticastFailure', description = 'File transfer failure: unable to join multicast group' },
	[9015] = { type = 'CPE', name = 'TransferServerUnreachable', description = 'File transfer failure: unable to contact file server' },
	[9016] = { type = 'CPE', name = 'TransferFileUnaccessable', description = 'File transfer failure: unable to access file' },
	[9017] = { type = 'CPE', name = 'TransferIncomplete', description = 'File transfer failure: unable to complete download' },
	[9018] = { type = 'CPE', name = 'TransferCorrupt', description = 'File transfer failure: file corrupted or otherwise unusable' },
	[9019] = { type = 'CPE', name = 'TransferAuthFailure', description = 'File transfer failure: file authentication failure' },
	[9020] = { type = 'CPE', name = 'TransferWindowExceeded', description = 'File transfer failure: unable to complete download within specified time windows' },
	[9021] = { type = 'CPE', name = 'TransferUncancellable', description = 'Cancelation of file tranfer not permitted in current transfer state' },
	[9022] = { type = 'CPE', name = 'UninitializedParameterValue', description = 'Uninitilaized parameter value' },
}

CWMP.Error = {}

local funcs = {}
for k, v in pairs(cwmpErrors) do
	if v.type == 'CPE' then
		funcs[v.name] = function() return k end
		CWMP.Error[v.name] = k
	end
end
CWMP.Error.funcs = funcs

function CWMP.Error.getByName(name, agentType)
	agentType = agentType or 'CPE'
	for code, obj in pairs(cwmpErrors) do
		if obj.name == name and obj.type == agentType then
			local err = {}
			err.code = code
			err.name = obj.name
			err.description = obj.description
			err.func = function() return code end
			return err
		end
	end
	error('Unknown CWMP Error Name "' .. name .. '" for agent type "' .. agentType .. '".')
end

function CWMP.Error.getByCode(code)
	id = tonumber(code)
	assert(cwmpErrors[code], 'Unknown CWMP Error Code "' .. code .. '".')
	local err = {}
	err.code = code
	err.name = cwmpErrors[code].name
	err.description = cwmpErrors[code].description
	err.func = function() return code end
	return err
end

function CWMP.Error.getFault(codeOrName, msg)
	local err
	if type(codeOrName) == 'string' then
		err = CWMP.Error.getByName(codeOrName)
	elseif type(codeOrName) == 'number' then
		err = CWMP.Error.getByCode(codeOrName)
	else
		error('Expected code as integer or name as string, got ' .. type(codeOrName) .. '.')
	end
	local cwmp = CWMP.Message.new('Fault')
	cwmp.message.faultcode = 'Client'
	cwmp.message.faultstring = 'CWMP fault'
	cwmp.message.detail.Fault.FaultCode = tostring(err.code)
	cwmp.message.detail.Fault.FaultString = err.description .. (msg and (': ' .. msg) or '')
	return cwmp
end

function CWMP.Error.getSetParameterValuesFault(paramName, codeOrName, msg)
	local err
	if type(codeOrName) == 'string' then
		err = CWMP.Error.getByName(codeOrName)
	else
		err = CWMP.Error.getByCode(codeOrName)
	end
	local cwmp = CWMP.Object.new('SetParameterValuesFault', 'SetParameterValuesFault', true)
	cwmp.ParameterName = paramName
	cwmp.FaultCode = tostring(err.code)
	cwmp.FaultString = err.description .. (msg and (': ' .. msg) or '')
	return cwmp
end

return CWMP.Error
