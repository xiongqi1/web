--[[
This script handles Bell Canada parameters for Cellular Status and Signal Conditions under Device.Celluar.Interface.{i}.X_<VENDOR>_XXX.

  RSRP.
  RSRQ.
  CellID.
  PCI.
  CQI.
  SINR.
  ECGI.

Copyright (C) 2017 NetComm Wireless Limited.
--]]

require("CWMP.Error")
require("Logger")
local logSubsystem = 'CELLSIGSTATS'
Logger.addSubsystem(logSubsystem)
local subRoot = conf.topRoot .. '.Cellular.Interface.*'
local rdbPrefix = 'wwan.'

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

-- get the index to RDB wwan.x of a parameter node under Cellular.Interface.{i}.
local function getInterfaceIndex(node)
	local path = node:getPath()
	assert(path, 'node:getPath failed')
	pathBits = path:explode('.')
	assert(#pathBits > 1, 'node does not have parent')
	local idx = tonumber(pathBits[#pathBits - 1])
	assert(idx and idx > 0, 'invalid node index')
	return idx - 1
end

-- get cellular measurement as a list of [type,earfcn,pci,rsrp,rsrq]
local function getMeasurement(index)
	local meas = luardb.get(rdbPrefix .. index .. '.cell_measurement.0')
	if not meas then
		return nil
	end
	local measBits = meas:explode(',')
	if #measBits ~= 5 then
		return
	end
	return measBits
end

return {
	[subRoot .. '.' .. xVendorPrefix .. '_RSRP'] = {
		get = function(node, name)
			local meas = getMeasurement(getInterfaceIndex(node))
			local result = meas and meas[4] or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_RSRQ'] = {
		get = function(node, name)
			local meas = getMeasurement(getInterfaceIndex(node))
			local result = meas and meas[5] or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_PCI'] = {
		get = function(node, name)
			local meas = getMeasurement(getInterfaceIndex(node))
			local result = meas and meas[3] or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_EARFCN'] = {
		get = function(node, name)
			local meas = getMeasurement(getInterfaceIndex(node))
			local result = meas and meas[2] or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_CellID'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.system_network_status.CellID') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_CQI'] = {
		get = function(node, name)
			--[[
				This one depends on qdiagd. It relies on qdiagd implementation.
				wwan.0.rrc_session.i.avg_cqi contains average CQI and the
				current session index is stored in wwan.0.rrc_session.index.
				It takes about 10 seconds to update avg_cqi, during the 10s
				window, the corresponding RDB avg_cqi is blank. In that case,
				we should use the previous indexed value.
			--]]
			local max_rrc_sessions = 10 -- this must match qdiagd
			local prefix = rdbPrefix .. getInterfaceIndex(node) .. '.rrc_session.'
			local result = ''
			-- need to lock rdb to read index and cqi atomically
			luardb.lock()
			-- find the index for the latest updated rrc_session
			local index = tonumber(luardb.get(prefix .. 'index'))
			Logger.log(logSubsystem, 'debug', 'rrc_session.index=' .. tostring(index))
			if index then
				result = luardb.get(prefix .. index .. '.avg_cqi') or ''
				Logger.log(logSubsystem, 'debug', 'avg_cqi[' .. index .. ']=' .. result)
				if result == '' then
					-- the RDB is not yet updated, read the previous one
					index = (index - 1) % max_rrc_sessions
					result = luardb.get(prefix .. index .. '.avg_cqi') or ''
					Logger.log(logSubsystem, 'debug', 'avg_cqi[' .. index .. ']=' .. result)
				end
			end
			luardb.unlock()
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_SINR'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.signal.rssinr') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_ECGI'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.system_network_status.ECGI') or ''
			return 0, result
		end,
	};
}
