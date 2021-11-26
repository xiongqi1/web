-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi voice module, CSD/VoLTE voice call

local QmiVoice = require("wmmd.Class"):new()


local turbo = require("turbo")
local ioloop = turbo.ioloop.instance()


-- mapping table to convert TTY mode names to tty_mode codes
QmiVoice.tty_mode_numbers={
  ["FULL"] = 0,
  ["VCO"] = 1,
  ["HCO"] = 2,
  ["OFF"] = 3,
}

QmiVoice.tty_mode_names={
  [0]="FULL",
  [1]="VCO",
  [2]="VCO",
  [3]="OFF",
}

-------------------------------------------------------------------------------------------------------------------
-- Update TTY mode with CSD
--
function QmiVoice:invoke_set_csd_tty_mode(tty_mode)
  self.l.log("LOG_DEBUG",string.format("invoke 'set csd tty mode' (tty_mode=%s)",tty_mode or "OFF"))

  return self.watcher.invoke("sys","set_csd_tty_mode",{tty_mode=tty_mode})
end


function QmiVoice:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_voice", "LOG_DAEMON") end)

  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.m = self.luaq.m
  self.ffi = require("ffi")
  self.wrdb = wrdb
end

QmiVoice.number_pi_names={
  [0x00] = "ALLOWED",
  [0x01] = "RESTRICTED",
  [0x02] = "UNAVAILABLE",
  [0x03] = "RESERVED",
  [0x04] = "PAYPHONE",
}

QmiVoice.call_state_names={
  [0x01] = "ORIGINATION",
  [0x02] = "INCOMING",
  [0x03] = "CONVERSATION",
  [0x04] = "CC_IN_PROGRESS",
  [0x05] = "ALERTING",
  [0x06] = "HOLD",
  [0x07] = "WAITING",
  [0x08] = "DISCONNECTING",
  [0x09] = "END",
  [0x0A] = "SETUP",
}

QmiVoice.call_type_names={
  [0x00] = "VOICE",
  [0x01] = "VOICE_FORCED",
  [0x02] = "VOICE_IP",
  [0x03] = "VT",
  [0x04] = "VIDEOSHARE",
  [0x05] = "TEST",
  [0x06] = "OTAPA",
  [0x07] = "STD_OTASP",
  [0x08] = "NON_STD_OTASP",
  [0x09] = "EMERGENCY",
  [0x0A] = "SUPS",
  [0x0B] = "EMERGENCY_IP",
  [0x0C] = "ECALL",
}

QmiVoice.call_dir_names={
  [0x01] = "MO",
  [0x02] = "MT",
}

QmiVoice.reject_cause_names ={
  ["USER_BUSY"] = 0x01,
  ["USER_REJECT"] = 0x02,
  ["LOW_BATTERY"] = 0x03,
  ["BLACKLISTED_CALL_ID"] = 0x04,
  ["DEAD_BATTERY"] = 0x05,
}

QmiVoice.voice_reason_names ={
  ["UNCONDITIONAL"] = 0x01,
  ["BUSY"] = 0x02,
  ["NOANSWER"] = 0x03,
  ["UNREACHABLE"] = 0x04,
  ["CALLWAITING"] = 0x0f,
  ["ALLFORWARDING"] = 0x05,
  ["ALLCONDITIONAL"] = 0x06,
}

QmiVoice.voice_service_names = {
  ["ACTIVATE"] = 0x01,
  ["DEACTIVATE"] = 0x02,
  ["REGISTER"] = 0x03,
  ["ERASE"] = 0x04,
}

QmiVoice.service_status_names = {
  [0x00] = "INACTIVE",
  [0x01] = "ACTIVE",
}

QmiVoice.sups_type_names={
  ["RELEASE_HELD_OR_WAITING"] = "SUPS_TYPE_RELEASE_HELD_OR_WAITING_V02",
  ["RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING"] = "SUPS_TYPE_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_V02",
  ["HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD"] = "SUPS_TYPE_HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD_V02",
  ["HOLD_ALL_EXCEPT_SPECIFIED_CALL"] = "SUPS_TYPE_HOLD_ALL_EXCEPT_SPECIFIED_CALL_V02",
  ["MAKE_CONFERENCE_CALL"] = "SUPS_TYPE_MAKE_CONFERENCE_CALL_V02",
  ["EXPLICIT_CALL_TRANSFER"] = "SUPS_TYPE_EXPLICIT_CALL_TRANSFER_V02",
  ["CCBS_ACTIVATION"] = "SUPS_TYPE_CCBS_ACTIVATION_V02",
  ["END_ALL_CALLS"] = "SUPS_TYPE_END_ALL_CALLS_V02",
  ["RELEASE_SPECIFIED_CALL"] = "SUPS_TYPE_RELEASE_SPECIFIED_CALL_V02",
  ["LOCAL_HOLD"] = "SUPS_TYPE_LOCAL_HOLD_V02",
  ["LOCAL_UNHOLD"] = "SUPS_TYPE_LOCAL_UNHOLD_V02",
}

QmiVoice.voip_sups_type_names={
  ["RELEASE_HELD_OR_WAITING"] = "VOIP_SUPS_TYPE_RELEASE_HELD_OR_WAITING_V02",
  ["RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING"] = "VOIP_SUPS_TYPE_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_V02",
  ["HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD"] = "VOIP_SUPS_TYPE_HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD_V02",
  ["MAKE_CONFERENCE_CALL"] = "VOIP_SUPS_TYPE_MAKE_CONFERENCE_CALL_V02",
  ["END_ALL_CALLS"] = "VOIP_SUPS_TYPE_END_ALL_CALLS_V02",
  ["MODIFY_CALL"] = "VOIP_SUPS_TYPE_MODIFY_CALL_V02",
  ["MODIFY_ACCEPT"] = "VOIP_SUPS_TYPE_MODIFY_ACCEPT_V02",
  ["MODIFY_REJECT"] = "VOIP_SUPS_TYPE_MODIFY_REJECT_V02",
  ["RELEASE_SPECIFIED_CALL_FROM_CONFERENCE"] = "VOIP_SUPS_TYPE_RELEASE_SPECIFIED_CALL_FROM_CONFERENCE_V02",
  ["ADD_PARTICIPANT"] = "VOIP_SUPS_TYPE_ADD_PARTICIPANT_V02",
  ["CALL_DEFLECTION"] = "VOIP_SUPS_TYPE_CALL_DEFLECTION_V02",
  ["CALL_HOLD"] = "VOIP_SUPS_TYPE_CALL_HOLD_V02",
  ["CALL_RESUME"] = "VOIP_SUPS_TYPE_CALL_RESUME_V02",
  ["MODIFY_SPEECH_CODEC"] = "VOIP_SUPS_TYPE_MODIFY_SPEECH_CODEC_V02",
}

