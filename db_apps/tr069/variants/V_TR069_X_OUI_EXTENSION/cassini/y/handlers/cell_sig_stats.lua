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
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.servcell_info.avg_wide_band_cqi') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_SINR'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.signal.snr') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_ECGI'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.system_network_status.ECGI') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_eNodeB'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.system_network_status.eNB_ID') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_NCGI'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.cgi') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_gNodeB'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.gNB_ID') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_gNB_CellID'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.CellID') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_gNB_PCI'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.pci') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_NR_ARFCN'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.arfcn') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_gNB_SSBIndex'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.ssb_index') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_SS_RSRP'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.rsrp') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_SS_RSRQ'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.rsrq') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_SS_SINR'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.snr') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_NR_RSSI'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.rssi') or ''
			return 0, result
		end,
	};
	[subRoot .. '.' .. xVendorPrefix .. '_NR_CQI'] = {
		get = function(node, name)
			local result = luardb.get(rdbPrefix .. getInterfaceIndex(node) .. '.radio_stack.nr5g.cqi') or ''
			return 0, result
		end,
	};
}
