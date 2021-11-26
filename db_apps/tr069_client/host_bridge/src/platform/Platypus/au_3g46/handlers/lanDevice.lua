function lanIFname ()
	local retVal="br0"
	local opMode=luanvramDB.get("OperationMode")

	opModeNum = tonumber(opMode)
	if opModeNum == 2 then
		retVal = "eth2"
	else
		retVal = "br0"
	end
	return retVal
end

return {
--	['**.IPInterfaceNumberOfEntries'] = {
--		get = function(node, name)
--			local nameBits = name:explode('.')
--			local ipInterface = findNode(paramRoot, 'InternetGatewayDevice.LANDevice.' .. nameBits[3] .. '.LANHostConfigManagement.IPInterface')
--			return ipInterface:countInstanceChildren()
--		end,
--		set = cwmpError.funcs.ReadOnly
--	},

	-- eth0 IPv4 config
	['**.IPInterface.1.IPInterfaceAddressingType'] = {
		-- we deny anything but "Static"
		get = function(node, name) return 'Static' end,
		set = function(node, name, value)
			if value == 'Static' then return 0 end
			return cwmpError.InvalidParameterValue
		end
	},

	-- eth0 config
	['**.LANEthernetInterfaceConfig.1.Enable'] = {
		get = function(node, name) return '1' end,
		set = function(node, name, value) return 0 end
	},
	['**.LANEthernetInterfaceConfig.1.Status'] = {
		get = function(node, name) return 'Up' end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Name'] = {
		get = function(node, name) return lanIFname() end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.MACAddress'] = {
		get = function(node, name)
			local if_name = lanIFname ()
			local retVal = readStringFromFile('/sys/class/net/'..if_name..'/address')

			if retVal == nil then retVal = "" end
			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.MaxBitRate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local maxRate=0
			local cmdreturn=execute_CmdLine("ethtool eth2")
			local temp=string.match(string.lower(cmdreturn), "supported link modes:[%s%w%/]+\n")

			for rate in string.gmatch(temp, "%d+") do
				if tonumber(rate) > tonumber(maxRate) then
					maxRate = rate
				end
			end

			return tostring(maxRate)
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.DuplexMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal=""
			local cmdreturn=execute_CmdLine("ethtool eth2")
			local temp=string.match(string.lower(cmdreturn), "duplex:[%s%w%/]+\n")
			local retVal = string.gsub(temp, "duplex:%s+", "")

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},

	-- eth0 stats
	['**.LANEthernetInterfaceConfig.1.Stats.BytesSent'] = {
		get = function(node, name)
			local if_name = lanIFname ()
			local retVal = readIntFromFile('/sys/class/net/'..if_name..'/statistics/tx_bytes')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.BytesReceived'] = {
		get = function(node, name)
			local if_name = lanIFname ()
			local retVal = readIntFromFile('/sys/class/net/'..if_name..'/statistics/rx_bytes')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.PacketsSent'] = {
		get = function(node, name)
			local if_name = lanIFname ()
			local retVal = readIntFromFile('/sys/class/net/'..if_name..'/statistics/tx_packets')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.PacketsReceived'] = {
		get = function(node, name)
			local if_name = lanIFname ()
			local retVal = readIntFromFile('/sys/class/net/'..if_name..'/statistics/rx_packets')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
}
