--[[
This script handles objects/parameters under Device.Ethernet.

  Interface.{i}.
  Interface.{i}.Stats.
  Link.{i}.
  Link.{i].Stats.

Currently, only eth0 is implemented.

Copyright (C) 2016 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require("Daemon")
require("Logger")
Logger.addSubsystem("EthernetDevice")

local subRoot = conf.topRoot .. '.Ethernet.'

------------------local variable----------------------------
local systemFS = '/sys/class/net/'
local iface = 'eth0'
------------------------------------------------------------

------------------local function----------------------------

-- read interface statistics from sysfs
local readStatistics = function (iface, name)
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

------------------------------------------------------------

return {
    [subRoot .. 'Interface.1.Enable'] = {
        get = function(node, name) return 0, '1' end,
        set = function(node, name, value)
            if value ~= '1' then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Value " .. value .. " is not valid"
            else
                return 0
            end
        end
    },

    [subRoot .. 'Interface.1.Status'] = {
        get = function(node, name) return 0, 'Up' end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.Name'] = {
        get = function(node, name)
            return 0, iface
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.MACAddress'] = {
        get = function(node, name)
            local ret, mac = pcall(Daemon.readStringFromFile, systemFS .. iface .. '/address')
            if not ret then mac = '' end
            return 0, mac:upper()
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Interface.1.MaxBitRate'] = {
        get = function(node, name) return 0, Daemon.readStringFromFile(systemFS .. iface .. '/speed') end,
        set = function(node, name, value)
            local result = -1
            if value == '-1' then
                result = os.execute('ethtool --change ' .. iface ..
                            ' autoneg on 2>/dev/null')
            elseif value == '1000' then
                -- For 1000Mbps, need auto negotiation and advertisement.
                -- In test Linux server PC, it takes a long time to see new speed in
                -- system file so give 5 seconds of delay.
                result = os.execute('ethtool --change ' .. iface ..
                            ' speed ' .. value .. ' duplex full autoneg on;sleep 5 2>/dev/null')
            else
                result = os.execute('ethtool --change ' .. iface ..
                            ' speed ' .. value .. ' autoneg off; sleep 5 2>/dev/null')
            end
            if result ~= 0 then
                return CWMP.Error.InvalidParameterValue,
                            "Error: Value " .. value .. " is not valid"
            end
            -- ethtool returns 0 when it failed to set speed so need to
            -- check whether the speed was changed or not.
            local speed = Daemon.readStringFromFile(systemFS .. iface .. '/speed')
            if value ~= '-1' and speed ~= value then
                return CWMP.Error.InvalidArguments,
                            "Error: Failed to set speed with value " .. value
            end
            return 0
        end
    },

    [subRoot .. 'Interface.1.DuplexMode'] = {
        get = function(node, name) return 0, string.upperWords(Daemon.readStringFromFile(systemFS .. iface .. '/duplex')) end,
        set = function(node, name, value)
            local result = -1
            if value == 'Auto' then
                result = os.execute('ethtool --change ' .. iface ..
                            ' autoneg on 2>/dev/null')
            elseif value == 'Half' or value == 'Full' then
                result = os.execute('ethtool --change ' .. iface ..
                            ' duplex ' .. string.lower(value) .. ' autoneg off 2>/dev/null')
            end
            if result ~= 0 then
                return CWMP.Error.InvalidParameterValue,
                            "Error: Value " .. value .. " is not valid"
            else
                return 0
            end
        end
    },

    [subRoot .. 'Interface.1.Stats.*'] = {
        get = function(node, name)
            if node.name == 'BytesSent' then
                return 0, readStatistics(iface, 'tx_bytes')
            end
            if node.name == 'BytesReceived' then
                return 0, readStatistics(iface, 'rx_bytes')
            end
            if node.name == 'PacketsSent' then
                return 0, readStatistics(iface, 'tx_packets')
            end
            if node.name == 'PacketsReceived' then
                return 0, readStatistics(iface, 'rx_packets')
            end
            if node.name == 'ErrorsSent' then
                return 0, readStatistics(iface, 'tx_errors')
            end
            if node.name == 'ErrorsReceived' then
                return 0, readStatistics(iface, 'rx_crc_errors')
            end
            if node.name == 'MulticastPacketsReceived' then
                return 0, readStatistics(iface, 'multicast')
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Link.1.Enable'] = {
        get = function(node, name) return 0, '1' end,
        set = function(node, name, value)
            if value ~= '1' then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Value " .. value .. " is not valid"
            else
                return 0
            end
        end
    },

    [subRoot .. 'Link.1.Status'] = {
        get = function(node, name) return 0, 'Up' end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Link.1.Name'] = {
        get = function(node, name) return 0, iface end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Link.1.MACAddress'] = {
        get = function(node, name)
            local ret, mac = pcall(Daemon.readStringFromFile, systemFS .. iface .. '/address')
            if not ret then mac = '' end
            return 0, mac:upper()
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

    [subRoot .. 'Link.1.LowerLayers'] = {
        get = function(node, name) return 0, subRoot .. 'Interface.1' end,
        set = function(node, name, value)
            if value ~= subRoot .. 'Interface.1' then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Value " .. value .. " is not valid"
            else
                return 0
            end
        end
    },

    [subRoot .. 'Link.1.Stats.*'] = {
        get = function(node, name)
            if node.name == 'BytesSent' then
                return 0, readStatistics(iface, 'tx_bytes')
            end
            if node.name == 'BytesReceived' then
                return 0, readStatistics(iface, 'rx_bytes')
            end
            if node.name == 'PacketsSent' then
                return 0, readStatistics(iface, 'tx_packets')
            end
            if node.name == 'PacketsReceived' then
                return 0, readStatistics(iface, 'rx_packets')
            end
            if node.name == 'ErrorsSent' then
                return 0, readStatistics(iface, 'tx_errors')
            end
            if node.name == 'ErrorsReceived' then
                return 0, readStatistics(iface, 'rx_crc_errors')
            end
            if node.name == 'MulticastPacketsReceived' then
                return 0, readStatistics(iface, 'multicast')
            end
        end,
        set = CWMP.Error.funcs.ReadOnly
    },

}

