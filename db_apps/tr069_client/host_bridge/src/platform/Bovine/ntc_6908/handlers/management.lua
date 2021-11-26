return {
	['InternetGatewayDevice.DeviceInfo.SerialNumber'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local lan_MAC=luardb.get("systeminfo.mac.eth0")

			lan_MAC = lan_MAC:gsub(":", "")

			if lan_MAC == nil or string.gsub(lan_MAC,"%s+", "") == "" then return "NETC:000000000000" end

			return "NETC:" .. string.match(lan_MAC, "%x+")
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['InternetGatewayDevice.ManagementServer.ConnectionRequestURL'] = {
		get = function(node, name)
			local host = ''
			local port = 8082

			for i = 1,6 do
				local ip = luardb.get('link.profile.' .. i .. '.iplocal')
				local enabled = luardb.get('link.profile.' .. i .. '.enable')
--				print('ip', ip, 'enabled', enabled)
				if enabled == '1' and ip ~= nil then
					host = ip
					break
				end
			end

--			print('host', host, 'port', port)

			if host == '' then return '' end
			return 'http://' .. host .. ':' .. port .. '/acscall'
		end,
		set = cwmpError.funcs.ReadOnly
	}
}