QmiVoice.reject_cause_names={
  ["USER_BUSY"] = "VOICE_REJECT_CAUSE_USER_BUSY_V02",
  ["USER_REJECT"] = "VOICE_REJECT_CAUSE_USER_REJECT_V02",
  ["LOW_BATTERY"] = "VOICE_REJECT_CAUSE_LOW_BATTERY_V02",
  ["BLACKLISTED_CALL_ID"] = "VOICE_REJECT_CAUSE_BLACKLISTED_CALL_ID_V02",
}


QmiVoice.alerting_type_names = {
  [0x00] = "LOCAL",
  [0x01] = "REMOTE",
}

QmiVoice.call_end_reason_names = {
  [0x00] = "OFFLINE",
  [0x14] = "CDMA_LOCK",
  [0x15] = "NO_SRV",
  [0x16] = "FADE",
  [0x17] = "INTERCEPT",
  [0x18] = "REORDER",
  [0x19] = "REL_NORMAL",
  [0x1A] = "REL_SO_REJ",
  [0x1B] = "INCOM_CALL",
  [0x1C] = "ALERT_STOP",
  [0x1D] = "CLIENT_END",
  [0x1E] = "ACTIVATION",
  [0x1F] = "MC_ABORT",
  [0x20] = "MAX_ACCESS_PROBE",
  [0x21] = "PSIST_N",
  [0x22] = "UIM_NOT_PRESENT",
  [0x23] = "ACC_IN_PROG",
  [0x24] = "ACC_FAIL",
  [0x25] = "RETRY_ORDER",
  [0x26] = "CCS_NOT_SUPPORTED_BY_BS",
  [0x27] = "NO_RESPONSE_FROM_BS",
  [0x28] = "REJECTED_BY_BS",
  [0x29] = "INCOMPATIBLE",
  [0x2A] = "ACCESS_BLOCK",
  [0x2B] = "ALREADY_IN_TC",
  [0x2C] = "EMERGENCY_FLASHED",
  [0x2D] = "USER_CALL_ORIG_DURING_GPS",
  [0x2E] = "USER_CALL_ORIG_DURING_SMS",
  [0x2F] = "USER_CALL_ORIG_DURING_DATA",
  [0x30] = "REDIR_OR_HANDOFF",
  [0x31] = "ACCESS_BLOCK_ALL",
  [0x32] = "OTASP_SPC_ERR",
  [0x33] = "IS707B_MAX_ACC",
  [0x34] = "ACC_FAIL_REJ_ORD",
  [0x35] = "ACC_FAIL_RETRY_ORD",
  [0x36] = "TIMEOUT_T42",
  [0x37] = "TIMEOUT_T40",
  [0x38] = "SRV_INIT_FAIL",
  [0x39] = "T50_EXP",
  [0x3A] = "T51_EXP",
  [0x3B] = "RL_ACK_TIMEOUT",
  [0x3C] = "BAD_FL",
  [0x3D] = "TRM_REQ_FAIL",
  [0x3E] = "TIMEOUT_T41",
  [0x66] = "INCOM_REJ",
  [0x67] = "SETUP_REJ",
  [0x68] = "NETWORK_END",
  [0x69] = "NO_FUNDS",
  [0x6A] = "NO_GW_SRV",
  [0x6B] = "NO_CDMA_SRV",
  [0x6C] = "NO_FULL_SRV",
  [0x6D] = "MAX_PS_CALLS",
  [0x6E] = "UNKNOWN_SUBSCRIBER",
  [0x6F] = "ILLEGAL_SUBSCRIBER",
  [0x70] = "BEARER_SERVICE_NOT_PROVISIONED",
  [0x71] = "TELE_SERVICE_NOT_PROVISIONED",
  [0x72] = "ILLEGAL_EQUIPMENT",
  [0x73] = "CALL_BARRED",
  [0x74] = "ILLEGAL_SS_OPERATION",
  [0x75] = "SS_ERROR_STATUS",
  [0x76] = "SS_NOT_AVAILABLE",
  [0x77] = "SS_SUBSCRIPTION_VIOLATION",
  [0x78] = "SS_INCOMPATIBILITY",
  [0x79] = "FACILITY_NOT_SUPPORTED",
  [0x7A] = "ABSENT_SUBSCRIBER",
  [0x7B] = "SHORT_TERM_DENIAL",
  [0x7C] = "LONG_TERM_DENIAL",
  [0x7D] = "SYSTEM_FAILURE",
  [0x7E] = "DATA_MISSING",
  [0x7F] = "UNEXPECTED_DATA_VALUE",
  [0x80] = "PWD_REGISTRATION_FAILURE",
  [0x81] = "NEGATIVE_PWD_CHECK",
  [0x82] = "NUM_OF_PWD_ATTEMPTS_VIOLATION",
  [0x83] = "POSITION_METHOD_FAILURE",
  [0x84] = "UNKNOWN_ALPHABET",
  [0x85] = "USSD_BUSY",
  [0x86] = "REJECTED_BY_USER",
  [0x87] = "REJECTED_BY_NETWORK",
  [0x88] = "DEFLECTION_TO_SERVED_SUBSCRIBER",
  [0x89] = "SPECIAL_SERVICE_CODE",
  [0x8A] = "INVALID_DEFLECTED_TO_NUMBER",
  [0x8B] = "MPTY_PARTICIPANTS_EXCEEDED",
  [0x8C] = "RESOURCES_NOT_AVAILABLE",
  [0x8D] = "UNASSIGNED_NUMBER",
  [0x8E] = "NO_ROUTE_TO_DESTINATION",
  [0x8F] = "CHANNEL_UNACCEPTABLE",
  [0x90] = "OPERATOR_DETERMINED_BARRING",
  [0x91] = "NORMAL_CALL_CLEARING",
  [0x92] = "USER_BUSY",
  [0x93] = "NO_USER_RESPONDING",
  [0x94] = "USER_ALERTING_NO_ANSWER",
  [0x95] = "CALL_REJECTED",
  [0x96] = "NUMBER_CHANGED",
  [0x97] = "PREEMPTION",
  [0x98] = "DESTINATION_OUT_OF_ORDER",
  [0x99] = "INVALID_NUMBER_FORMAT",
  [0x9A] = "FACILITY_REJECTED",
  [0x9B] = "RESP_TO_STATUS_ENQUIRY",
  [0x9C] = "NORMAL_UNSPECIFIED",
  [0x9D] = "NO_CIRCUIT_OR_CHANNEL_AVAILABLE",
  [0x9E] = "NETWORK_OUT_OF_ORDER",
  [0x9F] = "TEMPORARY_FAILURE",
  [0xA0] = "SWITCHING_EQUIPMENT_CONGESTION",
  [0xA1] = "ACCESS_INFORMATION_DISCARDED",
  [0xA2] = "REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE",
  [0xA3] = "RESOURCES_UNAVAILABLE_OR_UNSPECIFIED",
  [0xA4] = "QOS_UNAVAILABLE",
  [0xA5] = "REQUESTED_FACILITY_NOT_SUBSCRIBED",
  [0xA6] = "INCOMING_CALLS_BARRED_WITHIN_CUG",
  [0xA7] = "BEARER_CAPABILITY_NOT_AUTH",
  [0xA8] = "BEARER_CAPABILITY_UNAVAILABLE",
  [0xA9] = "SERVICE_OPTION_NOT_AVAILABLE",
  [0xAA] = "ACM_LIMIT_EXCEEDED",
  [0xAB] = "BEARER_SERVICE_NOT_IMPLEMENTED",
  [0xAC] = "REQUESTED_FACILITY_NOT_IMPLEMENTED",
  [0xAD] = "ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE",
  [0xAE] = "SERVICE_OR_OPTION_NOT_IMPLEMENTED",
  [0xAF] = "INVALID_TRANSACTION_IDENTIFIER",
  [0xB0] = "USER_NOT_MEMBER_OF_CUG",
  [0xB1] = "INCOMPATIBLE_DESTINATION",
  [0xB2] = "INVALID_TRANSIT_NW_SELECTION",
  [0xB3] = "SEMANTICALLY_INCORRECT_MESSAGE",
  [0xB4] = "INVALID_MANDATORY_INFORMATION",
  [0xB5] = "MESSAGE_TYPE_NON_IMPLEMENTED",
  [0xB6] = "MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE",
  [0xB7] = "INFORMATION_ELEMENT_NON_EXISTENT",
  [0xB8] = "CONDITONAL_IE_ERROR",
  [0xB9] = "MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE",
  [0xBA] = "RECOVERY_ON_TIMER_EXPIRED",
  [0xBB] = "PROTOCOL_ERROR_UNSPECIFIED",
  [0xBC] = "INTERWORKING_UNSPECIFIED",
  [0xBD] = "OUTGOING_CALLS_BARRED_WITHIN_CUG",
  [0xBE] = "NO_CUG_SELECTION",
  [0xBF] = "UNKNOWN_CUG_INDEX",
  [0xC0] = "CUG_INDEX_INCOMPATIBLE",
  [0xC1] = "CUG_CALL_FAILURE_UNSPECIFIED",
  [0xC2] = "CLIR_NOT_SUBSCRIBED",
  [0xC3] = "CCBS_POSSIBLE",
  [0xC4] = "CCBS_NOT_POSSIBLE",
  [0xC5] = "IMSI_UNKNOWN_IN_HLR",
  [0xC6] = "ILLEGAL_MS",
  [0xC7] = "IMSI_UNKNOWN_IN_VLR",
  [0xC8] = "IMEI_NOT_ACCEPTED",
  [0xC9] = "ILLEGAL_ME",
  [0xCA] = "PLMN_NOT_ALLOWED",
  [0xCB] = "LOCATION_AREA_NOT_ALLOWED",
  [0xCC] = "ROAMING_NOT_ALLOWED_IN_THIS_LOCATION_AREA",
  [0xCD] = "NO_SUITABLE_CELLS_IN_LOCATION_AREA",
  [0xCE] = "NETWORK_FAILURE",
  [0xCF] = "MAC_FAILURE",
  [0xD0] = "SYNCH_FAILURE",
  [0xD1] = "NETWORK_CONGESTION",
  [0xD2] = "GSM_AUTHENTICATION_UNACCEPTABLE",
  [0xD3] = "SERVICE_NOT_SUBSCRIBED",
  [0xD4] = "SERVICE_TEMPORARILY_OUT_OF_ORDER",
  [0xD5] = "CALL_CANNOT_BE_IDENTIFIED",
  [0xD6] = "INCORRECT_SEMANTICS_IN_MESSAGE",
  [0xD7] = "MANDATORY_INFORMATION_INVALID",
  [0xD8] = "ACCESS_STRATUM_FAILURE",
  [0xD9] = "INVALID_SIM",
  [0xDA] = "WRONG_STATE",
  [0xDB] = "ACCESS_CLASS_BLOCKED",
  [0xDC] = "NO_RESOURCES",
  [0xDD] = "INVALID_USER_DATA",
  [0xDE] = "TIMER_T3230_EXPIRED",
  [0xDF] = "NO_CELL_AVAILABLE",
  [0xE0] = "ABORT_MSG_RECEIVED",
  [0xE1] = "RADIO_LINK_LOST",
  [0xE2] = "TIMER_T303_EXPIRED",
  [0xE3] = "CNM_MM_REL_PENDING",
  [0xE4] = "ACCESS_STRATUM_REJ_RR_REL_IND",
  [0xE5] = "ACCESS_STRATUM_REJ_RR_RANDOM_ACCESS_FAILURE",
  [0xE6] = "ACCESS_STRATUM_REJ_RRC_REL_IND",
  [0xE7] = "ACCESS_STRATUM_REJ_RRC_CLOSE_SESSION_IND",
  [0xE8] = "ACCESS_STRATUM_REJ_RRC_OPEN_SESSION_FAILURE",
  [0xE9] = "ACCESS_STRATUM_REJ_LOW_LEVEL_FAIL",
  [0xEA] = "ACCESS_STRATUM_REJ_LOW_LEVEL_FAIL_REDIAL_NOT_ALLOWED",
  [0xEB] = "ACCESS_STRATUM_REJ_LOW_LEVEL_IMMED_RETRY",
  [0xEC] = "ACCESS_STRATUM_REJ_ABORT_RADIO_UNAVAILABLE",
  [0xED] = "SERVICE_OPTION_NOT_SUPPORTED",
  [0xEF] = "AS_REJ_LRRC_UL_DATA_CNF_FAILURE_TXN",
  [0xF0] = "AS_REJ_LRRC_UL_DATA_CNF_FAILURE_HO",
  [0xF1] = "AS_REJ_LRRC_UL_DATA_CNF_FAILURE_CONN_REL",
  [0xF2] = "AS_REJ_LRRC_UL_DATA_CNF_FAILURE_RLF",
  [0xF3] = "AS_REJ_LRRC_UL_DATA_CNF_FAILURE_CTRL_NOT_CONN",
  [0xF4] = "AS_REJ_LRRC_CONN_EST_SUCCESS",
  [0xF5] = "AS_REJ_LRRC_CONN_EST_FAILURE",
  [0xF6] = "AS_REJ_LRRC_CONN_EST_FAILURE_ABORTED",
  [0xF7] = "AS_REJ_LRRC_CONN_EST_FAILURE_ACCESS_BARRED",
  [0xF8] = "AS_REJ_LRRC_CONN_EST_FAILURE_CELL_RESEL",
  [0xF9] = "AS_REJ_LRRC_CONN_EST_FAILURE_CONFIG_FAILURE",
  [0xFA] = "AS_REJ_LRRC_CONN_EST_FAILURE_TIMER_EXPIRED",
  [0xFB] = "AS_REJ_LRRC_CONN_EST_FAILURE_LINK_FAILURE",
  [0xFC] = "AS_REJ_LRRC_CONN_EST_FAILURE_NOT_CAMPED",
  [0xFD] = "AS_REJ_LRRC_CONN_EST_FAILURE_SI_FAILURE",
  [0xFE] = "AS_REJ_LRRC_CONN_EST_FAILURE_CONN_REJECT",
  [0xFF] = "AS_REJ_LRRC_CONN_REL_NORMAL",
  [0x100] = "AS_REJ_LRRC_CONN_REL_RLF",
  [0x101] = "AS_REJ_LRRC_CONN_REL_CRE_FAILURE",
  [0x102] = "AS_REJ_LRRC_CONN_REL_OOS_DURING_CRE",
  [0x103] = "AS_REJ_LRRC_CONN_REL_ABORTED",
  [0x104] = "AS_REJ_LRRC_CONN_REL_SIB_READ_ERROR",
  [0x105] = "AS_REJ_LRRC_CONN_REL_ABORTED_IRAT_SUCCESS",
  [0x106] = "AS_REJ_LRRC_RADIO_LINK_FAILURE",
  [0x107] = "AS_REJ_DETACH_WITH_REATTACH_LTE_NW_DETACH",
  [0x108] = "AS_REJ_DETACH_WITH_OUT_REATTACH_LTE_NW_DETACH",
  [0x12C] = "BAD_REQ_WAIT_INVITE",
  [0x12D] = "BAD_REQ_WAIT_REINVITE",
  [0x12E] = "INVALID_REMOTE_URI",
  [0x12F] = "REMOTE_UNSUPP_MEDIA_TYPE",
  [0x130] = "PEER_NOT_REACHABLE",
  [0x131] = "NETWORK_NO_RESP_TIME_OUT",
  [0x132] = "NETWORK_NO_RESP_HOLD_FAIL",
  [0x133] = "DATA_CONNECTION_LOST",
  [0x134] = "UPGRADE_DOWNGRADE_REJ",
  [0x135] = "SIP_403_FORBIDDEN",
  [0x136] = "NO_NETWORK_RESP",
  [0x137] = "UPGRADE_DOWNGRADE_FAILED",
  [0x138] = "UPGRADE_DOWNGRADE_CANCELLED",
  [0x139] = "SSAC_REJECT",
  [0x13A] = "THERMAL_EMERGENCY",
  [0x13B] = "1XCSFB_SOFT_FAILURE",
  [0x13C] = "1XCSFB_HARD_FAILURE",
  [0x13D] = "CONNECTION_EST_FAILURE",
  [0x13E] = "CONNECTION_FAILURE",
  [0x13F] = "RRC_CONN_REL_NO_MT_SETUP",
  [0x140] = "ESR_FAILURE",
  [0x141] = "MT_CSFB_NO_RESPONSE_FROM_NW",
  [0x142] = "BUSY_EVERYWHERE",
  [0x143] = "ANSWERED_ELSEWHERE",
  [0x144] = "RLF_DURING_CC_DISCONNECT",
  [0x145] = "TEMP_REDIAL_ALLOWED",
  [0x146] = "PERM_REDIAL_NOT_NEEDED",
  [0x147] = "MERGED_TO_CONFERENCE",
  [0x148] = "LOW_BATTERY",
  [0x149] = "CALL_DEFLECTED",
  [0x14A] = "RTP_RTCP_TIMEOUT",
  [0x14B] = "RINGING_RINGBACK_TIMEOUT",
  [0x14C] = "REG_RESTORATION",
  [0x14D] = "CODEC_ERROR",
  [0x14E] = "UNSUPPORTED_SDP",
  [0x14F] = "RTP_FAILURE",
  [0x151] = "MULTIPLE_CHOICES",
  [0x152] = "MOVED_PERMANENTLY",
  [0x153] = "MOVED_TEMPORARILY",
  [0x154] = "USE_PROXY",
  [0x155] = "ALTERNATE_SERVICE",
  [0x156] = "ALTERNATE_EMERGENCY_CALL",
  [0x157] = "UNAUTHORIZED",
  [0x158] = "PAYMENT_REQUIRED",
  [0x159] = "METHOD_NOT_ALLOWED",
  [0x15A] = "NOT_ACCEPTABLE",
  [0x15B] = "PROXY_AUTHENTICATION_REQUIRED",
  [0x15C] = "GONE",
  [0x15D] = "REQUEST_ENTITY_TOO_LARGE",
  [0x15E] = "REQUEST_URI_TOO_LARGE",
  [0x15F] = "UNSUPPORTED_URI_SCHEME",
  [0x160] = "BAD_EXTENSION",
  [0x161] = "EXTENSION_REQUIRED",
  [0x162] = "INTERVAL_TOO_BRIEF",
  [0x163] = "CALL_OR_TRANS_DOES_NOT_EXIST",
  [0x164] = "LOOP_DETECTED",
  [0x165] = "TOO_MANY_HOPS",
  [0x166] = "ADDRESS_INCOMPLETE",
  [0x167] = "AMBIGUOUS",
  [0x168] = "REQUEST_TERMINATED",
  [0x169] = "NOT_ACCEPTABLE_HERE",
  [0x16A] = "REQUEST_PENDING",
  [0x16B] = "UNDECIPHERABLE",
  [0x16C] = "SERVER_INTERNAL_ERROR",
  [0x16D] = "NOT_IMPLEMENTED",
  [0x16E] = "BAD_GATEWAY",
  [0x16F] = "SERVER_TIME_OUT",
  [0x170] = "VERSION_NOT_SUPPORTED",
  [0x171] = "MESSAGE_TOO_LARGE",
  [0x172] = "DOES_NOT_EXIST_ANYWHERE",
  [0x173] = "SESS_DESCR_NOT_ACCEPTABLE",
  [0x174] = "SRVCC_END_CALL",
  [0x175] = "INTERNAL_ERROR",
  [0x176] = "SERVER_UNAVAILABLE",
  [0x177] = "PRECONDITION_FAILURE",
  [0x178] = "DRVCC_IN_PROG",
  [0x179] = "DRVCC_END_CALL",
  [0x17A] = "CS_HARD_FAILURE",
  [0x17B] = "CS_ACQ_FAILURE",
  [0x17C] = "FALLBACK_TO_CS",
  [0x17D] = "DEAD_BATTERY",
  [0x17E] = "HO_NOT_FEASIBLE",
  [0x17F] = "PDN_DISCONNECTED",
  [0x180] = "REJECTED_ELSEWHERE",
  [0x181] = "CALL_PULLED",
  [0x182] = "CALL_PULL_OUT_OF_SYNC",
  [0x183] = "HOLD_RESUME_FAILED",
  [0x184] = "HOLD_RESUME_CANCELED",
  [0x185] = "REINVITE_COLLISION",
  [0x1F4] = "1XCSFB_MSG_INVAILD",
  [0x1F5] = "1XCSFB_MSG_IGNORE",
  [0x1F6] = "1XCSFB_FAIL_ACQ_FAIL",
  [0x1F7] = "1XCSFB_FAIL_CALL_REL_REL_ORDER",
  [0x1F8] = "1XCSFB_FAIL_CALL_REL_REORDER",
  [0x1F9] = "1XCSFB_FAIL_CALL_REL_INTERCEPT_ORDER",
  [0x1FA] = "1XCSFB_FAIL_CALL_REL_NORMAL",
  [0x1FB] = "1XCSFB_FAIL_CALL_REL_SO_REJ",
  [0x1FC] = "1XCSFB_FAIL_CALL_REL_OTASP_SPC_ERR",
  [0x1FD] = "1XCSFB_FAILURE_SRCH_TT_FAIL",
  [0x1FE] = "1XCSFB_FAILURE_TCH_INIT_FAIL",
  [0x1FF] = "1XCSFB_FAILURE_FAILURE_USER_CALL_END",
  [0x200] = "1XCSFB_FAILURE_FAILURE_RETRY_EXHAUST",
  [0x201] = "1XCSFB_FAILURE_FAILURE_CALL_REL_REG_REJ",
  [0x202] = "1XCSFB_FAILURE_FAILURE_CALL_REL_NW_REL_ODR",
  [0x203] = "1XCSFB_HO_FAILURE",
  [0x258] = "EMM_REJ_TIMER_T3417_EXT_EXP",
  [0x259] = "EMM_REJ_TIMER_T3417_EXP",
  [0x25A] = "EMM_REJ_SERVICE_REQ_FAILURE_LTE_NW_REJECT",
  [0x25B] = "EMM_REJ_SERVICE_REQ_FAILURE_CS_DOMAIN_NOT_AVAILABLE",
  [0x25C] = "EMM_REJ",
}


