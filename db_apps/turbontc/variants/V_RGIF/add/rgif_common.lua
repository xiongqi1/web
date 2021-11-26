-----------------------------------------------------------------------------------------------------------------------
-- Common mappings for OWA RGIF APIs
--
-- It must add an entry to the 'maps' table for each path.
-- This entry itself is a table that contains:
-- 1. for each HTTP method used a default HTTP status code and an optional Lua handler function
-- 2. A model that describes how to get/set each item in the matching schema
--
-- Copyright (C) 2018 NetComm Wireless limited.
-----------------------------------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------------------------------------------
-- Other required RGIF modules
-----------------------------------------------------------------------------------------------------------------------
local rgif_config = require("rgif_config")
local rgif_utils = require("rgif_utils")
local ota_prefix = "service.fota."
if rgif_config.update_engine == "Netcomm-DM" then
	ota_prefix = "service.dm.fumo."
end
local installtime_rdb
if rgif_config.update_engine == "Netcomm-DM" then
	installtime_rdb = ota_prefix.."dau.reboot_target"
else
	installtime_rdb = ota_prefix.."installtime"
end

-----------------------------------------------------------------------------------------------------------------------
-- Local functions
-----------------------------------------------------------------------------------------------------------------------
--- The neighbour cell structure is too complex for a regular model-based approach.
-- Need to read it all out of RDB in one go while it's locked, then split
-- only some vars into several data points (while ignoring the rest).
local function get_LteNeighbours(self)
	luardb.lock()
	local cells = {}
	local i
	local p_earfcn = luardb.get("wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq")
	local p_pci = luardb.get("wwan.0.radio_stack.e_utra_measurement_report.servphyscellid")
	for i=0, tonumber(luardb.get('wwan.0.cell_measurement.qty') or 0)-1 do
		local rdb = luardb.get('wwan.0.cell_measurement.'..i) or ''
		logDebug("Cell "..i.." is: "..rdb)
		local split = {rdb:match("([^,]*),([^,]*),([^,]*),([^,]*),([^,]*)")}
		if split[1] == 'E' and split[2] == p_earfcn and split[3] == p_pci then
			logDebug("skip serving cell")
		elseif #split >= 5 then
			table.insert(cells, {
				EUTRACarrierARFCN = split[2],
				PhyCellID = split[3],
				RSRP = tostring(tonumber(split[4])),
				RSRQ = tostring(tonumber(split[5]))
			})
		end
	end
	luardb.unlock()
	self:write(cells)
end

local function get_PLMNList(self)
	local ids = {}
	for i=1, tonumber(luardb.get('wwan.0.plmn.plmn_list.num') or "0") do
		local plmnid = luardb.get('wwan.0.plmn.plmn_list.'..i..".plmnid")
		table.insert(ids, {PLMNID=plmnid})
	end
	self:write(ids)
end

-- Given an ipv6 with a prefix length return a prefix/subnetwork ipv6 address
local function ipv6_prefix(ipv6)
	if ipv6 == nil or ipv6 == "" then return "" end
	local _,_,ipv6,prefixBits = ipv6:find("(.*)/(.*)")
	if tonumber(prefixBits) < 1 or tonumber(prefixBits) > 128 then -- Invalid prefix length, return empty
		return ""
	end
	if prefixBits == 128 then -- 128 bits prefix, nothing to do. Just return the whole string
		return ipv6
	end
	local ffi=require("ffi")
	local ts = require("turbo.socket_ffi")
	local in6addr = ffi.new("struct in6_addr")
	-- Convert from string to binary format
	local rc = ffi.C.inet_pton(ts.AF_INET6, ipv6, in6addr)
	if rc ~= 1 then return "" end -- Invalid address, return empty string
	local charData = ffi.cast("char *", in6addr)
	local prefixChars = math.floor(prefixBits/8)
	local remainingBits = prefixBits - prefixChars * 8
	if remainingBits then -- there is part of byte needing to clear
		ffi.fill(charData+prefixChars+1, 16-prefixChars-1, 0) -- Clear the bytes after remainingBits firstly
		mask = bit.bnot(bit.lshift(1, 8-remainingBits) - 1)
		charData[prefixChars] = bit.band(charData[prefixChars], mask) -- Clear part of byte with remainingBits
	else
		ffi.fill(charData+prefixBits/8, 16-prefixChars, 0) -- The prefix length is byte aligned exactly, clear the bytes after prefix directly
	end
	-- Now convert binary format to string
	local INET6_ADDRSTRLEN = #"xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx"+1 -- calculate the longgest ipv6 address length
	local addrbuf = ffi.new("char [?]", INET6_ADDRSTRLEN)
	ffi.C.inet_ntop(ts.AF_INET6, in6addr, addrbuf, INET6_ADDRSTRLEN)
	return ffi.string(addrbuf).."/"..prefixBits
end

