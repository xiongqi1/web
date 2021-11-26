

function zero_pending(length)
	local retVal = ""

	for i=1, length do
		retVal=retVal.."0"
	end

	return retVal
end

function auto3Gbackup_postsession_cb()
	luanvramDB.commit()
	local newOpmode = luanvramDB.get('wwan_opmode')

	if newOpmode == nil or string.gsub(newOpmode,"%s+", "") == "" then return 0 end

	if newOpmode == "Manual" then 
		luardb.set("link.profile.1.enable", "0")
	end

	os.execute("kill -SIGHUP `pidof wwand` 2> /dev/null")
	return 0
end

--------------------------[start]:WWANProfile--------------------------
function init_arrayElement(element)
	element.param_index=0
	element.modified=0
	element.activated=0
	element.valid=0
	element.name=''
	element.user=''
	element.pass=''
	element.readonly=''
	element.dial=''
	element.auth=''
	element.metric=''
	element.APNName=''
end

---------- get table ----------
function get_param_idx_size()
	check_APN_getTBL()

	local count=0
	local local_getTbl = get_APN_getTBL()

	for i, tblitem in pairs(local_getTbl) do
		if tblitem.valid == 1 then count = count+1 end
	end

	return count
end

function get_APN_getTBL()
	return get_table
end

function check_APN_getTBL ()
	if get_table == nil then build_APN_getTBL() end
end

function reset_APN_getTBL_cb()
	get_table=nil
end

function build_APN_getTBL ()

	-- global variable
	get_table={}	--> ranged 1~6

	local paramIdx=1

	for i=1, 6 do
		local validity=1;
		local listItem = luanvramDB.get('wwanProfile'..(i-1))  --> wwanProfile# ranged 0~5
		local templistItem = {} -->  {param_index=0, modified=0, activated=0, valid=0, name="", user="", pass="", readonly="", dial="", auth="", metric="", APNName==""}

		init_arrayElement(templistItem)

		local pasedItem = listItem:explode(',')
		for _ , itemvalue in ipairs(pasedItem) do
			local name, value = itemvalue:match("\"(.*)\":\"(.*)\"")

			if name == 'name' or name == 'user' or name == 'pass' or name == 'readonly' or name == 'dial' or name == 'auth' or name == 'metric' or name == 'APNName' then
				templistItem[name]=value
			else
				validity = 0
			end
		end

		if validity == 1 then
			templistItem.valid = 1
			templistItem.param_index = paramIdx
			paramIdx = paramIdx +1
		end

		table.insert(get_table, templistItem)
	end

	dimclient.callbacks.register('cleanup', reset_APN_getTBL_cb)

	local activatedAPN = luanvramDB.get('wwanProfileIdx') --> ranged 0~5

	activatedAPN = tonumber(activatedAPN)
	if activatedAPN == nil then return 0 end

	get_table[activatedAPN+1].activated = 1

end

---------- set table ----------
function check_APN_setTBL ()
	if set_table == nil then build_APN_setTBL() end
end

function reset_APN_setTBL_cb()
	set_table=nil
end

function get_APN_setTBL()
	return set_table
end

function build_APN_setTBL ()
	check_APN_getTBL()

	-- global variable
	set_table={}	--> ranged 1~6

	local paramIdx=1
	local local_getTbl = get_APN_getTBL()

	if local_getTbl == nil then return 1 end

	for i, tblitem in pairs(local_getTbl) do
		local templistItem = {} -->  {param_index=0, modified=0, activated=0, valid=0, name="", user="", pass="", readonly="", dial="", auth="", metric="", APNName==""}

		init_arrayElement(templistItem)

		for name, value in pairs(tblitem) do
			templistItem[name] = value
		end

		table.insert(set_table, templistItem)
	end

	dimclient.callbacks.register('cleanup', reset_APN_setTBL_cb)
end

---------- orig table ----------
function check_APN_origTBL ()
	if orig_table == nil then build_APN_origTBL() end
end

function reset_APN_origTBL_cb()
	orig_table=nil
end

