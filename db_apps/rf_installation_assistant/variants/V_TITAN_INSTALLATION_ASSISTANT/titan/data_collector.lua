-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
-- data_collector.lua is a loadable library/module that support the Turbo based web server
-- for supporting Titan Installation Assistant functionality.
--
-- Provides routines to collect run time Lua data structures from RF and battery related
-- RDB variables. Runs in periodically restartable timer based callback.
--
-- The data structures are then used by Turbo based web server to format JSON data
-- as responses to HTML requests, thus implementing dynamically updating web page

require('stringutil')

local luarrb = require("luardb")
local llog = require("luasyslog")

local M = {}

pcall(function() log.open("installation assistant", "LOG_DAEMON") end)

-- warning codes. Must match client side warning string currently in create_html.js
g_warning_codes = {
    warn_no_warnings = 0,
    warn_cell_changed_during_ttest = 1,
    warn_cell_not_entered = 2,   -- serving cell wasn't entered in data entry screen
    warn_cell_not_first_choice = 3,   -- serving cell wasn't the first choice cell entered in data entry screen
    warn_no_data_connection = 4   -- no data connection, but speed test was somehow initiated
}

-- globals build at run-time
-- The structures are designed to avoid too much data going between client and the server.

g_user_selected_data = {} -- an array, indexed by [1] [2] [3], organized in user entry order

-- An array, as entered by the user for min rsrps levels. This is used only if
-- non-default rsrp pass levels are entered for a particular cell.
-- These are negative numbers
g_user_selected_rsrp_pass = {}

-- This is a mirror of rrc information read from RDB
g_rrc_info={}
-- indexed by earfcn and pci, contains 15 digit cell sector id - stored as a string.
g_cell_lookup={}

-- a flag, which if set to 1, indicates that we cannot provide any RF related information. For example, wwan is not up.
g_rf_unavailable = false

-- a flag to indicate there is no data connection
g_data_connection_unavailable = false

-- when this mode is set to true, all cells (even those not entered in the UI) are shown. However, it still only
-- applies to WLL cells, unless g_show_non_wll_cells is set to true.
g_show_fwi_cells_mode = false

-- if this is set, then cells that are plain mobility cells (e.g. not set with acting home PLMN / dual PLMN) are also shown
g_show_non_dual_plmn_cells = false

-- if g_show_fwi_cells_mode, g_show_non_dual_plmn_cells and this variable are all set, then all cells, inclulding
-- those that are not in the RDB whitelist are shown. Whitelist contains 2 PLMNs used in AT&T WLL (Wireless Local Loop)
g_show_non_wll_cells = false

-- stats contains summary of all data (but not individual samples), such as max, min, average, time stamp
-- and number of samples. It is indexed by earfcn/pci
-- it is basically a summary of all data contained in rf_samples
g_rf_stats = {}
-- contains all samples. Indexed by earfcn/pci
g_rf_samples = {}
-- contains the last RF reading only
g_current_rf_readings = {}
-- contains throughput test results
g_ttest_result = {}
g_ttest_result.test_data = {}
g_ttest_result.gen_data = {}

-- monitoring for changing serving cell id
g_cell_monitor = {}
g_cell_monitor.cell_on_start = 0
g_cell_monitor.cell_changed = false

local orientation_accuracy_levels = {
    ['Unavailable'] = 0,
    ['Unreliable'] = 1,
    ['Low'] = 2,
    ['Medium'] = 3,
    ['High'] = 4,
}

--[[
Given pci, find cell_id that corresponds to it, or 0 if no pci is found
Uses rrc_info data structures.
]]--
function get_cell_sector_id_by_pci_earfcn(pci, earfcn)
    for i=1,#g_rrc_info,1 do
        if g_rrc_info[i].pci == pci and g_rrc_info[i].earfcn == earfcn then
            --print ("Has dual plmn ", i, " ", g_rrc_info[i].has_dual_plmn, "Cell id", g_rrc_info[i].cell_id)
            if g_show_non_dual_plmn_cells or g_rrc_info[i].has_dual_plmn then
                return g_rrc_info[i].cell_sector_id, g_rrc_info[i].cell_id
            end
        end
    end
    return 0, 0
