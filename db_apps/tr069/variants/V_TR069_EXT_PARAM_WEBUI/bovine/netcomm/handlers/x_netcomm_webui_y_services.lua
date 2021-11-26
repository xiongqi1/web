 
local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Services.'

------------------local function prototype------------------
local convertInternalBoolean
local convertInternalInteger
local isChanged
local pReboot_cb
local heartbeat_once_cb
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

local avaliableValueTbl = {
	RebootRandomTimer	= {1, 2, 3, 5, 10, 15, 20, 25, 30, 35, 45, 60}
}

local cb_value_reboot_time = nil
local cb_value_rndmin = nil

pReboot_cb = function ()

	if cb_value_rndmin or cb_value_reboot_time then

		local currentRebootTimer = luardb.get('service.systemmonitor.forcereset')
		local currentRandTimer = luardb.get('service.systemmonitor.forcereset.rndmin')

		currentRebootTimer = currentRebootTimer and tonumber(currentRebootTimer) or nil
		currentRandTimer = currentRandTimer and tonumber(currentRandTimer) or nil

		local changedRebootTimer =  cb_value_reboot_time or currentRebootTimer or 0
		local changedRandTimer =  cb_value_rndmin or currentRandTimer or 0

		if changedRebootTimer > 1 and changedRandTimer >= changedRebootTimer then
			local ranTbl = avaliableValueTbl.RebootRandomTimer
			for i=#ranTbl, 1, -1 do
				if changedRebootTimer > ranTbl[i] then
					changedRandTimer = ranTbl[i]
					break;
				end
			end
		end
		luardb.set('service.systemmonitor.forcereset', changedRebootTimer)
		luardb.set('service.systemmonitor.forcereset.rndmin', changedRandTimer)
	end

cb_value_reboot_time = nil
cb_value_rndmin = nil
end

heartbeat_once_cb = function ()
	os.execute('/usr/sbin/cdcs_heartbeat once')
end
return {
--[[
-- bool: readwrite
	[subROOT .. 'DynamicDNS.DDNS_Enable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
--]]

-- bool: readwrite
	[subROOT .. 'DynamicDNS.Enable_DDNS'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.ddns.enable')
			if not result or result ~= '1' then 
				return 0, '0'
			end

			return 0, '1'
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if not isChanged('service.ddns.enable', internalBool) then return 0 end

			if internalBool == '1' then
				luardb.set('service.ddns.status', 'Enabled')
			else
				luardb.set('service.ddns.status', 'Disabled')
			end
			luardb.set('service.ddns.enable', internalBool)

			return 0
		end
	},

-- uint: readwrite (service.systemmonitor.periodicpingtimer)
-- (0=disable, 300-65535) secs
	[subROOT .. 'SystemMonitor.PeriodicPingSettings.Periodic_PING_Timer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.systemmonitor.periodicpingtimer')

			if not result or result == '0' or result == '' then return 0, '0' end

			result = convertInternalInteger{input=result, minimum=300, maximum=65535}

			if not result then return 0, '0' end

			return 0, tostring(result)
		end,
		set = function(node, name, value)
			local min = 300
			local max = 65535

			local setVal = convertInternalInteger{input=value, minimum=0, maximum=max}
			if not setVal then return CWMP.Error.InvalidParameterValue end
			if setVal > 0 and setVal < min then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.systemmonitor.periodicpingtimer', setVal) then return 0 end

			luardb.set('service.systemmonitor.periodicpingtimer', setVal)
			return 0
		end
	},
-- uint: readwrite (service.systemmonitor.pingacceleratedtimer)
-- (0=disable, 60-65535) secs
	[subROOT .. 'SystemMonitor.PeriodicPingSettings.Periodic_PING_Acc_Timer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.systemmonitor.pingacceleratedtimer')

			if not result or result == '0' or result == '' then return 0, '0' end

			result = convertInternalInteger{input=result, minimum=60, maximum=65535}

			if not result then return 0, '0' end

			return 0, tostring(result)
		end,
		set = function(node, name, value)
			local min = 60
			local max = 65535

			local setVal = convertInternalInteger{input=value, minimum=0, maximum=max}
			if not setVal then return CWMP.Error.InvalidParameterValue end
			if setVal > 0 and setVal < min then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.systemmonitor.pingacceleratedtimer', setVal) then return 0 end

			luardb.set('service.systemmonitor.pingacceleratedtimer', setVal)
			return 0
		end
	},
-- uint: readwrite (service.systemmonitor.failcount)
-- (0=disable, 1-65535) times
	[subROOT .. 'SystemMonitor.PeriodicPingSettings.Fail_Count'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.systemmonitor.failcount')

			if not result or result == '0' or result == '' then return 0, '0' end

			result = convertInternalInteger{input=result, minimum=1, maximum=65535}

			if not result then return 0, '0' end

			return 0, tostring(result)
		end,
		set = function(node, name, value)
			local min = 0
			local max = 65535

			local setVal = convertInternalInteger{input=value, minimum=min, maximum=max}
			if not setVal then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.systemmonitor.failcount', setVal) then return 0 end

			luardb.set('service.systemmonitor.failcount', setVal)
			return 0
		end
	},

