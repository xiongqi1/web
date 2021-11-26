----
-- DHCP Binding For LANDevice object tree
--
-- Unfortunately the current Bovine DHCP subsystem is not a particularly good match for
-- the TR-069 model.  It should probably be extended to fit better, in particular
-- the hard-coding of /24 netmasks is pretty limiting.
----
local dhcpConfig = {
	persist = false,
	idSelection = 'smallestUnused', -- 'nextLargest', 'smallestUnused', 'sequential', 'manual'
}

local class = rdbobject.getClass('tr069.dhcp.eth0', dhcpConfig)



-- watcher for lease changes
--[[
local leaseWatcher = function(name, value)
	dimclient.log('debug', 'dhcp lease index change')
	local leases = class:getAll()
	-- delete old leases
	local leaseCollection = findNode(paramRoot, 'InternetGatewayDevice.LANDevice.1.Hosts.Host')
	for _, lease in ipairs(leaseCollection.children) do
		if lease.name ~= '0' then
			local leases = class:getByProperty('id', lease.name)
			if #leases < 1 then
				dimclient.log('info', 'dhcp host watcher: deleting lease: id = ' .. lease.name)
				dimclient.delObject('InternetGatewayDevice.LANDevice.1.Hosts.Host.' .. lease.name)
			end
		end
	end
	-- add new leases
	for _, lease in ipairs(leases) do
		local leaseId = tonumber(lease.id or 0)
		if leaseId < 1 then
			-- lease unknown to dimclient
			local ret, id = dimclient.addObject('InternetGatewayDevice.LANDevice.1.Hosts.Host.')
			if ret > 0 then
				dimclient.log('error', 'addObject failed: ' .. ret)
			else
				-- we store the TR-069 instance id in RDB
				dimclient.log('info', 'new dhcp lease id = ' .. id)
				lease.id = id
			end
		else
			-- lease bound to Host object instance
			local leaseNode = findNode(paramRoot, 'InternetGatewayDevice.LANDevice.1.Hosts.Host.' .. leaseId)
			if leaseNode then
				dimclient.log('debug', 'dhcp host watcher: ignoring existing lease: host = ' .. (lease.host or ''))
			else
				dimclient.log('error', 'dhcp host watcher: unknown lease with id: host = ' .. (lease.host or '') .. ', id = ' .. leaseId)
			end
		end
	end
	print(paramRoot)
end
--]]

