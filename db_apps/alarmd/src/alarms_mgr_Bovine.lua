#!/usr/bin/lua

require('luardb')
require('luasyslog')
require('rdbobject')
require('rdbrpcclient')
require('tableutil')

conf = {
	alarmClass = 'alarms',
	alarmEndpoint = 'alarmd',
	alarmCnfClass = 'alarmCnf',
	--avcPrefix = 'avc',

	POLL_TIMEOUT =20,

	-- message source type
	MESSGAGE_TYPE_BOOL="bool",
	MESSGAGE_TYPE_MSG2="msg2",
	MESSGAGE_TYPE_MSGRDB="msgrdb",
	MESSGAGE_TYPE_RDB2="rdb2",
	MESSGAGE_TYPE_DYNAMIC="dynamic",
	MESSGAGE_TYPE_CONDITION="condition",

	DELETE_ALARMS_EVENT_TIMEOUT=120, -- default tr069 time period is 60s,

	SEND_ALARMS_EVENT_TIMEOUT=30,

	start_time=os.time(),

}
local	debug =0;

local rpc = rdbrpcclient:new(conf.alarmEndpoint);

--local avc_object = rdbobject.getClass(avcPrefix)

if not rpc  then
	luasyslog.open('alarms_mgr', 'LOG_DAEMON')

	luasyslog.log('notice', 'alarms_mgr not started due to rdbrpcclient failure')
	luasyslog.close();
	return ;

end



-- setup syslog facility
--luasyslog.open('alarms_mgr', 'LOG_DAEMON')

--[[
	alarm obj,
	each alarm obj would be

	alarms.1.occurred =110102
	alarms.1.subsys="Reset"
	alarms.1.message ="outdoor has been reboorted"



--]]

local alarms_obj=
{
	--rdb_prefix="alarms",

	rdb_acknowledge="alarms.acknowledge", -- acknowledge list
	cleared_items_count= 0,
	active_item_count=0;
	--rdb_changed_detected=0,
	--rdb_changed_key ="",
	--rdb_changed_value="",

	rdb_ack_detected=0,
	rdb_ack_key ="",
	rdb_ack_value="",

--[[
	rdb_index="alarms._index",
	rdb_id	=1,

	rdb_occurred ="raised",
	rdb_cleared ="cleared",
	rdb_subsys ="subsys",
	rdb_message ="message",
	raise_index="",

	lastestalarm =0,
--]]
}

local post_rdb_changed={};

local function set_alarm_on(subsys, message)
	if debug >0 then print( "set_alarm_on("..subsys ..",".. message..")>>>"); end
	local id = rpc:invoke('raise', { ['subsys'] = subsys, ['message'] = message }, conf.SEND_ALARMS_EVENT_TIMEOUT)
	if debug >0 then print( "set_alarm_on("..subsys ..",".. message..")<<< "..id);end
	id = tonumber(id) or 0;
	if id >0 then
		alarms_obj.active_item_count = alarms_obj.active_item_count+1;
	end

	return id;
end

local function _set_alarm_clear(id)


	alarms_obj.cleared_items_count = alarms_obj.cleared_items_count +1;


	local result  = rpc:invoke('clear', { ['id'] = id }, conf.SEND_ALARMS_EVENT_TIMEOUT)


	return result;
end

local function set_alarm_clear(id)

	if debug >0  then print( "set_alarm_clear("..id..")>>>"); end

	ret = pcall(_set_alarm_clear, id);

	if debug >0 then print( "set_alarm_clear("..id..")<<<", ret);end

end


local function list_alarms()
	local alarms = rdbobject.getClass(conf.alarmClass)
	local ids = alarms:getAll();
	for i, alarm in ipairs(ids) do
		print(alarms:getId(alarm), tostring(alarm.raised) .. ':' .. tostring(alarm.cleared), alarm.subsys, alarm.message)
	end

end

function flush_alarms()
	local alarms = rdbobject.getClass(conf.alarmClass)
	local ids = alarms:getAll();
	for i, alarm in ipairs(ids) do
		--print("delete ", alarms:getId(alarm));
		local ret = rpc:invoke('delete', { ['id'] = alarms:getId(alarm) }, conf.SEND_ALARMS_EVENT_TIMEOUT)
		--print("delete after");

	end

end

--local g_alarm_condition_changed =0;



----------------------------------------<STRAR> rules----------------------------------------

