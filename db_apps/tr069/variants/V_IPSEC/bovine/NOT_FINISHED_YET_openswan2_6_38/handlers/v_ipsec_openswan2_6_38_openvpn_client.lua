require("handlers.hdlerUtil")
 
require("Logger")
Logger.addSubsystem('webui_vpn_openvpn_client')

local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Networking.VPN.OpenVPN.Client.'
local g_depthOfInstance= 8

local RdbObj_config = {persist = false, idSelection = 'smallestUnused'}
local g_RdbObj_name = 'openvpnClientObj'
local g_class = rdbobject.getClass(g_RdbObj_name, RdbObj_config)

local g_rdbTemplateTrigger = 'openvpn.0.restart'
local g_namdOfdev = 'openvpn.0'
local g_prefixRdb = 'link.profile.'

local g_rdbVariables_configDefult = {
	['enable']		= '0',
	['name']		= 'New Profile',
	['network_addr']	= '10.0.0.0',
	['network_mask']	= '255.255.255.0',
	['vpn_authtype']	= '0',
	['user']		= '',
	['pass']		= '',
	['serveraddress']	= '',
	['serverport']		= '1194',
	['serverporttype']	= 'UDP',
	['local_ipaddr']	= '0.0.0.0',
	['remote_ipaddr']	= '0.0.0.0',
	['remote_nwaddr']	= '0.0.0.0',
	['remote_nwmask']	= '0.0.0.0',
	['certi']		= '',
	['defaultgw']		= '0',
}

local g_rdbVariables_internalDefault = {
	dev		= g_namdOfdev,
	vpn_type	= 'client',
	delflag		= '0',
}

local g_savedProfileMD5SUM = ''

local function init_RDBObjClass(class)
	if not class then
		Logger.log('webui_vpn_openvpn_client', 'error', 'init_RDBObjClass: rdbobject class is nil')
		return
	end

	local allInstances = class:getAll()

	for _, inst in ipairs(allInstances) do
		class:delete(inst)
	end

	for i, value in hdlerUtil.traverseRdbVariable{prefix=g_prefixRdb , suffix='.dev', startIdx=0} do
		if value == g_namdOfdev then
			local delflag = luardb.get(g_prefixRdb .. i .. '.delflag')
			local vpn_type = luardb.get(g_prefixRdb .. i .. '.vpn_type')

			if (not delflag or delflag ~= '1') and vpn_type and vpn_type == g_rdbVariables_internalDefault.vpn_type then
				local newInst = class:new()
				newInst.rdbVariableIdx = i
			end
		end
	end

end

-- return: number type
--	available rdb variable object index
local function get_newprofilenum()
	local availableIdx = 0

	for i, value in hdlerUtil.traverseRdbVariable{prefix=g_prefixRdb, suffix='.dev', startIdx=0} do
		availableIdx = i
	end

	availableIdx = availableIdx + 1

	return availableIdx
end

-- return: number type
--	new rdb variable object index
local function addRDBVariableInstance()
	local newIdx = get_newprofilenum()

	for key, value in pairs(g_rdbVariables_configDefult) do
		if value ~= '' then
			luardb.set(g_prefixRdb .. newIdx .. '.' .. key, value, 'p')
		else
			luardb.set(g_prefixRdb .. newIdx .. '.' .. key, '', 'p')
		end
	end

	for key, value in pairs(g_rdbVariables_internalDefault) do
		if value ~= '' then
			luardb.set(g_prefixRdb .. newIdx .. '.' .. key, value, 'p')
		else
			luardb.set(g_prefixRdb .. newIdx .. '.' .. key, '', 'p')
		end
	end

	return newIdx
end

local function delRDBVariableInstance(rdbIdx)
	if not rdbIdx or not tonumber(rdbIdx) then
		Logger.log('webui_vpn_openvpn_client', 'error', 'delRDBVariableInstance: Invalid rdb Index: ' .. (rdbIdx or 'nil'))
		return
	end

	local devName = luardb.get(g_prefixRdb .. rdbIdx .. '.dev')

	if not devName or devName ~= g_namdOfdev then
		Logger.log('webui_vpn_openvpn_client', 'error', 'delRDBVariableInstance: Invalid index = ' .. rdbIdx)
		return
	end

	luardb.set(g_prefixRdb .. rdbIdx .. '.name', '')
	luardb.set(g_prefixRdb .. rdbIdx .. '.enable', '0')
	luardb.set(g_prefixRdb .. rdbIdx .. '.delflag', '1')
