require('stringutil')
require('tableutil')


local program_path="/usr/bin/"
local start_daemon="start-stop-daemon -S -b -m ";
local pidfile_path="/var/run/"



local debug = conf.wntd.debug;

--debug=2;

local function dinfo( ...)
	if debug >1 then
		local  printResult="";
		 for i,v in ipairs(arg) do
			printResult = printResult .. tostring(v) .. "\t"
		 end
		 dimclient.log('info', printResult);
    end

end

local max_session =4;

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
		['PacketsSendLoss'] = 'packetssendloss',
		['PacketsReceiveLoss'] = 'packetsreceiveloss',
		['MaxThroughput'] 	= 'maxthroughput',
		['MinThroughput'] 	= 'minthroughput',

		['PacketsLossPercentage'] = 'packetslosspercentage',
		['RTTAverage'] 		= 'rttaverage',
		['RTTMax'] 			= 'rttmax',
		['RTTMin'] 			= 'rttmin',
		--['RTT'] 			= 'rtt',
		--['SendPathDelayDelta'] 		= 'sendpathdelaydelta',
		--['ReceivePathDelayDelta'] 	= 'receivepathdelaydelta',

		['SendPathDelayDeltaJitterAverage'] = 'sendpathdelaydeltajitteraverage',
		['SendPathDelayDeltaJitterMin'] 	= 'sendpathdelaydeltajittermin',
		['SendPathDelayDeltaJitterMax'] 	= 'sendpathdelaydeltajittermax',
		['ReceivePathDelayDeltaJitterAverage'] = 'receivepathdelaydeltajitteraverage',
		['ReceivePathDelayDeltaJitterMin'] 	= 'receivepathdelaydeltajittermin',
		['ReceivePathDelayDeltaJitterMax'] 	= 'receivepathdelaydeltajittermax',
	}

	local rdb_name =  conf.wntd.tr143Prefix.. '.' ..fieldMapping[pathBits[#pathBits-2]].. '.'..id .. '.' .. fieldMapping[pathBits[#pathBits]];
	 dinfo( 'getRDBkey ' .. rdb_name)
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


local function getRawDataName(rdb_name)


	local prefix, name = string.match(rdb_name, "(.+)%.(%w+)");
	--print(prefix, name)
	if name == "rtt" then
		return prefix..".statsname", name;
	end

	if name == "sendpathdelaydelta" then
		return prefix..".statsname", name;
	end

	if name == "receivepathdelaydelta" then
		return prefix..".statsname", name;
	end

	return nil
end

local function loadRawData( rdb_name, field_name)


	local filename = luardb.get(rdb_name);

	if not filename then return "" end
	print(filename);

	local file = io.open(filename);
	if not file then return "" end;

	local data ="";
	local l=0;
	local pattern="";
	for line in file:lines() do
		--print(line)
		if l ==0 then

			for key in string.gmatch(line, '(%w+)') do
				if key == field_name then
					pattern = pattern.."([-%d%.]*)";
					break;

				else
					pattern = pattern.."[-%d%.]*,";
				end

			end
			if not string.find(pattern, "%(") then return nil; end
			--print(pattern)

		else
			data = data .. string.match(line, pattern)..",";
		end
		l =l+1
	end
	file:close();
	return data;
end


local function getRDBValue(node, name)

	if node.type ~= 'default' then
		local rdb_name = getRDBKey(node, name);
		local rdb_stats_name, field_name = getRawDataName(rdb_name);
		if rdb_stats_name  then
			node.value = loadRawData(rdb_stats_name, field_name);
		else
			rdb_get_value(node, getRDBKey(node, name));
		end
	else
		node.value = node.default
	end
	dinfo( 'TR143: node type='.. node.type ..' value =' ..node.value)

end

local function dump_node( node)

	if debug >1 then
		dinfo( 'TR143: node name='.. node.name)
		dinfo( 'TR143: node path='.. node:getPath())
		dinfo( 'TR143: node type='.. node.type)
		dinfo( 'TR143: node value='.. node.value)
	end
end
local function dump_active_list(active_list)
	if debug >1 then
		for id, val in pairs(active_list) do
			if val then
				dinfo( 'Active list id ='.. id ..", "..val)
			end
		end
	end
end

local function explode_int(idlist)
	local ids={};
	for id in string.gmatch(idlist, "(%d+)") do
		table.insert(ids, tonumber(id));
	end
	return ids;
end

local program_table={
	download={
			rdb_id_list = conf.wntd.tr143Prefix..".session.download_activeids",
			name="tr143_69_http_d",
			program= program_path.."tr143_http",
			program_name="tr143_http",
			argument="-ed",
			active_list={},
		},
	upload={
			rdb_id_list = conf.wntd.tr143Prefix..".session.upload_activeids",
			name="tr143_69_http_u",
			program= program_path.."tr143_http",
			program_name="tr143_http",
			argument="-eu",
			active_list={},
		},
	udpecho={
			rdb_id_list = conf.wntd.tr143Prefix..".session.udpecho_activeids",
			name ="tr143_69_ping",
			program= program_path.."tr143_ping",
			program_name="tr143_ping",
			argument="-e",
			active_list={},
		},
}

function update_id_list(obj)

	local str="";
	for id, pid_file  in pairs(obj.active_list) do
		if pid_file then
			str = str..id..",";
		end
	end
	luardb.set(obj.rdb_id_list, str);
	if obj.node then
		dimclient.setParameter(obj.node:getPath(), str)
	end
	dump_active_list(obj.active_list);
end

function get_pid(pidfile)

	local f = io.open(pidfile);
	if f then
		local pid =f:read("*number");
		io.close(f);
		return pid;
	end
	return nil;
end

function check_process_exist( pid)

	if pid then
		local status = os.execute("/bin/ls /proc/"..pid..">/dev/null  2>&1");
		if  status and status ==0 then
			return true;
		end
	end
	return false;
end


function start_program(obj, id)

	dinfo( "start_program (".. obj.name ..',' .. id..') begin');
	dump_active_list(obj.active_list);

	id = tonumber(id);
	--start-stop-daemon -S -b -m -p /var/bin/tr143_http1.pid -x /usr/bin/tr143_http -- -i1 -d"
	local pidfile= pidfile_path.. obj.name..id..".pid";
	local argument =obj.argument .." -i"..id;
	local cmd = start_daemon .. " -p " .. pidfile.. " -x ".. obj.program .. " -- "..argument;
	--[[ --test case 1: failed to start program
		cmd = "";
	--]]
	local status = os.execute(cmd);
	dinfo( "execute " .. cmd ..",".. status);
	if status and status ==0 then

		os.execute("sleep 1");
		-- wait a while, make sure the process exist
		if  check_process_exist(get_pid(pidfile)) == true then
			obj.active_list[id] = pidfile;
			update_id_list(obj)

			dinfo( "start_program (".. obj.name ..',' .. id..') end');

			return 0;
		else
			os.execute("/bin/rm " .. pidfile);
			obj.active_list[id] = nil;

		end

	end
	return -1;
end

function stop_program(obj, id )

		dinfo( "stop_program (".. obj.name .."," ..id..') begin');
		--dump_active_list(obj.active_list);

	id = tonumber(id);
	--stop_program ="start-stop-daemon -K -b -q  -p $PIDFILE",

	if obj.active_list[id] then
		local pidfile = obj.active_list[id];
		local cmd = "start-stop-daemon -K -b -q  -p "..pidfile;
		--[[ -- test case 2: cannot stop program
			cmd =""
		--]]
		local status = os.execute(cmd);
		dinfo( "execute " .. cmd ..","..status);

		--if status and status ==0 then
		--end
		-- wait a while, if process still exist, force delete
		os.execute("sleep 1");
		local pid = get_pid(pidfile);
		if pid then
			--if debug >0 then dinfo( "get_pid " ..pid.. " from ".. pidfile);end;
			if  check_process_exist(pid) == true then
				dinfo( "force kill  " .. pidfile .. "pid=".. pid);
				os.execute("/bin/kill -9 ".. pid);
			end
		end

		os.execute("/bin/rm " .. pidfile);
		obj.active_list[id] =nil

		update_id_list(obj);

		dinfo( "stop_program (".. obj.name .."," ..id..') end');


		return 0;
	end
	return -1;
end

--[[
local function update_active_node(node, active_list, idlist)

	-- create current instances
	local ids = explode_int(idlist); --idlist.explode(idlist, ',');

	local changed = 0;
	dinfo( 'TR143: update_active_node start !!!')
	dump_active_list(active_list);

	-- remove deleted node
	for id, val in pairs(active_list) do

		if val and not table.contains(ids, id) then
			local obj_path= node:getPath()..'.'..id.. '.';
			dinfo( 'TR143: delObject ' .. obj_path )
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
			dinfo( 'TR143: addObject ' .. obj_path..', id= '..id)
			--new_alarm_id = alarm_id;
			local ret, id= dimclient.addObject(obj_path, id);
			--local ret = node:createInstance( obj_path, alarm_id);
			--local ret =0;
			if ret and ret ==0 then

				active_list[id] = 1;
				dinfo( 'TR143: addObject OK')
				changed = changed +1;

			end
		end

	end
	--new_alarm_id=nil
	if changed >0 then
		dump_active_list(active_list);
	end

	dinfo( 'TR143: update_active_node done !!!')
end
--]]

-- install and verify  existing active IDs
function init_active_node( node, m)
		-- create current instances
		-- get the current list
		local idlist = luardb.get(m.rdb_id_list)
		if not idlist then return  end;

		dinfo( 'TR143: init_active_node idList= '..idlist)
		local ids = explode_int(idlist);

		--local ids = idlist.explode(idlist, ',');
		for _, id in pairs(ids) do
			local pidfile = pidfile_path.. m.name..id..".pid";
			if check_process_exist( get_pid(pidfile)) then

				--dinfo( 'TR143: creating id= ' .. id)

				--local instance = node:createDefaultChild(id)
				m.active_list[id] =pidfile;
				stop_program(m, id);

			end
			--stance:setAccess(readwrite') -- instance can be deleted
		end
		-- remove all similar programs
		os.execute("/usr/bin/killall ".. m.program_name);

		update_id_list(m);


end
--[[
local function node_change_notify()
	dinfo( 'TR143: callback  !!!')

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

--]]
return {
	----
	-- Container Collection
	----
	['**.Diagnostics.Download|Upload|UDPEcho'] = {
		init = function(node, name, value)
			node.value = node.default
			dinfo( 'TR143:.|Download|Upload|UDPEcho| model  init')

			dump_node(node);



			node:setAccess('readonly') -- no creation of instances via TR-069


			if node.name == 'UDPEcho' then
				local path = string.match(node:getPath(), "(.+%.Diagnostics%.)").."Session.Maximum";

				max_session = tonumber(dimclient.parameter.get(path));

				dinfo( path..": ",  max_session);


				init_active_node( node, program_table.udpecho)

			elseif node.name == 'Download' then

				init_active_node( node, program_table.download)

			elseif node.name == 'Upload' then
				init_active_node( node, program_table.upload)

			end

			return 0
		end,
		get = function(node, name) return '' end,
		set = function(node, name, value)	return 0 end,

		unset = cwmpError.funcs.ReadOnly,
		--create = cwmpError.funcs.ResourceExceeded,
		--delete = cwmpError.funcs.InvalidArgument
		create = function(node, name, instanceId)
			dinfo( 'TR143: create instance ', instanceId,' name = ', name)
			local result = cwmpError.ResourceExceeded;
			-- launch program start-stop-daemon .. /usr/bin/tr143_http -i1 -e
			if node.name == 'UDPEcho' then
				if #program_table.udpecho.active_list < max_session then
					result = start_program( program_table.udpecho, instanceId)
				end
			elseif node.name == 'Download' then
				if #program_table.download.active_list < max_session then
					result = start_program( program_table.download, instanceId);
				end

			elseif node.name == 'Upload' then
				if #program_table.upload.active_list < max_session then
					result = start_program( program_table.upload, instanceId);
				end
			end
			if result == 0 then
				-- create parameter tree instance
				local treeInstance = node:createDefaultChild(instanceId)

				treeInstance:recursiveInit()
				node.instance = instanceId
			end
			return result;
		end,

		delete = function(node, name)
			dinfo( 'TR143: delete .TR143',name)


			return 0;
		end,

	},


	----
	-- Instance Object
	----
	['**.Diagnostics.Download|Upload|UDPEcho.*']= {
		init = function(node, name, value)
			node.value = node.default

			dinfo( 'TR143: |Download|Upload|UDPEcho|.*: init')
			dump_node(node);

			return 0
		end,
		get = function(node, name) return '' end,
		set = function(node, name)	return 0 end,
		unset = function(node, name)
			dinfo( 'TR143: .|Download|Upload|UDPEcho|.* unset',name)
			-- delete parameter tree instance
			node.parent:deleteChild(node)
			return 0

		end,
		--create = cwmpError.funcs.InvalidArgument,
		create = function(node, name)
			dinfo( 'TR143: .|Download|Upload|UDPEcho|.* create', name)
			return 0;
		end,

		delete = function(node, name)
			local m = string.match(node:getPath(),".+%.Diagnostics%.(%w+)");

			dinfo( 'TR143: .|Download|Upload|UDPEcho|.* delete', name, m)
			dump_node(node);


			---[[-- stop program
			if m == 'UDPEcho' then
				stop_program( program_table.udpecho, node.name)
			elseif m == 'Download' then
				stop_program( program_table.download, node.name);
			elseif m == 'Upload' then
				stop_program( program_table.upload, node.name);
			end
			--]]
			-- delete parameter tree instance
			--node.parent:deleteChild(node)
			return 0
		end,
	},

	----
	-- Instance Parameters
	----
	['**.Diagnostics.Download|Upload|UDPEcho.*.*'] = {
		init = function(node, name, value)
			-- skip default node instance

			dinfo( 'TR143: |Download|Upload|UDPEcho|.*.* init name = ', name)
			dump_node(node);

			if node.name == 'DiagnosticsState' then
				local watcher = function(key, value)
					dinfo( 'TR143: external change of ', key)
					if value then
						if value ~= node.value then
							dinfo( 'TR143: ', key, ' changed: "' .. node.value .. '" -> "' .. value .. '"')
							if value ~= 'None' and value ~= 'Requested' and value ~= 'Cancelled' then
								dimclient.addEventSingle('8 DIAGNOSTICS COMPLETE')
								dimclient.asyncInform()
							end
							dimclient.setParameter(node:getPath(), value)
						end
					end
				end
				local rdbKey = getRDBKey(node, name)
				dinfo( 'TR143: adding watcher for: ' .. rdbKey)
				luardb.watch(rdbKey, watcher)
			end
			getRDBValue(node, name)
			return 0
		end,
		get = function(node, name)
			dinfo( 'TR143: |Download|Upload|UDPEcho|.*.* get name = ', name, " ntype= ",node.type)
			getRDBValue(node, name)
			return node.value
		end,
		set = function(node, name, value)
			dinfo( 'TR143: |Download|Upload|UDPEcho|.*.*: set', name)
			node.value = value
			luardb.set(getRDBKey(node, name), value)
			return 0;
		end,
		unset = cwmpError.funcs.OK,
		create = cwmpError.funcs.InvalidArgument,
		delete = cwmpError.funcs.InvalidArgument,
	},
}
