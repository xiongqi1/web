require("handlers.hdlerUtil")

require("Logger")
Logger.addSubsystem('LANEthernetInterfaceConfig')


local subROOT = conf.topRoot .. '.LANDevice.1.'


------------------local variable----------------------------
local systemFS='/sys/class/net/'
local lan_iface = 'eth0'
------------------------------------------------------------

------------------local function prototype------------------
local parsePortStatus
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
			local ifaceStatus = os.execute('ifconfig ' .. lan_iface .. ' 2>/dev/null | grep -qi "UP "')
			local portStatus = luardb.get("hw.switch.port.0.status")
			local portStatusTbl = parsePortStatus(portStatus)

			if ifaceStatus ~= 0 then
				return 0, "Disabled"
			elseif portStatusTbl.status then
				if portStatusTbl.status == 'u' then
					return 0, "Up"
				else
					return 0, "NoLink"
				end
			end
			return 0, "Error"
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
}