end

-- if rdbIdx is nil, create new rdb variable instance and use its rdb variable index
-- if rdbIdx has value, use it to create rdb obj instance
-- return:
--	Failure: nil
--	Success: index of RDB object instance with number type
local function addRDBObjInstance(class, rdbIdx)
	local addrdbIdx = nil
	if not rdbIdx then
		addrdbIdx = addRDBVariableInstance()
	elseif not tonumber(rdbIdx) then
		Logger.log('webui_vpn_openvpn_client', 'error', 'addRDBObjInstance: Given rdb variable index is NOT valid: ' .. (rdbIdx or 'nil'))
		return
	else
		addrdbIdx = rdbIdx
	end

	local newInst = class:new()
	newInst.rdbVariableIdx = addrdbIdx

	return class:getId(newInst)
end

-- delete given indexed rdbobject instance and involved rdb variable instance
-- class: rdbobject class object
-- rdbobjectIdx: Index of rdbobject to delete
-- notDeleteRDBVariables: boolean argument. 
-- 			  true:		Do Not delete involved rdb variables
-- 			  false of nil:	Delete involved rdb variables
local function delRDBObjInstance(class, rdbobjectIdx, notDeleteRDBVariables)
	if not rdbobjectIdx or not tonumber(rdbobjectIdx) then
		Logger.log('webui_vpn_openvpn_client', 'error', 'delRDBObjInstance: Given rdb object index is NOT valid: ' .. (rdbobjectIdx or 'nil'))
		return
	end

	local rdbObjInstance = class:getById(rdbobjectIdx)

	if not rdbObjInstance then
		Logger.log('webui_vpn_openvpn_client', 'error', 'delRDBObjInstance: Given rdb object does Not exist: ' .. (rdbobjectIdx or 'nil'))
		return
	end

	local rdbVariableIdx = rdbObjInstance.rdbVariableIdx

	class:delete(rdbObjInstance)

	if not notDeleteRDBVariables then
		delRDBVariableInstance(rdbVariableIdx)
	end
end

-- This function gets indexes given dev and returns sorted array of number typed indexes.
local function getIndexes_RDBVariable()
	local index_array = {}

	for i, value in hdlerUtil.traverseRdbVariable{prefix=g_prefixRdb , suffix='.dev', startIdx=0} do
		if value == g_namdOfdev then
			local delflag = luardb.get(g_prefixRdb .. i .. '.delflag')
			local vpn_type = luardb.get(g_prefixRdb .. i .. '.vpn_type')

			if (not delflag or delflag ~= '1') and vpn_type and vpn_type == g_rdbVariables_internalDefault.vpn_type then
				table.insert(index_array, (tonumber(i) or 0))
			end
		end
	end
	table.sort(index_array)
	return index_array
end

local function getMD5sum()
	local indexesArr = getIndexes_RDBVariable()
	local strIndexes = table.concat(indexesArr, ',')
	strIndexes = '[' .. (strIndexes or '') .. ']'

	return strIndexes
end

local function poller(task)
	local currentMD5sum = getMD5sum()

	if g_savedProfileMD5SUM == currentMD5sum then return end

	g_savedProfileMD5SUM = currentMD5sum

	init_RDBObjClass(g_class)

	local node = task.data

	if not node or not node.children then
		Logger.log('webui_vpn_openvpn_client', 'error', 'Error in OpenVPN Client poller: invalid node obj')
		return
	end

	-- delete all of children data model object
	for _, child in ipairs(node.children) do
		if child.name ~= '0' then
			child.parent:deleteChild(child)
		end
	end
	node.instance = 0

	local maxInstanceId = 0
	local rdbOjbIndexTbl = g_class:getIds()

	for i, rdbObjIdx in ipairs(rdbOjbIndexTbl) do
		local id = tonumber(rdbObjIdx or 0)

		if id > 0 then
			local dataModelInstance = node:createDefaultChild(id)
			if id > maxInstanceId then maxInstanceId = id end
		end
	end

	node.instance = maxInstanceId
