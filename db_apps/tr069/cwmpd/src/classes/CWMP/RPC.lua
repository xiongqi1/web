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
require('CWMP.Object')
require('CWMP.Error')
require('Parameter')
require('Transfer')

local function nonEmptyOrNil(str)
	if str == '' then return nil else return str end
end

local implementedCalls = {
	['_UnknownMethod'] = {
		public = false,
		handle = function(request)
			return CWMP.Error.getFault('MethodNotSupported')
		end	
	},
	['GetRPCMethods'] = {
		public = true,
		handle = function(request)
			local reply = CWMP.Message.new('GetRPCMethodsResponse')
			for name, data in pairs(CWMP.RPC._calls) do
				if data.public then
					Logger.log('CWMP', 'debug', 'RPC: GetRPCMethods: ' .. name)
					reply.message.MethodList:add(name)
				end
			end
			return reply
		end
	},
	['GetParameterNames'] = {
		public = true,
		handle = function(request)
			local path = request.message.ParameterPath
			local nextLevel = request.message.NextLevel
			local reply = CWMP.Message.new('GetParameterNamesResponse')
			if nextLevel and not path:endsWith('.') and path ~= '' then
				return CWMP.Error.getFault('InvalidArguments', 'Path must end with a period or be empty when nextLevel is true.')
			end
			local parameters = paramTree:getByName(path, nextLevel)
			if not parameters then
				Logger.log('CWMP', 'warning', 'RPC: GetParameterNames: Invalid parameter name "' .. path .. '".')
				return CWMP.Error.getFault('InvalidParameterName', path)
			end
			for _, parameter in ipairs(parameters) do
				local paramInfo = CWMP.Object.new('ParameterInfoStruct')
				paramInfo.Name = parameter:getPath(true)
				paramInfo.Writable = parameter:isWritable()
				reply.message.ParameterList:add(paramInfo)
				Logger.log('CWMP', 'debug', 'RPC: GetParameterNames: ' .. (paramInfo.Writable and 'RW' or 'RO') .. ' ' .. paramInfo.Name)
			end
			return reply
		end	
	},
	['GetParameterValues'] = {
		public = true,
		handle = function(request)
			local parameters = request.message.ParameterNames
			local reply = CWMP.Message.new('GetParameterValuesResponse')
			for i = 1, parameters:count() do
				local path = parameters[i]
				local parameters = paramTree:getByName(path)
				if not parameters then
					Logger.log('CWMP', 'warning', 'RPC: GetParameterValues: Invalid parameter name "' .. path .. '".')
					return CWMP.Error.getFault('InvalidParameterName', path)	
				end
				for _, parameter in ipairs(parameters) do
					if parameter:isParameter() then  -- TT918, parameterValue only include parameters, no object
						local paramInfo = CWMP.Object.new('ParameterValueStruct')
						paramInfo.Name = parameter:getPath(true)
						if parameter.access == 'writeonly'  then
							--[[ Write-only parameters must also be readable.
							     CWMP specifies string type should be read as
							     empty string. But it does not say anything for
							     non-string types. Here, we use default value
							     for non-string types.
							--]]
							local cwmpType = parameter:getCWMPType()
							paramInfo:setMemberType('Value', cwmpType)
							if cwmpType == 'string' then
								paramInfo.Value = ''
							else
								paramInfo.Value = parameter:nodeToCWMP(parameter.default)
							end
						else
							paramInfo:setMemberType('Value', parameter:getCWMPType())
							local ret, value = parameter:getCWMPValue()
							if ret > 0 then
								Logger.log('CWMP', 'error', 'RPC: GetParameterValues: getCWMPValue("' .. paramInfo.Name .. '") error ' .. ret .. (msg and (': ' .. msg) or '.'))
								return CWMP.Error.getFault('InternalError', 'RPC: GetParameterValues: getCWMPValue("' .. paramInfo.Name .. '") error ' .. ret .. (msg and (': ' .. msg) or '.'))
							end
							paramInfo.Value = value
							if tostring(value) == 'nil' then
								Logger.log('CWMP', 'error', 'ERROR!! RPC: GetParameterValues: ' .. paramInfo.Name .. ' := "nil".')
							end

							Logger.log('CWMP', 'debug', 'RPC: GetParameterValues: ' .. paramInfo.Name .. ' := "' .. tostring(paramInfo.Value) .. '".')
						end
						
						reply.message.ParameterList:add(paramInfo)
					end
				end
			end
			return reply
		end	
	},
	['SetParameterValues'] = {
		public = true,
		handle = function(request)
			local errors = 0
			local keyValue = request.message.ParameterKey
			local parameters = request.message.ParameterList
			local fault = CWMP.Error.getFault('InvalidArguments')

			-- value of "Status" argument of SetParameterValuesResponse message.
			--
			--    * Description on "Status" argument:
			-- A successful response to SetParameterValuesResponse method
			-- returns an integer enumeration defined as follows:
			-- 0 = All Parameter changes have been validated and applied.
			-- 1 = All Parameter changes have been validated and committed, but some or all are not yet
			-- applied (for example, if a reboot is required before the new values are applied).
			local spvRes_status = 0

			for i = 1, parameters:count() do
				local paramSet = parameters[i]
				local parameter = paramTree:find(paramSet.Name)
				if not parameter then
					local spvf = CWMP.Error.getSetParameterValuesFault(paramSet.Name, 'InvalidParameterName')
					fault.message.detail.Fault:addExtraChild(spvf)
					Logger.log('CWMP', 'warning', 'RPC: SetParameterValues: Invalid parameter name "' .. paramSet.Name .. '".')
					errors = errors + 1
				elseif not parameter:isParameter() then
					local spvf = CWMP.Error.getSetParameterValuesFault(paramSet.Name, 'InvalidArguments', paramSet.Name .. ' is not a parameter.')
					fault.message.detail.Fault:addExtraChild(spvf)
					Logger.log('CWMP', 'warning', 'RPC: SetParameterValues: "' .. paramSet.Name .. '" is not a parameter.')
					errors = errors + 1
				elseif parameter.access == 'readonly' then
					local spvf = CWMP.Error.getSetParameterValuesFault(paramSet.Name, 'ReadOnly', paramSet.Name .. ' is a read-only parameter.')
					fault.message.detail.Fault:addExtraChild(spvf)
					Logger.log('CWMP', 'warning', 'RPC: SetParameterValues: "' .. paramSet.Name .. '" is read-only.')
					errors = errors + 1
				elseif parameter:getCWMPType() ~= paramSet:getMemberType('Value') then
					local spvf = CWMP.Error.getSetParameterValuesFault(paramSet.Name, 'InvalidParameterType', paramSet.Name .. ' is a ' .. parameter:getCWMPType() .. ' parameter.')
					fault.message.detail.Fault:addExtraChild(spvf)
					Logger.log('CWMP', 'warning', 'RPC: SetParameterValues: "' .. paramSet.Name .. '" is of type ' .. parameter:getCWMPType() .. ' ACS used ' .. paramSet:getMemberType('Value') .. '.')
					errors = errors + 1
				else
					local ret, msg = parameter:setCWMPValue(paramSet.Value)
					if ret > 1 then
						local spvf = CWMP.Error.getSetParameterValuesFault(paramSet.Name, ret, msg)
						fault.message.detail.Fault:addExtraChild(spvf)
						Logger.log('CWMP', 'error', 'RPC: SetParameterValues: setCWMPValue("' .. paramSet.Name .. '") error ' .. ret .. (msg and (': ' .. msg) or '.'))
						errors = errors + 1
					elseif ret == 1 then
						spvRes_status = 1
					else
						local displayValue
						-- Hide the value if reading is not allowed.
						if parameter.access == 'writeonly' then
							displayValue = '********'
						else
							displayValue = tostring(paramSet.Value)
						end
						Logger.log('CWMP', 'notice', 'SetParameterValues: ' .. paramSet.Name .. ' := "' .. displayValue .. '".')
					end
				end
			end
			if errors > 0 then
				return fault
			else
				local ret, msg = paramTree:setParameterKey(keyValue)
				if ret ~= 0 then
					return CWMP.Error.getFault(ret, msg)
				end
				local reply = CWMP.Message.new('SetParameterValuesResponse')
				reply.message.Status = spvRes_status
				return reply
			end
		end	
	},
	['GetParameterAttributes'] = {
		public = true,
		handle = function(request)
			local parameters = request.message.ParameterNames
			local reply = CWMP.Message.new('GetParameterAttributesResponse')
			for i = 1, parameters:count() do
				local path = parameters[i]
				local parameters = paramTree:getByName(path)
				if not parameters then
					return CWMP.Error.getFault('InvalidParameterName', path)					
				end
				for _, parameter in ipairs(parameters) do
					if parameter:isParameter() then
						local paramInfo = CWMP.Object.new('ParameterAttributeStruct', 'ParameterAttributeStruct', true)
						paramInfo.Name = parameter:getPath(true)
						paramInfo.Notification = parameter.notify
						for _, entity in ipairs(parameter.accessList) do
							paramInfo.AccessList:add(entity)
						end
						reply.message.ParameterList:add(paramInfo)
					end
				end
			end
			return reply
		end	
	},
	['SetParameterAttributes'] = {
		public = true,
		handle = function(request)
			local paramAttrs = request.message.ParameterList
			for i = 1, paramAttrs:count() do
				local paramAttr = paramAttrs[i]
				local parameters = paramTree:getByName(paramAttr.Name)
				if not parameters or #parameters < 1 then
					return CWMP.Error.getFault('InvalidParameterName', paramAttr.Name)
				end
				for _, parameter in ipairs(parameters) do
					if parameter:isParameter() then
						local notify = nil
						local accessList = nil
						if paramAttr.NotificationChange then
							notify = paramAttr.Notification
							if notify > parameter.maxNotify or notify < parameter.minNotify then
								-- FIXME: atomicity is required by standard!
								return CWMP.Error.getFault('NotificationRequestRejected', parameter:getPath(true) .. ' notify range [' .. parameter.minNotify .. ':' .. parameter.maxNotify .. '] ' .. notify .. ' requested.')
							end
						end
						if paramAttr.AccessListChange then
							accessList = {}
							for i = 1, paramAttr.AccessList:count() do
								table.insert(accessList, paramAttr.AccessList[i])
							end
						end
						local ret, msg = parameter:setCWMPAttributes(notify, accessList)
						if ret > 0 then
							Logger.log('CWMP', 'error', 'RPC: SetParameterAttributes: setCWMPAttributes("' .. parameter:getPath(true) .. '") error ' .. ret .. (msg and (': ' .. msg) or '.'))
							return CWMP.Error.getFault('InternalError', 'RPC: SetParameterAttributes: setCWMPAttributes("' .. parameter:getPath(true) .. '") error ' .. ret .. (msg and (': ' .. msg) or '.'))
						else
							Logger.log('CWMP', 'notice', 'SetParameterAttributes: ' .. parameter:getPath(true) .. ' := ' .. tostring(notify) .. ', ' .. (accessList and ('"' .. table.concat(accessList, ', ') .. '"') or 'nil') .. '.')
						end
					end
				end
			end
			return CWMP.Message.new('SetParameterAttributesResponse')
		end	
	},
	['AddObject'] = {
		public = true,
		handle = function(request)
			-- instance create
			local parameter = paramTree:find(request.message.ObjectName)
			if not parameter or not paramTree:isValidPartialPathName(request.message.ObjectName) then
				return CWMP.Error.getFault('InvalidParameterName', request.message.ObjectName)
			end
			if parameter.type ~= 'collection' then
				return CWMP.Error.getFault('InvalidArguments', request.message.ObjectName .. ' is not a multi-instance object.')
			end
			if parameter.access == 'readonly' then
				return CWMP.Error.getFault('InvalidArguments', request.message.ObjectName .. ' is not instantiable.')
			end
			local ret, instanceId = parameter:createInstance(parameter:getPath(true))
			assert(type(ret) == 'number', 'Expected numeric result code from parameter:createInstance("' .. request.message.ObjectName .. '"), got ' .. type(ret) .. '.')
			if ret ~= 0 then
				return CWMP.Error.getFault(ret, instanceId)
			end
			Logger.log('CWMP', 'notice', 'AddObject: ' .. request.message.ObjectName .. ' -> ' .. instanceId .. '.')

			-- ParameterKey update
			local keyValue = request.message.ParameterKey
			local ret, msg = paramTree:setParameterKey(keyValue)
			if ret ~= 0 then
				return CWMP.Error.getFault(ret, msg)
			end

			local response = CWMP.Message.new('AddObjectResponse')
			response.message.InstanceNumber = instanceId
			response.message.Status = 0
			return response
		end	
	},
	['DeleteObject'] = {
		public = true,
		handle = function(request)
			-- instance delete
			local parameter = paramTree:find(request.message.ObjectName)
			if not parameter or not paramTree:isValidPartialPathName(request.message.ObjectName) then
				return CWMP.Error.getFault('InvalidParameterName', request.message.ObjectName)
			end
			if parameter.type ~= 'object' then
				return CWMP.Error.getFault('InvalidArguments', request.message.ObjectName .. ' is not an object.')
			end
			if parameter.access == 'readonly' then
				return CWMP.Error.getFault('InvalidArguments', request.message.ObjectName .. ' is not deletable.')
			end
			local ret = parameter:deleteInstance(parameter:getPath(true))
			if ret ~= 0 then
				return CWMP.Error.getFault(ret)
			end
			Logger.log('CWMP', 'notice', 'DeleteObject: ' .. request.message.ObjectName)

			-- ParameterKey update
			local keyValue = request.message.ParameterKey
			local ret, msg = paramTree:setParameterKey(keyValue)
			if ret ~= 0 then
				return CWMP.Error.getFault(ret, msg)
			end

			local response = CWMP.Message.new('DeleteObjectResponse')
			response.message.Status = 0
			return response
		end	
	},
	['Reboot'] = {
		public = true,
		handle = function(request)
			local ret, msg = client.host:reboot(request.message.CommandKey)
			if ret ~= 0 then
				return CWMP.Error.getFault(ret, msg)
			end
			return CWMP.Message.new('RebootResponse')
		end	
	},
	['FactoryReset'] = {
		public = true,
		handle = function(request)
			local ret, msg = client.host:factoryReset()
			if ret ~= 0 then
				return CWMP.Error.getFault(ret, msg)
			end
			return CWMP.Message.new('FactoryResetResponse')
		end	
	},
	['Download'] = {
		public = true,
		handle = function(request)
			-- validate transport type support
			if not Transfer.isSupportedTransportURLType(request.message.URL) then
				return CWMP.Error.getFault('TransferUnsupportedProtocol')
			end
			
			-- validate file type
			if not Transfer.isSupportedFileType('download', request.message.FileType) then
				return CWMP.Error.getFault('InvalidArguments', 'File type "' .. request.message.FileType .. '" not supported.')
			end

			-- validate the number of download requests
			if (type(conf.rdb.downloadMaxRequests) == 'string') then
				local numRequests = Transfer.getNumTransfersByType('download')
				local maxRequests = tonumber(luardb.get(conf.rdb.downloadMaxRequests))
				if (numRequests > maxRequests) then
					return CWMP.Error.getFault('ResourcesExceeded',
						'Exceeded number of download requests at a time:'
						.. tostring(maxRequests))
				end
			end

			-- queue transfer
			local transfer = Transfer.new('download')
			transfer.key = request.message.CommandKey
			transfer.fileType = request.message.FileType
			transfer.url = request.message.URL
			transfer.username = nonEmptyOrNil(request.message.Username)
			transfer.password = nonEmptyOrNil(request.message.Password)
			transfer.size = request.message.FileSize
			transfer.target = nonEmptyOrNil(request.message.TargetFileName)
			transfer.delay = request.message.DelaySeconds
			transfer.successURL = nonEmptyOrNil(request.message.SuccessURL)
			transfer.failureURL = nonEmptyOrNil(request.message.FailureURL)
			transfer:persist()

			local response = CWMP.Message.new('DownloadResponse')
			response.message.Status = 1 -- queued
			if transfer.delay == 0 then
				response.message.StartTime = os.date('%FT%TZ')
			else
				response.message.StartTime = CWMP.unknownTime
			end
			response.message.CompleteTime = CWMP.unknownTime
			return response
		end	
	},
	['Upload'] = {
		public = true,
		handle = function(request)
			-- validate transport type support
			if not Transfer.isSupportedTransportURLType(request.message.URL) then
				return CWMP.Error.getFault('TransferUnsupportedProtocol')
			end
			
			-- validate file type
			if not Transfer.isSupportedFileType('upload', request.message.FileType) then
				return CWMP.Error.getFault('InvalidArguments', 'File type "' .. request.message.FileType .. '" not supported.')
			end
			
			-- queue transfer
			local transfer = Transfer.new('upload')
			transfer.key = request.message.CommandKey
			transfer.fileType = request.message.FileType
			transfer.url = request.message.URL
			transfer.username = nonEmptyOrNil(request.message.Username)
			transfer.password = nonEmptyOrNil(request.message.Password)
			transfer.delay = request.message.DelaySeconds
			transfer:persist()

			local response = CWMP.Message.new('UploadResponse')
			response.message.Status = 1 -- queued
			if transfer.delay == 0 then
				response.message.StartTime = os.date('%FT%TZ')
			else
				response.message.StartTime = CWMP.unknownTime
			end
			response.message.CompleteTime = CWMP.unknownTime
			return response
		end	
	},
	['ScheduleInform'] = {
		public = true,
		handle = function(request)
			local cmdKey = request.message.CommandKey
			local delaySec = request.message.DelaySeconds
			local reply = CWMP.Message.new('ScheduleInformResponse')
			if not tonumber(delaySec) or tonumber(delaySec) < 1 then
				return CWMP.Error.getFault('InvalidArguments', 'Error message for InvalidArguments') -- 9003
			end

			client.host:addSchedInform(cmdKey, delaySec)
			return reply
		end
	},
 }

CWMP.RPC = {}
CWMP.RPC._calls = implementedCalls

function CWMP.RPC.call(request)
	Logger.log('CWMP', 'info', 'RPC-RX: ' .. CWMP.Message.getShortDescription(request))
	local call = implementedCalls[request.type]
	if not call then call = implementedCalls['_UnknownMethod'] end
	local ret, reply = pcall(call.handle, request)
--	local ret, reply = true, call.handle(request)
	if ret then
		CWMP.Message.makeReply(reply, request)
	else
		Logger.log('CWMP', 'error', 'RPC: ' .. request.type .. ' handler returned error: ' .. reply)
		local fault = CWMP.Error.getFault('InternalError', reply)
		fault.message.detail.Fault.FaultString = fault.message.detail.Fault.FaultString .. ': ' .. reply
		CWMP.Message.makeReply(fault, request)
		reply = fault
	end
	Logger.log('CWMP', 'info', 'RPC-TX: ' .. CWMP.Message.getShortDescription(reply))
	client:runTasks('postRPC', reply)
	return reply
end

return CWMP.RPC
