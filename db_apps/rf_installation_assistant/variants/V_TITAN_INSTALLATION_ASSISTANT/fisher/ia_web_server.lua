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
local luardb = require("luardb")
local llog = require("luasyslog")

pcall(function() llog.open("installation assistant", "LOG_DAEMON") end)

-- directory where IA libraries are located
local g_ia_dir = "/usr/share/installation_assistant/"
-- in test mode, if already done, cd directly to the installation assistant directory,
-- then simply set g_ia_dir to "" before running.
require(g_ia_dir .. "support")
data_collector = require(g_ia_dir .. "data_collector")

-- directory where client code (HTML and Javascript) are located
local g_client_dir = g_ia_dir .. "client/"

-- This is really convenient for testing, use Ajax mechanism to see data easily. Web socket is hard to debug
-- Data format is the same. To view raw JSON data from server, enter URL such as "192.168.1.1/rf_current/anything",
local JsonRequestHandlerRfCurrent = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerRfStats = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerResetRfStats = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerVersions = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerSystemStatus = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerOrientation = class("JsonRequestHandler", turbo.web.RequestHandler)
-- web socket handler
local WSExHandler = class("WSExHandler", turbo.websocket.WebSocketHandler)

-- web socket related configuration
g_ws_cfg= {
    -- web socket controls the frequency of data updates sent by us (the server)
    WS_BATT_UPDATE_TIME_SECONDS = 10,
    WS_ORIENTATION_UPDATE_TIME_SECONDS = 1,
    WS_STATS_UPDATE_TIME_SECONDS = 10,
    WS_RF_CURR_UPDATE_TIME_SECONDS = 1,
    WS_SYS_STATUS_UPDATE_TIME_SECONDS = 1,
}

-- on first iteration, send straight away to load the page data faster
local g_orientation_count = g_ws_cfg.WS_ORIENTATION_UPDATE_TIME_SECONDS
local g_stats_count = g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS
local g_curr_count = g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS
local g_system_status_count = g_ws_cfg.WS_SYS_STATUS_UPDATE_TIME_SECONDS
-- a flag, which if set to 1, indicates that we cannot provide any RF related information. For example, wwan is not up.
local g_rf_unavailable = false

-- Reset all stats and all samples
function JsonRequestHandlerResetRfStats:delete()
    print "Reset stats"
    g_rf_stats = {}
    g_rf_samples = {}
    -- make sure stats update as soon as possible to an (almost) empty table
    g_stats_count = g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS
end

