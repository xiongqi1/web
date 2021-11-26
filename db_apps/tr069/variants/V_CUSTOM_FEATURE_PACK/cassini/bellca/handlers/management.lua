require("socket")
require("Daemon")
require("luardb")
require('Logger')

local logSubsystem = 'ManagementServer'
Logger.addSubsystem(logSubsystem)

local url=require("socket.url")
local dns=socket.dns

local fw_cg_rule_cmd = nil
if conf.enableManagementAPN == true then
	local fw_mangle_chain = luardb.get("tr069.management_apn.fw_mangle_chain") or "mangle_OUTPUT_TR069"
	local cg_class_id = luardb.get("tr069.management_apn.cg_class_id") or "1"

	-- Command to get the first cgroup module rule on ${fw_mangle_chain}
	fw_cg_rule_cmd = string.format("iptables -vn -t mangle -L %s 1 -m cgroup --cgroup %s", fw_mangle_chain, cg_class_id)
end

-- get host type ("inet"|"inet6"|"string"|nil)
local function get_host_type(host)
	local R = {ERROR = nil, IPV4 = "inet", IPV6 = "inet6", STRING = "string"}
	if type(host) ~= "string" then
		return R.ERROR
	end

	-- check for ipv4
	local chunks = {host:match("^(%d+)%.(%d+)%.(%d+)%.(%d+)$")}
	if #chunks == 4 then
		for _,v in pairs(chunks) do
			if tonumber(v) > 255 then return R.STRING end
		end
		return R.IPV4
	end

	-- check for ipv6 format, should be 8 'chunks' of numbers/letters
	-- without leading/trailing chars
	-- or fewer than 8 chunks, but with only one `::` group
	local chunks = {host:match("^"..(("(%x*):"):rep(8):gsub(":$","$")))}
	if #chunks == 8
		or #chunks < 8 and host:match('::') and not host:gsub("::","",1):match('::') then
		for _,v in pairs(chunks) do
			if #v > 0 and tonumber(v, 16) > 65535 then return R.STRING end
		end
		return R.IPV6
	end

	return R.STRING
end

-- get result of "ip route get" in object
local function get_route_info(ipaddr)

	local rval={}

	local fwmark_option = ""
	if conf.enableManagementAPN == true then
		local fw_status = Daemon.readCommandOutput(fw_cg_rule_cmd) or ""
		local fwmark = tonumber(string.match(fw_status, ".*MARK set (%S+)"))
		if fwmark then
			fwmark_option = "mark " .. fwmark
		end
	end

	local f=Daemon.readCommandOutput(string.format("ip route get %s %s", ipaddr, fwmark_option))
	-- example outputs:
	-- 115.187.134.12 via 192.168.101.1 dev rmnet_data0 src 192.168.101.2
	-- 115.187.134.12 dev sqn4g0.1121 src 10.159.230.90
	-- 2001:4860:4860::8888 from :: dev rmnet_data0  src 2001:8004:1400:8d2:507a:cfd0:3e20:2949  metric 256
	local ip,dev,src=f:match("(%S+).*%s+dev%s+(%S+)%s+src%s+(%S+)")
	if ip then
		rval["ip"]=ip
		rval["dev"]=dev
		rval["src"]=src
	end

	return rval
end

