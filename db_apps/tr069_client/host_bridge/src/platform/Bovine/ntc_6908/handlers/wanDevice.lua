------------------------<<Start: WANCommonInterfaceConfig >>------------------------
local function wwan_ifname()
	return luardb.get('wwan.0.netif_udev') or ''
end

local function readIntFromFileToString(filename)
	local file = io.open(filename, 'r')
	if not file then return nil end

	local n = file:read('*n')
	if not n then return nil end

	n = math.floor(n)
	file:close()
	return tostring(n)
end

local function readStrFromFileWithErrorCheck(filename)
	local file = io.open(filename, 'r')
	if not file then return nil end

	local line = file:read('*l')
	file:close()
	return line
end

local function readStatistics(name)
	local filename = '/sys/class/net/' .. wwan_ifname() .. '/statistics/' .. name

	local retVal = readIntFromFileToString(filename)

	if not retVal then return '0' else return retVal end
end
------------------------<< End : WANCommonInterfaceConfig >>------------------------

------------------------<<Start: WANIPConnection >>------------------------
local function traverseProfile (name)
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
local function getEnabledProfile()
	local index = nil

	for i, v in traverseProfile('enable') do
		if v == '1' then index = i; break end
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

local config = {
	persist = true,
	idSelection = 'smallestUnused',
}

local class = rdbobject.getClass('tr069.wan.PortMapping', config)

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

local function build_rdbValue(rule)
	local protocol, sourIP, sourPortS, sourPortE, destIp, destPortS
		= rule.PortMappingProtocol, rule.RemoteHost, rule.ExternalPort, rule.ExternalPortEndRange, rule.InternalClient, rule.InternalPort

	if not protocol or not sourIP or not sourPortS or not sourPortE or not destIp or not destPortS then return nil end
	if protocol ~= 'tcp' and protocol ~= 'udp' and protocol ~= 'all' then return nil end
	if sourIP ~= '' and not isValidIP4(sourIP) then return nil end
	if not tonumber(sourPortS) or not tonumber(sourPortE) then return nil end
	if not isValidIP4(destIp) then return nil end
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

	dimclient.log('debug', 'portMapping rdb value = [' .. rdbValue .. ']')

	return rdbValue

end

local function addRDBPortMapping (rule)
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

local function updateRDBPortMapping (rule)
	if not rule then return false end

	local rdbIndex =  rule.rdbId

	if not rdbIndex or rdbIndex == '-1' then return false end

	local rdbValue = build_rdbValue(rule)

	if not rdbValue then return false end

	luardb.set('service.firewall.dnat.' .. rdbIndex, rdbValue)
	needTriggerTemplate = true
	return true
end

local function delRDBPortMapping (rule)
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

local function createDefaultPersistObject (id)
	local routeRules = class:getByProperty('instanceId', tostring(id))

	if #routeRules > 0 then
		dimclient.log('error', 'ERROR in createDefaultPersistObject(): occupied id = ' .. id)
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

local function deletePortMapObject (id)
	local portMapRules = class:getByProperty('instanceId', tostring(id))

	if #portMapRules == 0 then
		dimclient.log('error', 'ERROR in deletePortMapObject(): Not Exist: id = ' .. id)
		return false
	end

	if #portMapRules > 1 then
		dimclient.log('error', 'ERROR in deletePortMapObject(): has More then 2 ids: id = ' .. id)
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
local function getMD5sumforPM ()
	local digest = execute_CmdLine('rdb_get -L "service.firewall.dnat." | grep \'^service\\.firewall\\.dnat\\.[0-9]\\+\' | sort | md5sum')
	digest = digest:match('(%w+)')

	return digest or ''
end

local function sort_func (a, b)
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

	if pOrder[a.protocol] < pOrder[b.protocol] then return true end
end