-- return system uptime as number type 
local function get_systemtime()
	local file, msg, errno = io.open('/proc/uptime', 'r')

	if file then
		local n = math.floor(file:read('*n'))
		file:close()
		return n
	end
	return 0
end

function init_event_generic (name)

	if debug > 0 then print("init_event_generic (".. name ..")"); end

	local rule = g_rules[name]
	if not rule then return end

	local event = rule.event

	event.StartOfPeriod = 0
	event.EndOfPeriod = 0
	event.AlarmAt = 0
	event.EndOfCooldown = 0
	event.TriggerTbl = {}
end

function reset_event_generic (name, currTime)
	if debug > 0 then print("reset_event_generic (".. name ..")"); end

	local rule = g_rules[name]
	if not rule then return end

	local event = rule.event
	local config = rule.currCnf
	local currentTime = currTime or get_systemtime()

	event.StartOfPeriod = currentTime
	event.EndOfPeriod = currentTime + config.period
	event.TriggerTbl = {}

	if debug > 0 then
		print("reset_event_generic: event.StartOfPeriod=" .. event.StartOfPeriod)
		print("reset_event_generic: event.EndOfPeriod=" .. event.EndOfPeriod)
	end

end

function set_event_generic (name)
	if debug > 0 then print("set_event_generic (".. name ..")"); end

	local rule = g_rules[name]
	if not rule then return end

	local config = rule.currCnf

	if config.threshold == 0 or config.period == 0 then return end

	local event = rule.event
	local currentTime = get_systemtime()

	table.insert(event.TriggerTbl, currentTime)
	

	if debug > 0 then
		print("------trigger table---------------------------------------")
		local result = table.concat(event.TriggerTbl, ',')
		print("event.TriggerTbl= " .. (result or 'nil'))
		print("----------------------------------------------------------")
	end

end

local function clear_condition_alarm(name)
	if not name then return end

	if debug > 0 then print("clear_condition_alarm (".. name ..")"); end

	local class = rdbobject.getClass(conf.alarmClass)
	local instances = class:getByProperty('subsys', name)

	for _, obj in ipairs(g_alarm_source) do
		if obj.type == conf.MESSGAGE_TYPE_CONDITION and obj.subsys == name then
			for _, inst in ipairs(instances) do
				alarm_id =  class:getId(inst)
				rpc:invoke('delete', { ['id'] = alarm_id}, 20)
			end
			break;
		end
	end
end

function eval_event_generic (name)
	if debug > 0 then print("eval_event_generic (".. name ..")"); end

	local rule = g_rules[name]
	if not rule then return end

	local config = rule.currCnf
	if config.threshold == 0 or config.period == 0 then return end

	local event = rule.event
	local currentTime = get_systemtime()

	if event.EndOfPeriod < currentTime then
		rule.methods.reset_event(name, currentTime)
	end

	local tempTbl = {}
	for i, value in ipairs(event.TriggerTbl) do
		if value >= event.StartOfPeriod then
			table.insert(tempTbl, value)
		end
	end

	event.TriggerTbl = tempTbl

	if event.EndOfCooldown < currentTime then
		if #event.TriggerTbl >= config.threshold then
			local obj = nil

			for _, element in ipairs(g_alarm_source) do
				if element.type == conf.MESSGAGE_TYPE_CONDITION and element.subsys == name then
					obj = element
					break
				end
			end
			if obj then
				clear_condition_alarm(obj.subsys)

				local alarmTime=os.date('%s')
				local startTime= alarmTime + event.StartOfPeriod - currentTime
				alarmTime = alarmTime and os.date('%FT%TZ', alarmTime) or nil
				startTime = startTime and os.date('%FT%TZ', startTime) or nil

				local count=#event.TriggerTbl
				local message = nil

				if startTime and alarmTime and count then
					message = count .. ' time(s) occurred between ' .. startTime .. ' and ' .. alarmTime
				end

				obj.id = set_alarm_on(obj.subsys, (message or obj.messages))
				rule.methods.reset_event(name, currentTime)
				event.AlarmAt = currentTime
				if config.cooldownperiod > 0 then
					event.EndOfCooldown = currentTime + config.cooldownperiod
				else
					event.EndOfCooldown = 0
				end
			end
		end
	end

	rule.methods.update_conf_counter(name)

end

