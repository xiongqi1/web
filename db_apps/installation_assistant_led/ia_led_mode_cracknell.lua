-----------------------------------------------------------------------------------------------------------------------
---- Platform specific Installation Assistant LED behaviour
----
---- Copyright (C) 2017 NetComm Wireless limited.
----
-------------------------------------------------------------------------------------------------------------------
local script_dir = arg[0]:match('.*/')
local common = require(script_dir .. 'ia_led_common')

local function update_connected(led, states)
      common.update_led_pattern(led, states, 'connected')
end

-- Mode to show that the ODU and IA are connected and communicating
local connected_mode = {
	[common.cfg.rdb_leds[1]] = {
		states = {
			['connected'] = {
				color = common.cfg.color_map['green'],
				blink_interval = 1000
			},
		},

		update = update_connected
	},

	[common.cfg.rdb_leds[2]] = {
		states = {
			['connected'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},
		},

		update = update_connected
	},

	[common.cfg.rdb_leds[3]] = {
		states = {
			['connected'] = {
				color = common.cfg.color_map['off'],
				blink_interval = 0
			},
		},

		update = update_connected
	},
}

-- Returns operation mode
local function get_op_mode()
      return connected_mode
end

local mode = {
    rdb_watch_vars = {},
    get_op_mode = get_op_mode,
}

return mode
