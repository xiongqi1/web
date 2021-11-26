-----------------------------------------------------------------------------------------------------------------------
-- Common utility functions for use by OWA APIs
--
-- Copyright (C) 2018 NetComm Wireless limited.
-----------------------------------------------------------------------------------------------------------------------
local rgif_config = require("rgif_config")
local rgif_utils = {}

-- filter function : convert failure cause collection value into a table for JSON
--
-- * Array of source format in RDB
--
--	EMM or ESM failure cause = failure count,...
--
-- * Target table format
--	{
--		{ Count = [failure count], FailureCause = "[EMM or ESM failure cause]" },
--		.
--		.
--		.
--	}
--
function rgif_utils.filt_act_failures(val)
	local ret = {}

	if val then
		local failure_collection = val:split(",")
		for _,failure_information in ipairs(failure_collection) do
			local failure_name,failure_count = failure_information:match("(.*)=(.*)")
			table.insert(ret,
				{
					Count=failure_count,
					FailureCause=failure_name,
				})
		end
	end

	return ret
end

-- Get current RRC session information
function rgif_utils.getCurrentRRC(skipLatency)
	local ret = {}
	local index = luardb.get("wwan.0.rrc_session.index")
	if index then
		index = tonumber(index)
	else
		return
	end
	if index == nil then return end
	local currentState = luardb.get(string.format("wwan.0.rrc_session.%s.state", index))
	if currentState and currentState == "stopped" then -- It has been included in the history already since this session has stopped.
		return
	end
	local properties = {StartTime="start_time", RCLRetransRateSent="rlc_retrans_rate_sent", RRCReleaseCause="",
				PDCPBytesReceived="pdcp_bytes_received", MaxPUSCHTxPower="max_pusch_tx_power",
				PDCPPacketLossRateSent="pdcp_packetloss_rate_sent", SCellWorstRSRQ="scell_worst_rsrq", AvgCQI="avg_cqi",
				RCLRetransRateReceived="rlc_retrans_rate_received", RLCAvgDownThroughput="rlc_avg_down_throughput",
				SCellAvgRSRP="scell_avg_rsrp", AvgPUCCHTxPower="avg_pucch_tx_power",
				AvgPUSCHTxPower="avg_pusch_tx_power", RLCMaxUpThroughput="rlc_max_up_throughput", CellID="wwan.0.system_network_status.CellID",
				MaxPUCCHTxPower="max_pucch_tx_power", PDCPBytesSent="pdcp_bytes_sent", SCellWorstRSRP="scell_worst_rsrp",
				SCellWorstRSSINR="scell_worst_rssinr", PDCPPacketLossRateReceived="pdcp_packetloss_rate_received",
				SCellAvgRSRQ="scell_avg_rsrq", RlcDlDuration="rlc_dl_duration", RLCAvgUpThroughput="rlc_avg_up_throughput",
				SCellAvgRSSINR="scell_avg_rssinr", RLCMaxDownThroughput="rlc_max_down_throughput",
				RlcUlDuration="rlc_ul_duration"}
	-- append PDCPLatencySent separately per variants
	if not skipLatency then
		properties.PDCPLatencySent="pdcp_latency_sent"
	end
	for k,v in pairs(properties) do
		local currentRDB
		if v:find("wwan.0") == nil then
			currentRDB = string.format("wwan.0.rrc_session.%d.%s", index, v)
		else
			currentRDB = v
		end
		local value = luardb.get(currentRDB)
		if k == "StartTime" then
			value = rgif_utils.toUTCTimeStr(value)
		end
		if k == "StartTime" or k == "RRCReleaseCause" or k == "CellID" then -- string type
			if value == nil then value = "" end
			ret[k] = value
		else -- integer type
			if value == nil then value = 0 end
			ret[k] = tonumber(value) or 0
		end
	end
	return ret
end

