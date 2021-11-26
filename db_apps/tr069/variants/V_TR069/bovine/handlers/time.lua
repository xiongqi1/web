----
-- NTP Bindings
--
-- Vaugely Time:2 compilant, but largely read-only.
-- NTPServerX and Enable are mapped directly to RDB by config file persist() handlers.
-- We also implement InternetGatewayDevice.DeviceInfo.UpTime here too.
----

require('Daemon')
require('Logger')
local logSubsystem = 'DeviceTime'
Logger.addSubsystem(logSubsystem)

local g_rdb_currFL='system.config.tz'
local g_timezoneRefFile = "/usr/zoneinfo.ref"
local g_zoneinfoTbl = {}

local function uptimePoller(task)
	local node = task.data
	local uptime = tostring(Daemon.readIntFromFile('/proc/uptime')) or '0'
	client:asyncParameterChange(node, node:getPath(), uptime)
end

local function buildZoneinfoTbl(refFile)
	local zoneinfoTbl = {}
	local file, msg = io.open(refFile, "r")
	if not file then print("Error: TimeZone reference File Open Failure (" .. refFile .. ")"); return zoneinfoTbl; end

	for line in file:lines() do
		local zoneinfo = {FL='', TZ='', NAME='', DST='', LTZ=''}
		contents=line:explode(';')

		if contents[1] and contents[2] and contents[3] and contents[4] then
			zoneinfo.FL=contents[1]
			zoneinfo.TZ=contents[2]
			
			zoneinfo.DST=contents[3]
			-- to add default value of start or end time (note "man tzset")
			if zoneinfo.DST and zoneinfo.DST ~= ''then 
				local dstInfo = string.explode(zoneinfo.DST, ',')
				if not dstInfo[2]:find("%/%d") then
					dstInfo[2] = dstInfo[2] .. '/2'
				end
				if not dstInfo[3]:find("%/%d") then
					dstInfo[3] = dstInfo[3] .. '/2'
				end
				zoneinfo.DST = dstInfo[1] .. ',' .. dstInfo[2] .. ',' .. dstInfo[3]
			end

			zoneinfo.NAME=contents[4]
			zoneinfo.LTZ=string.match(zoneinfo.NAME, "%(GMT(.-)%)")
			if zoneinfo.LTZ == '' then
				zoneinfo.LTZ = "+00:00"
			end

			table.insert(zoneinfoTbl, zoneinfo)
		end
	end
	file:close()

	return zoneinfoTbl
end

--@ DESCRIPTION: return timezone element matching with value of given field from TimeZone info table built by "buildZoneinfoTbl" function.
--@ Argument: 
--@ 		zoneinfoTbl: table built with "buildZoneinfoTbl" function.
--@ 		field: name of sub element (available value: FL, TZ, NAME, DST, LTZ)
--@		currValue: current TimeZone FL from "rdb_get system.config.tz"
--@ Return:
--@		Success: return a table that contains all of elements that match the value of given field "{FL='', TZ='', NAME='', DST='', LTZ=''}"
--@		Failure: nil
local function get_TZelem_ByField(zoneinfoTbl, field, currValue)
	local resultTbl = {}
	if not zoneinfoTbl or not field or field == '' or not currValue or currValue == '' then return resultTbl end
	
	for i, element in ipairs(zoneinfoTbl) do
		if element and element[field] then
			if element[field] == currValue then
				table.insert(resultTbl, element)
			end
		end
	end
	
	return resultTbl
end

--@ DESCRIPTION: set DST default start time and end time, if the data doesn't have specific time.
--@ Argument: 
--@ 		dst: TZ and DST pair with no seperator
local function set_defaultDSTTime(dst)
	if not dst then return '' end

	local retValue = ''
	local dstInfo = string.explode(dst, ',')

	if dstInfo[1] then 
		retValue = dstInfo[1]
	end
	if dstInfo[2] and dstInfo[3] then
		if not dstInfo[2]:find("%/%d") then
			retValue = retValue .. ',' .. dstInfo[2] .. '/2'
		else
			retValue = retValue .. ',' .. dstInfo[2]
		end

		if not dstInfo[3]:find("%/%d") then
			retValue = retValue .. ',' .. dstInfo[3] .. '/2'
		else
			retValue = retValue .. ',' .. dstInfo[3]
		end
	end

	return retValue
end

--@ DESCRIPTION: return element that has given TZ and DST from TimeZone info table built by "buildZoneinfoTbl" function.
--@ Argument: 
--@ 		tz_dst_pair: TZ and DST pair with no seperator
--@ Return:
--@		Success: return a element that match with given tz_dst_pair "{FL='', TZ='', NAME='', DST='', LTZ=''}"
--@		Failure: nil
local function find_TZ_DST_pair(tz_dst_pair)
	if not tz_dst_pair then return nil end

	for i, element in ipairs(g_zoneinfoTbl) do
		local tz_dst_fromTbl = (element.TZ and element.TZ or '') .. (element.DST and element.DST or '')
		if tz_dst_pair == tz_dst_fromTbl then
			return element
		end
	end
	return nil
