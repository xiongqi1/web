-----------------------------------------------------------------------------------------------------------------------
---- Platform specific Installation Assistant LED behaviour
----
---- Copyright (C) 2017 NetComm Wireless limited.
----
-------------------------------------------------------------------------------------------------------------------
local script_dir = arg[0]:match('.*/')
local common = require(script_dir .. 'ia_led_common')

local cfg = {
	rdb_cell_qty = 'wwan.0.cell_measurement.qty',
	rdb_cell_measurement_prefix = 'wwan.0.cell_measurement.',

	rdb_manual_cell_qty = 'wwan.0.manual_cell_meas.qty',
	rdb_manual_cell_seq = 'wwan.0.manual_cell_meas.seq',
	rdb_manual_cell_measurement_prefix = 'wwan.0.manual_cell_meas.',

	rdb_lte = 'wwan.0.system_network_status.reg_stat',

	rdb_apns = { },

	ether_port = 0,

	-- FR-3125 Requirement : Cell Scanning Mode
	strong_signal_rsrp = -75,
	low_signal_rsrp = -98,

	max_profile_num = 8,

	-- Manual cell measurements are no longer valid if they are not updated
	-- within the following number of seconds.
	min_manual_cell_refresh_time_sec = 10,
}

-- Context variable to keep information shared between functions in the daemon.
local context = { }

-- Finds a rdb var for the APN and adds it to the table.
local function add_apn_rdb_var(apn_rdb_vars, apn_name)
	local name = string.upper(apn_name)
	local profile_id = 1 -- Use profile 1 by default if the specified APN is not available

	-- Find the specified APN among profiles
	for i = 1, cfg.max_profile_num do
		local profile_rdb = "link.profile." .. i
		pdp_function = luardb.get(profile_rdb .. ".pdp_function")
		if pdp_function and pdp_function:upper() == name then
			profile_id = i
			break
		end
	end
	apn_rdb_vars[apn_name] = "link.profile." .. profile_id .. ".connect_progress"
end

-- Gets LTE registration status
local function get_lte_status()
	local lte_status = luardb.get(cfg.rdb_lte)

	if lte_status == '1' then
		return 'registered'
	elseif lte_status == '2' then
		return 'registering'
	elseif lte_status == '5' then
		return 'registered'
	elseif lte_status == '6' then
		return 'registered'
	elseif lte_status == '7' then
		return 'registered'
	elseif lte_status == '8' then
		return 'registered'
	elseif lte_status == '9' then
		return 'registered'
	elseif lte_status == '10' then
		return 'registered'
	else
		return 'deregistered'
	end
end

-- Gets APN status for the specified APN
local function get_apn_status(apn_name)
	if not cfg.rdb_apns[apn_name] then
		return 'disconnected'
	end

	if luardb.get(cfg.rdb_apns[apn_name]) == 'established' then
		return 'established'
	elseif luardb.get(cfg.rdb_apns[apn_name]) == 'establishing' then
		return 'establishing'
	else
		return 'disconnected'
	end
end

-- Returns true if the specfied link is up, Return false otherwise.
local function is_ethernet_up()
	local ether_status = "hw.switch.port." .. cfg.ether_port .. ".status"

	if string.sub(luardb.get(ether_status), 1, 1) == "u" then
		return true
	else
		return false
	end
end

-- Get ethernet status
local function get_ethernet_status()
	if is_ethernet_up() then
		file = io.open("/sys/class/net/eth" .. cfg.ether_port .. "/statistics/rx_packets")
		if not file then
			return 'link_up'
		end

		num_packets = tonumber(file:read())
		if context.num_rx_packets < 0 then
			-- Just record it if it is first time to get the status
			context.num_rx_packets = num_packets
		elseif num_packets > context.num_rx_packets then
			context.num_rx_packets = num_packets
			--print("updates rx_packets: " .. rx_packets)
			return 'link_active'
		end

		io.close(file)
		return 'link_up'
	end

	return 'link_down'
end

-- Finds the strongest signal strength and returns the corresponding string to the strength.
local function get_signal_strength()
	local max_rsrp = cfg.low_signal_rsrp - 1
	local num_signals = tonumber(luardb.get(cfg.rdb_cell_qty)) or 0
	local current_seq = tonumber(luardb.get(cfg.rdb_manual_cell_seq)) or 0
	local current_ts = os.time()

	if context.last_manual_cell_scan_seq ~= current_seq then
		context.last_manual_cell_scan_seq = current_seq
		context.last_manual_cell_scan_ts = current_ts
	end

	local num_manual_signals = 0
	if current_ts <= context.last_manual_cell_scan_ts + cfg.min_manual_cell_refresh_time_sec then
		num_manual_signals = tonumber(luardb.get(cfg.rdb_manual_cell_qty)) or 0
	end

	if num_signals + num_manual_signals < 1 then
		return 'no_signal'
	end

	-- check automatic cell scan
	for i = 1, num_signals do
		-- reading something like this:
		-- wwan.0.cell_measurement.0    -   E,900,1,-103.1,-8.2
		local t = luardb.get(cfg.rdb_cell_measurement_prefix .. (i - 1))
		if t then
			local l = t:explode(',')
			if #l then
				local rsrp = tonumber(l[4])
				if (rsrp > max_rsrp) then
					max_rsrp = rsrp
				end
			end
		end
	end

	-- check manual cell scan
	for i = 1, num_manual_signals do
		-- reading something like this:
		-- wwan.0.cell_measurement.0    -   E,900,1,-103.1,-8.2
		local t = luardb.get(cfg.rdb_manual_cell_measurement_prefix .. (i - 1))
		if t then
			local l = t:explode(',')
				if #l then
				local rsrp = tonumber(l[4])
				if (rsrp > max_rsrp) then
					max_rsrp = rsrp
				end
			end
		end
	end

	if (max_rsrp >= cfg.strong_signal_rsrp) then
		return 'strong_signal'
	elseif ((max_rsrp < cfg.strong_signal_rsrp) and (max_rsrp >= cfg.low_signal_rsrp)) then
		return 'medium_signal'
	else
		return 'low_signal'
	end
