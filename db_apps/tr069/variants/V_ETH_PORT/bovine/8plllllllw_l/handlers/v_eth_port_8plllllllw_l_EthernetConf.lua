require("handlers.hdlerUtil")

require("Logger")
Logger.addSubsystem('LANEthernetInterfaceConfig')


local subROOT = conf.topRoot .. '.LANDevice.1.'


------------------local variable----------------------------
local systemFS='/sys/class/net/'
local lan_iface = 'eth0'


-- [start] for X_NETCOMM_Switch Object --
local g_depthOfSwitchInstance= 7

local g_rdbSwitchPrefix='hw.switch.port.'
local g_rdbSwitchStatusSuffix='.status'

local g_NumOfDataModelObjInst = 0
-- [ end ] for X_NETCOMM_Switch Object --
------------------------------------------------------------

------------------local function prototype------------------
local parsePortStatus
local getNumOfSwitch
------------------------------------------------------------

------------------local function definition------------------

parsePortStatus = function(status)
	local tmpTbl = {status=nil, resolved=nil, speed=nil, duplex=nil}
	status = string.trim(status)
	if #status ~= 4 then return tmpTbl end

	-- Available port Status:
	--	u|d	-> link status
	--	r|-	-> resolved
	--	h|l	-> speed
	--	f|-	-> duplex
	tmpTbl.status, tmpTbl.resolved, tmpTbl.speed, tmpTbl.duplex = string.match(status, '(.)(.)(.)(.)')

	return tmpTbl
end

getNumOfSwitch = function ()
	count = 0
	for i, value in hdlerUtil.traverseRdbVariable{prefix=g_rdbSwitchPrefix , suffix=g_rdbSwitchStatusSuffix , startIdx=0} do
		value = string.trim(value)
		if not value or value == '' then break end
		count = count +1
	end
	return count
end
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

-- type:rw
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, "1"  -- fixed at the moment
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- bool:readwrite
-- Default Value:
-- Available Value:
-- Involved RDB variable: /sys/class/net/eth0/
	[subROOT .. 'LANEthernetInterfaceConfig.1.Enable'] = {
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
				Logger.log('LANEthernetInterfaceConfig', 'error', 'ERROR!!: Failure on [' .. 'ifconfig ' .. lan_iface .. ' ' .. value .. ']')
			end
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.Status'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
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
	[subROOT .. 'LANEthernetInterfaceConfig.1.Name'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, lan_iface
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.MACAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local contactPoint = systemFS .. lan_iface .. '/address'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local mac = Daemon.readStringFromFile(contactPoint)
				mac = string.trim(mac)
				return 0, string.upper(mac)
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
	[subROOT .. 'LANEthernetInterfaceConfig.1.MaxBitRate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local contactPoint = systemFS .. lan_iface .. '/speed'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local speed = Daemon.readStringFromFile(contactPoint)
				speed = string.trim(speed)
				return 0, speed
			end
			return 0, "100"
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.DuplexMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local contactPoint = systemFS .. lan_iface .. '/duplex'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local duplex = Daemon.readStringFromFile(contactPoint)
				duplex = string.upperWords(string.trim(duplex))
				return 0, duplex
			end
			return 0, "Full"
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.Stats.BytesSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultV = "0"
			local contactPoint = systemFS .. lan_iface .. '/statistics/tx_bytes'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local result = Daemon.readIntFromFile(contactPoint) or defaultV
				return 0, tostring(result)
			end
			return 0, defaultV
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.Stats.BytesReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultV = "0"
			local contactPoint = systemFS .. lan_iface .. '/statistics/rx_bytes'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local result = Daemon.readIntFromFile(contactPoint) or defaultV
				return 0, tostring(result)
			end
			return 0, defaultV
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.Stats.PacketsSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultV = "0"
			local contactPoint = systemFS .. lan_iface .. '/statistics/tx_packets'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local result = Daemon.readIntFromFile(contactPoint) or defaultV
				return 0, tostring(result)
			end
			return 0, defaultV
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.Stats.PacketsReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultV = "0"
			local contactPoint = systemFS .. lan_iface .. '/statistics/rx_packets'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local result = Daemon.readIntFromFile(contactPoint) or defaultV
				return 0, tostring(result)
			end
			return 0, defaultV
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.Stats.ErrorsSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultV = "0"
			local contactPoint = systemFS .. lan_iface .. '/statistics/tx_errors'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local result = Daemon.readIntFromFile(contactPoint) or defaultV
				return 0, tostring(result)
			end
			return 0, defaultV
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'LANEthernetInterfaceConfig.1.Stats.ErrorsReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultV = "0"
			local contactPoint = systemFS .. lan_iface .. '/statistics/rx_errors'
			if hdlerUtil.IsRegularFile(contactPoint) then
				local result = Daemon.readIntFromFile(contactPoint) or defaultV
				return 0, tostring(result)
			end
			return 0, defaultV
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: hw.switch.port.{i}.status
	[subROOT .. 'LANEthernetInterfaceConfig.1.LANEthernetSwitchNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local count = getNumOfSwitch()
			
			return 0, tostring(count)
		end,
		set = function(node, name, value)
			return 0
		end
	},

	[subROOT .. 'LANEthernetInterfaceConfig.1.X_NETCOMM_Switch'] = {
		init = function(node, name, value)
			local numOfInst = getNumOfSwitch()

			for i=1, numOfInst do
				node:createDefaultChild(i)
			end

			node.instance = numOfInst
			g_NumOfDataModelObjInst = numOfInst
			return 0
		end
	},

	[subROOT .. 'LANEthernetInterfaceConfig.1.X_NETCOMM_Switch.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfSwitchInstance = #pathBits
			return 0
		end
	},

	[subROOT .. 'LANEthernetInterfaceConfig.1.X_NETCOMM_Switch.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfSwitchInstance]
			local paramName = pathBits[g_depthOfSwitchInstance+1]
			local retVal = ''

			dataModelIdx = tonumber(dataModelIdx)
			if not dataModelIdx or dataModelIdx > g_NumOfDataModelObjInst or dataModelIdx < 1 then return CWMP.Error.InvalidParameterValue, "Error: Object does not exist: " .. name end

			local portStatus = luardb.get("hw.switch.port." .. (dataModelIdx-1) .. ".status")
			local portStatusTbl = parsePortStatus(portStatus)

			if paramName == 'Status' then
				local ifaceStatus = os.execute('ifconfig ' .. lan_iface .. ' 2>/dev/null | grep -qi "UP "')

				if ifaceStatus ~= 0 then
					retVal = "Disabled"
				elseif portStatusTbl.status then
					if portStatusTbl.status == 'u' then
						retVal = "Up"
					else
						retVal = "NoLink"
					end
				else
					retVal = "Error"
				end
			elseif paramName == 'MaxBitRate' then
				if portStatusTbl.status and portStatusTbl.speed then
					if portStatusTbl.status == 'u' and portStatusTbl.speed == 'h' then
						retVal = '100'
					else
						retVal = '10'
					end
				else
					retVal = ''
				end
			elseif paramName == 'DuplexMode' then
				if portStatusTbl.status and portStatusTbl.duplex then
					if portStatusTbl.status == 'u' and portStatusTbl.duplex == 'f' then
						retVal = 'Full'
					else
						retVal = 'Half'
					end
				else
					retVal = ''
				end
			else
				error('Dunno how to handle ' .. name)
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

}
