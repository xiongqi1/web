return {
	['InternetGatewayDevice.ManagementServer.ConnectionRequestURL'] = {
		get = function(node, name)
			local host = ''
			local port = 8082

			for i = 1,6 do
				local ip = luardb.get('link.profile.' .. i .. '.address')
				local enabled = luardb.get('link.profile.' .. i .. '.enable')
--				print('ip', ip, 'enabled', enabled)
				if enabled == '1' and ip ~= nil then
					host = ip
					break
				end
			end

			if host == '' then
				host = luardb.get('link.profile.0.address')
				if not host or host == '' then return '' end
			end

--			print('host', host, 'port', port)

			return 'http://' .. host .. ':' .. port .. '/acscall'
		end,
		set = cwmpError.funcs.ReadOnly
	}
}
