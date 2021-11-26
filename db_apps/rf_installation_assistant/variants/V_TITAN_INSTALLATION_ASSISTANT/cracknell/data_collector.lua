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
local llog = require("luasyslog")
local util = require("wmmd.util")
local bit = require("bit")

local M = {}

pcall(function() llog.open("installation assistant", "LOG_DAEMON") end)

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

-- mode2 static data
g_mode2 = nil

-- mode1 rfq information table
g_rfq_tab = {}
g_rfq_tab_length = 0

-- nbn plmn filtering, on by default
g_plmn_filter = true

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

-- return a magnetometer calibration status if the orientation is available, nil otherwise
local function read_mag_cal_status()
    if M.config.capabilities.orientation then
        mag_cal_status = luardb.get(M.config.rdb_orientation_prefix .. 'mag.cal_status')
        if mag_cal_status then
            return {mag_cal_status = mag_cal_status}
        end
    end
    return nil
end


-- Update neighbour cell data
function update_neigh_cell_data()
    local data = {}
    local neigh_data = luardb.get(M.config.rdb_ncdp_data) or ""

    -- Data format here is:
    -- 01/11/2018 00:02:22,2,42370,27447312,-92.6,-92.1,-92.4,1,41690,-80.4,-79.0,-79.2
    -- For each cell 6 CSV fields: PCI, EARFCN, ECGI, RSRP MAX, RSRP MIN, RSRP AVG
    -- and then this repeats as many times as there are cells
    local neigh_data_no_date = string.match(neigh_data,"%s*,(.*)")
    if neigh_data_no_date ~= nil and neigh_data_no_date ~= '' then
        local list = neigh_data_no_date:split(',')

        if #list < 6 then
            return data
        end

        for i = 0, #list/7 do
            data[i+1] = {
                pci = tonumber(list[1+6*i]),
                arfcn = tonumber(list[2+6*i]),
                rsrp =  tonumber(list[6+6*i]) -- average RSRP
            }
        end

        -- The requirement is to sort neighbour cells in the order of RSRP
        table.sort(data,  function(a,b)  return a.rsrp > b.rsrp end);
    end

    local max_len = 4
    if #data < max_len then
        max_len = #data
    end
    g_mode2.ncell = {}
    for i = 1,max_len do
        data[i].rsrp = string.format("%.1f", data[i].rsrp)
        table.insert(g_mode2.ncell, data[i])
    end
end


-- Update current alarms
local function update_alarms(num_lines)
    local alarm_list = luardb.get(M.config.rdb_alarms)
    local i = 0, j
    g_mode2.alarms = {}
    if alarm_list ~= nil and alarm_list ~= "" then
        for _, alarm in ipairs(alarm_list:split(',')) do
          -- skip alarms still in RDB but already cleared.
          local alarm_cleared = luardb.get("alarms." .. tonumber(alarm) .. ".cleared")
          if alarm_cleared == nil or alarm_cleared == "" then
              local alarm_description = luardb.get("alarms." .. tonumber(alarm) .. ".message")
              if alarm_description then
                  -- 58 characters with letter spacing of 1.3px
                  -- 45 chars with letter spacing of 3.76 (default for <td> in mode2 page in css)
                  table.insert(g_mode2.alarms, string.sub((tostring(i+1) .. ' ' .. alarm_description), 1, 58))
              end
              i = i + 1
              if i > num_lines then
                  break
              end
          end
        end
    end
end


-- Update log entries
function update_log_entries(num_lines)
    local cmd ="tail /var/log/messages -n "..tostring(num_lines)
    local f = assert (io.popen (cmd))
    local s = assert(f:read("*a"))
    f:close()
    g_mode2.log = {}
    for s1 in s:gmatch("[^\r\n]+") do
        -- 103 chars with letter spacing of 1.3px
        -- 80 chars with letter spacing of 3.76 (default for <td> in mode2 page in css)
        table.insert(g_mode2.log, s1:sub(36, 138) or "") -- remove date/time stamp and device name
    end
end

-- Support for displaying RF statistics
-- bucket boundaries, same dimensions as buckets themselves
local bucket_consts = {
    rsrp = {-96, -99, -10000},
    cinr = {23, 18, 13, 8, -10000} -- SINR for V3
}


