#!/usr/bin/env lua
-- Copyright (C) 2016 NetComm Wireless limited.
-- This script forms a daemon that connects to the GPSD daemon
-- from which it receives JSON formatted GPS data and writes it to the setGpsRdb

require('socket')
require('luardb')
require('luasyslog')
JSON = require('JSON')

luasyslog.open('gpsd_lua_client', 'LOG_DAEMON')
luasyslog.log('info', 'Started.')

-- This set of counters is used to help determine validity
gpsd_updateCnt=0	-- this is the global counter incremented every time we get data from gpsd
updateCntr = {}		-- this is a set of counters for each type of data, used to determine if data is valid

-- this checks the relevant update counter and returns true if the data is less than about 10 seconds old
-- Note we get 2-3 sets of data from gpsd per second hence 30 in the code ( actual time is not important )
-- name is used to identify the counter
function isDataValid(name)
	local cntr = updateCntr[name]
	if  cntr then
		-- print('Cntr '..name..' '..cntr..' '..gpsd_updateCnt)
		if gpsd_updateCnt - cntr < 30 then return true end
	end
	-- print('Not valid')
	return false
end

-- write data to the getGpsRdb
-- as an optimisation we keep the last value written and only update if different
lastRdbValues = {}
function setGpsRdb(name,val)
	local last = lastRdbValues[name]
	if last then
		-- print('setGpsRdb lastRdbValues '..last)
		if last == val then
			-- print('setGpsRdb last Rdb Value matched')
			return
		end
	end
	-- print('setGpsRdb '..name..' '..val)
	luardb.set('sensors.gps.0.'..name,val)
	lastRdbValues[name]=val
end

-- read from the rdb
function getGpsRdb(name)
	return luardb.get('sensors.gps.0.'..name)
end

function twoDigits(val)
	return string.format("%02d",val)
end

function threeDigits(val)
	return string.format("%03d",val)
end

-- This function updates latitude or longitude
-- latlon is the value of the data
-- name is 'latitude' or 'longitude'
-- pos and neg are the strings east/west etc
function formatTude(latlon, name, pos, neg)
	local dir
	local val
	if latlon then
		-- change a decimal number into degrees minutes seconds
		local deg, frac = math.modf(latlon)
		val = deg*100+frac*60
		if val >=0 then
			dir = pos
		else
			val = -val
			dir = neg
		end
		updateCntr[name] = gpsd_updateCnt
	else
		dir =  'N/A'
		val =  'N/A'
		if isDataValid(name) then return end
	end
	setGpsRdb('standalone.'..name,val)
	setGpsRdb('standalone.'..name..'_direction',dir)
	setGpsRdb('common.'..name,val)
	setGpsRdb('common.'..name..'_direction',dir)
end

-- This function handles the TPV message that looks like this
-- {"class":"TPV","tag":"RMC","device":"/dev/ttyUSB1","mode":3,"time":"2016-03-30T05:24:08.000Z","lat":-33.807382550,"lon":151.148264883,"alt":42.200,"track":176.9000,"speed":0.000}
-- Not all fields are present all the time so we need to check they exist
function processTpv(json_values)
	formatTude(json_values.lat,'latitude', 'N','S' )
	formatTude(json_values.lon,'longitude', 'E','W' )
	local val = json_values.alt
	if val then
		updateCntr['altitude'] = gpsd_updateCnt
		setGpsRdb('standalone.altitude',val)
	else
		if not isDataValid('altitude') then
			setGpsRdb('standalone.altitude', 'N/A')
		end
	end
	val = json_values.track
	if val then
		updateCntr['track'] = gpsd_updateCnt
		setGpsRdb('standalone.track_angle',val)
		setGpsRdb('standalone.true_track_made_good',val)
	else
		if not isDataValid('track') then
			setGpsRdb('standalone.track_angle', 'N/A')
			setGpsRdb('standalone.true_track_made_good', 'N/A')
		end
	end
	val = json_values.speed
	if val then
		updateCntr['speed'] = gpsd_updateCnt
		setGpsRdb('standalone.ground_speed_kph',string.format("%.1f",val*3.6))	-- speed is in m/s
		setGpsRdb('standalone.ground_speed_knots',string.format("%.1f",val*1.94384))
	else
		if not isDataValid('speed') then
			setGpsRdb('standalone.ground_speed_kph', 'N/A')
			setGpsRdb('standalone.ground_speed_knots', 'N/A')
		end
	end
	local time=json_values.time
	if time then
		updateCntr['time'] = gpsd_updateCnt
		setGpsRdb('common.date',string.sub(time,9,10)..string.sub(time,6,7)..string.sub(time,3,4))
		setGpsRdb('common.time',string.sub(time,12,13)..string.sub(time,15,16)..string.sub(time,18,21))
	else
		if not isDataValid('time') then
			setGpsRdb('common.date', 'N/A')
			setGpsRdb('common.time', 'N/A')
		end
	end
