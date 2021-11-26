
require('Daemon')
require('Logger')
Logger.addSubsystem('wireless3g')

------------------local function prototype------------------
local convertInternalBoolean
local convertInternalInteger
local hex2ascii
local zero_pending
local is_valid_model_name
local traverseRdbVariable
local activatedAPNIdx
local numberOfAPNList
local getAPNRDB
local clear_savedAPNTbl
local buildNsave_savedAPN
local callback_forActiveAPN
local setRdbEnableViaActivatedAPN
local setRdbEnableViaOthers
local setAPNRDB
local setAutoAPNRDB
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

--------------------------[start]:WirelessModem.Status--------------------------
hex2ascii = function (value)
	local retVal = ''
	value = string.gsub(value:gsub('^%s+', ''), '%s+$', '')
	local temp = value:explode(' ')
	for i, value in ipairs(temp) do
		local asciiCode=tonumber(value, 16)
		local asciiVal=string.char(asciiCode)
			retVal = retVal..asciiVal
	end
	return retVal
end

zero_pending = function (length)
	local retVal = ""

	for i=1, length do
		retVal=retVal.."0"
	end

	return retVal
end

is_valid_model_name = function (name)
	if not name or #name < 2 then return false end
	if name == "N/A" or not name:match("^%a") then return false end

	return true
end
--------------------------[ end ]:WirelessModem.Status--------------------------

--------------------------[start]:WWANProfile--------------------------
local NamePairs = {['ProfileName']='name', ['DialNum']='dialstr',
		['APN']='apn', ['AuthenticationType']='auth_type',
		['UserName']='user', ['Password']='pass',
		['ReconnectDelay']='reconnect_delay', ['ReconnectRetries']='reconnect_retries',
		['InterfaceMetric']='defaultroutemetric', ['NATMasquerading']='snat'}

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

-- Success: number typed index
--	    0 -> every apn list is deactivated
-- Failure: nil

activatedAPNIdx = function ()
	local activatedIndex = 0
	local indexingError = 0
	for i, value in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
		local dev=luardb.get('link.profile.' .. i .. '.dev')
		local isWWAN = dev and dev:match('wwan\.%d+') or nil

		if value == '1' and isWWAN then
			activatedIndex = i
			indexingError = indexingError + 1
		end
	end

	if indexingError > 1 then
		Logger.log('wireless3g', 'error', 'More then 1 APN list Activated')
		return nil
	end

	return activatedIndex
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

clear_savedAPNTbl = function ()
	savedAPN = nil
end

buildNsave_savedAPN = function ()

	if savedAPN then return end

	local activatedIdx = activatedAPNIdx()
	if not activatedIdx then return end

	local isStatusUp = getAPNRDB{GetName='status', GetIdx=activatedIdx}

	if isStatusUp == 'up' then
		savedAPN = {}
		for key, value in pairs(NamePairs) do
			savedAPN[value] = getAPNRDB{GetName=value, GetIdx=activatedIdx}
		end
		savedAPN["Index"] = activatedIdx -- number type

		local autoAPN = luardb.get('webinterface.autoapn')
		if not autoAPN or autoAPN ~= '1' then
			autoAPN = 0
		else
			autoAPN = 1
		end
		savedAPN["AutoAPN"] = autoAPN -- number type

		if client:isTaskQueued('cleanUp', clear_savedAPNTbl) ~= true then
			client:addTask('cleanUp', clear_savedAPNTbl)
		end
	end

	local apnlist_file = '/tmp/TR069savedAPNList'
	table.save(savedAPN, apnlist_file)
end

local changedActiveAPN = 0
local changedAutoAPN = 0

callback_forActiveAPN = function ()
	if changedActiveAPN == 0 and changedAutoAPN == 0 then return 0 end

	local activatedIdx = activatedAPNIdx()
	if not activatedIdx then return nil end

	if changedActiveAPN >= 1 and changedActiveAPN <= 6 then
		luardb.set('webinterface.profileIdx' , changedActiveAPN-1)
		luardb.set('link.profile.profilenum' , changedActiveAPN)
	end

	if changedAutoAPN ~= 0 or savedAPN["AutoAPN"] ~= 1 then
		luardb.set('link.profile.' .. activatedIdx .. '.enable', '0')

		os.execute('sleep 1')

		if changedActiveAPN == 0 then
			luardb.set('link.profile.' .. activatedIdx .. '.enable', '1')
		else
			luardb.set('link.profile.' .. changedActiveAPN .. '.enable', '1')
		end
	end

	if (changedAutoAPN == 0 and savedAPN["AutoAPN"] == 0) or (changedAutoAPN == 1 and savedAPN["AutoAPN"] == 1) then
		luardb.set('tr069.apn.validaty.trigger', changedActiveAPN)
	end

	changedActiveAPN = 0
	changedAutoAPN = 0
end

