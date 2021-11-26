-- Copyright (C) 2021 Casa Systems.
-- request handler: upload file

local m_turbo = require("turbo")
local m_files_manager = require("odu_dynamic_installer.files_manager")
local m_config = require("odu_dynamic_installer.config")
local m_monitor_odu_reboot = require("odu_dynamic_installer.monitor_odu_reboot")

local FileUploadHandler = class("FileUploadHandler", m_turbo.web.RequestHandler)

FileUploadHandler.uri = ""

function FileUploadHandler.buildFileUploadFullPath(file_name, uri, method)
    if uri ~= FileUploadHandler.uri or method ~= "POST" then
        return false, 405, "Method Not Allowed"
    end
    if m_files_manager.validate_uploaded_file_name(file_name) then
        if m_files_manager.is_file_busy(file_name, true) then
            return false, 409, "File is busy. Replacing is not allowed."
        end
        return m_config.upload_dir.."/"..file_name
    end

    return false
end

function FileUploadHandler:post()
    local in_file_name = self:get_argument("name")
    local in_file = self:get_argument("file", "")

    local error_msg
    local fp_error_code, fp_error_msg

    -- handle small file
    if in_file and in_file ~= "" then
        local file_path, fp_error_code, fp_error_msg = self.buildFileUploadFullPath(in_file_name, FileUploadHandler.uri, "POST")
        if file_path then
            local file_obj = io.open(file_path, "wb")
            if file_obj then
                local w_status, w_err = file_obj:write(in_file)
                file_obj:close()
                if not w_status then
                    error_msg = w_err
                    os.remove(file_path)
                end
            else
                error_msg = "Unable to prepare file to write"
            end

        else
            -- send error code to match the corresponding behaviour for big file
            error(m_turbo.web.HTTPError(fp_error_code or 400, fp_error_msg or "Invalid file name"))
            return
        end
    end

    if error_msg then
        self:write({status = "error", errorMsg = error_msg})
        return
    end

    local files_table = m_files_manager.get_star_files_with_odu(false, in_file_name)
    local file_info = files_table[in_file_name]
    if not file_info then
        self:write({status = "error", errorMsg = "File not found or invalid"})
        -- try to clean up
        m_files_manager.delete_file(in_file_name)
        return
    end

    local out = {
        status = "success",
        errorMsg = "",
        metadata = file_info.metadata,
        compatWithOdu = file_info.compat_with_odu,
        alreadyUpdatedOdu = file_info.already_updated_odu
    }

    self:write(out)
end

return FileUploadHandler
