--[[
This script handles UpTime parameters under

  Device.Cellular.Interface.{i}.X_OUI_UpTime (physical layer)
  Device.IP.Interface.{i}.X_OUI_UpTime (IP layer - overall)
  Device.IP.Interface.{i}.IPv4Address.{i}.X_OUI_UpTime (IP layer - IPv4)
  Device.IP.Interface.{i}.IPv6Address.{i}.X_OUI_UpTime (IP layer - IPv6)

Copyright (C) 2020 Casa Systems. Inc.
--]]

require("Daemon")
require("handlers.hdlerUtil")

--[[
require("Logger")
local logSubsystem = "Cellular_UpTime"
Logger.addSubsystem(logSubsystem, "debug") -- TODO: change level to 'notice' once debugged
--]]

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot_Cellular = conf.topRoot .. ".Cellular.Interface.*."
local subRoot_IP = conf.topRoot .. ".IP.Interface.*."

local g_rdbPrefix_wwan = "wwan."
local g_rdbPrefix_lpo = "link.policy."

local g_curr_uptime = nil
local g_depth_idx = 4

-- get index of instance.
local function getIndex(name, depth)
    local pathBits = name:explode('.')
    local idx = pathBits[depth]

    return idx
end

local function clear_sysuptime()
    g_curr_uptime = nil
end

local function get_sysuptime()
    if not g_curr_uptime then
        g_curr_uptime = tonumber(Daemon.readIntFromFile("/proc/uptime"))
        if client:isTaskQueued("cleanUp", clear_sysuptime) == false then
            client:addTask("cleanUp", clear_sysuptime)
        end
    end
    return g_curr_uptime
end

-- get uptime from given epoch.
--
-- rdb_name: name of rdb variable that has uptime epoch.
-- return: value of calculated uptime in string
local function get_param_uptime(rdb_name)
    local uptime_curr = get_sysuptime()
    local uptime_epoch = tonumber(luardb.get(rdb_name))
    if not uptime_curr or not uptime_epoch then
        return "0"
    end
    return tostring(uptime_curr - uptime_epoch)
end

return {
    -- Device.Cellular.Interface.{i}.X_OUI_UpTime (physical layer)
    [subRoot_Cellular .. xVendorPrefix .. "_UpTime"] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, g_depth_idx)) - 1
            return 0, get_param_uptime(g_rdbPrefix_wwan .. idx .. ".system_network_status.sysuptime_at_reg_start")
        end,
    },

    -- Device.IP.Interface.{i}.X_OUI_UpTime (IP layer - overall)
    [subRoot_IP .. xVendorPrefix .. "_UpTime"] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, g_depth_idx))
            return 0, get_param_uptime(g_rdbPrefix_lpo .. idx .. ".sysuptime_at_ifdev_up")
        end,
    },

    -- Device.IP.Interface.{i}.IPv4Address.{i}.X_OUI_UpTime (IP layer - IPv4)
    [subRoot_IP .. "IPv4Address.*." .. xVendorPrefix .. "_UpTime"] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, g_depth_idx))
            return 0, get_param_uptime(g_rdbPrefix_lpo .. idx .. ".sysuptime_at_ipv4_up")
        end,
    },

    -- Device.IP.Interface.{i}.IPv6Address.{i}.X_OUI_UpTime (IP layer - IPv6)
    [subRoot_IP .. "IPv6Address.*." .. xVendorPrefix .. "_UpTime"] = {
        get = function(node, name)
            local idx = tonumber(getIndex(name, g_depth_idx))
            return 0, get_param_uptime(g_rdbPrefix_lpo .. idx .. ".sysuptime_at_ipv6_up")
        end,
    },
}
