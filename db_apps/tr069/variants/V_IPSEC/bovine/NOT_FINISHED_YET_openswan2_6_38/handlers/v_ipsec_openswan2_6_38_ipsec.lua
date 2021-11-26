require("handlers.hdlerUtil")
 
require("Logger")
Logger.addSubsystem('webui_vpn_ipsec')

local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Networking.VPN.IPsec.'
local g_depthOfInstance= 7

local RdbObj_config = {persist = false, idSelection = 'smallestUnused'}
local g_RdbObj_name = 'ipsecObj'
local g_class = rdbobject.getClass(g_RdbObj_name, RdbObj_config)

local g_rdbTemplateTrigger = 'ipsec.0.profile'
local g_namdOfdev = 'ipsec.0'
local g_prefixRdb = 'link.profile.'

--------------------------------------------------------------------
local g_IPSEC_KEY_DIR = "/etc/ipsec.d"
local g_IPSEC_CERTS_DIR = g_IPSEC_KEY_DIR .. "/certs"
local g_IPSEC_CACERTS_DIR = g_IPSEC_KEY_DIR .. "/cacerts"
local g_IPSEC_CRLCERTS_DIR = g_IPSEC_KEY_DIR .. "/crls"
local g_IPSEC_PRIVATEKEY_DIR = g_IPSEC_KEY_DIR .. "/private"
local g_IPSEC_RSAKEY_DIR = g_IPSEC_KEY_DIR .. "/rsakey"
--------------------------------------------------------------------

local g_rdbVariables_configDefult = {
	enable		= '0',
	name		= 'New Profile',
	remote_gateway	= '0.0.0.0',
	remote_lan	= '0.0.0.0',
	remote_mask	= '255.255.255.0',
	local_lan	= '0.0.0.0',
	local_mask	= '255.255.255.0',
	enccap_protocol	= 'esp',
	ike_mode	= 'main',
	pfs		= 'on',
	ike_enc		= 'any',
	ike_hash	= 'any',
	ipsec_enc	= 'any',
	ipsec_hash	= 'any',
	ipsec_dhg	= 'any',
	ipsec_dpd	= 'hold',
	dpd_time	= '10',
	dpd_timeout	= '60',
	ike_time	= '3600',
	life_time	= '28800',
	ipsec_method	= 'psk',
	psk_value	= '',
	psk_remoteid	= '',
	psk_localid	= '',
	rsa_remoteid	= '',
	rsa_localid	= '',
	key_password	= '',
}

local g_rdbVariables_internalDefault = {
	dev		= g_namdOfdev,
	delflag		= '0',
}

local g_savedProfileMD5SUM = ''

local function init_RDBObjClass(class)
	if not class then
		Logger.log('webui_vpn_ipsec', 'error', 'init_RDBObjClass: rdbobject class is nil')
		return
	end

	local allInstances = class:getAll()

	for _, inst in ipairs(allInstances) do
		class:delete(inst)
	end

	for i, value in hdlerUtil.traverseRdbVariable{prefix=g_prefixRdb , suffix='.dev', startIdx=0} do
		if value == g_namdOfdev then
			local delflag = luardb.get(g_prefixRdb .. i .. '.delflag')

			if delflag and delflag == '0' then
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
		Logger.log('webui_vpn_ipsec', 'error', 'delRDBVariableInstance: Invalid rdb Index: ' .. (rdbIdx or 'nil'))
		return
	end

	local devName = luardb.get(g_prefixRdb .. rdbIdx .. '.dev')

	if not devName or devName ~= g_namdOfdev then
		Logger.log('webui_vpn_ipsec', 'error', 'delRDBVariableInstance: Invalid index = ' .. rdbIdx)
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
		Logger.log('webui_vpn_ipsec', 'error', 'addRDBObjInstance: Given rdb variable index is NOT valid: ' .. (rdbIdx or 'nil'))
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
		Logger.log('webui_vpn_ipsec', 'error', 'delRDBObjInstance: Given rdb object index is NOT valid: ' .. (rdbobjectIdx or 'nil'))
		return
	end

	local rdbObjInstance = class:getById(rdbobjectIdx)

	if not rdbObjInstance then
		Logger.log('webui_vpn_ipsec', 'error', 'delRDBObjInstance: Given rdb object does Not exist: ' .. (rdbobjectIdx or 'nil'))
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

			if delflag and delflag == '0' then
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
		Logger.log('webui_vpn_ipsec', 'error', 'Error in IPsec poller: invalid node obj')
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
		Logger.log('webui_vpn_ipsec', 'error', 'rdbObjIdx2rdbVariableIdx: invalid argument rdbObjIdx=' .. (rdbObjIdx or 'nil'))
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
			luardb.set(g_rdbTemplateTrigger, value)
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

