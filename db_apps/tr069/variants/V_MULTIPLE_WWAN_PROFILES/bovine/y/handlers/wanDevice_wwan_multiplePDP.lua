-- This script is for Wireless modem with multiple PDP feature.

require("handlers.hdlerUtil")

require('Logger')  -- for test JoanOfArc's Rain
Logger.addSubsystem('wanDevice1.lua')  -- for test JoanOfArc's Rain

local subROOT = conf.topRoot .. '.WANDevice.1.'


------------------local variable----------------------------
local g_wwan_ifaceTbl = nil  -- This should be an array to support multiple pdp
local g_max_enabled_profiles = nil

local systemFS = '/sys/class/net/'

local g_rdbPrefix_profile = 'link.profile.'
local g_rdbstartIdx_profile = 1

local g_depthOfConnectionInst = 6
local g_connectionStatusTbl = {}

local g_depthOfWANIPConInst = 7
------------------------------------------------------------


------------------local function prototype------------------

------------------------------------------------------------


------------------local function definition-----------------
local getWWAN_ifTbl
local getWWAN_ifName
local rebuildAndgetWWAN_ifTbl
local readStatistics
local getMaxEnabledProf
local poll_ConnectionObj
local getIndex
local getCurrEnabledProfIdx
local isStatusUp
------------------------------------------------------------

-- This functio returns table(g_wwan_ifaceTbl) that has some information of each profile.
-- Each element includes enable, status and interface like below,
-- {enable = $(rdb_get link.profile.{i}.enable), status = $(rdb_get link.profile.{i}.status), interface = $(rdb_get link.profile.{i}.interface)}
getWWAN_ifTbl = function()
	if g_wwan_ifaceTbl then return g_wwan_ifaceTbl end

	g_wwan_ifaceTbl = {}

	for i, dev in hdlerUtil.traverseRdbVariable{prefix=g_rdbPrefix_profile , suffix='.dev' , startIdx=g_rdbstartIdx_profile} do
		dev = string.match(dev, '%a+')

		if not dev or dev ~= 'wwan' then break end

		local element = {enable = '', status = '', interface = ''}

		element.enable = string.trim(luardb.get(g_rdbPrefix_profile .. i .. '.enable'))
		element.status = string.trim(luardb.get(g_rdbPrefix_profile .. i .. '.status'))
		if element.enable == '1' and element.status == 'up' then
			element.interface = string.trim(luardb.get('link.profile.' .. i .. '.interface'))
		end

		table.insert(g_wwan_ifaceTbl, element)
	end

	return g_wwan_ifaceTbl
end

getWWAN_ifName = function(idx)
	if not idx then return '' end

	local index = tonumber(idx) or ''
	if not index or index == '' then return '' end

	local ifNameTbl = getWWAN_ifTbl()

	return ifNameTbl[index].interface or ''
end

-- Usage: This function generates indexed WWAN interface name array by force and returns the array
rebuildAndgetWWAN_ifTbl = function()
	g_wwan_ifaceTbl = nil
	return getWWAN_ifTbl()
end

cleanUpFunc = function ()
	g_wwan_ifaceTbl = nil

end

-- return Statistics(string type) of given interface
-- If the given interface is "all", return total Statistics of currently activated wwan interfaces.
readStatistics = function (interface, filename)
	local retVal = 0
	local if_name = string.trim(interface)
	local i_filename = string.trim(filename)

	if if_name == '' or not i_filename == '' then return tostring(retVal) end

	local interfaceTbl = getWWAN_ifTbl()

	for _, elem in ipairs(interfaceTbl) do
		if  elem.enable == '1' and elem.status == 'up' and (if_name == 'all' or if_name == elem.interface) then
			local systemFile = systemFS .. elem.interface .. '/statistics/' .. i_filename
			if hdlerUtil.IsRegularFile(systemFile) then
				local result = Daemon.readIntFromFile(systemFile) or 0
				retVal = retVal + result
			end
		end
	end

	return tostring(retVal)
end

