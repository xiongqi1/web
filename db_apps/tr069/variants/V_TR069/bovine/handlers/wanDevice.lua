
require('Daemon')
require('Parameter.Validator')
require('Logger')
Logger.addSubsystem('wanDevice.lua')

local g_depthOf_X_NETCOMM_PortMapping=8
------------------local function prototype------------------
local convertInternalBoolean
local convertInternalInteger
local wwan_ifname
local readIntFromFileToString
local readStrFromFileWithErrorCheck
local readStatistics
local traverseProfile
local getEnabledProfile
local traverseRdbVariable
local build_rdbValue
local addRDBPortMapping
local updateRDBPortMapping
local delRDBPortMapping
local createDefaultPersistObject
local deletePortMapObject
local getMD5sumforPM
local sort_func
local sort_PortMapping
local build_objectList
local portMappingWatcher
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

------------------------<<Start: WANCommonInterfaceConfig >>------------------------
wwan_ifname = function ()
	return luardb.get('wwan.0.netif_udev') or ''
end

readIntFromFileToString = function (filename)
	local file = io.open(filename, 'r')
	if not file then return nil end

	local n = file:read('*n')
	if not n then return nil end

	n = math.floor(n)
	file:close()
	return tostring(n)
end

readStrFromFileWithErrorCheck = function (filename)
	local file = io.open(filename, 'r')
	if not file then return nil end

	local line = file:read('*l')
	file:close()
	return line
end

readStatistics = function (name)
	local filename = '/sys/class/net/' .. wwan_ifname() .. '/statistics/' .. name

	local retVal = readIntFromFileToString(filename)

	if not retVal then return '0' else return retVal end
end
------------------------<< End : WANCommonInterfaceConfig >>------------------------

------------------------<<Start: WANIPConnection >>------------------------
traverseProfile = function  (name)
	local i = 0;
	return (function () 
		i= i + 1
		local v = luardb.get('link.profile.' .. i .. '.' .. name)
		if v then 
			return i, v
		end
	end)
end

-- return value:  index number of enabled Profile with number type or nil
getEnabledProfile = function ()
	local index = nil

	for i, v in traverseProfile('enable') do
		local dev=luardb.get('link.profile.' .. i .. '.dev')
		local isWWAN = dev and dev:match('wwan\.%d+') or nil

		if v == '1' and isWWAN then index = i; break end
	end

	return index
end

------------------------<< End : WANIPConnection >>------------------------


------------------------<<Start: PortMapping >>------------------------
--[[ Lua Console script for taking all class objects out
require('rdbobject')

do
local class = rdbobject.getClass('tr069.wan.PortMapping')
routeRules = class:getAll()
	for _, rule in ipairs(routeRules) do
		if rule then
			class:delete(rule)
		end
	end
end
--]]

local maxNumOfInstance = 32

local config = {
	persist = true,
	idSelection = 'smallestUnused',
}

local class = rdbobject.getClass('tr069.wan.PortMapping', config)

-- usage: traverseRdbVariable{prefix='service.firewall.dnat', suffix=, startIdx=0}
-- If startIdx is nil, then default value is 1
traverseRdbVariable = function (arg)
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

build_rdbValue = function (rule)
	local protocol, sourIP, sourPortS, sourPortE, destIp, destPortS
		= rule.PortMappingProtocol, rule.RemoteHost, rule.ExternalPort, rule.ExternalPortEndRange, rule.InternalClient, rule.InternalPort

	if not protocol or not sourIP or not sourPortS or not sourPortE or not destIp or not destPortS then return nil end
	if protocol ~= 'tcp' and protocol ~= 'udp' and protocol ~= 'all' then return nil end
	if sourIP ~= '' and not Parameter.Validator.isValidIP4(sourIP) then return nil end
	if not tonumber(sourPortS) or not tonumber(sourPortE) then return nil end
	if not Parameter.Validator.isValidIP4(destIp) then return nil end
	if not tonumber(destPortS) or destPortS == '0' then return nil end

	if sourPortE == '0' then sourPortE = sourPortS end

	local diff = tonumber(sourPortE) - tonumber(sourPortS)
	local destPortE =  tonumber(destPortS) + diff

	local rdbValue = ''

	if sourIP == '' then
		rdbValue = '\"-p ' .. protocol .. ' --dport ' .. sourPortS .. ':' .. sourPortE .. ' -i [wanport] -j DNAT --to-destination ' .. destIp .. ':' .. destPortS .. '-' .. destPortE .. ' \"'
	else
		rdbValue = '\"-p ' .. protocol .. ' -s ' .. sourIP .. ' --dport ' .. sourPortS .. ':' .. sourPortE .. ' -i [wanport] -j DNAT --to-destination ' .. destIp .. ':' .. destPortS .. '-' .. destPortE .. ' \"'
	end

	Logger.log('wanDevice.lua', 'debug', 'portMapping rdb value = [' .. rdbValue .. ']')

	return rdbValue

end

addRDBPortMapping = function (rule)
	if not rule then return false end

	if rule.rdbId ~= '-1' then return false end

	local rdbValue = build_rdbValue(rule)

	if not rdbValue then return false end

	local lastIdx = 0
	for i, value in traverseRdbVariable{prefix='service.firewall.dnat', startIdx=0} do
		if value == '' then
			lastIdx = i
			break
		end
	end

	rule.rdbId = lastIdx
	luardb.set('service.firewall.dnat.' .. lastIdx, rdbValue)
	luardb.set('service.firewall.dnat.' .. (lastIdx + 1), '')

	needTriggerTemplate = true
	return true