local function sort_PortMapping()
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

	dimclient.log('debug', 'Port Mapping rdb variable updated')
	for i, value in ipairs(portMappingTbl) do
		local rdb_value = luardb.get('service.firewall.dnat.' .. value.idx)
		rdbTbl[#rdbTbl + 1] = rdb_value
	end

	for i = 1, #rdbTbl do
		luardb.set('service.firewall.dnat.' .. (i-1), rdbTbl[i])
	end

	needTriggerTemplate = true
end

local function build_objectList()
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

-- 	local invalidRules = class:getByProperty('rdbId', '-1')
-- 	for _, rule in ipairs(invalidRules) do
-- 		if rule then
-- 			class:delete(rule)
-- 		end
-- 	end
end

local function portMappingWatcher(node, name)
	dimclient.log('debug', 'portMappingWatcher: Checking Port Mapping rules')

	local currentDigest = getMD5sumforPM()

	if currentDigest == '' or savedDigest == currentDigest then return end

	savedDigest = currentDigest

	dimclient.log('debug', 'portMappingWatcher: Update Port Mapping rules')


	local mappingRules = class:getByProperty('isPersist', '0')
	for _, rule in ipairs(mappingRules) do
		if rule and rule.PortMappingEnabled == '1' then
			dimclient.log('debug', 'portMappingWatcher: delete route class object id =' .. rule.instanceId)
			class:delete(rule)
		end
	end

	sort_PortMapping()

	if needTriggerTemplate == true then
		dimclient.log('debug', 'portMappingWatcher: trigger nat.template for Port Mapping')
		luardb.set('service.firewall.dnat.trigger', '1')
		needTriggerTemplate = false
	end
	build_objectList()

	local leaseCollection = findNode(paramRoot, name)
	if leaseCollection == nil then return 0 end
	if leaseCollection.children ~= nil then
		for _, lease in ipairs(leaseCollection.children) do
			if lease.name ~= '0' then
				dimclient.log('info', 'portMappingWatcher: deleting Node: id = ' .. lease.name)
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
			dimclient.log('info', 'portMappingWatcher: Error Persist object does not have id, RemoteHost = ' .. (rule.RemoteHost or '*nil*'))
		else
			dimclient.log('debug', 'portMappingWatcher: creating Persist Rules: RemoteHost = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)
			local instance = node:createDefaultChild(rule.instanceId)
			listOccupied[id] = true
			if id > maxInstanceId then maxInstanceId = id end
		end
	end

	mappingRules = class:getByProperty('isPersist', '0')
	for _, rule in ipairs(mappingRules) do
		local id = tonumber(rule.instanceId or 0)
		if id > 0 then
			dimclient.log('debug', 'portMappingWatcher: creating Disabled Dynamic Rules: RemoteHost = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)
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

			dimclient.log('debug', 'portMappingWatcher: creating Enabled Dynamic Rules: RemoteHost = ' .. (rule.RemoteHost or '*nil*') .. ', availableid = ' .. availableid)

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

			if not wan_if or wan_if == '' then return 'Down' end
			return 'Up'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANCommonInterfaceConfig.TotalBytesSent'] = {	-- readonly uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return readStatistics('tx_bytes')

		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANCommonInterfaceConfig.TotalBytesReceived'] = {	-- readonly uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return readStatistics('rx_bytes')
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANCommonInterfaceConfig.TotalPacketsSent'] = {	-- readonly uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return readStatistics('tx_packets')
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANCommonInterfaceConfig.TotalPacketsReceived'] = {	-- readonly uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return readStatistics('rx_packets')
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

			if not enabledIdx then return '0' end
			return '1'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.ConnectionStatus'] = {	-- readonly string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local linkStatus = luardb.get('wwan.0.system_network_status.pdp0_stat')

			if linkStatus and linkStatus:lower() == 'up' then return 'Connected' end
			return 'Disconnected'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.Uptime'] = {	-- readonly uint: Bovine doesn't support this feature
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return '0'
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

			if not enabledIdx then return '0' end

			local retVal = luardb.get('link.profile.' .. enabledIdx .. '.snat')

			return retVal or '0'
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

			if not enabledIdx then return '' end

			local retVal = luardb.get('link.profile.' .. enabledIdx .. '.iplocal')

			return retVal or ''
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

			if not wan_if or wan_if ==  '' then return '' end

			local result = execute_CmdLine('ifconfig ' .. wan_if .. ' | grep inet')

			if not result then return '' end

			local ret, _, mask = result:find('Mask:(%d+.%d+.%d+.%d+)')

			if not ret or not isValidIP4Netmask(mask) then return '' end

			return mask
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

			if not wan_if or wan_if ==  '' then return '' end

			local result = execute_CmdLine('route -n')

			if not result then return '' end

			local ret, _, mask = result:find('0.0.0.0%s+(%d+.%d+.%d+.%d+)%s+0.0.0.0%s+.*' .. wan_if)

			if not ret or not isValidIP4(mask) then return '' end

			return mask
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Not use this parameter function table. This parameter has "readonly const" attribute in Bovine platform
	['**.WANConnectionDevice.1.WANIPConnection.1.DNSEnabled'] = {	-- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Not use this parameter function table. This parameter has "readonly const" attribute in Bovine platform
	['**.WANConnectionDevice.1.WANIPConnection.1.DNSOverrideAllowed'] = {	-- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return ''
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

			if not fd then return '' end

			for line in fd:lines()
			do
				local ret, _, dns = line:find('nameserver%s+(%d+\.%d+\.%d+\.%d+)')
				if ret and isValidIP4(dns) then
					table.insert(retTbl, dns)
				end
			end

			fd:close()

			return table.concat(retTbl, ',')
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

			return retVal or '1500'
		end,
		set = function(node, name, value)
			local fmtInt = convertInternalInteger{input=value, minimum=1, maximum=1540}
			if not fmtInt then return cwmpError.InvalidParameterValue end

			local wan_if = wwan_ifname()
			if not wan_if or wan_if == '' then return cwmpError.InternalError end

			local result = os.execute('ifconfig ' .. wan_if .. ' mtu ' .. fmtInt)
			if result == 0 then 
				return 0
			else
				return cwmpError.InvalidParameterValue
			end
		end
	},
-- 3G WAN doesn't support set function
	['**.WANConnectionDevice.1.WANIPConnection.1.MACAddress'] = {	-- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local filename = '/sys/class/net/' .. wwan_ifname() .. '/address'

			local retVal = readStrFromFileWithErrorCheck(filename)

			if not retVal then return '0' else return retVal end
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- 3G WAN doesn't support this feature
	['**.WANConnectionDevice.1.WANIPConnection.1.MACAddressOverride'] = {	-- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Bovine doesn't support this feature
	['**.WANConnectionDevice.1.WANIPConnection.1.ConnectionTrigger'] = {	-- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.RouteProtocolRx'] = {	-- readwrite string: support "Off", "RIPv1", "RIPv2"
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local ripEnabled = luardb.get('service.router.rip.enable')

			if not ripEnabled or ripEnabled == '0' then return 'Off' end

			if ripEnabled == '1' then
				local ripVersion = luardb.get('service.router.rip.version')
				if not ripVersion then return 'Off' end

				if ripVersion == '1' then 
					return 'RIPv1'
				elseif ripVersion == '2' then 
					return 'RIPv2'
				else
					return 'Off'
				end
			end
			return 'Off'
		end,
		set = function(node, name, value)
			if not value then return cwmpError.InvalidParameterValue end

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
				return cwmpError.InvalidParameterValue
			end

			return 0
		end
	},

------------------------<< End : WANIPConnection >>------------------------

------------------------<<Start: PortMapping >>------------------------
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.1.PortMappingNumberOfEntries'] = {	-- readwrite uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local forwarding = findNode(paramRoot, 'InternetGatewayDevice.WANDevice.1.WANConnectionDevice.1.WANIPConnection.1.PortMapping')
			return forwarding:countInstanceChildren()
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.1.PortMapping'] = {
		init = function(node, name, value)

			needTriggerTemplate = false

			sort_PortMapping()
			if needTriggerTemplate == true then
				dimclient.log('debug', 'trigger nat.template for Port Mapping')
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
					dimclient.log('info', 'PortMapping init: Error Persist object does not have id, destIp = ' .. (rule.RemoteHost or '*nil*'))
				else
					dimclient.log('debug', 'PortMapping init: creating Persist Rules: destIp = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)
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

				dimclient.log('debug', 'PortMapping init: creating Dynamic Rules: destIp = ' .. (rule.RemoteHost or '*nil*') .. ', id = ' .. id)

				local instance = node:createDefaultChild(rule.instanceId)
				listOccupied[id] = true
				if id > maxInstanceId then maxInstanceId = id end
			end
			node.instance = maxInstanceId

			savedDigest = getMD5sumforPM()
			return 0
		end,
		create = function(node, name, instanceId)
			dimclient.log('info', 'createInstance: [**.PortMapping]' .. node:getPath() .. ': ' .. name .. ', ' .. instanceId)
			-- create new instance object
			local instance = node:createDefaultChild(instanceId)
			if not createDefaultPersistObject(instanceId) then return cwmpError.InternalError end
			print(node)
			return 0
		end,
		poll = portMappingWatcher
	},
-- TODO
	['**.WANConnectionDevice.1.WANIPConnection.1.PortMapping.*'] = {
		create = function(node, name, instanceId)
			dimclient.log('info', 'createInstance: [**.PortMapping.*]' .. node:getPath() .. ': ' .. name .. ', ' .. instanceId)
			-- create new instance object
			local instance = node:createDefaultChild(instanceId)
			print(node)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('debug', 'delete Instance of PortMapping Object , name = ' .. name)
			node.parent:deleteChild(node)

			print(node)
			return 0
		end,
		unset = function(node, name)
			local pathBits = name:explode('.')
			if not deletePortMapObject(pathBits[9]) then return cwmpError.InternalError end
			return 0
		end
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
			return node.value
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local objs = class:getByProperty('instanceId', pathBits[9])
			if not value then return cwmpError.InvalidParameterValue end
			for _, obj in ipairs(objs) do
				if pathBits[10] == 'PortMappingEnabled' then
					local internalBool = convertInternalBoolean(value)
					if internalBool == nil then return cwmpError.InvalidParameterValue end
					if obj.PortMappingEnabled == internalBool then return 0 end
					if internalBool == '1' then  --> from disable to enable
						addRDBPortMapping(obj)
					else
						delRDBPortMapping(obj)
					end
					obj.PortMappingEnabled = internalBool
				elseif pathBits[10] == 'PortMappingLeaseDuration' then
					if value ~= '0' then return cwmpError.InvalidParameterValue end
-- 					obj.PortMappingLeaseDuration = '0'
					return 0
				elseif pathBits[10] == 'RemoteHost' then
					if not isValidIP4(value) then return cwmpError.InvalidParameterValue end
					if obj.RemoteHost == value then return 0 end

					obj.RemoteHost = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'ExternalPort' then
					local interalInt = convertInternalInteger{input=value, minimum=1, maximum=65535}
					if interalInt == nil then return cwmpError.InvalidParameterValue end
					if value == obj.ExternalPort then return 0 end

					obj.ExternalPort = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'ExternalPortEndRange' then
					local range = obj.ExternalPortEndRange
					local interalInt = convertInternalInteger{input=value, minimum=0, maximum=65535}
					if interalInt == nil then return cwmpError.InvalidParameterValue end
					if value == range then return 0 end
					if range ~= '0' and tonumber(range) < tonumber(obj.ExternalPort) then return cwmpError.InvalidParameterValue end

					obj.ExternalPortEndRange = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'InternalPort' then
					local interalInt = convertInternalInteger{input=value, minimum=1, maximum=65535}
					if interalInt == nil then return cwmpError.InvalidParameterValue end
					if value == obj.InternalPort then return 0 end

					obj.InternalPort = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'PortMappingProtocol' then
					if value ~= 'tcp' and value ~= 'udp' and value ~= 'all' then return cwmpError.InvalidParameterValue end
					if value == obj.PortMappingProtocol then return 0 end

					obj.PortMappingProtocol = value
					updateRDBPortMapping(obj)
				elseif pathBits[10] == 'InternalClient' then
					if value ~= '' and not isValidIP4(value) then return cwmpError.InvalidParameterValue end
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
		unset = function(node, name)
			return 0
		end
	},
------------------------<< End : PortMapping >>------------------------

}
