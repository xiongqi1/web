require('CWMP.Error')

local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Networking.DialOnDemand.'

------------------local function prototype------------------
local convertInternalBoolean
local getUniqList
local isAvailableValue
local traverseRdbVariable
local dod_Trigger_cb
local isChanged
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

getUniqList = function (listTbl)
	if not listTbl or type(listTbl) ~= 'table' then return {} end

	local uniqValue = nil
	local sortedTbl = {}
	local uniqTbl = {}

	for _, v in ipairs(listTbl) do
		local isNum = tonumber(v)
		if isNum then
			table.insert(sortedTbl, tonumber(v))
		end
	end

	table.sort(sortedTbl, function (a,b) return (a < b)end)

	for i, v in ipairs(sortedTbl) do
		if v ~= uniqValue then
			table.insert(uniqTbl, v)
			uniqValue = v
		end
	end

	return uniqTbl
end

local avaliableValueTbl = {
	KeepOnlineTimer		= {1 ,2 ,3 ,5 ,10 ,15 ,20 ,25 ,30 ,35 ,45 ,60},
	MinOnlineTimer		= {1 ,2 ,3 ,5 ,10 ,15 ,20 ,25 ,30 ,35 ,45 ,60},
	DialDelayTimer		= {0 ,3 ,5 ,10 ,15 ,20 ,25 ,30 ,35 ,45 ,60 ,120 ,180 ,300},
	DeactivationTimer	= {0 ,1 ,2 ,3 ,5 ,10 ,15 ,20 ,25 ,30 ,35 ,45 ,60},
	PeriodicOnlineTimer	= {0 ,1 ,2 ,3 ,5 ,10 ,15 ,20 ,25 ,30 ,35 ,45 ,60 ,120 ,180 ,240 ,300 ,360 ,720},
	PeriodicOnlineRandomTimer	= {0, 1 ,2 ,3 ,5 ,10 ,15 ,20 ,25 ,30 ,35 ,45 ,60}
}

isAvailableValue = function (key, value)
	if not key or not value then return false end
	if not avaliableValueTbl[key] then return false end
	if not tonumber(value) then return false end

	local numValue = tonumber(value)

	for _, v in ipairs(avaliableValueTbl[key]) do
		if numValue == v then return true end
	end

	return false
end

-- usage: traverseRdbVariable{prefix='service.firewall.dnat', suffix=, startIdx=0}
-- If startIdx is nil, then default value is 1
traverseRdbVariable = function  (arg)
	local i = arg.startIdx or 1;
	local cmdPrefix, cmdSuffix

	cmdPrefix = arg.prefix and arg.prefix .. '.' or ''
	cmdSuffix = arg.suffix and '.' .. arg.suffix or ''
		
	return (function ()
		local index = i
		local v = luardb.get(cmdPrefix .. index .. cmdSuffix)
		i= i + 1
		if v then 
			return index, v
		end
	end)
end

local cb_enable_DOD = nil
local cb_IgnoreNCSI = nil
local cb_PeriodicOnlineTimer = nil
local cb_PeriodicOnlineRandomTimer = nil
local cb_KeepOnlineTimer = nil
local cb_MinOnlineTimer = nil
local cb_SelectDoDProfile = nil

