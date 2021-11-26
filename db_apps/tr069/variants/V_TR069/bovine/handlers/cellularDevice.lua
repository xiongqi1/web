--[[
This script handles objects/parameters under Device.Cellular.

  Interface.{i}.
  Interface.{i}.USIM.
  Interface.{i}.Stats.
  AccessPoint.{i}.

Copyright (C) 2016 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require("Logger")

local logSubsystem = 'CellularDevice'
Logger.addSubsystem(logSubsystem)

local subRoot = conf.topRoot .. '.Cellular.'

------------------local variable----------------------------
local g_wwan_ifaces = nil  -- a table of wwan interfaces' names
local g_wwan_apts = nil -- a table of wwan access points
local systemFS = '/sys/class/net/'

local g_rdbPrefix_profile = 'link.profile.'
local g_rdbstartIdx_profile = 1

local g_wwan_ifaces_old = {} -- g_wwan_ifaces since last poll

local g_depthOfIfaceInst = 4 -- path length of Device.Cellular.Interface.{i}
local g_depthOfAptInst = 4 -- path length of Device.Cellular.AccessPoint.{i}
------------------------------------------------------------

------------------local function prototype------------------
local getWWAN_ifNameTbl
local getWWAN_ifName
local rebuildAndGetWWAN_ifNameTbl
local cleanUpFunc
local getNumOfActivatedIfaces
local getCurrEnabledProfIdx
local getMaxEnabledProf
local readStatistics
local poll_InterfaceCollection
local getIndex
local isStatusUp
------------------------------------------------------------

------------------local function definition------------------

-- This function returns indexed array of WWAN interfaces.
-- g_wwan_ifaces table will be created when called getWWAN_ifNameTbl() function
-- and destroyed in cleanUp task(cleanUpFunc()).
-- We collect interface information from link.profile.* rdb vars
getWWAN_ifNameTbl = function()
    if g_wwan_ifaces then return g_wwan_ifaces end

    g_wwan_ifaces = {}

    for i, dev in hdlerUtil.traverseRdbVariable{prefix=g_rdbPrefix_profile ,
                                                suffix='.dev' ,
                                                startIdx=g_rdbstartIdx_profile}
    do
        dev = string.match(dev, '%a+')
        -- the .dev rdb var should have value wwan.{i}
        -- the first .dev rdb var violating that means the end
        if not dev or dev ~= 'wwan' then break end

        local iface = ''
        local enable = string.trim(
            luardb.get(g_rdbPrefix_profile .. i .. '.enable')
        )
        local status = string.trim(
            luardb.get(g_rdbPrefix_profile .. i .. '.status')
        )
        -- only fill in interface name for profile that is both enabled and up
        -- otherwise, fill in empty interface name
        if enable == "1" and status:lower() == 'up' then
            iface = string.trim(
                luardb.get(g_rdbPrefix_profile .. i .. '.interface')
            )
        end
        table.insert(g_wwan_ifaces, iface)
    end

    return g_wwan_ifaces
end

-- get WWAN interface name by index to g_wwan_ifaces
getWWAN_ifName = function(idx)
    if not idx then return '' end

    local index = tonumber(idx) or ''
    if not index or index == '' then return '' end

    local ifNameTbl = getWWAN_ifNameTbl()

    return ifNameTbl[index] or ''
end

-- Usage: This function forces rebuilding g_wwan_ifaces and returns it
rebuildAndGetWWAN_ifNameTbl = function()
    g_wwan_ifaces = nil
    return getWWAN_ifNameTbl()
end

cleanUpFunc = function ()
Logger.log(logSubsystem, 'info', 'cleanUp')
    g_wwan_ifaces = nil
    g_statusList = nil
end

-- Return the number of activated WWAN interfaces (with non-empty iface name)
getNumOfActivatedIfaces = function ()
    local interfaces = getWWAN_ifNameTbl()
    local count = 0

    for _, value in ipairs(interfaces) do
        if value ~= '' then
            count = count + 1
        end
    end

    return count
end

-- This will return an array that has the indexes of enabled profiles
getCurrEnabledProfIdx = function ()
    local tbl = {}
    for i, dev in hdlerUtil.traverseRdbVariable{prefix=g_rdbPrefix_profile,
                                                suffix='.dev',
                                                startIdx=g_rdbstartIdx_profile}
    do
        dev = string.match(dev, '%a+')

        if not dev or dev ~= 'wwan' then break end

        local enable = string.trim(
            luardb.get('link.profile.' .. i .. '.enable')
        )
        if enable == '1' then
            table.insert(tbl, i)
        end
    end
    return tbl
end

-- Return the number of enabled profiles
getMaxEnabledProf = function ()
    local tbl = getCurrEnabledProfIdx
    return #tbl
end

-- read wwan interface statistics from fs /sys/class/net
readStatistics = function (iface, name)
    local filename = systemFS .. iface .. '/statistics/' .. name
    local defaultV = "0"

    if hdlerUtil.IsRegularFile(filename) then
        local result = Daemon.readIntFromFile(filename) or defaultV
        return tostring(result)
    end
    return defaultV
end

-- poll interface changes and update Cellular.Interface & AccessPoint
poll_InterfaceCollection = function (task)
    Logger.log(logSubsystem, 'info', 'poll_InterfaceCollection')

    local currTbl = getWWAN_ifNameTbl()
    local isSame = true

    Logger.log(logSubsystem, 'info', #currTbl .. ' ifaces found')

    for i, v in ipairs(currTbl) do
        if v ~= g_wwan_ifaces_old[i] then
            isSame = false
            break
        end
    end

    if isSame then return end

    Logger.log(logSubsystem, 'info', 'ifaces changed')

    local node = task.data -- Cellular.Interface

    g_wwan_ifaces_old = currTbl

    -- delete all of children except 'default'
    for _, child in ipairs(node.children) do
        if child.name ~= '0' then
            child.parent:deleteChild(child)
        end
    end

    for i, v in ipairs(currTbl) do
        if v ~= '' then
            node:createDefaultChild(i)
        end
    end

    -- AccessPoint always has the same number of children as Interface
    node = node.parent:getChild('AccessPoint')

    for _, child in ipairs(node.children) do
        if child.name ~= '0' then
            child.parent:deleteChild(child)
        end
    end

    for i, v in ipairs(currTbl) do
        if v ~= '' then
            node:createDefaultChild(i)
        end
    end

end

-- get the part of dot-separated name at depth
getIndex = function (name, depth)
    local pathBits = name:explode('.')
    local idx = pathBits[depth]

    return idx
end

-- return the interface status of g_wwan_ifaces[i]
isStatusUp = function (instanceIdx)
    local idx = tonumber(string.trim(instanceIdx))
    if not idx then return nil end

    if g_statusList then
        return g_statusList[idx]
    end

    g_statusList = {}

    for i, dev in hdlerUtil.traverseRdbVariable{prefix=g_rdbPrefix_profile,
                                                suffix='.dev',
                                                startIdx=g_rdbstartIdx_profile}
    do
        dev = string.match(dev, '%a+')

        if not dev or dev ~= 'wwan' then break end

        local enable = string.trim(
            luardb.get(g_rdbPrefix_profile .. i .. '.enable')
        )
        local status = string.trim(
            luardb.get(g_rdbPrefix_profile .. i .. '.status')
        )

        if enable == "1" and status:lower() == 'up' then
            table.insert(g_statusList, '1')
        else
            table.insert(g_statusList, '0')
        end
    end

    return g_statusList[idx]
end
------------------------------------------------------------

return {
    [subRoot .. 'RoamingStatus'] = {
        get = function(node, name)
            local roaming = luardb.get('wwan.0.system_network_status.roaming')
            return 0, roaming == 'active' and 'Roaming' or 'Home'
        end
    },

    [subRoot .. 'Interface'] = {
        init = function(node, name)
            -- Delete all of children first
            for _, child in ipairs(node.children) do
                if child.name ~= '0' then
                    child.parent:deleteChild(child)
                end
            end
            local ifaceTbl = getWWAN_ifNameTbl()

            for i, v in ipairs(ifaceTbl) do
                if v ~= '' then
                    node:createDefaultChild(i)
                end
            end

            -- make sure we get updated interface list on every new session
            if client:isTaskQueued('cleanUp', cleanUpFunc) ~= true then
                client:addTask('cleanUp', cleanUpFunc, true)
            end
            if client:isTaskQueued('preSession',
                                     poll_InterfaceCollection) ~= true then
                client:addTask('preSession', poll_InterfaceCollection,
                               true, node)
            end
            return 0
        end
    },

    [subRoot .. 'Interface.*'] = {
        init = function(node, name)
            local pathBits = name:explode('.')
            g_depthOfIfaceInst = #pathBits -- useful for getting index later
            return 0
        end
    },

    [subRoot .. 'Interface.*.*'] = {
        get = function(node, name)
            local idx = getIndex(name, g_depthOfIfaceInst)
            if not idx then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Parameter " .. name .. " does not exist"
            end
            if node.name == 'Enable' then
                local retVal = '0'
                local enable = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.enable')
                )
                if enable == '1' then
                    retVal = '1'
                end
                return 0, retVal
            end
            if node.name == 'Status' then
                local retVal = 'Down'
                local status = isStatusUp(idx)
                if status and status == '1' then
                    retVal = 'Up'
                end
                return 0, retVal
            end
            if node.name == 'Name' then
                return 0, getWWAN_ifName(idx)
            end
            if node.name == 'IMEI' then
                local dev = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.dev')
                )
                if not dev or dev:match('^wwan%.%d+$') ~= dev then
                    return 0, ''
                end
                local retVal = string.trim(luardb.get(dev .. '.imei'))
                return 0, retVal
            end
            if node.name == 'NetworkInUse' then
                local dev = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.dev')
                )
                if not dev or dev:match('^wwan%.%d+$') ~= dev then
                    return 0, ''
                end
                local retVal = string.trim(luardb.get(dev .. '.system_network_status.network.unencoded'))
                return 0, retVal
            end
            if node.name == 'CurrentAccessTechnology' then
                local dev = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.dev')
                )
                if not dev or dev:match('^wwan%.%d+$') ~= dev then
                    return 0, ''
                end
                local retVal = string.trim(luardb.get(dev .. '.system_network_status.service_type'))
                return 0, retVal
            end
        end,

        set = function(node, name, value)
            value = string.trim(value)
            local idx = getIndex(name, g_depthOfIfaceInst)
            if not idx then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Parameter " .. name .. " does not exist"
            end
            if node.name == 'Enable' then
                if value ~= '1' and value ~= '0' then
                    return CWMP.Error.InvalidParameterValue
                end
                local enable = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.enable')
                )
                if enable ~= value then
                    luardb.set(g_rdbPrefix_profile .. idx .. '.enable', value)
                end
                return 0
            end
        end
    },

    [subRoot .. 'Interface.*.USIM.*'] = {
        get = function(node, name)
            local idx = getIndex(name, g_depthOfIfaceInst)
            if not idx then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Parameter " .. name .. " does not exist"
            end

            local dev = string.trim(
                luardb.get(g_rdbPrefix_profile .. idx .. '.dev')
            )
            if not dev or dev:match('^wwan%.%d+$') ~= dev then
                return 0, ''
            end

            if node.name == 'Status' then
                --[[ cf. connection_mgr.c
                    .sim.status.status
                     SIM PIN | SIM PIN Required | SIM locked* -> 1
                     SIM PUK | *PUK* -> 2
                     PH-NET PIN | SIM PH-NET | *MEP* -> 3
                     SIM not inserted | SIM removed -> 4
                     *OK* -> 0
                    .meplock.status
                     locked -> 3
                --]]
                local retVal = 'None'
                local status = string.trim(
                    luardb.get(dev .. '.sim.status.status')
                )
                if not status or status:find('not inserted') or
                  status:find('removed') then
                    retVal = 'None'
                elseif status:find('OK') then
                    retVal = 'Valid'
                elseif status:find('SIM PIN') or status:find('SIM locked') then
                    retVal = 'Available'
                elseif status:find('PUK') or status:find('PH-NET') or
                  status:find('MEP') then
                    retVal = 'Blocked'
                elseif status:find('Negotiating') then
                    retVal = 'Available'
                end
                status = string.trim(luardb.get(dev..'.meplock.status'))
                if status:lower() == 'locked' then
                    retVal = 'Blocked'
                end
                return 0, retVal
            end

            if node.name == 'MSISDN' then
                local msisdn = string.trim(luardb.get(dev..'.sim.data.msisdn'))
                msisdn = msisdn or ''
                return 0, msisdn
            end
        end
    },

    [subRoot .. 'Interface.*.Stats.*'] = {
        get = function(node, name)
            local idx = getIndex(name, g_depthOfIfaceInst)
            if not idx then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Parameter " .. name .. " does not exist"
            end
            local iface = getWWAN_ifName(idx)
            if not iface or iface == '' then
                return 0, "0"
            end
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
        end
    },

    [subRoot .. 'AccessPoint'] = {
        init = function(node, name)
            -- Delete all of children first
            for _, child in ipairs(node.children) do
                if child.name ~= '0' then
                    child.parent:deleteChild(child)
                end
            end
            local ifaceTbl = getWWAN_ifNameTbl()

            for i, v in ipairs(ifaceTbl) do
                if v ~= '' then
                    node:createDefaultChild(i)
                end
            end

            -- cleanUp and preSession tasks are the same as Interface
            return 0
        end
    },

    [subRoot .. 'AccessPoint.*'] = {
        init = function(node, name)
            local pathBits = name:explode('.')
            g_depthOfAptInst = #pathBits -- useful for getting index later
            return 0
        end
    },

    [subRoot .. 'AccessPoint.*.*'] = {
        get = function(node, name)
            local idx = getIndex(name, g_depthOfAptInst)
            if not idx then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Parameter " .. name .. " does not exist"
            end
            if node.name == 'Enable' then
                local retVal = '0'
                local enable = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.enable')
                )
                if enable == '1' then
                    retVal = '1'
                end
                return 0, retVal
            end
            if node.name == 'APN' then
                local retVal
                local autoapn = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.autoapn')
                )
                if autoapn == '1' then
                    retVal = "[auto]"
                else
                    retVal = string.trim(
                        luardb.get(g_rdbPrefix_profile .. idx .. '.apn')
                    )
                end
                return 0, retVal
            end
            if node.name == 'Interface' then
                local ifname = getWWAN_ifName(idx)
                local ifcoll = node.parent.parent.parent:getChild('Interface')
                if not ifcoll then
                    return CWMP.Error.InternalError,
                        "Error: Could not find Interface collection"
                end
                local iface = ifcoll:getChild(tostring(idx))
                if not iface then
                   return CWMP.Error.InternalError,
                        "Error: Could not find Interface " .. idx
                end
                return 0, iface:getPath()
            end
        end,

        set = function(node, name, value)
            value = string.trim(value)
            local idx = getIndex(name, g_depthOfAptInst)
            if not idx then
                return CWMP.Error.InvalidParameterValue,
                    "Error: Parameter " .. name .. " does not exist"
            end
            if node.name == 'Enable' then
                if value ~= '1' and value ~= '0' then
                    return CWMP.Error.InvalidParameterValue
                end
                local enable = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.enable')
                )
                if enable ~= value then
                    luardb.set(g_rdbPrefix_profile .. idx .. '.enable', value)
                end
                return 0
            end
            if node.name == 'APN' then
                local autoapn = string.trim(
                    luardb.get(g_rdbPrefix_profile .. idx .. '.autoapn')
                )
                if value == '[auto]' then
                    if autoapan ~= '1' then
                        luardb.set(g_rdbPrefix_profile .. idx .. '.autoapn',
                                   '1')
                    end
                else
                    local apn = string.trim(
                        luardb.get(g_rdbPrefix_profile .. idx .. '.apn')
                    )
                    if value ~= apn then
                        luardb.set(g_rdbPrefix_profile .. idx .. '.apn', value)
                    end
                end
                return 0
            end
            if node.name == 'Interface' then
                return 0 -- interface will automatically be set up
            end
        end
    },

}

