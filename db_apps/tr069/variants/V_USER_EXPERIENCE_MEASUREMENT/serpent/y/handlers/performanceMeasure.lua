--[[
This script handles Bell Canada parameters for traffic performance measurement under
"Device.Cellular.X_<VENDOR>_AccessPoint_Performance."

  RTTTrackingMode
  MeasurementPeriod
  InstantaneousSamplingPeriod
  NumberOfMeasureRecords
  IdleTrafficLimitDownlink
  IdleTrafficLimitUplink
  Timestamps

and

"Device.Cellular.AccessPoint.{i}.X_<VENDOR>_Performance."

  Enable
  Latency
  PeakDownlinkThroughput
  AverageDownlinkBytesReceived
  AverageDownlinkTimeDuration
  AverageDownlinkThroughput
  PeakUplinkThroughput
  AverageUplinkBytesSent
  AverageUplinkTimeDuration
  AverageUplinkThroughput

Refer to "https://pdgwiki.netcommwireless.com/mediawiki/index.php/User_Experience_Throughput_Measurement"

Copyright (C) 2018 NetComm Wireless Limited.
--]]

require("CWMP.Error")
require("Logger")
local logSubsystem = 'performanceMeasure'
Logger.addSubsystem(logSubsystem)

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

------------ .Cellular.<VENDOR>_AccessPoint_Performance ------------
local subRootConf = conf.topRoot .. '.Cellular.' .. xVendorPrefix .. '_AccessPoint_Performance'
local rdbPrefix = "performance.measurement."

local function performanceMeasurementTrigger()
	Logger.log(logSubsystem, 'debug', 'trigger performance.measurement.trigger')
	luardb.set(rdbPrefix .. "trigger", "1");
end

local function registerTriggerCallback()
	if client:isTaskQueued('postSession', performanceMeasurementTrigger) == false then
		client:addTask('postSession', performanceMeasurementTrigger)
	end
end

local function updateRdbIfNeeded(rdbName, newValue)
	local oldValue = luardb.get(rdbName)
	if oldValue ~= newValue then
		luardb.set(rdbName, newValue, 'p')
		registerTriggerCallback()
	end
end

local function uintValidator(value, min, max)
	local nValue = tonumber(value)
	local nMin = tonumber(min)
	local nMax = tonumber(max)
	if not nValue or nValue < 0 then return false end
	if nMax and nValue > nMax then return false end
	if nMin and nValue < nMin then return false end
	return true
end


--------------- .Cellular.AccessPoint.*.<VENDOR>_Performance ----------------
local subRootResult = conf.topRoot .. '.Cellular.AccessPoint.*.' .. xVendorPrefix .. '_Performance'

local resultToRdbTbl = {
	['Latency'] = 'latency',
	['PeakDownlinkThroughput'] = 'PeakDownlinkThroughput',
	['AverageDownlinkBytesReceived'] = 'AverageDownlinkBytesReceived',
	['AverageDownlinkTimeDuration'] = 'AverageDownlinkTimeDuration',
	['AverageDownlinkThroughput'] = 'AverageDownlinkThroughput',
	['PeakUplinkThroughput'] = 'PeakUplinkThroughput',
	['AverageUplinkBytesSent'] = 'AverageUplinkBytesSent',
	['AverageUplinkTimeDuration'] = 'AverageUplinkTimeDuration',
	['AverageUplinkThroughput'] = 'AverageUplinkThroughput',
}