QmiVoice.dtmf_event_names = {
  [0x00] = "REV_BURST",
  [0x01] = "REV_START_CONT",
  [0x03] = "REV_STOP_CONT",
  [0x05] = "FWD_BURST",
  [0x06] = "FWD_START_CONT",
  [0x07] = "FWD_STOP_CONT",
  [0x08] = "IP_INCOMING_DTMF_START",
  [0x09] = "IP_INCOMING_DTMF_STOP",
}

QmiVoice.rtp_dtmf_to_str = {
  [0] = "0",
  [1] = "1",
  [2] = "2",
  [3] = "3",
  [4] = "4",
  [5] = "5",
  [6] = "6",
  [7] = "7",
  [8] = "8",
  [9] = "9",
  [10] = "*",
  [11] = "#",
  [12] = "A",
  [13] = "B",
  [14] = "C",
  [15] = "D",
}

-- convert ffi c array into table
function QmiVoice:conv_carray_to_table(c_array,len)

  local t={}

  if len>0 then
    for i = 0,len-1 do
      table.insert(t,c_array[i])
    end
  end

  return t
end

--[[
	* convert UTF-16 to ASCII

	RG handles ASCII only, not handling special char in UTF-16
]]--
function QmiVoice:conv_utf16_to_ascii(str_in_utf16)
  local ascii = {}
  local hi
  local lo

  for _,v in ipairs(str_in_utf16) do
    hi = self.bit.band(v,0xff80)
    lo = self.bit.band(v,0x007f)

    if (hi == 0) and (lo ~= 0) then
      table.insert(ascii,string.char(lo))
    end
  end

  return table.concat(ascii)