end

-- return number typed rdb Variable index
local function rdbObjIdx2rdbVariableIdx(rdbObjIdx)
	if not rdbObjIdx or not tonumber(rdbObjIdx) then
		Logger.log('webui_vpn_openvpn_client', 'error', 'rdbObjIdx2rdbVariableIdx: invalid argument rdbObjIdx=' .. (rdbObjIdx or 'nil'))
		return
	end

	local rdbobjectInstance = g_class:getById(rdbObjIdx)

	return rdbobjectInstance.rdbVariableIdx
end

-- return number typed rdb Variable index or nil
local function nodeName2rdbVariableIdx(name)
	if not name then return end

	local pathBits = name:explode('.')
	local dataModelIdx = pathBits[g_depthOfInstance]

	if not dataModelIdx or not tonumber(dataModelIdx) then return end

	return rdbObjIdx2rdbVariableIdx(dataModelIdx)
end

local function getRdbVariable(rdbVariableIdx, rdbVariableName)
	if not rdbVariableIdx or not tonumber(rdbVariableIdx) or not rdbVariableName then return end

	return luardb.get(g_prefixRdb .. rdbVariableIdx .. '.' .. rdbVariableName)
end

local triggeredRdbVariableIdx = nil

local function triggerRdbVariable()
	if not triggeredRdbVariableIdx then return end

	for _, value in ipairs(triggeredRdbVariableIdx) do
		if value and tonumber(value) then
			luardb.set(g_rdbTemplateTrigger, '1')
			break;
		end
	end

	triggeredRdbVariableIdx = nil
end

local function registerCallback(rdbVariableIdx)
	if not rdbVariableIdx then return end
	if not triggeredRdbVariableIdx then triggeredRdbVariableIdx = {} end

	if not table.contains(triggeredRdbVariableIdx, rdbVariableIdx) then
		table.insert(triggeredRdbVariableIdx, rdbVariableIdx)
	end

	if client:isTaskQueued('postSession', triggerRdbVariable) ~= true then
		client:addTask('postSession', triggerRdbVariable, false)
	end
end

-- rdbVariableIdx: index of rdb variables
-- rdbVariableName: surfix of rdb variables.
-- newValue: new value to set, should be string type
-- ignoreTrigger: if true, won't trigger triggerRdbVariable() function
--		if false or nil, will trigger triggerRdbVariable() function

local function setRdbVariable(rdbVariableIdx, rdbVariableName, newValue, ignoreTrigger)
	if not rdbVariableIdx or not tonumber(rdbVariableIdx) or not rdbVariableName or not newValue then return end

	local currentValue = luardb.get(g_prefixRdb .. rdbVariableIdx .. '.' .. rdbVariableName)

	if currentValue == tostring(newValue) then return end

	luardb.set(g_prefixRdb .. rdbVariableIdx .. '.' .. rdbVariableName, newValue)

	if not ignoreTrigger then
		registerCallback(rdbVariableIdx)
	end
end