local function pdpContextOnOff(self)
	local pdnName = string.upper(self:get_argument("PDN name"))
	-- Only data/EMS/CBRSSAS PDN can be controlled. For user's convenience, case insensitive comparition here
	if pdnName ~= "DATA" and pdnName ~= "EMS" and pdnName ~= "CBRSSAS" then
		self:set_status(400)
		return
	end
	if pdnName == "EMS" and luardb.get('vlan.ems.deny_rg_control') == "1" then
		self:set_status(403)
		return
	end
	if not rgif_config.has_sas and pdnName == "CBRSSAS" then
		self:set_status(400)
		return
	end
	if pdnName == "EMS" and luardb.get('link.profile.4.deny_rg_control') == "1" then
		self:set_status(403)
		return
	end
	local enable = string.upper(self:get_argument("Enable"))
	local index_tbl = rgif_config.apn_indices or {1, 4}
	for _, i in pairs(index_tbl) do
		local profile="link.profile."..i
		if luardb.get(profile..".pdp_function"):upper() == pdnName then
			-- AT&T CBRS/SAS requirement
			-- Do not allow RG enabling/disabling Data/EMS profile directly.
			if rgif_config.has_sas then
				luardb.set(profile..".enabled_by_rg", enable == '0' and "0" or "1")
				logErr(string.format("pdpContextOnOff: Set %s.enabled_by_rg %s", profile, enable == '0' and "0" or "1"))
			else
				luardb.set(profile..".enable", enable == '0' and "0" or "1")
			end
			break
		end
	end
end


local function pdpContextReset(self)
	local pdnName = string.upper(self:get_argument("PDN name"))
	-- For user's convenience, case insensitive comparition here
	if pdnName ~= "DATA" and pdnName ~= "EMS" and
	   pdnName ~= "VOICE" and pdnName ~= "CBRSSAS" then
		self:set_status(400)
		return
	end
	if not rgif_config.has_sas and pdnName == "CBRSSAS" then
		self:set_status(400)
		return
	end
	if not rgif_config.has_voice and pdnName == "VOICE" then
		self:set_status(400)
		return
	end
	if pdnName == "VOICE" then --VOICE reset can't be done through profile reset
		luardb.set("wwan.0.ims.register.command", "reset")
		return
	end
	for i = 1,5 do
		local profile="link.profile."..i
		if luardb.get(profile..".pdp_function"):upper() == pdnName then
			luardb.set(profile..".trigger", "1")
			break
		end
	end
end

-- Calculate the average temperature from the sensors on the MSM die (TSENS)
local function cpu_temperature()
	local sysfile
	local val
	local total = 0
	local sensorNum = 0
	local scaling = tonumber(rgif_config.thermal_zone_scaling) or 1
	for i = rgif_config.thermal_zone_first, rgif_config.thermal_zone_last do
		local file = io.open("/sys/devices/virtual/thermal/thermal_zone"..i.."/temp")
		if file then
			local val = tonumber(file:read('*l') or 0) / scaling
			file:close()
			total = total + val
			sensorNum = sensorNum + 1
			logDebug("Read thermal_zone"..i.."/temp as "..val)
		end
	end
	return string.format("%.1f", total/(sensorNum > 0 and sensorNum or 1))
end

-- This function convert ISIM hex raw data to string
-- First byte is 0x80, second byte is length, the following belong to data payload
local function isim_rawdata_to_str(val)
	local function hex(v) -- convert hex in string to decimal
		return tonumber(v:sub(1,1), 16)*16 + tonumber(v:sub(2,2), 16)
	end
	local function ascii_to_str(ascii)
		return string.format("%c", hex(ascii))
	end
	local data = ""
	if val == nil then return data end
	if val:sub(1,2) ~= "80" then return data end
	local length = hex(val:sub(3,4))
	local start = 5
	while length >0 do
		local c = ascii_to_str(val:sub(start, start+1))
		data = data..c
		start = start + 2
		length = length -1
	end
	return data
end

local function wanStats(field)
	return function(val)
		if val == "wwan down" then return "0" end
		local fmt = ""
		for i=1, 10 do
			if i == field then
				fmt = fmt.."(%d+)"
			else
				fmt = fmt.."%d+"
			end
			if i ~= 10 then
				fmt = fmt..","
			end
		end
		return string.gsub(val, fmt, "%1")
	end
end

-- According to input downlink/uplink EARFCN,
-- calculate Uplink and Downlink freqencies.
local function earfcn_to_freq(dl_earfcn, ul_earfcn)
	-- Currently only band 2 and band 30 are supported. So other conversion params are not listed here.
	local conversion = {
		[2] = {dl_low=1930, dl_offset=600, dl_earfcn={low=600, high=1199}, ul_low=1850, ul_offset=18600},
		[30] = {dl_low=2350, dl_offset=9770, dl_earfcn={low=9770, high=9869}, ul_low=2305, ul_offset=27660},
	}

	local freqs = {}
	if dl_earfcn == nil or ul_earfcn == nil then return freqs end
	for _, param in pairs(conversion) do
		if dl_earfcn >= param.dl_earfcn.low and dl_earfcn <= param.dl_earfcn.high then
			freqs["dl"] = string.format("%d MHz", param.dl_low + 0.1 * (dl_earfcn - param.dl_offset))
			freqs["ul"] = string.format("%d MHz", param.ul_low + 0.1 * (ul_earfcn - param.ul_offset))
		end
	end
	return freqs
end


