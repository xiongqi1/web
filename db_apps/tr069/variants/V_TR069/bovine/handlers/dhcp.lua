require('Daemon')
require('Parameter.Validator')
require('Logger')
Logger.addSubsystem('dhcp.lua')

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

------------------local function prototype------------------
local convertInternalBoolean
local convertInternalInteger
local leaseWatcher
------------------------------------------------------------


convertInternalBoolean = function (val)
	local inputStr = tostring(val)

	if not inputStr then return nil end

	inputStr = (string.gsub(inputStr, "^%s*(.-)%s*$", "%1"))

	if inputStr == '1' or inputStr:lower() == 'true' then
		return '1'
	elseif inputStr == '0' or inputStr:lower() == 'false' then
		return '0'
	end

	return nil
end

-- usage: convertInternalInteger{input=number, minimum=0, maximum=50}
-- If number doesn't have a specific range, just set "minimum" or "maximum" to nil or omit this argument.
-- success: return interger type value
-- false: return nil
convertInternalInteger = function (arg)
	local convertedInt = tonumber(arg.input)

	if type(convertedInt) == 'number'
	then
		local minimum = arg.minimum or -2147483648
		local maximum = arg.maximum or 2147483647

		minimum = tonumber(minimum)
		maximum = tonumber(maximum)

		if minimum == nil or maximum == nil then return nil end
		if convertedInt < minimum or convertedInt > maximum then return nil end

		return convertedInt

	else
		return nil
	end
end

leaseWatcher = function()
	Logger.log('dhcp.lua', 'debug', 'dhcp lease index change')

	local leases = class:getAll()
	local leaseCollection = paramTree:find('$ROOT.LANDevice.1.Hosts.Host')
	local unallocated = class:getByProperty('id', '0')

	if #unallocated == 0 and #leases == leaseCollection:countInstanceChildren() then return end

	Logger.log('dhcp.lua', 'info', 'dhcp leaseWatcher: Update Object List')

	local node = paramTree:find('$ROOT.LANDevice.1.Hosts.Host')

	for _, lease in ipairs(leaseCollection.children) do
		if lease.name ~= '0' then
			Logger.log('dhcp.lua', 'debug', 'dhcp host watcher: deleting lease: id = ' .. lease.name)
			lease.parent:deleteChild(lease)
		end
	end

	local maxInstanceId = 0
	-- add new leases
	for idx, lease in ipairs(leases) do
		Logger.log('dhcp.lua', 'debug', 'leaseWatcher: creating lease: host = ' .. (lease.host or '*nil*') .. ', id = ' .. idx)
		lease.id = idx
		local instance = node:createDefaultChild(idx)
		if idx > maxInstanceId then maxInstanceId = idx end
	end
	node.instance = maxInstanceId
end

