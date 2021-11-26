-- Copyright (C) 2018 NetComm Wireless limited.
--
-- Power saving mode handlers for hunter.

require('luardb')
local turbo = require("turbo")

local config = require("rds.config")
local basepath = config.basepath
local logger = require("rds.logging")

--------- Power saving mode handler ----------
local PowerSavingModeHandler = class("PowerSavingModeHandler", turbo.web.RequestHandler)

-- system.power_saving.type [wait|always_on|immediately_off]
-- immediately_off(Maximum Saving):Turn off Ethernet, WiFi while on Battery Backup
-- wait(Balanced): Retain WiFi and Ethernet Connectivity for specific duration on backup power,
--                or until the battery is 50%.
-- always_on(None): Keep WiFi and Ethernet Connectivity enabled when on battery backup power
local rdb_psm_type = "system.power_saving.type"
-- system.power_saving.retain_sec_on_batt <sec>
local rdb_psm_dur = "system.power_saving.retain_sec_on_batt"

function PowerSavingModeHandler:get(path)
    local resp = {
        psm_mode = luardb.get(rdb_psm_type) or 'wait',
        psm_retain_sec = luardb.get(rdb_psm_dur) or '300',
    }
    self:write(resp)
    self:set_status(200)
end

function PowerSavingModeHandler:put(path)
    local result = false
    local mode_list = {immediately_off = true, wait = true, always_on = true}
    local req = self:get_json()
    if req then
        if req.psm_retain_sec then
            luardb.set(rdb_psm_dur, req.psm_retain_sec)
            result = true
        end
        if req.psm_mode and mode_list[req.psm_mode] then
            luardb.set(rdb_psm_type, req.psm_mode)
            result = true
        end
    end

    self:set_status(result and 200 or 400)
end

-- Return array of path/handler mappings
return {
    {basepath .. "/power_saving_mode", PowerSavingModeHandler},
}
