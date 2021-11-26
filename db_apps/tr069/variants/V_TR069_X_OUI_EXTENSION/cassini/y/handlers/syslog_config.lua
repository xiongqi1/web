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
	luardb.set("service.syslog.option.trigger", "1")
end

local function register_cb_trigger_syslogd()
	if client:isTaskQueued('postSession', cb_trigger_syslogd) == false then
		client:addTask('postSession', cb_trigger_syslogd)
	end
end

local function is_persistent_log()
	local rdbName = "service.syslog.option.logtofile"
	local result = luardb.get(rdbName)
	return result == '1'
end

-- get maximum log storage size in kb.
-- service.syslog.option.buffersizekb or
-- service.syslog.option.rotatesize and service.syslog.option.rotategens
-- depend on service.syslog.option.logtofile
local function get_max_log_size()
	local persistent = is_persistent_log()
	if persistent then
		local rdbName = "service.syslog.option.rotatesize"
		local size = tonumber(luardb.get(rdbName)) or 500
		rdbName = "service.syslog.option.rotategens"
		local gens = (tonumber(luardb.get(rdbName)) or 1) + 1
		return size * gens
	else
		local rdbName = "service.syslog.option.buffersizekb"
		return tonumber(luardb.get(rdbName)) or 256
	end
end

return {
	-- param MaximumSize uint notify(0,0,2) readonly dynamic("syslog_config", "");
	[subRoot .. '.MaximumSize'] = {
		get = function(node, name)
			return 0, tostring(get_max_log_size() * 1024)
		end
	},
	-- param X_<VENDOR>_MaximumSizeInKB uint notify(0,0,2) readwrite dynamic("syslog_config", "");
	[subRoot .. '.' .. xVendorPrefix .. '_MaximumSizeInKB'] = {
		get = function(node, name)
			return 0, tostring(get_max_log_size())
		end,
		set = function(node, name, value)
			local persistent = is_persistent_log()
			local setVal = tonumber(value)
			if persistent then
				if not setVal or setVal < 500 or setVal > 5000 then
					return CWMP.Error.InvalidParameterValue
				end
				local rdbName = "service.syslog.option.rotategens"
				local gens = (tonumber(luardb.get(rdbName)) or 1) + 1
				local size = math.floor(setVal / gens)
				rdbName = "service.syslog.option.rotatesize"
				local curVal = tonumber(luardb.get(rdbName))
				if size ~= curVal then
					luardb.set(rdbName, size)
					register_cb_trigger_syslogd()
				end
			else
				if not setVal or setVal < 100 or setVal > 512 then
					return CWMP.Error.InvalidParameterValue
				end
				local rdbName = "service.syslog.option.buffersizekb"
				local curVal = tonumber(luardb.get(rdbName))
				if setVal ~= curVal then
					luardb.set(rdbName, setVal)
					register_cb_trigger_syslogd()
				end
			end
			return 0
		end
	},
	-- param X_<VENDOR>_Persistent bool notify(0,0,2) readwrite dynamic("syslog_config", "");
	-- service.syslog.option.logtofile
	[subRoot .. '.' .. xVendorPrefix .. '_Persistent'] = {
		get = function(node, name)
			return 0, is_persistent_log() and '1' or '0'
		end,
		set = function(node, name, value)
			if not value or (value ~= '1' and value ~= '0') then
				return CWMP.Error.InvalidParameterValue
			end

			local rdbName = "service.syslog.option.logtofile"
			local curVal = luardb.get(rdbName)
			if curVal ~= value then
				luardb.set(rdbName, value)
				register_cb_trigger_syslogd()
			end
			return 0
		end
	},
	-- param X_<VENDOR>_Level uint notify(0,0,2) readwrite dynamic("syslog_config", "");
	-- service.syslog.option.capturelevel
	-- ## Log capture level in number(4(Error), 5(Warning), 6(Notice), 7(Info), 8(Debug))
	[subRoot .. '.' .. xVendorPrefix .. '_Level'] = {
		get = function(node, name)
			local rdbName = "service.syslog.option.capturelevel"
			return 0, tostring(tonumber(luardb.get(rdbName)) or 6)
		end,
		set = function(node, name, value)
			local setVal = tonumber(value)
			if not setVal or setVal < 4 or setVal > 8 then
				return CWMP.Error.InvalidParameterValue
			end

			local rdbName = "service.syslog.option.capturelevel"
			local curVal = tonumber(luardb.get(rdbName))
			if setVal ~= curVal then
				luardb.set(rdbName, setVal)
				register_cb_trigger_syslogd()
			end
			return 0
		end
	}
}
