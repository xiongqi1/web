#!/usr/bin/env lua
require('luardb')
require('luasyslog')
require('stringutil')
require('tableutil')
require('rdbobject')



-- setup syslog facility
luasyslog.open('statsmonitord', 'LOG_DAEMON')

-- our config file is shared with the TR-069 hostbridge
conf = dofile('/usr/lib/tr-069/config.lua')
--conf = dofile('config.lua')
local debug = conf.wntd.debug;

--debug = 4

local statsmonitorClassConfig = { persist = true}


local rdb_samples_count =conf.stats.statsmonitoringSamplePrefix..".samplescount";
local rdb_statsfilelock =conf.stats.statsmonitoringSamplePrefix..".statsfilelock";

--local rdb_sample_time =conf.stats.statsmonitoringSamplePrefix..".sampletime";
local conf_prefix =conf.stats.statsMonitoringConfigurationPrefix;

local radio_prefix = 'wwan.0.'

------------------------------------------------
debug=4

local function dinfo( ...)
	if debug >1 then
		local  printResult="";
		for i = 1, select('#', ...) do
				local v = select(i, ...)
				printResult = printResult .. tostring(v) .. "\t"
		 end
		if debug ==8 then
			print(printResult);
		else
			luasyslog.log('LOG_INFO', printResult)
		end
	end
end

