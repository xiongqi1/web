
--require('rdbrpcclient')


local alarmsClassConfig = { persist = false }


--local alarmd = rdbrpcclient:new(conf.wntd.alarmdEndpoint)
local alarms = rdbobject.getClass(conf.wntd.alarmClass, alarmsClassConfig)

local alarm_node=nil;
local active_alarm_list={};
local cleared_event_table={};

local alarm_changed = nil;
local alarm_updating = false;

local debug = conf.wntd.debug;

debug=1;

------------------------------------------------
local function dinfo(v)
  if debug > 1 then
    dimclient.log('info', v)
  end
end

local function getRDBKey(node, name)
	local pathBits = name:explode('.')
	local id = pathBits[#pathBits - 1]
	local fieldMapping = {
		['Raised']	= 'raised',
		['Cleared']	= 'cleared',
		['Subsystem']	= 'subsys',
		['Description']	= 'message'
	}
	local rdb_name =  conf.wntd.alarmClass .. '.' .. id .. '.' .. fieldMapping[pathBits[#pathBits]];
	dinfo( 'getRDBkey ' .. rdb_name)
	return rdb_name;
end

local rdb_timestamp=conf.wntd.alarmdEndpoint..'.timestamp'


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
	dinfo( 'Alarm: node type='.. node.type ..' value =' ..node.value)

end


local function dump_node( node)
	if debug >2 then
		dinfo( 'Alarm: node name='.. node.name)
		dinfo( 'Alarm: node path='.. node:getPath())
		dinfo( 'Alarm: node type='.. node.type)
		dinfo( 'Alarm: node value='.. node.value)
	end
end
local function dump_list()

	for alarm_id, val in pairs(active_alarm_list) do
		if val then
			dinfo( 'Alarm: Active list alarm_id ='.. alarm_id )
		end
	end
end

local function update_all_node(node)

	-- create current instances
	local ids = alarms:getIds()
	local changed = 0;
	dinfo( 'Alarm: update_all_node start !!!')
	dump_list();

	alarm_updating = true;
	-- remove deleted node
	for alarm_id, val in pairs(active_alarm_list) do

		if val and not table.contains(ids, alarm_id) then
			local obj_path= node:getPath()..'.'..alarm_id.. '.';
			dinfo( 'Alarm: delObject ' .. obj_path )
			dimclient.delObject(obj_path);
			--node:deleteInstance(obj_path);
			--table.remove(active_alarm_list, i);
			active_alarm_list[alarm_id] =nil;
			changed = changed +1;
		end

	end
	--add new node
	for _, alarm_id in  pairs(ids) do
		if not active_alarm_list[alarm_id] then
		 -- new note
			local obj_path= node:getPath()..'.'
			dinfo( 'Alarm: addObject ' .. obj_path..', alarm_id= '..alarm_id)
			--new_alarm_id = alarm_id;
			local ret, id= dimclient.addObject(obj_path, alarm_id);
			--local ret = node:createInstance( obj_path, alarm_id);

			if ret and ret ==0 then

				active_alarm_list[alarm_id] = 1;
				--table.insert(active_alarm_list, alarm_id);
				dinfo( 'Alarm: addObject OK')
				changed = changed +1;

			end
		end

	end
	alarm_updating = false;

	--new_alarm_id=nil
	if changed >0 then
		--dump_list();
	end
	dinfo( 'Alarm: update_all_node done !!!')
end

local function node_change_notify()
	dinfo( 'Alarm: callback  !!!')

	if alarm_changed then

		update_all_node(alarm_node);
		alarm_changed = nil;

	end

end


return {
	----
	-- Container Collection
	----
	['**.Alarms.Timestamp'] = {
		init = function(node, name, value)
			node.value = node.default
			dinfo( 'Alarm: model init '.. name)
			dump_node(node);
			node:setAccess('readonly') -- no creation of instances via TR-069
			--
			-- install watcher for timestamp external changes
			local watcher = function(key, value)
				if value then
					dinfo( 'Alarms: external change of ' .. key)
					if value ~= node.value then
						dinfo( 'Alarms: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						-- find node "alarm"
						-- remove all the child
						-- create all the child
						if alarm_node then
							alarm_changed  = true;
							dimclient.callbacks.register('preSession', node_change_notify)
							--update_all_node(alarm_node);
						end
						--dimclient.setParameter(node:getPath(), value)

					end
				end

			end
			luardb.watch(rdb_timestamp, watcher)
			return 0

		end,
		get = function(node, name)

			rdb_get_value(node, rdb_timestamp)
			return node.value;
		end,

		set = cwmpError.funcs.OK;
		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.ResourceExceeded,
		delete = cwmpError.funcs.InvalidArgument
	},

	----
	-- Container Collection
	----
	['**.Alarms.Alarm'] = {
		init = function(node, name, value)
			node.value = node.default
			dinfo( 'Alarms.Alarm: model init')
			dump_node(node);
			alarm_node =node;

			node:setAccess('readonly') -- no creation of instances via TR-069

			--update_all_node(alarm_node);
---[[
			-- create current instances
			local ids = alarms:getIds()
			for _, id in ipairs(ids) do
				dinfo( 'Alarm: creating alarm_id= ' .. id)


				local instance = node:createDefaultChild(id)
				active_alarm_list[id] =1;
				--stance:setAccess(readwrite') -- instance can be deleted
			end
			dump_list();
--]]
			return 0
		end,
		get = function(node, name) return '' end,
		set = function(node, name, value)	return 0 end,

		unset = cwmpError.funcs.ReadOnly,
		--create = cwmpError.funcs.ResourceExceeded,
		--delete = cwmpError.funcs.InvalidArgument
		create = function(node, name, instanceId)
			dinfo( 'Alarm: create instance ' .. instanceId .. ' name = '.. name)
			if 	alarm_updating == false then
				return cwmpError.NotSupported;
			end
			-- create parameter tree instance
			local treeInstance = node:createDefaultChild(instanceId)

			treeInstance:recursiveInit()
			node.instance = instanceId
--[[
			-- watch .cleared
			local rdb_watcher = function(key, value)
				dinfo( 'Alarms: external change of ' .. key)
				if value ~= node.value then
					dinfo( 'Alarms: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
					local pathname= node:getPath().. '.' .. node.instance.. '.Cleared';
					dimclient.setParameter(node:getPath(), value)
				end

			end
			local rdb_name =  conf.wntd.alarmClass .. '.' .. node.instance.. '.Cleared'
			luardb.watch(rdb_name, rdb_watcher);
--]]

			return 0
		end,

		delete = function(node, name)
			 dinfo( 'Alarm: delete .Alarm'..name)
			return 0;
		end,

	},


	----
	-- Instance Object
	----
	['**.Alarms.Alarm.*'] = {
		init = function(node, name, value)
			node.value = node.default
			dinfo( 'Alarms.Alarm.* : init')
			dump_node(node);
			return 0
		end,
		get = function(node, name) return '' end,
		set = function(node, name)	return 0 end,
		unset = function(node, name)
			dinfo( 'Alarm: .Alarm.* unset  '..name)
			-- remove node in the cleared list
			for k, n in pairs(cleared_event_table) do
				if n == node then
					cleared_event_table[k] = nil;
					break;
				end
			end
			-- delete parameter tree instance
			node.parent:deleteChild(node)
			return 0

		end,
		--create = cwmpError.funcs.InvalidArgument,
		create = function(node, name)
			dinfo( 'Alarms.Alarm.* create '..name)
			return 0;
		end,

		delete = function(node, name)
			dinfo( 'Alarms.Alarm.*: delete  '..name)

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
			dinfo( 'Alarm: delete instance ' .. id)

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
	['**.Alarms.Alarm.*.*'] = {
		init = function(node, name, value)
			-- skip default node instance
			dinfo( 'Alarm: init **.Alarms.Alarm.*.*')
			dump_node(node);
			if node.type == 'default' then
				node.value = node.default
				return 0;
			end
			local key = getRDBKey(node, name);
			rdb_get_value(node, key);
			if node.name ~= 'Cleared' then
				return 0;
			end
---[[
		-- install watcher for external changes
			local watcher = function(key, value)

				-- the rdb detection will be detected now, so if not value
				if value then
					dinfo( 'Alarm.*.Cleared: external change of ' .. key)
					--find node
					local node = cleared_event_table[key];
					if node  then
						if value ~= node.value then

							dinfo( 'Alarm.*.Cleared: ' .. key .. ' changed: "' .. value .. '" -> "' .. node.value .. '"')

							dimclient.setParameter(node:getPath(), value)
						end
					end
				end
			end

			cleared_event_table[key] = node;

			luardb.watch(key, watcher)
--]]

			return 0
		end,
		get = function(node, name)
			dinfo( 'Alarms.Alarm.*.*: get name = '.. name .. " ntype= "..node.type)

			getRDBValue(node, name);

			return node.value
		end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.OK,
		create = cwmpError.funcs.ReadOnly,
		delete = cwmpError.funcs.OK,
--		create = cwmpError.funcs.InvalidArgument,
	--	delete = cwmpError.funcs.InvalidArgument
	},
}
