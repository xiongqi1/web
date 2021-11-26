--[[
This script handles objects/parameters under Device.X_<VENDOR>.Services.SpeedTest.

  TR069 parameter details:
    * Device.X_<VENDOR>.Services.SpeedTest.SpeedTestTrigger
      Available Values: 1
      - Starting speed test.

    * Device.X_<VENDOR>.Services.SpeedTest.SpeedTestResult.
      - Status: Current status of speed test ([inprogress|finished])
      - PingLatency:
      - DownloadBandwidth:
      - UploadBandwidth:
      - LastUpdateTimeUTC:


Copyright (C) 2021 Casa Systems. Inc.
--]]

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. "." .. xVendorPrefix .. ".Services.SpeedTest."

------------------local function prototype------------------
local speedtest_cb
------------------------------------------------------------


------------------local variable definition ----------------
------------------------------------------------------------


------------------local function definition ----------------
speedtest_cb = function ()
    -- Do not need to do anything, if speed_test.sh is in being processed.
    -- Because speedtest template is already triggered via other service.
    if luardb.get("service.speedtest.current_state") ~= "inprogress" then
        luardb.set('service.speedtest.trigger', '1')
    end
end
------------------------------------------------------------

return {
    -- bool:writeonly
    -- Available Value: 1
    -- rdb variable: "service.speedtest.trigger"
    [subRoot .. 'SpeedTestTrigger'] = {
        set = function(node, name, value)
            value = string.trim(value)
            if value ~= "1" then return CWMP.Error.InvalidParameterValue end

            -- Trigger Speed test after terminating inform session to avoid interference to the test.
            if client:isTaskQueued('postSession', speedtest_cb) ~= true then
                client:addTask('postSession', speedtest_cb, false)
            end

            return 0
        end
    },

    -- string:readonly
    -- rdb variable: "service.speedtest.ping_latency"
    [subRoot .. "SpeedTestResult.PingLatency"] = {
        get = function(node, name)
            local retVal = ""
            local latency = luardb.get("service.speedtest.ping_latency")
            if tonumber(latency) then
                retVal = string.format("%s ms", latency)
            end
            return 0, retVal
        end,
    };

    -- string:readonly
    -- rdb variable: "service.speedtest.download_bandwidthMbps"
    [subRoot .. "SpeedTestResult.DownloadBandwidth"] = {
        get = function(node, name)
            local retVal = ""
            local bandwidth = luardb.get("service.speedtest.download_bandwidthMbps")
            if tonumber(bandwidth) then
                retVal = string.format("%s Mbps", bandwidth)
            end
            return 0, retVal
        end,
    };

    -- string:readonly
    -- rdb variable: "service.speedtest.upload_bandwidthMbps"
    [subRoot .. "SpeedTestResult.UploadBandwidth"] = {
        get = function(node, name)
            local retVal = ""
            local bandwidth = luardb.get("service.speedtest.upload_bandwidthMbps")
            if tonumber(bandwidth) then
                retVal = string.format("%s Mbps", bandwidth)
            end
            return 0, retVal
        end,
    };

    -- string:readonly
    -- rdb variable: ""
    [subRoot .. "SpeedTestResult.LastUpdateTimeUTC"] = {
        get = function(node, name)
            local retVal = ""
            local utcInSec = tonumber(luardb.get("service.speedtest.last_result_updated") or 0)
            if utcInSec > 0 then
                retVal = os.date("%Y-%m-%dT%XZ", utcInSec)
            end
            return 0, retVal
        end,
    };
}
