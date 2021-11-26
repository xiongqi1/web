----
-- * Copyright Notice:
-- * Copyright (C) 2021 Casa Systems.
-- *
-- * This file or portions thereof may not be copied or distributed in any form
-- * (including but not limited to printed or electronic forms and binary or object forms)
-- * without the expressed written consent of Casa Systems.
-- * Copyright laws and International Treaties protect the contents of this file.
-- * Unauthorized use is prohibited.
-- *
-- *
-- * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
-- * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
-- * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
-- * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
-- * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
-- * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
-- * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
-- * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
-- * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- * SUCH DAMAGE.
----

require("BulkData")
require("Logger")
require('stringutil')
local JSON = require("JSON")
local urlModule = require("socket.url")

local profileRdbObjName = "tr069.bulkData.profile"
local rdbObjConfig = {persist = true, idSelection = "smallestUnused"}

local profileRdbObj = rdbobject.getClass(profileRdbObjName, rdbObjConfig)

BulkData.Daemon = {}

---------------------function prototype---------------------
local resetProfileInstance
local removeTmpFile

local isServiceEnabled
local getInstanceValue
local setInstanceValue
local setInstanceValueIfChanged

local getState
local setState

local cvtFileToJsonTbl
local cvtJsonTblToFile
local getArchiveFilename
local generateEncodingFile
local cvtToJsonObjectHierarchy

local getNextCollectionTime
local getNextRetryTime
local tranferBulkData
------------------------------------------------------------

local tempBulkDataDir = "/tmp/BulkDataCol/"

--[[      < State machine diagram >
--                 init
--                  |
--               waiting
--                  |
--              reqEncoding
--                  |
--      --------waitEncoding -----
--      |                        |
--  encodingFailed        encodingSuccess
--      |                        |
--     init            -----transfer------
--                     |                 |
--                    init      ----httpRetryInit----
--                              |                   |
--                             init           httpRetryWaiting
--                                                  |
--                                      -----httpRetryTransfer----
--                                      |                        |
--                                     init                httpRetryInit
--]]
local stateMachine = {
    ["init"] = function(rdbObj, profileInst)
        local now = os.time()
        local nextCollection = getNextCollectionTime(profileInst, now)
        setInstanceValueIfChanged(profileInst, "_nextCollectionTime", nextCollection)
        setInstanceValueIfChanged(profileInst, "_lastHttpUri", "")
        setInstanceValueIfChanged(profileInst, "_httpRetryCnt", 0)
        setInstanceValueIfChanged(profileInst, "_nextHttpRetryTime", 0)
        removeTmpFile(profileInst)
        setState(profileInst, "waiting")
    end,
    ["waiting"] = function(rdbObj, profileInst)
        local now = os.time()
        local nextCollection = tonumber(getInstanceValue(profileInst, "_nextCollectionTime")) or now
        if nextCollection <= now then
            setState(profileInst, "reqEncoding")
        end
    end,
    ["reqEncoding"] = function(rdbObj, profileInst)
        setInstanceValue(profileInst, "_tmpEncodingFile", os.tmpname())
        luardb.set("tr069.bulkData.config.encoding_trigger", "1")
        setState(profileInst, "waitEncoding")
    end,
    ["waitEncoding"] = function(rdbObj, profileInst)
        -- Wait response(encodingFailed or encodingSuccess) from TR069 client
    end,
    ["encodingFailed"] = function(rdbObj, profileInst)
        setInstanceValueIfChanged(profileInst, "Status", "Error-Encode")
        setState(profileInst, "init")
    end,
    ["encodingSuccess"] = function(rdbObj, profileInst)
        setInstanceValueIfChanged(profileInst, "Status", "Enabled")
        setState(profileInst, "transfer")
    end,
    ["transfer"] = function(rdbObj, profileInst)
        local archiveFilename = generateEncodingFile(rdbObj, profileInst)
        if archiveFilename and tranferBulkData(rdbObj, profileInst, archiveFilename) then
            setInstanceValueIfChanged(profileInst, "Status", "Enabled")
            os.remove(archiveFilename)
            setState(profileInst, "init")
        else
            setInstanceValueIfChanged(profileInst, "Status", "Error-Transfer")
            local protocol = getInstanceValue(profileInst, "Protocol") or ""
            local httpRetryEnable = getInstanceValue(profileInst, "HttpRetryEnable") or "0"
            if protocol == "HTTP" and httpRetryEnable == "1" then
                setState(profileInst, "httpRetryInit")
            else
                setInstanceValueIfChanged(profileInst, "_httpRetryCnt", 0) -- reset retry cnt
                setState(profileInst, "init")
            end
        end
    end,
    ["httpRetryInit"] = function(rdbObj, profileInst)
        local _httpRetryCnt = tonumber(getInstanceValue(profileInst, "_httpRetryCnt")) or 0
        setInstanceValueIfChanged(profileInst, "_httpRetryCnt", _httpRetryCnt + 1) -- increase retry cnt

        local now = os.time()
        local nextCollection = getNextCollectionTime(profileInst, now)
        local nextRetry = getNextRetryTime(profileInst, now)

        if nextCollection > nextRetry then
            setInstanceValue(profileInst, "_nextHttpRetryTime", nextRetry)
            setState(profileInst, "httpRetryWaiting")
        else
            setInstanceValueIfChanged(profileInst, "_httpRetryCnt", 0) -- reset retry cnt
            setState(profileInst, "init")
        end
    end,
    ["httpRetryWaiting"] = function(rdbObj, profileInst)
        local now = os.time()
        local nextRetry = tonumber(getInstanceValue(profileInst, "_nextHttpRetryTime"))
        if nextRetry >= now then
            setState(profileInst, "httpRetryTransfer")
        end
    end,
    ["httpRetryTransfer"] = function(rdbObj, profileInst)
        local archiveFilename = getArchiveFilename(rdbObj, profileInst)
        if archiveFilename and tranferBulkData(rdbObj, profileInst, archiveFilename) then
            setInstanceValueIfChanged(profileInst, "Status", "Enabled")
            os.remove(archiveName)
            setInstanceValueIfChanged(profileInst, "_httpRetryCnt", 0) -- reset retry cnt
            setState(profileInst, "init")
        else
            local protocol = getInstanceValue(profileInst, "Protocol") or ""
            local httpRetryEnable = getInstanceValue(profileInst, "HttpRetryEnable") or "0"
            if protocol == "HTTP" and httpRetryEnable == "1" then
                setState(profileInst, "httpRetryInit")
            else
                setInstanceValueIfChanged(profileInst, "_httpRetryCnt", 0) -- reset retry cnt
                setState(profileInst, "init")
            end
        end
    end,
    ["disabled"] = function(rdbObj, profileInst)
        if isServiceEnabled(profileInst) then
            setInstanceValueIfChanged(profileInst, "Status", "Enabled")
            setState(profileInst, "init")
            return
        end
        setInstanceValueIfChanged(profileInst, "Status", "Disabled")
    end,
}