local function SIMFile(self, filename)
	local res = {}
	local val = ""
	local function getFileContent(rdbname, prefix, record)
		local i = 1
		local val
		local res = ""
		res = prefix.." :"
		while true do
			if record then
				val = luardb.get(string.format("%s.%d", rdbname, i))
				if val and val ~= "" and val ~= "nil" then
					res = res..string.format("[%d] %s,", i, val)
				else
					break
				end
			else
				val = luardb.get(rdbname)
				if val and val ~= "" and val ~= "nil" then
					res = res..string.format("%s", val)
				end
				break
			end
			i = i + 1
		end
		return res
	end
	if filename == "ICCID" then
		val = getFileContent("wwan.0.sim.raw_data.iccid", "EF-ICCID", false)
	elseif filename == "MSISDN" then
		val = getFileContent("wwan.0.sim.raw_data.msisdn", "EF-MSISDN", true)
	elseif filename == "MBDN" then
		val = getFileContent("wwan.0.sim.raw_data.mbdn", "EF-3GPP-MBDN", true)
	elseif filename == "HPPLMN" then
		val = getFileContent("wwan.0.sim.raw_data.hpplmn", "EF-HPPLMN-TIMER", false)
	elseif filename == "LOCI" then
		val = getFileContent("wwan.0.sim.raw_data.loci", "EF-LOCI", false)
	elseif filename == "PSLOCI" then
		val = getFileContent("wwan.0.sim.raw_data.psloci", "EF-PS-LOCI", false)
	elseif filename == "ACC" then
		val = getFileContent("wwan.0.sim.raw_data.acc", "EF-ACC", false)
	elseif filename == "AD" then
		val = getFileContent("wwan.0.sim.raw_data.ad", "EF-ADMIN", false)
	elseif filename == "FPLMN" then
		val = getFileContent("wwan.0.sim.raw_data.fplmn", "EF-FPLMN", false)
	elseif filename == "AHPLMN" then
		val = getFileContent("wwan.0.sim.raw_data.ahplmn", "EF-AHPLMN", false)
	elseif filename == "PNN" then
		val = getFileContent("wwan.0.sim.raw_data.pnn", "EF-PNN", true)
	elseif filename == "OPL" then
		val = getFileContent("wwan.0.sim.raw_data.opl", "EF-OPL", true)
	elseif filename == "OPLMNACT" then
		val = getFileContent("wwan.0.sim.raw_data.oplmnwact", "EF-OPLMNAcT", false)
	elseif filename == "IMPU" then
		val = getFileContent("wwan.0.sim.raw_data.impu", "EF-IMPU", true)
	elseif filename == "PLMNSEL" then
		val = getFileContent("wwan.0.sim.raw_data.plmnsel", "EF-PLMN-SELECTOR", false)
	else
		return self:doError(400, "Unknown data type")
	end
	res["SIM Type"] = "USIM"
	res["Content"] = val
	self:write({result=res})
end

local function LTEBandLock(self, band)

	--
	-- LTEBandLock() supports LTE band group 2 and LTE band group 30 only.
	--
	-- TODO: Band groups are currently shared with all of variants on the Serpent. It will be is required to
	-- introduce platform specific band groups to support variable combination of band groups.
	-- This variable band group supporting will be possible when WMMD2 is ready.
	--

	-- Check whether it is a valid band
	local valid = false
	local bands = luardb.get("wwan.0.module_band_list")

	-- Example of standard band list string format in RDB example
	--
	-- 79,LTE Band 2 - 1900Mhz&a0,LTE Band 30 - 2300Mhz&fb,LTE all
	--

	-- collect LTE bands from modem capability
	logInfo(string.format("[ltebandlock] modem band capability : %s", bands))
	local band_entities = bands:split("&")
	local lte_band_entities={}
	for _,entity in ipairs(band_entities) do
		local band_index,band_name = entity:match("(%x+),(.+)")
		if band_index and band_name then
			local lte_band_index = tonumber(band_name:lower():match("lte band (%d+)"))

			if not lte_band_index and band_name:lower():find("lte all") then
				lte_band_index="LTE all"
			end

			if lte_band_index then
				lte_band_entities[lte_band_index] = {name=band_name,index=band_index}
				logInfo(string.format("[ltebandlock] add LTE band index (lte_band_index=%s,rdb_index=%s,band_name=%s", lte_band_index, band_index, band_name))
			end
		end
	end

	-- conversion table that convert RG API desired band groun names to RDB band group names
	local rdb_band_names={
		["2"] = lte_band_entities[2],
		["30"] = lte_band_entities[30],
		["2+30"] = lte_band_entities["LTE all"],
		["30+2"] = lte_band_entities["LTE all"],
	}

	-- bypass if desired band is not supported
	local rdb_band = rdb_band_names[band]
	if not rdb_band then
		logWarn(string.format("[ltebandlock] desired band not supported (band=%s)",band))
		self:set_status(400)
	end

	-- Lock band now
	logInfo(string.format("[ltebandlock] set desired band (req_band=%s,rdb_band_index=%s,rdb_nad_name=%s)", band, rdb_band.index, rdb_band.name))
	luardb.set("wwan.0.currentband.cmd.param.band", rdb_band.index)
	luardb.set("wwan.0.currentband.cmd.command", "set")
end