-- Return the number of profiles activated at the same time by number type
getMaxEnabledProf = function ()
	if g_max_enabled_profiles then return g_max_enabled_profiles end

	local maxNum = string.trim(luardb.get('wwan.0.max_sub_if'))
	maxNum = tonumber(maxNum)
	if maxNum then
		g_max_enabled_profiles = maxNum
	else
		g_max_enabled_profiles = 2
	end
	return g_max_enabled_profiles
end

poll_ConnectionObj = function (task)
	local currTbl=getWWAN_ifTbl()
	local isSame = true

	for i, elem in ipairs(currTbl) do
		if elem.enable ~= g_connectionStatusTbl[i].enable or elem.status ~= g_connectionStatusTbl[i].status then
			isSame = false
			break
		end
	end

	if isSame then return end

	local node = task.data

	g_connectionStatusTbl = currTbl

	-- delete all of children data model object
	for _, child in ipairs(node.children) do
		if child.name ~= '0' then
			child.parent:deleteChild(child)
		end
	end

	for i, elem in ipairs(g_connectionStatusTbl) do
		if elem.enable == '1' and elem.status == 'up' then
			node:createDefaultChild(i)
		end
	end
end

getIndex = function (name, depth)
	local pathBits = name:explode('.')
	local idx = pathBits[depth]

	return idx
end

-- This will return indexed Array that has the indexes of enabled profile.
getCurrEnabledProfIdx = function ()
	local tbl = {}
	for i, dev in hdlerUtil.traverseRdbVariable{prefix=g_rdbPrefix_profile , suffix='.dev' , startIdx=g_rdbstartIdx_profile} do
		dev = string.match(dev, '%a+')

		if not dev or dev ~= 'wwan' then break end
		
		local enable = string.trim(luardb.get('link.profile.' .. i .. '.enable'))
		if enable == '1' then
			table.insert(tbl, i)
		end
	end
	return tbl
end

isStatusUp = function (instanceIdx)
	local idx = tonumber(string.trim(instanceIdx))
	if not idx then return nil end

	local interfaceTbl = getWWAN_ifTbl()
	if #interfaceTbl < 1 or not interfaceTbl[idx] then return nil end
	if interfaceTbl[idx].enable == '1' and interfaceTbl[idx].status == 'up' then
		return '1'
	end
	return '0'
end




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

---------------------- [start] WANCommonInterfaceConfig ----------------------

-- TODO:
-- type:rw
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANConnectionNumberOfEntries'] = {
		init = function(node, name, value)
			if client:isTaskQueued('cleanUp', cleanUpFunc) ~= true then
				client:addTask('cleanUp', cleanUpFunc, true) -- persistent callback function
			end
			return 0
		end,
		get = function(node, name)
			local targetName = subROOT .. 'WANConnectionDevice.'
			
			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value: Unknown
