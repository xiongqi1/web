require('Daemon')
require('Parameter.Validator')
require('Logger')
Logger.addSubsystem('layer3Forwarding')

----
-- Layer3Forwarding is the routing table
----
--[[ script for taking all class objects out
require('rdbobject')

do
local class = rdbobject.getClass('tr069.layer3.forwarding')
routeRules = class:getAll()
	for _, rule in ipairs(routeRules) do
		if rule then
			class:delete(rule)
		end
	end
end
--]]
local maxNumOfInstance = 32

local forwardingConfig = {
	persist = true,
	idSelection = 'smallestUnused', -- 'nextLargest', 'smallestUnused', 'sequential', 'manual'
}

local class = rdbobject.getClass('tr069.layer3.forwarding', forwardingConfig)

------------------local function prototype------------------
local convertInternalBoolean
local convertInternalInteger
local inet_ntoa
local isBitSet
local build_objectList
local getMD5sum
local routeWatcher
local createDefaultPersistObject
local deleteRouteObject
local addRoutingrule
local delRoutingrule
local add_Index_AddTable
local add_RoutingRules_cb
local cvt_IfaceName_toInternal
local cvt_IfaceName_toExternal
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

inet_ntoa = function (addr)
	local ret, _, o1, o2, o3, o4= addr:find('^(%w%w)(%w%w)(%w%w)(%w%w)$')

	if not ret then return nil end

	return tonumber(o4, 16) .. '.' .. tonumber(o3, 16) .. '.' .. tonumber(o2, 16) .. '.'  .. tonumber(o1, 16)
end

isBitSet = function (num, bit)
	local module = 0
	local result = tonumber(num)
	bit = tonumber(bit)

	if not result or not bit then return false end
	for i=1, bit do
		module = result % 2
		result = (result - module) / 2
	end

	if module == 1 then return true else return false end
end

build_objectList = function ()

	local activatedPersistRules = class:getByProperty('status', 'Enabled')
	local file = io.open('/proc/net/route', 'r')

	for line in file:lines()
	do
		local skip = false
		local ret, _, iface, destIp, gatewayIp, flags, _, _, metric, destSubM, mtu, _, _, _
			= line:find('(%w+)%s*(%w+)%s*(%w+)%s*(%d+)%s*(%d+)%s*(%d+)%s*(%d+)%s*(%w+)%s*(%d+)%s*(%d+)%s*(%d+)%s*')
		if ret then
			destIp = inet_ntoa(destIp)
			gatewayIp = inet_ntoa(gatewayIp)
			destSubM = inet_ntoa(destSubM)

			for _, persistRule in ipairs(activatedPersistRules) do
				if persistRule.destIp == destIp and persistRule.gatewayIp == gatewayIp and persistRule.destSubM == destSubM then
					skip = true
					if isBitSet(flags, 3) then
						persistRule.type = 'Host'
					elseif destSubM == '0.0.0.0' then
						persistRule.type = 'Default'
					else
						persistRule.type = 'Network'
					end
					persistRule.iface = cvt_IfaceName_toExternal(iface)
					persistRule.fmetric = metric
					break
				end
			end

			if skip == false then
				local newRule = class:new()
				newRule.isPersist = '0'
				newRule.instanceId = 0
				newRule.enable = '1'
				newRule.status = 'Enabled'
				if isBitSet(flags, 3) then
					newRule.type = 'Host'
				elseif destSubM == '0.0.0.0' then
					newRule.type = 'Default'
				else
					newRule.type = 'Network'
				end
				newRule.destIp = destIp or '0.0.0.0'
				newRule.destSubM = destSubM or '0.0.0.0'
				newRule.sourceIp = '0.0.0.0'
				newRule.sourceSubM = '0.0.0.0'
				newRule.forwardPolicy = '-1'
				newRule.gatewayIp = gatewayIp or '0.0.0.0'
				newRule.iface = cvt_IfaceName_toExternal(iface)
				newRule.fmetric = metric
				if mtu == '0' then
					newRule.mtu = "1500"
				else
					newRule.mtu = mtu
				end
			end
		end
	end

	file:close()
