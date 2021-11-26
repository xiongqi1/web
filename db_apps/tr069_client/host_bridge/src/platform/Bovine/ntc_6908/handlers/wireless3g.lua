

--------------------------[start]:WirelessModem.Status--------------------------
function hex2ascii(value)
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

function zero_pending(length)
	local retVal = ""

	for i=1, length do
		retVal=retVal.."0"
	end

	return retVal
end
--------------------------[ end ]:WirelessModem.Status--------------------------

--------------------------[start]:WWANProfile--------------------------
NamePairs = {['ProfileName']='name', ['DialNum']='dialstr',
		['APN']='apn', ['AuthenticationType']='auth_type',
		['UserName']='user', ['Password']='pass',
		['ReconnectDelay']='reconnect_delay', ['ReconnectRetries']='reconnect_retries',
		['InterfaceMetric']='defaultroutemetric', ['NATMasquerading']='snat'}

-- usage: traverseRdbVariable{prefix='service.firewall.dnat', suffix=, startIdx=0}
-- If startIdx is nil, then default value is 1
local function traverseRdbVariable (arg)
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

local function activatedAPNIdx ()
	local activatedIndex = 0
	local indexingError = 0
	for i, value in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
		if value == '1' then
			activatedIndex = i
			indexingError = indexingError + 1
		end
	end

	if indexingError > 1 then
		dimclient.log('error', 'More then 1 APN list Activated')
		return nil
	end

	return activatedIndex
end

-- return: number type
local function numberOfAPNList()
	local count = 0

	for i, value in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
		count = count + 1
	end

	return count
end

-- usage: getAPNRDB{GetName='enable', GetIdx=1}
local function getAPNRDB(arg)
	local name = arg.GetName
	local idx = arg.GetIdx

	if not name or not idx then return nil end

	return luardb.get('link.profile.' .. idx .. '.' .. name)
end

function clear_savedAPNTbl()
	savedAPN = nil
end

function buildNsave_savedAPN()

	if savedAPN then return end

	local activatedIdx = activatedAPNIdx()
	if not activatedIdx then return end

	local isStatusUp = getAPNRDB{GetName='status', GetIdx=activatedIdx}

	if isStatusUp == 'up' then
		savedAPN = {}
		for key, value in pairs(NamePairs) do
			savedAPN[value] = getAPNRDB{GetName=value, GetIdx=activatedIdx}
		end
		savedAPN["Index"] = activatedIdx
		dimclient.callbacks.register('cleanup', clear_savedAPNTbl)
	end

	local apnlist_file = '/tmp/TR069savedAPNList'
	table.save(savedAPN, apnlist_file)
end

changedActiveAPN = 0

function callback_forActiveAPN ()
	if changedActiveAPN == 0 then return 0 end

	local activatedIdx = activatedAPNIdx()
	if not activatedIdx then return nil end

	luardb.set('link.profile.' .. activatedIdx .. '.enable', '0')

	os.execute('sleep 1')

	luardb.set('link.profile.' .. changedActiveAPN .. '.enable', '1')
	luardb.set('tr069.apn.validaty.trigger', changedActiveAPN)

	changedActiveAPN = 0
end

function setRdbEnableViaActivatedAPN (idx, value)
	if not idx or not value then return nil end
	if not convertInternalInteger{input=idx, minimum=1, maximum=6} then return nil end
	if not convertInternalBoolean(value) then return nil end

	if tostring(value) == '0' then return 0 end

	changedActiveAPN = tonumber(idx) or 0

	dimclient.callbacks.register('postSession', callback_forActiveAPN)

	return 0
end

function setRdbEnableViaOthers (idx, value)
	if not idx or not value then return nil end
	if not convertInternalInteger{input=idx, minimum=1, maximum=6} then return nil end
	if not convertInternalBoolean(value) then return nil end

	if tostring(value) == '0' then return 0 end
	if tostring(changedActiveAPN) ~= '0' then return 0 end

	changedActiveAPN = tonumber(idx) or 0

	dimclient.callbacks.register('postSession', callback_forActiveAPN)

	return 0
end
-- usage: setAPNRDB{SetName='enable', SetIdx=1, SetValue=0, From='ActivatedAPN'|'Others'}
local function setAPNRDB(arg)
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

	['**.X_NETCOMM.WirelessModem.Status.NetworkProvider'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local provider = luardb.get('wwan.0.system_network_status.network')

			if not provider then return '' end

			provider = string.gsub(provider:gsub('^%s+', ''), '%s+$', '')
			if provider:find("^%%") then
				local encodedVal = provider:gsub('%%', ' ')  --> from % to space
				provider = hex2ascii(encodedVal)
			end

			return provider
		end,
		set = function(node, name, value)
			return 0
		end
	},

	['**.X_NETCOMM.WirelessModem.Status.PDPStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pdpStatus = luardb.get('wwan.0.system_network_status.pdp0_stat')

			if not pdpStatus then return '' end

			if pdpStatus == '1' or pdpStatus.lower() == "up" then
				return 'up'
			elseif pdpStatus == '0' or pdpStatus.lower() == "down" then
				return 'down'
			end

			return ''
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

			if lac == nil or cellID == nil then return retVal end
			if lac == "" or cellID == "" then return retVal end

			lac =  zero_pending(4-#lac)..lac
			cellID = zero_pending(8-#cellID)..cellID
			retVal = string.format("%s %s", lac, cellID)

			return retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

	['**.X_NETCOMM.WirelessModem.Status.MCC_MNC'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = ''
			local mcc = luardb.get('wwan.0.imsi.plmn_mcc')
			local mnc = luardb.get('wwan.0.imsi.plmn_mnc')

			if mcc == nil or mnc == nil then return retVal end
			if mcc == "" or mnc == "" then return retVal end

			retVal = string.format("%d %02d",mcc, mnc)

			return retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

	['**.X_NETCOMM.WirelessModem.Status.ConnectionUpTime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = ''
			local sysuptime = readIntFromFile('/proc/uptime')
			local networkuptime = luardb.get('wwan.0.conn_up_time_point')

			if not sysuptime or not networkuptime then return retVal end

			networkuptime = math.floor(networkuptime)
			if not networkuptime then return retVal end

			retVal = tonumber(sysuptime) - tonumber(networkuptime)
			return tostring(retVal)
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
			if not index then return cwmpError.InternalError end
			return tostring(index)
		end,
		set = function(node, name, value)
			local count = numberOfAPNList()

			if not tonumber(count) then return cwmpError.InternalError end

			value = convertInternalInteger{input=value, minimum=1, maximum=count} --> we don't need index 0, which disables all of APN
			if value == nil then return cwmpError.InvalidParameterValue end

			local activatedIdx = activatedAPNIdx()
			if not activatedIdx then return cwmpError.InternalError end
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
		create = function(node, name, instanceId)
			local instance = node:createDefaultChild(instanceId)
			return 0
		end
	},
	['**.X_NETCOMM.WWANProfile.APNLists.*'] = {
		create = function(node, name, instanceId)
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			node.parent:deleteChild(node)
			return 0
		end
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

			if not retVal then return cwmpError.InternalError end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local instName = pathBits[6]
			local rdbName = NamePairs[instName]

			if not instName or not value then return cwmpError.InternalError end
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

			if not value then return cwmpError.InvalidParameterValue end

			local curValue = getAPNRDB{GetName=rdbName, GetIdx=idx}
			if not curValue and tostring(value) == curValue then return 0 end --> success

			if not setAPNRDB{SetName=rdbName, SetIdx=idx, SetValue=value, From='Others'} then
				return cwmpError.InvalidParameterValue
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
