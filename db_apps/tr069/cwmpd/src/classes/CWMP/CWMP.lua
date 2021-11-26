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
Logger.addSubsystem('CWMP')

CWMP = {}

-- versions and their NS URNs
CWMP.versions = {
	['1.0'] = 'urn:dslforum-org:cwmp-1-0',
	['1.1'] = 'urn:dslforum-org:cwmp-1-1',
	['1.2'] = 'urn:dslforum-org:cwmp-1-2',
	['1.3'] = 'urn:dslforum-org:cwmp-1-2',
	['1.4'] = 'urn:dslforum-org:cwmp-1-2'
}

-- The unknown/invalid time value
CWMP.unknownTime = '0001-01-01T00:00:00Z'

--[[
	Format epoc time according to ISO 8601 datetime format:
	UTC:   YYYY-MM-DDThh:mm:ss[.ssssss]Z
	Local: YYYY-MM-DDThh:mm:ss[.ssssss]+/-hh:mm
	If time is earlier than year 2000 (approx), it is relative to boot,
	and there should be no timezone suffix.
	If time contains fractional part, it is inserted before timezone suffix
	with exactly 6 digits (zero-padded).
--]]
function CWMP.formatEpocTime(time, localtz)
	time = tonumber(time) or -1
	if time <= 0 then
		-- unknown time
		return CWMP.unknownTime
	end
	local microsec = math.floor((time - math.floor(time)) * 1000000)
	if microsec == 0 then
		microsec = '' -- do not include absent microsec portion
	else
		microsec = string.format('.%06d', microsec)
	end
	if time <= (30 * 366 * 86400) then
		-- near unix epoch - relative boot time
		-- which TR-069 wants as a year-0001 epoch relative time.
		-- this is a bit of a kludge!
		local year = tonumber(os.date('!%Y', time)) - 1969 -- 1970 is year 1
		return string.format('%04d', year) .. os.date('!-%m-%dT%T', time) .. microsec
	elseif localtz then
		-- local time zone
		local localtime = os.date('%FT%T%z',time)
		-- adjust according to XML format
		return localtime:sub(1,-6) .. microsec .. localtime:sub(-5, -3) .. ':' .. localtime:sub(-2)
	else
		-- UTC time
		return os.date('!%FT%T', time) .. microsec .. 'Z'
	end
end

-- event codes
CWMP.events = {
	['0 BOOTSTRAP'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'InformResponse'
	},
	['1 BOOT'] = {
		removedBy = { '0 BOOTSTRAP', '1 BOOT' },
		clearedBy = 'InformResponse'
	},
	['2 PERIODIC'] = {
		removedBy = { '0 BOOTSTRAP', '2 PERIODIC' },
		clearedBy = 'InformResponse'
	},
	['3 SCHEDULED'] = {
		removedBy = { '0 BOOTSTRAP', '3 SCHEDULED' },
		clearedBy = 'InformResponse'
	},
	['4 VALUE CHANGE'] = {
		removedBy = { '0 BOOTSTRAP', '1 BOOT', '4 VALUE CHANGE' },
		clearedBy = 'InformResponse'
	},
	['5 KICKED'] = {
		removedBy = { '0 BOOTSTRAP', '5 KICKED' },
		clearedBy = 'KickedResponse'
	},
	['6 CONNECTION REQUEST'] = {
		removedBy = { '0 BOOTSTRAP', '6 CONNECTION REQUEST' },
		clearedBy = 'InformResponse'
	},
	['7 TRANSFER COMPLETE'] = {
		removedBy = { '0 BOOTSTRAP', '7 TRANSFER COMPLETE' },
		clearedBy = 'TransferCompleteResponse'
	},
	['8 DIAGNOSTICS COMPLETE'] = {
		removedBy = { '0 BOOTSTRAP', '1 BOOT', '8 DIAGNOSTICS COMPLETE' },
		clearedBy = 'InformResponse'
	},
	['9 REQUEST DOWNLOAD'] = {
		removedBy = { '0 BOOTSTRAP', '9 REQUEST DOWNLOAD' },
		clearedBy = 'RequestDownloadResponse'
	},
	['10 AUTONOMOUS TRANSFER COMPLETE'] = {
		removedBy = { '0 BOOTSTRAP', '10 AUTONOMOUS TRANSFER COMPLETE' },
		clearedBy = 'AutonomousTransferCompleteResponse'
	},
	['11 DU STATE CHANGE COMPLETE'] = {
		removedBy = { '0 BOOTSTRAP', '11 DU STATE CHANGE COMPLETE' },
		clearedBy = 'DUStateChangeCompleteResponse'
	},
	['12 AUTONOMOUS DU STATE CHANGE COMPLETE'] = {
		removedBy = { '0 BOOTSTRAP', '12 AUTONOMOUS DU STATE CHANGE COMPLETE' },
		clearedBy = 'AutonomousDUStateChangeCompleteResponse'
	},

	['M Reboot'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'InformResponse'
	},
	['M ScheduleInform'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'InformResponse'
	},
	['M Download'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'TransferCompleteResponse'
	},
	['M ScheduledDownload'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'TransferCompleteResponse'
	},
	['M Upload'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'TransferCompleteResponse'
	},
	['M ChangeDUState'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'DUStateChangeCompleteResponse'
	},
	-- triggered by user(webui or reset button)
	['X OUI U Factory Reset'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'InformResponse'
	},
	-- triggered by tr069
	['X OUI M Factory Reset'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'InformResponse'
	},
	-- triggered by others
	['X OUI RG Factory Reset'] = {
		removedBy = { '0 BOOTSTRAP' },
		clearedBy = 'InformResponse'
	},
 }

