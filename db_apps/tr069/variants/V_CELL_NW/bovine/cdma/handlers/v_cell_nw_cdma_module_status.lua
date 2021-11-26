require('CWMP.Error')

local subROOT = conf.topRoot .. '.X_NETCOMM.WirelessModem.Status.'

------------------local function prototype------------------

------------------------------------------------------------



return {
-- RDB variable: wwan.0.module_info.cdma.activated
	[subROOT .. 'Activation'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('wwan.0.module_info.cdma.activated')
			if not result or result ~= "1" then
				return 0, "Not activated"
			end
			return 0, 'Activated'
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- RDB variable: wwan.0.system_network_status.roaming
	[subROOT .. 'RoamingStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local reg_stat = luardb.get('wwan.0.system_network_status.reg_stat')
			
			if reg_stat and reg_stat == "5" then
				return 0, 'Roaming'
			end

			return 0, 'Not roaming'
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- RDB variable: wwan.0.system_network_status.reg_stat
	[subROOT .. 'RegStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local registrationStr = {
				['0'] = "Not registered, searching stopped",
				['1'] = "Registered, home network",
				['2'] = "Not registered, searching...",
				['3'] = "Registration denied",
				['4'] = "Unknown",
				['5'] = "Registered, roaming",
				['6'] = "Registered for SMS (home network)",
				['7'] = "Rregistered for SMS (roaming)",
				['8'] = "Emergency",
				['9'] = "N/A"
			}
			local returnValue = "N/A"
			local result = luardb.get('wwan.0.system_network_status.reg_stat')
			result = tostring(result) or "9"
			returnValue = registrationStr[result] or returnValue

			return 0, returnValue
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- RDB variable: sys.sensors.io.vin.adc
	[subROOT .. 'DCInputVoltage'] = {
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

-- Possible Value: DCJack, PoE, DCJack+PoE
-- RDB variable: sys.sensors.info.powersource
	[subROOT .. 'PowerInputMode'] = {
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
}
