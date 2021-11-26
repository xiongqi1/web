#!/usr/bin/lua

local luardb = require("luardb")
local luasyslog = require("luasyslog")
local luasocket = require("socket")

-- open syslog
luasyslog.open('tr069_watchdog', 'LOG_DAEMON')

-- log level
local levelTbl = {
    emergency = true,
    alert = true,
    critical = true,
    error = true,
    warning = true,
    notice = true,
    info = true,
    debug = true,
}

local function logger(level, fmt, ...)
    level = levelTbl[level] and level or "notice"
    luasyslog.log(level,  string.format(fmt, ...))
end

local function get_uptime()
    local file, msg, errno = io.open('/proc/uptime', 'r')
    assert(file, 'Could not read uptime: (' .. tostring(errno) .. ') ' .. tostring(msg) .. '.')
    local n = math.floor(file:read('*n'))
    file:close()
    return n
end

if luardb.get("tr069.watchdog.keep_client_enabled") == "1"
    and luardb.get("service.tr069.enable") ~= "1" then
    luardb.set("service.tr069.enable", 1)
    return
end

local informEnable = luardb.get("tr069.server.periodic.enable")
if informEnable ~= "1" then
    return
end

-- TR069 daemon status check formula:
--     (CurrentTime(in UTC) - Device.ManagementServer.X_OUI_LastInformTime(in UTC))
--     > (1 day + (Device.ManagementServer.PeriodicInformInterval + 300)))
--
-- CurrentTime: current time in UTC
-- X_OUI_LastInformTime: tr069.lastSuccInformAt
-- PeriodicInformInterval: tr069.server.periodic.interval
-- PeriodicInformEnable: tr069.server.periodic.enable

local currT = os.time()
local lastInformT = tonumber(luardb.get("tr069.lastSuccInformAt")) or 0
local informInterval = tonumber(luardb.get("tr069.server.periodic.interval"))

-- lastInformT will be 0 if we haven't successfully informed or the daemon
-- has somehow failed to start. To give the initial inform a reasonable time
-- to complete after boot, check the uptime is greater than an hour.
if lastInformT == 0 then
    local uptime = get_uptime()
    if uptime and uptime < (60 * 60) then
        return
    end
end

if informInterval then
    if (currT - lastInformT) > (informInterval + 60*60*24 + 300) then
        logger("error", "Restart TR069 client due to stability test failure, currTime=[%s], lastInformTime=[%s], informInterval=[%s]", currT, lastInformT, informInterval)

        local daemonName = "^lua /usr/lib/tr-069/cwmpd.lua$"
        local pgrepCmd = string.format('pgrep -f "%s" >/dev/null 2>&1', daemonName)
        local pkillCmd = string.format('pkill -f "%s" >/dev/null 2>&1', daemonName)

        -- disable tr069 client
        luardb.set("service.tr069.enable", 0)

        luasocket.sleep(2)

        local currT = os.time()
        local endT = currT + 120
        while (os.execute(pgrepCmd) == 0) and (currT < endT) do
            luasocket.sleep(5)
            currT = os.time()
        end

        if currT > endT then
            logger("error", "Kill tr069 client by force")
            os.execute(pkillCmd)
            luasocket.sleep(5)
        end
        luardb.set("service.tr069.enable", 1)
    end
end