local function GetLTEStatus(self, param)
	local ret = {}
	if param == "RRCModeStatus" then
		ret["RRCModeStatus"] = luardb.get("wwan.0.radio_stack.rrc_stat.rrc_stat")
	elseif param == "IMSRegistration" then
		ret["IMSRegistration"] = luardb.get("wwan.0.ims.register.reg_stat")
	elseif param == "ServCellInfo" then
		local freqs = earfcn_to_freq(tonumber(luardb.get("wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq")),
					tonumber(luardb.get("wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.ul_freq")))
		ret["CellID"] = luardb.get("wwan.0.system_network_status.CellID")
		ret["Frequency Band"] = luardb.get("wwan.0.system_network_status.current_band")
		ret["Uplink Frequency"] = freqs.ul or ""
		ret["Downlink Frequency"] = freqs.dl or ""
		ret["Uplink EARFCN"] = luardb.get("wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.ul_freq")
		ret["Downlink EARFCN"] = luardb.get("wwan.0.radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq")
		ret["TAC"] = luardb.get("wwan.0.radio.information.tac")
		ret["LAC"] = luardb.get("wwan.0.system_network_status.LAC")
		ret["RSRP"] = luardb.get("wwan.0.signal.0.rsrp")
		ret["RSRQ"] = luardb.get("wwan.0.signal.rsrq")
		ret["MCC"] = luardb.get("wwan.0.system_network_status.MCC")
		ret["MNC"] = luardb.get("wwan.0.system_network_status.MNC")
	elseif param == "ActivePDPContextInfo" then
		ret = {}
		for _, i in ipairs({1,4}) do
			local linkProfile = string.format("link.profile.%d", i)
			logDebug("Getting linkProfile "..linkProfile.."\n")
			if luardb.get(linkProfile..".connect_progress") == "established" then
				local active = {}
				active.IPv6Address = luardb.get(linkProfile..".ipv6_ipaddr")
				active.IPv4Address = luardb.get(linkProfile..".iplocal")
				active.IPVersion = luardb.get(linkProfile..".pdp_type")
				active.QoS = luardb.get(linkProfile..".QoS")
				active.APN = luardb.get(linkProfile..".apn")
				table.insert(ret, active)
			end
		end
	end
	self:write({FieldTestResult=ret})
end

local function fieldTestGet(self)
	local run = nil
	local data = nil
	if self.request.arguments == nil then
		return self:doError(400, "Arguments missing")
	end
	for k,v in pairs(self.request.arguments) do
		if k:lower() == "run" then
			run = v
		elseif k:lower() == "data" then
			data = v
		end
	end
	if run == "SIMFile" then
		if data == nil then
			return self:doError(400, "Arguments missing")
		end
		SIMFile(self, data)
	elseif run == "GetLTEStatus" then
		GetLTEStatus(self, data)
	elseif run == "DeviceInitiatedSession" then
		local nextEvent
		-- Innopath-DM : .next_di : offset in second unit
		-- Netcomm-DM  : .di.next : time since epoch in second unit
		if rgif_config.update_engine == "Netcomm-DM" then
			 nextEvent = tonumber(luardb.get(ota_prefix.."di.next") or 0)
		else
			 nextEvent = tonumber(luardb.get(ota_prefix.."next_di") or 0) + os.time()
		end
		self:write({result=rgif_utils.toUTCTimeStr(nextEvent)})
	elseif run == "ModuleLog" then
		local qxdm_status = luardb.get("service.qxdm_ethernet.enable")
		if qxdm_status == "1" then
			self:write({result="enable"})
		else
			self:write({result="disable"})
		end
	else
		return self:doError(400, "Unknown command")
	end
end

local function emsEnable(self)
	if luardb.get('vlan.ems.deny_rg_control') == "1" then
		self:set_status(403) return
	end
	local enable = 0
	for k,v in pairs(self.request.arguments) do
		if k:lower() == "emsenable" then
			enable = v
			break
		end
	end
	-- AT&T CBRS/SAS requirement
	-- Do not allow RG enabling/disabling EMS directly.
	if rgif_config.has_sas then
		luardb.set('link.profile.4.enabled_by_rg', enable)
		-- Update client address and trigger any required actions
		rgif_utils.updateClientAddress(self.request.remote_ip or self.request.connection.address)
		logErr("emsEnable: Set link.profile.4.enabled_by_rg " ..enable)
	else
		luardb.set('link.profile.4.enable', enable)
		-- Update client address and trigger any required actions
		rgif_utils.updateClientAddress(self.request.remote_ip or self.request.connection.address)
	end
end

-- note that do_enable is an integer, not bool
local function adb_ssh_control(self, do_enable)

	-- no effect on engineering builds. SSH and ADB remain enabled.
	if luardb.get("system.product.skin") ~=  "ATT" then
		return
	end

	-- templates are written in a way that triggering them unnecessarily
	-- (when the rdb is not changing state) is undesirable. So, debounce.
	if do_enable == 1 then
		if luardb.get("service.adb.enable") == "0" then
			luardb.set("service.adb.enable", 1, "p")
		end
		if luardb.get("service.ssh.enable") == "0" then
			if rgif_config.rgif_model == "magpie" then
				luardb.set("service.ssh.enable", 1)
			else
				luardb.set("service.ssh.enable", 1, "p")
			end
		end
	else
		if luardb.get("service.adb.enable") == "1" then
			luardb.set("service.adb.enable", 0, "p")
		end
		if luardb.get("service.ssh.enable") == "1" then
			if rgif_config.rgif_model == "magpie" then
				luardb.set("service.ssh.enable", 0)
			else
				luardb.set("service.ssh.enable", 0, "p")
			end
		end
	end