-- check Bulk data collection is enabled
isServiceEnabled = function(profileInst)
    return luardb.get("tr069.bulkData.config.enable") == "1" and getInstanceValue(profileInst, "Enable") == "1"
end

-- get property value with rdb object instance
--
-- profileInst: rdb object instance
-- name: rdb object property name to get
--
-- return: value
getInstanceValue = function(profileInst, name)
    assert(profileInst, "Invalid profile instance")
    return profileInst[name]
end

-- set property value with rdb object instance
--
-- profileInst: rdb object instance
-- name: rdb object property name to set
-- value: value to set
setInstanceValue = function(profileInst, name, value)
    assert(profileInst and name, "Invalid profile instance or name")
    profileInst[name] = value
end

-- set property value with rdb object instance when it changed.
--
-- profileInst: rdb object instance
-- name: rdb object property name to set
-- value: value to set
setInstanceValueIfChanged = function(profileInst, name, value)
    if profileInst[name] ~= value then
        setInstanceValue(profileInst, name, value)
    end
end

-- reset profile instance
--
-- profileInst: rdb object profile instance
resetProfileInstance = function(profileInst)
    assert(profileInst, "Invalid profile instance")
    setState(profileInst, "init")
end

-- remove temporary files if exist.
--
-- profileInst: rdb object profile instance
removeTmpFile = function(profileInst)
    local tmpFileName = getInstanceValue(profileInst, "_tmpEncodingFile")
    if tmpFileName then
        pcall(os.remove, tmpFileName)
    end
    setInstanceValueIfChanged(profileInst, "_tmpEncodingFile", "")
end

-- get status of profile instance
getState = function(profileInst)
    assert(profileInst, "Invalid profile instance")
    return profileInst._state
end

