
local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Services.GPS.'

------------------local function prototype------------------
local convert_meter_to
local round
local convert_feet_to_meter
------------------------------------------------------------


------------------local variable definition ----------------
------------------------------------------------------------


------------------local function definition ----------------

-- Convert meter to kilometers or miles
-- val -> distance in unit meter
-- unit: '0' -> kilometers, '1' -> miles (default:'0')
convert_meter_to = function (val, unit)
	local meter_to_miles=0.000621371192;
	local metre_to_feet=3.2808399;
	local mile_to_feet=5280;

	val = string.trim(val)
	if val == '' then val = 0 end
	if not unit then unit ='0' end

	local intVal = tonumber(val)

	if not intVal then intVal = 0 end

	if unit == '1' then
		intVal = intVal * meter_to_miles
		if intVal < 1 then
			return string.format("%d", intVal*mile_to_feet+0.5) .. ' Feet(s)'
		else
			return string.format("%.1f", intVal) .. ' Mile(s)'
		end
	else
		if intVal > 1000 then
			return string.format("%.1f", intVal/1000) .. ' Kilometer(s)'
		else
			return intVal .. ' Meter(s)'
		end
	end
	return "0"
end

round = function (num, idp)
  local mult = 10^(idp or 0)
  return math.floor(num * mult + 0.5) / mult
end

convert_feet_to_meter = function (val)
	local feet_to_meter = 0.3048
	val = string.trim(val)
	if val == '' then val = 0 end
	local intVal = tonumber(val)

	return string.format("%d", round(intVal * feet_to_meter))
end
------------------------------------------------------------


return {

-- =====[START] Odometer==================================
-- bool:readwrite
-- Default Value: 0 -> stop
-- Available Value: 0 -> stop, 1 -> start
-- Involved RDB variable: sensors.gps.0.odometer_enable
	[subROOT .. 'Odometer.Enable_Odometer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('sensors.gps.0.odometer_enable')
			retVal = string.trim(retVal)

			if retVal ~= 'start' and retVal ~= 'stop' then retVal = 'stop' end 
			return 0, retVal == 'start' and "1" or "0"
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "1" and value ~= "0" then return CWMP.Error.InvalidParameterValue end

			if value == "1" then
				value = 'start'
			else
				value = 'stop'
			end

			local curVal = luardb.get('sensors.gps.0.odometer_enable')
			
			if curVal == value then return 0 end
			luardb.set('sensors.gps.0.odometer_enable', value)
			return 0
		end
	},

-- string:readonly
-- Default Value: ""
-- Available Value: Metric system / Imperial system
-- Involved RDB variable: sensors.gps.0.odometer (This rdb variable returns Odometer Value in kilometer, need to convert unit following DisplayUnit parameter)
--
-- function to convert unit is from translate_unit() on gps.html as below
------------------------------------------------------------------------------------------------------------
-- var meter_to_miles=0.000621371192;
-- var metre_to_feet=3.2808399;
-- var mile_to_feet=5280;
-- 
-- function translate_unit(val) {
--         if (isNaN(val)==true) {
--                 val = 0;
--         }
--         if (disp_miles=="1") {
--                 val*=meter_to_miles;
--                 if( val<1 ) {
--                         return parseInt(val*mile_to_feet+0.5)+"&nbsp;&nbsp;"+_("feet");
--                 }
--                 return val.toFixed(1)+"&nbsp;&nbsp;"+_("miles");
--         } else {
--                 if(val>1000) {
--                         return (val/1000).toFixed(1)+"&nbsp;&nbsp;"+_("kilometer");
--                 }
--                 return parseInt(val)+"&nbsp;&nbsp;"+_("meter");
--         }
-- }
------------------------------------------------------------------------------------------------------------
	[subROOT .. 'Odometer.OdometerReading'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local odometer = luardb.get('sensors.gps.0.odometer')
			local displayUnit = luardb.get('sensors.gps.0.odometer_miles')
			
			odometer = string.trim(odometer)
			if odometer == '' then odometer = 0 end

			displayUnit = string.trim(displayUnit)
			if displayUnit == '' then displayUnit = '0' end

			
			return 0, convert_meter_to(odometer, "0") .. " / " .. convert_meter_to(odometer, "1")
			--return 0, convert_meter_to(odometer, displayUnit)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- bool:readwrite
-- Default Value: 0
-- Available Value: 0 -> kilometers, 1 -> miles
-- Involved RDB variable: sensors.gps.0.odometer_miles
	[subROOT .. 'Odometer.DisplayUnit'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('sensors.gps.0.odometer_miles')
			retVal = string.trim(retVal)

			if retVal ~= '1' and retVal ~= '0' then retVal = '0' end 
			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "1" and value ~= "0" then return CWMP.Error.InvalidParameterValue end

			local curVal = luardb.get('sensors.gps.0.odometer_miles')
			
			if curVal == value then return 0 end
			luardb.set('sensors.gps.0.odometer_miles', value)
			return 0
		end
	},

-- bool:writeonly
-- Default Value:
-- Available Value: 1
-- Involved RDB variable: sensors.gps.0.odometer_reset (should set the rdb value as 'reset' to reset odometer)
	[subROOT .. 'Odometer.ResetOdometer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ""
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "1" then return CWMP.Error.InvalidParameterValue end
			luardb.set('sensors.gps.0.odometer_reset', 'reset')
			return 0
		end
	},
-- =====[ END ] Odometer==================================
}
