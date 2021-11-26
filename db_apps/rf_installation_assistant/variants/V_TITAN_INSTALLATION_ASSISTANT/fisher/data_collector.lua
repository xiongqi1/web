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

local luardb = require("luardb")

local M = {}

pcall(function() log.open("installation assistant", "LOG_DAEMON") end)

-- a flag, which if set to 1, indicates that we cannot provide any RF related information. For example, wwan is not up.
g_rf_unavailable = false

-- stats contains summary of all data (but not individual samples), such as max, min, average, time stamp
-- and number of samples. It is indexed by earfcn/pci
-- it is basically a summary of all data contained in rf_samples
g_rf_stats = {}
-- contains all samples. Indexed by earfcn/pci
g_rf_samples = {}
-- contains the last RF reading only
g_current_rf_readings = {}

-- by default, only cells from white-listed PLMNs are shown. This variable allows showing all cells
g_show_all_cells = false

-- by default, if ecgi is unknown (yet), the cell is not shown
g_show_unknown_cells = false

-- by default, web ui shows ECI. This variable enables showing ECGI instead
g_show_ecgi_or_eci = false

-- This is a mirror of rrc information read from RDB
local g_rrc_info = {}

--[[
Given pci and earfcn, return the corresponding ECGI, or nil if not found
Uses rrc_info data structures.
--]]
local function get_ecgi_by_pci_earfcn(pci, earfcn)
    for _, entry in ipairs(g_rrc_info) do
        if entry.pci == pci and entry.earfcn == earfcn then
            return entry.ecgi
        end
    end
    local serving_cell = get_current_serving_cell()
    if serving_cell.pci == pci and serving_cell.earfcn == earfcn then
        return luardb.get("wwan.0.system_network_status.ECGI")
    end
    return nil
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
        Advance = {
            SimICCID = luardb.get("wwan.0.system_network_status.simICCID"),
            CellId = luardb.get("wwan.0.system_network_status.CellID"),
            PCI = luardb.get("wwan.0.system_network_status.PCID"),
            EARFCN = luardb.get("wwan.0.system_network_status.channel"),
            RSRP = luardb.get(config.rdb_rsrp_0),
            RSRQ = luardb.get("wwan.0.signal.rsrq"),
        },
    }

    -- Use cell_measurement report if available. This is consistent with the main scan page
    -- Serving cell is always index 0.
    local t = luardb.get(M.config.rdb_cell_measurement_prefix.."0")
    if t then
        local l = t:explode(',')
        if #l == 5 then
            sys_status.Advance.EARFCN = l[2]
            sys_status.Advance.RSRP = l[4]
            sys_status.Advance.RSRQ = l[5]
        end
    end

    if M.config.capabilities.nrb200 then
        sys_status.Battery = {
            Status = luardb.get(M.config.rdb_batt_status) or 'Error',
            ChargePercent = luardb.get(M.config.rdb_batt_charge_percentage) or '0',
        }
    end
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
    local ecgi = curr_data_sample.ecgi
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

    if M.config.capabilities.orientation then
        table.insert(g_rf_samples[earfcn][pci].history, {['cell_type']=cell_type,['earfcn']=earfcn, ['freq']=freq,
            ['rsrp'] = rsrp, ['rsrq'] = rsrq, ['orientation']=orientation})
    else
        table.insert(g_rf_samples[earfcn][pci].history, {['cell_type']=cell_type, ['earfcn']=earfcn, ['freq']=freq,
            ['rsrp'] = rsrp, ['rsrq'] = rsrq, ['cinr'] = cinr, ['band'] = band, ['rsrp_delta'] = rsrp_delta})
    end
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
    stats_elem.ecgi = ecgi

    -- record min and max value since run
    -- Do the rsrp first
    if not stats_elem.rsrp_min or stats_elem.rsrp_min > rsrp then
        stats_elem.rsrp_min  = rsrp
    end
    if not stats_elem.rsrp_max or stats_elem.rsrp_max < rsrp then
        stats_elem.rsrp_max = rsrp
        if M.config.capabilities.orientation then
            -- Update the best orientation as long as it meets the mininum RSRP requirement.
            if rsrp >= M.config.rf_limits.RSRP.pass then
                stats_elem.best_orientation = orientation
            end
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
        sum_rsrp = sum_rsrp + (x.rsrp or 0)
        sum_rsrq = sum_rsrq + (x.rsrq or 0)
    end

    if stats_elem.ns > 0 then
        stats_elem.rsrp_avg = sum_rsrp/stats_elem.ns
        stats_elem.rsrq_avg = sum_rsrq/stats_elem.ns
    end