end

--[[ QMI notification ]]--

function QmiVoice:QMI_VOICE_DTMF_IND(type, event, qm)
  self.l.log("LOG_INFO","QMI_VOICE_DTMF_IND received")

  --luaq.log_cdata(string.format("[%s] resp", "QMI_VOICE_DTMF_IND"), qm.resp)

  local dtmf_event = qm.resp.dtmf_info.dtmf_event
  local dtmf_len = tonumber(qm.resp.dtmf_info.digit_buffer_len)
  local dtmf_digits = qm.resp.dtmf_info.digit_buffer
  local call_id = tonumber(qm.resp.dtmf_info.call_id)

  -- get DTMF volume
  local dtmf_volume
  if self.luaq.is_c_true(qm.resp.volume_valid) then
    dtmf_volume = tonumber(qm.resp.volume)
  end

  if ((dtmf_event == "DTMF_EVENT_IP_INCOMING_DTMF_START_V02") or (dtmf_event == "DTMF_EVENT_IP_INCOMING_DTMF_STOP_V02")) and (dtmf_len>0) then
    local a={
      call_id=call_id,
      action=self.dtmf_event_names[tonumber(dtmf_event)],
      digit=string.char(dtmf_digits[0]),
      volume=dtmf_volume,
    }

    self.l.log("LOG_DEBUG",string.format("call_id=%d,action=%s,dtmf=%d,digit='%s',volume=%d",a.call_id,a.action,tonumber(dtmf_digits[0]),a.digit,dtmf_volume or -1))
    self.watcher.invoke("sys","modem_on_dtmf",a)
  end