dod_Trigger_cb = function ()

	-- KeepOnlineTimer should be less than or equal MinOnlineTimer
	if cb_KeepOnlineTimer or cb_MinOnlineTimer  then
		local currentKeepOT = luardb.get('dialondemand.traffic_online')
		local currentMinOT  = luardb.get('dialondemand.min_online')

		currentKeepOT = tonumber(currentKeepOT)
		currentMinOT = tonumber(currentMinOT)

		local changedKeepOT =  cb_KeepOnlineTimer or currentKeepOT or 20
		local changedMinOT =  cb_MinOnlineTimer or currentMinOT or 20

		if changedKeepOT > changedMinOT then 
			changedMinOT = changedKeepOT
		end
		luardb.set('dialondemand.traffic_online', changedKeepOT)
		luardb.set('dialondemand.min_online', currentMinOT)
	end

	-- PeriodicOnlineRandomTimer should be less than PeriodicOnlineTimer.
	if cb_PeriodicOnlineRandomTimer or cb_PeriodicOnlineTimer then

		local currentPOTimer = luardb.get('dialondemand.periodic_online')
		local currentPORTimer = luardb.get('dialondemand.periodic_online_random')

		currentPOTimer = tonumber(currentPOTimer)
		currentPORTimer = tonumber(currentPORTimer)

		local changedPOTimer =  cb_PeriodicOnlineTimer or currentPOTimer or 0
		local changedPORTimer =  cb_PeriodicOnlineRandomTimer or currentPORTimer or 30

		if changedPOTimer > 1 and changedPORTimer >= changedPOTimer then
			local ranTbl = avaliableValueTbl.PeriodicOnlineRandomTimer
			for i=#ranTbl, 1, -1 do
				if changedPOTimer > ranTbl[i] then
					changedPORTimer = ranTbl[i]
					break;
				end
			end
		end
		luardb.set('dialondemand.periodic_online', changedPOTimer)
		luardb.set('dialondemand.periodic_online_random', changedPORTimer)
	end

	if cb_IgnoreNCSI then
		luardb.set('dialondemand.ignore_win7', cb_IgnoreNCSI)
	end

	if cb_SelectDoDProfile then
		luardb.set('dialondemand.profile', cb_SelectDoDProfile)
	end

	if cb_enable_DOD then
		luardb.set('dialondemand.enable', cb_enable_DOD)
	else
		local enable_DOD=luardb.get('dialondemand.enable')

		if enable_DOD then
			luardb.set('dialondemand.enable', enable_DOD)
		end
	end

-- 	for i, value in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
-- 		local dev=luardb.get('link.profile.' .. i .. '.dev')
-- 		local isWWAN = dev and dev:match('wwan\.%d+') or nil
-- 
-- 		if not isWWAN then break end
-- 
-- 		if value == '1' then
-- 			luardb.set('link.profile.' .. i .. '.enable', 0)
-- 			os.execute('sleep 2')
-- 			luardb.set('link.profile.' .. i .. '.enable', 1)
-- 			break
-- 		end
-- 	end

	luardb.set('dialondemand.trigger', '1')

	cb_enable_DOD = nil
	cb_IgnoreNCSI = nil
	cb_PeriodicOnlineTimer = nil
	cb_PeriodicOnlineRandomTimer = nil
	cb_KeepOnlineTimer = nil
	cb_MinOnlineTimer = nil
	cb_SelectDoDProfile = nil
end

isChanged = function (name, newVal)
	local prevVal = luardb.get(name)

	if not prevVal then return true end

	if tostring(prevVal) == tostring(newVal) then return false end

	return true
end

