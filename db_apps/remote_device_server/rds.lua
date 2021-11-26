#!/usr/bin/env lua

-- Copyright (C) 2018 NetComm Wireless limited.
--
-- turbo lua webserver for responding to remote device client requests.

-- TODO
--  Add template to start webserver; start-stop-daemon(while loop run/wait script)

local logger = require("rds.logging")
local turbo = require("turbo")
local config = require("rds.config")

-- Merge handlers from all lua modules found in /usr/share/rds
--
-- @return Merged table of handlers.
local function gatherHandlers()
    local allHandlers = {}
    local handlerFiles = io.popen("find /usr/share/rds -name '*.lua'")
    for file in handlerFiles:lines() do
        for _, handler in ipairs(dofile(file)) do
            table.insert(allHandlers, handler)
        end
    end
    return allHandlers
end

local handlers = gatherHandlers()

local app = turbo.web.Application:new(handlers)

logger.logNotice("starting server")
app:listen(config.port, config.bind_address)
turbo.ioloop.instance():start()