--    print ("Cell id: ", stats_elem.pci, " earfcn ", stats_elem.earfcn)
end

-- Check if a cell should be displayed
function is_display_cell(ecgi)
    if g_show_all_cells then
        return true
    end
    if not ecgi then -- this is an unknown cell
        return g_show_unknown_cells
    end
    local keys = luardb.keys(M.config.rdb_wll_mcc_mnc_prefix)
    for _, key in ipairs(keys) do
        if string.sub(key, 1, #M.config.rdb_wll_mcc_mnc_prefix) == M.config.rdb_wll_mcc_mnc_prefix then
            local prefix = luardb.get(key)
            if prefix and string.sub(ecgi, 1, 6) == prefix then
                return true
            end
        end
    end
    return false
end

-- We need the following variables to check the result of manual scan is up to date.
local last_manual_cell_scan_seq = 0 -- the last sequence number on manual cell scan
local last_manual_cell_scan_ts = 0 -- the last timestamp on manual cell scan

-- this is called periodically from turbo framework. Should be set to network manager's standard update frequency (10 seconds)
-- Depending on the configuration setting, this is either done under timer control, or using trigger
-- mechanism of luardb (this function is kicked when network manager writes to the subscribed RDBs)
local function poll()

    local cell_measurements={}
    local cell_rdb_info={}

    -- build RDB information
    local rf_scan_cell_qty_rdb = luardb.get(M.config.rdb_cell_qty)
    local val = tonumber(rf_scan_cell_qty_rdb) or 0
    table.insert(cell_rdb_info,{count=val,prefix=M.config.rdb_cell_measurement_prefix})
    local rf_scan_cell_seq_rdb = luardb.get(M.config.rdb_cell_manual_seq)
    local current_ts = os.time()
    if last_manual_cell_scan_seq ~= rf_scan_cell_seq_rdb then
        last_manual_cell_scan_seq = rf_scan_cell_seq_rdb
        last_manual_cell_scan_ts = current_ts
    end
    -- The result of manual scan is only valid when it's updated within a certain time interval.
    if current_ts <= last_manual_cell_scan_ts + M.config.MIN_MANUAL_CELL_REFRESH_TIME_SEC then
        rf_scan_cell_qty_rdb = luardb.get(M.config.rdb_cell_manual_qty)
        val = tonumber(rf_scan_cell_qty_rdb) or 0
        table.insert(cell_rdb_info,{count=val,prefix=M.config.rdb_cell_manual_meas_prefix})
    end

    -- read all cell measurement RDBs
    local index
    for _,rdb_info in ipairs(cell_rdb_info) do
        for i=1, rdb_info.count do
            -- reading something like this:
            -- wwan.0.cell_measurement.0 - E,900,1,-103.1,-8.2
            -- wwan.0.manual_cell_meas.0 - E,9820,1,-79.23,-6.51
            local t = luardb.get(rdb_info.prefix..i-1)
            local l = t:explode(',')

            if #l == 5 then
                local cell_type,earfcn,pci,rsrp,rsrq = l[1],tonumber(l[2]),tonumber(l[3]),tonumber(l[4]),tonumber(l[5])
                index = string.format("%s.%s.%s",cell_type,earfcn,pci)
                if not cell_measurements[index] then
                  cell_measurements[index] = {[1]=cell_type,[2]=earfcn,[3]=pci,[4]=rsrp,[5]=rsrq}
                end
            else
                -- @TODO - do something more meaningful here
                print("Error rdb data format wrong in neighbor cell measurement, size ", #g_current_rf_readings)
                return
            end
        end
    end

    local current_orientation = nil
    if M.config.capabilities.orientation then
        current_orientation = read_orientation()
    end

    local serving_cell = get_current_serving_cell()
    -- zeroise current readings array
    g_current_rf_readings = {}

    local i = 0
    for _,l in pairs(cell_measurements) do
        i = i + 1
        g_current_rf_readings[i]= {
            cell_type=(l[1]),
            earfcn=tonumber(l[2]),
            pci=tonumber(l[3]),
            rsrp=tonumber(l[4]),
            rsrq=tonumber(l[5])
        }

        g_current_rf_readings[i].ecgi = get_ecgi_by_pci_earfcn(g_current_rf_readings[i].pci, g_current_rf_readings[i].earfcn)

        -- Limitation: only serving cell can provide RSSINR information.
        if g_current_rf_readings[i].pci == serving_cell.pci and
           g_current_rf_readings[i].earfcn == serving_cell.earfcn then
            g_current_rf_readings[i].rssinr = tonumber(luardb.get(M.config.rdb_rssinr)) or 0
        end
        if M.config.capabilities.orientation then
            g_current_rf_readings[i].orientation = current_orientation
        end
        rf_update_stats(g_current_rf_readings[i], current_ts)
    end

    -- Sort by rsrp (best one shown first)
    table.sort(g_current_rf_readings, function(a,b)
        return a.rsrp > b.rsrp
    end)

    -- completely remove stats for elements that have stale data
    cleanup_old_data(current_ts)
end

-- externally accessible callback (called from turbo io loop)
function M.run()
    poll()
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

-- We need to know which cell is the current serving cell. This can be read from RDB
function get_current_serving_cell()
    return {
        pci = tonumber(luardb.get('wwan.0.system_network_status.PCID')) or 0,
        earfcn = tonumber(luardb.get('wwan.0.system_network_status.channel')) or 0,
    }
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
--]]