end

-- used when QXDM is enabled or disabled
-- The second argument is 0 or 1, the third argument should
-- match the last 6 digits of IMEI for the function to succeed.
-- This is also used in production builds to enable ADB and QXDM,
-- otherwise production builds have no way to get into them and
-- reflash the software.
local function qxdm_control(self, do_enable, imei)
	-- confirm IMEI last 6 digits match
	local imei_rdb = luardb.get("wwan.0.imei")
	if imei_rdb == nil then self:set_status(400) return end
	if string.sub(imei_rdb, string.len(imei_rdb) - 5) == imei then
		-- service.qxdm_ethernet.enable is defined as persistent in
		-- default config file so don't need 'p' flag here
		luardb.set("service.qxdm_ethernet.enable", do_enable)
		adb_ssh_control(self, do_enable)
	else
		self:set_status(400)
	end
end

local function fieldTestPut(self)
	local run = nil
	local data = nil
	if self.request.arguments == nil then
		self:set_status(400)
	end
	for k,v in pairs(self.request.arguments) do
		if k:lower() == "run" then
			run = v
		elseif k:lower() == "data" then
			data = v
		end
	end
	-- Magpie does not support LTEbandLock API
	if rgif_config.rgif_model ~= "magpie" and run == "LTEBandLock" then
		LTEBandLock(self, data)
	elseif run == "DeviceInitiatedSession" then
		-- time value in second unit
		local epochTime = rgif_utils.getEpochTime(data)
		if epochTime then
			-- Innopath-DM : .next_di : offset in second unit
			-- Netcomm-DM  : .di.next : time since epoch in second unit
			local offset = epochTime - os.time()
			if rgif_config.update_engine == "Netcomm-DM" then
				luardb.set(ota_prefix.."di.next", epochTime, "p")
			else
				luardb.set(ota_prefix.."next_di", offset, "p")
			end
		else
			self:set_status(400)
		end
	elseif run == "ModuleLog" then
		-- data is in the format of enable,imei or disable,imei6
		-- imei6 is the last six digits of the imei
		local imei6 -- last six digits
		-- expect to find either "enable," or "disable,"
		imei6, do_enable = string.gsub(data, "enable,", "")
		if do_enable == 1 then
			qxdm_control(self, 1, imei6)
		else
			imei6, do_disable = string.gsub(data, "disable,", "")
			if do_disable == 1 then
				qxdm_control(self, 0, imei6)
			else
				self:set_status(400)
			end
		end
	else
		self:set_status(400)
	end
end

local function upgradeFirmware(self)
	local pending = luardb.get(installtime_rdb)
	if pending == nil or tonumber(pending) < os.time() then
		self:set_status(405) -- upgrade could not be made when there is no pending downloading.
		return
	end
	-- trigger upgrade now
	luardb.set("service.system.upgrade", "1")
end

local function lastGPSScanTime()
	local scanDate = luardb.get("sensors.gps.0.standalone.date")
	local scanTime = luardb.get("sensors.gps.0.standalone.time")
	if scanDate == nil or scanTime == nil then
		return "n/a"
	end
	local day,month,year = scanDate:match("(%d%d)(%d%d)(%d%d)")
	local hourmin, second = scanTime:match("(%d%d%d%d)(%d%d)")
	local r = io.popen("date -u +%s --date='"..string.format("%s%s%s%s.%s", year, month, day, hourmin, second).."'")
	local epoch = r:read("*line")
	r:close()
	return rgif_utils.toUTCTimeStr(epoch)
end

local function devicelog()
	local r = io.popen("logcat | tail -c 32k")
	local str = r:read("*all")
	r:close()
	return str
end

--- Write thermal zones to syslog
local function log_temps()
	local function get_temp(index)
		local file = io.open("/sys/devices/virtual/thermal/thermal_zone"..index.."/temp")
		if file then
			local line = file:read('*l')
			file:close()
			return line or ""
		end
		return nil
	end

	local therm = "PERIODIC_THERMAL: "
	local i = 0
	while true do
		local temp_i = get_temp(i)
		if temp_i then
			therm = therm..(i > 0 and ", " or "")..temp_i
		else
			break
		end
		i = i + 1
	end
	logNotice(therm)
end