-- simple types for parameter values
CWMP.simpleTypes = {
	'string',
	'int',
	'unsignedInt',
	'long',
	'unsignedLong',
	'boolean',
	'dateTime',
	'base64'
}

-- protocol element types
CWMP.types = {
	-- fault
	['Fault'] = {
		{ name = 'faultcode', type = { id = 'unsignedInt', min = 8000, max = 9899 } },
		{ name = 'faultstring', type = { id = 'string' } },
		{ name = 'detail', type = { id = 'object', class = 'FaultDetail' } },
		getDescription = function(obj) return 'Fault (' .. obj.detail.Fault.FaultCode .. ', "' .. obj.detail.Fault.FaultString .. '")' end
	},
	['FaultDetail'] = {
		{ name = 'Fault', type = { id = 'object', class = 'FaultStruct' } , forceNS = true },
		getDescription = function(obj) return 'FaultDetail (' .. obj.Fault.FaultCode .. ', "' .. obj.Fault.FaultString .. '")' end
	},
	['SetParameterValuesFault'] = {
		{ name = 'ParameterName', type = { id = 'string', max = 256 } },
		{ name = 'FaultCode', type = { id = 'unsignedInt', min = 8000, max = 9899 } },
		{ name = 'FaultString', type = { id = 'string' } },
		getDescription = function(obj) return 'SetParameterValuesFault (' .. obj.ParameterName .. ', ' .. obj.FaultCode .. ', "' .. obj.FaultString .. '")' end
	},

	-- methods
	['GetRPCMethods'] = {
	},
	['GetRPCMethodsResponse'] = {
		{ name = 'MethodList', type = { id = 'array', type = { id = 'string', max = 64 } } },
		getDescription = function(obj) return 'GetRPCMethodsResponse (' .. obj.MethodList:count() .. ' methods)' end
	},

	['SetParameterValues'] = {
		{ name = 'ParameterList', type = { id = 'array', type = { id = 'object', class = 'ParameterValueStruct' } } },
		{ name = 'ParameterKey', type = { id = 'string', max = 32 } },
		getDescription = function(obj) return 'SetParameterValues (key = "' .. obj.ParameterKey .. '", ' .. obj.ParameterList:count() .. ' parameters)' end
	},
	['SetParameterValuesResponse'] = {
		{ name = 'Status', type = { id = 'int', max = 1 } },
	},

	['GetParameterValues'] = {
		{ name = 'ParameterNames', type = { id = 'array', type = { id = 'string', max = 256 } } },
		getDescription = function(obj) return 'GetParameterValues (' .. obj.ParameterNames:count() .. ' paths)' end
	},
	['GetParameterValuesResponse'] = {
		{ name = 'ParameterList', type = { id = 'array', type = { id = 'object', class = 'ParameterValueStruct' } } },
		getDescription = function(obj) return 'GetParameterValuesResponse (' .. obj.ParameterList:count() .. ' parameters)' end
	},

	['GetParameterNames'] = {
		{ name = 'ParameterPath', type = { id = 'string', max = 256 } },
		{ name = 'NextLevel', type = { id = 'boolean' } },
		getDescription = function(obj) return 'GetParameterNames (path = "' .. obj.ParameterPath .. '", next = ' .. tostring(obj.NextLevel) .. ')' end
	},
	['GetParameterNamesResponse'] = {
		{ name = 'ParameterList', type = { id = 'array', type = { id = 'object', class = 'ParameterInfoStruct' } } },
		getDescription = function(obj) return 'GetParameterNamesResponse (' .. obj.ParameterList:count() .. ' parameters)' end
	},

	['SetParameterAttributes'] = {
		{ name = 'ParameterList', type = { id = 'array', type = { id = 'object', class = 'SetParameterAttributesStruct' } } },
		getDescription = function(obj) return 'SetParameterAttributes (' .. obj.ParameterList:count() .. ' parameters)' end
	},
	['SetParameterAttributesResponse'] = {
	},

	['GetParameterAttributes'] = {
		{ name = 'ParameterNames', type = { id = 'array', type = { id = 'string', max = 256 } } },
		getDescription = function(obj) return 'GetParameterAttributes (' .. obj.ParameterNames:count() .. ' paths)' end
	},
	['GetParameterAttributesResponse'] = {
		{ name = 'ParameterList', type = { id = 'array', type = { id = 'object', class = 'ParameterAttributeStruct' } } },
		getDescription = function(obj) return 'GetParameterAttributesResponse (' .. obj.ParameterList:count() .. ' parameters)' end
	},

	['AddObject'] = {
		{ name = 'ObjectName', type = { id = 'string', max = 256 } },
		{ name = 'ParameterKey', type = { id = 'string', max = 32 } },
		getDescription = function(obj) return 'AddObject (key = "' .. obj.ParameterKey .. '", path = "' .. obj.ObjectName .. '")' end
	},
	['AddObjectResponse'] = {
		{ name = 'InstanceNumber', type = { id = 'unsignedInt', min = 1 } },
		{ name = 'Status', type = { id = 'int', max = 1 } },
		getDescription = function(obj) return 'AddObjectResponse (id = ' .. obj.InstanceNumber .. ', status = ' .. obj.Status .. ')' end
	},

	['DeleteObject'] = {
		{ name = 'ObjectName', type = { id = 'string', max = 256 } },
		{ name = 'ParameterKey', type = { id = 'string', max = 32 } },
		getDescription = function(obj) return 'DeleteObject (key = "' .. obj.ParameterKey .. '", path = "' .. obj.ObjectName .. '")' end
	},
	['DeleteObjectResponse'] = {
		{ name = 'Status', type = { id = 'int', max = 1 } },
		getDescription = function(obj) return 'DeleteObjectResponse (status = ' .. obj.Status .. ')' end
	},

	['Download'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		{ name = 'FileType', type = { id = 'string', max = 64 } },
		{ name = 'URL', type = { id = 'string', max = 256 } },
		{ name = 'Username', type = { id = 'string', max = 256 } },
		{ name = 'Password', type = { id = 'string', max = 256 } },
		{ name = 'FileSize', type = { id = 'unsignedInt' } },
		{ name = 'TargetFileName', type = { id = 'string', max = 256 } },
		{ name = 'DelaySeconds', type = { id = 'unsignedInt' } },
		{ name = 'SuccessURL', type = { id = 'string', max = 256 } },
		{ name = 'FailureURL', type = { id = 'string', max = 256 } },
		getDescription = function(obj) return 'Download (key = "' .. obj.CommandKey .. '", type = "' .. obj.FileType .. '", url = "' .. obj.URL .. '", when = ' .. obj.DelaySeconds .. ')' end
	},
	['DownloadResponse'] = {
		{ name = 'Status', type = { id = 'int', max = 1 } },
		{ name = 'StartTime', type = { id = 'dateTime' } },
		{ name = 'CompleteTime', type = { id = 'dateTime' } },
		getDescription = function(obj) return 'DownloadResponse (status = ' .. obj.Status .. ')' end
	},

	['Upload'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		{ name = 'FileType', type = { id = 'string', max = 64 } },
		{ name = 'URL', type = { id = 'string', max = 256 } },
		{ name = 'Username', type = { id = 'string', max = 256 } },
		{ name = 'Password', type = { id = 'string', max = 256 } },
		{ name = 'DelaySeconds', type = { id = 'unsignedInt' } },
		getDescription = function(obj) return 'Upload (key = "' .. obj.CommandKey .. '", type = "' .. obj.FileType .. '", url = "' .. obj.URL .. '", when = ' .. obj.DelaySeconds .. ')' end
	},
	['UploadResponse'] = {
		{ name = 'Status', type = { id = 'int', max = 1 } },
		{ name = 'StartTime', type = { id = 'dateTime' } },
		{ name = 'CompleteTime', type = { id = 'dateTime' } },
		getDescription = function(obj) return 'UploadResponse (status = ' .. obj.Status .. ')' end
	},

	['Reboot'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		getDescription = function(obj) return 'Reboot (key = "' .. obj.CommandKey .. '")' end
	},
	['RebootResponse'] = {
	},

	['Inform'] = {
		{ name = 'DeviceId', type = { id = 'object', class = 'DeviceIdStruct' } },
		{ name = 'Event', type = { id = 'array', type = { id = 'object', class = 'EventStruct' }, min = 1, max = 64 } },
		{ name = 'MaxEnvelopes', type = { id = 'unsignedInt', min = 1, max = 1 } },
		{ name = 'CurrentTime', type = { id = 'dateTime' } },
		{ name = 'RetryCount', type = { id = 'unsignedInt' } },
		{ name = 'ParameterList', type = { id = 'array', type = { id = 'object', class = 'ParameterValueStruct' } } },
		getDescription = function(obj) return 'Inform (' .. obj.Event:count() .. ' events, ' .. obj.ParameterList:count() .. ' parameters)' end
	},
	['InformResponse'] = {
		{ name = 'MaxEnvelopes', type = { id = 'unsignedInt', min = 1, max = 1 } },
	},

	['TransferComplete'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		{ name = 'FaultStruct', type = { id = 'object', class = 'FaultStruct' } },
		{ name = 'StartTime', type = { id = 'dateTime' } },
		{ name = 'CompleteTime', type = { id = 'dateTime' } },
		getDescription = function(obj) return 'TransferComplete (key = "' .. obj.CommandKey .. '")' end
	},
	['TransferCompleteResponse'] = {
	},

	['AutonomousTransferComplete'] = {
		{ name = 'AnnounceURL', type = { id = 'string', max = 1024 } },
		{ name = 'TransferURL', type = { id = 'string', max = 1024 } },
		{ name = 'IsDownload', type = { id = 'boolean' } },
		{ name = 'FileType', type = { id = 'string', max = 64 } },
		{ name = 'FileSize', type = { id = 'unsignedInt' } },
		{ name = 'TargetFileName', type = { id = 'string', max = 256 } },
		{ name = 'FaultStruct', type = { id = 'object', class = 'FaultStruct' } },
		{ name = 'StartTime', type = { id = 'dateTime' } },
		{ name = 'CompleteTime', type = { id = 'dateTime' } },
	},
	['AutonomousTransferCompleteResponse'] = {
	},

	['GetQueuedTransfers'] = {
	},
	['GetQueuedTransfersResponse'] = {
		{ name = 'TransferList', type = { id = 'array', type = { id = 'object', class = 'QueuedTransferStruct' }, max = 16 } },
		getDescription = function(obj) return 'GetQueuedTransfersResponse (' .. obj.TransferList:count() .. ' transfers)' end
	},

	['ScheduleInform'] = {
		{ name = 'DelaySeconds', type = { id = 'unsignedInt', min = 1 } },
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		getDescription = function(obj) return 'ScheduleInform (key = "' .. obj.CommandKey .. '", when = ' .. obj.DelaySeconds .. ')' end
	},
	['ScheduleInformResponse'] = {
	},

	['Upload'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		{ name = 'FileType', type = { id = 'string', max = 64 } },
		{ name = 'URL', type = { id = 'string', max = 256 } },
		{ name = 'Username', type = { id = 'string', max = 256 } },
		{ name = 'Password', type = { id = 'string', max = 256 } },
		{ name = 'DelaySeconds', type = { id = 'unsignedInt' } },
		getDescription = function(obj) return 'Upload (key = "' .. obj.CommandKey .. '", type = "' .. obj.FileType .. '", url = "' .. obj.URL .. '". when = ' .. obj.DelaySeconds .. ')' end
	},
	['UploadResponse'] = {
		{ name = 'Status', type = { id = 'int', max = 1 } },
		{ name = 'StartTime', type = { id = 'dateTime' } },
		{ name = 'CompleteTime', type = { id = 'dateTime' } },
		getDescription = function(obj) return 'UploadResponse (status = ' .. obj.Status .. ')' end
	},

	['FactoryReset'] = {
	},
	['FactoryResetResponse'] = {
	},

	['GetAllQueuedTransfers'] = {
	},
	['GetAllQueuedTransfersResponse'] = {
		{ name = 'TransferList', type = { id = 'array', type = { id = 'object', class = 'AllQueuedTransferStruct' }, max = 16 } },
		getDescription = function(obj) return 'GetAllQueuedTransfersResponse (' .. obj.TransferList:count() .. ' transfers)' end
	},

	['ScheduleDownload'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		{ name = 'FileType', type = { id = 'string', max = 64 } },
		{ name = 'URL', type = { id = 'string', max = 256 } },
		{ name = 'Username', type = { id = 'string', max = 256 } },
		{ name = 'Password', type = { id = 'string', max = 256 } },
		{ name = 'FileSize', type = { id = 'unsignedInt' } },
		{ name = 'TargetFileName', type = { id = 'string', max = 256 } },
		{ name = 'TimeWindowList', type = { id = 'array', type = { id = 'object', class = 'TimeWindowStruct' }, min = 1, max = 2 } },
		getDescription = function(obj) return 'ScheduleDownload (key = "' .. obj.CommandKey .. '", type = "' .. obj.FileType .. '", url = "' .. obj.URL .. '", ' .. CWMP.Array.count(obj.TimeWindowList) .. ' windows)' end
	},
	['ScheduleDownloadResponse'] = {
	},

	['CancelTransfer'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		getDescription = function(obj) return 'CancelTransfer ("' .. obj.CommandKey .. '")' end
	},
	['CancelTransferResponse'] = {
	},

	['RequestDownload'] = {
		{ name = 'FileType', type = { id = 'string', max = 64 } },
		{ name = 'FileTypeArg', type = { id = 'array', type = { id = 'object', class = 'ArgStruct' }, max = 16 } },
		getDescription = function(obj) return 'RequestDownload (type = "' .. obj.FileType .. '", ' .. obj.FileTypeArg:count() .. ' args)' end
	},
	['RequestDownloadResponse'] = {
	},

	-- structs
	['ParameterValueStruct'] = {
		{ name = 'Name', type = { id = 'string', max = 256 } },
		{ name = 'Value', type = { id = 'anySimpleType' } },
	},
	['ParameterInfoStruct'] = {
		{ name = 'Name', type = { id = 'string', max = 256 } },
		{ name = 'Writable', type = { id = 'boolean' } },
	},
	['SetParameterAttributesStruct'] = {
		{ name = 'Name', type = { id = 'string', max = 256 } },
		{ name = 'NotificationChange', type = { id = 'boolean' } },
		{ name = 'Notification', type = { id = 'int', max = 2 } },
		{ name = 'AccessListChange', type = { id = 'boolean' } },
		{ name = 'AccessList', type = { id = 'array', type = { id = 'string', max = 64 } } },
	},
	['ParameterAttributeStruct'] = {
		{ name = 'Name', type = { id = 'string', max = 256 } },
		{ name = 'Notification', type = { id = 'int', min = 0, max = 2 } },
		{ name = 'AccessList', type = { id = 'array', type = { id = 'string', max = 64 } } },
	},
	['DeviceIdStruct'] = {
		{ name = 'Manufacturer', type = { id = 'string', max = 64 } },
		{ name = 'OUI', type = { id = 'string', max = 6 } },
		{ name = 'ProductClass', type = { id = 'string', max = 64 } },
		{ name = 'SerialNumber', type = { id = 'string', max = 64 } },
	},
	['EventStruct'] = {
		{ name = 'EventCode', type = { id = 'string', max = 64 } },
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
	},
	['FaultStruct'] = {
		{ name = 'FaultCode', type = { id = 'unsignedInt', min = 8000, max = 9899 } },
		{ name = 'FaultString', type = { id = 'string', } },
	},
	['QueuedTransferStruct'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		{ name = 'State', type = { id = 'int', min = 1, max = 3 } },
	},
	['AllQueuedTransferStruct'] = {
		{ name = 'CommandKey', type = { id = 'string', max = 32 } },
		{ name = 'State', type = { id = 'int', min = 1, max = 3 } },
		{ name = 'IsDownload', type = { id = 'boolean' } },
		{ name = 'FileType', type = { id = 'string', max = 64 } },
		{ name = 'FileSize', type = { id = 'unsignedInt' } },
		{ name = 'TargetFileName', type = { id = 'string', max = 256 } },
	},
	['TimeWindowStruct'] = {
		{ name = 'WindowStart', type = { id = 'unsignedInt' } },
		{ name = 'WindowEnd', type = { id = 'unsignedInt' } },
		{ name = 'WindowMode', type = { id = 'string', max = 64 } },
		{ name = 'UserMessage', type = { id = 'string', max = 256 } },
		{ name = 'MaxRetries', type = { id = 'int', min = -1 } },
	},
	['ArgStruct'] = {
		{ name = 'Name', type = { id = 'string', max = 64 } },
		{ name = 'Value', type = { id = 'string', max = 256 } },
	},
}