function init_conf_generic (name)
	if debug > 0 then print("init_conf_generic (".. name ..")"); end

	local rule = g_rules[name]
	if not rule then return end

	local class = rdbobject.getClass(conf.alarmCnfClass)
	local instances = class:getByProperty('name', name)

	local currentCnf = rule.currCnf
	local defaultCnf = rule.defaultCnf

	for _, inst in ipairs(instances) do
		local id = class:getId(inst)
		local prefix = conf.alarmCnfClass .. '.' .. id .. '.'

		currentCnf.counter = inst.counter and tonumber(inst.counter) or defaultCnf.counter
		currentCnf.threshold = inst.threshold and tonumber(inst.threshold) or defaultCnf.threshold
		currentCnf.period = inst.period and tonumber(inst.period) or defaultCnf.period
		currentCnf.cooldownperiod = inst.cooldownperiod and tonumber(inst.cooldownperiod) or defaultCnf.cooldownperiod
		luardb.watch(prefix ..  'threshold' , rule.methods.update_conf)
		luardb.watch(prefix ..  'period' , rule.methods.update_conf)
		luardb.watch(prefix ..  'cooldownperiod' , rule.methods.update_conf)
		break
	end
end

function update_conf_generic (key, value)
	if debug > 0 then print("update_conf_generic (".. key .. ', ' .. value ..")"); end

	for name, obj in pairs(g_rules) do
		local rule = obj
		if rule then
			local class = rdbobject.getClass(conf.alarmCnfClass)
			local instances = class:getByProperty('name', name)
			local updated = false
			local currentCnf = rule.currCnf

			for _, inst in ipairs(instances) do
				for key, value in pairs(currentCnf) do
					if key ~= 'counter' and currentCnf[key] ~= tonumber(inst[key]) then
						currentCnf[key] = tonumber(inst[key])
						updated = true
						if key == 'cooldownperiod' then rule.event.EndOfCooldown = 0 end
					end
				end
				break;
			end

			if updated then
				rule.methods.reset_event(name)
			end
		end
	end
end

function update_conf_counter_generic (name)
	if debug > 0 then print("increase_conf_counter_generic (".. name ..")"); end

	local rule = g_rules[name]
	if not rule then return end

	local event = rule.event
	local currentCnf = rule.currCnf

	local class = rdbobject.getClass(conf.alarmCnfClass)
	local instances = class:getByProperty('name', name)

	for _, inst in ipairs(instances) do

		if tonumber(inst.counter) ~= #event.TriggerTbl then
			inst.counter = #event.TriggerTbl
			currentCnf.counter = #event.TriggerTbl
		end
		break;
	end
end

g_rules = {
	['CellID'] = {  --> the key should be same with g_alarm_source.{x}.subsys
		event = {
			StartOfPeriod = 0,
			EndOfPeriod = 0,
			AlarmAt = 0,
			EndOfCooldown = 0,
			TriggerTbl = {},  --> this is the array to store number typed system time the event is triggered
		},
		currCnf = {},
		defaultCnf = {
			counter = 0,
			threshold = 2,
			period = 3600,
			cooldownperiod = 3600,
		},

		methods= {
			init_event = init_event_generic,
			reset_event = reset_event_generic,
			set_event = set_event_generic,
			eval_event = eval_event_generic,
			init_conf = init_conf_generic,
			update_conf = update_conf_generic,
			update_conf_counter = update_conf_counter_generic,
		},
	},
	['RAT'] = { --> the key should be same with g_alarm_source.{x}.subsys
		event = {
			StartOfPeriod = 0,
			EndOfPeriod = 0,
			AlarmAt = 0,
			EndOfCooldown = 0,
			TriggerTbl = {},
		},
		currCnf = {},
		defaultCnf = {
			counter = 0,
			threshold = 2,
			period = 3600,
			cooldownperiod = 3600,
		},

		methods= {
			init_event = init_event_generic,
			reset_event = reset_event_generic,
			set_event = set_event_generic,
			eval_event = eval_event_generic,
			init_conf = init_conf_generic,
			update_conf = update_conf_generic,
			update_conf_counter = update_conf_counter_generic,
		},
	},
}


local function callRuleMethod(objName, funcName)

	if debug > 0 then print("callRuleMethod (".. objName .. "," .. funcName ..")"); end

	local obj = g_rules[objName]

	if not obj or not obj.methods[funcName] or type(obj.methods[funcName]) ~= 'function' then return end

	obj.methods[funcName](objName)
