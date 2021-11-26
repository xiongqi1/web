--[[
This script handles objects/parameters under Device.IP.Interface.

  {i}.
  {i}.IPv4Address.{i}.
  {i}.IPv6Address.{i}.
  {i}.Stats.

  Currently, only 1 IP interface is supported
  i.e., Management vlan interface ptm0.<vlan>

Copyright (C) 2016 NetComm Wireless Limited.
--]]

require("Daemon")
require("handlers.hdlerUtil")
require("Logger")

local logSubsystem = 'IpInterface'
Logger.addSubsystem(logSubsystem, 'debug') -- TODO: change level to 'notice' once debugged

local subRoot = conf.topRoot .. '.IP.'

local numIFs = 1 -- # of IP.Interface.{i}
local numAddrs = 1 -- # of IP.Interface.{i}.IPv4/6Address.{i}
local depthOfIfaceInst = 4 -- path length of Device.IP.Interface.{i}

local rdbVarPrefix = 'ip.intf.'
local debugIp = '192.168.1.1'

local boolValidValues = { ['0'] = true, ['1'] = true }
local statusValues = { ['1'] = 'Up', ['0'] = 'Down' }

local mgmtVlan = hdlerUtil.getVVariableValue('V_MANAGEMENT_VID')
Logger.log(logSubsystem, 'debug', 'mgmtVlan retrieved: ' .. mgmtVlan or '')

local interfaceNames = { [1] = 'ptm0.' .. mgmtVlan, [2] = 'eth0_1' }