function rdb_set_origTBL (tbl)
	local count = 0

	for i, element in ipairs(tbl) do
		local loop_val=""
		count = count + 1
		for name, value in pairs(element) do 
			if loop_val == "" then
				loop_val = '"' .. name .. '":"' .. value .. '"'
			else
				loop_val = loop_val.. ',"' .. name .. '":"' .. value .. '"'
			end
		end

		luardb.set('tr069.saved.3Gprofile'..i, loop_val)
	end

	luardb.set('tr069.saved.numOf3Gprofile', count)
end

function rdb_reset_origTBL ()
	local count = luardb.get('tr069.saved.numOf3Gprofile')

	if count == nil then return 0 end

	count = tonumber(count)
	if count == nil or count < 1 then return 0 end

	luardb.set('tr069.saved.numOf3Gprofile', '0')

	for i=1, count do
		luardb.unset('tr069.saved.3Gprofile'..i)
	end


end

function build_and_save_APN_origTBL ()
	check_APN_getTBL()

	-- global variable
	orig_table={}	--> ranged 1~6

	local paramIdx=1
	local local_GetTbl = get_APN_getTBL()
	local local_SetTbl = get_APN_setTBL()

	if local_GetTbl == nil or local_SetTbl == nil then return 1 end

	for i, tblitem in pairs(local_GetTbl) do
		local templistItem = {} -->  {param_index=0, modified=0, activated=0, valid=0, name="", user="", pass="", readonly="", dial="", auth="", metric="", APNName==""}

		init_arrayElement(templistItem)

		for name, value in pairs(tblitem) do
			templistItem[name] = value
		end

		templistItem.modified = local_SetTbl[i].modified
		table.insert(orig_table, templistItem)
	end
	rdb_set_origTBL(orig_table)
	dimclient.callbacks.register('cleanup', reset_APN_origTBL_cb)
end

---------- get/set function ----------
--> success => return 0, value, failure => return 1, ""
function get_APNListItem(idx, name)
	check_APN_getTBL()

	local tableIdx=0

	idx = tonumber(idx)

	if idx == nil then 
		dimclient.log('info', 'GET APN List: invalid index not integer')
		return 1, ""
	end

	for i, element in pairs(get_table) do
		if element.param_index == idx then
			tableIdx = i
		end
	end

	if tableIdx == 0 then
		dimclient.log('info', 'GET APN List: invalid index')
		return 1, ""
	end

	if get_table[tableIdx] == nil or get_table[tableIdx][name] == nil then
		dimclient.log('info', 'SET APN List: invalid name')
		return 1, ""
	end

	if name == 'auth' then
		return 0, string.upper(get_table[tableIdx][name])
	end

	return 0, get_table[tableIdx][name]
end


function set_APNListItem(idx, name, value)
	check_APN_setTBL()

	if name == 'wwanProfileIdx' then
		local activatedIdx = tonumber(value)
		if activatedIdx == nil then return 1 end

		if set_table[activatedIdx + 1].activated == 1 then return 0 end

		for i, element in pairs(set_table) do
			if element.param_index == (activatedIdx + 1) then
				if element.activated == 0 then
					set_table[i].activated = 1
					set_table[i].modified = 1
				end
			else
				if element.activated == 1 then
					set_table[i].activated = 0
					set_table[i].modified = 1
				end
			end
		end
		dimclient.callbacks.register('postSession', setAPNnvram_cb)
		return 0
	end

	local tableIdx=0

	idx = tonumber(idx)

	if idx == nil then 
		dimclient.log('info', 'SET APN List: invalid index not integer')
		return 1
	end

	for i, element in pairs(set_table) do
		if element.param_index == idx then
			tableIdx = i
		end
	end

	if tableIdx == 0 then
		dimclient.log('info', 'SET APN List: invalid index')
		return 1
	end

	if set_table[tableIdx] == nil or set_table[tableIdx][name] == nil then
		dimclient.log('info', 'SET APN List: invalid name')
		return 1
	end

	set_table[tableIdx][name] = value
	set_table[tableIdx].modified = 1

	dimclient.callbacks.register('postSession', setAPNnvram_cb)
	return 0
end

