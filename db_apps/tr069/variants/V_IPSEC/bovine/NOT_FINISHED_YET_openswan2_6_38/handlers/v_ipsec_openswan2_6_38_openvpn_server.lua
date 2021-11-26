require("handlers.hdlerUtil")
require("handlers.CGI_Iface")

require("Logger")
Logger.addSubsystem('webui_vpn_openvpn_server')

local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Networking.VPN.OpenVPN.Server.'
local g_depthOfInstance= 8

local RdbObj_config = {persist = false, idSelection = 'smallestUnused'}
local g_RdbObj_name = 'openvpnServerObj'
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
	vpn_type	= 'server',
	delflag		= '0',
}

local g_actionScript = '/usr/lib/tr-069/scripts/openvpn_action.sh '
local g_OPENVPN_KEY_DEFAULT_DIR='/usr/local/cdcs/openvpn-keys/'

local g_savedProfileMD5SUM = ''

local function init_RDBObjClass(class)
	if not class then
		Logger.log('webui_vpn_openvpn_server', 'error', 'init_RDBObjClass: rdbobject class is nil')
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
		Logger.log('webui_vpn_openvpn_server', 'error', 'delRDBVariableInstance: Invalid rdb Index: ' .. (rdbIdx or 'nil'))
		return
	end

	local devName = luardb.get(g_prefixRdb .. rdbIdx .. '.dev')

	if not devName or devName ~= g_namdOfdev then
		Logger.log('webui_vpn_openvpn_server', 'error', 'delRDBVariableInstance: Invalid index = ' .. rdbIdx)
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
		Logger.log('webui_vpn_openvpn_server', 'error', 'addRDBObjInstance: Given rdb variable index is NOT valid: ' .. (rdbIdx or 'nil'))
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
		Logger.log('webui_vpn_openvpn_server', 'error', 'delRDBObjInstance: Given rdb object index is NOT valid: ' .. (rdbobjectIdx or 'nil'))
		return
	end

	local rdbObjInstance = class:getById(rdbobjectIdx)

	if not rdbObjInstance then
		Logger.log('webui_vpn_openvpn_server', 'error', 'delRDBObjInstance: Given rdb object does Not exist: ' .. (rdbobjectIdx or 'nil'))
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
		Logger.log('webui_vpn_openvpn_server', 'error', 'Error in OpenVPN Server poller: invalid node obj')
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
	local rdbObjIndexTbl = g_class:getIds()

	if #rdbObjIndexTbl > 1 then 
		Logger.log('webui_vpn_openvpn_server', 'error', 'Error in OpenVPN Server poller: Too many rdbObject Instance')
		return
	end

	for i, rdbObjIdx in ipairs(rdbObjIndexTbl) do
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
		Logger.log('webui_vpn_openvpn_server', 'error', 'rdbObjIdx2rdbVariableIdx: invalid argument rdbObjIdx=' .. (rdbObjIdx or 'nil'))
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

local openvpn_infoTbl = nil

local function reset_openvpn_infoTbl ()
	openvpn_infoTbl = nil
end

-- return Information on Certification with table type
-- exampel:
-- openvpn_infoTbl = {
--	openvpn_dh = '1'
--	openvpn_output =''
--	server_certificate = "AU,NSW,Sydney,Netcomm,Netcomm,test@netcomm.com.au/Nov 27 05:19:38 2012 GMT,Nov 25 05:19:38 2022 GMT/"
--	client_certificate = {"02,AU,NSW,Lane Cove,Netcomm,test1,test@netcomm.com.au,Y,192.168.20.223 255.255.255.0"}
--	installed_certificate = {"AU,NSW,Sydney,Netcomm,Netcomm,test@netcomm.com.au/AU,NSW,Sydney,Netcomm,test1,test@netcomm.com.au/Nov 26 22:56:43 2012 GMT,Nov 24 22:56:43 2022 GMT"}
--	server_secret_time = ''
--	client_secret_time = ''
-- }
local function get_openvpnInfo ()
	if openvpn_infoTbl then return openvpn_infoTbl end

	openvpn_infoTbl = {}

	if client:isTaskQueued('cleanUp', reset_openvpn_infoTbl) ~= true then
		client:addTask('cleanUp', reset_openvpn_infoTbl, false)
	end


	local info = Daemon.readCommandOutput('openvpn_keygen.sh info')
	if not info then return end

	CGI_Iface.parser(openvpn_infoTbl, info)

	return openvpn_infoTbl