local function fileValidationCheck(rdbVariableIdx, dirName, filePrefix, checkStr)
	if not rdbVariableIdx or not tonumber(rdbVariableIdx) or not dirName or not filePrefix  then return end

	local fileExt_available = {'crt', 'pem', 'key'}
	local validation = "Not uploaded" -- "Uploaded but not valid", "Uploaded"

	for _, ext in ipairs(fileExt_available) do
		local fullname = dirName .. '/' .. filePrefix .. rdbVariableIdx .. '.' .. ext
		if hdlerUtil.IsRegularFile(fullname) then
			validation = "Uploaded but not valid"
			if not checkStr or os.execute('grep -q "' .. checkStr .. '" ' .. fullname) == 0 then
				validation = "Uploaded"
			end
			break
		end
	end

	return validation
end

local function certFileInstall(rdbVariableIdx, dirName, filePrefix, url)
	if not rdbVariableIdx or not tonumber(rdbVariableIdx) or not dirName or not filePrefix then 
		return false, 'certFileInstall: Invalid Function Argument'
	end

	if not url then return false, 'certFileInstall: URL is nil' end

	local urlTbl = socket.url.parse(url)

	if not urlTbl.scheme or (urlTbl.scheme ~= 'ftp' and urlTbl.scheme ~= 'http') then
		return false, 'Do not support the protocol ' .. (urlTbl.scheme or 'nil')
	end

	if not urlTbl.host or not urlTbl.path then
		return false, 'Given URL is not valid'
	end

	local fileExt_available = {'key', 'pem', 'crt'}

	local ext = urlTbl.path:match('%.([^.]+)$')
	if not ext or not table.contains(fileExt_available, ext) then
		return false, 'Error: Wrong Type of File!'
	end

	local localDir = dirName .. '/' .. filePrefix .. rdbVariableIdx .. '.' .. ext
	local transferResult = os.execute('wget -q -O ' .. localDir .. ' ' .. url)

	if transferResult ~= 0 then
		return false, 'Certificate File install failed. Error Code: ' .. (transferResult or 'nil')
	end

	os.execute('install -d -m 0700 ' .. dirName)
	os.execute('chmod 0600 ' .. localDir)
	return true
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
	[subROOT .. 'IPsecNumberOfEntries'] = {
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
			Logger.log('webui_vpn_ipsec', 'info', 'createInstance: [**.IPsec.Profiles.*]')

			local dataModelIdx = addRDBObjInstance(g_class)

			if not dataModelIdx then
				Logger.log('webui_vpn_ipsec', 'info', 'Fail to create RDB Ojbec instance')
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
-- 			Logger.log('webui_vpn_ipsec', 'error', 'IPsec profiles g_depthOfInstance = ' .. g_depthOfInstance)
			return 0
		end,
		delete = function(node, name)
			Logger.log('webui_vpn_ipsec', 'info', 'deleteInstance: [**.IPsec.Profiles.*], name = ' .. name)

			node.parent:deleteChild(node)

			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfInstance]
			delRDBObjInstance(g_class, dataModelIdx)

			g_savedProfileMD5SUM = getMD5sum()
			return 0
		end,
	},
-- bool:readwrite
-- Default: 0, Avaliable: 0|1
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
-- Default: ""
-- link.profile.{i}.name
	[subROOT .. 'Profiles.*.Profile_name'] = {
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
-- Default: 0.0.0.0, Avaliable: IP Address
-- link.profile.{i}.remote_gateway
	[subROOT .. 'Profiles.*.Remote_IPSec_server_address'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'remote_gateway')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'remote_gateway', value)
			return 0
		end
	},
-- string:readwrite
-- Default: 0.0.0.0, Avaliable: IP Address
-- link.profile.{i}.remote_lan
	[subROOT .. 'Profiles.*.Remote_LAN_address'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'remote_lan')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'remote_lan', value)
			return 0
		end
	},
-- string:readwrite
-- Default: 255.255.255.0, Avaliable: NetMask
-- link.profile.{i}.remote_mask
	[subROOT .. 'Profiles.*.Remote_LAN_subnet_mask'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'remote_mask')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			if not Parameter.Validator.isValidIP4Netmask(value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'remote_mask', value)
			return 0
		end
	},