function setAPNnvram_cb ()
	check_APN_setTBL()

	local setVal=""
	local valueChanged=0
	local needRollBack=0
	local local_getTbl = get_APN_getTBL()

	for i, element in ipairs(set_table) do
		local tempitem = element

		if tempitem.modified == 1 then
			if local_getTbl[i].activated ~= tempitem.activated then needRollBack = 1 end
			if local_getTbl[i].activated == 1 and tempitem.activated == 1 then needRollBack = 1 end

			setVal = '"name":"'..tempitem.name ..
				'","user":"'..tempitem.user ..
				'","pass":"'..tempitem.pass ..
				'","readonly":"'..tempitem.readonly ..
				'","dial":"'..tempitem.dial ..
				'","auth":"'..tempitem.auth ..
				'","metric":"'..tempitem.metric ..
				'","APNName":"'..tempitem.APNName ..'"'

			luanvramDB.set('wwanProfile'..(i-1), setVal)

			if tempitem.activated == 1 then
				luanvramDB.set('wwanProfileIdx', (i-1))

				luanvramDB.set('wwan_APN', tempitem.APNName)
				luanvramDB.set('Dial', tempitem.dial)
				luanvramDB.set('wwan_auth', tempitem.auth)
				luanvramDB.set('wwan_user', tempitem.user)
				luanvramDB.set('wwan_pass', tempitem.pass)
				luanvramDB.set('wwan_metric', tempitem.metric)
			end
			valueChanged = 1
		end
	end

	if valueChanged == 1 then
		if needRollBack == 1 then
			build_and_save_APN_origTBL()
-- 			luardb.watch('link.profile.1.status', nil)
			luardb.watch('link.profile.1.status', wwan_profile_watcher)
		end

		luanvramDB.commit()

		if needRollBack == 1 then
			os.execute("kill -SIGHUP `pidof wwand` 2> /dev/null")
		end
	end
end

function template_trigger()
	luardb.set('tr069.checkvalidity.3Gprofile', '1')
end

function wwan_profile_watcher()
	local status = luardb.get('link.profile.1.status')
	dimclient.log('info', 'wwan_profile_watcher status='..status)

	if status == nil then return 1 end

	if status == 'down' then 
		template_trigger()
		luardb.watch('link.profile.1.status', nil)
	elseif status == 'up' then
		rdb_reset_origTBL()
		luardb.watch('link.profile.1.status', nil)
	end
end
--------------------------[ end ]:WWANProfile--------------------------

