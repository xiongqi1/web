-- #!/usr/bin/env lua
require('stringutil')
require('tableutil')

local debug = conf.wntd.debug;

debug=1



local dot1agClassConfig = { 
	persist = false, -- avoid to cleanup invalid AVCs
	idSelection = 'manual'
}


local dot1agObjectClass = rdbobject.getClass(conf.wntd.dot1agPrefix,dot1agClassConfig)

--
local dot1ag_rmp_ObjectClass={}


local dot1ag_max_session=conf.wntd.dot1ag_max_session
local dot1ag_rmp_max_session=32


--- dot1ag object cannot be deleted
local disable_delete_operation=true


------------------------------------------------
--- mapping from ACS name to rdb
local mda_rdb_mapping={
	["__rdb_name__"] = "mda",
	---			{ type, access, rdb last name, persistent or not ,low limit, high limit
	["PeerMode"] = {"bool","rw","peermode", 1},
	["MdLevel"] = {"uint","rw","mdlevel", 1, 0, 7},
	["PrimaryVid"] = {"uint","rw","primaryvid", 1, 0, 4095},
	["MDFormat"] = {"uint","rw","mdformat", 1, 1, 4},
	["MdIdType2or4"] = {"string","rw","mdidtype2or4", 1},
	["MaFormat"] = {"uint","rw","maformat", 1, 1, 4},
	["MaIdType2"] = {"string","rw","maidtype2", 1},
	["MaIdNonType2"] = {"uint","rw","maidnontype2", 1},
	["CCMInterval"] = {"uint","rw","ccminterval", 1, 0, 7},
	["lowestAlarmPri"] = {"uint","rw","lowestalarmpri", 1, 1, 6},
	["AVCID"] = {"string","rw","avcid", 1},
	["GetStats"] = {"bool","rw","getstats", 0},
	["errorCCMdefect"] = {"bool","r","errorccmdefect", 0},
	["xconCCMdefect"] = {"bool","r","xconccmdefect", 0},
	["CCMsequenceErrors"] = {"uint","r","ccmsequenceerrors", 0},
	["fngAlarmTime"] = {"uint","rw","fngalarmtime", 1, 250, 10000},
	["fngResetTime"] = {"uint","rw","fngresettime", 1, 250, 10000},
	["someRMEPCCMdefect"] = {"uint","r","somermepccmdefect", 0},
	["someMACstatusDefect"] = {"uint","r","somemacstatusdefect", 0},
	["someRDIdefect"] = {"uint","r","somerdidefect", 0},
	["highestDefect"] = {"uint","r","highestdefect", 0},
	["fngState"] = {"uint","r","fngstate", 0},
	["Status"] = {"string","r","status", 0},
}


local lmp_rdb_mapping = {
	["__rdb_name__"] = "lmp",
	["MEPactive"] = {"bool","rw","mepactive", 1},
	["mpid"] = {"uint","rw","mpid", 1, 0, 8191},
	["direction"] = {"int","rw","direction", 1, -1, 2},
	["port"] = {"uint","rw","port", 1},
	["vid"] = {"uint","rw","vid", 1, 0, 4095},
	["vidtype"] = {"uint","rw","vidtype", 1, 0, 2},
	["macAdr"] = {"string","r","macadr", 1},
	["CCMenable"] = {"bool","rw","ccmenable", 1},
	["CCIsentCCMs"] = {"uint","r","ccisentccms", 0},
	["xconnCCMdefect"] = {"uint","r","xconnccmdefect", 0},
	["errorCCMdefect"] = {"uint","r","errorccmdefect", 0},
	["CCMsequenceErrors"] = {"uint","r","ccmsequenceerrors", 0},
	["TxCounter"] = {"uint","r","txcounter", 0},
	["RxCounter"] = {"uint","r","rxcounter", 0},
	["Status"] = {"string","r","status", 0},
	["CoS"] = {"uint","rw","cos", 1, 0, 7},
	["LBRsInOrder"] = {"uint","r","lbrsinorder", 0},
	["LBRsOutOfOrder"] = {"uint","r","lbrsoutoforder", 0},
	["LBRnoMatch"] = {"uint","r","lbrnomatch", 0},
	["LBRsTransmitted"] = {"uint","r","lbrstransmitted", 0},
	["LTRsUnexpected"] = {"uint","r","ltrsunexpected", 0},
}

local ltm_rdb_mapping = {
	["__rdb_name__"] = "lmp.ltm",
	
	["send"] = {"bool","rw","send", 0},
	["rmpid"] = {"uint","rw","rmpid", 1, 0, 8191},
	["destmac"] = {"string","rw","destmac", 1},
	["flag"] = {"uint","rw","flag", 1, 0, 255},
	["ttl"] = {"uint","rw","ttl", 1, 0, 255},
	["timeout"] = {"uint","rw","timeout", 1},
	["LTMtransID"] = {"uint","rw","ltmtransid", 0, 1, ""},
	["Status"] = {"string","r","status", 0},
};
local ltr_rdb_mapping =  {
	["__rdb_name__"] = "lmp.ltr",

	["ltmTransId"] = {"uint","r","ltmtransid", 0},
	["rmpid"] = {"string","r","rmpid", 0},
	["srcmac"] = {"string","r","srcmac", 0},
	["flag"] = {"string","r","flag", 0},
	["relayaction"] = {"string","r","relayaction", 0},
	["ttl"] = {"string","r","ttl", 0},
};
local lbm_rdb_mapping = {
	["__rdb_name__"] = "lmp.lbm",

	["LBMsToSend"] = {"uint","rw","lbmstosend", 0, 0, 64},
	["rmpid"] = {"uint","rw","rmpid", 1, 0, 8191},
	["destmac"] = {"string","rw","destmac", 1},
	["timeout"] = {"uint","rw","timeout", 1},
	["rate"] = {"uint","rw","rate", 1},
	["LBMtransID"] = {"uint","rw","lbmtransid", 0, 1, ""},
	["TLVDataLen"] = {"uint","rw","tlvdatalen", 1, 0, 1480},
	["Status"] = {"string","r","status", 0},
};


local  rmp_rdb_mapping = {
	["__rdb_name__"] = "rmp",
	["mpid"] = {"uint","rw","mpid", 1, 0, 8191},
	["ccmDefect"] = {"bool","r","ccmdefect", 0},
	["lastccmRDI"] = {"bool","r","lastccmrdi", 0},
	["lastccmPortState"] = {"uint","r","lastccmportstate", 0},
	["lastccmIFStatus"] = {"uint","r","lastccmifstatus", 0},
	["lastccmSenderID"] = {"uint","r","lastccmsenderid", 0},
	["lastccmmacAddr"] = {"string","r","lastccmmacaddr", 0},
	["TxCounter"] = {"uint","r","txcounter", 0},
	["RxCounter"] = {"uint","r","rxcounter", 0},
	["Curr"] = {"string","r","curr", 0},
	["Accum"] = {"string","r","accum", 0},
	["Ratio"] = {"string","r","ratio", 0},
}
-----------------------------------Y1731


local  y_mda_rdb_mapping = {
	["__y_name__"] = "mda",
	["Enable"] = {"bool","rw","enable", 1},
	--["AVCID"] = {"string","rw","avcid", 1},
	["MegLevel"] = {"uint","rw","meglevel", 1, 0, 7},
	["MegId"] = {"string","rw","megid", 1},
	["MegIdFormat"] = {"uint","rw","megidformat", 1, 1, 32},
	["MegIdLength"] = {"uint","rw","megidlength", 1, 0, 45},
	--["someRDIdefect"] = {"uint","r","somerdidefect", 0},
	["ALMSuppressed"] = {"bool","r","almsuppressed", 0},
	--["Status"] = {"string","r","status", 0},
	["__dot1ag_list__"] = {
		["AVCID"] = {"string","rw","avcid",1},
		["someRDIdefect"] = {"uint","r","somerdidefect",0},
		["Status"] = {"string","r","status",0},
	},
}

local  y_lmp_rdb_mapping = {
	["__y_name__"] = "lmp",
	["Cos"] = {"uint","rw","cos", 1, 0, 7},
	--["Status"] = {"string","r","status", 0},
	["__dot1ag_list__"] = {
		["Status"] = {"string","r","status",0},
	},
}

local y_dmm_rdb_mapping = {
	["__y_name__"] = "lmp.dmm",
	["rmpid"] = {"uint","rw","rmpid", 1, 0, 8191},
	["destmac"] = {"string","rw","destmac", 1},
	["timeout"] = {"uint","rw","timeout", 1},
	["Send"] = {"uint","rw","send", 0, 0, 65536},
	["Rate"] = {"uint","rw","rate", 1},
	["Type"] = {"uint","rw","type", 1, 0, 1},
	["TLVDataLen"] = {"uint","rw","tlvdatalen", 1, 0, 1446},
	["Dly"] = {"string","r","dly", 0},
	["DlyAvg"] = {"string","r","dlyavg", 0},
	["DlyMin"] = {"string","r","dlymin", 0},
	["DlyMax"] = {"string","r","dlymax", 0},
	["Var"] = {"string","r","var", 0},
	["VarAvg"] = {"string","r","varavg", 0},
	["VarMin"] = {"string","r","varmin", 0},
	["VarMax"] = {"string","r","varmax", 0},
	["Count"] = {"uint","r","count", 0},
	["Status"] = {"string","r","status", 0},
}




local y_slm_rdb_mapping = {
	["__y_name__"] = "lmp.slm",
	["rmpid"] = {"uint","rw","rmpid", 1, 0, 8191},
	["destmac"] = {"string","rw","destmac", 1},
	["timeout"] = {"uint","rw","timeout", 1},
	["Send"] = {"uint","rw","send", 0, 0, 65536},
	["Rate"] = {"uint","rw","rate", 1},
	["TestID"] = {"string","rw","testid", 0},
	["TLVDataLen"] = {"uint","rw","tlvdatalen", 1, 0, 1460},
	["Curr"] = {"string","r","curr", 0},
	["Accum"] = {"string","r","accum", 0},
	["Ratio"] = {"string","r","ratio", 0},
	["Count"] = {"string","r","count", 0},
	["Status"] = {"string","r","status", 0},


}

if dot1ag_max_session ==1 then	
	lmp_rdb_mapping["macAdr"] = {"string","rw","macadr", 1}
end

------------------------------------------------
local function dinfo( ...)
	if debug >1 then
		local  printResult="";
		for i = 1, select('#', ...) do
			local v = select(i, ...)
			printResult = printResult .. tostring(v) .. "\t"
		 end
		Logger.log('Parameter', 'info', printResult)
	end
end



local function dump_node( node)
	if debug >1 then
		dinfo('node dump');
		dinfo('name=', node.name)
		dinfo('path='.. node:getPath())
		dinfo('type='.. node.type)
		dinfo('value='.. node.value)
	end
end

------------------------------------------------
-- parse a path name
--- return id, last name
--InternetGatewayDevice.X_NETCOMM.Dot1ag.1.Mda.AVCID => 1, AVCID
local function parse_path(path, pos)
	local pathBits = path:explode('.')
	if not pos then pos = 2 end
	local id =tonumber(pathBits[#pathBits - pos]);
	if(id and  id >0 and id <= dot1ag_max_session) then
		return id, pathBits[#pathBits ];
	end
end

--InternetGatewayDevice.X_NETCOMM.Dot1ag.1.Rmp.2.ccmDefect => 1, 2, ccmDefect
local function parse_rmp_path(path)
	local pathBits = path:explode('.')
	local id1 =tonumber(pathBits[#pathBits - 3]);
	local id2 =tonumber(pathBits[#pathBits - 1]);
	
	if ( id1 and id2 and id1 >0 and id1 <= dot1ag_max_session and id2 >0 and id2 <= dot1ag_rmp_max_session ) then
		return id1, id2, pathBits[#pathBits ];
	end
end

-- from path to rdb name
--InternetGatewayDevice.X_NETCOMM.Dot1ag.1.Mda.AVCID, mda_rdb_mapping =>
-- dot1ag.1.mda.avcid, "string","rw","avcid", 1

local function getRDBAttr(lookup_table, path)

	local prefix= conf.wntd.y1731Prefix;
	local rdb_name =lookup_table["__y_name__"];
	if not rdb_name then
		rdb_name =lookup_table["__rdb_name__"];
		prefix= conf.wntd.dot1agPrefix;
	end
	if rdb_name =="rmp" then
		local id1, id2,  var_name = parse_rmp_path(path);
		if id1 then
			local attr= lookup_table[var_name];
			if attr then
				return prefix.."."..id1..".rmp."..id2..'.'..attr[3], attr
			end
		
		end
	else
		local id, var_name = parse_path(path);
		if id then
			local attr= lookup_table[var_name];
			if attr then
				return prefix.."."..id.."."..rdb_name..'.'..attr[3], attr
			else
				dot1ag_list= lookup_table["__dot1ag_list__"];
				if(dot1ag_list) then
					attr= dot1ag_list[var_name];
					if attr then
						return conf.wntd.dot1agPrefix.."."..id.."."..rdb_name..'.'..attr[3], attr
					end
				end
			end
		end
	end

end

-- get and check node value,
local function get_node_value(node, val)

	if node.type ~= 'default' then
		node.value = val;
	else
		node.value = node.default
	end
	if node.type == 'datetime' or node.type == 'uint' or node.type == 'int' then

		if not node.value  or node.value =='' then
			node.value =  '0';
		end
	elseif node.type == 'string' then

		node.value = node.value or '';

	end
	--dinfo("get_node_value ".. node:getPath() .. " node.value: ".. node.value);
	return node.value;

end



local function getRDBValue(lookup_table, node, path)

	if node.type ~= 'default' then
		-- dot1ag.1.mda.avcid, "string","rw","avcid", 1
		--"dot1ag.1.mda.mdlevel", {"uint","rw","mdlevel", 1, 0, 7}
		local rdb_name, attr = getRDBAttr(lookup_table, path);
		local rdb_value 
		if rdb_name  then
			rdb_value = luardb.get(rdb_name);
			--print( 'Dot1ag: getRDBValue rdb_name='.. rdb_name, rdb_value)
			get_node_value(node, rdb_value);
		end	
		
		if not rdb_value then
--			print("CWMP.Error.InvalidArguments", rdb_name ,attr)
			return   CWMP.Error.InvalidArguments
		end
	else
		node.value = node.default
	end
	dinfo( 'Dot1ag: getRDBValue node type='.. node.type ..' value =',node.value)
	return  0, node.value  --CWMP.Error.OK
end

local function setRDBValue(lookup_table, node, path, value)

	if node.type ~= 'default' then
		--"dot1ag.1.mda.avcid", {"string","rw","avcid", 1}
		--"dot1ag.1.mda.mdlevel", {"uint","rw","mdlevel", 1, 0, 7}
		local rdb_name, attr =getRDBAttr(lookup_table, path);
		if rdb_name  then
	
			if (attr[2] =='r') then
				--print("CWMP.Error.ReadOnly")
				return  CWMP.Error.ReadOnly
			end
			local l = tonumber(attr[5]);
			local h= tonumber(attr[6]);
			if l  then
				value = tonumber(value)
				if    value < l  then
					return  CWMP.Error.InvalidArguments
				end
			end
			if h then
				value = tonumber(value)
				if  value > h then
					return  CWMP.Error.InvalidArguments
				end
			end
			

			luardb.set(rdb_name, value);
		end
	else
		node.value = node.default
	end
	dinfo( 'Dot1ag: setRDBValue node type='.. node.type ..' value =' ..node.value)
	return 0 --CWMP.Error.OK
end

local function createRDBVar(lookup_table, node, path)

	--"dot1ag.1.mda.avcid", {"string","rw","avcid", 1}
	--"dot1ag.1.mda.mdlevel", {"uint","rw","mdlevel", 1, 0, 7}

	-- validate default value consistency
	local rdb_name, attr =getRDBAttr(lookup_table, path);
	if rdb_name  then
		-- grab current value from RDB
		node.value = luardb.get(rdb_name)
		if node.value == nil then
			-- absent from RDB - create it as default value
			--dinfo('Parameter', 'notice', 'rdb.init(' .. path .. '):  create defaulting rdb variable "' .. rdb_name .. '" := "' .. node.default .. '"')
			luardb.set(rdb_name, node.default)
			node.value = node.default
		else
			-- validate RDB value for consistency
			--dinfo('Parameter', 'debug', 'rdb.init(' .. path .. '): rdb value "' .. node.value .. '"')
			local ret, msg = node:validateValue(node.value)
			if ret then
				-- if the rdb is not initialized ,assigned it with default value
				if ret ==CWMP.Error.UninitializedParameterValue then
					--dinfo('Parameter', 'notice', 'rdb.init(' .. path .. '): defaulting rdb variable "' .. rdb_name.. '" := "' .. node.default .. '"')
					luardb.set(rdb_name, node.default)
					node.value = node.default
				else
					-- maybe we shouldn't die here, but rather default it?
					dinfo('Parameter', 'error', 'rdb.init(' .. path .. ') rdb "' .. node.rdbKey .. '" value error: ' .. msg)
					return ret, msg
				end
			end
		end

		-- if this is a persistent parameter we force the RDB persist flag
		if attr[4]== 1 then
			luardb.setFlags(rdb_name, Daemon.flagOR(luardb.getFlags(rdb_name), 'p'))				
		end


	end
	
	--dinfo( 'dot1ag: node type='.. node.type ..' value =' ..node.value)
	return 0
end

local rdbChangedList={}

local function rdbChangeNotify()
	for rdb_name, _ in pairs(rdbChangedList) do
		luardb.set(rdb_name, "")
	end
	rdbChangedList = {}
	dinfo('dot1ag parameters changed')
end

local function queueChange(id)

	local when = client.inSession and 'postSession' or 'postInit'

	local rdb_name =  conf.wntd.dot1agPrefix .. '.' .. id .. '.changed'
	rdbChangedList[rdb_name] = true

	if not client:isTaskQueued(when, rdbChangeNotify) then
		client:addTask(when, rdbChangeNotify)
		dinfo(rdb_name .. ': installed RDB change notifier task (' .. when .. ')')
	else
		dinfo(rdb_name .. ': RDB change notifier task already installed (' .. when .. ')')
	end
end

----------------------------------------------------
--- process handler
return {

	--- handler for Dot1ag.*, create,delete
	--- -- Container Collection init and create
	['**.Dot1ag|Y1731'] = {
		init = function(node, name)
			dinfo('Dot1ag : model init')
			node:setAccess('readwrite') -- allow creation of instances
			-- create instances
			local ids = dot1agObjectClass:getIds()
			for _, id in ipairs(ids) do
				if tonumber(id) then
					dinfo('Parameter', 'info', 'Dot1ag: creating existing instance ' .. id)
					local instance = node:createDefaultChild(id)
					-- we don't init the new existing Dot1ag nodes as they will be processed
					-- after us by the main tree initialisation calling us now
				end
			end
			return 0
		end,
		
		create = function(node, name)
			dinfo( 'Dot1ag: create new instance ',  name)

			local instanceId;

			-- found an ussed  id
			local result = CWMP.Error.ResourcesExceeded;
			local ids = dot1agObjectClass:getIds()
			for i  = 1, dot1ag_max_session do
				if not table.contains(ids, tostring(i)) then
					instanceId = tostring(i)
					break;
				end
				
			end	

			if instanceId  then
				local instance = dot1agObjectClass:new(instanceId)
				instanceId = dot1agObjectClass:getId(instance)

				if instanceId then
					-- create parameter tree instance
					local treeInstance = node:createDefaultChild(instanceId)

					treeInstance:recursiveInit()
					node.instance = instanceId
					result =0;
					
				end
			end
			dinfo( 'Dot1ag: create instance << ', instanceId)
			return result, instanceId;
		end,
		
	},
	
	----
	-- Instance Object
	----
	
	['**.Dot1ag|Y1731.*'] = {
		init = function(node, name, value)
			dinfo('Dot1ag.* : model init '..name)
			node:setAccess('readwrite') -- can be deleted
			node.value = node.default

			return 0
		end,
		delete = function(node, name)
			dinfo('Dot1ag.* : model delete'..name)
			if disable_delete_operation then
				return 	CWMP.Error.RequestDenied
			end
			local id = tonumber(node.name)
			dinfo('Parameter', 'info', 'Dot1ag: delete instance ' .. id)
			-- delete the RDB instance
			local instance = dot1agObjectClass:getById(id)
			dot1agObjectClass:delete(instance)
			-- delete parameter tree instance
			node.parent:deleteChild(node)
			
			return 0
		end
	},
------------------------------------------	
	---Instance field  get, set
	['**.Dot1ag.*.Mda.*'] = {
		init = function(node, name)
			dinfo('Dot1ag.*.Mda.* : model init '..name)
			createRDBVar(mda_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Dot1ag.*.Mda.*: get', name)
			getRDBValue(mda_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Dot1ag.*.Mda.*: set', name)
			setRDBValue(mda_rdb_mapping, node, name, value)
			queueChange(node.parent.parent.name)
			return 0
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},

	['**.Dot1ag.*.Lmp.*'] = {
		init = function(node, name)
			dinfo('Dot1ag.*.Lmp.* : model init '..name)
			createRDBVar(lmp_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Dot1ag.*.Lmp.*: get', name)
			getRDBValue(lmp_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Dot1ag.*.Lmp.*: set', name)
			setRDBValue(lmp_rdb_mapping, node, name, value)
			queueChange(node.parent.parent.name)
			return 0
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},
	['**.Dot1ag.*.Ltm.*'] = {
		init = function(node, name)
			dinfo('Dot1ag.*.Ltm.* : model init '..name)
			createRDBVar(ltm_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Dot1ag.*.Ltm.*: get', name)
			getRDBValue(ltm_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Dot1ag.*.Ltm.*: set', name)
			setRDBValue(ltm_rdb_mapping, node, name, value)
			queueChange(node.parent.parent.name)
			return 0
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},
	['**.Dot1ag.*.Ltr.*'] = {
		init = function(node, name)
			dinfo('Dot1ag.*.Ltr.* : model init '.. name)
			createRDBVar(ltr_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Dot1ag.*.Ltr.*: get', name)
			getRDBValue(ltr_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  CWMP.Error.ReadOnly,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},
	['**.Dot1ag.*.Lbm.*'] = {
		init = function(node, name)
			dinfo('Dot1ag.*.Lbm.* : model init '..name)
			createRDBVar(lbm_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Dot1ag.*.Lbm.*: get', name)
			getRDBValue(lbm_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Dot1ag.*.Lbm.*: set', name)
			setRDBValue(lbm_rdb_mapping, node, name, value)
			queueChange(node.parent.parent.name)
			return 0
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},
--------------------------------------	
	---Instance field  get, set
	['**.Y1731.*.Mda.*'] = {
		init = function(node, name)
			dinfo('Y1731.*.Mda.* : model init '..name)
			createRDBVar(y_mda_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Y1731.*.Mda.*: get', name)
			getRDBValue(y_mda_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Y1731.*.Mda.*: set', name)
			setRDBValue(y_mda_rdb_mapping, node, name, value)
			queueChange(node.parent.parent.name)
			return 0
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},

	['**.Y1731.*.Lmp.*'] = {
		init = function(node, name)
			dinfo('Y1731.*.Lmp.* : model init '..name)
			createRDBVar(y_lmp_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Y1731.*.Lmp.*: get', name)
			getRDBValue(y_lmp_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Y1731.*.Lmp.*: set', name)
			setRDBValue(y_lmp_rdb_mapping, node, name, value)
			queueChange(node.parent.parent.name)
			return 0
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},
	['**.Y1731.*.Dmm.*'] = {
		init = function(node, name)
			dinfo('Y1731.*.Dmm.* : model init '..name)
			createRDBVar(y_dmm_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Y1731.*.Dmm.*: get', name)
			getRDBValue(y_dmm_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Y1731.*.Dmm.*: set', name)
			setRDBValue(y_dmm_rdb_mapping, node, name, value)
			queueChange(node.parent.parent.name)
			return 0
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},
	['**.Y1731.*.Slm.*'] = {
		init = function(node, name)
			dinfo('Y1731.*.Slm.* : model init '..name)
			createRDBVar(y_slm_rdb_mapping, node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'Y1731.*.Slm.*: get', name)
			getRDBValue(y_slm_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Y1731.*.Slm.*: set', name)
			setRDBValue(y_slm_rdb_mapping, node, name, value)
			queueChange(node.parent.parent.name)
			return 0
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},
------------------------------------------	
	--- handler for rmp
	--- Instance Collection init and create
	['**.Dot1ag.*.Rmp'] = {
		init = function(node, name)
			dinfo('Dot1ag.*.Rmp : model init ' .. name)
			
			node:setAccess('readwrite') -- allow creation of instances

			id1 = node.parent.name
			local rdb_name = conf.wntd.dot1agPrefix..'.'..id1..'.rmp'
			
			-- if dot1ag.x.rmp._index does not exit, create it

			if not luardb.get(rdb_name.."._index")  then
				luardb.set(rdb_name.."._index", "")
			end
			
			dot1ag_rmp_ObjectClass[id1]= rdbobject.getClass(rdb_name, dot1agClassConfig);			
			
			-- build  instances from exist _index
			local objectClass = dot1ag_rmp_ObjectClass[id1];
			
			if objectClass then

				local ids = objectClass:getIds()
				for _, id2 in ipairs(ids) do
					if tonumber(id2) then
						dinfo('Parameter', 'info', 'Dot1ag.*.Rmp.*: creating existing instance ' ..id1..'.'.. id2 )
						local instance = node:createDefaultChild(id2)
						-- we don't init the new existing Dot1ag nodes as they will be processed
						-- after us by the main tree initialisation calling us now
					end
				end
			end
			return 0
		end,
		
		create =  function(node, name)
			dinfo( 'Dot1ag.*.Rmp: create instance ',  name)
			local instanceId;
			--	InternetGatewayDevice.X_NETCOMM.Dot1ag.1.Rmp.
			id1 = node.parent.name

			local objectClass = dot1ag_rmp_ObjectClass[id1];
			local result = CWMP.Error.ResourcesExceeded;
			
			if objectClass then


				-- found an ussed  id
				local ids = objectClass:getIds()


				for i  = 1, dot1ag_rmp_max_session do
					if not table.contains(ids, tostring(i)) then
						instanceId = tostring(i)
						break;
					end
					
				end	
				
				if instanceId then 
					-- create new instance		
					local instance = objectClass:new(instanceId)
					instanceId = objectClass:getId(instance)
		
					-- create parameter tree instance
					local treeInstance = node:createDefaultChild(instanceId)

					treeInstance:recursiveInit()
					node.instance = instanceId
					--rmp_changed(id1, id2, true)	
					result =0
				end
			end
			return result, instanceId;
		end,
	},

	--- Instance Object init and delete
	['**.Dot1ag.*.Rmp.*'] = {
	
		init = function(node, name, value)
			dinfo('Dot1ag.*.Rmp.* : model init')
			node:setAccess('readwrite') -- can be deleted
			node.value = node.default
			return 0
		end,
	
		delete = function(node, name)
			dinfo( 'Dot1ag.*.Rmp.*: delete ',name)
			--InternetGatewayDevice.X_NETCOMM.Dot1ag.2.Rmp.1.
			id1 = node.parent.parent.name
			id2 = node.name
			local objectClass = dot1ag_rmp_ObjectClass[id1];
			if objectClass then
				-- delete the RDB instance
				local instance = objectClass:getById(id2)
				objectClass:delete(instance)
				-- delete parameter tree instance
				node.parent:deleteChild(node)

			end
			return 0

		end,
	},
---[[
	-- Instance field init and get, set
	['**.Dot1ag.*.Rmp.*.*'] = {
		init = function(node, name)
			dinfo('Dot1ag.*.Rmp.*.* : model init')
			createRDBVar(rmp_rdb_mapping, node, name)

			return 0
		end,
		get = function(node, name)
			dinfo( 'Dot1ag.*.Rmp.*.*: get', name)
			getRDBValue(rmp_rdb_mapping, node, name)
			return 0, node.value;
		end,
		set =  function(node, name, value)
			dinfo( 'Dot1ag.*.Rmp.*.*: set', name)
			local err = setRDBValue(rmp_rdb_mapping, node, name, value)
			if err == 0 then
				id1 = node.parent.parent.parent.name
				local objectClass = dot1ag_rmp_ObjectClass[id1];
				if objectClass then
					-- activate dot1ag.x._index
					luardb.set(objectClass.name.."._index", luardb.get(objectClass.name.."._index") )
				end
			end
			return err;
		end,
		unset = CWMP.Error.ReadOnly,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArguments,
	},
	--]]
}