end

-- return a battery information table if the battery is available, nil otherwise
local function read_battery()
    local battery = {}
    if M.config.capabilities.nrb200 then
        battery.status = luardb.get(M.config.rdb_batt_status) or 'Error'
        battery.charge_percentage = tonumber(luardb.get(M.config.rdb_batt_charge_percentage)) or 0
    else
        battery.status = 'Error'
        battery.charge_percentage = 0
    end
    return battery
end

-- return an orientation information table if the orientation is available, nil otherwise
local function read_orientation()
    local orientation = nil
    if M.config.capabilities.orientation then
        accuracy = luardb.get(M.config.rdb_orientation_prefix .. 'acc_status')
        azimuth = luardb.get(M.config.rdb_orientation_prefix .. 'azimuth')
        elevation = luardb.get(M.config.rdb_orientation_prefix .. 'pitch')
        if accuracy and accuracy:len() and azimuth then
            orientation = {
                -- Capitalise the first letter
                accuracy = (accuracy:sub(1, 1):upper() ..
                           (accuracy:sub(2) and accuracy:sub(2):lower() or '')),
                azimuth = tonumber(azimuth) or 0,
                elevation = tonumber(elevation) or 0
            }
        end
    end
    return orientation
end

-- return network registration status as a string
local function get_network_registration_status()
    local regStatusStrings = {
        ["0"] = "Not registered, searching stopped",
        ["1"] = "Registered, home network",
        ["2"] = "Not registered, searching",
        ["3"] = "Registration denied",
        ["4"] = "Unknown",
        ["5"] = "Registered, roaming",
        ["6"] = "Registered for SMS(home network)",
        ["7"] = "Registered for SMS(roaming)",
        ["8"] = "Emergency",
        ["9"] = "N/A",
    }

    local regStatusCode = luardb.get("wwan.0.system_network_status.reg_stat")
    return regStatusStrings[regStatusCode] or "N/A"
end

-- return the connection up time of the specified profile
local function get_connection_uptime(profile_id)
    local usage_current = luardb.get("link.profile." .. profile_id .. ".usage_current")
    if not usage_current then
        return "N/A"
    end

    local usage_elems = usage_current:explode(",")
    if #usage_elems == 0 then
        return "N/A"
    end

    local current_time = os.time()
    local start_time = tonumber(usage_elems[1])
    if not start_time or start_time > current_time then
        return "N/A"
    end

    local connUpTime =  current_time - start_time
    return string.format("%d:%02d:%02d", connUpTime / 3600 , (connUpTime / 60) % 60,
                         connUpTime % 60)
end

-- return current system status information
local function read_system_status()
    local sys_status = {
        CellConn = {
            SimStatus = luardb.get("wwan.0.sim.status.status"),
            RegStatus = get_network_registration_status(),
        },
        WwanConn = {
            -- Show the first APN that should be the default APN in the
            -- installation stage.
            [1] = {
                APN = luardb.get("link.profile.1.apn"),
                IPv4Addr = luardb.get("link.profile.1.iplocal"),
                IPv4Dns1 = luardb.get("link.profile.1.dns1"),
                IPv4Dns2 = luardb.get("link.profile.1.dns2"),
                ConnUpTime = get_connection_uptime(1),
            },
        },
        Versions = {
            SwVersion = luardb.get("sw.version") or 0,
        },
    }

    return sys_status
end

--[[
When data for cell/earfcn is too old, all samples, and all stats are removed for the element relevant to
channel/cell
]]--
local function cleanup_old_data(ts)
    for earfcn, pci_list in pairs(g_rf_stats) do
        for pci, stats_summary in pairs(pci_list) do
            if (ts - stats_summary['most_recent_sample_ts']) > M.config.AGING_TIME_SECONDS then
                -- note that setting g_rf_stats[earfcn][pci] to {} is not what we want as the
                -- empty element [earfcn][pci] will still exist - we want this element to be
                -- removed completely
                g_rf_stats[earfcn][pci] = nil

                -- same as above
                g_rf_samples[earfcn][pci] = nil
            end
        end
    end