return {
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.IMEI'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.imei')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.Manufacturer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.manufacture')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.HardwareVersion'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.hardware_version')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.FirmwareVersion'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.firmware_version')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.Model'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.model')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.CurrentNetwork'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.system_network_status.network.unencoded')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.NetworkAttached'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('wwan.0.system_network_status.attached')

			if retVal == nil then
				return "0"
			elseif retVal == "1" or retVal == "0" then
				return retVal
			else
				return "0"
			end

		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.PDPStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.system_network_status.pdp0_stat')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.CGI'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = ""
			local mcc = luardb.get('wwan.0.imsi.plmn_mcc')
			local mnc = luardb.get('wwan.0.imsi.plmn_mnc')
			local lac = luardb.get('wwan.0.system_network_status.LAC')
			local cellID = luardb.get('wwan.0.system_network_status.CellID')

			if mcc == nil or mnc == nil or lac == nil or cellID == nil then return retVal end
			if mcc == "" or mnc == "" or lac == "" or cellID == "" then return retVal end

			cellID = zero_pending(8-#cellID)..cellID
			retVal = string.format("%s%s %s %s",mcc, mnc, lac, cellID)

			return retVal
		end,
		set = function(node, name, value)
			
			return 0
		end
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.Band'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.system_network_status.service_type')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.ConnectionBearer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return "" --luardb.get('')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.EcIo'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return "" --luardb.get('')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.RSCP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luardb.get('wwan.0.radio.information.signal_strength')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.PLMN'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = ""
			local mcc = luardb.get('wwan.0.imsi.plmn_mcc')
			local mnc = luardb.get('wwan.0.imsi.plmn_mnc')

			if mcc == nil or mnc == nil then return retVal end
			if mcc == "" or mnc == "" then return retVal end

			retVal = string.format("%s%s",mcc, mnc)

			return retVal
		end,
		set = function(node, name, value)
			
			return 0
		end
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.APN'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return luanvramDB.get('wwan_APN')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.Username'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return "" --luardb.get('')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.Password'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return "" --luardb.get('')
		end,
		set = cwmpError.funcs.ReadOnly
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Status.SIMStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local meplock = luardb.get('meplock.status')

			if meplock == "locked" then
				return "MEP-LOCK"
			end

			return luardb.get('wwan.0.sim.status.status')
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.X_NETCOMM_COM.WirelessModem.Status.NetworkLock'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local meplock = luardb.get('meplock.status')

			meplock = string.gsub(meplock:gsub("^%s+", ""), "%s+$", "")

			if meplock == "locked" then
				return "1"
			elseif meplock == "unlocked" or meplock == "ok" then
				return "0"
			else
				return "0"
			end
		end,
		set = function(node, name, value)
			local meplock = luardb.get('meplock.status')

			meplock = string.gsub(meplock:gsub("^%s+", ""), "%s+$", "")

			if value == "1" and meplock == "locked" then return 0 end
			if value == "0" and meplock == "unlocked" then return 0 end

			if value == "1" then
				luardb.set('meplock.code', 'lock')
			elseif value == "0" then
				local wan_MAC = execute_CmdLine("mac -r wlan")
				local retVal = string.match(wan_MAC, "[%x%p]+")

				if retVal == nil then
					return cwmpError.InternalError
				else
					os.execute("echo " ..retVal.. "_NetworkUnlockCode | passgen %10n > /etc_rw/mepinfo/mepunlock.key")
				end
			else
				return cwmpError.InvalidParameterValue
			end
			return 0
		end
	},
--------------------------[start]:Automatic 3G Backup--------------------------

-- Done
	['**.X_NETCOMM_COM.WirelessModem.Auto3Gbackup.OperationMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local opMode = luanvramDB.get('wwan_opmode')
			local retVal = '0'

			if opMode == nil or string.gsub(opMode,"%s+", "") == "" then return "0" end

			if opMode == "AlwaysOn" then
				retVal = "1"
			elseif opMode == "Manual" then
				retVal = "2"
			elseif opMode == "wanBackup" then
				retVal = "3"
			end

			return retVal
		end,
		set = function(node, name, value)
			local valuename = {"AlwaysOn", "Manual", "wanBackup"}
			local old_opMode=luanvramDB.get('wwan_opmode')

			local setVal = tonumber(value)

			if setVal == nil or setVal < 1 or setVal > 3 then return cwmpError.InvalidParameterValue end

			if valuename[setVal] ~= old_opMode then
				luanvramDB.set('wwan_opmode', valuename[setVal])
				dimclient.callbacks.register('postSession', auto3Gbackup_postsession_cb)
			end
			return 0
		end
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Auto3Gbackup.InternetHost'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('internetHost')

			if retVal == nil or string.gsub(retVal,"%s+", "") == "" then return "" end
			return retVal
		end,
		set = function(node, name, value)
			local old_host=luanvramDB.get('internetHost')

			if value ~= old_host then
				luanvramDB.set('internetHost', value)
				dimclient.callbacks.register('postSession', auto3Gbackup_postsession_cb)
			end
			return 0
		end
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Auto3Gbackup.SecondAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('internetHost2')

			if retVal == nil or string.gsub(retVal,"%s+", "") == "" then return "" end
			return retVal
		end,
		set = function(node, name, value)
			luanvramDB.set('internetHost2', value)
			return 0
		end
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Auto3Gbackup.PingTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('foptimer')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = function(node, name, value)
			local setVal = tonumber(value)

			if setVal == nil or setVal < 0 or setVal > 65535 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('foptimer', setVal)
			return 0
		end
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Auto3Gbackup.PingAccTimer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('fopacctimer')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = function(node, name, value)
			local setVal = tonumber(value)

			if setVal == nil or setVal < 1 or setVal > 65535 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('fopacctimer', setVal)
			return 0
		end
	},
