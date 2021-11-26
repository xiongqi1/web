
local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Services.GPS.'

------------------local function prototype------------------

------------------------------------------------------------


------------------local variable definition ----------------

------------------------------------------------------------


------------------local function definition ----------------

------------------------------------------------------------


return {
-- =====[START] MSB==================================

-- bool:readwrite
-- Default Value: 
-- Available Value: 
-- Involved RDB variable: "sensors.gps.0.gpsone.enable"
	[subROOT .. 'MSB.Enable_AGPS'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = '0'
			local retVal = luardb.get('sensors.gps.0.gpsone.enable')
			retVal = string.trim(retVal)

			if retVal ~= '0' and retVal ~= '1' then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "1" and value ~= "0" then return CWMP.Error.InvalidParameterValue end

			local curVal = luardb.get('sensors.gps.0.gpsone.enable')
			
			if curVal == value then return 0 end
			luardb.set('sensors.gps.0.gpsone.enable', value)
			return 0
		end
	},

-- uint:readwrite
-- Default Value: 5
-- Available Value: 3|5|10
-- Involved RDB variable: "sensors.gps.0.gpsone.auto_update.max_retry_count"
	[subROOT .. 'MSB.MaxRetryCnt'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 5
			local retVal = luardb.get('sensors.gps.0.gpsone.auto_update.max_retry_count')
			retVal = tonumber(string.trim(retVal))

			if not retVal then retVal = defaultVal end
			return 0, tostring(retVal)
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "3" and value ~= "5" and value ~= "10" then return CWMP.Error.InvalidParameterValue end

			local curVal = luardb.get('sensors.gps.0.gpsone.auto_update.max_retry_count')
			
			if curVal == value then return 0 end
			luardb.set('sensors.gps.0.gpsone.auto_update.max_retry_count', value)
			return 0
		end
	},

-- uint:readwrite
-- Default Value: 3
-- Available Value: 3|5|10|15|30 (unit: minute)
-- Involved RDB variable: "sensors.gps.0.gpsone.auto_update.retry_delay"
	[subROOT .. 'MSB.RetryDelay'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 3
			local retVal = luardb.get('sensors.gps.0.gpsone.auto_update.retry_delay')
			retVal = tonumber(string.trim(retVal))

			if not retVal then retVal = defaultVal end
			return 0, tostring(retVal)
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "3" and value ~= "5" and value ~= "10" and value ~= "15" and value ~= "30" then return CWMP.Error.InvalidParameterValue end

			local curVal = luardb.get('sensors.gps.0.gpsone.auto_update.retry_delay')
			
			if curVal == value then return 0 end
			luardb.set('sensors.gps.0.gpsone.auto_update.retry_delay', value)
			return 0
		end
	},

-- uint:readwrite
-- Default Value: 5
-- Available Value: 0~7  (unit: day, 0-Manual)
-- Involved RDB variable: "sensors.gps.0.gpsone.auto_update.update_period"
	[subROOT .. 'MSB.AutoUpdatePeriod'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = 5 * (24*60)
			local retVal = luardb.get('sensors.gps.0.gpsone.auto_update.update_period')
			retVal = tonumber(string.trim(retVal)) or defaultVal

			retVal = math.floor(retVal / (24*60))
			return 0, tostring(retVal)
		end,
		set = function(node, name, value)
			value = tonumber(string.trim(value))
			if not value or value < 0 or value > 7 then return CWMP.Error.InvalidParameterValue end

			value = value * (24*60)
			local curVal = luardb.get('sensors.gps.0.gpsone.auto_update.update_period')
			
			if curVal == value then return 0 end
			luardb.set('sensors.gps.0.gpsone.auto_update.update_period', value)
			return 0
		end
	},

-- datetime:readonly
-- Default Value: 
-- Available Value: 
-- Involved RDB variable: "sensors.gps.0.gpsone.xtra.info.gnss_time"
	[subROOT .. 'MSB.GNSSDataLastUpdate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = '0'
			local retVal = luardb.get('sensors.gps.0.gpsone.xtra.info.gnss_time')
			retVal = string.trim(retVal)

			if retVal == '' or not tonumber(retVal) then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- datetime:readonly
-- Default Value: 
-- Available Value: 
-- Involved RDB variable: "sensors.gps.0.gpsone.xtra.info.valid_time"
	[subROOT .. 'MSB.GNSSDataExpires'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = '0'
			local retVal = luardb.get('sensors.gps.0.gpsone.xtra.info.valid_time')
			retVal = string.trim(retVal)

			if retVal == '' or not tonumber(retVal) then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- datetime:readonly
-- Default Value: 
-- Available Value: 
-- Involved RDB variable: "sensors.gps.0.gpsone.xtra.updated_time"
	[subROOT .. 'MSB.AGPSLastUpdate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultVal = '0'
			local retVal = luardb.get('sensors.gps.0.gpsone.xtra.updated_time')
			retVal = string.trim(retVal)

			if retVal == '' or not tonumber(retVal) then retVal = defaultVal end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- =====[ END ] MSB==================================
}
