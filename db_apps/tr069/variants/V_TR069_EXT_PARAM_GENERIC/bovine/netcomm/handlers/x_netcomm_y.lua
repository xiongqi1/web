return {
-- [start] Vodafone requirements (Cinterion module)
	['**.X_NETCOMM.WirelessModem.Status.DLDataCounter'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local counter = luardb.get("statistics.usage_current");
			if not counter then return 0, "N/A" end

			if counter:match("wwan down") then return 0, "0" end

			local retvalue = counter:explode(',')

			if not retvalue[3] or not tonumber(retvalue[3]) then return 0, "N/A" end
			return 0, tostring(retvalue[3])
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.ULDataCounter'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local counter = luardb.get("statistics.usage_current");
			if not counter then return 0, "N/A" end

			if counter:match("wwan down") then return 0, "0" end

			local retvalue = counter:explode(',')

			if not retvalue[4] or not tonumber(retvalue[4]) then return 0, "N/A" end
			return 0, tostring(retvalue[4])
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- 	['**.X_NETCOMM.WirelessModem.Status.ModuleTemperature'] = {
-- 		init = function(node, name, value) return 0 end,
-- 		get = function(node, name)
-- 			return 0, tostring(retVal)
-- 		end,
-- 		set = function(node, name, value)
-- 			return 0
-- 		end
-- 	},
-- 	['**.X_NETCOMM.WirelessModem.Status.ModuleVoltage'] = {
-- 		init = function(node, name, value) return 0 end,
-- 		get = function(node, name)
-- 			return 0, tostring(retVal)
-- 		end,
-- 		set = function(node, name, value)
-- 			return 0
-- 		end
-- 	},
	['**.X_NETCOMM.WirelessModem.Status.RSSI'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = '';

			value = luardb.get("wwan.0.radio.information.rssi") or '';
			if not value or  not tonumber(value) then return 0, "N/A" end

			return 0, tostring(value) .. 'dBm'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.RSCP0'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local module_type = luardb.get("wwan.0.module_type") or '';
			if not module_type == '' then return 0, "N/A" end

			local value = '';

			if module_type == 'Cinterion' then
				value = luardb.get("wwan.0.system_network_status.RSCPs0") or '';
				if not tonumber(value) then return 0, "N/A" end
				value = "-" .. value .. "dB"
			else
				value = luardb.get("wwan.0.radio.information.rscp0") or '';
				if value == ''  or value == 'n/a' then return 0, "N/A" end
			end

			return 0, tostring(value)
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.RSCP1'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local module_type = luardb.get("wwan.0.module_type") or '';
			if not module_type == '' then return 0, "N/A" end

			local value = '';

			if module_type == 'Cinterion' then
				return 0, "N/A"
			else
				value = luardb.get("wwan.0.radio.information.rscp1") or '';
				if value == '' or value == 'n/a' then return 0, "N/A" end
			end

			return 0, tostring(value)
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.EcIo0'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local module_type = luardb.get("wwan.0.module_type") or '';
			if not module_type == '' then return 0, "N/A" end

			local value = '';

			if module_type == 'Cinterion' then
				value = luardb.get("wwan.0.system_network_status.ECIOs0") or '';
				if not tonumber(value) then return 0, "N/A" end
				value = "-" .. value .. "dB"
			else
				value = luardb.get("wwan.0.radio.information.ecio0") or '';
				if value == ''  or value == 'n/a' then return 0, "N/A" end
			end

			return 0, tostring(value)
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.EcIo1'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local module_type = luardb.get("wwan.0.module_type") or '';
			if not module_type == '' then return 0, "N/A" end

			local value = '';

			if module_type == 'Cinterion' then
				return 0, "N/A"
			else
				value = luardb.get("wwan.0.radio.information.ecio1") or '';
				if value == ''  or value == 'n/a' then return 0, "N/A" end
			end

			return 0, tostring(value)
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.RSRP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = '';

			value = luardb.get("wwan.0.signal.0.rsrp") or '';
			if not value or  not tonumber(value) then return 0, "N/A" end

			return 0, tostring(value) .. ' dBm'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.RSRQ'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = '';

			value = luardb.get("wwan.0.signal.rsrq") or '';
			if not value or  not tonumber(value) then return 0, "N/A" end

			return 0, tostring(value) .. ' dB'
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.LocalConnStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local rdbValue = luardb.get('hw.switch.port.0.status')

			if not rdbValue then return 0, "N/A" end

			if rdbValue:match("^ur") then
				return 0, "Up"
			else
				return 0, "Down"
			end
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.X_NETCOMM.WirelessModem.Status.RoamingStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("wwan.0.system_network_status.roaming");

			if not value then return 0, "N/A" end

			if value == 'active' then
				return 0, '1'
			else
				return 0, '0'
			end
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- string:readonly
-- 
-- sys.sensors.io.vin.adc
	['**.X_NETCOMM.WirelessModem.Status.DCInputVoltage'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("sys.sensors.io.vin.adc");

			if not value then value = 'N/A' end

			return 0, value
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- string:readonly
-- Possible Value: DCJack, PoE, DCJack+PoE
-- sys.sensors.info.powersource
	['**.X_NETCOMM.WirelessModem.Status.PowerInputMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local value = luardb.get("sys.sensors.info.powersource");

			if not value then value = 'N/A' end

			return 0, value
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- [ end ] Vodafone requirements (Cinterion module)
}
