--[[
This script handles objects/parameters under Device.IP.Interface.

  {i}.
  {i}.IPv4Address.{i}.
  {i}.IPv6Address.{i}.
  {i}.Stats.

  Currently, only 6 IP interfaces are supported, i=1...6,
  corresponding to 6 Cellular AccessPoints.

  TODO: add support of other types of interfaces, such as ethernet ...

  This only works on Serpent with WMMD.

Copyright (C) 2016 NetComm Wireless Limited.
--]]

require("Daemon")
require("handlers.hdlerUtil")
require("Logger")

local ipv6_util = require("ipv6_util")

local logSubsystem = 'IpInterface'
Logger.addSubsystem(logSubsystem, 'debug') -- TODO: change level to 'notice' once debugged

local subRoot = conf.topRoot .. '.IP.'

local numAPs = 6 -- # of Cellular.AccessPoint.{i}
local numIFs = numAPs -- # of IP.Interface.{i}
local numAddrs = 1 -- # of IP.Interface.{i}.IPv4/6Address.{i}

local g_depthOfIfaceInst = 4 -- path length of Device.IP.Interface.{i}
local g_depthOfAddrInst = 6 -- path length of Device.IP.Interface.{i}.IPv4/6Address.{i}

local g_rdbPrefix_lp = 'link.profile.'

--[[
	Wrting to the following parameters need to be done post-session:
	Interface.{i}.Enable
	Interface.{i}.IPv4Enable
	Interface.{i}.IPv6Enable
	Interface.{i}.IPv4Address.{i}.Enable
	Interface.{i}.IPv6Address.{i}.Enable
--]]
local g_lp_needsync = {}
local tmp_enable_ip = {}
local tmp_enable_ipv4 = {}
local tmp_enable_ipv6 = {}
local function set_needsync(idx)
	Logger.log(logSubsystem, 'debug', 'set needsync #' .. idx)
	g_lp_needsync[idx] = true
end
-- the following two are task functions to be called pre-/post-session
local function clear_needsync()
	Logger.log(logSubsystem, 'debug', 'clear needsync')
	g_lp_needsync = {}
	tmp_enable_ip = {}
	tmp_enable_ipv4 = {}
	tmp_enable_ipv6 = {}
end
local function sync_lps()
	Logger.log(logSubsystem, 'debug', 'sync link profiles enable/pdp_type')
	for idx,_ in pairs(g_lp_needsync) do
		local enKey = g_rdbPrefix_lp .. idx .. '.enable'
		local pdpKey = g_rdbPrefix_lp .. idx .. '.pdp_type'
		local enable = luardb.get(enKey)
		local pdp_type = luardb.get(pdpKey)
		local new_pdp_type = pdp_type
		local enable_ip = tmp_enable_ip[idx]
		local enable_ipv4 = tmp_enable_ipv4[idx]
		local enable_ipv6 = tmp_enable_ipv6[idx]
		if (enable_ipv4 == '0' and enable_ipv6 == '0') or
		   (enable_ipv4 == '0' and enable_ipv6 ~= '1' and pdp_type == 'ipv4') or
		   (enable_ipv4 ~= '1' and enable_ipv6 == '0' and pdp_type == 'ipv6') then
			-- we should never set pdp_type to empty, instead set enable to '0'
			Logger.log(logSubsystem, 'debug', 'disable link profile #'..idx)
			luardb.set(enKey, '0')
		else
			if pdp_type == 'ipv6' and enable_ipv4 == '1' then
				if enable_ipv6 == '0' then
					Logger.log(logSubsystem, 'debug', '+IPv4 -IPv6')
					new_pdp_type = 'ipv4'
				else
					Logger.log(logSubsystem, 'debug', '+IPv4')
					new_pdp_type = 'ipv4v6'
				end
			elseif pdp_type == 'ipv4' and enable_ipv6 == '1' then
				if enable_ipv4 == '0' then
					Logger.log(logSubsystem, 'debug', '-IPv4 +IPv6')
					new_pdp_type = 'ipv6'
				else
					Logger.log(logSubsystem, 'debug', '+IPv6')
					new_pdp_type = 'ipv4v6'
				end
			elseif pdp_type == 'ipv4v6' then
				if enable_ipv4 == '0' then
					Logger.log(logSubsystem, 'debug', '-IPv4')
					new_pdp_type = 'ipv6'
				elseif enable_ipv6 == '0' then
					Logger.log(logSubsystem, 'debug', '-IPv6')
					new_pdp_type = 'ipv4'
				end
			end
			if new_pdp_type ~= pdp_type then
				Logger.log(logSubsystem, 'debug', 'pdp_type changed: ' .. pdp_type .. '->' .. new_pdp_type)
				luardb.set(pdpKey, new_pdp_type)
				luardb.set(g_rdbPrefix_lp .. idx .. '.writeflag', '1')
			end
			if enable_ip and enable_ip ~= enable then
				Logger.log(logSubsystem, 'debug', 'enable changed: ' .. enable .. '->' .. enable_ip)
				luardb.set(enKey, enable_ip)
			end
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