return {
	['**.LANHostConfigManagement.DHCPServerConfigurable'] = {
		get = function(node, name)
			return 0, '1'
		end,
		set = function(node, name, value)
			if value == '1' then return 0 end
			return CWMP.Error.InvalidParameterValue
		end
	},
	['**.LANHostConfigManagement.DHCPServerEnable'] = {
		get = function(node, name)
			local value = luardb.get('service.dhcp.enable')

			local internalBool = convertInternalBoolean(value)

			if internalBool == nil then return 0, "0"
			else return 0, internalBool end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if internalBool == nil then return CWMP.Error.InvalidParameterValue end

			luardb.set('service.dhcp.enable', internalBool)
			return 0
		end
	},
	['**.LANHostConfigManagement.DHCPRelay'] = {
		get = function(node, name)
			return 0, '0'
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
	['**.LANHostConfigManagement.MinAddress'] = {
		get = function(node, name)
			local addresses = luardb.get('service.dhcp.range.0')
			addresses = addresses:explode(',')
			if #addresses ~= 2 then return CWMP.Error.InternalError, 'Reading Failure: invalid address range' end

			if not Parameter.Validator.isValidIP4(addresses[1]) then return 0, "" end
			return 0, addresses[1]
		end,
		set = function(node, name, value)
			if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
			local addresses = luardb.get('service.dhcp.range.0')
			addresses = addresses:explode(',')
			if #addresses ~= 2 then return CWMP.Error.InternalError end
			addresses[1] = value
			luardb.set('service.dhcp.range.0', table.concat(addresses, ','))
			return 0
		end
	},
	['**.LANHostConfigManagement.MaxAddress'] = {
		get = function(node, name)
			local addresses = luardb.get('service.dhcp.range.0')
			addresses = addresses:explode(',')
			if #addresses ~= 2 then return CWMP.Error.InternalError, 'Reading Failure: invalid address range' end

			if not Parameter.Validator.isValidIP4(addresses[2]) then return 0, "" end
			return 0, addresses[2]
		end,
		set = function(node, name, value)
			if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
			local addresses = luardb.get('service.dhcp.range.0')
			addresses = addresses:explode(',')
			if #addresses ~= 2 then return CWMP.Error.InternalError end
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
				if not Parameter.Validator.isValidIP4(address[3]) then break end
				table.insert(retTbl, address[3])
			end
			return 0, table.concat(retTbl, ',')
		end,
		set = function(node, name, value)
			local addresses = value:explode(',')
			for _, addr in ipairs(addresses) do
				if not Parameter.Validator.isValidIP4(addr) then return CWMP.Error.InvalidParameterValue end
			end
			if value ~= '' then
				return CWMP.Error.InvalidParameterValue
			end
			return 0
		end
	},
	['**.LANHostConfigManagement.SubnetMask'] = { -- ideally the DHCP subsystem should support more than /24!
		get = function(node, name)
			local netMask=luardb.get('link.profile.0.netmask')
			if not Parameter.Validator.isValidIP4Netmask(netMask) then return 0, '' end
			return 0, netMask
		end,
		set = function(node, name, value)
			if not Parameter.Validator.isValidIP4Netmask(value) then return CWMP.Error.InvalidParameterValue end
			luardb.set('link.profile.0.netmask', value)
			return 0
		end
	},
	['**.LANHostConfigManagement.DNSServers'] = { -- three are required by TR-069, the subsystem ignores the third at the moment...
		get = function(node, name)
			local addresses = {}
			for i = 1,3 do
				local addr = luardb.get('service.dhcp.dns' .. i .. '.0') or ''
				if Parameter.Validator.isValidIP4(addr) then
					table.insert(addresses, addr)
				end
			end
			return 0, table.concat(addresses, ',')
		end,
		set = function(node, name, value)
			local addresses = value:explode(',')
			if #addresses > 3 then return CWMP.Error.InvalidParameterValue end
			for i = 1,3 do
				if Parameter.Validator.isValidIP4(addresses[i]) then return CWMP.Error.InvalidParameterValue end
			end
			for i = 1,3 do
				luardb.set('service.dhcp.dns' .. i .. '.0', address[i])
			end
		end
	},
	['**.LANHostConfigManagement.DomainName'] = {
		get = function(node, name)
			return 0, luardb.get('service.dhcp.suffix.0')
		end,
		set = function(node, name, value)
			luardb.set('service.dhcp.suffix.0', value)
			return 0
		end
	},
	['**.LANHostConfigManagement.IPRouters'] = { -- we currently can only use the device ethernet interface IP as the gateway
		get = function(node, name)
			return 0, luardb.get('link.profile.0.address')
		end,
		set = function(node, name, value)
			local ip = luardb.get('link.profile.0.address')
			local routers = value:explode(',')
			if #routers > 1 then return CWMP.Error.InvalidParameterValue end -- only required to support one
			for _, addr in ipairs(routers) do
				-- for now just allow setting to the device interface IP
				if not Parameter.Validator.isValidIP4(addr) then return CWMP.Error.InvalidParameterValue end
				if addr ~= ip then return CWMP.Error.InvalidParameterValue end
			end
			return 0
		end
	},
	['**.LANHostConfigManagement.DHCPLeaseTime'] = {
		get = function(node, name)
			local retVal = luardb.get('service.dhcp.lease.0')

			if retVal == "infinite" then return 0, "-1" end

			retVal = convertInternalInteger{input=retVal, minimum=0}
			if retVal == nil then return 0, "0" end

			return 0, tostring(retVal)
		end,
		set = function(node, name, value)
			local lease = tonumber(value)
			if lease == nil then return CWMP.Error.InvalidParameterValue end
			if lease < -1 then return CWMP.Error.InvalidParameterValue end
			if lease == -1 then lease = 'infinite' end
			luardb.set('service.dhcp.lease.0', lease)
			return 0
		end
	},

	-- DHCP hosts table
	['**.Hosts.HostNumberOfEntries'] = {
		get = function(node, name)
			local pathBits = name:explode('.')
			local forwarding = paramTree:find('$ROOT.LANDevice.' .. pathBits[3] .. '.Hosts.Host')
			local retVal = tostring(forwarding:countInstanceChildren())

			return 0, retVal or "0"
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
	
	['**.Hosts.Host'] = {
		init = function(node, name, value)

			Logger.log('dhcp.lua', 'debug', 'dhcp host init')
			
			-- initial parse of leases
			local maxInstanceId = 0
			local leases = class:getAll()
			for idx, lease in ipairs(leases) do
				Logger.log('dhcp.lua', 'debug', 'dhcp host init: creating lease: host = ' .. (lease.host or '*nil*') .. ', id = ' .. idx)
				lease.id = idx
				local instance = node:createDefaultChild(idx)
				if idx > maxInstanceId then maxInstanceId = idx end
			end
			node.instance = maxInstanceId

			if client:isTaskQueued('cleanUp', leaseWatcher) ~= true then
				client:addTask('cleanUp', leaseWatcher, true) -- persistent callback function
			end
			return 0
		end,
-- 		create = function(node, name, instanceId)
-- 			Logger.log('dhcp.lua', 'debug', 'dhcp host create')
-- 			local instance = node:createDefaultChild(instanceId)
-- 			return 0
-- 		end,
-- 		delete = function(node, name)
-- 			Logger.log('dhcp.lua', 'debug', 'dhcp host delete, name = ' .. name)
-- 			node.parent:deleteChild(node)
-- 			return 0
-- 		end,
-- 		poll = leaseWatcher
	},
	['**.Hosts.Host.*'] = {
-- 		create = function(node, name, instanceId)
-- 			Logger.log('dhcp.lua', 'debug', 'dhcp host create')
-- 			local instance = node:createDefaultChild(instanceId)
-- 			return 0
-- 		end,
-- 		delete = function(node, name)
-- 			Logger.log('dhcp.lua', 'debug', 'dhcp host delete, name = ' .. name)
-- 			node.parent:deleteChild(node)
-- 			return 0
-- 		end,
		
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
			return 0, node.value
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
}
