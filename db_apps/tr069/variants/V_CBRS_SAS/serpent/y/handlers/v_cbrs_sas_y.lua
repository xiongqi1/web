
--[[
	SAS handler
	Copyright (C) 2021 NetComm Wireless Limited.
--]]


require("Logger")
require("stringutil")
-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

-- the following prefixes are supported
local prefixes = {conf.topRoot .. '.' .. xVendorPrefix .. '.Services.SAS.'}

local logSubsystem = 'Services.SAS'
Logger.addSubsystem(logSubsystem)
------------------local function prototype------------------
------------------------------------------------------------


------------------local variable definition ----------------

--This is the number of default child nodes that we will initially create at the beginning(SAS.Grant.1 .. SAS.Grant.5).
--If we get more than 5 grants then additional new nodes will be created internally.
local g_numOfGrantDefaultChildNodes = 5

--Currently SAS client defines 20 as the max. number of grants that we could possible have.
local g_numOfGrantInstance = 15

--This corresponds to the child node index(Device.X_CASASYSTEMS.Services.SAS.Grant.*)
local g_depthOfGrantInstance = 5
------------------------------------------------------------


------------------local function definition ----------------
------------------------------------------------------------


local function populateCellInfo(parentNode,cellType)
	local handle = io.popen("rdb dump sas.cell")
	local cellInfoRdb = handle:read('*all')
	handle:close()

	local cellInfoTbl = cellInfoRdb:explode('sas.cell.')
	for index=1,#cellInfoTbl,1 do
		if cellInfoTbl[index]:match(cellType) then
			Pci = cellInfoTbl[index]:explode('.')[1]
	        end
	end

	local cellInfo = {}
	cellInfo.Authorized=luardb.get(string.format("sas.cell.%s.authorized", Pci))
	cellInfo.Pci=luardb.get(string.format("sas.cell.%s.pci", Pci))
	cellInfo.PciEarfcn=luardb.get(string.format("sas.cell.%s.pci_earfcn", Pci))
	cellInfo.RequiredGrantIdx=luardb.get(string.format("sas.cell.%s.grants", Pci))
	cellInfo.FreqRange=luardb.get(string.format("sas.cell.%s.freq", Pci))
	cellInfo.EcgiList=luardb.get(string.format("sas.cell.%s.ecgi_list", Pci))


	for i,v in pairs(cellInfo) do
		Logger.log(logSubsystem, 'debug', 'cellInfo.' .. i .. ': ' ..v)
	end

	for _, param in ipairs(parentNode.children) do
		if param.name == 'Authorized' then
			param.value = cellInfo.Authorized
		elseif param.name == 'Pci' then
			param.value = cellInfo.Pci
		elseif param.name == 'PciEarfcn' then
			param.value = cellInfo.PciEarfcn
		elseif param.name == 'RequiredGrantIdx' then
			param.value = cellInfo.RequiredGrantIdx
		elseif param.name == 'FreqRange' then
			param.value = cellInfo.FreqRange
		elseif param.name == 'EcgiList' then
			param.value = cellInfo.EcgiList
		else
			Logger.log(logSubsystem, 'error', 'Unknown param name: '..param.name)
		end
	end
end

