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
		set = function(node, name, value) end
	},
	['**.LANEthernetInterfaceConfig.1.Status'] = {
		get = function(node, name) return 'Up' end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.MaxBitRate'] = {
		get = function(node, name) return readStringFromFile('/sys/class/net/eth0/speed') end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.DuplexMode'] = {
		get = function(node, name) return string.upperWords(readStringFromFile('/sys/class/net/eth0/duplex')) end,
		set = cwmpError.funcs.ReadOnly
	},

	-- eth0 stats
	['**.LANEthernetInterfaceConfig.1.Stats.BytesSent'] = {
		get = function(node, name)
			local defaultV = "0"
			local retVal = readIntFromFile('/sys/class/net/eth0/statistics/tx_bytes') or defaultV
			return tostring(retVal) or defaultV
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.BytesReceived'] = {
		get = function(node, name)
			local defaultV = "0"
			local retVal = readIntFromFile('/sys/class/net/eth0/statistics/rx_bytes') or defaultV
			return tostring(retVal) or defaultV
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.PacketsSent'] = {
		get = function(node, name)
			local defaultV = "0"
			local retVal = readIntFromFile('/sys/class/net/eth0/statistics/tx_packets') or defaultV
			return tostring(retVal) or defaultV
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.PacketsReceived'] = {
		get = function(node, name)
			local defaultV = "0"
			local retVal = readIntFromFile('/sys/class/net/eth0/statistics/rx_packets') or defaultV
			return tostring(retVal) or defaultV
		end,
		set = cwmpError.funcs.ReadOnly
	},
}
