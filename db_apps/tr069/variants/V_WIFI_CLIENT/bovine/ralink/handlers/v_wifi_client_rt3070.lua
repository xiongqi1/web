require('CWMP.Error')

local subROOT = conf.topRoot .. '.X_NETCOMM.WiFiClientProfile.'

------------------local function prototype------------------
local convertInternalBoolean
local isChanged
local convertInternalInteger
------------------------------------------------------------

convertInternalBoolean = function (val)
	local inputStr = tostring(val)

	if not inputStr then return nil end

	inputStr = (string.gsub(inputStr, "^%s*(.-)%s*$", "%1"))

	if inputStr == '1' or inputStr:lower() == 'true' then
		return '1'
	elseif inputStr == '0' or inputStr:lower() == 'false' then
		return '0'
	end

	return nil
end

-- usage: convertInternalInteger{input=number, minimum=0, maximum=50}
-- If number doesn't have a specific range, just set "minimum" or "maximum" to nil or omit this argument.
-- success: return interger type value
-- false: return nil
convertInternalInteger = function (arg)
	local convertedInt = tonumber(arg.input)

	if type(convertedInt) == 'number'
	then
		local minimum = arg.minimum or -2147483648
		local maximum = arg.maximum or 2147483647

		minimum = tonumber(minimum)
		maximum = tonumber(maximum)

		if minimum == nil or maximum == nil then return nil end
		if convertedInt < minimum or convertedInt > maximum then return nil end

		return convertedInt

	else
		return nil
	end
end

isChanged = function (name, newVal)
	local prevVal = luardb.get(name)

	if not prevVal then return true end

	if tostring(prevVal) == tostring(newVal) then return false end

	return true
end

return {
	[subROOT .. 'Bssid'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			result=luardb.get('wlan.0.sta.bssid')
			return 0, result

		end,
		set = function(node, name, value)
			local setVal=value:match("^%s*(%w+:%w+:%w+:%w+:%w+:%w+)%s*$")
			if not setVal or setVal == '' then return CWMP.Error.InvalidParameterValue end
			if not isChanged('wlan.0.sta.bssid', setVal) then return 0 end
			luardb.set('wlan.0.sta.bssid', setVal)
			return 0
		end
	},

	[subROOT .. 'NetworkAuthentication'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			result=luardb.get('wlan.0.sta.network_auth')
			if not result or result == '' then return 0, '' end
			return 0, result

		end,
		set = function(node, name, value)
			local AvailableAuthList={'EPAUTO','SHARED','WPA','WPAPSK','WPA2','WPA2PSK','WPA1PSKWPA2PSK','WPA1WPA2'}
			local setVal=value:upper()
			if not setVal or not table.contains(AvailableAuthList, setVal) then return CWMP.Error.InvalidParameterValue end
			if not isChanged('wlan.0.sta.network_auth', setVal) then return 0 end
			luardb.set('wlan.0.sta.network_auth', setVal)
			return 0
		end
	},

	[subROOT .. 'OpenAndSharedAuth.NetworkKey1'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			result=luardb.get('wlan.0.sta.network_key1')
			if not result or result == '' then return 0, '' end
			return 0, result
		end,

		set = function(node, name, value)
			local strlen= string.len(value)
			if strlen ~= 10 and strlen ~= 26 then return CWMP.Error.InvalidParameterValue end
			local setVal = value
			if not isChanged('wlan.0.sta.network_key1', setVal) then return 0 end
			luardb.set('wlan.0.sta.network_key1', setVal)
			return 0
		end
	},

	[subROOT .. 'OpenAndSharedAuth.NetworkKey2'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			result=luardb.get('wlan.0.sta.network_key2')
			if not result or result == '' then return 0, '' end
			return 0, result

		end,

		set = function(node, name, value)
			local strlen= string.len(value)
			if strlen ~= 10 and strlen ~= 26 then return CWMP.Error.InvalidParameterValue end
			local setVal = value
			if not isChanged('wlan.0.sta.network_key2', setVal) then return 0 end
			luardb.set('wlan.0.sta.network_key2', setVal)
			return 0
		end
	},

	[subROOT .. 'OpenAndSharedAuth.NetworkKey3'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			result=luardb.get('wlan.0.sta.network_key3')
			if not result or result == '' then return 0, '' end
			return 0, result
		end,

		set = function(node, name, value)
			local strlen= string.len(value)
			if strlen ~= 10 and strlen ~= 26 then return CWMP.Error.InvalidParameterValue end
			local setVal = value
			if not isChanged('wlan.0.sta.network_key3', setVal) then return 0 end
			luardb.set('wlan.0.sta.network_key3', setVal)
			return 0
		end
	},

	[subROOT .. 'OpenAndSharedAuth.NetworkKey4'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			result=luardb.get('wlan.0.sta.network_key4')
			if not result or result == '' then return 0, '' end
			return 0, result
		end,

		set = function(node, name, value)
			local strlen= string.len(value)
			if strlen ~= 10 and strlen ~= 26 then return CWMP.Error.InvalidParameterValue end
			local setVal = value
			if not isChanged('wlan.0.sta.network_key4', setVal) then return 0 end
			luardb.set('wlan.0.sta.network_key4', setVal)
			return 0
		end
	},

	[subROOT .. 'PSKAuth.WpaEncryptionType'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''

			result=luardb.get('wlan.0.sta.encryption_type')
			if not result or result == '' then return 0, '' end
			return 0, result

		end,

		set = function(node, name, value)
			local AvailableEncryptTypeList={'TKIP', 'AES','TKIPAES'}
			local setVal=value:upper()

			if not setVal or not table.contains(AvailableAuthList, setVal) then return CWMP.Error.InvalidParameterValue end
			if not isChanged('wlan.0.sta.encryption_type', setVal) then return 0 end
			luardb.set('wlan.0.sta.encryption_type', setVal)
			return 0
		end
	},

	[subROOT .. 'WPAAuth.WpaRadiusServerIP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			result=luardb.get('wlan.0.sta.radius_server_ip')
			if not result or result == '' then return 0, '' end
			return 0, result
		end,

		set = function(node, name, value)
			local setVal=value:match("^%s*(%d+.%d+.%d+.%d+)%s*$")
			if not setVal or setVal == '' then return CWMP.Error.InvalidParameterValue end
			if not isChanged('wlan.0.sta.radius_server_ip', setVal) then return 0 end
			luardb.set('wlan.0.sta.radius_server_ip', setVal)
			return 0
		end
	},

	[subROOT .. 'WPAAuth.WpaEncryptionType'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			result=luardb.get('wlan.0.sta.encryption_type')
			if not result or result == '' then return 0, '' end
			return 0, result
		end,

		set = function(node, name, value)
			local AvailableEncryptTypeList={'TKIP', 'AES','TKIPAES'}
			local setVal=value:upper()
			if not setVal or not table.contains(AvailableAuthList, setVal) then return CWMP.Error.InvalidParameterValue end
			if not isChanged('wlan.0.sta.encryption_type', setVal) then return 0 end
			luardb.set('wlan.0.sta.encryption_type', setVal)
			return 0
		end
	},

}