local leaseWatcher = function(name, value)
	dimclient.log('debug', 'dhcp lease index change')

	local leases = class:getAll()
	local leaseCollection = findNode(paramRoot, 'InternetGatewayDevice.LANDevice.1.Hosts.Host')
	local unallocated = class:getByProperty('id', '0')

	if #unallocated == 0 and #leases == leaseCollection:countInstanceChildren() then return end

	for _, lease in ipairs(leaseCollection.children) do
		if lease.name ~= '0' then
			dimclient.log('info', 'dhcp host watcher: deleting lease: id = ' .. lease.name)
			dimclient.delObject('InternetGatewayDevice.LANDevice.1.Hosts.Host.' .. lease.name)
		end
	end

	-- add new leases
	for _, lease in ipairs(leases) do
		-- lease unknown to dimclient
		local ret, id = dimclient.addObject('InternetGatewayDevice.LANDevice.1.Hosts.Host.')
		if ret > 0 then
			dimclient.log('error', 'addObject failed: ' .. ret)
		else
			-- we store the TR-069 instance id in RDB
			dimclient.log('info', 'new dhcp lease id = ' .. id)
			lease.id = id
		end
	end
	print(paramRoot)
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
			local value = luardb.get('service.dhcp.enable')

			local internalBool = convertInternalBoolean(value)

			if internalBool == nil then return "0"
			else return internalBool end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if internalBool == nil then return cwmpError.InvalidParameterValue end

			luardb.set('service.dhcp.enable', internalBool)
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
			local addresses = luardb.get('service.dhcp.range.0')
			addresses = addresses:explode(',')
			if #addresses ~= 2 then return cwmpError.InternalError end

			if not isValidIP4(addresses[1]) then return "" end
			return addresses[1]
		end,
		set = function(node, name, value)
			if not isValidIP4(value) then return cwmpError.InvalidParameterValue end
			local addresses = luardb.get('service.dhcp.range.0')
			addresses = addresses:explode(',')
			if #addresses ~= 2 then return cwmpError.InternalError end
			addresses[1] = value
			luardb.set('service.dhcp.range.0', table.concat(addresses, ','))
			return 0
		end
	},
	['**.LANHostConfigManagement.MaxAddress'] = {
		get = function(node, name)
			local addresses = luardb.get('service.dhcp.range.0')
			addresses = addresses:explode(',')
			if #addresses ~= 2 then return cwmpError.InternalError end

			if not isValidIP4(addresses[2]) then return "" end
			return addresses[2]
		end,
		set = function(node, name, value)
			if not isValidIP4(value) then return cwmpError.InvalidParameterValue end
			local addresses = luardb.get('service.dhcp.range.0')
			addresses = addresses:explode(',')
			if #addresses ~= 2 then return cwmpError.InternalError end
			addresses[2] = value
			luardb.set('service.dhcp.range.0', table.concat(addresses, ','))
			return 0
		end
	},
	['**.LANHostConfigManagement.ReservedAddresses'] = { -- should implement reserved addresses?
		get = function(node, name)
			local retTbl = {}
			for i=1, 16 do  --> Max length of ReservedAddresses is 256, so 16 items are max.
				local item = luardb.get('service.dhcp.static.' .. (i-1))

				if item == nil then break end
				local address = item:explode(',')
				if not isValidIP4(address[3]) then break end
				table.insert(retTbl, address[3])
			end
			return table.concat(retTbl, ',')
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
			local netMask=luardb.get('link.profile.0.netmask')
			if not isValidIP4Netmask(netMask) then return '' end
			return netMask
		end,
		set = function(node, name, value)
			if not isValidIP4Netmask(value) then return cwmpError.InvalidParameterValue end
			luardb.set('link.profile.0.netmask', value)
			return 0
		end
	},
	['**.LANHostConfigManagement.DNSServers'] = { -- three are required by TR-069, the subsystem ignores the third at the moment...
		get = function(node, name)
			local addresses = {}
			for i = 1,3 do
				local addr = luardb.get('service.dhcp.dns' .. i .. '.0') or ''
				if isValidIP4(addr) then
					table.insert(addresses, addr)
				end
			end
			return table.concat(addresses, ',')
		end,
		set = function(node, name, value)
			local addresses = value:explode(',')
			if #addresses > 3 then return cwmpError.ResourceExceeded end
			for i = 1,3 do
				if isValidIP4(addresses[i]) then return cwmpError.InvalidParameterValue end
			end
			for i = 1,3 do
				luardb.set('service.dhcp.dns' .. i .. '.0', address[i])
			end
		end
	},
	['**.LANHostConfigManagement.DomainName'] = {
		get = function(node, name)
			return luardb.get('service.dhcp.suffix.0')
		end,
		set = function(node, name, value)
			luardb.set('service.dhcp.suffix.0', value)
			return 0
		end
	},
	['**.LANHostConfigManagement.IPRouters'] = { -- we currently can only use the device ethernet interface IP as the gateway
		get = function(node, name)
			return luardb.get('link.profile.0.address')
		end,
		set = function(node, name, value)
			local ip = luardb.get('link.profile.0.address')
			local routers = value:explode(',')
			if #routers > 1 then return cwmpError.ResourceExceeded end -- only required to support one
			for _, addr in ipairs(routers) do
				-- for now just allow setting to the device interface IP
				if not isValidIP4(addr) then return cwmpError.InvalidParameterValue end
				if addr ~= ip then return cwmpError.InvalidParameterValue end
			end
			return 0
		end
	},
	['**.LANHostConfigManagement.DHCPLeaseTime'] = {
		get = function(node, name)
			local retVal = luardb.get('service.dhcp.lease.0')

			if retVal == "infinite" then return "-1" end

			retVal = convertInternalInteger{input=retVal, minimum=0}
			if retVal == nil then return "0" end

			return retVal
		end,
		set = function(node, name, value)
			local lease = tonumber(value)
			if lease == nil then return cwmpError.InvalidParameterValue end
			if lease < -1 then return cwmpError.InvalidParameterValue end
			if lease == -1 then lease = 'infinite' end
			luardb.set('service.dhcp.lease.0', lease)
			return 0
		end
	},

	-- DHCP hosts table
	['**.Hosts.HostNumberOfEntries'] = {
		get = function(node, name)
			local pathBits = name:explode('.')
			local forwarding = findNode(paramRoot, 'InternetGatewayDevice.LANDevice.' .. pathBits[3] .. '.Hosts.Host')
			local retVal = tostring(forwarding:countInstanceChildren())

			return retVal or "0"
		end,
		set = cwmpError.funcs.ReadOnly
	},
	
	['**.Hosts.Host'] = {
		init = function(node, name, value)
--[[
			dimclient.log('debug', 'dhcp host init')
			
			-- initial parse of leases
			local maxInstanceId = 0
			local leases = class:getAll()
			for _, lease in ipairs(leases) do
				local id = tonumber(lease.id or 0)
				if id < 1 then
					-- lease unknown to dimclient
					-- we can't add it yet because the dimclient has not been configured :(
					dimclient.log('info', 'dhcp host init: deferring new lease, host = ' .. (lease.host or '*nil*'))
				else
					-- lease previously bound to Host object instance
					dimclient.log('debug', 'dhcp host init: creating lease: host = ' .. (lease.host or '*nil*') .. ', id = ' .. id)
					local instance = node:createDefaultChild(lease.id)
					if id > maxInstanceId then maxInstanceId = id end
				end
			end
			node.instance = maxInstanceId
			luardb.watch(class.name .. '._index', leaseWatcher)
			return 0
--]]
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
		init = function(node, name, value)
			local pathBits = name:explode('.')
			local objs = class:getByProperty('id', pathBits[6])
			for _, obj in ipairs(objs) do
				-- strictly there should only ever be one!
				if pathBits[7] == 'LeaseTimeRemaining' then
					local now = os.date('%s')
					node.value = '' .. ((obj.expiry or now) - now)
				elseif pathBits[7] == 'IPAddress' then
					node.value = obj.ip or ''
				elseif pathBits[7] == 'MACAddress' then
					node.value = obj.mac or ''
				elseif pathBits[7] == 'HostName' then
					node.value = obj.host or ''
				elseif pathBits[7] == 'InterfaceType' then
					node.value = obj.interface or ''
				else
					error('Dunno how to handle ' .. name)
				end
			end
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local objs = class:getByProperty('id', pathBits[6])
			for _, obj in ipairs(objs) do
				-- strictly there should only ever be one!
				if pathBits[7] == 'LeaseTimeRemaining' then
					local now = os.date('%s')
					node.value = '' .. ((obj.expiry or now) - now)
				elseif pathBits[7] == 'IPAddress' then
					node.value = obj.ip or ''
				elseif pathBits[7] == 'MACAddress' then
					node.value = obj.mac or ''
				elseif pathBits[7] == 'HostName' then
					node.value = obj.host or ''
				elseif pathBits[7] == 'InterfaceType' then
					node.value = obj.interface or ''
				else
					error('Dunno how to handle ' .. name)
				end
			end
			return node.value
		end,
		set = cwmpError.funcs.ReadOnly
	},
}