end

local function set_condition_alarm (obj , val)
	if debug > 0 then print("set_condition_alarm (".. obj.rdb ..",".. val..")"); end

	val = tonumber(val);

	if not val then
		val =0;
	end

	local active = val and val > 0;

	if active then
		callRuleMethod(obj.subsys, 'set_event')
		luardb.set(obj.rdb, '')
	end
end

----------------------------------------< END > rules----------------------------------------

g_alarm_source=
{
        {
                type=conf.MESSGAGE_TYPE_CONDITION,
                subsys="CellID", --> the value of subsys should be same with the key of g_rules
                rdb="cellID_alarm.trigger",
                messages="Cell ID Alarm triggered",
                acknownledgable=0,
                active = 0,
                id =0,
        },
        {
                type=conf.MESSGAGE_TYPE_CONDITION,
                subsys="RAT", --> the value of subsys should be same with the key of g_rules
                rdb="rat_alarm.trigger",
                messages="RAT Alarm triggered",
                acknownledgable=0,
                active = 0,
                id =0,
        },
}

local rdb_unid_changed="unid.changed";


local function dump_obj(obj)

	print(obj.rdb, obj.type, obj.id , obj.active);
end


--[[
local g_alarm_source_file=
{
	{
		type=conf.MESSGAGE_TYPE_FILE,
		subsys="Reset",
		--rdb="indoor.comms_failed",
		not_exist="/tmp/etc_rw/normal_reboot",
		must_exist="/tmp/etc_rw/cdcs",
		raisetime,
		cleartime,
		messages={"Outdoor reset(power failure)"},
		acknownledgable=1,
	},
}
--]]

local function set_bool_alarm( obj , val)

	if debug > 0 then print("set_bool_alarm (".. obj.rdb ..",".. val..")"); end

	val = tonumber(val);

	if not val then
		val =0;
	end

	active = val and val >0;
	if active ~= obj.active then
		if obj.active == true then
			-- deactive now
			set_alarm_clear(obj.id);
			obj.id=0;
		end

		if active == true then
			-- active now
			obj.id = set_alarm_on(obj.subsys, obj.messages);
		end

		obj.active = active;
	end
end

function hasbit(x, p)
  return x % (p + p) >= p
end

function setbit(x, p)
  return hasbit(x, p) and x or x + p
end

function clearbit(x, p)
  return hasbit(x, p) and x - p or x
end

local function set_msg2_alarm( obj, val)

	val = tonumber(val);
	if not val then
		val =0;
	end

	active =0;
	for i=1,2 do
		if hasbit(val, obj.bitmap[i]) then
			active = i;
			break;
		end
	end
	--print("set_msg2_alarm (".. obj.rdb ..",".. val.."), active ="..active);

	if active ~= obj.active then

		-- if old state is active, remove this alarm at first
		if  obj.id and obj.id>0 then
			set_alarm_clear(obj.id);
			obj.id =0;
		end

		if active ~=0 then
			-- active now
			obj.id = set_alarm_on(obj.subsys, obj.messages[active]);
		end


		obj.active = active;
	end

end


local function set_msgrdb_alarm( obj, val)


	active =val or "";

	if debug >0 then print("set_msg2_alarm (".. obj.rdb ..",".. val.."), active ="..active); end

	if active ~= obj.active then

		-- if old state is active, remove this alarm at first
		if  obj.id and obj.id>0 then
			set_alarm_clear(obj.id);
			obj.id =0;
		end

		if active ~= "" then
			-- active now
			obj.id = set_alarm_on(obj.subsys, obj.messages .. active);
		end


		obj.active = active;
	end

end
local function set_rdb2_alarm(obj, val)

	val = val or "Up";

	if debug >0 then
		print("set_rdb2_alarm (".. obj.rdb ..",".. val..")>>>");
	end

	if val ~= obj.active then


		if obj.id and obj.id > 0 then

			set_alarm_clear(obj.id);
			obj.id =0;
		end
		local found =false;
		-- need read rdb2 for display msg
		for _, key in pairs(obj.keywords) do
			if val == key then
				obj.id = set_alarm_on(obj.subsys, obj.prefix ..luardb.get(obj.rdb2));
				found =true;
				break;
			end
		end
		if not found then
			-- directly use keyword as display msg
			for _, key in pairs(obj.keyword_msg) do
				if val == key then
					obj.id = set_alarm_on(obj.subsys, obj.prefix ..val);
					found =true;
					break;
				end
			end
		end