end

-- Formation of Input string: N="CertiName"; C="Country"; ST="State"; L="City"; O="Organisation"; E="Email";
-- return value; table = { name="CertiName", country="Country", state="State", city="City", org="Organisation", email="Email"}
local function GenArgumentParser(str)
	if not str then return '' end

	local matchingTbl = {name="N", country="C", state="ST", city="L", org="O", email="E"}
	local tbl = {}
	if not str:find(';%s*$') then
		str = str .. ';'
	end

	for key, value in pairs(matchingTbl) do
		local pattern = '[^a-zA-Z0-9@%_-%. ]+'

		tbl[key] = string.gsub ((str:match(value .. '%s*=%s*"%s*([^\"]*)"%s*;') or ''), '%s*$' , '')

		if string.find(tbl[key],pattern) then return false, "Parameter is invalid. Allowed characters: a~z A~Z 0~9 @ _ - . and space" end
	end

	if tbl.country and #tbl.country > 2 then
		tbl.country = string.match(tbl.country, '..')
	end
	return tbl
end

local function build_arguments(argList, argsName)

	if not argList or type(argList) ~= 'table' then return false, 'Invalid Argument' end
	if not argsName or type(argsName) ~= 'table' then return false, '' end

	local argStr = nil
	for _, name in ipairs(argsName) do
		if not argList[name] or argList[name] == '' then return false, 'Invalid Argument: ' .. name end
		if not argStr then 
			argStr = argList[name]
		else 
			argStr = argStr .. ',' .. argList[name]
		end
	end

	if not argStr then argStr = '' end

	return true, argStr
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
	[subROOT .. 'OpenVPNServerNumberOfEntries'] = {
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
			local rdbObjIndexTbl = g_class:getIds()

			if #rdbObjIndexTbl > 1 then 
				Logger.log('webui_vpn_openvpn_server', 'error', 'Error in OpenVPN Server init: Too many rdbObject Instance')
				return 0
			end

			for i, rdbObjIdx in ipairs(rdbObjIndexTbl) do
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
			Logger.log('webui_vpn_openvpn_server', 'info', 'createInstance: [**.OpenVPN.Server.Profiles.*]')

			local numOfChildren = node:countInstanceChildren()
			if numOfChildren > 0 then
				return CWMP.Error.ResourcesExceeded, 'Support 1 instance at most'
			end
			local dataModelIdx = addRDBObjInstance(g_class)

			if not dataModelIdx then
				Logger.log('webui_vpn_openvpn_server', 'info', 'Fail to create RDB Ojbec instance')
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
-- 			Logger.log('webui_vpn_openvpn_server', 'error', 'OpenVPN.Server profiles g_depthOfInstance = ' .. g_depthOfInstance)
			return 0
		end,
		delete = function(node, name)
			Logger.log('webui_vpn_openvpn_server', 'info', 'deleteInstance: [**.OpenVPN.Server.Profiles.*], name = ' .. name)

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
-- string:readwrite
-- Default: 10.0.0.0, Available: IP Address
-- link.profile.{i}.network_addr
	[subROOT .. 'Profiles.*.VPN_Network_Address'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'network_addr')
			if not getValue then getValue = '10.0.0.0' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			if not Parameter.Validator.isValidIP4(value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'network_addr', value)
			return 0
		end
	},