-- string:readwrite
-- Default: 0.0.0.0, Avaliable: IP Address
-- link.profile.{i}.local_lan
	[subROOT .. 'Profiles.*.Local_LAN_address'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'local_lan')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'local_lan', value)
			return 0
		end
	},
-- string:readwrite
-- Default: 255.255.255.0, Avaliable: NetMask
-- link.profile.{i}.local_mask
	[subROOT .. 'Profiles.*.Local_LAN_subnet_mask'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'local_mask')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			if not Parameter.Validator.isValidIP4Netmask(value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'local_mask', value)
			return 0
		end
	},
-- string:readwrite
-- Default: esp, Avaliable: any|esp|ah
-- link.profile.{i}.enccap_protocol
	[subROOT .. 'Profiles.*.Encapsulation_type'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'enccap_protocol')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'any', 'esp', 'ah'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'enccap_protocol', value)
			return 0
		end
	},
-- string:readwrite
-- Default: main, Avaliable: any|main|aggressive
-- link.profile.{i}.ike_mode
	[subROOT .. 'Profiles.*.IKE_mode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ike_mode')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'any', 'esp', 'aggressive'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'ike_mode', value)
			return 0
		end
	},
-- bool:readwrite
-- Default: 1, Avaliable: 0|1
-- link.profile.{i}.pfs [on (1), off (0)]
	[subROOT .. 'Profiles.*.PFS'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'pfs')
			if not getValue then getValue = 'on' end

			if getValue == 'on' then
				getValue = '1'
			else
				getValue = '0'
			end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			if setVal == '1' then
				setVal = 'on'
			else
				setVal = 'off'
			end
			setRdbVariable(rdbVariableIdx, 'pfs', setVal)
		end
	},
-- string:readwrite
-- Default: any, Avaliable: any|aes|aes128|aes192|aes256|3des|des
-- link.profile.{i}.ike_enc
	[subROOT .. 'Profiles.*.IKE_encryption'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ike_enc')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'any', 'aes', 'aes128', 'aes192', 'aes256', '3des', 'des'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'ike_enc', value)
			return 0
		end
	},
-- string:readwrite
-- Default: any, Avaliable: any|md5|sha1
-- link.profile.{i}.ike_hash
	[subROOT .. 'Profiles.*.IKE_hash'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ike_hash')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'any', 'md5', 'sha1'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'ike_hash', value)
			return 0
		end
	},
-- string:readwrite
-- Default: Any, Avaliable: any|aes|aes128|aes192|aes256|3des|des
-- link.profile.{i}.ipsec_enc
	[subROOT .. 'Profiles.*.IPSec_encryption'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ipsec_enc')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'any', 'aes', 'aes128', 'aes192', 'aes256', '3des', 'des'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'ipsec_enc', value)
			return 0
		end
	},
-- string:readwrite
-- Default: any, Avaliable: any|md5|sha1
-- link.profile.{i}.ipsec_hash
	[subROOT .. 'Profiles.*.IPSec_hash'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ipsec_hash')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'any', 'md5', 'sha1'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'ipsec_hash', value)
			return 0
		end
	},
-- string:readwrite
-- Default: any, Avaliable: any|modp768|modp1024|modp1536|modp2048|modp3072|modp4096|modp6144|modp8192
-- link.profile.{i}.ipsec_dhg
	[subROOT .. 'Profiles.*.DH_group'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ipsec_dhg')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'any', 'modp768', 'modp1024', 'modp1536', 'modp2048', 'modp3072', 'modp4096', 'modp6144', 'modp8192'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'ipsec_dhg', value)
			return 0
		end
	},
-- string:readwrite
-- Default: hold, Avaliable: none|clear|hold|restart
-- link.profile.{i}.ipsec_dpd
	[subROOT .. 'Profiles.*.DPD_action'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ipsec_dpd')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'none', 'clear', 'hold', 'restart'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'ipsec_dpd', value)
			return 0
		end
	},
-- uint:readwrite
-- Default: 10, Unit: secs
-- link.profile.{i}.dpd_time
	[subROOT .. 'Profiles.*.DPD_keep_alive_time'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'dpd_time')
			if not getValue and not tonumber(getValue) then getValue = '10' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = hdlerUtil.ToInternalInteger{input=value, minimum=0}

			if not value then return CWMP.Error.InvalidArguments end

			setRdbVariable(rdbVariableIdx, 'dpd_time', value)
			return 0
		end
	},
