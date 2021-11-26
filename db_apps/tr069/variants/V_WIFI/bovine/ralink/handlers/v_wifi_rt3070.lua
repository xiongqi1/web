require("handlers.hdlerUtil")


local subROOT = conf.topRoot .. '.LANDevice.1.X_NETCOMM_WLANConfiguration.1.'



------------------local variable----------------------------
local g_systemFS='/sys/class/net/'
local g_wlanIface='ra'

local g_wifi_countryListTbl = {}
local g_networkmodeList = {
	["b-g"] = "0",
	["b-only"] = "1",
	["g-only"] = "4",
	["n-only"] = "6",
	["b-g-n"] = "9",
}
local g_wdsmodeList = {
	["disable"]="0",
	["bridge"]="2",
	["repeater"]="3",
}

-- parameter name and rdb variable name pairs
local g_ssidconf_namingPairs = {
	["Enable"]="enable",
	["SSID"]="ssid",
	["BroadcastSSID"]="hide_accesspoint",
	["NetworkAuth"]="network_auth",
	["EncryptionMode"]="encryption_type",
	["WEPKeyIndex"]="network_key_id",
	["WEPKey1"]="network_key1",
	["WEPKey2"]="network_key2",
	["WEPKey3"]="network_key3",
	["WEPKey4"]="network_key4",
	["WPAGroupRekeyinterval"]="wpa_group_rekey_interval",
	["RADIUSServerIPAddress"]="radius_server_ip",
	["RADIUSPort"]="radius_port",
	["RADIUSKey"]="radius_key",
	["WPAPreSharedKey"]="wpa_pre_shared_key",
	["FilteringPolicy"]="access_policy",
	["MACaddressList"]="access_control_list",
}

local g_available_authMode = {
	["OPEN"] = true,
	["SHARED"] = true,
	["WPA"] = true,
	["WPAPSK"] = true,
	["WPA2"] = true,
	["WPA2PSK"] = true,
	["WPAPSKWPA2PSK"] = true,
	["WPA1WPA2"] = true,
}

local g_wlan_rdbPrefix = 'wlan.'
local g_wlan_startIdx = '0'

local g_numOfSSIDConfInstance = 0
local g_depthOfSSIDConfInst = 7
------------------------------------------------------------

------------------local function prototype------------------
local pairsByKeys
local check_set_rdb
local isValidMACAddress
local rdb_get_ssidConf
local rdb_set_ssidConf
------------------------------------------------------------

------------------local function definition------------------
pairsByKeys = function(t, f)
	local a = {}
	for n in pairs(t) do table.insert(a, n) end
	table.sort(a, f)
	local i = 0      -- iterator variable
	local iter = function ()   -- iterator function
		i = i + 1
		if a[i] == nil then 
			return nil
		else 
			return a[i], t[a[i]]
		end
	end
	return iter
end

check_set_rdb = function(rdbname, rdbvalue)
	rdbname = string.trim(rdbname)
	if not rdbname or rdbname == '' then return false end
	local currVal = luardb.get(rdbname)
	if not currVal then  return false end
	if currVal ~= rdbvalue then
		luardb.set(rdbname, rdbvalue)
	end
	return true
end

isValidMACAddress = function(mac)
	mac = string.trim(mac)
	if not mac or mac == '' then return false end

	mac = string.upper(mac)

	if mac == 'FF:FF:FF:FF:FF:FF' or not string.match(mac, '^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$') then return false end

	return true
end

rdb_get_ssidConf = function(paramName, instIdx)
	paramName = string.trim(paramName)
	local index = tonumber(instIdx)

	if not paramName or paramName == '' or not index then return false end

	if index < 1 or index > g_numOfSSIDConfInstance then return false end

	index = index - 1

	if not g_ssidconf_namingPairs[paramName] then return false end

	return luardb.get(g_wlan_rdbPrefix .. index .. '.' .. g_ssidconf_namingPairs[paramName])
end

rdb_set_ssidConf = function(paramName, instIdx, setVal)
	paramName = string.trim(paramName)
	local index = tonumber(instIdx)

	if not paramName or paramName == '' or not index or not setVal then return false end

	if index < 1 or index > g_numOfSSIDConfInstance then return false end

	index = index - 1

	if not g_ssidconf_namingPairs[paramName] then return false end

	return check_set_rdb(g_wlan_rdbPrefix .. index .. '.' .. g_ssidconf_namingPairs[paramName], setVal)
