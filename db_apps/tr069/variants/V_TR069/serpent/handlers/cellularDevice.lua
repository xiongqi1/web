--[[
This script handles objects/parameters under Device.Cellular.

  Interface.{i}.
  Interface.{i}.USIM.
  Interface.{i}.Stats.
  AccessPoint.{i}.

  Currently, only ONE interface is supported, i=1.
  A fixed number of 6 AccessPoints are supported, i=1..6.

  This only works on Serpent with WMMD.

Copyright (C) 2016 NetComm Wireless Limited.
--]]

require("Daemon")
require("handlers.hdlerUtil")
require("Logger")

local logSubsystem = 'Cellular'
Logger.addSubsystem(logSubsystem, 'debug') -- TODO: change level to 'notice' once debugged

local subRoot = conf.topRoot .. '.Cellular.'

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local numIFs = 1 -- # of Cellular.Interface.{i}
local numAPs = 6 -- # of Cellular.AccessPoint.{i}

local g_depthOfIfaceInst = 4 -- path length of Device.Cellular.Interface.{i}
local g_depthOfApInst = 4 -- path length of Device.Cellular.AccessPoint.{i}

local g_rdbPrefix_wwan = 'wwan.'
local g_rdbPrefix_lp = 'link.profile.'

local g_lp_writeflag = {}
local g_lp_trigflag = {}
local function set_writeflag(idx)
	Logger.log(logSubsystem, 'debug', 'set writeflag #' .. idx)
	g_lp_writeflag[idx] = true
end
-- the following two are task functions to be called pre-/post-session
local function clear_writeflag()
	Logger.log(logSubsystem, 'debug', 'clear writeflag')
	g_lp_writeflag = {}
end
local function write_writeflag()
	Logger.log(logSubsystem, 'debug', 'write writeflag: ' .. table.tostring(g_lp_writeflag))
	for idx,_ in pairs(g_lp_writeflag) do
		local rdbKey = g_rdbPrefix_lp .. idx .. '.writeflag'
		Logger.log(logSubsystem, 'debug', 'write ' .. rdbKey)
		luardb.set(rdbKey, '1')
	end
end

local function set_trigflag(idx)
	Logger.log(logSubsystem, 'debug', 'set trigflag #' .. idx)
	g_lp_trigflag[idx] = true
end
-- the following two are task functions to be called pre-/post-session
local function clear_trigflag()
	Logger.log(logSubsystem, 'debug', 'clear trigflag')
	g_lp_trigflag = {}
end
local function write_trigflag()
	Logger.log(logSubsystem, 'debug', 'write trigflag: ' .. table.tostring(g_lp_trigflag))
	for idx,_ in pairs(g_lp_trigflag) do
		local rdbKey = g_rdbPrefix_lp .. idx .. '.trigger'
		Logger.log(logSubsystem, 'debug', 'write ' .. rdbKey)
		luardb.set(rdbKey, '1')
	end
end

local g_lp_rdb_q = {}
local function save_rdb_param(var, val)
	Logger.log(logSubsystem, 'debug', 'save rdb queue : ' .. var .. ' ' .. val)
	local temp = {}
	temp['name'] = var
	temp['val'] = val
	table.insert(g_lp_rdb_q, temp)
end
-- the following two are task functions to be called pre-/post-session
local function clear_rdb_queue()
	Logger.log(logSubsystem, 'debug', 'clear rdb queue')
	g_lp_rdb_q = {}
end
local function write_delayed_rdb_values()
	Logger.log(logSubsystem, 'debug', 'write delayed rdb queue')
	for idx, rdbStr in ipairs(g_lp_rdb_q) do
		Logger.log(logSubsystem, 'debug', 'write delayed rdb [' .. idx .. '] ' .. rdbStr['name'] .. ' ' .. rdbStr['val'])
		luardb.set(rdbStr['name'], rdbStr['val'])
	end
end

