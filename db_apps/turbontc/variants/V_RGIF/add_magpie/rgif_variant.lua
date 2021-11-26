-----------------------------------------------------------------------------------------------------------------------
-- Magpie specific mappings for OWA RGIF APIs
--
-- Copyright (C) 2018 NetComm Wireless limited.
-----------------------------------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------------------------------------------
-- Other required RGIF modules
-----------------------------------------------------------------------------------------------------------------------
local rgif_config = require("rgif_config")
local rgif_utils = require("rgif_utils")

-----------------------------------------------------------------------------------------------------------------------
-- Local functions
-----------------------------------------------------------------------------------------------------------------------
local function convertInteger(val)
	return math.floor(tonumber(val) or 0)
end

local function convertInteger1(val)
	-- choose first integer in comma separated list
	return convertInteger((val or ""):match("(%d+)"))
end

local function convertInteger2(val)
	-- choose second integer in comma separated list
	return convertInteger((val or ""):match("%d+,(%d+)"))
end

local function convertString(val)
	return (val == nil) and "" or tostring(val)
end

local function convertSecondsToMinutes(val)
	-- convert seconds to minutes
	return convertInteger((tonumber(val) or 0)/60)
end

local function convertMinutesToSeconds(val)
	-- convert seconds to minutes
	return convertInteger((tonumber(val) or 0) * 60)
end

-- Populate table of RDB values from a prefix and properties suffix
local function getRdbsBySuffix(prefix, properties)
	local instance = {}
	for k, v in pairs(properties) do
		instance[k] = v.convert(luardb.get(prefix..v.suffix))
	end
	return instance
end

-- Return number of entities in RDB for a given prefix using either and explicit count RDB, or by
-- counting available entries.  Note if suffix is not given, then luardb.keys is used but this
-- is a more expensive action to perform
-- If emptyCheck is set then count up until the value of the RDB variable is not empty.
local function getRdbCount(prefix, suffix, emptyCheck)
	local rdbCount = tonumber(luardb.get(prefix.."_count"))
	if not rdbCount then
		rdbCount = 0
		if suffix then
			while true do
				local val = luardb.get(prefix.."."..rdbCount.."."..suffix)
				if val == nil then
					break
				end
				if emptyCheck and val == "" then
					return rdbCount
				end
				rdbCount = rdbCount + 1
			end
		else
			while #luardb.keys(prefix.."."..rdbCount..".") > 0 do
				rdbCount = rdbCount + 1
			end
		end
	end
	return rdbCount
end

-- Get CellList for the given RRC
local function getRRCCellList(index)
	index = index or tonumber(luardb.get("wwan.0.rrc_session.index"))
	if not index then
		return nil
	end
	local properties = {
			MaxPUSCHTxPower = {suffix = "max_pusch_tx_power", convert = convertInteger},
			AvgPUSCHTxPower = {suffix = "avg_pusch_tx_power", convert = convertInteger},
			MaxPUCCHTxPower = {suffix = "max_pucch_tx_power", convert = convertInteger},
			AvgPUCCHTxPower = {suffix = "avg_pucch_tx_power", convert = convertInteger},
			AvgCQI = {suffix = "avg_cqi", convert = convertInteger},
		}

	local cellPrefix = "wwan.0.rrc_session."..index..".cell"
	local cellCount = getRdbCount(cellPrefix)
	local cellList = {}
	for cellIndex = 0, cellCount - 1 do
		table.insert(cellList, getRdbsBySuffix(cellPrefix.."."..cellIndex..".", properties))
	end

	return cellList
end

-- Insert CellList into results from getCurrentRRC
local function getCurrentRRCWithCellList()
	local ret = rgif_utils.getCurrentRRC(true)
	if ret then
		ret["CellList"] = getRRCCellList()
	end
	return ret
end

