require('Daemon')
require('Parameter.Validator')

local subROOT = conf.topRoot .. '.X_NETCOMM.WWANProfile.'
local g_depthOfInstance= 5
------------------local function prototype------------------
local convertInternalBoolean
local convertInternalInteger
local numberOfAPNList
local getAPNRDB
local setAPNRDB
local traverseRdbVariable
local numOfEnabledProfiles
------------------------------------------------------------

convertInternalBoolean = function (val)
	local inputStr = tostring(val)

	if not inputStr then return nil end

	inputStr = (string.gsub(inputStr, "^%s*(.-)%s*$", "%1"))

	if inputStr == '1' or inputStr:lower() == 'true' then
		return '1'
	elseif inputStr == '0' or inputStr:lower() == 'false' then
		return '0'
	end

	return nil
end

-- usage: convertInternalInteger{input=number, minimum=0, maximum=50}
-- If number doesn't have a specific range, just set "minimum" or "maximum" to nil or omit this argument.
-- success: return interger type value
-- false: return nil
convertInternalInteger = function (arg)
	local convertedInt = tonumber(arg.input)

	if type(convertedInt) == 'number'
	then
		local minimum = arg.minimum or -2147483648
		local maximum = arg.maximum or 2147483647

		minimum = tonumber(minimum)
		maximum = tonumber(maximum)

		if minimum == nil or maximum == nil then return nil end
		if convertedInt < minimum or convertedInt > maximum then return nil end

		return convertedInt

	else
		return nil
	end
end


local NamePairs = {['ProfileName']='name', ['DialNum']='dialstr',
		['APN']='apn', ['AuthenticationType']='auth_type',
		['UserName']='user', ['Password']='pass',
		['ReconnectDelay']='reconnect_delay', ['ReconnectRetries']='reconnect_retries',
		['InterfaceMetric']='defaultroutemetric', ['NATMasquerading']='snat',
		['AutoAPN']='autoapn', ['DefaultRoute']='defaultroute',
		['Enable']='enable', ['RoutingSetting']='routes',
		['APNStatus']='status'
		}

-- usage: traverseRdbVariable{prefix='service.firewall.dnat', suffix=, startIdx=0}
-- If startIdx is nil, then default value is 1
traverseRdbVariable = function  (arg)
	local i = arg.startIdx or 1;
	local cmdPrefix, cmdSuffix

	cmdPrefix = arg.prefix and arg.prefix .. '.' or ''
	cmdSuffix = arg.suffix and '.' .. arg.suffix or ''

	return (function ()
		local index = i
		local v = luardb.get(cmdPrefix .. index .. cmdSuffix)
		i= i + 1
		if v then
			return index, v
		end
	end)
end

numOfEnabledProfiles = function ()
	local count = 0

	for i, enabled in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
		local dev=luardb.get('link.profile.' .. i .. '.dev')
		local isWWAN = dev and dev:match('wwan\.%d+') or nil

		if isWWAN and enabled == '1' then
			count = count + 1
		end
	end
	return count
end

getCurrentDefaultRoute = function ()
	local index=1

	for i, enabled in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
		local dev=luardb.get('link.profile.' .. i .. '.dev')
		local isDefault=luardb.get('link.profile.' .. i .. '.defaultroute')
		local isWWAN = dev and dev:match('wwan\.%d+') or nil

		if isWWAN and isDefault == '1' then
			index = i
		end
	end
	return index
end

-- return: number type
numberOfAPNList = function ()
	local count = 0

	for i, value in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
		local dev=luardb.get('link.profile.' .. i .. '.dev')
		local isWWAN = dev and dev:match('wwan\.%d+') or nil

		if isWWAN then
			count = count + 1
		end
	end

	return count
end

-- usage: getAPNRDB{GetName='enable', GetIdx=1}
getAPNRDB = function (arg)
	local name = arg.GetName
	local idx = arg.GetIdx

	if not name or not idx then return nil end

	return luardb.get('link.profile.' .. idx .. '.' .. name)
end