local function dump_table(t, level)

	if not level then
		level =0
		print('+('..#t..')');
	end
	for name, v in pairs(t) do
		name =string.rep('-', level).." "..name;
		if type(v) =='table' then
			print(name, '+('..#v..')');
			dump_table(v, level+1);
		else
			print(name, v);
		end
	end
end
local unid_stats_name=
{
	["bytessent"] = "swStats.TxBytes",
	["bytesreceived"] = "swStats.RxBytes",
}

local avc_stats_name=
{
	["bytessent"] = "greStats.txbytes",
	["bytesreceived"] = "greStats.rxbytes",
	["framessent"] = "greStats.txpkt",
	["framesreceived"] = "greStats.rxpkt",
}

local radio_stats_name=
{
	['rsrp0'] 	='signal.0.rsrp',
	['rsrp1'] 	='signal.1.rsrp',
	['cinr0'] 	='signal.0.cinr',
	['cinr1'] 	='signal.1.cinr',
	['txpower'] ='signal.tx_power_PUSCH',

}

local system_stats_name=
{
	['cpuaverage']	= 'cpu.0.average';
	['cpumax'] 	= 'cpu.0.max';
	['cpumin'] 	= 'cpu.0.min';
	['memoryaverage'] = 'memory.0.average';
	['memorymax'] = 'memory.0.max';
	['memorymin'] = 'memory.0.min';
}


----------------------------------------------------

local avcObjectClass = rdbobject.getClass(conf.wntd.avcPrefix,statsmonitorClassConfig)

local avcStatsObjectClass = rdbobject.getClass(conf.stats.statsmonitoringAvcPrefix,statsmonitorClassConfig)

local unidStatsObjectClass = rdbobject.getClass(conf.stats.statsmonitoringUnidPrefix,statsmonitorClassConfig)

-- Stats data
local sample_time="";
local avc_tab = {}
local unids_tab = {}
local radio_tab ={}
local system_stats_tab={}
local prev_cpu_idle_tick;
local prev_cpu_total_tick;
local prev_cpu_idle_tick_snapshot;
local prev_cpu_total_tick_snapshot;
local prev_free_memory;
local prev_free_memory_shapshot;
local cpu_peak_utilization =0;
local cpu_min_utilization;
local memory_peak_utilization =0;
local memory_min_utilization;

-- samples contol status
local sample_enable 	=false;
local report_samples 	=96;
local sample_interval 	=10;
local samples_count 	=0;
local collect_time = 0;
local current_time;
local snapshot_interval =5;
local time_ref = os.time{year=2011, month=1, day=1, hour=0};
local interval=sample_interval;

local function makeAvcStats(id, initval)
	--print("makeAvcStats ", id);
	id = id.."";
	local instance = avcStatsObjectClass:new(id);
	if instance then
		--local mt = getmetatable(instance);
		--print("avc instance " , table.tostring(instance));
--[[
		for name, _ in pairs(avc_stats_name) do
			luardb.set(mt.prefix..name, initval);
		end
--]]
		avc_tab[id]={};
		for name, _ in pairs(avc_stats_name) do
			avc_tab[id][name] = initval;
		end
	end

end


local function deleteAvcStats(id)
	--print("deleteAvcStats", id);
	id = id.."";
	local instance = pcall(getById, avcStatsObjectClass, id)
	if instance then
		--local mt = getmetatable(instance);
		--print("avc instance" , table.tostring(instance));
--[[
		for name, _ in pairs(avc_stats_name) do
			luardb.unset(mt.prefix..name);
		end
--]]
		avc_tab[id] =nil;

		avcStatsObjectClass:delete(instance)
	end
end


local function restart()
	sample_time = ""
	for id, obj in pairs(unids_tab) do

		for name, val in pairs(obj) do
			obj[name] = "";
		end
	end

	for id, obj in pairs(avc_tab) do
		for name, val in pairs (obj) do
			obj[name] = "";
		end
	end

	for name, _ in pairs(radio_stats_name) do
		radio_tab[name] ='';
	end

	for name, _ in pairs(system_stats_name) do
		system_stats_tab[name] ='';
	end


	samples_count =0;
end


local function update_stats()

	local file= io.open(conf.stats.statsmonitoringfile, "wt");
	if file then
		--print(table.tostring(unids_tab));
		luardb.set (rdb_statsfilelock,"1");
		file:write(sample_time.."\n");
		for id, obj in pairs(avc_tab) do
			for name, val in pairs (obj) do
				file:write("avc."..id.."."..name..":"..val.."\n");
			end
		end
		for id, obj in pairs(unids_tab) do
			for name, val in pairs (obj) do
				file:write("unid."..id.."."..name..":"..val.."\n");
			end
		end

		for name, val in pairs(radio_tab) do
			file:write("radio.0."..name..":"..val.."\n");
		end

		for name, val in pairs(system_stats_tab) do
			file:write( system_stats_name[name]..":"..val.."\n");
		end

		file:close();

		luardb.set (rdb_statsfilelock,"");

	end --	if file then
	luardb.set(rdb_samples_count, samples_count);

end

local function load_stats()
	samples_count = tonumber(luardb.get(rdb_samples_count)) or 0

	local file= io.open(conf.stats.statsmonitoringfile, "rt");
	sample_time= nil;
	local need_reset =false;
	local timer_count =0;
	if file then

		for lines in file:lines() do

			--file:write(sample_time.."\n");
			-- trim space
			-- get key,id, name and value{}
			-- check avc_tab, unids_tab and radio, save the value{}

			lines = lines:trim();
			if lines:len()  ==0 then
				if not sample_time then
					sample_time =''
					timer_count=0
				end
			elseif lines:len() > 0 then
				--print(lines)
				if not sample_time then
					-- count ,
					for comma in lines:gmatch(",") do
						timer_count = timer_count + 1;
					end

					sample_time = lines;
				else

					local key, id, name, value = lines:match("(%w+)%.(%d%d*)%.([%w%d]+):(.*)");
					--print(key, id, name, value)
					if key then
						local count =0;
						for comma in value:gmatch(",") do
							count = count + 1;
						end
						if timer_count ~= count then need_reset=true; end

						if key == 'avc' then
							if avc_tab[id] and avc_tab[id][name] then
								avc_tab[id][name] =value;
							end
						elseif key =='unid' then
							if unids_tab[id] and unids_tab[id][name] then
								unids_tab[id][name] = value;
							end
						elseif key == 'radio' then
							if tonumber(id) == 0 and radio_tab[name] then
								radio_tab[name] = value;
							end
						elseif key == 'memory' or key == 'cpu' then
							local internal_name=key..name;
							if tonumber(id) == 0 and system_stats_tab[internal_name] then
								system_stats_tab[internal_name] = value;
							end

						end
					end --if key then
				end--if not sample_time then
			end --if lines:length() > 0
		end--for lines in file.lines() do

		file:close();

	end --	if file then
	if not sample_time then
		sample_time=""
	end

	if need_reset or timer_count > report_samples  then
		restart();
		update_stats();
	else
		if timer_count ~=samples_count then
			dinfo("samples count is different:(".. samples_count..','.. timer_count.."), fix it")
			samples_count = timer_count;
		end

		luardb.set(rdb_samples_count, samples_count)
	end


end


-----------------------------------------------------
-- initialisation
-- get all existing mpls circuits, delete them
-- init the pool of free tagged vlan ids
-- gather all rdb avc info into table, keys are the 'i' of avc.{i}


--[[
local function avcChangedCB(rdb, val)
	-- wait changed finished
	if not val or val=="" then

		local ids = avcObjectClass:getIds()
		for id, avc in pairs(avc_tab) do
			if not ids[id] then
				deleteAvcStats(id);
			end
		end
		local initval="";
		for i =1, samples_count  do
			initval= initval..",";
		end
		for _, id in ipairs(ids) do
			if not avc_tab[id] and id ~= 0 then
				makeAvcStats(id, initval);
			end
		end
	end

end
--]]

local function avcChangedCB(rdb, val)
	if sample_enable == false  then return  end;
	-- wait changed finished
	if not val or val=="" then
		--print("avcChangedCB");
		local ids = avcObjectClass:getIds()
		local ids_stats = avcStatsObjectClass:getIds();
		--print("avc ids " , table.tostring(ids));
		--print( "stats avc ids :", table.tostring(ids_stats));

		for _, id in pairs(ids_stats) do
			if not table.contains(ids, id) then
				deleteAvcStats(id);
			end
		end
		local initval="";
		for i =1, samples_count  do
			initval= initval..",";
		end
		for _, id in ipairs(ids) do
			if id ~= 0 and not table.contains(ids_stats, id) then
				makeAvcStats(id, initval);
			end
		end
	end

end
--
local function initAvc()
	-- sync rdbobject and memory object
	local ids_stats = avcStatsObjectClass:getIds();
	for _, id in pairs(ids_stats) do
		avc_tab[id]={};
		for name, _ in pairs(avc_stats_name) do
			avc_tab[id][name] = "";
		end
	end
	avcChangedCB()
end

local function deleteAllAvc()
	local ids = avcStatsObjectClass:getIds();
	for _, id in pairs(ids) do
		deleteAvcStats(id);
	end
end

---------------------------------------------------------
-- unid processing

-- create instances

local function makeUnidStats(id, initval)
	--print("makeUnidStats >>> ", id);
	id = id.."";
	local instance = unidStatsObjectClass:new(id);
	if instance then
--[[
		local mt = getmetatable(instance);
		for name, _ in pairs(unid_stats_name) do
			luardb.set(mt.prefix..name, initval);
		end
--]]
		unids_tab[id] = {};
		for name, _ in pairs(unid_stats_name) do
			unids_tab[id][name]= initval;
		end
		--print("makeUnidStats <<< ", id);
	end
end

local function deleteUnidStats(id)
	--print("deleteUnidStats >>> ", id);
	id = id.."";
	local ret, instance = pcall( getById, unidStatsObjectClass,id)
	if ret and instance then
--[[
		local mt = getmetatable(instance);
		for name, _ in pairs(unid_stats_name) do
			luardb.unset(mt.prefix..name);
		end
--]]

		unids_tab[id]=nil;
		--print("deleteUnidStats <<< ", id);
		unidStatsObjectClass:delete(instance)
	end
end




local function unidChangedCB(rdb, val)

	if sample_enable == false then return  end;
	-- wait changed finished
	if not val or val=="" then
		--print("unidChangedCB");
		local prefix = conf.wntd.unidPrefix;
		local ids = unidStatsObjectClass:getIds();
		for _, id in pairs(ids) do
			local rdb_val =luardb.get(conf.wntd.unidPrefix .. "."..id..".enable");
			if not rdb_val or  rdb_val ~= "1" then
				deleteUnidStats(id);
			end
		end
		local initval="";
		for i =1, samples_count  do
			initval= initval..",";
		end
		--print(table.tostring(ids));
		for id = 1,conf.wntd.unidCount do
			if not table.contains(ids, id.."") then
				local rdb_val =luardb.get(conf.wntd.unidPrefix .. "."..id..".enable");
				if  rdb_val == "1" then
					makeUnidStats(id, initval);
				end

			end
		end

	end
end

local function initUnid()
	-- syn the rdbobject and memory object
	local ids = unidStatsObjectClass:getIds();
	for _, id in pairs(ids) do
		unids_tab[id] = {};
		for name, _ in pairs(unid_stats_name) do
			unids_tab[id][name]= "";
		end
	end
	unidChangedCB();
end

local function deleteAllUnid()
	local ids = unidStatsObjectClass:getIds();
	--print(table.tostring(ids));
	for _, id in pairs(ids) do
		deleteUnidStats(id);
	end

end

local function initRadio()
	for name, _ in pairs(radio_stats_name) do
		radio_tab[name] ='';
	end
end

local function deleteRadio()
	for name, _ in pairs(radio_stats_name) do
		radio_tab[name] =nil;
	end
end




local function defaultnumber(n, v)
  return tonumber(n) or v
end

local function defaultnumber_nozero(n, v)
	n = tonumber(n)
	if n  and  n ~=0 then
		return n
	else
		return v;
	end
end


--[[  Data fields

    * 0 user: normal processes executing in user mode
    * 1 nice: niced processes executing in user mode
    * 2 system: processes executing in kernel mode
    * 3 idle: waiting
    * 4 iowait: waiting for I/O to complete
    * 5 irq: handling interrupts
    * 6 softirq: servicing softirqs
    * 7 always zero?

  --]]

local function load_cpuinfo()
	local total_tick =0;
	local idle_tick =0
	local file = io.open("/proc/stat");
	if file then
			local line1 = file:read('*l');
			if line1  then
			   local list1 = {}
				 for v in string.gmatch(line1, "([%d%w]+)") do
				   table.insert(list1, v)
				 end
				if #list1 >4 and list1[1] == 'cpu' then
					for i =2, #list1 do
						--print(i, list1[i])
						total_tick  = total_tick + tonumber(list1[i])
					end
					idle_tick = tonumber(list1[5])
				end
			end
		file:close();
	end
	return idle_tick, total_tick;
end

local function load_meminfo()
	local total_mem=0;
	local free_mem =0
	local file = io.open("/proc/meminfo");
	if file then
			local line1 = file:read('*l');
			local line2 = file:read('*l');
			if line1  and line2 then
				total_mem = line1:match('MemTotal:%s*(%d+)');
				free_mem = line2:match('MemFree:%s*(%d+)');
			end
		file:close();
	end
	return free_mem, total_mem;
end

local function initSystem()
	for name, _ in pairs(system_stats_name) do
		system_stats_tab[name] ='';
	end
	cpu_peak_utilization =0;
	cpu_min_utilization =nil;
	memory_peak_utilization =0;
	memory_min_utilization =nil;
	prev_cpu_idle_tick, prev_cpu_total_tick = load_cpuinfo();
	prev_cpu_idle_tick_snapshot =prev_cpu_idle_tick;
	prev_cpu_total_tick_snapshot = prev_cpu_total_tick
	prev_free_memory = load_meminfo();
	prev_free_memory_snapshot = prev_free_memory


end

local function deleteSystem()
	for name, _ in pairs(system_stats_name) do
		system_stats_tab[name] =nil;
	end
end

local function system_snapshot(idle_tick, total_tick, free_mem, total_mem)

	local newval =0;

	if not idle_tick then
		idle_tick, total_tick = load_cpuinfo();
	end

	if total_tick ~= prev_cpu_total_tick_snapshot then
		newval = (idle_tick - prev_cpu_idle_tick_snapshot)/(total_tick - prev_cpu_total_tick_snapshot)*100;
		newval = 100.0-newval;

		if newval > cpu_peak_utilization then
			cpu_peak_utilization = newval
		end
		if not cpu_min_utilization or newval < cpu_min_utilization then
			cpu_min_utilization = newval
		end
		prev_cpu_idle_tick_snapshot = idle_tick;
		prev_cpu_total_tick_snapshot = total_tick;
	end

	if not free_mem then
		free_mem, total_mem = load_meminfo();
	end
	if total_mem ~= 0 then
		newval = (free_mem + prev_free_memory_snapshot)/2/total_mem*100;
		newval = 100.0-newval;
		if newval > memory_peak_utilization then
			memory_peak_utilization = newval;
		end
		if not memory_min_utilization or newval < memory_min_utilization then
			memory_min_utilization = newval;
		end

		prev_free_memory_snapshot = free_mem;
	end
end

--- collect sample data from unid and avc

local function collect(current_time)

	--print('collect()', os.date("%Y-%m-%dT%XZ",current_time));
	sample_time = sample_time .. os.date("%Y-%m-%dT%XZ",current_time)..",";

	for id, obj in pairs(unids_tab) do
		local prefix =conf.wntd.unidPrefix .. "." ..id..".";
		for name, val in pairs (obj) do
			local newval = luardb.get(prefix.. unid_stats_name[name]);
			if not newval then newval ="" end;
			obj[name] = val ..newval..",";
		end
	end

	for id, obj in pairs(avc_tab) do
		local prefix =conf.wntd.avcPrefix .. "." ..id..".";
		for name, val in pairs (obj) do
			local newval = luardb.get(prefix .. avc_stats_name[name]);
			if not newval then newval ="" end;
			obj[name] = val ..newval..",";
		end
	end

	for name, val in pairs (radio_tab) do
		local newval = luardb.get(radio_prefix.. radio_stats_name[name]);
		--print(name, newval, radio_stats_name[name]);
		newval = newval or "";
		radio_tab[name] = val ..newval..",";
	end


	local idle_tick, total_tick = load_cpuinfo();
	local newval =0;

	if total_tick ~= prev_cpu_total_tick then
		newval = (idle_tick - prev_cpu_idle_tick)/(total_tick - prev_cpu_total_tick)*100;
	end

	newval = string.format('%.1f', 100.0-newval);
	--print("cpu utility", idle_tick, total_tick, newval);

	system_stats_tab['cpuaverage'] = system_stats_tab['cpuaverage'] ..newval..",";
	prev_cpu_idle_tick = idle_tick;
	prev_cpu_total_tick = total_tick;

	local free_mem, total_mem = load_meminfo();

	newval =0;
	if total_mem ~= 0 then
		newval = (free_mem + prev_free_memory)/2/total_mem*100;
	end

	newval = string.format('%.1f', 100.0-newval);
	--print("memory utility", free_mem, total_mem, newval)

	system_stats_tab['memoryaverage'] = system_stats_tab['memoryaverage'] ..newval..",";
	prev_free_memory = free_mem;

	system_snapshot(idle_tick, total_tick, free_mem, total_mem);

	newval = string.format('%.1f', cpu_peak_utilization);
	system_stats_tab['cpumax'] = system_stats_tab['cpumax'] ..newval..",";

	newval = string.format('%.1f', cpu_min_utilization or 0);
	system_stats_tab['cpumin'] = system_stats_tab['cpumin'] ..newval..",";

	newval = string.format('%.1f', memory_peak_utilization);
	system_stats_tab['memorymax'] = system_stats_tab['memorymax'] ..newval..",";

	newval = string.format('%.1f', memory_min_utilization);
	system_stats_tab['memorymin'] = system_stats_tab['memorymin'] ..newval..",";

	cpu_peak_utilization =0;
	cpu_min_utilization =nil;
	memory_peak_utilization =0;
	memory_min_utilization =nil;

	--dump_table(unids_tab);
	--dump_table(avc_tab);
	--dump_table(radio_tab);
end


-- remove one of comma field from front
local function remove_front()
	local t;
	t = sample_time:match("[%d%w%.]*,(.*)");
	sample_time = t or ''
	--print('remove_front' , sample_time)
	for id, obj in pairs(unids_tab) do
		for name, val in pairs (obj) do
			t= obj[name]
			t = t:match("[%d%w%.]*,(.*)");
			obj[name]=t or ''
		end
	end

	for id, obj in pairs(avc_tab) do
		for name, val in pairs (obj) do
			t= obj[name]
			t = t:match("[%d%w%.]*,(.*)");
			obj[name]=t or ''
		end
	end

	for name, val in pairs (radio_tab) do
		t= radio_tab[name]
		t = t:match("[%d%w%.]*,(.*)");
		radio_tab[name] = t or ''
	end

	for name, val in pairs (system_stats_tab) do
		t= system_stats_tab[name]
		t = t:match("[%d%w%.]*,(.*)");
		system_stats_tab[name] = t or ''
	end

end



local function computeFirstInterval(time_ref, current_time, sample_interval)
	if time_ref == 0 then
		return sample_interval;
	else
		local t = current_time - time_ref;
		if t > 0 then
			return sample_interval - t %sample_interval;
		else
			return sample_interval + t %sample_interval;
		end
	end

end

local function get_ref_time()
	local t = luardb.get(conf_prefix..".timereference");
	t = tonumber(t)

if not time_ref or time_ref == 0 then
	time_ref= os.time{year=2011, month=1, day=1, hour=0};
end

end

local function enable_sample()
	dinfo("enable statsmonitoring");
	report_samples = luardb.get(conf_prefix..".reportsamples");
	sample_interval = luardb.get(conf_prefix..".sampleinterval");
	report_samples = defaultnumber(report_samples, 96);
	sample_interval = defaultnumber_nozero(sample_interval,900);
	local t = luardb.get(conf_prefix..".timereference");

	time_ref= defaultnumber_nozero(t, time_ref);

	current_time = os.time();

	interval = computeFirstInterval(time_ref, current_time, sample_interval)
	collect_time = current_time + interval;
	--print("first interval=".. interval, "sample_interval=".. sample_interval);
	sample_enable = true;
	samples_count =0;
	initUnid()
	initAvc()
	initRadio()
	initSystem()

end

local function disable_sample()
	dinfo("disable statsmonitoring");
	sample_enable = false;
	samples_count ="";
	sample_time = ""
	sample_interval = 36000; --1 hours
	deleteAllAvc();
	deleteAllUnid();
	deleteRadio();
	deleteSystem()
end

local function enableChangedCB(rdb, val)
	val = tonumber(val) == 1 or false;
	--print(rdb, val);
	if val and not sample_enable then
		enable_sample();
		load_stats();

	elseif not val then
		if sample_enable then
			disable_sample()
		end
		update_stats();
	end

end




----------------------------------------------------------
-- whole daemon processing
local cmd = arg[1];
if cmd then
	if cmd == '-v8' then
		debug =8;
	end
end

-- if local time is < 2011-01-01 00:00:00 then the clock is not synchronized by NTP
-- wait
current_time = os.time();
local t = luardb.get(conf_prefix..".timereference");

time_ref= defaultnumber_nozero(t, time_ref);

--print(time_ref);

while current_time < time_ref do

	luardb.wait(10);
	current_time = os.time();
end

--
-- we watch the "<avcPrefix>.changed" variable
-- the TR-069 client will put a comma seperated list
-- of AVC IDs which have been changed into this variable
luardb.watch(conf.wntd.avcPrefix .. '.changed', avcChangedCB)

-- we watch the "<unidPrefix>.changed" variable
-- the TR-069 client will put a comma seperated list
-- of UNI-D IDs which have been changed into this variable
luardb.watch(conf.wntd.unidPrefix .. '.changed', unidChangedCB)

luardb.watch(conf_prefix..'.sampleenable', enableChangedCB);

enableChangedCB(nil, luardb.get(conf_prefix..'.sampleenable'));


while true do

	if interval >  snapshot_interval  then
		interval = snapshot_interval;
	end

	luardb.wait(interval);
	current_time = os.time();

	interval = collect_time - current_time;
	if interval <=0 then

		if sample_enable  then
			if samples_count >= report_samples then
				--print("restart", samples_count);
				--samples_count =0;
				--restart();
				samples_count = samples_count-1
				remove_front();
			end
			collect(current_time);
			samples_count = samples_count+1

			update_stats();
			interval = computeFirstInterval(time_ref, current_time, sample_interval)
		else
			interval = sample_interval;

		end --if sample_enable then

		collect_time = current_time + interval;

	else
		-- do snapshot on system resource
		system_snapshot()
	end --if interval < sample_interval then

end
