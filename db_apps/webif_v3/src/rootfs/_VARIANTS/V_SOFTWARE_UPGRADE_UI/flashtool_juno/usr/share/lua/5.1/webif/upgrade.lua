--[[
    Script to handle SW upgrade for Saturn/Neptune

    Copyright (C) 2020 Casa Systems
--]]

require "stringutil"
require "tableutil"

local messages = {
    in_progress = "upgradeInProgress",
    invalid = "invalidFirmware",
    uploaded = "firmwareUploaded",
    upgraded = "firmwareUpgraded",
    check = "checkConnection",
}

-- get the current running SW version and version of inactive partition
local function getVersion()
    -- everything after the first underscore
    return string.match(luardb.get("sw.version") or "", "^[^_]+_(.+)$")
end

--[[
    Preparation for post request.
--]]
local function prePostProcedure()

    local preCmdList = {
        -- To reserve stack memory.
        "/etc/init.d/start_wlan_services stop",
        "/etc/init.d/start_loc_launcher stop",
        "/etc/init.d/start_bluetooth stop",
        -- Preparation for firmware file transfer.
        "rdb set service.updating_software.command_status ''", -- Supposed to create a rdb variable before using rdb_wait with it.
        "rdb_set service.updating_software.command prepare",
        "rdb_wait service.updating_software.command_status",
    }
    os.execute(table.concat(preCmdList, "; "))

    return messages.in_progress
end

--[[
    start FW upgrade process

    [Input]
    uploaded_file: full path of uploaded file
    uploaded_commit = 1 : flash firmware

    [Output]
    processed_message: upgrade status
    extra_messages: extra status messages
    ping_url: url to poll web server online status
    ping_delay: an estimated reboot time
--]]
local function upgradeFirmware(uploaded_file, uploaded_commit)
    local cmd = string.format("rdb set service.updating_software.firmware_file %s", string.gsub(uploaded_file, "(.*/)(.*)", "%2"))
    cmd = string.format("%s; rdb set service.updating_software.command start", cmd)
    os.execute(cmd)

    local processed_message = messages.in_progress
    local extra_messages = messages.check .. ";" .. messages.upgraded
    local ping_delay = 240
    -- There is a CORS issue when fw upgrading with factory reset option.
    -- To solve this, ping to web_server_status which responses as JSONP
    local ping_url = "/web_server_status"
    return processed_message, extra_messages, ping_url, ping_delay
end

--[[
    verify uploaded FW file

    [Input]
    uploaded_file: full path of uploaded file

    [Output]
    processed_message: verification outcomoe
    extra_messages: FW file version
--]]
local function verifyFirmware(uploaded_file)
    local verifySucc = true

    -- verify the uploaded firmware is compatible with this product model
    local cmd = string.format("star '%s' -M fw.variants", uploaded_file)
    local res = string.trim(shellExecute(cmd))
    if #res > 0 then
        local compat_models = string.explode(res, ",")
        local model = luardb.get("system.product")
        if not table.contains(compat_models, model) then
            verifySucc = false
        end
    else
        verifySucc = false
    end

    local processed_message, extra_messages
    -- get fw.version
    cmd = string.format("star '%s' -M fw.version", uploaded_file)
    res = string.trim(shellExecute(cmd))
    if #res > 0 then
        extra_messages = res
    else
        extra_messages = ""
        verifySucc = false
    end

    -- verify other meta data of the uploaded firmware if necessary

    if verifySucc then
        processed_message = messages.uploaded
    else
        processed_message = messages.invalid
    end

    return processed_message, extra_messages
end

local M = {}

function M.get()
    return "", getVersion()
end

function M.post(file, commit)
    if file:find("'") then
        -- defend against shell injection
        return
    end
    if commit == -1 then
        return prePostProcedure()
    end
    if commit == 1 then
        return upgradeFirmware(file, commit)
    end
    return verifyFirmware(file)
end

return M