-- encode current RF reading as JSON blob abd send back to the client
function format_rf_curr_report()
    local rf_reports = { }

    -- if we wanted to show all cells, we could do simply
    -- rf_reports.current_readings = g_current_rf_readings
    rf_reports.current_readings = {}
    for _, entry in ipairs(g_current_rf_readings) do
        if is_display_cell(entry.ecgi) then
            rf_reports.current_readings[#rf_reports.current_readings + 1] = entry
        end
    end

    -- limit the display to reasonable number of cells
    while #rf_reports.current_readings > config.MAX_DISPLAYED_CELLS do
        table.remove(rf_reports.current_readings)
    end

    rf_reports.limits = config.rf_limits
    rf_reports.report_type = "RfCurrent"
    rf_reports.show_ecgi_or_eci = g_show_ecgi_or_eci
    return rf_reports
end

-- encode stats summary as JSON blob and send back to the client
function format_stats_report()
    local rf_reports = {}
    rf_reports.stats = {}

    local result = {}
    local i = 1
    for earfcn, pci_list in pairs(g_rf_stats) do
        for pci, stats_summary in pairs(pci_list) do
            if is_display_cell(stats_summary['ecgi']) then
                result[i] = {
                    pci = pci,
                    earfcn = earfcn,
                    ecgi = stats_summary['ecgi'],
                    rsrp_min = stats_summary['rsrp_min'],
                    rsrp_max = stats_summary['rsrp_max'],
                    rsrp_avg = string.format("%.2f", stats_summary['rsrp_avg']),
                    rsrp_avg_num = stats_summary['rsrp_avg'], -- so we can sort using faster non-floating point math
                    rsrq_min = stats_summary['rsrq_min'],
                    rsrq_max = stats_summary['rsrq_max'],
                    rsrq_avg = string.format("%.2f", stats_summary['rsrq_avg']),
                    ns = stats_summary['ns'],
                    ts = stats_summary['most_recent_sample_ts']
                }
                result[i].orientation = stats_summary['best_orientation']
                i = i + 1
            end
        end
    end

    -- As per requirements, sort by rsrp (high to low)
    table.sort(result, function(a,b)
        return a.rsrp_avg_num > b.rsrp_avg_num
    end)

    while #result > config.MAX_DISPLAYED_CELLS_STATS do
        table.remove(result) -- no second argument, means remove the last element
    end

    rf_reports.stats = result
    rf_reports.limits = config.rf_limits
    rf_reports.report_type = "RfStats"
    rf_reports.show_ecgi_or_eci = g_show_ecgi_or_eci
    return rf_reports
end

-- The next 2 functions are for convenience of seeing data on the client side. In reality, the client NEVER
-- uses ajax to get data (Websocket is used) but it is hard to debug and test using Websocket.
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

-- Miscellaneous static information, mainly versions
function JsonRequestHandlerVersions:get()
    local versions = {}

    versions.sw_ver = luardb.get(config.rdb_sw_version) or 0
    versions.hw_ver = luardb.get(config.rdb_hw_version) or 0
    versions.fw_ver = luardb.get(config.rdb_fw_version) or 0
    versions.imsi = luardb.get(config.rdb_imsi) or 0
    versions.imei = luardb.get(config.rdb_imei) or 0
    versions.serial_number = luardb.get(config.rdb_serial_number) or 0
    versions.class = luardb.get(config.rdb_class) or 0
    versions.model = luardb.get(config.rdb_model) or 0
    versions.skin = luardb.get(config.rdb_skin) or 0
    versions.title = luardb.get(config.rdb_title) or 0
    versions.modem_hw_ver = luardb.get(config.rdb_modem_hw_ver) or 0
    versions.board_hw_ver = luardb.get(config.rdb_board_hw_ver) or 0
    versions.mac = luardb.get(config.rdb_mac) or 0

    self:write(versions)
end

-- Get system status information, intended for ATS validation on production line
function JsonRequestHandlerSystemStatus:get()
    local statusResponse = {}
    statusReading = data_collector.get_system_status_reading()

    statusResponse.sim_iccid = statusReading.Advance.SimICCID or ''
    statusResponse.cell_id = statusReading.Advance.CellId or ''
    statusResponse.network_registration_status = statusReading.CellConn.RegStatus or ''
    statusResponse.earfcn = statusReading.Advance.EARFCN or ''
    statusResponse['rsrp.0'] = luardb.get(config.rdb_rsrp_0) or ''
    statusResponse['rsrp.1'] = luardb.get(config.rdb_rsrp_1) or ''
    statusResponse.rsrq = statusReading.Advance.RSRQ or ''
    statusResponse.band = luardb.get(config.rdb_band) or ''

    self:write(statusResponse)
end

-- Get orientation information, intended for ATS validation on production line
function JsonRequestHandlerOrientation:get()
    local orientationResponse = {}
    orientationReading = data_collector.get_orientation_reading()

    orientationResponse.azimuth = tostring(orientationReading.azimuth) or ''
    orientationResponse.elevation = tostring(orientationReading.elevation) or ''
    orientationResponse.accuracy = orientationReading.accuracy or ''

    self:write(orientationResponse)
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

-- Send orientation information via WebSocket at regular intervals
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

-- Send RF unavailable information via WebSocket at regular intervals
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

-- Send RF statistics via WebSocket at regular intervals
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

-- Send RF current readings via WebSocket at regular intervals
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

function WSExHandler:_send_rf()
    if g_rf_unavailable then
        if not self:_send_rf_unavailable() then
            return false
        end
        return true -- nothing else to do in RF data sending
    end

    if not self:_send_rf_statistics() then
        return false
    end

    if not self:_send_rf_current_readings() then
        return false
    end

    return true
end

-- Send available information at regular intervals
function WSExHandler:_data_send()
    if config.capabilities.orientation then
        if not self:_send_orientation() then
            return false
        end
    end

    if not self:_send_rf() then
        return false
    end

    if not self:_send_system_status() then
        return false
    end

    g_last_report_time = turbo.util.gettimemonotonic()
    return true
end

function WSExHandler:_data_send_wrapper()
    if self:_data_send() then
        self._timer = _G.io_loop_instance:add_timeout((turbo.util.gettimemonotonic() + 1000), self._data_send_wrapper, self)
    else
        self._timer_running = false
        self.stream:close()
    end
end

function WSExHandler:open()
    print "In open"
    self._timer = _G.io_loop_instance:add_timeout((turbo.util.gettimemonotonic() + 1000), self._data_send_wrapper, self)
    self._timer_running = true
    g_batt_count = g_ws_cfg.WS_BATT_UPDATE_TIME_SECONDS
    g_stats_count = g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS
    g_curr_count = g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS
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
-- @TODO - delete these for production.
    {"/rf_current/(.*)$", JsonRequestHandlerRfCurrent },
    {"/rf_stats/(.*)$", JsonRequestHandlerRfStats },
    {"/rf_reset_stats/(.*)$", JsonRequestHandlerResetRfStats },
    -- web socket
    {"^/ws$", WSExHandler},
    -- the following must be before default handler
    {"^/$", turbo.web.StaticFileHandler, g_client_dir .. "status.html" },
    {"/scan.html", turbo.web.StaticFileHandler, g_client_dir .. "scan.html" },
    {"/versions/(.*)$", JsonRequestHandlerVersions },
    {"/status/(.*)$", JsonRequestHandlerSystemStatus },
    {"/orientation/(.*)$", JsonRequestHandlerOrientation },

-- Important: this has to be the last entry to allow other routes to be handled first,
-- this one being a "catch-all" route.
    {"^/(.*)$", turbo.web.StaticFileHandler, g_client_dir}
})