end

-- Whatever the user entered in data entry page is stored in
-- g_user_selected_rsrp_pass array, which can be between 0 and 3 elements long.
-- However, we do not really know how this relates to the cell without looking it up
local function get_rsrp_pass_level(cell_sector_id)

    local ret = nil

    for i = 1, #g_user_selected_data, 1 do
        if cell_sector_id == g_user_selected_data[i] then
            ret = g_user_selected_rsrp_pass[i]
        end
    end

    if ret == nil or ret < M.config.rf_limits.RSRP.min or ret > M.config.rf_limits.RSRP.max then
        ret = M.config.rf_limits.RSRP.pass -- default level
    end
    return ret
end


--[[
Every time we read RDB for current RF readings, we call this function as many times as we have
cells in RDB. Then samples object which contains individual samples is updated, and stats object
which derives stats parameters from samples is recalculated.
The oldest sample (which is always the first in the table) is removed if the limit of number of samples
is reached.
]]--
local function rf_update_stats(curr_data_sample, most_recent_sample_ts)

    local earfcn = curr_data_sample.earfcn
    local pci = curr_data_sample.pci
    local cell_type = curr_data_sample.cell_type
    local cell_sector_id = curr_data_sample.cell_sector_id
    local freq = curr_data_sample.freq
    local rsrp = tonumber(curr_data_sample.rsrp)
    local rsrq = tonumber(curr_data_sample.rsrq)
    local orientation = curr_data_sample.orientation

    -- 1) update samples arrays

    if not g_rf_samples[earfcn] then
        g_rf_samples[earfcn] = {}
    end

    if not g_rf_samples[earfcn][pci] then
        g_rf_samples[earfcn][pci] = {}
    end

    if not g_rf_samples[earfcn][pci].history then
        g_rf_samples[earfcn][pci].history = {}
    end

    if #g_rf_samples[earfcn][pci].history > M.config.RF_HISTORY_SIZE-1 then
        table.remove(g_rf_samples[earfcn][pci].history,1)
    end

    table.insert(g_rf_samples[earfcn][pci].history, {['cell_type']=cell_type, ['earfcn']=earfcn, ['freq']=freq,
        ['rsrp'] = rsrp, ['rsrq'] = rsrq, ['cell_sector_id'] = cell_sector_id, ['orientation']=orientation})
    --print ("History size: ", #g_rf_samples[earfcn][pci].history)

    -- 2) calculate stats over existing samples
    if not g_rf_stats[earfcn] then
        g_rf_stats[earfcn] = {}
    end

    if not g_rf_stats[earfcn][pci] then
        g_rf_stats[earfcn][pci] = {}
    end

    -- shorthand to element of g_rf_stats with the given earfcn and pci
    local stats_elem = g_rf_stats[earfcn][pci]

    stats_elem.freq = freq
    stats_elem.cell_sector_id = cell_sector_id

    -- record min and max value since run
    -- Do the rsrp first
    if not stats_elem.rsrp_min or stats_elem.rsrp_min > rsrp then
        stats_elem.rsrp_min  = rsrp
    end
    if not stats_elem.rsrp_max or stats_elem.rsrp_max < rsrp then
        stats_elem.rsrp_max = rsrp
        -- Update the best orientation as long as it meets the mininum RSRQ requirement.
        if rsrq >= M.config.rf_limits.RSRQ.pass then
            stats_elem.best_orientation = orientation
        end
    end

    -- Do the rsrq next
    if not stats_elem.rsrq_min or stats_elem.rsrq_min > rsrq then
        stats_elem.rsrq_min  = rsrq
    end
    if not stats_elem.rsrq_max or stats_elem.rsrq_max < rsrq then
        stats_elem.rsrq_max = rsrq
    end

    -- ns short for num_samples, keeping json short due to bandwidth limitations of nrb200
    -- record the number of samples and the time stamp of the most recent one
    stats_elem.ns = #g_rf_samples[earfcn][pci].history
    stats_elem.most_recent_sample_ts = most_recent_sample_ts

    -- calculate averages
    local sum_rsrp = 0
    local sum_rsrq = 0
    for _, x in pairs(g_rf_samples[earfcn][pci].history) do
        sum_rsrp = sum_rsrp + x.rsrp
        sum_rsrq = sum_rsrq + x.rsrq
    end

    if stats_elem.ns > 0 then
        stats_elem.rsrp_avg = sum_rsrp/stats_elem.ns
        stats_elem.rsrq_avg = sum_rsrq/stats_elem.ns
    end

    if stats_elem.ns >= M.config.RF_NUMBER_OF_SAMPLES_FOR_PASS then
        if stats_elem.rsrp_avg >= get_rsrp_pass_level(cell_sector_id) then
            stats_elem.res = "Pass"
        else
            stats_elem.res = "Fail"
        end
    else
        stats_elem.res = "Insufficient data"
    end
