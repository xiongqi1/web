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
local JsonRequestHandlerBattery = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerVersions = class("JsonRequestHandler", turbo.web.RequestHandler)
local JsonRequestHandlerMagnetometer = class("JsonRequestHandler", turbo.web.RequestHandler)

-- derive request handlers to customise request handling
local JsonRequestHandlerResetRfStats = class("JsonRequestHandler", turbo.web.RequestHandler)
-- web socket handler
local WSExHandler = class("WSExHandler", turbo.websocket.WebSocketHandler)

-- web socket related configuration
g_ws_cfg= {
    -- web socket controls the frequency of data updates sent by us (the server)
    WS_BATT_UPDATE_TIME_SECONDS = 10,
    WS_MAGNETOMETER_CALIBRATION_UPDATE_TIME_SECONDS = 1,
    WS_STATS_UPDATE_TIME_SECONDS = 5,
    WS_RF_CURR_UPDATE_TIME_SECONDS = 1,
    WS_MODE2_UPDATE_TIME_SECONDS = 1,
}

-- on first iteration, send straight away to load the page data faster
local g_batt_count = g_ws_cfg.WS_BATT_UPDATE_TIME_SECONDS
local g_mag_cal_count = g_ws_cfg.WS_MAGNETOMETER_CALIBRATION_UPDATE_TIME_SECONDS
local g_stats_count = g_ws_cfg.WS_STATS_UPDATE_TIME_SECONDS
local g_curr_count = g_ws_cfg.WS_RF_CURR_UPDATE_TIME_SECONDS
local g_mode2_count = g_ws_cfg.WS_MODE2_UPDATE_TIME_SECONDS

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

    -- Basically a copy. Any filtering can be done here (for example, do not include non-NBN cells).
    -- Check code for Titan which does element by element copy skipping elements not needed for display.
    rf_reports.current_readings = g_current_rf_readings

    -- limit the display to reasonable number of cells
    while #rf_reports.current_readings > config.MAX_DISPLAYED_CELLS do
        table.remove(rf_reports.current_readings)
    end

    rf_reports.limits = config.rf_limits
    rf_reports.report_type = "RfCurrent"
    --self:write(rf_reports)
    return rf_reports
end


-- encode stats summary as JSON blob abd send back to the client
function format_stats_report()
    local rf_reports = {}
    rf_reports.stats = {}

    local result = {}
    local i = 1
    for earfcn, pci_list in pairs(g_rf_stats) do
        for pci, stats_summary in pairs(pci_list) do
            if true then
                result[i] = {

                    pci = pci,
                    earfcn = earfcn,
                    band = earfcn_map(earfcn), --only interested in the first agument
                    ecgi = stats_summary.ecgi,

                    rsrp_min = stats_summary['rsrp_min'],
                    rsrp_max = stats_summary['rsrp_max'],
                    rsrp_avg = string.format("%.2f", stats_summary['rsrp_avg']),
                    rsrp_avg_num = stats_summary['rsrp_avg'], -- so we can sort using faster non-floating point math
                    rsrp_median = string.format("%.2f", stats_summary['rsrp_median']),
                    rsrp_delta_avg = string.format("%.2f", stats_summary['rsrp_delta_avg']),

                    rsrq_min = stats_summary['rsrq_min'],
                    rsrq_max = stats_summary['rsrq_max'],
                    rsrq_avg = string.format("%.2f", stats_summary['rsrq_avg']),

                    ns = stats_summary['ns'],
                    res = stats_summary['res'],
                    ts = stats_summary['most_recent_sample_ts']
                }
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
    return rf_reports
end

-- The next 3 functions are for convenience of seeing data on the client side. In reality, the client NEVER
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

-- encode battery data as JSON blob and send back to the client
function JsonRequestHandlerBattery:get()
    report = data_collector.get_battery_reading()
    report.report_type = "Batt"
    self:write(report)
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

-- Start magnetometer calibration
function JsonRequestHandlerMagnetometer:post()
    luardb.set("sensors.orientation.0.cmd.command", "calibrate")
    luardb.set("sensors.orientation.0.cmd.trigger", "1")
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

-- Send magnetometer calibration status via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_mag_cal()
    g_mag_cal_count = g_mag_cal_count + 1
    if g_mag_cal_count >= g_ws_cfg.WS_MAGNETOMETER_CALIBRATION_UPDATE_TIME_SECONDS then
        g_mag_cal_count = 0
        report = data_collector.get_mag_cal_status()
        report.report_type = "MagCal"
        if not pcall(self.write_message, self, report) then
            return false
        end
    end
    return true
end