-- Done
	['**.X_NETCOMM_COM.WirelessModem.Auto3Gbackup.FailCount'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('fopfailCnt')

			if retVal == nil then return "0" end
			retVal = tostring(retVal)
			if retVal == nil or not retVal:match('^(%d+)$') then return "0" end

			return retVal
		end,
		set = function(node, name, value)
			local setVal = tonumber(value)

			if setVal == nil or setVal < 1 or setVal > 65535 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('fopfailCnt', setVal)
			return 0
		end
	},
--------------------------[end]:Automatic 3G Backup--------------------------

--------------------------[start]:DeviceStatus--------------------------------
-- TELUS Only
	['**.X_NETCOMM_COM.DeviceStatus.CPUUsage'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = execute_CmdLine('top -n 1 | grep -i "cpu:"')

			if retVal == nil then return "" end

			retVal = retVal:gsub("\n$","")
				return retVal
		end,
		set = function(node, name, value)
				return 0
		end
	},
-- TELUS Only
	['**.X_NETCOMM_COM.DeviceStatus.MEMUsage'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = ""
			local totalMem = execute_CmdLine('cat /proc/meminfo | grep -i "MemTotal:"')
			local freeMem = execute_CmdLine('cat /proc/meminfo | grep -i "MemFree:"')

			if totalMem ~= nil then
				totalMem = totalMem:match("%d+")
				if totalMem ~= nil then
					retVal = "MemTotal: " .. totalMem .. "kB / "
			else
					retVal = "MemTotal: " .. "Read Error!!/"
			end
		end

			if freeMem ~= nil then
				freeMem = freeMem:match("%d+")
				if freeMem ~= nil then
					retVal = retVal .. "MemFree: " .. freeMem .. "kB"
					if tonumber(totalMem) ~= nil and tonumber(freeMem) ~= nil then
						local ratio = tonumber(freeMem)*100 / tonumber(totalMem)
						if ratio ~= nil then
							retVal = string.format(retVal .. "(%0.2f%%)", ratio)
		end
			end
			else
					retVal = retVal .. "MemFree: " .. "Read Error!! "
		end
		end

			return retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},
--------------------------[end]:DeviceStatus----------------------------------