-- usage: setAPNRDB{SetName='enable', SetIdx=1, SetValue=0}
setAPNRDB = function (arg)
	local name = arg.SetName
	local idx = arg.SetIdx
-- 
	if not name or not idx then return nil end
	if not arg.SetValue then return nil end

	luardb.set('link.profile.' .. idx .. '.' .. name, arg.SetValue)
	return 0
end


return {
	[subROOT .. 'APNLists'] = {
		init = function(node, name, value)
			local maxInstanceId = numberOfAPNList()

			if maxInstanceId == nil or maxInstanceId == 0 then return 0 end

			for id=1, maxInstanceId do
				local instance = node:createDefaultChild(id)
			end
			node.instance = maxInstanceId
			return 0
		end,
	},
	[subROOT .. 'APNLists.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfInstance = #pathBits
			return 0
		end,
	},
	[subROOT .. 'APNLists.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[g_depthOfInstance])
			local instName = pathBits[g_depthOfInstance+1]
			local rdbName = NamePairs[instName]

			if not instName or not rdbName then error('Dunno how to handle ' .. name) end

			local retVal = getAPNRDB{GetName=rdbName, GetIdx=idx}

			if not retVal then return CWMP.Error.InternalError, 'Unable to read APN List: ' .. rdbName or "NULL" end

			return 0, retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[g_depthOfInstance])
			local instName = pathBits[g_depthOfInstance+1]
			local rdbName = NamePairs[instName]
			local addresses

			if not instName or not value then return CWMP.Error.InternalError end
			if not rdbName then error('Dunno how to handle ' .. name) end

			if instName == 'AuthenticationType' then
				value = value:lower()
				if value ~= 'chap' and value ~= 'pap' then value = nil end
			elseif instName == 'ReconnectDelay' then -- range: 30~65535
				value = convertInternalInteger{input=value, minimum=30, maximum=65535}
			elseif instName == 'ReconnectRetries' then -- range: 0~65535, 0=Unlimited
				value = convertInternalInteger{input=value, minimum=0, maximum=65535}
			elseif instName == 'InterfaceMetric' then -- range: 0~65535
				value = convertInternalInteger{input=value, minimum=0, maximum=65535}
			elseif instName == 'NATMasquerading' or instName == 'Enable' or instName == 'AutoAPN' or instName == 'DefaultRoute' then -- boolean
				value = convertInternalBoolean(value)
			elseif instName == 'RoutingSetting' and value ~= '/' then
				addresses = value:explode('/')
				if not Parameter.Validator.isValidIP4(addresses[1]) then value = nil end
				if not Parameter.Validator.isValidIP4Netmask(addresses[2]) then value = nil end
			end

			if not value then return CWMP.Error.InvalidParameterValue end

			local curValue = getAPNRDB{GetName=rdbName, GetIdx=idx}

			if curValue ~= nil and tostring(value) == curValue then return 0 end --> success

			if instName == 'Enable' then
				if value == '1' then
					local max_enabled_profiles=luardb.get('wwan.0.max_sub_if')

					if not max_enabled_profiles then max_enabled_profiles = 2 end
					max_enabled_profiles = tonumber(max_enabled_profiles)
					if numOfEnabledProfiles() >= max_enabled_profiles then
						return CWMP.Error.InvalidParameterValue, 'The maximum number of enabled profiles has been exceeded.'
					end
				end
			end

			if instName == 'DefaultRoute' then
				if value == '1' then
					local currDefaultIdx = getCurrentDefaultRoute()
					if not currDefaultIdx then currDefaultIdx = 1 end
					currDefaultIdx = tonumber(currDefaultIdx)
					if currDefaultIdx ~= idx then
						setAPNRDB{SetName=rdbName, SetIdx=currDefaultIdx, SetValue='0'}
					end
				else
					return CWMP.Error.InvalidParameterValue, 'This parameter does not support 0|Disable|False'
				end
			end

			setAPNRDB{SetName=rdbName, SetIdx=idx, SetValue=value}
			return 0
		end
	},
}