-- Available Value:
-- Involved RDB variable: wwan.0.system_network_status.network.unencoded
	[subROOT .. 'WANCommonInterfaceConfig.WANAccessProvider'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('wwan.0.system_network_status.network.unencoded')
			return 0, string.trim(retVal)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value: "Up", "Down"
-- Involved RDB variable: wwan.0.netif_udev
	[subROOT .. 'WANCommonInterfaceConfig.PhysicalLinkStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('wwan.0.netif_udev')
			retVal = string.trim(retVal)
			if retVal == '' then
				return 0, "Down"
			end
			return 0, "Up"
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANCommonInterfaceConfig.TotalBytesSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, readStatistics('all', 'tx_bytes')
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANCommonInterfaceConfig.TotalBytesReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, readStatistics('all', 'rx_bytes')
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANCommonInterfaceConfig.TotalPacketsSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, readStatistics('all', 'tx_packets')
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANCommonInterfaceConfig.TotalPacketsReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, readStatistics('all', 'rx_packets')
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANCommonInterfaceConfig.MaximumActiveConnections'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, tostring(getMaxEnabledProf())
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANCommonInterfaceConfig.NumberOfActiveConnections'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = subROOT .. 'WANCommonInterfaceConfig.Connection.'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- object:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANCommonInterfaceConfig.Connection'] = {
		init = function(node, name, value)
			-- Delete all of children first
			for _, child in ipairs(node.children) do
				if child.name ~= '0' then
					child.parent:deleteChild(child)
				end
			end

			g_connectionStatusTbl = getWWAN_ifTbl()

			for i, elem in ipairs(g_connectionStatusTbl) do
				if elem.enable == '1' and elem.status == 'up' then
					node:createDefaultChild(i)
				end
			end

			if client:isTaskQueued('preSession', poll_ConnectionObj) ~= true then
				client:addTask('preSession', poll_ConnectionObj, true, node) -- persistent callback function
			end

			return 0
		end,
	},

	[subROOT .. 'WANCommonInterfaceConfig.Connection.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfConnectionInst = #pathBits
			return 0
		end,
	},

	[subROOT .. 'WANCommonInterfaceConfig.Connection.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfConnectionInst]
			local paramName = pathBits[g_depthOfConnectionInst+1]
			local retVal = ''

			if paramName == 'ActiveConnectionDeviceContainer' then
				retVal = subROOT .. 'WANConnectionDevice.1'
			elseif paramName == 'ActiveConnectionServiceID' then
				retVal = subROOT .. 'WANConnectionDevice.1.WANIPConnection.' .. dataModelIdx
			else
				error('Dunno how to handle ' .. name)
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

---------------------- [ end ] WANCommonInterfaceConfig ----------------------


---------------------- [start] WANEthernetInterfaceConfig ----------------------

---------------------- [ end ] WANEthernetInterfaceConfig ----------------------




---------------------- [start] WANConnectionDevice ----------------------
-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANConnectionDevice.WANIPConnectionNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = subROOT .. 'WANConnectionDevice.1.WANIPConnection.'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANConnectionDevice.WANPPPConnectionNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = subROOT .. 'WANConnectionDevice.1.WANPPPConnection.'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection'] = {
		init = function(node, name, value)
			for i, dev in hdlerUtil.traverseRdbVariable{prefix=g_rdbPrefix_profile , suffix='.dev' , startIdx=g_rdbstartIdx_profile} do
				dev = string.match(dev, '%a+')

				if not dev or dev ~= 'wwan' then break end
				node:createDefaultChild(i)
			end
			return 0
		end,
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfWANIPConInst = #pathBits
			return 0
		end,
	},

-- bool:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.enable
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.Enable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local retVal = "0"
			local enable = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.enable'))

			if enable == "1" then
				retVal = "1"
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)

			if value ~= '1' and value ~= '0' then return CWMP.Error.InvalidParameterValue end

			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local curr = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.enable'))

			if curr ~= value then
				local idxTbl = getCurrEnabledProfIdx()
				if value == '1' then
					if #idxTbl >= getMaxEnabledProf() then --> disable profile that has the lowest index and if the profile is set as defaultroute, new profile is set to defaultroute.
						luardb.set(g_rdbPrefix_profile .. idxTbl[1] .. '.enable', "0")

						local defaultroute = string.trim(luardb.get(g_rdbPrefix_profile .. idxTbl[1] .. '.defaultroute'))
						if defaultroute == '1' then 
							luardb.set(g_rdbPrefix_profile .. idxTbl[1] .. '.defaultroute', '0')
							luardb.set(g_rdbPrefix_profile .. idx .. '.defaultroute', '1')
						end
					end
				else
					local defaultroute = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.defaultroute'))
					if defaultroute == '1' and #idxTbl >= 2 then --> if current profile is set as defaultroute, enabled profile that has the lowest index is set to defaultroute.
						luardb.set(g_rdbPrefix_profile .. idx .. '.defaultroute', '0')
						for i, enabledIdx in ipairs(idxTbl) do
							if idx ~= enabledIdx then
								luardb.set(g_rdbPrefix_profile .. enabledIdx .. '.defaultroute', '1')
								break
							end
						end
					end
				end
				luardb.set(g_rdbPrefix_profile .. idx .. '.enable', value)
			end
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.enable and link.profile.{i}.status
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.ConnectionStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)

			if status and status == '1' then
				return 0, "Connected"
			end
			return 0, "Disconnected"
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- TODO:
-- string:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: 
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.Name'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ""
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.usage_current [StartTime,endTime,DataReceived,DataSent] or [wwan down]
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.Uptime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)
			local uptime = 0

			if status and status == "1" then
				local usage_current = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.usage_current'))
				if usage_current == 'wwan down' then
					uptime = 0
				else
					local usageTbl = usage_current:explode(',')
					local startTime = tonumber(string.trim(usageTbl[1]))
					if not startTime then
						uptime = 0
					else
						uptime = tonumber(os.date('%s')) - startTime
					end
					
				end
			end
			return 0, tostring(uptime)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- bool:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.snat
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.NATEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local snat = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.snat'))
			
			if snat or snat == '1' then
				return 0, "1"
			end
			return 0, "0"
		end,
		set = function(node, name, value)
			value = string.trim(value)

			if value ~= '1' and value ~= '0' then return CWMP.Error.InvalidParameterValue end

			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local curr = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.snat'))

			if curr ~= value then
				luardb.set(g_rdbPrefix_profile .. idx .. '.snat', value)
			end
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.iplocal
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.ExternalIPAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)
			local externalIP = ''
			if status and status == "1" then
				externalIP = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.iplocal'))
			end
			return 0, externalIP
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: 
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.SubnetMask'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)

			if status and status == "1" then
				local wan_if = getWWAN_ifName(idx)

				if wan_if and wan_if ~= '' then
					local result = Daemon.readCommandOutput('ifconfig ' .. wan_if .. ' | grep "Mask:"')
					result = string.trim(result)

					local ret, _, mask = result:find('Mask:(%d+.%d+.%d+.%d+)')
					if ret and Parameter.Validator.isValidIP4Netmask(mask) then return 0, mask end  -- do not need to traverse anymore, it supports single PDP
				end
			end

			return 0, ""
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: 
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.DefaultGateway'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)
			local defaultroute = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.defaultroute'))

			if status and status == "1" and defaultroute == '1' then
				local wan_if = getWWAN_ifName(idx)

				if wan_if and wan_if ~= '' then
					local result = Daemon.readCommandOutput('route -n')
					result = string.trim(result)

					local ret, _, gateway = result:find('0.0.0.0%s+(%d+.%d+.%d+.%d+)%s+0.0.0.0%s+.*' .. wan_if)
					if ret and Parameter.Validator.isValidIP4(gateway) then return 0, gateway end  -- do not need to traverse anymore, it supports single PDP
				end
			end

			return 0, ""
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: 
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.DNSServers'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)

			if status and status == "1" then
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
			end

			return 0, ""
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: 
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.MaxMTUSize'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)

			if status and status == "1" then
				local wan_if = getWWAN_ifName(idx)

				local filename = systemFS .. wan_if .. '/mtu'

				if hdlerUtil.IsRegularFile(filename) then
					local result = Daemon.readIntFromFile(filename)
					return 0, tostring(result)
				end
			end

			return 0, "0"
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)

			if status and status == "1" then
				local fmtInt = hdlerUtil.ToInternalInteger{input=value, minimum=1, maximum=1500}
				if not fmtInt then return CWMP.Error.InvalidParameterValue end

				local wan_if = getWWAN_ifName(idx)

				if wan_if and wan_if ~= '' then
					os.execute('ifconfig ' .. wan_if .. ' mtu ' .. fmtInt)
					
					local plmn_mcc = string.trim(luardb.get('wwan.0.imsi.plmn_mcc'))
					local plmn_mnc = string.trim(luardb.get('wwan.0.imsi.plmn_mnc'))
					plmn_mcc = tonumber(plmn_mcc)
					plmn_mnc = tonumber(plmn_mnc)
					
					if plmn_mcc and plmn_mnc and plmn_mcc == 505 and plmn_mnc == 1 then
						luardb.set('system.config.telstra.mtu', fmtInt)
					else
						luardb.set('system.config.mtu', fmtInt)
					end
					return 0
				end
			else
				return CWMP.Error.InvalidParameterValue, "The instance is not enabled"
			end

			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: 
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.MACAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)

			if status and status == "1" then
				local wan_if = getWWAN_ifName(idx)

				local filename = systemFS .. wan_if .. '/address'

				if hdlerUtil.IsRegularFile(filename) then
					local result = Daemon.readStringFromFile(filename)
					return 0, string.upper(result)
				end
			end
			return 0, ""
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readwrite
-- Default Value: Off
-- Available Value: [Off|RIPv1|RIPv2]
-- Involved RDB variable: * service.router.rip.enable [1|2]
--			* service.router.rip.version [1|2]
--			* service.router.rip.interface [lan,wwan0] (comma-separated network interface list)
--			* service.router.rip.trigger ==> template trigger
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.RouteProtocolRx'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)
			local defaultroute = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.defaultroute'))
			local retVal = "Off"

			if status and status == "1" and defaultroute == '1' then
				local enable = string.trim(luardb.get('service.router.rip.enable'))
				local version = string.trim(luardb.get('service.router.rip.version'))
				local interface = string.trim(luardb.get('service.router.rip.interface'))

				if enable == '1' and string.find(interface, "wwan0") then
					if version == "1" then
						retVal = "RIPv1"
					elseif version == "2" then
						retVal = "RIPv2"
					end
				end
			end
			return 0, retVal
		end,