end

-- Updates the status of power
-- power is always on as long as this process runs
local function update_power_status(led, states)
	update_led_pattern(led, states, 'power_on')
end

-- Updates the status of ethernet
local function update_ethernet_status(led, states)
	local ether_state;

	ether_state = get_ethernet_status()
	update_led_pattern(led, states, ether_state)
end

-- Updates the status of LTE registration
local function update_lte_status(led, states)
	local lte_state;

	lte_state = "lte_" .. get_lte_status() .. "_data_" .. get_apn_status('data');
	update_led_pattern(led, states, lte_state)
end

-- Updates the status of signal strength
local function update_signal_strength(led, states)
	local signal_state;

	signal_state = get_signal_strength()
	update_led_pattern(led, states, signal_state)
end

-- Normal operation mode to reflect Titan internal states on LEDs
local normal_operation_mode = {
	[common.cfg.rdb_leds[1]] = {
		states = {
			['power_off'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},

			['power_on'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},
		},

		update = update_power_status
	},

	[common.cfg.rdb_leds[2]] = {
		states = {
			['lte_registered_data_established'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},

			['lte_registered_data_establishing'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 500
			},

			['lte_registered_data_disconnected'] = {
				color = common.cfg.color_map['yellow'],
				blink_interval = 0
			},

			['lte_registering_data_established'] = {
				color = common.cfg.color_map['yellow'],
				blink_interval = 500
			},

			['lte_registering_data_establishing'] = {
				color = common.cfg.color_map['yellow'],
				blink_interval = 500
			},

			['lte_registering_data_disconnected'] = {
				color = common.cfg.color_map['yellow'],
				blink_interval = 500
			},

			['lte_deregistered_data_established'] = {
				color = common.cfg.color_map['red'],
				blink_interval = 0
			},

			['lte_deregistered_data_establishing'] = {
				color = common.cfg.color_map['red'],
				blink_interval = 0
			},

			['lte_deregistered_data_disconnected'] = {
				color = common.cfg.color_map['red'],
				blink_interval = 0
			},
		},

		update = update_lte_status
	},

	[common.cfg.rdb_leds[3]] = {
		states = {
			['link_down'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},

			['link_up'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},

			['link_active'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 100
			},
		},

		update = update_ethernet_status
	}
}

-- Cell scanning mode to show signal strength on LEDs
local cell_scanning_mode = {
	[common.cfg.rdb_leds[1]] = {
		states = {
			['strong_signal'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},

			['medium_signal'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},

			['low_signal'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},

			['no_signal'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},
		},

		update = update_signal_strength
	},

	[common.cfg.rdb_leds[2]] = {
		states = {
			['strong_signal'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},

			['medium_signal'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},

			['low_signal'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},

			['no_signal'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},
		},

		update = update_signal_strength
	},

	[common.cfg.rdb_leds[3]] = {
		states = {
			['strong_signal'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},

			['medium_signal'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},

			['low_signal'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 0
			},

			['no_signal'] = {
				color = common.cfg.color_map['red'],
				blink_interval = 250
			},
		},

		update = update_signal_strength
	}
}

-- Returns operation mode
-- Normal mode if the POE is available, Cell scan mode otherwise
local function get_op_mode()
	if is_ethernet_up() then
		return normal_operation_mode
	else
		return cell_scanning_mode
	end
end

-- Intialises variables
local function init()
	-- Sets rx_packets a negative value to indicate it hasn't been updated
	context.num_rx_packets = -1

	context.last_manual_cell_scan_seq = 0
	context.last_manual_cell_scan_ts = 0

	-- Adds rdb variables used to check the status of LTE registration
	add_apn_rdb_var(cfg.rdb_apns, 'data')
end

-- Get RDB variables that need to be watched
local function get_rdb_watch_vars()
    vars = {}
    table.insert(vars, cfg.rdb_lte)
    for k, v in pairs(cfg.rdb_apns) do
	    table.insert(vars, v)
    end
    return vars
end

init()

local mode = {
    rdb_watch_vars = get_rdb_watch_vars(),
    get_op_mode = get_op_mode,
}

return mode


