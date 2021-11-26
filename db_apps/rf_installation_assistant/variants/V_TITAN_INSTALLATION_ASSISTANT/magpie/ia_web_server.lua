#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
-- ia_web_server.lua is an executable script that implements a Turbo based web server
-- for supporting Titan Installation Assistant functionality
--
_G.TURBO_SSL = true

-- (for development only) disabling static cache
-- _G.TURBO_STATIC_MAX = -1

require('stringutil')

local turbo = require("turbo")
local luardb = require("luardb")
local llog = require("luasyslog")

pcall(function() llog.open("installation assistant", "LOG_DAEMON") end)

-- directory where IA libraries are located
local g_ia_dir = "/usr/share/installation_assistant/"
-- in test mode, if already done cd directly to the installation assistant directory,
-- then simply set g_ia_dir to "" before running.
if #g_ia_dir > 0 then
    package.path = package.path .. ";" .. g_ia_dir .. "?.lua"
end
require("support")
data_collector = require("data_collector")

local m_cbrs = require("cbrs")

-- directory where client code (HTML and Javascript) are located
local g_client_dir = g_ia_dir .. "client/"

-- Websocket report filter, default no filter enabled
local g_ws_report_filter = 0

-- directory to generate QR code
local g_qr_code_dir = "/tmp/generated_qr_code/"
-- QR code image size (square, so width or height) in pixels
local g_qr_image_size = 370
-- create the qr code directory
os.execute("mkdir -p "..g_qr_code_dir)

-- rdb_bridge connection status
local g_bridge_status_rdb = "service.rdb_bridge.connection_status"

local g_battery_percentage_threshold_update_firmware = 20
-- directory to find available firmware files
local g_firmware_directory = "/mnt/emmc/firmware/"
-- detected firmware meta informaton list
local g_firmware_list = {}
-- whether firmware list is available (e.g. firmware directory is available)
local g_firmware_list_available = false
-- meta information of target firmware version to upgrade to; nil means not upgrading
local g_upgrading_firmware
-- upgrading firmware stage
local g_upgrading_fw_stages = {
    NO_UPDATING = 0,
    TRIGGERED = 1,
    DOWNLOADING = 2,
    FLASHING = 3,
    PREPARE_REBOOT = 4,
    REBOOTING = 5,
    REBOOTED = 6,
    DONE = 7,
    FAILED = 8,
}
local g_upgrading_firmware_stage = g_upgrading_fw_stages.NO_UPDATING
-- upgrading firmware status report messages
local g_upgrading_fw_messages = {}
local g_getting_flashing_messages = false
local g_done_flashing_detected = false
-- time counter during rebooting
local g_time_counter_during_rebooting = 0

local g_battery_percentage_threshold_factory_reset = 10
-- factory reset stage
local g_factory_reset_stages = {
    NO_FACTORY_RESET = 0,
    TRIGGERED = 1,
    UNREGISTER_CBRS = 2,
    EXECUTE_FACTORY_RESET = 3,
    FACTORY_RESET_IN_PROGRESS = 4,
    REBOOTING = 5,
    REBOOTED = 6,
    DONE = 7,
    FAILED = 8,
}
local g_factory_reset_stage = g_factory_reset_stages.NO_FACTORY_RESET
local g_time_counter_during_rebooting_factory_reset = 0
local g_time_counter_unregister_cbrs_factory_reset = 0

local g_in_critical_process = false

-- for LED processing
local g_upgrade_fw_led_rdb = "owa.update"

-- request handlers
local JsonRequestHandlerRfCurrent = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerBattery = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerVersions = class("JsonRequestHandler", turbo.web.RequestHandler)
local UserConfDataEntry = class("JsonRequestHandler", turbo.web.RequestHandler)
local OperatingConfig = class("OperatingConfig", turbo.web.RequestHandler)
local FtpServerConfigure = class("JsonRequestHandler", turbo.web.RequestHandler)
local CpiParamConfigure = class("JsonRequestHandler", turbo.web.RequestHandler)
local SasRegister = class("JsonRequestHandler", turbo.web.RequestHandler)
local SasDeregister = class("JsonRequestHandler", turbo.web.RequestHandler)
local MountType = class("MountType", turbo.web.RequestHandler)
local SetWsFilter = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerRunTtest = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestControl = class("JsonRequestHandler", turbo.web.RequestHandler)
local QrCodeGenerateHandler = class("QrCodeGenerateHandler", turbo.web.RequestHandler)
local QrCodeImage = class("QrCodeImage", turbo.web.RequestHandler)
local OrientationData = class("OrientationData", turbo.web.RequestHandler)
local OwaFirmwareUpgrade = class("OwaFirmwareUpgrade", turbo.web.RequestHandler)
local WsOwaFirmwareUpgradeReport = class("WsOwaFirmwareUpgradeReport", turbo.websocket.WebSocketHandler)
local OwaFactoryReset = class("OwaFactoryReset", turbo.web.RequestHandler)
local UploadCpiPKCS12 = class("UploadCpiPKCS12", turbo.web.RequestHandler)
local KeepAlive = class("KeepAlive", turbo.web.RequestHandler)
local InstallationState = class("InstallationState", turbo.web.RequestHandler)
local WSExHandler = class("WSExHandler", turbo.websocket.WebSocketHandler)

-- web socket related configuration
g_ws_cfg= {
    -- web socket controls the frequency of data updates sent by us (the server)
    WS_BATT_UPDATE_TIME_SECONDS = 1,
    WS_ORIENTATION_UPDATE_TIME_SECONDS = 1,
    WS_STATS_UPDATE_TIME_SECONDS = 10,
    WS_RF_CURR_UPDATE_TIME_SECONDS = 1,
    WS_TTEST_CURR_UPDATE_TIME_SECONDS = 1,
    WS_SYS_STATUS_UPDATE_TIME_SECONDS = 3,
    WS_ANT_STATUS_UPDATE_TIME_SECONDS = 1,
    WS_REG_STATUS_UPDATE_TIME_SECONDS = 1,
    WS_UPGRADING_FW_STATUS_TIME_SECONDS = 1,
    WS_FACTORY_RESET_STATUS_TIME_SECONDS = 1,
}

-- on first iteration, send straight away to load the page data faster
local g_batt_count = g_ws_cfg.WS_BATT_UPDATE_TIME_SECONDS
local g_curr_count = g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS
local g_ttest_count = g_ws_cfg.WS_TTEST_CURR_UPDATE_TIME_SECONDS
local g_ant_status_count = g_ws_cfg.WS_ANT_STATUS_UPDATE_TIME_SECONDS
local g_reg_status_count = g_ws_cfg.WS_REG_STATUS_UPDATE_TIME_SECONDS
local g_orientation_count = g_ws_cfg.WS_ORIENTATION_UPDATE_TIME_SECONDS
local g_upgrading_fw_status_count = g_ws_cfg.WS_UPGRADING_FW_STATUS_TIME_SECONDS
local g_factory_reset_status_count = g_ws_cfg.WS_FACTORY_RESET_STATUS_TIME_SECONDS

-- Handle POST request to generate QR code
function QrCodeGenerateHandler:post()
    local date_time = self:get_argument("dateTime", nil, true)
    -- format YYYYMMDDHHMMSS
    if string.find(date_time, "^%d+$") and #date_time == 14 then
        local ban_number = luardb.get(config.get_rdb_prefix.."installation.customer_ban")
        if ban_number then
            local file_name = ban_number.."_"..date_time..".png"
            local file_path = g_qr_code_dir..file_name
            local error_log_path = file_path..".error"
            -- delete previously generated qr code
            os.execute("rm -f "..g_qr_code_dir.."*")
            -- generate qr code
            local ret = os.execute("qr_code_generator "..file_path.." "..g_qr_image_size.." "..date_time.." 2>"..error_log_path)
            if ret == 0 then
                -- success
                self:set_status(200)
                self:write(file_name)
            else
                self:set_status(500)
                local error_file = io.open(error_log_path, "rb")
                if error_file then
                    self:write(error_file:read("*a"))
                    error_file:close()
                else
                    self:write("Unable to capture error log.")
                end
            end
        else
            self:set_status(503)
            self:write("Customer BAN is required.")
        end
    else
        self:set_status(400)
        self:write("Date/Time is required.")
    end