--[[
* set to off
	I. enabled
		1. only lan			==> nothing
		2. only wwan0			==> to disable and no trigger
		3. others with wwan0		==> take out wwan0 and trigger
		4. others with no wwan0		==> nothing
	II. disabled				==> nothing
	
* set to RIPv1
	I. enabled
	    version 1
		1. only lan			==> add wwan0 and trigger
		2. only wwan0			==> nothing
		3. others with wwan0		==> nothing
		4. others with no wwan0		==> add wwan0 and trigger
	    version 2
		1. only lan			==> add wwan0, set to version 1 and trigger
		2. only wwan0			==> set to version 1 and trigger
		3. others with wwan0		==> set to version 1 and trigger
		4. others with no wwan0		==> add wwan0, set to version 1 and trigger
	II. disabled				==> set wwan0, set version 1 and set enable

* set to RIPv2
	I. enabled
	    version 1
		1. only lan			==> add wwan0, set to version 2 and trigger
		2. only wwan0			==> set to version 2 and trigger
		3. others with wwan0		==> set to version 2 and trigger
		4. others with no wwan0		==> add wwan0, set to version 2 and trigger
	    version 2
		1. only lan			==> add wwan0 and trigger
		2. only wwan0			==> nothing
		3. others with wwan0		==> nothing
		4. others with no wwan0		==> add wwan0 and trigger
	II. disabled				==> set wwan0, set version 2 and set enable
--]]
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local status = isStatusUp(idx)
			local defaultroute = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.defaultroute'))

			if status and status == "1" and defaultroute == '1' then
				value = string.trim(value)
				if value ~= "Off" and value ~= "RIPv1" and value ~= "RIPv2" then
					return CWMP.Error.InvalidParameterValue
				end

				local curr_enable = string.trim(luardb.get('service.router.rip.enable'))
				local curr_version = string.trim(luardb.get('service.router.rip.version'))
				local curr_interface = string.trim(luardb.get('service.router.rip.interface'))
				local curr_ifaceTbl = string.explode(curr_interface, ',')

				local new_enable = ''
				local new_version = ''
				local new_interface = ''
				local need_to_trigger = false

				local nameOfwwan = 'wwan0'

				function has_interface(ifaceTbl, ifaceName)
					local index = nil
					local hasIt = false
					local only = false

					index = table.find(ifaceTbl, ifaceName)
					if index then
						hasIt = true
						if #ifaceTbl == 1 then
							only = true
						end
					end
					
					return hasIt, index, only
				end

				function delete_interface(ifaceTbl, idx)
					if idx and tonumber(idx) then
						table.remove(ifaceTbl, idx)
					end
					return table.concat(ifaceTbl, ',')
				end

				function add_interface(ifaceTbl, ifaceName)
					if ifaceName then
						table.insert(ifaceTbl, ifaceName)
					end
					return table.concat(ifaceTbl, ',')
				end

				local has_iface, idx_iface, only_iface = has_interface(curr_ifaceTbl, nameOfwwan)

				if value == "Off" then
					if curr_enable == '1' then
						if has_iface then
							if only_iface then
								new_enable = '0'
							else
								new_interface = delete_interface(curr_ifaceTbl, idx_iface)
								need_to_trigger = true
							end
						end
					end
				else
					local setVersion = (value == "RIPv1") and "1" or "2"
					if curr_enable == '1' then
						if curr_version == setVersion then
							if not has_iface then
								new_interface = add_interface(curr_ifaceTbl, nameOfwwan)
								need_to_trigger = true
							end
						elseif curr_version ~= setVersion then
							if not has_iface then
								new_interface = add_interface(curr_ifaceTbl, nameOfwwan)
							end
							new_version = setVersion
							need_to_trigger = true
						end
					else
						new_enable = '1'
						new_version = setVersion
						if not has_iface then
							new_interface = add_interface(curr_ifaceTbl, nameOfwwan)
						end
					end
				end
				
				if new_version ~= '' and new_version ~= curr_version then
					luardb.set('service.router.rip.version', new_version)
				end

				if new_interface ~= '' and new_interface ~= curr_interface then
					luardb.set('service.router.rip.interface', new_interface)
				end

				if new_enable ~= '' and new_enable ~= curr_enable then
					luardb.set('service.router.rip.enable', new_enable)
				end

				if need_to_trigger then
					luardb.set('service.router.rip.trigger', '1')
				end
			end
			return 0
		end
	},