end

getMD5sum = function (filename)
	local file = io.open(filename, 'r')

	if not file then return nil end
	file:close()

	local cmdResult = Daemon.readCommandOutput('md5sum ' .. filename)

	if not cmdResult then return nil end

	local ret, _, digest = cmdResult:find('(%w+)%s*')

	if ret then
		return digest
	else
		return nil
	end
end

local savedDigest

routeWatcher = function (node, name)
	Logger.log('layer3Forwarding', 'debug', 'routeWatcher: Checking Layer3 forwarding rules')

	local currentDigest = getMD5sum('/proc/net/route')

	if currentDigest == nil or savedDigest == currentDigest then return end

	savedDigest = currentDigest

	Logger.log('layer3Forwarding', 'info', 'routeWatcher: Update Layer3 forwarding rules')

	local node = paramTree:find('$ROOT.Layer3Forwarding.Forwarding')

	local routeRules = class:getByProperty('isPersist', '0')
	for _, rule in ipairs(routeRules) do
		if rule and rule.enable == '1' then
			Logger.log('layer3Forwarding', 'debug', 'routeWatcher: delete route class object id =' .. rule.instanceId)
			class:delete(rule)
		end
	end

	build_objectList()

	local leaseCollection = node
	if leaseCollection == nil then return 0 end
	if leaseCollection.children ~= nil then
		for _, lease in ipairs(leaseCollection.children) do
			if lease.name ~= '0' then
				Logger.log('layer3Forwarding', 'debug', 'routeWatcher: deleting Node: id = ' .. lease.name)
				lease.parent:deleteChild(lease)
			end
		end
		leaseCollection.instance = 0
	end

	local maxInstanceId = 0
	local listOccupied = {}

	local routeRules = class:getByProperty('isPersist', '1')

	for _, rule in ipairs(routeRules) do
		local id = tonumber(rule.instanceId or 0)
		if id < 1 then
			Logger.log('layer3Forwarding', 'info', 'routeWatcher: Error Persist object does not have id, destIp = ' .. (rule.destIp or '*nil*'))
		else
			Logger.log('layer3Forwarding', 'debug', 'routeWatcher: creating Persist Rules: destIp = ' .. (rule.destIp or '*nil*') .. ', id = ' .. id)
			local instance = node:createDefaultChild(rule.instanceId)
			listOccupied[id] = true
			if id > maxInstanceId then maxInstanceId = id end
		end
	end

	local routeRules = class:getByProperty('isPersist', '0')
	for _, rule in ipairs(routeRules) do
		local id = tonumber(rule.instanceId or 0)
		if id > 0 then
			Logger.log('layer3Forwarding', 'debug', 'routeWatcher: creating Disabled Dynamic Rules: destIp = ' .. (rule.destIp or '*nil*') .. ', id = ' .. id)
			local instance = node:createDefaultChild(rule.instanceId)
			listOccupied[id] = true
			if id > maxInstanceId then maxInstanceId = id end
		end
	end

	local routeRules = class:getByProperty('isPersist', '0')
	for _, rule in ipairs(routeRules) do
		local id = tonumber(rule.instanceId or 0)

		if id == 0 then
			local availableid = 1
			for i, _ in ipairs(listOccupied) do
				availableid = availableid + 1
			end
			rule.instanceId = availableid

			Logger.log('layer3Forwarding', 'debug', 'routeWatcher: creating Enabled Dynamic Rules: destIp = ' .. (rule.destIp or '*nil*') .. ', availableid = ' .. availableid)

			local instance = node:createDefaultChild(rule.instanceId)
			listOccupied[availableid] = true
			if availableid > maxInstanceId then maxInstanceId = availableid end
		end
	end
	node.instance = maxInstanceId
end

