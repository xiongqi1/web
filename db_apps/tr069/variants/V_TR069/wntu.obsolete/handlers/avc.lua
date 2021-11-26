----
-- AVC Object Handler
----
require('Logger')
require('Daemon')
require('CWMP.Error')
require('Parameter.Validator')
require('luardb')
require('rdbevent')

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
		['CIR']		= 'cir',
		['PIR']		= 'pir',
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

local function getTCRDBKey(node, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['CIR']		= 'cir',
		['PIR']		= 'pir',
	}
	local fieldMapping2 = {
		['TC1']		= 'tc1',
		['TC2']		= 'tc2',
		['TC3']		= 'tc3',
		['TC4']		= 'tc4',
	}

	if not fieldMapping[pathBits[#pathBits]] then
		error('AVC: unknown parameter to map "' .. pathBits[#pathBits] .. 'cd .tmp	"')
	end
	return conf.wntd.avcPrefix .. '.' .. pathBits[#pathBits - 2] .. '.' ..fieldMapping2[pathBits[#pathBits-1]] ..'_'..  fieldMapping[pathBits[#pathBits]]
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

local function rdbChangeNotify()
	if #changedAVCs > 0 then
		Logger.log('Parameter', 'info', 'AVC: delivering change notifications: ' .. table.concat(changedAVCs, ','))
		luardb.lock()
		-- increment sequence number for each changed AVC
		for _, id in ipairs(changedAVCs) do
			if luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.enable') then
				local sequence = luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.sequence') or 0
				luardb.set(conf.wntd.avcPrefix .. '.' .. id .. '.sequence', sequence + 1)
			end
		end
		luardb.set(conf.wntd.avcPrefix .. '.changed', table.concat(changedAVCs, ','))
		luardb.unlock()
		changedAVCs = {}
	else
		Logger.log('Parameter', 'warning', 'AVC: no change notifications to deliver')
	end
end

local function queueChange(id)
	id = tostring(id)
	local when = client.inSession and 'postSession' or 'postInit'

	if table.contains(changedAVCs, id) then
		Logger.log('Parameter', 'debug', 'AVC: change notification ' .. id .. ' already queued')
	else
		table.insert(changedAVCs, id)
		Logger.log('Parameter', 'info', 'AVC: queued change notification ' .. id .. ' (' .. when .. ')')
	end
	if not client:isTaskQueued(when, rdbChangeNotify) then
		client:addTask(when, rdbChangeNotify)
		Logger.log('Parameter', 'info', 'AVC: installed RDB change notifier task (' .. when .. ')')
	else
		Logger.log('Parameter', 'debug', 'AVC: RDB change notifier task already installed (' .. when .. ')')
	end
end

local function validateParameterValue(name, value)
	local pathBits = name:explode('.')
	local paramName = pathBits[#pathBits]
	local rules = {
		['Enable'] = { '0', '1', 'true', 'false' },
		['Status'] = { 'Disabled', 'Error', 'Up' },
		['ID'] = function(val)
			if val:len() ~= 15 then return true end
		end,
		['SmartEdgeAddress'] = function(ip)
			if not Parameter.Validator.isValidIP4(ip) then return true end
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
		['CIR'] = function(rate)
			rate = tonumber(rate) or -1
			if rate <= 0  then return true end
		end,

		['PIR'] = function(rate)
			rate = tonumber(rate) or -1
			if rate <= 0  then return true end
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
		if not table.contains(validator, value) then return CWMP.Error.InvalidParameterValue end
	elseif type(validator) == 'function' then
		if validator(value) then return CWMP.Error.InvalidParameterValue end
	else
		error('AVC: Not sure how to handle this kind of validator? ' .. type(validator))
	end
	return 0
end

local classConfig = {
	persist = true,
	idSelection = 'sequential',
}
local rdbObjectClass = rdbobject.getClass(conf.wntd.avcPrefix, classConfig)

return {
	----
	-- Container Collection
	----
	['**.AVC'] = {
		init = function(node, name, value)
			Logger.log('Parameter', 'info', 'AVC: model init')
			node:setAccess('readwrite') -- allow creation of instances
			-- create instances
			local ids = rdbObjectClass:getIds()
			for _, id in ipairs(ids) do
				Logger.log('Parameter', 'info', 'AVC: creating existing instance ' .. id)
				local instance = node:createDefaultChild(id)
				-- we don't init the new existing AVC nodes as they will be processed
				-- after us by the main tree initialisation calling us now
			end
			return 0
		end,
		create = function(node, name)
			-- create the RDB instance
			local instance = rdbObjectClass:new()
			local instanceId = rdbObjectClass:getId(instance)
			Logger.log('Parameter', 'info', 'AVC: create instance ' .. instanceId)
			-- create parameter tree instance
			local treeInstance = node:createDefaultChild(instanceId)
			-- but here we do init the new subtree as this is run-time from an AddObject
			treeInstance:recursiveInit()
			return 0, instanceId
		end,
	},

	----
	-- Stats
	----
	['**.AVC.*.Stats'] = {
		-- nothing required
	},
	['**.AVC.*.Stats.*'] = {
		init = function(node, name, value)
			-- check RDB state and default if required
			node.key = getStatsRDBKey(node, name)
			node.value = luardb.get(node.key) or node.default
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.key) or node.default
			return 0, node.value
		end,
	},
	['**.AVC.*.TC1|TC2|TC3|TC4.*'] = {
		init = function(node, name, value)
			-- check RDB state and default if required
			node.key = getTCRDBKey(node, name)
			node.value = luardb.get(node.key)
			if node.value == nil then
				Logger.log('Parameter', 'info', 'AVC: defaulting ' .. node.key .. ' to "' .. node.default .. '".')
				node.value = node.default
			end
			local ret = validateParameterValue(name, node.value)
			if ret > 0 then
				Logger.log('Parameter', 'error', 'AVC: value "' .. node.value .. '" for ' .. node.key .. ' is invalid.')
			end
			luardb.set(node.key, node.value, 'p')

			-- install watcher for external changes
			local watcher = function(key, value)
				if value then
					Logger.log('Parameter', 'debug', 'AVC: change notification for ' .. key)
					if value ~= node.value then
						Logger.log('Parameter', 'info', 'AVC: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						client:asyncParameterChange(node, node:getPath(), value)
						node.value = value
					end
				end
			end
			rdbevent.onChange(node.key, watcher)
			-- we don't need to queueChange here
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.key) or node.default
			return 0, node.value
		end,
		set = function(node, name, value)
			luardb.set(node.key, value);
			queueChange(node.parent.parent.name)
			return 0
		end
	},

	----
	-- Instance Object
	----
	['**.AVC.*'] = {
		init = function(node, name, value)
			node:setAccess('readwrite') -- can be deleted
			node.value = node.default
			queueChange(node.name)
			return 0
		end,
		delete = function(node, name)
			local id = tonumber(node.name)
			Logger.log('Parameter', 'info', 'AVC: delete instance ' .. id)
			-- delete the RDB instance
			local instance = rdbObjectClass:getById(id)
			rdbObjectClass:delete(instance)
			-- delete parameter tree instance
			node.parent:deleteChild(node)
			queueChange(node.name)
			return 0
		end
	},

	----
	-- Instance Fields
	----
	['**.AVC.*.*'] = {
		init = function(node, name, value)
			-- check RDB state and default if required
			local pathBits = name:explode('.')
			local key = getRDBKey(node, name)
			node.value = luardb.get(key)
			if node.value == nil then
				Logger.log('Parameter', 'info', 'AVC: defaulting ' .. key .. ' to "' .. node.default .. '".')
				node.value = node.default
			end
			local ret = validateParameterValue(name, node.value)
			if ret > 0 then
				Logger.log('Parameter', 'error', 'AVC: value "' .. node.value .. '" for ' .. key .. ' is invalid.')
				-- defaulting except the avc.{i}.unid which might have '4' when the debug port was not enabled
				if string.match(key,"%.unid") == nil then 
					Logger.log('Parameter', 'info', 'AVC: defaulting ' .. key .. ' to "' .. node.default .. '".')
					node.value = node.default
				end	
			end
			luardb.set(key, node.value, 'p')

			-- install watcher for external changes
			local watcher = function(key, value)
				if value then
					Logger.log('Parameter', 'debug', 'AVC: change notification for ' .. key)
					if value ~= node.value then
						Logger.log('Parameter', 'info', 'AVC: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						client:asyncParameterChange(node, node:getPath(), value)
						node.value = value
					end
				end
			end
			rdbevent.onChange(key, watcher)
			-- we don't need to queueChange here
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(getRDBKey(node, name))
			return 0, node.value
		end,
		set = function(node, name, value)
			local ret = validateParameterValue(name, value)
			if ret > 0 then return ret end
			node.value = value
			luardb.set(getRDBKey(node, name), value)
			queueChange(node.parent.name)
			return 0
		end
	},
}