-- Get PCellList for ServCell/History
local function getPCellHistory()
	local properties = {
			CellID = {suffix = "CellID", convert = convertString},
			RSRP = {suffix = "rsrp", convert = convertString},
			RSRQ = {suffix = "rsrq", convert = convertString},
			RSSINR = {suffix = "rssinr", convert = convertString},
			HandoverAttempt = {suffix = "handover_attempt", convert = convertInteger},
			HandoverSuccess = {suffix = "handover_count", convert = convertInteger},
			MACiBLERReceived = {suffix = "mac_i_bler_received", convert = convertInteger},
			MACiBLERSent = {suffix = "mac_i_bler_sent", convert = convertInteger},
			MACrBLERReceived = {suffix = "mac_r_bler_received", convert = convertInteger},
			MACrBLERSent = {suffix = "mac_r_bler_sent", convert = convertInteger},
			AvgCQI = {suffix = "avg_cqi", convert = convertInteger},
			TotalPRBsReceived = {suffix = "total_prbs_received", convert = convertInteger},
			TotalPRBsSent = {suffix = "total_prbs_sent", convert = convertInteger},
			TotalActiveTTIsReceived = {suffix = "total_active_ttis_received", convert = convertInteger},
			TotalActiveTTIsSent = {suffix = "total_active_ttis_sent", convert = convertInteger},
			PMIDistribution = {suffix = "pmi_distribution", convert = convertString},
			RIDistribution = {suffix = "ri_distribution", convert = convertString},
			ReceivedModulationDistribution = {suffix = "received_modulation_distribution", convert = convertString},
			SendModulationDistribution = {suffix = "send_modulation_distribution", convert = convertString},
			PUSCHTransmitPowerDistribution = {suffix = "pusch_transmit_power_distribution", convert = convertString},
			RLFCount = {suffix = "rlf_count", convert = convertInteger},
			NumberofRRCEstabAttempts = {suffix = "number_of_rrc_estab_attempts", convert = convertInteger},
			NumberofRRCEstabFailures = {suffix = "number_of_rrc_estab_failures", convert = convertInteger},
			RRCEstabLatency = {suffix = "rrc_estab_latency", convert = convertInteger},
			NumberofRRCReEstabAttempts = {suffix = "number_of_rrc_reestab_attempts", convert = convertInteger},
			NumberofRRCReEstabFailures = {suffix = "number_of_rrc_reestab_failures", convert = convertInteger},
			RRCReEstabLatency = {suffix = "rrc_reestab_latency", convert = convertInteger},
		}

	local pcellPrefix = "wwan.0.servcell_info.pcell"
	local pcellCount = getRdbCount(pcellPrefix, "CellID", true)
	local pcellList = {}
	for pcellIndex = 0, pcellCount - 1 do
		table.insert(pcellList, getRdbsBySuffix(pcellPrefix.."."..pcellIndex..".", properties))
	end

	return pcellList
end

-- Get Grouping param for SAS
local function getSasGroupingParam()
	local groupingParam = luardb.get('sas.config.groupingParam')
	local str=''
	-- sas.config.groupingParam is a json string of an array of group id & type pairs
	-- convert it to a comma separated string '<groupid>|<type>,<groupid|type>..'
	if groupingParam ~= nil and groupingParam ~= '' then
		local groups = turbo.escape.json_decode(groupingParam)
		local sep = ""
		for _,group in ipairs(groups) do
			str = str .. sep .. group['groupId'] .. '|' .. group['groupType']
			sep = ","
		end
	end
	return str
end

-- Get CellList for SAS.GrantList[i]
local function convertSasEcgiList(rdb_ecgi_list)
	local ecgis = (rdb_ecgi_list or ""):split(",")
	local ecgiList = {}
	for _, ecgi in ipairs(ecgis) do
		table.insert(ecgiList, {ECGI = ecgi})
	end
	return ecgiList
end

-- Get GrantState for SAS.GrantList[i]
local function convertSasGrantState(rdb_grant_state)
	return convertString(rdb_grant_state or "Unknown")
end