createDefaultPersistObject = function (id)
	local routeRules = class:getByProperty('instanceId', tostring(id))

	if #routeRules > 0 then
		Logger.log('layer3Forwarding', 'error', 'ERROR in createDefaultPersistObject(): occupied id = ' .. id)
		return false
	end

	local newRule = class:new()
	newRule.isPersist = '1'
	newRule.instanceId = id
	newRule.enable = '0'
	newRule.status = 'Disabled'
	newRule.type = 'Host'
	newRule.destIp = '0.0.0.0'
	newRule.destSubM = '0.0.0.0'
	newRule.sourceIp = '0.0.0.0'
	newRule.sourceSubM = '0.0.0.0'
	newRule.forwardPolicy = '-1'
	newRule.gatewayIp = '0.0.0.0'
	newRule.iface = ''
	newRule.fmetric = '0'
	newRule.mtu = '1500'

	return true
end

deleteRouteObject = function (id)
	local routeRules = class:getByProperty('instanceId', tostring(id))

	if #routeRules == 0 then
		Logger.log('layer3Forwarding', 'error', 'ERROR in deleteRouteObject(): Not Exist: id = ' .. id)
		return false
	end

	if #routeRules > 1 then
		Logger.log('layer3Forwarding', 'error', 'ERROR in deleteRouteObject(): has More then 2 ids: id = ' .. id)
		return false
	end

	for _, rule in ipairs(routeRules) do
		if rule then
			delRoutingrule(rule)
			class:delete(rule)
		end
	end

	return true
end

addRoutingrule = function (rule)
	if not rule then return 0 end
	if rule.enable == '0' or rule.status == "Enabled" then return 0 end

	local cmd = ''

	if (rule.destIp == '' or rule.destIp == '0.0.0.0' ) and (rule.destSubM == '' or rule.destSubM == '0.0.0.0' ) then
		cmd = 'route add default'
	else
		cmd = 'route add -net ' .. rule.destIp .. ' netmask ' .. rule.destSubM
	end

	if rule.gatewayIp ~= '' and rule.gatewayIp ~= '0.0.0.0' then
		cmd = cmd .. ' gw ' .. rule.gatewayIp
	end

	local interfaceName = cvt_IfaceName_toInternal(rule.iface)
	if interfaceName ~= '' then
		cmd = cmd .. ' dev ' .. interfaceName
	end

	if tonumber(rule.fmetric) and tonumber(rule.fmetric) >= 0 then
		cmd = cmd .. ' metric ' .. rule.fmetric
	end

	if tonumber(rule.mtu) and tonumber(rule.mtu) > 0 then
		cmd = cmd .. ' mss ' .. rule.mtu
	end

	Logger.log('layer3Forwarding', 'info', 'addRoutingrule = ' .. cmd)

	local result = os.execute(cmd)

	if result == 0 then
		rule.status = 'Enabled'
	else
		rule.status = 'Disabled'
	end
end

delRoutingrule = function (rule)
	if not rule then return 0 end
	if rule.status == "Disabled" then return 0 end

	local cmd = ''

	if (rule.destIp == '' or rule.destIp == '0.0.0.0' ) and (rule.destSubM == '' or rule.destSubM == '0.0.0.0' ) then
		cmd = 'route del default'
	else
		cmd = 'route del -net ' .. rule.destIp .. ' netmask ' .. rule.destSubM
	end

	if rule.gatewayIp ~= '' and rule.gatewayIp ~= '0.0.0.0' then
		cmd = cmd .. ' gw ' .. rule.gatewayIp
	end

	local interfaceName = cvt_IfaceName_toInternal(rule.iface)

	if interfaceName ~= '' then
		cmd = cmd .. ' dev ' .. interfaceName
	end

	Logger.log('layer3Forwarding', 'info', 'delRoutingrule = ' .. cmd)

	local result = os.execute(cmd)

	if result == 0 then
		rule.status = 'Disabled'
	end
end

needToAddIds = {}

add_Index_AddTable = function (index)
	local index = tostring(index)
	if not index then return 0 end

	local routeRules = class:getByProperty('instanceId', index)
	if #routeRules == 0 then return 0 end

	if not table.contains(needToAddIds, index) then table.insert(needToAddIds, index) end
end