--[[
		if val == obj.keywords[1] then
			-- active now
			obj.id = set_alarm_on(obj.subsys, obj.prefix ..luardb.get(obj.rdb2));
		elseif val == obj.keywords[2] then
			-- active now
			obj.id = set_alarm_on(obj.subsys, obj.prefix ..val);

		elseif val == obj.keywords[3] then

			obj.id = set_alarm_on(obj.subsys, obj.prefix ..val);

		end
--]]
		obj.active=val;


	end
	if debug >0 then
		print("set_rdb2_alarm () <<<");
	end

end

--[[
function set_dynamic_alarm(obj, keywords, val)

	if debug >0 then print("set_dynamic_alarm(".. obj.rdb_status ..","..val.."), previous status=" .. obj.active); end

	--if status changed.
	if val ~= obj.active then
		--print("set_dynamic_alarm: state changed from "..obj.active, "to "..val);
		-- set old alarm clear anyware
		if obj.id and obj.id > 0 then
			set_alarm_clear(obj.id);
			obj.id =0;
		end

		-- set new alarm
		if val == keywords[1] then
			-- active now
			obj.id = set_alarm_on(obj.subsys, luardb.get(obj.rdb_lasterr));
		elseif val == keywords[2] then
			-- active now
			obj.id = set_alarm_on(obj.subsys, val);

		elseif val == keywords[3] then

			obj.id = set_alarm_on(obj.subsys, val);

		end

		--print("set_dynamic_alarm:new acitve  =" .. val, "id =" .. obj.id);

		obj.active=val;

		if debug >0 then
			dump_obj(obj)
		end

	end

end

function remove_dynamic_alarm(obj, alarm_id)
	for id, childobj in pairs(obj.active_list) do

		if childobj.id and childobj.id == alarm_id then
			childobj.id =0;
			if childobj.delete then
				obj.active_list[id]= nil;
			end
			return true;
		end
	end
	return false;
end




function scan_dynamic_alarm(obj, changed_list)

	if debug >0 then print("\nscan_dynamic_alarm(".. obj.subsys..")");end;

	local _active_list = obj._active_list;

	-- collect all active id list
	for id  in pairs(_active_list) do


		local rdb = obj.prefix .. id.. obj.enable;
		local ret, value= pcall(luardb.get, rdb);
		if ret and value then

			if debug >0 then print("scan_dynamic_alarm: "..rdb .."="..value); end;
			value = tonumber(value);

			if value and value >0 then
				-- active item

				if obj.active_list[id] then
					--update
					--print("update id ="..id .. ", alarm_id="..obj.active_list[id].id);
					val = luardb.get(obj.active_list[id].rdb_status) or 'normal'

					set_dynamic_alarm(obj.active_list[id],  obj.keywords, val);
					obj.active_list[id].delete =0;

				else
					--add
					--print("add new  id ="..id);
					obj.active_list[id] = {rdb_status=obj.prefix..id..obj.status,
											rdb_lasterr=obj.prefix..id..obj.lasterr,
											subsys=obj.subsys,
											active ="", -- error status
											id=0,
											delete=0,
											};
					val = luardb.get(obj.active_list[id].rdb_status) or 'normal'

					set_dynamic_alarm(obj.active_list[id],  obj.keywords, val);
				end


			else
				-- deactive
				if  obj.active_list[id] then
					-- cleared  item
					--print("cleared  id ="..id);
					set_dynamic_alarm(obj.active_list[id], obj.keywords, "normal");
					obj.active_list[id].delete =1;

				else
				   --nothing
				end
			end --if value >0 then
			luardb.watch(obj.prefix..id..obj.status, dynamic_rdb_alarm_changed);
		else
			luardb.watch(obj.prefix..id..obj.status, nil);

		end --		if value then

	end --	for id  in pairs(_active_list) do


end

--]]


