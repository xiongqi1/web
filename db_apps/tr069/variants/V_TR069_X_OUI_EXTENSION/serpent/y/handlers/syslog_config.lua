--[[
This script handles Bell Canada parameters for system log configurations under Device.DeviceInfo.VendorLogFile.1.[X_<VENDOR>_XXX|MaximumSize]

  MaximumSize
  X_<VENDOR>_MaximumSize
  X_<VENDOR>_Persistent
  X_<VENDOR>_Level

Copyright (C) 2018 NetComm Wireless Limited.
--]]

require("CWMP.Error")
require("Logger")
local logSubsystem = 'syslog_config'
Logger.addSubsystem(logSubsystem)
local subRoot = conf.topRoot .. '.DeviceInfo.VendorLogFile.1'

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local function cb_trigger_syslogd()
	Logger.log(logSubsystem, 'debug', 'trigger service.syslog.option.trigger')
	luardb.set("service.syslog.option.trigger", "1");
end

local function register_cb_trigger_syslogd()
	if client:isTaskQueued('postSession', cb_trigger_syslogd) == false then
		client:addTask('postSession', cb_trigger_syslogd)
	end
end

return {
	-- param MaximumSize uint notify(0,0,2) readonly dynamic("syslog_config", "");
	-- service.syslog.option.sizekb
	[subRoot .. '.MaximumSize'] = {
		get = function(node, name)
			local rdbName = "service.syslog.option.sizekb";
			return 0, tostring((tonumber(luardb.get(rdbName)) or 256) * 1024)
		end,
	};
	-- param X_<VENDOR>_MaximumSizeInKB uint notify(0,0,2) readwrite dynamic("syslog_config", "");
	-- service.syslog.option.sizekb
	-- ## Maximum log file size in KB (1~1000000)
	[subRoot .. '.' .. xVendorPrefix .. '_MaximumSizeInKB'] = {
		get = function(node, name)
			local rdbName = "service.syslog.option.sizekb";
			return 0, tostring(tonumber(luardb.get(rdbName)) or 256);
		end,
		set = function(node, name, value)
			local setVal = tonumber(value)
			if not setVal or setVal < 1 or setVal > 1000000 then
				return CWMP.Error.InvalidParameterValue
			end

			local rdbName = "service.syslog.option.sizekb";
			local curVal = tonumber(luardb.get(rdbName));
			if setVal ~= curVal then
				register_cb_trigger_syslogd();
				luardb.set(rdbName, setVal);
			end
			return 0
		end
	};
	-- param X_<VENDOR>_Persistent bool notify(0,0,2) readwrite dynamic("syslog_config", "");
	-- service.syslog.option.logtofile
	[subRoot .. '.' .. xVendorPrefix .. '_Persistent'] = {
		get = function(node, name)
			local rdbName = "service.syslog.option.logtofile";
			local result = luardb.get(rdbName);
			if not result or (result ~= "1" and result ~= "0") then
				result = "0"; -- default value: "0"
			end
			return 0, result
		end,
		set = function(node, name, value)
			if not value or (value ~= '1' and value ~= '0') then
				return CWMP.Error.InvalidParameterValue
			end

			local rdbName = "service.syslog.option.logtofile";
			local curVal = luardb.get(rdbName);
			if curVal ~= value then
				register_cb_trigger_syslogd();
				luardb.set(rdbName, value);
			end
			return 0
		end
	};
	-- param X_<VENDOR>_Level uint notify(0,0,2) readwrite dynamic("syslog_config", "");
	-- service.syslog.option.capturelevel
	-- ## Log capture level in number(4(Error), 5(Warning), 6(Notice), 7(Info), 8(Debug))
	[subRoot .. '.' .. xVendorPrefix .. '_Level'] = {
		get = function(node, name)
			local rdbName = "service.syslog.option.capturelevel";
			return 0, tostring(tonumber(luardb.get(rdbName)) or 6)
		end,
		set = function(node, name, value)
			local setVal = tonumber(value)
			if not setVal or setVal < 4 or setVal > 8 then
				return CWMP.Error.InvalidParameterValue
			end

			local rdbName = "service.syslog.option.capturelevel";
			local curVal = tonumber(luardb.get(rdbName));
			if setVal ~= curVal then
				register_cb_trigger_syslogd();
				luardb.set(rdbName, setVal);
			end
			return 0
		end
	};
}