--[[
    Writing to the following parameters need to be done post-session:
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

local function clear_needsync()
	g_lp_needsync = {}
	tmp_enable_ip = {}
	tmp_enable_ipv4 = {}
	tmp_enable_ipv6 = {}
end

-- Syncs all the enables under "Device.IP"
-- i.e., enable, ipv4enable and ipv6enable according to standards.
-- This function will be called post-session
local function sync_enable()
    for idx,_ in pairs(g_lp_needsync) do
        local rdb_enable_ip = rdbVarPrefix .. idx .. '.enable'
        local rdb_enable_ipv4 = rdbVarPrefix .. idx .. '.enable_ipv4'
        local rdb_enable_ipv6 = rdbVarPrefix .. idx .. '.enable_ipv6'
        local enable_ip = luardb.get(rdb_enable_ip)
        local enable_ipv4 = luardb.get(rdb_enable_ipv4)
        local enable_ipv6 = luardb.get(rdb_enable_ipv6)
        local iface = interfaceNames[idx]

        Logger.log(logSubsystem, 'debug', 'enable_ip:' .. enable_ip ..
            ' enable_ipv4:' .. enable_ipv4 .. ' enable_ipv6:' .. enable_ipv6)
        if (enable_ip == '1' and (enable_ipv4 == '1' or enable_ipv6 == '1')) then
            -- First enable the interface, since it could have been disable before
            Daemon.readCommandOutput('ifconfig ' .. iface .. ' up')
            local cmd_output = Daemon.readCommandOutput("ifconfig " .. interfaceNames[idx])
            local addr = cmd_output:match("inet addr:(%d+%.%d+%.%d+%.%d+)")
            Logger.log(logSubsystem, 'debug', 'ip addr retrieved: ' .. addr)
            if (enable_ipv4 == '1') then
                -- For DSL port ptm0.<vlan> the IP address should get assigned from DHCP
                -- so we set the rdb variable and wait.
            else
                local cmd='ip addr del ' .. addr .. '/24 dev ' .. interfaceNames[idx]
                Logger.log(logSubsystem, 'debug', 'ip addr del cmd: ' .. cmd)
                Daemon.readCommandOutput(cmd)
            end
            if (enable_ipv6 == '1') then
                Daemon.readCommandOutput('sysctl -w net.ipv6.conf.' .. interfaceNames[idx] .. '.disable_ipv6=0')
            else
                Daemon.readCommandOutput('sysctl -w net.ipv6.conf.' .. interfaceNames[idx] .. '.disable_ipv6=1')
            end
        else
            Daemon.readCommandOutput('ifconfig ' .. iface .. ' down')
        end
        -- clear the global variables so that next session can use them
        clear_needsync()
    end
end

-- get the part of dot-separated name at depth
local function getIndex(name, depth)
    local pathBits = name:explode('.')
    return pathBits[depth]
end

-- read interface statistics from fs /sys/class/net
local systemFS = '/sys/class/net/'
local function readStatistics(iface, name)
	-- Strip out the vlan part of the interface, since statistics don't get updated for vlan interface properly
	local iface_base = iface:match('(%S+)%.')
	Logger.log(logSubsystem, 'error', "Stripping vlan interface " .. iface .. " to " .. iface_base)
	local filename = systemFS .. iface_base .. '/statistics/' .. name
	local defaultV = "0"

	if hdlerUtil.IsRegularFile(filename) then
		local result = Daemon.readIntFromFile(filename) or defaultV
		return tostring(result)
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

-- Task function: Perform a teardown/bring up of the interface
local function resetInterface(task)
    local idx = tonumber(task.data)
    if not idx or idx < 1 or idx > numIFs then
        Logger.log(logSubsystem, 'error', 'Cannot reset invalid interface index:' .. tostring(idx) .. 'numOfIFs: ' .. numIFs)
        return
        end
    Logger.log(logSubsystem, 'debug', 'teardown/bringup interface ' .. interfaceNames[idx])
    -- @TODO: Not sure if ip should be flushed and wait for the next dhcp assignment.
    -- Right now doing only a config down/up here.
    Daemon.readCommandOutput('ifconfig ' .. interfaceNames[idx] .. ' down')
    Daemon.readCommandOutput('ifconfig ' .. interfaceNames[idx] .. ' up')
end

return {
    [subRoot .. 'Interface.*.Enable'] = {
        init = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            local rdb_val = luardb.get(rdbVarPrefix .. idx .. '.enable')
            if rdb_val == nil then
                -- Enable by default
                luardb.set(rdbVarPrefix .. idx .. '.enable', '1', 'p')
            end
            g_lp_needsync[idx] = true -- defer to post-session
            -- This is a persistent task runs after every session to see if any
            -- changes done on the enable fields i.e., if g_lp_needsync is set
            -- @TODO: Enable this if needed
            --if client:isTaskQueued('sessionDeferred', sync_enable) ~= true then
            --    client:addTask('sessionDeferred', sync_enable, true)
            --end
            return 0
        end,
        get = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
	    return 0, luardb.get(rdbVarPrefix .. idx .. '.enable')
        end,
        set = function(node, name, value)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            if not boolValidValues[value] then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Invalid parameter value " .. value .. " for node "
                    .. name
            end
            if idx == 1 and value == '0' then
                return CWMP.Error.ReadOnly, "Cannot disable this interface"
            end
            luardb.set(rdbVarPrefix .. idx .. '.enable', value, 'p')
            g_lp_needsync[idx] = true -- defer to post-session
            return 0
        end
    },

    [subRoot .. 'Interface.*.Status'] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            local iface = interfaceNames[idx]
            local status=Daemon.readStringFromFile(systemFS .. iface .. '/operstate')
            Logger.log(logSubsystem, 'debug', 'idx: ' .. idx .. ' interface name: '
                .. iface .. 'status received: ' .. status)
            return 0, status
        end
    },

    [subRoot .. 'Interface.*.Name'] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            return 0, interfaceNames[idx]
        end
    },

    [subRoot .. 'Interface.*.Type'] = {
        get = function(node, name)
            return 0, 'Normal'
        end
    },

    [subRoot .. 'Interface.*.Reset'] = {
        get = function(node, name)
            return 0, '0' -- always return false as per spec
        end,
        set = function(node, name, value)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            if value == '1' then
                -- Perform a teardown/bring up of the interface post-session
                -- This is not a persistent task, runs only once post this session
                if client:isTaskQueued('sessionDeferred', resetInterface, idx) ~= true then
                    client:addTask('sessionDeferred', resetInterface, false, idx)
                end
            end
            return 0
        end
    },

    [subRoot .. 'Interface.*.IPv4Enable'] = {
        init = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            local rdb_val = luardb.get(rdbVarPrefix .. idx .. '.enable_ipv4')
            if rdb_val == nil then
                -- Enable by default
                luardb.set(rdbVarPrefix .. idx .. '.enable_ipv4', '1', 'p')
            end
            g_lp_needsync[idx] = true -- defer to post-session
            return 0
        end,
        get = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
	    return 0, luardb.get(rdbVarPrefix .. idx .. '.enable_ipv4')
        end,
        set = function(node, name, value)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            if not boolValidValues[value] then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Invalid parameter value " .. value .. " for node "
                    .. name
            end
            if idx == 1 and value == '0' then
                return CWMP.Error.ReadOnly, "Cannot disable ipv4 for this interface"
            end
            luardb.set(rdbVarPrefix .. idx .. '.enable_ipv4', value, 'p')
            g_lp_needsync[idx] = true
            return 0
        end
    },

    [subRoot .. 'Interface.*.IPv6Enable'] = {
        init = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            local rdb_val = luardb.get(rdbVarPrefix .. idx .. '.enable_ipv6')
            if rdb_val == nil then
                -- Enable by default
                luardb.set(rdbVarPrefix .. idx .. '.enable_ipv6', '1', 'p')
            end
            g_lp_needsync[idx] = true -- defer to post-session
            return 0
        end,
        get = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            return 0, luardb.get(rdbVarPrefix .. idx .. '.enable_ipv6')
        end,
        set = function(node, name, value)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            if not boolValidValues[value] then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Invalid parameter value " .. value .. " for node "
                    .. name
            end
            if idx == 1 and value == '0' then
                return CWMP.Error.ReadOnly, "Cannot disable ipv6 for this interface"
            end
            luardb.set(rdbVarPrefix .. idx .. '.enable_ipv6', value, 'p')
            g_lp_needsync[idx] = true
            return 0
        end
    },

    [subRoot .. 'Interface.*.IPv4Address.*.*'] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            local iface = interfaceNames[idx]

            if node.name == 'Enable' then
                return 0, luardb.get(rdbVarPrefix .. idx .. '.enable_ipv4')
            end

            if node.name == 'Status' then
                local cmd_output = Daemon.readCommandOutput("ifconfig " .. interfaceNames[idx])
                local addr = cmd_output:match("inet addr:(%d+%.%d+%.%d+%.%d+)") or ''
                Logger.log(logSubsystem, 'debug', 'ip addr received: ' .. addr)

                status=Daemon.readStringFromFile('/sys/class/net/' .. iface .. '/operstate')
                Logger.log(logSubsystem, 'debug', 'status received: ' .. status)
                if status == 'up' and addr ~= '' then
                    return 0, 'Enabled'
                else
                    return 0, 'Disabled'
                end
            end

            if node.name == 'IPAddress' then
                local cmd_output = Daemon.readCommandOutput("ifconfig " .. interfaceNames[idx])
                local addr = cmd_output:match("inet addr:(%d+%.%d+%.%d+%.%d+)") or ''
                Logger.log(logSubsystem, 'debug', 'ip addr received: ' .. addr)
                return 0, addr
            end

            if node.name == 'SubnetMask' then
                local cmd_output = Daemon.readCommandOutput("ifconfig " .. interfaceNames[idx])
                local mask = cmd_output:match("Mask:(%d+%.%d+%.%d+%.%d+)") or ''
                Logger.log(logSubsystem, 'debug', 'Mask received: ' .. mask)
                return 0, mask
            end

            if node.name == 'AddressingType' then
                if idx == 1 then
                    return 0, 'DHCP'
                else
                    return 0, 'Static'
                end
            end
        end,
        set = function(node, name, value)
            if node.name == 'Enable' then
                return CWMP.Error.ReadOnly, "You should not change this field. Change IPv4Enable instead"
            end

            return CWMP.Error.InvalidParameterName,
                "Error: Parameter " .. name .. " does not exist"
        end
    },

    [subRoot .. 'Interface.*.IPv6Address.*.*'] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            local iface = interfaceNames[idx]

            if node.name == 'Enable' then
                return 0, luardb.get(rdbVarPrefix .. idx .. '.enable_ipv6')
            end

            if node.name == 'Status' then
                local cmd_output = Daemon.readCommandOutput("ifconfig " .. interfaceNames[idx])
                local addr = cmd_output:match("inet6 addr:%s+(%S+)%s+Scope:Global") or ''
                Logger.log(logSubsystem, 'debug', 'ip addr received: ' .. addr)

                status=Daemon.readStringFromFile('/sys/class/net/' .. iface .. '/operstate')
                Logger.log(logSubsystem, 'debug', 'status received: ' .. status)
                if status == 'up' and addr ~= '' then
                    return 0, 'Enabled'
                else
                    return 0, 'Disabled'
                end
            end

            if node.name == 'IPAddress' then
                local cmd_output = Daemon.readCommandOutput("ifconfig " .. interfaceNames[idx])
                local addr = cmd_output:match("inet6 addr:%s+(%S+)%s+Scope:Global") or ''
                Logger.log(logSubsystem, 'debug', 'ip addr received: ' .. addr)
                return 0, addr
            end

            if node.name == 'Origin' then
                return 0, 'DHCPv6'
            end

            if node.name == 'Prefix' then
                return 0, ''
            end
        end,
        set = function(node, name, value)
            if node.name == 'Enable' then
                return CWMP.Error.ReadOnly, "You should not change this field. Change IPv6Enable instead"
            end

            return CWMP.Error.InvalidParameterName,
                "Error: Parameter " .. name .. " does not exist"
        end
    },

    [subRoot .. 'Interface.*.Stats.*'] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, depthOfIfaceInst))
            if not idx then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " does not exist"
            end
            local iface = interfaceNames[idx]

            local fsName = dmName2fsName[node.name]
            if not fsName then
                return CWMP.Error.InvalidParameterName,
                    "Error: Parameter " .. name .. " is not supported"
            end

            local enable = luardb.get(rdbVarPrefix .. idx .. '.enable')
            Logger.log(logSubsystem, 'debug', 'In stats retreival, enable: ' .. tostring(enable))
            if enable == '1' then
                -- only bother if interface is enabled and up
                return 0, readStatistics(iface, fsName)
            end
            return 0, '0'
        end
    }
}
