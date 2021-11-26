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
-- RRC stat
g_rrc_stat = ""
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

-- stats contains summary of all data (but not individual samples), such as max, min, of detected cells
-- It is indexed by earfcn/pci
-- it is saved to add as data item of QR code in later stages
g_rf_stats = {}
-- number of consecutive scan results: an entry in g_rf_stats will be deleted if it is not present in
-- consecute scan results
g_rf_stats_stale_threshold = 3

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
    battery.status = luardb.get(M.config.rdb_batt_status)
    if M.config.capabilities.nitv2 and battery.status then
        if battery.status == "trickle" or battery.status == "fast" then
            battery.status = "Charging"
        elseif battery.status == "none" or battery.status == "done" then
            battery.status = "Not charging"
        elseif battery.status == "fault" then
            battery.status = "Fault"
        else
            battery.status = "Unknown"
        end
    end
    battery.status = battery.status or 'Error'
    battery.charge_percentage = tonumber(luardb.get(M.config.rdb_batt_charge_percentage)) or 0
    return battery
end

--[[
When data for cell/earfcn is too old and all stats are removed for the element relevant to
channel/cell
]]--
local function cleanup_rf_stats(ts)
    for earfcn, pci_list in pairs(g_rf_stats) do
        for pci, stats_summary in pairs(pci_list) do
            if stats_summary.most_recent_sample_ts ~= ts then
                stats_summary.stale_index = stats_summary.stale_index + 1
            end
            -- delete rf_stats entry if stale_index >= threshold i.e. no corresponding entry in
            -- consecutive recent scan result
            if stats_summary.stale_index >= g_rf_stats_stale_threshold then
                g_rf_stats[earfcn][pci] = nil
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
    local rssinr = tonumber(curr_data_sample.rssinr)
    local orientation = curr_data_sample.orientation

    -- calculate stats over existing samples
    if not g_rf_stats[earfcn] then
        g_rf_stats[earfcn] = {}
    end

    if not g_rf_stats[earfcn][pci] then
        g_rf_stats[earfcn][pci] = {}
    end

    -- shorthand to element of g_rf_stats with the given earfcn and pci
    local stats_elem = g_rf_stats[earfcn][pci]

    stats_elem.most_recent_sample_ts = most_recent_sample_ts
    stats_elem.stale_index = 0

    stats_elem.freq = freq
    stats_elem.cell_sector_id = cell_sector_id

    -- record min and max value since run
    -- Do the rsrp first
    if not stats_elem.rsrp_min or stats_elem.rsrp_min > rsrp then
        stats_elem.rsrp_min  = rsrp
    end
    if not stats_elem.rsrp_max or stats_elem.rsrp_max < rsrp then
        stats_elem.rsrp_max = rsrp
    end

    -- Do the rsrq next
    if not stats_elem.rsrq_min or stats_elem.rsrq_min > rsrq then
        stats_elem.rsrq_min  = rsrq
    end
    if not stats_elem.rsrq_max or stats_elem.rsrq_max < rsrq then
        stats_elem.rsrq_max = rsrq
    end

    if rssinr and (not stats_elem.rssinr_max or stats_elem.rssinr_max < rssinr) then
        stats_elem.rssinr_max = rssinr
    end
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
        g_ttest_result.test_data[i].server = luardb.get(M.config.get_rdb_prefix.._rdb_root.."server")
        g_ttest_result.test_data[i].value = luardb.get(_rdb_root_res.."progress") or 0
        g_ttest_result.test_data[i].max = luardb.get(_rdb_root_res.."expected_secs") or 100
        g_ttest_result.test_data[i].repeats = luardb.get(_rdb_root.."repeats") or 1
        g_ttest_result.test_data[i].repeat_no = repeat_no + 1
        g_ttest_result.test_data[i].avg_speed, g_ttest_result.test_data[i].succ_count = average_speeds(_rdb_root)

        -- write completed average speed in Mbps to a RDB variable to help other component to get the result
        if g_ttest_result.test_data[i].status == "completed" then
            local avg_speed_bytes_per_second = tonumber(g_ttest_result.test_data[i].avg_speed)
            if avg_speed_bytes_per_second then
                local avg_speed_Mbps = avg_speed_bytes_per_second/125000
                -- AT&T wants only 2 decimal places
                luardb.set(_rdb_root.."avg_speed_mbps", string.format("%.2f", avg_speed_Mbps))
            else
                luardb.set(_rdb_root.."avg_speed_mbps", "")
            end
        end
    end
    g_ttest_result.gen_data.serving_cell = get_serving_cell_from_rdb()
    g_ttest_result.gen_data.phys_cell = luardb.get("wwan.0.radio_stack.e_utra_measurement_report.servphyscellid") or 0
    g_ttest_result.gen_data.warning = get_warning()
end

-- the last sequence number on manual cell scan
local last_manual_cell_scan_seq = 0
-- the last timestamp on manual cell scan
local last_manual_cell_scan_ts = 0