local g_lp_last_val = {}
local function clear_last_val()
	Logger.log(logSubsystem, 'debug', 'clear last rdb value list')
	g_lp_last_val = {}
end

-- Save to RDB if the value is changed in same session.
-- If it is a first assignment then compare with current RDB value
-- otherwise compare with last saved value for the case when there are
-- multiple assignment for same RDB variable in same session.
-- Set write flag or trigger flag depending on RDB name respectively.
local function save_if_changed(param, idx, newVal, flag)
	local rdbKey = g_rdbPrefix_lp .. idx .. param
	local curr_val = g_lp_last_val[rdbKey] or luardb.get(rdbKey)
	if curr_val ~= newVal then
		g_lp_last_val[rdbKey] = newVal
		save_rdb_param(rdbKey, newVal)
		if flag == 'write' then
			set_writeflag(idx)
		elseif flag == 'trig' then
			set_trigflag(idx)
		end
	end
end

-- get the part of dot-separated name at depth
local function getIndex(name, depth)
	local pathBits = name:explode('.')
	return pathBits[depth]
end

-- read wwan interface statistics from fs /sys/class/net
local systemFS = '/sys/class/net/'
local function readStatistics(iface, name)
	local filename = systemFS .. iface .. '/statistics/' .. name
	local defaultV = "0"

	if hdlerUtil.IsRegularFile(filename) then
		local result = Daemon.readStringFromFile(filename) or defaultV
		if not tonumber(result) then
			result = defaultV
		end
		return result
	end
	return defaultV
end

-- data model name to file system name mapping
local dmName2fsName = {
	['BytesSent']       = 'tx_bytes',
	['BytesReceived']   = 'rx_bytes',
	['PacketsSent']     = 'tx_packets',
	['PacketsReceived'] = 'rx_packets',
	['ErrorsSent']      = 'tx_errors',
	['ErrorsReceived']  = 'rx_errors'
}

-- get the named statistics summed over multiple interfaces
local function sumStatistics(ifaces, name)
	local sum = 0
	Logger.log(logSubsystem, 'debug', name .. ' of ifaces ' .. table.concat(ifaces, ' '))
	for _, iface in ipairs(ifaces) do
		sum = sum + readStatistics(iface, name)
	end
	return tostring(sum)
end