-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(maps, _, util)
	local basepath = "/api/v1"
	maps[basepath.."/DeviceInfo"] = {
		get = {code = '200'},
		model = {
			Manufacturer = util.map_fix("NetComm Wireless Limited"),
			ManufacturerOUI = util.map_fs('r', '/sys/class/net/eth0/address', false, util.filt_pat("(%x%x):(%x%x):(%x%x).*", "%1%2%3")),
			ModelName = util.map_rdb('r', 'system.product.class'),-- same as ProductClass
			Description = util.map_rdb('r', 'system.product.title'),
			ProductClass = util.map_rdb('r', 'system.product.class'),
			SerialNumber = util.map_rdb('r', 'system.product.sn'),
			HardwareVersion = util.map_rdb('r', 'system.product.hwver'),
			SoftwareVersion = util.map_rdb('r', 'sw.version'),
			SoftwareVersionBuildDate = util.map_rdb('r', 'sw.date'),
			UpTime = util.map_fs('r', '/proc/uptime', false, util.filt_pat("^(%d+)%..*")),
			PRIVersion = util.map_rdb('r', 'wan.0.priid_config'),
			SoftwareUpdatePending = util.map_rdb('r', installtime_rdb, function(val) if val == nil or tonumber(val) < os.time() then return "Completed" end return "Pending" end), -- system.fota.pending is set when the firmware download is finished and no immediate update is required.
			Temperature = util.map_fn('r', cpu_temperature, {}),
			TemperatureTime = util.map_fn('r', rgif_utils.getUTCTimeNowStr),
			ResetProcessor= util.map_fix("Application processor"), -- There might be other reset sources. Currently, only application processor is indicated.
			AdditionalHardwareVersion= util.map_rdb('r', 'wwan.0.hardware_version'),
			IQAgentVersion= util.map_fix(""),
			AdditionalSoftwareVersion= util.map_rdb('r', 'wwan.0.firmware_version'),
			MacAddress= util.map_fs('r', '/sys/class/net/eth0/address'),
			SoftwareUpdateTime= util.map_rdb('r', installtime_rdb, function(val) if val == nil then return -1 end return os.time() <= tonumber(val) and tonumber(val) - os.time() or -1 end),
                        FirstUseDate = util.map_rdb('r', 'system.product.firstusagedate', function(val) if val == nil or not tonumber(val) then return "" end return os.date('!%Y-%m-%dT%TZ', val) end), -- Convert date in EPOC number to ISO-8601 format
			ResetCause= util.map_fn('r', function() return _G._lastRebootReason end, {}),
			ApiVersion = util.map_fix(rgif_config.api_version),
		}
	}
	maps[basepath.."/WANIPConnection/HistoricalStats"] = {
		get = {
			code = "200",
			compare = {
				func = function() return rgif_utils.apn end,
				field = "APN"
			},
		},
		delete = {
			code = "200",
			compare = {
				func = function() return rgif_utils.apn end,
				field = "APN"
			}
		},
		history = {
			compressed = false,
			poll = {},
			entryNumber = true,
			limit = 97,
			limitCount = (rgif_config.apn_numbers or 2)
	                -- Default two interfaces history recorded
		},
		model = {
			INSTANCES = rgif_config.apn_indices or {1, 4},
			ITEMS = {
				APN= util.map_rdb('r', 'link.profile.&I.apn'),
        --[[

          * extracted from flash_statistics.c in db_apps/mtd_statistics

          wwan_usage_current[idx].endTime = currentTime; //currentSessionCurrentTime
          snprintf( value, sizeof(value), "%lu,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
            wwan_usage_current[idx].StartTime, wwan_usage_current[idx].endTime,
            wwan_usage_current[idx].DataReceived, wwan_usage_current[idx].DataSent,
            wwan_usage_current[idx].DataPacketsReceived, wwan_usage_current[idx].DataPacketsSent,
            wwan_usage_current[idx].DataErrorsReceived, wwan_usage_current[idx].DataErrorsSent,
            wwan_usage_current[idx].DataDiscardPacketsReceived, wwan_usage_current[idx].DataDiscardPacketsSent
            );
          sprintf(cmd, "link.profile.%u.usage_current", idx+1);
          P("set '%s' to '%s'", cmd, value);
          update_rdb_if_chg(cmd, value);

          * field index

           #1 StartTime
           #2 endTime
           #3 DataReceived
           #4 DataSent
           #5 DataPacketsReceived
           #6 DataPacketsSent
           #7 DataErrorsReceived
           #8 DataErrorsSent
           #9 DataDiscardPacketsReceived
          #10 DataDiscardPacketsSent

        ]]--

				EthernetBytesSent= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(4)),
				EthernetBytesReceived= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(3)),
				EthernetPacketsReceived= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(5)),
				EthernetPacketsSent= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(6)),
				EthernetErrorsReceived= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(7)),
				EthernetErrorsSent= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(8)),
				EthernetDiscardPacketsSent= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(10)),
				EthernetDiscardPacketsReceived= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(9)),
				StartTime= util.map_fn('r', rgif_utils.getUTCTimeNowStr),
				NumberofEntries= util.map_fix(0),	-- will be overwritten by history's entryNumber handler
			}
		}
	}
	maps[basepath.."/DeviceInfo/ProcessStatus"] = {
		get = {code = "200"},
		model = {
			CPUUsagePeak= util.map_rdb('r', 'system.stats.cpu.combined.max'),
			CPUUsageUser= util.map_rdb('r', 'system.stats.cpu.user'),
			CPULoadAverage= util.map_fs('r', '/proc/loadavg'),
			CPUUsageAverage= util.map_rdb('r', 'system.stats.cpu.combined'),
			CPUUsageIdle= util.map_rdb('r', 'system.stats.cpu.idle'),
			CPUUsageSystem= util.map_rdb('r', 'system.stats.cpu.system'),
		}
	}
	maps[basepath.."/DeviceInfo/DeviceLog"] = {
		get = {code = "200"},
		model = {
			DeviceLog = util.map_fn('r', devicelog)
		}
	}
	maps[basepath.."/NeighborList/LTECell"] = {
		get = {
			code = "200",
			-- this is get-only and there's nothing to check,
			-- so we can simply return the function defined above
			handler = function() return get_LteNeighbours end
		},
	}
	maps[basepath.."/PDPContext"] = {
		get = {code = "200"},
		put = {
			code = "200",
			trigger = function() return pdpContextOnOff end
		},
		model = {
			-- Currently only profile 1 and profile 4 - data and EMS PDP context status can be
			-- shown, voice and SOS is not under connection manager's control.
			-- If pdpcontext_index is defined in config file, then use the indices in the config
			INSTANCES = rgif_config.apn_indices or {1, 4},
			ITEMS = {
				InterfaceIPv4= util.map_rdb('r', 'link.profile.&I.interface'),
				IPv6Prefix= util.map_rdb('r', 'link.profile.&I.ipv6_ipaddr', ipv6_prefix),
				ConnectionStatus= util.map_rdb('r', 'link.profile.&I.connect_progress'),
				IPv4Address= util.map_rdb('r', 'link.profile.&I.iplocal'),
				APN= util.map_rdb('r', 'link.profile.&I.apn'),
				IPv6Dns2= util.map_rdb('r', 'link.profile.&I.ipv6_dns2'),
				IPv4Dns2= util.map_rdb('r', 'link.profile.&I.dns2'),
				IPv4Dns1= util.map_rdb('r', 'link.profile.&I.dns1'),
				IPv6Dns1= util.map_rdb('r', 'link.profile.&I.ipv6_dns1'),
				["PDN Function"]= util.map_rdb('r', 'link.profile.&I.pdp_function', util.filt_dic({Voice="IMS", Data="User Data", EMS="EMS", SOS="SOS", CBRSSAS="CBRSSAS"})), -- Do coversion since PDN function display is different from PDN reset part.
				IPv6Address= util.map_rdb('r', 'link.profile.&I.ipv6_ipaddr', util.filt_pat('/.*$', "")),
				InterfaceIPv6= util.map_rdb('r', 'link.profile.&I.interface'),
				["PDN Type"]= util.map_rdb('r', 'link.profile.&I.pdp_type'),
				ConnectionUptime= util.map_rdb('r', 'link.profile.&I.usage_current',
					util.filt_pat('^([^,]*).*', nil, -- extract connection start
						util.filt_lin(-1, os.time,     -- subtract from current time
							util.filt_def(0)))),         -- if down then 0
			}
		}
	}
	maps[basepath.."/NotificationServer"] = {
		get = {code = "200"},
		model = {
			ServerProtocol=util.map_fix("WebSocket"),
			ServerAddress=util.map_fn('r', function() local prot="wss" if luardb.get("service.turbontc.httpenabled") == "1" then prot="ws" end return prot.."://"..luardb.get("vlan.admin.address")..":"..luardb.get("r", "service.turbontc.port").."/notification" end),
			ServerPort=util.map_rdb("r", "service.turbontc.port")
		}
	}
	maps[basepath.."/PDPContextReset"] = {
		put = {
			code = "200",
			trigger = function() return pdpContextReset end
		},
	}
	-- no model for this one - there's no real data anyway
	maps[basepath.."/Reboot"] = {
		put = {
			code = "200",
			trigger=function() return function (self) luardb.set('service.system.reset', '1') end end
		},
	}
	maps[basepath.."/upgradeFirmware"] = {
		put = {
			code = "200",
			trigger = function() return upgradeFirmware end
		},
		model = {
		}
	}
	maps[basepath.."/NetworkTime"] = {
		get = {code = "200"},
		model = {
			TimeZone= util.map_rdb('r', 'wwan.0.networktime.timezone'),
			Time= util.map_fn('r', rgif_utils.getUTCTimeNowStr),
		}
	}
	maps[basepath.."/GPS"] = {
		get = {code = "200"},
		model = {
			ScanStatus= util.map_rdb('r', 'sensors.gps.0.standalone.status'),
			Altitude= util.map_rdb('r', 'sensors.gps.0.standalone.altitude',
				util.filt_lin(1000)),			--avoid "nil" when not locked.
			ValidLocation= util.map_rdb('r', 'sensors.gps.0.standalone.valid',
				util.filt_dic({valid=1, invalid=0})),
			LockedLongitude= util.map_rdb('r', 'sensors.gps.0.standalone.longitude',
				util.filt_nmeacoord('sensors.gps.0.standalone.longitude_direction',
					util.filt_lin(1000000))),
			LockedLatitude= util.map_rdb('r', 'sensors.gps.0.standalone.latitude',
				util.filt_nmeacoord('sensors.gps.0.standalone.latitude_direction',
					util.filt_lin(1000000))),
			ValidAltitude= util.map_rdb('r', 'sensors.gps.0.standalone.valid',
				util.filt_dic({valid=1, invalid=0})),
			LastScanTime= util.map_fn('r', lastGPSScanTime)
		}
	}
	maps[basepath.."/DeviceInfo/MemoryStatus"] = {
		get = {code = "200"},
		model = {
			Total= util.map_rdb('r', 'system.stats.mem.total'),
			Cached= util.map_rdb('r', 'system.stats.mem.cached'),
			Free= util.map_rdb('r', 'system.stats.mem.free'),
		}
	}
	-- no model for this one - there's no real data anyway
	maps[basepath.."/FactoryReset"] = {
		put = {
			code = "200",
			trigger=function() return function(self) luardb.set('service.system.factory_reset', '1') end end
		},
	}
	maps[basepath.."/WANIPConnection/Stats"] = {
		get = {code = "200"},
		model = {
			INSTANCES = rgif_config.apn_indices or {1, 4},
			ITEMS = {
				APN= util.map_rdb('r', 'link.profile.&I.apn'),

        --[[

          * extracted from flash_statistics.c in db_apps/mtd_statistics

          wwan_usage_current[idx].endTime = currentTime; //currentSessionCurrentTime
          snprintf( value, sizeof(value), "%lu,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
            wwan_usage_current[idx].StartTime, wwan_usage_current[idx].endTime,
            wwan_usage_current[idx].DataReceived, wwan_usage_current[idx].DataSent,
            wwan_usage_current[idx].DataPacketsReceived, wwan_usage_current[idx].DataPacketsSent,
            wwan_usage_current[idx].DataErrorsReceived, wwan_usage_current[idx].DataErrorsSent,
            wwan_usage_current[idx].DataDiscardPacketsReceived, wwan_usage_current[idx].DataDiscardPacketsSent
            );
          sprintf(cmd, "link.profile.%u.usage_current", idx+1);
          P("set '%s' to '%s'", cmd, value);
          update_rdb_if_chg(cmd, value);

          * field index

           #1 StartTime
           #2 endTime
           #3 DataReceived
           #4 DataSent
           #5 DataPacketsReceived
           #6 DataPacketsSent
           #7 DataErrorsReceived
           #8 DataErrorsSent
           #9 DataDiscardPacketsReceived
          #10 DataDiscardPacketsSent

        ]]--

				EthernetBytesSent= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(4)),
				EthernetBytesReceived= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(3)),
				EthernetPacketsReceived= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(5)),
				EthernetPacketsSent= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(6)),
				EthernetErrorsReceived= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(7)),
				EthernetErrorsSent= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(8)),
				EthernetDiscardPacketsSent= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(10)),
				EthernetDiscardPacketsReceived= util.map_rdb('r', 'link.profile.&I.usage_current', wanStats(9)),
			}
		}
	}
	maps[basepath.."/USIM"] = {
		get = {code = "200"},
		model = {
			IMSI= util.map_rdb('r', 'wwan.0.imsi.msin'),
			MSISDN= util.map_rdb('r', 'wwan.0.sim.data.msisdn'),
			ICCID= util.map_rdb('r', 'wwan.0.system_network_status.simICCID'),
			IMEI= util.map_rdb('r', 'wwan.0.imei'),
			MailBoxNum= util.map_rdb('r', 'wwan.0.sim.data.mbdn'),
			Status = util.map_rdb('r', 'wwan.0.sim.status.status'),
		}
	}
	maps[basepath.."/RunFieldTest"] = {
		get = {
			code = "200",
			handler =  function() return fieldTestGet end
		},
		put = {
			code = "200",
			trigger =  function() return fieldTestPut end
		},
		model = {
		}
	}
	maps[basepath.."/Activation"] = {
		get = {code = "200"},
		model = {
			active= util.map_rdb('r', 'wwan.0.sim.data.activation'),
		}
	}
	maps[basepath.."/AutoUpdateCheck"] = {
		get = {code = "200"},
		put = {
			code = "200",
			trigger=function() return function(self) luardb.set(ota_prefix..'auto_firmware_check', self:get_argument('AutoUpdateCheck')) end end-- trigger is used here instead of using generic model 'w' operation directly. The reason for this is 'get' output field name is "Enabled" while 'put' parameter is "AutoUpdateCheck". This might be a spec's mistake.
		},
		model = {
			Enabled = util.map_rdb('r', ota_prefix..'auto_firmware_check')
		}
	}
	maps[basepath.."/EMSEnable"] = {
		get = {code = "200"},
		put = {
			code = "200",
			trigger= function() return emsEnable end
		},
		model = {
			emsEnable=util.map_fn('r', function()
				local val
				if rgif_config.has_sas then
					val=luardb.get("link.profile.4.enabled_by_rg")
				else
					val=luardb.get("link.profile.4.enable")
				end
				return val
				end)
		}
	}
	maps[basepath.."/PLMNList"] = {
		get = {
			code = "200",
			handler = function() return get_PLMNList end
		},
	}
	maps[basepath.."/PLMNListStats"] = {
		get = {code = "200"},
		model = {
			MaxPLMNListEntries = util.map_fix(40), -- The same as NAS_3GPP_NETWORK_INFO_LIST_MAX_V01 defined in QMI header file
			PLMNListNumberOfEntries = util.map_rdb('r', 'wwan.0.plmn.plmn_list.num')
		}
	}

	local wait = tonumber(luardb.get('system.stats.interval')) or 30
	-- For some reason the following code does not execute the chunk:
	-- turbo.ioloop.instance():set_interval(wait*1000, require('systemstatsd'))
	-- so it was changed to (note the original version returned a func, not table):
	if wait > 0 then
		local func = require('systemstatsd').collect
		turbo.ioloop.instance():set_interval(wait*1000, func)
	end
	wait = tonumber(luardb.get('system.stats.thermal_interval')) or 1800
	if wait > 0 then
		turbo.ioloop.instance():set_interval(wait*1000, log_temps)
	end
end
return module
