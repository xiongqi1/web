--[[
    NIT DA interface

    Copyright (C) 2021 Casa Systems.
--]]

local common_module = require('common')
require("luardb")

local module = {}

-- Set the battery stats from the NIT
-- @param self A RequestHandler instance
local function battery(self)
	if not common_module.da_authorize(self, 'sensors') then
		logErr("failed to authorize to use NIT battery sensors api")
		return
	end
	local battery_level = self:get_argument("Level")
	local battery_voltage = self:get_argument("Voltage")
	local battery_status = self:get_argument("Status")
	luardb.set('nit.battery.level', battery_level)
	luardb.set('nit.battery.voltage', battery_voltage)
	luardb.set('nit.battery.status', battery_status)
end

-- Set the compass data from the NIT
-- @param self A RequestHandler instance
local function compass(self)
	if not common_module.da_authorize(self, 'sensors') then
		logErr("failed to authorize to use NIT compass sensors api")
		return
	end

	local compass_calibration = self:get_argument("Status")
	local bearing_raw = self:get_argument("BearingRaw")
	local bearing_corrected = self:get_argument("BearingCorrected")

	luardb.set("nit.compass.status", compass_calibration)
	luardb.set("nit.compass.status_p", compass_calibration)
	luardb.set("nit.compass.bearing_raw", bearing_raw)
	if compass_calibration == "0" then
		luardb.set("nit.compass.bearing_corrected", bearing_corrected)
		luardb.set("nit.azimuth_p", bearing_corrected)
	else
		-- If compass not calibrated, set true north to blank 
		luardb.set("nit.compass.bearing_corrected", "")
		luardb.set("nit.azimuth_p", bearing_raw)
	end
end

-- Set the downtilt data from the NIT
-- @param self A RequestHandler instance
local function downtilt(self)
	if not common_module.da_authorize(self, 'sensors') then
		logErr("failed to authorize to use NIT downtilt sensors api")
		return
	end

	local downtilt = self:get_argument("Angle")
	luardb.set("nit.downtilt", downtilt)
	luardb.set("nit.downtilt_p", downtilt)
end

function module.init(maps, _, util, authorizer_)
	logDebug("init NIT API")
	local basepath = "/api/v2/NIT"
	maps[basepath.."/GPS"] = {
		get = {code = '200'},
		model = {
			Altitude = util.map_rdb('r', 'sensors.gps.0.common.altitude'),
			Latitude = util.map_rdb('r', 'sensors.gps.0.common.latitude_degrees'),
			Longitude = util.map_rdb('r', 'sensors.gps.0.common.longitude_degrees'),
			Height = util.map_rdb('r', 'sensors.gps.0.common.height_of_geoid'),
			VerticalUncertainty = util.map_rdb('r', 'sensors.gps.0.common.vertical_uncertainty'),
			HorizontalUncertainty = util.map_rdb('r', 'sensors.gps.0.common.horizontal_uncertainty'),
			Source = util.map_rdb('r', 'sensors.gps.0.source'),
			Status = util.map_rdb('r', 'sensors.gps.0.common.status'),
			Date = util.map_rdb('r', 'sensors.gps.0.common.date')
		}
	}

	maps[basepath .. "/Battery"] = {
		put = {
			code = '200',
			trigger = function () return battery end
		},
	}

	maps[basepath .. "/Compass"] = {
		put = {
			code = '200',
			trigger = function () return compass end
		},
	}

	maps[basepath .. "/Downtilt"] = {
		put = {
			code = '200',
			trigger = function () return downtilt end
		},
	}
end

return module
