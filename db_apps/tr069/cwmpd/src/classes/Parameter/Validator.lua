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
require('Parameter')
require('tableutil')
require('Int64')

Parameter.Validator = {}

local nodeValidators = {
	----
	-- Types
	----
	['string'] = function(node, value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)
		if max and string.len(value) > max then
			return CWMP.Error.InvalidParameterValue, 'Parameter length may not exceed ' .. max .. '.'
		end
		if min and string.len(value) < min then
			return CWMP.Error.InvalidParameterValue, 'Parameter length must be at least ' .. min .. '.'
		end
	end,
	['int'] = function(node, value)
		if string.len(value) ==0 then
			return CWMP.Error.UninitializedParameterValue, 'Parameter is not initialized'
		end
		local num = tonumber(value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)

		if not num then
			return CWMP.Error.InvalidParameterType, 'Parameter is an integer, got a non-number "' .. tostring(value) .. '".'
		end
		if max and num > max then
			return CWMP.Error.InvalidParameterValue, 'Parameter may not exceed ' .. max .. '.'
		end
		if min and num < min then
			return CWMP.Error.InvalidParameterValue, 'Parameter may not be less than ' .. min .. '.'
		end
	end,
	['uint'] = function(node, value)
		if string.len(value) ==0 then
			return CWMP.Error.UninitializedParameterValue, 'Parameter is not initialized'
		end
		local num = tonumber(value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)

		if not num then
			return CWMP.Error.InvalidParameterType, 'Parameter is an unsigned integer, got a non-number "' .. tostring(value) .. '".'
		end
		if num < 0 then
			return CWMP.Error.InvalidParameterType, 'Parameter is an unsigned integer, got a negative number.'
		end
		if max and num > max then
			return CWMP.Error.InvalidParameterValue, 'Parameter may not exceed ' .. max .. '.'
		end
		if min and num < min then
			return CWMP.Error.InvalidParameterValue, 'Parameter may not be less than ' .. min .. '.'
		end
	end,
	['long'] = function(node, value)
		if string.len(value) ==0 then
			return CWMP.Error.UninitializedParameterValue, 'Parameter is not initialized'
		end
		local num = tobignumber(value)
		local max = tobignumber(node.max)
		local min = tobignumber(node.min)

		if not num then
			return CWMP.Error.InvalidParameterType, 'Parameter is an long integer, got a non-number "' .. tostring(value) .. '".'
		end
		if max and num > max then
			return CWMP.Error.InvalidParameterValue, 'Parameter may not exceed ' .. max .. '.'
		end
		if min and num < min then
			return CWMP.Error.InvalidParameterValue, 'Parameter may not be less than ' .. min .. '.'
		end
	end,
	['ulong'] = function(node, value)
		if string.len(value) ==0 then
			return CWMP.Error.UninitializedParameterValue, 'Parameter is not initialized'
		end
		local num = tobignumber(value)
		local max = tobignumber(node.max)
		local min = tobignumber(node.min)

		if not num then
			return CWMP.Error.InvalidParameterType, 'Parameter is an unsigned long integer, got a non-number "' .. tostring(value) .. '".'
		end
		if num < 0 then
			return CWMP.Error.InvalidParameterType, 'Parameter is an unsigned long integer, got a negative number.'
		end
		if max and num > max then
			return CWMP.Error.InvalidParameterValue, 'Parameter may not exceed ' .. max .. '.'
		end
		if min and num < min then
			return CWMP.Error.InvalidParameterValue, 'Parameter may not be less than ' .. min .. '.'
		end
	end,
	['bool'] = function(node, value)
		if string.len(value) ==0 then
			return CWMP.Error.UninitializedParameterValue, 'Parameter is not initialized'
		end

		local validValues = { '0', '1' }
		if not table.contains(validValues, value) then
			return CWMP.Error.InvalidParameterType, 'Parameter is an boolean, got a non-boolean "' .. tostring(value) .. '".'
		end
	end,
	['datetime'] = function(node, value)
		-- FIXME: implement something here!
	end,
	['base64'] = function(node, value)
		if string.len(value) ==0 then
			return CWMP.Error.UninitializedParameterValue, 'Parameter is not initialized'
		end
		local max = tonumber(node.max)
		local min = tonumber(node.min)
		if max and string.len(value) > max then
			return CWMP.Error.InvalidParameterValue, 'Parameter length may not exceed ' .. max .. '.'
		end
		if min and string.len(value) < min then
			return CWMP.Error.InvalidParameterValue, 'Parameter length must be at least ' .. min .. '.'
		end
	end,

	----
	-- Custom Validators
	----
	['IPv4'] = function(node, value)
		local min = tonumber(node.min)
		if min and min == 0 and value == '' then return end
		if not Parameter.Validator.isValidIP4(value) then
			return CWMP.Error.InvalidParameterValue, ('Value "' .. value .. '" is not a valid IPv4 address.')
		end
	end,
	['IPv4Mask'] = function(node, value)
		local min = tonumber(node.min)
		if min and min == 0 and value == '' then return end
		if not Parameter.Validator.isValidIP4Netmask(value) then
			return CWMP.Error.InvalidParameterValue, ('Value "' .. value .. '" is not a valid IPv4 netmask.')
		end
	end,
	['URL'] = function(node, value)
		local max = tonumber(node.max)
		local min = tonumber(node.min)
		if max and string.len(value) > max then
			return CWMP.Error.InvalidParameterValue, 'Parameter length may not exceed ' .. max .. '.'
		end
		if min and string.len(value) < min then
			return CWMP.Error.InvalidParameterValue, 'Parameter length must be at least ' .. min .. '.'
		end
		if not Parameter.Validator.isValidURL(value) then
			return CWMP.Error.InvalidParameterValue, ('"' .. value .. '" is not a valid URL.')
		end
	end,
}

function Parameter.Validator.validateNodeValue(node, value)
	local validatorType = node.type
	if node.validator and node.validator ~= '' then validatorType = node.validator end
	local validator = nodeValidators[validatorType]
	if not validator then error('No validator of type "' .. validatorType .. '".') end
	if type(validator) ~= 'function' then error('Validator of type "' .. validatorType .. '" is not a function!') end
	return validator(node, value)
end


function Parameter.Validator.isValidIP4(ip)
	local ret, _, o1, o2, o3, o4 = string.find(ip, '^(%d+)%.(%d+)%.(%d+)%.(%d+)$')
	if ret == nil then return false end
        local octet = { o1, o2, o3, o4 }
	for i = 1,4 do
		if tonumber(octet[i]) > 255 then return false end
        end
	return true
end

local validNetmaskOctets = { 255, 254, 252, 248, 240, 224, 192, 128, 0 }

function Parameter.Validator.isValidIP4Netmask(mask)
	local ret, _, o1, o2, o3, o4 = string.find(mask, '^(%d+)%.(%d+)%.(%d+)%.(%d+)$')
	if ret == nil then return false end
        local octet = { o1, o2, o3, o4 }
	local transition = false
	for i = 1,4 do
		if not table.contains(validNetmaskOctets, tonumber(octet[i])) then
			return false
		end
		if tonumber(octet[i]) < 255 and not transition then
			transition = true
		elseif transition and tonumber(octet[i]) ~= 0 then
			return false
		end
        end
	return true
end

function Parameter.Validator.isValidURL(url)
	-- FIXME: add some actual URL validation
	return true
end


return Parameter.Validator
