--[[
    A handler module to process server certificate requests

    Copyright (C) 2020 Casa Systems Inc.
--]]
require("lfs")
local luardb = require("luardb")
local turbo = require("turbo")

-- Create a handler to to process server certificate requests
local ServerCertiHandler = class("ServerCertiHandler", turbo.web.RequestHandler)

local function runKeyGenCmd(cmd)
    local res = shellExecute(cmd, false)
    if #res > 0 then
        return 0, res
    else
        return 1, "error"
    end
end

-- get server certificate information
-- url : info - get certificate infomation
-- do not check cookies/csrf here
function ServerCertiHandler:get(url)
    turbo.log.debug('ServerCertiHandler:get('..url..')')
    self:add_header('Content-Type', 'application/json')

    if url ~= "info" then
        error(turbo.web.HTTPError(404, "Invalid URL"))
    end

    local cmd = string.format("ca_keygen.sh %s", url)
    local result, messages = runKeyGenCmd(cmd)
    --[[ certificate information example
        # ca_keygen.sh.sh info
        server_certificate = "AU,NSW,Lane Cove,Casa Systems,,Kwonhee.Han@Casa-systems.com/Dec 11 20:37:22 2020 GMT,Dec 9 20:37:22 2030 GMT/";
        server_certificate_serial_no = "51A48E2B3013B93A114C9C7A13D2B4E138E78EB9";
        server_secret_time = "";
    ]]--
    if result == 0 then
        local response = {}
        response["result"] = "0"
        messages = string.gsub(messages, "\n", "")
        local msg = string.explode(messages, ";")
        for _, v in ipairs(msg) do
            if string.find(v, '=') then
                v = string.gsub(v, "^\n", "")
                local idx, val = string.match(v, "([^%s]+)%s*=%s*(.+)")
                if idx and val then
                    val = string.gsub(string.gsub(val, "^\"", ""), "\"$", "")
                    response[idx] = val
                end
            end
        end
        self:write(response)
    else
        error(turbo.web.HTTPError(500, "Internal server error"))
    end
end

-- generate server certificate
-- url : gen_ca - generate server certificate
-- do not check cookies/csrf here
function ServerCertiHandler:post(url)
    turbo.log.debug('ServerCertiHandler:post('..url..')')
    local csrf_session = getCsrfSession()
    csrf_session.prepareSession(self)

    -- login is required for server certificate function
    if not validateLogin(self) then
        error(turbo.web.HTTPError(401, "Unauthorised"))
    end

    if url ~= "gen_ca" then
        error(turbo.web.HTTPError(404, "Invalid URL"))
    end

    local session = self.session
    local csrfTokenPost = self:get_argument("csrfTokenPost", "")
    if not csrf_session.verifyCsrfToken(session.csrf, csrfTokenPost) then
        error(turbo.web.HTTPError(403, "Invalid csrfToken"))
    end

    self:add_header('Content-Type', 'application/json')

    -- Should call os.execute rather than other server functions in order to
    -- send the response back as soon as possible and keep running time-consuming
    -- process in background
    local result = os.execute("ca_keygen.sh ca")
    if result == 0 then
        self:write('{"result":"0"}')
    else
        error(turbo.web.HTTPError(500, "Internal server error"))
    end

    csrf_session.updateSession(self)
end


-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(handlers)
    table.insert(handlers, {"^/server_certi/(.*)$", ServerCertiHandler})
end

return module