-- A helper to fill the bucket from samples
local function bucket_fill(samples, bucket, bucket_consts)
    -- clear buckets
    for i = 1, #bucket do
        bucket[i] = 0
    end

    -- fill buckets with as many samples as fall in bucket limits
    local sample_count = 0
    for _, val in ipairs(samples) do
        if tonumber(val) ~= nil then -- take care of potentially empty comma separated fields
            sample_count = sample_count + 1
            for i = 1, #bucket_consts do
                if tonumber(val) > bucket_consts[i] then
                    bucket[i] = bucket[i] + 1
                    break -- found the bucket
                end
            end
        end
    end

    -- now scale buckets to fractions adding to 100%
    if sample_count == 0 then return end
    bucket[1] = 100 -- to avoid any rounding errors and make sure buckets always add to 100%
    for i = 2, #bucket do
        if sample_count > 0 then
            bucket[i] = math.floor(((bucket[i]*100)/sample_count)+0.5)
            bucket[1] = bucket[1] - bucket[i]
        end
    end
end


-- The end result is to fill the bucket with samples from statsmonitoring.out
function update_sig_stats() -- global

    local cmd ="grep -e radio.0.rsrp -e radio.0.cinr /opt/statsmonitoring.out 2>/dev/null"

    local f = assert(io.popen (cmd))
    local s = assert(f:read("*a"))
    f:close()

    local stats_data = {}

    --[[
        Iterate line by line over output such as
        radio.0.cinr0:comma separated samples
        radio.0.cinr1:comma separated samples
        radio.0.rsrp1:comma separated samples
        radio.0.rsrp0:comma separated samples

        Build a table indexed by name of parameter
    --]]

    stats_data['cinr'] = {} -- SINR for V3
    stats_data['cinr'].raw_data = ""
    stats_data['cinr'].processed_data = {}
    stats_data['rsrp'] = {}
    stats_data['rsrp'].raw_data = ""
    stats_data['rsrp'].processed_data = {}
    for s1 in string.gmatch(s, "[^\r\n]+") do
        local param_name, v = string.match(s1, "radio.0.(.*)[01]:(.*)")
        assert (param_name == 'cinr' or param_name == 'rsrp')
        -- merge param0 and param1 into one raw string.
        stats_data[param_name].raw_data = stats_data[param_name].raw_data .. v
        stats_data[param_name].processed_data = stats_data[param_name].raw_data:split(',')
    end

    for k,v in pairs(stats_data) do
        bucket_fill(v['processed_data'], g_mode2[k], bucket_consts[k])
    end
end


local function update_unid_status()
    g_mode2.unid1_status = luardb.get(M.config.rdb_unid1_status)
    g_mode2.unid2_status = luardb.get(M.config.rdb_unid2_status)
    g_mode2.unid3_status = luardb.get(M.config.rdb_unid3_status)
    g_mode2.unid4_status = luardb.get(M.config.rdb_unid4_status)
end


local function update_avc_status()
    local avc_index = luardb.get(M.config.rdb_avc_index)

    g_mode2.avc = {}
    if avc_index then
        for i_,id in ipairs(avc_index:split(',')) do
            table.insert(g_mode2.avc, luardb.get('avc.' .. id .. '.status'))
        end
    end
end


local function update_tx_power()
    -- See 3GPP TS 36.101, 6.2.2 for maximum transmit power of 23 dBm (+/- 2)
    -- See 3GPP TS 36.101, 6.3.2 for minimum transmit power of -40 dBm
    -- CMW allows wider range, setting from -50 to +33.
    local tx_pwr = tonumber(luardb.get(M.config.rdb_serving_cell_tx_power)) or nil
    if tx_pwr then
        if tx_pwr > 33 then
            tx_pwr = 33
        elseif tx_pwr < -50 then
            tx_pwr = -50
        end
        g_mode2.p_tx_power = string.format("%.1f", tx_pwr)
    else
        g_mode2.p_tx_power = ""
    end
end


local function update_cell_info()
    local temp = luardb.get(M.config.rdb_serving_cell_pci) or ""
    g_mode2.p_pci = temp:split(',')[2] or ""
    g_mode2.p_earfcn = luardb.get(M.config.rdb_serving_cell_earfcn) or ""
    g_mode2.p_band = luardb.get(M.config.rdb_serving_cell_band) or ""
    g_mode2.state = luardb.get(M.config.rdb_state) or ""
end