setRdbEnableViaActivatedAPN = function (idx, value)
	if not idx or not value then return nil end
	if not convertInternalInteger{input=idx, minimum=1, maximum=6} then return nil end
	if not convertInternalBoolean(value) then return nil end

	if tostring(value) == '0' then return 0 end

	changedActiveAPN = tonumber(idx) or 0
	if client:isTaskQueued('postSession', callback_forActiveAPN) ~= true then
		client:addTask('postSession', callback_forActiveAPN)
	end

	return 0
end

setRdbEnableViaOthers = function (idx, value)
	if not idx or not value then return nil end
	if not convertInternalInteger{input=idx, minimum=1, maximum=6} then return nil end
	if not convertInternalBoolean(value) then return nil end

	if tostring(value) == '0' then return 0 end
	if tostring(changedActiveAPN) ~= '0' then return 0 end

	changedActiveAPN = tonumber(idx) or 0

	if client:isTaskQueued('postSession', callback_forActiveAPN) ~= true then
		client:addTask('postSession', callback_forActiveAPN)
	end

	return 0
end
-- usage: setAPNRDB{SetName='enable', SetIdx=1, SetValue=0, From='ActivatedAPN'|'Others'}
setAPNRDB = function (arg)
	local name = arg.SetName
	local idx = arg.SetIdx
	local from = arg.From or 'Others'

	if not name or not idx then return nil end
	if not arg.SetValue then return nil end

	if name ~= 'enable' then
		local currentVal = getAPNRDB{GetName=name, GetIdx=idx}

		if not currentVal then return nil end
		if currentVal == tostring(arg.SetValue) then return 0 end
	end
	if not savedAPN then
		local activatedIdx = activatedAPNIdx()
		if activatedIdx == idx then
			buildNsave_savedAPN()
		end
	end

	if name == 'enable' then
		if from == 'ActivatedAPN' then
			setRdbEnableViaActivatedAPN (idx, arg.SetValue)
		elseif from == 'Others' then
			setRdbEnableViaOthers (idx, arg.SetValue)
		else
			return nil
		end
		return 0
	end

	luardb.set('link.profile.' .. idx .. '.' .. name, arg.SetValue)
	return 0
end

setAutoAPNRDB = function (setValue)
	if not setValue then return nil end
	if setValue ~= '1' and setValue ~= '0' then return nil end

	changedAutoAPN =1

	local activatedAPN = activatedAPNIdx()
	setAPNRDB{SetName='enable', SetIdx=activatedAPN, SetValue='1', From='Others'}

	luardb.set('webinterface.autoapn', setValue)
	return 0
end
--------------------------[ end ]:WWANProfile--------------------------

