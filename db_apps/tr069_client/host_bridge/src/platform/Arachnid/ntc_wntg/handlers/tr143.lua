require('stringutil')
require('tableutil')

local rdb_download_list= conf.wntd.tr143Prefix..".session.download_activeids";
local rdb_upload_list= conf.wntd.tr143Prefix..".session.upload_activeids";
local rdb_udpecho_list= conf.wntd.tr143Prefix..".session.udpecho_activeids";



local download_node=nil;
local upload_node=nil;
local udpecho_node=nil;

local active_download_list={};
local active_upload_list={};
local active_udpecho_list={};


local tasks={};

local debug=0;

local function getRDBKey(node, name)
	local pathBits = name:explode('.')
	local id = pathBits[#pathBits - 1]
	local fieldMapping = {
		-- function group
		['Download']= 'download',
		['Upload']	= 'upload',
		['UDPEcho']	= 'udpecho',

		-- download
		['DiagnosticsState'] = 'diagnosticsstate',
		['SmartEdgeAddress'] = 'smartedgeaddress',
		['MPLSTag'] 		= 'mplstag',
		['CoS'] 			= 'cos',
		['CoSToEXP'] 		= 'costoexp',
		['CoSToDSCP'] 		= 'costodscp',
		['InterfaceAddress'] = 'interfaceaddress',
		['InterfaceNetmask'] = 'interfacenetmask',
		['DownloadURL'] 	= 'downloadurl',
		['ROMTime'] 		= 'romtime',
		['BOMTime'] 		= 'bomtime',
		['EOMTime'] 		= 'eomtime',
		['TestBytesReceived'] = 'testbytesreceived',
		['TotalBytesReceived'] = 'totalbytesreceived',

		--['upload'] = 'upload',
		['UploadURL']		= 'uploadurl',
		['TestFileLength'] = 'testfilelength',
		['TotalBytesSent'] = 'totalbytessent',
		--['UDPEcho'] = 'UDPEcho',
		['ServerAddress'] 	= 'serveraddress',
		['ServerPort'] 		= 'serverport',
		['PacketCount'] 	= 'packetcount',
		['PacketSize'] 		= 'packetsize',
		['PacketInterval'] 	= 'packetinterval',
		['StragglerTimeout']= 'stragglertimeout',
		['BytesSent'] 		= 'bytessent',
		['BytesReceived'] 	= 'bytesreceived',
		['PacketsSent'] 	= 'packetssent',
		['PacketsReceived'] = 'packetsreceived',
		['PacketsLossPercentage'] = 'packetslosspercentage',
		['RTTAverage'] 		= 'rttaverage',
		['RTTMax'] 			= 'rttmax',
		['RTTMin'] 			= 'rttmin',
		['SendPathPacketDelayDeltaAverage'] = 'sendpathpacketdelaydeltaaverage',
		['SendPathPacketDelayDeltaMin'] 	= 'sendpathpacketdelaydeltamin',
		['SendPathPacketDelayDeltaMax'] 	= 'sendpathpacketdelaydeltamax',
		['ReceivePathPacketDelayDeltaAverage'] = 'receivepathpacketdelaydeltaaverage',
		['ReceivePathPacketDelayDeltaMin'] 	= 'receivepathpacketdelaydeltamin',
		['ReceivePathPacketDelayDeltaMax'] 	= 'receivepathpacketdelaydeltamax',

	}

	local rdb_name =  conf.wntd.tr143Prefix.. '.' ..fieldMapping[pathBits[#pathBits-2]].. '.'..id .. '.' .. fieldMapping[pathBits[#pathBits]];
	 if debug >0 then dimclient.log('info', 'getRDBkey ' .. rdb_name) end
	return rdb_name;
end

-- get and check rdb value,
local function rdb_get_value(node, name)

	node.value = luardb.get(name);
	if node.type == 'datetime' or node.type == 'uint' or node.type == 'int' then

		if not node.value  or node.value =='' then
			node.value =  '0';
		end
	elseif node.type == 'string' then

		node.value = node.value or '';

	end
	return node.value;

end

local function getRDBValue(node, name)

	if node.type ~= 'default' then
		rdb_get_value(node, getRDBKey(node, name));
	else
		node.value = node.default
	end
	if debug >0 then dimclient.log('info', 'TR143: node type='.. node.type ..' value =' ..node.value) end

end

local function dump_node( node)

	dimclient.log('info', 'TR143: node name='.. node.name)
	dimclient.log('info', 'TR143: node path='.. node:getPath())
	dimclient.log('info', 'TR143: node type='.. node.type)
	dimclient.log('info', 'TR143: node value='.. node.value)

end
local function dump_active_list(active_list)

	for id, val in pairs(active_list) do
		if val then
			dimclient.log('info', 'Active list id ='.. id )
		end
	end
end

local function explode_int(idlist)
	local ids={};
	for id in string.gmatch(idlist, "(%d+)") do
		table.insert(ids, id);
	end
	return ids;
end

local function update_active_node(node, active_list, idlist)

	-- create current instances
	local ids = explode_int(idlist); --idlist.explode(idlist, ',');

	local changed = 0;
	dimclient.log('info', 'TR143: update_active_node start !!!')
	dump_active_list(active_list);

	-- remove deleted node
	for id, val in pairs(active_list) do

		if val and not table.contains(ids, id) then
			local obj_path= node:getPath()..'.'..id.. '.';
			dimclient.log('info', 'TR143: delObject ' .. obj_path )
			dimclient.delObject(obj_path);
			--node:deleteInstance(obj_path);
			active_list[id] =nil;
			changed = changed +1;
		end

	end
	--add new node
	for _, id in  pairs(ids) do
		if not active_list[id] then
		 -- new note
			local obj_path= node:getPath()..'.'
			dimclient.log('info', 'TR143: addObject ' .. obj_path..', id= '..id)
			--new_alarm_id = alarm_id;
			local ret, id= dimclient.addObject(obj_path, id);
			--local ret = node:createInstance( obj_path, alarm_id);
			--local ret =0;
			if ret and ret ==0 then

				active_list[id] = 1;
				dimclient.log('info', 'TR143: addObject OK')
				changed = changed +1;

			end
		end

	end
	--new_alarm_id=nil
	if changed >0 then
		dump_active_list(active_list);
	end

	dimclient.log('info', 'TR143: update_active_node done !!!')
end

function init_active_node( node, active_list, rdb_list_name)
		-- create current instances
		-- get the current list
		local idlist = luardb.get(rdb_list_name)
		if not idlist then return  end;
		dimclient.log('info', 'TR143: init_active_node idList= '..idlist)
		local ids = explode_int(idlist);

		--local ids = idlist.explode(idlist, ',');
		for _, id in pairs(ids) do

			dimclient.log('info', 'TR143: creating id= ' .. id)

			local instance = node:createDefaultChild(id)
			active_list[id] =1;
			--stance:setAccess(readwrite') -- instance can be deleted
		end
		dump_active_list(active_list);

end

local function node_change_notify()
	dimclient.log('info', 'TR143: callback  !!!')

	if tasks['download_changed'] then
		local idlist = luardb.get(rdb_download_list)
		update_active_node(download_node, active_download_list, idlist);
		tasks['download_changed']= nil;
	end
	if tasks['upload_changed'] then
		local idlist = luardb.get(rdb_upload_list)
		update_active_node(upload_node, active_upload_list, idlist);
		tasks['upload_changed']=nil;
	end

	if tasks['udpecho_changed'] then
		local idlist = luardb.get(rdb_udpecho_list)
		update_active_node(udpecho_node, active_udpecho_list, idlist);
		tasks['udpecho_changed']=nil;
	end

end


return {
	----
	-- Container Collection
	----
	['**.Diagnostics.Session.DownloadActiveIDs'] = {
		init = function(node, name, value)
			node.value = node.default
			dimclient.log('info', 'TR143: model download init'.. name)
			if debug>0 then
				dump_node(node);
			end
			node:setAccess('readonly') -- no creation of instances via TR-069
			-- create current instances
			-- install watcher for external changes
			local watcher = function(key, value)
				dimclient.log('info', 'TR143: downloadlist external change of ' .. key)
				if value ~= node.value then
					dimclient.log('info', 'TR143: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
					-- find node "alarm"
					-- remove all the child
					-- create all the child
					if download_node then
						tasks['download_changed'] = true;
						dimclient.callbacks.register('preSession', node_change_notify)

						--update_active_node(download_node, active_download_list, value);
					end

					dimclient.setParameter(node:getPath(), value)

				end

			end
			luardb.watch(rdb_download_list, watcher)
			return 0

		end,

		get = function(node, name)

			rdb_get_value(node, rdb_download_list)
			return node.value;
		end,

		set = function(node, name, value)

			--luardb.set(rdb_timestamp, value)
			return 0;
		end,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.ResourceExceeded,
		delete = cwmpError.funcs.InvalidArgument
	},

	['**.Diagnostics.Session.UploadActiveIDs'] = {
		init = function(node, name, value)
			node.value = node.default
			dimclient.log('info', 'TR143: model upload init'.. name)
			if debug>0 then
				dump_node(node);
			end
			node:setAccess('readonly') -- no creation of instances via TR-069
			-- create current instances
			-- install watcher for external changes
			local watcher = function(key, value)
				dimclient.log('info', 'TR143: uploadlist external change of ' .. key)
				if value ~= node.value then
					dimclient.log('info', 'TR143: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
					-- find node "alarm"
					-- remove all the child
					-- create all the child
					if upload_node then
						tasks['upload_changed'] = true;
						dimclient.callbacks.register('preSession', node_change_notify)

						--update_active_node(upload_node, active_upload_list, value );
					end

					dimclient.setParameter(node:getPath(), value)

				end

			end
			luardb.watch(rdb_upload_list, watcher)
			return 0

		end,
		get = function(node, name)

			rdb_get_value(node, rdb_upload_list)
			return node.value;
		end,

		set = function(node, name, value)

			--luardb.set(rdb_timestamp, value)
			return 0;
		end,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.ResourceExceeded,
		delete = cwmpError.funcs.InvalidArgument
	},

	['**.Diagnostics.Session.UDPEchoActiveIDs'] = {
		init = function(node, name, value)
			node.value = node.default
			dimclient.log('info', 'TR143: model udpecho init'.. name)
			if debug>0 then
				dump_node(node);
			end
			node:setAccess('readonly') -- no creation of instances via TR-069
			-- create current instances
			-- install watcher for external changes
			local watcher = function(key, value)
				dimclient.log('info', 'TR143: udpecholist external change of ' .. key)
				if value ~= node.value then
					dimclient.log('info', 'TR143: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
					-- find node "alarm"
					-- remove all the child
					-- create all the child
					if udpecho_node then
						tasks['udpecho_changed'] = true;
						dimclient.callbacks.register('preSession', node_change_notify)
						--update_active_node(udpecho_node, active_udpecho_list, value);
					end
					dimclient.setParameter(node:getPath(), value)

				end

			end
			luardb.watch(rdb_udpecho_list, watcher)
			return 0

		end,
		get = function(node, name)

			rdb_get_value(node, rdb_udpecho_list)
			return node.value;
		end,

		set = function(node, name, value)

			--luardb.set(rdb_timestamp, value)
			return 0;
		end,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.ResourceExceeded,
		delete = cwmpError.funcs.InvalidArgument
	},

	----
	-- Container Collection
	----
	['**.Diagnostics.Download|Upload|UDPEcho'] = {
		init = function(node, name, value)
			node.value = node.default
			dimclient.log('info', 'TR143:.|Download|Upload|UDPEcho| model  init')
			if debug>0 then
				dump_node(node);
			end


			node:setAccess('readonly') -- no creation of instances via TR-069


			if node.name == 'UDPEcho' then

				udpecho_node =node;
				init_active_node( node, active_udpecho_list, rdb_udpecho_list)

			elseif node.name == 'Download' then

				download_node =node;
				init_active_node( node, active_download_list, rdb_download_list)

			elseif node.name == 'Upload' then
				upload_node =node;
				init_active_node( node, active_upload_list, rdb_upload_list)

			end

			return 0
		end,
		get = function(node, name) return '' end,
		set = function(node, name, value)	return 0 end,

		unset = cwmpError.funcs.ReadOnly,
		--create = cwmpError.funcs.ResourceExceeded,
		--delete = cwmpError.funcs.InvalidArgument
		create = function(node, name, instanceId)
			dimclient.log('info', 'TR143: create instance ' .. instanceId .. ' name = '.. name)
			-- create parameter tree instance
			local treeInstance = node:createDefaultChild(instanceId)

			treeInstance:recursiveInit()
			node.instance = instanceId
			return 0
		end,

		delete = function(node, name)
			if(debug>0) then dimclient.log('info', 'TR143: delete .TR143'..name) end
			return 0;
		end,

	},


	----
	-- Instance Object
	----
	['**.Diagnostics.Download|Upload|UDPEcho.*']= {
		init = function(node, name, value)
			node.value = node.default
			if debug >0 then
				dimclient.log('info', 'TR143: |Download|Upload|UDPEcho|.*: init')
				dump_node(node);
			end;
			return 0
		end,
		get = function(node, name) return '' end,
		set = function(node, name)	return 0 end,
		unset = function(node, name)
			dimclient.log('info', 'TR143: .|Download|Upload|UDPEcho|.* unset  '..name)
			-- delete parameter tree instance
			node.parent:deleteChild(node)
			return 0

		end,
		--create = cwmpError.funcs.InvalidArgument,
		create = function(node, name)
			if(debug>0) then dimclient.log('info', 'TR143: .|Download|Upload|UDPEcho|.* create '..name) end
			return 0;
		end,

		delete = function(node, name)
			if(debug>0) then dimclient.log('info', 'TR143: .|Download|Upload|UDPEcho|.* delete  '..name) end

			-- delete parameter tree instance
			--node.parent:deleteChild(node)
			return 0
		end,
		--alarm object cannot delete by user.
	--[[
		delete = function(node, name)
			-- determine ID
			local pathBits = name:explode('.')
			local id = pathBits[#pathBits]
			dimclient.log('info', 'TR143: delete instance ' .. id)

			-- delete the alarm instance
			alarmd:invoke('delete', { ['id'] = id }, 20)

			-- delete parameter tree instance
			node.parent:deleteChild(node)
			return 0
		end
		--]]
	},

	----
	-- Instance Parameters
	----
	['**.Diagnostics.Download|Upload|UDPEcho.*.*'] = {
		init = function(node, name, value)
			-- skip default node instance
			if debug>0 then
				dimclient.log('info', 'TR143: init |Download|Upload|UDPEcho|.*.*')
				dump_node(node);
			end

			getRDBValue(node, name);
			return 0
		end,
		get = function(node, name)
			if(debug>0) then dimclient.log('info', 'TR143: |Download|Upload|UDPEcho|.*.* get name = '.. name .. " ntype= "..node.type) end
			getRDBValue(node, name);
			return node.value
		end,
--[[
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.ReadOnly,
		delete = cwmpError.funcs.ReadOnly,
--]]
	---[[
		set = function(node, name, value)
			if(debug>0) then dimclient.log('info', 'TR143: |Download|Upload|UDPEcho|.*.*: set'..name) end

			if node.type ~= 'default' then
				node.value = value
				local rdb_name = getRDBKey(node, name);
				luardb.set(getRDBKey(node, name), value)
			end

			return 0;
		end,
		unset = function(node, name)
			if(debug>0) then dimclient.log('info', 'TR143: |Download|Upload|UDPEcho|.*.*: unset'..name) end
			return 0;
		end,
		create = function(node, name)
			if(debug>0)then dimclient.log('info', 'TR143: |Download|Upload|UDPEcho|.*.*: create '..name) end

			return 0;
		end,

		delete = function(node, name)
			if(debug>0)then dimclient.log('info', 'TR143: |Download|Upload|UDPEcho|.*.*: delete '..name) end
			--node.parent:deleteChild(node)
			return 0;
		end,
	--]]
--		create = cwmpError.funcs.InvalidArgument,
	--	delete = cwmpError.funcs.InvalidArgument
	},
}