-- uint:readwrite
-- Default: 60, Unit: secs
-- link.profile.{i}.dpd_timeout
	[subROOT .. 'Profiles.*.DPD_timeout'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'dpd_timeout')
			if not getValue and not tonumber(getValue) then getValue = '60' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = hdlerUtil.ToInternalInteger{input=value, minimum=0}

			if not value then return CWMP.Error.InvalidArguments end

			setRdbVariable(rdbVariableIdx, 'dpd_timeout', value)
			return 0
		end
	},
-- uint:readwrite
-- Default: 3600, Avaliable: 0-78400, 0=Unlimited,  Unit: secs
-- link.profile.{i}.ike_time
	[subROOT .. 'Profiles.*.IKE_rekey_time'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ike_time')
			if not getValue and not tonumber(getValue) then getValue = '3600' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = hdlerUtil.ToInternalInteger{input=value, minimum=0, maximum=78400}

			if not value then return CWMP.Error.InvalidArguments end

			setRdbVariable(rdbVariableIdx, 'ike_time', value)
			return 0
		end
	},
-- uint:readwrite
-- Default: 28800, Avaliable: 0-78400, 0=Unlimited,  Unit: secs
-- link.profile.{i}.life_time
	[subROOT .. 'Profiles.*.SA_life_time'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'life_time')
			if not getValue and not tonumber(getValue) then getValue = '28800' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = hdlerUtil.ToInternalInteger{input=value, minimum=0, maximum=78400}

			if not value then return CWMP.Error.InvalidArguments end

			setRdbVariable(rdbVariableIdx, 'life_time', value)
			return 0
		end
	},
-- string:readwrite
-- Default: psk, Avaliable: psk(preSharedKeys), rsa(RSA keys), cert(Certificates)
-- link.profile.{i}.ipsec_method
	[subROOT .. 'Profiles.*.Key_mode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'ipsec_method')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			value = value:lower()

			local availValue = {'psk', 'rsa', 'cert'}
			if not value or not table.contains(availValue, value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'ipsec_dpd', value)
			return 0
		end
	},

-------------------------------------------------------------------------------------------------------
-- string:readwrite
-- link.profile.{i}.psk_value
	[subROOT .. 'Profiles.*.PreSharedKey.PreSharedKey'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'psk_value')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'psk_value', value)
			return 0
		end
	},
-- string:readwrite
-- link.profile.{i}.psk_remoteid
	[subROOT .. 'Profiles.*.PreSharedKey.Remote_Id'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'psk_remoteid')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'psk_remoteid', value)
			return 0
		end
	},
-- string:readwrite
-- link.profile.{i}.psk_localid
	[subROOT .. 'Profiles.*.PreSharedKey.Local_Id'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'psk_localid')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'psk_localid', value)
			return 0
		end
	},
-- string:readwrite
-- link.profile.{i}.rsa_remoteid
	[subROOT .. 'Profiles.*.RASKeys.Remote_Id'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'rsa_remoteid')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'rsa_remoteid', value)
			return 0
		end
	},
-- string:readwrite
-- link.profile.{i}.rsa_localid
	[subROOT .. 'Profiles.*.RASKeys.Local_Id'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'rsa_localid')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'rsa_localid', value)
			return 0
		end
	},
-- TODO:
-- string:writeonly
-- 
	[subROOT .. 'Profiles.*.RASKeys.Generate_Local_RSA_key'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			return 0, luardb.get('result.' .. rdbVariableIdx .. '.rsa.keygen') or ''
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal or setVal ~= '1' then return CWMP.Error.InvalidArguments end

			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local result = os.execute('pgrep _pluto_adns || pgrep pluto')
			if not result or result ~= 0 then
				luardb.set('result.' .. rdbVariableIdx .. '.rsa.keygen', 'Failure: Ipsec process is not running')
				return CWMP.Error.InternalError, 'IPsec process is not running'
			end

			local command = '/usr/lib/tr-069/scripts/ipsec_action.sh'
			local argument = 'action=genrsa&param=' .. rdbVariableIdx
			luardb.set('result.' .. rdbVariableIdx .. '.rsa.keygen', 'Generating Local RAS Key')
			os.execute(command .. ' "' .. argument .. '"&')
			return 0
		end
	},
-- string:readwrite
-- /etc/ipsec.d/rsakey/leftrsa{i}.key" --> g_IPSEC_RSAKEY_DIR .. '/' .. leftrsa{i}.key
	[subROOT .. 'Profiles.*.RASKeys.Upload_Local_RSA_key'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local result = fileValidationCheck(rdbVariableIdx, g_IPSEC_RSAKEY_DIR, 'leftrsa')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local ret, message = certFileInstall(rdbVariableIdx, g_IPSEC_RSAKEY_DIR, 'leftrsa', value)

			if not ret then
				return CWMP.Error.InternalError, (message or 'nil')
			end
			return 0
		end
	},
