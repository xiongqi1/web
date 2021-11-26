require("socket")
require("Daemon")
require("luardb")

local url=require("socket.url")
local dns=socket.dns

-- get result of "ip route get" in object
local function get_route_info(ipaddr)

	local rval={}
	
	local f=Daemon.readCommandOutput("ip route get " .. ipaddr)
	
	-- search ip address
	local ip,gw,dev,src=f:match("(%d+%.%d+%.%d+%.%d+) +via +(%d+%.%d+%.%d+%.%d+) +dev +([a-zA-Z0-9_]+) +src +(%d+%.%d+%.%d+%.%d+)")
	if ip then
		rval["ip"]=ip
		rval["gw"]=gw
		rval["dev"]=dev
		rval["src"]=src
	end

	return rval
end

-- get source network interface IP address to access ACS
local function get_acs_src_ipaddr()
	
	-- get acs url
	acs_url=luardb.get("tr069.server.url")
	if not acs_url or acs_url=="" then
		return nil
	end

	-- parse acs url
	parsed_acs_url=url.parse(acs_url)
	if not parsed_acs_url or not parsed_acs_url.host then
		return nil
	end
	
	-- get acs ip address
	local acs_ip=dns.toip(parsed_acs_url.host)
	if not acs_ip then
		return nil
	end
	
	-- get route info
	local route_info=get_route_info(acs_ip)
	
	return route_info.src
end

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

local function getCurrentDefaultRoute()
	local index=1

	for i, enabled in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
		local dev=luardb.get('link.profile.' .. i .. '.dev')
		local isDefault=luardb.get('link.profile.' .. i .. '.defaultroute')
		local isWWAN = dev and dev:match('wwan\.%d+')

		if isWWAN and isDefault == '1' then
			index = i
		end
	end
	return index
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
--[[			
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
]]--
			host = get_acs_src_ipaddr()

			if not host then
				local defaultRoute = getCurrentDefaultRoute()
				host = luardb.get('link.profile.'.. defaultRoute ..'.iplocal')
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
