require("handlers.hdlerUtil")
require('Parameter.Validator')

require("Logger")
Logger.addSubsystem('LANHostConfigManagement')

local subROOT = conf.topRoot .. '.LANDevice.*.LANHostConfigManagement.'
local g_depthOfDevice = 3

------------------local variable----------------------------
local lan_iface = 'eth0'
local systemFS = '/sys/class/net/'

-- [start] for DHCPStaticAddress Object --
local g_depthOfInstance= 6
local g_suffixRdb=''

local g_NumOfDataModelObjInst = 0

local g_DHCPReservedAddrList = nil
-- [ end ] for DHCPStaticAddress Object --

------------------------------------------------------------

------------------local function prototype------------------
local get_NumOfRDBInst
local poller
local addRDBObjInst
local getDHCPReservedAddrList
local clearDHCPReservedAddrList
local setDHCPStaticAddressList
local triggerDHCPStaticAddressList
local getIndex
local getDHCPSettingsRDBName
local getAddressSettingsRDBName
------------------------------------------------------------

------------------local function definition------------------
getDHCPSettingsRDBName = function (name)
	-- default to 'service.dhcp.' if name is nil
	if not name then Logger.log('Daemon', 'error', 'name is nil'); return "service.dhcp."; end

	local pathBits = name:explode('.')
	local index = pathBits[g_depthOfDevice]

	if index == 1 then return "service.dhcp." end
	return "vlan." .. index-2 .. ".dhcp."
end

getAddressSettingsRDBName = function (name)
	-- default to 'link.profile.0.' if name is nil
	if not name then Logger.log('Daemon', 'error', 'name is nil'); return "link.profile.0."; end

	local pathBits = name:explode('.')
	local index = pathBits[g_depthOfDevice]

	if index == "1" then return "link.profile.0." end
	return "vlan." .. index-2 .. "."
end

-- [start] for DHCPStaticAddress Object --
get_NumOfRDBInst = function (name)
	count = 0
	local indexedPrefix = getDHCPSettingsRDBName(name) .. 'static.'
	for i, value in hdlerUtil.traverseRdbVariable{prefix=indexedPrefix , suffix=g_suffixRdb , startIdx=0} do
		value = string.trim(value)
		if not value or value == '' then break end
		count = count +1
	end
	return count
end

poller = function (task)
	local node = task.data
	local name = node:getPath(true) or node.name

	local currNumOf = get_NumOfRDBInst(name)

	if g_NumOfDataModelObjInst == currNumOf then return end

	g_NumOfDataModelObjInst = currNumOf

	local node = task.data

	if not node or not node.children then
		Logger.log('LANHostConfigManagement', 'error', 'Error in DHCPStaticAddress poller: invalid node obj')
		return
	end

	-- delete all of children data model object
	for _, child in ipairs(node.children) do
		if child.name ~= '0' then
			child.parent:deleteChild(child)
		end
	end

	for i=1, currNumOf do
		node:createDefaultChild(i)
	end

	node.instance = currNumOf

end

addRDBObjInst = function (name)
	local currNumOfRdb = get_NumOfRDBInst(name)

	luardb.set(getDHCPSettingsRDBName(name) .. 'static.' .. currNumOfRdb, ',00:00:00:00:00:00,0.0.0.0,disabled')
	luardb.set(getDHCPSettingsRDBName(name) .. 'static.' .. (currNumOfRdb+1), '')

	g_DHCPReservedAddrList = nil
	getDHCPReservedAddrList(name)

	return currNumOfRdb + 1
end

delRDBObjInst = function (dataModelObjIdx, name)
	if not tonumber(dataModelObjIdx) then return nil end

	local rdbIndex = tonumber(dataModelObjIdx) - 1

	if rdbIndex < 0 then return nil end

	local currNumOf = get_NumOfRDBInst(name)

	if currNumOf == 0 or (currNumOf -1) < rdbIndex then return currNumOf end

	local currTbl = getDHCPReservedAddrList(name)

	for i=0, (currNumOf-1) do
		if rdbIndex <= i then
			local nextContent = luardb.get(getDHCPSettingsRDBName(name) .. 'static.' .. (i+1)) or ''
			luardb.set(getDHCPSettingsRDBName(name) .. 'static.' .. i, nextContent)
		end
	end

