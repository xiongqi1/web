return {
	['InternetGatewayDevice.DeviceInfo.SerialNumber'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local lan_MAC=execute_CmdLine("mac -r eth")

			lan_MAC = lan_MAC:gsub(":", "")

			if lan_MAC == nil or string.gsub(lan_MAC,"%s+", "") == "" then return "NETC000000000000" end

-- Requirement from TELUS --> SerialNumber without colon(:)
			return "NETC" .. string.match(lan_MAC, "%x+")
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['InternetGatewayDevice.DeviceInfo.ProvisioningCode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('tr069.state.provisioning_code')
		end,
		set = function(node, name, value)
			luanvramDB.set('tr069.state.provisioning_code', value)
			return 0
		end
	},
	['InternetGatewayDevice.ManagementServer.URL'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('tr069.server.url')
		end,
		set = function(node, name, value)
			luardb.set('tr069.server.url', value)
			luanvramDB.set('tr069.server.url', value)
			return 0
		end
	},
	['InternetGatewayDevice.ManagementServer.Username'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('tr069.server.username')
		end,
		set = function(node, name, value)
			luardb.set('tr069.server.username', value)
			luanvramDB.set('tr069.server.username', value)
			return 0
		end
	},
	['InternetGatewayDevice.ManagementServer.Password'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('tr069.server.password')
		end,
		set = function(node, name, value)
			luardb.set('tr069.server.password', value)
			luanvramDB.set('tr069.server.password', value)
			return 0
		end
	},
	['InternetGatewayDevice.ManagementServer.PeriodicInformEnable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('tr069.server.periodic.enable')
		end,
		set = function(node, name, value)
			luardb.set('tr069.server.periodic.enable', value)
			luanvramDB.set('tr069.server.periodic.enable', value)
			return 0
		end
	},
	['InternetGatewayDevice.ManagementServer.PeriodicInformInterval'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('tr069.server.periodic.interval')
		end,
		set = function(node, name, value)
			luardb.set('tr069.server.periodic.interval', value)
			luanvramDB.set('tr069.server.periodic.interval', value)
			return 0
		end
	},
	['InternetGatewayDevice.ManagementServer.PeriodicInformTime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local time = luanvramDB.get('tr069.server.periodic.time')

			if time == nil or not time:match('^(%d+)$') then return "0" end
			return time
		end,
		set = function(node, name, value)
			luanvramDB.set('tr069.server.periodic.time', value)
			return 0
		end
	},
	['InternetGatewayDevice.ManagementServer.ConnectionRequestURL'] = {
		get = function(node, name)
			local wanConMode = luanvramDB.get('wanConnectionMode')
			local OperationMode = luanvramDB.get('OperationMode')
			local wanIF = ''
			local host = ''
			local port = 7547

			for i = 0,1 do
				local ip = luardb.get('link.profile.' .. i .. '.iplocal')
				local enabled = luardb.get('link.profile.' .. i .. '.enable')
				if enabled == '1' and ip ~= nil then
					host = ip
					break
				end
			end

			if host ~= nil and isValidIP4(host) then return 'http://' .. host .. ':' .. port .. '/acscall' end

			host = ''

			if wanConMode == nil or OperationMode == nil then 
				wanIF = ''
			elseif wanConMode == "PPPOE" or wanConMode == "L2TP" or wanConMode == "PPTP" then
				wanIF = ''
			else
				if OperationMode == "0" then wanIF = "br0"
				elseif OperationMode == "1" then wanIF = "eth2.2"
				elseif OperationMode == "2" then wanIF = "ra0"
				elseif OperationMode == "3" then wanIF = "apcli0" end
			end

			if wanIF ~= '' then 
				host = luanvram.getIfIp(wanIF)
			end

			if host == nil or not isValidIP4(host) then return '' end
			return 'http://' .. host .. ':' .. port .. '/acscall'
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['InternetGatewayDevice.ManagementServer.ConnectionRequestUsername'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.conreq.username')

			if retVal == nil or string.gsub(retVal,"%s+", "") == "" then return "dps" end

			return retVal
		end,
		set = function(node, name, value)
			luanvramDB.set('tr069.conreq.username', value)
			return 0
		end
	},
	['InternetGatewayDevice.ManagementServer.ConnectionRequestPassword'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.conreq.password')

			if retVal == nil or string.gsub(retVal,"%s+", "") == "" then return "dps" end

			return retVal
		end,
		set = function(node, name, value)
			luanvramDB.set('tr069.conreq.password', value)
			return 0
		end
	},
------------------ [start] version 1.2(TR111) ------------------
-- Done
	['InternetGatewayDevice.ManagementServer.UDPConnectionRequestAddress'] = {  -- readonly string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local udpConreqAddr = luardb.get('tr069.tr111.udpconreqaddr')

			if udpConreqAddr == nil then
				return ""
			else
				udpConreqAddr = string.gsub(udpConreqAddr:gsub("^%s+", ""), "%s+$", "")
				return udpConreqAddr
			end
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			luardb.set('tr069.tr111.udpconreqaddr', value)
			return 0
		end
	},
-- Deleted
	['InternetGatewayDevice.ManagementServer.UDPConnectionRequestAddressNotificationLimit'] = { -- readwrite uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Done
	['InternetGatewayDevice.ManagementServer.STUNEnable'] = { -- readwrite bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local stunEnabled = luanvramDB.get('tr069.tr111.STUNEnable')

			if stunEnabled == nil then
				return "0"
			elseif stunEnabled == "1" or stunEnabled == "0" then
				return stunEnabled
			else
				return "0"
			end
		end,
		set = function(node, name, value)
			if value == nil then
				return cwmpError.InvalidParameterValue
			elseif value == "0" or value == "1" then
				luanvramDB.set('tr069.tr111.STUNEnable', value)
				return 0
			else
				return cwmpError.InvalidParameterValue
			end
		end
	},
-- Done
	['InternetGatewayDevice.ManagementServer.STUNServerAddress'] = { -- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr111.STUNAddr')

			if retVal == nil then return "" end
			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			luanvramDB.set('tr069.tr111.STUNAddr', value)
			return 0
		end
	},
-- Done
	['InternetGatewayDevice.ManagementServer.STUNServerPort'] = { -- readwrite uint [0:65535]
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr111.STUNPort')

			if retVal == nil then return 0 end
			if tonumber(retVal) == nil then return 0 end
			return retVal

		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end

			local portnum = tonumber(value)
			if portnum == nil then return cwmpError.InvalidParameterValue end

			if portnum < 0 or portnum > 65535 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('tr069.tr111.STUNPort', portnum)
			return 0
		end
	},
-- Done
	['InternetGatewayDevice.ManagementServer.STUNUsername'] = {  -- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr111.STUNName')

			if retVal == nil then return "" end
			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			luanvramDB.set('tr069.tr111.STUNName', value)
			return 0 
		end
	},
-- Done
	['InternetGatewayDevice.ManagementServer.STUNPassword'] = { -- readwrite string
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr111.STUNPassword')

			if retVal == nil then return "" end
			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			luanvramDB.set('tr069.tr111.STUNPassword', value)
			return 0 
		end
	},
-- TODO
	['InternetGatewayDevice.ManagementServer.STUNMaximumKeepAlivePeriod'] = { -- readwrite int [-1:] A value of -1 indicates that no maximum period is specified.
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return -1
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- TODO
	['InternetGatewayDevice.ManagementServer.STUNMinimumKeepAlivePeriod'] = { -- readwrite uint
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 60
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Done
	['InternetGatewayDevice.ManagementServer.NATDetected'] = {  -- readonly bool
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local natDetected = luardb.get('tr069.tr111.NATDetected')

			if natDetected == nil then return "0" end

			if natDetected == "0" or natDetected == "1" then 
				return natDetected 
			else
				return "0"
			end

			return 0
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			if value == "1" or value == "0" then
				luardb.set('tr069.tr111.NATDetected', value)
			end

			return 0
		end
	},
------------------ [end] version 1.2(TR111) --------------------
}
