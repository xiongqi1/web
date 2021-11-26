#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
-- ia_web_server.lua is an executable script that implements a Turbo based web server
-- for supporting Titan Installation Assistant functionality
--
_G.TURBO_SSL = true

require('stringutil')

local turbo = require("turbo")
local luarrb = require("luardb")
local llog = require("luasyslog")

pcall(function() llog.open("installation assistant", "LOG_DAEMON") end)

-- directory where IA libraries are located
local g_ia_dir = "/usr/share/installation_assistant/"
-- in test mode, if already done cd directly to the installation assistant directory,
-- then simply set g_ia_dir to "" before running.
require(g_ia_dir .. "support")
data_collector = require(g_ia_dir .. "data_collector")

-- directory where client code (HTML and Javascript) are located
local g_client_dir = g_ia_dir .. "client/"
-- directory where report files are stored. Must be r/w part of file system.
local g_report_dir = "/usr/local/inst_assistant_reports"
-- remove report files more than X days old
local g_old_file_age_days = 90
local g_old_file_age_secs = (60*60*24*g_old_file_age_days)

-- derive request handlers to customise GET request handling
local JsonRequestHandlerRfCurrent = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerRfStats = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerBattery = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerVersions = class("JsonRequestHandler", turbo.web.RequestHandler)
local UserConfDataEntry = class("JsonRequestHandler", turbo.web.RequestHandler)
local FtpServerConfigure = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerResetRfStats = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerRunTtest = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestControl = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestGenerateReport = class("JsonRequestHandler", turbo.web.RequestHandler)
local WSExHandler = class("WSExHandler", turbo.websocket.WebSocketHandler)

-- web socket related configuration
g_ws_cfg= {
    -- web socket controls the frequency of data updates sent by us (the server)
    WS_BATT_UPDATE_TIME_SECONDS = 10,
    WS_ORIENTATION_UPDATE_TIME_SECONDS = 1,
    WS_STATS_UPDATE_TIME_SECONDS = 10,
    WS_RF_CURR_UPDATE_TIME_SECONDS = 1,
    WS_TTEST_CURR_UPDATE_TIME_SECONDS = 1,
    WS_SYS_STATUS_UPDATE_TIME_SECONDS = 3,
}

-- on first iteration, send straight away to load the page data faster
local g_batt_count = g_ws_cfg.WS_BATT_UPDATE_TIME_SECONDS
local g_orientation_count = g_ws_cfg.WS_ORIENTATION_UPDATE_TIME_SECONDS
local g_stats_count = g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS
local g_curr_count = g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS
local g_system_status_count = g_ws_cfg.WS_SYS_STATUS_UPDATE_TIME_SECONDS
local g_ttest_count = g_ws_cfg.WS_TTEST_CURR_UPDATE_TIME_SECONDS

-- Reset all stats and all samples
function JsonRequestHandlerResetRfStats:delete()
    print "Reset stats"
    g_rf_stats = {}
    g_rf_samples = {}
    -- make sure stats update as soon as possible to an (almost) empty table
    g_stats_count = g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS
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

    os.execute("killall ttest_ftp.sh")
    -- the simple os.execute("ttest_ftp.sh 0 1 &") doesn't work well because
    -- child (ftp script) inherits all parent fds, including sockets and the net effect is
    -- that page becomes unresponsive. The method below closes all file descriptors.
    -- This is an overkill but seems fast enough.
    -- The solution comes from http://stackoverflow.com/questions/4835608/os-execute-without-inheriting-parents-fds
    -- Note we start from 3 as there is no need to close stdin/stdout/stderr
    os.execute("eval exec `seq 3 255 | sed -e 's/.*/&<\\&-/'`;ttest_ftp.sh 0 1 &")

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

    -- clear all readings and start refreshing
    g_curr_count = g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS
    g_rf_stats = {}
    g_rf_samples = {}
    g_stats_count = g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS - 2
    -- sanity check
    if g_stats_count <= 0 then
        g_stats_count = 1
    end
end

-- A helper: work out if cell in question has been amongst the ones specified by web user
local function is_user_selected_cell(pci, earfcn)

    if g_cell_lookup[earfcn] ~= nil and g_cell_lookup[earfcn][pci] ~= nil then
        for i = 1, #g_user_selected_data,1 do
            if g_cell_lookup[earfcn][pci] == g_user_selected_data[i] then
                return true
            end
        end
    end
    return false
end