end
------------------------------------------------------------

return {
-- string:readwrite
-- Default Value: AU
-- Available Value: Available values are listed in CountryList parameter
-- Involved RDB variable: localisation.region;0;0;0;32;AU
	[subROOT .. 'Country'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal=luardb.get('localisation.region') or ''
			
			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not value or value == '' then return CWMP.Error.InvalidParameterValue end

			if not g_wifi_countryListTbl or not g_wifi_countryListTbl[value] then return CWMP.Error.InvalidParameterValue end

			check_set_rdb('localisation.region', value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value: ";" separated list formatted [value of Country parameter]="Country name"
-- Involved RDB variable: This list is from "wlan.html" on webif
	[subROOT .. 'CountryList'] = {
		init = function(node, name, value)
			g_wifi_countryListTbl = table.load('/usr/lib/tr-069/handlers/wifi_country_list_table.lua') or {}
			return 0
		end,
		get = function(node, name)
			local retVal=''
			for key, value in pairsByKeys(g_wifi_countryListTbl) do
				retVal=retVal .. key .. '="' .. value .. '";'
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readwrite
-- Default Value: b-g-n(9)
-- Available Value: [b-g(0) | b-only(1) | g-only(4) | n-only(6) | b-g-n(9)]
-- Involved RDB variable: wlan.0.wireless_mode;0;0;0;32;9
	[subROOT .. 'NetworkMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local default = "b-g-n"
			local retVal = luardb.get('wlan.0.wireless_mode') or "9"
			local key = table.find(g_networkmodeList, retVal) or default

			return 0, key
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not value or value == '' then return CWMP.Error.InvalidParameterValue end

			if not g_networkmodeList[value] then return CWMP.Error.InvalidParameterValue end

			check_set_rdb('wlan.0.wireless_mode', g_networkmodeList[value])
			return 0
		end
	},

-- uint:readwrite
-- Default Value: 0
-- Available Value: It depends on the country setting. Available channel is listed in PossibleChannels parameter.
-- Involved RDB variable: wlan.0.conf.channel;0;0;0;32;0
	[subROOT .. 'Channel'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local defaultV = '0'
			local retVal = luardb.get('wlan.0.conf.channel') or defaultV
			if not tonumber(retVal) then retVal = defaultV end

			return 0, retVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not value or value == '' or not tonumber(value) then return CWMP.Error.InvalidParameterValue end

			check_set_rdb('wlan.0.conf.channel', value)
			return 0
		end
	},

-- uint:readonly
-- Default Value: '0'
-- Available Value: 0 ~ 
-- Involved RDB variable: wlan.0.currChan [this value is available only in case auto frequency mode(wlan.0.conf.channel = 0), other case just use "wlan.0.conf.channel"]
	[subROOT .. 'CurrentChannel'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = '0'
			local channelSetting = luardb.get('wlan.0.conf.channel')
			if channelSetting and channelSetting == '0' then
				local currChan = luardb.get('wlan.0.currChan') or '0'
				if tonumber(currChan) then
					retVal = currChan
				end
			elseif channelSetting then
				if tonumber(channelSetting) then
					retVal = channelSetting
				end
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'PossibleChannels'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local maxchannel="11"
			local country = luardb.get('localisation.region') or ''
			local wlmode = luardb.get('wlan.0.wireless_mode') or ''

			if country=="AU" or country=="JP" or country=="TW" or country=="FR" or country=="IE" or country=="CN" or country=="HK" or country=="ALL" then
				maxchannel = 13
			end
			if country=="JP" and wlmode == '1' then
				maxchannel = 14
			end
			return 0, "0~" .. maxchannel
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:readwrite
-- Default Value: Disable(0)
-- Available Value: [disable(0)|bridge(2)|repeater(3)]
-- Involved RDB variable: wlan.0.wds_mode;0;0;0;32;0
	[subROOT .. 'WDS.WDSMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local internel_wds_mode = luardb.get('wlan.0.wds_mode') or '0'

			local externalVal = table.find(g_wdsmodeList, internel_wds_mode) or "Disable"

			return 0, externalVal
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not value or value == '' then return CWMP.Error.InvalidParameterValue end

			value = string.lower(value)
			if not g_wdsmodeList[value] then return CWMP.Error.InvalidParameterValue end

			check_set_rdb('wlan.0.wds_mode', g_wdsmodeList[value])
			return 0
		end
	},

-- string:readonly
-- Default Value:
-- Available Value: MAC address separated by ":"
-- Involved RDB variable: wlan.0.mac (need to make it uppercase)
	[subROOT .. 'WDS.SourceMACaddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local mac = luardb.get('wlan.0.mac') or ''
			mac = string.upper(mac)
			return 0, mac
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- string:writeonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: wlan.0.wds_key;0;0;0;32; (This menu is displayed, if the Encrypt Type is not OPEN. When the type is OPEN, this menu is gone)
	[subROOT .. 'WDS.EncryptKey'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ""
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidParameterValue end

			check_set_rdb('wlan.0.wds_key', value)
			return 0
		end
	},

-- TODO
-- string:readwrite
-- Default Value: ""
-- Available Value: ";" separated list. the maximun num of elements of the list is 4
-- Involved RDB variable: wlan.0.wds_maclist - MAC Address lists that is separated with ";"
	[subROOT .. 'WDS.AP_MACAddressList'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wds_maclist = luardb.get('wlan.0.wds_maclist') or ''

			wds_maclist = string.trim(wds_maclist)
			return 0, wds_maclist
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if not value then return CWMP.Error.InvalidParameterValue end

			local output_macTbl = {}
			local inupt_macTbl = value:explode(';')

			for _, addr in ipairs(inupt_macTbl) do
				if isValidMACAddress(addr) and not table.contains(output_macTbl, string.upper(addr)) then
					table.insert(output_macTbl, string.upper(addr))
				end
			end

			if #output_macTbl > 4 then return CWMP.Error.InvalidParameterValue end

			check_set_rdb('wlan.0.wds_maclist', table.concat(output_macTbl, ';'))
			return 0
		end
	},

-- TODO
-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'SSIDConfigurationNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = subROOT .. 'SSIDConfiguration'
			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- TODO
-- object:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable: wlan.{i}.enable;0;0;0;32;1
	[subROOT .. 'SSIDConfiguration'] = {
		init = function(node, name, value)
			local idx = 0
			for i, value in hdlerUtil.traverseRdbVariable{prefix=g_wlan_rdbPrefix , suffix='.enable' , startIdx=g_wlan_startIdx} do
				value = string.trim(value)
				if not value then break end
				idx = idx + 1
				node:createDefaultChild(idx)
			end

			node.instance = idx
			g_numOfSSIDConfInstance = idx
			return 0
		end,
	},

	[subROOT .. 'SSIDConfiguration.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfSSIDConfInst = #pathBits
			return 0 
		end,
	},



-- TODO
--[[
* Enable			- bool:readwrite [wlan.{i}.enable;0;0;0;32;1]
* BSSID			- string:readonly []
* SSID			- string:readwrite [wlan.{i}.ssid;0;0;0;32;NetComm Wireless]
* BroadcastSSID		- bool:readwrite [wlan.{i}.hide_accesspoint;0;0;0;32;]	--> rdb value 0: Enable, 1: Disable
* NetworkAuth		- string:readwrite [wlan.{i}.network_auth;0;0;0;32;WPA2PSK], Available value: [OPEN | SHARED | WPA | WPAPSK | WPA2 | WPA2PSK | WPAPSKWPA2PSK | WPA1WPA2]
* EncryptionMode		- string:readwrite [wlan.{i}.encryption_type], Available value: [NONE|WEP|TKIP|AES]
				OPEN		-> encryption_type [NONE|WEP]
				SHARED		-> encryption_type [NONE|WEP]
				WPA		-> encryption_type [TKIP]
				WPAPSK		-> encryption_type [TKIP]
				WPA2		-> encryption_type [TKIP]
				WPA2PSK		-> encryption_type [AES]
				WPAPSKWPA2PSK	-> encryption_type [AES]
				WPA1WPA2		-> encryption_type [AES]
* WEPEncryptionLevel	- string:readonly [64-bit,104-bit] (Comma-separated list of the supported key lengths)
* WEPKeyIndex		- uint:readwrite [wlan.{i}.network_key_id;0;0;0;32;1], Available value: [1-4]
* WEPKey1		- string:writeonly [wlan.{i}.network_key1;0;0;0;32;]
* WEPKey2		- string:writeonly [wlan.{i}.network_key2;0;0;0;32;]
* WEPKey3		- string:writeonly [wlan.{i}.network_key3;0;0;0;32;]
* WEPKey4		- string:writeonly [wlan.{i}.network_key4;0;0;0;32;]
* WPAGroupRekeyinterval	- uint:readwrite [wlan.{i}.wpa_group_rekey_interval;0;0;0;32;600]
* RADIUSServerIPAddress	- string:readwrite [wlan.{i}.radius_server_ip;0;0;0;32;0.0.0.0], Available value: IP Address
* RADIUSPort		- uint:readwrite [wlan.{i}.radius_port;0;0;0;32;1812]
* RADIUSKey		- string:writeonly [wlan.{i}.radius_key;0;0;0;32;]
* WPAPreSharedKey		- string:writeonly [wlan.{i}.wpa_pre_shared_key;0;0;0;96;a1b2c3d4e5] (WPA Pre-Shared Key should be between 8 and 63 ASCII characters or 64 hexadecimal digits.)
--]]
	[subROOT .. 'SSIDConfiguration.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfSSIDConfInst]
			local paramName = pathBits[g_depthOfSSIDConfInst+1]
			local retVal = ''
			local defaultV = ''
			
			if not dataModelIdx or not paramName then return CWMP.Error.InvalidParameterValue end

			local indexNumType = tonumber(dataModelIdx)
			if not indexNumType or  indexNumType < 1 or indexNumType > g_numOfSSIDConfInstance then return CWMP.Error.InvalidParameterValue end

			if paramName == 'Enable' then
				defaultV='0'
			elseif paramName == 'SSID' then
				defaultV = ''
			elseif paramName == 'BroadcastSSID' then
				defaultV = '0'
			elseif paramName == 'NetworkAuth' then
				defaultV = 'WPA2PSK'
			elseif paramName == 'WEPKeyIndex' then
				defaultV='1'
			elseif paramName == 'WPAGroupRekeyinterval' then
				defaultV='600'
			elseif paramName == 'RADIUSServerIPAddress' then
				defaultV='0.0.0.0'
			elseif paramName == 'RADIUSPort' then
				defaultV='1812'
			elseif paramName == 'MACFiltering' then
				defaultV=''
			elseif paramName == 'WEPEncryptionLevel' then
				return 0, '64-bit,104-bit'
			elseif paramName == 'WEPKey1' then
				-- writeonly, so return empty
				return 0, ''
			elseif paramName == 'WEPKey2' then
				-- writeonly, so return empty
				return 0, ''
			elseif paramName == 'WEPKey3' then
				-- writeonly, so return empty
				return 0, ''
			elseif paramName == 'WEPKey4' then
				-- writeonly, so return empty
				return 0, ''
			elseif paramName == 'RADIUSKey' then
				-- writeonly, so return empty
				return 0, ''
			elseif paramName == 'WPAPreSharedKey' then
				-- writeonly, so return empty
				return 0, ''
			elseif paramName == 'BSSID' then
				local sysfile = g_systemFS .. g_wlanIface .. (indexNumType-1) .. '/address'
				if hdlerUtil.IsRegularFile(sysfile) then
					local mac = Daemon.readStringFromFile(sysfile)
					mac = string.trim(mac)
					return 0, string.upper(mac)
				else
					return 0, ''
				end
			elseif paramName == 'EncryptionMode' then
				local networkAuth = rdb_get_ssidConf('NetworkAuth', dataModelIdx) or 'WPA2PSK'
				local encrypMode = rdb_get_ssidConf('EncryptionMode', dataModelIdx) or ''

				networkAuth = string.upper(networkAuth)
				encrypMode = string.upper(encrypMode)

				if networkAuth == 'OPEN' or networkAuth == 'SHARED' then
					if encrypMode ~= 'WEP' then
						encrypMode = 'NONE'
					end
				elseif networkAuth == 'WPA' or networkAuth == 'WPAPSK' or networkAuth == 'WPA2' then
					encrypMode = 'TKIP'
				elseif networkAuth == 'WPA2PSK' or networkAuth == 'WPAPSKWPA2PSK' or networkAuth == 'WPA1WPA2' then
					encrypMode = 'AES'
				else
					encrypMode = ''
				end
				return 0, encrypMode
			else
				error('Dunno how to handle ' .. name)
			end

			retVal = rdb_get_ssidConf(paramName, dataModelIdx) or defaultV

			if paramName == 'Enable' then
				retVal = string.trim(retVal)
				if retVal ~= '1' and retVal ~= '0' then retVal = defaultV end
			elseif paramName == 'BroadcastSSID' then --> rdb value 0: Enable, 1: Disable
				retVal = string.trim(retVal)
				if retVal ~= '1' and retVal ~= '0' then retVal = defaultV end
				if retVal == '1' then
					retVal = '0'
				else
					retVal = '1'
				end
			elseif paramName == 'WEPKeyIndex' then
				local integerVal = tonumber(retVal)
				if not integerVal or integerVal < 1 or integerVal >4 then retVal = defaultV end
			elseif paramName == 'WPAGroupRekeyinterval' or paramName == 'RADIUSPort' then
				local integerVal = tonumber(retVal)
				if not integerVal or integerVal < 1 then retVal = defaultV end
			end

			return 0, retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfSSIDConfInst]
			local paramName = pathBits[g_depthOfSSIDConfInst+1]
			local retVal = ''
			local defaultV = ''

			if not dataModelIdx or not paramName then return CWMP.Error.InvalidParameterName end

			local indexNumType = tonumber(dataModelIdx)
			if not indexNumType or  indexNumType < 1 or indexNumType > g_numOfSSIDConfInstance then return CWMP.Error.InvalidParameterName end

			local setValue = ''

			if paramName == 'Enable' then	-- bool:readwrite
				setValue = string.trim(value)
				if setValue ~= '1' and setValue ~= '0' then return CWMP.Error.InvalidParameterValue end
			elseif paramName == 'SSID' then	-- string:readwrite
				setValue = string.trim(value)
			elseif paramName == 'BroadcastSSID' then	-- bool:readwrite, inverted
				setValue = string.trim(value)
				if setValue ~= '1' and setValue ~= '0' then return CWMP.Error.InvalidParameterValue end
				if setValue == '1' then
					setValue = '0'
				else
					setValue = '1'
				end
			elseif paramName == 'NetworkAuth' then	-- string:readwrite [OPEN | SHARED | WPA | WPAPSK | WPA2 | WPA2PSK | WPAPSKWPA2PSK | WPA1WPA2]
				setValue = string.trim(value)
				setValue = string.upper(setValue)
				if not g_available_authMode[setValue] then return CWMP.Error.InvalidParameterValue end
			elseif paramName == 'EncryptionMode' then	-- string:readwrite [NONE|WEP|TKIP|AES]
				local networkAuth = rdb_get_ssidConf('NetworkAuth', dataModelIdx) or 'WPA2PSK'

				networkAuth = string.upper(networkAuth)
				setValue = string.trim(value)
				setValue = string.upper(setValue)

				if networkAuth == 'OPEN' or networkAuth == 'SHARED' then
					if setValue ~= 'NONE' and setValue ~= 'WEP'then return CWMP.Error.InvalidParameterValue end
				elseif networkAuth == 'WPA' or networkAuth == 'WPAPSK' or networkAuth == 'WPA2' then
					if setValue ~= 'TKIP' then return CWMP.Error.InvalidParameterValue end
				elseif networkAuth == 'WPA2PSK' or networkAuth == 'WPAPSKWPA2PSK' or networkAuth == 'WPA1WPA2' then
					if setValue ~= 'AES' then return CWMP.Error.InvalidParameterValue end
				else
					return 0
				end
			elseif paramName == 'WEPKeyIndex' then	-- uint:readwrite [1-4]
				setValue = tonumber(value)
				if not setValue or setValue < 1 or setValue > 4 then return CWMP.Error.InvalidParameterValue end
				setValue = tostring(setValue)
			elseif paramName == 'WEPKey1' or paramName == 'WEPKey2' or paramName == 'WEPKey3' or paramName == 'WEPKey4' then	-- string:writeonly
				setValue = value
				if not setValue or #setValue ~= 0 and #setValue ~= 10 and #setValue ~= 26 then return CWMP.Error.InvalidParameterValue end
				if string.match(setValue, '%X') then return CWMP.Error.InvalidParameterValue end
			elseif paramName == 'WPAGroupRekeyinterval' or paramName == 'RADIUSPort' then	-- uint:readwrite
				setValue = tonumber(value)
				if not setValue or setValue < 1 then return CWMP.Error.InvalidParameterValue end
				setValue = tostring(setValue)
			elseif paramName == 'RADIUSServerIPAddress' then	-- string:readwrite
				setValue = string.trim(value)
				if not Parameter.Validator.isValidIP4(setValue) then return CWMP.Error.InvalidParameterValue end
			elseif paramName == 'RADIUSKey' then	-- string:writeonly
				setValue = value
			elseif paramName == 'WPAPreSharedKey' then	-- string:writeonly (WPA Pre-Shared Key should be between 8 and 63 ASCII characters or 64 hexadecimal digits.)
				setValue = value
				if not setValue or #setValue < 8 or #setValue > 64 then return CWMP.Error.InvalidParameterValue end
				if #setValue == 64 and string.match(setValue, '%X') then return CWMP.Error.InvalidParameterValue end
			else
				error('Dunno how to handle ' .. name)
			end

			rdb_set_ssidConf(paramName, dataModelIdx, setValue)
			return 0
		end
	},

-- TODO
--[[
* MACFiltering.FilteringPolicy	- string:readwrite [wlan.{i}.access_policy;0;0;0;32;0], Available value: [Disable(0)|Allow(1)|Block(2)]
* MACFiltering.MACaddressList	- string:readwrite [wlan.{i}.access_control_list;0;0;0;32;] (";" separated MAC address list)
--]]
	[subROOT .. 'SSIDConfiguration.*.MACFiltering.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfSSIDConfInst]
			local paramName = pathBits[g_depthOfSSIDConfInst+2]
			local retVal = ''

			if not dataModelIdx or not paramName then return CWMP.Error.InvalidParameterValue end

			local indexNumType = tonumber(dataModelIdx)
			if not indexNumType or  indexNumType < 1 or indexNumType > g_numOfSSIDConfInstance then return CWMP.Error.InvalidParameterValue end

			if paramName == 'FilteringPolicy' then
				local rdbValue = rdb_get_ssidConf(paramName, dataModelIdx) or '0'
				if rdbValue == '1' then 
					retVal='Allow'
				elseif rdbValue == '2' then 
					retVal='Block'
				else
					retVal='Disable'
				end
			elseif paramName == 'MACaddressList' then
				retVal = rdb_get_ssidConf(paramName, dataModelIdx) or ''
			else
				error('Dunno how to handle ' .. name)
			end
			return 0, retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfSSIDConfInst]
			local paramName = pathBits[g_depthOfSSIDConfInst+2]
			local retVal = ''
			local defaultV = ''

			if not dataModelIdx or not paramName or not value then return CWMP.Error.InvalidParameterName end

			local indexNumType = tonumber(dataModelIdx)
			if not indexNumType or  indexNumType < 1 or indexNumType > g_numOfSSIDConfInstance then return CWMP.Error.InvalidParameterName end

			local setValue = ''

			if paramName == 'FilteringPolicy' then
				value = string.trim(value)
				value = string.lower(value)
				if value == 'disable' then
					setValue = '0'
				elseif value == 'allow' then
					setValue = '1'
				elseif value == 'block' then
					setValue = '2'
				else
					return CWMP.Error.InvalidParameterValue
				end
			elseif paramName == 'MACaddressList' then
				local in_macList = value:explode(';')
				local out_macList = {}

				for _, mac in ipairs(in_macList) do
					mac = string.trim(mac)

					if mac ~= '' and isValidMACAddress(mac) then return CWMP.Error.InvalidParameterName end

					if mac ~= '' and not table.contains(out_macList, string.upper(mac)) then
						table.insert(out_macList, string.upper(mac))
					end
				end
				setValue = table.concat(out_macList, ';')
			else
				error('Dunno how to handle ' .. name)
			end

			rdb_set_ssidConf(paramName, dataModelIdx, setValue)
			return 0
		end
	},
}
