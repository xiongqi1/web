-- Copyright (C) 2021 Casa Systems.
-- request handler: get file info

local m_turbo = require("turbo")
local m_files_manager = require("odu_dynamic_installer.files_manager")

local OduFileInfoHandler = class("OduFileInfoHandler", m_turbo.web.RequestHandler)

function OduFileInfoHandler:get()
    local in_file_name = self:get_argument("fileName")

    local files_table = m_files_manager.get_star_files_with_odu()
    local file_info = files_table[in_file_name]
    if not file_info then
        self:write({errorMsg = "Not found"})
        return
    end

    local progress_status, error_msg = m_files_manager.get_file_progress_status(in_file_name)

    local out = {
        progressStatus = progress_status,
        errorMsg = error_msg,
        metadata = file_info.metadata,
        compatWithOdu = file_info.compat_with_odu,
        alreadyUpdatedOdu = file_info.already_updated_odu
    }

    self:write(out)
end

return OduFileInfoHandler
