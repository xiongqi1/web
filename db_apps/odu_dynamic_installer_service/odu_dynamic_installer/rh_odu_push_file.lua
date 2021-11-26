-- Copyright (C) 2021 Casa Systems.
-- request handler: Pushing file to ODU

local m_turbo = require("turbo")
local m_files_manager = require("odu_dynamic_installer.files_manager")
local m_odu_entry = require("odu_dynamic_installer.odu_entry")
local m_monitor_odu_reboot = require("odu_dynamic_installer.monitor_odu_reboot")

local OduPushFileHandler = class("OduPushFileHandler", m_turbo.web.RequestHandler)

function OduPushFileHandler:post()
    local odu_status = m_odu_entry.get_odu_connection_status()
    if not odu_status.phy_connect then
        self:write({status = "error", errorMsg = "ODU not connected"})
        return
    end
    if not odu_status.interface_connect then
        self:write({status = "error", errorMsg = "Connection to ODU is unavailable"})
        return
    end

    local in_file_name = self:get_argument("fileName")
    local files_table = m_files_manager.get_star_files_with_odu()
    local file_info = files_table[in_file_name]
    if not file_info then
        self:write({status = "error", errorMsg = "Not found"})
        return
    end

    if m_monitor_odu_reboot.is_monitoring_odu_reboot_in_progress() then
        self:write({status = "error", errorMsg = "ODU rebooting is in progress"})
        return
    end

    -- no checking compatibility and already-update
    -- assuming that at this step client already knows that and wants to push file anyway
    -- the outcome will be decided by the ODU server

    m_files_manager.mark_pushing_file_to_odu(in_file_name)

    local res, err = m_odu_entry.push_file_to_odu(m_files_manager.get_file_path(in_file_name),
        file_info.metadata["type"] == "firmware")
    if res then
        self:write({status = "success", errorMsg = ""})
        if file_info.metadata.reboot then
            m_files_manager.setup_monitor_odu_processed_file(in_file_name)
            m_monitor_odu_reboot.setup_monitor_odu_reboot()
        else
            m_files_manager.mark_odu_processed_file_success(in_file_name)
        end
    else
        self:write({status = "error", errorMsg = err})
        m_files_manager.mark_odu_processed_file_failed(in_file_name, err)
    end
end

return OduPushFileHandler