-- set status of profile instance if changed
setState = function(profileInst, state)
    assert(profileInst, "Invalid profile instance")
    if not stateMachine[state] then
        Logger.log("BulkData", "error", "Error!! Invalid Bulk Data Colleciton state")
        return
    end
    if profileInst._state ~= state then
        profileInst._state = state
    end
end

-- convert JSON archive file to JSON table
--
-- fname: filename of archive encoded with JSON
--
-- return: { Report = {
--                 {Previous report, if file transfer failed},
--                 {Previous report, if file transfer failed},
--         }
cvtFileToJsonTbl = function(fname)
    local jsonTbl = {Report={}}
    local fd = io.open(fname, "r")
    if fd then
        local status, tmpTbl = pcall(JSON.decode, JSON, fd:read("*a"))
        if status and tmpTbl.Report then
            jsonTbl = tmpTbl
        end
        fd:close()
    end
    return jsonTbl
end

-- convert JSON table to JSON archive file
-- (All previous data on the archive file is erased)
--
-- fname: filename to store JSON table.
-- jsonTbl: JSON table
--
-- return: true or false
cvtJsonTblToFile = function(fname, jsonTbl)
    local fd = io.open(fname, "w+") -- update mode, all previous data is erased.
    local status, content = pcall(JSON.encode_pretty, JSON, jsonTbl)
    if fd and status then
        fd:write(content)
        fd:close()
        return true
    end
    return false
end

-- get archive filename which store encoded bulk data.
--
-- rdbObj: profile rdb object
-- profileInst: rdb object profile instance
--
-- return full path of archive file
-- Encoded data archive name.
-- tempBulkDataDir .. "Profile" .. "_" .. profileIdx .. "_JSON_" .. encodingType([ObjectHierarchy|NameValuePair])
-- Ex) /tmp/BulkDataCol/Profile_1_JSONPairs or /tmp/BulkDataCol/Profile_1_JSONHierarchy
getArchiveFilename = function(rdbObj, profileInst)
    local profileIdx = rdbObj:getId(profileInst)
    local encodingType = getInstanceValue(profileInst, "JsonReportFormat") or "ObjectHierarchy" -- [ObjectHierarchy|NameValuePair]

    return string.format("%sProfile_%s_JSON_%s", tempBulkDataDir, profileIdx, encodingType)
end

-- generate file with encoded data
--
-- rdbObj: profile rdb object
-- profileInst: rdb object profile instance
--
-- return: generate file name or nil
generateEncodingFile = function(rdbObj, profileInst)
    if getInstanceValue(profileInst, "EncodingType") ~= "JSON" then
        return nil -- only support JSON type
    end
    local profileIdx = rdbObj:getId(profileInst)
    local encodingType = getInstanceValue(profileInst, "JsonReportFormat") or "ObjectHierarchy" -- [ObjectHierarchy|NameValuePair]
    local archiveName = getArchiveFilename(rdbObj, profileInst)
    local archiveJsonTbl = cvtFileToJsonTbl(archiveName)
    local newJsonTbl = cvtFileToJsonTbl(getInstanceValue(profileInst, "_tmpEncodingFile") or "")

    if newJsonTbl and newJsonTbl.Report and newJsonTbl.Report[1] then
        local maxNumOfFailedReports = 10
        local numOfFailedReports = tonumber(getInstanceValue(profileInst, "NumberOfRetainedFailedReports")) or 0
        if numOfFailedReports < -1 or numOfFailedReports > 10 then
            numOfFailedReports = maxNumOfFailedReports
        end
        for i = numOfFailedReports+1, #archiveJsonTbl.Report do
            table.remove(archiveJsonTbl.Report, 1)
        end
        -- set CollectionTime field
        local jsonReportTimestamp = getInstanceValue(profileInst, "JsonReportTimestamp")
        if jsonReportTimestamp == "ISO-8601" then
            newJsonTbl.Report[1].CollectionTime = os.date("!%Y-%m-%dT%XZ")
        elseif jsonReportTimestamp == "Unix-Epoch" then
            newJsonTbl.Report[1].CollectionTime = os.time()
        end
        local resultTbl
        if encodingType == "ObjectHierarchy" then
            resultTbl = cvtToJsonObjectHierarchy(newJsonTbl.Report[1])
        else
            resultTbl = newJsonTbl.Report[1]
        end
        table.insert(archiveJsonTbl.Report, resultTbl)

        if newJsonTbl.uri then
            setInstanceValueIfChanged(profileInst, "_lastHttpUri", "?" .. newJsonTbl.uri)
        else
            setInstanceValueIfChanged(profileInst, "_lastHttpUri", "")
        end

    end

    os.execute(string.format("rm -f %sProfile_%s_JSON_*", tempBulkDataDir, profileIdx))
    if cvtJsonTblToFile(archiveName, archiveJsonTbl) then
        return archiveName
    end
    return nil