-- string:readwrite
-- Default: 255.255.255.0, Available: NetMask
-- link.profile.{i}.network_mask
	[subROOT .. 'Profiles.*.VPN_Network_SubnetMask'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'network_mask')
			if not getValue then getValue = '255.255.255.0' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			if not Parameter.Validator.isValidIP4Netmask(value) then return CWMP.Error.InvalidParameterValue end

			setRdbVariable(rdbVariableIdx, 'network_mask', value)
			return 0
		end
	},
-- string:writeonly
-- Default: , Available: 'true' - to generate new DH key
-- get method refers the result of the last attempt, set method is a trigger to generate DH key.
	[subROOT .. 'Profiles.*.Generate_DH'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('result.build_dh')
			if not retVal then retVal = '' end
			return 0, retVal
		end,
		set = function(node, name, value)
			local setvalue = value and value:lower() or ''

			if setvalue == 'true' then
				luardb.set('result.build_dh', 'Generating DH key from: ' .. (os.date('%FT%TZ') or ''))
				os.execute('openvpn_keygen.sh "init"')
			end
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
-- string:writeonly
-- Default: , Available set method format: C="Country"; ST="State"; L="City"; O="Organisation"; E="Email";
-- g_actionScript .. ' ' 'action=ca&param=,Country,State,City,Organisation,Email'
	[subROOT .. 'Profiles.*.Server_Certificates.Generate_CA'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local certiInfo = get_openvpnInfo()
			if not certiInfo then return CWMP.Error.InternalError, "Failed to get DH key" end

			if not certiInfo.openvpn_dh or certiInfo.openvpn_dh ~= '1' then return CWMP.Error.InternalError, "Error: Needed DH key to generate Server Certificates" end

			local args, msg =GenArgumentParser(value)

			if not args then return CWMP.Error.InvalidArguments, msg end
			local argsName = {'country', 'state', 'city', 'org', 'email'}

			local ret, argument = build_arguments(args, argsName)

			if not ret then return CWMP.Error.InvalidArguments, argument end

			local cmd = g_actionScript .. '"action=ca&param=,' .. argument .. '"'
			local result = os.execute(cmd)
			if not result or result ~= 0 then return CWMP.Error.InternalError, "Generate_CA failed" end
			return 0
		end
	},
-- string:readonly
-- Default: , Available: Country,State,City,Organisation,CommonName,Email/Not Before,Not After
-- get_openvpnInfo().server_certificate
-- Example: "AU,NSW,Sydney,Netcomm wireless,Netcomm wireless,test@netcomm.com.au/Nov 29 04:13:11 2012 GMT,Nov 27 04:13:11 2022 GMT/"
	[subROOT .. 'Profiles.*.Server_Certificates.Server_Certificate_Info'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local certiInfo = get_openvpnInfo()
			if not certiInfo or not certiInfo.server_certificate then return CWMP.Error.InternalError, "Failed to get Certi info" end

			local args = certiInfo.server_certificate:explode('/')
			if #args < 2 then return CWMP.Error.InvalidArguments end

			return 0, args[1] .. '/' .. args[2]
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- string:writeonly
-- Default: , Available set method format:  N="CertiName"; C="Country"; ST="State"; L="City"; O="Organisation"; E="Email";
-- g_actionScript .. ' ' 'action=genclient&param=CertiName,Country,State,City,Organisation,Email'
	[subROOT .. 'Profiles.*.Certificate_Management.Generate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local args, msg =GenArgumentParser(value)

			if not args then return CWMP.Error.InvalidArguments, msg end
			local argsName = {'name', 'country', 'state', 'city', 'org', 'email'}

			local ret, argument = build_arguments(args, argsName)

			if not ret then return CWMP.Error.InvalidArguments, argument end

			local cmd = g_actionScript .. '"action=genclient&param=' .. argument .. '"'
			local result = os.execute(cmd)
			if not result or result ~= 0 then return CWMP.Error.InternalError, "Generating Client Certificate failed" end
			return 0
		end
	},
-- string:writeonly
-- Default: , Available set parameter format: CertiName, URL (Example of URL: ftp://username:password@sample.com/path/)
-- 
	[subROOT .. 'Profiles.*.Certificate_Management.Upload'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local args = value:explode(',')
			if #args < 2 then return CWMP.Error.InvalidArguments end

			local CertiName = string.gsub( (string.gsub(args[1], '^%s*"?%s*', '') or ''), '%s*"?%s*$', '')
			local filename = g_OPENVPN_KEY_DEFAULT_DIR .. 'server/' .. (CertiName or '') .. '.tgz'
			local url = string.gsub( (string.gsub(args[2], '^%s*"?%s*', '') or ''), '%s*"?%s*$', '')

			if not hdlerUtil.IsRegularFile(filename) then return CWMP.Error.InternalError, "Certificate file not exist" end

			local ret, msg = Daemon.uploadFileToURL(url, filename)

			if ret then
				return  CWMP.Error.InternalError, (msg or '')
			end
			return 0
		end
	},
-- string:writeonly
-- Default: , Available: CertiName
-- g_actionScript .. ' ' 'action=rmclient&param=CertiName'
	[subROOT .. 'Profiles.*.Certificate_Management.Revoke'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local cmd = g_actionScript .. '"action=rmclient&param=' .. value .. '"'
			local result = os.execute(cmd)
			if not result or result ~= 0 then return CWMP.Error.InvalidArguments end
			return 0
		end
	},
-- string:writeonly
-- Default: , Available set parameter format: "CertiName", "Network Address", "Network Mask"
-- g_actionScript .. ' ' 'action=setclientnw&param=certiName,nwaddr,nwmask'
	[subROOT .. 'Profiles.*.Certificate_Management.Set_Network_Information'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local args = value:explode(',')
			if #args < 3 then return CWMP.Error.InvalidArguments end

			local certiName = string.gsub( (string.gsub(args[1], '^%s*"?%s*', '') or ''), '%s*"?%s*$', '')
			local nwaddr = string.gsub( (string.gsub(args[2], '^%s*"?%s*', '') or ''), '%s*"?%s*$', '')
			local nwmask = string.gsub( (string.gsub(args[3], '^%s*"?%s*', '') or ''), '%s*"?%s*$', '')

			if not Parameter.Validator.isValidIP4(nwaddr) then return CWMP.Error.InvalidParameterValue, 'Invalid Network Address' end
			if not Parameter.Validator.isValidIP4Netmask(nwmask) then return CWMP.Error.InvalidParameterValue, 'Invalid Network Mask' end

			local cmd = g_actionScript .. '"action=setclientnw&param=' .. certiName .. ',' ..  nwaddr .. ',' .. nwmask .. '"'
			local result = os.execute(cmd)
			if not result or result ~= 0 then return CWMP.Error.InvalidArguments end

			return 0
		end
	},
-- string:readonly
-- Default: , Available: [CertiName,Country,State,City,Organisation,Email,Revoked,[NetworkAddress NetworkMask]], [...]
-- get_openvpnInfo().client_certificate
-- Example: [test1,AU,NSW,Lane Cove,Netcomm,test@netcomm.com.au,Y,], [ ... ]
	[subROOT .. 'Profiles.*.Certificate_Management.Client_Certificates_Info'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local certiInfo = get_openvpnInfo()
			if not certiInfo or not certiInfo.client_certificate then return CWMP.Error.InternalError, "Failed to get Certification info" end
			if type(certiInfo.client_certificate) ~= 'table' then return CWMP.Error.InternalError, "Failed to get Certification info" end

			local returnValue = nil

			for _, value in ipairs(certiInfo.client_certificate) do
				local infoTbl = value:explode(',')

				if #infoTbl > 8 then
					infoTbl[1] = infoTbl[6]
					table.remove(infoTbl, 6)
					local infoStr = table.concat(infoTbl, ',')

					if not returnValue then 
						returnValue = '[' .. infoStr .. ']'
					else 
						returnValue = returnValue .. ', [' .. infoStr .. ']'
					end
				end
			end

			if not returnValue then returnValue = '' end

			return 0, returnValue
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- string:readwrite
-- Default: , Available: 
-- link.profile.{i}.user
	[subROOT .. 'Profiles.*.Username_Password.Username'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'user')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'user', value)
			return 0
		end
	},
-- string:readwrite
-- Default: , Available: 
-- link.profile.{i}.pass
	[subROOT .. 'Profiles.*.Username_Password.Password'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InternalError, 'Instance does not exist' end

			local getValue = getRdbVariable(rdbVariableIdx, 'pass')
			if not getValue then getValue = '' end
			return 0, getValue
		end,
		set = function(node, name, value)
			local rdbVariableIdx = nodeName2rdbVariableIdx(name)

			if not rdbVariableIdx then return CWMP.Error.InvalidArguments, 'Instance does not exist' end

			setRdbVariable(rdbVariableIdx, 'pass', value)
			return 0
		end
	},
-- TODO: description
-- string:writeonly
-- Default: , Available set parameter format: URL (Example of URL: ftp://username:password@sample.com/path/)
-- 
	[subROOT .. 'Profiles.*.Username_Password.Upload_CA'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local url = string.gsub( (string.gsub(value, '^%s*"?%s*', '') or ''), '%s*"?%s*$', '')
			local filename = g_OPENVPN_KEY_DEFAULT_DIR .. 'server/ca.tgz'

			if not hdlerUtil.IsRegularFile(filename) then return CWMP.Error.InternalError, "Certificate file not exist" end

			local ret, msg = Daemon.uploadFileToURL(url, filename)

			if ret then
				return  CWMP.Error.InternalError, (msg or '')
			end
			return 0
		end
	},
-- TODO: description
-- string:readwrite
-- Default: , Available set parameter format: "Network Address", "Network Mask"
-- g_actionScript .. ' ' 'action=setclientnw&param=ca,nwaddr,nwmask'
	[subROOT .. 'Profiles.*.Username_Password.Set_Network_Information'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local args = value:explode(',')
			if #args < 2 then return CWMP.Error.InvalidArguments end

			local certiName = 'ca'
			local nwaddr = string.gsub( (string.gsub(args[1], '^%s*"?%s*', '') or ''), '%s*"?%s*$', '')
			local nwmask = string.gsub( (string.gsub(args[2], '^%s*"?%s*', '') or ''), '%s*"?%s*$', '')

			if not Parameter.Validator.isValidIP4(nwaddr) then return CWMP.Error.InvalidParameterValue, 'Invalid Network Address' end
			if not Parameter.Validator.isValidIP4Netmask(nwmask) then return CWMP.Error.InvalidParameterValue, 'Invalid Network Mask' end

			local cmd = g_actionScript .. '"action=setclientnw&param=' .. certiName .. ',' ..  nwaddr .. ',' .. nwmask .. '"'
			local result = os.execute(cmd)
			if not result or result ~= 0 then return CWMP.Error.InvalidArguments end

			return 0
		end
	},
-- TODO: description
-- string:readonly
-- Default: , Available set parameter format: "Network Address", "Network Mask"
-- g_actionScript .. ' ' 'action=setclientnw&param=ca,nwaddr,nwmask'
	[subROOT .. 'Profiles.*.Username_Password.Network_Information'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local certiInfo = get_openvpnInfo()
			if not certiInfo or not certiInfo.server_certificate then return CWMP.Error.InternalError, "Failed to get Certi info" end

			local args = certiInfo.server_certificate:explode('/')
			if #args < 3 then return 0, '' end

			return 0, args[3]
		end,
		set = function(node, name, value)
			return 0
		end
	},
}