-- 	luardb.unset(getDHCPSettingsRDBName(name) .. 'static.' .. currNumOf)
-- 	luardb.unset(getDHCPSettingsRDBName(name) .. 'static.' .. (currNumOf-1))

	if currTbl[tonumber(dataModelObjIdx)].status == 'enabled' then
		if client:isTaskQueued('postSession', triggerDHCPStaticAddressList, name) ~= true then
			client:addTask('postSession', triggerDHCPStaticAddressList, false, name)
		end
	end

	g_DHCPReservedAddrList = nil
	getDHCPReservedAddrList(name)

	return get_NumOfRDBInst(name)
end

getDHCPReservedAddrList = function(name)
	if g_DHCPReservedAddrList then return g_DHCPReservedAddrList end

	g_DHCPReservedAddrList = {}

	for _, addrInfo in hdlerUtil.traverseRdbVariable{prefix=getDHCPSettingsRDBName(name) .. 'static.', startIdx=0} do
		addrInfo = string.trim(addrInfo)
		if addrInfo == '' then break end

		local tempTbl = {}
		tempTbl = string.explode(addrInfo, ',')
		if #tempTbl < 4 then break end

		tempTbl.name = string.trim(tempTbl[1])
		tempTbl.mac_addr = string.trim(tempTbl[2])
		tempTbl.ip_addr = string.trim(tempTbl[3])
		tempTbl.status = string.trim(tempTbl[4])

		table.insert(g_DHCPReservedAddrList, tempTbl)
	end
	return g_DHCPReservedAddrList
end

clearDHCPReservedAddrList = function ()
	g_DHCPReservedAddrList = nil
end

-- service.dhcp.static.{i}  or vlan.*.dhcp.static.{i} [Ex: name,mac_addr,ip_addr,status(enabled|disabled)]
setDHCPStaticAddressList =  function (index, element, value, name)
	if not tonumber(index) or not element or not value then return nil end

	if not g_DHCPReservedAddrList[index] then return nil end

	g_DHCPReservedAddrList[index][element] = value
	local curVal = g_DHCPReservedAddrList[index]

	local setVal = string.format('%s,%s,%s,%s', curVal.name, curVal.mac_addr, curVal.ip_addr, curVal.status)

	luardb.set(getDHCPSettingsRDBName(name) .. 'static.' .. (index -1), setVal)

	if client:isTaskQueued('postSession', triggerDHCPStaticAddressList, name) ~= true then
		client:addTask('postSession', triggerDHCPStaticAddressList, false, name)
	end
end

triggerDHCPStaticAddressList = function (task)
	local name = task.data
	luardb.set(getDHCPSettingsRDBName(name) .. 'static.trigger', '1')
end
-- [ end ] for DHCPStaticAddress Object --

------------------------------------------------------------

return {
--[[
-- type:rw
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. ''] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ""
		end,
		set = function(node, name, value)
			return 0
		end
	},
--]]

-- bool:readwrite
-- Default Value: 1
-- Available Value: 1|0
-- Involved RDB variable: service.dhcp.enable or vlan.*.dhcp.enable
	[subROOT .. 'DHCPServerEnable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get(getDHCPSettingsRDBName(name) .. "enable");

			if not retVal or ( retVal ~= "1" and retVal ~= "0" ) then return 0, "1" end
			return 0, retVal
		end,
		set = function(node, name, value)
			if not value or ( value ~= "1" and value ~= "0" ) then return CWMP.Error.InvalidParameterValue end
			local currRelayMode = luardb.get(getDHCPSettingsRDBName(name) .. "relay.0")
			local currDhcpMode =  luardb.get(getDHCPSettingsRDBName(name) .. "enable")

			if currDhcpMode and currDhcpMode == value then return 0 end

			if value == "1" and currRelayMode and currRelayMode == "1" then
				luardb.set(getDHCPSettingsRDBName(name) .. "relay.0", "0")
			end

			luardb.set(getDHCPSettingsRDBName(name) .. "enable", value)
			return 0
		end
	},