-- This is a helper function which migrates the current indexed session RDBs
-- to RDBs with fixed name so that the data model can work with fixed RDB names.
-- The RDB to be migrated should have an "index" to tell the RDBs to migrate.
-- @param name - RDB name prefix
-- @param checkpoint - A table to check whether it can be migrated.
function rgif_utils.migrateSessionRDBs(name, index, condition)
	logDebug(string.format(">>> Migrating for index %d", index))
	local rdbName = string.format("%s.%d.", name, index)
	-- Checking whether it meets the condtion
	for name, expected in pairs(condition) do
		local condRdb = rdbName..name
		local v = luardb.get(condRdb)
		if type(expected) == "string" and v ~= expected then
			logDebug("Condition doesn't match, do nothing.")
			return false
		elseif type(expected) == "function" and not expected(v) then
			logDebug("Condition doesn't match, do nothing.")
			return false
		end
	end
	local keys = luardb.keys(rdbName)
	local matched = false -- Flag to indicate something is migrated
	local rdbNameMatch = rdbName:gsub("%.", "%%%.") -- replace all . with %. to use in match string
	for _, key in ipairs(keys) do
		if key:match(rdbNameMatch) then
			matched = true
			local migratedName = string.format("%s.migrated%s", name, key:sub(#rdbName))
			local val = luardb.get(key)
			luardb.set(migratedName, val)
		end
	end
	if matched then
		return true
	else
		return false
	end
end

-- Migrate last stopped RRC session properties to RDBs with fixed names.
function rgif_utils.migrateRRCSessionRDBs()
	local index = luardb.get("wwan.0.rrc_session.index")
	if index == nil then return false end
	index = tonumber(index)
	if index == nil then return false end
	return rgif_utils.migrateSessionRDBs("wwan.0.rrc_session", index, {state="stopped"})
end

-- This function returns a UTC time string for a second since epoch
function rgif_utils.toUTCTimeStr(val)
	if val == nil or val == "" then return "n/a" end
	return os.date("!%FT%TZ", val)
end

-- This function returns the current UTC time string
function rgif_utils.getUTCTimeNowStr()
	return rgif_utils.toUTCTimeStr(os.time())
end

-- This function returns a function which returns a table of failure causes
-- based on a list of RDBs with names ended with ".count" and ".failure_cause".
-- ".failure_cause" gives the failure cause string and ".count" gives the number
-- of how many times it happens.
function rgif_utils.failureCausesConvert(prefix)
	return function()
		local failures = {}
		local index = 0
		local cause = luardb.get(string.format("%s.%d.failure_cause", prefix, index))
		while cause do
			local count = tonumber(luardb.get(string.format("%s.%d.count", prefix, index))) or 0
			if count ~= 0 then
				table.insert(failures, {Count=count, FailureCause=cause})
			end
			index = index + 1
			cause = luardb.get(string.format("%s.%d.failure_cause", prefix, index))
		end
		return failures
	end
end

-- Compare two time string t1, t2 in ISO-8601 format like "2015-07-23T04:59:39UTC"
-- Return true if t1 no earlier than t2, otherwise false
function rgif_utils.timeStrCompare(t1, t2)
	local year1,mon1,day1,hour1,min1,sec1
	local year2,mon2,day2,hour2,min2,sec2
	year1,mon1,day1,hour1,min1,sec1 = string.match(t1, "(%d%d%d%d)-(%d%d)-(%d%d)T(%d%d):(%d%d):(%d%d)")
	year2,mon2,day2,hour2,min2,sec2 = string.match(t2, "(%d%d%d%d)-(%d%d)-(%d%d)T(%d%d):(%d%d):(%d%d)")
	return os.time{year=year1,month=mon1,day=day1,hour=hour1,min=min1,sec=sec1} >= os.time{year=year2,month=mon2,day=day2,hour=hour2,min=min2,sec=sec2}
end

-- Get epoch seconds from ISO-8601 format like "2015-07-23T04:59:39UTC"
function rgif_utils.getEpochTime(t1)
	local year1,mon1,day1,hour1,min1,sec1
	year1,mon1,day1,hour1,min1,sec1 = string.match(t1, "(%d%d%d%d)-(%d%d)-(%d%d)T(%d%d):(%d%d):(%d%d)")
	if not year1 then return nil end
	return os.time{year=year1,month=mon1,day=day1,hour=hour1,min=min1,sec=sec1}
end

-- Given a new/updated client IP address, determine the MAC for the client and update RDB
-- as needed to trigger any associated templates
function rgif_utils.updateClientAddress(address)
	logInfo("RGIF client address is "..address)
	if address and address ~= "" then
		local dhcp_address = luardb.get("vlan.admin.dhcp_event.client_ip_address") or ""
		local client_mac = luardb.get("vlan.admin.dhcp_event.mac") or ""
		if (address ~= dhcp_address) or (client_mac == "") then
			local r = io.popen("arp -n "..address)
			local str = r:read("*all")
			r:close()
			client_mac = string.match(str, "(%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)") or ""
		end
		-- The vlan.voice.rg_mac RDB holds the MAC address of the registered client for the
		-- purposes of triggering templates, even if voice is not otherwise used
		local old_mac = luardb.get("vlan.voice.rg_mac")
		if client_mac ~= old_mac then
			logNotice("Setting client MAC to "..client_mac)
			luardb.set("vlan.voice.rg_mac", client_mac)
		end
	end
end

-- Return a function which judges whether a time is no earlier than that from url argument "since"
-- The argument "since" is case-insensitively compared.
-- If no such argument is provide, it always returns true
function rgif_utils.since(self)
	return function(t)
		local from = nil
		if self.request.arguments == nil then return true end -- No argument is provided.
		for k,v in pairs(self.request.arguments) do
			if k:lower() == "since" then
				from = v
				break
			end
		end
		if from == nil then return true end
		return rgif_utils.timeStrCompare(t, from) == true
	end
end

-- Return a function which judges whether a pass-in APN parameter matches that from url argument "apn"
-- The argument "apn" is case insensitive.
-- If no such argument is provided, it always returns true
function rgif_utils.apn(self)
	return function(t)
		local name = nil
		if self.request.arguments == nil then return true end -- No argument is provided.
		for k,v in pairs(self.request.arguments) do
			if k:lower() == "apn" then
				name = v
				break
			end
		end
		if name == nil then return true end
		local index_tbl = rgif_config.apn_indices or {1, 4}
		local match = false
		for _, index in pairs(index_tbl) do
			if name == luardb.get("link.profile."..index..".apn") then
				match = true
				break
			end
		end
		if not match then
			self:set_status(400)
		end
		return t:strip() == name:strip()
	end
end

-- Return a function which returns true if the given RDB prefix has a valid start_time value for
-- all the given indices (or for just the prefix if no indices are given)
function rgif_utils.hasStart(name, indices)
	if not indices then
		return function()
			local value = luardb.get(name..".start_time")
			return (tonumber(value) or 0) ~= 0
		end
	else
		return function()
			for _, index in pairs(indices) do
				local value = luardb.get(name.."."..index..".start_time")
				if (tonumber(value) or 0) == 0 then
					return false
				end
			end
			return true
		end
	end
end

return rgif_utils