end

updateRDBPortMapping = function (rule)
	if not rule then return false end

	local rdbIndex =  rule.rdbId

	if not rdbIndex or rdbIndex == '-1' then return false end

	local rdbValue = build_rdbValue(rule)

	if not rdbValue then return false end

	luardb.set('service.firewall.dnat.' .. rdbIndex, rdbValue)
	needTriggerTemplate = true
	return true
end

delRDBPortMapping = function (rule)
	if not rule then return false end

	local rdbIndex = rule.rdbId
	if not rdbIndex or rdbIndex == '-1' then return false end
	local numTypeIdx = tonumber(rdbIndex)
	if not numTypeIdx then return false end

	for i, value in traverseRdbVariable{prefix='service.firewall.dnat', startIdx=0} do
		if value == '' then break end

		if i >= numTypeIdx then
			local nextRdbValue = luardb.get('service.firewall.dnat.' .. (i+1))
			if nextRdbValue == nil then
				nextRdbValue = ''
			end
			luardb.set('service.firewall.dnat.' .. i, nextRdbValue)
		end
	end

	local unsetRdb = false
	for i, value in traverseRdbVariable{prefix='service.firewall.dnat', startIdx=0} do
		if unsetRdb == true then luardb.unset('service.firewall.dnat.' .. i) end

		if value == '' then unsetRdb = true end
	end

	rule.rdbId = '-1'
	needTriggerTemplate = true
	return true
end

createDefaultPersistObject = function (id)
	local routeRules = class:getByProperty('instanceId', tostring(id))

	if #routeRules > 0 then
		Logger.log('wanDevice.lua', 'error', 'ERROR in createDefaultPersistObject(): occupied id = ' .. id)
		return false
	end

	local newRule = class:new()
	newRule.isPersist = '1'
	newRule.instanceId = id
	newRule.rdbId = -1
	newRule.PortMappingEnabled = '0'
	newRule.PortMappingLeaseDuration = '0'
	newRule.RemoteHost = ''
	newRule.ExternalPort = '10000'
	newRule.ExternalPortEndRange = '0'
	newRule.InternalPort = '10000'
	newRule.PortMappingProtocol = 'tcp'
	newRule.InternalClient = ''
	newRule.PortMappingDescription = ''

	return true
end

deletePortMapObject = function (id)
	local portMapRules = class:getByProperty('instanceId', tostring(id))

	if #portMapRules == 0 then
		Logger.log('wanDevice.lua', 'error', 'ERROR in deletePortMapObject(): Not Exist: id = ' .. id)
		return false
	end

	if #portMapRules > 1 then
		Logger.log('wanDevice.lua', 'error', 'ERROR in deletePortMapObject(): has More then 2 ids: id = ' .. id)
		return false
	end

	for _, rule in ipairs(portMapRules) do
		if rule then
			delRDBPortMapping(rule)
			class:delete(rule)
		end
	end

	return true
end


-- Original Command
-- rdb_get -L "service.firewall.dnat." | grep '^service\.firewall\.dnat\.[0-9]\+' | sort | md5sum
getMD5sumforPM = function ()
	local digest = Daemon.readCommandOutput('rdb_get -L "service.firewall.dnat." | grep \'^service\\.firewall\\.dnat\\.[0-9]\\+\' | sort | md5sum')
	digest = digest:match('(%w+)')

	return digest or ''
end

sort_func = function (a, b)
	local pOrder = {tcp=1, udp=2, all=3}
	local a1, a2, a3, a4, b1, b2, b3, b4

	if not b then return false end

	a1, a2, a3, a4 = string.match(a.sourIP, '(%d+)%.(%d+)%.(%d+)%.(%d+)')
	a1, a2, a3, a4 = tonumber(a1), tonumber(a2), tonumber(a3), tonumber(a4)
	b1, b2, b3, b4 = string.match(b.sourIP, '(%d+)%.(%d+)%.(%d+)%.(%d+)')
	b1, b2, b3, b4 = tonumber(b1), tonumber(b2), tonumber(b3), tonumber(b4)
	if not a1 or not a2 or not a3 or not a4 then return false end
	if not b1 or not b2 or not b3 or not b4 then return false end
	if a1 > b1 then return true elseif a1 < b1 then return false end
	if a2 > b2 then return true elseif a2 < b2 then return false end
	if a3 > b3 then return true elseif a3 < b3 then return false end
	if a4 > b4 then return true elseif a4 < b4 then return false end

	if tonumber(a.sourPortS) > tonumber(b.sourPortS) then return true end
	if tonumber(a.sourPortS) < tonumber(b.sourPortS) then return false end
	a.protocol=a.protocol:lower()
	b.protocol=b.protocol:lower()
	if pOrder[a.protocol] < pOrder[b.protocol] then return true end
end

