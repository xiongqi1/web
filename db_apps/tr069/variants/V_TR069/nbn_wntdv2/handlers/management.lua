require('Daemon')

return {
	[conf.topRoot .. '.ManagementServer.ConnectionRequestURL'] = {
		init = function(node, name)
			assert(node.access == 'readonly', 'ConnectionRequestURL must be read-only.')
			local uri = luardb.get(conf.rdb.requestPrefix .. '.uri')
			if not uri or uri == '' then
				uri = Daemon.getRandomString()
				luardb.set(conf.rdb.requestPrefix .. '.uri', uri, 'p')
			end
			local port = luardb.get(conf.rdb.requestPrefix .. '.port')
			if not port or port == '' then
				port = conf.net.connectionRequestPort
				luardb.set(conf.rdb.requestPrefix .. '.port', port, 'p')
			end
			return 0
		end,
		get = function(node, name)
			local port = luardb.get(conf.rdb.requestPrefix .. '.port')
			local uri = luardb.get(conf.rdb.requestPrefix .. '.uri')

			local host = ''
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
				if not host or host == '' then
					return CWMP.Error.InternalError, 'Unable to determine device IP address.'
				end
			end

--			print('host', host, 'port', port, 'uri', uri)

			return 0, 'http://' .. host .. ':' .. port .. '/' .. uri
		end
	},
	[conf.topRoot .. '.DeviceInfo.DeviceLog'] = {
		get = function(node, name)
			local log = Daemon.readCommandOutput('tail -c 32k ' .. conf.fs.systemLog)
			return 0, log or ''
		end
	},
}