-- string:readwrite
-- /etc/ipsec.d/rsakey/rightrsa{i}.key --> g_IPSEC_RSAKEY_DIR .. '/' .. rightrsa{i}.key
	[subROOT .. 'Profiles.*.RASKeys.Upload_Remote_RSA_key'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local result = fileValidationCheck(rdbVariableIdx, g_IPSEC_RSAKEY_DIR, 'rightrsa')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local ret, message = certFileInstall(rdbVariableIdx, g_IPSEC_RSAKEY_DIR, 'rightrsa', value)

			if not ret then
				return CWMP.Error.InternalError, (message or 'nil')
			end
			return 0
		end
	},
-- string:readwrite
-- link.profile.{i}.key_password
	[subROOT .. 'Profiles.*.Certificates.Private_Key_Pass_Phrase'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'key_password')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'key_password', value)
			return 0
		end
	},
-- string:readwrite
-- /etc/ipsec.d/private/local{i}.[key|pem] --> g_IPSEC_PRIVATEKEY_DIR .. '/' .. local{i}.[key|pem]
	[subROOT .. 'Profiles.*.Certificates.Upload_Local_Private_Key'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local result = fileValidationCheck(rdbVariableIdx, g_IPSEC_PRIVATEKEY_DIR, 'local', 'BEGIN RSA PRIVATE KEY')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local ret, message = certFileInstall(rdbVariableIdx, g_IPSEC_PRIVATEKEY_DIR, 'local', value)

			if not ret then
				return CWMP.Error.InternalError, (message or 'nil')
			end
			return 0
		end
	},
-- string:readwrite
-- /etc/ipsec.d/certs/local{i}.crt --> g_IPSEC_CERTS_DIR  .. '/' .. local{i}.crt
	[subROOT .. 'Profiles.*.Certificates.Upload_Local_Public_Certificate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local result = fileValidationCheck(rdbVariableIdx, g_IPSEC_CERTS_DIR, 'local', 'BEGIN CERTIFICATE')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local ret, message = certFileInstall(rdbVariableIdx, g_IPSEC_CERTS_DIR, 'local', value)

			if not ret then
				return CWMP.Error.InternalError, (message or 'nil')
			end
			return 0
		end
	},
-- string:readwrite
-- /etc/ipsec.d/certs/remote{i}.crt --> g_IPSEC_CERTS_DIR  .. '/' .. remote{i}.crt
	[subROOT .. 'Profiles.*.Certificates.Upload_Remote_Public_Certificate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local result = fileValidationCheck(rdbVariableIdx, g_IPSEC_CERTS_DIR, 'remote', 'BEGIN CERTIFICATE')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local ret, message = certFileInstall(rdbVariableIdx, g_IPSEC_CERTS_DIR, 'remote', value)

			if not ret then
				return CWMP.Error.InternalError, (message or 'nil')
			end
			return 0
		end
	},
-- string:readwrite
-- /etc/ipsec.d/cacerts/ca{i}.crt --> g_IPSEC_CACERTS_DIR .. '/' .. ca{i}.crt
	[subROOT .. 'Profiles.*.Certificates.Upload_CA_Certificate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local result = fileValidationCheck(rdbVariableIdx, g_IPSEC_CACERTS_DIR, 'ca', 'BEGIN CERTIFICATE')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local ret, message = certFileInstall(rdbVariableIdx, g_IPSEC_CACERTS_DIR, 'ca', value)

			if not ret then
				return CWMP.Error.InternalError, (message or 'nil')
			end
			return 0
		end
	},
-- string:readwrite
-- /etc/ipsec.d/crls/crl{i}.crt --> g_IPSEC_CRLCERTS_DIR .. '/' .. crl{i}.crt
	[subROOT .. 'Profiles.*.Certificates.Upload_CRL_Certificate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local result = fileValidationCheck(rdbVariableIdx, g_IPSEC_CRLCERTS_DIR, 'crl', 'BEGIN CERTIFICATE')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local ret, message = certFileInstall(rdbVariableIdx, g_IPSEC_CRLCERTS_DIR, 'crl', value)

			if not ret then
				return CWMP.Error.InternalError, (message or 'nil')
			end
			return 0
		end
	},

}
