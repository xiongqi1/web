----
-- DHCP Binding For LANDevice object tree
--
-- Unfortunately the current Bovine DHCP subsystem is not a particularly good match for
-- the TR-069 model.  It should probably be extended to fit better, in particular
-- the hard-coding of /24 netmasks is pretty limiting.
----

-- watcher for lease changes
local leaseWatcher = function(node, name)
	dimclient.log('debug', 'rebuild hosts object table')
--	dimclient.log('debug', 'name='..name)

	init_dhcpTbl()

--	dimclient.log('debug', 'length of dhcpTbl='..#dhcpTbl)
--	dimclient.log('debug', 'number of children='..node:countInstanceChildren())

	if dhcpTbl ~= nil and #dhcpTbl == node:countInstanceChildren() then return 0 end

	local leaseCollection = findNode(paramRoot, name)
	if leaseCollection == nil then return 0 end
	if leaseCollection.children ~= nil then
		for _, lease in ipairs(leaseCollection.children) do
			if lease.name ~= '0' then
				dimclient.log('info', 'dhcp host watcher: deleting lease: id = ' .. lease.name)
				dimclient.delObject(name.. '.' .. lease.name)
			end
		end
		leaseCollection.instance = 0
	end

	local maxInstanceId = 0

	if dhcpTbl == nil then return 0 end

	for id, v in ipairs(dhcpTbl) do
			local ret, objidx = dimclient.addObject(name..'.')
			if ret > 0 then
				dimclient.log('error', 'addObject failed: ' .. ret)
			else
				-- we store the TR-069 instance id in RDB
				dimclient.log('info', 'new dhcp lease id = ' .. objidx)
			end
	end
	node.instance = maxInstanceId

	return 0
end

function init_dhcpTbl ()
	dhcpTbl={}

	local isDHCPEnabled = luanvram.bufget('dhcpEnabled')

	if isDHCPEnabled ~= nil and isDHCPEnabled == "1" then
		os.execute("killall -q -USR1 udhcpd")
		local content = readEntireFile('/var/udhcpd.leases')
		for count =1, #content, 24 do
			local item={}

			if count+23 > #content then break end

			for i = 1, 24 do
				item[#item+1]=content.byte(content,count+i-1)
			end

			local macAddr=string.format("%02x:%02x:%02x:%02x:%02x:%02x",item[1], item[2], item[3], item[4], item[5], item[6])
			local ipAddr=string.format("%d.%d.%d.%d",item[17],item[18],item[19],item[20])
			local expire=(item[21]*2^24)+(item[22]*2^16)+(item[23]*2^8)+(item[24])
			dhcpTbl[#dhcpTbl+1]={["macAddr"]=macAddr, ["ipAddr"]=ipAddr, ["expire"]=expire, ["IFType"]="Ethernet", ["AddrSrc"]="DHCP"}
		end
	end

	local wifiinfo = luanvram.getstationinfo()

	if wifiinfo ~= nil then
		local wifiitems = wifiinfo:explode('&')

		for i, v in ipairs(wifiitems) do
			if v == nil or v == "" then break end

			local iteminfo = v:explode(',')
			local idxfound = 0
			for idx, dhcpinfo in ipairs(dhcpTbl) do
				if string.lower(iteminfo[1]) == string.lower(dhcpinfo.macAddr) then
					idxfound = idx
					break
				end
			end
			if idxfound ~= 0 then
				dhcpTbl[idxfound].IFType = "802.11"
			else
				dhcpTbl[#dhcpTbl+1] = {["macAddr"]=iteminfo[1], ["ipAddr"]=iteminfo[2], ["expire"]=0, ["IFType"]="802.11", ["AddrSrc"]="Static"}
			end
		end
	end

--	dimclient.callbacks.register('cleanup', reset_dhcpTbl_cb)
end

function reset_dhcpTbl_cb()
	dhcpTbl = nil
end

function setLan_callback ()
	os.execute('/usr/lib/tr-069/scripts/tr069_setLan.lua')
end

return {
	['**.LANHostConfigManagement.DHCPServerConfigurable'] = {
		get = function(node, name)
			return '1'
		end,
		set = function(node, name, value)
			if value == '1' then return 0 end
			return cwmpError.InvalidParameterValue
		end
	},
	['**.LANHostConfigManagement.DHCPServerEnable'] = {
		get = function(node, name)
			local retVal = luanvramDB.get('dhcpEnabled')

			if retVal == nil then
				return "0"
			elseif retVal == "0" or retVal == "1" then
				return retVal
			else
				return "0"
			end
		end,
		set = function(node, name, value)
			if value == "0" or value == "1" then
				luanvramDB.set('dhcpEnabled', value)
				dimclient.callbacks.register('cleanup', setLan_callback)
			else
				return cwmpError.InvalidParameterValue
			end
			return 0
		end
	},
	['**.LANHostConfigManagement.DHCPRelay'] = {
		get = function(node, name)
			return '0'
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.LANHostConfigManagement.MinAddress'] = {
		get = function(node, name)
			return luanvramDB.get('dhcpStart')
		end,
		set = function(node, name, value)
			if not isValidIP4(value) then return cwmpError.InvalidParameterValue end
			luanvramDB.set('dhcpStart', value)
			dimclient.callbacks.register('cleanup', setLan_callback)
			return 0
		end
	},
	['**.LANHostConfigManagement.MaxAddress'] = {
		get = function(node, name)
			return luanvramDB.get('dhcpEnd')
		end,
		set = function(node, name, value)
			if not isValidIP4(value) then return cwmpError.InvalidParameterValue end
			luanvramDB.set('dhcpEnd', value)
			dimclient.callbacks.register('cleanup', setLan_callback)
			return 0
		end
	},
	['**.LANHostConfigManagement.ReservedAddresses'] = { -- should implement reserved addresses?
		get = function(node, name)
			return ''
		end,
		set = function(node, name, value)
			local addresses = value:explode(',')
			for _, addr in ipairs(addresses) do
				if not isValidIP4(addr) then return cwmpError.InvalidParameterValue end
			end
			if value ~= '' then
				return cwmpError.InvalidParameterValue
			end
			return 0
		end
	},
	['**.LANHostConfigManagement.SubnetMask'] = { -- ideally the DHCP subsystem should support more than /24!
		get = function(node, name)
			return luanvramDB.get('dhcpMask')
		end,
		set = function(node, name, value)
			if not isValidIP4Netmask(value) then return cwmpError.InvalidParameterValue end
			luanvramDB.set('dhcpMask', value)
			dimclient.callbacks.register('cleanup', setLan_callback)
			return 0
		end
	},
	['**.LANHostConfigManagement.DNSServers'] = { -- three are required by TR-069, the subsystem ignores the third at the moment...
		get = function(node, name)
			local nv_items={'dhcpPriDns', 'dhcpSecDns'}
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
			local nv_items={'dhcpPriDns', 'dhcpSecDns'}
			local addresses = value:explode(',')
			if #addresses > 3 then return cwmpError.ResourceExceeded end
			for i, v in ipairs(nv_items) do
				if not isValidIP4(addresses[i]) then return cwmpError.InvalidParameterValue end
			end
			for i, v in ipairs(nv_items) do
				luanvramDB.set(v, addresses[i])
			end
			dimclient.callbacks.register('cleanup', setLan_callback)
			return 0
		end
	},
	['**.LANHostConfigManagement.DomainName'] = {
		get = function(node, name)
			return "" --luardb.get('service.dhcp.suffix.0')
		end,
		set = function(node, name, value)
			--luardb.set('service.dhcp.suffix.0', value)
			return 0
		end
	},
	['**.LANHostConfigManagement.IPRouters'] = { -- we currently can only use the device ethernet interface IP as the gateway
		get = function(node, name)
			return luanvramDB.get('dhcpGateway')
		end,
		set = function(node, name, value)
			local ip = luanvramDB.get('dhcpGateway')
			local routers = value:explode(',')
			if #routers > 1 then return cwmpError.ResourceExceeded end -- only required to support one
			for _, addr in ipairs(routers) do
				-- for now just allow setting to the device interface IP
				if not isValidIP4(addr) then return cwmpError.InvalidParameterValue end
				luanvramDB.set('dhcpGateway', addr)
			end
			dimclient.callbacks.register('cleanup', setLan_callback)
			return 0
		end
	},
	['**.LANHostConfigManagement.DHCPLeaseTime'] = {
		get = function(node, name)
			local retVal = luanvramDB.get('dhcpLease')

			if retVal == nil or tonumber(retVal) == nil then return "0" end
			return retVal
		end,
		set = function(node, name, value)
			local lease = tonumber(value)
			if lease < -1 then return cwmpError.InvalidParameterValue end
			if lease == -1 then lease = 'infinite' end
			luanvramDB.set('dhcpLease', lease)
			dimclient.callbacks.register('cleanup', setLan_callback)
			return 0
		end
	},

	['**.LANHostConfigManagement.IPInterface.1.IPInterfaceIPAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('lan_ipaddr')
		end,
		set = function(node, name, value)
			if not isValidIP4(value) then return cwmpError.InvalidParameterValue end
			luanvramDB.set('lan_ipaddr', value)
			dimclient.callbacks.register('cleanup', setLan_callback)
			return 0
		end
	},
	['**.LANHostConfigManagement.IPInterface.1.IPInterfaceSubnetMask'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('lan_netmask')
		end,
		set = function(node, name, value)
			if not isValidIP4Netmask(value) then return cwmpError.InvalidParameterValue end
			luanvramDB.set('lan_netmask', value)
			dimclient.callbacks.register('cleanup', setLan_callback)
			return 0
		end
	},

	-- DHCP hosts table
	['**.Hosts.HostNumberOfEntries'] = {
		get = function(node, name)
			local pathBits = name:explode('.')
			local forwarding = findNode(paramRoot, 'InternetGatewayDevice.LANDevice.' .. pathBits[3] .. '.Hosts.Host')
			local retVal = forwarding:countInstanceChildren()
			if retVal == nil then return 0 end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end
			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},

	['**.Hosts.Host'] = {
		init = function(node, name, value)
			dimclient.log('debug', 'dhcp host init')
	
			-- initial parse of leases
			local maxInstanceId = 0
--			local leases = class:getAll()
--			for _, lease in ipairs(leases) do
--				local id = tonumber(lease.id or 0)
--				if id < 1 then
--					-- lease unknown to dimclient
--					-- we can't add it yet because the dimclient has not been configured :(
--					dimclient.log('info', 'dhcp host init: deferring new lease, host = ' .. (lease.host or '*nil*'))
--				else
--					-- lease previously bound to Host object instance
--					dimclient.log('debug', 'dhcp host init: creating lease: host = ' .. (lease.host or '*nil*') .. ', id = ' .. id)
--					local instance = node:createDefaultChild(lease.id)
--					if id > maxInstanceId then maxInstanceId = id end
--				end
--			end

			if dhcpTbl == nil then init_dhcpTbl() end
			for id, v in ipairs(dhcpTbl) do
					-- lease previously bound to Host object instance
--					dimclient.log('debug', 'dhcp host init: creating lease: host = ' .. (lease.host or '*nil*') .. ', id = ' .. id)
					local instance = node:createDefaultChild(id)
					if id > maxInstanceId then maxInstanceId = id end
			end
			node.instance = maxInstanceId
--			luardb.watch(class.name .. '._index', leaseWatcher)
			return 0
		end,
		create = function(node, name, instanceId)
			dimclient.log('debug', 'dhcp host create')
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('debug', 'dhcp host delete, name = ' .. name)
			node.parent:deleteChild(node)
			return 0
		end,
		poll = leaseWatcher
	},
	['**.Hosts.Host.*'] = {
		create = function(node, name, instanceId)
			dimclient.log('debug', 'dhcp host create')
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('debug', 'dhcp host delete, name = ' .. name)
			node.parent:deleteChild(node)
			return 0
		end,

	},

	['**.Hosts.Host.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local objidx=tonumber(pathBits[6])
			local paraname=pathBits[7]

			if objidx > #dhcpTbl then return cwmpError.InternalError end

			-- strictly there should only ever be one!
			if pathBits[7] == 'LeaseTimeRemaining' then
				if dhcpTbl[objidx].expire == nil or tonumber(dhcpTbl[objidx].expire) == nil then
					node.value = "0"
				else
					node.value = tostring(dhcpTbl[objidx].expire)
				end
			elseif pathBits[7] == 'IPAddress' then
				node.value = dhcpTbl[objidx].ipAddr
			elseif pathBits[7] == 'MACAddress' then
				node.value = dhcpTbl[objidx].macAddr
			elseif pathBits[7] == 'HostName' then
				node.value = ''
			elseif pathBits[7] == 'InterfaceType' then
				node.value = dhcpTbl[objidx].IFType
			elseif pathBits[7] == 'AddressSource' then
				node.value = dhcpTbl[objidx].AddrSrc
			else
				error('Dunno how to handle ' .. name)
			end

			return node.value
		end,
		set = cwmpError.funcs.ReadOnly
	},
}
