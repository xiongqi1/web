-- Copyright (C) 2021 Casa Systems.
-- request handler: delete files

local m_turbo = require("turbo")
local m_files_manager = require("odu_dynamic_installer.files_manager")

local DeleteFilesHandler = class("DeleteFilesHandler", m_turbo.web.RequestHandler)

function DeleteFilesHandler:post()
    local files_list = self:get_json()

    if not files_list or type(files_list) ~= "table"
            or type(files_list.fileNames) ~= "table" or #files_list.fileNames == 0 then
        self:write({status = "error", errorMsg = "Invalid request"})
        return
    end

    local error_msg
    for _, file_name in pairs(files_list.fileNames) do
        if m_files_manager.is_file_busy(file_name, true) or not m_files_manager.delete_file(file_name) then
            if not error_msg then
                error_msg = "Failed to delete "..file_name
            else
                error_msg = error_msg..", "..file_name
            end
        end
    end

    local status = error_msg and "error" or "success"
    local out = {
        status = status,
        errorMsg = error_msg,
    }

    self:write(out)
end

return DeleteFilesHandler
