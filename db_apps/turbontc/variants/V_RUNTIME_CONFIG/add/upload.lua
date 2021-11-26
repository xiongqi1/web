local luardb = require("luardb")
local turbo = require("turbo")

local authorizer = {}
local UploadConfigHandler = class("UploadConfigHandler", turbo.web.RequestHandler)

function UploadConfigHandler:put(url)
    if not authorizer.da_authorize or not authorizer.da_authorize(self, "config") then
        error(turbo.web.HTTPError(401, "Not Authorized"))
    end

    local file = self:get_argument("file", "ERROR")
    local fileName= self:get_argument("filename", "ERROR")

    if not file or "ERROR" == file or not fileName or "ERROR" == fileName or
       string.find(fileName, "..", 1, true)  then
        error(turbo.web.HTTPError(400, "File not uploaded"))
    end

    -- write file content to disk
    fileName = "/opt/cdcs/upload/" .. fileName
    local fobj = io.open(fileName, "wb")
    if nil == fobj then
        error(turbo.web.HTTPError(500, "File not stored"))
    end
    fobj:write(file)
    fobj:close()
    file = nil

    -- trigger the runtime config processing - apply_runtime_config.template
    luardb.set("service.runtime_config.filename", fileName)
    luardb.set('service.runtime_config.commit', 1)
    self:set_status(200)
end

-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(maps, handler, util, authorizer_)
    table.insert(handler, {'^/upload/config$', UploadConfigHandler})
    authorizer = authorizer_
end

return module
