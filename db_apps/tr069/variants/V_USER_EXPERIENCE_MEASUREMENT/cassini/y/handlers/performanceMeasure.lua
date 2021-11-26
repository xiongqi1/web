--[[
This script handles Bell Canada parameters for traffic performance measurement under
"Device.Cellular.X_<VENDOR>_Performance."

  MeasurementPeriod
  InstantaneousSamplingPeriod
  NumberOfMeasureRecords
  Timestamps

"Device.Cellular.Interface.{i}.X_<VENDOR>_Performance."

  Enable
  LTECellularNetwork_ECGI_eNodeB_CellID_PCI_EARFCN
  LTECellularSignal_RSRP_RSRQ_SINR_RSSI_CQI
  5GCellularNetwork_NCGI_gNodeB_gNB-CellID_gNB-PCI_NR-ARFCN_gNB-SSBIndex
  5GCellularSignal_SS-RSRP_SS-RSRQ_SS-SINR_NR-RSSI_NR-CQI

"Device.Cellular.X_<VENDOR>_AccessPoint_Performance."

  RTTTrackingMode
  IdleTrafficLimitDownlink
  IdleTrafficLimitUplink

and

"Device.Cellular.AccessPoint.{i}.X_<VENDOR>_Performance."

  Enable
  Latency
  PeakDownlinkThroughput_PeakUplinkThroughput
  AverageDownlinkThroughput_AverageUplinkThroughput
  DownlinkBytesReceived_UplinkBytesSent
  AverageDownlinkTimeDuration_AverageUplinkTimeDuration
  IPv4Address_IPv6Address
  UpTime

Refer to "https://pdgwiki.netcommwireless.com/mediawiki/index.php/User_Experience_Throughput_Measurement"

Copyright (C) 2020 Casa Systems.
--]]

require("CWMP.Error")
require("Logger")
local logSubsystem = 'performanceMeasure'
Logger.addSubsystem(logSubsystem)

local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

------------ .Cellular.X_<VENDOR>_Performance ------------
local subRootConf = conf.topRoot .. '.Cellular.' .. xVendorPrefix .. '_Performance'
------------ .Cellular.X_<VENDOR>_AccessPoint_Performance ------------
local subRootApConf = conf.topRoot .. '.Cellular.' .. xVendorPrefix .. '_AccessPoint_Performance'

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

--------------- .Cellular.Interface.*.X_<VENDOR>_Performance ----------------
local subRootIfResult = conf.topRoot .. '.Cellular.Interface.*.' .. xVendorPrefix .. '_Performance'
local ifResultToRdbTbl = {
	['LTECellularNetwork_ECGI_eNodeB_CellID_PCI_EARFCN'] = {'ecgi', 'enodeb', 'cellid', 'pci', 'earfcn'},
	['LTECellularSignal_RSRP_RSRQ_SINR_RSSI_CQI'] = {'rsrp', 'rsrq', 'sinr', 'rssi', 'cqi'},
	['5GCellularNetwork_NCGI_gNodeB_gNB-CellID_gNB-PCI_NR-ARFCN_gNB-SSBIndex'] = {'ncgi', 'gnodeb', 'gnb_cellid', 'gnb_pci', 'nr_arfcn', 'gnb_ssbindex'},
	['5GCellularSignal_SS-RSRP_SS-RSRQ_SS-SINR_NR-RSSI_NR-CQI'] = {'ss_rsrp', 'ss_rsrq', 'ss_sinr', 'nr_rssi', 'nr_cqi'},
}

--------------- .Cellular.AccessPoint.*.X_<VENDOR>_Performance ----------------
local subRootApResult = conf.topRoot .. '.Cellular.AccessPoint.*.' .. xVendorPrefix .. '_Performance'

local apResultToRdbTbl = {
	['Latency'] = 'latency',
	['PeakDownlinkThroughput_PeakUplinkThroughput'] = {'PeakDownlinkThroughput', 'PeakUplinkThroughput'},
	['DownlinkBytesReceived_UplinkBytesSent'] = {'TotalDownlinkBytesReceived', 'TotalUplinkBytesSent'},
	['AverageDownlinkTimeDuration_AverageUplinkTimeDuration'] = {'AverageDownlinkTimeDuration', 'AverageUplinkTimeDuration'},
	['AverageDownlinkThroughput_AverageUplinkThroughput'] = {'AverageDownlinkThroughput', 'AverageUplinkThroughput'},
	['IPv4Address_IPv6Address'] = {'IPv4Address', 'IPv6Address'},
	['UpTime'] = 'UpTime',
}