-- encode current RF reading as JSON blob abd send back to the client
function format_rf_curr_report()
    local j = 1
    local rf_reports = { }

    -- if we wanted to show all cells (including non WLL), we could do simply
    -- rf_reports.current_readings = g_current_rf_readings

    -- build a new array with only selected cells
    rf_reports.current_readings = {}
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

    -- limit the display to reasonable number of cells
    while #rf_reports.current_readings > config.MAX_DISPLAYED_CELLS do
        table.remove(rf_reports.current_readings)
    end

    rf_reports.limits = config.rf_limits
    rf_reports.data_connection_unavailable = g_data_connection_unavailable
    rf_reports.report_type = "RfCurrent"
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

-- encode stats summary as JSON blob abd send back to the client
function format_stats_report()
    local rf_reports = {}
    rf_reports.stats = {}

    local result = {}
    local i = 1
    for earfcn, pci_list in pairs(g_rf_stats) do
        for pci, stats_summary in pairs(pci_list) do
            if ((show_fwi_cells_mode() == true and is_wll_cell(stats_summary['cell_sector_id'])) or
            (#g_user_selected_data > 0 and is_user_selected_cell(pci, earfcn))) then
                result[i] = {
                    pci = pci,
                    earfcn = earfcn,
                    cell_sector_id = stats_summary['cell_sector_id'],
                    band = earfcn_map(earfcn), --only interested in the first agument
                    rsrp_min = stats_summary['rsrp_min'],
                    rsrp_max = stats_summary['rsrp_max'],
                    rsrp_avg = string.format("%.2f", stats_summary['rsrp_avg']),
                    rsrp_avg_num = stats_summary['rsrp_avg'], -- so we can sort by a numeric value
                    rsrq_min = stats_summary['rsrq_min'],
                    rsrq_max = stats_summary['rsrq_max'],
                    rsrq_avg = string.format("%.2f", stats_summary['rsrq_avg']),
                    ns = stats_summary['ns'],
                    res = stats_summary['res'],
                    ts = stats_summary['most_recent_sample_ts']
                }
                result[i].orientation = stats_summary['best_orientation']
                result[i].rank = get_cell_rank(result[i].cell_sector_id)
                i = i + 1
            end
        end
    end

    -- As per requirements, sort by entered Cell sector ID order, followed by rsrp (high to low)
    table.sort(result, function(a,b)
        if a.rank == b.rank then return a.rsrp_avg_num > b.rsrp_avg_num else return a.rank > b.rank end
    end)

    while #result > config.MAX_DISPLAYED_CELLS_STATS do
        table.remove(result) -- no second argument, means remove the last element
    end

    rf_reports.stats = result
    rf_reports.limits = config.rf_limits
    rf_reports.report_type = "RfStats"
    -- This is here rather than in RfReport to avoid a race condition
    rf_reports.show_fwi_cells_mode = g_show_fwi_cells_mode
    rf_reports.show_non_dual_plmn_cells = g_show_non_dual_plmn_cells
    --self:write(rf_reports)
    return rf_reports
end

-- NOTE : these 3 get methods are not used at the moment because client does not use Ajax to send "get" requests
-- Instead, web socket is used. However, we will keep it enabled as there is no harm and it is very easy to
-- switch back to Ajax if needed (for example, for reasons of browser incompatibility with Websocket.

-- encode RF current data as JSON blob and send back to the client
function JsonRequestHandlerRfCurrent:get()
    local report = {}
    report = format_rf_curr_report()
    self:write(report)
end

-- encode RF stats data as JSON blob and send back to the client
function JsonRequestHandlerRfStats:get()
    local stats_report = {}
    stats_report = format_stats_report()
    self:write(stats_report)
end

-- encode battery data as JSON blob and send back to the client
function JsonRequestHandlerBattery:get()
    report = data_collector.get_battery_reading()
    report.report_type = "Batt"
    self:write(report)
end

-- encode version data as JSON blob and send back to the client
function JsonRequestHandlerVersions:get()
    local versions = {}

    versions.sw_ver = luardb.get("sw.version") or 0

    self:write(versions)
end

-- Data entry page will POST to /user_entry which will be handled here
function UserConfDataEntry:post(url)

    local int i = 1
    local pci, mcc, mnc, cell_id, cell_valid

    g_user_selected_data = {}
    g_user_selected_rsrp_pass = {}

    for j = 1, config.MAX_USER_CELLS, 1 do
        -- process data sent by the client in POST request
        local plmn_str = self:get_argument("plmn" .. tostring(j), "", true)
        local cell_str = self:get_argument("cid" .. tostring(j), "", true)
        local str = plmn_str..cell_str
        if str ~= nil and str ~= "" and string.len(str) == 15 then -- check for length even though client validates it, too
            -- Record user entered data
            g_user_selected_data[i] = str
            -- Set to default - if a valid value entered, it will be overriden
            g_user_selected_rsrp_pass[i] = config.rf_limits.RSRP.pass
            local pass_val = tonumber(self:get_argument("pass" .. tostring(j), "", true))
            -- pass level is only meaningful if cell sector id has been entered as well
            if pass_val ~= nil then
                pass_val = -pass_val
                if pass_val >= config.rf_limits.RSRP.min and pass_val <= config.rf_limits.RSRP.max then
                    g_user_selected_rsrp_pass[i] = pass_val
                end
            end
            i = i + 1 -- increment i only if cell data entered is valid
        end
    end
    -- redirect to RF page
    local js = "<script>window.open(\"/scan.html\", \"_self\")</script>"
    self:write(js)
end

-- Send the server data to the user config entry form.
function UserConfDataEntry:get(url)

    -- normally, this is set to AT&T mcc mnc of 001010. But changing the RDB
    -- allows to test with non-AT&T networks, including Amari & CMW.

    local default_plmn = luardb.get(config.rdb_default_plmn)
    data = {}
    data.cid = {"", "", ""}
    data.plmn = { default_plmn, default_plmn, default_plmn }
    -- After changes requested in TT#19820 there is no longer concept of default pass level. For reference
    -- this is the correct initialization if AT&T change their mind again.
    --data.pass = {-config.rf_limits.RSRP.pass, -config.rf_limits.RSRP.pass, -config.rf_limits.RSRP.pass}
    data.pass = {"", "", ""}
    -- overridden if user already entered data. When data entry page is opened
    -- first time, g_user_selected_data is not populated so the next loop is skipped.
    for i = 1, #g_user_selected_data, 1 do
        if string.len(g_user_selected_data[i]) == 15 then
            data.plmn[i] = string.sub(g_user_selected_data[i], 1, 6)
            data.cid[i] = string.sub(g_user_selected_data[i], 7, 15)
        end
    end

    for i = 1, #g_user_selected_rsrp_pass, 1 do
        data.pass[i] = -g_user_selected_rsrp_pass[i]
    end
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

    server_data = {}
    server_data.dload = {}
    server_data.uload = {}

    server_data.dload.server = luardb.get(_rdb_root_dload.."server")
    server_data.dload.user = luardb.get(_rdb_root_dload.."user")
    server_data.dload.pass = luardb.get(_rdb_root_dload.."password")
    server_data.dload.remote_file = luardb.get(_rdb_root_dload.."remote_file_name")
    server_data.dload.local_file = luardb.get(_rdb_root_dload.."local_file_name")

    server_data.uload.server = luardb.get(_rdb_root_uload.."server")
    server_data.uload.user = luardb.get(_rdb_root_uload.."user")
    server_data.uload.pass = luardb.get(_rdb_root_uload.."password")
    server_data.uload.remote_file = luardb.get(_rdb_root_uload.."remote_file_name")
    server_data.uload.local_file = luardb.get(_rdb_root_uload.."local_file_name")

    self:write(server_data)
end

function format_final_report()
    ret = {}
    ret.stats_report = format_stats_report()
    ret.rf_report = format_rf_curr_report()
    ret.ttest_report = format_ttest_result()
    ret.rf_unavaliable = g_rf_unavailable
    ret.message = "No RF data available"
    return ret
end

-- @TODO
function JsonRequestGenerateReport:get(param)
    local final_report = {}
    final_report = format_final_report()
    self:write(final_report)
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

-- Send orientation information via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_orientation()
    g_orientation_count = g_orientation_count + 1
    if g_orientation_count >= g_ws_cfg.WS_ORIENTATION_UPDATE_TIME_SECONDS then
        g_orientation_count = 0
        report = data_collector.get_orientation_reading()
        report.report_type = "Orientation"
        if not pcall(self.write_message, self, report) then
            return false
        end
    end
    return true
end

-- Send system status via WebSocket at regular intervals
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_system_status()
    g_system_status_count = g_system_status_count + 1
    if g_system_status_count >= g_ws_cfg.WS_SYS_STATUS_UPDATE_TIME_SECONDS then
        g_system_status_count = 0
        local report = data_collector.get_system_status_reading()
        report.report_type = "SysStatus"
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

-- Send RF statistics via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_rf_statistics()
    -- RF Data available, produce output
    g_stats_count = g_stats_count + 1
    if g_stats_count >= g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS then
        g_stats_count = 0
        local stats_report = format_stats_report()
        if not pcall(self.write_message, self, stats_report) then
            return false
        end
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

-- Send available information at regular interval
function WSExHandler:_data_send()
    local result = true

    if config.capabilities.nrb200 then
        result = self:_send_battery()
    end

    if result and config.capabilities.orientation then
        result = self:_send_orientation()
    end

    if result then
        if g_rf_unavailable then
            result = self:_send_rf_unavailable()
        else
            result = self:_send_rf_statistics()
            if result then
                result = self:_send_rf_current_readings()
            end
            if result then
                result = self:_send_ttest_result()
            end
        end
    end

    -- send system status
    if result then
        result = self:_send_system_status()
    end


    if result then
        self._timer = _G.io_loop_instance:add_timeout((turbo.util.gettimemonotonic() + 1000), self._data_send, self)
    else
        self._timer_running = false
        self.stream:close()
    end
end

function WSExHandler:open()
    print "In open"
    self._timer = _G.io_loop_instance:add_timeout((turbo.util.gettimemonotonic() + 1000), self._data_send, self)
    self._timer_running = true
    g_batt_count = g_ws_cfg.WS_BATT_UPDATE_TIME_SECONDS
    g_stats_count = g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS
    g_curr_count = g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS
    g_ttest_count = g_ws_cfg.WS_TTEST_CURR_UPDATE_TIME_SECONDS
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
    {"/rf_stats/(.*)$", JsonRequestHandlerRfStats },
    {"/rf_reset_stats/(.*)$", JsonRequestHandlerResetRfStats },
    {"/rf_restart_ttest/(.*)$", JsonRequestHandlerRunTtest },
    {"/control/(.*)$", JsonRequestControl },
    {"/report/(.*)$", JsonRequestGenerateReport },
    {"/battery/(.*)$", JsonRequestHandlerBattery },
    {"/versions/(.*)$", JsonRequestHandlerVersions },
    {"/user_entry/(.*)$", UserConfDataEntry },
    {"/user_entry_ftp", FtpServerConfigure },
-- web socket
    {"^/ws$", WSExHandler},
    -- the following must be before default handler
    {"/ttest.html", turbo.web.StaticFileHandler, g_client_dir .. "ttest.html" },
    {"/report.html", turbo.web.StaticFileHandler, g_client_dir .. "report.html" },
    {"/server_detail.html", turbo.web.StaticFileHandler, g_client_dir .. "server_detail.html" },
-- land on data_entry page by default if just the IP address is entered in the address bar of the browser
    {"^/$", turbo.web.StaticFileHandler, g_client_dir .. "data_entry.html" },
    {"/scan.html", turbo.web.StaticFileHandler, g_client_dir .. "scan.html" },

-- Important: this has to be the last entry to allow other routes to be handled first,
-- this one being a "catch-all" route.
    {"^/(.*)$", turbo.web.StaticFileHandler, g_client_dir}
})

-- Loads configuration before starting any applications
config = load_overriding_module(g_ia_dir, 'config')

-- Initialises data collector before running the web server
data_collector.init(config)

-- @TODO - this will be a different port and probably localhost address
-- depending on RDB variable, allow binding to local host only (e.g. NRB200 access only, default) or
-- allow binding the web server socket on all interfaces (in which case, there can be access via
-- admin or data VLAN, which is useful for testing/development purposes.
if luardb.get(config.rdb_bind_all) == "1" then
    app:listen(8888)
else
    app:listen(8888, "127.0.0.1") -- access only from local processes, e.g. NRB200 web relay
end



-- Start a data collector "task", "run" lives in data_collector.lua
-- Note the method (poll/notification) is different depending on the configuration
local instance = turbo.ioloop.instance()
if config.use_polling then
    instance:set_interval(config.POLL_INTERVAL_MS, data_collector.run)
else
    local cb, rdbFD = luardb.synchronousMode(true)
    instance:add_handler(rdbFD, turbo.ioloop.READ, cb)
    luardb.watch(config.rdb_cell_qty, data_collector.run)
end

-- create directory where test files are kept
os.execute("mkdir -p /tmp/ttests")

-- a mode useful for testing to show all cells, not just AT&T WLL
if luardb.get(config.rdb_show_non_wll_cells) == "1" then
    g_show_non_wll_cells = true
end

-- start a slow poll "task" to monitor the system status, for example the fact that
-- RF is available
instance:set_interval(config.STATUS_POLL_INTERVAL_MS, data_collector.status_poll)

instance:start()
