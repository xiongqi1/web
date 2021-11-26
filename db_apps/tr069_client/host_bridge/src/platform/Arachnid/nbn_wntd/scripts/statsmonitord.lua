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

debug = 4

local statsmonitorClassConfig = { persist = false}


local rdb_samples_count =conf.stats.statsmonitoringSamplePrefix..".samplescount";
local rdb_statsfilelock =conf.stats.statsmonitoringSamplePrefix..".statsfilelock";

--local rdb_sample_time =conf.stats.statsmonitoringSamplePrefix..".sampletime";
local conf_prefix =conf.stats.statsMonitoringConfigurationPrefix;

------------------------------------------------
local function dinfo(v)
  if debug > 1 then
    luasyslog.log('LOG_INFO', v)
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
	["bytesreceived"] = "greStats.txbytes",
	["framessent"] = "greStats.txpkt",
	["framesreceived"] = "greStats.txpkt",
}

----------------------------------------------------

local avcObjectClass = rdbobject.getClass(conf.wntd.avcPrefix,statsmonitorClassConfig)

local avcStatsObjectClass = rdbobject.getClass(conf.stats.statsmonitoringAvcPrefix,statsmonitorClassConfig)

local unidStatsObjectClass = rdbobject.getClass(conf.stats.statsmonitoringUnidPrefix,statsmonitorClassConfig)

-- Stats data
local sample_time="";
local avc_tab = {}
local unids_tab = {}

-- samples contol status
local sample_enable 	=0;
local report_samples 	=96;
local sample_interval 	=10;
local samples_count 	=0;
local collect_time = 0;
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
	local instance = avcStatsObjectClass:getById(id)
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


--[[

local function update_stats()

	luardb.set(conf.stats.statsmonitoringSamplePrefix..".sampletime", sample_time);
	for id, obj in pairs(avc_tab) do
		for name, val in pairs (obj) do
			luardb.set(conf.stats.statsmonitoringSamplePrefix..".avc."..id..".stats."..name, val);
		end
	end
	for id, obj in pairs(unids_tab) do
		for name, val in pairs (obj) do
			luardb.set(conf.stats.statsmonitoringSamplePrefix..".unid."..id..".stats."..name, val);
		end
	end

end
--]]

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
		file:close();

		luardb.set (rdb_statsfilelock,"");

	end --	if file then
	luardb.set(rdb_samples_count, samples_count);

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
	if sample_enable ~=1 then return  end;
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
	local instance = unidStatsObjectClass:getById(id)
	if instance then
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

	if sample_enable ~=1 then return  end;
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



local function defaultnumber(n, v)
  return tonumber(n) or v
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
end


--- collect sample data from unid and avc

local function collect()

	sample_time = sample_time .. os.date("%Y-%m-%dT%XZ")..",";

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

end

--[[

local function restart()
	sample_time = ""
	luardb.set(rdb_sample_time, sample_time);

	local ids = unidStatsObjectClass:getIds();
	for _, id in pairs(ids) do
		local instance = unidStatsObjectClass:getById(id);
		local mt = getmetatable(instance);
		for name1, _ in pairs (unid_stats_name) do
			luardb.set(mt.prefix .. name1, "");
		end
	end

	local ids = avcStatsObjectClass:getIds();
	for _, id in pairs(ids) do
		local instance = avcStatsObjectClass:getById(id);
		local mt = getmetatable(instance);
		for name1, _ in pairs (avc_stats_name) do
			luardb.set(mt.prefix .. name1, "");
		end
	end
end


local function collect()
	sample_time = sample_time .. os.date("%Y-%m-%dT%XZ")..",";
	luardb.set(rdb_sample_time, sample_time);

	local ids = unidStatsObjectClass:getIds();
	for _, id in pairs(ids) do
		local prefix =conf.wntd.unidPrefix .. "." ..id..".";
		local instance = unidStatsObjectClass:getById(id);
		local mt = getmetatable(instance);
		for name1, name2 in pairs (unid_stats_name) do
			local newval = luardb.get(prefix .. name2);
			if not newval then newval ="" end;
			local oldval = luardb.get(mt.prefix .. name1) or "";
			luardb.set(mt.prefix .. name1, oldval..newval..",");
		end
	end

	local ids = avcStatsObjectClass:getIds();
	for _, id in pairs(ids) do
		local prefix =conf.wntd.avcPrefix .. "." ..id..".";
		local instance = avcStatsObjectClass:getById(id);
		local mt = getmetatable(instance);
		for name1, name2 in pairs (avc_stats_name) do
			local newval = luardb.get(prefix .. name2);
			if not newval then newval ="" end;
			local oldval = luardb.get(mt.prefix .. name1) or "";
			luardb.set(mt.prefix .. name1, oldval..newval..",");
		end
	end

end
--]]

local function computeFirstInterval( sample_interval)
	local time_ref= luardb.get(conf_prefix..".timereference");
	time_ref= defaultnumber(time_ref, 0);
	if time_ref == 0 then
		return sample_interval;
	else
		local t = os.time() - time_ref;
		if t > 0 then
			return sample_interval - t %sample_interval;
		else
			return sample_interval + t %sample_interval;
		end
	end


end


local function enableChangedCB(rdb, val)
	val = defaultnumber(val, 0);
	--print(rdb, val);
	if sample_enable ~= val then


		if  val == 1 then

			dinfo("enable statsmonitoring");
			report_samples = luardb.get(conf_prefix..".reportsamples");
			sample_interval = luardb.get(conf_prefix..".sampleinterval");
			report_samples = defaultnumber(report_samples, 96);
			sample_interval = defaultnumber(sample_interval,900);
			if sample_interval ==0 then sample_interval =1; end

			interval = computeFirstInterval(sample_interval)
			collect_time = os.time() + interval;
			--print("first interval=".. interval, "sample_interval=".. sample_interval);
			sample_enable =1;
			samples_count =0;
			initUnid()
			initAvc()
		else
			dinfo("disable statsmonitoring");
			sample_enable ="";
			samples_count ="";
			sample_time = ""
			sample_interval = 36000; --1 hours
			deleteAllAvc();
			deleteAllUnid();
		end
		--print("avc "..table.tostring(avc_tab));
		--print("unid "..table.tostring(unids_tab));
		update_stats();
	end
end




----------------------------------------------------------
-- whole daemon processing

-- if local time is < 2011-01-01 00:00:00 then the clock is not synchronized by NTP
-- wait
local local_start_time = os.time();
while local_start_time < 1293840000 do

	luardb.wait(10);
	local_start_time = os.time();

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

	luardb.wait(interval);
	interval = collect_time - os.time();
	if interval <=0 then

		interval = sample_interval;
		collect_time = os.time() + sample_interval;

		if sample_enable ==1 then
			if samples_count < report_samples then
				collect();
				samples_count = samples_count+1

			else
				--print("restart", samples_count);
				--luardb.set("statsmonitoring.test", samples_count);
				--os.execute("sleep 1");
				samples_count =0;
				restart();
			end
			update_stats();
		end --if sample_enable ==1 then
	end --if interval < sample_interval then
end