-- bool:readwrite
-- Default Value: 0
-- Available Value: 1|0
-- Involved RDB variable: service.dhcp.relay.0 or vlan.*.dhcp.relay.0
	[subROOT .. 'DHCPRelay'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get(getDHCPSettingsRDBName(name) .. "relay.0");

			if not retVal or ( retVal ~= "1" and retVal ~= "0" ) then return 0, "0" end
			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "1" and value ~= "0" then return CWMP.Error.InvalidParameterValue end
			local currRelayMode = luardb.get(getDHCPSettingsRDBName(name) .. "relay.0")
			local currDhcpMode =  luardb.get(getDHCPSettingsRDBName(name) .. "enable")

			if currRelayMode and currRelayMode == value then return 0 end

			if value == "1" and currDhcpMode and currDhcpMode == "1" then
				luardb.set(getDHCPSettingsRDBName(name) .. "enable", "0")
			end

			luardb.set(getDHCPSettingsRDBName(name) .. "relay.0", value)
			return 0
		end
	},

-- string:readwrite
-- Default Value: ""
-- Available Value: IP Address
-- Involved RDB variable: service.dhcp.relay.server.0 or vlan.*.dhcp.relay.server.0
	[subROOT .. 'X_NETCOMM_DHCPRelayAddr'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get(getDHCPSettingsRDBName(name) .. "relay.server.0");
			if not retVal or not Parameter.Validator.isValidIP4(retVal) then return 0, "" end

			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not value or Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
			luardb.set(getDHCPSettingsRDBName(name) .. "relay.server.0", value)
			return 0
		end
	},

-- string:readwrite
-- Default Value:
-- Available Value: IP Address
-- Involved RDB variable: service.dhcp.range.0 or vlan.*.dhcp.range.0 [Comma-separated min and max addresses- 192.168.1.100,192.168.1.244]
	[subROOT .. 'MinAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get(getDHCPSettingsRDBName(name) .. 'range.0')
			if not retVal then return 0, "" end

			local addresses = retVal:explode(',')

			if not addresses or #addresses ~= 2 then return CWMP.Error.InternalError, 'Invalid address range [' .. retVal .. ']' end

			if not Parameter.Validator.isValidIP4(addresses[1]) then return 0, "" end
			return 0, addresses[1]

		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not value or not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
			local addresses = luardb.get(getDHCPSettingsRDBName(name) .. 'range.0')
			addresses = addresses:explode(',')
			if not addresses or #addresses ~= 2 then return CWMP.Error.InternalError end
			if addresses[1] ~= value then
				addresses[1] = value
				luardb.set(getDHCPSettingsRDBName(name) .. 'range.0', table.concat(addresses, ','))
			end
			return 0
		end
	},

-- string:readwrite
-- Default Value:
-- Available Value: IP Address
-- Involved RDB variable: service.dhcp.range.0 or vlan.*.dhcp.range.0 [Comma-separated min and max addresses- 192.168.1.100,192.168.1.244]
	[subROOT .. 'MaxAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get(getDHCPSettingsRDBName(name) .. 'range.0')
			if not retVal then return 0, "" end

			local addresses = retVal:explode(',')

			if not addresses or #addresses ~= 2 then return CWMP.Error.InternalError, 'Invalid address range [' .. retVal .. ']' end

			if not Parameter.Validator.isValidIP4(addresses[2]) then return 0, "" end
			return 0, addresses[2]
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not value or not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
			local addresses = luardb.get(getDHCPSettingsRDBName(name) .. 'range.0')
			addresses = addresses:explode(',')
			if not addresses or #addresses ~= 2 then return CWMP.Error.InternalError end
			if addresses[2] ~= value then
				addresses[2] = value
				luardb.set(getDHCPSettingsRDBName(name) .. 'range.0', table.concat(addresses, ','))
			end
			return 0
		end
	},

