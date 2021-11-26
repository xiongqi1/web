-- Copyright (C) 2021 Casa Systems.
-- request handler: get files list

local m_turbo = require("turbo")
local m_files_manager = require("odu_dynamic_installer.files_manager")

local OduFilesListHandler = class("OduFilesListHandler", m_turbo.web.RequestHandler)

function OduFilesListHandler:get()
    local files_table = m_files_manager.get_star_files_with_odu()
    local files_list = {}
    for _, data in pairs(files_table) do
        files_list[#files_list + 1] = {
            fileName = data.file_name,
            metadata = data.metadata,
            compatWithOdu = data.compat_with_odu,
            alreadyUpdatedOdu = data.already_updated_odu
        }
    end

    self:write({fileList = files_list})
end

return OduFilesListHandler
