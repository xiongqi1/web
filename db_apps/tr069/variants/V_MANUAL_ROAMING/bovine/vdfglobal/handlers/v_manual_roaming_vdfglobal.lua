local subROOT = conf.topRoot .. '.X_NETCOMM.RoamingSettings.'


return {
-- int:readwrite
-- Default Value: 30, Available Value: 10-2880 minutes (0=disable)
-- rdb variable: manualroam.best_network_retry_time
	[subROOT .. 'BestNetworkRetryPeriod'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("manualroam.best_network_retry_time");

			if not value then value = '30' end

			return 0, value
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end

			local num_value = tonumber(value)
			if not num_value then return CWMP.Error.InvalidArguments end

			if num_value < 0 or num_value > 2880 then return CWMP.Error.InvalidArguments end
			if num_value > 0 and num_value < 10 then return CWMP.Error.InvalidArguments end

			luardb.set("manualroam.best_network_retry_time", num_value)

			return 0
		end
	},

-- int:readwrite
-- Default Value: -105, Available Value: -105 - -95 dBm
-- rdb variable: manualroam.rssi_user_threshold
	[subROOT .. 'RSSIThreshold'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("manualroam.rssi_user_threshold");

			if not value then value = '-105' end

			return 0, value
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end

			local num_value = tonumber(value)
			if not num_value then return CWMP.Error.InvalidArguments end

			if num_value < -105 or num_value > -95 then return CWMP.Error.InvalidArguments end

			luardb.set("manualroam.rssi_user_threshold", num_value)

			return 0
		end
	},

-- int:readwrite
-- Default Value: -105, Available Value: -105 - -95 dBm
-- rdb variable: manualroam.rscp_user_threshold
	[subROOT .. 'RSCPThreshold'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("manualroam.rscp_user_threshold");

			if not value then value = '-105' end

			return 0, value
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end

			local num_value = tonumber(value)
			if not num_value then return CWMP.Error.InvalidArguments end

			if num_value < -105 or num_value > -95 then return CWMP.Error.InvalidArguments end

			luardb.set("manualroam.rscp_user_threshold", num_value)

			return 0
		end
	},

-- int:readwrite
-- Default Value: -120, Available Value: -120 - -110 dBm
-- rdb variable: manualroam.rsrp_user_threshold
	[subROOT .. 'RSRPThreshold'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("manualroam.rsrp_user_threshold");

			if not value then value = '-120' end

			return 0, value
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end

			local num_value = tonumber(value)
			if not num_value then return CWMP.Error.InvalidArguments end

			if num_value < -120 or num_value > -110 then return CWMP.Error.InvalidArguments end

			luardb.set("manualroam.rsrp_user_threshold", num_value)

			return 0
		end
	},
}