-- string:readonly - Comma-separated list of addresses
-- Default Value:
-- Available Value:
-- Involved RDB variable: service.dhcp.static.{i} or vlan.*.dhcp.static.{i} (Ex: name,mac_addr,ip_addr,enabled|disabled)
--			need trigger to apply changes (service.dhcp.static.trigger)
	[subROOT .. 'ReservedAddresses'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local addrList = {}

			for _, addrInfo in hdlerUtil.traverseRdbVariable{prefix=getDHCPSettingsRDBName(name) .. 'static.', startIdx=0} do
				local infoTbl = addrInfo:explode(',')
				if infoTbl[3] and Parameter.Validator.isValidIP4(infoTbl[3]) then
					table.insert(addrList, infoTbl[3])
				end
			end
			return 0, table.concat(addrList, ',')
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readwrite
-- Default Value: 255.255.255.0
-- Available Value: Network Mask
-- Involved RDB variable: link.profile.0.netmask or vlan.*.netmask
	[subROOT .. 'SubnetMask'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local netMask=luardb.get(getAddressSettingsRDBName(name) .. 'netmask')
			if not Parameter.Validator.isValidIP4Netmask(netMask) then return 0, '' end
			return 0, netMask
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not vakye or not Parameter.Validator.isValidIP4Netmask(value) then return CWMP.Error.InvalidParameterValue end
			local currVal = luardb.get(getAddressSettingsRDBName(name) .. 'netmask')
			if currVal and currVal == value then return 0 end

			luardb.set(getAddressSettingsRDBName(name) .. 'netmask', value)
			return 0
		end
	},

-- string:readwrite - Comma-separated list of DNS servers offered to DHCP clients(DNS1,DNS2)
-- Default Value: ""
-- Available Value: support at most two DNS servers
-- Involved RDB variable: "service.dhcp.dns1.0" and "service.dhcp.dns2.0" or "vlan.*.dhcp.dns1.0" and "vlan.*.dhcp.dns2.0"
	[subROOT .. 'DNSServers'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local addrList = {}

			for _, address in hdlerUtil.traverseRdbVariable{prefix=getDHCPSettingsRDBName(name) .. 'dns', suffix='.0', startIdx=1} do
				if Parameter.Validator.isValidIP4(address) then
					table.insert(addrList, address)
				else
					table.insert(addrList, '0.0.0.0')
				end
			end
			return 0, table.concat(addrList, ',')
		end,
		set = function(node, name, value)
			local numOfDNS = 2
			if not value then return CWMP.Error.InvalidParameterValue end

			local addrList = value:explode(',')
			if #addrList > numOfDNS then  return CWMP.Error.InvalidParameterValue, "Support at most " .. numOfDNS .. " DNS servers" end

			for i=1, numOfDNS do
				local dns_addr = string.trim(addrList[i])
				if dns_addr == '' then dns_addr = '0.0.0.0' end

				local currVal = luardb.get(getDHCPSettingsRDBName(name) .. 'dns' .. i .. '.0')
				if currVal ~= dns_addr then
					luardb.set (getDHCPSettingsRDBName(name) .. 'dns' .. i .. '.0', dns_addr)
				end

			end
			return 0
		end
	},

-- string:readwrite
-- Default Value: ""
-- Available Value:
-- Involved RDB variable: service.dhcp.suffix.0 or vlan.*.dhcp.suffix.0
	[subROOT .. 'DomainName'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get(getDHCPSettingsRDBName(name) .. 'suffix.0')
			return 0, retVal or ''
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidParameterValue end

			local currVal = luardb.get(getDHCPSettingsRDBName(name) .. 'suffix.0')
			if currVal ~= value then
				luardb.set(getDHCPSettingsRDBName(name) .. 'suffix.0', value)
			end
			return 0
		end
	},

-- string:readwrite
-- Default Value:
-- Available Value: IP Address
-- Involved RDB variable: link.profile.0.address or vlan.*.address
	[subROOT .. 'IPRouters'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, luardb.get(getAddressSettingsRDBName(name) .. 'address') or ''
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidParameterValue end

			local addrList = value:explode(',')
			local setVal = string.trim(addrList[1])

			if not Parameter.Validator.isValidIP4(setVal) then return CWMP.Error.InvalidParameterValue end

			local currVal = luardb.get(getAddressSettingsRDBName(name) .. 'address')

			if currVal ~= setVal then
				luardb.set(getAddressSettingsRDBName(name) .. 'address', setVal)
			end
			return 0
		end
	},

-- int:readwrite
-- Default Value: 86400
-- Available Value:
-- Involved RDB variable: service.dhcp.lease.0 or vlan.*.dhcp.lease.0
	[subROOT .. 'DHCPLeaseTime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get(getDHCPSettingsRDBName(name) .. 'lease.0')
			retVal = string.trim(retVal)
			if retVal == '' or not tonumber(retVal) then retVal = "86400" end
			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			local integerV = hdlerUtil.ToInternalInteger{input=value, minimum=0}
			if not integerV then return CWMP.Error.InvalidParameterValue end

			local currVal = luardb.get(getDHCPSettingsRDBName(name) .. 'lease.0')
			currVal = string.trim(currVal)
			if currVal == value then return 0 end
			luardb.set(getDHCPSettingsRDBName(name) .. 'lease.0', value)
			return 0
		end
	},

-- bool:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'IPInterface.1.Enable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
--[[
-- "/sys/class/net/[interface]/operstate" on ntc_8000 variants does not work properly. So used ifconfig instead of it.
			local contactPoint = systemFS .. lan_iface .. '/operstate'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local status = Daemon.readStringFromFile(contactPoint)
				status = string.trim(status)
				if status == 'up' then
					return 0, "1"
				end
			end
			return 0, "0"
--]]

			local result = os.execute('ifconfig ' .. lan_iface .. ' 2>/dev/null | grep -qi "UP "')

			if result == 0 then return 0, "1" end

			return 0, "0"
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= '1' and value ~= '0' then return CWMP.Error.InvalidParameterValue end

			if value == '1' then
				value = 'up'
			else

				value = 'down'
			end

			local result = os.execute('ifconfig ' .. lan_iface .. ' ' .. value)
			if result ~= 0 then
				Logger.log('LANHostConfigManagement', 'error', 'ERROR!!: Failure on [' .. 'ifconfig ' .. lan_iface .. ' ' .. value .. ']')
			end
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'DHCPStaticAddressNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local count = get_NumOfRDBInst(name)

			return 0, tostring(count)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- object:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'DHCPStaticAddress'] = {
		init = function(node, name, value)
			node:setAccess('readwrite')

			local numOfInst = get_NumOfRDBInst(name)

			for i=1, numOfInst do
				node:createDefaultChild(i)
			end

			node.instance = numOfInst

			g_NumOfDataModelObjInst = numOfInst

			if client:isTaskQueued('preSession', poller, node) ~= true then
				client:addTask('preSession', poller, true, node) -- persistent callback function
			end

			if client:isTaskQueued('cleanUp', clearDHCPReservedAddrList) ~= true then
				client:addTask('cleanUp', clearDHCPReservedAddrList, true) -- persistent callback function
			end
			return 0
		end,
		create = function(node, name)
			local dataModelIdx = addRDBObjInst(name)

			if not dataModelIdx then
				Logger.log('LANHostConfigManagement', 'info', 'Fail to create RDB Ojbec instance')
				return CWMP.Error.InternalError
			end

			g_NumOfDataModelObjInst = dataModelIdx

			-- create new data model instance object
			node:createDefaultChild(dataModelIdx)
			return 0, dataModelIdx
		end,
	},