return {

	[subROOT .. 'Enable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.enable')

			if result and result == '1' then 
				return 0, '1'
			else
				return 0, '0'
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.enable', internalBool) then return 0 end

			cb_enable_DOD = internalBool

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- readonly
-- Return: Online/Offline/Disabled
-- 	[subROOT .. 'Status'] = {
-- 		init = function(node, name, value) return 0 end,
-- 		get = function(node, name)
-- 			local dod_enabled = luardb.get('dialondemand.enable')
-- 			local dod_status = luardb.get('dialondemand.status')
-- 
-- 			if dod_enabled and dod_enabled == '1' then
-- 				if dod_status and dod_status == '1' then
-- 					return 0, "Online"
-- 				else
-- 					return 0, "Offline"
-- 				end
-- 			else
-- 				return 0, "Disabled"
-- 			end
-- 		end,
-- 		set = function(node, name, value)
-- 			return 0
-- 		end
-- 	},

-- string:readonly
-- Return Available indexes of Dod Profiles separated by a comma (example: 1,2,3,5,6)
-- rdb variable used: "link.profile.{i}.dev" and "link.profile.{i}.enable"
	[subROOT .. 'AvailableDoDProfiles'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = ''

			for i, value in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
				local dev=luardb.get('link.profile.' .. i .. '.dev')
				local isWWAN = dev and dev:match('wwan\.%d+') or nil

				if not isWWAN then break end

				if value == '1' then
					retVal = retVal .. i .. ','
				end
			end
			if retVal == '' then 
				retVal = 'None' 
			else
				retVal = retVal:gsub(',$', '')
			end

			return 0, retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- string:writeonly
-- To activate DoD service on specific Profile, issue GetParameterValues on "AvailableDoDProfiles" parameter to get the newest available DoD profile lists.
-- If activating unavailable DoD profile, DoD service doesn't work properly.
-- rdb variable used: dialondemand.profile
	[subROOT .. 'SelectDoDProfile'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local setVal = string.gsub((value and value:gsub('^%s*', '') or '') , '%s*$', '')

			for i, rdbvalue in traverseRdbVariable{prefix='link.profile', suffix='enable', startIdx=1} do
				local dev=luardb.get('link.profile.' .. i .. '.dev')
				local isWWAN = dev and dev:match('wwan\.%d+') or nil

				if not isWWAN then break end

				if tostring(i) == setVal and rdbvalue == '1' then
					if not isChanged('dialondemand.profile', setVal) then return 0 end

					cb_SelectDoDProfile = setVal

					if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
						client:addTask('postSession', dod_Trigger_cb)
					end
					return 0
				end
			end
			return CWMP.Error.InvalidArguments
		end
	},
-- string:readonly
-- Return the index number of the profile that DoD service is on or 'none'
-- rdb variable used: dialondemand.profile
	[subROOT .. 'ProfileDODEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('dialondemand.profile')
			if not retVal then return 0, 'None' end

			local cmdResult = Daemon.readCommandOutput('diald-ctrl.sh stat')
			local ret, _, status = cmdResult:find('(%w+)%s*')

			if ret and status == 'running' then
				return 0, retVal
			else
				return 0, 'None'
			end
			
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Return: 1/0
	[subROOT .. 'EnablePortFilter'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.ports_en')

			if result and result == '1' then 
				return 0, '1'
			else
				return 0, "0"
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.ports_en', internalBool) then return 0 end

			luardb.set('dialondemand.ports_en', internalBool)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Dial only when traffic appears to the following UDP/TCP destination ports. Multiple ports can be specified with comma-separated.
	[subROOT .. 'PortFilter'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local portList=luardb.get('dialondemand.ports_list')

			if not portList then portList = '' end

			return 0, portList
		end,
		set = function(node, name, value)
			local portList = value:explode(',')

			portList = getUniqList(portList)
			local listStr=table.concat(portList, ',')

			if not listStr then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.ports_list', listStr) then return 0 end

			luardb.set('dialondemand.ports_list', listStr)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},
-- TODO:
-- bool:readwrite
-- Default:0, Available Value: 0|1
-- rdb variable used: dialondemand.dod_verbose_logging
	[subROOT .. 'VerboseLogging'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.dod_verbose_logging')

			if result and result == '1' then 
				return 0, '1'
			else
				return 0, "0"
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.dod_verbose_logging', internalBool) then return 0 end

			luardb.set('dialondemand.dod_verbose_logging', internalBool)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},
-- TODO:
-- string:writeonly
-- Available Value: 0(Manual disconnect)|1(Manual connect)
-- Command-Line Command used: diald-ctrl.sh "up" and diald-ctrl.sh "down"
	[subROOT .. 'ManualConnect'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if internalBool == '1' then
				os.execute('diald-ctrl.sh "up"')
			else
				os.execute('diald-ctrl.sh "down"')
			end
			return 0
		end
	},
-- TODO:
-- string:readonly
-- Available Value: Online|Offline|Disabled
-- rdb variable used: dialondemand.enable and dialondemand.status
	[subROOT .. 'OnlineStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local is_enabled = luardb.get('dialondemand.enable') or '0'

			if is_enabled == '1' then
				local online_status = luardb.get('dialondemand.status') or '0'

				if online_status == '1' then
					return 0, 'Online'
				else
					return 0, 'Offline'
				end
			else
				return 0, 'Disabled'
			end
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- Return: 1/0
	[subROOT .. 'DialTraffic.IgnoreICMP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.ignore_icmp')

			if result and result == '1' then
				return 0, "1"
			else
				return 0, "0"
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.ignore_icmp', internalBool) then return 0 end

			luardb.set('dialondemand.ignore_icmp', internalBool)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Return: 1/0
	[subROOT .. 'DialTraffic.IgnoreTCP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.ignore_tcp')

			if result and result == '1' then
				return 0, "1"
			else
				return 0, "0"
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.ignore_tcp', internalBool) then return 0 end

			luardb.set('dialondemand.ignore_tcp', internalBool)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Return: 1/0
	[subROOT .. 'DialTraffic.IgnoreUDP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.ignore_udp')

			if result and result == '1' then
				return 0, "1"
			else
				return 0, "0"
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.ignore_udp', internalBool) then return 0 end

			luardb.set('dialondemand.ignore_udp', internalBool)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Return: 1/0
	[subROOT .. 'DialTraffic.IgnoreDNS'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.ignore_dns')

			if result and result == '1' then
				return 0, '1'
			else
				return 0, '0'
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.ignore_dns', internalBool) then return 0 end

			luardb.set('dialondemand.ignore_dns', internalBool)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Return: 1/0
	[subROOT .. 'DialTraffic.IgnoreNTP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.ignore_ntp')

			if result and result == '1' then
				return 0, "1"
			else
				return 0, "0"
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.ignore_ntp', internalBool) then return 0 end

			luardb.set('dialondemand.ignore_ntp', internalBool)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Return: 1/0
	[subROOT .. 'DialTraffic.IgnoreNCSI'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.ignore_win7')

			if result and result == '1' then
				return 0, "1"
			else
				return 0, "0"
			end
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.ignore_win7', internalBool) then return 0 end

			cb_IgnoreNCSI = internalBool

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Keep online for this period of time when traffic appears
-- Available value: 1/2/3/5/10/15/20/25/30/35/45/60, unit:minutes
-- set in callback function - This value has to be less than or equal MinOnlineTimer
	[subROOT .. 'Timer.KeepOnlineTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.traffic_online')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local available = isAvailableValue("KeepOnlineTimer", value)
			if not available then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.traffic_online', value) then return 0 end

			cb_KeepOnlineTimer = tonumber(value)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Do not attempt to hang up for this period of time after dial
-- Available value: 1/2/3/5/10/15/20/25/30/35/45/60, unit:minutes
-- set in callback function - This value has to be more than or equal KeepOnlineTimer
	[subROOT .. 'Timer.MinOnlineTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.min_online')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local available = isAvailableValue("MinOnlineTimer", value)
			if not available then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.min_online', value) then return 0 end

			cb_MinOnlineTimer = tonumber(value)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Do not attempt to dial for this period of time after hang-up
-- Available value: 0(immediately)/3/5/10/15/20/25/30/35/45/60/120/180/300, unit:seconds
	[subROOT .. 'Timer.DialDelayTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.dial_delay')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local available = isAvailableValue("DialDelayTimer", value)
			if not available then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.dial_delay', value) then return 0 end

			luardb.set('dialondemand.dial_delay', tonumber(value))

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},

-- Force to hang up after this period of time regardless of traffic
-- Available value: 0(never)/1/2/3/5/10/15/20/25/30/35/45/60, unit:minutes
	[subROOT .. 'Timer.DeactivationTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.deactivation_timer')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local available = isAvailableValue("DeactivationTimer", value)
			if not available then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.deactivation_timer', value) then return 0 end

			luardb.set('dialondemand.deactivation_timer', tonumber(value))

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},
-- TODO
-- Dial every this period of time
-- Available value: 0(never)/1/2/3/5/10/15/20/25/30/35/45/60/120/180/240/300/360/720, units: minutes
-- set in callback function
	[subROOT .. 'Timer.PeriodicOnlineTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.periodic_online')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local available = isAvailableValue("PeriodicOnlineTimer", value)
			if not available then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.periodic_online', value) then return 0 end

			cb_PeriodicOnlineTimer = tonumber(value)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},
-- TODO
-- Randomize dial starting time
-- Available value: 0(never)/1/2/3/5/10/15/20/25/30/35/45/60, unit:minutes
-- set in callback function - This value has to be less than PeriodicOnlineTimer.
	[subROOT .. 'Timer.PeriodicOnlineRandomTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('dialondemand.periodic_online_random')

			if not result then return CWMP.Error.InternalError end
			return 0, result
		end,
		set = function(node, name, value)
			local available = isAvailableValue("PeriodicOnlineRandomTimer", value)
			if not available then return CWMP.Error.InvalidArguments end

			if not isChanged('dialondemand.periodic_online_random', value) then return 0 end

			cb_PeriodicOnlineRandomTimer = tonumber(value)

			if client:isTaskQueued('postSession', dod_Trigger_cb) ~= true then
				client:addTask('postSession', dod_Trigger_cb)
			end
			return 0
		end
	},
}
