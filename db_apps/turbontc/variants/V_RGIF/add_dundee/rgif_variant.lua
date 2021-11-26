-----------------------------------------------------------------------------------------------------------------------
-- Titan (dundee) specific mappings for OWA RGIF APIs
--
-- Copyright (C) 2018 NetComm Wireless limited.
-----------------------------------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------------------------------------------
-- Other required RGIF modules
-----------------------------------------------------------------------------------------------------------------------
local rgif_utils = require("rgif_utils")

-----------------------------------------------------------------------------------------------------------------------
-- Local functions
-----------------------------------------------------------------------------------------------------------------------
-- Return all voicecall session status RDBs. Currently from 0 to 15.
local function voicecallStatusRDBs()
	local rdbs = {}
	local i = 0
	while i < 16 do
		table.insert(rdbs, string.format("wwan.0.voicecall_session.%d.state", i))
		i = i +1
	end
	return rdbs
end

local function migrateSIPSessionRDBs(rdbName)
	if rdbName == nil then return false end
	local index=rdbName:match("wwan%.0%.voicecall_session%.(%d+)%..+")
	if index == nil then return false end
	index = tonumber(index)
	if index == nil then return false end
	return rgif_utils.migrateSessionRDBs("wwan.0.voicecall_session", index, {state="stopped"})
end

-- Get active voice session start time.
-- This implementation supports up 10 active cocurrent sessions
-- although there is only one active voice session really supported
-- currently.
local function getActiveVoiceSession(self)
	local ret = {}
	for i=0,15 do -- There is a limit of 16 voice session RDBs.
		local prefix = string.format("wwan.0.voicecall_session.%d", i)
		local startTimeRDB = prefix..".start_time"
		local stopTimeRDB = prefix..".stop_time"
		local startTime = luardb.get(startTimeRDB)
		local stopTime = luardb.get(stopTimeRDB)
		if (startTime and startTime ~= "") and (stopTime == nil or stopTime == "" or stopTime == "0") then
			table.insert(ret, {StartTime=rgif_utils.toUTCTimeStr(startTime)})
		end
	end
	self:write(ret)
end

