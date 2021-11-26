--[[
    A handler module to process settings backup and restore requests

    Copyright (C) 2021 Casa Systems Inc.
--]]
require("lfs")
require("support")
require('srvrUtils')
local luardb = require("luardb")
local g_turbo_dir = "/usr/share/lua/5.1/webif"
local turbo = require("turbo")

-- Create a handler to to process server certificate requests
local SettingsBackupHandler = class("SettingsBackupHandler", turbo.web.RequestHandler)

-- Save current settings to backup file or restore saved settings.
-- url : save / restore
function SettingsBackupHandler:post(url)
    turbo.log.notice('SettingsBackupHandler:post('..url..')')
    local csrf_session = getCsrfSession()
    csrf_session.prepareSession(self)

    -- login is required for server certificate function
    if not validateLogin(self) then
        error(turbo.web.HTTPError(401, "Unauthorised"))
    end

    if url ~= "save" and url ~= "restore" then
        error(turbo.web.HTTPError(404, "Invalid URL"))
    end

    local session = self.session
    local csrfTokenPost = self:get_argument("csrfToken", "")
    if not csrf_session.verifyCsrfToken(session.csrf, csrfTokenPost) then
        error(turbo.web.HTTPError(403, "Invalid csrfToken"))
    end

    self:add_header('Content-Type', 'application/json')

    local password = self:get_argument("password", "")
    -- password is base64 encoded string and will be base64 decoded in settings_backup.sh.
    local cmd = "/usr/bin/settings_backup.sh " .. url.. " " ..password
    local result  = executeCommand(cmd)
    local response = {}
    local data = {}
    if not result or result[1] == "error" then
        error(turbo.web.HTTPError(500, "failed to " ..url.. " current settings"))
        csrf_session.updateSession(self)
        return
    end
    if url == "save" then
        -- /www/config is symlinked to V_EXPORTCONFPATH (/tmp/config for Cassini)
        data.filename = "/config/" ..result[1]
    elseif url == "restore" then
        for _, v in ipairs(result) do
            local rdbVal = v:explode("=")
            data[rdbVal[1]] = rdbVal[2]
            turbo.log.debug(string.format("data[%s] = %s", rdbVal[1], rdbVal[2]))
        end
    end
    response.result = 0
    response.data = data
    self:write(response)
    csrf_session.updateSession(self)
end

-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(handlers)
    table.insert(handlers, {"^/settings_backup/(.*)$", SettingsBackupHandler})
end

return module