--    print ("Cell id: ", stats_elem.pci, " earfcn ", stats_elem.earfcn)
end

-- For sorting purposes, AT&T requires the Cell Sector IDs entered first to
-- be displayed first. So a simple helper to return the rank (3-0) of cell
-- in question. 0 means this cell has not been selected by user for display
function get_cell_rank(cell_sector_id)

    -- no rank in show all cells mode. Cells are ordered by rsrp
    if (show_fwi_cells_mode() == true) then
        return 0
    end

    local rank = M.config.MAX_USER_CELLS
    for i = 1, #g_user_selected_data, 1 do
        if cell_sector_id == g_user_selected_data[i] then
            return rank
        end
        rank = rank - 1
    end
    return 0
end

-- if this function returns true, the web interface will show data for all available
-- cells, even if they are not entered in the data entry page.
function show_fwi_cells_mode()
    return g_show_fwi_cells_mode
end

-- when g_show_all_cells_mode mode is set to true, all cells (even those not entered in the UI) are shown.
-- However, it still only applies to whitelisted cells, unless g_show_non_wll_cells is set to 1
function is_wll_cell(cell_sector_id)
    if g_show_non_wll_cells == true then
        return true
    else
        local keys = luardb.keys(M.config.rdb_wll_mcc_mnc_prefix)
        for _, key in ipairs(keys) do
            local prefix = luardb.get(key)

            if prefix == nil then
                return false
            end

            if string.sub(cell_sector_id, 1, 6) == prefix then
                return true
            end
        end
    end
    return false
end


-- According to input EARFCN, calculate the band, and the (Downlink) freqency.
-- Returns 2 arguments, e.g. band, freq
function earfcn_map(dl_earfcn)
    -- Currently only band 2 and band 30 are supported. So other conversion params are not listed here.
    local conversion = {
        [2] = {dl_low=1930, dl_offset=600, dl_earfcn={low=600, high=1199}},
        [30] = {dl_low=2350, dl_offset=9770, dl_earfcn={low=9770, high=9869}},
    }

    local freq = 0
    for band, param in pairs(conversion) do
        if dl_earfcn >= param.dl_earfcn.low and dl_earfcn <= param.dl_earfcn.high then
            freq = string.format("%d (MHz)", param.dl_low + 0.1 * (dl_earfcn - param.dl_offset))
            return band, freq
        end
    end
    return 0, 0
end

-- Average speeds of completed test repeats.
-- Returns two values:
-- speed (in Mbytes per second)
-- number of repeats succeeded so far.
local function average_speeds(rdb_root)
    local avg = 0
    local count = 0
    local speed_ok_count = 0

    -- if not in RDB assume a very high expected speed (should never happen)
    local speed_expected = tonumber(luardb.get(rdb_root.."speed_expected")) or 1000000
    local max_repeats = tonumber(luardb.get(rdb_root.."repeats")) or 1
    for i = 1, max_repeats, 1 do
        local single_sample = tonumber(luardb.get(rdb_root.."res."..tostring(i-1)..".speed"))

        if single_sample ~= nil then
            avg = avg + single_sample
            count = count + 1
            if single_sample > speed_expected then
                speed_ok_count = speed_ok_count + 1
            end
        end
    end

    if count > 0 then
        return avg/count, speed_ok_count
    end

    return "N/A", speed_ok_count