-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(maps, _, util)
	local basepath = "/api/v1"
	maps[basepath.."/VoiceService/CallHistory"] = {
		get = {
			code = "200",
			compare = {
				func = function() return rgif_utils.since end,
				field = "StartTime"
			}
		},
		delete = {code = "200"},
		history = {
			limit = 100,
			limitCount = 1,
			watch = {rdbName=voicecallStatusRDBs()},
			preAction = migrateSIPSessionRDBs
		},
		model = {
			INSTANCES = {},
			ITEMS = {
				OriginatingURI= util.map_rdb('r', "wwan.0.voicecall_session.migrated.originating_uri"),
				OutboundLastRTPTime= util.map_rdb('r', "wwan.0.voicecall_session.migrated.outbound_last_rtp_time"),
				OutboundLastRTPToD= util.map_rdb('r', "wwan.0.voicecall_session.migrated.outbound_last_rtp_tod", rgif_utils.toUTCTimeStr),-- placeholder for real data to be added in the future
				InboundCumulativeAveragePacketSize= util.map_rdb('r', "wwan.0.voicecall_session.migrated.inbound_cumulative_average_packet_size"),
				StopTime= util.map_rdb('r', "wwan.0.voicecall_session.migrated.stop_time", rgif_utils.toUTCTimeStr), -- util.filt conversion would be necessary if the time saved here is not UTC string
				SIPResultCode= util.map_rdb('r', "wwan.0.voicecall_session.migrated.sip_result_code"),
				OutboundCumulativeAveragePacketSize= util.map_rdb('r', "wwan.0.voicecall_session.migrated.outbound_cumulative_average_packet_size"),
				LastCallNumber= util.map_rdb('r', "wwan.0.voicecall_session.migrated.last_call_number"),
				OutboundTotalRTPPackets= util.map_rdb('r', "wwan.0.voicecall_session.migrated.outbound_total_rtp_packets"),
				PayloadType= util.map_rdb('r', "wwan.0.voicecall_session.migrated.payload_type"),
				AvgRTPLatency= util.map_rdb('r', "wwan.0.voicecall_session.migrated.avg_rtp_latency"),
				DecoderDelay= util.map_rdb('r', "wwan.0.voicecall_session.migrated.decoder_delay"),
				MaxReceiveInterarrivalJitter= util.map_rdb('r', "wwan.0.voicecall_session.migrated.max_receive_interarrival_jitter"),
				AvgReceiveInterarrivalJitter= util.map_rdb('r', "wwan.0.voicecall_session.migrated.avg_receive_interarrival_jitter"),
				EncoderDelay= util.map_rdb('r', "wwan.0.voicecall_session.migrated.encoder_delay"),
				InboundTotalRTPPackets= util.map_rdb('r', "wwan.0.voicecall_session.migrated.inbound_total_rtp_packets"),
				InboundLastRTPTime= util.map_rdb('r', "wwan.0.voicecall_session.migrated.inbound_last_rtp_time"),
				InboundLastRTPToD= util.map_rdb('r', "wwan.0.voicecall_session.migrated.inbound_last_rtp_tod", rgif_utils.toUTCTimeStr), -- placeholder for real data to be added in the future
				CallDirection= util.map_rdb('r', "wwan.0.voicecall_session.migrated.call_direction"),
				InboundDejitterDiscardedRTPPackets= util.map_rdb('r', "wwan.0.voicecall_session.migrated.inbound_dejitter_discarded_rtp_packets"),
				StartTime= util.map_rdb('r', "wwan.0.voicecall_session.migrated.start_time", rgif_utils.toUTCTimeStr), -- util.filt conversion would be necessary if the time saved here is not UTC string
				InboundDecoderDiscardedRTPPackets= util.map_rdb('r', "wwan.0.voicecall_session.migrated.inbound_decoder_discarded_rtp_packets"),
				CodecGSMUMTS= util.map_rdb('r', "wwan.0.voicecall_session.migrated.codec_gsm_umts"),
				TerminatingURI= util.map_rdb('r', "wwan.0.voicecall_session.migrated.terminating_uri"),
				InboundLostRTPPackets= util.map_rdb('r', "wwan.0.voicecall_session.migrated.inbound_lost_rtp_packets"),
			}
		}
	}
	maps[basepath.."/VoIPClientAddress"] = {
		get = {code = "200"},
		put = {	code = "200"},
		model = {
			clientMACAddress= util.map_rdb('rw', "vlan.voice.rg_mac"),
		}
	}
	maps[basepath.."/VoiceService/MessageWaiting"] = {
		get = {code = "200"},
		model = {
			MessageWaiting= util.map_rdb('r', "wwan.0.mwi.voicemail.active"),
		}
	}
	maps[basepath.."/VoiceService/LineService"] = {
		get = {code = "200"},
		put = {
			code = "200",
			trigger=function() return function(self) luardb.set("pbx.enable", self:get_argument("enableService")) end end
		},
		model = {
			SIPRegistrarServerPort= util.map_rdb('r', "pbx.config.registrar.port"),
			SIPRegisterExpires= util.map_rdb('r', "pbx.config.registrar.default_expiration"),
			ServiceState= util.map_rdb('r', "pbx.status.ServiceState"),
			LineRTPPriorityMark= util.map_rdb('r', "pbx.status.LineRTPPriorityMark"),
			SIPRFC3265Enabled= util.map_rdb('r', "pbx.status.SIPRFC3265Enabled"),
			SIPRFC3311UAC= util.map_rdb('r', "pbx.status.SIPRFC3311UAC"),
			SIPRFC3262UAC= util.map_rdb('r', "pbx.status.SIPRFC3262UAC"),
			LineRTCPTxInterval= util.map_rdb('r', "pbx.status.LineRTCPTxInterval"),
			SIPReInviteExpires= util.map_rdb('r', "pbx.status.SIPReInviteExpires"),
			LineInterDigitTimer= util.map_rdb('r', "pbx.dialplan.incall.interdigit_delay", function(val) return math.floor(tonumber((val == nil or val == "") and "0" or val)/1000) end),
			SIPRFC3262UAS= util.map_rdb('r', "pbx.status.SIPRFC3262UAS"),
			SIPUserAgentTransport= util.map_rdb('r', "pbx.config.registrar.transport"),
			LineRTPDSCP= util.map_rdb('r', "pbx.status.LineRTPDSCP"),
			LineShortInterDigitTimer= util.map_rdb('r', "pbx.status.LineShortInterDigitTimer", function(val) return math.floor(tonumber((val == nil or val == "" ) and "0" or val)/1000) end),
			SIPUserAgentDomain= util.map_rdb('r', "pbx.status.SIPUserAgentDomain"),
			LineRTCPEnabled= util.map_rdb('r', "pbx.status.LineRTCPEnabled"),
			SIPRegistrarServerTransport= util.map_rdb('r', "pbx.config.registrar.transport"),
			DirectoryNumber= util.map_rdb('r', "pbx.sip.ua.1.dirno"),
			SIPPriorityMark= util.map_rdb('r', "pbx.status.SIPPriorityMark"),
			SIPAuthUserName= util.map_rdb('r', "pbx.sip.ua.1.user"),
			SIPAuthPassword= util.map_rdb('r', "pbx.sip.ua.1.pw"),
			SIPRegistrarServer= util.map_rdb('r', "vlan.voice.address"),
			SIPUserAgentPort= util.map_rdb('r', "pbx.config.registrar.port"),
			SIPDSCP= util.map_rdb('r', "pbx.status.SIPDSCP"),
			SIPRegisterRetryInterval= util.map_rdb('r', "pbx.status.SIPRegisterRetryInterval"),
			LineFirstDigitTimer= util.map_rdb('r', "pbx.dialplan.incall.initdigit_delay", function(val) return math.floor(tonumber((val == nil or val == "" ) and "0" or val)/1000) end),
			SIPRegistersMinExpires= util.map_rdb('r', "pbx.config.registrar.minimum_expiration"),
			LineLocalURI = util.map_fn('r', function() return string.format("sip:%s@%s", luardb.get("pbx.sip.ua.1.user"), luardb.get("vlan.voice.address"))end)
		}
	}
	maps[basepath.."/VoiceService/IMSSIPConfiguration"] = {
		get = {code = "200"},
		model = {
			SIPAuthUserName = util.map_rdb('r', "wwan.0.sim.raw_data.impi", isim_rawdata_to_str),
			SIPAuthPassword = util.map_fix(""),
			SIPLocalURI = util.map_rdb('r', "wwan.0.sim.raw_data.impu.1", isim_rawdata_to_str)
		}
	}
	maps[basepath.."/VoiceService/Stats"] = {
		get = {code = "200"},
		delete = {
			code = "200",
			trigger = function() return function(self) luardb.set("voicecall.statistics.reset", '1') statsLastResetTimestamp = os.time() end end
		},
		model = {
			Overruns= util.map_rdb('r', "voicecall.statistics.overruns"),
			OutgoingCallsFailed= util.map_rdb('r', "voicecall.statistics.outgoing_calls_failed"),
			CallsDropped= util.map_rdb('r', "voicecall.statistics.calls_dropped"),
			OutgoingCallsAttempted= util.map_rdb('r', "voicecall.statistics.outgoing_calls_attempted"),
			BytesSent= util.map_rdb('r', "voicecall.statistics.bytes_sent"),
			TimeStampLastReset= util.map_fn('r', function()return rgif_utils.toUTCTimeStr(statsLastResetTimestamp) end, {}),
			IncomingCallsConnected= util.map_rdb('r', "voicecall.statistics.incoming_calls_connected"),
			IncomingCallsReceived= util.map_rdb('r', "voicecall.statistics.incoming_calls_received"),
			OutgoingCallsConnected= util.map_rdb('r', "voicecall.statistics.outgoing_calls_connected"),
			PacketsSent= util.map_rdb('r', "voicecall.statistics.packets_sent"),
			ServerDownTime= util.map_fix(0),
			IncomingCallsFailed= util.map_rdb('r', "voicecall.statistics.incoming_calls_failed"),
			Underruns= util.map_rdb('r', "voicecall.statistics.underruns"),
			OutgoingCallsAnswered= util.map_rdb('r', "voicecall.statistics.outgoing_calls_answered"),
			PacketsLost= util.map_rdb('r', "voicecall.statistics.packets_lost"),
			IncomingCallsAnswered= util.map_rdb('r', "voicecall.statistics.incoming_calls_answered"),
			PacketsReceived= util.map_rdb('r', "voicecall.statistics.packets_received"),
			IncomingCallsNotAnswered= util.map_fn('r', function() local received = luardb.get("voicecall.statistics.incoming_calls_received") local answered = tonumber(luardb.get("voicecall.statistics.incoming_calls_answered")) if received and answered then return received - answered else return 0 end end, {}),
			TotalCallTime= util.map_rdb('r', "voicecall.statistics.total_call_time"),
			BytesReceived= util.map_rdb('r', "voicecall.statistics.bytes_received"),
			CurrCallRemoteIP = util.map_rdb('r', "voicecall.statistics.curr_call_remote_ip"),
			LastCallRemoteIP = util.map_rdb('r', "voicecall.statistics.last_call_remote_ip")
		}
	}
	maps[basepath.."/RRC"] = {
		get = {code = "200"},
		history = {
			limit = 99,
			limitCount = 1,
			watch = {rdbName="wwan.0.rrc_session.index"},
			preAction = rgif_utils.migrateRRCSessionRDBs,
			append = {beginning = true, func = rgif_utils.getCurrentRRC} -- Append the current RRC session to the beginning of history
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
				PDCPLatencySent= util.map_fix(0),
			}
		}
	}
	maps[basepath.."/ServCell/History"] = {
		get = {code = "200"},
		history = {
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
			}
		}
	}
	maps[basepath.."/VoiceService/IMSStatus"] = {
		get = {code = "200"},
	--	observe = {
	--		IMSStatus=function(val) if val == "registered" then _G._ims_lastRegistrationTime = rgif_utils.getUTCTimeNowStr() end end
	--	},
		model = {
			LastSuccessfulRegistrationTime= util.map_rdb('r', "wwan.0.ims.register.time", rgif_utils.toUTCTimeStr),
			IMSStatus= util.map_rdb('r', "wwan.0.ims.register.reg_stat", util.filt_dic({["not registered"]="Registration Failure"})),
			IMSFailureCause= util.map_rdb('r', "wwan.0.ims.register.reg_error_string", util.filt_rdbdef("wwan.0.ims.register.reg_failure_error_code")),
		}
	}
	maps[basepath.."/PDPContext/History"] = {
		get = {code = "200"},
		delete = {
			code = "200",
			compare = {
				func = function() return rgif_utils.apn end,
				field = "APN"
			}
		},
		history = {
			--- 16 records are taken every hour and additional 4 records for current records.
			limit = 97,
			limitCount = 4,
			watch = {rdbName="qdiagd.trigger.interval"},
			preAction = rgif_utils.hasStart("wwan.0.pdpcontext", {3,2,1,0})
		},
		model = {
			INSTANCES = {3,2,1,0},
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
		}
	}
	maps[basepath.."/VoiceLineSessions"] = {
		get = {
			code = "200",
			handler = function() return getActiveVoiceSession end
		},
	}
end
return module
