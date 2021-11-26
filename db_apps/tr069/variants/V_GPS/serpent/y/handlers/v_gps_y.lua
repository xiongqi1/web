--[[
	GPS handler
	Copyright (C) 2018 NetComm Wireless Limited.
--]]

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

-- the following prefixes are supported
local prefixes = {conf.topRoot .. '.' .. xVendorPrefix .. '.Services.GPS.'}

------------------local function prototype------------------
------------------------------------------------------------


------------------local variable definition ----------------
local g_numOfSatellitesInstance = 12
local g_depthOfSatellitesInstance = 7
------------------------------------------------------------


------------------local function definition ----------------
------------------------------------------------------------


local handlers = {

-- =====[START] GPSConfiguration==================================
-- bool:readwrite
-- Default Value: 0(disable)
-- Available Value: 0(diable), 1(enable)
-- Involved RDB variable: sensors.gps.0.enable
	['GPSConfiguration.Enable_GPS'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = '0'
			local retVal = luardb.get('sensors.gps.0.enable')
			retVal = string.trim(retVal)

			if retVal ~= '0' and retVal ~= '1' then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "1" and value ~= "0" then return CWMP.Error.InvalidParameterValue end

			local curVal = luardb.get('sensors.gps.0.enable')

			if curVal == value then return 0 end
			luardb.set('sensors.gps.0.enable', value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.enable" and "sensors.gps.0.source"
	['GPSConfiguration.GPSStatus.DataSource'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = "N/A"
			local resultList = {
				['1'] = { ['agps'] = 'MS Assisted GPS',
					  ['standalone'] = 'Stand-alone GPS',
					  ['historical-standalone'] = 'Previously Stored GPS Data (Standalone)',
					  ['historical-agps'] = 'Previously Stored GPS Data (Mobile Assisted)',
					},
				['0'] = {
					--[[
						Since Serpent keeps updating gps RDBs even enable=='0',
						we show "N/A" in this case.
					--]]
					},
			}

			local enable = luardb.get('sensors.gps.0.enable')
			local source = luardb.get('sensors.gps.0.source')
			enable = string.trim(enable)
			source = string.trim(source)

			if enable ~= '1' and enable ~= '0' then enable = '0' end

			return 0, (resultList[enable][source] or defaultVal)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.common.date"
--[[
-- from "db_apps/webif/gps.c"

        /* read date & time */
        tmp = get_single("sensors.gps.0.common.date");          /* ex) 230394 */
        if (strncmp(tmp, "N/A", 3)) {
                temp = atoi(tmp);
                dd = temp / 10000;
                mm = (temp % 10000) / 100;
                yy = temp % 100;
                if (yy >= 80) {
                        printf("date=\"%02d.%02d.19%02d\";\n", dd, mm, yy);
                } else {
                        printf("date=\"%02d.%02d.20%02d\";\n", dd, mm, yy);
                }
        } else {
                printf("date=\"%s\";", tmp);
        }
--]]
	['GPSConfiguration.GPSStatus.DateTime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.common.date')
			retVal = tonumber(string.trim(retVal)) or defaultVal

			if retVal ~= defaultVal then
				retVal = string.format("%06d", retVal)
				local dd, mm, yy = string.match(retVal, "(%d%d)(%d%d)(%d%d)")
				retVal = string.format("%s.%s.%s%s", dd, mm, (tonumber(yy) >= 80 and '19' or '20'), yy)
			end

			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.common.latitude"
	['GPSConfiguration.GPSStatus.Latitude'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.common.latitude')
			retVal = string.trim(retVal)
			if retVal == '' then
				retVal = defaultVal
			else
				-- add correct sign
				local dir = luardb.get('sensors.gps.0.common.latitude_direction')
				if string.trim(dir) == 'S' then
					retVal = '-' .. retVal
				end
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.common.latitude_degrees"
	['GPSConfiguration.GPSStatus.Latitude_DecimalDegrees'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.common.latitude_degrees')
			retVal = string.trim(retVal)
			if retVal == '' then
				retVal = defaultVal
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.common.longitude"
	['GPSConfiguration.GPSStatus.Longitude'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.common.longitude')
			retVal = string.trim(retVal)

			if retVal == '' then
				retVal = defaultVal
			else
				-- add correct sign
				local dir = luardb.get('sensors.gps.0.common.longitude_direction')
				if string.trim(dir) == 'W' then
					retVal = '-' .. retVal
				end
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.common.longitude_degrees"
	['GPSConfiguration.GPSStatus.Longitude_DecimalDegrees'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.common.longitude_degrees')
			retVal = string.trim(retVal)

			if retVal == '' then
				retVal = defaultVal
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.common.height_of_geoid"
	['GPSConfiguration.GPSStatus.Altitude'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.common.height_of_geoid')
			retVal = string.trim(retVal)

			if retVal == '' then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.standalone.ground_speed_kph" and "sensors.gps.0.standalone.ground_speed_knots"
	['GPSConfiguration.GPSStatus.GroundSpeed'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			local ground_speed_kph, ground_speed_knots
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				ground_speed_kph = defaultVal
				ground_speed_knots = defaultVal
			else
				ground_speed_kph = string.trim(luardb.get('sensors.gps.0.standalone.ground_speed_kph'))
				ground_speed_knots = string.trim(luardb.get('sensors.gps.0.standalone.ground_speed_knots'))
			end
			if ground_speed_kph == '' then ground_speed_kph = defaultVal end
			if ground_speed_knots == '' then ground_speed_knots = defaultVal end

			return 0, ground_speed_kph .. ' km/h, ' .. ground_speed_knots .. ' knots'
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.standalone.pdop"
	['GPSConfiguration.GPSStatus.PDOP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.standalone.pdop')
			retVal = string.trim(retVal)

			if retVal == '' then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.standalone.hdop"
	['GPSConfiguration.GPSStatus.HDOP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.standalone.hdop')
			retVal = string.trim(retVal)

			if retVal == '' then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.standalone.vdop"
	['GPSConfiguration.GPSStatus.VDOP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.standalone.vdop')
			retVal = string.trim(retVal)

			if retVal == '' then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: "sensors.gps.0.standalone.valid"
	['GPSConfiguration.GPSStatus.StandaloneGPSStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.standalone.valid')
			retVal = string.trim(retVal)

			if retVal == '' then
			       	retVal = defaultVal
			elseif retVal == 'valid' then
			       	retVal = 'Normal'
			else
			       	retVal = 'Invalid'
		       	end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- -- Default Value:
-- -- Available Value:
-- -- Involved RDB variable: "sensors.gps.0.standalone.number_of_satellites"
	['GPSConfiguration.NumberOfSatellites'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 'N/A'
			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				return 0, defaultVal
			end
			local retVal = luardb.get('sensors.gps.0.standalone.number_of_satellites')
			retVal = string.trim(retVal)

			if retVal == '' then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- object:readonly
-- Involved RDB variable: "sensors.gps.0.standalone.satellitedata"
	['GPSConfiguration.SatellitesStatus'] = {
		init = function(node, name, value)
			for i=1, g_numOfSatellitesInstance do
				node:createDefaultChild(i)
			end
			return 0
		end,
	},

-- instances:readonly
-- Involved RDB variable: "sensors.gps.0.standalone.satellitedata"
	['GPSConfiguration.SatellitesStatus.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfSatellitesInstance = #pathBits
			return 0
		end,
	},

-- Involved RDB variable: "sensors.gps.0.standalone.satellitedata"
	['GPSConfiguration.SatellitesStatus.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local dataModelIdx = tonumber(pathBits[g_depthOfSatellitesInstance] or 0) or 0
			local paramName = pathBits[g_depthOfSatellitesInstance+1] or ''
			local retVal = ''

			if dataModelIdx == 0 then
				return CWMP.Error.InvalidParameterValue, "Error: Parameter does not exist: " .. name
			end

			local defaultVal='0,N/A,N/A,N/A,N/A'
			local satellitedata = string.trim(luardb.get('sensors.gps.0.standalone.satellitedata'))
			local dataList = satellitedata:explode(';') or {}
			local subData = dataList[dataModelIdx] or defaultVal

			if string.trim(luardb.get('sensors.gps.0.enable')) ~= '1' then
				subData = defaultVal
			end
			local subDataList = subData:explode(',')

			if paramName == 'InUse' then
				retVal = subDataList[1] or '0'
			elseif paramName == 'PRN' then
				retVal = subDataList[2] or 'N/A'
			elseif paramName == 'SNR' then
				retVal = subDataList[3] or 'N/A'
			elseif paramName == 'Elevation' then
				retVal = subDataList[4] or 'N/A'
			elseif paramName == 'Azimuth' then
				retVal = subDataList[5] or 'N/A'
			else
				error('Dunno how to handle ' .. name)
			end

			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- =====[ END ] GPSConfiguration==================================
}

local ret = {}
for _, prefix in ipairs(prefixes) do
	for key, handler in pairs(handlers) do
		ret[prefix .. key] = handler
	end
end

return ret