local ExtendedCellularHandler = {
	get = function(node, name)
		local idx = tonumber(getIndex(name, g_depthOfApInst))
		if not idx then
			return CWMP.Error.InvalidParameterName,
				"Error: Parameter " .. name .. " does not exist"
		end

		if node.name == 'ProfileName' then
			local profile_name = luardb.get(g_rdbPrefix_lp .. idx .. '.name') or ''
			return 0, profile_name
		end

		if node.name == 'AuthenticationType' then
			local auth_type = luardb.get(g_rdbPrefix_lp .. idx .. '.auth_type') or ''
			return 0, auth_type
		end

		if node.name == 'PdpType' then
			local pdp_type = luardb.get(g_rdbPrefix_lp .. idx .. '.pdp_type') or ''
			return 0, pdp_type
		end

		if node.name == 'ReconnectDelay' then
			local delay = luardb.get(g_rdbPrefix_lp .. idx .. '.reconnect_delay') or ''
			return 0, delay
		end

		if node.name == 'DefaultRouteMetric' then
			local metric = luardb.get(g_rdbPrefix_lp .. idx .. '.defaultroutemetric') or ''
			return 0, metric
		end

		if node.name == 'Mtu' then
			local mtu = luardb.get(g_rdbPrefix_lp .. idx .. '.mtu') or ''
			return 0, mtu
		end

		if node.name == 'NatEnable' then
			local enable = luardb.get(g_rdbPrefix_lp .. idx .. '.snat')
			if enable ~= '1' then
				enable = '0'
			end
			return 0, enable
		end

		if node.name == 'ProfileRouting' then
			local routes = luardb.get(g_rdbPrefix_lp .. idx .. '.routes') or ''
			return 0, routes
		end

		return CWMP.Error.InvalidParameterName,
			"Error: Parameter " .. name .. " does not exist"
	end,
	set = function(node, name, value)
		local idx = tonumber(getIndex(name, g_depthOfApInst))
		if not idx then
			return CWMP.Error.InvalidParameterName,
				"Error: Parameter " .. name .. " does not exist"
		end

		Logger.log(logSubsystem, 'debug', 'set AP#' .. idx .. ' ' .. node.name .. '=' .. value)

		if node.name == 'ProfileName' then
			save_if_changed('.name', idx, value, 'noflag')
			return 0
		end

		if node.name == 'AuthenticationType' then
			local rdbKey = g_rdbPrefix_lp .. idx .. '.auth_type'
			local auth_type = luardb.get(rdbKey)
			if value ~= "chap" and value ~= "pap" then
				return CWMP.Error.InvalidParameterValue
			end
			save_if_changed('.auth_type', idx, value, 'write')
			return 0
		end

		if node.name == 'PdpType' then
			local rdbKey = g_rdbPrefix_lp .. idx .. '.pdp_type'
			local pdp_type = luardb.get(rdbKey)
			if value ~= "ipv4" and value ~= "ipv6" and value ~= "ipv4v6" then
				return CWMP.Error.InvalidParameterValue
			end
			save_if_changed('.pdp_type', idx, value, 'write')
			return 0
		end

		if node.name == 'ReconnectDelay' then
			local rdbKey = g_rdbPrefix_lp .. idx .. '.reconnect_delay'
			local delay = luardb.get(rdbKey)
			new_delay = tonumber(value)
			if new_delay < 30 or new_delay > 65535 then
				return CWMP.Error.InvalidParameterValue
			end
			save_if_changed('.reconnect_delay', idx, value, 'trig')
			return 0
		end

		if node.name == 'DefaultRouteMetric' then
			local rdbKey = g_rdbPrefix_lp .. idx .. '.defaultroutemetric'
			local metric = luardb.get(rdbKey)
			new_metric = tonumber(value)
			if new_metric < 0 or new_metric > 65535 then
				return CWMP.Error.InvalidParameterValue
			end
			save_if_changed('.defaultroutemetric', idx, value, 'trig')
			return 0
		end

		if node.name == 'Mtu' then
			local rdbKey = g_rdbPrefix_lp .. idx .. '.mtu'
			local mtu = luardb.get(rdbKey)
			new_mtu = tonumber(value)
			if new_mtu < 1 or new_mtu > 2000 then
				return CWMP.Error.InvalidParameterValue
			end
			save_if_changed('.mtu', idx, value, 'trig')
			return 0
		end

		if node.name == 'NatEnable' then
			save_if_changed('.snat', idx, value, 'trig')
			return 0
		end

		if node.name == 'ProfileRouting' then
			local rdbKey = g_rdbPrefix_lp .. idx .. '.routes'
			local routes = luardb.get(rdbKey)
			local new_routes = value:explode('/')
			if not new_routes or #new_routes ~= 2 then
				return CWMP.Error.InvalidParameterValue, "Invalid Format"
			end
			if not Parameter.Validator.isValidIP4(new_routes[1]) or not Parameter.Validator.isValidIP4Netmask(new_routes[2]) then
				return CWMP.Error.InvalidParameterValue, "Invalid Format"
			end
			save_if_changed('.routes', idx, value, 'trig')
			return 0
		end

		return CWMP.Error.InvalidParameterName,
			"Error: Parameter " .. name .. " does not exist"
	end
}