end

-- Is the cell sector id one of 3 entered by the user
local function is_user_entered_cell(cell)
    for i = 1, #g_user_selected_data, 1 do
        if g_user_selected_data[i] == cell then
            return true
        end
    end
    return false
end

-- Format any errors that may be needed to be shown on UI
-- for now, simply warn that cell has changed during the test
local function get_warning()
    if g_cell_monitor.cell_changed then
        return g_warning_codes.warn_cell_changed_during_ttest
    elseif g_data_connection_unavailable then
        return g_warning_codes.warn_no_data_connection
    elseif not is_user_entered_cell(get_serving_cell_from_rdb()) then
        return g_warning_codes.warn_cell_not_entered
    end
    return 0
end

-- process ttest results - e.g. read rdb variables generated by external throughput test
-- and set internal data structures accordingly
local function process_ttest_results(rdb_root, max_tests)

    for i = 1, max_tests, 1 do
        local rdb_index = i - 1
        local _rdb_root = rdb_root.."."..tostring(rdb_index).."."
        local repeat_no = tonumber(luardb.get(_rdb_root.."current_repeat")) or 0
        local _rdb_root_res = _rdb_root.."res."..tostring(repeat_no).."."

        if g_ttest_result.test_data[i] == nil then g_ttest_result.test_data[i] = {} end
        g_ttest_result.test_data[i].status = luardb.get(_rdb_root_res.."status") or "not started"
        g_ttest_result.test_data[i].speed_expected = luardb.get(_rdb_root.."speed_expected") or 1000000 -- some unreleasitically high number
        local is_dload = luardb.get(_rdb_root.."is_dload")
        if is_dload == "1" then g_ttest_result.test_data[i].type = "Download" else g_ttest_result.test_data[i].type = "Upload" end
        if g_ttest_result.test_data[i].status == "completed" then
            g_ttest_result.test_data[i].res = luardb.get(_rdb_root_res.."result") or "fail" -- default to fail.
            g_ttest_result.test_data[i].duration = luardb.get(_rdb_root_res.."duration") or "N/A"
        else
            g_ttest_result.test_data[i].res = "N/A"
            g_ttest_result.test_data[i].duration = "N/A"
        end
        g_ttest_result.test_data[i].speed = luardb.get(_rdb_root_res.."speed") or "N/A"

        g_ttest_result.test_data[i].method = "FTP" -- hardcoded for now
        g_ttest_result.test_data[i].server = luardb.get(_rdb_root.."server")
        g_ttest_result.test_data[i].value = luardb.get(_rdb_root_res.."progress") or 0
        g_ttest_result.test_data[i].max = luardb.get(_rdb_root_res.."expected_secs") or 100
        g_ttest_result.test_data[i].repeats = luardb.get(_rdb_root.."repeats") or 1
        g_ttest_result.test_data[i].repeat_no = repeat_no + 1
        g_ttest_result.test_data[i].avg_speed, g_ttest_result.test_data[i].succ_count = average_speeds(_rdb_root)
    end
    g_ttest_result.gen_data.serving_cell = get_serving_cell_from_rdb()
    g_ttest_result.gen_data.phys_cell = luardb.get("wwan.0.radio_stack.e_utra_measurement_report.servphyscellid") or 0
    g_ttest_result.gen_data.imei = luardb.get("wwan.0.imei") or 0
    g_ttest_result.gen_data.iccid = luardb.get("wwan.0.system_network_status.simICCID") or 0
    g_ttest_result.gen_data.msisdn = luardb.get("wwan.0.sim.data.msisdn") or 0
    g_ttest_result.gen_data.hw_ver = luardb.get("system.product.hwver") or 0 -- this is empty unless factory calibrated.
    g_ttest_result.gen_data.sw_ver = luardb.get("sw.version") or 0
    g_ttest_result.gen_data.sn = luardb.get("system.product.sn") or 0
    g_ttest_result.gen_data.mac = luardb.get("system.product.mac") or 0
    g_ttest_result.gen_data.pn = luardb.get("system.product.pn") or 0
    g_ttest_result.gen_data.class = luardb.get("system.product.class") or 0
    if M.config.capabilities.nrb200 then
        g_ttest_result.gen_data.hw_ver_nrb200 = luardb.get("service.nrb200.hw_ver") or 0
        g_ttest_result.gen_data.sw_ver_nrb200 = luardb.get("service.nrb200.sw_ver") or 0
    end
    g_ttest_result.gen_data.warning = get_warning()
