require('stringutil')
require('tableutil')
require('Parameter')

--local rdb_download_list= conf.wntd.tr143Prefix..".session.download_activeids";
--local rdb_upload_list= conf.wntd.tr143Prefix..".session.upload_activeids";
--local rdb_udpecho_list= conf.wntd.tr143Prefix..".session.udpecho_activeids";


local program_path="/usr/bin/"
local start_daemon="start-stop-daemon -S -b -m ";
local pidfile_path="/var/run/"


--local download_node=nil;
--local upload_node=nil;
--local udpecho_node=nil;

local debug = conf.wntd.debug;

--debug=2;

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




local max_session =2;

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

local function getTCRDBKey(node, name)
	local pathBits = name:explode('.')
	--position -3
	local fieldMapping3 = {
		-- function group
		['Download']= 'download',
		['Upload']	= 'upload',
		['UDPEcho']	= 'udpecho',
	}
	--position -1
	local fieldMapping1 = {
		['TC1']		= 'tc1',
		['TC2']         = 'tc2',
		['TC4']		= 'tc4',
	}
	--position 0
	local fieldMapping = {
		['CIR']		= 'cir',
		['PIR']		= 'pir',
	}

	if not fieldMapping[pathBits[#pathBits]] then
		error('TR143: unknown parameter to map "' .. pathBits[#pathBits] .. 'cd .tmp	"')
	end
	local rdb_name =  conf.wntd.tr143Prefix .. '.' ..fieldMapping3[pathBits[#pathBits-3]]..'.'..pathBits[#pathBits - 2] .. '.' ..fieldMapping1[pathBits[#pathBits-1]] ..'_'..  fieldMapping[pathBits[#pathBits]]
	dinfo( 'getTCRDBKey', rdb_name)
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
			rdb_get_value(node, rdb_name);
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
			instanceid=0,
		},
	upload={
			rdb_id_list = conf.wntd.tr143Prefix..".session.upload_activeids",
			name="tr143_69_http_u",
			program= program_path.."tr143_http",
			program_name="tr143_http",
			argument="-eu",
			active_list={},
			instanceid=0,
		},
	udpecho={
			rdb_id_list = conf.wntd.tr143Prefix..".session.udpecho_activeids",
			name ="tr143_69_ping",
			program= program_path.."tr143_ping",
			program_name="tr143_ping",
			argument="-e",
			active_list={},
			instanceid=0,
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
		client:asyncParameterChange(obj.node, obj.node:getPath(), str)
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

local rdbChangedList={}

local function rdbChangeNotify()
	for rdb_name, _ in pairs(rdbChangedList) do
		luardb.set(rdb_name, "")
	end
	rdbChangedList = {}
	dinfo('tr143 parameters changed')
end

local function queueChange(field, id)

	local when = client.inSession and 'postSession' or 'postInit'
	local fieldMapping = {
		-- function group
		['Download'] = 'download',
		['Upload'] = 'upload',
		['UDPEcho'] = 'udpecho',
	}
	local rdb_name =  conf.wntd.tr143Prefix .. '.' .. fieldMapping[field] .. '.'.. id .. '.changed'
	rdbChangedList[rdb_name] = true

	if not client:isTaskQueued(when, rdbChangeNotify) then
		client:addTask(when, rdbChangeNotify)
		dinfo('installed RDB change notifier task (' .. when .. ')')
	else
		dinfo('RDB change notifier task already installed (' .. when .. ')')
	end
end

return {
	----
	-- Container Collection
	----
	['**.Diagnostics.Download|Upload|UDPEcho'] = {
		init = function(node, name)
			node.value = node.default
			dinfo( 'TR143:.|Download|Upload|UDPEcho| model  init')

			dump_node(node);

			node:setAccess('readwrite')


			if node.name == 'UDPEcho' then
				local path = string.match(node:getPath(), "(.+%.Diagnostics%.)").."Session.Maximum";
				local val = paramTree:getValue(path)

				max_session = tonumber(val);

				dinfo( path..": ",  max_session);


				init_active_node( node, program_table.udpecho)

			elseif node.name == 'Download' then

				init_active_node( node, program_table.download)

			elseif node.name == 'Upload' then
				init_active_node( node, program_table.upload)

			end

			return 0
		end,
		get = function(node, name) return 0, '' end,
		set = function(node, name, value)	return 0 end,

		unset = CWMP.Error.ReadOnly,
		create = function(node, name)
			dinfo( 'TR143: create instance ', 'name = ', name)

			local instanceId;

			local result = CWMP.Error.ResourcesExceeded;
			-- launch program start-stop-daemon .. /usr/bin/tr143_http -i1 -e
			if node.name == 'UDPEcho' then
				if table.count(program_table.udpecho.active_list) < max_session then
					program_table.udpecho.instanceid = program_table.udpecho.instanceid+1
					instanceId = program_table.udpecho.instanceid;
					result = start_program( program_table.udpecho, instanceId)
				end
			elseif node.name == 'Download' then
				if table.count(program_table.download.active_list) < max_session then
					program_table.download.instanceid = program_table.download.instanceid+1
					instanceId = program_table.download.instanceid;
					result = start_program( program_table.download, instanceId);
				end

			elseif node.name == 'Upload' then
				if table.count(program_table.upload.active_list) < max_session then
					program_table.upload.instanceid = program_table.upload.instanceid+1
					instanceId = program_table.upload.instanceid;
					result = start_program( program_table.upload, instanceId);
				end
			end
			if result == 0 then
				-- create parameter tree instance
				local treeInstance = node:createDefaultChild(instanceId)

				treeInstance:recursiveInit()
				node.instance = instanceId
			end
			return result, instanceId;
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
		init = function(node, name)
			node.value = node.default

			dinfo( 'TR143: |Download|Upload|UDPEcho|.*: init')
			dump_node(node);

			return 0
		end,
		get = function(node, name) return 0,'' end,
		set = function(node, name)	return 0 end,
		unset = function(node, name)
			dinfo( 'TR143: .|Download|Upload|UDPEcho|.* unset',name)
			-- delete parameter tree instance
			node.parent:deleteChild(node)
			return 0

		end,
		--create = CWMP.Error.InvalidArgument,
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

	['**.Diagnostics.Download|Upload|UDPEcho.*.TC1|TC2|TC4.*'] = {
		init = function(node, name, value)
			--dinfo( 'TR143: |Download|Upload|UDPEcho|.*.TC1|TC2|TC4.* init name = ', name)
			-- check RDB state and default if required
			node.key = getTCRDBKey(node, name)
			node.value = luardb.get(node.key) or node.default
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.key) or node.default
			return 0, node.value
		end,
		set = function(node, name, value)
			luardb.set(node.key, value);
			return 0
		end
	},
	----
	-- Instance Parameters
	----
	['**.Diagnostics.Download|Upload|UDPEcho.*.*'] = {
		init = function(node, name)
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
								client:asyncInform('8 DIAGNOSTICS COMPLETE')
							end
							client:asyncParameterChange(node, node:getPath(), value)
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
			return 0, node.value
		end,
		set = function(node, name, value)
			dinfo( 'TR143: |Download|Upload|UDPEcho|.*.*: set', name)
			node.value = value
			luardb.set(getRDBKey(node, name), value)
			queueChange(node.parent.parent.name, node.parent.name)
			return 0;
		end,
		unset = CWMP.Error.OK,
		create = CWMP.Error.InvalidArguments,
		delete = CWMP.Error.InvalidArguments,
	},
}
