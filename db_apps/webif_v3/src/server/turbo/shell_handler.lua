--[[
    Simple shell output handler class.
    Copyright (C) 2021 Casa Systems Inc.
--]]

local turbo = require("turbo")

local ShellHandler = class("ShellHandler", turbo.web.RequestHandler)

function ShellHandler:prepare()
    if type(self.options) ~= "string" then
        error("ShellHandler not initialized with correct parameters.")
    end
    self.cmd = self.options
end

--- GET method for shell command output.
-- @param path The path captured from request.
function ShellHandler:get(path)
    local file, err = io.popen(self.cmd, "r")
    if not file then
        error(turbo.web.HTTPError(404))
    end

    local sz = 1024*32 -- 32KB chunks seems like a good value?
    self:set_chunked_write()
    while (true)
    do
        local data = file:read(sz)
        if not data then
            break
        end
        self:write(data)
        self:flush()
        if (data:len() < sz) then
            break
        end

    end
    file:close()
end
return ShellHandler
