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

local debug = conf.wntd.debug;

local avcClassConfig = { persist = false} -- avoid to cleanup invalid AVCs
local statsmonitorClassConfig = { persist = true}

local rdb_samples_count =conf.stats.statsmonitoringSamplePrefix..".samplescount";
local rdb_statsfilelock =conf.stats.statsmonitoringSamplePrefix..".statsfilelock";

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
	["bytessent"] = "txbytes",
	["bytesreceived"] = "rxbytes",
	["framessent"] = "txpkt",
	["framesreceived"] = "rxpkt",
}

local radio_stats_name=
{
	['rsrp0'] = 'servcell_info.rsrp.0',
	['rsrp1'] = 'servcell_info.rsrp.1',
	-- cinr0 and cinr1 were originally included for sequans.
	-- qualcomm does not support cinr at all but has recommended
	-- using rssinr. qualcomm also only provides combined rssinr so
	-- set both 0 and 1 to the same value for backward compatability.
	['cinr0'] = 'signal.rssinr',
	['cinr1'] = 'signal.rssinr',
	['txpower'] = 'signal.tx_power_PUSCH',
	['servingcellid'] = 'radio_stack.e_utra_measurement_report.servphyscellid',
	['servingcellearfcn'] = 'radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq',

}

local system_stats_name=
{
	['cpuaverage']	= 'cpu.0.average',
	['cpumax'] 	= 'cpu.0.max',
	['cpumin'] 	= 'cpu.0.min',
	['memoryaverage'] = 'memory.0.average',
	['memorymax'] = 'memory.0.max',
	['memorymin'] = 'memory.0.min',
}



-- stats data item position in file /pro/net/dev
local proc_net_dev_cfg = {
	['rxbytes'] = 2,
	['rxpkt'] = 3,
	['txbytes'] = 10,
	['txpkt'] =11,
}
local throughput_threshold = {
	['rxbkt'] = {},
	['txbkt'] = {},
}
local throughput_bucket_name = {
	['rxbkt'] = 'rxbytes',
	['txbkt'] = 'txbytes',
}
----------------------------------------------------

local avcObjectClass = rdbobject.getClass(conf.wntd.avcPrefix,avcClassConfig)

local avcStatsObjectClass = rdbobject.getClass(conf.stats.statsmonitoringAvcPrefix,statsmonitorClassConfig)

local unidStatsObjectClass = rdbobject.getClass(conf.stats.statsmonitoringUnidPrefix,avcClassConfig)

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
local prev_free_memory_snapshot;
local cpu_peak_utilization =0;
local cpu_min_utilization;
local memory_peak_utilization =0;
local memory_min_utilization;

-- samples contol status
local sample_enable	= false;
local report_samples = 192;
local sample_interval= 900;
local samples_count = 0;
local collect_time = 0;
local throughput_bucket_max = 9
--- throughput related parameters
local throughput_enable ={
	['rxbkt'] = false, -- throughput downlink sampling
	['txbkt'] = false, -- throughput uplink sampling
}
local throughput_sample_interval = 10 -- throughput sampling interval
local throughput_sample_collected = false -- throughput data get collected
local throughput_collect_time = 0	-- throughput data collection time

-- gre{id} stats and its throughput(has been sorted into bucket)
-- each gre contains value 'dev' and object 'stats', 'old_stats', 'rxbytes','txbytes'
-- eg: {[1]={['dev']='gre1',['stats'] ={['rxbytes']=,...}, ['rxbytes']={}, ['txbytes']={}}}
local avc_dev_stats = {}
local snapshot_interval =5;
local time_ref = os.time{year=2011, month=1, day=1, hour=0};

local function makeAvcStats(id, initval, create_id)
	local instance = {}
	if create_id then
		instance = avcStatsObjectClass:new(id);
	end
	if instance then
		avc_tab[id]={};
		for name, _ in pairs(avc_stats_name) do
			avc_tab[id][name] = initval;
		end
		-- throughput bucket name rxbkt1, rxbkt2 .. txbkt1,txbkt2
		for xbkt, threshold in pairs(throughput_threshold) do
			for i=1, #threshold  do
				avc_tab[id][xbkt .. i] = initval;
			end
		end
	end

