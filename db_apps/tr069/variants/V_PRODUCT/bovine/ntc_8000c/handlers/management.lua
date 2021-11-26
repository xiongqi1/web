require('Daemon')

-- usage: traverseRdbVariable{prefix='service.firewall.dnat', suffix=, startIdx=0}
-- If startIdx is nil, then default value is 1
local function traverseRdbVariable (arg)
	local i = arg.startIdx or 1;
	local cmdPrefix, cmdSuffix

	cmdPrefix = arg.prefix and arg.prefix .. '.' or ''
	cmdSuffix = arg.suffix and '.' .. arg.suffix or ''
		
	return (function ()
		local index = i
		local v = luardb.get(cmdPrefix .. index .. cmdSuffix)
		i= i + 1
		if v then 
			return index, v
		end
	end)
end

local function GetWifiClientHost()
   local IfPipe,IfText,IP
   IfPipe = io.popen('ifconfig ra0')
   IfText = IfPipe:read("*all")
   IfPipe:close()
   IP = IfText:match('inet addr:(%d*.%d*.%d*.%d*).*Bcast:.*')
   return IP
end

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
			-- Considering Failover mode
			local failoverInf=luardb.get('service.failover.interface')
			if failoverInf and failoverInf == 'wlan' then
				host = GetWifiClientHost()
				if not host or host == '' then
					return CWMP.Error.InternalError, 'Unable to determine device IP address.'
				end
			else 
				-- where failoverInf == "wwan"
				local enabledIdx = 0
				for i, value in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
					local dev=luardb.get('link.profile.' .. i .. '.dev')
					local isWWAN = dev and dev:match('wwan\.%d+') or nil

					if value == '1' and isWWAN then
						enabledIdx = i
						break
					end
				end

				if enabledIdx ~= 0 then
					local status = luardb.get('link.profile.' .. enabledIdx .. '.status' )
					if status and status:lower() == 'up' then
						host = luardb.get('link.profile.' .. enabledIdx .. '.iplocal' )
					end
				end

				if host == '' then
					host = luardb.get('link.profile.0.address')
					if not host or host == '' then
						return CWMP.Error.InternalError, 'Unable to determine device IP address.'
					end
				end
			end
--			print('host', host, 'port', port, 'uri', uri)

			return 0, 'http://' .. host .. ':' .. port .. '/' .. uri
		end
	},
	[conf.topRoot .. '.DeviceInfo.DeviceLog'] = {
		get = function(node, name)
			local filename = os.tmpname ()

			if not filename then return 0, '' end

			os.execute('logread.sh > ' .. filename)
			local deviceLog = Daemon.readCommandOutput('tail -c 32k ' .. filename)

			os.remove (filename)

			if not deviceLog then return 0, '' end
			return 0, deviceLog
		end
	},
}