---------------------- [ end ] WANConnectionDevice ----------------------






---------------------- [start] X_NETCOMM_APNProfile ----------------------
-- string:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.name
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.ProfileName'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local profilename = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.name'))
			return 0, profilename
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)
			luardb.set(g_rdbPrefix_profile .. idx .. '.name', value)
			return 0
		end
	},

-- string:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.apn
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.APN'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local apn = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.apn'))
			return 0, apn
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)
			luardb.set(g_rdbPrefix_profile .. idx .. '.apn', value)
			return 0
		end
	},

-- string:readwrite
-- Default Value: CHAP
-- Available Value: CHAP|PAP
-- Involved RDB variable: link.profile.{i}.auth_type
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.AuthenticationType'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local auth_type = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.auth_type'))
			return 0, string.upper(auth_type)

		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)
			value = string.lower(value)

			if value ~= "chap" and value ~= "pap" then return CWMP.Error.InvalidParameterValue end

			luardb.set(g_rdbPrefix_profile .. idx .. '.auth_type', value)
			return 0
		end
	},

-- string:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.user
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.UserName'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local username = luardb.get(g_rdbPrefix_profile .. idx .. '.user')
			if not username then username = '' end
			return 0, username
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			luardb.set(g_rdbPrefix_profile .. idx .. '.user', value)
			return 0
		end
	},