return {
--------------------------[start]:WirelessModem.Status--------------------------
--[[
-- TODO
	['**.X_NETCOMM.WirelessModem.Status.'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0
		end,
		set = function(node, name, value)
			return 0
		end
	},
--]]

	['**.X_NETCOMM.WirelessModem.Status.Model'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)

			local nameArray = {}
			nameArray[1] = luardb.get("wwan.0.model") or ''		-- result from AT command (AT+CGMM)
			nameArray[2] = luardb.get("wwan.0.module_name") or ''		-- vendor-device-id-based hard-coded name
			nameArray[3] = luardb.get("wwan.0.product_udev") or ''		-- USB configuraiton - product
			nameArray[4] = luardb.get("webinterface.module_model") or ''	-- hard-coded name from rdb configuration

			for _,v in ipairs(nameArray) do
				if is_valid_model_name(v) then
					return 0, v
				end
			end

			return 0, "N/A"
		end,
		set = function(node, name, value)
			return 0
		end
	},

	['**.X_NETCOMM.WirelessModem.Status.NetworkProvider'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local provider = luardb.get('wwan.0.system_network_status.network')

			if not provider then return 0, '' end

			provider = string.gsub(provider:gsub('^%s+', ''), '%s+$', '')
			if provider:find("^%%") then
				local encodedVal = provider:gsub('%%', ' ')  --> from % to space
				provider = hex2ascii(encodedVal)
			end

			return 0, provider
		end,
		set = function(node, name, value)
			return 0
		end
	},

	['**.X_NETCOMM.WirelessModem.Status.PDPStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pdpStatus = luardb.get('wwan.0.system_network_status.pdp0_stat')

			if not pdpStatus then return 0, '' end

			if pdpStatus == '1' or pdpStatus:lower() == "up" then
				return 0, 'up'
			elseif pdpStatus == '0' or pdpStatus:lower() == "down" then
				return 0, 'down'
			end

			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},

	['**.X_NETCOMM.WirelessModem.Status.LAC_CellID'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = ''
			local mnc = luardb.get('wwan.0.imsi.plmn_mnc')
			local lac = luardb.get('wwan.0.system_network_status.LAC')
			local cellID = luardb.get('wwan.0.system_network_status.CellID')

			if lac == nil or cellID == nil then return 0, retVal end
			if lac == "" or cellID == "" then return 0, retVal end

			lac =  zero_pending(4-#lac)..lac
			cellID = zero_pending(8-#cellID)..cellID
			retVal = string.format("%s %s", lac, cellID)

			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

	['**.X_NETCOMM.WirelessModem.Status.MCC_MNC'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = ''
			local mcc = luardb.get('wwan.0.system_network_status.MCC')
			local mnc = luardb.get('wwan.0.system_network_status.MNC')

			if mcc == nil or mnc == nil then return 0, retVal end
			if mcc == "" or mnc == "" then return 0, retVal end

			retVal = string.format("%d %02d",mcc, mnc)

			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

	['**.X_NETCOMM.WirelessModem.Status.ConnectionUpTime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = '0'
			local sysuptime = Daemon.readIntFromFile('/proc/uptime')
			local networkuptime = luardb.get('wwan.0.conn_up_time_point')

			if not sysuptime or not networkuptime or networkuptime == "0" then return 0, retVal end

			networkuptime = math.floor(networkuptime)
			if not networkuptime then return 0, retVal end

			retVal = tonumber(sysuptime) - tonumber(networkuptime)
			return 0, tostring(retVal)
		end,
		set = function(node, name, value)
			return 0
		end
	},
--------------------------[ end ]:WirelessModem.Status--------------------------

--------------------------[start]:WWANProfile--------------------------
-- TODO -->  test and set function
	['**.X_NETCOMM.WWANProfile.ActivatedAPN'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local index = activatedAPNIdx()
			if not index then return 0, '0' end	
			return 0, tostring(index)
		end,
		set = function(node, name, value)
			local count = numberOfAPNList()

			if not tonumber(count) then return CWMP.Error.InternalError end

			value = convertInternalInteger{input=value, minimum=1, maximum=count} --> we don't need index 0, which disables all of APN
			if value == nil then return CWMP.Error.InvalidParameterValue end

			local activatedIdx = activatedAPNIdx()
			if not activatedIdx then return CWMP.Error.InternalError end
			if activatedIdx == value then return 0 end

			if activatedIdx ~= 0 then
				setAPNRDB{SetName='enable', SetIdx=activatedIdx, SetValue=0, From='ActivatedAPN'}
			end

			if value ~= 0 then
				setAPNRDB{SetName='enable', SetIdx=value, SetValue=1, From='ActivatedAPN'}
			end
			return 0
		end
	},
	['**.X_NETCOMM.WWANProfile.AutoAPN'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local index = luardb.get('webinterface.autoapn')
			if not index or index ~= '1' then index= '0' end	
			return 0, index
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidParameterValue end
			value = convertInternalBoolean(value)
			if not value then return CWMP.Error.InvalidParameterValue end

			local currVal = luardb.get('webinterface.autoapn')
			if value ~= currVal then
				setAutoAPNRDB(value)
			end
			return 0
		end
	},
	['**.X_NETCOMM.WWANProfile.APNLists'] = {
		init = function(node, name, value)
			local maxInstanceId = numberOfAPNList()

			if maxInstanceId == nil or maxInstanceId == 0 then return 0 end

			for id=1, maxInstanceId do
				local instance = node:createDefaultChild(id)
			end
			node.instance = maxInstanceId
			return 0
		end,
-- 		create = function(node, name, instanceId)
-- 			local instance = node:createDefaultChild(instanceId)
-- 			return 0
-- 		end
	},
	['**.X_NETCOMM.WWANProfile.APNLists.*'] = {
-- 		create = function(node, name, instanceId)
-- 			local instance = node:createDefaultChild(instanceId)
-- 			return 0
-- 		end,
-- 		delete = function(node, name)
-- 			node.parent:deleteChild(node)
-- 			return 0
-- 		end
	},

-- TODO --> test and set function
	['**.X_NETCOMM.WWANProfile.APNLists.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local instName = pathBits[6]
			local rdbName = NamePairs[instName]

			if not instName or not rdbName then error('Dunno how to handle ' .. name) end

			local retVal = getAPNRDB{GetName=rdbName, GetIdx=idx}

			if not retVal then return CWMP.Error.InternalError, 'Unable to read APN List: ' .. rdbName or "NULL" end

			return 0, retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local instName = pathBits[6]
			local rdbName = NamePairs[instName]

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
			elseif instName == 'NATMasquerading' then -- boolean
				value = convertInternalBoolean(value)
			end

			if not value then return CWMP.Error.InvalidParameterValue end

			local curValue = getAPNRDB{GetName=rdbName, GetIdx=idx}
			if not curValue and tostring(value) == curValue then return 0 end --> success

			if not setAPNRDB{SetName=rdbName, SetIdx=idx, SetValue=value, From='Others'} then
				return CWMP.Error.InvalidParameterValue
			else
				local activatedAPN = activatedAPNIdx()
				if idx == activatedAPN
					and (instName == 'DialNum' or instName == 'APN' or instName == 'AuthenticationType' or instName == 'UserName' 
					or instName == 'Password' or instName == 'InterfaceMetric' or instName == 'NATMasquerading') then
					setAPNRDB{SetName='enable', SetIdx=idx, SetValue='1', From='Others'}
				end
				return 0
			end
		end
	},

--------------------------[ end ]:WWANProfile--------------------------
}