end

function QmiVoice:QMI_VOICE_ALL_CALL_STATUS_IND(type, event, qm)
  self.l.log("LOG_INFO","QMI_VOICE_ALL_CALL_STATUS_IND received")

  --luaq.log_cdata(string.format("[%s] resp", "QMI_VOICE_ALL_CALL_STATUS_IND"), qm.resp)

  --[[
  -- emulate [private call id] - AT&T public network
  qm.resp.remote_party_number_valid = 1;
  qm.resp.remote_party_number_len = 6;

  local str
  for i=0,5 do
    qm.resp.remote_party_number[i].call_id = i;
    str = ""
    qm.resp.remote_party_number[i].number = str
    qm.resp.remote_party_number[i].number_len = #str;
    qm.resp.remote_party_number[i].number_pi = 1;
  end
  ]]--

  local rparties_number={}

  -- remote party number
  if self.luaq.is_c_true(qm.resp.remote_party_number_valid) then
    for i = 0 , qm.resp.remote_party_number_len-1 do

      local remote_party_number = qm.resp.remote_party_number[i]
      local call_id = tonumber(remote_party_number.call_id)
      local number = self.ffi.string(remote_party_number.number,remote_party_number.number_len)
      local number_pi=self.number_pi_names[tonumber(remote_party_number.number_pi)]

      local rparity_number={
        number=number,
        number_pi=number_pi,
      }

      rparties_number[call_id]=rparity_number
    end
  end

  --[[
  -- emulate [remote party name]
  qm.resp.remote_party_name_valid = 1;
  qm.resp.remote_party_name_len = 6;

  local str
  for i=0,5 do
    qm.resp.remote_party_name[i].call_id = i;
    str = string.format("hello myself %d",i)
    qm.resp.remote_party_name[i].name = str
    qm.resp.remote_party_name[i].name_len = #str;
    qm.resp.remote_party_name[i].name_pi = 0;
  end
  ]]--

  -- remote party name
  local rparties_name={}
  if self.luaq.is_c_true(qm.resp.remote_party_name_valid) then
    for i = 0 , qm.resp.remote_party_name_len-1 do

      local remote_party_name = qm.resp.remote_party_name[i]
      local call_id = tonumber(remote_party_name.call_id)
      local name = self.ffi.string(remote_party_name.name,remote_party_name.name_len)
      local name_pi=self.number_pi_names[tonumber(remote_party_name.name_pi)]

      local rparity_name={
        name=name,
        name_pi=name_pi,
      }

      rparties_name[call_id]=rparity_name
    end
  end

  --[[
       * caller name example
       voice_all_call_status_indTlvs[2] {
          Type = 0x2D
          Length = 33
          ip_caller_name {
             num_instances = 1
             ip_caller_name[0] {
                call_id = 1
                ip_caller_name_len = 15
                ip_caller_name = {
                   84, 101, 115, 116, 105, 110, 103, 32,
                   68, 101, 109, 111, 44, 32, 65
                }
             }
          }
       }
  ]]--


  --[[
  -- emulate [ip caller name] - AT&T string call id
  qm.resp.ip_caller_name_valid = 1;
  qm.resp.ip_caller_name_len = 6;

  local str
  for i=0,5 do
    qm.resp.ip_caller_name[i].call_id = i;
    qm.resp.ip_caller_name[i].ip_caller_name = { 84, 101, 115, 116, 105, 110, 103, 32, 68, 101, 109, 111, 44, 32, 65 }
    qm.resp.ip_caller_name[i].ip_caller_name_len = 15
  end
  ]]--

  -- caller name for ip call
  if self.luaq.is_c_true(qm.resp.ip_caller_name_valid) then
    for i = 0 , qm.resp.ip_caller_name_len-1 do

      local ip_caller_name = qm.resp.ip_caller_name[i]
      local call_id = tonumber(ip_caller_name.call_id)
      local cname = self:conv_carray_to_table(ip_caller_name.ip_caller_name,ip_caller_name.ip_caller_name_len)
      local name = self:conv_utf16_to_ascii(cname)
      local name_pi=self.number_pi_names[0] -- ALLOW

      local rparity_name={
        name=name,
        name_pi=name_pi,
      }

      rparties_name[call_id]=rparity_name
    end
  end

  -- alerting type
  local atype={}
  if self.luaq.is_c_true(qm.resp.alerting_type_valid) then
    for i = 0 , qm.resp.alerting_type_len-1 do

      local alerting_type = qm.resp.alerting_type[i]

      atype[tonumber(alerting_type.call_id)]= self.alerting_type_names[tonumber(alerting_type.alerting_type)]
    end

  end

  -- collect end call reasons
  local call_end_reasons={}
  if self.luaq.is_c_true(qm.resp.call_end_reason) then
    for i = 0 , qm.resp.call_end_reason_len-1 do
      local call_end_reason_struc = qm.resp.call_end_reason[i]
      local call_id = tonumber(call_end_reason_struc.call_id)
      local call_end_reason = tonumber(call_end_reason_struc.call_end_reason)

      call_end_reasons[call_id] = self.call_end_reason_names[call_end_reason] or (call_end_reason and tostring(call_end_reason))
    end
  end

  -- call info
  for i = 0, qm.resp.call_info_len-1 do

    local call_info = qm.resp.call_info[i]

    --[[
    -- emulate [early media] - AT&T test cases
    -- change "CONVERSATION" into "LOCAL ALERTING"
    if call_state_names[tonumber(call_info.call_state)] == "CONVERSATION" then
      call_info.call_state = "CALL_STATE_ALERTING_V02"
      atype[1] = "LOCAL"

      local t = require("turbo")
      local il = t.ioloop.instance()

      local timeout = t.util.gettimemonotonic()+10000
      local ref=il:add_timeout(timeout,
        function()

          local a = {
            call_id=1,
            call_state="CONVERSATION",
            call_type=call_type_names[tonumber(call_info.call_type)],
            call_dir=call_dir_names[tonumber(call_info.direction)],
            alert_type = atype[call_id] or "NONE",
          }

          watcher.invoke("sys","modem_on_call",a)
        end
      )

    end
    ]]--

    local call_id = tonumber(call_info.call_id)
    local call_state = self.call_state_names[tonumber(call_info.call_state)]
    local call_type = self.call_type_names[tonumber(call_info.call_type)]
    local call_dir = self.call_dir_names[tonumber(call_info.direction)]

    local a = {
      call_id=call_id,
      call_state=call_state,
      call_type=call_type,
      call_dir=call_dir,
      --alert_type = "LOCAL",
      alert_type = atype[call_id] or "NONE",
      call_end_reasons = call_end_reasons[call_id] or "NONE"
    }

    if rparties_name[call_id] then
      a.name = rparties_name[call_id].name
      a.name_pi = rparties_name[call_id].name_pi
    end

    if rparties_number[call_id] then
      a.number = rparties_number[call_id].number
      a.number_pi = rparties_number[call_id].number_pi

      -- put "anonymous" back due to Qualcomm IMS behaviour replacing number with blank for restricted call id
      if a.number_pi == "RESTRICTED" then
        a.number = "anonymous"

        --[[

  -- do not override name fields
  -- let RG decide what to use for name or Qualcomm IMS stack decide

        a.name = "Anonymous"
        a.name_pi = number_pi_names[1] -- RESTRICTED

  ]]--
      end
    end

    self.watcher.invoke("sys","modem_on_call",a)
  end


  return true