-- Get GrantReason for SAS.GrantList[i]
local function convertSasGrantReason(rdb_grant_reason)
	return convertString(rdb_grant_reason or "Unknown")
end

-- Get GrantList for SAS
local function getSasGrants()
	local properties = {
			GrantID = {suffix = "id", convert = convertString},
			FreqRangeLow = {suffix = "freq_range_low", convert = convertInteger},
			FreqRangeHigh = {suffix = "freq_range_high", convert = convertInteger},
			MaxEIRP = {suffix = "max_eirp", convert = convertInteger},
			GrantExpireTime = {suffix = "expire_time", convert = convertString},
			ChannelType = {suffix = "channel_type", convert = convertString},
			GrantState = {suffix = "state", convert = convertSasGrantState},
			GrantReason = {suffix = "grantRequiredFor", convert = convertSasGrantReason},
			-- note: GrantReason means purpose of the grant not sas.grant.x.reason which is SAS response code
			GrantHeartbeatTime = {suffix = "next_heartbeat", convert = convertString},
			CellList = {suffix = "ecgi_list", convert = convertSasEcgiList},
		}

	local grantPrefix = "sas.grant"
	local grantIndices = (luardb.get(grantPrefix.."._index") or ""):split(",")
	local grantList = {}
	for _, grantIndex in ipairs(grantIndices) do
		table.insert(grantList, getRdbsBySuffix(grantPrefix.."."..grantIndex..".", properties))
	end

	return grantList
end

-- Change SAS parameters
local function setSasParam(self)
	local param = nil
	local value = nil
	if self.request.arguments == nil then
		self:set_status(400)
		return
	end
	for k,v in pairs(self.request.arguments) do
		if k:lower() == "param" then
			param = v:lower()
		elseif k:lower() == "value" then
			value = v
		end
	end
	if param == "callsign" then
		luardb.set("sas.config.callSign", value)
	elseif param == "groupingparam" then
		local valueList = (value or ''):split(",")
		local groupingParam = {}
		for _, group in ipairs(valueList) do
			local v = group:split("|")
			if #v ~= 2 then self:set_status(400) return end
			table.insert(groupingParam, {['groupId'] = v[1], ['groupType'] = v[2]})
		end
		if next(groupingParam) == nil then self:set_status(400) return end
		local js = turbo.escape.json_encode(groupingParam)
		luardb.set("sas.config.groupingParam", js, "p")
	else
		self:set_status(400)
	end
end

-- Change SAS config settings
local function setSasConfig(self)
	local param = nil
	local value = nil
	if self.request.arguments == nil then
		self:set_status(400)
		return
	end
	for k,v in pairs(self.request.arguments) do
		if k:lower() == "param" then
			param = v:lower()
		elseif k:lower() == "value" then
			value = v
		end
	end
	if param == "grantrelinquishwaittime" then
		local delaySec = convertMinutesToSeconds(value)
		-- Allowed range: 30 minutes (1800 seconds) to 2880 minutes (172800 seconds)
		if delaySec >= 1800 and delaySec <= 172800 then
			luardb.set("sas.timer.grant_relinquish_wait_time", delaySec, "p")
		else
			self:set_status(400)
		end
	elseif param == "grantretrywaittime" then
		local delaySec = convertMinutesToSeconds(value)
		-- Allowed range: 5 minutes (300 seconds) to 2880 minutes (172800 seconds)
		if delaySec >= 300 and delaySec <= 172800 then
			luardb.set("sas.timer.grant_retry_wait_time", delaySec, "p")
		else
			self:set_status(400)
		end
	else
		self:set_status(400)
	end
end

-- Get Log for SAS/History
local function getSasLog()
	local prefix = 'sas.eventlog'
	local logfile = luardb.get(prefix .. '.filename')
	local n = luardb.get(prefix .. '.maxrotate')
	local str = ''
	-- read all event logs with oldest entry first, recent last
	for i = n, 0, -1 do
		local filename = logfile .. (i > 0 and ('.' .. i) or '')
		local f = io.open(filename, "r")
		if f ~= nil then
			str = str .. f:read("*all")
			io.close(f)
		end
	end
	return str
