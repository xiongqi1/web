--[[
    A handler module to perform swivelling scan operation

    Copyright (C) 2021 Casa Systems Inc.
--]]
require("lfs")
local luardb = require("luardb")
local turbo = require("turbo")

-- Create a handler to perform swivelling scan operation
local SwivellingScanHandler = class("SwivellingScanHandler", turbo.web.RequestHandler)

-- do swivelling scan operation
-- url : moveToHomePosition - goto home position
--       startSwivellingScan - start swivelling scan
--       stopSwivellingScan - stop swivelling scan
function SwivellingScanHandler:post(url)
    turbo.log.debug('SwivellingScanHandler:post('..url..')')
    local csrf_session = getCsrfSession()
    csrf_session.prepareSession(self)

    -- login is required for swivelling scan
    if not validateLogin(self) then
        error(turbo.web.HTTPError(401, "Unauthorised"))
    end

    if url ~= "moveToHomePosition" and
       url ~= "startSwivellingScan" and
       url ~= "stopSwivellingScan" then
        error(turbo.web.HTTPError(404, "Invalid URL"))
    end

    local session = self.session
    local csrfTokenPost = self:get_argument("csrfTokenPost", "")
    if not csrf_session.verifyCsrfToken(session.csrf, csrfTokenPost) then
        error(turbo.web.HTTPError(403, "Invalid csrfToken"))
    end

    self:add_header('Content-Type', 'application/json')

    local params = {}
    if url == "startSwivellingScan" then
        params = {"param", '{"force":true}\0'}
    end
    local rval, result = luardb.invoke("SwivellingScan", url, 3, 199, unpack(params))

    local response = {
        result = result,
        text = rval
    }
    turbo.log.debug(string.format("SwivellingScanHandler:result = %s", tostring(result)))

    self:write(response)
    csrf_session.updateSession(self)
end


-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(handlers)
    table.insert(handlers, {"^/swivelling_scan/(.*)$", SwivellingScanHandler})
end

return module