end

QmiVoice.cbs={
  "QMI_VOICE_DTMF_IND",

  --[[
  QMI_VOICE_ADDITIONAL_CALL_INFO_IND=function(type, event, qm)
    l.log("LOG_INFO","QMI_VOICE_ADDITIONAL_CALL_INFO_IND received")
    luaq.log_cdata(string.format("[%s] resp", "QMI_VOICE_ADDITIONAL_CALL_INFO_IND"), qm.resp)
  end,
  ]]--

  "QMI_VOICE_ALL_CALL_STATUS_IND",
}

--[[ commands ]]--

function QmiVoice:stop_dtmf(type,event,a)
  local qm = self.luaq.new_msg(self.m.QMI_VOICE_STOP_CONT_DTMF)
  qm.req.call_id=a.call_id

  self.l.log("LOG_INFO", string.format("stop  DTMF (cid=%d)",a.call_id))

  if not self.luaq.send_msg(qm) then
    return
  end

  return self.luaq.ret_qm_resp(qm)
end

function QmiVoice:start_dtmf(type,event,a)
  local qm = self.luaq.new_msg(self.m.QMI_VOICE_START_CONT_DTMF)
  qm.req.cont_dtmf_info.call_id=a.call_id
  qm.req.cont_dtmf_info.digit=string.byte(a.digit)

  self.l.log("LOG_INFO", string.format("start  DTMF (cid=%d,digit=%s)",a.call_id,a.digit))

  if not self.luaq.send_msg(qm) then
    return
  end

  return self.luaq.ret_qm_resp(qm)