-- Loads configuration before starting any applications
config = load_overriding_module(g_ia_dir, 'config')

-- Initialises data collector before running the web server
data_collector.init(config)

local server_port = tonumber(luardb.get("service.nrb200.web_ui.server_port")) or 8888
-- When On-board Wi-Fi is used for a communication channel, the web server
-- can be accessed by the network interface. The rdb_bind_all configuration can
-- also be used to open the door for debug purpose.
if luardb.get(config.rdb_bind_all) == "1" or config.capabilities.onboard_wifi then
    app:listen(server_port)
else
    app:listen(server_port, "127.0.0.1") -- access only from local processes, e.g. NRB200 web relay
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

-- Initialises the last report time
g_last_report_time = turbo.util.gettimemonotonic()

-- a mode useful for testing to show all cells, not just white-listed ones
-- we inherited the historic 'improper' config and RDB name 'shown_non_wll_cells'
if luardb.get(config.rdb_show_non_wll_cells) == "1" then
    g_show_all_cells = true
end

if luardb.get(config.rdb_show_unknown_cells) == "1" then
    g_show_unknown_cells = true
end

if luardb.get(config.rdb_show_ecgi_or_eci) == "1" then
    g_show_ecgi_or_eci = true
end

-- start a slow poll "task" to monitor the system status, for example the fact that
-- RF is available
instance:set_interval(config.STATUS_POLL_INTERVAL_MS, data_collector.status_poll)

instance:start()
