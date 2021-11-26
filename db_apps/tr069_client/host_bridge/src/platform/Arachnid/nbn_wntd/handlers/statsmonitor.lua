-- #!/usr/bin/env lua
require('stringutil')
require('tableutil')

local debug = conf.wntd.debug;

debug=1



local statsmonitorClassConfig = { persist = false }



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
local avc_path;
local unid_path;



------------------------------------------------
local function dinfo(v)
  if debug > 1 then
    dimclient.log('info', v)
  end
end

------------------------------------------------
local function getStatsValue(node, name)
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
	--dinfo("get_node_value ".. node:getPath() .. " node.value: ".. node.value);
	return node.value;

end



local function update_tab(objClass, path, tab)

	local ids = objClass:getIds();

	-- remove deleted node
	for id, _ in  pairs(tab) do
		if not table.contains(ids, id) then
			local obj_path= path..'.'..id.. '.';
			dinfo('delObject ' .. obj_path )
			dimclient.delObject(obj_path);
			tab[id] =nil;
		end
	end --for _, id in  pairs(tab) do

	--add new node
	for _, id in pairs(ids) do
		if not tab[id] then
			local obj_path= path..'.'
			dinfo('addObject ' .. obj_path..', id= '..id)
			local ret= dimclient.addObject(obj_path, id);
			if ret and ret ==0 then
				tab[id]={};
				dinfo('addObject OK')
			end

		end
	end
end



local function node_change_notify()

	if avc_path and unid_path then
		dinfo("node_change_notify");
		update_tab(avcStatsObjectClass, avc_path, avc_tab);
		update_tab(unidStatsObjectClass,unid_path, unid_tab);
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
					local tab, id, name, val = string.match(line, "(%w+)%.(%d%d*)%.(%w+):(.*)");

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

						end
					end
				end
				l = l +1
			end
			file:close();
		end
	end --if avc_path and unid_path then
end

local function queueChange()
	dimclient.callbacks.register('preSession', node_change_notify)
end

--local samples_count=nil
local function SamplesCountChanged(rdb, val)

	--if samples_count ~= val then
	--	samples_count = val;
		--samplesCount changed, insert callback
		--dinfo("samplescount changed: "..val)
		if avc_path and unid_path then
			dimclient.callbacks.register('preSession', node_change_notify)
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
			return node.value;
		end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.ReadOnly,
		delete = cwmpError.funcs.InvalidArgument,
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
				instance:setAccess('readwrite') -- instance can be deleted
				avc_tab[id] = {};
			end
			avc_path = node:getPath();
			node_change_notify();

			dinfo('Sample.AVC: model done')
			return 0
		end,

		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = function(node, name, instanceId)
			dinfo('Sample.AVC: create instance ' .. instanceId)
			-- create parameter tree instance
			local treeInstance = node:createDefaultChild(instanceId)
			treeInstance:recursiveInit()
			node.instance = instanceId
			return 0
		end,
		delete = cwmpError.funcs.InvalidArgument,
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
				instance:setAccess('readwrite') -- instance can be deleted
				unid_tab[id] = {};
			end
			unid_path = node:getPath();
			node_change_notify();
			return 0
		end,
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = function(node, name, instanceId)
			dinfo('Sample.UNID: create instance ' .. instanceId)
			-- create parameter tree instance
			local treeInstance = node:createDefaultChild(instanceId)
			treeInstance:recursiveInit()
			node.instance = instanceId
			return 0
		end,
		delete = cwmpError.funcs.InvalidArgument,
	},

	['**.StatsMonitoring.Sample.AVC|UNID.*'] = {
		init = function(node, name, value)
			dinfo('Sample.AVC|UNID.* init '..node:getPath())
			node.value = node.default
			return 0
		end,
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = function(node, name)
			dinfo('Sample.AVC|UNID: unset'.. name)
			node.parent:deleteChild(node)
			return 0;
		end,
		create = cwmpError.funcs.ReadOnly,
		delete = cwmpError.funcs.OK,
	},

	----
	-- Instance Fields
	----
	['**.StatsMonitoring.Sample.AVC|UNID.*.Stats.*'] = {
		init = function(node, name, value)
			dinfo('Sample.AVC|UNID.*.Stats.*: init '..node:getPath())
			get_node_value(node, getStatsValue(node, name))
			return 0
		end,
		get = function(node, name)
			dinfo('Stats.Sample: get '.. node:getPath())
			return get_node_value(node, getStatsValue(node, name))
		end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.OK,
		create = cwmpError.funcs.ReadOnly,
		delete = cwmpError.funcs.OK,
	},
}
