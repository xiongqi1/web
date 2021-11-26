----
-- NTP Bindings
--
-- Vaugely Time:2 compilant, but largely read-only.
-- NTPServerX and Enable are mapped directly to RDB by config file persist() handlers.
-- We also implement InternetGatewayDevice.DeviceInfo.UpTime here too.
----
---[[

--[[
Object1 --> "b"  nvram_get('WirelessMode') = "1"
Object2 --> "g"  nvram_get('WirelessMode') = "0"
Object3 --> "g-only"  nvram_get('WirelessMode') = "4"
Object4 --> "n"  nvram_get('WirelessMode') = "9"
Object5 --> "n-only" (Out of Spec-TBD)  nvram_get('WirelessMode') = "6"
--]]

-- local function curWLANConfObjidx(name)
-- 	local pathBits = name:explode('.')
-- 	return pathBits[5]
-- end
-- 
-- local function enabledWLANConfObjidx()
-- 	local mode = luanvramDB.get('WirelessMode')
-- 	local nvramVal={"1", "0", "4", "9", "6"}
-- 	local enabledidx=0
-- 	
-- 	for i,v in ipairs(nvramVal) do
-- 		if mode == v then
-- 			enabledidx = i
-- 			break
-- 		end
-- 	end
-- 	return tostring(enabledidx)
-- end

local function curWLANConfObjidx(name)
	return 1
end

local function enabledWLANConfObjidx()
	return 1
end
local WLANConfigurationObjWatcher = function(node, name)
	dimclient.log('debug', 'rebuild WLANConfiguration Object table')

	local pathBits = name:explode('.')

	local numOfBssid = tonumber(luanvram.bufget('BssidNum'))

	if numOfBssid == nil then return 0 end

	if numOfBssid == node:countInstanceChildren() then return 0 end

	local leaseCollection = findNode(paramRoot, name)
	if leaseCollection == nil then return 0 end
	if leaseCollection.children ~= nil then
		for _, lease in ipairs(leaseCollection.children) do
			if lease.name ~= '0' then
				dimclient.log('info', 'wifi station watcher: deleting lease: id = ' .. lease.name)
				dimclient.delObject(name.. '.' .. lease.name)
			end
		end
		leaseCollection.instance = 0
	end

	for id=1, numOfBssid do
			local ret, objidx = dimclient.addObject(name..'.')
			if ret > 0 then
				dimclient.log('error', 'addObject failed: ' .. ret)
			else
				-- we store the TR-069 instance id in RDB
				dimclient.log('info', 'new wifi station id = ' .. objidx)
			end
	end
	node.instance = numOfBssid
	return 0
end

local wifistationWatcher = function(node, name)
	dimclient.log('debug', 'rebuild wifi station object table')
	dimclient.log('debug', 'name='..name)


	local pathBits = name:explode('.')
	local idx = tonumber(pathBits[5])
	local numOfBssid = tonumber(luanvram.bufget('BssidNum'))

	if idx == nil or numOfBssid == nil then return 0 end

-- 	dimclient.log('debug', 'idx='..idx..', numOfBssid='..numOfBssid)
-- 	dimclient.log('debug', 'length of DeviceTbl='..#DeviceTbl[idx])
-- 	dimclient.log('debug', 'number of children='..node:countInstanceChildren())

	init_DeviceTbl()

	if DeviceTbl[idx] == nil then return 0 end
	if #DeviceTbl[idx] == node:countInstanceChildren() then return 0 end

	local leaseCollection = findNode(paramRoot, name)
	if leaseCollection == nil then return 0 end
	if leaseCollection.children ~= nil then
		for _, lease in ipairs(leaseCollection.children) do
			if lease.name ~= '0' then
				dimclient.log('info', 'wifi station watcher: deleting lease: id = ' .. lease.name)
				dimclient.delObject(name.. '.' .. lease.name)
			end
		end
		leaseCollection.instance = 0
	end
	local maxInstanceId = 0

	if DeviceTbl[idx] == nil then return 0 end
 
	for id, v in ipairs(DeviceTbl[idx]) do
			local ret, objidx = dimclient.addObject(name..'.')
			if ret > 0 then
				dimclient.log('error', 'addObject failed: ' .. ret)
			else
				-- we store the TR-069 instance id in RDB
				dimclient.log('info', 'new wifi station id = ' .. objidx)
			end
	end
	node.instance = maxInstanceId
	return 0
end

-- local DeviceTbl={}

function init_DeviceTbl ()
	local arraySize = tonumber(luanvram.bufget('BssidNum'))
	DeviceTbl={}

	if arraySize == nil then return 0 end
	for i=1, arraySize do
		DeviceTbl[i] = {}
	end

	local info = luanvram.getstationinfo()

	if info == nil or info == "" then return end

	local items = info:explode('&')

	for i, v in ipairs(items) do
		if v == nil or v == "" then break end

		local iteminfo = v:explode(',')
		ssidIdx = tonumber(iteminfo[3])
		if ssidIdx ~= nil then
			DeviceTbl[ssidIdx][#DeviceTbl[ssidIdx] +1] = {["macAddr"]=iteminfo[1],["ipAddr"]=iteminfo[2],["SSIDidx"]=iteminfo[3]}
		end
	end

-- 	dimclient.callbacks.register('cleanup', reset_DeviceTbl_cb)

end

function lengthOfTbl(ssidIdx)
	return #DeviceTbl[ssidIdx]
end

function getItemOfTbl(ssidIdx, arrayIdx, itemname)  --> ssidIdx:number, arrayIdx:number, itemname:string
	local retVal = DeviceTbl[ssidIdx][arrayIdx][itemname]

	if retVal == nil then return "" end
	return retVal
end

function possiblechannel()
	local bChannels={US=11, CA=11, FR=13, IE=13, TW=13, CN=13, HK=13, AU=13, UAE=13, JP=14,}
	local gChannels={US=11, CA=11, FR=13, IE=13, TW=13, CN=13, HK=13, AU=13, UAE=13, JP=13,}
	local ChannelsTbl={bChannels, gChannels}

	local wirelessmode = luanvramDB.get('WirelessMode')
	local curObjidx = 0
	local country=luanvramDB.get('CountryCode')

	if wireless == "1" then
		curObjidx = 0 -- bChannels
	else
		curObjidx = 1 -- gChannels
	end

	local resultStr=ChannelsTbl[tonumber(curObjidx)][country]

	if resultStr == nil then
		return
	else
		return 1, resultStr
	end
end


function get_wirelessStat()
	if wirelessStat == nil then
		wirelessStat=execute_CmdLine('iwpriv ra0 show stat; dmesg')
	end

	dimclient.callbacks.register('cleanup', clear_wirelessStat)

	return wirelessStat
end

----------[start] CallBack Functions ----------
function basic_cleanup_cb()
	luanvramDB.commit()

	luanvram.wifibasicCB()

-- 	os.execute("/usr/bin/appweb -I")

	return 0
end

function BeaconAd_cb()
	local cmd = ""
	local setValue = getItem(1, 'HideSSID')

	if setValue == "1" or setValue == "0" then
		cmd = build_BeaconAd_command(setValue)
		os.execute(cmd)
	end
	return 0
end

function reset_DeviceTbl_cb()
	DeviceTbl = nil
end

function clear_wirelessStat()
	wirelessStat = nil
end
----------[end] CallBack Functions ----------

function getItem(idx, name)
	local nvramVal = luanvramDB.get(name)
	local itemTbl = nvramVal:explode(';')
	local retVal = itemTbl[tonumber(idx)]

	if retVal == nil then return "" end

	retVal = string.gsub(retVal:gsub("^%s+", ""), "%s+$", "")

	if retVal == nil then
		return ""
	else
		return retVal
	end
end

function setItem(idx, name, value)
	local setValue
	local nvramVal = luanvramDB.get(name)
	local itemTbl = nvramVal:explode(';')
	
	value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

	itemTbl[tonumber(idx)] = value

	for _,v in ipairs(itemTbl) do
		if setValue == nil then --> Do not define setValue. This should be nil at first
			setValue = v
		else
			setValue = setValue .. ";" .. v
		end
	end
	if setValue ~= nil then
		luanvramDB.set(name, setValue)
	end

	return 0
end

function setAllItems(name, value)
	local setValue
	local nvramVal = luanvramDB.get(name)
	local itemTbl = nvramVal:explode(';')
	
	value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

	for _,v in ipairs(itemTbl) do
		if setValue == nil then --> Do not define setValue. This should be nil at first
			setValue = value
		else
			setValue = setValue .. ";" .. value
		end
	end
	if setValue ~= nil then
		luanvramDB.set(name, setValue)
	end

	return 0
end

function build_BeaconAd_command(value)
	local setValue
	local nvramVal = luanvramDB.get('HideSSID')
	local itemTbl = nvramVal:explode(';')
	
	value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

	for i,v in ipairs(itemTbl) do
		if setValue == nil then --> Do not define setValue. This should be nil at first
			setValue = 'iwpriv ra' .. i-1 .. ' set HideSSID=' .. value
		else
			setValue = setValue .. ";" .. 'iwpriv ra' .. i-1 .. ' set HideSSID=' .. value
		end
	end
	if setValue ~= nil then
		return setValue
	end

	return ""
end

function updateFlash8021x()
	local lanAddr = luanvram.getIfIp('br0')

	lanAddr = string.gsub(lanAddr:gsub("^%s+", ""), "%s+$", "")

	if not isValidIP4(lanAddr) then return 0 end

	luanvramDB.set('own_ip_addr', lanAddr)
	luanvramDB.set('EAPifname', 'br0')
	luanvramDB.set('PreAuthifname', 'br0')
end

function confWPAGeneral(idx)
-- 	setItem(tonumber(idx), 'DefaultKeyID','2')
	setItem(tonumber(idx), 'RekeyMethod','TIME')
end

function getAuthMode(idx)
	local authMode = luanvramDB.get('AuthMode')
	local itemTbl = authMode:explode(';')
	local retVal = itemTbl[tonumber(idx)]

	retVal = string.gsub(retVal:gsub("^%s+", ""), "%s+$", "")

	if retVal == nil then
		return ""
	else
		return retVal
	end
end

function setAuthMode(idx, value)
	local setValue = nil
	local authMode = luanvramDB.get('AuthMode')
	local itemTbl = authMode:explode(';')
	
	value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

	itemTbl[tonumber(idx)] = value

	for _,v in ipairs(itemTbl) do
		if setValue == nil then --> Do not define setValue. This should be nil at first
			setValue = v
		else
			setValue = setValue .. ";" .. v
		end
	end

	if setValue ~= nil then
-- 		if value:find("WPA") ~= nil and value:find("PSK") == nil then
-- 			confWPAGeneral(idx)
-- 		end

		if value:find("WPA") ~= nil then
			confWPAGeneral(idx)
		end

		setItem(1, 'IEEE8021X', '0')
		luanvramDB.set('AuthMode', setValue)
	end

	return 0
end

function getEncrypType(idx)
	local encrypType = luanvramDB.get('EncrypType')
	local itemTbl = encrypType:explode(';')
	local retVal = itemTbl[idx]

	retVal = string.gsub(retVal:gsub("^%s+", ""), "%s+$", "")

	if retVal == nil then
		return ""
	else
		return retVal
	end
end

function setEncrypType(idx, value)
	local setValue = nil
	local encrypType = luanvramDB.get('EncrypType')
	local itemTbl = encrypType:explode(';')
	
	value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

	itemTbl[tonumber(idx)] = value

	for _,v in ipairs(itemTbl) do
		if setValue == nil then --> Do not define setValue. This should be nil at first
			setValue = v
		else
			setValue = setValue .. ";" .. v
		end
	end

	if setValue ~= nil then
		luanvramDB.set('EncrypType', setValue)
	end

	return 0
end

function ascii2hexdecimal(value)
	local strlen = #value
	local directives = ""

	if strlen == 0 then
		return "", false
	end

	for i=1,#value do directives = directives .. "%02x" end

	return string.format(directives, value:byte(1, #value)), true
end

function retrieve_lastStringBlock(input, startStr, endStr)
	local pos = 1
	local retVal = ""
	local sidx1, eidx1, sidx2, eidx2
	local startidx

	if input == nil or startStr == nil or endStr == nil then return "" end

	while true do
		sidx1, eidx1 = string.find(input, startStr, pos)
		if sidx1 ~= nil then
			startidx = sidx1
			pos = eidx1 + 1
		else
			if startidx ~= nil then
				sidx2, eidx2 = string.find(input, endStr, startidx)
				if eidx2 ~= nil then
					retVal = string.sub(input, startidx, eidx2)
				end
			end
			break
		end
	end

	return retVal
end

return {

-- Done
	['**.LANWLANConfigurationNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local forwarding = findNode(paramRoot, 'InternetGatewayDevice.LANDevice.' .. pathBits[3] .. '.WLANConfiguration')
			local retVal = forwarding:countInstanceChildren()
			if retVal == nil then return 0 end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end
			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},

	['**.WLANConfiguration'] = {
		init = function(node, name, value)

			local pathBits = name:explode('.')

			local numOfBssid = luanvramDB.get('BssidNum')

			local maxInstanceId = tonumber(numOfBssid)

			if maxInstanceId == nil then return 0 end

			for id=1, maxInstanceId do
				local instance = node:createDefaultChild(id)
			end
			node.instance = maxInstanceId
			return 0
		end,
		create = function(node, name, instanceId)
			dimclient.log('debug', 'WLANConfiguration Object create')
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('debug', 'Delete WLANConfiguration Object, name = ' .. name)
			node.parent:deleteChild(node)
			return 0
		end,
		poll = WLANConfigurationObjWatcher
	},
	['**.WLANConfiguration.*']  = {
		create = function(node, name, instanceId)
			dimclient.log('debug', 'Wireless AssociatedDevice create')
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('debug', 'Wireless AssociatedDevice delete, name = ' .. name)
			node.parent:deleteChild(node)
			return 0
		end,
		
	},
-- Done
	['**.WLANConfiguration.*.Enable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('RadioOff')

			if retVal == "1" then
				return "0"
			else
				return "1"
			end
		end,
		set = function(node, name, value)

			if value == "1" or value == "0" then
				if value == "1" then
					luanvramDB.set('RadioOff', "0")
				else
					luanvramDB.set('RadioOff', "1")
				end
				dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			end
			return 0
		end
	},
-- Done
	['**.WLANConfiguration.*.Status'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('RadioOff')

			if retVal == "0" then
				return "Up"
			else
				return "Disabled"
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.WLANConfiguration.*.BSSID'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5]) - 1
			local numOfBssid = luanvramDB.get('BssidNum')

			if idx >= tonumber(numOfBssid) then return "" end

			if idx ~= nil then
				local retVal = readStringFromFile('/sys/class/net/ra' ..idx.. '/address')
				if retVal ~= nil then
					return retVal
				end
			end

			return ""
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- TODO: Deleted
	['**.WLANConfiguration.*.MaxBitRate'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Done
	['**.WLANConfiguration.*.Channel'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local cmdReturn = execute_CmdLine("iwlist ra0 channel")
			local temp=string.match(cmdReturn:lower() , "current channel:%d+")

			if temp == nil then return 0 end

			local currentChannel = temp:gsub("current channel:", "")

			if currentChannel == nil then
				return 0
			else
				return currentChannel
			end
--[[
			local retVal = luanvramDB.get('Channel')

			if retVal == nil then
				return ""
			end
			return retVal
]]--
		end,
		set = function(node, name, value)
			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")
			local min, max = possiblechannel()

			if min == nil or max == nil then return 0 end

			local valueNum = tonumber(value)

			if valueNum == nil then return cwmpError.InvalidParameterValue end

			if valueNum <= tonumber(max) and valueNum >= tonumber(min) then
				luanvramDB.set('Channel', value)
				dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			else
				return cwmpError.InvalidParameterValue
			end

			return 0
		end
	},
-- Version 1.4
	['**.WLANConfiguration.*.AutoChannelEnable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('Channel')

			if retVal == "0" then
				return "1"
			else
				return "0"
			end
		end,
		set = function(node, name, value)
			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if value ~= "1" and value ~= "0" then return cwmpError.InvalidParameterValue end

			if value == "1" then
				luanvramDB.set('Channel', "0")
				dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			elseif value == "0" then
				local cmdReturn = execute_CmdLine("iwlist ra0 channel")
				local temp=string.match(cmdReturn:lower() , "current channel:%d+")

				if temp == nil then
					luanvramDB.set('Channel', "5")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end

				local currentChannel = temp:gsub("current channel:", "")

				if currentChannel == nil then
					return cwmpError.InternalError
				else
					luanvramDB.set('Channel', currentChannel)
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
				end
			end

			return 0
		end
	},
-- TELUS Only
	['**.WLANConfiguration.*.CurrentChannel'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local cmdReturn = execute_CmdLine("iwlist ra0 channel")
			local temp=string.match(cmdReturn:lower() , "current channel:%d+")

			if temp == nil then return 0 end

			local currentChannel = temp:gsub("current channel:", "")

			if currentChannel == nil then
				return '0'
			else
				return currentChannel
			end
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Done
	['**.WLANConfiguration.*.SSID'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local retVal = luanvramDB.get('SSID' .. idx)

			if retVal == nil then
				return ""
			end
			return retVal
		end,
		set = function(node, name, value)
			local retVal = string.gsub(value:gsub("^%s+", ""), "%s+$", "")
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			if retVal ~= nil and retVal ~= "" then
				luanvramDB.set('SSID' .. idx, value)
				dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			end
			return 0
		end
	},
-- TODO
	['**.WLANConfiguration.*.BeaconType'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end
			local authMode = getAuthMode(idx)
			local ieee8021x = getItem(idx, "IEEE8021X")

			if ieee8021x ~= nil and ieee8021x == "1" then return "802.1X" end

			if authMode == "Disable" then
				return "None"
			elseif authMode == "OPEN" or authMode == "SHARED" then
				return "Basic"
			elseif authMode == "WPAPSK" or authMode == "WPA" then
				return "WPA"
			elseif authMode == "WPA2PSK" or authMode == "WPA2" then
				return "11i"
			elseif authMode == "WPAPSKWPA2PSK" or authMode == "WPA1WPA2" then
				return "WPAand11i"
			else
				return "None"
			end
		end,
		set = function(node, name, value)
			local beacontype = string.gsub(value:gsub("^%s+", ""), "%s+$", "")
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end
			local authMode = getAuthMode(idx)
			local encrypType = getEncrypType(idx)

			if beacontype == "None" then
				if authMode ~= "Disable" then
					setAuthMode(idx, "OPEN")
					setEncrypType(idx, "NONE")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
				end
			elseif beacontype == "Basic" then
				if authMode ~= "SHARED" and authMode ~= "OPEN" then
					setAuthMode(idx, "OPEN")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
				end
			elseif beacontype == "WPA" then
				if authMode ~= "WPAPSK" and authMode ~= "WPA" then
					setAuthMode(idx, "WPAPSK")
					if encrypType ~= "TKIP" and encrypType ~= "AES" and encrypType ~= "TKIPAES" then
						setEncrypType(idx, "TKIP")
					end
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
				end
			elseif beacontype == "11i" then
				if authMode ~= "WPA2PSK" and authMode ~= "WPA2" then
					setAuthMode(idx, "WPA2PSK")
					if encrypType ~= "TKIP" and encrypType ~= "AES" and encrypType ~= "TKIPAES" then
						setEncrypType(idx, "TKIP")
					end
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
				end
			elseif beacontype == "WPAand11i" then
				if authMode ~= "WPAPSKWPA2PSK" and authMode ~= "WPA1WPA2" then
					setAuthMode(idx, "WPAPSKWPA2PSK")
					if encrypType ~= "TKIP" and encrypType ~= "AES" and encrypType ~= "TKIPAES" then
						setEncrypType(idx, "TKIP")
					end
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
				end
			else 
				return cwmpError.InvalidParameterValue
			end

			return 0
		end
	},
-- TODO
	['**.WLANConfiguration.*.MACAddressControlEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local ctlmode = luanvramDB.get('AccessPolicy0')

			if ctlmode == "1" then
				return "1"
			else
				return "0"
			end
		end,
		set = function(node, name, value)
			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if value == "1" or value == "0" then
				luanvramDB.set('AccessPolicy0', value)
				os.execute('iwpriv ra0 set AccessPolicy=' .. value)
				return 0
			else
				return cwmpError.InvalidParameterValue
			end
		end
	},
-- Done
	['**.WLANConfiguration.*.Standard'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local wirelessmode = luanvramDB.get('WirelessMode')
			local retVal = " "

			if wirelessmode == "0" then
				retVal = "g"
			elseif wirelessmode == "1" then
				retVal = "b"
			elseif wirelessmode == "4" then
				retVal = "g-only"
			elseif wirelessmode == "6" then
				retVal = "n"
			elseif wirelessmode == "9" then
				retVal = "n"
			end

			return retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Done
	['**.WLANConfiguration.*.WEPKeyIndex'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local retVal = getItem(idx,'DefaultKeyID')

			if retVal == nil or retVal == "" then
				return "0"
			end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			local tempnumber = tonumber(value)

			if tempnumber == nil then
				return cwmpError.InvalidParameterValue
			elseif tempnumber > 0 and tempnumber < 5 then
				setItem(idx, 'DefaultKeyID',value)
				dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			else
				return cwmpError.InvalidParameterValue
			end
			return 0
		end
	},
-- Done:: WPAPSK1
	['**.WLANConfiguration.*.KeyPassphrase'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local retVal = luanvramDB.get('WPAPSK' .. idx)

			if retVal == nil then
				return ""
			end
			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			luanvramDB.set('WPAPSK' .. idx, value)
			dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			return 0
		end
	},
	['**.WLANConfiguration.*.WEPEncryptionLevel'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local encrypType = getEncrypType(idx)

			if encrypType ~= "WEP" then
				return "Disabled"
			end

			local WEPKeyidx = getItem(idx, 'DefaultKeyID')

			WEPKeyidx = string.gsub(WEPKeyidx:gsub("^%s+", ""), "%s+$", "")

			if WEPKeyidx == "" then  --> this should place at first.
				return "Disabled"
			elseif string.find(WEPKeyidx, "[^%d]") ~= nil then
				return "Disabled"
			end

			local WEPKey = luanvramDB.get("Key" ..WEPKeyidx.. "Str1" )
			WEPKey = string.gsub(WEPKey:gsub("^%s+", ""), "%s+$", "")

			local keytype = "Key" ..WEPKeyidx.. "Type"
			local ishex = getItem(idx, keytype)

			if ishex == "1" then
				WEPKey = ascii2hexdecimal(WEPKey)
			end

			if #WEPKey == 10 then
				return "40-bit"
			elseif #WEPKey == 26 then
				return "104-bit"
			else
				return "Disabled"
			end
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Done
	['**.WLANConfiguration.*.BasicEncryptionModes'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local returnStr = ""
			local authMode = getAuthMode(idx)
			local encrypType = getEncrypType(idx)

			if authMode == "OPEN" or authMode == "SHARED" then
				if encrypType == "WEP" then
					returnStr = "WEPEncryption"
				elseif encrypType == "NONE" then
					returnStr = "None"
				end
			end

			return returnStr
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			local authMode = getAuthMode(idx)

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if authMode == "OPEN" or authMode == "SHARED" then
				if value == "WEPEncryption" then
					setEncrypType(idx, "WEP")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "None" then
					setEncrypType(idx, "NONE")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end
			end

			return cwmpError.InvalidParameterValue
		end
	},
-- Done
	['**.WLANConfiguration.*.BasicAuthenticationMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local returnStr = ""
			local authMode = getAuthMode(idx)

			if authMode == "SHARED" then
				returnStr = "SharedAuthentication"
			elseif authMode == "OPEN" then
				returnStr = "None"
			end

			return returnStr
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			local authMode = getAuthMode(idx)

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if authMode == "OPEN" or authMode == "SHARED" then
				if value == "SharedAuthentication" then
					setAuthMode(idx, "SHARED")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "None" then
					setAuthMode(idx, "OPEN")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end
			end
			return cwmpError.InvalidParameterValue
		end
	},
-- Done
	['**.WLANConfiguration.*.WPAEncryptionModes'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local returnStr = ""
			local authMode = getAuthMode(idx)
			local encrypType = getEncrypType(idx)

			if authMode == "WPAPSK" or authMode == "WPA" or authMode == "WPAPSKWPA2PSK" or authMode == "WPA1WPA2" then
				if encrypType == "TKIP" then
					returnStr = "TKIPEncryption"
				elseif encrypType == "AES" then
					returnStr = "AESEncryption"
				elseif encrypType == "TKIPAES" then
					returnStr = "TKIPandAESEncryption"
				end
			end

			return returnStr
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			local authMode = getAuthMode(idx)

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if authMode == "WPAPSK" or authMode == "WPA" or authMode == "WPAPSKWPA2PSK" or authMode == "WPA1WPA2" then
				if value == "TKIPEncryption" then
					setEncrypType(idx, "TKIP")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "AESEncryption" then
					setEncrypType(idx, "AES")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "TKIPandAESEncryption" then
					setEncrypType(idx, "TKIPAES")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end
			end
			return cwmpError.InvalidParameterValue
		end
	},
-- TODO:: Set function: NVITEM-AuthMode
	['**.WLANConfiguration.*.WPAAuthenticationMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local returnStr = ""
			local authMode = getAuthMode(idx)

			if authMode == "WPAPSK" or authMode == "WPAPSKWPA2PSK" then
				returnStr = "PSKAuthentication"
			elseif authMode == "WPA" or authMode == "WPA1WPA2" then
				returnStr = "EAPAuthentication"
			end

			return returnStr
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			local authMode = getAuthMode(idx)

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if authMode == "WPAPSK" or authMode == "WPA" then
				if value == "PSKAuthentication" then
					setAuthMode(idx, "WPAPSK")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "EAPAuthentication" then
					setAuthMode(idx, "WPA")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end
			elseif authMode == "WPAPSKWPA2PSK" or authMode == "WPA1WPA2" then
				if value == "PSKAuthentication" then
					setAuthMode(idx, "WPAPSKWPA2PSK")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "EAPAuthentication" then
					setAuthMode(idx, "WPA1WPA2")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end
			end
			return cwmpError.InvalidParameterValue
		end
	},
	['**.WLANConfiguration.*.IEEE11iEncryptionModes'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local returnStr = ""
			local authMode = getAuthMode(idx)
			local encrypType = getEncrypType(idx)

			if authMode == "WPA2PSK" or authMode == "WPA2" or authMode == "WPAPSKWPA2PSK" or authMode == "WPA1WPA2" then
				if encrypType == "TKIP" then
					returnStr = "TKIPEncryption"
				elseif encrypType == "AES" then
					returnStr = "AESEncryption"
				elseif encrypType == "TKIPAES" then
					returnStr = "TKIPandAESEncryption"
				end
			end

			return returnStr
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			local authMode = getAuthMode(idx)

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if authMode == "WPA2PSK" or authMode == "WPA2" or authMode == "WPAPSKWPA2PSK" or authMode == "WPA1WPA2" then
				if value == "TKIPEncryption" then
					setEncrypType(idx, "TKIP")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "AESEncryption" then
					setEncrypType(idx, "AES")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "TKIPandAESEncryption" then
					setEncrypType(idx, "TKIPAES")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end
			end
			return cwmpError.InvalidParameterValue
		end
	},
-- TODO:: Set function: NVITEM-AuthMode
	['**.WLANConfiguration.*.IEEE11iAuthenticationMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local returnStr = ""
			local authMode = getAuthMode(idx)

			if authMode == "WPA2PSK" or authMode == "WPAPSKWPA2PSK"  then
				returnStr = "PSKAuthentication"
			elseif authMode == "WPA2" or authMode == "WPA1WPA2" then
				returnStr = "EAPAuthentication"
			end

			return returnStr
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			local authMode = getAuthMode(idx)

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if authMode == "WPA2PSK" or authMode == "WPA2" then
				if value == "PSKAuthentication" then
					setAuthMode(idx, "WPA2PSK")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "EAPAuthentication" then
					setAuthMode(idx, "WPA2")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end
			elseif authMode == "WPAPSKWPA2PSK" or authMode == "WPA1WPA2" then
				if value == "PSKAuthentication" then
					setAuthMode(idx, "WPAPSKWPA2PSK")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				elseif value == "EAPAuthentication" then
					setAuthMode(idx, "WPA1WPA2")
					dimclient.callbacks.register('cleanup', basic_cleanup_cb)
					return 0
				end
			end
			return cwmpError.InvalidParameterValue
		end
	},
-- Done:: NVITEM- CountryCode
	['**.WLANConfiguration.*.PossibleChannels'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local min, max = possiblechannel()
			local resultStr=""

			if min == nil or max == nil then
				resultStr=""
			else
				resultStr= min .. "-" .. max
			end

			return resultStr
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- TODO: Deleted
	['**.WLANConfiguration.*.BasicDataTransmitRates'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- TODO: Deleted
	['**.WLANConfiguration.*.OperationalDataTransmitRates'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- TODO: Deleted
	['**.WLANConfiguration.*.PossibleDataTransmitRates'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Done
	['**.WLANConfiguration.*.BeaconAdvertisementEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local retVal = getItem(idx, 'HideSSID')

			if retVal == "1" then
				return "0"
			else
				return "1"
			end
		end,
		set = function(node, name, value)
			local cmd = ""
			local setValue = ""

			if value == "1" then
				setValue = "0"
			elseif value == "0" then
				setValue = "1"
			else
				return cwmpError.InvalidParameterValue
			end

			setAllItems('HideSSID', setValue)
			dimclient.callbacks.register('cleanup', BeaconAd_cb)
			return 0
		end
	},
-- Done
	['**.WLANConfiguration.*.RadioEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('RadioOff')

			if retVal == "1" then
				return "0"
			else
				return "1"
			end
		end,
		set = function(node, name, value)

			if value == "1" or value == "0" then
				if value == "1" then
					luanvramDB.set('RadioOff', "0")
				else
					luanvramDB.set('RadioOff', "1")
				end
				dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			end
			return 0
		end
	},
-- TELUS Only
	['**.WLANConfiguration.*.TransmitPower'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('TxPower')

			if retVal == nil then return "0" end
			retVal = string.gsub(retVal:gsub("^%s+", ""), "%s+$", "")

			if tonumber(retVal) > 0 and tonumber(retVal) <= 100 then
				return retVal
			else
				return "0"
			end
		end,
		set = function(node, name, value)

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			local numVal = tonumber(value)

			if numVal == nil then return cwmpError.InvalidParameterValue end

			
			if numVal > 0 and numVal <= 100 then
				luanvramDB.set('TxPower', value)
				os.execute('iwpriv ra0 set TxPower=' .. value)
				return 0
			else
				return cwmpError.InvalidParameterValue
			end
		end
	},
-- Delete
	['**.WLANConfiguration.*.AutoRateFallBackEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- TODO
	['**.WLANConfiguration.*.TotalBytesSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if idx < 1  or tonumber(numOfBssid) < idx then return 0 end

			local retVal = get_wirelessStat()

			retVal = retrieve_lastStringBlock(retVal, '%-%- IF%-ra'.. (idx-1) ..' %-%-', '%-%- IF%-ra'.. (idx-1) ..' end %-%-')

			if retVal == nil then return 0 end

			retVal = string.match(retVal:match("Byte Sent = %d+"), "%d+")

			if retVal == nil then
				return 0
			else
				return retVal
			end

		end,
		set = cwmpError.funcs.ReadOnly
	},
-- TODO
	['**.WLANConfiguration.*.TotalBytesReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if idx < 1  or tonumber(numOfBssid) < idx then return "0" end

			local retVal = get_wirelessStat()

			retVal = retrieve_lastStringBlock(retVal, '%-%- IF%-ra'.. (idx-1) ..' %-%-', '%-%- IF%-ra'.. (idx-1) ..' end %-%-')

			if retVal == nil then return "0" end

			retVal = string.match(retVal:match("Bytes Received = %d+"), "%d+")

			if retVal == nil then
				return "0"
			else
				return retVal
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- TODO
	['**.WLANConfiguration.*.TotalPacketsSent'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if idx < 1  or tonumber(numOfBssid) < idx then return "0" end

			local retVal = get_wirelessStat()

			retVal = retrieve_lastStringBlock(retVal, '%-%- IF%-ra'.. (idx-1) ..' %-%-', '%-%- IF%-ra'.. (idx-1) ..' end %-%-')

			if retVal == nil then return "0" end

			retVal = string.match(retVal:match("Packets Sent = %d+"), "%d+")

			if retVal == nil then
				return "0"
			else
				return retVal
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- TODO
	['**.WLANConfiguration.*.TotalPacketsReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if idx < 1  or tonumber(numOfBssid) < idx then return "0" end

			local retVal = get_wirelessStat()

			retVal = retrieve_lastStringBlock(retVal, '%-%- IF%-ra'.. (idx-1) ..' %-%-', '%-%- IF%-ra'.. (idx-1) ..' end %-%-')

			if retVal == nil then return "0" end

			retVal = string.match(retVal:match("Packets Received = %d+"), "%d+")

			if retVal == nil then
				return "0"
			else
				return retVal
			end
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.WLANConfiguration.*.TotalAssociations'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local forwarding = findNode(paramRoot, 'InternetGatewayDevice.LANDevice.' .. pathBits[3] .. '.WLANConfiguration.' .. pathBits[5] .. '.AssociatedDevice')
			local retVal = forwarding:countInstanceChildren()
			if retVal == nil then return 0 end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end
			return retVal
		end,
		set = cwmpError.funcs.ReadOnly
	},

	['**.WLANConfiguration.*.AssociatedDevice'] = {
		init = function(node, name, value)

			local pathBits = name:explode('.')

			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if idx < 1  or tonumber(numOfBssid) < idx then return "" end

-- 			dimclient.log('debug', 'Wireless AssociatedDevice init')

			local maxInstanceId = 0

			if DeviceTbl == nil then init_DeviceTbl() end

			for id, v in ipairs(DeviceTbl[idx]) do
				local instance = node:createDefaultChild(id)
				if id > maxInstanceId then maxInstanceId = id end
			end
			node.instance = maxInstanceId
			return 0
		end,
		create = function(node, name, instanceId)
			dimclient.log('debug', 'wifi station create')
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('debug', 'wifi station delete, name = ' .. name)
			node.parent:deleteChild(node)
			return 0
		end,
		poll = wifistationWatcher
	},

	['**.WLANConfiguration.*.AssociatedDevice.*']  = {
		create = function(node, name, instanceId)
			dimclient.log('debug', 'Wireless AssociatedDevice create')
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('debug', 'Wireless AssociatedDevice delete, name = ' .. name)
			node.parent:deleteChild(node)
			return 0
		end,
		
	},

	['**.WLANConfiguration.*.AssociatedDevice.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')

			local ssididx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if ssididx < 1  or tonumber(numOfBssid) < ssididx then return "" end

			local objidx=tonumber(pathBits[7])
			local paraname=pathBits[8]

			if objidx > #DeviceTbl[ssididx] then return cwmpError.InternalError end

			-- strictly there should only ever be one!
			if paraname == 'AssociatedDeviceMACAddress' then
				node.value = getItemOfTbl(ssididx, objidx, "macAddr")
			elseif paraname == 'AssociatedDeviceIPAddress' then
				node.value = getItemOfTbl(ssididx, objidx, "ipAddr")
			elseif paraname == 'AssociatedDeviceAuthenticationState' then
				if getAuthMode(ssididx) == "OPEN" then
					node.value = '0'
				else
					node.value = '1'
				end
			else
				error('Dunno how to handle ' .. name)
			end

			return node.value
		end,
		set = cwmpError.funcs.ReadOnly
	},

-- Done
	['**.WLANConfiguration.*.PreSharedKey.*.PreSharedKey'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local retVal = getItem(idx, 'RADIUS_Key')

			if retVal == nil then
				return ""
			end
			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			setItem(idx, 'RADIUS_Key', value)
			dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			return 0
		end
	},
-- Done: Mulit
	['**.WLANConfiguration.*.PreSharedKey.*.KeyPassphrase'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local retVal = luanvramDB.get('WPAPSK' .. idx)

			if retVal == nil then
				return ""
			end
			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			luanvramDB.set('WPAPSK' .. idx, value)
			dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			return 0
		end
	},
-- Done: Multi
	['**.WLANConfiguration.*.WEPKey.*.WEPKey'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return "" end

			local key = "Key" ..pathBits[7].. "Str" .. idx
			local keytype = "Key" ..pathBits[7].. "Type"
			local retVal = luanvramDB.get(key)
			local ishex = getItem(idx, keytype)  --> 0 is hex, 1 is Ascii

			if ishex == "1" then
				retVal = ascii2hexdecimal(retVal)
			end

			if retVal == nil then
				return ""
			end

			if #retVal ~= 10 and #retVal ~= 26 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numOfBssid = luanvramDB.get('BssidNum')

			if tonumber(numOfBssid) < idx then return cwmpError.InvalidParameterValue end

			local key = "Key" ..pathBits[7].. "Str" .. idx
			local keytype = "Key" ..pathBits[7].. "Type"
			local ishex = getItem(idx, keytype)  --> 0 is hex, 1 is Ascii

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if #value ~= 10 and #value ~= 26 then return cwmpError.InvalidParameterValue end

			local temp = value:find("[^%x]+")

			if temp == nil then
				if ishex == "1" then setItem(idx, keytype, "0") end
				luanvramDB.set(key, value)
				dimclient.callbacks.register('cleanup', basic_cleanup_cb)
			else
				return cwmpError.InvalidParameterValue
			end
			return 0
		end
	},
	['InternetGatewayDevice.LANConfigSecurity.ConfigPassword'] = {
		init = function(node, name, value) return 0 end,
		get = cwmpError.funcs.WriteOnly,
		set = function(node, name, value)
			local old_user=luanvramDB.get("Login")
			
			if string.gsub(value,"%s+", "") == "" then return cwmpError.InvalidParameterValue end

			luanvramDB.set("Password", value)
			luanvramDB.commit()

			luardb.set("admin.user."..old_user, value)
			os.execute("chpasswd.sh "..old_user.." "..value)
			return 0
		end
	},
}
--]]

--[[
return {
	['**'] = {
	}
}
--]]