sort_PortMapping = function ()
	local portMappingTbl = {}

	for i, value in traverseRdbVariable{prefix='service.firewall.dnat', startIdx=0} do
		if value == '' then break end

		local ret, protocol, sourIP, sourPortS, sourPortE, destIp, destPortS, destPortE
		local v = value

		ret, _, sourIP = v:find('%-s%s+(%d+%.%d+%.%d+%.%d+)%s+')

		if ret then
			v = v:gsub('%-s%s+(%d+%.%d+%.%d+%.%d+)%s+', '')
		else
			sourIP = '0.0.0.0'
		end

		ret,_,protocol, sourPortS, sourPortE, destIp, destPortS, destPortE = 
			v:find('%-p%s+(%S+)%s+%-%-dport%s+(%d+):(%d+)%s+%-i%s+%S+%s+%-j%s+%S+%s+%-%-to%-destination%s+(%d+%.%d+%.%d+%.%d+):(%d+)%-(%d+)')

		if ret then
			local tempTbl={['idx'] = i, ['sourIP']=sourIP, ['sourPortS']=sourPortS, ['protocol']=protocol}
			table.insert(portMappingTbl, tempTbl)
		end
	end

	table.sort(portMappingTbl, sort_func)

	local needrealloc = false
	for i, value in ipairs(portMappingTbl) do
		if (i-1) ~= value.idx then
			needrealloc = true
			break
		end
	end

	if needrealloc == false then return end

	local rdbTbl = {}

	Logger.log('wanDevice.lua', 'debug', 'Port Mapping rdb variable updated')
	for i, value in ipairs(portMappingTbl) do
		local rdb_value = luardb.get('service.firewall.dnat.' .. value.idx)
		rdbTbl[#rdbTbl + 1] = rdb_value
	end

	for i = 1, #rdbTbl do
		luardb.set('service.firewall.dnat.' .. (i-1), rdbTbl[i])
	end

	needTriggerTemplate = true
end

build_objectList = function ()
	local persistRules = class:getByProperty('isPersist', '1')

	for _, rule in ipairs(persistRules) do
		rule.rdbId = -1
	end

	for i, value in traverseRdbVariable{prefix='service.firewall.dnat', startIdx=0} do
		if value == '' then break end

		local skip = false
		local ret, protocol, sourIP, sourPortS, sourPortE, destIp, destPortS, destPortE
		local v = value

		ret, _, sourIP = v:find('%-s%s+(%d+%.%d+%.%d+%.%d+)%s+')

		if ret then
			v = v:gsub('%-s%s+(%d+%.%d+%.%d+%.%d+)%s+', '')
		else
			sourIP = ''
		end

		ret,_,protocol, sourPortS, sourPortE, destIp, destPortS, destPortE = 
			v:find('%-p%s+(%S+)%s+%-%-dport%s+(%d+):(%d+)%s+%-i%s+%S+%s+%-j%s+%S+%s+%-%-to%-destination%s+(%d+%.%d+%.%d+%.%d+):(%d+)%-(%d+)')

		if ret then
			for _, rule in ipairs(persistRules) do
				if rule.RemoteHost == sourIP and rule.ExternalPort == sourPortS and rule.PortMappingProtocol == protocol then
					if (rule.ExternalPortEndRange == '0' and sourPortS == sourPortE) or (rule.ExternalPortEndRange == sourPortE) then
						skip = true
						rule.rdbId = i
						rule.PortMappingEnabled = '1'
						break
					end
				end
			end

			if skip == false then
				local newRule = class:new()
				newRule.isPersist = '0'
				newRule.instanceId = 0
				newRule.rdbId = i
				newRule.PortMappingEnabled = '1'
				newRule.PortMappingLeaseDuration = '0'
				newRule.RemoteHost = sourIP
				newRule.ExternalPort = sourPortS
				if sourPortS == sourPortE then
					newRule.ExternalPortEndRange = '0'
				else
					newRule.ExternalPortEndRange = sourPortE
				end
				newRule.InternalPort = destPortS
				newRule.PortMappingProtocol = protocol
				newRule.InternalClient = destIp
				newRule.PortMappingDescription = ''
			end
		end

	end

	for _, rule in ipairs(persistRules) do
		if rule.rdbId == '-1' then
			rule.PortMappingEnabled = '0'
		end
	end

-- 	local invalidRules = class:getByProperty('rdbId', '-1')
-- 	for _, rule in ipairs(invalidRules) do
-- 		if rule then
-- 			class:delete(rule)
-- 		end
-- 	end
end

local savedDigest

portMappingWatcher = function ()
	Logger.log('wanDevice.lua', 'debug', 'portMappingWatcher: Checking Port Mapping rules')

	local currentDigest = getMD5sumforPM()

	if currentDigest == '' or savedDigest == currentDigest then return end

	savedDigest = currentDigest

	Logger.log('wanDevice.lua', 'info', 'portMappingWatcher: Update Port Mapping rules')

--	local node = paramTree:find('$ROOT.WANDevice.1.WANConnectionDevice.1.WANIPConnection.1.PortMapping')
	local node = paramTree:find('$ROOT.WANDevice.1.WANConnectionDevice.1.WANIPConnection.X_NETCOMM_PortMapping')

	local mappingRules = class:getByProperty('isPersist', '0')
	for _, rule in ipairs(mappingRules) do
		if rule and rule.PortMappingEnabled == '1' then
			Logger.log('wanDevice.lua', 'debug', 'portMappingWatcher: delete route class object id =' .. rule.instanceId)
			class:delete(rule)
		end
	end

	sort_PortMapping()

	if needTriggerTemplate == true then
		Logger.log('wanDevice.lua', 'debug', 'portMappingWatcher: trigger nat.template for Port Mapping')
		luardb.set('service.firewall.dnat.trigger', '1')
		needTriggerTemplate = false
	end
	build_objectList()

	local leaseCollection = node
	if leaseCollection == nil then return 0 end
	if leaseCollection.children ~= nil then
		for _, lease in ipairs(leaseCollection.children) do
			if lease.name ~= '0' then
				Logger.log('wanDevice.lua', 'info', 'portMappingWatcher: deleting Node: id = ' .. lease.name)
				lease.parent:deleteChild(lease)
			end
		end
		leaseCollection.instance = 0
	end

	local maxInstanceId = 0
	local listOccupied = {}

	mappingRules = class:getByProperty('isPersist', '1')

	for _, rule in ipairs(mappingRules) do
		local id = tonumber(rule.instanceId or 0)
		if id < 1 then
			Logger.log('wanDevice.lua', 'info', 'portMappingWatcher: Error Persist object does not have id, RemoteHost = ' .. (rule.RemoteHost or '*nil*'))
		else
			Logger.log('wanDevice.lua', 'debug', 'portMappingWatcher: creating Persist Rules: RemoteHost = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)
			local instance = node:createDefaultChild(rule.instanceId)
			listOccupied[id] = true
			if id > maxInstanceId then maxInstanceId = id end
		end
	end

	mappingRules = class:getByProperty('isPersist', '0')
	for _, rule in ipairs(mappingRules) do
		local id = tonumber(rule.instanceId or 0)
		if id > 0 then
			Logger.log('wanDevice.lua', 'debug', 'portMappingWatcher: creating Disabled Dynamic Rules: RemoteHost = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)
			local instance = node:createDefaultChild(rule.instanceId)
			listOccupied[id] = true
			if id > maxInstanceId then maxInstanceId = id end
		end
	end

	mappingRules = class:getByProperty('isPersist', '0')
	for _, rule in ipairs(mappingRules) do
		local id = tonumber(rule.instanceId or 0)

		if id == 0 then
			local availableid = 1
			for i, _ in ipairs(listOccupied) do
				availableid = availableid + 1
			end
			rule.instanceId = availableid

			Logger.log('wanDevice.lua', 'debug', 'portMappingWatcher: creating Enabled Dynamic Rules: RemoteHost = ' .. (rule.RemoteHost or '*nil*') .. ', availableid = ' .. availableid)

			local instance = node:createDefaultChild(rule.instanceId)
			listOccupied[availableid] = true
			if availableid > maxInstanceId then maxInstanceId = availableid end
		end
	end
	node.instance = maxInstanceId
end

------------------------<< End : PortMapping >>------------------------


return {

------------------------<<Start: WANCommonInterfaceConfig >>------------------------
	['**.WANCommonInterfaceConfig.PhysicalLinkStatus'] = {	-- readonly string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wan_if = wwan_ifname()

			if not wan_if or wan_if == '' then return 0, 'Down' end
			return 0, 'Up'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANCommonInterfaceConfig.TotalBytesSent'] = {	-- readonly uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, readStatistics('tx_bytes')

		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANCommonInterfaceConfig.TotalBytesReceived'] = {	-- readonly uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, readStatistics('rx_bytes')
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANCommonInterfaceConfig.TotalPacketsSent'] = {	-- readonly uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, readStatistics('tx_packets')
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANCommonInterfaceConfig.TotalPacketsReceived'] = {	-- readonly uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, readStatistics('rx_packets')
		end,
		set = function(node, name, value)
			return 0
		end
	},
------------------------<< End : WANCommonInterfaceConfig >>------------------------

------------------------<<Start: WANIPConnection >>------------------------
-- Basically, the structure of this parameter lists totally doesn't match with our WEBUI structure.
-- So the parameters that belong to each APN list cannot be fully supported.
-- This feature will be provided in APN list parameters(vendor specific parameter).
	['**.WANConnectionDevice.1.WANIPConnection.1.Enable'] = {	-- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local enabledIdx = getEnabledProfile()

			if not enabledIdx then return 0, '0' end
			return 0, '1'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.ConnectionStatus'] = {	-- readonly string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local linkStatus = luardb.get('wwan.0.system_network_status.pdp0_stat')

			if linkStatus and linkStatus:lower() == 'up' then return 0, 'Connected' end
			return 0, 'Disconnected'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.Uptime'] = {	-- readonly uint: Bovine doesn't support this feature
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, '0'
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Cannot support set function, because of conflict with WEBUI
-- NAT Enable/Disable feature will be provided in APN list parameters(vendor specific parameter).
	['**.WANConnectionDevice.1.WANIPConnection.1.NATEnabled'] = {	-- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local enabledIdx = getEnabledProfile()

			if not enabledIdx then return 0, '0' end

			local retVal = luardb.get('link.profile.' .. enabledIdx .. '.snat')

			return 0, retVal or '0'
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- "Write" attribute is only for "Static" AddressingType.
	['**.WANConnectionDevice.1.WANIPConnection.1.ExternalIPAddress'] = {	-- readwrite string:
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local enabledIdx = getEnabledProfile()

			if not enabledIdx then return 0, '' end

			local retVal = luardb.get('link.profile.' .. enabledIdx .. '.iplocal')

			local status = luardb.get('link.profile.' .. enabledIdx .. '.status' )
			if status and status:lower() ~= 'up' then
				retVal = ''
			end

			return 0, retVal or ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- "Write" attribute is only for "Static" AddressingType.
	['**.WANConnectionDevice.1.WANIPConnection.1.SubnetMask'] = {	-- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wan_if = wwan_ifname()

			if not wan_if or wan_if ==  '' then return 0, '' end

			local result = Daemon.readCommandOutput('ifconfig ' .. wan_if .. ' | grep inet')

			if not result then return 0, '' end

			local ret, _, mask = result:find('Mask:(%d+.%d+.%d+.%d+)')

			if not ret or not Parameter.Validator.isValidIP4Netmask(mask) then return 0, '' end

			return 0, mask
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- "Write" attribute is only for "Static" AddressingType.
	['**.WANConnectionDevice.1.WANIPConnection.1.DefaultGateway'] = {	-- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wan_if = wwan_ifname()

			if not wan_if or wan_if ==  '' then return 0, '' end

			local result = Daemon.readCommandOutput('route -n')

			if not result then return 0, '' end

			local ret, _, mask = result:find('0.0.0.0%s+(%d+.%d+.%d+.%d+)%s+0.0.0.0%s+.*' .. wan_if)

			if not ret or not Parameter.Validator.isValidIP4(mask) then return 0, '' end

			return 0, mask
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Not use this parameter function table. This parameter has "readonly const" attribute in Bovine platform
	['**.WANConnectionDevice.1.WANIPConnection.1.DNSEnabled'] = {	-- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Not use this parameter function table. This parameter has "readonly const" attribute in Bovine platform
	['**.WANConnectionDevice.1.WANIPConnection.1.DNSOverrideAllowed'] = {	-- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.DNSServers'] = {	-- readonly string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retTbl = {}
			local resolveFileName = '/etc/resolv.conf'
			local fd = io.open(resolveFileName, 'r')

			if not fd then return 0, '' end

			for line in fd:lines()
			do
				local ret, _, dns = line:find('nameserver%s+(%d+\.%d+\.%d+\.%d+)')
				if ret and Parameter.Validator.isValidIP4(dns) then
					table.insert(retTbl, dns)
				end
			end

			fd:close()

			return 0, table.concat(retTbl, ',')
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.MaxMTUSize'] = {	-- readwrite uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local filename = '/sys/class/net/' .. wwan_ifname() .. '/mtu'

			local retVal = readIntFromFileToString(filename)

			return 0, retVal or '1500'
		end,
		set = function(node, name, value)
			local fmtInt = convertInternalInteger{input=value, minimum=1, maximum=1540}
			if not fmtInt then return CWMP.Error.InvalidParameterValue end

			local wan_if = wwan_ifname()
			if not wan_if or wan_if == '' then return CWMP.Error.InternalError end

			local result = os.execute('ifconfig ' .. wan_if .. ' mtu ' .. fmtInt)
			if result == 0 then 
				return 0
			else
				return CWMP.Error.InvalidParameterValue
			end
		end
	},
-- 3G WAN doesn't support set function
	['**.WANConnectionDevice.1.WANIPConnection.1.MACAddress'] = {	-- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local filename = '/sys/class/net/' .. wwan_ifname() .. '/address'

			local retVal = readStrFromFileWithErrorCheck(filename)

			if not retVal then return 0, '0' else return 0, retVal end
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- 3G WAN doesn't support this feature
	['**.WANConnectionDevice.1.WANIPConnection.1.MACAddressOverride'] = {	-- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Bovine doesn't support this feature
	['**.WANConnectionDevice.1.WANIPConnection.1.ConnectionTrigger'] = {	-- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.RouteProtocolRx'] = {	-- readwrite string: support "Off", "RIPv1", "RIPv2"
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local ripEnabled = luardb.get('service.router.rip.enable')

			if not ripEnabled or ripEnabled == '0' then return 0, 'Off' end

			if ripEnabled == '1' then
				local ripVersion = luardb.get('service.router.rip.version')
				if not ripVersion then return 0, 'Off' end

				if ripVersion == '1' then 
					return 0, 'RIPv1'
				elseif ripVersion == '2' then 
					return 0, 'RIPv2'
				else
					return 0, 'Off'
				end
			end
			return 0, 'Off'
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidParameterValue end

			value = value:lower()

			if value == 'off' then
				luardb.set('service.router.rip.enable', '0')
			elseif value == 'ripv1' then
				luardb.set('service.router.rip.version', '1')
				luardb.set('service.router.rip.enable', '1')
			elseif value == 'ripv2' then
				luardb.set('service.router.rip.version', '2')
				luardb.set('service.router.rip.enable', '1')
			else
				return CWMP.Error.InvalidParameterValue
			end

			return 0
		end
	},

------------------------<< End : WANIPConnection >>------------------------

--[[
------------------------<<Start: PortMapping >>------------------------
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.1.PortMappingNumberOfEntries'] = {	-- readwrite uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local forwarding = paramTree:find('$ROOT.WANDevice.1.WANConnectionDevice.1.WANIPConnection.1.PortMapping')
			return 0, tostring(forwarding:countInstanceChildren())
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.1.PortMapping'] = {
		init = function(node, name, value)

			node:setAccess('readwrite')

			needTriggerTemplate = false

			sort_PortMapping()
			if needTriggerTemplate == true then
				Logger.log('wanDevice.lua', 'debug', 'trigger nat.template for Port Mapping')
				luardb.set('service.firewall.dnat.trigger', '1')
				needTriggerTemplate = false
			end

			local listOccupied = {}

			local mappingRules = class:getByProperty('isPersist', '0')
			for _, rule in ipairs(mappingRules) do
				if rule then
					class:delete(rule)
				end
			end

			build_objectList()

			-- initial parse of leases
			local maxInstanceId = 0

			mappingRules = class:getByProperty('isPersist', '1')

			for _, rule in ipairs(mappingRules) do
				local id = tonumber(rule.instanceId or 0)
				if id < 1 then
					Logger.log('wanDevice.lua', 'info', 'PortMapping init: Error Persist object does not have id, destIp = ' .. (rule.RemoteHost or '*nil*'))
				else
					Logger.log('wanDevice.lua', 'debug', 'PortMapping init: creating Persist Rules: destIp = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)
					local instance = node:createDefaultChild(rule.instanceId)
					listOccupied[id] = true
					if id > maxInstanceId then maxInstanceId = id end
				end
			end

			mappingRules = class:getByProperty('isPersist', '0')
			for _, rule in ipairs(mappingRules) do
				local id = 1
				for i, _ in ipairs(listOccupied) do
					id = id + 1
				end
				rule.instanceId = id

				Logger.log('wanDevice.lua', 'debug', 'PortMapping init: creating Dynamic Rules: destIp = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)

				local instance = node:createDefaultChild(rule.instanceId)
				listOccupied[id] = true
				if id > maxInstanceId then maxInstanceId = id end
			end
			node.instance = maxInstanceId

			savedDigest = getMD5sumforPM()
			if client:isTaskQueued('cleanUp', portMappingWatcher) ~= true then
				client:addTask('cleanUp', portMappingWatcher, true) -- persistent callback function
			end
			return 0
		end,
		create = function(node, name, instanceId)
			Logger.log('wanDevice.lua', 'info', 'createInstance: [**.PortMapping]' .. node:getPath())

			if not instanceId then
				for i=1, maxNumOfInstance do
					local instance = class:getByProperty('instanceId', tostring(i))
					if #instance == 0 then instanceId = i; break; end
				end
			end

			if not instanceId then return CWMP.Error.ResourcesExceeded end

			Logger.log('wanDevice.lua', 'info', 'createInstance: [**.PortMapping] instanceId = ' .. instanceId)

			-- create new instance object
			local instance = node:createDefaultChild(instanceId)
			if not createDefaultPersistObject(instanceId) then return CWMP.Error.InternalError end
			return 0, instanceId
		end,
-- 		poll = portMappingWatcher
	},
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.1.PortMapping.*'] = {
-- 		create = function(node, name, instanceId)
-- 			Logger.log('wanDevice.lua', 'info', 'createInstance: [**.PortMapping.*]' .. node:getPath() .. ': ' .. name .. ', ' .. instanceId)
-- 			-- create new instance object
-- 			local instance = node:createDefaultChild(instanceId)
-- 			return 0
-- 		end,
		delete = function(node, name)
			Logger.log('wanDevice.lua', 'debug', 'delete Instance of PortMapping Object , name = ' .. name)
			node.parent:deleteChild(node)

			local pathBits = name:explode('.')
			if not deletePortMapObject(pathBits[9]) then return CWMP.Error.InternalError end
			return 0
		end,
	},
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.1.PortMapping.*.*'] = {
		init = function(node, name, value)
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local objs = class:getByProperty('instanceId', pathBits[9])
			for _, obj in ipairs(objs) do
				if pathBits[10] == 'PortMappingEnabled' then
					node.value = obj.PortMappingEnabled or ''
				elseif pathBits[10] == 'PortMappingLeaseDuration' then
					node.value = obj.PortMappingLeaseDuration or ''
				elseif pathBits[10] == 'RemoteHost' then
					node.value = obj.RemoteHost or ''
				elseif pathBits[10] == 'ExternalPort' then
					node.value = obj.ExternalPort or ''
				elseif pathBits[10] == 'ExternalPortEndRange' then
					node.value = obj.ExternalPortEndRange or ''
				elseif pathBits[10] == 'InternalPort' then
					node.value = obj.InternalPort or ''
				elseif pathBits[10] == 'PortMappingProtocol' then
					node.value = obj.PortMappingProtocol or ''
				elseif pathBits[10] == 'InternalClient' then
					node.value = obj.InternalClient or ''
				elseif pathBits[10] == 'PortMappingDescription' then
					node.value = obj.PortMappingDescription or ''
				else
					error('Dunno how to handle ' .. name)
				end
			end
			return 0, node.value
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local objs = class:getByProperty('instanceId', pathBits[9])
			if not value then return CWMP.Error.InvalidParameterValue end
			for _, obj in ipairs(objs) do
				if pathBits[10] == 'PortMappingEnabled' then
					local internalBool = convertInternalBoolean(value)
					if internalBool == nil then return CWMP.Error.InvalidParameterValue end
					if obj.PortMappingEnabled == internalBool then return 0 end
					if internalBool == '1' then  --> from disable to enable
						addRDBPortMapping(obj)
					else
						delRDBPortMapping(obj)
					end
					obj.PortMappingEnabled = internalBool
				elseif pathBits[10] == 'PortMappingLeaseDuration' then
					if value ~= '0' then return CWMP.Error.InvalidParameterValue end
-- 					obj.PortMappingLeaseDuration = '0'
					return 0
				elseif pathBits[10] == 'RemoteHost' then
					if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
					if obj.RemoteHost == value then return 0 end

					obj.RemoteHost = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'ExternalPort' then
					local interalInt = convertInternalInteger{input=value, minimum=1, maximum=65535}
					if interalInt == nil then return CWMP.Error.InvalidParameterValue end
					if value == obj.ExternalPort then return 0 end

					obj.ExternalPort = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'ExternalPortEndRange' then
					local range = obj.ExternalPortEndRange
					local interalInt = convertInternalInteger{input=value, minimum=0, maximum=65535}
					if interalInt == nil then return CWMP.Error.InvalidParameterValue end
					if value == range then return 0 end
					if range ~= '0' and tonumber(range) < tonumber(obj.ExternalPort) then return CWMP.Error.InvalidParameterValue end

					obj.ExternalPortEndRange = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'InternalPort' then
					local interalInt = convertInternalInteger{input=value, minimum=1, maximum=65535}
					if interalInt == nil then return CWMP.Error.InvalidParameterValue end
					if value == obj.InternalPort then return 0 end

					obj.InternalPort = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'PortMappingProtocol' then
					if value ~= 'tcp' and value ~= 'udp' and value ~= 'all' then return CWMP.Error.InvalidParameterValue end
					if value == obj.PortMappingProtocol then return 0 end

					obj.PortMappingProtocol = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'InternalClient' then
					if value ~= '' and not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
					if value == obj.InternalClient then return 0 end

					obj.InternalClient = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'PortMappingDescription' then
					obj.PortMappingDescription = value
				else
					error('Dunno how to handle ' .. name)
				end
			end
			return 0
		end,
	},
------------------------<< End : PortMapping >>------------------------
--]]


------------------------<<Start: X_NETCOMM_PortMapping >>------------------------
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.X_NETCOMM_PortMappingNumberOfEntries'] = {	-- readwrite uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local forwarding = paramTree:find('$ROOT.WANDevice.1.WANConnectionDevice.1.WANIPConnection.X_NETCOMM_PortMapping')
			return 0, tostring(forwarding:countInstanceChildren())
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.X_NETCOMM_PortMapping'] = {
		init = function(node, name, value)

			node:setAccess('readwrite')

			needTriggerTemplate = false

			sort_PortMapping()
			if needTriggerTemplate == true then
				Logger.log('wanDevice.lua', 'debug', 'trigger nat.template for Port Mapping')
				luardb.set('service.firewall.dnat.trigger', '1')
				needTriggerTemplate = false
			end

			local listOccupied = {}

			local mappingRules = class:getByProperty('isPersist', '0')
			for _, rule in ipairs(mappingRules) do
				if rule then
					class:delete(rule)
				end
			end

			build_objectList()

			-- initial parse of leases
			local maxInstanceId = 0

			mappingRules = class:getByProperty('isPersist', '1')

			for _, rule in ipairs(mappingRules) do
				local id = tonumber(rule.instanceId or 0)
				if id < 1 then
					Logger.log('wanDevice.lua', 'info', 'X_NETCOMM_PortMapping init: Error Persist object does not have id, destIp = ' .. (rule.RemoteHost or '*nil*'))
				else
					Logger.log('wanDevice.lua', 'debug', 'X_NETCOMM_PortMapping init: creating Persist Rules: destIp = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)
					local instance = node:createDefaultChild(rule.instanceId)
					listOccupied[id] = true
					if id > maxInstanceId then maxInstanceId = id end
				end
			end

			mappingRules = class:getByProperty('isPersist', '0')
			for _, rule in ipairs(mappingRules) do
				local id = 1
				for i, _ in ipairs(listOccupied) do
					id = id + 1
				end
				rule.instanceId = id

				Logger.log('wanDevice.lua', 'debug', 'X_NETCOMM_PortMapping init: creating Dynamic Rules: destIp = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)

				local instance = node:createDefaultChild(rule.instanceId)
				listOccupied[id] = true
				if id > maxInstanceId then maxInstanceId = id end
			end
			node.instance = maxInstanceId

			savedDigest = getMD5sumforPM()
			if client:isTaskQueued('postSession', portMappingWatcher) ~= true then
				client:addTask('postSession', portMappingWatcher, true) -- persistent callback function
			end
			return 0
		end,
		create = function(node, name, instanceId)
			Logger.log('wanDevice.lua', 'info', 'createInstance: [**.X_NETCOMM_PortMapping]' .. node:getPath())

			if not instanceId then
				for i=1, maxNumOfInstance do
					local instance = class:getByProperty('instanceId', tostring(i))
					if #instance == 0 then instanceId = i; break; end
				end
			end

			if not instanceId then return CWMP.Error.ResourcesExceeded end

			Logger.log('wanDevice.lua', 'info', 'createInstance: [**.X_NETCOMM_PortMapping] instanceId = ' .. instanceId)

			-- create new instance object
			local instance = node:createDefaultChild(instanceId)
			if not createDefaultPersistObject(instanceId) then return CWMP.Error.InternalError end
			return 0, instanceId
		end,
-- 		poll = portMappingWatcher
	},
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.X_NETCOMM_PortMapping.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOf_X_NETCOMM_PortMapping = #pathBits
			return 0
		end,
-- 		create = function(node, name, instanceId)
-- 			Logger.log('wanDevice.lua', 'info', 'createInstance: [**.X_NETCOMM_PortMapping.*]' .. node:getPath() .. ': ' .. name .. ', ' .. instanceId)
-- 			-- create new instance object
-- 			local instance = node:createDefaultChild(instanceId)
-- 			return 0
-- 		end,
		delete = function(node, name)
			Logger.log('wanDevice.lua', 'debug', 'delete Instance of X_NETCOMM_PortMapping Object , name = ' .. name)
			node.parent:deleteChild(node)

			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOf_X_NETCOMM_PortMapping]
			if not deletePortMapObject(dataModelIdx) then return CWMP.Error.InternalError end
			return 0
		end,
	},
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.X_NETCOMM_PortMapping.*.*'] = {
		init = function(node, name, value)
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOf_X_NETCOMM_PortMapping]
			local paramName = pathBits[g_depthOf_X_NETCOMM_PortMapping+1]

			local objs = class:getByProperty('instanceId', dataModelIdx)
			for _, obj in ipairs(objs) do
				if paramName == 'PortMappingEnabled' then
					node.value = obj.PortMappingEnabled or ''
				elseif paramName == 'PortMappingLeaseDuration' then
					node.value = obj.PortMappingLeaseDuration or ''
				elseif paramName == 'RemoteHost' then
					node.value = obj.RemoteHost or ''
				elseif paramName == 'ExternalPort' then
					node.value = obj.ExternalPort or ''
				elseif paramName == 'ExternalPortEndRange' then
					node.value = obj.ExternalPortEndRange or ''
				elseif paramName == 'InternalPort' then
					node.value = obj.InternalPort or ''
				elseif paramName == 'PortMappingProtocol' then
					node.value = obj.PortMappingProtocol or ''
				elseif paramName == 'InternalClient' then
					node.value = obj.InternalClient or ''
				elseif paramName == 'PortMappingDescription' then
					node.value = obj.PortMappingDescription or ''
				else
					error('Dunno how to handle ' .. name)
				end
			end
			return 0, node.value
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOf_X_NETCOMM_PortMapping]
			local paramName = pathBits[g_depthOf_X_NETCOMM_PortMapping+1]

			local objs = class:getByProperty('instanceId', dataModelIdx)
			if not value then return CWMP.Error.InvalidParameterValue end
			for _, obj in ipairs(objs) do
				if paramName == 'PortMappingEnabled' then
					local internalBool = convertInternalBoolean(value)
					if internalBool == nil then return CWMP.Error.InvalidParameterValue end
					if obj.PortMappingEnabled == internalBool then return 0 end
					if internalBool == '1' then  --> from disable to enable
						addRDBPortMapping(obj)
					else
						delRDBPortMapping(obj)
					end
					obj.PortMappingEnabled = internalBool
				elseif paramName == 'PortMappingLeaseDuration' then
					if value ~= '0' then return CWMP.Error.InvalidParameterValue end
-- 					obj.PortMappingLeaseDuration = '0'
					return 0
				elseif paramName == 'RemoteHost' then
					if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
					if obj.RemoteHost == value then return 0 end

					obj.RemoteHost = value
					updateRDBPortMapping(obj)
				elseif paramName == 'ExternalPort' then
					local interalInt = convertInternalInteger{input=value, minimum=1, maximum=65535}
					if interalInt == nil then return CWMP.Error.InvalidParameterValue end
					if value == obj.ExternalPort then return 0 end

					obj.ExternalPort = value
					updateRDBPortMapping(obj)
				elseif paramName == 'ExternalPortEndRange' then
					local range = obj.ExternalPortEndRange
					local interalInt = convertInternalInteger{input=value, minimum=0, maximum=65535}
					if interalInt == nil then return CWMP.Error.InvalidParameterValue end
					if value == range then return 0 end
					if interalInt ~= 0 and interalInt < tonumber(obj.ExternalPort) then return CWMP.Error.InvalidParameterValue end

					obj.ExternalPortEndRange = value
					updateRDBPortMapping(obj)
				elseif paramName == 'InternalPort' then
					local interalInt = convertInternalInteger{input=value, minimum=1, maximum=65535}
					if interalInt == nil then return CWMP.Error.InvalidParameterValue end
					if value == obj.InternalPort then return 0 end

					obj.InternalPort = value
					updateRDBPortMapping(obj)
				elseif paramName == 'PortMappingProtocol' then
					if value ~= 'tcp' and value ~= 'udp' and value ~= 'all' then return CWMP.Error.InvalidParameterValue end
					if value == obj.PortMappingProtocol then return 0 end

					obj.PortMappingProtocol = value
					updateRDBPortMapping(obj)
				elseif paramName == 'InternalClient' then
					if value ~= '' and not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
					if value == obj.InternalClient then return 0 end

					obj.InternalClient = value
					updateRDBPortMapping(obj)
				elseif paramName == 'PortMappingDescription' then
					obj.PortMappingDescription = value
				else
					error('Dunno how to handle ' .. name)
				end
			end
			return 0
		end,
	},
------------------------<< End : X_NETCOMM_PortMapping >>------------------------
}
