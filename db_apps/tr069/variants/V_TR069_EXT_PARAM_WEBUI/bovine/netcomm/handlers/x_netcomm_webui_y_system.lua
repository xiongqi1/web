
local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.System.'

return {
--[[
-- bool: readwrite
	[subROOT .. 'Administration'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
--]]

-- uint:readwrite
-- Default Value: 0, Available Value: 0~65535 minutes (0=disable)
-- system.led_off_timer
	[subROOT .. 'Administration.LEDOperation.LEDPowerOffTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("system.led_off_timer");
			value = string.trim(value)
			if not tonumber(value) then value = '0' end

			return 0, value
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end

			local num_value = tonumber(value)
			if not num_value then return CWMP.Error.InvalidArguments end

			if num_value < 0 or num_value > 65535 then return CWMP.Error.InvalidArguments end

			luardb.set("system.led_off_timer", num_value)

			return 0
		end
	},

}