-- Get a value of rdb variable corresponding with parameter
-- If the rdb variable does not exist, function returns parameter default value.
local function getParamRdbValue(paramNode, paramName)
	local pathBits = paramName:explode('.')
	local ap = (pathBits[#pathBits - 3] == 'AccessPoint')
	local idx = pathBits[#pathBits - 2]
	if not tonumber(idx) then
		error('Error: unknown index parameter "' .. (idx or 'nil') .. '"')
	end

	local rdbKey = ap and apResultToRdbTbl[pathBits[#pathBits]] or ifResultToRdbTbl[pathBits[#pathBits]]
	local rdbValue
	if type(rdbKey) == 'string' then
		rdbValue = luardb.get(rdbPrefix .. (ap and tostring(idx) or 'cellular') .. '.' .. rdbKey)
	elseif type(rdbKey) == 'table' then
		local combTab
		for _, rdbk in ipairs(rdbKey) do
			local rdbv = luardb.get(rdbPrefix .. (ap and tostring(idx) or 'cellular') .. '.' .. rdbk)
			if not rdbv then
				combTab = nil
				break
			end
			local vBits = rdbv:explode(',')
			if not combTab then
				combTab = vBits
			else
				if #combTab ~= #vBits then
					combTab = nil
					break
				end
				for i = 1, #combTab do
					combTab[i] = combTab[i] .. ';' .. vBits[i]
				end
			end
		end
		if combTab then
			rdbValue = table.concat(combTab, ',')
		end
	end
	-- if only ';' is included, do not show
	if rdbValue and not rdbValue:match('[^;]') then
		rdbValue = nil
	end
	return rdbValue or paramNode.default
end

return {
	-----------------------------------------------------------------
	------------ .Cellular.X_<VENDOR>_Performance ------------
	-----------------------------------------------------------------
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


	------------------------------------------------------------------
	----------- .Cellular.Interface.*.X_<VENDOR>_Performance ------------
	------------------------------------------------------------------
	--[[
		param Enable bool notify(0,0,2) readwrite dynamic('performanceMeasure', 0);
		param LTECellularNetwork_ECGI_eNodeB_CellID_PCI_EARFCN string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param LTECellularSignal_RSRP_RSRQ_SINR_RSSI_CQI string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param 5GCellularNetwork_NCGI_gNodeB_gNB-CellID_gNB-PCI_NR-ARFCN_gNB-SSBIndex string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param 5GCellularSignal_SS-RSRP_SS-RSRQ_SS-SINR_NR-RSSI_NR-CQI string notify(0,0,2) readonly dynamic('performanceMeasure', '');
	--]]
	[subRootIfResult .. '.*'] = {
		get = function(node, name)
			local pathBits = name:explode('.')
			if pathBits[#pathBits - 2] ~= '1' then
				return CWMP.Error.InvalidParameterName -- only support 1 physical interface
			end
			local paramName = pathBits[#pathBits]
			if ifResultToRdbTbl[paramName] then
				return 0, getParamRdbValue(node, name)
			elseif paramName == 'Enable' then
				local rdbName = rdbPrefix .. "cellular.enable"
				return 0, (luardb.get(rdbName) == '1' and '1' or node.default)
			else
				return CWMP.Error.InvalidParameterName
			end
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			if pathBits[#pathBits - 2] ~= '1' then
				return CWMP.Error.InvalidParameterName -- only support 1 physical interface
			end
			local paramName = pathBits[#pathBits]
			if paramName == 'Enable' then
				local rdbName = rdbPrefix .. "cellular.enable"
				if not value or (value ~= '1' and value ~= '0') then
					return CWMP.Error.InvalidParameterValue
				end

				updateRdbIfNeeded(rdbName, value)
				return 0
			elseif ifResultToRdbTbl[paramName] then
				return  CWMP.Error.ReadOnly
			else
				return CWMP.Error.InvalidParameterName
			end
		end
	};

	-----------------------------------------------------------------
	------------ .Cellular.X_<VENDOR>_AccessPoint_Performance ------------
	-----------------------------------------------------------------
	-- Below configuration handlers could be combined with generic handler, though.
	-- Possibly, the list could be extended soon or later, so those are left separately for now.

	-- Description: Select RTT Tracking mode ( 0=SYN-SYN/ACK, 1=SYN-SYN/ACK-ACK ).
	-- 		By default, SYN-SYN/ACK is used.
	-- Data model: param RTTTrackingMode uint notify(0,0,2) readwrite dynamic('performanceMeasure', 0);
	-- Rdb name: performance.measurement.RTTTrackingMode
	[subRootApConf .. '.RTTTrackingMode'] = {
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
	-- Description: Downlink threshold to detect idle traffic (Kbit per second).
	-- Data model: param IdleTrafficLimitDownlink uint notify(0,0,2) readwrite dynamic('performanceMeasure', 100);
	-- Rdb name: performance.measurement.IdleTrafficLimitDownlink
	[subRootApConf .. '.IdleTrafficLimitDownlink'] = {
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
	[subRootApConf .. '.IdleTrafficLimitUplink'] = {
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
	----------- .Cellular.AccessPoint.*.X_<VENDOR>_Performance ------------
	------------------------------------------------------------------
	--[[
		param Enable bool notify(0,0,2) readwrite dynamic('performanceMeasure', 0);
		param Latency string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param PeakDownlinkThroughput_PeakUplinkThroughput string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param DownlinkBytesReceived_UplinkBytesSent string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param AverageDownlinkTimeDuration_AverageUplinkTimeDuration string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param AverageDownlinkThroughput_AverageUplinkThroughput string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param IPv4Address_IPv6Address string notify(0,0,2) readonly dynamic('performanceMeasure', '');
		param UpTime string notify(0,0,2) readonly dynamic('performanceMeasure', '');
	--]]
	[subRootApResult .. '.*'] = {
		get = function(node, name)
			local pathBits = name:explode('.')
			local paramName = pathBits[#pathBits]
			if apResultToRdbTbl[paramName] then
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
			elseif apResultToRdbTbl[paramName] then
				return  CWMP.Error.ReadOnly
			else
				return CWMP.Error.InvalidParameterName
			end
		end
	};
}