end


local function deleteAvcStats(id)
	local ret, instance = pcall(avcStatsObjectClass.getById, avcStatsObjectClass, id)
	if ret and instance then
		avc_tab[id] =nil;
		avc_dev_stats[id] = nil
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

	for name, _ in pairs(radio_tab) do
		radio_tab[name] ='';
	end

	for name, _ in pairs(system_stats_tab) do
		system_stats_tab[name] ='';
	end
	samples_count =0;
end

-- check any item has zero-lenth value
local function has_empty_item()
	for id, obj in pairs(unids_tab) do
		for name, val in pairs(obj) do
			if val:len() == 0 then
				return true
			end
		end
	end

	for id, obj in pairs(avc_tab) do
		for name, val in pairs (obj) do
			if val:len() == 0 then
				return true
			end
		end
	end

	for name, val in pairs(radio_tab) do
		if val:len() == 0 then
			return true
		end
	end

	for name, val in pairs(system_stats_tab) do
		if val:len() == 0 then
			return true
		end
	end
	return false
end

local function update_stats()

	local file= io.open(conf.stats.statsmonitoringfile, "wt");
	if file then
		luardb.set (rdb_statsfilelock,"1");
		file:write(sample_time.."\n");
		for id, obj in pairs(avc_tab) do
			--avc stats is sorted by the item name
			local ks={}
			for name, _ in pairs(obj) do
				table.insert(ks, name)
			end
			table.sort (ks) -- sort keys
			for _, name in pairs (ks) do
				file:write("avc."..id.."."..name..":"..obj[name].."\n");
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
								avc_tab[id][name] = value;
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
			end --if lines:len() > 0
		end--for lines in file.lines() do

		file:close();

	end --	if file then
	-- if there is new RDB introduced, or statsmonitoring.out file crashed
	-- reset all the stats and cleanup statsmonitoring.out
	if timer_count > 0 and not need_reset then
		need_reset = has_empty_item()
	end

	if not sample_time then
		sample_time=""
	end
	if need_reset or timer_count > report_samples  then
		restart();
		update_stats();
	else
		samples_count = timer_count
		luardb.set(rdb_samples_count, timer_count)
	end
end


-----------------------------------------------------
-- initialisation
-- get all existing mpls circuits, delete them
-- init the pool of free tagged vlan ids
-- gather all rdb avc info into table, keys are the 'i' of avc.{i}

local function avcChangedCB(rdb, val)
	if not sample_enable then return  end;
	-- wait changed finished
	if not val or val=="" then
		-- sync avcObjectClass and avcStatsObjectClass
		local ids = avcObjectClass:getIds()
		local ids_stats = avcStatsObjectClass:getIds();
		for _, id in pairs(ids_stats) do
			if not table.contains(ids, id) then
				deleteAvcStats(id);
			end
		end
		local initval="";
		for i =1, samples_count  do
			initval= initval..",";
		end

		ids_stats = avcStatsObjectClass:getIds();
		for _, id in ipairs(ids) do
			if id ~= '0' then
				if table.contains(ids_stats, id) then
					--avc_tab[id] may be empty if it has not GRE interface
					if not avc_tab[id] then
						makeAvcStats(id, initval, false);
					end
				else
					makeAvcStats(id, initval, true);
				end
			end
		end
		--- update 'dev' for each avc_dev_stats
		for id, obj in pairs(avc_tab) do
			local trunk_vid = luardb.get(conf.wntd.avcPrefix .. "." ..id..".trunk_vid");
			if trunk_vid and #trunk_vid > 0 then
				if  not avc_dev_stats[id] then
					avc_dev_stats[id] = {}
				end
				avc_dev_stats[id]['dev'] = conf.wntd.grePrefix..trunk_vid
			else
				-- no data collection for disabeld AVC
				avc_tab[id] = nil
				avc_dev_stats[id] = nil
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
		-- throughput bucket name rxbkt1, rxbkt2 .. txbkt1,txbkt2
		for xbkt, threshold in pairs(throughput_threshold) do
			for i=1, #threshold  do
				avc_tab[id][xbkt .. i] = "";
			end
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
	local instance = unidStatsObjectClass:new(id);
	if instance then
		unids_tab[id] = {};
		for name, _ in pairs(unid_stats_name) do
			unids_tab[id][name]= initval;
		end
	end