-- Get a value of rdb variable corresponding with parameter
-- If the rdb variable does not exist, function returns parameter default value.
local function getParamRdbValue(paramNode, paramName)
	local pathBits = paramName:explode('.')

	local idx = pathBits[#pathBits - 2]
	if not tonumber(idx) then
		error('Error: unknown index parameter "' .. (idx or 'nil') .. '"')
	end

	local rdbValue = luardb.get(rdbPrefix .. tostring(idx) .. '.' .. resultToRdbTbl[pathBits[#pathBits]])

	return rdbValue or paramNode.default
end

return {
	-----------------------------------------------------------------
	------------ .Cellular.<VENDOR>_AccessPoint_Performance ------------
	-----------------------------------------------------------------
	-- Below configuration handlers could be combined with generic handler, though.
	-- Possibly, the list could be extended soon or later, so those are left separately for now.

	-- Description: Select RTT Tracking mode ( 0=SYN-SYN/ACK, 1=SYN-SYN/ACK-ACK ).
	-- 		By default, SYN-SYN/ACK is used.
	-- Data model: param RTTTrackingMode uint notify(0,0,2) readwrite dynamic('performanceMeasure', 0);
	-- Rdb name: performance.measurement.RTTTrackingMode
	[subRootConf .. '.RTTTrackingMode'] = {
		get = function(node, name)
			local rdbName = rdbPrefix .. "RTTTrackingMode";
			return 0, tostring(tonumber(luardb.get(rdbName)) or node.default);
		end,
		set = function(node, name, value)
			local rdbName = rdbPrefix .. "RTTTrackingMode";
			if not uintValidator(value, 0, 1) then
				return CWMP.Error.InvalidParameterValue
			end

			updateRdbIfNeeded(rdbName, value)
			return 0
		end
	};
	-- Description: Throughput measurement and passive latency record Period in minutes
	-- 		(by default 60 minutes).
	-- Data model: param MeasurementPeriod uint notify(0,0,2) readwrite dynamic('performanceMeasure', 60);
	-- Rdb name: performance.measurement.MeasurementPeriod
	[subRootConf .. '.MeasurementPeriod'] = {
		get = function(node, name)
			local rdbName = rdbPrefix .. "MeasurementPeriod";
			return 0, tostring(tonumber(luardb.get(rdbName)) or node.default);
		end,
		set = function(node, name, value)
			local rdbName = rdbPrefix .. "MeasurementPeriod";
			if not uintValidator(value, 1, nil) then
				return CWMP.Error.InvalidParameterValue
			end

			updateRdbIfNeeded(rdbName, value)
			return 0
		end
	};
	-- Description: Instantaneous sampling period to measure peak throughput in seconds
	-- 		(by default 3 seconds).
	-- Data model: param InstantaneousSamplingPeriod uint notify(0,0,2) readwrite dynamic('performanceMeasure', 3);
	-- Rdb name: performance.measurement.InstantaneousSamplingPeriod
	[subRootConf .. '.InstantaneousSamplingPeriod'] = {
		get = function(node, name)
			local rdbName = rdbPrefix .. "InstantaneousSamplingPeriod";
			return 0, tostring(tonumber(luardb.get(rdbName)) or node.default);
		end,
		set = function(node, name, value)
			local rdbName = rdbPrefix .. "InstantaneousSamplingPeriod";
			if not uintValidator(value, 1, nil) then
				return CWMP.Error.InvalidParameterValue
			end

			updateRdbIfNeeded(rdbName, value)
			return 0
		end
	};
	-- Description: size of a comma delimited list of measurement result.
	-- Data model: param NumberOfMeasureRecords uint notify(0,0,2) readwrite dynamic('performanceMeasure', 25);
	-- Rdb name: performance.measurement.NumberOfMeasureRecords
	[subRootConf .. '.NumberOfMeasureRecords'] = {
		get = function(node, name)
			local rdbName = rdbPrefix .. "NumberOfMeasureRecords";
			return 0, tostring(tonumber(luardb.get(rdbName)) or node.default);
		end,
		set = function(node, name, value)
			local rdbName = rdbPrefix .. "NumberOfMeasureRecords";
			if not uintValidator(value, 1, nil) then
				return CWMP.Error.InvalidParameterValue
			end

			updateRdbIfNeeded(rdbName, value)
			return 0
		end
	};
	-- Description: Downlink threshold to detect idle traffic (Kbit per second).
	-- Data model: param IdleTrafficLimitDownlink uint notify(0,0,2) readwrite dynamic('performanceMeasure', 100);
	-- Rdb name: performance.measurement.IdleTrafficLimitDownlink
	[subRootConf .. '.IdleTrafficLimitDownlink'] = {
		get = function(node, name)
			local rdbName = rdbPrefix .. "IdleTrafficLimitDownlink";
			return 0, tostring(tonumber(luardb.get(rdbName)) or node.default);
		end,
		set = function(node, name, value)
			local rdbName = rdbPrefix .. "IdleTrafficLimitDownlink";
			if not uintValidator(value, 1, nil) then
				return CWMP.Error.InvalidParameterValue
			end

			updateRdbIfNeeded(rdbName, value)
			return 0
		end
	};
	-- Description: Uplink threshold to detect idle traffic (Kbit per second).
	-- Data model: param IdleTrafficLimitUplink uint notify(0,0,2) readwrite dynamic('performanceMeasure', 50);
	-- Rdb name: performance.measurement.IdleTrafficLimitUplink
	[subRootConf .. '.IdleTrafficLimitUplink'] = {
		get = function(node, name)
			local rdbName = rdbPrefix .. "IdleTrafficLimitUplink";
			return 0, tostring(tonumber(luardb.get(rdbName)) or node.default);
		end,
		set = function(node, name, value)
			local rdbName = rdbPrefix .. "IdleTrafficLimitUplink";
			if not uintValidator(value, 1, nil) then
				return CWMP.Error.InvalidParameterValue
			end

			updateRdbIfNeeded(rdbName, value)
			return 0
		end
	};

	------------------------------------------------------------------
	----------- .Cellular.AccessPoint.*.<VENDOR>_Performance ------------
	------------------------------------------------------------------
	--[[
		param Enable bool notify(0,0,2) readwrite dynamic('performanceMeasure', 0);
		param Latency string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param PeakDownlinkThroughput string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param AverageDownlinkBytesReceived string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param AverageDownlinkTimeDuration string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param AverageDownlinkThroughput string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param PeakUplinkThroughput string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param AverageUplinkBytesSent string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param AverageUplinkTimeDuration string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param AverageUplinkThroughput string notify(0,0,2) readonly dynamic('performanceMeasure', '');
	--]]
	[subRootResult .. '.*'] = {
		get = function(node, name)
			local pathBits = name:explode('.')
			local paramName = pathBits[#pathBits]
			if resultToRdbTbl[paramName] then
				return 0, getParamRdbValue(node, name)
			elseif paramName == 'Enable' then
				local idx = pathBits[#pathBits - 2]
				if not tonumber(idx) then
					return CWMP.Error.InvalidParameterName
				end
				local rdbName = rdbPrefix .. tostring(idx) .. ".Enable"
				return 0, (luardb.get(rdbName) == '1' and '1' or node.default)
			else
				return CWMP.Error.InvalidParameterName
			end
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local paramName = pathBits[#pathBits]
			if paramName == 'Enable' then
				local idx = pathBits[#pathBits - 2]
				if not tonumber(idx) then
					return CWMP.Error.InvalidParameterName
				end
				local rdbName = rdbPrefix .. tostring(idx) .. ".Enable"
				if not value or (value ~= '1' and value ~= '0') then
					return CWMP.Error.InvalidParameterValue
				end

				updateRdbIfNeeded(rdbName, value)
				return 0
			elseif resultToRdbTbl[paramName] then
				return  CWMP.Error.ReadOnly
			else
				return CWMP.Error.InvalidParameterName
			end
		end
	};
}