--------------------------[start]:WWANProfile--------------------------
	['**.X_NETCOMM_COM.WWANProfile.ActivatedAPN'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			check_APN_getTBL()

			local local_getTbl = get_APN_getTBL()

			if local_getTbl == nil then return 0 end

			for i, element in pairs(local_getTbl) do
				if element.activated == 1 then
					return tostring(i)
				end
			end
			return '0'
		end,
		set = function(node, name, value)
			local setVal = tonumber(value)
			if setVal == nil or setVal > get_param_idx_size() then return cwmpError.InvalidParameterValue end

			local local_getTbl = get_APN_getTBL()

			if local_getTbl == nil then return 0 end

			for i, element in pairs(local_getTbl) do
				if element.param_index == setVal then 
					set_APNListItem(0, 'wwanProfileIdx', i-1)
					return 0
				end
			end
			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.OperationMode'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local opMode = luanvramDB.get('wwan_opmode')
			local retVal = '0'

			if opMode == nil or string.gsub(opMode,"%s+", "") == "" then return "0" end

			if opMode == "AlwaysOn" then
				retVal = "1"
			elseif opMode == "Manual" then
				retVal = "2"
			elseif opMode == "wanBackup" then
				retVal = "3"
			end

			return retVal
		end,
		set = function(node, name, value)
			local valuename = {"AlwaysOn", "Manual", "wanBackup"}
			local old_opMode=luanvramDB.get('wwan_opmode')

			local setVal = tonumber(value)

			if setVal == nil or setVal < 1 or setVal > 3 then return cwmpError.InvalidParameterValue end

			if valuename[setVal] ~= old_opMode then
				luanvramDB.set('wwan_opmode', valuename[setVal])
				dimclient.callbacks.register('postSession', wwanprofile_postsession_cb)
			end
			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.WWANNAT'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('wwan_NAT')

			if retVal == nil then return "0" end
			if retVal == "0" or retVal == "1" then return retVal end

			return "0"
		end,
		set = function(node, name, value)

			if value == "1" or value == "0" then 
				luanvramDB.set('wwan_NAT', value)
				dimclient.callbacks.register('postSession', wwanprofile_postsession_cb)
			end

			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists'] = {
		init = function(node, name, value) 
			local maxInstanceId = get_param_idx_size()

			if maxInstanceId == nil or maxInstanceId == 0 then return 0 end

			for id=1, maxInstanceId do
				local instance = node:createDefaultChild(id)
			end
			node.instance = maxInstanceId
			return 0
		end,
		create = function(node, name, instanceId)
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			node.parent:deleteChild(node)
			return 0
		end,
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*']  = {
		create = function(node, name, instanceId)
			local instance = node:createDefaultChild(instanceId)
			return 0
		end,
		delete = function(node, name)
			node.parent:deleteChild(node)
			return 0
		end,		
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*.ProfileName'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, isROnly = get_APNListItem(idx, 'readonly')
			if result == 0 and isROnly == "1" then node:setAccess('readonly') end
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, retVal = get_APNListItem(idx, 'name')

			if result == 1 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			if idx == nil then return cwmpError.InvalidParameterName end

			set_APNListItem(idx, 'name', value)
			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*.APN'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, isROnly = get_APNListItem(idx, 'readonly')
			if result == 0 and isROnly == "1" then node:setAccess('readonly') end
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, retVal = get_APNListItem(idx, 'APNName')

			if result == 1 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			if idx == nil then return cwmpError.InvalidParameterName end

			set_APNListItem(idx, 'APNName', value)
			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*.Dial'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, isROnly = get_APNListItem(idx, 'readonly')
			if result == 0 and isROnly == "1" then node.parent:deleteChild(node) end
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, retVal = get_APNListItem(idx, 'dial')

			if result == 1 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			if idx == nil then return cwmpError.InvalidParameterName end

			set_APNListItem(idx, 'dial', value)
			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*.AuthenticationType'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, isROnly = get_APNListItem(idx, 'readonly')
			if result == 0 and isROnly == "1" then node.parent:deleteChild(node) end
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, retVal = get_APNListItem(idx, 'auth')

			if result == 1 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local upperVal= string.upper(value)

			if idx == nil then return cwmpError.InvalidParameterName end
			if upperVal == nil then return cwmpError.InvalidParameterValue end

			if upperVal == 'PAP' or upperVal == 'CHAP' then
				set_APNListItem(idx, 'auth', value)
				return 0
			end

			return cwmpError.InvalidParameterValue
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*.UserName'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, isROnly = get_APNListItem(idx, 'readonly')
			if result == 0 and isROnly == "1" then node.parent:deleteChild(node) end
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, retVal = get_APNListItem(idx, 'user')

			if result == 1 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			if idx == nil then return cwmpError.InvalidParameterName end

			set_APNListItem(idx, 'user', value)
			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*.Password'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, isROnly = get_APNListItem(idx, 'readonly')
			if result == 0 and isROnly == "1" then node.parent:deleteChild(node) end
			return 0
		end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, retVal = get_APNListItem(idx, 'pass')

			if result == 1 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			if idx == nil then return cwmpError.InvalidParameterName end

			set_APNListItem(idx, 'pass', value)
			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*.InterfaceMetric'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, retVal = get_APNListItem(idx, 'metric')

			if result == 1 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])
			local numVal = tonumber(value)

			if idx == nil then return cwmpError.InvalidParameterName end
			if numVal == nil then return cwmpError.InvalidParameterValue end
			if numVal < 0 or numVal > 999 then return cwmpError.InvalidParameterValue end

			set_APNListItem(idx, 'metric', value)
			return 0
		end
	},
	['**.X_NETCOMM_COM.WWANProfile.APNLists.*.ReadonlyList'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local idx = tonumber(pathBits[5])

			local result, retVal = get_APNListItem(idx, 'readonly')

			if result == 1 then return "" end

			return retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},
--------------------------[ end ]:WWANProfile--------------------------
}