end

local function deleteUnidStats(id)
	local ret, instance = pcall(unidStatsObjectClass.getById, unidStatsObjectClass,id)
	if ret and instance then
		unids_tab[id]=nil;
		unidStatsObjectClass:delete(instance)
	end
end


local function unidChangedCB(rdb, val)

	if not sample_enable then return  end;
	-- wait changed finished
	if not val or val=="" then
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
		for id = 1,conf.wntd.unidCount do
			if not table.contains(ids, id.."") then
				local rdb_val =luardb.get(conf.wntd.unidPrefix .. "."..id..".enable");
				if  rdb_val == "1" then
					makeUnidStats(id.."", initval);
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
	if n  and  n > 0 then
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
		if  prev_cpu_total_tick_snapshot then
			newval = (idle_tick - prev_cpu_idle_tick_snapshot)/(total_tick - prev_cpu_total_tick_snapshot)*100;
			newval = 100.0-newval;

			if newval > cpu_peak_utilization then
				cpu_peak_utilization = newval
			end
			if not cpu_min_utilization or newval < cpu_min_utilization then
				cpu_min_utilization = newval
			end
		end
		prev_cpu_idle_tick_snapshot = idle_tick;
		prev_cpu_total_tick_snapshot = total_tick;
	end

	if not free_mem then
		free_mem, total_mem = load_meminfo();
	end
	if total_mem ~= 0 then
		if prev_free_memory_snapshot then
			newval = (free_mem + prev_free_memory_snapshot)/2/total_mem*100;
			newval = 100.0-newval;
			if newval > memory_peak_utilization then
				memory_peak_utilization = newval;
			end
			if not memory_min_utilization or newval < memory_min_utilization then
				memory_min_utilization = newval;
			end
		end
		prev_free_memory_snapshot = free_mem;
	end
end

