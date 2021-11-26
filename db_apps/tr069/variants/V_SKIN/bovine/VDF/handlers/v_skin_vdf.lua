
local subROOT = conf.topRoot .. '.X_NETCOMM.'

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
-- Default Value: 1200, Available Value: 1~31536000 seconds
-- manualroam.reset_max_delay
	[subROOT .. 'RouterSetting.MaxBackOffTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("manualroam.reset_max_delay");

			if not value then value = '1200' end

			return 0, value
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end

			local num_value = tonumber(value)
			if not num_value then return CWMP.Error.InvalidArguments end

			if num_value < 30 or num_value > 31536000 then return CWMP.Error.InvalidArguments end

			luardb.set("manualroam.reset_max_delay", num_value)

			return 0
		end
	},

}
