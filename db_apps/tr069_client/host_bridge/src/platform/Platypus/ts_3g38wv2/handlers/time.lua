----
-- NTP Bindings
--
-- Vaugely Time:2 compilant, but largely read-only.
-- NTPServerX and Enable are mapped directly to RDB by config file persist() handlers.
-- We also implement InternetGatewayDevice.DeviceInfo.UpTime here too.
----
timezoneTbl = 
{
	{"-11:00", "UCT_-11"},
	{"-10:00", "UCT_-10"},
	{"-09:00", "NAS_-09"},
	{"-08:00", "PST_-08"},
	{"-07:00", "MST_-07"},
	{"-06:00", "CST_-06"},
	{"-05:00", "EST_-05"},
	{"-04:00", "AST_-04"},
	{"-03:00", "UCT_-03"},
	{"-02:00", "NOR_-02"},
	{"-01:00", "EUT_-01"},
	{"-00:00", "GMT_000"},
	{"+00:00", "GMT_000"},
	{"+01:00", "MET_001"},
	{"+02:00", "EET_002"},
	{"+03:00", "IST_003"},
	{"+04:00", "UCT_004"},
	{"+05:00", "UCT_005"},
	{"+06:00", "UCT_006"},
	{"+07:00", "UCT_007"},
	{"+08:00", "AWS_008"},
	{"+09:00", "KST_009"},
	{"+10:00", "AES_010"},
	{"+11:00", "UCT_011"},
	{"+12:00", "UCT_012"},
}


function ntp_postsession_cb()
	os.execute("ntp.sh")
	return 0
end


return {
	['**.DeviceInfo.UpTime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal=readIntFromFile('/proc/uptime')
			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil then return "0" end
			if not retVal:match('^(%d+)$') then return "0" end
			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.Time.NTPServer1'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name) return luanvramDB.get('NTPServerIP') end,
		set = function(node, name, value)
			luanvramDB.set('NTPServerIP', value)
			return 0
		end
	},
	['**.Time.CurrentLocalTime'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name) return os.date('%s')	end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.Time.LocalTimeZone'] = {
		init = function(node, name, value) return 0 end,
--		get = function(node, name) return os.date('%z') end,  -- "man strftime" for formation
		get = function(node, name) 
			local txt = luanvramDB.get('TZ')

			txt=txt:gsub("[%a+_]", "")
			withminus=txt:find("-")

			if withminus == nil then
				txt=txt:gsub("^0", "+")
			end

			return txt .. ":00"
		end,
		set = function(node, name, value)
			setVal = ""

			value = string.gsub(value:gsub("^%s+" , ""), "%s+$", "")

			for _,v in ipairs(timezoneTbl) do
				if value == v[1] then
					setVal = v[2]
					break
				end
			end

			if setVal ~= "" then
				luanvramDB.set('TZ', setVal)
				dimclient.callbacks.register('postSession', ntp_postsession_cb)
			else
				return cwmpError.InvalidArgument
			end

			return 0
		end
	},
	['**.Time.LocalTimeZoneName'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name) return os.date('%Z') end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.Time.DaylightSavingsUsed'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name) 
			local daylight = luanvramDB.get('Daylight_Savings')
			if daylight == "1" then
				return "1"
			else
				return "0"
			end
		end,
		set = function(node, name, value)
			if value == "1" or value == "0" then
				luanvramDB.set('Daylight_Savings', value)
				dimclient.callbacks.register('postSession', ntp_postsession_cb)
			else
				return cwmpError.InvalidArgument
			end

			return 0
		end
	},
}
