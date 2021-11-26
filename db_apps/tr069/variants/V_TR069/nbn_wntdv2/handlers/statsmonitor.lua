-- #!/usr/bin/env lua
require('stringutil')
require('tableutil')

local debug = conf.wntd.debug;

debug=1



local statsmonitorClassConfig = { persist = true}



local sample_prefix =conf.stats.statsmonitoringSamplePrefix;

--local rdb_sample_time =conf.stats.statsmonitoringSamplePrefix..".sampletime";

local rdb_samples_count =conf.stats.statsmonitoringSamplePrefix..".samplescount";
local rdb_statsfilelock =conf.stats.statsmonitoringSamplePrefix..".statsfilelock";


local avcStatsObjectClass = rdbobject.getClass(conf.stats.statsmonitoringAvcPrefix,statsmonitorClassConfig)

local unidStatsObjectClass = rdbobject.getClass(conf.stats.statsmonitoringUnidPrefix,statsmonitorClassConfig)

--
local sample_time=""
local avc_tab = {}
local unid_tab = {}
local radio_tab={}
local cpu_tab ={}
local memory_tab ={}

local avc_node;
local unid_node;

local radio_prefix="radio.0." -- prefix in tmp file

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


------------------------------------------------
local function getStatsValue(name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['BytesSent'] 	= 'bytessent',
		['BytesReceived'] = 'bytesreceived',
		['FramesSent'] = 'framessent',
		['FramesReceived'] = 'framesreceived',
	}
	if not fieldMapping[pathBits[#pathBits]] then
		error('StatsMonitoring: unknown parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	local key =pathBits[#pathBits - 3];
	local id = pathBits[#pathBits - 2].."";
	local fieldname = fieldMapping[pathBits[#pathBits]];
	--dinfo("getStatsValue key=".. key..", id= "..id..", fieldname=".. fieldname);
	if key =='AVC' then
		if avc_tab[id] then
			return avc_tab[id][fieldname];
		end
	elseif key =='UNID' then
		if unid_tab[id] then
			return unid_tab[id][fieldname];
		end

	end

	--return sample_prefix .. '.' .. pathBits[#pathBits - 2] .. '.stats.' .. fieldMapping[pathBits[#pathBits]]
end

------------------------------------------------
local function getRadioStatsValue(name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['RSRP0'] 	= 'rsrp0',
		['RSRP1'] 	= 'rsrp1',
		['CINR0'] 	= 'cinr0',
		['CINR1'] 	= 'cinr1',
		['TxPower'] = 'txpower',
		['FramesReceived'] = 'framesreceived',
	}
	if not fieldMapping[pathBits[#pathBits]] then
		error('StatsMonitoring: unknown parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	local fieldname = fieldMapping[pathBits[#pathBits]];

	--return sample_prefix .. '.' .. pathBits[#pathBits - 2] .. '.stats.' .. fieldMapping[pathBits[#pathBits]]
	return radio_tab[fieldname];
end


------------------------------------------------
local function getSystemStatsValue(tag, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['MinUsage'] 	 = 'min',
		['MaxUsage'] 	 = 'max',
		['AverageUsage'] = 'average',
	}

	if not fieldMapping[pathBits[#pathBits]] then
		error('StatsMonitoring: unknown parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	local fieldname = fieldMapping[pathBits[#pathBits]];

	--return sample_prefix .. '.' .. pathBits[#pathBits - 2] .. '.stats.' .. fieldMapping[pathBits[#pathBits]]
	if tag =='cpu' then
		return cpu_tab[fieldname];
	else
		return memory_tab[fieldname];
	end

end

local function dump_node( node)
	dinfo('node dump');
	dinfo('name='.. node.name)
	dinfo('path='.. node:getPath())
	dinfo('type='.. node.type)
	dinfo('value='.. node.value)

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
	dinfo("get_node_value ".. node:getPath() .. " node.value: ".. node.value);
	return node.value;

end



local function update_tab(objClass, node, tab)

	local ids = objClass:getIds();

	-- remove deleted node
	for id, _ in  pairs(tab) do
		if not table.contains(ids, id) then

			--local obj_path= path..'.'..id.. '.';
			local child_node = node:getChild(id);
			if child_node then
				dinfo('delObject ' .. child_node:getPath())
				--dimclient.delObject(obj_path);
				local ret = child_node:deleteInstance(child_node:getPath(true))
				--dump_table(child_node);
			end
			tab[id] =nil;

		end
	end --for _, id in  pairs(tab) do

	--add new node
	for _, id in pairs(ids) do
		if not tab[id] then
			--local obj_path= path..'.'
			--local ret= dimclient.addObject(obj_path, id);
			--dinfo('addObject ', node:getPath(),  id)
			node.new_id = id;
			local ret, instanceId = node:createInstance(node:getPath(true))
			dinfo('addObject ', node:getPath(), ret, instanceId)
			--dump_table(node);
			if ret and ret ==0 then
				tab[instanceId]={};
			end

		end
	end
end



local function node_change_notify()

	if avc_node and unid_node then
		dinfo("node_change_notify");
		update_tab(avcStatsObjectClass, avc_node, avc_tab);
		update_tab(unidStatsObjectClass,unid_node, unid_tab);
		--if file is locked, delay 1 s
		if luardb.get(rdb_statsfilelock) =='1' then
			os.execute("sleep 1");
		end
		--read file
		local file = io.open(conf.stats.statsmonitoringfile);
		local l=0;
		if file then
			for line in file:lines() do
				-- sample times
				if l ==0 then
					sample_time = line;
				else
					local tab, id, name, val = string.match(line, "(%w+)%.(%d%d*)%.([%w%d]+):(.*)");

					--dinfo(line);
					--dinfo(tab..","..id..","..name..","..val);
					if tab then
						id=id.."";
						if tab =='avc' then
							if avc_tab[id] then
								dinfo(tab..","..id..","..name..","..val);
								avc_tab[id][name] = val;
							end
						elseif tab =='unid' then
							if unid_tab[id] then
								dinfo(tab..","..id..","..name..","..val);
								unid_tab[id][name] = val;
							end
						elseif tab == 'radio' then
							radio_tab[name] = val;
						elseif tab == 'cpu' then
							cpu_tab[name] = val
						elseif tab == 'memory' then
							memory_tab[name] = val
						end
					end
				end
				l = l +1
			end
			file:close();
		end
	end --if avc_node and unid_node then
end


--local samples_count=nil
local function SamplesCountChanged(rdb, val)

	local when = 'preSession'
	--if samples_count ~= val then
	--	samples_count = val;
		--samplesCount changed, insert callback
		--dinfo("samplescount changed: "..val)
		if avc_node and unid_node then
			if not client:isTaskQueued(when, node_change_notify) then
				--dinfo("samplescount changed: addTask")
				client:addTask(when, node_change_notify)
			end
			--node_change_notify();
		end

	--end --	if sample_time ~= val then

end

----------------------------------------------------
--- process handler
return {


	--- handler for SampleTime, readonly, watched
	['**.StatsMonitoring.Sample.SampleTime'] = {
		init = function(node, name, value)
			dinfo('Sample.SampleTime : model init')
			luardb.watch(rdb_samples_count, SamplesCountChanged);

			return 0
		end,
		get = function(node, name)
			get_node_value(node, sample_time);
			return 0, node.value;
		end,
		set = CWMP.Error.ReadOnly,
		unset = CWMP.Error.ReadOnly,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArgument,
	},

	['**.StatsMonitoring.Sample.Radio.Signal.*'] = {
		init = function(node, name)
			dinfo('Sample.Radio.Signal.* : model init')
			return 0
		end,
		get = function(node, name)

			get_node_value(node, getRadioStatsValue(name));
			return 0, node.value;
		end,
		set = CWMP.Error.ReadOnly,
		unset = CWMP.Error.ReadOnly,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArgument,
	},

	['**.StatsMonitoring.Sample.CPU.Stats.*'] = {
		init = function(node, name)
			dinfo('Sample.CPU.Stats.* : model init')
			return 0
		end,
		get = function(node, name)

			get_node_value(node, getSystemStatsValue('cpu', name));
			return 0, node.value;
		end,
		set = CWMP.Error.ReadOnly,
		unset = CWMP.Error.ReadOnly,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArgument,
	},
	['**.StatsMonitoring.Sample.Memory.Stats.*'] = {
		init = function(node, name)
			dinfo('Sample.Memory.Stats.* : model init')
			return 0
		end,
		get = function(node, name)

			get_node_value(node, getSystemStatsValue('memory', name));
			return 0, node.value;
		end,
		set = CWMP.Error.ReadOnly,
		unset = CWMP.Error.ReadOnly,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.InvalidArgument,
	},
	----
	-- AVC Container Collection handler
	----
	['**.StatsMonitoring.Sample.AVC'] = {
		init = function(node, name, value)
			dinfo('Sample.AVC: model init')
			node:setAccess('readwrite') -- allow creation of instances
			--dump_node(node);
			-- create instances
			local ids = avcStatsObjectClass:getIds()
			for _, id in ipairs(ids) do
				dinfo('Sample.AVC: creating existing instance ' .. id)
				local instance = node:createDefaultChild(id)
				-- new instance subtree is initialised by post-config parse tree init
				avc_tab[id] = {};
			end
			avc_node = node;

			node_change_notify();

			dinfo('Sample.AVC: model done')
			return 0
		end,

		get = function(node, name) return 0, '' end,
		set = CWMP.Error.ReadOnly,
		unset = CWMP.Error.ReadOnly,
		create = function(node, name)
			dinfo('Sample.AVC: create instance ', name)
			-- create parameter tree instance
			if node.new_id then
				local instanceId = node.new_id;
				local treeInstance = node:createDefaultChild(instanceId)
				treeInstance:recursiveInit()
				node.new_id =nil;
				return 0, instanceId
			end
			--node.instance = instanceId
			return CWMP.Error.ReadOnly
		end,
		delete = CWMP.Error.InvalidArgument,
	},
	----
	-- UNID Container Collection handler
	----

	['**.StatsMonitoring.Sample.UNID'] = {
		init = function(node, name, value)
			dinfo('Sample.UNID: model init')
			node:setAccess('readwrite') -- allow creation of instances
			-- create instances
			local ids = unidStatsObjectClass:getIds()
			for _, id in ipairs(ids) do
				dinfo('Sample.UNID: creating existing instance ' .. id)
				local instance = node:createDefaultChild(id)
				-- new instance subtree is initialised by post-config parse tree init
				unid_tab[id] = {};
			end
			unid_node = node;
			node_change_notify();
			return 0
		end,
		get = function(node, name) return 0, '' end,
		set = CWMP.Error.ReadOnly,
		unset = CWMP.Error.ReadOnly,
		create = function(node, name)
			dinfo('Sample.UNID: create instance ', name)
			-- create parameter tree instance
			if node.new_id then
				local instanceId = node.new_id;
				local treeInstance = node:createDefaultChild(instanceId)
				treeInstance:recursiveInit()
				node.new_id =nil;
				return 0, instanceId
			end
			--node.instance = instanceId
			return CWMP.Error.ReadOnly
		end,
		delete = CWMP.Error.InvalidArgument,
	},

	['**.StatsMonitoring.Sample.AVC|UNID.*'] = {
		init = function(node, name, value)
			dinfo('Sample.AVC|UNID.* init '..node:getPath())
			node.value = node.default
			return 0
		end,
		get = function(node, name) return 0, '' end,
		set = CWMP.Error.ReadOnly,
		create = CWMP.Error.ReadOnly,
		delete = function(node, name)
			dinfo('Sample.AVC|UNID: delete', name)
			node.parent:deleteChild(node)
			return 0;
		end,
	},

	----
	-- Instance Fields
	----
	['**.StatsMonitoring.Sample.AVC|UNID.*.Stats.*'] = {
		init = function(node, name, value)
			dinfo('Sample.AVC|UNID.*.Stats.*: init '..node:getPath())
			get_node_value(node, getStatsValue(name))
			return 0
		end,
		get = function(node, name)
			dinfo('Stats.Sample: get '.. node:getPath())
			return 0, get_node_value(node, getStatsValue( name))
		end,
		set = CWMP.Error.ReadOnly,
		unset = CWMP.Error.OK,
		create = CWMP.Error.ReadOnly,
		delete = CWMP.Error.OK,
	},
}