end

function QmiVoice:set_sups_service(type,event,a)
  local qm = self.luaq.new_msg(self.m.QMI_VOICE_SET_SUPS_SERVICE,a.qmi_lua_sid)

  qm.req.supplementary_service_info.reason = self.voice_reason_names[a.reason]
  qm.req.supplementary_service_info.voice_service = self.voice_service_names[a.service]

  if a.number then
    qm.req.number_valid = true
    qm.req.number = a.number
  end

  if a.no_reply_timer then
    qm.req.timer_value.timer_value_valid = true
    qm.req.timer_value = a.no_reply_timer
  end

  -- send
  if not self.luaq.send_msg(qm) then
    return
  end

  return self.luaq.ret_qm_resp(qm)
end


function QmiVoice:get_call_waiting(type,event,a)

  self.l.log("LOG_DEBUG","QMI_VOICE_GET_CALL_WAITING: request")

  -- VOICE_SUPS_CLASS_VOICE_V02
  local succ,qerr,resp,qm = self.luaq.req(self.m.QMI_VOICE_GET_CALL_WAITING,{},0,a.qmi_lua_sid)

  if not succ then
    self.l.log("LOG_ERR","QMI_VOICE_GET_CALL_WAITING: request failed")

  elseif self.luaq.is_c_true(resp.service_class_valid) then

    local service_class = tonumber(resp.service_class) or 0
    self.l.log("LOG_DEBUG",string.format("QMI_VOICE_GET_CALL_WAITING: got service class (val=%d)",service_class))

    -- check to see if voice sups class is active
    succ = bit.band(service_class,1) ~= 0
  else
    succ = false
  end

  return succ
end

function QmiVoice:get_call_forwarding(type,event,a)
  local succ,qerr,resp,qm = self.luaq.req(self.m.QMI_VOICE_GET_CALL_FORWARDING,{reason=self.voice_reason_names[a.reason]},0,a.qmi_lua_sid)

  if self.luaq.is_c_true(resp.get_call_forwarding_info_valid) and (resp.get_call_forwarding_info_len>0) then
    local call_forwarding_info = resp.get_call_forwarding_info[0]

    a.status = self.service_status_names[tonumber(call_forwarding_info.service_status)]
    a.service_class = tonumber(call_forwarding_info.service_class)
    a.no_reply_timer = tonumber(call_forwarding_info.no_reply_timer)
    a.number=call_forwarding_info.number
  else
    succ = false
  end

  return succ
end

-- post QMI_VOICE_ANSWER_CALL to reject a call with a reject cause
function QmiVoice:reject_call(type,event,a)
  local succ,qerr,resp = self.luaq.req(self.m.QMI_VOICE_MANAGE_CALLS,{
    call_id=(a.call_id and (a.call_id>0)) and a.call_id or nil,
    sups_type=self.sups_type_names["RELEASE_SPECIFIED_CALL"],
    reject_cause=self.reject_cause_names[a.reject_cause],
  })

  return succ,qerr,resp
end

function QmiVoice:answer_call(type,event,a)
  local params = {
    call_id=a.call_id,
    reject_call=a.reject_cause and 1,
    reject_cause=self.reject_cause_names[a.reject_cause],
  }

  return self.luaq.req(self.m.QMI_VOICE_ANSWER_CALL,params,0,a.qmi_lua_sid)
end

function QmiVoice:setup_answer(type,event,a)
  local params = {
    call_id=a.call_id,
    reject_setup=not a.reject_cause and 0 or 1,
    reject_cause=self.reject_cause_names[a.reject_cause]
  }

  return self.luaq.req(self.m.QMI_VOICE_SETUP_ANSWER,params,0,a.qmi_lua_sid)
end

function QmiVoice:end_call(type,event,a)
  local params = {
    call_id=a.call_id,
    end_cause=self.reject_cause_names[a.end_cause],
  }


  return self.luaq.req(self.m.QMI_VOICE_END_CALL,params,0,a.qmi_lua_sid)
end