-- @param - childNode : Child node in the 'Grant' subtree (Device.X_CASASYSTEMS.Services.SAS.Grant.x). Range - [0 .. g_numOfGrantInstance]
-- @param - grantIdx  : Corresponds to an RDB index which has valid grant info. Range - [0 .. g_numOfGrantInstance+1]
-- @Desc              : Fills up the child node (Device.X_CASASYSTEMS.Services.SAS.Grant.*) with the information from grant database
local function populateNode(childNode, grantIdx)
	local grantInfo = {}
	grantInfo.id = luardb.get(string.format("sas.grant.%d.id",grantIdx)) or 'N/A'
	grantInfo.state = luardb.get(string.format("sas.grant.%d.state",grantIdx)) or 'N/A'
	grantInfo.freqlow = luardb.get(string.format("sas.grant.%d.freq_range_low",grantIdx)) or 'N/A'
	grantInfo.freqhigh = luardb.get(string.format("sas.grant.%d.freq_range_high",grantIdx)) or 'N/A'
	grantInfo.requiredfor = luardb.get(string.format("sas.grant.%d.grantRequiredFor",grantIdx)) or 'N/A'
	grantInfo.nextheartbeat = luardb.get(string.format("sas.grant.%d.next_heartbeat",grantIdx)) or 'N/A'
	grantInfo.expiretime = luardb.get(string.format("sas.grant.%d.expire_time",grantIdx)) or 'N/A'
	grantInfo.channeltype = luardb.get(string.format("sas.grant.%d.channel_type",grantIdx)) or 'N/A'
	grantInfo.eirp = luardb.get(string.format("sas.grant.%d.max_eirp",grantIdx)) or 'N/A'

	for _, param in ipairs(childNode.children) do
		if param.name == 'Id' then
			param.value = grantInfo.id
		elseif param.name == 'State' then
			param.value = grantInfo.state
		elseif param.name == 'FreqLow' then
			param.value = grantInfo.freqlow
		elseif param.name == 'FreqHigh' then
			param.value = grantInfo.freqhigh
		elseif param.name == 'RequiredFor' then
			param.value = grantInfo.requiredfor
		elseif param.name == 'NextHeartbeat' then
			param.value = grantInfo.nextheartbeat
		elseif param.name == 'ExpireTime' then
			param.value = grantInfo.expiretime
		elseif param.name == 'ChannelType' then
			param.value = grantInfo.channeltype
		elseif param.name == 'Eirp' then
			param.value = grantInfo.eirp
		else
			Logger.log(logSubsystem, 'error', 'Unknown param name: '..param.name)
		end
	end
end

-- @Desc	: Returns the total number of grants that are currently available. This includes grants that are in
--		  GRANTED and AUTHORIZED states.
local function getGrantCount()
	local grantCnt = 0
	for grantIdx=1,g_numOfGrantInstance do
		local cmd = string.format("sas.grant.%d.state", grantIdx)
		local val = luardb.get(cmd) or ""
		if val ~= "" then
			grantCnt = grantCnt + 1
		end
	end
	return grantCnt
end

-- @param - childNode	: Child node in the 'Grant' subtree (Device.X_CASASYSTEMS.Services.SAS.Grant.x).
-- @param - grantNodeIdx: Corresponds to the child node's index in the grant subtree. Range - [1 .. g_numOfGrantInstance]
-- @Desc		: Fills up the child node (Device.X_CASASYSTEMS.Services.SAS.Grant.*) with the information from grant database.
--			  If there are 3 active grants in RDB datbase (sas.grant.0, sas.grant.4 and sas.grant.5) and if user requests
--			  for Device.X_CASASYSTEMS.Services.SAS.Grant.2 then we will return the grant info sas.grant.4, because that
--			  is the second grant out of the 3 active grants that we have.
--			  Whereas, if the user requests for Device.X_CASASYSTEMS.Services.SAS.Grant.4 we will return "N/A" as there are
--			  only 3 active grants present. We will *not return* the info present in sas.grant.4 !
--			  Note that there is no direct mapping between Device.X_CASASYSTEMS.Services.SAS.Grant.x and the database's sas.grant.x.

local function getGrantInfo(childNode, grantNodeIdx)
	local grantCnt = 0

	--Reset the node before updating with latest info.
	populateNode(childNode,g_numOfGrantInstance+1)

	--Update the data model with the latest info from the grant database(sas.grant.x.*)
	for grantIdx=1,g_numOfGrantInstance do
		local cmd = string.format("sas.grant.%d.state", grantIdx)
		local val = luardb.get(cmd) or ""
		if val ~= "" then
			grantCnt = grantCnt + 1
			if grantCnt == grantNodeIdx then
				Logger.log(logSubsystem, 'debug', 'getGrantInfo: Found grant info in the database')
				populateNode(childNode, grantIdx)
				return grantNodeIdx
			end
		end
	end
	Logger.log(logSubsystem, 'debug', 'getGrantInfo: No grant info found in the database')
end

