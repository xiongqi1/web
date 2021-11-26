#!/usr/bin/env lua

--[[
    Deamon for tr143 ipping/download/upload diagnostics.
    It watches rdb variables and start/stop ipping/download/upload testing.
    Only one testing within each type can be in progress at any time.
    New request will cancel existing testing of the same type.
--]]

require('luardb')
require('luasyslog')
require('stringutil')
require('tableutil')

-- setup syslog facility
luasyslog.open('tr143_diagd', 'LOG_DAEMON')

-- our config file is shared with the TR-069 hostbridge
conf = dofile('/usr/lib/tr-069/config.lua')
package.path = conf.fs.code .. '/scripts/?.lua;' .. package.path

local core
if arg[1] == '-p' then
    core = 'ipping'
elseif arg[1] == '-u' then
    core = 'upload'
else
    core = 'download'
end

local engine
if core == 'ipping' then
    engine = require('tr143_ping_core')
else
    engine = require('tr143_http_core')
end

local debug = conf.log.debug
local log_level = conf.log.level or 'info'
debug = true
log_level = 'info'

-- debug info log utility. relies on luasyslog.open()
local function dinfo(...)
    if debug then
        local printResult = ''
        for _, v in ipairs{...} do
            printResult = printResult .. tostring(v) .. '\t'
        end
        luasyslog.log(log_level, printResult)
    end
end

local rdbPrefix = conf.rdb.tr143Prefix .. '.' .. core

local running, toRun
local quit

-- TR-181 specifies whenever parameters changed, test needs to be terminated
local function cancel(rdb, val)
    dinfo('Diagnostics param changed:', rdb .. '=' .. tostring(val))
    engine.setTestState('Canceled')
    toRun = nil
end

local function watchIPPingRdbs()
    luardb.watch(rdbPrefix .. '.interface', cancel)
    luardb.watch(rdbPrefix .. '.host', cancel)
    luardb.watch(rdbPrefix .. '.reps', cancel)
    luardb.watch(rdbPrefix .. '.timeout', cancel)
    luardb.watch(rdbPrefix .. '.blksz', cancel)
    luardb.watch(rdbPrefix .. '.dscp', cancel)
    luardb.watch(rdbPrefix .. '.proto', cancel)
end

local function watchHttpRdbs()
    luardb.watch(rdbPrefix .. '.interface', cancel)
    luardb.watch(rdbPrefix .. '.url', cancel)
    luardb.watch(rdbPrefix .. '.transports', cancel)
    luardb.watch(rdbPrefix .. '.dscp', cancel)
    luardb.watch(rdbPrefix .. '.ethernet_priority', cancel)
    luardb.watch(rdbPrefix .. '.proto', cancel)
    luardb.watch(rdbPrefix .. '.test_file_len', cancel)
end

local function enableChangedCb(rdb, val)
    dinfo('enable changed:', rdb .. '=' .. tostring(val))
    if val == '0' then
        engine.setTestState('Canceled') -- stop the engine
        quit = 1 -- stop the main loop
    end
end

local function stateChangedCb(rdb, val)
    dinfo('state changed:', rdb .. '=' .. tostring(val))
    -- only interested in state Requested and Canceled
    if val == 'Canceled' then
        -- pass the instruction on to the engine
        -- the engine will set state to None on termination
        engine.setTestState('Canceled')
        toRun = nil
        return
    end
    if val ~= 'Requested' then
        return
    end
    if running then -- only run one diagnostic at a time
        dinfo('Existing ' .. running .. ' is running. Restarting ...')
        engine.setTestState('Restart')
    end
    toRun = core
    dinfo('toRun:', toRun)
end

local urlparser = require('socket.url')

-- get startTime parameter from url query
-- http://...?startTime=secondsSinceEpoch
-- startTime specifies when the actual upload/download should start
local function getStartTime(url)
    local parsed_url = urlparser.parse(url)
    if not parsed_url or not parsed_url.query then
        return url, nil
    end
    dinfo('query=' .. parsed_url.query)

    -- in a query two separators are possible: '&' and ';'
    local querySep = '&'
    local queryComp = parsed_url.query:explode(querySep)
    if #queryComp == 1 then
        querySep = ';'
        queryComp = parsed_url.query:explode(querySep)
    end
    local startTime
    local newQueryComp = {}
    for _,v in ipairs(queryComp) do
        if not startTime then
            startTime = tonumber(v:match('^startTime=(%d+)$'))
            if not startTime then
                -- discard startTime query component since we already consume it
                newQueryComp[#newQueryComp + 1] = v
            end
        else
            newQueryComp[#newQueryComp + 1] = v
        end
    end

    if startTime then
        if #newQueryComp == 0 then
            parsed_url.query = nil
        else
            parsed_url.query = table.concat(newQueryComp, querySep)
        end
        url = urlparser.build(parsed_url)
    end

    return url, startTime
end

dinfo('Daemon started: ' .. core)
-- dinfo('watch: ', conf.rdb.enable, rdbPrefix .. '.state')

luardb.watch(conf.rdb.enable, enableChangedCb)
luardb.watch(rdbPrefix .. '.state', stateChangedCb)

if core == 'ipping' then
    watchIPPingRdbs()
else
    watchHttpRdbs()
end

while true do
    if not toRun then -- event handler might set toRun during testing
        luardb.wait(0)
    end

    dinfo('main loop: ', 'quit='..tostring(quit), 'toRun='..tostring(toRun))
    if quit == 1 then
        break
    end

    if toRun then
        engine.setTestState(nil) -- clear state for test launch
    end

    local host, reps, timeout, blksz, url, len, iface, dscp, startTime, proto
    iface = luardb.get(rdbPrefix .. '.interface_iface')
    if iface == '' then iface = nil end
    dscp = tonumber(luardb.get(rdbPrefix .. '.dscp'))
    proto = luardb.get(rdbPrefix .. '.proto')
    if not proto or proto == '' then
        proto = 'Any'
    end
    if toRun == 'ipping' then
        running = toRun
        toRun = nil
        host = luardb.get(rdbPrefix .. '.host')
        reps = tonumber(luardb.get(rdbPrefix .. '.reps'))
        timeout = tonumber(luardb.get(rdbPrefix .. '.timeout'))
        blksz = tonumber(luardb.get(rdbPrefix .. '.blksz'))
        dinfo(running .. ' started')
        engine.ipping(host, reps, timeout, blksz, dscp, iface, proto)
        dinfo(running .. ' ended')
        running = nil
    elseif toRun == 'download' then
        running = toRun
        toRun = nil
        url = luardb.get(rdbPrefix .. '.url')
        dinfo(running .. ' started')
        url, startTime = getStartTime(url)
        engine.download(url, dscp, iface, startTime, proto)
        dinfo(running .. ' ended')
        running = nil
    elseif toRun == 'upload' then
        running = toRun
        toRun = nil
        url = luardb.get(rdbPrefix .. '.url')
        len = tonumber(luardb.get(rdbPrefix .. '.test_file_len')) or 1000000
        dinfo(running .. ' started')
        url, startTime = getStartTime(url)
        engine.upload(url, len, dscp, iface, startTime, proto)
        dinfo(running .. ' ended')
        running = nil
    end
end

dinfo('Daemon exited: ' .. core)
