-- Copyright (C) 2021 Casa Systems.
-- monitoring ODU reboot

local m_turbo_util = require("turbo.util")
local m_files_manager = require("odu_dynamic_installer.files_manager")
local m_odu_entry = require("odu_dynamic_installer.odu_entry")

local m_ioloop

local odu_reboot = {
    PREPARING_REBOOT = "preparing_reboot",
    REBOOTING = "rebooting",
    REBOOTED = "rebooted",
    DISCONNECTED = "disconnected",
    TIMEOUT = "timeout"
}

local odu_reboot_status
local monitor_odu_reboot_ref
local monitor_odu_reboot_interval_secs
local monitor_odu_reboot_interval_num
local monitor_odu_reboot_max_interval_num

local function setup(ioloop)
    m_ioloop = ioloop
end

local function clear_monitor_odu_reboot()
    if monitor_odu_reboot_ref then
        -- not harmful to try to remove expired timer
        m_ioloop:remove_timeout(monitor_odu_reboot_ref)
        monitor_odu_reboot_ref = nil
    end
end

local function monitor_odu_reboot()
    monitor_odu_reboot_interval_num = monitor_odu_reboot_interval_num + 1
    local info = m_odu_entry.get_odu_info()
    if info.phy_connect then
        if info.interface_connect then
            if info.model and info.config_update_status then
                if odu_reboot_status == odu_reboot.REBOOTING then
                    odu_reboot_status = odu_reboot.REBOOTED
                    m_files_manager.on_odu_reboot_event(odu_reboot_status, info)
                    clear_monitor_odu_reboot()
                    return
                end
            end
        else
            if odu_reboot_status ~= odu_reboot.REBOOTING then
                odu_reboot_status = odu_reboot.REBOOTING
                m_files_manager.on_odu_reboot_event(odu_reboot_status)
                -- continue monitoring
            end
        end
    else
        odu_reboot_status = odu_reboot.DISCONNECTED
        m_files_manager.on_odu_reboot_event(odu_reboot_status)
        clear_monitor_odu_reboot()
        return
    end

    if monitor_odu_reboot_interval_num >= monitor_odu_reboot_max_interval_num then
        clear_monitor_odu_reboot()
        odu_reboot_status = odu_reboot.TIMEOUT
        m_files_manager.on_odu_reboot_event(odu_reboot_status)
    else
        monitor_odu_reboot_ref = m_ioloop:add_timeout(
            m_turbo_util.gettimemonotonic() + monitor_odu_reboot_interval_secs*1000,
            monitor_odu_reboot)
    end
end

local function setup_monitor_odu_reboot(interval_secs, max_interval_num)
    interval_secs = tonumber(interval_secs) or 5
    -- default 10 mins as it can take up to 6-7 minutes for ODU to reboot
    max_interval_num = tonumber(max_interval_num) or (60/interval_secs)*10

    clear_monitor_odu_reboot()

    monitor_odu_reboot_interval_secs = interval_secs
    monitor_odu_reboot_interval_num = 0
    monitor_odu_reboot_max_interval_num = max_interval_num
    monitor_odu_reboot_ref = m_ioloop:add_timeout(
        m_turbo_util.gettimemonotonic() + monitor_odu_reboot_interval_secs*1000,
        monitor_odu_reboot)
    odu_reboot_status = odu_reboot.PREPARING_REBOOT
end

local function is_monitoring_odu_reboot_in_progress()
    return monitor_odu_reboot_ref and true or false
end

return {
    setup = setup,
    setup_monitor_odu_reboot = setup_monitor_odu_reboot,
    is_monitoring_odu_reboot_in_progress = is_monitoring_odu_reboot_in_progress
}