-- A tiny helper to implement the above
local function check_new_entry(earfcn, pci)
    for i = 1, #g_rrc_info do
        if g_rrc_info[i].earfcn == earfcn and g_rrc_info[i].pci == pci then
            return false, i
        end
    end
    return true, 0
end

-- given mcc, mnc and eci, build ecgi 15-digit string
-- mcc is always three characters long
-- mnc is two or three characters long
-- the remaining 9 or 10 characters, padded with leading zeroes, is the cell id
local function build_ecgi(mcc, mnc, eci)
    -- probably the fastest method to do this is a lookup table
    local zero_pad =
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
    local len = string.len(mcc .. mnc .. eci)
    if len > 15 or len < 6 or string.len(mcc) ~= 3 or string.len(mnc) < 2 or string.len(mnc) > 3 or string.len(eci) < 1 then
        return ""
    end
    return (mcc .. mnc .. zero_pad[16-len] .. eci)
end

-- read rrc_info RDBs and populate g_rrc_info
local function read_rrc_rdbs()
    local rrc_info_qty = tonumber(luardb.get(M.config.rdb_rrc_info_qty)) or 0
    g_rrc_info = {}
    local j = 1
    if rrc_info_qty > 0 then
        for i = 1, rrc_info_qty do
            -- reading something like this:
            -- wwan.0.rrc_info.cell.0 310,410,904,5,27447301
            -- being mcc, mnc, earfcn, pci, eci
            local t = luardb.get(M.config.rdb_rrc_info_prefix .. i-1)
            if t then
                local l = t:explode(',')
                if #l == 5 then
                    -- ignore unless this is the first entry in the list for each (earfcn, pci)
                    local is_new, index = check_new_entry(tonumber(l[3]), tonumber(l[4]))
                    if is_new then
                        g_rrc_info[j] = {
                            mcc = l[1],
                            mnc = l[2],
                            earfcn = tonumber(l[3]),
                            pci = tonumber(l[4]),
                            eci = l[5]
                        }

                        -- this is a concatenation of 3 values
                        g_rrc_info[j].ecgi = build_ecgi(g_rrc_info[j].mcc, g_rrc_info[j].mnc, g_rrc_info[j].eci)
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
end

-- This is a poll function called periodically that simply needs to set some
-- global "environmental" variables that will affect how web pages are served. For example, if
-- the radio has not attached, the whole web interface apart from the battery is probably irrelevant.
function M.status_poll()
    -- work out if rf is available
    if luardb.get("link.profile.1.enable") ~= "1" or luardb.get("wwan.0.sim.status.status") ~= "SIM OK" then
        g_rf_unavailable = true
    else
        g_rf_unavailable = false
    end

	read_rrc_rdbs()

    -- If the web server timeout is available, the server will be shut down when
    -- there is no communication session to the web client for the specified
    -- timeout period.
    local timeout = tonumber(luardb.get("service.nrb200.web_ui.timeout"))
    if timeout and timeout > 0 then
        local current_time = turbo.util.gettimemonotonic()
        if current_time > g_last_report_time + (timeout * 1000) then
            luardb.set("service.nrb200.attached", "0")
            -- On-board Wi-Fi is used, the web server can tell Wi-Fi that
            -- it's not going to use the Wi-Fi any more.
            if M.config.capabilities.onboard_wifi then
                luardb.set("wlan.0.apps.installation_assistant.inactive", "1")
            end
        end
    end
end

-- Initialises data_collector module
-- @param config Configuration
function M.init(config)
    M.config = config
end

return M