-- return mode2 info if available
local function read_mode2_info()
    if not g_mode2 then
        g_mode2 = {}
        g_mode2.sw_ver = luardb.get(M.config.rdb_sw_version)
        g_mode2.hw_ver = luardb.get(M.config.rdb_hw_version)
        g_mode2.fw_ver = luardb.get(M.config.rdb_fw_version)
        g_mode2.serial = luardb.get(M.config.rdb_serial_number)
        g_mode2.tx_pwr = ""
        g_mode2.log_entries = M.config.rdb_max_log_entries
        g_mode2.p_rsrp0 = ""
        g_mode2.p_rsrp1 = ""
        g_mode2.p_sinr = ""
        g_mode2.avc_rx = ""
        g_mode2.avc_tx = ""

        -- 3 buckets for RSRP
        g_mode2.rsrp = {0, 0, 0}
        -- 5 buckets for CINR (SINR for V3)
        g_mode2.cinr = {0, 0, 0, 0, 0}

        g_mode2.pci = luardb.get(M.config.rdb_pci):split(',')[1] or  ''
        update_neigh_cell_data()
        update_cell_info()
        update_unid_status()
        update_avc_status()
        update_alarms(M.config.rdb_max_alarm_entries)
        update_sig_stats()
        update_tx_power()

        local file = io.popen("cat /proc/uptime")
        g_mode2.uptime = tonumber(string.format("%.0d", file:read('*all'):split(' ')[1]))
        file:close()
    end

    g_mode2.date_time = os.date("%Y-%m-%dT%H:%M:%S")

    -- we get called every second, so just increment the counter each time we are here
    g_mode2.uptime = g_mode2.uptime + 1

    g_mode2.attached = luardb.get(M.config.rdb_attached) or "0"
    if g_mode2.attached == "1" then
        g_mode2.p_rsrp = luardb.get(M.config.rdb_serving_cell_rsrp)
        g_mode2.p_rsrp0 = luardb.get(M.config.rdb_serving_cell_rsrp0)
        g_mode2.p_rsrp1 = luardb.get(M.config.rdb_serving_cell_rsrp1)
        g_mode2.p_sinr = luardb.get(M.config.rdb_serving_cell_sinr)

        if bit.band(g_mode2.uptime, 3) == 0 then
            update_cell_info()
        end

        update_tx_power()

        g_mode2.avc_rx = 0
        g_mode2.avc_tx = 0
        local file = io.popen("awk 'NR>2{print $1,$2,$10}' /proc/net/dev | grep gre | tr '\n' ',' | sed 's/://g'")
        local gre_if = string.format("%s", file:read('*all'))
        file:close()
        for i_,interface in ipairs(gre_if:split(',')) do
            if interface ~= '' then
                items = interface:split(' ')
                g_mode2.avc_rx = g_mode2.avc_rx + tonumber(items[2])
                g_mode2.avc_tx = g_mode2.avc_tx + tonumber(items[3])
            end
        end
    end

    if bit.band(g_mode2.uptime, 5) == 0 then
        -- mode2.pci is used for display, first one in the list
        g_mode2.pci = luardb.get(M.config.rdb_pci):split(',')[1] or  ''

        if bit.band(g_mode2.uptime, 10) == 0 then
            update_unid_status()
            update_avc_status()

            if bit.band(g_mode2.uptime, 30) == 0 then
                update_neigh_cell_data()

                if bit.band(g_mode2.uptime, 60) == 0 then
                    update_sig_stats()
                end
            end
        end
    end

    g_mode2.status_led = luardb.get(M.config.rdb_idu_led_status)
    g_mode2.cable_fault = luardb.get(M.config.rdb_idu_cable_fault)
    g_mode2.comms_failed = luardb.get(M.config.rdb_idu_comms_failed)
    g_mode2.odu = luardb.get(M.config.rdb_idu_led_odu)
    g_mode2.sig_low = luardb.get(M.config.rdb_idu_led_signal_low)
    g_mode2.sig_med = luardb.get(M.config.rdb_idu_led_signal_med)
    g_mode2.sig_high = luardb.get(M.config.rdb_idu_led_signal_high)

    if bit.band(g_mode2.uptime, 3) == 0 then
      -- current alarms
      update_alarms(M.config.rdb_max_alarm_entries)
    end

    -- log entries, last n lines
    update_log_entries(g_mode2.log_entries)

    return g_mode2
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
local function rf_update_samples_and_stats(curr_data_sample, most_recent_sample_ts)

    local earfcn = curr_data_sample.earfcn
    local pci = curr_data_sample.pci
    local cell_type = curr_data_sample.cell_type
    local freq = curr_data_sample.freq
    local rsrp = tonumber(curr_data_sample.rsrp)
    local rsrq = tonumber(curr_data_sample.rsrq)
    local band = tonumber(curr_data_sample.band)
    local rsrp_delta = tonumber(curr_data_sample.rsrp_delta)
    local ecgi = curr_data_sample.ecgi

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
        ['rsrp'] = rsrp, ['rsrq'] = rsrq, ['band'] = band, ['rsrp_delta'] = rsrp_delta, ['ecgi'] = ecgi})
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
    stats_elem.band = band
    stats_elem.ecgi = ecgi

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

    -- ns short for num_samples, keeping json short due to bandwidth limitations of nrb200
    -- record the number of samples and the time stamp of the most recent one
    stats_elem.ns = #g_rf_samples[earfcn][pci].history
    stats_elem.most_recent_sample_ts = most_recent_sample_ts

    -- calculate averages, and a median for RSRP as per NBN V1/V2
    local sum_rsrp = 0
    local sum_rsrq = 0
    local sum_rsrp_delta = 0
    local t = {} -- local table for median calculation
    for _, x in pairs(g_rf_samples[earfcn][pci].history) do
        sum_rsrp = sum_rsrp + (x.rsrp or 0)
        sum_rsrq = sum_rsrq + (x.rsrq or 0)
        sum_rsrp_delta = sum_rsrp_delta + (x.rsrp_delta or 0)
        table.insert(t, x.rsrp)
    end

    table.sort(t)
    local mid_point = math.floor(#t/2+0.5)

    if stats_elem.ns > 0 then
        stats_elem.rsrp_avg = sum_rsrp/stats_elem.ns
        stats_elem.rsrq_avg = sum_rsrq/stats_elem.ns
        stats_elem.rsrp_delta_avg = sum_rsrp_delta/stats_elem.ns
        stats_elem.rsrp_median = t[mid_point]
    end

    if stats_elem.ns >= M.config.RF_NUMBER_OF_SAMPLES_FOR_PASS then
        if stats_elem.rsrp_avg >= M.config.rf_limits.RSRP.pass and
            stats_elem.rsrp_delta_avg >= M.config.rf_limits.RSRP_Delta.pass
        then
            stats_elem.res = "PASS"
        else
            stats_elem.res = "FAIL"
        end
    else
        stats_elem.res = "Insufficient data"
    end
--    print ("Cell id: ", stats_elem.pci, " earfcn ", stats_elem.earfcn)
end

-- According to input EARFCN, calculate the band, and the (Downlink) freqency.
-- Returns 2 arguments, band and frequency in MHz.
-- (Note that frequency is unlike the Titan version which returns a text string)
function earfcn_map(dl_earfcn)
    -- Only band 40 and band 42 are supported.
    local conversion = {
        [40] = {dl_low=2300, dl_offset=38650, dl_earfcn={low=38650, high=39649}},
        [42] = {dl_low=3400, dl_offset=41590, dl_earfcn={low=41590, high=43589}},
    }

    local freq = 0
    for band, param in pairs(conversion) do
        if dl_earfcn >= param.dl_earfcn.low and dl_earfcn <= param.dl_earfcn.high then
            freq = param.dl_low + 0.1*(dl_earfcn - param.dl_offset)
            return band, freq
        end
    end
    return 0, 0
end

-- Unit test support. There is no architecture for this in the system. Unit test has to call via wrapper
function M.earfcn_map_wrapper(dl_earfcn)
    return earfcn_map(dl_earfcn)
end

-- val is csv bundle of <pci-earfcn:ecgi:mcc-mnc>, <...>
local function update_rfq_tab(val)
    -- only update if thre are new entries
    if g_rfq_tab_length ~= #val then
      g_rfq_tab_length = #val
      local list = val:split(',')
      for _, entry in ipairs(list) do
          if entry and entry ~= "" then
              local item = entry:split(':')
              if not g_rfq_tab[item[1]] then
                  local temp = {}
                  temp.ecgi = item[2] or ""
                  temp.plmn = item[3] or "0"
                  g_rfq_tab[item[1]] = temp
              end
          end
      end
    end
end

local function update_plmn_filtering(val)
    if val == "0" then
        g_plmn_filter = false
    else
        g_plmn_filter = true
    end
end

-- Look for the strongest interfering cell of the same frequency.
-- Return delta (RSRP of the current sample minus strongest interfering cell), as well
-- as the PCI of this second cell.
-- Not optimized.
local function calc_rsrp_delta(curr_data_sample)
    delta = M.config.rf_limits.RSRP_Delta.max -- maximum possible delta value
    pci_delta = 0 -- interfering cell PCI
    for i = 1, #g_current_rf_readings do
        elem = g_current_rf_readings[i]
        -- interfering cells are on the same frequency only
        if elem.earfcn == curr_data_sample.earfcn then
            -- check that we are not comparing against itself
            if elem.pci ~= curr_data_sample.pci then
                if delta > (curr_data_sample.rsrp - elem.rsrp) then
                    delta = curr_data_sample.rsrp - elem.rsrp
                    pci_delta = elem.pci -- PCI of the strongest interfering cell
                end
            end
        end
    end
    return delta, pci_delta
end

-- this is called periodically from turbo framework. Should be set to network manager's standard update frequency (10 seconds)
-- Depending on the configuration setting, this is either done under timer control, or using trigger
-- mechanism of luardb (this function is kicked when network manager writes to the subscribed RDBs)
local function poll()

    -- work out cell quantity in RDB
    local rf_scan_cell_qty_rdb = luardb.get(M.config.rdb_cell_manual_qty)
    local val = tonumber(rf_scan_cell_qty_rdb) or 0

    local current_ts = os.time()

    update_plmn_filtering(luardb.get(M.config.rdb_plmn_filtering))
    update_rfq_tab(luardb.get(M.config.rdb_rfq_tab) or "")
    local filter_plmn = luardb.get(M.config.filter_plmn) or ""

    -- zeroise current readings array
    g_current_rf_readings={}

    local j = 0
    for i=1, val do

        -- reading something like this:
        -- wwan.0.manual_cell_meas.0 - E,900,1,-103 (when RSRQ is not available)
        -- wwan.0.manual_cell_meas.0 - E,900,1,-103.1,-6.5 (when RSRQ is available)
        -- The value represents Network Type,EARFCN,PCI,RSRP,RSRQ as a comma separated string.
        local t = luardb.get(M.config.rdb_cell_manual_meas_prefix..i-1)
        if t then
            local l = t:explode(',')
            if #l == 4 or #l == 5 then
                local index = l[3].."-"..l[2]
                if not g_plmn_filter or not g_rfq_tab[index] or
                   g_rfq_tab[index].plmn == filter_plmn then
                    j = j + 1
                    g_current_rf_readings[j]= {
                        cell_type=(l[1]),
                        earfcn=tonumber(l[2]),
                        pci=tonumber(l[3]),
                        rsrp=tonumber(l[4]),
                        rsrq=(#l==5 and tonumber(l[5]) or 0),
                        ecgi=(g_rfq_tab[index] and g_rfq_tab[index].ecgi) or "----"
                    }
                    g_current_rf_readings[j].band, g_current_rf_readings[j].freq=earfcn_map(g_current_rf_readings[j].earfcn)
                end
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
    end

    -- Calculate the delta to the strongest interfering cell on the same frequency.
    -- At the moment, we rely only on one current reading for each cell.
    -- However, we may need to analyze previous samples (@TODO requirement)
    -- in which case we will need to look at samples arrays in the calc_rsrp_delta
    -- Update historical samples and stats only after rsrp_delta is calculated.
    for i=1, j do
        g_current_rf_readings[i].rsrp_delta, g_current_rf_readings[i].pci_delta = calc_rsrp_delta(g_current_rf_readings[i])
        rf_update_samples_and_stats(g_current_rf_readings[i], current_ts)
    end

    -- completely remove stats for elements that have stale data
    cleanup_old_data(current_ts)
end

-- externally accessible callback (called from turbo io loop)
function M.run()
    poll()
end

-- get battery information
function M.get_battery_reading()
    -- return real-time battery information
    return read_battery()
end

-- get magnetometer calibration status
function M.get_mag_cal_status()
    -- return real-time magnetometer calibration status
    return read_mag_cal_status() or {mag_cal_status="Not Calibrated"}
end

-- get mode2 info
function M.get_mode2_info()
    -- return real-time mode2 information
    return read_mode2_info() or {}
end

local measurement_seq = 0 -- Sequence number at the last measurement update
local measurement_time_ms = 0 -- Elapsed time since the last measurement update

-- This is a poll function called periodically to do the following tasks
-- 1) Checks whether the measurements are updated within the timeframe.
function M.status_poll()
    local new_seq = luardb.get(M.config.rdb_cell_manual_meas_prefix..'seq')

    if measurement_seq == new_seq then
        measurement_wait_time_ms = measurement_wait_time_ms + M.config.STATUS_POLL_INTERVAL_MS
        if measurement_wait_time_ms > M.config.MAX_MEASUREMENT_INTERVAL_MS then
             g_rf_unavailable = true
        end
    else
        measurement_seq = new_seq
        measurement_wait_time_ms = 0
        g_rf_unavailable = false
    end
end

-- Initialises data_collector module
-- @param config Configuration
function M.init(config)
    M.config = config
end

return M