-- string:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: link.profile.{i}.pass
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.Password'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local password = luardb.get(g_rdbPrefix_profile .. idx .. '.pass')
			if not password then password = '' end
			return 0, password
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			luardb.set(g_rdbPrefix_profile .. idx .. '.pass', value)
			return 0
		end
	},

-- uint:readwrite
-- Default Value: 30
-- Available Value: 30~65535 (unit: seconds)
-- Involved RDB variable: link.profile.{i}.reconnect_delay
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.ReconnectDelay'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local reconnect_delay = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.reconnect_delay'))
			numtype = tonumber(reconnect_delay)
			if not numtype or numtype < 30 or numtype > 65535 then reconnect_delay = "30" end
			return 0, reconnect_delay
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)
			local numtype = tonumber(value)
			if not numtype or numtype < 30 or numtype > 65535 then return CWMP.Error.InvalidParameterValue end
			luardb.set(g_rdbPrefix_profile .. idx .. '.reconnect_delay', value)
			return 0
		end
	},

-- uint:readwrite
-- Default Value: 0
-- Available Value: 0~65535, 0=Unlimited 
-- Involved RDB variable: link.profile.{i}.reconnect_retries
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.ReconnectRetries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local reconnect_retries = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.reconnect_retries'))
			numtype = tonumber(reconnect_retries)
			if not numtype or numtype < 0 or numtype > 65535 then reconnect_retries = "0" end
			return 0, reconnect_retries
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)
			local numtype = tonumber(value)
			if not numtype or numtype < 0 or numtype > 65535 then return CWMP.Error.InvalidParameterValue end
			luardb.set(g_rdbPrefix_profile .. idx .. '.reconnect_retries', value)
			return 0
		end
	},

