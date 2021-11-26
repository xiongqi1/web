-- Copyright (C) 2018 NetComm Wireless limited.
--
-- AT&T Device wipe web request handlers.

require('luardb')
local turbo = require("turbo")
local util = require("rds.util")

local config = require("rds.config")
local wmmdconfig = require("wmmd.config")
local basepath = config.basepath
local logger = require("rds.logging")
local lfs = require("lfs")

-- get the number of received message files.
local function getCntOfMessages()
    local cnt = 0
    for file in lfs.dir(wmmdconfig.incoming_sms_dir) do
        local fn = wmmdconfig.incoming_sms_dir.."/"..file
        local prefix = string.sub(file, 1, 5)
        -- As a result of a discussion, take out validation check for file contents.
        if lfs.attributes(fn, "mode") == "file" and prefix == "rxmsg" then
            cnt = cnt + 1
        end
    end
    return cnt
end

--------- Device wipe handler ----------
local DeviceWipeHandler = class("DeviceWipeHandler", turbo.web.RequestHandler)

local rdbN_CL_entry = "voice_call.call_history."
local rdbN_CL_index = rdbN_CL_entry .. "index"

-- device wipe PUT handler for call_log
--
-- req: request body {num_of_new_entries:[1~100]}
--      number of call log entries to generate
-- return: true or false
local function l_set_call_log(req)
    if not req and not req.num_of_new_entries then return false end

    local MAX_ENTRIES = 100
    local MAX_UID = 1000

    local num_of_new_entries = tonumber(req.num_of_new_entries)
    if not num_of_new_entries
        or num_of_new_entries < 1 or num_of_new_entries > MAX_ENTRIES then
        return false
    end

    local rdb_idx=luardb.get(rdbN_CL_index) or -1
    local call_log_uid = string.match(luardb.get(rdbN_CL_entry .. rdb_idx) or '', "^(%d+).*") or 0

    for i=rdb_idx+1, (num_of_new_entries+rdb_idx) do
        local idx = i % MAX_ENTRIES
        local call_start = os.time()

        local entry_data = string.format("%d,%s,%s,,%s,%s", call_log_uid, 'in', "0123456789", call_start, '1')
        luardb.set(rdbN_CL_entry .. idx, entry_data, 'p')
        luardb.set(rdbN_CL_index, idx, 'p')

        call_log_uid = (call_log_uid + 1) % MAX_UID
    end
    return true
end

-- device wipe PUT handler for sms
--
-- req: request body {num_of_new_entries:[1~100]}
--      number of sms entries to generate
-- return: true or false
local function l_set_sms(req)
    if not req and not req.num_of_new_entries then return false end

    local MAX_ENTRIES = 100
    local numOfMsg = getCntOfMessages()

    local num_of_new_entries = tonumber(req.num_of_new_entries)
    if not num_of_new_entries
        or num_of_new_entries < 1 or num_of_new_entries > MAX_ENTRIES
        or numOfMsg >= MAX_ENTRIES then
        return false
    end

    if numOfMsg + num_of_new_entries > MAX_ENTRIES then
        num_of_new_entries = MAX_ENTRIES - numOfMsg
    end

    local rx_timestamp = os.time()
    for idx=1, num_of_new_entries do
        local time = os.date("%d/%m/%y - %H:%M:%S - gmt: +00:00", rx_timestamp)
        local fullpath = string.format('%s/rxmsg_%s_unread', wmmdconfig.incoming_sms_dir, os.date("%y%m%d%H%M%S", rx_timestamp))
        local fd = io.open(fullpath, "w+")

        if not fd then return false end

        fd:write(string.format(
            "From : 0123456789\n"
          .."Time : %s\n"
          .."LocalTimestamp : %s\n"
          .."Coding : UCS2\n"
          .."Wap : false\n"
          .."This is for Device Wipe test", time, rx_timestamp))

        fd:close()
        os.execute("sleep 1")
        rx_timestamp = rx_timestamp + 1
    end
    return true
end

function DeviceWipeHandler:put(path)
    local actionList = {
        ['call_log'] = l_set_call_log,
        ['sms'] = l_set_sms,
    }

    local action = path:gsub("(.*/)(.*)", "%2")
    if not actionList[action] then
        self:set_status(404) -- resource not found
        return
    end

    local result = actionList[action](self:get_json())
    self:set_status(result and 200 or 400)
end

-- Return array of path/handler mappings
return {
        {basepath .. "/device_wipe/[%w_]+$", DeviceWipeHandler},
}

