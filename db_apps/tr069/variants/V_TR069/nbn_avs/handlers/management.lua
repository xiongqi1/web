require("Daemon")
require("luardb")
local socket = require("socket")
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

	return route_info.src
end

-- callback function to set targetcellid in postsession.
local function setTargetCellIdCb(task)
	if not task or not task.data then
		return false, "Failed to set TargetCellID: Invalid Parameter Value"
	end

	luardb.set("wwan.0.pci", task.data)
	return true
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
			-- TODO:: Although get_acs_src_ipaddr() is generic for all of variants to get network path to ACS,
			--        the function has huge delay in cracknell variant(around 10 seconds).
			--        Moreover the delay affects whole performance of tr069 client(every inform session establishing).
			--        So this function should be modified, though.
			--        In cracknell, the out-going route is fixed on link.profile.1.iplocal,
			--        so do not need to query the network path.
			--host = get_acs_src_ipaddr()

			local ip = luardb.get('link.profile.1.iplocal')
			local enabled = luardb.get('link.profile.1.enable')
			if enabled == '1' and ip ~= nil then
				host = ip
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
			local log = Daemon.readCommandOutput('logcat.sh -k 32')
			return 0, log or ''
		end
	},

	-- A comma separated list of the target PCIs.
	-- The first one is the most preferred one.
	-- This is used together with the ‘Radio.Frequencie s’ to select a cell
	-- Available PCI range is from 0 to 503.
	[conf.topRoot .. '.X_NETCOMM.Radio.TargetCellID'] = {
		get = function(node, name)
			local pci=luardb.get("wwan.0.pci")
			return 0, pci or ''
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidParameterValue end

			-- Validation check
			if value ~= '' then
				local pciTable = value:explode(',')
				for _, item in ipairs(pciTable) do
					local nitem = tonumber(item)
					if not nitem or nitem < 0 or nitem > 503 then
						return CWMP.Error.InvalidParameterValue
					end
				end
			end

			-- Only the last SetParameterValues RPC is valid for this parameter.
			-- The callback function registered previousely should be removed.
			if client:isTaskQueued('postSession', setTargetCellIdCb) == true then
				client:removeTask('postSession', setTargetCellIdCb)
			end

			client:addTask('postSession', setTargetCellIdCb, false, value)

			-- real setting will be applied in postSession stage,
			-- so "Status" field on SetParameterValuesResponse shoud be set to "1"
			return 1
		end
	},
}