add_RoutingRules_cb = function ()
	for _, idx in ipairs(needToAddIds) do
		local rules = class:getByProperty('instanceId', idx)
		Logger.log('layer3Forwarding', 'debug', 'add RoutingRule idx= ' .. idx)
		for _, rule in pairs(rules) do
			addRoutingrule(rule)
		end
	end

	needToAddIds = {}
end

-- TODO:: make this compatible
local IfaceExternal =  { wwan = conf.topRoot .. '.WANDevice.1.WANConnectionDevice.1.WANIPConnection.1',
			lan = conf.topRoot .. '.LANDevice.1.LANEthernetInterfaceConfig.1' }
cvt_IfaceName_toInternal = function (name)

	if name == IfaceExternal.lan then
		return 'br0'
	elseif name == IfaceExternal.wwan then
		return luardb.get('wwan.0.netif_udev')
	else
		return ''
	end
end


-- TODO:: make this compatible
cvt_IfaceName_toExternal = function (name)
	local wwan_if = luardb.get('wwan.0.netif_udev')

	if name == 'br0' then
		return IfaceExternal.lan
	elseif name == wwan_if then
		return IfaceExternal.wwan
	else
		return ''
	end
end

return {
	['**.ForwardNumberOfEntries'] = {
		get = function(node, name)
			local forwarding = paramTree:find('$ROOT.Layer3Forwarding.Forwarding')
			return 0, tostring(forwarding:countInstanceChildren())
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
	['**.Forwarding'] = {
		init = function(node, name)
			Logger.log('layer3Forwarding', 'debug', 'init: ' .. node:getPath() .. ': ' .. name)

			node:setAccess('readwrite')

			local routeRules = class:getByProperty('isPersist', '1')

			for _, rule in ipairs(routeRules) do
				delRoutingrule(rule)
				rule.status = 'Disabled'
				addRoutingrule(rule)
			end

			local listOccupied = {}

			local routeRules = class:getByProperty('isPersist', '0')
			for _, rule in ipairs(routeRules) do
				if rule then
					class:delete(rule)
				end
			end

			build_objectList()

			-- initial parse of leases
			local maxInstanceId = 0

			local routeRules = class:getByProperty('isPersist', '1')

			for _, rule in ipairs(routeRules) do
				local id = tonumber(rule.instanceId or 0)
				if id < 1 then
					Logger.log('layer3Forwarding', 'info', 'layer3Forwarding init: Error Persist object does not have id, destIp = ' .. (rule.destIp or '*nil*'))
				else
					Logger.log('layer3Forwarding', 'debug', 'layer3Forwarding init: creating Persist Rules: destIp = ' .. (rule.destIp or '*nil*') .. ', id = ' .. id)
					local instance = node:createDefaultChild(rule.instanceId)
					listOccupied[id] = true
					if id > maxInstanceId then maxInstanceId = id end
				end
			end

			local routeRules = class:getByProperty('isPersist', '0')
			for _, rule in ipairs(routeRules) do
				local id = 1
				for i, _ in ipairs(listOccupied) do
					id = id + 1
				end
				rule.instanceId = id

				Logger.log('layer3Forwarding', 'debug', 'layer3Forwarding init: creating Dynamic Rules: destIp = ' .. (rule.destIp or '*nil*') .. ', id = ' .. id)

				local instance = node:createDefaultChild(rule.instanceId)
				listOccupied[id] = true
				if id > maxInstanceId then maxInstanceId = id end
			end
			node.instance = maxInstanceId

			savedDigest = getMD5sum('/proc/net/route')

			if client:isTaskQueued('cleanUp', routeWatcher) ~= true then
				client:addTask('cleanUp', routeWatcher, true) -- persistent callback function
			end
			return 0
		end,
		create = function(node, name, instanceId) -- doesn't support instanceId in new tr069 client
			Logger.log('layer3Forwarding', 'info', 'createInstance: [**.Forwarding]' .. node:getPath())

			if not instanceId then
				for i=1, maxNumOfInstance do
					local instance = class:getByProperty('instanceId', tostring(i))
					if #instance == 0 then instanceId = i; break; end
				end
			end

			if not instanceId then return CWMP.Error.ResourcesExceeded end

			Logger.log('layer3Forwarding', 'info', 'createInstance: [**.Forwarding] instanceId = ' .. instanceId)

			-- create new instance object
			local instance = node:createDefaultChild(instanceId)
			if not createDefaultPersistObject(instanceId) then return CWMP.Error.InternalError end
			return 0, instanceId
		end,
-- 		delete = function(node, name)
-- 			Logger.log('layer3Forwarding', 'debug', '**.Forwarding delete, name = ' .. name)
-- 			node.parent:deleteChild(node)
-- 		end,
-- 		poll = routeWatcher
	},
	['**.Forwarding.*'] = {
-- 		create = function(node, name, instanceId)
-- 			Logger.log('layer3Forwarding', 'info', 'createInstance: [**.Forwarding.*]' .. node:getPath() .. ': ' .. name .. ', ' .. instanceId)
-- 			-- create new instance object
-- 			local instance = node:createDefaultChild(instanceId)
-- 			return 0
-- 		end,
		delete = function(node, name)
			Logger.log('layer3Forwarding', 'debug', 'delete Instance of Forwarding Object , name = ' .. name)
			node.parent:deleteChild(node)

			local pathBits = name:explode('.')
			if not deleteRouteObject(pathBits[4]) then return CWMP.Error.InternalError end
			return 0
		end,
	},
	['**.Forwarding.*.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			local objs = class:getByProperty('instanceId', pathBits[4])
			for _, obj in ipairs(objs) do
				if pathBits[5] == 'Enable' then
					node.value = obj.enable or ''
				elseif pathBits[5] == 'Status' then
					node.value = obj.status or ''
				elseif pathBits[5] == 'Type' then
					node.value = obj.type or ''
				elseif pathBits[5] == 'DestIPAddress' then
					node.value = obj.destIp or ''
				elseif pathBits[5] == 'DestSubnetMask' then
					node.value = obj.destSubM or ''
				elseif pathBits[5] == 'SourceIPAddress' then
					node.value = obj.sourceIp or ''
				elseif pathBits[5] == 'SourceSubnetMask' then
					node.value = obj.sourceSubM or ''
				elseif pathBits[5] == 'ForwardingPolicy' then
					node.value = obj.forwardPolicy or ''
				elseif pathBits[5] == 'GatewayIPAddress' then
					node.value = obj.gatewayIp or ''
				elseif pathBits[5] == 'Interface' then
					node.value = obj.iface or ''
				elseif pathBits[5] == 'ForwardingMetric' then
					node.value = obj.fmetric or ''
				elseif pathBits[5] == 'MTU' then
					node.value = obj.mtu or ''
				else
					error('Dunno how to handle ' .. name)
				end
			end
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local objs = class:getByProperty('instanceId', pathBits[4])
			for _, obj in ipairs(objs) do
				if pathBits[5] == 'Enable' then
					node.value = obj.enable or ''
				elseif pathBits[5] == 'Status' then
					node.value = obj.status or ''
				elseif pathBits[5] == 'Type' then
					node.value = obj.type or ''
				elseif pathBits[5] == 'DestIPAddress' then
					node.value = obj.destIp or ''
				elseif pathBits[5] == 'DestSubnetMask' then
					node.value = obj.destSubM or ''
				elseif pathBits[5] == 'SourceIPAddress' then
					node.value = obj.sourceIp or ''
				elseif pathBits[5] == 'SourceSubnetMask' then
					node.value = obj.sourceSubM or ''
				elseif pathBits[5] == 'ForwardingPolicy' then
					node.value = obj.forwardPolicy or ''
				elseif pathBits[5] == 'GatewayIPAddress' then
					node.value = obj.gatewayIp or ''
				elseif pathBits[5] == 'Interface' then
					node.value = obj.iface or ''
				elseif pathBits[5] == 'ForwardingMetric' then
					node.value = obj.fmetric or ''
				elseif pathBits[5] == 'MTU' then
					node.value = obj.mtu or ''
				else
					error('Dunno how to handle ' .. name)
				end
			end
			return 0, node.value
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local objs = class:getByProperty('instanceId', pathBits[4])
			local needCallBack = false
			for _, obj in ipairs(objs) do
				if not value then return CWMP.Error.InvalidParameterValue end

				if pathBits[5] == 'Enable' then
					local internalBool = convertInternalBoolean(value)
					if internalBool == nil then return CWMP.Error.InvalidParameterValue end
					if obj.enable == internalBool then return 0 end
					if obj.status == 'Enabled' and obj.enable == '1' then 
						delRoutingrule(obj)
-- 						add_Index_AddTable(pathBits[4])
-- 						needCallBack = true
					end
					if obj.status == 'Disabled' and obj.enable == '0' then 
						add_Index_AddTable(pathBits[4])
						needCallBack = true
					end
					obj.enable = internalBool

				elseif pathBits[5] == 'Status' then  -- readonly
					return 0

				elseif pathBits[5] == 'Type' then
					if value == 'Default' or value == 'Network' or value == 'Host'
					then
						obj.type = value
						return 0
					else
						return CWMP.Error.InvalidParameterValue
					end

				elseif pathBits[5] == 'DestIPAddress' then
					if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
					if obj.destIp == value then return 0 end
					if obj.status == 'Enabled' then 
						delRoutingrule(obj)
						add_Index_AddTable(pathBits[4])
						needCallBack = true
					end
					obj.destIp = value

				elseif pathBits[5] == 'DestSubnetMask' then
					if not Parameter.Validator.isValidIP4Netmask(value) then return CWMP.Error.InvalidParameterValue end
					if obj.destSubM == value then return 0 end
					if obj.status == 'Enabled' then
						delRoutingrule(obj)
						add_Index_AddTable(pathBits[4])
						needCallBack = true
					end
					obj.destSubM = value

				elseif pathBits[5] == 'SourceIPAddress' then
					if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
					obj.sourceIp = value
					return 0

				elseif pathBits[5] == 'SourceSubnetMask' then
					if not Parameter.Validator.isValidIP4Netmask(value) then return CWMP.Error.InvalidParameterValue end
					obj.sourceSubM = value
					return 0

				elseif pathBits[5] == 'ForwardingPolicy' then
					if value ~= '-1' then  return CWMP.Error.InvalidParameterValue end
					return 0

				elseif pathBits[5] == 'GatewayIPAddress' then
					if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end
					if obj.gatewayIp == value then return 0 end
					if obj.status == 'Enabled' then
						delRoutingrule(obj)
						add_Index_AddTable(pathBits[4])
						needCallBack = true
					end
					obj.gatewayIp = value

				elseif pathBits[5] == 'Interface' then
					if obj.iface == value then return 0 end
					if obj.status == 'Enabled' then
						delRoutingrule(obj)
						add_Index_AddTable(pathBits[4])
						needCallBack = true
					end
					obj.iface = value

				elseif pathBits[5] == 'ForwardingMetric' then
					value = convertInternalInteger{input=value, minimum=-1}
					if value == nil then return CWMP.Error.InvalidParameterValue end

					if obj.fmetric == tostring(value) then return 0 end
					if obj.status == 'Enabled' then
						delRoutingrule(obj)
						add_Index_AddTable(pathBits[4])
						needCallBack = true
					end
					obj.fmetric = tostring(value)

				elseif pathBits[5] == 'MTU' then
					value = convertInternalInteger{input=value, minimum=1, maximum=1540}
					if value == nil then return CWMP.Error.InvalidParameterValue end

					if obj.mtu == value then return 0 end
					if obj.status == 'Enabled' then
						delRoutingrule(obj)
						add_Index_AddTable(pathBits[4])
						needCallBack = true
					end
					obj.mtu = value

				else
					error('Dunno how to handle ' .. name)
					return 0
				end
			end

			if needCallBack == true and client:isTaskQueued('postSession', add_RoutingRules_cb) ~= true then
				client:addTask('postSession', add_RoutingRules_cb)
			end
			return 0
		end,
	},
}