end

-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(maps, _, util)
	local basepath = "/api/v1"
	maps[basepath.."/RRC"] = {
		get = {code = "200"},
		history = {
			limit = 100,
			limitCount = 1,
			watch = {rdbName="wwan.0.rrc_session.index"},
			preAction = rgif_utils.migrateRRCSessionRDBs,
			append = {beginning = true, func = getCurrentRRCWithCellList}	-- Append the current RRC session to the beginning of history
		},
		model = {
			INSTANCES = {},
			ITEMS = {
				StartTime= util.map_rdb('r', 'wwan.0.rrc_session.migrated.start_time', rgif_utils.toUTCTimeStr),
				RCLRetransRateSent= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rlc_retrans_rate_sent'),
				RRCReleaseCause= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rrc_release_cause'),
				PDCPBytesReceived= util.map_rdb('r', 'wwan.0.rrc_session.migrated.pdcp_bytes_received'),
				MaxPUSCHTxPower= util.map_rdb('r', 'wwan.0.rrc_session.migrated.max_pusch_tx_power'),
				PDCPPacketLossRateSent= util.map_rdb('r', 'wwan.0.rrc_session.migrated.pdcp_packetloss_rate_sent'),
				SCellWorstRSRQ= util.map_rdb('r', 'wwan.0.rrc_session.migrated.scell_worst_rsrq'),
				AvgCQI= util.map_rdb('r', 'wwan.0.rrc_session.migrated.avg_cqi'),
				RCLRetransRateReceived= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rlc_retrans_rate_received'),
				RLCAvgDownThroughput= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rlc_avg_down_throughput'),
				SCellAvgRSRP= util.map_rdb('r', 'wwan.0.rrc_session.migrated.scell_avg_rsrp'),
				AvgPUCCHTxPower= util.map_rdb('r', 'wwan.0.rrc_session.migrated.avg_pucch_tx_power'),
				RLCMaxUpThroughput= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rlc_max_up_throughput'),
				AvgPUSCHTxPower= util.map_rdb('r', 'wwan.0.rrc_session.migrated.avg_pusch_tx_power'),
				MaxPUCCHTxPower= util.map_rdb('r', 'wwan.0.rrc_session.migrated.max_pucch_tx_power'),
				PDCPBytesSent= util.map_rdb('r', 'wwan.0.rrc_session.migrated.pdcp_bytes_sent'),
				SCellWorstRSRP= util.map_rdb('r', 'wwan.0.rrc_session.migrated.scell_worst_rsrp'),
				SCellWorstRSSINR= util.map_rdb('r', 'wwan.0.rrc_session.migrated.scell_worst_rssinr'),
				PDCPPacketLossRateReceived= util.map_rdb('r', 'wwan.0.rrc_session.migrated.pdcp_packetloss_rate_received'),
				CellID= util.map_rdb('r', "wwan.0.system_network_status.CellID"),
				SCellAvgRSRQ= util.map_rdb('r', 'wwan.0.rrc_session.migrated.scell_avg_rsrq'),
				RlcDlDuration= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rlc_dl_duration'),
				RLCAvgUpThroughput= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rlc_avg_up_throughput'),
				SCellAvgRSSINR= util.map_rdb('r', 'wwan.0.rrc_session.migrated.scell_avg_rssinr'),
				RLCMaxDownThroughput= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rlc_max_down_throughput'),
				RlcUlDuration= util.map_rdb('r', 'wwan.0.rrc_session.migrated.rlc_ul_duration'),
				CellList = util.map_fn('r', getRRCCellList, {"migrated"}),
			}
		}
	}
	maps[basepath.."/ServCell/History"] = {
		get = {code = "200"},
		history = {
			--- 97 records for 24 hours
			limit = 97,
			limitCount = 1,
			watch = {rdbName="wwan.0.servcell_info.start_time"},
			preAction = rgif_utils.hasStart("wwan.0.servcell_info")
		},
		model = {
			INSTANCES = {},
			ITEMS = {
				StartTime= util.map_rdb('r', "wwan.0.servcell_info.start_time", rgif_utils.toUTCTimeStr),
				NumberofRRCREEstabFailures= util.map_rdb('r', "wwan.0.servcell_info.number_of_rrc_reestab_failures"),
				TotalPRBsReceived= util.map_rdb('r', 'wwan.0.servcell_info.total_prbs_received'),
				ReceivedModulationDistribution= util.map_rdb('r', "wwan.0.servcell_info.received_modulation_distribution"),
				RRCREEstabLatency= util.map_rdb('r', "wwan.0.servcell_info.rrc_reestab_latency"),
				NumberofRRCEstabAttempts= util.map_rdb('r', "wwan.0.servcell_info.number_of_rrc_estab_attempts"),
				MACrBLERSent= util.map_rdb('r', "wwan.0.servcell_info.mac_r_bler_sent"),
				TotalActiveTTIsReceived= util.map_rdb('r', 'wwan.0.servcell_info.total_active_ttis_received'),
				NumberofRRCREEstabAttempts= util.map_rdb('r', "wwan.0.servcell_info.number_of_rrc_reestab_attempts"),
				PMIDistribution= util.map_rdb('r', "wwan.0.servcell_info.pmi_distribution"),
				RRCEstabLatency= util.map_rdb('r', "wwan.0.servcell_info.rrc_estab_latency"),
				NumberofRRCEstabFailures= util.map_rdb('r', "wwan.0.servcell_info.number_of_rrc_estab_failures"),
				NumberofRLFFailureCauses= util.map_fn('r', rgif_utils.failureCausesConvert("wwan.0.servcell_info.rlf_failures"), nil, function(v) return table.getn(v) end),
				TotalActiveTTIsSent= util.map_rdb('r', 'wwan.0.servcell_info.total_active_ttis_sent'),
				FailureCause= util.map_fn('r', rgif_utils.failureCausesConvert("wwan.0.servcell_info.rlf_failures")),
				RSRQ= util.map_rdb('r', "wwan.0.servcell_info.rsrq"),
				RIDistribution= util.map_rdb('r', "wwan.0.servcell_info.ri_distribution"),
				MACiBLERReceived= util.map_rdb('r', "wwan.0.servcell_info.mac_i_bler_received"),
				RLFCount= util.map_rdb('r', 'wwan.0.servcell_info.rlf_count'),
				SendModulationDistribution= util.map_rdb('r', "wwan.0.servcell_info.send_modulation_distribution"),
				RSRP= util.map_rdb('r', "wwan.0.servcell_info.rsrp"),
				TotalPRBsSent= util.map_rdb('r', 'wwan.0.servcell_info.total_prbs_sent'),
				CellID= util.map_rdb('r', "wwan.0.system_network_status.CellID"),
				PUSCHTransmitPowerDistribution= util.map_rdb('r', 'wwan.0.servcell_info.pusch_transmit_power_distribution'),
				MACiBLERSent= util.map_rdb('r', "wwan.0.servcell_info.mac_i_bler_sent"),
				AvgWidebandCQI= util.map_rdb('r', "wwan.0.servcell_info.avg_wide_band_cqi"),
				HandoverCount= util.map_rdb('r', 'wwan.0.servcell_info.handover_count'),
				RSSINR= util.map_rdb('r', 'wwan.0.servcell_info.rssinr'),
				MACrBLERReceived= util.map_rdb('r', "wwan.0.servcell_info.mac_r_bler_received"),
				PCellList = util.map_fn('r', getPCellHistory),
			}
		}
	}
	maps[basepath.."/PDPContext/History"] = {
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
			--- 97 records for 24 hours per APN
			limit = 97,
			limitCount = (rgif_config.apn_numbers or 2),
			watch = {rdbName="qdiagd.trigger.interval"},
			preAction = rgif_utils.hasStart("wwan.0.pdpcontext", rgif_config.pdpcontext_indices or {4,3,0})
		},
		model = {
			INSTANCES = rgif_config.pdpcontext_indices or {4,3,0},
			ITEMS = {
				StartTime= util.map_rdb('r', "wwan.0.pdpcontext.&I.start_time", rgif_utils.toUTCTimeStr),
				DefaultAvgBearerDuration= util.map_rdb('r', "wwan.0.pdpcontext.&I.default_avg_bearer_duration"),
				DefaultBearerDeactAttempts= util.map_rdb('r', "wwan.0.pdpcontext.&I.default_bearer_deact_attempts"),
				DedicatedBearerDeactAttempts= util.map_rdb('r', "wwan.0.pdpcontext.&I.dedicated_bearer_deact_failures"),
				DefaultBearerDeactFailures= util.map_rdb('r', "wwan.0.pdpcontext.&I.default_bearer_deact_failures"),
				APN= util.map_rdb('r', 'wwan.0.pdpcontext.&I.apn'),
				DefaultBearerActFailures= util.map_rdb('r', "wwan.0.pdpcontext.&I.default_bearer_act_failures"),
				DefaultBearerActLatency= util.map_rdb('r', "wwan.0.pdpcontext.&I.default_avg_bearer_act_latency"),
				DedicatedBearerDeactFailures= util.map_rdb('r', "wwan.0.pdpcontext.&I.dedicated_bearer_deact_failures"),
				DedicatedBearerActAttempts= util.map_rdb('r', "wwan.0.pdpcontext.&I.dedicated_bearer_act_attempts"),
				DedicatedAvgBearerDuration= util.map_rdb('r', "wwan.0.pdpcontext.&I.dedicated_avg_bearer_duration"),
				DedicatedBearerActFailureCauses= util.map_rdb('r', "wwan.0.pdpcontext.&I.dedicated_bearer_act_failure_causes", rgif_utils.filt_act_failures),
				DedicatedBearerActFailures= util.map_rdb('r', "wwan.0.pdpcontext.&I.dedicated_bearer_act_failures"),
				DedicatedBearerActLatency= util.map_rdb('r', "wwan.0.pdpcontext.&I.dedicated_avg_bearer_act_latency"),
				DefaultBearerActAttempts= util.map_rdb('r', "wwan.0.pdpcontext.&I.default_bearer_act_attempts"),
				DefaultBearerActFailureCauses= util.map_rdb('r', "wwan.0.pdpcontext.&I.default_bearer_act_failure_causes", rgif_utils.filt_act_failures),
				DefaultBearerNormalRelease = util.map_rdb('r', "wwan.0.pdpcontext.&I.default_bearer_normal_releases"),
				DefaultBearerAbnormalRelease = util.map_rdb('r', "wwan.0.pdpcontext.&I.default_bearer_abnormal_releases"),
			}
		}
	}
	maps[basepath.."/ServCell/RF"] = {
		get = {code = "200"},
		model = {
			MNC= util.map_rdb('r', "wwan.0.system_network_status.MNC"),
			EGCI= util.map_rdb('r', "wwan.0.system_network_status.ECGI"),
			EUTRACarrierARFCN= util.map_rdb('r', "wwan.0.system_network_status.channel"),
			PUSCHTx= util.map_rdb('r', "wwan.0.radio_stack.e_utra_pusch_transmission_status.total_pusch_txpower"),
			AttachAvgLatency= util.map_rdb('r', "wwan.0.pdpcontext.attach_avg_latency"),
			TAC= util.map_rdb('r', "wwan.0.radio.information.tac"),
			MCC= util.map_rdb('r', "wwan.0.system_network_status.MCC"),
			DLBandwidth= util.map_rdb('r', "wwan.0.radio_stack.e_utra_measurement_report.dl_bandwidth"),
			PhyCellID= util.map_rdb('r', "wwan.0.system_network_status.PCID"),
			RSRQ= util.map_rdb('r', "wwan.0.signal.rsrq"),
			AttachAttempts= util.map_rdb('r', "wwan.0.pdpcontext.attach_attempts"),
			AttachFailures= util.map_rdb('r', "wwan.0.pdpcontext.attach_failures"),
			RSRP= util.map_rdb('r', "wwan.0.signal.0.rsrp"),
			CellID= util.map_rdb('r', "wwan.0.system_network_status.CellID"),
			ULBandwidth= util.map_rdb('r', "wwan.0.radio_stack.e_utra_measurement_report.ul_bandwidth"),
			LTECellNumberOfEntries= util.map_rdb('r', "wwan.0.cell_measurement.qty"),
			RSSINR= util.map_rdb('r', "wwan.0.signal.rssinr"),
			FreqBandIndicator= util.map_rdb('r', "wwan.0.system_network_status.current_band"),
			RSTxPower = util.map_rdb('r', "wwan.0.system_network_status.RSTxPower"),
			RSSI = util.map_rdb('r', "wwan.0.signal.rssi"),
			NetworkServiceStatus = util.map_rdb('r', "wwan.0.system_network_status.system_mode"),
			CQI = util.map_rdb('r', "wwan.0.system_network_status.0.avg_cqi", convertInteger1),
		}
	}
	maps[basepath.."/SAS"] = {
		get = {code = "200"},
		put = {
			code = "200",
			trigger = function() return setSasParam end
		},
		model = {
			CBSDID = util.map_rdb('r', "sas.cbsdid"),
			AntennaAzimuth = util.map_rdb('r', "sas.antenna.azimuth"),
			AntennaDowntilt = util.map_rdb('r', "sas.antenna.downtilt"),
			Latitude = util.map_rdb('r', "sas.antenna.latitude"),
			Longitude = util.map_rdb('r', "sas.antenna.longitude"),
			Height = util.map_rdb('r', "sas.antenna.height"),
			GroupingParam = util.map_fn('r', getSasGroupingParam),
			CBSDSerialNumber = util.map_rdb('r', "sas.config.cbsdSerialNumber"),
			CallSign = util.map_rdb('r', "sas.config.callSign"),
			GrantList = util.map_fn('r', getSasGrants),
		}
	}
	maps[basepath.."/SAS/Config"] = {
		get = {code = "200"},
		put = {
			code = "200",
			trigger = function() return setSasConfig end
		},
		model = {
			GrantRelinquishWaitTime = util.map_rdb('r', "sas.timer.grant_relinquish_wait_time", convertSecondsToMinutes),
			GrantRetryWaitTime = util.map_rdb('r', "sas.timer.grant_retry_wait_time", convertSecondsToMinutes),
		}
	}
	maps[basepath.."/SAS/History"] = {
		get = {code = "200"},
		model = {
			Log = util.map_fn('r', getSasLog)
		}
	}
	maps[basepath.."/CarrierAggregation/History"] = {
		get = {code = "200"},
		delete = {code = "200"},
		history = {
			poll = {},
			limit = 97,
			limitCount = 1
		},
		model = {
			INSTANCES = {},
			ITEMS = {
				StartTime= util.map_fn('r', rgif_utils.getUTCTimeNowStr),
				CellUsage = util.map_rdb('r', "wwan.0.system_network_status.ca.cell_usage"),
				CAMode = util.map_rdb('r', "wwan.0.system_network_status.ca.ca_mode"),
			}
		}
	}
	maps[basepath.."/Neighbor/History"] = {
		get = {code = "200"},
		delete = {code = "200"},
		history = {
			poll = {},
			limit = 97,
			limitCount = 1
		},
		model = {
			INSTANCES = {},
			ITEMS = {
				StartTime= util.map_rdb('r', "wwan.0.cell_measurement.ncell.start_time"),
				CellMeasurement = util.map_rdb('r', "wwan.0.cell_measurement.ncell.data"),
			}
		}
	}
end
return module
