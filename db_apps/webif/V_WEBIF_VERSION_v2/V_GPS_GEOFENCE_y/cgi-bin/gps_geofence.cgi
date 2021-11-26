#!/usr/bin/env lua
require('luardb')
require('rdbobject')
require('stringutil')
require('tableutil')

local query_string_cmd = os.getenv('cmd')

local skip_session_validation = 0

if query_string_cmd and query_string_cmd == 'getList' then
	skip_session_validation = 1
end

print("Content-type: text/html\n")

if skip_session_validation ~= 1 then
	if( os.getenv("SESSION_ID")=="nil" or os.getenv("SESSION_ID") ~=  os.getenv("sessionid") ) then
		return
	end

	-- CSRF token must be valid
	local csrfToken = os.getenv('csrfToken')
	local csrfTokenGet = os.getenv('csrfTokenGet')
	if ( csrfToken == "nil" or csrfTokenGet == "nil" or csrfToken ~= csrfTokenGet ) then
		os.exit(254)
	end
end

local is_valid_instId = function(class, instId)
	if not class or not instId or not tonumber(instId) then return false end

	ids=class:getIds();

	for _, val in ipairs(ids) do
		if tonumber(val) == tonumber(instId) then
			return true
		end
	end
	return false
end

local rdbObjConf = {
	persist = true,
	idSelection = 'smallestUnused'
}

local rdbCirObj = rdbobject.getClass('sensors.gps.0.geofence.circular', rdbObjConf)

if query_string_cmd=="getList" then
	local latitude=luardb.get('sensors.gps.0.common.latitude')
	local latitude_dir=luardb.get('sensors.gps.0.common.latitude_direction')
	local longitude=luardb.get('sensors.gps.0.common.longitude')
	local longitude_dir=luardb.get('sensors.gps.0.common.longitude_direction')


	print("{")
	-- current gps coordinate data
	print('"curr_lati":"' .. (latitude or '') .. '",')
	print('"curr_lati_dir":"' .. (latitude_dir or '') .. '",')
	print('"curr_long":"' .. (longitude or '') .. '",')
	print('"curr_long_dir":"' .. (longitude_dir or '') .. '",')

	print('"geofenceList":[')
	local allList = rdbCirObj:getAll();
	for idx, inst in ipairs(allList) do
		if idx ~= 1 then print(",") end
		print("{")
		local loop_cnt=1
		for key, val in rdbCirObj:iterator(inst) do
			if loop_cnt ~= 1 then print(",") end
			loop_cnt = loop_cnt+1;
			print('"' .. (key or '') .. '":"' .. (val or '') .. '"')
		end
		print("}")
	end

	print("]") -- geofenceList
	print("}")

elseif query_string_cmd=="editEntry" then
	local query_string_index = os.getenv('index')
	local query_string_name = os.getenv('name')
	local query_string_lati = os.getenv('latitude')
	local query_string_long = os.getenv('longitude')
	local query_string_radius = os.getenv('radius')
	local query_string_evtnoti = os.getenv('evtnoti')
	local result = 1;

	if query_string_index and tonumber(query_string_index) and (tonumber(query_string_index) == -1 or is_valid_instId(rdbCirObj, query_string_index)) then
		local rdbInst;
		if tonumber(query_string_index) == -1 then -- create new instance
			rdbInst = rdbCirObj:new()
			rdbInst.rdbIndex = rdbCirObj:getId(rdbInst)
			rdbInst.status = ''
		else
			rdbInst = rdbCirObj:getById(query_string_index)
		end

		rdbInst.name = query_string_name
		rdbInst.lati = query_string_lati
		rdbInst.long = query_string_long
		rdbInst.radius = query_string_radius
		rdbInst.evtnoti = query_string_evtnoti
		result = 0;
	end

	if result == 0 then
		luardb.set('sensors.gps.0.geofence.daemon.trigger', '1'); -- trigger daemon if list is updated.
	end

	print('{"cgiresult":"' .. result .. '"}')

elseif query_string_cmd=="delEntry" then
	local query_string_index = os.getenv('index')
	local result = 1;
	if query_string_index and tonumber(query_string_index) and is_valid_instId(rdbCirObj, query_string_index) then

		local rdbInst = rdbCirObj:getById(query_string_index)

		if rdbInst then
			rdbCirObj:delete(rdbInst)
			result = 0;
		end
	end

	if result == 0 then
		luardb.set('sensors.gps.0.geofence.daemon.trigger', '1'); -- trigger daemon if list is updated.
	end

	print('{"cgiresult":"' .. result .. '"}')
end