return {
	[subRoot .. 'RoamingStatus'] = {
		get = function(node, name)
			local roaming = luardb.get('wwan.0.system_network_status.roaming')
			return 0, roaming == 'active' and 'Roaming' or 'Home'
		end
	},

	[subRoot .. 'Interface'] = {
		init = function(node, name)
			node:setAccess('readonly') -- we do not support create/delete
			for _, child in ipairs(node.children) do
				if child.name ~= '0' then
					child.parent:deleteChild(child)
				end
			end
			for i=1,numIFs do
				local instance = node:createDefaultChild(i)
				instance:recursiveInit()
			end
			return 0
		end
	},

	[subRoot .. 'Interface.*'] = {
		init = function(node, name)
			local pathBits = name:explode('.')
			g_depthOfIfaceInst = #pathBits -- saved for getting index later
			return 0
		end
	},

	[subRoot .. 'Interface.*.*'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			idx = idx - 1

			if node.name == 'Enable' then
				local enable = luardb.get(g_rdbPrefix_wwan .. idx .. '.enable')
				if enable ~= '0' then
					enable = '1' -- wwan.x is enabled by default
				end
				return 0, enable
			end

			if node.name == 'Status' then
				local status = luardb.get(g_rdbPrefix_wwan .. idx .. '.operating_mode')
				if status ~= 'online' then
					return 0, 'Down'
				else
					return 0, 'Up'
				end
			end

			if node.name == 'Name' then
				local rdbKey = g_rdbPrefix_wwan .. idx .. '.dev'
				local name = luardb.get(rdbKey) or ''
				if name == '' then
					name = 'rmnet'
					Logger.log(logSubsystem, 'debug', rdbKey .. ' is missing. use default ' .. name)
				end
				return 0, name
			end

			if node.name == 'IMEI' then
				local imei = luardb.get(g_rdbPrefix_wwan .. idx .. '.imei') or ''
				return 0, imei
			end

			if node.name == 'CurrentAccessTechnology' then
				local rat = luardb.get(g_rdbPrefix_wwan .. idx .. '.system_network_status.service_type') or ''
				if rat == 'None (no service)' then
					rat = ''
				elseif rat == 'CDMA2000 1X' then
					rat = 'CDMA2000OneX'
				elseif rat == 'CDMA2000 HRPD (1xEV-DO)' then
					rat = 'CDMA2000HRPD'
				elseif rat == 'GSM' then
					rat = 'GPRS'
				end
				return 0, rat
			end

			if node.name == 'NetworkInUse' then
				local network = luardb.get(g_rdbPrefix_wwan .. idx .. '.system_network_status.network') or ''
				if network == '' then
					network = luardb.get(g_rdbPrefix_wwan .. idx .. '.system_network_status.PLMN') or ''
				end
				return 0, network
			end

			if node.name == 'RSSI' then
				local rssi = tonumber(luardb.get(g_rdbPrefix_wwan .. idx .. '.signal.rssi'))
				local rat = luardb.get(g_rdbPrefix_wwan .. idx .. '.system_network_status.service_type') or ''
				local lo, up = -117, -25 -- lower/upper limit for rssi - LTE
				if rat == 'GSM' then
					lo = -111; up = -49
				elseif rat == 'UMTS' then
					lo = -117; up = -54
				end

				if not rssi then
					rssi = lo
				elseif rssi > up then
					rssi = up
				elseif rssi < lo then
					rssi = lo
				end
				return 0, tostring(rssi)
			end

			return CWMP.Error.InvalidParameterName,
			    "Error: Parameter " .. name .. " does not exist"
		end,

		set = function(node, name, value)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			idx = idx - 1

			if node.name == 'Enable' then
				local rdbKey = g_rdbPrefix_wwan .. idx .. '.enable'
				local enable = luardb.get(rdbKey)
				if enable ~= value then
					luardb.set(rdbKey, value)
				end
				return 0
			end

			return CWMP.Error.InvalidParameterName,
			    "Error: Parameter " .. name .. " does not exist"
		end
	},

	[subRoot .. 'Interface.*.USIM.*'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			idx = idx - 1

			if node.name == 'Status' then
				local status = luardb.get(g_rdbPrefix_wwan .. idx .. '.sim.status.status') or ''
				--[[ wmmd: .sim.status.status
					SIM BUSY|SIM PIN|SIM PUK|SIM ERR|SIM BLOCKED|SIM OK|SIM not inserted
				--]]
				if status == 'SIM not inserted' then
					status = 'None'
				elseif status == 'SIM OK' then
					status = 'Valid'
				elseif status == 'SIM PIN' then
					status = 'Available'
				elseif status == 'SIM PUK' or status == 'SIM BLOCKED' then
					status = 'Blocked'
				else
					status = 'Error'
				end
				return 0, status
			end

			if node.name == 'MSISDN' then
				local msisdn = luardb.get(g_rdbPrefix_wwan .. idx .. '.sim.data.msisdn') or ''
				return 0, msisdn
			end

			if node.name == 'IMSI' then
				local imsi = luardb.get(g_rdbPrefix_wwan .. idx .. '.imsi.msin') or ''
				return 0, imsi
			end

			if node.name == 'ICCID' then
				local iccid = luardb.get(g_rdbPrefix_wwan .. idx .. '.system_network_status.simICCID') or ''
				return 0, iccid
			end

			return CWMP.Error.InvalidParameterName,
			    "Error: Parameter " .. name .. " does not exist"
		end
	},

	[subRoot .. 'Interface.*.Stats.*'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if idx ~= 1 then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end

			local fsName = dmName2fsName[node.name]
			if not fsName then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " is not supported"
			end
			local ifaces = {}
			for i=1,numAPs do
				local enable = luardb.get(g_rdbPrefix_lp .. i .. '.enable')
				local status = luardb.get(g_rdbPrefix_lp .. i .. '.status')
				local rdbKey = g_rdbPrefix_lp .. i .. '.interface'
				local iface = luardb.get(rdbKey) or ''
				if enable == '1' and status == 'up' then
					if iface == '' then
						iface = 'rmnet_data' .. i-1
						Logger.log(logSubsystem, 'warning', rdbKey .. ' is missing. use default ' .. iface)
					end
					table.insert(ifaces, iface)
				end
			end
			return 0, sumStatistics(ifaces, fsName)
		end
	},

	[subRoot .. 'AccessPoint'] = {
		init = function(node, name)
			node:setAccess('readonly') -- we do not support create/delete
			for _, child in ipairs(node.children) do
				if child.name ~= '0' then
					child.parent:deleteChild(child)
				end
			end
			for i=1,numAPs do
				local instance = node:createDefaultChild(i)
				instance:recursiveInit()
			end
			if client:isTaskQueued('preSession', clear_last_val) ~= true then
				client:addTask('preSession', clear_last_val, true)
			end
			--[[ Whenever changing profile setting RDB variales which may affect the connection to ACS server,
			     save the RDB variable and value to a queue then write them after current session is completed.
			--]]
			if client:isTaskQueued('preSession', clear_rdb_queue) ~= true then
				client:addTask('preSession', clear_rdb_queue, true)
			end
			if client:isTaskQueued('sessionDeferred', write_delayed_rdb_values) ~= true then
				client:addTask('sessionDeferred', write_delayed_rdb_values, true)
			end
			--[[ Whenever APN settings need to be changed, writeflag has to be written for the changes to take effects.
				This is deferred so that changes to multiple parameters do not incur multiple writeflag writes.
			--]]
			if client:isTaskQueued('preSession', clear_writeflag) ~= true then
				client:addTask('preSession', clear_writeflag, true)
			end
			if client:isTaskQueued('sessionDeferred', write_writeflag) ~= true then
				client:addTask('sessionDeferred', write_writeflag, true)
			end
			--[[ Whenever some APN settings need to be changed, trigflag has to be written for the changes to take effects.
				This is deferred so that changes to multiple parameters do not incur multiple trigflag writes.
			--]]
			if client:isTaskQueued('preSession', clear_trigflag) ~= true then
				client:addTask('preSession', clear_trigflag, true)
			end
			if client:isTaskQueued('sessionDeferred', write_trigflag) ~= true then
				client:addTask('sessionDeferred', write_trigflag, true)
			end

			return 0
		end
	},

	[subRoot .. 'AccessPoint.*'] = {
		init = function(node, name)
			local pathBits = name:explode('.')
			g_depthOfApInst = #pathBits
			return 0
		end
	},

	[subRoot .. 'AccessPoint.*.*'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfApInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end

			if node.name == 'Enable' then
				local enable = luardb.get(g_rdbPrefix_lp .. idx .. '.enable')
				if enable ~= '1' then
					enable = '0'
				end
				return 0, enable
			end

			if node.name == 'Interface' then
				-- link.profile.i.dev => wwan.x
				local dev = luardb.get(g_rdbPrefix_lp .. idx .. '.dev') or ''
				local ix = tonumber(dev:match('wwan%.(%d+)'))
				if not ix then
					ix = 0
				end
				ix = ix + 1
				local ifcoll = node.parent.parent.parent:getChild('Interface')
				if not ifcoll then
					return CWMP.Error.InternalError,
					    "Error: Could not find Interface collection"
				end
				local iface = ifcoll:getChild(tostring(ix))
				if not iface then
					return CWMP.Error.InternalError,
					    "Error: Could not find Interface " .. idx
				end
				return 0, iface:getPath()
			end

			if node.name == 'APN' then
				local apn = luardb.get(g_rdbPrefix_lp .. idx .. '.apn') or ''
				return 0, apn
			end

			if node.name == 'Username' then
				local user = luardb.get(g_rdbPrefix_lp .. idx .. '.user') or ''
				return 0, user
			end

			if node.name == 'Password' then
				local pass = luardb.get(g_rdbPrefix_lp .. idx .. '.pass') or ''
				return 0, pass
			end

			return CWMP.Error.InvalidParameterName,
			    "Error: Parameter " .. name .. " does not exist"
		end,

		set = function(node, name, value)
			local idx = tonumber(getIndex(name, g_depthOfApInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end

			Logger.log(logSubsystem, 'debug', 'set AP#' .. idx .. ' ' .. node.name .. '=' .. value)
			if node.name == 'Enable' then
				save_if_changed('.enable', idx, value, 'noflag')
				return 0
			end

			if node.name == 'Interface' then
				return CWMP.Error.ReadOnly, "You should not change AccessPoint's Interface field" -- we do not allow writing
			end

			if node.name == 'APN' then
				save_if_changed('.apn', idx, value, 'write')
				return 0
			end

			if node.name == 'Username' then
				-- Prohibit profile2 (IMS) and profile3(SOS) setting except for APN name
				if idx == 2 or idx == 3 then
					if conf.lockImsSosProfile == true then
						return CWMP.Error.ReadOnly, "You should not change AccessPoint's username for profile 2 and 3." -- we do not allow writing
					end
				end
				save_if_changed('.user', idx, value, 'write')
				return 0
			end

			if node.name == 'Password' then
				-- Prohibit profile2 (IMS) and profile3(SOS) setting except for APN name
				if idx == 2 or idx == 3 then
					if conf.lockImsSosProfile == true then
						return CWMP.Error.ReadOnly, "You should not change AccessPoint's password for profile 2 and 3." -- we do not allow writing
					end
				end
				save_if_changed('.pass', idx, value, 'write')
				return 0
			end

			return CWMP.Error.InvalidParameterName,
			    "Error: Parameter " .. name .. " does not exist"
		end
	},
	[subRoot .. 'AccessPoint.*.' .. xVendorPrefix .. '.*'] = ExtendedCellularHandler,
}