end

-- this is called periodically from turbo framework. Should be set to network manager's standard update frequency (10 seconds)
-- Depending on the configuration setting, this is either done under timer control, or using trigger
-- mechanism of luardb (this function is kicked when network manager writes to the subscribed RDBs)
local function poll()

    -- work out cell quantity in RDB
    local rf_scan_cell_qty_rdb = luardb.get(M.config.rdb_cell_qty)
    local val = tonumber(rf_scan_cell_qty_rdb) or 0
    --print("Number of cells ", val, "Qty ", rf_scan_cell_qty_rdb, "Rdb ", config.rdb_cell_qty)

    local current_ts = os.time()
    local current_orientation = read_orientation()

    local serving_cell = get_serving_cell_from_rdb()

    -- zeroise current readings array
    g_current_rf_readings={}

    for i=1, val do

        -- reading something like this:
        -- wwan.0.cell_measurement.0	-	E,900,1,-103.1,-8.2
        local t = luardb.get(M.config.rdb_cell_measurement_prefix..i-1)
        if t then
            local l = t:explode(',')
            if #l == 5 then
                g_current_rf_readings[i]= {
                    cell_type=(l[1]),
                    earfcn=tonumber(l[2]),
                    pci=tonumber(l[3]),
                    rsrp=tonumber(l[4]),
                    rsrq=tonumber(l[5])
                }
                g_current_rf_readings[i].band, g_current_rf_readings[i].freq=earfcn_map(g_current_rf_readings[i].earfcn)

                -- cell sector id cannot be found from pci and earfcn, the next function returns 0,0
                g_current_rf_readings[i].cell_sector_id, _ = get_cell_sector_id_by_pci_earfcn(g_current_rf_readings[i].pci, g_current_rf_readings[i].earfcn)

                g_current_rf_readings[i].serving_cell = serving_cell
                --[[
                    Optimization of sorting routine.
                    From the documentation for table.sort:

                    If comp is given, then it must be a function that receives two table elements,
                    and returns true when the first is less than the second (so that not comp(a[i+1],a[i]) will be true after the sort).

                    Failure to comply with the above causes sort to hang and turbo to fail in the most un-intuitive ways
                ]]--
                g_current_rf_readings[i].rank = get_cell_rank(g_current_rf_readings[i].cell_sector_id)

                -- rssinr is only present in serving cell
                if g_current_rf_readings[i].cell_sector_id == g_current_rf_readings[i].serving_cell then
                    g_current_rf_readings[i].rssinr = tonumber(luardb.get(M.config.rdb_rssinr)) or 0
                end

                g_current_rf_readings[i].orientation = current_orientation

                -- some fiddling around with rsrp pass level. If entered by the user for that particular PCI/cell id, use it. Otherwise, use default value
                g_current_rf_readings[i].rsrp_pass = get_rsrp_pass_level(g_current_rf_readings[i].cell_sector_id)

                if ((g_current_rf_readings[i].rsrp >= g_current_rf_readings[i].rsrp_pass) and
                    (g_current_rf_readings[i].rsrq >= M.config.rf_limits.RSRQ.pass)) then
                    g_current_rf_readings[i].res = "Good"
                else
                    g_current_rf_readings[i].res = "Bad"
                end
                --print("rf_data size", #g_current_rf_readings)
            else
                -- @TODO - do something more meaningful here
                print("Error rdb data format wrong, size ", #g_current_rf_readings)
                return
            end
        else
        -- @TODO - do something more meaningful here
            print("Error rdb data cannot be found")
            return
        end
        rf_update_stats(g_current_rf_readings[i], current_ts)
    end

    -- according to requirements received from AT&T, sort by order of entry. If not entered, sort by rsrp (best one shown first)
    table.sort(g_current_rf_readings, function(a,b)
        if a.rank == b.rank then return a.rsrp > b.rsrp else return a.rank > b.rank end
    end)

    -- completely remove stats for elements that have stale data
    cleanup_old_data(current_ts)
end

-- externally accessible callback (called from turbo io loop)
function M.run()
    poll()
end

-- get battery information
function M.get_battery_reading()
    -- return real-time orientation information
    return read_battery()
end

-- get orientation information
function M.get_orientation_reading()
    -- return real-time orientation information
    return read_orientation() or {}
end

-- get system status information
function M.get_system_status_reading()
    -- return real-time system status information
    return read_system_status() or {}
end

-- g_cell_lookup tables is indexed by earfcn/pci and contains the cell sector id,
-- which is a 15-digit long string.
function update_cell_lookup_table()

    g_cell_lookup = {}
    local earfcn
    for i=1,#g_rrc_info,1 do
        local earfcn = g_rrc_info[i].earfcn
        local pci = g_rrc_info[i].pci
        if not g_cell_lookup[earfcn] then
            g_cell_lookup[earfcn] = {}
        end
        if not g_cell_lookup[earfcn][pci] then
            g_cell_lookup[earfcn][pci] = {}
        end
        g_cell_lookup[earfcn][pci] = g_rrc_info[i].cell_sector_id
    end
end

-- given mcc, mnc and cell_id, build cell sector id 15-digit string
-- mcc is always three characters long
-- mnc is two or three characters long
-- the remaining 9 or 10 characters, padded with leading zeroes is the cell id
function build_cell_sector_id(mcc, mnc, cell_id)
    -- probably the fastest method to do this is a lookup table
    zero_pad =
    {
        "",
        "0",
        "00",
        "000",
        "0000",
        "00000",
        "000000",
        "0000000",
        "00000000",
        "000000000",
    }

    -- the extreme cases are:
    -- AAABBBCCCCCCCCC no padding necessary
    -- AAABBC, e.g. 001017 e.g. 9 zeros need to be added
    local len = string.len(mcc..mnc..cell_id)
    if len > 15 or len < 6 or string.len(mcc) ~= 3 or string.len(mnc) < 2 or string.len(mnc) > 3 or string.len(cell_id) < 1 then
        return ""
    end
    return (mcc..mnc..zero_pad[16-len]..cell_id)
end

-- We need to know which cell is the current serving cell. This can be read from RDB
function get_serving_cell_from_rdb()
    return luardb.get("wwan.0.system_network_status.ECGI") or ""
end

--[[
Reads rdb variables for rrc cell info, e.g. wwan.0.rrc_info.cell.6
The additional complication is that the rrc_info.cell variables can be
duplicated for the same unique pair of (earfcn, pci) if the network is
setup with acting PLMN - for example, here is a real-life capture that
causes a problem. Here acting PLMN is 312680 and SIM is setup for 310410:

wwan.0.cell_measurement.0 "E,9820,491,-102.9,-12.6"
wwan.0.cell_measurement.1 "E,9820,495,-110.4,-14.1"
wwan.0.cell_measurement.qty "2"
wwan.0.rrc_info.cell.0 "310,410,9820,495,89303189"
wwan.0.rrc_info.cell.1 "312,680,9820,495,89303189"
wwan.0.rrc_info.cell.2 "312,420,1175,22,552194"
wwan.0.rrc_info.cell.3 "310,410,9820,491,89293975"
wwan.0.rrc_info.cell.4 "312,680,9820,491,89293975"
wwan.0.rrc_info.cell.qty 5
wwan.0.radio_stack.e_utra_measurement_report.cellid 89293975
wwan.0.radio_stack.e_utra_measurement_report.mcc 312
wwan.0.radio_stack.e_utra_measurement_report.mnc 680
wwan.0.radio_stack.e_utra_measurement_report.servphyscellid 491

Because the same pair (earfcn, pci) - (9820, 491) and (9820,495) is present
twice in the RDB- once for 310,410 PLMN and once for 312,680 PLMN, lookup assumes
the serving cell is 89303189 rather than 89293975 and as a result,
the serving cell (asterisk) is not shown in the GUI.

The solution is, when reading rrc info, to ignore anything but the first entry.
The first entry is guaranteed to contain the correct ECGI according to 3GPP

In addition, returns the index of existing element if found as the second argument
]]--

-- A tiny helper to implement the above
function check_new_entry(earfcn, pci)
    for i=1,#g_rrc_info,1 do
        if g_rrc_info[i].earfcn == earfcn and g_rrc_info[i].pci == pci then
            return false, i
        end
    end
    return true, 0
end

function read_rrc_rdbs()
    local rrc_info_qty = tonumber(luardb.get(M.config.rdb_rrc_info_qty)) or 0
    g_rrc_info = {}
    local j = 1
    if rrc_info_qty > 0 then -- @todo - should there be an upper limit?
        for i=1, rrc_info_qty do
            -- reading something like this:
            -- wwan.0.rrc_info.cell.0 310,410,904,5,27447301
            -- being mcc, mnc, earfcn, pci, cell_id
            local t = luardb.get(M.config.rdb_rrc_info_prefix..i-1)
            if t then
                local l = t:explode(',')
                if #l == 5 then
                    -- ignore unless this is the first entry in the list for each (earfcn, pci)
                    is_new, index = check_new_entry(tonumber(l[3]), tonumber(l[4]))
                    if is_new then
                        g_rrc_info[j]= {
                            mcc=l[1],
                            mnc=l[2],
                            earfcn=tonumber(l[3]),
                            pci=tonumber(l[4]),
                            cell_id=l[5]
                        }

                        -- this is a combination of 3 values
                        g_rrc_info[j].cell_sector_id = build_cell_sector_id(g_rrc_info[j].mcc, g_rrc_info[j].mnc, g_rrc_info[j].cell_id)
                        g_rrc_info[j].has_dual_plmn = false -- first time discovered, not a dual plmn until second record found
                        j = j + 1
                    else
                        if index >= 1 and index <= #g_rrc_info then
                            g_rrc_info[index].has_dual_plmn = true -- mark the previously found entry as dual PLMN
                        end
                    end
                end
            end
        end
    end

    update_cell_lookup_table()
end

-- Monitor serving cell change during the test
local function cell_change_monitor()
    local cell = luardb.get(M.config.rdb_serving_cell_info_prefix.."cellid") or 0
    if cell ~= 0 and cell ~= g_cell_monitor.cell_on_start and g_cell_monitor.cell_on_start ~= 0 then
        g_cell_monitor.cell_changed = true
        g_cell_monitor.cell_final = cell
        --print ("Cell changed! "..cell.." ".. g_cell_monitor.cell_on_start)
    end
end

-- This is a poll function called periodically that simply needs to set some
-- global "environmental" variables that will affect how web pages are server. For example, if
-- the radio has not attached, the whole web interface apart from the battery is probably irrelevant.
-- For now, there is a single global set here - this can become more complex in the future when
-- things like throughput test and so forth are supported.
function M.status_poll()

    -- work out if rf is available
    if luardb.get("link.profile.1.enable") ~= "1" or luardb.get("wwan.0.sim.status.status") ~= "SIM OK" then
        g_rf_unavailable = true
    else
        g_rf_unavailable = false
    end

    -- work out if data connectivity is available.
    if luardb.get("link.profile.1.status") ~= "up" or luardb.get("wwan.0.system_network_status.attached") ~= "1" then
        g_data_connection_unavailable = true
    else
        g_data_connection_unavailable = false
    end

    read_rrc_rdbs()

    cell_change_monitor()

    -- process RDBs relating to throughput test. This is asynchronous to any web interface activity.
    -- hardcoded to 2 tests (one download, one upload)
    process_ttest_results("service.ttest.ftp", 2)
end

-- Initialises data_collector module
-- @param config Configuration
function M.init(config)
    M.config = config
end

return M