-- Send mode2 info via WebSocket at regular interval
-- @return true if the message is successfully transferred, false otherwise.
function WSExHandler:_send_mode2_info()
    g_mode2_count = g_mode2_count + 1
    if g_mode2_count >= g_ws_cfg.WS_MODE2_UPDATE_TIME_SECONDS then
        g_mode2_count = 0
        report = data_collector.get_mode2_info()
        report.report_type = "Mode2"
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

-- Send available information at regular interval
function WSExHandler:_data_send()

    if g_wmmd_mode == "online" then
        if not self:_send_mode2_info() then
            return false
        end
    else
        if config.capabilities.nrb200 then
            if not self:_send_battery() then
                return false
            end
        end

        if config.capabilities.orientation then
            if not self:_send_mag_cal() then
                return false
            end
        end

        if not self:_send_rf() then
            return false
        end
    end

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

-- Loads configuration before starting any applications
config = load_overriding_module(g_ia_dir, 'config')

-- get the current wmmd mode
g_wmmd_mode = luardb.get(config.rdb_wmmd_mode) or ""

-- Initialises data collector before running the web server
data_collector.init(config)

-- default landing page
-- land on scan page by default if nrb200 connected to wntdv3 and wmmd is not online
local g_default_landing_page = "scan.html"

-- land on mode 2 page by default if nrb200 connected to wntdv3 and wmmd is online
if g_wmmd_mode == "online" then
    g_default_landing_page = "mode2.html"
end

app = turbo.web.Application:new({

    {"/rf_current/(.*)$", JsonRequestHandlerRfCurrent },
    {"/magnetometer/calibrate", JsonRequestHandlerMagnetometer},
    {"/rf_stats/(.*)$", JsonRequestHandlerRfStats },
    {"/battery/(.*)$", JsonRequestHandlerBattery },
    {"/versions/(.*)$", JsonRequestHandlerVersions },

-- note that so far, NBN variant has no ability to manually reset stats. But keep it here just in case if they change their mind
    {"/rf_reset_stats/(.*)$", JsonRequestHandlerResetRfStats },
-- web socket
    {"^/ws$", WSExHandler},
-- land on default page (set above) if just the IP address is entered in the address bar of the browser
-- do not allow for scan or mode2 to be accessible in the other mode
    {"^/$", turbo.web.StaticFileHandler, g_client_dir .. g_default_landing_page },
    {"/scan.html", turbo.web.StaticFileHandler, g_client_dir .. g_default_landing_page },
    {"/mode2.html" , turbo.web.StaticFileHandler, g_client_dir .. g_default_landing_page },

-- Important: this has to be the last entry to allow other routes to be handled first,
-- this one being a "catch-all" route.
    {"^/(.*)$", turbo.web.StaticFileHandler, g_client_dir}
})

-- Depending on the RDB variable, allow binding to local host only (e.g. NRB200 access only, default) or
-- allow binding the web server socket on all interfaces (in which case, there can be access via LAN or WIFI)
-- which is useful for testing/development purposes. Also, we can use either http or https but not both.
if luardb.get(config.rdb_bind_all) == "1" then
    if luardb.get(config.rdb_turbo_use_https) == "1" then
        app:listen(8888, "0.0.0.0",
        { ssl_options = { key_file = luardb.get(config.rdb_key_file), cert_file = luardb.get(config.rdb_crt_file)}})
    else
        app:listen(8888)
    end
else
    -- access only from local processes, e.g. NRB200 relay assistant
    if luardb.get(config.rdb_turbo_use_https) == "1" then
        app:listen(8888, "127.0.0.1",
        { ssl_options = { key_file = luardb.get(config.rdb_key_file), cert_file = luardb.get(config.rdb_crt_file)}})
    else
        app:listen(8888, "127.0.0.1")
    end
end

local instance = turbo.ioloop.instance()

-- no need for background poll for mode 2
if g_wmmd_mode ~= "online" then
  -- Start a data collector "task", "run" lives in data_collector.lua
  -- Note the method (poll/notification) is different depending on the configuration
  if config.use_polling then
      instance:set_interval(config.POLL_INTERVAL_MS, data_collector.run)
  else
      local cb, rdbFD = luardb.synchronousMode(true)
      instance:add_handler(rdbFD, turbo.ioloop.READ, cb)
      luardb.watch(config.rdb_cell_manual_qty, data_collector.run)
  end

  -- start a slow poll "task" to monitor the system status, for example the fact that
  -- RF is available
  instance:set_interval(config.STATUS_POLL_INTERVAL_MS, data_collector.status_poll)
end

instance:start()