-- uint: readwrite (service.systemmonitor.forcereset)
-- (0=disable, 5-65535) mins 
	[subROOT .. 'SystemMonitor.PeriodicReboot.Force_reboot_every'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.systemmonitor.forcereset')

			if not result or result == '0' or result == '' then return 0, '0' end

			result = convertInternalInteger{input=result, minimum=5, maximum=65535}

			if not result then return 0, '0' end

			return 0, tostring(result)
		end,
		set = function(node, name, value)
			local min = 5
			local max = 65535

			local setVal = convertInternalInteger{input=value, minimum=0, maximum=max}
			if not setVal then return CWMP.Error.InvalidParameterValue end
			if setVal > 0 and setVal < min then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.systemmonitor.forcereset', setVal) then return 0 end

			cb_value_reboot_time = setVal

			if client:isTaskQueued('postSession', pReboot_cb) ~= true then
				client:addTask('postSession', pReboot_cb)
			end

-- 			luardb.set('service.systemmonitor.forcereset', setVal)
			return 0
		end
	},

-- uint: readwrite (service.systemmonitor.forcereset.rndmin)
-- Available value: 1/2/3/5/10/15/20/25/30/35/45/60, unit:minutes
	[subROOT .. 'SystemMonitor.PeriodicReboot.Randomize_reboot_time'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.systemmonitor.forcereset.rndmin')

			if not result or result == '' then return 0, '1' end

			result = convertInternalInteger{input=result, minimum=0, maximum=60}

			if not result then return 0, '1' end

			return 0, tostring(result)
		end,
		set = function(node, name, value)
			local min = 0
			local max = 60

			local setVal = convertInternalInteger{input=value, minimum=min, maximum=max}
			if not setVal then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.systemmonitor.forcereset.rndmin', setVal) then return 0 end

			local ranTbl = avaliableValueTbl.RebootRandomTimer
			local vaildRand = false

			for i=#ranTbl, 1, -1 do
				if setVal == ranTbl[i] then
					vaildRand = true
					break;
				end
			end

			if not vaildRand then return CWMP.Error.InvalidParameterValue end

			cb_value_rndmin = setVal

			if client:isTaskQueued('postSession', pReboot_cb) ~= true then
				client:addTask('postSession', pReboot_cb)
			end

-- 			luardb.set('service.systemmonitor.forcereset.rndmin', setVal)
			return 0
		end
	},

-- string: readonly
-- /www/cdcs.mib
	[subROOT .. 'SNMP.SNMPConfiguration.SNMP_mib_info'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local file = io.open('/www/snmp.mib', 'r')
			if not file then return 0, 'N/A' end

			local mibInfo = file:read("*all")
			if not mibInfo then return 0, 'N/A' end

			return 0, mibInfo
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- TODO: Check validation on ip address
-- string: readwrite - (service.snmp.snmp_trap_dest - heartbeat.template)
-- only ip address type is available
	[subROOT .. 'SNMP.SNMPTraps.Trap_Destination'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.snmp.snmp_trap_dest')

			result = result and string.gsub(result, "^%s*(.-)%s*$", "%1") or ''

			if not Parameter.Validator.isValidIP4(result) then return 0, '' end

			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and string.gsub(value, "^%s*(.-)%s*$", "%1") or ''

			if not Parameter.Validator.isValidIP4(setVal) then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.snmp.snmp_trap_dest', setVal) then return 0 end

			luardb.set('service.snmp.snmp_trap_dest', setVal)
			return 0
		end
	},
-- TODO: 
-- uint: readwrite - (service.snmp.heartbeat_interval - heartbeat.template)
-- Should be bigger than or equal to 0
	[subROOT .. 'SNMP.SNMPTraps.Heartbeat_Interval'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.snmp.heartbeat_interval')
			result = result and result or '0'
			if result == '' then result = '0' end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = convertInternalInteger{input=value, minimum=0}
			if not setVal then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.snmp.heartbeat_interval', setVal) then return 0 end

			if setVal == 0 then setVal = '' end
			luardb.set('service.snmp.heartbeat_interval', setVal)
			return 0
		end
	},
-- TODO: 
-- uint: readwrite - (service.snmp.trap_persist)
-- Don't need to trigger a template
	[subROOT .. 'SNMP.SNMPTraps.Trap_Persistence_Time'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.snmp.trap_persist')
			result = result and result or '0'
			if result == '' then result = '0' end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = convertInternalInteger{input=value, minimum=0}
			if not setVal then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.snmp.trap_persist', setVal) then return 0 end

			if setVal == 0 then setVal = '' end
			luardb.set('service.snmp.trap_persist', setVal)
			return 0
		end
	},
-- TODO: 
-- uint: readwrite - (service.snmp.trap_resend)
-- Don't need to trigger a template
	[subROOT .. 'SNMP.SNMPTraps.Trap_Retransmission_Time'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.snmp.trap_resend')
			result = result and result or '0'
			if result == '' then result = '0' end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = convertInternalInteger{input=value, minimum=0}
			if not setVal then return CWMP.Error.InvalidParameterValue end

			if not isChanged('service.snmp.trap_resend', setVal) then return 0 end

			if setVal == 0 then setVal = '' end
			luardb.set('service.snmp.trap_resend', setVal)
			return 0
		end
	},
-- TODO: 
-- bool: writeonly
-- /usr/sbin/cdcs_heartbeat once
-- available set vaule = 1
	[subROOT .. 'SNMP.SNMPTraps.Send_Heartbeat_Now'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, '0'
		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			if internalBool == '1' then
				if client:isTaskQueued('postSession', heartbeat_once_cb) ~= true then
					client:addTask('postSession', heartbeat_once_cb)
				end
			else
				return CWMP.Error.InvalidArguments
			end
			return 0
		end
	},
}