-- uint:readwrite
-- Default Value: 20
-- Available Value:  0~65535
-- Involved RDB variable: link.profile.{i}.defaultroutemetric
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.InterfaceMetric'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local defaultroutemetric = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.defaultroutemetric'))
			numtype = tonumber(defaultroutemetric)
			if not numtype or numtype < 0 or numtype > 65535 then defaultroutemetric = "20" end
			return 0, defaultroutemetric
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)
			local numtype = tonumber(value)
			if not numtype or numtype < 0 or numtype > 65535 then return CWMP.Error.InvalidParameterValue end
			luardb.set(g_rdbPrefix_profile .. idx .. '.defaultroutemetric', value)
			return 0
		end
	},

-- bool:readwrite
-- Default Value: 
-- Available Value:
-- Involved RDB variable: link.profile.{i}.autoapn
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.AutoAPN'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local autoapn = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.autoapn'))

			if autoapn == '1' then return 0, "1" end

			return 0, "0"
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)

			if value ~= '1' and value ~= '0' then return CWMP.Error.InvalidParameterValue end

			local curr_autoapn = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.autoapn'))

			if curr_autoapn ~= value then
				luardb.set(g_rdbPrefix_profile .. idx .. '.autoapn', value)
			end
			return 0
		end
	},

-- string:readwrite
-- Default Value: 
-- Available Value: Network_address/Network_mask
-- Involved RDB variable: link.profile.{i}.routes
	[subROOT .. 'WANConnectionDevice.1.WANIPConnection.*.X_NETCOMM_APNProfile.RoutingSettings'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local routes = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.routes'))

			if routes == '' then return 0, '/' end

			return 0, routes
		end,
		set = function(node, name, value)
			local idx = getIndex(name, g_depthOfWANIPConInst)
			if not idx then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			value = string.trim(value)

			if value == '/' then
				luardb.set(g_rdbPrefix_profile .. idx .. '.routes', value)
				return 0
			end

			local routes = value:explode('/')
			
			if not routes or #routes ~= 2 then return CWMP.Error.InvalidParameterValue, "Invalid Format" end

			routes[1] = string.trim(routes[1])
			routes[2] = string.trim(routes[2])

			if not Parameter.Validator.isValidIP4(routes[1]) or not Parameter.Validator.isValidIP4Netmask(routes[2]) then
				return CWMP.Error.InvalidParameterValue, "Invalid Format"
			end

			local curr_routes = string.trim(luardb.get(g_rdbPrefix_profile .. idx .. '.routes'))
			value = table.concat(routes, '/')
			if curr_routes ~= value then
				luardb.set(g_rdbPrefix_profile .. idx .. '.routes', value)
			end
			return 0
		end
	},


---------------------- [ end ] X_NETCOMM_APNProfile ----------------------



















}