end

-- convert JSON NameValuePair to ObjectHierarchy format
--
-- jsonTbl: Json table in NameValuePair format
cvtToJsonObjectHierarchy = function(jsonTbl)
    function cvtNameToHierarchy(targetTbl, key)
        if not targetTbl[key] then
            targetTbl[key] = {}
        end
        return targetTbl[key]
    end

    local retTbl = {}
    local pathTbl
    for key, value in pairs(jsonTbl) do
        local tmpTbl = retTbl
        pathTbl = string.explode(key, ".")
        for idx=1, #pathTbl do
            if idx < #pathTbl then
                tmpTbl = cvtNameToHierarchy(tmpTbl, pathTbl[idx])
            else
                tmpTbl[pathTbl[idx]] = value
            end
        end
    end
    return retTbl
end

-- get next collection time in seconds since epoch "00:00:00 UTC, January 1, 1970
--
-- profileInst: rdb object profile instance
-- now: current time in seconds since epoch.
--
-- return seconds since epoch "00:00:00 UTC, January 1, 1970
getNextCollectionTime = function(profileInst, now)
    local nextCollection
    local interval = tonumber(getInstanceValue(profileInst, "ReportingInterval")) or 86400
    local phase = tonumber(getInstanceValue(profileInst, "TimeReference")) or 0
    local delta = math.abs(now - phase)

    if now >= phase then
        if delta < interval then
            interval =  interval - delta
        else
            interval = interval - (delta % interval)
        end
    else
        if delta < interval then
            interval = delta
        else
            interval = delta % interval
        end
    end

    nextCollection = interval + now
    return nextCollection
end

-- get next http retry time in seconds since epoch "00:00:00 UTC, January 1, 1970
--
-- profileInst: rdb object profile instance
-- now: current time in seconds since epoch.
--
-- return seconds since epoch "00:00:00 UTC, January 1, 1970
getNextRetryTime = function(profileInst, now)
    local m = tonumber(getInstanceValue(profileInst, "HttpRetryMinimumWaitInterval")) or 5
    local k = tonumber(getInstanceValue(profileInst, "HttpRetryIntervalMultiplier")) or 2000
    local tries = tonumber(getInstanceValue(profileInst, "_httpRetryCnt")) or 0

    local min = m * (k/1000)^(tries - 1)
    local max = m * (k/1000)^tries
    Logger.log("BulkData", "error", "Retry interval: (m = " .. m .. ", k = " .. k .. "): [" .. min .. "-" .. max .. "].")
    return math.random(min, max) + now
end

