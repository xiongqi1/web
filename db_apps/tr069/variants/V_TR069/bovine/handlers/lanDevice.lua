require('Daemon')
return {
--	['**.IPInterfaceNumberOfEntries'] = {
--		get = function(node, name)
--			local nameBits = name:explode('.')
--			local ipInterface = paramTree:find('$ROOT.LANDevice.' .. nameBits[3] .. '.LANHostConfigManagement.IPInterface')
--			return 0, ipInterface:countInstanceChildren()
--		end,
--		set = CWMP.Error.funcs.ReadOnly
--	},

	-- eth0 IPv4 config
	['**.IPInterface.1.IPInterfaceAddressingType'] = {
		-- we deny anything but "Static"
		get = function(node, name) return 0, 'Static' end,
		set = function(node, name, value)
			if value == 'Static' then return 0 end
			return CWMP.Error.InvalidParameterValue
		end
	},

	-- eth0 config
	['**.LANEthernetInterfaceConfig.1.Enable'] = {
		get = function(node, name) return 0, '1' end,
		set = function(node, name, value) end
	},
	['**.LANEthernetInterfaceConfig.1.Status'] = {
		get = function(node, name) return 0, 'Up' end,
		set = CWMP.Error.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.MaxBitRate'] = {
		get = function(node, name) return 0, Daemon.readStringFromFile('/sys/class/net/eth0/speed') end,
		set = CWMP.Error.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.DuplexMode'] = {
		get = function(node, name) return 0, string.upperWords(Daemon.readStringFromFile('/sys/class/net/eth0/duplex')) end,
		set = CWMP.Error.funcs.ReadOnly
	},

	-- eth0 stats
	['**.LANEthernetInterfaceConfig.1.Stats.BytesSent'] = {
		get = function(node, name)
			local defaultV = "0"
			local retVal = Daemon.readIntFromFile('/sys/class/net/eth0/statistics/tx_bytes') or defaultV
			return 0, tostring(retVal) or defaultV
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.BytesReceived'] = {
		get = function(node, name)
			local defaultV = "0"
			local retVal = Daemon.readIntFromFile('/sys/class/net/eth0/statistics/rx_bytes') or defaultV
			return 0, tostring(retVal) or defaultV
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.PacketsSent'] = {
		get = function(node, name)
			local defaultV = "0"
			local retVal = Daemon.readIntFromFile('/sys/class/net/eth0/statistics/tx_packets') or defaultV
			return 0, tostring(retVal) or defaultV
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
	['**.LANEthernetInterfaceConfig.1.Stats.PacketsReceived'] = {
		get = function(node, name)
			local defaultV = "0"
			local retVal = Daemon.readIntFromFile('/sys/class/net/eth0/statistics/rx_packets') or defaultV
			return 0, tostring(retVal) or defaultV
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
}