end

-- StaticFileHandler adds small file content (default < 1MB) to a cache buffer.
-- As multiple QR code images may be generated while each is accessed/downloaded only once,
-- adding those image content to buffer is undesirable.
-- Hence this class QrCodeImage is to load the file content without adding to cache.
function QrCodeImage:get(file_name)
    if not file_name or type(file_name) ~= "string" or string.match(file_name, "%.%.") then
        self:set_status(404)
        return
    end
    local fd = io.open(g_qr_code_dir..file_name, "rb")
    if not fd then
        self:set_status(404)
        return
    end
    local content = fd:read("*a")
    if not content then
        self:set_status(404)
    else
        self:set_status(200)
        self:add_header("Content-Length", tonumber(content:len()))
        self:add_header("Content-Type", "image/png")
        self:write(content)
    end

    fd:close()
end

-- get list of uploaded OWA firmware files that are compatible with the connected OWA
-- meta information of those OWA firmware files will be stored in table g_firmware_list
function get_firmware_list()
    -- if already loaded, no need to load firmware list again
    if g_firmware_list_available and #g_firmware_list > 0 then
        return
    end
    -- check whether firmware directory is available
    if os.execute("ls "..g_firmware_directory.." >/dev/null 2>&1") == 0 then
        g_firmware_list_available = true
    else
        g_firmware_list_available = false
        g_firmware_list = {}
        return
    end

    local product_model = luardb.get(config.get_rdb_prefix.."system.product.model")
    if not product_model then
        g_firmware_list_available = false
        g_firmware_list = {}
        return
    end

    g_firmware_list = {}

    local dir_handle = io.popen("ls "..g_firmware_directory.."*.txt 2>/dev/null")
    if dir_handle then
        local file_name = dir_handle:read("*l")
        while file_name do
            local file_handle = io.open(file_name)
            if file_handle then
                local file_content = file_handle:read("*a")
                if file_content then
                    local meta_content = file_content:explode("\n")
                    if meta_content and #meta_content >= 4 then
                        -- add entry only if current model is supported
                        local supported = false
                        local supported_models = meta_content[4]:explode(",")
                        for _,model in pairs(supported_models) do
                            if model == product_model then
                                supported = true
                                break
                            end
                        end
                        if supported then
                            local file = file_name:match("/([^/]+).txt$")
                            g_firmware_list[#g_firmware_list + 1] = {
                                index = #g_firmware_list + 1,
                                version=meta_content[1],
                                file=file..".star"
                            }
                        end
                    end
                end
                file_handle:close()
            end
            file_name = dir_handle:read("*l")
        end
        dir_handle:close()
    end
end

-- initialise upgrading fw process
-- @param target_firmware meta information of target firmware
function init_upgrading_fw(target_firmware)
    g_in_critical_process = true
    g_upgrading_firmware = target_firmware

    g_upgrading_firmware_stage = g_upgrading_fw_stages.TRIGGERED

    g_upgrading_fw_messages = {}
    g_getting_flashing_messages = false
    g_done_flashing_detected = false
    g_time_counter_during_rebooting = 0

    luardb.set("service.upgrade.filename", target_firmware.file)
    luardb.set("service.upgrade.start", "1")

    luardb.set(g_upgrade_fw_led_rdb, "start")
end

-- indicate that upgrading firmware has failed
function failed_upgrading_fw()
    luardb.set(g_upgrade_fw_led_rdb, "failed")
    g_in_critical_process = false
end

-- indicate that upgrading firmware has been done successfully
function done_upgrading_fw()
    luardb.set(g_upgrade_fw_led_rdb, "done")
    g_in_critical_process = false
end