--Collect the network interface stats
--parameter(in/out) dev_stats: collect data for all contained devices
-- eg: { ['1']={['dev']='gre1',['2']={['dev']='gre21' } ..
local function readIface(dev_stats)
	local result = {}
	for line in io.lines('/proc/net/dev') do
		local fields = {}
		for field in string.gmatch(line, "%S+") do
			table.insert(fields, field)
		end
		for id, dev_stats in pairs(dev_stats) do
			if dev_stats['dev'] then
				if fields[1] == dev_stats['dev'] .. ':' then
					res = {}
					for key, pos in pairs(proc_net_dev_cfg) do
						res[key] = tonumber(fields[pos])
					end
					dev_stats['old_stats'] = dev_stats['stats']
					dev_stats['stats'] = res
				end
			end
		end
	end
end

-- calculate throughput on gre interface.
-- sort throughput into bucket
local function avc_throughput(dev_stats, period)
	for id, obj in pairs(dev_stats) do
		old_stats = obj['old_stats']
		if old_stats then
			local stats = obj['stats']
			for xbkt, threshold in pairs(throughput_threshold) do
				-- create all zero list
				if obj[xbkt] == nil then
					obj[xbkt] = {}
					for i=1, #threshold  do
						table.insert(obj[xbkt], 0)
					end
				end
				local xbytes = throughput_bucket_name[xbkt]
				-- compute throughput
				local throughput = (stats[xbytes] - old_stats[xbytes])/period
				if throughput >= 0 then -- if the grex is reset, the throughput will be ignored
					index = 1
					if throughput > 0 then
						index = #threshold
						for i, val in ipairs(threshold) do
							if throughput < val then
								index = i - 1;
								break
							end
						end
					end
					obj[xbkt][index] = obj[xbkt][index] + 1
				end
			end
		end
	end
end
-- reset throughput bucket
local function reset_bucket(dev_stats)
		for id, obj in pairs(dev_stats) do
			for xbkt, threshold in pairs(throughput_threshold) do
				obj[xbkt] = {}
				for i=1, #threshold  do
					table.insert(obj[xbkt], 0)
				end
			end
		end
end
-- load throughput bucket definition
local function load_threshold(threshold_list, default_list)
	fields = {0}

	if threshold_list then
		local _, count = string.gsub(threshold_list, "[%d%.]+", "")
        if count < 1 then
			threshold_list = default_list
		end
	else
		threshold_list = default_list
	end
    local count=0;
    for field in string.gmatch(threshold_list, "[%d%.]+") do
        count = count + 1
        if count >= throughput_bucket_max then
            break
        end
        n = tonumber(field)*1000000/8
		table.insert(fields, n)
	end
	return fields
end


-- collect all the stats items
local function collect(current_time, avc_stats)
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
	-- avc stats will read from avc_stats instead of its rdb stats
	for id, obj in pairs(avc_tab) do
		for name, val in pairs (obj) do
			local newval
			-- avc_tab update wiht avc._index. avc_stats update with gre interface
			if avc_stats[id] then
				if avc_stats[id]['stats'] then
					if avc_stats_name[name] then -- for txbytes, rxbytes, txpkt, rxpkt
						newval =  avc_stats[id]['stats'][avc_stats_name[name]]
					elseif throughput_sample_collected then
						local xbkt, index = name:match("([rt]xbkt)(%d+)")
						-- name match throughput bucket:rxbkt1, rxbkt2 .. txbkt1,txbkt2
						-- save thoughput data only when it is enabled on direction
						if xbkt and throughput_enable[xbkt] and avc_stats[id][xbkt] then
							newval = avc_stats[id][xbkt][tonumber(index)]
						end
					end
				end
			end --if avc_stats[id] then
			if not newval then newval ="" end;
			obj[name] = val ..newval..",";
		end
	end

	for name, val in pairs (radio_tab) do
		local newval = luardb.get(radio_prefix.. radio_stats_name[name]);
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
	throughput_sample_collected = false
	--dump_table(unids_tab);
	--dump_table(avc_tab);
	--dump_table(radio_tab);
end


-- remove one of comma field from front
local function remove_front()
	local t;
	t = sample_time:match("[%d%w%.]*,(.*)");
	sample_time = t or ''
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

-- Definition of TimeReference in TR-135:
-- An absolute time reference in UTC to
-- determine when sample intervals will
-- complete. Each sample interval MUST
-- complete at this reference time plus or minus
-- an integer multiple of SampleInterval.
-- ...
-- TimeReference is used only to set the "phase"
-- of the sample and fetch intervals. The actual
-- value of TimeReference can be arbitrarily far
-- into the past or future.

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

local function throughput_params_changed()
	local val = luardb.get(conf_prefix..".throughputsampleinterval")
	throughput_sample_interval = defaultnumber_nozero(val, 10)

	val = luardb.get(conf_prefix..".downlinkthroughputenable")
	throughput_enable['rxbkt'] = tonumber(val) == 1

	val = luardb.get(conf_prefix..".uplinkthroughputenable")
	throughput_enable['txbkt'] = tonumber(val) == 1

	val = luardb.get(conf_prefix..".downlinkthroughputbucket")
	throughput_threshold['rxbkt'] = load_threshold(val, '0,1.5,3,6,12,25,50,100')

	val = luardb.get(conf_prefix..".uplinkthroughputbucket")
	throughput_threshold['txbkt'] = load_threshold(val, '0,0.05,0.2,0.5,2,5,20,40')

end

local function enable_sample()
	dinfo("enable statsmonitoring");
	local val = luardb.get(conf_prefix..".reportsamples");
	report_samples = defaultnumber(val, 192);

	val = luardb.get(conf_prefix..".sampleinterval");
	sample_interval = defaultnumber_nozero(val, 900);

	val = luardb.get(conf_prefix..".timereference");
	time_ref= defaultnumber_nozero(val, time_ref);

	throughput_params_changed()

	local current_time = os.time();

	local interval = computeFirstInterval(time_ref, current_time, sample_interval)
	collect_time = current_time + interval
	throughput_collect_time = current_time + throughput_sample_interval
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
	samples_count = 0;
	sample_time = ""
	sample_interval = 36000; --1 hours
	deleteAllAvc();
	deleteAllUnid();
	deleteRadio();
	deleteSystem()
end

-- The conditions to ignore this statsmonitoring.configuration change
-- 1) enable an enabled configuration, when the configuration is not changed
---2) disable an disabled configuration
local function config_changed()
	local enabled = luardb.get(conf_prefix..'.sampleenable')
	if tonumber(enabled) == 1 then
		throughput_params_changed() -- always load throughput config
		if sample_enable then
			-- reset if ...reportsamples or ...sampleinterval changed
			local val = luardb.get(conf_prefix..".reportsamples");
			local new_report_samples = defaultnumber(val, 192);
			val = luardb.get(conf_prefix..".sampleinterval");
			local new_sample_interval = defaultnumber_nozero(val, 900);
			if new_report_samples ~= report_samples or new_sample_interval ~= sample_interval then
				update_stats();
				disable_sample()
			else
				return  -- condition 1)
			end
		end
		enable_sample();
		load_stats();
	else
		if sample_enable then
			update_stats();
			disable_sample()
		end
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

