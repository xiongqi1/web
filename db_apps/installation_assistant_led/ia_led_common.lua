-----------------------------------------------------------------------------------------------------------------------
---- Installation Assistant LED controller common code
----
---- Copyright (C) 2017 NetComm Wireless limited.
----
-------------------------------------------------------------------------------------------------------------------
local cfg = {
	rdb_leds = {
		'service.nrb200.led.0',
		'service.nrb200.led.1',
		'service.nrb200.led.2',
	},

	color_map = {
		off = 0,
		red = 1,
		green = 2,
		yellow = 3,
	},
}

-- Context variable to keep information shared between functions in the daemon.
local context = { }

-- Updates rdb variable if the value has changed
local function update_value(key, value)
	if context.led_patterns[key] ~= value then
		luardb.set(key, value)
		context.led_patterns[key] = value
	end
end

-- Updates display pattern for the led.
function update_led_pattern(led, state_table, state)
	if state_table[state].color then
		update_value(led .. ".color", state_table[state].color)
	end

	if state_table[state].blink_interval then
		update_value(led .. ".blink_interval", state_table[state].blink_interval)
	end
end

-- Intialises variables
local function init()
	-- Keeps track of display patterns for all LEDs to minimise rdb updates
	if not context.led_patterns then
		context.led_patterns = {}
	end

	for _, v in pairs(cfg.rdb_leds) do
		if not context.led_patterns[v] then
			context.led_patterns[v] = {}
		end

		context.led_patterns[v].color = luardb.get(v .. ".color") or cfg.color_map['off']
		context.led_patterns[v].blink_interval = luardb.get(v .. ".blink_interval") or 0
	end
end

init()

local common = {
    cfg = cfg,
    update_led_pattern = update_led_pattern,
}

return common