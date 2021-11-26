--[[
    RDS handler for MEP lock

    Copyright (C) 2018 NetComm Wireless Limited.
--]]

local turbo = require("turbo")
local util = require("rds.util")

local config = require("rds.config")
local basepath = config.basepath

local rdb = require("luardb")

local MepLockHandler = class("MepLockHandler", turbo.web.RequestHandler)

local PREFIX = 'wwan.0.sim.mep'
local UNLOCK_TIMEOUT_SECS = 15

--[[
    IPC handler for status call
    client lua call example - rdc:get("meplock.status")
    HTTP resp code: 200 or 404
    HTTP resp json: {"state":<MEP lock state>, "retries":<retries left>}
--]]
function MepLockHandler:get(path)
    -- path: /mdm/meplock/status
    if not path:match("/status$") then
        self:set_status(404) -- resource not found
        return
    end
    local state = rdb.get(PREFIX .. ".state")
    local retries = tonumber(rdb.get(PREFIX .. ".retries"))
    self:write({state=state, retries=retries})
    self:set_status(200)
end

--[[
    IPC handler for unlock call
    client lua call example - rdc:set("meplock.unlock", '{"nck":"123456"}')
    HTTP resp code: 200, 400 or 404
--]]
function MepLockHandler:put(path)
    -- path: /mdm/meplock/unlock
    if not path:match("/unlock$") then
        self:set_status(404) -- resource not found
        return
    end
    local req = self:get_json()
    local code = 400 -- bad request
    if req and req.nck and #req.nck > 0 and #req.nck <= 16 then
        -- start a new thread to perform the blocking rdb invoke
        local thread = turbo.thread.Thread(function(th)
                --[ real rdb invoke
                local ret = rdb.invoke(PREFIX, "deactivate", UNLOCK_TIMEOUT_SECS, 0, "nck", req.nck)
                --]]
                --[[ mock rdb invoke for testing
                os.execute('sleep 10')
                local ret = #req.nck == "123" and 0 or 1
                --]]
                th:send(ret == 0 and "200" or "400")
                th:stop()
        end)
        -- wait_for_data will yield when data is not available
        code = tonumber(thread:wait_for_data())
        thread:wait_for_finish()
    end
    self:set_status(code)
end

return {{basepath .. "/meplock/%w+$", MepLockHandler}}