-- Report upgrading OWA firmware status via websocket
-- Being invoked periodically, it considers the current stage of the process
-- and sends report to websocket client.
function get_upgrading_fw_status_report()
    local report = {}
    report.current_version = luardb.get(config.rdb_sw_version)
    if g_upgrading_firmware_stage == g_upgrading_fw_stages.TRIGGERED then
        local file_transfer_status = luardb.get("service.upgrade.file_trans")
        -- in case downloading is too fast and already done, still switch
        -- stage to DOWNLOADING, next stage will be considered
        if file_transfer_status == "1" or file_transfer_status == "2" then
            g_upgrading_firmware_stage = g_upgrading_fw_stages.DOWNLOADING
        elseif file_transfer_status == "3" then
            g_upgrading_firmware_stage = g_upgrading_fw_stages.FAILED
            failed_upgrading_fw()
            report.messages = {"Failed to copy the firmware file to OWA"}
        end
    elseif g_upgrading_firmware_stage == g_upgrading_fw_stages.DOWNLOADING then
        local file_transfer_status = luardb.get("service.upgrade.file_trans")
        if file_transfer_status == "3" then
            g_upgrading_firmware_stage = g_upgrading_fw_stages.FAILED
            failed_upgrading_fw()
            report.messages = {"Failed to copy the firmware file to OWA"}
        elseif file_transfer_status == "2" then
            if g_getting_flashing_messages and #g_upgrading_fw_messages > 0 then
                report.messages = g_upgrading_fw_messages
                g_upgrading_fw_messages = {}
                g_upgrading_firmware_stage = g_upgrading_fw_stages.FLASHING
            end
        end
    elseif g_upgrading_firmware_stage == g_upgrading_fw_stages.FLASHING then
        if #g_upgrading_fw_messages > 0 then
            report.messages = g_upgrading_fw_messages
            g_upgrading_fw_messages = {}
        end

        if not g_getting_flashing_messages then
           if g_done_flashing_detected then
               if luardb.get(g_bridge_status_rdb) == "synchronised" then
                   g_upgrading_firmware_stage = g_upgrading_fw_stages.PREPARE_REBOOT
               else
                   g_upgrading_firmware_stage = g_upgrading_fw_stages.REBOOTING
               end
           else
               g_upgrading_firmware_stage = g_upgrading_fw_stages.FAILED
               failed_upgrading_fw()
               report.messages = {"Failed in flashing firmware"}
           end
        end
    elseif g_upgrading_firmware_stage == g_upgrading_fw_stages.PREPARE_REBOOT then
        if luardb.get(g_bridge_status_rdb) ~= "synchronised" then
            g_upgrading_firmware_stage = g_upgrading_fw_stages.REBOOTING
        end
    elseif g_upgrading_firmware_stage == g_upgrading_fw_stages.REBOOTING then
        if luardb.get(g_bridge_status_rdb) == "synchronised" then
            g_upgrading_firmware_stage = g_upgrading_fw_stages.REBOOTED
        else
            g_time_counter_during_rebooting = g_time_counter_during_rebooting + 1
            -- allow up to 10 minutes for reboot to complete before considering it as failed
            if g_time_counter_during_rebooting > 600 then
                g_upgrading_firmware_stage = g_upgrading_fw_stages.FAILED
                failed_upgrading_fw()
                report.messages = {"Failed in rebooting and synchronizing OWA"}
            end
        end
    elseif g_upgrading_firmware_stage == g_upgrading_fw_stages.REBOOTED then
        -- check version
        local current_version = report.current_version
        -- ignore prefix in the version, only compare version number string
        if not current_version
                or #current_version < #g_upgrading_firmware.version
                or current_version:sub(#current_version - #g_upgrading_firmware.version + 1) ~= g_upgrading_firmware.version then
            g_upgrading_firmware_stage = g_upgrading_fw_stages.FAILED
            failed_upgrading_fw()
            report.messages = {"Current version does not match with target firmware version"}
        else
            g_upgrading_firmware_stage = g_upgrading_fw_stages.DONE
            done_upgrading_fw()
        end
    else
        -- no stage changes
    end
    report.stage = g_upgrading_firmware_stage
    return report
end

-- process queries for list of compatible OWA firmware files and upgrading status
function OwaFirmwareUpgrade:get(access_type)
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    if access_type == "list_firmware" then
        if not g_firmware_list_available then
            get_firmware_list()
        end
        if g_firmware_list_available then
            self:write(g_firmware_list)
        else
            self:set_status(503)
        end
    elseif access_type == "upgrade_status" then
        local status = {}
        status.stage = g_upgrading_firmware_stage
        if g_upgrading_firmware_stage ~= g_upgrading_fw_stages.NO_UPDATING
                and g_upgrading_firmware_stage ~= g_upgrading_fw_stages.FAILED
                and g_upgrading_firmware_stage ~= g_upgrading_fw_stages.DONE
                and g_upgrading_firmware then
            status.target_version = g_upgrading_firmware.version
        end
        status.current_version = luardb.get(config.rdb_sw_version)
        self:write(status)
    end
end

-- handle triggering upgrading firmware request
function OwaFirmwareUpgrade:post(access_type)
    if access_type == "do_upgrade" then
        local battery = data_collector.get_battery_reading()
        if g_in_critical_process then
            self:write("Unable to start updating OWA firmware due to an ongoing critical process")
            self:set_status(503)
            return
        elseif battery.charge_percentage < g_battery_percentage_threshold_update_firmware then
            self:write("Battery power is insufficient to update OWA firmware")
            self:set_status(503)
            return
        else
            local firmware_index = tonumber(self:get_argument("firmwareIndex"))
            if firmware_index then
                local firmware = g_firmware_list[firmware_index]
                if firmware then
                    init_upgrading_fw(firmware)
                else
                    self:set_status(400)
                end
            else
                self:set_status(400)
            end
        end
    end
end

-- receive OWA firmware flashing logs from OWA
function WsOwaFirmwareUpgradeReport:on_message(msg)
    llog.log("LOG_INFO", "OWA FW Updating: "..msg)
    if msg ~= "<< NO MORE UPGRADING FW REPORT >>" then
        g_upgrading_fw_messages[#g_upgrading_fw_messages + 1] = msg
        if not g_getting_flashing_messages then
            g_getting_flashing_messages = true
        end
        if msg == "Done" then
            g_done_flashing_detected = true
        end
    else
        g_getting_flashing_messages = false
    end
end

-- Initialse factory reset
function init_factory_reset()
    g_in_critical_process = true
    g_factory_reset_stage = g_factory_reset_stages.TRIGGERED
    g_time_counter_during_rebooting_factory_reset = 0
    g_time_counter_unregister_cbrs_factory_reset = 0
end

-- process when factory reset failed
function failed_factory_reset()
    g_in_critical_process = false
end

-- process when factory reset succeeds
function done_factory_reset()
    g_in_critical_process = false
end

-- Report OWA factory reset status via websocket
-- Being invoked periodically, it considers the current stage of the process
-- and sends report to websocket client.
function get_factory_reset_status_report()
    local report = {}
    if g_factory_reset_stage == g_factory_reset_stages.TRIGGERED then
        if luardb.get("wwan.0.currentband.config") == "LTE Band 48 - TDD 3600" then
            local cbrs_reg_state = luardb.get("sas.registration.state")
            if cbrs_reg_state and #cbrs_reg_state and cbrs_reg_state ~= "Unregistered" then
                luardb.set("sas.registration.cmd", "force_deregister")
                g_factory_reset_stage = g_factory_reset_stages.UNREGISTER_CBRS
            else
                g_factory_reset_stage = g_factory_reset_stages.EXECUTE_FACTORY_RESET
            end
        else
            g_factory_reset_stage = g_factory_reset_stages.EXECUTE_FACTORY_RESET
        end
    elseif g_factory_reset_stage == g_factory_reset_stages.UNREGISTER_CBRS then
        local cbrs_reg_state = luardb.get("sas.registration.state")
        if cbrs_reg_state == "Unregistered" then
            g_factory_reset_stage = g_factory_reset_stages.EXECUTE_FACTORY_RESET
        else
            g_time_counter_unregister_cbrs_factory_reset = g_time_counter_unregister_cbrs_factory_reset + 1
            -- allow up to 10 minutes, then go ahead
            if g_time_counter_unregister_cbrs_factory_reset > 600 then
                g_factory_reset_stage = g_factory_reset_stages.EXECUTE_FACTORY_RESET
            end
        end
    elseif g_factory_reset_stage == g_factory_reset_stages.EXECUTE_FACTORY_RESET then
        luardb.set("owa.service.system.factory_reset", "11")
        g_factory_reset_stage = g_factory_reset_stages.FACTORY_RESET_IN_PROGRESS
    elseif g_factory_reset_stage == g_factory_reset_stages.FACTORY_RESET_IN_PROGRESS then
        if luardb.get(g_bridge_status_rdb) ~= "synchronised" then
            g_factory_reset_stage = g_factory_reset_stages.REBOOTING
        end
    elseif g_factory_reset_stage == g_factory_reset_stages.REBOOTING then
        if luardb.get(g_bridge_status_rdb) == "synchronised" then
            g_factory_reset_stage = g_factory_reset_stages.REBOOTED
        else
            g_time_counter_during_rebooting_factory_reset = g_time_counter_during_rebooting_factory_reset + 1
            -- allow up to 10 minutes for reboot to complete before considering it as failed
            if g_time_counter_during_rebooting_factory_reset > 600 then
                g_factory_reset_stage = g_factory_reset_stages.FAILED
                failed_factory_reset()
            end
        end
    elseif g_factory_reset_stage == g_factory_reset_stages.REBOOTED then
        -- nothing to check
        -- just conclude that factory reset is done
        g_factory_reset_stage = g_factory_reset_stages.DONE
        done_factory_reset()
    else
        -- no stage changes
    end
    report.stage = g_factory_reset_stage
    return report
end

-- handle triggering factory reset
-- Full factory reset will be triggered in OWA
function OwaFactoryReset:post()
    local battery = data_collector.get_battery_reading()
    if g_in_critical_process then
        self:write("Unable to start factory reset due to an ongoing critical process")
        self:set_status(503)
    elseif battery.charge_percentage < g_battery_percentage_threshold_factory_reset then
        self:write("Battery power is insufficient to start factory reset")
        self:set_status(503)
    else
        init_factory_reset()
    end
end

-- handle GET request for factory reset status
function OwaFactoryReset:get()
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    self:write({stage = g_factory_reset_stage})
end

-- Start the throughput (FTP) test
g_first_landing = true -- supports detection of auto start of speed test.
function JsonRequestHandlerRunTtest:delete(url)

    -- If this request is sent from Scan page, the url path ends with /auto.
    -- Otherwise, if the user clicked on Rerun page, then url ends with /rerun
    -- In the former case, run the test automatically only if this is the very
    -- first time the page is visited.
    -- In the later, re-run it unconditionally.
    for substr in string.gmatch(url, '([^/]+)') do
        if substr == "auto" and not g_first_landing then
            --print ("Not rerunning the test")
            return
        end
    end

    g_first_landing = false

    g_ttest_result = {}
    g_ttest_result.test_data = {}
    g_ttest_result.gen_data = {}

    luardb.set("service.ttest.ftp.command", "start")

    -- make sure update is as soon as possible
    g_ttest_count = g_ws_cfg.WS_TTEST_CURR_UPDATE_TIME_SECONDS

    -- start monitorig for possible cell change during the test
    g_cell_monitor.cell_on_start = luardb.get(config.rdb_serving_cell_info_prefix.."cellid") or 0
    g_cell_monitor.cell_changed = false
end

-- At the moment, used only for showAllCells control.
-- We could move all other control type functions (e.g. reset stats and start test for example)
-- and thus reduce the number of Request handlers.
function JsonRequestControl:post(url)

    local showFWICells = self:get_argument("showFWICells", "", true)
    if (showFWICells ~= nil and showFWICells ~= "") then
        if showFWICells == "true" then
            g_show_fwi_cells_mode = true
        else
            g_show_fwi_cells_mode = false
        end
    end

    local showAllCells = self:get_argument("showAllCells", "", true)
    if (showAllCells ~= nil and showAllCells ~= "") then
        if showAllCells == "true" then
            g_show_non_dual_plmn_cells = true
        else
            g_show_non_dual_plmn_cells = false
        end
    end
end

-- A helper: work out if cell in question has been amongst the ones specified by web user
local function is_user_selected_cell(pci, earfcn)

    if g_cell_lookup[earfcn] ~= nil and g_cell_lookup[earfcn][pci] ~= nil then
        -- Filtering using filters up to config.MAX_USER_CELLS_FOR_FILTERING_SCAN or 3 even though there are
        -- more filters entered by web user
        local numberOfFilter = math.min(#g_user_selected_data, config.MAX_USER_CELLS_FOR_FILTERING_SCAN or 3)
        for i = 1, numberOfFilter, 1 do
            if g_cell_lookup[earfcn][pci] == g_user_selected_data[i] then
                return true
            end
        end
    end
    return false
end

-- encode current RF reading as JSON blob and send back to the client
function format_rf_curr_report()
    local j = 1
    local rf_reports = { }

    if g_rrc_stat and (g_rrc_stat:find("idle", 1) or g_rrc_stat == "inactive") then
        rf_reports.scannable = true
    end

    rf_reports.limits = config.rf_limits
    rf_reports.data_connection_unavailable = g_data_connection_unavailable
    rf_reports.report_type = "RfCurrent"

    -- if we wanted to show all cells (including non WLL), we could do simply
    -- rf_reports.current_readings = g_current_rf_readings

    -- build a new array with only selected cells
    rf_reports.current_readings = {}
    if #g_current_rf_readings == 0 then
        return rf_reports
    else
        for i = 1, #g_current_rf_readings do
            if show_fwi_cells_mode() == true then
                local cell_sector_id, _ = get_cell_sector_id_by_pci_earfcn(g_current_rf_readings[i].pci, g_current_rf_readings[i].earfcn)
                if is_wll_cell(cell_sector_id) then
                    rf_reports.current_readings[j] = g_current_rf_readings[i]
                    j = j + 1
                end
            else
                if is_user_selected_cell(g_current_rf_readings[i].pci, g_current_rf_readings[i].earfcn) then
                    rf_reports.current_readings[j] = g_current_rf_readings[i]
                    j = j + 1
                end
            end
        end

        -- get current heading settings and set heading for scanning results
        local heading_list = {}
        for i = 0, config.MAX_USER_CELLS do
            local heading = luardb.get(string.format("installation.cell_filter.%d.heading", i))
            local ecgi = luardb.get(string.format("installation.cell_filter.%d.ecgi", i))
            if heading and ecgi and #heading > 0 and #ecgi > 0 then
                heading_list[ecgi] = heading
            end
        end
        for _, cell_rf_data in pairs(rf_reports.current_readings) do
            if cell_rf_data.cell_sector_id then
                cell_rf_data.heading = heading_list[cell_rf_data.cell_sector_id]
            end
        end

        -- limit the display to reasonable number of cells
        while #rf_reports.current_readings > config.MAX_DISPLAYED_CELLS do
            table.remove(rf_reports.current_readings)
        end
    end

    --self:write(rf_reports)
    return rf_reports
end

function format_ttest_result()
    local report = {}
    report.report_type = "TTest"
    report.test_data = g_ttest_result.test_data
    report.gen_data = g_ttest_result.gen_data
    report.data_connection_unavailable = g_data_connection_unavailable
    return report
end

-- get cell scan stats result to export to QR code which includes all FWI WLL cells
local function get_cell_stats_report_for_qr_code()
    local qr_cell_rf_stats = {}

    for earfcn, pci_list in pairs(g_rf_stats) do
        for pci, stats_summary in pairs(pci_list) do
            if is_wll_cell(stats_summary['cell_sector_id']) then
                qr_cell_rf_stats[#qr_cell_rf_stats + 1] = {
                    cell_sector_id = stats_summary['cell_sector_id'],
                    rsrp_max = stats_summary['rsrp_max'],
                    rsrq_max = stats_summary['rsrq_max'],
                    rssinr_max = stats_summary['rssinr_max'],
                }
            end
        end
    end

    -- write to RDB variable to provide RF stats to QR code
    -- format: ecgi1,max_rsrp,max_rsrq,max_rssinr;ecgi2,max_rsrp,max_rsrq,max_rssinr;...
    -- sort by RSRP (high to low)
    table.sort(qr_cell_rf_stats, function(a,b)
        return a.rsrp_max > b.rsrp_max
    end)
    local rf_stats_str = ""
    for _,rf in pairs(qr_cell_rf_stats) do
        rf_stats_str = string.format("%s%s%s,%.2f,%.2f", rf_stats_str, #rf_stats_str > 0 and ";" or "",
            rf.cell_sector_id, rf.rsrp_max, rf.rsrq_max)
        if rf.rssinr_max then
            rf_stats_str = string.format("%s,%.2f", rf_stats_str, rf.rssinr_max)
        end
    end
    luardb.set("installation.cell_rf_stats", rf_stats_str)
end

-- NOTE : these 3 get methods are not used at the moment because client does not use Ajax to send "get" requests
-- Instead, web socket is used. However, we will keep it enabled as there is no harm and it is very easy to
-- switch back to Ajax if needed (for example, for reasons of browser incompatibility with Websocket.

-- encode RF current data as JSON blob and send back to the client
function JsonRequestHandlerRfCurrent:get()
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    local report = {}
    report = format_rf_curr_report()
    self:write(report)
end

-- encode battery data as JSON blob and send back to the client
function JsonRequestHandlerBattery:get()
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    report = data_collector.get_battery_reading()
    report.report_type = "Batt"
    self:write(report)
end

-- encode version data as JSON blob and send back to the client
function JsonRequestHandlerVersions:get()
    local versions = {}

    versions.sw_ver = luardb.get(config.rdb_sw_version) or 0

    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    self:write(versions)
end

-- Query most precise orientation data
function OrientationData:get()
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    local code = 500
    if luardb.get("owa.orien.status") == "0" then
        local pcall_ret, rpc_ret, data = pcall(luardb.invoke, 'azimuth_down_tilt', 'get', 10, 30)
        if pcall_ret and rpc_ret == 0 and data then
            local azimuth, down_tilt = string.match(data, "^(-?%d+);(-?%d+)$")
            if azimuth and down_tilt then
                self:write({azimuth = azimuth, down_tilt = down_tilt})
                code = 200
            end
        end
    end
    self:set_status(code)
end

-- Error message for invalid data received from client
local g_err_msg =""

-- Data entry page will POST to /user_entry which will be handled here
function UserConfDataEntry:post(url)

    local pci, mcc, mnc, cell_id, cell_valid

    -- Validate data received from client
    local customer_ban = self:get_argument("ban", "", true)

    local cell_sector_id_list = {}
    for j = 1, config.MAX_USER_CELLS, 1 do
        -- process data sent by the client in POST request
        local plmn_str = self:get_argument("plmn" .. tostring(j), "", true)
        local cell_str = self:get_argument("cid" .. tostring(j), "", true)
        local heading_str = self:get_argument("heading" .. tostring(j), "", true)
        local str = plmn_str..cell_str
        if str ~= nil and str ~= "" and string.len(str) == 15 then -- check for length even though client validates it, too
            local cell_sector_id = {}
            cell_sector_id.plmn = plmn_str
            cell_sector_id.cid = cell_str
            if #heading_str > 0 then
                cell_sector_id.heading = tonumber(heading_str) or ""
            else
                cell_sector_id.heading = ""
            end
            -- Set to default - if a valid value entered, it will be overriden
            cell_sector_id.rsrp_pass = config.rf_limits.RSRP.pass
            local pass_val = tonumber(self:get_argument("pass" .. tostring(j), "", true))
            -- pass level is only meaningful if cell sector id has been entered as well
            if pass_val ~= nil then
                pass_val = -pass_val
                if pass_val >= config.rf_limits.RSRP.min and pass_val <= config.rf_limits.RSRP.max then
                    cell_sector_id.rsrp_pass = pass_val
                end
            end
            table.insert(cell_sector_id_list, cell_sector_id)
        end
    end


    local call_sign = self:get_argument("call_sign", "", true)  -- optional
    local user_id = self:get_argument("user_id", "", true)
    local band_select = self:get_argument("band_select", "", true)
    local groupParams_json_arr = self:get_argument("grp_params", "", true)  -- optional
    local cpi_id = self:get_argument("cpi_id", "", true)  -- optional
    local cpi_name = self:get_argument("cpi_name", "", true)  -- optional
    local cpi_pem_key = self:get_argument("cpi_pem_key", "", true)  -- optional

    if #cpi_pem_key > 1 then
        -- try to convert to correct format
        local format_valid = false
        local error_str = "wrong format"
        local header, body, footer = cpi_pem_key:match("(%-%-%-%-%-BEGIN .+%-%-%-%-%-)(.+)(%-%-%-%-%-END .+%-%-%-%-%-)")
        if header and body and footer then
            -- only base64 characters are accepted
            body = body:gsub("[^%w%+/=]", "")
            cpi_pem_key = string.format("%s\n%s\n%s", header, body, footer)
            format_valid, error_str = m_cbrs.validate_pem_key(cpi_pem_key)
        end
        if not format_valid then
            llog.log("LOG_ERR", string.format("Failed to parse CPI key in Quick Copy: %s", error_str))
            -- ignore invalid key from quick copy
            cpi_pem_key = ""
        end
    end

    g_err_msg = ""
    if string.len(customer_ban) == 0 then
        g_err_msg = "BAN is an empty string"
    elseif band_select ~= "30" and band_select ~= "48" then
        g_err_msg = "Operating band is not 30 or 48 (on server)"
    elseif string.len(cpi_id) > 256 then
        g_err_msg = "CPI Id contains more than 256 octets"
    elseif string.len(cpi_name) > 256 then
        g_err_msg = "CPI Name contains more than 256 octets "
    end
    if band_select == "30" then
        if string.len(user_id) > 253 then
            g_err_msg = "User Id contains more than 253 octets"
        end
    else
        if string.len(user_id) == 0 or string.len(user_id) > 253 then
            g_err_msg = "User Id is an empty string or contains more than 253 octets"
        end
    end

    -- Error, do not proceed any further. Reload data entry page
    if string.len(g_err_msg) > 0 then
        local js = "<script>window.open(\"/data_entry.html\", \"_self\")</script>"
        self:write(js)
        return
    end

    -- Data received from client have been validated at this point
    luardb.set("installation.customer_ban", customer_ban)
    luardb.set("sas.config.userId", user_id)
    luardb.set("sas.config.callSign", call_sign)                  -- optional but non-hidden field
    luardb.set("sas.config.groupingParam", groupParams_json_arr)  -- optional but non-hidden field

    local band_name = "LTE Band 48 - TDD 3600"  -- index "A8"
    if band_select == "30" then
        band_name = "LTE Band 30 - 2300MHz"  -- index "A0"
    end
    luardb.set("wwan.0.currentband.config", band_name)
    luardb.set("wwan.0.currentband.cmd.param.band", band_name)
    luardb.set("wwan.0.currentband.cmd.command", "set")

    -- Optional hidden field data, set RDB if not blank
    if string.len(cpi_id) > 0 then luardb.set("sas.config.cpiId", cpi_id, "p") end
    if string.len(cpi_name) > 0 then luardb.set("sas.config.cpiName", cpi_name, "p") end
    if string.len(cpi_pem_key) > 0 then
        m_cbrs.set_cpi_key_from_quick_copy(cpi_pem_key)
    else
        m_cbrs.set_cpi_key_from_quick_copy(nil)
    end

    g_user_selected_data = {}
    g_user_selected_rsrp_pass = {}

    local i = 0
    for _, cell_sector_id in pairs(cell_sector_id_list) do
        g_user_selected_data[i+1] = cell_sector_id.plmn..cell_sector_id.cid
        g_user_selected_rsrp_pass[i+1] = cell_sector_id.rsrp_pass
        luardb.set(string.format("installation.cell_filter.%d.plmn", i), cell_sector_id.plmn)
        luardb.set(string.format("installation.cell_filter.%d.cid", i), cell_sector_id.cid)
        luardb.set(string.format("installation.cell_filter.%d.ecgi", i), g_user_selected_data[i+1])
        luardb.set(string.format("installation.cell_filter.%d.rsrp_pass", i), cell_sector_id.rsrp_pass)
        luardb.set(string.format("installation.cell_filter.%d.heading", i), cell_sector_id.heading)
        i = i + 1
    end
    -- erase stale settings
    for j = i, config.MAX_USER_CELLS do
        if luardb.get(string.format("installation.cell_filter.%d.plmn", j)) then
            luardb.set(string.format("installation.cell_filter.%d.plmn", j), "")
            luardb.set(string.format("installation.cell_filter.%d.cid", j), "")
            luardb.set(string.format("installation.cell_filter.%d.ecgi", j), "")
            luardb.set(string.format("installation.cell_filter.%d.rsrp_pass", j), "")
            luardb.set(string.format("installation.cell_filter.%d.heading", j), "")
        end
    end

    -- If in band 48, notify SAS client to Force CBRS Deregistration
    if band_select == "48" then
        luardb.set("sas.registration.cmd", "force_deregister")
    end

    -- Entering scan_connnect installation state
    -- firstly reset stale cell data
    luardb.set(config.rdb_cell_manual_seq, "");
    luardb.set(config.rdb_cell_manual_qty, "");
    g_rf_stats = {}
    luardb.set("installation.cell_rf_stats", "")
    luardb.set("installation.state", "scan_connect")

    -- redirect to RF page
    local js = "<script>window.open(\"/scan.html\", \"_self\")</script>"
    self:write(js)
end

function clear_local_speed_test_result()
   local command = [[
   for i in 0 1; do
       rdb_del -L service.ttest.ftp.${i}.res.;
       rdb_set service.ttest.ftp.${i}.current_repeat "";
       rdb_set service.ttest.ftp.${i}.avg_speed_mbps "";
   done
   ]]
   os.execute(command)
end

-- Send the server data to the user config entry form.
function UserConfDataEntry:get(url)

    -- reset any speed test
    clear_local_speed_test_result()

    -- Note: cpi_id, cpi_name and cpi_pem_key values are not sent to client

    local customer_ban = luardb.get(config.get_rdb_prefix.."installation.customer_ban") or ""
    local call_sign = luardb.get(config.get_rdb_prefix.."sas.config.callSign") or ""
    local user_id = luardb.get(config.get_rdb_prefix.."sas.config.userId") or ""

    local band_name = luardb.get(config.get_rdb_prefix.."wwan.0.currentband.config") or ""
    -- default band is 48
    local band_select = "48"
    if band_name == "LTE Band 30 - 2300MHz" then
        band_select = "30"
    end

    local default_plmn = luardb.get(config.rdb_default_plmn)
    local reg_status = luardb.get("sas.registration.state") or ""
    local grp_params = luardb.get(config.get_rdb_prefix.."sas.config.groupingParam") or ""

    data = {}
    data.err_msg = {g_err_msg}
    data.ban = {customer_ban}
    data.call_sign = {call_sign}
    data.user_id = {user_id}
    data.grp_params = {grp_params}
    data.band_select = {band_select}

    data.cid = {}
    data.plmn = {}
    data.pass = {}
    data.heading = {}
    for j = 0, config.MAX_USER_CELLS-1 do
        local plmn = luardb.get(string.format(config.get_rdb_prefix.."installation.cell_filter.%d.plmn", j))
        if not plmn or #plmn == 0 then
            plmn = default_plmn
        end
        local cid = luardb.get(string.format(config.get_rdb_prefix.."installation.cell_filter.%d.cid", j)) or ""
        local rsrp_pass = luardb.get(string.format(config.get_rdb_prefix.."installation.cell_filter.%d.rsrp_pass", j)) or ""
        local heading = luardb.get(string.format(config.get_rdb_prefix.."installation.cell_filter.%d.heading", j)) or ""
        if plmn and cid then
            table.insert(data.plmn, plmn)
            table.insert(data.cid, cid)
            if #rsrp_pass > 0 then
                rsrp_pass = tonumber(rsrp_pass) or config.rf_limits.RSRP.pass
                -- UI displays it as -dBm
                rsrp_pass = -rsrp_pass
            end
            table.insert(data.pass, rsrp_pass)
            table.insert(data.heading, heading)
        end
    end

    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    self:write(data)
end

-- handle queries for feedback of installation state from OWA
function InstallationState:get()
    local response = {
        state = luardb.get("owa.installation.state") or ""
    }

    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    self:write(response)
end

-- handle queries to change installation state
function InstallationState:post()
    local new_state = self:get_argument("state", "", true)
    if new_state == "scan_connect" or new_state == "normal_operation" or new_state == "data_entry" then
        luardb.set("installation.state", new_state)
    end
end

-- Get current operating configuration
function OperatingConfig:get()
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    local band_name = luardb.get("wwan.0.currentband.config") or ""
    -- default band is 48
    local band_select = "48"
    if band_name == "LTE Band 30 - 2300MHz" then
        band_select = "30"
    end

    data = {}
    data.band_select = band_select

    self:write(data)
end

-- supports FTP server configuration server side code
-- hardcode for 1 download and 1 upload server
_rdb_root_dload = "service.ttest.ftp.0."
_rdb_root_uload = "service.ttest.ftp.1."

-- Server configuration page will POST to /user_entry_ftp which will be handled here
function FtpServerConfigure:post(url)


    local server = self:get_argument("dload_server", "", true)
    local user = self:get_argument("dload_user", "", true)
    local pass = self:get_argument("dload_pass", "", true)
    local remote_file = self:get_argument("dload_remote_file", "", true)
    local local_file = self:get_argument("dload_local_file", "", true)

    luardb.set(_rdb_root_dload.."server", server)
    luardb.set(_rdb_root_dload.."user", user)
    luardb.set(_rdb_root_dload.."password", pass)
    luardb.set(_rdb_root_dload.."remote_file_name", remote_file)
    luardb.set(_rdb_root_dload.."local_file_name", local_file)

-- note: this is slightly assymetric. Ignore upload server details EXCEPT for the
-- remote path/file.
-- On the client side, there 4 out of 5 inputs are read-only.
-- We can uncomment these 4 lines and corresponding "disabled" tag in HTML to
-- have completely independent download and upload configuration.
    --server = self:get_argument("uload_server", "", true)
    --user = self:get_argument("uload_user", "", true)
    --pass = self:get_argument("uload_pass", "", true)
    --local_file = self:get_argument("uload_local_file", "", true)
    local remote_file_upload = self:get_argument("uload_remote_file", "", true)

    -- do not overwrite the original server file, no matter what
    if remote_file_upload == remote_file then
        remote_file_upload = remote_file_upload.."_upl"
    end

    luardb.set(_rdb_root_uload.."server", server)
    luardb.set(_rdb_root_uload.."user", user)
    luardb.set(_rdb_root_uload.."password", pass)
    luardb.set(_rdb_root_uload.."remote_file_name", remote_file_upload)
    -- local file for upload is generated by OWA and is not user configurable

    -- now redirect to the original page which will be refreshed with new RDB data as saved
    local js = "<script>window.open(\"/server_detail.html\", \"_self\")</script>"
    self:write(js)
end

-- When the FTP server configuration page is initializing, it will do a GET to /user_entry_ftp which will be handled here
-- server_data will be sent to the client and used to populate the elements in the page.
function FtpServerConfigure:get(url)
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    server_data = {}
    server_data.dload = {}
    server_data.uload = {}

    server_data.dload.server = luardb.get(config.get_rdb_prefix.._rdb_root_dload.."server")
    server_data.dload.user = luardb.get(config.get_rdb_prefix.._rdb_root_dload.."user")
    server_data.dload.pass = luardb.get(config.get_rdb_prefix.._rdb_root_dload.."password")
    server_data.dload.remote_file = luardb.get(config.get_rdb_prefix.._rdb_root_dload.."remote_file_name")
    server_data.dload.local_file = luardb.get(config.get_rdb_prefix.._rdb_root_dload.."local_file_name")

    server_data.uload.server = luardb.get(config.get_rdb_prefix.._rdb_root_uload.."server")
    server_data.uload.user = luardb.get(config.get_rdb_prefix.._rdb_root_uload.."user")
    server_data.uload.pass = luardb.get(config.get_rdb_prefix.._rdb_root_uload.."password")
    server_data.uload.remote_file = luardb.get(config.get_rdb_prefix.._rdb_root_uload.."remote_file_name")
    server_data.uload.local_file = luardb.get(config.get_rdb_prefix.._rdb_root_uload.."local_file_name")

    self:write(server_data)
end



-- When the CBRS install parameter page is initializing, it will do a GET to /user_entry_cpi_param which will be handled here
-- server_data will be sent to the client and used to populate the elements in the page.
function CpiParamConfigure:get(url)
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    server_data = {}
    server_data.cpi_id = luardb.get(config.get_rdb_prefix.."sas.config.cpiId")
    server_data.cpi_name = luardb.get(config.get_rdb_prefix.."sas.config.cpiName")
    server_data.cpi_key_from_quick_copy = m_cbrs.is_cpi_key_from_quick_copy()
    server_data.pkcs12_valid = m_cbrs.is_pkcs12_valid()

    -- client code will not care about this certification time queried from server
    -- keep it here but commented out in case for some reasons it will need queried data from server in the future
    --server_data.install_certification_time = luardb.get("sas.config.installCertificationTime")

    self:write(server_data)
end

function validateParam(new_val, min_val, max_val)
    return (new_val ~= nil and new_val >= min_val and new_val <= max_val)
end

function updateRdbIfChanged(rdb_name, new_val)
    local curr_val = luardb.get(rdb_name) or ""
    if curr_val ~= new_val then
      luardb.set(rdb_name, new_val)
    end
end

-- Handle uploading CPI PKCS12 file and passphrase
function UploadCpiPKCS12:post()
    local key_file = self:get_argument("key_file")
    local passphrase = self:get_argument("passphrase")

    if #passphrase > 256 then
        self:set_status(400)
        self:write("Passcode is too long. Maximum supported length is 256.")
        return
    end

    local ret, error_str = m_cbrs.set_pkcs12(key_file, passphrase)
    if not ret then
        self:set_status(400)
        self:write(error_str)
        return
    end
end

-- When pressing Register button in the CBRS install parameter page, it will do a POST to /sas_register which will be handled here
-- to invoke SAS registration.
function SasRegister:post(url)
    if not m_cbrs.cpi_key_available() then
        self:set_status(400)
        return
    end

    local params = self:get_json(true)
    if params.use_pkcs12 and not m_cbrs.is_pkcs12_valid() then
        self:set_status(400)
        return
    end
    params.reg_latitude = tonumber(params.reg_latitude)
    params.reg_longitude = tonumber(params.reg_longitude)
    params.reg_height = tonumber(params.reg_height)
    params.reg_azimuth = tonumber(params.reg_azimuth)
    params.reg_downtilt = tonumber(params.reg_downtilt)
    if not validateParam(params.reg_latitude, -90, 90) then return 0 end
    if not validateParam(params.reg_longitude, -180, 180) then return 0 end
    if not validateParam(params.reg_height, 0, 10000) then return 0 end
    if not validateParam(params.reg_azimuth, 0, 359) then return 0 end
    if not validateParam(params.reg_downtilt, -90, 90) then return 0 end
    if (string.len(params.cpi_id) > 256) then
      llog.log("LOG_ERR",string.format("CPI ID length %d is too long", string.len(params.cpi_id)))
      self:set_status(400)
      return
    end
    if (string.len(params.cpi_name) > 256) then
      llog.log("LOG_ERR",string.format("CPI name length %d is too long", string.len(params.cpi_name)))
      self:set_status(400)
      return
    end

    -- WINNF-TS-0016 specifies format YYYY-MM-DDThh:mm:ssZ
    -- However test data with AT&T SAS server is 2018-08-06T15:51:12.866Z i.e there is ".866" before "Z".
    -- Hence allowing some charaters before "Z" in the pattern
    local install_certification_time = params.install_certification_time
    local ict_pattern = "^%d%d%d%d%-%d%d%-%d%dT%d%d:%d%d:%d%d.*Z$";
    if not install_certification_time or not string.match(install_certification_time, ict_pattern) then
        llog.log("LOG_ERR",string.format("Invalid Install Certification Time"))
        self:set_status(400)
        return
    end

    -- invoke registration process
    local reg_params = {
        use_pkcs12 = params.use_pkcs12,
        latitude = params.reg_latitude,
        longitude = params.reg_longitude,
        height = params.reg_height,
        azimuth = params.reg_azimuth,
        downtilt = params.reg_downtilt,
        cpi_id = params.cpi_id,
        cpi_name = params.cpi_name,
        install_certification_time = params.install_certification_time
    }
    if not m_cbrs.invoke_registration(reg_params) then
        self:set_status(400)
        return
    end
end

-- When entering Mount Installation page or Move To New Location page, it will do a GET to /sas_deregister which will be handled here
-- to invoke SAS de-registration.
function SasDeregister:get(url)
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    -- Invoke de-registration
    luardb.set("sas.registration.cmd", "deregister")
end

-- Get antenna mount type
function MountType:get()
  -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    local cur_mount_type = luardb.get(config.get_rdb_prefix.."sas.antenna.mount_type")
    if cur_mount_type and #cur_mount_type > 0 then
        self:write(cur_mount_type)
    else
        self:write("")
    end
end

-- Update antenna mount type
function MountType:post()
    local cur_mount_type = luardb.get(config.get_rdb_prefix.."sas.antenna.mount_type")
    local new_mount_type = self:get_argument("mount_type", "", true)
    if new_mount_type == "remove" then
      new_mount_type = ""
    end
    if new_mount_type ~= cur_mount_type then
      llog.log("LOG_INFO",string.format("set sas.antenna.mount_type to %s", new_mount_type))
      luardb.set("sas.antenna.mount_type", new_mount_type)
    end
end

-- handle keep-alive request
function KeepAlive:get()
    local owa_sync_status = luardb.get("service.rdb_bridge.connection_status") or ""
    local response = {
        owa_sync_status = owa_sync_status
    }
  -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")
    self:write(response)
end

-- Set websocket report filter mask
function SetWsFilter:get(url)
    -- disable cache
    self:set_header("Cache-Control", "no-cache, no-store, must-revalidate")
    self:set_header("Pragma", "no-cache")
    self:set_header("Expires", "0")

    g_ws_report_filter = self:get_argument("mask", "", true)
    llog.log("LOG_INFO",string.format("set WS report filter to 0x%04x", g_ws_report_filter))
end

function WSExHandler:on_message(msg)
    --print ("Received "..msg)
end

function WSExHandler:prepare()
    --print "In prepare"
end

-- If needed, this can be made into a table of supported protocols
-- for now, keep minimalistic.
-- Client will have to create a web socket with correct protocol by
-- specifying a correct Sec-WebSocket-Protocol field in header.
WSExHandler.protocol = "v1.installation_assistant"
function WSExHandler:subprotocol(protocols)
    for _, p in pairs(protocols) do
        if p == WSExHandler.protocol then
            return p
        end
    end
    return nil
end

-- Send battery information via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_battery()
    g_batt_count = g_batt_count + 1
    if g_batt_count >= g_ws_cfg.WS_BATT_UPDATE_TIME_SECONDS then
        g_batt_count = 0
        report = data_collector.get_battery_reading()
        report.report_type = "Batt"
        if not pcall(self.write_message, self, report) then
            return false
        end
    end
    return true
end

-- Send RF unavailable information via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_rf_unavailable()
    local inv_report = {}
    inv_report.report_type = "Invalid"
    inv_report.message = "No RF data available"
    if not pcall(self.write_message, self, inv_report) then
        return false
    end
    return true
end

-- Send RF current readings via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_rf_current_readings()
    g_curr_count = g_curr_count + 1
    if g_curr_count >= g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS then
        g_curr_count = 0
        local curr_report = format_rf_curr_report()
        if not pcall(self.write_message, self, curr_report) then
            return false
        end
    end
    return true
end

-- Send test result via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_ttest_result()
    g_ttest_count = g_ttest_count + 1
    if g_ttest_count >= g_ws_cfg.WS_TTEST_CURR_UPDATE_TIME_SECONDS then
        g_ttest_count = 0
        local ttest_result = format_ttest_result()
        if not pcall(self.write_message, self, ttest_result) then
            return false
        end
    end
    return true
end

-- Send antenna status via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_antenna_status()
    g_ant_status_count = g_ant_status_count + 1
    if g_ant_status_count >= g_ws_cfg.WS_ANT_STATUS_UPDATE_TIME_SECONDS then
        g_ant_status_count = 0
        local report = data_collector.get_antenna_status()
        report.report_type = "AntennaStatus"
        if not pcall(self.write_message, self, report) then
            return false
        end
    end
    return true
end

-- Send registration status via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_registration_status()
    g_reg_status_count = g_reg_status_count + 1
    if g_reg_status_count >= g_ws_cfg.WS_REG_STATUS_UPDATE_TIME_SECONDS then
        g_reg_status_count = 0
        local report = data_collector.get_registration_status()
        report.report_type = "RegistrationStatus"
        if not pcall(self.write_message, self, report) then
            return false
        end
    end
    return true
end

-- Send orientation data via WebSocket
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_orientation_data()
    g_orientation_count = g_orientation_count + 1
    if g_orientation_count >= g_ws_cfg.WS_ORIENTATION_UPDATE_TIME_SECONDS then
        g_orientation_count = 0
        local report = data_collector.get_orientation_data()
        report.report_type = "OrientationData"
        if not pcall(self.write_message, self, report) then
            return false
        end
    end
    return true
end

-- Send upgrading FW status via WebSocket
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_upgrading_fw_status()
    g_upgrading_fw_status_count = g_upgrading_fw_status_count + 1
    if g_upgrading_fw_status_count >= g_ws_cfg.WS_UPGRADING_FW_STATUS_TIME_SECONDS then
        g_upgrading_fw_status_count = 0
        local report = get_upgrading_fw_status_report()
        report.report_type = "UpgradingFwStatus"
        if not pcall(self.write_message, self, report) then
            return false
        end
    end
    return true
end

-- Send factory reset status via WebSocket
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_factory_reset_status()
    g_factory_reset_status_count = g_factory_reset_status_count + 1
    if g_factory_reset_status_count >= g_ws_cfg.WS_FACTORY_RESET_STATUS_TIME_SECONDS then
        g_factory_reset_status_count = 0
        local report = get_factory_reset_status_report()
        report.report_type = "FactoryResetStatus"
        if not pcall(self.write_message, self, report) then
            return false
        end
    end
    return true
end

-- Bit mask for websocket report filtering
-- This mask definition should match with client side defined in common.js
BITMASK_BATTERY = 0x0001
BITMASK_ORIENTATION = 0x0002
BITMASK_RF_STATISTICS = 0x0004
BITMASK_RF_CURR_READING = 0x0008
BITMASK_THROUGHPUT_TEST = 0x0010
BITMASK_SYSTEM_STATUS = 0x0020
BITMASK_ANTENNA_STATUS = 0x0040
BITMASK_REGISTRATION_STATUS = 0x0080
BITMASK_UPGRADING_FW_STATUS = 0x0100
BITMASK_FACTORY_RESET_STATUS = 0x0200

-- Check the bitmask is set in global filter
local function f_enabled(bitmask)
    return (bit.band(g_ws_report_filter, bitmask) == bitmask)
end

-- Send available information at regular interval
function WSExHandler:_data_send()
    if f_enabled(BITMASK_BATTERY) then
        self:_send_battery()
    end

    if f_enabled(BITMASK_RF_CURR_READING) then
        self:_send_rf_current_readings()
        get_cell_stats_report_for_qr_code()
    end

    if f_enabled(BITMASK_THROUGHPUT_TEST) then
        if g_rf_unavailable then
            self:_send_rf_unavailable()
        else
            if f_enabled(BITMASK_THROUGHPUT_TEST) then
                self:_send_ttest_result()
            end
        end
    end

    -- send antenna status
    if f_enabled(BITMASK_ANTENNA_STATUS) then
        self:_send_antenna_status()
    end

    -- send registration status
    if f_enabled(BITMASK_REGISTRATION_STATUS) then
        self:_send_registration_status()
    end

    -- send orientation data
    if f_enabled(BITMASK_ORIENTATION) then
        self:_send_orientation_data()
    end

    if f_enabled(BITMASK_UPGRADING_FW_STATUS) then
        self:_send_upgrading_fw_status()
    end

    if f_enabled(BITMASK_FACTORY_RESET_STATUS) then
        self:_send_factory_reset_status()
    end

    self._timer = _G.io_loop_instance:add_timeout((turbo.util.gettimemonotonic() + 1000), self._data_send, self)
end

function WSExHandler:open()
    print "In open"
    self._timer = _G.io_loop_instance:add_timeout((turbo.util.gettimemonotonic() + 1000), self._data_send, self)
    self._timer_running = true
    g_batt_count = g_ws_cfg.WS_BATT_UPDATE_TIME_SECONDS
    g_curr_count = g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS
    g_ttest_count = g_ws_cfg.WS_TTEST_CURR_UPDATE_TIME_SECONDS
    g_ant_status_count = g_ws_cfg.WS_ANT_STATUS_UPDATE_TIME_SECONDS
    g_reg_status_count = g_ws_cfg.WS_REG_STATUS_UPDATE_TIME_SECONDS
    g_orientation_count = g_ws_cfg.WS_ORIENTATION_UPDATE_TIME_SECONDS
    g_upgrading_fw_status_count = g_ws_cfg.WS_UPGRADING_FW_STATUS_TIME_SECONDS
    g_factory_reset_status_count = g_ws_cfg.WS_FACTORY_RESET_STATUS_TIME_SECONDS
end

-- this is necessary as otherwise browser refresh button (same will happen in other scenarios)
-- opens another client connection with the old one still active.
function WSExHandler:on_close()
    print "In on-close"
    if self._timer_running then
        _G.io_loop_instance:remove_timeout(self._timer)
        self._timer_running = false
    end
end

function WSExHandler:on_error(code, reason)
    print "In on-error"
    if self._timer_running then
        _G.io_loop_instance:remove_timeout(self._timer)
        self._timer_running = false
    end
end

app = turbo.web.Application:new({
    {"/rf_current/(.*)$", JsonRequestHandlerRfCurrent },
    {"/rf_restart_ttest/(.*)$", JsonRequestHandlerRunTtest },
    {"/control/(.*)$", JsonRequestControl },
    {"/battery/(.*)$", JsonRequestHandlerBattery },
    {"^/orientation_data$", OrientationData },
    {"/versions/(.*)$", JsonRequestHandlerVersions },
    {"/user_entry/(.*)$", UserConfDataEntry },
    {"/operating_config$", OperatingConfig},
    {"/user_entry_ftp", FtpServerConfigure },
    {"/user_entry_cpi_param", CpiParamConfigure },
    {"/sas_register", SasRegister },
    {"/sas_deregister", SasDeregister },
    {"/mount_type", MountType },
    {"/set_ws_filter", SetWsFilter },
    {"^/generate_qr_code$", QrCodeGenerateHandler },
    {"/qr_code/(.*)$", QrCodeImage },
    {"^/owa_firmware_upgrade/(.+)$", OwaFirmwareUpgrade },
    {"^/owa_factory_reset$", OwaFactoryReset },
    {"^/ws_upgrade_report$", WsOwaFirmwareUpgradeReport },
    {"^/upload_cpi_pkcs12$", UploadCpiPKCS12 },
    {"^/keep_alive$", KeepAlive },
    {"^/installation_state$", InstallationState },
-- web socket
    {"^/ws$", WSExHandler},
-- open index.html by default if just the IP address is entered in the address bar of the browser
    {"^/$", turbo.web.StaticFileHandler, g_client_dir .. "index.html" },

-- Important: this has to be the last entry to allow other routes to be handled first,
-- this one being a "catch-all" route.
    {"^/(.*)$", turbo.web.StaticFileHandler, g_client_dir}
})

-- Loads configuration before starting any applications
config = load_overriding_module(g_ia_dir, 'config')

config.get_rdb_prefix = config.get_rdb_prefix or ""

-- Initialises data collector before running the web server
data_collector.init(config)

local httpserver_kwargs = {
    -- maximum body size 10 MB should be sufficient
    max_body_size = 1024*1024*10
}

-- @TODO - this will be a different port and probably localhost address
-- depending on RDB variable, allow binding to local host only (e.g. NRB200 access only, default) or
-- allow binding the web server socket on all interfaces (in which case, there can be access via
-- admin or data VLAN, which is useful for testing/development purposes.
if luardb.get(config.rdb_bind_all) == "1" then
    app:listen(config.listen_port, nil, httpserver_kwargs)
else
    app:listen(config.listen_port, "127.0.0.1", httpserver_kwargs) -- access only from local processes, e.g. NRB200 web relay
end

local instance = turbo.ioloop.instance()
local cb, rdbFD = luardb.synchronousMode(true)
instance:add_handler(rdbFD, turbo.ioloop.READ, cb)
luardb.watch(config.rdb_cell_manual_qty, data_collector.get_cell_measurement)
luardb.watch(config.rdb_rrc_info_qty, data_collector.read_rrc_rdbs)

data_collector.read_rrc_stat()
luardb.watch(config.rdb_rrc_stat, data_collector.read_rrc_stat)

m_cbrs.init(instance, llog)

-- create directory where test files are kept
os.execute("mkdir -p /tmp/ttests")

-- a mode useful for testing to show all cells, not just AT&T WLL
if luardb.get(config.rdb_show_non_wll_cells) == "1" then
    g_show_non_wll_cells = true
end

-- polling as backup in case the subscribe-RDB mechanism is not working
instance:set_interval(500, function()
    data_collector.read_rrc_stat()
    data_collector.read_rrc_rdbs()
    data_collector.get_cell_measurement()
end)

-- start a slow poll "task" to monitor the system status, for example the fact that
-- RF is available
instance:set_interval(config.STATUS_POLL_INTERVAL_MS, data_collector.status_poll)

instance:start()