end

-- TR-181 (1&2 with root Device.) changed parameter name from LocalTimeZoneName to LocalTimeZone (LocalTimeZone in TR-098 has been removed in TR-181)
local timeZoneParamName = (conf.topRoot == 'Device') and 'LocalTimeZone' or 'LocalTimeZoneName'
return {
	['**.DeviceInfo.UpTime'] = {
		get = function(node, name)
			return 0, tostring(Daemon.readIntFromFile('/proc/uptime')) or '0'
		end,
		attrib = function(node, name)
			if node.notify > 0 and not client:isTaskQueued('preSession', uptimePoller) then
				client:addTask('preSession', uptimePoller, true, node)
			elseif node.notify < 1 and client:isTaskQueued('preSession', uptimePoller) then
				client:removeTask('preSession', uptimePoller)
			end
			return 0
		end,
	},
	['**.Time.CurrentLocalTime'] = {
		-- this should report local time
		get = function(node, name) return 0, os.date('%s')..'L'	end
	},
	-- The following handlers only apply to TR-098
	['InternetGatewayDevice.Time.LocalTimeZone'] = {
		init = function(node, name)
			if #g_zoneinfoTbl == 0 then
				g_zoneinfoTbl = buildZoneinfoTbl(g_timezoneRefFile)
			end
			return 0
		end,
		get = function(node, name)
			local currRDB_FL = luardb.get(g_rdb_currFL);
			if not currRDB_FL then return 0, '+00:00' end

			local currentTZElem = get_TZelem_ByField(g_zoneinfoTbl, 'FL', currRDB_FL)
			if not currentTZElem or #currentTZElem == 0 or not currentTZElem[1].LTZ or currentTZElem[1].LTZ == '' then return 0, '+00:00' end
			return 0, currentTZElem[1].LTZ
		end,
		set = function(node, name, value)
			local setLocalTZ = value and value:gsub('%s', '') or '' -- take out all of white spaces.

			if setLocalTZ:match("[+-]%d%d:%d%d") then
				if setLocalTZ == "-00:00" then setLocalTZ = "+00:00" end
				local currentTZElem = get_TZelem_ByField(g_zoneinfoTbl, 'LTZ', setLocalTZ)
				if not currentTZElem or #currentTZElem == 0 or not currentTZElem[1].FL or currentTZElem[1].FL == '' then return CWMP.Error.InvalidArguments end
				
				luardb.set(g_rdb_currFL, currentTZElem[1].FL)
			else
				return CWMP.Error.InvalidArguments
			end
			return 0
		end
	},
	-- The following handlers apply to both TR-098 & TR-181 but with different param names
	['**.Time.' .. timeZoneParamName] = {
		-- We should build the table here in the case the above parameter InternetGatewayDevice.Time.LocalTimeZone does not exist.
		init = function(node, name)
			if #g_zoneinfoTbl == 0 then
				g_zoneinfoTbl = buildZoneinfoTbl(g_timezoneRefFile)
			end
			return 0
		end,
		get = function(node, name)
			local currRDB_FL = luardb.get(g_rdb_currFL);
			-- the right timezone name for UTC is UTC0
			if not currRDB_FL then return 0, 'UTC0' end
			Logger.log(logSubsystem, 'debug', 'currRDB_FL=' .. currRDB_FL)

			local currentTZElem = get_TZelem_ByField(g_zoneinfoTbl, 'FL', currRDB_FL)
			if not currentTZElem or #currentTZElem == 0 or not currentTZElem[1].TZ or not currentTZElem[1].DST then return 0, 'UTC0' end
			Logger.log(logSubsystem, 'debug', 'currentTZElem=' .. currentTZElem[1].FL .. ';' .. currentTZElem[1].TZ .. ';' .. currentTZElem[1].DST .. ';' .. currentTZElem[1].NAME)

			return 0, currentTZElem[1].TZ .. currentTZElem[1].DST
		end,
		-- The format of the IEEE 1003.1 POSIX timezone specification is defined as follows:
		-- stdoffset[dst[offset],[start[/time],end[/time]]]
		set = function(node, name, value)
			-- fill in the time portion with default (2) if missing
			local tz_dst = set_defaultDSTTime(value)

			if not tz_dst:find("%S") then return CWMP.Error.InvalidArguments end  -- value is empty string
			local elem = find_TZ_DST_pair(tz_dst)
			if not elem then return CWMP.Error.InvalidArguments, 'Not supported Time Zone' end

			luardb.set(g_rdb_currFL, elem.FL)
			return 0
		end
	},
	['**.Time.X_NETCOMM_LTZoneNameList'] = {
		get = function(node, name)
			local retVal = ''

			for _, element in ipairs(g_zoneinfoTbl) do
				local zoneinfo = element.NAME .. ': ' .. element.TZ .. element.DST .. ';\n'
				retVal = retVal .. zoneinfo
			end

			return 0, retVal
		end,
	},
}