-- @param - nodeHead	: This is the head node of the grant subtree (Device.X_CASASYSTEMS.Services.SAS.Grant).
-- @Desc		: Creates new child node(s) if we have more grants and fills up them with the available
--			  grant infro from grant database(sas.grant.x). By default we create 5 child nodes,
--			  Device.X_CASASYSTEMS.Services.SAS.Grant.1 .. Device.X_CASASYSTEMS.Services.SAS.Grant.5,
--			  and display up to 5 active grants. If, we get more grants then we create addiotnal nodes
--			  and populate the grant info. The additionally created nodes for the new grants will not be
--			  displayed in the ACS server side, until the user clicks refresh on the grant subtree -
--			  Device.X_CASASYSTEMS.Services.SAS.Grant
local function createNewNode(nodeHead)
	local grantCnt = getGrantCount()
	local noOfChildren = nodeHead:countInstanceChildren()
	if grantCnt > noOfChildren then
		Logger.log(logSubsystem, 'debug', 'grantCnt: ' .. grantCnt .. ' and noOfChildren: ' .. noOfChildren)
		Logger.log(logSubsystem, 'debug', 'Create additional nodes for extra grants')
		for instId=noOfChildren+1,grantCnt do
			Logger.log(logSubsystem, 'debug', 'Creating new child node ' .. instId)
			local childNode = nodeHead:createDefaultChild(instId)
			childNode:recursiveInit()
		end
	end
end

local function isSasRegistered()
	if string.trim(luardb.get('service.sas_client.enable')) ~= '1' or
		string.trim(luardb.get('sas.registration.state')) ~= 'Registered' then
		return 0
	else
		return 1
	end
end

local handlers = {

-- =====[START] SAS==================================

-- instances:readonly
	['NumberOfGrants'] = {
		init = function(node, name, value) return 0 end,
		get  = function(node, name)
			local returnVal
			local grantCnt = 0

			if isSasRegistered() ~= 1 then
				return 0, 'N/A'
			end

			grantCnt = getGrantCount()
			returnVal=tostring(grantCnt)
			Logger.log(logSubsystem, 'debug', 'NumberOfGrants.get() - total no. of grants - ' .. grantCnt)
			return 0, returnVal
		end,
		set  = function(node, name, value) return 0 end,
	},
	['ServingCell.*'] = {
		init = function(node, name, value) return 0 end,
		get  = function(node, name)
			local retVal

			if isSasRegistered() ~= 1 then
				return 0, 'N/A'
			end

			populateCellInfo(node.parent,"ServingCell")

			Logger.log(logSubsystem, 'debug', 'ServingCell.*.get() - name - ' .. name .. ' and paramValue - ' .. node.value)

			retVal = node.value
			return 0, retVal

		end,
		set  = function(node, name, value) return 0 end,
	},
	['NeighborCell.*'] = {
		init = function(node, name, value) return 0 end,
		get  = function(node, name)
			local retVal

			if isSasRegistered() ~= 1 then
				return 0, 'N/A'
			end

			populateCellInfo(node.parent,"Neighbor")

			Logger.log(logSubsystem, 'debug', 'ServingCell.*.get() - name - ' .. name .. ' and paramValue - ' .. node.value)

			retVal = node.value
			return 0, retVal

		end,
		set  = function(node, name, value) return 0 end,
	},
-- object:readonly
	['Grant'] = {
		init = function(node, name, value)
			for instId = 1,g_numOfGrantDefaultChildNodes do
				Logger.log(logSubsystem, 'debug', 'Grant.init() - Create default child node - ' .. instId)
				local childNode = node:createDefaultChild(instId)
				childNode:recursiveInit()
				--Initialize the grant parameters with N/A
				populateNode(childNode,g_numOfGrantInstance+1)
			end
			return 0
		end,
	},


-- instances:readonly
	['Grant.*'] = {
		init = function(node, name, value) return 0 end,
	},

-- instances:readonly
-- Involved RDB variable: "sas.grant.x.state"
	['Grant.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal

			if isSasRegistered() ~= 1 then
				return 0, 'N/A'
			end

			local pathBits = name:explode('.')
			local grantNodeIdx = tonumber(pathBits[g_depthOfGrantInstance+1] or 1) or 1
			Logger.log(logSubsystem, 'debug', 'Grant.*.*.get() - Fetching value for grant-' .. grantNodeIdx)
			getGrantInfo(node.parent, grantNodeIdx)

			Logger.log(logSubsystem, 'debug', 'Grant.*.*.get() - name - ' .. name .. ' and paramValue - ' .. node.value)

			--Create additional child nodes(SAS.Grant.*) if needed for extra grants that we may have got
			createNewNode(node.parent.parent)

			retVal = node.value
			return 0, retVal
		end,
		set = function(node, name, value) return 0 end
	},
-- =====[END] SAS==================================
}


local ret = {}
for _, prefix in ipairs(prefixes) do
	for key, handler in pairs(handlers) do
		ret[prefix .. key] = handler
	end
end

return ret