-- Check and erase stale cell measurement data
-- Cell measurement data is considered as stale if it is not updated after a predefined period.
-- It occurs when all network scan fails during that period. As the network scan is actually PCI PLMN scanning,
-- network scan fails may be because UE enters RRC Connected state.
-- No valid cell measurement is provided so current cell measurement is stale.
local function check_stale_cell_measurement()
    local current_ts = os.time()
    if last_manual_cell_scan_ts > 0 and current_ts > last_manual_cell_scan_ts + M.config.MIN_MANUAL_CELL_REFRESH_TIME_SEC then
        -- Manual cell measurements are no longer valid
        -- zeroise current readings array
        g_current_rf_readings = {}
        last_manual_cell_scan_ts = 0
    end
end

-- get cell measurement data
local function get_cell_measurement()

    local rf_scan_cell_seq_rdb = luardb.get(M.config.rdb_cell_manual_seq)
    local current_ts = os.time()
    local serving_cell = get_serving_cell_from_rdb()
    if #serving_cell == 0 then
        -- RSSINR is stale now, erase it
        luardb.set(M.config.rdb_rssinr, "")
    end

    if last_manual_cell_scan_seq ~= rf_scan_cell_seq_rdb then
        -- new cell measurement available
        -- zeroise current readings array
        g_current_rf_readings = {}

        local rf_scan_cell_qty_rdb = tonumber(luardb.get(M.config.rdb_cell_manual_qty)) or 0
        for i=1, rf_scan_cell_qty_rdb do
            -- example of cell_measurement_line: E,43090,234,-54.88,9.50
            local cell_measurement_line = luardb.get(M.config.rdb_cell_manual_meas_prefix..i-1)
            local cell_measurement = cell_measurement_line and cell_measurement_line:explode(',')
            if cell_measurement and #cell_measurement == 5 then
                g_current_rf_readings[i]= {
                    cell_type=(cell_measurement[1]),
                    earfcn=tonumber(cell_measurement[2]),
                    pci=tonumber(cell_measurement[3]),
                    rsrp=tonumber(cell_measurement[4]),
                    rsrq=tonumber(cell_measurement[5])
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

                -- some fiddling around with rsrp pass level. If entered by the user for that particular PCI/cell id, use it. Otherwise, use default value
                g_current_rf_readings[i].rsrp_pass = get_rsrp_pass_level(g_current_rf_readings[i].cell_sector_id)

                if ((g_current_rf_readings[i].rsrp >= g_current_rf_readings[i].rsrp_pass) and
                    (g_current_rf_readings[i].rsrq >= M.config.rf_limits.RSRQ.pass)) then
                    g_current_rf_readings[i].res = "Good"
                else
                    g_current_rf_readings[i].res = "Bad"
                end

                rf_update_stats(g_current_rf_readings[i], current_ts)
            end
        end
        -- according to requirements received from AT&T, sort by order of entry. If not entered, sort by rsrp (best one shown first)
        table.sort(g_current_rf_readings, function(a,b)
            if a.rank == b.rank then return a.rsrp > b.rsrp else return a.rank > b.rank end
        end)

        last_manual_cell_scan_seq = rf_scan_cell_seq_rdb
        last_manual_cell_scan_ts = current_ts

        -- completely remove stats for elements that have stale data
        cleanup_rf_stats(current_ts)
    end
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

-- helper lookup table to add padding '0' to cell sector ID
local zero_pad = {
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

-- given mcc, mnc and cell_id, build cell sector id 15-digit string
-- mcc is always three characters long
-- mnc is two or three characters long
-- the remaining 9 or 10 characters, padded with leading zeroes is the cell id
function build_cell_sector_id(mcc, mnc, cell_id)
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
    local cell_id = luardb.get("wwan.0.system_network_status.CellID") or ""
    if #cell_id == 0 then
        return ""
    end
    -- Due to AT&T's dual PLMN, ECGI needs to be read from wwan.0.system_network_status.ECGI
    -- which is updated by qdiagd
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

-- get antenna status
local function get_antenna_status()
    local server_data = {}
    if luardb.get("sensors.gps.0.common.status") == "success" then
        server_data.latitude = luardb.get("sensors.gps.0.common.latitude_degrees") or ""
        server_data.longitude = luardb.get("sensors.gps.0.common.longitude_degrees") or ""
        server_data.height = luardb.get("sensors.gps.0.common.height_of_geoid") or ""
    end

    -- orientation sensor calibration status :
    --    0 : Calibrated, all data information is valid
    --    1 : Calibrating
    --    2 : Calibrated, but no GPS (hence azimuth and GPS data is invalid)
    --    3 : Error
    local cal_status = luardb.get("owa.orien.status") or '3'
    server_data.orientation_status = cal_status
    if cal_status == '0' then
      server_data.azimuth = luardb.get("owa.orien.azimuth") or ""
    end
    if cal_status == '0' or cal_status == '2' then
      server_data.bearing = luardb.get("owa.mag.bearing") or ""
      server_data.down_tilt = luardb.get("owa.orien.tilt") or ""
    end
    return server_data
end

-- get registration status information
-- Determine current registration status from RDB variables as below table condition
--
--    Registration status           sas.registration.state    sas.grant.0.state
-- ------------------------------------------------------------------------------------------
--    Not Registered                Unregistered              don't care
--    Registering                   Registering               don't care
--    Registered – Grant Pending    Registered                GRANTED
--    Registered – Grant Authorized Registered                AUTHORIZED
local function get_registration_status()
    local band_name = luardb.get("wwan.0.currentband.config") or ""
    local cbrs_band_mode = "0"
    if band_name == "LTE Band 48 - TDD 3600" then
        cbrs_band_mode = "1"
    end

    local reg_state = luardb.get("sas.registration.state")
    local reg_resp_code = luardb.get("sas.registration.response_code") or ""
    local reg_resp_msg = luardb.get("sas.registration.response_message") or ""
    local reg_resp_data = luardb.get("sas.registration.response_data") or ""
    local grant_state = luardb.get("sas.grant.0.state")
    local grant_resp_reason = luardb.get("sas.grant.0.reason") or ""
    local grant_resp_msg = luardb.get("sas.grant.0.response_message") or ""
    local grant_resp_data = luardb.get("sas.grant.0.response_data") or ""
    local result = ""
    local err_code = reg_resp_code
    local err_msg = reg_resp_msg
    local err_data = reg_resp_data
    local request_error_code = luardb.get("sas.registration.request_error_code") or ""
    if reg_state == 'Unregistered' then
        result = "Not Registered"
    elseif reg_state == 'Registering' then
        result = reg_state
    elseif reg_state == 'Attaching' then
        result = "Attaching LTE"
    elseif reg_state == 'Registered' then
        if grant_state == 'GRANTED' then
            result = "Registered – Grant Pending"
        elseif grant_state == 'AUTHORIZED' then
            result = "Registered – Grant Authorized"
        elseif grant_state == 'Attaching' then
            result = "Registered – Attaching LTE"
        else
            result = "Registered"
        end
        err_code = grant_resp_reason
        err_msg = grant_resp_msg
        err_data = grant_resp_data
        request_error_code = luardb.get("sas.grant.0.request_error_code") or ""
    end

    local server_data = {}
    server_data.reg_state = result
    server_data.err_code = err_code
    server_data.err_msg = err_msg
    server_data.err_data = err_data
    server_data.request_error_code = request_error_code
    server_data.auth_count = luardb.get("sas.authorization_count")
    server_data.scell_id = get_serving_cell_from_rdb()
    server_data.cbrs_band_mode = cbrs_band_mode
    server_data.cpi_id = luardb.get("sas.config.cpiId")
    server_data.sim_status = luardb.get("wwan.0.sim.status.status")
    server_data.network_status_system_mode = luardb.get("wwan.0.system_network_status.system_mode")
    return server_data
end

-- get cell measurement
function M.get_cell_measurement()
    get_cell_measurement()
end

-- read neighbour cells information
function M.read_rrc_rdbs()
   read_rrc_rdbs()
end

-- Read RRC stat and update global variable g_rrc_stat
function M.read_rrc_stat()
    g_rrc_stat = luardb.get(M.config.rdb_rrc_stat) or ""
end

-- get battery information
function M.get_battery_reading()
    -- return real-time orientation information
    return read_battery()
end

-- get antenna status
function M.get_antenna_status()
    -- return real-time antenna status
    return get_antenna_status() or {}
end

-- get registration status information
function M.get_registration_status()
    -- return real-time registration status information
    return get_registration_status() or {}
end

-- get orientation information
function M.get_orientation_data()
    local data = {}
    data.orientation_status = luardb.get("owa.orien.status")
    if data.orientation_status == "0" then
        data.azimuth = luardb.get("owa.orien.azimuth") or ""
    end
    if data.orientation_status == "0" or data.orientation_status == "2" then
        data.down_tilt = luardb.get("owa.orien.tilt") or ""
        data.bearing = luardb.get("owa.mag.bearing") or ""
    end
    return data
end

-- This is a poll function called periodically that simply needs to set some
-- global "environmental" variables that will affect how web pages are server. For example, if
-- the radio has not attached, the whole web interface apart from the battery is probably irrelevant.
-- For now, there is a single global set here - this can become more complex in the future when
-- things like throughput test and so forth are supported.
function M.status_poll()

    -- work out if rf is available
    if luardb.get("wwan.0.sim.status.status") ~= "SIM OK" then
        g_rf_unavailable = true
    else
        g_rf_unavailable = false
    end

    -- work out if data connectivity is available.
    if luardb.get("wwan.0.system_network_status.attached") ~= "1" then
        g_data_connection_unavailable = true
    else
        g_data_connection_unavailable = false
    end

    check_stale_cell_measurement()

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
