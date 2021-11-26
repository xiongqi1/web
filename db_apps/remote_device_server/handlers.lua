-- Copyright (C) 2018 NetComm Wireless limited.
--
-- Common web request handlers.

require('variants')
require('luardb')
local turbo = require("turbo")

local config = require("rds.config")
local basepath = config.basepath
local logger = require("rds.logging")

--------- System management handler ----------
local SystemManagementHandler = class("SystemManagementHandler", turbo.web.RequestHandler)

function SystemManagementHandler:put(path)
    -- System reboot handler
    local function l_reboot(req)

        if variants.V_PARTITION_LAYOUT == "fisher_ab" then
            -- Normal reboot. Reset BOOT_COUNT
            os.execute("flashtool --accept")
        end

        luardb.set('service.system.reset', '1')
        return true
    end

    local actionList = {
        ['reboot'] = l_reboot,
    }

    local action = path:gsub("(.*/)(.*)", "%2")
    if not actionList[action] then
        self:set_status(404) -- resource not found
        return
    end

    local req = self:get_json()
    local result = actionList[action](req)
    self:set_status(result and 200 or 400)
end

--------- System Product Info handler ----------
local SystemProductInfoHandler = class("SystemProductInfoHandler", turbo.web.RequestHandler)

-- To get value of rdb variable prefixed with "system.product."
function SystemProductInfoHandler:get(path)
    local rdb_prefix = "system.product."
    local fd = io.popen("rdb list " .. rdb_prefix)

    if not fd then
        self:set_status(404) -- resource not found
        return
    end
    local resp = {}
    for line in fd:lines() do
        local rdb_name = line:match(rdb_prefix .. "([_.%w]+)$")
        if rdb_name then
            resp[rdb_name] = luardb.get(rdb_prefix .. rdb_name)
        end
    end

    self:write(resp)
    self:set_status(200)
end

--------- System log service handler ----------
local SystemLogServiceHandler = class("SystemLogServiceHandler", turbo.web.RequestHandler)
local syslog_rdb_prefix = "service.syslog.option."

-- To get value of rdb variable prefixed with "service.syslog.option."
function SystemLogServiceHandler:get(path)
    local fd = io.popen("rdb list " .. syslog_rdb_prefix)

    if not fd then
        self:set_status(404) -- resource not found
        return
    end
    local resp = {}
    for line in fd:lines() do
        local rdb_name = line:match(syslog_rdb_prefix .. "([_.%w]+)$")
        if rdb_name then
            resp[rdb_name] = luardb.get(syslog_rdb_prefix .. rdb_name)
        end
    end

    self:write(resp)
    self:set_status(200)
end

function SystemLogServiceHandler:put(path)
    local result = false
    -- the req is associative array(key: rdb suffix, value: new value)
    -- Ex: {logtofile='1', numofrotatedlogs='4', sizekb='10240'}
    -- Note: Do not need to explicitly set trigger with json request.
    local req = self:get_json()
    if type(req) == 'table' then
        for k, v in pairs(req) do
            if k ~= 'trigger' then
                luardb.set(syslog_rdb_prefix .. k, v)
            end
        end
        -- trigger when any of rdb variable is set.
        if next(req) then
            luardb.set(syslog_rdb_prefix .. 'trigger', '1')
        end
        result = true
    end

    self:set_status(result and 200 or 400)
end

-- Return array of path/handler mappings
return {
    {basepath .. "/system_management/%w+$", SystemManagementHandler},
    {basepath .. "/system_product_info$", SystemProductInfoHandler},
    {basepath .. "/system_log$", SystemLogServiceHandler},
}