-- type:rw
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'DHCPStaticAddress.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfInstance = #pathBits
			return 0
		end,
		delete = function(node, name)
			Logger.log('LANHostConfigManagement', 'info', 'deleteInstance: [**LANHostConfigManagement.DHCPStaticAddress.*], name = ' .. name)

			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfInstance]

			local numOfDataModelInst = delRDBObjInst(dataModelIdx)
			if numOfDataModelInst then

				for _, child in ipairs(node.parent.children) do
					if child.name == tostring(g_NumOfDataModelObjInst) then
						child.parent:deleteChild(child)
						break;
					end
				end
				g_NumOfDataModelObjInst  = dataModelIdx
			end
			return 0
		end,
	},

-- type:rw
-- Default Value:
-- Available Value:
-- Involved RDB variable: service.dhcp.static.{0} or vlan.*.dhcp.static.{0}
-- element of g_DHCPReservedAddrList[i] --> {name, mac_addr, ip_addr, status(enabled|disabled)}
	[subROOT .. 'DHCPStaticAddress.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfInstance]
			local paramName = pathBits[g_depthOfInstance+1]
			local retVal = ''
			local infoTbl = getDHCPReservedAddrList(name)

			if dataModelIdx then dataModelIdx = tonumber(dataModelIdx) end
			if not dataModelIdx or not infoTbl[dataModelIdx] then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			if paramName == 'Enable' then	-- status
				if infoTbl[dataModelIdx].status and infoTbl[dataModelIdx].status == 'enabled' then
					retVal = '1'
				else
					retVal = '0'
				end
			elseif paramName == 'Chaddr' then	-- mac address
				if infoTbl[dataModelIdx].mac_addr then
					retVal = string.upper(infoTbl[dataModelIdx].mac_addr)
				else
					retVal='00:00:00:00:00:00'
				end
			elseif paramName == 'Yiaddr' then	-- ip address
				if infoTbl[dataModelIdx].ip_addr and Parameter.Validator.isValidIP4(infoTbl[dataModelIdx].ip_addr) then
					retVal = infoTbl[dataModelIdx].ip_addr
				else
					retVal='0.0.0.0'
				end
			elseif paramName == 'X_NETCOMM_ComputerName' then	-- name
				if infoTbl[dataModelIdx].name then
					retVal = infoTbl[dataModelIdx].name
				else
					retVal=''
				end
			else
				error('Dunno how to handle ' .. name)
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfInstance]
			local paramName = pathBits[g_depthOfInstance+1]
			local setValue = ''
			local infoTbl = getDHCPReservedAddrList(name)

			if dataModelIdx then dataModelIdx = tonumber(dataModelIdx) end
			if not dataModelIdx or not infoTbl[dataModelIdx] then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)

			if paramName == 'Enable' then	-- status
				if value ~= '0' and value ~= '1' then return CWMP.Error.InvalidParameterValue end
				if value == '1' then
					setValue = 'enabled'
				else
					setValue = 'disabled'
				end
				if not infoTbl[dataModelIdx].status or infoTbl[dataModelIdx].status ~= setValue then
					setDHCPStaticAddressList(dataModelIdx, 'status', setValue, name);
				end
			elseif paramName == 'Chaddr' then	-- mac address
				setValue = string.upper(value)
				if not string.match(setValue, '^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$') then
					return CWMP.Error.InvalidParameterValue, "Invalid MAC Address: [" .. setValue .. ']'
				end
				if not infoTbl[dataModelIdx].mac_addr or infoTbl[dataModelIdx].mac_addr ~= setValue then
					setDHCPStaticAddressList(dataModelIdx, 'mac_addr', setValue, name);
				end
			elseif paramName == 'Yiaddr' then	-- ip address
				setValue = value
				if not Parameter.Validator.isValidIP4(setValue) then return CWMP.Error.InvalidParameterValue, "Invalid IP Address: [" .. setValue .. ']' end

				if not infoTbl[dataModelIdx].ip_addr or  infoTbl[dataModelIdx].ip_addr ~= setValue then
					setDHCPStaticAddressList(dataModelIdx, 'ip_addr', setValue, name);
				end
			elseif paramName == 'X_NETCOMM_ComputerName' then	-- name
				setValue = value

				if not infoTbl[dataModelIdx].name or infoTbl[dataModelIdx].name ~= setValue then
					setDHCPStaticAddressList(dataModelIdx, 'name', setValue, name);
				end
			else
				error('Dunno how to handle ' .. name)
			end

			return 0
		end
	},
}
