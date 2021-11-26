----
-- NTP Bindings
--
-- Vaugely Time:2 compilant, but largely read-only.
-- NTPServerX and Enable are mapped directly to RDB by config file persist() handlers.
-- We also implement InternetGatewayDevice.DeviceInfo.UpTime here too.
----

return {
	['**.DeviceInfo.UpTime'] = {
		get = function(node, name)
			local retVal=readIntFromFile('/proc/uptime')
			if retVal == nil then return "0" end
			return tostring(retVal)
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.Time.CurrentLocalTime'] = {
		get = function(node, name) return os.date('%s')	end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.Time.LocalTimeZone'] = {
		get = function(node, name) return '+00:00' end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.Time.LocalTimeZoneName'] = {
		get = function(node, name) return 'UTC' end,
		set = cwmpError.funcs.ReadOnly
	},
}