-- wait until system time is valid
local rdb_system_time_updated = 'system.time.updated'
local system_time_updated = luardb.get(rdb_system_time_updated)
while not system_time_updated or system_time_updated == '' do
	luardb.wait(10);
	system_time_updated = luardb.get(rdb_system_time_updated)
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

luardb.watch(conf_prefix..'.changed', config_changed)

config_changed()

collect_time = os.time()
throughput_collect_time = collect_time
local snapshot_collect_time = collect_time -- system resource snapshot time
while true do
	-- throughput test starting condition
	local throughput_enabled = (throughput_enable['rxbkt'] or throughput_enable['txbkt'])
							and sample_enable and sample_interval > 2*throughput_sample_interval

	-- get min delay from three collecting tasks
	current_time = os.time();
	local interval = collect_time - current_time
	local tp_interval = throughput_collect_time - current_time
	local st_interval = snapshot_collect_time - current_time
	interval = interval > st_interval and st_interval or interval
	interval = (throughput_enabled and interval > tp_interval) and tp_interval or interval
	if interval > 0 then
		luardb.wait(interval);
	end
	current_time = os.time();
	--throughput data collection
	tp_interval = throughput_collect_time - current_time
	if tp_interval <= 0 then
		if throughput_enabled then
			throughput_sample_collected = true
			readIface(avc_dev_stats)
			avc_throughput(avc_dev_stats, throughput_sample_interval - tp_interval)
		end
		throughput_collect_time = current_time + throughput_sample_interval + tp_interval
	end
	--original tr-135 data collection
	interval = collect_time - current_time;
	if interval <= 0 then

		if sample_enable  then
			if samples_count >= report_samples then
				samples_count = samples_count-1
				remove_front();
			end
			readIface(avc_dev_stats)
			collect(current_time, avc_dev_stats);
			samples_count = samples_count+1

			update_stats();
			reset_bucket(avc_dev_stats)
			interval = computeFirstInterval(time_ref, current_time, sample_interval)
		else
			interval = sample_interval;
		end --if sample_enable then
		collect_time = current_time + interval;
		throughput_collect_time = current_time + throughput_sample_interval
	end --if interval <= 0

	st_interval = snapshot_collect_time - current_time
	if st_interval <= 0 then
		-- do snapshot on system resource
		system_snapshot()
		snapshot_collect_time = current_time + snapshot_interval
	end

end