local function rdb_alarm_changed(key, val)

	if debug >0 then print( "rdb_alarm_changed (".. key ..',' .. val..')');end;

	for name, obj in pairs(g_alarm_source) do

		if obj.rdb == key then

			--if debug >0 then print( "rdb = ".. key, "obj.type= ".. obj.type);end;
			if obj.type == conf.MESSGAGE_TYPE_BOOL then
				set_bool_alarm(obj, val);
			elseif obj.type == conf.MESSGAGE_TYPE_MSG2 then
				set_msg2_alarm(obj, val);

			elseif obj.type == conf.MESSGAGE_TYPE_MSGRDB then
				set_msgrdb_alarm(obj, val);

			elseif obj.type == conf.MESSGAGE_TYPE_RDB2 then
				set_rdb2_alarm(obj, val);

			elseif obj.type == conf.MESSGAGE_TYPE_CONDITION then
				set_condition_alarm(obj, val);

			--elseif obj.type == conf.MESSGAGE_TYPE_DYNAMIC then
				--print( "dynamic changed");
			--	scan_dynamic_alarm(obj, val);
			end
		end
	end
end
local function rdb_alarm_changed_hook(key, val)
	post_rdb_changed[key] = val;
	--alarms_obj.rdb_changed_detected =1;
	--alarms_obj.rdb_changed_key=key;
	--alarms_obj.rdb_changed_value=val;
end


local function unid_changed(key, val)
	-- rescan unid list

	for _, obj in pairs(g_alarm_source) do
		if obj.subsys == "UNID" then

			local ret, value = pcall(luardb.get, obj.rdb);
			--print("unid_changed: ", obj.show, obj.rdb, ret, value);
			if not value then
			--[[
					if obj.show ==1 then
						luardb.watch(obj.rdb, nil);
						if debug >0 then
							print("unid_changed: remove watch", obj.rdb);
						end
					end
			--]]
					set_rdb2_alarm(obj, value);

				--end
				obj.show = 0;
			else
				if obj.show ==0 then
					luardb.watch(obj.rdb, rdb_alarm_changed_hook);
					if debug >0 then
						print("unid_changed: add watch", obj.rdb);
					end
				end
					set_rdb2_alarm(obj, value);
				--end;
				obj.show =1;

			end
		end
	end

end


local function rdb_acknowledge_changed(key, val)
	if val then
		if debug >0 then print("rdb_acknowledge_changed(".. key..","..val..")"); end

		num =0;
		for d in string.gmatch(val, "(%d+)") do

			d = tonumber(d);
			for name, obj in pairs(g_alarm_source) do

				if obj.active and obj.active ~=0 then

	--[[
							if debug >0 then
								print( "current d ", d);
								dump_obj(obj);
							end
	--]]
					if obj.id == d and obj.acknownledgable ==1 then

						-- clear this state
						if obj.type == conf.MESSGAGE_TYPE_BOOL then
							luardb.set(obj.rdb, "0");

						elseif obj.type == conf.MESSGAGE_TYPE_MSG2 then
							luardb.set(obj.rdb, "0");
						elseif obj.type == conf.MESSGAGE_TYPE_MSGRDB then
							luardb.set(obj.rdb, "");
						end
					end
				end
			end
			num = num+1;
		end
		if num >0 then
			--print( "acknowledge received : " .. val);
			luardb.set(alarms_obj.rdb_acknowledge, "", 'p');
		end
	end
end
--[[
local function rdb_acknowledge_changed_hook(key, val)
	alarms_obj.rdb_ack_detected =1;
	alarms_obj.rdb_ack_key=key;
	alarms_obj.rdb_ack_value=val;

end
--]]

local function cleanup_alarms(current_time)

	if alarms_obj.cleared_items_count >0 then
		local alarms = rdbobject.getClass(conf.alarmClass)
		local ids = alarms:getAll()
		for i, alarm in ipairs(ids) do

			--print(alarms:getId(alarm), tostring(alarm.raised) .. ':' .. tostring(alarm.cleared), alarm.subsys, alarm.message)
			local t = tonumber(alarm.cleared);
			if t  then
				if (t + conf.DELETE_ALARMS_EVENT_TIMEOUT) < current_time then
					alarm_id =  alarms:getId(alarm);
					rpc:invoke('delete', { ['id'] = alarm_id}, 20)
					alarms_obj.cleared_items_count =alarms_obj.cleared_items_count-1;
					--deactive item
					for _, obj in pairs(g_alarm_source) do

						--if obj.type == conf.MESSGAGE_TYPE_DYNAMIC then
						--	if remove_dynamic_alarm(obj, alarm_id) then
						--		break;
						--	end
						if obj.id and obj.id  == alarm_id then
							obj.id =0;
							break;

						end
					end

				end
			end
		end

		--os.execute("rdb dump alarms|sort");
		--list_alarms();

	end
	-- if the delete does not success, this var could be negative, correct it.
	if alarms_obj.cleared_items_count <0 then
		alarms_obj.cleared_items_count =0;
	end