function CWMP.versionExists(ver)
	if CWMP.versions[ver] then return true end
	return false
end

-- compare ver1 and ver2, both as string 'major.minor'
-- return 0 if ver1==ver2, negative if ver1<ver2, positive if ver1>ver2, nil if error
function CWMP.versionCompare(ver1, ver2)
	if ver1 == ver2 then return 0 end
	if not ver1 or not ver2 then return nil end

	local major1, minor1 = ver1:match('(%d+)%.(%d+)')
	local major2, minor2 = ver2:match('(%d+)%.(%d+)')

	if not major1 or not major2 then return nil end

	major1, minor1 = tonumber(major1), tonumber(minor1)
	major2, minor2 = tonumber(major2), tonumber(minor2)

	return (major1 == major2) and (minor1 - minor2) or (major1 - major2)
end

function CWMP.eventExists(eventCode)
	if CWMP.events[eventCode] then return true end
	return false
end

function CWMP.typeExists(typeName)
	if CWMP.types[typeName] then return true end
	return false
end

function CWMP.isSimpleType(typeName)
	return table.contains(CWMP.simpleTypes, typeName)
end

function CWMP.memberExists(typeName, memberName)
	local typeInfo = CWMP.types[typeName]
	if not typeInfo then return false end
	for idx, memberInfo in ipairs(typeInfo) do
		if memberInfo.name == memberName then return true end
	end
	return false
end

function CWMP.getMemberType(typeName, memberName)
	local typeInfo = CWMP.types[typeName]
	if not typeInfo then error('No such CWMP type "' .. typeName .. '".') end
	for idx, memberInfo in ipairs(typeInfo) do
		if memberInfo.name == memberName then return memberInfo.type end
	end
	error('CWMP type "' .. typeName .. '" has no member "' .. memberName .. '".')
end

function CWMP.validMemberValue(typeName, memberName, value)
	-- FIXME implement validation!
	return true
end

function CWMP.validArrayValue(typeInfo, idx, value)
	-- FIXME implement validation!
	return true
end

return CWMP