-- Task function: set the link profile trigger
local function lp_trigger(task)
	local idx = tonumber(task.data)
	if not idx or idx < 1 or idx > numIFs then
		Logger.log(logSubsystem, 'error', 'Cannot trigger invalid link profile index=' .. tostring(idx))
		return
	end
	Logger.log(logSubsystem, 'debug', 'trigger link profile #' .. idx)
	luardb.set(g_rdbPrefix_lp .. idx .. '.trigger', '1')
end

return {
	[subRoot .. 'InterfaceNumberOfEntries'] = {
		get = function(node, name)
			local coll = node.parent:getChild('Interface')
			if not coll then
				return 0, '0'
			end
			return 0, tostring(coll:countInstanceChildren())
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
			if client:isTaskQueued('preSession', clear_needsync) ~= true then
				client:addTask('preSession', clear_needsync, true)
			end
			if client:isTaskQueued('sessionDeferred', sync_lps) ~= true then
				client:addTask('sessionDeferred', sync_lps, true)
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

	[subRoot .. 'Interface.*.Enable'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			local enable = luardb.get(g_rdbPrefix_lp .. idx .. '.enable')
			if enable ~= '1' then
				enable = '0'
			end
			return 0, enable
		end,

		set = function(node, name, value)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			local rdbKey = g_rdbPrefix_lp .. idx .. '.enable'
			local enable = luardb.get(rdbKey)
			if enable ~= value then
				tmp_enable_ip[idx] = value
				set_needsync(idx) -- defer to post-session
			end
			return 0
		end
	},

	[subRoot .. 'Interface.*.Status'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			local status = luardb.get(g_rdbPrefix_lp .. idx .. '.status')
			if status == 'up' then
				status = 'Up'
			elseif status == 'down' then
				status = 'Down'
			else
				status = 'Unknown'
			end
			return 0, status
		end
	},

	[subRoot .. 'Interface.*.Name'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			local rdbKey = g_rdbPrefix_lp .. idx .. '.interface'
			local name = luardb.get(rdbKey) or ''
			if name == '' then
				name = 'rmnet_data' .. idx-1
				Logger.log(logSubsystem, 'debug', rdbKey .. ' is missing. use default ' .. name)
			end
			return 0, name
		end
	},

	[subRoot .. 'Interface.*.LowerLayers'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			-- link.profile.i.dev => wwan.x
			local rdbKey = g_rdbPrefix_lp .. idx .. '.dev'
			local dev = luardb.get(rdbKey) or ''
			local ix = tonumber(dev:match('wwan%.(%d+)'))
			if not ix then
				ix = 0
				Logger.log(logSubsystem, 'warning', rdbKey .. ' = "' .. dev .. '" is invalid. use default wwan.0')
			end
			ix = ix + 1
			local cell = node.parent.parent.parent.parent:getChild('Cellular')
			if not cell then
				return CWMP.Error.InternalError,
				    "Error: Could not find Cellular object"
			end
			local ifcoll = cell:getChild('Interface')
			if not ifcoll then
				return CWMP.Error.InternalError,
				    "Error: Could not find Cellular.Interface collection"
			end
			local iface = ifcoll:getChild(tostring(ix))
			if not iface then
				return CWMP.Error.InternalError,
				    "Error: Could not find CellularInterface " .. idx
			end
			return 0, iface:getPath()
		end,
		set = function(node, name)
			return CWMP.Error.ReadOnly, "You should not change LowerLayers of this Interface" -- we do not allow writing this field
		end
	},
	[subRoot .. 'Interface.*.Type'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			return 0, 'Normal'
		end
	},
	[subRoot .. 'Interface.*.Reset'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			return 0, '0' -- always return false as per spec
		end,
		set = function(node, name, value)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			if value == '1' then
				-- set link.profile.i.trigger post-session
				if client:isTaskQueued('sessionDeferred', lp_trigger, idx) ~= true then
					client:addTask('sessionDeferred', lp_trigger, false, idx)
				end
			end
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv4Enable'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			local pdp_type = luardb.get(g_rdbPrefix_lp .. idx .. '.pdp_type')
			-- unless it is ipv6 only, we assume IPv4 is enabled
			if pdp_type == 'ipv6' then
				return 0, '0'
			end
			return 0, '1'
		end,
		set = function(node, name, value)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			tmp_enable_ipv4[idx] = value
			local pdp_type = luardb.get(g_rdbPrefix_lp .. idx .. '.pdp_type')
			if (pdp_type == 'ipv4' or pdp_type == 'ipv4v6') and value == '0' or
			   (pdp_type == 'ipv6' and value == '1') then
				g_lp_needsync[idx] = true
			end
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv4AddressNumberOfEntries'] = {
		get = function(node, name)
			local coll = node.parent:getChild('IPv4Address')
			if not coll then
				return 0, '0'
			end
			return 0, tostring(coll:countInstanceChildren())
		end
	},

	[subRoot .. 'Interface.*.IPv6Enable'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			local pdp_type = luardb.get(g_rdbPrefix_lp .. idx .. '.pdp_type')
			-- unless it is ipv4 only, we assume IPv6 is enabled
			if pdp_type == 'ipv4' then
				return 0, '0'
			end
			return 0, '1'
		end,
		set = function(node, name, value)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			tmp_enable_ipv6[idx] = value
			local pdp_type = luardb.get(g_rdbPrefix_lp .. idx .. '.pdp_type')
			if (pdp_type == 'ipv6' or pdp_type == 'ipv4v6') and value == '0' or
			   (pdp_type == 'ipv4' and value == '1') then
				g_lp_needsync[idx] = true
			end
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv6AddressNumberOfEntries'] = {
		get = function(node, name)
			local coll = node.parent:getChild('IPv6Address')
			if not coll then
				return 0, '0'
			end
			return 0, tostring(coll:countInstanceChildren())
		end
	},

	[subRoot .. 'Interface.*.IPv6PrefixNumberOfEntries'] = {
		get = function(node, name)
			local coll = node.parent:getChild('IPv6Prefix')
			if not coll then
				return 0, '0'
			end
			return 0, tostring(coll:countInstanceChildren())
		end
	},

	[subRoot .. 'Interface.*.IPv4Address'] = {
		init = function(node, name)
			node:setAccess('readonly') -- we do not support create/delete
			for _, child in ipairs(node.children) do
				if child.name ~= '0' then
					child.parent:deleteChild(child)
				end
			end
			for i=1,numAddrs do
				local instance = node:createDefaultChild(i)
				instance:recursiveInit()
			end
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv4Address.*'] = {
		init = function(node, name)
			local pathBits = name:explode('.')
			g_depthOfAddrInst = #pathBits -- saved for getting index later
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv4Address.*.*'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end

			if node.name == 'Enable' then
				local pdp_type = luardb.get(g_rdbPrefix_lp .. idx .. '.pdp_type')
				-- unless it is ipv6 only, we assume IPv4 is enabled
				if pdp_type == 'ipv6' then
					return 0, '0'
				end
				return 0, '1'
			end

			if node.name == 'Status' then
				local status = luardb.get(g_rdbPrefix_lp .. idx .. '.status_ipv4')
				if status == 'up' then
					status = 'Enabled'
				elseif status == 'down' then
					status = 'Disabled'
				else
					status = 'Error'
				end
				return 0, status
			end

			if node.name == 'IPAddress' then
				local addr = luardb.get(g_rdbPrefix_lp .. idx .. '.iplocal') or ''
				return 0, addr
			end

			if node.name == 'SubnetMask' then
				local mask = luardb.get(g_rdbPrefix_lp .. idx .. '.mask') or ''
				return 0, mask
			end

			if node.name == 'AddressingType' then
				return 0, 'DHCP'
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

			if node.name == 'Enable' then
				return CWMP.Error.ReadOnly, "You should not change this field. Change IPv4Enable instead"
			end

			return CWMP.Error.InvalidParameterName,
			    "Error: Parameter " .. name .. " does not exist"
		end
	},

	[subRoot .. 'Interface.*.IPv6Address'] = {
		init = function(node, name)
			node:setAccess('readonly') -- we do not support create/delete
			for _, child in ipairs(node.children) do
				if child.name ~= '0' then
					child.parent:deleteChild(child)
				end
			end
			for i=1,numAddrs do
				local instance = node:createDefaultChild(i)
				instance:recursiveInit()
			end
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv6Address.*'] = {
		init = function(node, name)
			local pathBits = name:explode('.')
			g_depthOfAddrInst = #pathBits -- saved for getting index later
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv6Address.*.*'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end

			if node.name == 'Enable' then
				local pdp_type = luardb.get(g_rdbPrefix_lp .. idx .. '.pdp_type')
				-- unless it is ipv4 only, we assume IPv6 is enabled
				if pdp_type == 'ipv4' then
					return 0, '0'
				end
				return 0, '1'
			end

			if node.name == 'Status' then
				local status = luardb.get(g_rdbPrefix_lp .. idx .. '.status_ipv6')
				if status == 'up' then
					status = 'Enabled'
				elseif status == 'down' then
					status = 'Disabled'
				else
					status = 'Error'
				end
				return 0, status
			end

			if node.name == 'IPAddress' then
				local addr = luardb.get(g_rdbPrefix_lp .. idx .. '.ipv6_ipaddr') or ''
				return 0, addr and addr:explode('/')[1] or ''
			end

			if node.name == 'Origin' then
				return 0, 'AutoConfigured' -- LTE always uses SLAAC to configure UE IPv6
			end

			if node.name == 'Prefix' then
				local idx2 = getIndex(name, g_depthOfAddrInst)
				if not idx2 then
					return 0, ''
				end
				return 0, subRoot .. 'Interface.' .. idx .. '.IPv6Prefix.' .. idx2
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

			if node.name == 'Enable' then
				return CWMP.Error.ReadOnly, "You should not change this field. Change IPv6Enable instead"
			end

			return CWMP.Error.InvalidParameterName,
			    "Error: Parameter " .. name .. " does not exist"
		end
	},

	[subRoot .. 'Interface.*.IPv6Prefix'] = {
		init = function(node, name)
			node:setAccess('readonly') -- we do not support create/delete
			for _, child in ipairs(node.children) do
				if child.name ~= '0' then
					child.parent:deleteChild(child)
				end
			end
			for i=1,numAddrs do
				local instance = node:createDefaultChild(i)
				instance:recursiveInit()
			end
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv6Prefix.*'] = {
		init = function(node, name)
			local pathBits = name:explode('.')
			g_depthOfAddrInst = #pathBits -- saved for getting index later
			return 0
		end
	},

	[subRoot .. 'Interface.*.IPv6Prefix.*.*'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end

			if node.name == 'Enable' then
				local pdp_type = luardb.get(g_rdbPrefix_lp .. idx .. '.pdp_type')
				-- unless it is ipv4 only, we assume IPv6 is enabled
				if pdp_type == 'ipv4' then
					return 0, '0'
				end
				return 0, '1'
			end

			if node.name == 'Status' then
				local status = luardb.get(g_rdbPrefix_lp .. idx .. '.status_ipv6')
				if status == 'up' then
					status = 'Enabled'
				elseif status == 'down' then
					status = 'Disabled'
				else
					status = 'Error'
				end
				return 0, status
			end

			if node.name == 'Prefix' then
				local addr = luardb.get(g_rdbPrefix_lp .. idx .. '.ipv6_ipaddr') or ''
				local addrPrefix = addr:explode('/')
				local prefixLen = tonumber(addrPrefix[2])
				if prefixLen ~= 64 then
					-- SLAAC must use /64 prefix length
					return 0, ''
				end
				local expanded = ipv6_util.ipv6_addr_expand(addrPrefix[1])
				if not expanded then
					return 0, ''
				end
				local prefBits = expanded:explode(':')
				return 0, prefBits[1] .. ':' .. prefBits[2] .. ':' .. prefBits[3] .. ':' .. prefBits[4] .. '::/64'
			end

			if node.name == 'Origin' then
				return 0, 'AutoConfigured' -- LTE always uses SLAAC to configure UE IPv6
			end

			if node.name == 'OnLink' then
				return 0, '0'
			end

			if node.name == 'Autonomous' then
				return 0, '1'
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

			if node.name == 'Enable' then
				return CWMP.Error.ReadOnly, "You should not change this field. Change IPv6Enable instead"
			end

			return CWMP.Error.InvalidParameterName,
			    "Error: Parameter " .. name .. " does not exist"
		end
	},

	[subRoot .. 'Interface.*.Stats.*'] = {
		get = function(node, name)
			local idx = tonumber(getIndex(name, g_depthOfIfaceInst))
			if not idx then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " does not exist"
			end
			local fsName = dmName2fsName[node.name]
			if not fsName then
				return CWMP.Error.InvalidParameterName,
				    "Error: Parameter " .. name .. " is not supported"
			end
			local enable = luardb.get(g_rdbPrefix_lp .. idx .. '.enable')
			local status = luardb.get(g_rdbPrefix_lp .. idx .. '.status')
			local rdbKey = g_rdbPrefix_lp .. idx .. '.interface'
			local iface = luardb.get(rdbKey) or ''
			if enable == '1' and status == 'up' then
				-- only bother if link profile is enabled and up
				if iface == '' then
					iface = 'rmnet_data' .. idx-1
					Logger.log(logSubsystem, 'warning', rdbKey .. ' is missing. use default ' .. iface)
				end
				return 0, readStatistics(iface, fsName)
			end
			return 0, '0'
		end
	}
}
