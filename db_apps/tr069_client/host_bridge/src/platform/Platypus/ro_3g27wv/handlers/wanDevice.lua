function wanIFname ()
	return "eth2.2"
end

function wanConnectionMode ()
	return luanvramDB.get('wanConnectionMode');
end

return {
	['**.WANCommonInterfaceConfig.PhysicalLinkStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal=""
			local if_name = wanIFname ()
			local cmdreturn=execute_CmdLine("ethtool " .. if_name)
			local temp=string.match(string.lower(cmdreturn), "link detected:[%s%w%/]+\n")

			if temp == nil then
				return ""
			end

			local retVal = string.gsub(temp, "link detected:%s+", "")

			if string.find(retVal, "no") ~= nil then
				return "Down"
			elseif string.find(retVal, "yes") ~= nil then
				return "Up"
			else
				return ""
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANCommonInterfaceConfig.TotalBytesSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local if_name = wanIFname ()
			local retVal = readIntFromFile('/sys/class/net/'..if_name..'/statistics/tx_bytes')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANCommonInterfaceConfig.TotalBytesReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local if_name = wanIFname ()
			local retVal = readIntFromFile('/sys/class/net/'..if_name..'/statistics/rx_bytes')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANCommonInterfaceConfig.TotalPacketsSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local if_name = wanIFname ()
			local retVal = readIntFromFile('/sys/class/net/'..if_name..'/statistics/tx_packets')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANCommonInterfaceConfig.TotalPacketsReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local if_name = wanIFname ()
			local retVal = readIntFromFile('/sys/class/net/'..if_name..'/statistics/rx_packets')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},

-- 0 ==> enable, 1 = disable, NVRAM ITEM: wanNatEnable
	['**.WANConnectionDevice.1.WANIPConnection.NATEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('wanNatEnable')

			if retVal == "0" then
				return "1"
			else 
				return "0"
			end
		end,
		set = function(node, name, value)
			
			if value == "1" then
				luanvramDB.set('wanNatEnable', "0")
			elseif value == "0" then
				luanvramDB.set('wanNatEnable', "1")
			else
				return cwmpError.InvalidParameterValue
			end
			return 0
		end
	},

--------------------------------WANIPConnection--------------------------------
	['**.WANConnectionDevice.1.WANIPConnection.1.ConnectionStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal=""
			local wanConMode = luanvramDB.get('wanConnectionMode')

			if wanConMode == "PPPOE" or wanConMode == "PPTP" or wanConMode == "L2TP" then
				return "Disconnected"
			end

			local if_name = wanIFname ()
			local cmdreturn=execute_CmdLine("ethtool " .. if_name)
			local temp=string.match(string.lower(cmdreturn), "link detected:[%s%w%/]+\n")
			local retVal = string.gsub(temp, "link detected:%s+", "")

			if string.find(retVal, "no") ~= nil then
				return "Disconnected"
			elseif string.find(retVal, "yes") ~= nil then
				return "Connected"
			else
				return ""
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.Uptime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return ""
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.NATEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local natenabled = luanvramDB.get('wanNatEnable')

			if natenabled == "0" then
				return "1"
			else
				return "0"
			end
		end,
		set = function(node, name, value)

			if value == "1" then
				luanvramDB.put('wanNatEnable', '0')
			elseif value == "0" then
				luanvramDB.put('wanNatEnable', '1')
			else 
				return cwmpError.InvalidParameterValue
			end

			return 0
		end
	},
-- NVRAM ITEM: wanConnectionMode
	['**.WANConnectionDevice.1.WANIPConnection.1.AddressingType'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('wanConnectionMode')

			if retVal == "DHCP" then
				return "DHCP"
			elseif retVal == "STATIC" then
				return "Static"
			end

			return ""
		end,
		set = function(node, name, value)
			if value == "DHCP" then
				luanvramDB.set('wanConnectionMode', "DHCP")
			elseif value == "Static" then
				luanvramDB.set('wanConnectionMode', "STATIC")
			else
				return cwmpError.InvalidParameterValue
			end
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.ExternalIPAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wanConMode = luanvramDB.get('wanConnectionMode')
			local OperationMode = luanvramDB.get('OperationMode')
			local wanIF = ""
			local retVal = ""

			local enabled = luardb.get('link.profile.1.enable')

			if enabled ~= nil and enabled == '1' then
				retVal = luardb.get('link.profile.1.iplocal')
			end

			if retVal ~= nil and isValidIP4(retVal) then return retVal end

			retVal = ""

			if wanConMode == nil or OperationMode == nil then 
				wanIF = ""
			elseif wanConMode == "PPPOE" or wanConMode == "L2TP" or wanConMode == "PPTP" then
				wanIF = ""
			else
				if OperationMode == "0" then wanIF = "br0"
				elseif OperationMode == "1" then wanIF = "eth2.2"
				elseif OperationMode == "2" then wanIF = "ra0"
				elseif OperationMode == "3" then wanIF = "apcli0" end
			end

			if wanIF ~= "" then 
				retVal = luanvram.getIfIp(wanIF)
			end

			if retVal ~= nil and isValidIP4(retVal) then return retVal end

			return "0.0.0.0"
		end,
		set = function(node, name, value)
			local lan_ipaddr = luanvramDB.get('lan_ipaddr')
			local lan2enabled = luanvramDB.get('Lan2Enabled')
			local OperationMode = luanvramDB.get('OperationMode')

			if not isValidIP4(value) then return cwmpError.InvalidParameterValue end

			if OperationMode ~= "0" then
				if lan_ipaddr == value then return cwmpError.InvalidParameterValue end
				if lan2enabled == "1" then
					local lan2_ipaddr = luanvramDB.get('lan2_ipaddr')
					if lan2_ipaddr == value then return cwmpError.InvalidParameterValue end
				end
			end

			luanvramDB.set('wan_ipaddr', value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.SubnetMask'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('wan_netmask')
		end,
		set = function(node, name, value)
			if not isValidIP4Netmask(value) then return cwmpError.InvalidParameterValue end

			luanvramDB.set('wan_netmask', value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.DefaultGateway'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('wan_gateway')
		end,
		set = function(node, name, value)
			luanvramDB.set('wan_gateway', value)
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.DNSServers'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local nv_items={'wan_primary_dns', 'wan_secondary_dns'}
			local addresses = {}
			for _, v in ipairs(nv_items) do
				local addr = luanvramDB.get(v) or ''
				if isValidIP4(addr) then
					table.insert(addresses, addr)
				end
			end
			return table.concat(addresses, ',')
		end,
		set = function(node, name, value)
			local nv_items={'wan_primary_dns', 'wan_secondary_dns'}
			local addresses = value:explode(',')
			if #addresses > 2 then return cwmpError.ResourceExceeded end
			for i, v in ipairs(nv_items) do
				if not isValidIP4(addresses[i]) then return cwmpError.InvalidParameterValue end
			end
			for i, v in ipairs(nv_items) do
				luanvramDB.set(v, addresses[i])
			end
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.MaxMTUSize'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('wan_static_mtu')
		end,
		set = function(node, name, value)
			local tmpString = value

			tmpString = string.gsub(tmpString, "%s+", "")
			local test = string.find(tmpString, "[^%d]")

			if test == nil then
				luanvramDB.set('wan_static_mtu', tmpString)
			else
				return cwmpError.InvalidParameterValue
			end
			return 0
		end
	},
	['**.WANConnectionDevice.1.WANIPConnection.1.MACAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wan_MAC = execute_CmdLine("mac -r wan")
			local retVal = string.match(wan_MAC, "[%x%p]+")

			if retVal == nil then
				return ""
			else
				return retVal
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
--------------------------------WANPPPConnection--------------------------------
	['**.WANConnectionDevice.2.WANPPPConnection.1.ConnectionStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wanConMode = luanvramDB.get('wanConnectionMode')
			local ispppif = execute_CmdLine('if test -d /sys/class/net/ppp0 ; then echo 1; else echo 0; fi')
			ispppif = string.gsub(ispppif, "%s+", "")

			if wanConMode == "PPPOE" or wanConMode == "PPTP" then
				if ispppif == "1" then
					return "Connected"
				end
			end

			return "Disconnected"
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANConnectionDevice.2.WANPPPConnection.1.ConnectionType'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wanConMode = luanvramDB.get('wanConnectionMode')

			if wanConMode == "PPPOE" then
				return "PPPoE_Bridged"
			elseif wanConMode == "PPTP" then
				return "PPTP_Relay"
			else
				return ""
			end

		end,
		set = function(node, name, value)
			if value == "PPPoE_Bridged" then
				luanvramDB.get('wanConnectionMode', "PPPOE")
			elseif value == "PPTP_Relay" then
				luanvramDB.get('wanConnectionMode', "PPTP")
			else
				return cwmpError.InvalidParameterValue
			end

			return 0
		end
	},
	['**.WANConnectionDevice.2.WANPPPConnection.1.NATEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local natenabled = luanvramDB.get('wanNatEnable')

			if natenabled == nil then
				return "0"
			elseif natenabled == "0" then
				return "1"
			else
				return "0"
			end
		end,
		set = function(node, name, value)
			if value == "1" then
				luanvramDB.put('wanNatEnable', '0')
			elseif value == "0" then
				luanvramDB.put('wanNatEnable', '1')
			else 
				return cwmpError.InvalidParameterValue
			end

			return 0
		end
	},
	['**.WANConnectionDevice.2.WANPPPConnection.1.Username'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('wan_pppoe_user')

			if retVal == nil or string.gsub(retVal, "%s+", "") == "" then return "" end
			return retVal
		end,
		set = function(node, name, value)
			luanvramDB.set('wan_pppoe_user', value)
			return 0
		end
	},
	['**.WANConnectionDevice.2.WANPPPConnection.1.Password'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('wan_pppoe_pass')

			if retVal == nil or string.gsub(retVal, "%s+", "") == "" then return "" end
			return retVal
		end,
		set = function(node, name, value)
			luanvramDB.set('wan_pppoe_pass', value)
			return 0
		end
	},
	['**.WANConnectionDevice.2.WANPPPConnection.1.ExternalIPAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wanConMode = luanvramDB.get('wanConnectionMode')

			if wanConMode == "PPPOE" or wanConMode == "PPTP" then
				local ip = luardb.get('link.profile.0.iplocal')
				local enabled = luardb.get('link.profile.0.enable')
				if enabled == '1' and ip ~= nil then
					return ip
				end
			end

			return ""
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANConnectionDevice.2.WANPPPConnection.1.MACAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wan_MAC = execute_CmdLine("mac -r wan")
			local retVal = string.match(wan_MAC, "[%x%p]+")

			if retVal == nil then
				return ""
			else
				return retVal
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANConnectionDevice.2.WANPPPConnection.1.TransportType'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wanConMode = luanvramDB.get('wanConnectionMode')

			if wanConMode == "PPPOE" then
				return "PPPoE"
			elseif wanConMode == "PPTP" then
				return "PPTP"
			else
				return ""
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.WANConnectionDevice.2.WANPPPConnection.1.ConnectionTrigger'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local conTrigger = luanvramDB.get('wan_pppoe_opmode')

			if conTrigger == "Manual" then
				return "Manual"
			elseif conTrigger == "OnDemand" then
				return "OnDemand"
			elseif conTrigger == "KeepAlive" then
				return "AlwaysOn"
			else
				return ""
			end
		end,
		set = function(node, name, value)

			if value == "Manual" then
				luanvramDB.set('wan_pppoe_opmode', "Manual")
			elseif value == "OnDemand" then
				luanvramDB.set('wan_pppoe_opmode', "OnDemand")
			elseif value == "AlwaysOn" then
				luanvramDB.set('wan_pppoe_opmode', "KeepAlive")
			end
			return 0
		end
	},
}
