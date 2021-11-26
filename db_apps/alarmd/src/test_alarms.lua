#!/usr/bin/lua

require('cdcsutil')
require ('luardb')
require('rdbobject')
require('rdbrpcclient')

local WAIT=0

conf = {
	alarmClass = 'alarms',
	alarmEndpoint = 'alarmd',

}
local alarm_count =0;
rpc = rdbrpcclient:new(conf.alarmEndpoint)

local start_timer = os.time();

-- bitshift functions (<<, >> equivalent)
-- shift left
function lsh(value,shift)
	return (value*(2^shift))
end

function lsh2(value, shift1, shift2)
	return value*(2^shift1) +value*(2^shift2)
end

-- shift right
function rsh(value,shift)
	return value/2^shift
end

-- return single bit (for OR)
function bit(x,b)
	return (math.mod(x, 2^b) - math.mod(x, 2^(b-1)) > 0)
end

function list_alarms()
	local number =0;
	local alarms = rdbobject.getClass(conf.alarmClass)
	for i, alarm in ipairs(alarms:getAll()) do
		if alarms:getId(alarm) then
			print(alarms:getId(alarm), tostring(alarm.raised) .. ':' .. tostring(alarm.cleared), alarm.subsys, alarm.message)
			number = number +1;
		end
	end
	print( "list_alarms: ".. number);

end

ALARMS_BOOL_STATE={"indoor.comms_failed",
					"psu.on_battery",
					"psu.low_battery",
					"psu.replace_battery",
					"psu.missing_battery",
					};

ALARMS_MSGRDB_STATE	={"system.outdoor_reboot_state"};

ALARMS_MSG_STATE={	"system.indoor_reboot_state"};

ALARMS_RDB2_STATE={  {"radio.status","radio.lasterr"},

					};

rdb_unid_changed="unid.changed";
--avc_change="avc.changed";

UNID_RDB_STATE={
			{1, "unid.1.status","unid.1.lasterr"},
			{2, "unid.2.status","unid.2.lasterr"},
			{3, "unid.3.status","unid.3.lasterr"},
			{4, "unid.4.status","unid.4.lasterr"},
			--{1, "avc.1.status","avc.1.lasterr", "avc.1.enable",avc_change},
			--{2, "avc.2.status","avc.2.lasterr", "avc.2.enable",avc_change},
			--{31, "avc.31.status","avc.31.lasterr", "avc.31.enable",avc_change},
			--{32, "avc.32.status","avc.32.lasterr", "avc.32.enable",avc_change},

};


function rdb_changed(rdb, val)
	--print( "set acknownledge to " .. val);
	if string.len(val) > 0 then
		luardb.set("alarms.acknownledge", val);
		--os.execute("rdb dump alarms|sort");
		list_alarms();
		alarm_count = alarm_count+1;
	end
end

luardb.watch("alarms._index", rdb_changed);

for count =0, 2 do
---[[
print("Start Test BOOL state")

for name, rdb in pairs(ALARMS_BOOL_STATE) do

	print("set ".. rdb .. " on");

	luardb.set(rdb, 1);
	os.execute("sleep "..WAIT);
	print("set ".. rdb .. " off");

	luardb.set(rdb, 0);
	os.execute("sleep "..WAIT);

end
--]]

---[[
print("Start Test MSG2 state")

for name, rdb in pairs(ALARMS_MSG_STATE) do

	print("set ".. rdb .. " 1");

	luardb.set(rdb, 1);
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " 2");
	luardb.set(rdb, 2);
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " 3");
	luardb.set(rdb, 3);
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " 0");
	luardb.set(rdb, 0);
	os.execute("sleep "..WAIT);

end
--]]
---[[
print("Start Test MSGRDB state")

for name, rdb in pairs(ALARMS_MSGRDB_STATE) do

	print("set ".. rdb .. "");

	luardb.set(rdb, "");
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " power failuer");
	luardb.set(rdb, "power failure");
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " System Update");
	luardb.set(rdb, "System Upgrade");
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. "");
	luardb.set(rdb, "");


end
--]]
---[[

print("Start Test RDB2 state")

for name, obj in pairs(ALARMS_RDB2_STATE) do

	rdb = obj[1];
	rdb2= obj[2];

	print("set ".. rdb .. " normal");
	luardb.set(rdb, "normal");
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " Disable");
	luardb.set(rdb, "Disable");
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " NoLink");
	luardb.set(rdb, "NoLink");
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " Error");
	luardb.set(rdb2, "test Error for " .. rdb);
	luardb.set(rdb, "Error");
	os.execute("sleep "..WAIT);

	print("set ".. rdb .. " normal");
	luardb.set(rdb, "normal");
	os.execute("sleep "..WAIT);

end
--]]

---[[
print("Start Test UNID state")

for test=-1, 0  do


	for name, obj in pairs(UNID_RDB_STATE) do

		local id = obj[1];
		local rdb = obj[2];
		local rdb2= obj[3];

		if test ==0 then
			luardb.unset(rdb);
			luardb.set(rdb_unid_changed, "");
			print("remove  ".. rdb );
		else
			luardb.set(rdb, "");
			luardb.set(rdb_unid_changed, "");
			print("create  ".. rdb );
		end
		os.execute("sleep "..WAIT);


		luardb.set(rdb, "normal");

		os.execute("sleep "..WAIT);

		print("set ".. rdb .. " Disable");
		luardb.set(rdb, "Disable");
		os.execute("sleep "..WAIT);

		print("set ".. rdb .. " NoLink");
		luardb.set(rdb, "NoLink");
		os.execute("sleep "..WAIT);

		print("set ".. rdb .. " Error");
		luardb.set(rdb2, "test Error for " .. rdb);
		luardb.set(rdb, "Error");
		os.execute("sleep "..WAIT);

		print("set ".. rdb .. " normal");
		luardb.set(rdb, "normal");
		os.execute("sleep "..WAIT);

	end



end

--]]


print("Total Alarms " .. alarm_count);
print("Expected Alarms 40");

end

