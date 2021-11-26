#!/usr/bin/env lua
--------------------------------------------------------------------------------
-- This is the generic IA UI, showing waiting indicator before OWA type is identified
--
-- Copyright (C) 2020 Casa Systems.
--
--------------------------------------------------------------------------------

_G.TURBO_SSL = true

-- (for development only) disabling static cache
-- _G.TURBO_STATIC_MAX = -1

require('stringutil')

local turbo = require("turbo")
local luardb = require("luardb")
local llog = require("luasyslog")

pcall(function() llog.open("ia-generic", "LOG_DAEMON") end)

-- directory where IA libraries are located
local g_ia_dir = "/usr/share/installation_assistant/generic/"
-- in test mode, if already done cd directly to the installation assistant directory,
-- then simply set g_ia_dir to "" before running.
if #g_ia_dir > 0 then
    package.path = package.path .. ";" .. g_ia_dir .. "?.lua"
end

require("support")

-- directory where client code (HTML and Javascript) are located
local g_client_dir = g_ia_dir .. "client/"

-- request handlers
local KeepAliveHandler = class("KeepAliveHandler", turbo.web.RequestHandler)

-- handle keep-alive request
function KeepAliveHandler:get()
    local ui_model = luardb.get("installation.ui_model") or ""
    local response = {
        owa_sync_status = (#ui_model > 0 and "synchronised" or "")
    }
  -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")
    self:write(response)
end

-- Loads configuration before starting any applications
local config = load_overriding_module(g_ia_dir, 'config')

local app = turbo.web.Application:new({
    {"^/keep_alive$", KeepAliveHandler},
    {"^/$", turbo.web.StaticFileHandler, g_client_dir .. 'index.html'},
    {"^/(.*)$", turbo.web.StaticFileHandler, g_client_dir}
})

app:listen(config.listen_port)

local instance = turbo.ioloop.instance()

instance:start()