-- transfer encoded bulk data to collection server
--
-- rdbObj: profile rdb object
-- profileInst: rdb object profile instance
--
-- return: true or false
tranferBulkData = function(rdbObj, profileInst, archiveName)
    local protocol = getInstanceValue(profileInst, "Protocol")
    if protocol == "HTTP" then
        local url = getInstanceValue(profileInst, "HttpURL") or ""
        local username = getInstanceValue(profileInst, "HttpUsername") or ""
        local password = getInstanceValue(profileInst, "HttpPassword") or ""
        local compression = getInstanceValue(profileInst, "HttpCompression") or "None" --[GZIP|None]
        local method = getInstanceValue(profileInst, "HttpMethod") or "POST"
        local useDateHeader = getInstanceValue(profileInst, "HttpUseDateHeader") or "1"
        local encodingType = getInstanceValue(profileInst, "EncodingType") or "JSON"
        local jsonFormat = getInstanceValue(profileInst, "JsonReportFormat") or "ObjectHierarchy"
        local httpUri = getInstanceValue(profileInst, "_lastHttpUri") or ""

        local urlTbl = urlModule.parse(url)
        if not urlTbl then
            Logger.log("BulkData", "warning", "Error!! Failed Bulk Data Collection. Invalid URL")
            return false
        end

        -- username and password have to be passed in via -u option so that special characters are preserved/properly encoded
        local userinfo = ""
        if username ~= "" then
            userinfo = " -u '" .. username
            if password ~= "" then
                userinfo = userinfo .. ":" .. password
            end
            userinfo = userinfo .. "'"
        end

        local hostport = urlTbl.host
        if not hostport then
            Logger.log("BulkData", "info", "Error!! Failed Bulk Data Collection. No host is specified")
            return false
        end
        if urlTbl.port then
            hostport = hostport .. ":" .. urlTbl.port
        end

        local cmd = "curl"
        if urlTbl.host:match("^%[.+%]$") then
            -- host is a numerical IPv6 address, disable URL globbing to avoid curl error 3
            cmd = cmd .. ' -g'
        end
        if compression == "GZIP" then
            cmd = cmd .. " --compressed"
        end
        if method == "PUT" then
            cmd = cmd .. " -X PUT"
        else
            cmd = cmd .. " -X POST"
        end
        if useDateHeader == "1" then
            cmd = string.format('%s -H "Date: %s"', cmd, os.date("!%a, %d %b %Y %X GMT"))
        end
        if encodingType == "JSON" then
            cmd = string.format('%s -H "Content-Type: application/json; charset=UTF-8"', cmd)
            cmd = string.format('%s -H "BBF-Report-Format: \"%s\""', cmd, jsonFormat)
        end

        ---[=[ << SSL/TLS setting.
        if conf.net.ssl_version == 6 then
            cmd = cmd .. ' --tlsv1.2' -- tls v1.2 and upper
        elseif conf.net.ssl_version == 7 then
            cmd = cmd .. ' --tlsv1.3' -- tls v1.3 and upper
        else
            cmd = cmd .. ' -1' -- require: TLSv1 (-1) and upper by default
        end

        -- require: --noverifyhost (curl patch) unless specified otherwise
        if not conf.net.ssl_verify_host then
            cmd = cmd .. ' --noverifyhost'
        end
        -- If ca certificate file is specified, append it.
        if string.trim(conf.net.ca_cert) ~= "" then
            cmd = cmd .. ' --cacert ' .. conf.net.ca_cert
        end
        -- CA directory that contains individual CA certs
        if string.trim(conf.net.ca_path) ~= "" then
            cmd = cmd .. ' --capath ' .. conf.net.ca_path
        end
        if string.trim(conf.net.client_cert) ~= "" then
            cmd = cmd .. ' --cert ' .. conf.net.client_cert
        end
        if string.trim(conf.net.client_key) ~= "" then
            cmd = cmd .. ' --key ' .. conf.net.client_key
        end
        -- do not restrain ciphers unless explicitly requested
        if string.trim(conf.net.ssl_cipher_list) ~= "" then
            cmd = cmd .. ' --ciphers ' .. conf.net.ssl_cipher_list
        end
        --]=] SSL/TLS setting. >>

        cmd = cmd .. " -f -s -T " .. archiveName .. userinfo .. " " .. "'" .. urlTbl.scheme .. '://' .. hostport .. urlTbl.path .. httpUri .. "'"
        setInstanceValueIfChanged(profileInst, "_lastCollectionEpoch", os.time())
        if os.execute(cmd) == 0 then
            return true
        end

    end

    Logger.log("BulkData", "warning", "Error!! Failed Bulk Data Collection. Data transfer failure")
    return false
end

local function init(self)
    self.running = true
    self.host:onShutdown(function() self:stop() end)
    os.execute(string.format("mkdir -p %s", tempBulkDataDir))

    for _, inst in pairs(profileRdbObj:getAll()) do
        resetProfileInstance(inst)
    end
end

local function poll(self)
    for _, inst in pairs(profileRdbObj:getAll()) do
        -- This condition should be prior to others.
        -- Rdb profile instance could be created dynamically in a runtime with TR069 AddObject() method.
        -- In that case, Rdb profile instance does not have explicit stateMachine state.
        if not getState(inst) then
            setState(inst, "init")
        end
        if not isServiceEnabled(inst) then
            setState(inst, "disabled")
        end
        stateMachine[inst._state](profileRdbObj, inst)
    end
    self.host:eventWait(1)
end

local function stop(self)
    os.execute(string.format("rm -rf %s", tempBulkDataDir))
    self.running = false
end

local function new(host)
    local daemon = {
        running = false,

        host = host,

        ["init"] = init,
        ["poll"] = poll,
        ["stop"] = stop,
    }
    return daemon
end

BulkData.Daemon = {
    new = new,
    getState = getState,
    setState = setState,
}

return BulkData.Daemon
