----
-- AVC Object Handler
----

local function getRDBKey(node, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['Enable']	= 'enable',
		['Status']	= 'status',
		['ID']		= 'avcid',
		['SmartEdgeAddress']	= 'peer_address',
		['MPLSTag']	= 'mpls_tag',
		['UNID']	= 'unid',
		['VID']		= 'vid',
		['CoS']		= 'cos',
		['DefaultTXCoS']	= 'default_tx_cos',
		['DefaultRXCoS']	= 'default_rx_cos',
		['PCPToCoS']	= 'pcp_to_cos',
		['DSCPToCoS']	= 'dscp_to_cos',
		['CoSToEXP']	= 'cos_to_exp',
		['CoSToDSCP']	= 'cos_to_dscp',
		['EXPToCoS']	= 'exp_to_cos',
		['CoSToPCP']	= 'cos_to_pcp',
	}
	if not fieldMapping[pathBits[#pathBits]] then
		error('AVC: unknown parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	return conf.wntd.avcPrefix .. '.' .. pathBits[#pathBits - 1] .. '.' .. fieldMapping[pathBits[#pathBits]]
end

local function getStatsRDBKey(node, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['BytesSent']		= 'greStats.txbytes',
		['BytesReceived']	= 'greStats.rxbytes',
		['FramesSent']		= 'greStats.txpkt',
		['FramesReceived']	= 'greStats.rxpkt',
		['Timestamp']		= 'greStats.stamp',
		['PoliceBytes']		= 'qdStats.bytes',
		['PoliceFrames']	= 'qdStats.frames',
		['PoliceFramesDropped']	= 'qdStats.dropped',
		['PoliceTimestamp']	= 'qdStats.stamp',
		['TrunkBytesReceived']	= 'swStats.rxbytes',
		['TrunkBytesSent']	= 'swStats.txbytes',
		['TrunkFramesReceived']	= 'swStats.rxpkt',
		['TrunkFramesSent']	= 'swStats.txpkt',
		['TrunkTimestamp']	= 'swStats.stamp'
	}
	if not fieldMapping[pathBits[#pathBits]] then
		error('AVC: unknown stats parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	return conf.wntd.avcPrefix .. '.' .. pathBits[#pathBits - 2] .. '.' .. fieldMapping[pathBits[#pathBits]]
end


local changedAVCs = {}

local function rdbChangeNofity()
	if #changedAVCs > 0 then
		dimclient.log('info', 'AVC: delivering change notifications: ' .. table.concat(changedAVCs, ','))
		luardb.lock()
		-- increment sequence number for each changed AVC
		for _, id in ipairs(changedAVCs) do
			local sequence = luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.sequence') or 0
			luardb.set(conf.wntd.avcPrefix .. '.' .. id .. '.sequence', sequence + 1)
		end
		luardb.set(conf.wntd.avcPrefix .. '.changed', table.concat(changedAVCs, ','))
		luardb.unlock()
		changedAVCs = {}
	else
		dimclient.log('info', 'AVC: no change notifications to deliver')
	end
end

local function queueChange(node, name, callback)
	local pathBits = name:explode('.')
	local id = pathBits[#pathBits - 1]
	if not tonumber(id) then id = pathBits[#pathBits] end
	callback = callback or 'postSession'

	if table.contains(changedAVCs, id) then
--		dimclient.log('info', 'AVC: change notification ' .. id .. ' already queued')
	else
		table.insert(changedAVCs, id)
		dimclient.callbacks.register(callback, rdbChangeNofity)
		dimclient.log('info', 'AVC: queued change notification ' .. id)
	end
end

local function validateParameterValue(name, value)
	local pathBits = name:explode('.')
	local paramName = pathBits[#pathBits]
	local rules = {
		['Enable'] = { '0', '1', 'true', 'false' },
		['Status'] = { 'Disabled', 'Error', 'Up' },
		['ID'] = function(val)
			if val:len() ~= 12 then return true end
		end,
		['SmartEdgeAddress'] = function(ip)
			local bits = ip:explode('.')
			if #bits ~= 4 then return true end
			for i = 1,4 do
				local octet = tonumber(bits[i]) or -1
				if octet < 0 or octet > 255 then return true end
			end
		end,
		['MPLSTag'] = function(val)
			local tag = tonumber(val) or -1
			if tag < 0 or tag > 1048575 then return true end
		end,
		['UNID'] = function(unidId)
			unidId = tonumber(unidId) or -1
			if unidId < 0 or unidId > conf.wntd.unidCount then return true end
		end,
		['VID']	= function(vid)
			vid = tonumber(vid) or -2
			if vid < -1 or vid > 4095 then return true end
		end,
		['CoS'] = function(cos)
			cos = tonumber(cos) or -1
			if cos < 0 or cos > 7 then return true end
		end,
		['DefaultTXCoS'] = function(cos)
			cos = tonumber(cos) or -1
			if cos < 0 or cos > 7 then return true end
		end,
		['DefaultRXCoS'] = function(cos)
			cos = tonumber(cos) or -1
			if cos < 0 or cos > 7 then return true end
		end,
		['PCPToCoS'] = function(pcpToCoS)
			local bits = pcpToCoS:explode(',')
			if #bits ~= 8 then return true end
			for i = 1,8 do
				local cos = tonumber(bits[i]) or -1
				if cos < 0 or cos > 7 then return true end
			end
		end,
		['DSCPToCoS'] = function(dscpToCoS)
			local bits = dscpToCoS:explode(',')
			if #bits ~= 64 then return true end
			for i = 1,64 do
				local cos = tonumber(bits[i]) or -1
				if cos < 0 or cos > 7 then return true end
			end
		end,
		['CoSToEXP'] = function(cosToEXP)
			local bits = cosToEXP:explode(',')
			if #bits ~= 8 then return true end
			for i = 1,8 do
				local exp = tonumber(bits[i]) or -1
				if exp < 0 or exp > 7 then return true end
			end
		end,
		['CoSToDSCP'] = function(cosToDSCP)
			local bits = cosToDSCP:explode(',')
			if #bits ~= 8 then return true end
			for i = 1,8 do
				local dscp = tonumber(bits[i]) or -1
				if dscp < 0 or dscp > 63 then return true end
			end
		end,
		['EXPToCoS'] = function(expToCoS)
			local bits = expToCoS:explode(',')
			if #bits ~= 8 then return true end
			for i = 1,8 do
				local cos = tonumber(bits[i]) or -1
				if cos < 0 or cos > 7 then return true end
			end
		end,
		['CoSToPCP'] = function(cosToPCP)
			local bits = cosToPCP:explode(',')
			if #bits ~= 8 then return true end
			for i = 1,8 do
				local pcp = tonumber(bits[i]) or -1
				if pcp < 0 or pcp > 7 then return true end
			end
		end,
	}
	
	local validator = rules[paramName]
	if not validator then
		error('AVC: No validator rule for parameter name "' .. paramName .. '".')
	end
	if type(validator) == 'table' then
		if not table.contains(validator, value) then return cwmpError.InvalidParameterValue end
	elseif type(validator) == 'function' then
		if validator(value) then return cwmpError.InvalidParameterValue end
	else
		error('AVC: Not sure how to handle this kind of validator? ' .. type(validator))
	end
	return 0
end

local rdbObjectClass = rdbobject.getClass(conf.wntd.avcPrefix)

return {
	----
	-- Container Collection
	----
	['**.AVC'] = {
		init = function(node, name, value)
			dimclient.log('info', 'AVC: model init')
			node:setAccess('readwrite') -- allow creation of instances
			-- create instances
			local ids = rdbObjectClass:getIds()
			for _, id in ipairs(ids) do
				dimclient.log('info', 'AVC: creating existing instance ' .. id)
				local instance = node:createDefaultChild(id)
				instance:setAccess('readwrite') -- instance can be deleted
			end
			return 0
		end,
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = function(node, name, instanceId)
			dimclient.log('info', 'AVC: create instance ' .. instanceId)
			-- create the RDB instance
			local instance = rdbObjectClass:new(instanceId)
			-- create parameter tree instance
			local treeInstance = node:createDefaultChild(instanceId)
			treeInstance:recursiveInit()
			node.instance = instanceId
			return 0
		end,
		delete = cwmpError.funcs.InvalidArgument
	},

	----
	-- Stats
	----
	['**.AVC.*.Stats'] = {
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.InvalidArgument,
		delete = cwmpError.funcs.InvalidArgument
	},
	['**.AVC.*.Stats.*'] = {
		init = function(node, name, value)
			-- skip default node instance
			if node.type == 'default' then
				node.value = node.default
				return 0
			end
			
			-- check RDB state and default if required
			node.key = getStatsRDBKey(node, name)
			node.value = luardb.get(node.key) or node.default
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.key) or node.default
			return node.value
		end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly
	},


	----
	-- Instance Object
	----
	['**.AVC.*'] = {
		init = function(node, name, value)
			node.value = node.default
			queueChange(node, name)
			return 0
		end,
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.InvalidArgument,
		delete = function(node, name)
			-- determine ID
			local pathBits = name:explode('.')
			local id = pathBits[#pathBits - 1]
			dimclient.log('info', 'AVC: delete instance ' .. id)
			-- delete the RDB instance
			local instance = rdbObjectClass:getById(id)
			rdbObjectClass:delete(instance)
			-- delete parameter tree instance
			node.parent:deleteChild(node)
			queueChange(node, name)
			return 0
		end
	},

	----
	-- Instance Fields
	----
	['**.AVC.*.*'] = {
		init = function(node, name, value)
			-- skip default node instance
			if node.type == 'default' then
				node.value = node.default
				return 0
			end
			
			-- check RDB state and default if required
			local pathBits = name:explode('.')
			local key = getRDBKey(node, name)
			node.value = luardb.get(key)
			if node.value == nil then
				dimclient.log('info', 'AVC: defaulting ' .. key .. ' to "' .. node.default .. '".')
				node.value = node.default
			end
			local ret = validateParameterValue(name, node.value)
			if ret > 0 then
				dimclient.log('error', 'AVC: value "' .. node.value .. '" for ' .. key .. ' is invalid.')
				node.value = node.default
			end
			luardb.set(key, node.value, 'p')
			
			-- install watcher for external changes
			local watcher = function(key, value)
				dimclient.log('info', 'AVC: external change of ' .. key)
				if value ~= node.value then
					dimclient.log('info', 'AVC: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
					dimclient.setParameter(node:getPath(), value)
				end
			end
			luardb.watch(key, watcher)
			queueChange(node, name, 'preSession')
			return 0
		end,
		get = function(node, name)
			if node.type ~= 'default' then
				node.value = luardb.get(getRDBKey(node, name))
			end
			return node.value
		end,
		set = function(node, name, value)
			local ret = validateParameterValue(name, value)
			if ret > 0 then return ret end

			if node.type ~= 'default' then
				node.value = value
				luardb.set(getRDBKey(node, name), value)
				queueChange(node, name)
			end
			return 0
		end
	},
}
