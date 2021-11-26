require("socket")
require("Daemon")
require("luardb")
require("handlers.hdlerUtil")

local url=require("socket.url")
local dns=socket.dns

-- get result of "ip route get" in object
local function get_route_info(ipaddr)

	local rval={}

	local f=Daemon.readCommandOutput("ip route get " .. ipaddr)
	-- example outputs:
	-- 115.187.134.12 via 192.168.101.1 dev rmnet_data0 src 192.168.101.2
	-- 115.187.134.12 dev sqn4g0.1121 src 10.159.230.90
	local ip,dev,src=f:match("(%d+%.%d+%.%d+%.%d+).* +dev +(%S+) +src +(%d+%.%d+%.%d+%.%d+)")
	if ip then
		rval["ip"]=ip
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
		-- Noticed malfunction on dns.toip() method.
		-- If dns server does not work or not implemented,
		-- dns.toip() always returns "nil" even for querying with ip address.
		-- However, with ip address, it is still possible to get route info.
		if not Parameter.Validator.isValidIP4(parsed_acs_url.host) then
			return nil
		else
			acs_ip = parsed_acs_url.host
		end
	end

	-- get route info
	local route_info=get_route_info(acs_ip)
	-- In IP handover mode, the wwan interface has a fake address
	local ip_handover_en = luardb.get('service.ip_handover.enable')
	if ip_handover_en == '1' then
		local fake_wwan_addr = luardb.get('service.ip_handover.fake_wwan_address')
		if fake_wwan_addr == route_info.src then
			return luardb.get('service.ip_handover.last_wwan_ip')
		end
	end
	return route_info.src
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
			host = get_acs_src_ipaddr()

			if not host then
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
			local filename = os.tmpname ()

			if not filename then return 0, '' end

			os.execute('logread.sh > ' .. filename)
			local deviceLog = Daemon.readCommandOutput('tail -c 32k ' .. filename)

			os.remove (filename)

			if not deviceLog then return 0, '' end
			return 0, deviceLog
		end
	},
        [conf.topRoot .. '.DeviceInfo.X_NETCOMM_RebootReason'] = {
            get = function(node, name)
                local val = hdlerUtil.determineRebootReason()
                if val == nil then
                    return CWMP.Error.InternalError,
                        "Error: Could not read Reboot Reason for node " .. name
                end
                return 0, val
            end,
            set = CWMP.Error.funcs.ReadOnly
        },
}