function QmiVoice:dial_call(type,event,a)
  local succ,qerr,resp = self.luaq.req(self.m.QMI_VOICE_DIAL_CALL,{calling_number=a.calling_number},a.timeout,a.qmi_lua_sid)
  if succ and self.luaq.is_c_true(resp.call_id_valid) then
    a.call_id = resp.call_id
  end

  return succ,qerr,resp
end

function QmiVoice:dial_pcall(type,event,a)
  local params = {
    calling_number=a.calling_number,
    clir_type="CLIR_INVOCATION_V02",
    pi="IP_PRESENTATION_NUM_RESTRICTED_V02"
  }

  local succ,qerr,resp = self.luaq.req(self.m.QMI_VOICE_DIAL_CALL,params,a.timeout,a.qmi_lua_sid)
  if succ and self.luaq.is_c_true(resp.call_id_valid) then
    a.call_id = resp.call_id
  end

  return succ,qerr,resp
end

function QmiVoice:dial_ocall(type,event,a)
  local params = {
    calling_number=a.calling_number,
    clir_type="CLIR_SUPPRESSION_V02",
    pi="IP_PRESENTATION_NUM_ALLOWED_V02"
  }

  local succ,qerr,resp = self.luaq.req(self.m.QMI_VOICE_DIAL_CALL,params,a.timeout,a.qmi_lua_sid)
  if succ and self.luaq.is_c_true(resp.call_id_valid) then
    a.call_id = resp.call_id
  end

  return succ,qerr,resp
end

function QmiVoice:dial_ecall(type,event,a)
  local succ,qerr,resp = self.luaq.req(self.m.QMI_VOICE_DIAL_CALL,{call_type="CALL_TYPE_EMERGENCY_V02", calling_number=a.calling_number},a.timeout,a.qmi_lua_sid)
  if succ and self.luaq.is_c_true(resp.call_id_valid) then
    a.call_id = resp.call_id
  end

  return succ,qerr,resp
end

function QmiVoice:manage_calls(type,event,a)

  local succ,qerr,resp = self.luaq.req(self.m.QMI_VOICE_MANAGE_CALLS,{
    call_id=(a.call_id and (a.call_id>0)) and a.call_id or nil,
    sups_type=self.sups_type_names[a.sups_type],
    reject_cause=self.reject_cause_names[a.reject_cause],
  })

  return succ,qerr,resp
end

function QmiVoice:manage_ip_calls(type,event,a)

  local succ,qerr,resp = self.luaq.req(self.m.QMI_VOICE_MANAGE_IP_CALLS,{
    call_id=(a.call_id and (a.call_id>0)) and a.call_id or nil,
    sups_type=self.voip_sups_type_names[a.sups_type],
    reject_cause=self.reject_cause_names[a.reject_cause],
  })

  return succ,qerr,resp
end

-------------------------------------------------------------------------------------------------------------------
-- Set TTY mode
--
-- @param type Watcher invoke type.
-- @param event Watcher invoke event or command.
-- @param params Watcher invoke parameters
function QmiVoice:set_tty_mode(type,event,params)

  -- finalise CSD when TTY mode is applied
  self.l.log("LOG_INFO",string.format("[CSD] finalise CSD by changing TTY mode (tty_mode=%s)",params.tty_mode))
  self.watcher.invoke("sys","fini_voice_call",{force_to_fini=true})

  self.l.log("LOG_INFO",string.format("set QMI set_tty_mode (tty_mode=%s)",params.tty_mode))

  -- create a new message for QMI request
  local qm = self.luaq.new_msg(self.m.QMI_VOICE_SET_CONFIG,params.qmi_lua_sid)

  -- get tty mode.
  local tty_mode = self.tty_mode_numbers[params.tty_mode]
  if not tty_mode then
    self.l.log("LOG_ERR",string.format("unknown TTY mode provided, disable TTY (tty_mode=%s)",params.tty_mode))
    tty_mode = self.tty_mode_numbers["OFF"]
  end

  -- store TTY mode to RDB
  if not params.do_not_store_to_rdb then
    local rdb_tty_mode =  params.tty_mode or "OFF"
    self.l.log("LOG_DEBUG",string.format("store TTY mode to RDB (rdb_tty_mode=%s)",rdb_tty_mode))
    self.wrdb:set("wmmd.config.tty_mode",rdb_tty_mode)
  end


  -- build QMI request parameter
  qm.req.tty_mode_valid = 1
  qm.req.tty_mode = tty_mode

  --[[

  * Android UI information only. We do not need this setting.

  qm.req.ui_tty_setting_valid = 1
  qm.req.ui_tty_setting = tty_mode

]]--

  -- send
  if not self.luaq.send_msg(qm) then
    return
  end

  -- update csd TTY mode
  self:invoke_set_csd_tty_mode(params.tty_mode)

  return self.luaq.ret_qm_resp(qm)
end

function QmiVoice:poll(type, event)
  self.l.log("LOG_DEBUG","qmi voice poll")

  local succ,err,resp

  -- [TODO] do not read multiple times

  return true
end

function QmiVoice:stop(type, event)
  self.l.log("LOG_INFO","qmi voice stop")
end


QmiVoice.cbs_system={
  "stop_dtmf",
  "start_dtmf",
  "set_sups_service",
  "get_call_forwarding",
  "get_call_waiting",
  -- post QMI_VOICE_ANSWER_CALL to reject a call with a reject cause
  "reject_call",
  "answer_call",
  "setup_answer",
  "dial_call",
  "dial_ecall",
  "dial_pcall",
  "dial_ocall",
  "end_call",
  "manage_calls",
  "manage_ip_calls",
  "poll",
  "stop",
  "set_tty_mode",
}


function QmiVoice:init()

  self.l.log("LOG_INFO", "initiate qmi_voice")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  -- schedule to apply initiate CSD status
  ioloop:add_callback(function()

      -- restore TTY mode from RDB
      local rdb_val = self.wrdb:get("wmmd.config.tty_mode")
      local rdb_tty_mode = (not rdb_val or rdb_val == "") and "OFF" or rdb_val

      self.l.log("LOG_INFO", string.format("restore TTY mode from RDB (rdb_tty_mode=%s)",rdb_tty_mode))

      self.watcher.invoke("sys","set_tty_mode",{tty_mode=rdb_tty_mode,do_not_store_to_rdb=true})
  end
  )

  -- register indications
  local succ,qerr,resp = self.luaq.req(self.m.QMI_VOICE_INDICATION_REGISTER,{
    reg_dtmf_events=1,
    call_events=1,
  --conference_events=1,
  })

end

return QmiVoice