end

-- This function handles the satellite data with the SKY message that looks like this
-- {"PRN":26,"el":5,"az":307,"ss":0,"used":false}
-- a comma seperated string is returned
function formatSatellite(sat)
	local used='0'
	if (sat.used) then
		used='1'
	end
	local ss = sat.ss
	if ss == 0 then
		ss = 'N/A'
	end
	local el = sat.el
	if el == 0 then
		el = 'N/A'
	else
		el = twoDigits(el)
	end
	local az = sat.az
	if az == 0 then
		az = 'N/A'
	else
		az = threeDigits(az)
	end
	return used..','..twoDigits(sat.PRN)..','..ss..','..el..','..az
end

-- This function handles the SKY message that looks like this
-- {"class":"SKY","tag":"GSV","device":"/dev/ttyUSB1","satellites":[{"PRN":8,"el":8,"az":217,"ss":29,"used":true},{"PRN":10,"el":58,"az":251,"ss":45,"used":true},{"PRN":15,"el":29,"az":130,"ss":41,"used":true},{"PRN":16,"el":12,"az":279,"ss":30,"used":false},{"PRN":18,"el":63,"az":160,"ss":43,"used":true},{"PRN":21,"el":59,"az":97,"ss":33,"used":true},{"PRN":27,"el":35,"az":232,"ss":32,"used":true},{"PRN":33,"el":0,"az":0,"ss":37,"used":false},{"PRN":20,"el":12,"az":116,"ss":0,"used":false},{"PRN":24,"el":12,"az":84,"ss":0,"used":false},{"PRN":26,"el":5,"az":307,"ss":0,"used":false},{"PRN":29,"el":8,"az":21,"ss":0,"used":false},{"PRN":32,"el":26,"az":343,"ss":0,"used":false}]}
-- Not all fields are present all the time so we need to check they exist
function processSky(json_values)
	local satData = ''
	local num_sats=0
	if (json_values.satellites) then
		num_sats=#json_values.satellites
	end
	local num_sats_tracked=0
	setGpsRdb('standalone.number_of_satellites',num_sats)

	if (json_values.satellites) then
		for key,sat in pairs(json_values.satellites) do
			-- print(key)
			-- for key1,value1 in pairs(sat) do print(key1,value1) end
			satData=satData..';'..formatSatellite(sat)
			if sat.used then
				num_sats_tracked = num_sats_tracked + 1
			end
		end
	end
	if num_sats < 12 then
		for i=num_sats,12 do
			satData=satData..';'..'N/A,N/A,N/A,N/A,N/A'
		end
	end
	setGpsRdb('standalone.vdop',json_values.vdop or 'N/A')
	setGpsRdb('standalone.hdop',json_values.hdop or 'N/A')
	setGpsRdb('standalone.pdop',json_values.pdop or 'N/A')
	setGpsRdb('standalone.number_of_satellites_tracked',num_sats_tracked)
	setGpsRdb('standalone.satellitedata',string.sub(satData,2,-1)) -- remove the first ';'
	setGpsRdb('source','standalone')
end

function processString(s)
	local json_values = JSON:decode(s)
	if json_values.class == 'TPV' then
		-- print('receiv TPV')
		-- for key,value in pairs(json_values) do print(key,value) end
		processTpv(json_values)
		updateCntr['TPV'] = gpsd_updateCnt
	elseif json_values.class == 'SKY' then
		-- print('receiv SKY')
		-- for key,value in pairs(json_values) do print(key,value) end
		processSky(json_values)
	else
		luasyslog.log('error', 'Received unexpected data'..s)
		-- for key,value in pairs(json_values) do print(key,value) end
	end
	if isDataValid('TPV') then
		setGpsRdb('standalone.valid','valid')
	else
		setGpsRdb('standalone.valid','N/A')
		processTpv({})
	end
end

-- This is the main loop that sits wating for data from the GPSD
function connectToGpsd()
	local c = assert(socket.connect('127.0.0.1', 2947)) -- Connect to GPSD
	c:settimeout(5)
	c:send('?WATCH={"enable":true,"json":true}\n') -- Start GPSD talking
	while true do
		if getGpsRdb('gpsd_status') ~= 'enable' then break end

		-- print('receiv')
		local s, status = c:receive('*l')
		if status then
			if status == "closed" then break end
		end
		-- luasyslog.log('debug', 'Received data')
		if s then
			-- print(s)
			pcall(processString, s)
			gpsd_updateCnt = gpsd_updateCnt + 1
		end
	end
	c:close()
end

connectToGpsd()

luasyslog.log('info', 'Stopped.')

