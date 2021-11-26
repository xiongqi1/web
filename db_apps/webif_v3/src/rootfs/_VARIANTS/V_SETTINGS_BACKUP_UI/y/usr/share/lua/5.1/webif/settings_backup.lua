--[[
    Script to handle settings save and restore

    Dummy module in order to reuse fileUploader class.

    Copyright (C) 2021 Casa Systems
--]]

require "stringutil"
require "tableutil"

local M = {}

function M.post(file, commit)
    if file:find("'") then
        -- defend against shell injection
        return
    end
    return "uploaded"
end

return M
