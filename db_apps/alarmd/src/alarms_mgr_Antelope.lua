#!/usr/bin/lua

require('luardb')
require('luasyslog')
require('rdbobject')
require('rdbrpcclient')

conf = {
	alarmClass = 'alarms',
	alarmEndpoint = 'alarmd',
	--avcPrefix = 'avc',

	POLL_TIMEOUT =20,

	-- message source type
	MESSGAGE_TYPE_BOOL="bool",
	MESSGAGE_TYPE_MSG2="msg2",
	MESSGAGE_TYPE_MSGRDB="msgrdb",
	MESSGAGE_TYPE_RDB2="rdb2",
	MESSGAGE_TYPE_DYNAMIC="dynamic",

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
	alarms.1.message ="outdoor has been rebooted"



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


local g_alarm_source=
{
	{
		type=conf.MESSGAGE_TYPE_BOOL,
		subsys="Reset",
		rdb="indoor.comms_failed",
		messages="Indoor-outdoor communication failure",
		acknownledgable=0,
		active = 0,
		id =0,
	},
	{
		type=conf.MESSGAGE_TYPE_BOOL,
		subsys="Power",
		rdb="psu.on_battery",
		messages="Battery is on",
		acknownledgable=0,
		active = 0,
		id =0,
	},
	{
		type=conf.MESSGAGE_TYPE_BOOL,
		subsys="Power",
		rdb="psu.low_battery",
		messages="Battery is low",
		acknownledgable=0,
		active = 0,
		id =0,
	},
	{
		type=conf.MESSGAGE_TYPE_BOOL,
		subsys="Power",
		rdb="psu.replace_battery",
		messages="Battery is replaced",
		acknownledgable=0,
		active = 0,
		id =0,
	},
	{
		type=conf.MESSGAGE_TYPE_BOOL,
		subsys="Power",
		rdb="psu.missing_battery",
		messages="Battery is missing",
		acknownledgable=0,
		active = 0,
		id =0,
	},
	{
		type=conf.MESSGAGE_TYPE_MSGRDB,
		subsys="Reset",
		rdb="system.outdoor_reboot_state",
		messages="Outdoor reboot due to ", -- message root, other part from rdb
		acknownledgable=1,
	},
	{
		type=conf.MESSGAGE_TYPE_MSG2,
		subsys="Reset",
		rdb="system.indoor_reboot_state",
		messages={"Indoor unit reboot due to Power Failure",
					"Indoor unit reboot due to Watchdog Reset"},
		bitmap={0x1, 0x2},
		acknownledgable=1,
		active =0,
	},

	{
		type=conf.MESSGAGE_TYPE_RDB2,
		subsys="Radio",
		rdb="wwan.status",
		rdb2 ="wwan.lasterr",
		show =1,
		keywords={"error","fatal", "disconnecting", "reset", "ms-wait", "waiting"},
		keyword_msg={},
		prefix="Radio: ",
		acknownledgable=0,
	},
	{
		type=conf.MESSGAGE_TYPE_RDB2,
		subsys="UNID",
		rdb="unid.1.status",
		rdb2="unid.1.lasterr",
		keywords={"Error"},
		keyword_msg={"NoLink", "Disable"},
		prefix="UNID.1: ",
		show =0,
		acknownledgable=0,
	},

	{
		type=conf.MESSGAGE_TYPE_RDB2,
		subsys="UNID",
		rdb="unid.2.status",
		rdb2="unid.2.lasterr",
		show =0,
		keywords={"Error"},
		keyword_msg={"NoLink", "Disable"},
		prefix="UNID.2: ",
		acknownledgable=0,
	},
	{
		type=conf.MESSGAGE_TYPE_RDB2,
		subsys="UNID",
		rdb="unid.3.status",
		rdb2="unid.3.lasterr",
		show =0,
		keywords={"Error"},
		keyword_msg={"NoLink", "Disable"},
		prefix="UNID.3: ",
		acknownledgable=0,
	},
	{
		type=conf.MESSGAGE_TYPE_RDB2,
		subsys="UNID",
		rdb="unid.4.status",
		rdb2="unid.4.lasterr",
		show =0,
		keywords={"Error"},
		keyword_msg={"NoLink", "Disable"},
		prefix="UNID.4: ",
		acknownledgable=0,
	},

--[[
	{
		type=conf.MESSGAGE_TYPE_DYNAMIC,
		subsys="UNID",
		prefix="unid.",
		status=".status",
		lasterr =".lasterr",
		enable=".enable",

		rdb="unid.changed",
		active_list={},

		_active_list ={1, 2, 3, 4,},

		keywords={"Error","Nolink", "Disable"},

		acknownledgable=0,
	},
	{
		type=conf.MESSGAGE_TYPE_DYNAMIC,
		subsys="AVC",
		prefix="avc.",
		status=".status",
		lasterr =".lasterr",
		enable=".enable",

		rdb="avc.changed",
		active_list={},
		rdbobj= avc_object,

		keywords={"Error", "Disable"},

		acknownledgable=0,
	},
--]]


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



local function initialize()


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

	current_time = os.time();
	if (current_time - conf.start_time ) > conf.DELETE_ALARMS_EVENT_TIMEOUT then
		conf.start_time = current_time;
		cleanup_alarms(current_time);
	end


end