end

--[[
	* fields of conf.alarmCnfClass object
 -- name
 -- counter
 -- threshold
 -- period
 -- cooldownperiod
--]]
local function init_ConfigObject(name)
	local nameTbl = {}
	local rdbobject_config = { persist = true, idSelection = 'smallestUnused'}
	local class = rdbobject.getClass(name, rdbobject_config)

	for name, obj in pairs(g_rules) do
		table.insert(nameTbl, name)
	end

	local instances = class:getAll()

	for _, inst in ipairs(instances) do
		if not table.contains(nameTbl, inst.name) then
			class:delete(inst)
		end
	end

	for name, obj in pairs(g_rules) do
		local instances = class:getByProperty('name', name)

		if #instances == 0 then
			local newConf = class:new()

			newConf['name'] = name
			for key, value in pairs(obj.defaultCnf) do
				newConf[key] = value
			end
			newConf.counter = 0
		else
			for i, inst in ipairs(instances) do
				if i > 1 then
					class:delete(inst)
				end
			end
		end
	end
end

local function initialize()

	init_ConfigObject(conf.alarmCnfClass);
	flush_alarms();

	for name, obj in pairs(g_alarm_source) do

		if obj.type == conf.MESSGAGE_TYPE_BOOL then

			luardb.watch(obj.rdb, rdb_alarm_changed_hook);
			set_bool_alarm(obj, luardb.get(obj.rdb));
		elseif obj.type == conf.MESSGAGE_TYPE_MSG2 then

			luardb.watch(obj.rdb, rdb_alarm_changed_hook);
			set_msg2_alarm(obj, luardb.get(obj.rdb));
		elseif obj.type == conf.MESSGAGE_TYPE_MSGRDB then

			luardb.watch(obj.rdb, rdb_alarm_changed_hook);
			set_msgrdb_alarm(obj, luardb.get(obj.rdb));

		elseif obj.type == conf.MESSGAGE_TYPE_RDB2 then
			if obj.show ==1 then
				luardb.watch(obj.rdb, rdb_alarm_changed_hook);
				set_rdb2_alarm(obj, luardb.get(obj.rdb));
			end

		elseif obj.type == conf.MESSGAGE_TYPE_CONDITION then
			callRuleMethod(obj.subsys, 'init_conf')
			callRuleMethod(obj.subsys, 'reset_event')
			luardb.watch(obj.rdb, rdb_alarm_changed_hook);
			set_condition_alarm(obj, luardb.get(obj.rdb));

--[[
		elseif obj.type == conf.MESSGAGE_TYPE_DYNAMIC then
			--print("watch ".. obj.rdb);

			luardb.watch(obj.rdb, rdb_alarm_changed_hook);
			scan_dynamic_alarm(obj,luardb.get(obj.rdb));
--]]
		end

	end

	unid_changed(rdb_unid_changed, "");

	luardb.set(alarms_obj.rdb_acknowledge, "", 'p');

	luardb.watch(alarms_obj.rdb_acknowledge, rdb_acknowledge_changed);

	luardb.watch(rdb_unid_changed, unid_changed);

	--detect_file_alarm();

end






initialize();


while( true) do


	luardb.wait(conf.POLL_TIMEOUT);

	for rdb, val in pairs(post_rdb_changed) do
		if val then
			rdb_alarm_changed(rdb, val);
		end
	end
	post_rdb_changed={};
--[[
	if alarms_obj.rdb_ack_detected >0 then
		alarms_obj.rdb_ack_detected=0;
		rdb_acknowledge_changed(alarms_obj.rdb_ack_key, alarms_obj.rdb_ack_value);
	end

	if g_alarm_condition_changed ==1 then
		g_alarm_condition_changed=0;

	end
--]]

	for name, _ in pairs(g_rules) do
		callRuleMethod(name, 'eval_event')
	end

	current_time = os.time();
	if (current_time - conf.start_time ) > conf.DELETE_ALARMS_EVENT_TIMEOUT then
		conf.start_time = current_time;
		cleanup_alarms(current_time);
	end


end