-- get source network interface IP address to access ACS
-- If req_ipv6 is true, get ipv6 address of the ACS.
-- Otherwise, ipv4.
local function get_acs_src_ipaddr(req_ipv6)

	-- get acs url
	local acs_url=luardb.get("tr069.server.url")
	if not acs_url or acs_url=="" then
		return nil
	end

	-- parse acs url
	parsed_acs_url=url.parse(acs_url)
	if not parsed_acs_url or not parsed_acs_url.host then
		return nil
	end

	-- Noticed malfunction on dns.toip() method.
	-- If dns server does not work or not implemented,
	-- dns.toip() always returns "nil" even for querying with ip address.
	-- However, with ip address, it is still possible to get route info.
	-- Also, dns.toip() always causes around 10 seconds delay.
	-- So in the case of ip address, just skip dns query.
	local acs_ip = nil
	local host_type = get_host_type(parsed_acs_url.host)
	if host_type == 'string' then
		-- This is work-around to avoid unnecessary DNS query.
		if req_ipv6 then
			acs_ip = "2001:4860:4860::8888"
		else
			acs_ip = "8.8.8.8"
		end
		--[[ @TODO
		-- This is generic approch to get network path between CPE and ACS.
		-- After DNS cache is enabled, this file should be removed to use original management.lua on V_TR069/cassini,
		-- which reverts original routine instead of static "ip route get".
		local addrinfo = dns.getaddrinfo(parsed_acs_url.host) or {}
		-- If IPv6 enabled, get IPv6 from DNS query.
		if req_ipv6 then
			for _, info in ipairs(addrinfo) do
				if info.family == "inet6" then
					acs_ip = info.addr
					break
				end
			end
		end
		-- If IPv6 is not enabled. Or, IPv6 address is not valid from DNS query.
		if not acs_ip then
			for _, info in ipairs(addrinfo) do
				if info.family == "inet" then
					acs_ip = info.addr
					break
				end
			end
		end
		--]]
	elseif host_type == 'inet' or host_type == 'inet6' then
		acs_ip = parsed_acs_url.host
	end

	if not acs_ip then
		return nil
	end

	-- get route info
	local route_info=get_route_info(acs_ip)
	local addr_type = get_host_type(acs_ip)
	-- In IP handover mode, the wwan interface has a fake address
	local ip_handover_en = luardb.get('service.ip_handover.enable')
	if addr_type == "inet" and ip_handover_en == '1' then
		local fake_wwan_addr = luardb.get('service.ip_handover.fake_wwan_address')
		if fake_wwan_addr == route_info.src then
			return luardb.get('service.ip_handover.last_wwan_ip')
		end
	end

	-- To support simultaneous ip handover on multiple PDNs
	for idx=1, (tonumber(conf.numOfApnProfiles) or 6) do
		local rdb_prefix = string.format("link.profile.%d.ip_handover.", idx)
		if luardb.get(rdb_prefix .. "enable") == "1"
			and route_info.src == luardb.get(rdb_prefix .. "fake_wwan_address") then
			return luardb.get(rdb_prefix .. "last_wwan_ip")
		end
	end
	return route_info.src
end

local function logConnReqURL(task)
	local node = task.data
	Logger.log(logSubsystem, "warning", "Reported ConnectionRequestURL: " .. node.value or "NIL" )
end

local function makeWatcher(node)
	return function(key, value)
		if value ~= nil and value ~= node.value then
			-- validate new RDB value
			local ret, msg = node:validateValue(value)
			if ret then
				luardb.set(node.rdbKey, node.default)
				node.value = value
				return
			end
			client:asyncParameterChange(node, node:getPath(), value)
			node.value = value
		end
	end
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
			-- To update DNS cache.
			local req_ipv6 = luardb.get("tr069.request.enable_ipv6") == "1" and true or false
			get_acs_src_ipaddr(req_ipv6)

			-- Register rdb notification watcher.
			rdbevent.onChange(conf.rdb.requestPrefix .. '.port', makeWatcher(node))
			if conf.enableManagementAPN == true then
				rdbevent.onChange("tr069.management_apn.current_fwmark", makeWatcher(node))
			end

			if client:isTaskQueued("postSession", logConnReqURL) ~= true then
				client:addTask("postSession", logConnReqURL, true, node)
			end
			return 0
		end,
		get = function(node, name)
			local port = luardb.get(conf.rdb.requestPrefix .. '.port')
			local uri = luardb.get(conf.rdb.requestPrefix .. '.uri')

			local host = ''
			local req_ipv6 = luardb.get("tr069.request.enable_ipv6") == "1" and true or false
			host = get_acs_src_ipaddr(req_ipv6)

			if not host then
				host = luardb.get('link.profile.0.address')
				if not host or host == '' then
					return CWMP.Error.InternalError, 'Unable to determine device IP address.'
				end
			end

--			print('host', host, 'port', port, 'uri', uri)

			local is_ipv6 = get_host_type(host) == "inet6"

			return 0, string.format("http://%s%s%s:%s/%s", (is_ipv6 and "[" or ""), host, (is_ipv6 and "]" or ""), port, uri)
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