return {
--[[
-- bool: readwrite
	[subROOT .. 'path'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
--]]

-- uint:readonly
	[subROOT .. 'OpenVPNClientNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local nodeOfCollectionRoot = paramTree:find(subROOT .. 'Profiles')
			return 0, tostring(nodeOfCollectionRoot:countInstanceChildren())
		end,
		set = function(node, name, value)
			return 0
		end
	},

	[subROOT .. 'Profiles'] = {
		init = function(node, name, value)
			node:setAccess('readwrite')

			init_RDBObjClass(g_class)

			local maxInstanceId = 0
			local rdbOjbIndexTbl = g_class:getIds()

			for i, rdbObjIdx in ipairs(rdbOjbIndexTbl) do
				local id = tonumber(rdbObjIdx or 0)

				if id > 0 then
					local dataModelInstance = node:createDefaultChild(id)
					if id > maxInstanceId then maxInstanceId = id end
				end
			end

			node.instance = maxInstanceId

			g_savedProfileMD5SUM = getMD5sum()

			if client:isTaskQueued('preSession', poller) ~= true then
				client:addTask('preSession', poller, true, node) -- persistent callback function
			end
			return 0
		end,
		create = function(node, name)
			Logger.log('webui_vpn_openvpn_client', 'info', 'createInstance: [**.OpenVPN.Client.Profiles.*]')

			local dataModelIdx = addRDBObjInstance(g_class)

			if not dataModelIdx then
				Logger.log('webui_vpn_openvpn_client', 'info', 'Fail to create RDB Ojbec instance')
				return CWMP.Error.InternalError
			end

			g_savedProfileMD5SUM = getMD5sum()

			-- create new data model instance object
			node:createDefaultChild(dataModelIdx)
			return 0, dataModelIdx
		end,
	},

	[subROOT .. 'Profiles.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfInstance = #pathBits
-- 			Logger.log('webui_vpn_openvpn_client', 'error', 'OpenVPN.Client profiles g_depthOfInstance = ' .. g_depthOfInstance)
			return 0
		end,
		delete = function(node, name)
			Logger.log('webui_vpn_openvpn_client', 'info', 'deleteInstance: [**.OpenVPN.Client.Profiles.*], name = ' .. name)

			node.parent:deleteChild(node)

			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfInstance]
			delRDBObjInstance(g_class, dataModelIdx)

			g_savedProfileMD5SUM = getMD5sum()
			return 0
		end,
	},
-- bool:readwrite
-- Default: 0, Available: 0|1
-- link.profile.{i}.enable
	[subROOT .. 'Profiles.*.Enable_Profile'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'enable')
			if not getValue or (getValue ~= '0' and getValue ~= '1') then getValue = '0' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			setRdbVariable(rdbVariableIdx, 'enable', setVal)
			return 0
		end
	},
-- string:readwrite
-- Default: , Available: 
-- link.profile.{i}.name
	[subROOT .. 'Profiles.*.Profile_Name'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'name')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'name', value, true)
			return 0
		end
	},
-- string:readwrite
-- Default: , Available: IP Address
-- link.profile.{i}.serveraddress
	[subROOT .. 'Profiles.*.Server_IP_Address'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'serveraddress')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'serveraddress', value)
			return 0
		end
	},
-- uint:readwrite
-- Default: 1194, Available: 
-- link.profile.{i}.serverport
	[subROOT .. 'Profiles.*.Server_Port'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'serverport')
			if not getValue and not tonumber(getValue) then getValue = '1194' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = hdlerUtil.ToInternalInteger{input=value, minimum=0, maximum=65535}

			if not value then return CWMP.Error.InvalidArguments end

			setRdbVariable(rdbVariableIdx, 'serverport', value)
			return 0
		end
	},
-- string:readwrite
-- Default: UDP, Available: UDP|TCP
-- link.profile.{i}.serverporttype
	[subROOT .. 'Profiles.*.Server_Protocol'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'serverporttype')
			if not getValue then getValue = 'UDP' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:upper()

			local availValue = {'UDP', 'TCP'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'serverporttype', value)
			return 0
		end
	},
-- bool:readwrite
-- Default: 0, Available: 0|1
-- link.profile.{i}.defaultgw
	[subROOT .. 'Profiles.*.Use_Default_Gateway'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'defaultgw')
			if not getValue or (getValue ~= '0' and getValue ~= '1') then getValue = '0' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			setRdbVariable(rdbVariableIdx, 'defaultgw', setVal)
			return 0
		end
	},
-- bool:readwrite
-- Default: 0, Available: 0(Certificat)|1(Username/Password)
-- link.profile.{i}.vpn_authtype
	[subROOT .. 'Profiles.*.Authentication_Type'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'vpn_authtype')
			if not getValue or (getValue ~= '0' and getValue ~= '1') then getValue = '0' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			setRdbVariable(rdbVariableIdx, 'vpn_authtype', setVal)
			return 0
		end
	},
}
