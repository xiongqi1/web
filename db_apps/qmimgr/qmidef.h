#ifndef __QMIDEF_H__
#define __QMIDEF_H__

#include "minilib.h"

/* total count of service types to create */
#define QMIUNICLIENT_SERVICE_CLIENT 18


// SERVICE ID
////////////////////////////////////////////////////////////////////////////////
// Control service
#define QMICTL 0
// Wireless data service
#define QMIWDS 1
// Device management service
#define QMIDMS 2
// Network access service
#define QMINAS 3
// Quality of service, err, service
#define QMIQOS 4
// Wireless messaging service
#define QMIWMS 5
// Position determination service
#define QMIPDS 6
// QMI voice service
#define QMIVOICE 9
// QMI user identity module service
#define QMIUIM 11
// QMI Location Service
#define QMILOC 16
// IPV6 connection service
#define QMIIPV6 17

// MESSAGE type
////////////////////////////////////////////////////////////////////////////////

#define QMI_MSGTYPE_REQ		0
#define QMI_MSGTYPE_RESP	2
#define QMI_MSGTYPE_INDI	4


// Common
////////////////////////////////////////////////////////////////////////////////
#define IPV6    0x6
#define IPV4    0x4

#define QMI_RESP_RESULT_CODE_TYPE	0x02
#define QMI_RESP_RESULT_EXTCODE_TYPE	0xe0

#define QMI_RESULT_CODE_SUCCESS		0x0000
#define QMI_RESULT_CODE_FAILURE		0x0001

#define QMI_RESP_TYPE_RETURN		0x10

#define QMI_MAX_INTERFACE		4

struct qmi_resp_result {
	unsigned short qmi_result;
	unsigned short qmi_error;
} __packed;

	// qmi error code
	#define QMI_ERR_NONE 0x0000
	#define QMI_ERR_MALFORMED_MSG 0x0001
	#define QMI_ERR_NO_MEMORY 0x0002
	#define QMI_ERR_INTERNAL 0x0003
	#define QMI_ERR_ABORTED 0x0004
	#define QMI_ERR_CLIENT_IDS_EXHAUSTED 0x0005
	#define QMI_ERR_UNABORTABLE_TRANSACTION 0x0006
	#define QMI_ERR_INVALID_CLIENT_ID 0x0007
	#define QMI_ERR_INVALID_HANDLE 0x0009
	#define QMI_ERR_INVALID_PROFILE 0x000A
	#define QMI_ERR_INVALID_PINID 0x000B
	#define QMI_ERR_INCORRECT_PIN 0x000C
	#define QMI_ERR_NO_NETWORK_FOUND 0x000D
	#define QMI_ERR_CALL_FAILED 0x000E
	#define QMI_ERR_OUT_OF_CALL 0x000F
	#define QMI_ERR_NOT_PROVISIONED 0x0010
	#define QMI_ERR_MISSING_ARG 0x0011
	#define QMI_ERR_ARG_TOO_LONG 0x0013
	#define QMI_ERR_INVALID_TX_ID 0x0016
	#define QMI_ERR_DEVICE_IN_USE 0x0017
	#define QMI_ERR_OP_NETWORK_UNSUPPORTED 0x0018
	#define QMI_ERR_OP_DEVICE_UNSUPPORTED 0x0019
	#define QMI_ERR_NO_EFFECT 0x001A
	#define QMI_ERR_NO_FREE_PROFILE 0x001B
	#define QMI_ERR_INVALID_PDP_TYPE 0x001C
	#define QMI_ERR_INVALID_TECH_PREF 0x001D
	#define QMI_ERR_INVALID_PROFILE_TYPE 0x001E
	#define QMI_ERR_INVALID_SERVICE_TYPE 0x001F
	#define QMI_ERR_INVALID_REGISTER_ACTION 0x0020
	#define QMI_ERR_INVALID_PS_ATTACH_ACTION 0x0021
	#define QMI_ERR_AUTHENTICATION_FAILED 0x0022
	#define QMI_ERR_PIN_BLOCKED 0x0023
	#define QMI_ERR_PIN_PERM_BLOCKED 0x0024
	#define QMI_ERR_UIM_NOT_INITIALIZED 0x0025
	#define QMI_ERR_MAX_QOS_REQUESTS_IN_USE 0x0026
	#define QMI_ERR_INCORRECT_FLOW_FILTER 0x0027
	#define QMI_ERR_NETWORK_QOS_UNAWARE 0x0028
	#define QMI_ERR_INVALID_QOS_ID 0x0029
	#define QMI_ERR_NUM_QOS_IDS 0x002A
	#define QMI_ERR_FLOW_SUSPENDED 0x002C
	#define QMI_ERR_INVALID_DATA_FORMAT 0x002D
	#define QMI_ERR_INVALID_ARG 0x0030
	#define QMI_ERR_INVALID_TRANSITION 0x003C
	#define QMI_ERR_INVALID_QMI_CMD 0x0047
	#define QMI_ERR_INFO_UNAVAILABLE 0x004A
	#define QMI_ERR_EXTENDED_INTERNAL 0x0051
	#define QMI_ERR_BUNDLING_NOT_SUPPORTED 0x004D

	// qmi call end reasons
	#define QMI_WDS_CALL_END_REASON_UNSPECIFIED 1
	#define QMI_WDS_CALL_END_REASON_CLIENT_END 2
	#define QMI_WDS_CALL_END_REASON_NO_SRV 3
	#define QMI_WDS_CALL_END_REASON_FADE 4
	#define QMI_WDS_CALL_END_REASON_REL_NORMAL 5
	#define QMI_WDS_CALL_END_REASON_ACC_IN_PROG 6
	#define QMI_WDS_CALL_END_REASON_ACC_FAIL 7
	#define QMI_WDS_CALL_END_REASON_REDIR_OR_HANDOFF 8
	#define QMI_WDS_CALL_END_REASON_CLOSE_IN_PROGRESS 9
	#define QMI_WDS_CALL_END_REASON_AUTH_FAILED 10
	#define QMI_WDS_CALL_VERBOSE_END_REASON_LLC_SNDCP_FAILURE 25
	#define QMI_WDS_CALL_VERBOSE_END_REASON_INSUFFICIENT_RESOURCES 26
	#define QMI_WDS_CALL_VERBOSE_END_REASON_UNKNOWN_APN 27
	#define QMI_WDS_CALL_VERBOSE_END_REASON_UNKNOWN_PDP 28
	#define QMI_WDS_CALL_VERBOSE_END_REASON_AUTH_FAILED 29
	#define QMI_WDS_CALL_VERBOSE_END_REASON_GGSN_REJECT 30
	#define QMI_WDS_CALL_VERBOSE_END_REASON_ACTIVATION_REJECT 31
	#define QMI_WDS_CALL_VERBOSE_END_REASON_OPTION_NOT_SUPPORTED 32
	#define QMI_WDS_CALL_VERBOSE_END_REASON_OPTION_UNSUBSCRIBED 33
	#define QMI_WDS_CALL_VERBOSE_END_REASON_OPTION_TEMP_OOO 34
	#define QMI_WDS_CALL_VERBOSE_END_REASON_NSAPI_ALREADY_USED 35
	#define QMI_WDS_CALL_VERBOSE_END_REASON_REGULAR_DEACTIVATION 36
	#define QMI_WDS_CALL_VERBOSE_END_REASON_QOS_NOT_ACCEPTED 37
	#define QMI_WDS_CALL_VERBOSE_END_REASON_NETWORK_FAILURE 38
	#define QMI_WDS_CALL_VERBOSE_END_REASON_UMTS_REACTIVATION_REQ 39
	#define QMI_WDS_CALL_END_REASON_CDMA_LOCK 500
	#define QMI_WDS_CALL_END_REASON_INTERCEPT 501
	#define QMI_WDS_CALL_END_REASON_REORDER 502
	#define QMI_WDS_CALL_END_REASON_REL_SO_REJ 503
	#define QMI_WDS_CALL_END_REASON_INCOM_CALL 504
	#define QMI_WDS_CALL_END_REASON_ALERT_STOP 505
	#define QMI_WDS_CALL_END_REASON_ACTIVATION 506
	#define QMI_WDS_CALL_END_REASON_MAX_ACCESS_PROBE 507
	#define QMI_WDS_CALL_END_REASON_CCS_NOT_SUPPORTED_BY_BS 508
	#define QMI_WDS_CALL_END_REASON_NO_RESPONSE_FROM_BS 509
	#define QMI_WDS_CALL_END_REASON_REJECTED_BY_BS 510
	#define QMI_WDS_CALL_END_REASON_INCOMPATIBLE 511
	#define QMI_WDS_CALL_END_REASON_ALREADY_IN_TC 512
	#define QMI_WDS_CALL_END_REASON_USER_CALL_ORIG_DURING_GPS 513
	#define QMI_WDS_CALL_END_REASON_USER_CALL_ORIG_DURING_SMS 514
	#define QMI_WDS_CALL_END_REASON_NO_CDMA_SRV 515
	#define QMI_WDS_CALL_END_REASON_CONF_FAILED 1000
	#define QMI_WDS_CALL_END_REASON_INCOM_REJ 1001
	#define QMI_WDS_CALL_END_REASON_NO_GW_SRV 1002
	#define QMI_WDS_CALL_END_REASON_NETWORK_END 1003
	#define QMI_WDS_CALL_END_REASON_LLC_SNDCP_FAILURE 1004
	#define QMI_WDS_CALL_END_REASON_INSUFFICIENT_RESOURCES 1005
	#define QMI_WDS_CALL_END_REASON_OPTION_TEMP_OOO 1006
	#define QMI_WDS_CALL_END_REASON_NSAPI_ALREADY_USED 1007
	#define QMI_WDS_CALL_END_REASON_REGULAR_DEACTIVATION 1008
	#define QMI_WDS_CALL_END_REASON_NETWORK_FAILURE 1009
	#define QMI_WDS_CALL_END_REASON_UMTS_ 1010
	#define QMI_WDS_CALL_END_REASON_PROTOCOL_ERROR 1011
	#define QMI_WDS_CALL_END_REASON_OPERATOR_DETERMINED_BARRING 1012
	#define QMI_WDS_CALL_END_REASON_UNKNOWN_APN 1013
	#define QMI_WDS_CALL_END_REASON_UNKNOWN_PDP 1014
	#define QMI_WDS_CALL_END_REASON_GGSN_REJECT 1015
	#define QMI_WDS_CALL_END_REASON_ACTIVATION_REJECT 1016
	#define QMI_WDS_CALL_END_REASON_OPTION_NOT_SUPPORTED 1017
	#define QMI_WDS_CALL_END_REASON_OPTION_UNSUBSCRIBED 1018
	#define QMI_WDS_CALL_END_REASON_QOS_NOT_ACCEPTED 1019
	#define QMI_WDS_CALL_END_REASON_TFT_SEMANTIC_ERROR 1020
	#define QMI_WDS_CALL_END_REASON_TFT_SYNTAX_ERROR 1021
	#define QMI_WDS_CALL_END_REASON_UNKNOWN_PDP_CONTEXT 1022
	#define QMI_WDS_CALL_END_REASON_FILTER_SEMANTIC_ERROR 1023
	#define QMI_WDS_CALL_END_REASON_FILTER_SYNTAX_ERROR 1024
	#define QMI_WDS_CALL_END_REASON_PDP_WITHOUT_ACTIVE_TFT 1025
	#define QMI_WDS_CALL_END_REASON_INVALID_TRANSACTION_ID 1026
	#define QMI_WDS_CALL_END_REASON_MESSAGE_INCORRECT_SEMANTIC 1027
	#define QMI_WDS_CALL_END_REASON_INVALID_MANDATORY_INFO 1028
	#define QMI_WDS_CALL_END_REASON_MESSAGE_TYPE_UNSUPPORTED 1029
	#define QMI_WDS_CALL_END_REASON_MSG_TYPE_NONCOMPATIBLE_STATE 1030
	#define QMI_WDS_CALL_END_REASON_UNKNOWN_INFO_ELEMENT 1031
	#define QMI_WDS_CALL_END_REASON_CONDITIONAL_IE_ERROR 1032
	#define QMI_WDS_CALL_END_REASON_MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE 1033
	#define QMI_WDS_CALL_END_REASON_APN_TYPE_CONFLICT 1034
	#define QMI_WDS_CALL_END_REASON_CD_GEN_OR_BUSY 1500
	#define QMI_WDS_CALL_END_REASON_CD_BILL_OR_AUTH 1501
	#define QMI_WDS_CALL_END_REASON_CHG_HDR 1502
	#define QMI_WDS_CALL_END_REASON_EXIT_HDR 1503
	#define QMI_WDS_CALL_END_REASON_HDR_NO_SESSION 1504
	#define QMI_WDS_CALL_END_REASON_HDR_ORIG_DURING_GPS_FIX 1505
	#define QMI_WDS_CALL_END_REASON_HDR_CS_TIMEOUT 1506
	#define QMI_WDS_CALL_END_REASON_HDR_RELEASED_BY_CM 1507




// QMI Control Service (QMICTL #1)
////////////////////////////////////////////////////////////////////////////////

#define QMI_CTL_GET_CLIENT_ID 0x0022
	#define QMI_CTL_GET_CLIENT_ID_REQ_TYPE_SERVICE_TYPE 0x01
	#define QMI_CTL_GET_CLIENT_ID_RESP_TYPE_CLIENT_ID 0x01

	struct qmi_ctl_get_client_id_req_service_type {
		unsigned char qmi_svc_type;
	} __packed;

	struct qmi_ctl_get_client_id_resp_client_id {
		unsigned char qmi_svc_type;
		unsigned char client_id;
	} __packed;

#define QMI_CTL_GET_VERSION_INFO 0x0021
	#define QMI_CTL_GET_VERSION_INFO_RESP_TYPE_SERVICE_VERSION 0x01
	struct qmi_ctl_get_version_info_resp_svc_ver_sub {
		unsigned char qmi_svc_type;
		unsigned short major_ver;
		unsigned short minor_ver;
	} __packed;
	struct qmi_ctl_get_version_info_resp_svc_ver {
		unsigned char service_version_list_len;
		struct qmi_ctl_get_version_info_resp_svc_ver_sub version[0];
	} __packed;

#define QMI_CTL_SET_READY 0x0021
#define QMI_CTL_SET_DATA_FORMAT 0x0026
	#define QMI_CTL_SET_DATA_FORMAT_REQ 0x01
	#define QMI_CTL_SET_DATA_FORMAT_REQ_LINK_PROTO 0x10

#define QMI_CTL_SET_SYNC 0x0027

// QMI Wireless Data Service (QMIWDS #1)
////////////////////////////////////////////////////////////////////////////////

#define QMI_WDS_SET_EVENT_REPORT 0x0001

#define QMI_WDS_START_NETWORK_INTERFACE 0x0020
	#define QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_PROFILE_INDEX 0x31
	#define QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_APN 0x14
	#define QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_AUTH 0x16
	#define QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_USER 0x17
	#define QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_PASS 0x18
	#define QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_IPPREF 0x19
	#define QMI_WDS_START_NETWORK_INTERFACE_REQ_TYPE_TECHNOLOGY 0x30
	#define QMI_WDS_START_NETWORK_INTERFACE_RESP_TYPE 0x01
	#define QMI_WDS_START_NETWORK_INTERFACE_RESP_TYPE_CALL_END_REASON 0x10
	#define QMI_WDS_START_NETWORK_INTERFACE_RESP_TYPE_VERBOSE_CALL_END_REASON 0x11

#define QMI_WDS_STOP_NETWORK_INTERFACE 0x0021
	#define QMI_WDS_STOP_NETWORK_INTERFACE_REQ_TYPE 0x01

#define QMI_WDS_GET_PKT_SRVC_STATUS 0x0022
	#define QMI_WDS_GET_PKT_SRVC_STATUS_RESP_TYPE 0x01
		#define QMI_WDS_PKT_DATA_DISCONNECTED 0x01
		#define QMI_WDS_PKT_DATA_CONNECTED 0x02
		#define QMI_WDS_PKT_DATA_SUSPENDED 0x03
		#define QMI_WDS_PKT_AUTHENTICATING 0x04

		struct qmi_wds_get_pkt_srvc_status_indi_pss {
			unsigned char conn_stat;
			unsigned char reconn_req;
		} __packed;

	#define QMI_WDS_GET_PKT_SRVC_STATUS_RESP_IP_FAMILY 0x012



// common profile struct
struct qmi_wds_profile_index_reqresp_t {
	unsigned char profile_type;
	unsigned char profile_index;
} __packed;

#define QMI_WDS_MODIFY_PROFILE_SETTINGS 0x0028
	#define QMI_WDS_MODIFY_PROFILE_SETTINGS_REQ_TYPE 0x01
	#define QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_PDN_TYPE 0xa2
	#define QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_APN_NAME 0xa1
	#define QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_USER 0x9b
	#define QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_PASS 0x9c
	#define QMI_WDS_MODIFY_3GPP2_PROFILE_REQRESP_TYPE_AUTH 0x9a

	// use QMI_WDS_CREATE_PROFILE request types

#define QMI_WDS_DELETE_PROFILE 0x0029
	#define QMI_WDS_DELETE_PROFILE_REQ_TYPE 0x01
		// use qmi_wds_profile_index_reqresp_t for request

#define QMI_WDS_CREATE_PROFILE 0x0027
	#define QMI_WDS_PROFILE_REQRESP_TYPE_PROFILE_TYPE 0x01
	#define QMI_WDS_PROFILE_REQRESP_TYPE_PROFILE_NAME 0x10
	#define QMI_WDS_PROFILE_REQRESP_TYPE_PDP_TYPE 0x11
	#define QMI_WDS_PROFILE_REQRESP_TYPE_APN_NAME 0x14
	#define QMI_WDS_PROFILE_REQRESP_TYPE_USER 0x1b
	#define QMI_WDS_PROFILE_REQRESP_TYPE_PASS 0x1c
	#define QMI_WDS_PROFILE_REQRESP_TYPE_AUTH 0x1d

		#define QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_PAP 1
		#define QMI_WDS_PROFILE_REQRESP_TYPE_AUTH_CHAP 2

		struct qmi_wds_profile_reqresp_auth {
			unsigned char auth_preference;
		} __packed;

	#define QMI_WDS_PROFILE_REQRESP_TYPE_IPV4_ADDR_PREF 0x1e
		struct qmi_wds_profile_reqresp_ipv4_addr_pref {
			unsigned char ipv4_addr_pref[4];
		} __packed;

	#define QMI_WDS_PROFILE_REQRESP_TYPE_TFT_ID1 0x23
	#define QMI_WDS_PROFILE_REQRESP_TYPE_TFT_ID2 0x24
		struct qmi_wds_profile_reqresp_tft_id {
			unsigned char filter_id;
			unsigned char eval_id;
			unsigned char ip_version;
			unsigned char source_ip[16];
			unsigned char source_ip_mask;
			unsigned char next_header;
			unsigned short dest_port_range_start;
			unsigned short dest_port_range_end;
			unsigned short src_port_range_start;
			unsigned short src_port_range_end;
			unsigned int ipsec_spi;
			unsigned short tos_mask;
			unsigned int flow_label;
		} __packed;

#define QMI_WDS_PROFILE_REQRESP_TYPE_PDP_CONTEXT_NUMBER 	0x25
#define QMI_WDS_PROFILE_REQRESP_TYPE_SECONDARY_FLAG		0x26
#define QMI_WDS_PROFILE_REQRESP_TYPE_PRIMARY_ID			0x27
#define QMI_WDS_PROFILE_REQRESP_TYPE_ADDR_ALLOCATION_PREFERENCE	0x2d
#define QMI_WDS_PROFILE_REQRESP_TYPE_QCI			0x2e
		struct qmi_wds_profile_reqresp_type_qci {
			unsigned char qci;
			unsigned int g_dl_bit_rate;
			unsigned int max_dl_bit_rate;
			unsigned int g_ul_bit_rate;
			unsigned int max_ul_bit_rate;
		} __packed;



	#define QMI_WDS_CREATE_PROFILE_RESP_TYPE 0x01
		// use qmi_wds_profile_index_reqresp_t for response

#define QMI_WDS_GET_PROFILE_SETTINGS 0x002b
	#define QMI_WDS_GET_PROFILE_SETTINGS_REQ_TYPE 0x01
		// use qmi_wds_profile_index_reqresp_t for request
	// use QMI_WDS_CREATE_PROFILE request types for response types




#define QMI_WDS_GET_PROFILE_LIST 0x002a
	#define QMI_WDS_GET_PROFILE_LIST_RESP_TYPE 0x01

		struct qmi_wds_get_profile_list_resp_sub {
			unsigned char profile_type;
			unsigned char profile_index;
			unsigned char profile_name_length;
			char profile_name[0];
		} __packed;

		struct qmi_wds_get_profile_list_resp {
			unsigned char num_instances;
			struct qmi_wds_get_profile_list_resp_sub profile[0];
		} __packed;

#define QMI_WDS_GET_RUNTIME_SETTINGS 0x002d
	#define QMI_WDS_GET_RUNTIME_SETTINGS_REQ_TYPE 0x10
		struct qmi_wds_get_runtime_settings_req {
			unsigned int requested_settings;
		} __packed;
        #define QMI_WDS_GET_RUNTIME_SETTINGS_MASK_PROFILE_ID 0x0001
        #define QMI_WDS_GET_RUNTIME_SETTINGS_MASK_DNS_ADDR 0x0010
        #define QMI_WDS_GET_RUNTIME_SETTINGS_MASK_IP_ADDR 0x0100
        #define QMI_WDS_GET_RUNTIME_SETTINGS_MASK_GW_ADDR 0x0200

	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_DNS1 0x15
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_DNS2 0x16
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPADDR 0x1e
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_PROFID 0x1f
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_GATEWAY 0x20
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_NETMASK 0x21
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPV6ADDR 0x25
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPV6GATEWAY 0x26
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPV6DNS1 0x27
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_IPV6DNS2 0x28
	#define QMI_WDS_GET_RUNTIME_SETTINGS_RESP_TYPE_MTU 0x29
		struct qmi_wds_get_runtime_settings_resp_type_mtu {
			unsigned int mtu;
		} __packed;

#define QMI_WDS_SET_CLIENT_IP_FAMILY_PREF  0x004D

#define QMI_WDS_BIND_MUX_DATA_PORT_REQ     0x00A2

// QMI Device Management Service (QMIDMS #2)
////////////////////////////////////////////////////////////////////////////////
#define QMI_DMS_GET_DEVICE_MFR 0x0021
	#define QMI_DMS_GET_DEVICE_MFR_RESP_TYPE 0x01

#define QMI_DMS_GET_DEVICE_MODEL_ID 0x0022
	#define QMI_DMS_GET_DEVICE_MODEL_ID_RESP_TYPE 0x01

#define QMI_DMS_GET_DEVICE_SERIAL_NUMBERS 0x0025
	#define QMI_DMS_GET_DEVICE_SERIAL_NUMBERS_RESP_TYPE_ESN 0x10
	#define QMI_DMS_GET_DEVICE_SERIAL_NUMBERS_RESP_TYPE_IMEI 0x11
	#define QMI_DMS_GET_DEVICE_SERIAL_NUMBERS_RESP_TYPE_MEID 0x12

#define QMI_DMS_GET_DEVICE_REV_ID 0x0023
	#define QMI_DMS_GET_DEVICE_REV_ID_RESP_TYPE 0x01

#define QMI_DMS_GET_MSISDN 0x0024
	#define QMI_DMS_GET_MSISDN_RESP_TYPE 0x01

#define QMI_DMS_UIM_GET_ICCID 0x003c
	#define QMI_DMS_UIM_GET_ICCID_REQ_TYPE 0x01

#define QMI_DMS_GET_IMSI 0x0043
	#define QMI_DMS_GET_IMSI_RESP_TYPE 0x01

#define QMI_DMS_GET_BAND_CAPABILITY 0x0045
	#define QMI_DMS_GET_BAND_CAPABILITY_RESP_TYPE_BAND 0x01
	#define QMI_DMS_GET_BAND_CAPABILITY_RESP_TYPE_LTEBAND 0x10
	#define QMI_DMS_GET_BAND_CAPABILITY_RESP_TYPE_TDSBAND 0x11
struct qmi_band_bit_mask {
  unsigned long long band; // band is a 64-bit bitmap
} __packed;

// common uim pin resp struct
struct qmi_dms_uim_pin_resp {
	char verify_retries_left;
	char unblock_retries_left;
} __packed;

#define QMI_DMS_UIM_SET_PIN_PROTECTION 0x0027
	#define QMI_DMS_UIM_SET_PIN_PROTECTION_REQ_TYPE 0x01
		struct qmi_dms_uim_set_pin_protection_req {
			unsigned char pin_id;
			unsigned char protection_setting;
			unsigned char pin_length;
			unsigned char pin_value[0];
		} __packed;

	#define QMI_DMS_UIM_SET_PIN_PROTECTION_RESP_TYPE 0x10

#define QMI_DMS_UIM_VERIFY_PIN  0x0028
	#define QMI_DMS_UIM_VERIFY_PIN_REQ_TYPE 0x01
		struct qmi_dms_uim_verify_pin_req {
			unsigned char pin_id;
			unsigned char pin_length;
			unsigned char pin_value[0];
		} __packed;

#define QMI_DMS_UIM_UNBLOCK_PIN 0x0029
	#define QMI_DMS_UIM_UNBLOCK_PIN_REQ_TYPE 0x01
		struct qmi_dms_uim_unblock_pin_req {
			unsigned char unblock_pin_id;
			unsigned char puk_length;
			unsigned char puk_value[0];
		};

		struct qmi_dms_uim_unblock_pin_req2 {
			unsigned char new_pin_length;
			unsigned char new_pin_value[0];
		} __packed;

	#define QMI_DMS_UIM_UNBLOCK_PIN_RESP_TYPE 0x10


#define QMI_DMS_UIM_CHANGE_PIN 0x002a
	#define QMI_DMS_UIM_CHANGE_PIN_REQ_TYPE 0x01
		struct qmi_dms_uim_change_pin_req {
			unsigned char pin_id;
			unsigned char old_pin_length;
			unsigned char old_pin_value[0];
		} __packed;

		struct qmi_dms_uim_change_pin_req2 {
			unsigned char new_pin_length;
			unsigned char new_pin_value[0];
		} __packed;


#define QMI_DMS_UIM_GET_PIN_STATUS 0x002b
	#define QMI_DMS_UIM_GET_PIN_STATUS_RESP_TYPE_PIN1 0x11
			struct qmi_dms_uim_get_pin_status_resp {
				unsigned char status;
				unsigned char verify_retries_left;
				unsigned char unblock_retries_left;
			} __packed;

	#define QMI_DMS_UIM_GET_PIN_STATUS_RESP_TYPE_PIN2 0x12

/* Sierra Sprint Activation flag */
#define QMI_DMS_GET_ACTIVATED_STATE 0x0031
	#define QMI_DMS_GET_ACTIVATED_STATE_RESP_TYPE 0x01


#define QMI_WDS_GET_DEFAULT_PROFILE_NUM 0x0049
	#define QMI_WDS_GET_DEFAULT_PROFILE_NUM_REQ_TYPE 0x01
			struct qmi_wds_get_default_profile_num_req {
				unsigned char profile_type;
				unsigned char profile_family;
			} __packed;
	#define QMI_WDS_GET_DEFAULT_PROFILE_NUM_RESP_TYPE 0x01

#define QMI_WDS_SET_DEFAULT_PROFILE_NUM 0x004a
	#define QMI_WDS_SET_DEFAULT_PROFILE_NUM_REQ_TYPE 0x01
			struct qmi_wds_set_default_profile_num_req {
				unsigned char profile_type;
				unsigned char profile_family;
				unsigned char profile_index;
			} __packed;

// QMI Network Access Service (QMINAS #3)
////////////////////////////////////////////////////////////////////////////////
#define QMI_NAS_INDICATION_REGISTER 0x0003
#define QMI_NAS_INDICATION_REGISTER_TYPE_NETWORK_REJECT_INFORMATION	0x21
			struct qmi_nas_indication_register_network_reject_information {
				unsigned char reg_network_reject;
				unsigned char suppress_sys_info;
			} __packed;

#define QMI_NAS_INDICATION_REGISTER_REQ_TYPE_SIG_INFO 0x19

#define QMI_NAS_GET_SIGNAL_STRENGTH 0x0020
	#define QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE 0x10

		#define QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE_RSSI	(1<<0)
		#define QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE_ECIO	(1<<1)
		#define QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE_IO		(1<<2)
		#define QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE_SINR	(1<<3)
		#define QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE_ERROR_RATE	(1<<4)
		#define QMI_NAS_GET_SIGNAL_STRENGTH_REQ_TYPE_RSRQ	(1<<5)

		struct qmi_nas_get_signal_strength_req {
			unsigned short request_mask;
		} __packed;
	#define QMI_NAS_GET_SIGNAL_STRENGTH_RESP_TYPE_SIG_STRENGTH 0x01
		struct qmi_nas_get_signal_strength_resp_sig_strength {
			signed char sig_strength;
			unsigned char radio_if;
		} __packed;


#define QMI_NAS_GET_SERVING_SYSTEM 0x0024
	#define QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE 0x01
		struct qmi_nas_get_serving_system_resp {
			/*
				Registration state of the mobile. Values:
				• 0x00 – NOT_REGISTERED – Not registered;
				mobile is not currently searching for a new network
				to provide service
				• 0x01 – REGISTERED – Registered with a network
				• 0x02 – NOT_REGISTERED_SEARCHING – Not
				registered, but mobile is currently searching for a
				new network to provide service
				• 0x03 – REGISTRATION_DENIED – Registration
				denied by the visible network
				• 0x04 – REGISTRATION_UNKNOWN –
				Registration state is unknown
			*/

			unsigned char registration_state;
			unsigned char cs_attach_stat;
			unsigned char ps_attach_stat;
			unsigned char registered_network;
			unsigned char in_use_radio_if_list_num;
			unsigned char radio_if[1];
		} __packed;

	#define QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_ROAMING_INDICATOR 0x10
		struct qmi_nas_get_serving_system_resp_type_roaming_indicator {
			unsigned char roaming_indicator;
		} __packed;

	#define QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_CURRENT_PLMN 0x12
		struct qmi_nas_get_serving_system_resp_type_current_plmn {
			unsigned short mobile_country_code;
			unsigned short mobile_network_code;
			unsigned char network_description_length;
			char network_description[0];
		} __packed;

	#define QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_CDMA_SYSTEM_ID 0x13
		struct qmi_nas_get_serving_system_resp_type_cdma_system_id {
			unsigned short system_id;
			unsigned short network_id;
		} __packed;

	#define QMI_NAS_GET_SERVING_SYSTEM_RESP_TYPE_UMTS_PSC 0x26
		//uint16 umts_psc

#define QMI_NAS_PERFORM_NETWORK_SCAN 0x0021
	#define QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE 0x10
		struct qmi_nas_perform_network_scan_resp2 {
			unsigned short mobile_country_code;
			unsigned short mobile_network_code;

			#define QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_IN_USE_STATUS (0x3<<0)
			#define QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_ROAMING_STATUS (0x3<<2)
			#define QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_FORBIDDEN_STATUS (0x3<<4)
			#define QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_PREFERRED_STATUS (0x3<<6)
			unsigned char network_status;

			unsigned char network_description_length;
			unsigned char network_description[0];
		} __packed;

		struct qmi_nas_perform_network_scan_resp {
			unsigned short num_network_info_instances;
			struct qmi_nas_perform_network_scan_resp2 networks[0];
		} __packed;

	// this response type is undocumented!
	#define QMI_NAS_PERFORM_NETWORK_SCAN_RESP_TYPE_ACT 0x11
		struct qmi_nas_perform_network_scan_resp_act2 {
			unsigned short mobile_country_code;
			unsigned short mobile_network_code;
			unsigned char radio_access_technology;
		} __packed;

		struct qmi_nas_perform_network_scan_resp_act {
			unsigned short num_network_info_instances;
			struct qmi_nas_perform_network_scan_resp_act2 networks[0];
		} __packed;


#define QMI_NAS_INITIATE_NETWORK_REGISTER 0x0022
#define QMI_NAS_INITIATE_NETWORK_REGISTER_REQ_TYPE_REG_ACTION 0x01
#define QMI_NAS_INITIATE_NETWORK_REGISTER_REQ_TYPE_NETWORK_INFO 0x10
		struct qmi_nas_initiate_network_register_req_network_info {
			unsigned short mobile_country_code;
			unsigned short mobile_network_code;
			unsigned char radio_access_technology;
		} __packed;


#define QMI_NAS_INITIATE_ATTACH 0x0023

	#define QMI_NAS_INITIATE_ATTACH_REQ_TYPE_PS_ATTACH_ACTION 0x10
	#define QMI_NAS_INITIATE_ATTACH_RESP_TYPE 0x01

#define QMI_NAS_GET_RF_BAND_INFO 0x0031
	#define QMI_NAS_GET_RF_BAND_INFO_RESP_TYPE 0x01
		struct qmi_nas_get_rf_band_info_resp {
			unsigned char num_instances;
			struct {
				unsigned char raido_if;
				unsigned short active_band;
				unsigned short active_channel;
			} __packed interfaces[1];
		} __packed;

#define QMI_NAS_GET_OPERATOR_NAME_DATA 0x0039
	#define QMI_NAS_GET_OPERATOR_NAME_DATA_RESP_TYPE_PLMN_NETWORK_NAME 0x10

#define QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO 0x003e
	#define QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO_REQ_TYPE 0x01
	#define QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO_REQ_TYPE_3GPP2_MASK 0x10

	#define QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO_RESP_TYPE_CHANNEL 0x15
		struct qmi_nas_get_3gpp2_subscription_info_resp_type_channel {
			unsigned short pri_ch_a;
			unsigned short pri_ch_b;
			unsigned short sec_ch_a;
			unsigned short sec_ch_b;
		} __packed;

	#define QMI_NAS_GET_3GPP2_SUBSCRIPTION_INFO_RESP_TYPE_MDN 0x16
		struct qmi_nas_get_3gpp2_subscription_info_resp_type_mdn {
			unsigned char mdn_len;
			char mdn[0];
		} __packed;

#define QMI_NAS_GET_SYS_INFO 0x004d
	#define QMI_NAS_GET_SYS_INFO_RESP_TYPE_GSM_RAC 0x2c
	#define QMI_NAS_GET_SYS_INFO_RESP_TYPE_WCDMA_RAC 0x2d

#define QMI_NAS_GET_SIG_INFO 0x004f
	#define QMI_NAS_GET_SIG_INFO_RESP_TYPE_CDMA 0x10
	struct qmi_nas_get_sig_info_resp_cdma {
		signed char rssi;
		unsigned short ecio;
	} __packed;

	#define QMI_NAS_GET_SIG_INFO_RESP_TYPE_HDR 0x11
	struct qmi_nas_get_sig_info_resp_hdr {
		signed char rssi;
		unsigned short ecio;
		unsigned char sinr;
		int io;
	} __packed;

	#define QMI_NAS_GET_SIG_INFO_RESP_TYPE_GSM 0x12
	struct qmi_nas_get_sig_info_resp_gsm {
		signed char gsm_sig_info;
	} __packed;

	#define QMI_NAS_GET_SIG_INFO_RESP_TYPE_WCDMA 0x13
	struct qmi_nas_get_sig_info_resp_wcdma {
		signed char rssi;
		short ecio;
	} __packed;

	#define QMI_NAS_GET_SIG_INFO_RESP_TYPE_LTE 0x14
	struct qmi_nas_get_sig_info_resp_lte {
		signed char rssi;
		signed char rsrq;
		short rsrp;
		short snr;
	} __packed;

	#define QMI_NAS_GET_SIG_INFO_RESP_TYPE_TDSCDMA 0x15
	struct qmi_nas_get_sig_info_resp_tdscdma {
		signed char rscp;
	} __packed;

	#define QMI_NAS_GET_SIG_INFO_RESP_TYPE_TDSCDMA_EXT 0x16
	struct qmi_nas_get_sig_info_resp_tdscdma_ext {
		float rssi;
		float rscp;
		float ecio;
		float sinr;
	} __packed;

#define QMI_NAS_SIG_INFO_IND 0x0051

#define QMI_NAS_GET_CDMA_POSITION_INFO 0x0065
	#define QMI_NAS_GET_CDMA_POSITION_INFO_RESP_TYPE 0x10
	struct qmi_nas_get_cdma_position_info_resp_type {
		char ue_in_idle;
		char bs_len;
		struct {
			int pilot_type;
			unsigned short sid;
			unsigned short nid;
			unsigned short base_id;
			unsigned short pilot_pn;
			unsigned short pilot_strength;
			int base_lat;
			int base_long;
			long long time_stamp;
		} __packed bs[0];
	} __packed;

#define QMI_NAS_NETWORK_REJECT_IND 0x0068
	#define QMI_NAS_NETWORK_REJECT_IND_TYPE_REGISTRATION_REJECTION_CAUSE 0x03
	struct qmi_nas_network_reject_ind {
		unsigned char rej_cause;
	} __packed;

#define QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE 0x0033
#define QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_MODE_PREF 0x11
#define QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_BAND_PREF 0x12
#define QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE_REQ_TYPE_LTE_BAND_PREF 0x15

#define QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE 0x0034
#define QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE_RESP_TYPE_MODE_PREF 0x11
#define QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE_RESP_TYPE_BAND_PREF 0x12
#define QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE_RESP_TYPE_LTE_BAND_PREF 0x15

#define QMI_NAS_CONFIG_SIG_INFO2 0x006C

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_CDMA_RSSI_THRESHOLD 0x10
struct qmi_nas_config_sig_info2_req_cdma_rssi_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_CDMA_RSSI_DELTA 0x11

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_CDMA_ECIO_THRESHOLD 0x12
struct qmi_nas_config_sig_info2_req_cdma_ecio_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_CDMA_ECIO_DELTA 0x13

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_RSSI_THRESHOLD 0x14
struct qmi_nas_config_sig_info2_req_hdr_rssi_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_RSSI_DELTA 0x15

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_ECIO_THRESHOLD 0x16
struct qmi_nas_config_sig_info2_req_hdr_ecio_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_ECIO_DELTA 0x17

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_SINR_THRESHOLD 0x18
struct qmi_nas_config_sig_info2_req_hdr_sinr_threshold {
	unsigned char len;
	unsigned short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_SINR_DELTA 0x19

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_IO_THRESHOLD 0x1A
struct qmi_nas_config_sig_info2_req_hdr_io_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_HDR_IO_DELTA 0x1B

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_GSM_RSSI_THRESHOLD 0x1C
struct qmi_nas_config_sig_info2_req_gsm_rssi_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_GSM_RSSI_DELTA 0x1D

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_WCDMA_RSSI_THRESHOLD 0x1E
struct qmi_nas_config_sig_info2_req_wcdma_rssi_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_WCDMA_RSSI_DELTA 0x1F

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_WCDMA_ECIO_THRESHOLD 0x20
struct qmi_nas_config_sig_info2_req_wcdma_ecio_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_WCDMA_ECIO_DELTA 0x21

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RSSI_THRESHOLD 0x22
struct qmi_nas_config_sig_info2_req_lte_rssi_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RSSI_DELTA 0x23

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_SNR_THRESHOLD 0x24
struct qmi_nas_config_sig_info2_req_lte_snr_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_SNR_DELTA 0x25

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RSRQ_THRESHOLD 0x26
struct qmi_nas_config_sig_info2_req_lte_rsrq_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RSRQ_DELTA 0x27

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RSRP_THRESHOLD 0x28
struct qmi_nas_config_sig_info2_req_lte_rsrp_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RSRP_DELTA 0x29

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_LTE_RPT_CONF 0x2A
struct qmi_nas_config_sig_info2_req_lte_rpt_conf {
	unsigned char rpt_rate;
	unsigned char avg_period;
} __packed;

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_RSCP_THRESHOLD 0x2B
struct qmi_nas_config_sig_info2_req_tdscdma_rscp_threshold {
	unsigned char len;
	signed short list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_RSCP_DELTA 0x2C

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_RSSI_THRESHOLD 0x2D
struct qmi_nas_config_sig_info2_req_tdscdma_rssi_threshold {
	unsigned char len;
	float list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_RSSI_DELTA 0x2E

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_ECIO_THRESHOLD 0x2F
struct qmi_nas_config_sig_info2_req_tdscdma_ecio_threshold {
	unsigned char len;
	float list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_ECIO_DELTA 0x30

#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_SINR_THRESHOLD 0x31
struct qmi_nas_config_sig_info2_req_tdscdma_sinr_threshold {
	unsigned char len;
	float list[0];
} __packed;
#define QMI_NAS_CONFIG_SIG_INFO2_REQ_TYPE_TDSCDMA_SINR_DELTA 0x32

// Wireless messaging service (QMIWMS #5)
////////////////////////////////////////////////////////////////////////////////
#define QMI_WMS_SET_EVENT_REPORT	0x0001
	#define QMI_WMS_SET_EVENT_REPORT_REQ_TYPE 0x10
	struct qmi_wms_set_event_report_req {
		unsigned char report_mt_message;
	} __packed;

	#define QMI_WMS_SET_EVENT_REPORT_RESP_TYPE_MT_MSG 0x10
	struct qmi_wms_set_event_report_resp_mt_msg {
		unsigned char storage_type;
		unsigned int storage_index;
	} __packed;

	#define QMI_WMS_SET_EVENT_REPORT_RESP_TYPE_MSG_MODE 0x12
	struct qmi_wms_set_event_report_resp_msg_mode {
		unsigned char message_mode;
	} __packed;

	#define QMI_WMS_SET_EVENT_REPORT_RESP_TYPE_SMS_ON_IMS 0x16
	struct qmi_wms_set_event_report_resp_sms_on_ims {
		unsigned char sms_on_ims;
	} __packed;


#define QMI_WMS_RAW_SEND		0x0020

#define QMI_MESSAGE_FORMAT_CDMA		0x00
#define QMI_MESSAGE_FORMAT_GW_PP	0x06

	#define QMI_WMS_RAW_SEND_REQ_TYPE_RAW_MESSAGE_DATA 0x01
		struct qmi_wms_raw_send_req_raw_message_data {
			/*
				• 0x00 – MESSAGE_FORMAT_CDMA – CDMA
				• 0x02 to 0x05 – Reserved
				• 0x06 – MESSAGE_FORMAT_GW_PP – GW_PP
			*/
			unsigned char format;
			unsigned short len;
			char raw_message[0];
		} __packed;

	#define QMI_WMS_RAW_SEND_REQ_TYPE_FORCE_ON_DC 0x10
		struct qmi_wms_raw_send_req_force_on_dc {
		} __packed;
	#define QMI_WMS_RAW_SEND_REQ_TYPE_FOLLOW_ON_DC 0x11
		struct qmi_wms_raw_send_req_follow_on_dc {
		} __packed;

	#define QMI_WMS_RAW_SEND_REQ_TYPE_LINK_CONTROL 0x12
		struct qmi_wms_raw_send_req_link_control {
		} __packed;

	#define QMI_WMS_RAW_SEND_REQ_TYPE_RETRY_MESSAGE 0x14
		struct qmi_wms_raw_send_req_retry_message {
		} __packed;

	#define QMI_WMS_RAW_SEND_REQ_TYPE_RETRY_MESSAGE_ID 0x15
		struct qmi_wms_raw_send_req_retry_message_id {
		} __packed;

	#define QMI_WMS_RAW_SEND_RESP_TYPE_MESSAGE_ID 0x01
		struct qmi_wms_raw_send_resp_message_id {
			unsigned short message_id;
		} __packed;

	#define QMI_WMS_RAW_SEND_RESP_TYPE_CAUSE_CODE 0x10
		struct qmi_wms_raw_send_resp_cause_code {
		} __packed;

	#define QMI_WMS_RAW_SEND_RESP_TYPE_ERROR_CLASS 0x11
		struct qmi_wms_raw_send_resp_error_class {
		} __packed;

	#define QMI_WMS_RAW_SEND_RESP_TYPE_GW_CAUSE_INFO 0x12
		struct qmi_wms_raw_send_resp_gw_cause_info {
		} __packed;

	#define QMI_WMS_RAW_SEND_RESP_TYPE_MSG_DELIVERY_FAILURE_TYPE 0x13
		struct qmi_wms_raw_send_resp_msg_delivery_failure_type {
		} __packed;

	#define QMI_WMS_RAW_SEND_RESP_TYPE_MSG_DELIVERY_FAILURE_CAUSE 0x14
		struct qmi_wms_raw_send_resp_msg_delivery_failure_cause {
		} __packed;

	#define QMI_WMS_RAW_SEND_RESP_TYPE_CALL_CONTROL_MOD_INFO 0x15
		struct qmi_wms_raw_send_resp_call_control_mod_info {
		} __packed;

#define QMI_WMS_RAW_READ		0x0022
	#define QMI_WMS_RAW_READ_REQ_TYPE_STORAGE_TYPE 0x01
		struct qmi_wms_raw_read_req_storage_type {
			unsigned char storage_type;
			unsigned int storage_index;
		} __packed;

	#define QMI_WMS_RAW_READ_REQ_TYPE_MSG_MODE 0x10
	#define QMI_WMS_RAW_READ_REQ_TYPE_SMS_ON_IMS 0x11

	#define QMI_WMS_RAW_READ_RESP_TYPE 0x01
		struct qmi_wms_raw_read_resp {
			/*
				• 0x00 – TAG_TYPE_MT_READ
				• 0x01 – TAG_TYPE_MT_NOT_READ
				• 0x02 – TAG_TYPE_MO_SENT
				• 0x03 – TAG_TYPE_MO_NOT_SENT
			*/
			unsigned char tag_type;
			/*
				• 0x00 – MESSAGE_FORMAT_CDMA – CDMA
				• 0x02 to 0x05 – Reserved
				• 0x06 – MESSAGE_FORMAT_GW_PP – GW_PP
				• 0x08 - MESSAGE_FORMAT_MWI – MWI
			*/
			unsigned char format;
			unsigned short len;
			char data[];
		} __packed;


#define QMI_WMS_MODIFY_TAG		0x0023
	#define QMI_WMS_MODIFY_TAG_REQ_TYPE 0x01
		struct qmi_wms_modify_tag_req {
			unsigned char storage_type;
			unsigned int storage_index;
			unsigned char tag_type;
		} __packed;
	#define QMI_WMS_MODIFY_TAG_REQ_TYPE_MSG_MODE 0x10

#define QMI_WMS_DELETE			0x0024
	#define QMI_WMS_DELETE_REQ_TYPE_STORAGE_TYPE 0x01
	#define QMI_WMS_DELETE_REQ_TYPE_INDEX 0x10
	#define QMI_WMS_DELETE_REQ_TYPE_TAG_TYPE 0x11
	#define QMI_WMS_DELETE_REQ_TYPE_MSG_MODE 0x12

#define QMI_WMS_GET_MESSAGE_PROTOCOL	0x0030
	#define QMI_WMS_GET_MESSAGE_PROTOCOL_RESP_TYPE 0x01

#define QMI_WMS_LIST_MESSAGES		0x0031
	#define QMI_WMS_LIST_MESSAGES_REQ_TYPE_STORAGE_TYPE 0x01
		struct qmi_wms_list_messages_req_storage_type {
			/*
				Memory storage. Values:
				• 0x00 – STORAGE_TYPE_UIM
				• 0x01 – STORAGE_TYPE_NV
			*/
			#define QMI_WMS_STORAGE_TYPE_UIM 0x00
			#define QMI_WMS_STORAGE_TYPE_NV 0x01

			unsigned char storage_type;
		} __packed;



	#define QMI_WMS_LIST_MESSAGES_REQ_TYPE_REQ_TAG 0x10
		struct qmi_wms_list_messages_req_req_tag {
			/*
				Message tag. Values:
				• 0x00 – TAG_TYPE_MT_READ
				• 0x01 – TAG_TYPE_MT_NOT_READ
				• 0x02 – TAG_TYPE_MO_SENT
				• 0x03 – TAG_TYPE_MO_NOT_SENT
			*/

			#define QMI_WMS_TAG_TYPE_MT_READ 0x00
			#define QMI_WMS_TAG_TYPE_MT_NOT_READ 0x01
			#define QMI_WMS_TAG_TYPE_MO_SENT 0x02
			#define QMI_WMS_TAG_TYPE_MO_NOT_SENT 0x03

   			unsigned char tag_type;
		} __packed;

	#define QMI_WMS_LIST_MESSAGES_REQ_TYPE_MSG_MODE 0x11
		struct qmi_wms_list_messages_req_type_msg_mode {
			/*
				Message mode. Values:
				• 0x00 – MESSAGE_MODE_CDMA – CDMA
				• 0x01 – MESSAGE_MODE_GW – GW
			*/

			#define QMI_WMS_LIST_MESSAGES_REQ_TYPE_MESSAGE_MODE_CDMA 0x00
			#define QMI_WMS_LIST_MESSAGES_REQ_TYPE_MESSAGE_MODE_GW 0x01

			unsigned char message_mode;
		} __packed;

	#define QMI_WMS_LIST_MESSAGES_RESP_TYPE 0x01
		struct qmi_wms_list_messages_resp {
			unsigned int N_messages;

			struct {
				unsigned int message_index;
				unsigned char tag_type;
			} __packed n[0];

		} __packed;

#define QMI_WMS_SET_ROUTES		0x0032
	struct qmi_wms_route_t {
		unsigned char message_type;
		unsigned char message_class;

		/*
			• 0x00 – STORAGE_TYPE_UIM
			• 0x01 – STORAGE_TYPE_NV
			• -1 – STORAGE_TYPE_NONE
		*/

		unsigned char route_memory;
		unsigned char route_value;
	} __packed;

	struct qmi_wms_route_list {
		unsigned short n_routes;
		struct qmi_wms_route_t n[0];
	} __packed;

	#define QMI_WMS_SET_ROUTES_REQ_TYPE_ROUTE_LIST 0x01
	#define QMI_WMS_SET_ROUTES_REQ_TYPE_TRAN_STAT_REPORT 0x10

#define QMI_WMS_GET_ROUTES		0x0033
	#define QMI_WMS_GET_ROUTES_REQ_TYPE_ROUTE_LIST 0x01
	#define QMI_WMS_GET_ROUTES_RESP_TYPE_ROUTE_LIST 0x01
	#define QMI_WMS_GET_ROUTES_REQ_TYPE_TRAN_STAT_REPORT 0x10


#define QMI_WMS_GET_SMSC_ADDRESS 	0x0034
	#define QMI_WMS_GET_SMSC_ADDRESS_RESP_TYPE 0x01
		struct qmi_wms_smsc_address_req {
			char smsc_address_type[3];
			unsigned char smsc_address_length;
			char smsc_address_digits[0];
		} __packed;

#define QMI_WMS_SET_SMSC_ADDRESS	0x0035
	#define QMI_WMS_SET_SMSC_ADDRESS_REQ_TYPE_SMSC_ADDR 0x1
	#define QMI_WMS_SET_SMSC_ADDRESS_REQ_TYPE_ADDR_TYPE 0x10


#define QMI_WMS_GET_DOMAIN_PREF		0x0040
#define QMI_WMS_SET_DOMAIN_PREF		0x0041

#define QMI_WMS_INDICATION_REGISTER	0x0047
	#define QMI_WMS_INDICATION_REGISTER_REQ_TYPE_TLFINO 0x10
	#define QMI_WMS_INDICATION_REGISTER_REQ_TYPE_NWREG 0x11
	#define QMI_WMS_INDICATION_REGISTER_REQ_TYPE_CALLSTAT 0x12
	#define QMI_WMS_INDICATION_REGISTER_REQ_TYPE_SREADY 0x13
	#define QMI_WMS_INDICATION_REGISTER_REQ_TYPE_BCEEVENT 0x14


#define QMI_WMS_GET_TRANSPORT_LAYER_INFO	0x0048
	#define QMI_WMS_GET_TRANSPORT_LAYER_INFO_RESP_TYPE_REG 0x10
	#define QMI_WMS_GET_TRANSPORT_LAYER_INFO_RESP_TYPE_TLINFO 0x11
		struct qmi_wms_get_transport_layer_info_resp_tlinfo {
			char transport_type;
			char transport_cap;
		} __packed;

#define QMI_WMS_GET_DOMAIN_PREF_CONFIG		0x0051
	#define QMI_WMS_GET_DOMAIN_PREF_CONFIG_RESP_TYPE_LTE_DP 0x10
		#define QMI_WMS_DOMAIN_PREF_CONFIG_LTE_DP_NONE 0x00
		#define QMI_WMS_DOMAIN_PREF_CONFIG_LTE_DP_IMS 0x01

	#define QMI_WMS_GET_DOMAIN_PREF_CONFIG_RESP_TYPE_DP_PREF 0x11
		#define QMI_WMS_DOMAIN_PREF_CONFIG_GW_DP_PREF_CS 0x00
		#define QMI_WMS_DOMAIN_PREF_CONFIG_GW_DP_PREF_PS 0x01
		#define QMI_WMS_DOMAIN_PREF_CONFIG_GW_DP_PREF_CS_ONLY 0x02
		#define QMI_WMS_DOMAIN_PREF_CONFIG_GW_DP_PREF_PS_ONLY 0x03

#define QMI_WMS_SET_DOMAIN_PREF_CONFIG		0x0052


#define QMI_WMS_GET_SERVICE_READY_STATUS	0x005c
	#define QMI_WMS_GET_SERVICE_READY_STATUS_RESP_TYPE_REG 0x10
	#define QMI_WMS_GET_SERVICE_READY_STATUS_RESP_TYPE_RSTAT 0x11

#define QMI_WMS_SERVICE_READY_IND	0x005d


// Position determination service (QMIPDS #6)
////////////////////////////////////////////////////////////////////////////////
#define QMI_PDS_RESET 0x0000

#define QMI_PDS_SET_EVENT_REPORT 0x0001
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_POSITION_DATA_NMEA 0x10
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_EPOSITION_DATA_NMEA 0x11
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_PARSED_POSITION_DATA 0x12
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_EXTRA_DATA 0x13
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_TIME_INJECTION 0x14
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_WIFI_POSITION 0x15
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_SATELITE_INFORMATION 0x16
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_NETWORK_INTIATED_PROMPT_VX 0x17
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_NETWORK_INTIATED_PROMPT_SUPL 0x18
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_NETWORK_INTIATED_PROMPT_UMTS_CP 0x19
	#define QMI_PDS_SET_EVENT_REPORT_REQ_TYPE_PDS_COMMON_EVENTS 0x1A

	#define QMI_PDS_SET_EVENT_REPORT_RESP_TYPE_POSITION_SESSION_STATUS 0x12
		struct qmi_pds_set_event_report_resp_position_session_status {
			unsigned char position_session_status;
		} __packed;
	#define QMI_PDS_SET_EVENT_REPORT_RESP_TYPE_PARSED_POSITION_DATA 0x13
		struct qmi_pds_set_event_report_resp_parsed_position_data {
			unsigned int valid_mask;
			unsigned short calendar_year;
			unsigned char calendar_month;
			unsigned char calendar_day_of_week;
			unsigned char calendar_day_of_month;
			unsigned char calendar_hour;
			unsigned char calendar_minute;
			unsigned char calendar_second;
			unsigned short calendar_millisecond;
			unsigned char leap_seconds;
			unsigned long long timestamp_utc;
			unsigned int time_unc;
			double latitude;
			double longitude;
			float altitude_wrt_ellipsoid;
			float altitude_wrt_sea_level;
			float horizontal_speed;
			float vertical_speed;
			float heading;
			float horizontal_unc_circular;
			float horizontal_unc_ellipse_semi_major;
			float horizontal_unc_ellipse_semi_minor;
			float horizontal_unc_ellipse_orient_azimuth;
			float vertical_unc;
			float horizontal_vel_unc;
			float vertical_vel_unc;
			unsigned char horizontal_confidence;
			float position_dop;
			float horizontal_dop;
			float vertical_dop;
			unsigned char position_op_mode;
		} __packed;
	#define QMI_PDS_SET_EVENT_REPORT_RESP_TYPE_SATELLITE_INFORMATION 0x17
		struct qmi_pds_set_event_report_resp_type_satellite_information {
			unsigned int valid_mask;
			unsigned char iono_valid;
			unsigned char svns_len;

			struct {
				unsigned int svn_valid_mask;
				unsigned char svn_system;
				unsigned char svn_prn;
				unsigned char svn_health_status;
				unsigned char svn_process_status;
				unsigned char svn_ephemeris_state;
				unsigned char svn_almanac_state;
				float svn_elevation;
				unsigned short svn_azimuth;
				unsigned short svn_cno;
			} e[0];
		} __packed;

	#define QMI_PDS_SET_EVENT_REPORT_RESP_TYPE_POSITION_SOURCE 0x1C
		struct qmi_pds_set_event_report_resp_position_source {
			unsigned int pds_position_source;
		} __packed;

#define QMI_PDS_GET_GPS_SERVICE_STATE 0x0020
	#define QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE 0x01
		struct qmi_pds_get_gps_service_state_resp {

			#define QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE_GSS_DISABLE 0x00
			#define QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE_GSS_ENABLE 0x01
			unsigned char gps_service_state;
			#define QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE_TSS_UNKNOWN 0x00
			#define QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE_TSS_INACTIVE 0x01
			#define QMI_PDS_GET_GPS_SERVICE_STATE_RESP_TYPE_TSS_ACTIVE 0x02
			unsigned char tracking_session_state;
		} __packed;

#define QMI_PDS_START_TRACKING_SESSION 0x0022

	#define QMI_PDS_START_TRACKING_SESSION_REQ_TYPE 0x01
		struct qmi_pds_start_tracking_session_req {
			unsigned char session_control;
			unsigned char session_type;

			#define QMI_PDS_START_TRACKING_SESSION_REQ_TYPE_OPMODE_STANDALONE 0x00
			#define QMI_PDS_START_TRACKING_SESSION_REQ_TYPE_OPMODE_MSB 0x01
			#define QMI_PDS_START_TRACKING_SESSION_REQ_TYPE_OPMODE_MSA 0x02
			unsigned char session_operation;

			unsigned char session_server_option;
			unsigned char position_data_timeout; /* maximum 255 */
			unsigned int position_data_count;  /* at least 1 */
			unsigned int position_data_interval;
			unsigned int position_data_accuracy;
		} __packed;


#define QMI_PDS_DETERMINE_POSITION 0x0024

#define QMI_PDS_END_TRACKING_SESSION 0x0025
	#define QMI_PDS_END_TRACKING_SESSION_REQ_TYPE 0x01
	#define QMI_PDS_END_TRACKING_SESSION_RESP_TYPE 0x02

#define QMI_PDS_GET_NMEA_CONFIG 0x0026
	#define QMI_PDS_GET_NMEA_CONFIG_RESP_TYPE 0x01
	#define QMI_PDS_GET_NMEA_CONFIG_RESP_TYPE_EXT_NMEA_SENTENCE_MASK 0x10

#define QMI_PDS_SET_NMEA_CONFIG 0x0027
	#define QMI_PDS_SET_NMEA_CONFIG_REQ_TYPE 0x01
		struct qmi_pds_nmea_config_reqresp {
			/*
				• 0x01 – GPGGA
				• 0x02 – GPRMC
				• 0x04 – GPGSV
				• 0x08 – GPGSA
				• 0x10 – GPVTG
				• 0x20 – GLGSV
				• 0x40 – GNGSA
				• 0x80 – GNGNS
			*/
			unsigned char nmea_sentence_mask;
			/*
				• 0x00 – None (disabled)
				• 0x01 – USB
				• 0x02 – UART1
				• 0x03 – UART2
				• 0x04 – Shared memory
			*/
			unsigned char nmea_port;
			/*
				• 0x00 – 1 Hz from the time requested until the final position is determined
				• 0x01 – Only when the final position is determined
			*/
			unsigned char nmea_reporting;
		} __packed;

	#define QMI_PDS_SET_NMEA_CONFIG_RESP_TYPE_EXT_NMEA_SENTENCE_MASK 0x10
		struct qmi_pds_nmea_config_resp_ext_nmea_sentence_mask {
			/*
				• 0x01 – PQXFI
				• 0x02 – PSTIS
			*/
			unsigned short ext_nmea_sentence_mask;
		} __packed;

#define QMI_PDS_GET_DEFAULT_TRACKING_SESSION 0x0029
	#define QMI_PDS_GET_DEFAULT_TRACKING_SESSION_RESP_TYPE 0x01

#define QMI_PDS_SET_DEFAULT_TRACKING_SESSION 0x002a
	#define QMI_PDS_SET_DEFAULT_TRACKING_SESSION_REQ_TYPE 0x01
		struct qmi_pds_default_tracking_session_reqresp {
			unsigned char session_operation;
			unsigned char position_data_timeout;
			unsigned int position_data_interval;
			unsigned int position_data_accuracy;
		} __packed;

#define QMI_PDS_SET_AUTO_TRACKING_STATE 0x0031
	#define QMI_PDS_SET_AUTO_TRACKING_STATE_REQ_TYPE 0x01
		struct qmi_pds_auto_tracking_state_reqresp {
			unsigned char auto_tracking_state;
		} __packed;

#define QMI_PDS_GET_AUTO_TRACKING_STATE 0x0030
	#define QMI_PDS_GET_AUTO_TRACKING_STATE_RESP_TYPE 0x01


#define QMI_PDS_GPS_READY 0x0060

#define QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT 0x62
	#define QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT_REQ_TYPE 0x01
		struct qmi_pds_set_gnss_engine_error_recovery_report_req {
			unsigned char enable;
		} __packed;

	#define QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT_RESP_TYPE 0x01
		struct qmi_pds_set_gnss_engine_error_recovery_report_resp {
			unsigned int reset_type;
			unsigned int assist_delete_mask;
		} __packed;


// QMI VOICE
////////////////////////////////////////////////////////////////////////////////
#define QMI_VOICE_INDICATION_REGISTER 0x0003

	#define QMI_VOICE_INDICATION_REGISTER_REQ_TYPE_DTMF_EVENTS 0x10
	#define QMI_VOICE_INDICATION_REGISTER_REQ_TYPE_VOICE_PRIVACY_EVENTS 0x11
	#define QMI_VOICE_INDICATION_REGISTER_REQ_TYPE_SUPPLEMENTARY_SERVICE_NOTIFICATION_EVENTS 0x12
	#define QMI_VOICE_INDICATION_REGISTER_REQ_TYPE_CALL_NOTIFICATION_EVENTS 0x13
	#define QMI_VOICE_INDICATION_REGISTER_REQ_TYPE_HANDOVER_EVENTS 0x14
	#define QMI_VOICE_INDICATION_REGISTER_REQ_TYPE_SPEECH_CODEC_EVENTS 0x15
	#define QMI_VOICE_INDICATION_REGISTER_REQ_TYPE_ADDITIONAL_CALL_INFORMATION_EVENTS 0x24


#define QMI_VOICE_DIAL_CALL 0x0020

	#define QMI_VOICE_DIAL_CALL_REQ_TYPE_CALLING_NUMBER 0x01
		struct qmi_voice_dial_call_req_calling_number {
			char calling_number[0];
		} __packed;

	#define QMI_VOICE_DIAL_CALL_REQ_TYPE_CALL_ID 0x10



#define QMI_VOICE_END_CALL 0x0021
	#define QMI_VOICE_END_CALL_REQ_TYPE_CALL_ID 0x01
	#define QMI_VOICE_END_CALL_RESP_TYPE 0x10

#define QMI_VOICE_ANSWER_CALL 0x0022
	#define QMI_VOICE_ANSWER_CALL_REQ_TYPE_CALL_ID 0x01
	#define QMI_VOICE_ANSWER_CALL_RESP_TYPE_CALL_ID 0x10

#define QMI_VOICE_ALL_CALL_STATUS_IND 0x002e
	#define QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_CALL_INFO	0x01
		struct qmi_voice_all_call_status_type_call_info {
			unsigned char num_instance;
			struct qmi_voice_all_call_status_type_call_info_entry_t {
				unsigned char call_id;

				#define CALL_STATE_ORIGINATION 0x01
				#define CALL_STATE_INCOMING 0x02
				#define CALL_STATE_CONVERSATION 0x03
				#define CALL_STATE_CC_IN_PROGRESS 0x04
				#define CALL_STATE_ALERTING 0x05
				#define CALL_STATE_HOLD 0x06
				#define CALL_STATE_WAITING 0x07
				#define CALL_STATE_DISCONNECTING 0x08
				#define CALL_STATE_END 0x09
				#define CALL_STATE_SETUP 0x0a
				unsigned char call_state;

				#define CALL_TYPE_VOICE 0x00
				#define CALL_TYPE_VOICE_IP 0x02
				#define CALL_TYPE_VT 0x03
				#define CALL_TYPE_VIDEOSHARE 0x04
				#define CALL_TYPE_TEST 0x05
				#define CALL_TYPE_OTAPA 0x06
				#define CALL_TYPE_STD_OTASP 0x07
				#define CALL_TYPE_NON_STD_OTASP 0x08
				#define CALL_TYPE_EMERGENCY 0x09
				#define CALL_TYPE_SUPS 0x0a
				#define CALL_TYPE_EMERGENCY_IP 0x0b
				#define CALL_TYPE_EMERGENCY_VT 0x0c
				unsigned char call_type;

				#define CALL_DIRECTION_MO 0x01
				#define CALL_DIRECTION_MT 0x02
				unsigned char direction;

				#define CALL_MODE_NO_SRV 0x00
				#define CALL_MODE_CDMA 0x01
				#define CALL_MODE_GSM 0x02
				#define CALL_MODE_UMTS 0x03
				#define CALL_MODE_LTE 0x04
				#define CALL_MODE_TDS 0x05
				#define CALL_MODE_UNKNOWN 0x06
				#define CALL_MODE_WLAN 0x07
				unsigned char mode;

				unsigned char is_mpty;

				#define ALS_LINE1 0x00
				#define ALS_LINE2 0x01
				unsigned char als;
			} __packed e[0];
		} __packed;
	#define QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_REMOTE_PARTY_NUMBER	0x10
		struct qmi_voice_all_call_status_ind_remote_party_number {
			unsigned char num_instance;
			struct qmi_voice_all_call_status_ind_remote_party_number_entry_t {
				unsigned char call_id;

				#define PRESENTATION_ALLOWED 0x00
				#define PRESENTATION_RESTRICTED 0x01
				#define PRESENTATION_NUM_UNAVAILABLE 0x02
				#define PRESENTATION_PAYPHONE 0x04
				unsigned char number_pi;
				unsigned char number_len;
				char number[0];
			} __packed e[0];
		} __packed;

	#define QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_REMOTE_PARTY_NAME 0x11
		struct qmi_voice_all_call_status_ind_remote_party_name {
			unsigned char num_instance;
			struct qmi_voice_all_call_status_ind_remote_party_name_entry_t {
				unsigned char call_id;

				#define PRESENTATION_NAME_PRESENTATION_ALLOWED 0
				#define PRESENTATION_NAME_PRESENTATION_RESTRICTED 1
				#define PRESENTATION_NAME_UNAVAILABLE 2
				#define PRESENTATION_NAME_NAME_PRESENTATION_RESTRICTED 3
				unsigned char name_pi;
				unsigned char coding_scheme;
				unsigned char name_len;
				char name[0];
			} __packed e[0];
		} __packed;

	#define QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_ALERTING_TYPE 0x12
		struct qmi_voice_all_call_status_ind_alerting_type {
			unsigned char num_instance;
			struct qmi_voice_all_call_status_ind_alerting_type_entry_t {
				unsigned char call_id;

				#define ALERTING_LOCAL 0
				#define ALERTING_REMOTE 1
				unsigned short altering_type;
			} __packed e[0];
		} __packed;

	#define QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_CALL_END_REASON 0x14
		struct qmi_voice_all_call_status_ind_call_end_reason {
			unsigned char num_instance;
			struct qmi_voice_all_call_status_ind_call_end_reason_entry_t {
				unsigned char call_id;
				unsigned short call_end_reason;
			} __packed e[0];
		} __packed;


	#define QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_SRVCC_CALL 0x1f
		struct qmi_voice_all_call_status_ind_srvcc_call {
			unsigned char num_instance;
			struct qmi_voice_all_call_status_ind_srvcc_call_entry_t {
				unsigned char call_id;
				unsigned char is_srvcc_call;
			} __packed e[0];
		} __packed;

	#define QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_ADDITIONAL_CALL_INFO 0x28
		struct qmi_voice_all_call_status_ind_additional_call_info {
			unsigned char num_instance;
			struct qmi_voice_all_call_status_ind_additional_call_info_entry_t {
				unsigned char call_id;
				unsigned char is_add_info_present;
				unsigned short num_indications;
			} __packed e[0];
		} __packed;


	#define QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_CALL_ATTRIB_STAT 0x29

		struct qmi_voice_all_call_status_ind_call_attrib_stat {
			unsigned char num_instance;
			struct qmi_voice_all_call_status_ind_call_attrib_stat_entry_t {
				unsigned char call_id;

				#define	VOICE_CALL_ATTRIB_STATUS_OK 0
				#define VOICE_CALL_ATTRIB_STATUS_RETRY_NEEDED 1
				#define VOICE_CALL_ATTRIB_STATUS_MEDIA_PAUSED 2
				#define VOICE_CALL_ATTRIB_STATUS_MEDIA_NOT_READY 3
				int call_attrib_status;
			} __packed e[0];

		} __packed;

#define QMI_VOICE_SET_SUPS_SERVICE 0x0033
	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_SERV_INFO 0x01
		struct qmi_voice_set_sups_service_req_serv_info {


			#define VOICE_SERVICE_ACTIVATE 0x01
			#define VOICE_SERVICE_DEACTIVATE 0x02
			#define VOICE_SERVICE_REGISTER 0x03
			#define VOICE_SERVICE_ERASE 0x04
			unsigned char voice_serivce;

			#define VOICE_REASON_FWD_UNCONDITIONAL 0x01
			#define VOICE_REASON_FWD_MOBILEBUSY 0x02
			#define VOICE_REASON_FWD_NOREPLY 0x03
			#define VOICE_REASON_FWD_UNREACHABLE 0x04
			#define VOICE_REASON_FWD_ALLFORWARDING 0x05
			#define VOICE_REASON_FWD_ALLCONDITIONAL 0x06
			#define VOICE_REASON_BARR_ALLOUTGOING 0x07
			#define VOICE_REASON_BARR_OUTGOINGINT 0x08
			#define VOICE_REASON_BARR_OUTGOINGINTEXTOHOME 0x09
			#define VOICE_REASON_BARR_ALLINCOMING 0x0A
			#define VOICE_REASON_BARR_INCOMINGROAMING 0x0B
			#define VOICE_REASON_BARR_ALLBARRING 0x0C
			#define VOICE_REASON_BARR_ALLOUTGOINGBARRING 0x0D
			#define VOICE_REASON_BARR_ALLINCOMINGBARRING 0x0E
			#define VOICE_REASON_CALLWAITING 0x0F
			#define VOICE_REASON_CLIP 0x10
			#define VOICE_REASON_COLP 0x12
			#define VOICE_REASON_COLR 0x13
			#define VOICE_REASON_CNAP 0x14
			#define VOICE_REASON_BARR_INCOMINGNUMBER 0x15
			#define VOICE_REASON_BARR_INCOMINGANONYMOUS 0x16
			unsigned char reason;
		} __packed;

	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_BARRING_PASSWORD 0x11
	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_NUMBER 0x12
	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_NO_REPLY_TIMER 0x13
	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_NUMBER_TYPE_AND_PLAN 0x14
		struct qmi_voice_set_sups_service_req_call_forwarding_number_type_and_plan {
			#define QMI_VOICE_NUM_TYPE_UNKNOWN 0x00
			#define QMI_VOICE_NUM_TYPE_INTERNATIONAL 0x01
			#define QMI_VOICE_NUM_TYPE_NATIONAL 0x02
			#define QMI_VOICE_NUM_TYPE_NETWORK 0x03
			#define QMI_VOICE_NUM_TYPE_SUBSCRIBER 0x04
			#define QMI_VOICE_NUM_TYPE_RESERVED 0x05
			#define QMI_VOICE_NUM_TYPE_ABBREVIATED 0x06
			#define QMI_VOICE_NUM_TYPE_RESERVED_EXTENSION 0x07
			unsigned char num_type;

			#define QMI_VOICE_NUM_PLAN_UNKNOWN 0x00
			#define QMI_VOICE_NUM_PLAN_ISDN 0x01
			#define QMI_VOICE_NUM_PLAN_DATA 0x03
			#define QMI_VOICE_NUM_PLAN_TELEX 0x04
			#define QMI_VOICE_NUM_PLAN_NATIONAL 0x08
			#define QMI_VOICE_NUM_PLAN_PRIVATE 0x09
			#define QMI_VOICE_NUM_PLAN_RESERVED_CTS 0x0B
			unsigned char num_plan;

		} __packed;

	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_EXTENDED_SERVICE_CLASS 0x15
	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_BARRING_NUMBERS_LIST 0x16
		struct qmi_voice_set_sups_service_req_call_barring_numbers_list {
			unsigned char num_instances;
			struct qmi_voice_set_sups_service_req_call_barring_numbers_list_entry_t {
				unsigned char barred_number_len;
				unsigned char barred_number[0];
			} __packed e[0];
		} __packed;

	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_COLR_PRESENTATION_INFORMATION 0x17
		#define COLR_PRESENTATION_NOT_RESTRICTED 0x00
		#define COLR_PRESENTATION_RESTRICTED 0x01

	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_START_TIME 0x18
		struct qmi_voice_get_call_forwarding_call_forwarding_time {
			unsigned short year;
			unsigned char month;
			unsigned char day;
			unsigned char hour;
			unsigned char minute;
			unsigned char second;
			signed char time_zone;
		} __packed;

	#define QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_END_TIME 0x19

	#define QMI_VOICE_SET_SUPS_SERVICE_RESP_TYPE_ALPHA_IDENTIFIER 0x11
		struct qmi_voice_get_call_forwarding_alpha_identifier {
			#define ALPHA_DCS_GSM 0x01
			#define ALPHA_DCS_UCS2 0x02
			unsigned char alpha_dcs;
			unsigned char alpha_len;
			unsigned char alpha_text;
		} __packed;

	#define QMI_VOICE_SET_SUPS_SERVICE_RESP_TYPE_CALL_CONTROL_RESULT_TYPE 0x12
		#define CC_RESULT_TYPE_VOICE 0x00
		#define CC_RESULT_TYPE_SUPS 0x01
		#define CC_RESULT_TYPE_USSD 0x02

	#define QMI_VOICE_SET_SUPS_SERVICE_RESP_TYPE_CALL_ID 0x13
	#define QMI_VOICE_SET_SUPS_SERVICE_RESP_TYPE_CALL_CONTROL_SUPPLEMENTARY_SERVICE_TYPE 0x14
		#define VOICE_CC_SUPS_RESULT_SERVICE_TYPE_ACTIVATE 0x01
		#define VOICE_CC_SUPS_RESULT_SERVICE_TYPE_DEACTIVATE 0x02
		#define VOICE_CC_SUPS_RESULT_SERVICE_TYPE_REGISTER 0x03
		#define VOICE_CC_SUPS_RESULT_SERVICE_TYPE_ERASE 0x04
		#define VOICE_CC_SUPS_RESULT_SERVICE_TYPE_INTERROGATE 0x05
		#define VOICE_CC_SUPS_RESULT_SERVICE_TYPE_REGISTER_PASSWORD 0x06
		#define VOICE_CC_SUPS_RESULT_SERVICE_TYPE_USSD 0x07

	#define QMI_VOICE_SET_SUPS_SERVICE_RESP_TYPE_SERVICE_STATUS 0x15
	#define QMI_VOICE_SET_SUPS_SERVICE_RESP_TYPE_FAILURE_CAUSE_DESCRIPTION 0x16
	#define QMI_VOICE_SET_SUPS_SERVICE_RESP_TYPE_RETRY_DURATION 0x17

#define QMI_VOICE_GET_CALL_FORWARDING 0x0038

	#define QMI_VOICE_GET_CALL_FORWARDING_REQ_TYPE_CALL_FORWARDING_REASON 0x01

		#define QMI_VOICE_REASON_FWDREASON_UNCONDITIONAL 0x01
		#define QMI_VOICE_REASON_FWDREASON_MOBILEBUSY 0x02
		#define QMI_VOICE_REASON_FWDREASON_NOREPLY 0x03
		#define QMI_VOICE_REASON_FWDREASON_UNREACHABLE 0x04
		#define QMI_VOICE_REASON_FWDREASON_ALLFORWARDING 0x05
		#define QMI_VOICE_REASON_FWDREASON_ALLCONDITIONAL 0x06

	#define QMI_VOICE_GET_CALL_FORWARDING_REQ_TYPE_SERVICE_CLASS 0x10
	#define QMI_VOICE_GET_CALL_FORWARDING_REQ_TYPE_EXTENDED_SERVICE_CLASS 0x11

	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_GET_CALL_FORWARDING_INFO 0x10
		struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_info {
			unsigned char num_instances;
			struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_info_entry_t {
				#define SERVICE_STATUS_INACTIVE 0x00
				#define SERVICE_STATUS_ACTIVE 0x01
				unsigned char service_status;
				#define CLASS_NONE 0X00
				#define CLASS_VOICE 0X01
				#define CLASS_DATA 0X02
				#define CLASS_FAX 0X04
				#define CLASS_SMS 0X08
				#define CLASS_DATACIRCUITSYNC 0X10
				#define CLASS_DATACIRCUITASYNC 0X20
				#define CLASS_PACKETACCESS 0X40
				#define CLASS_PADACCESS 0X80
				unsigned char service_class;
				unsigned char number_len;
				char number[0];
				unsigned char no_reply_timer;
			} __packed e[0];
			unsigned char no_reply_timer;
		} __packed;

	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_FAILURE_CAUSE 0x11
	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_ALPHA_IDENTIFIER 0x12

	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_CALL_CONTROL_RESULT_TYPE 0x13

	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_CALL_ID 0x14

	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_CALL_CONTROL_SUPPLEMENTARY_SERVICE_TYPE 0x15

	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_GET_CALL_FORWARDING_EXTENDED_INFO 0x16
		struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_extended_info {
			unsigned char num_instances;
			struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_extended_info_entry_t {
				unsigned char service_status;
				unsigned char service_class;
				unsigned char no_reply_timer;
				unsigned char pi;
				#define QMI_VOICE_SI_USER_PROVIDED_NOT_SCREENED 0x00
				#define QMI_VOICE_SI_USER_PROVIDED_VERIFIED_PASSED 0x01
				#define QMI_VOICE_SI_USER_PROVIDED_VERIFIED_FAILED 0x02
				#define QMI_VOICE_SI_NETWORK_PROVIDED 0x03
				unsigned char si;
				unsigned char num_type;
				unsigned char num_plan;
				unsigned char num_len;
				unsigned char num[0];
			} __packed e[0];
		} __packed;

	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_GET_CALL_FORWARDING_EXTENDED_INFO_2 0x17
		struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_extended_info_2 {
			unsigned char num_instances;
			struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_extended_info_2_t {
				unsigned int service_class_ext;
				unsigned char service_status;
				unsigned char no_reply_timer;
				unsigned char pi;
				unsigned char si;
				unsigned char num_type;
				unsigned char num_plan;
				unsigned char num_len;
				unsigned char num[0];
			} __packed e[0];

		} __packed;

	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_RETRY_DURATION 0x18
	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_PROVISION 0x19
		#define PROVISION_STATUS_NOT_PROVISIONED 0x00
		#define PROVISION_STATUS_PROVISIONED 0x01


	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_CALL_FORWARDING_START_TIME 0x1A
	#define QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_CALL_FORWARDING_END_TIME 0x1B

// User identify module service (QMIUIM #11)
////////////////////////////////////////////////////////////////////////////////
// shared stuff
#define QMI_UIM_REQ_TYPE_SESSION_INFO 0x01
	struct qmi_uim_session_info {
		unsigned char session_type;
		unsigned char aid_len;
		unsigned char aid[0];
	} __packed;

#define QMI_UIM_READ_TRANSPARENT 0x0020
	#define QMI_UIM_READ_TRANSPARENT_REQ_TYPE_FILE_ID 0x02
	struct qmi_uim_read_transparent_req_file_id {
		unsigned short file_id;
		unsigned char path_len;
		unsigned char path[0];
	} __packed;
	#define QMI_UIM_READ_TRANSPARENT_REQ_TYPE_READ_TRANSPARENT 0x03
	struct qmi_uim_read_transparent_req_read_transparent {
		unsigned short offset;
		unsigned short length;
	} __packed;
	#define QMI_UIM_READ_TRANSPARENT_RESP_TYPE_CARD_RESULT 0x10
	struct qmi_uim_read_transparent_resp_card_result {
		unsigned char sw1;
		unsigned char sw2;
	} __packed;
	#define QMI_UIM_READ_TRANSPARENT_RESP_TYPE_READ_RESULT 0x11
	struct qmi_uim_read_transparent_resp_read_result {
		unsigned short content_len;
		unsigned char content[0];
	} __packed;

#define QMI_UIM_GET_CARD_STATUS 0x002F
	#define QMI_UIM_GET_CARD_STATUS_RESP_TYPE_CARD_STATUS 0x10
	struct qmi_uim_get_card_status_resp_card_status_app {
		unsigned char app_type;
		unsigned char app_state;
		unsigned char perso_state;
		unsigned char perso_feature;
		unsigned char perso_retries;
		unsigned char perso_unlock_retires;
		unsigned char aid_len;
		unsigned char aid_value[0];
	} __packed;
	struct qmi_uim_get_card_status_resp_card_status_app2 {
		unsigned char univ_pin;
		unsigned char pin1_state;
		unsigned char pin1_retries;
		unsigned char puk1_retries;
		unsigned char pin2_state;
		unsigned char pin2_retries;
		unsigned char puk2_retries;
	} __packed;
	struct qmi_uim_get_card_status_resp_card_status_slot {
		unsigned char card_state;
		unsigned char upin_state;
		unsigned char upin_retries;
		unsigned char upuk_retries;
		unsigned char error_code;
		unsigned char num_app;
		struct qmi_uim_get_card_status_resp_card_status_app apps[0];
	} __packed;
	struct qmi_uim_get_card_status_resp_card_status {
		unsigned short index_gw_pri;
		unsigned short index_1x_pri;
		unsigned short index_gw_sec;
		unsigned short index_1x_sec;
		unsigned char num_slot;
        struct qmi_uim_get_card_status_resp_card_status_slot slots[0];
	} __packed;

// common uim pin resp struct
struct qmi_uim_pin_resp {
	char verify_retries_left;
	char unblock_retries_left;
} __packed;

#define QMI_UIM_SET_PIN_PROTECTION 0x0025
	#define QMI_UIM_SET_PIN_PROTECTION_REQ_TYPE 0x02
		struct qmi_uim_set_pin_protection_req {
			unsigned char pin_id;
			unsigned char pin_operation;
			unsigned char pin_length;
			unsigned char pin_value[0];
		} __packed;

	#define QMI_UIM_SET_PIN_PROTECTION_RESP_TYPE 0x10

#define QMI_UIM_VERIFY_PIN  0x0026
	#define QMI_UIM_VERIFY_PIN_REQ_TYPE 0x02
		struct qmi_uim_verify_pin_req {
			unsigned char pin_id;
			unsigned char pin_length;
			unsigned char pin_value[0];
		} __packed;

	#define QMI_UIM_VERIFY_PIN_RESP_TYPE 0x10

#define QMI_UIM_UNBLOCK_PIN 0x0027
	#define QMI_UIM_UNBLOCK_PIN_REQ_TYPE 0x02
		struct qmi_uim_unblock_pin_req {
			unsigned char pin_id;
			unsigned char puk_length;
			unsigned char puk_value[0];
		} __packed;

		struct qmi_uim_unblock_pin_req2 {
			unsigned char new_pin_length;
			unsigned char new_pin_value[0];
		} __packed;

	#define QMI_UIM_UNBLOCK_PIN_RESP_TYPE 0x10


#define QMI_UIM_CHANGE_PIN 0x0028
	#define QMI_UIM_CHANGE_PIN_REQ_TYPE 0x02
		struct qmi_uim_change_pin_req {
			unsigned char pin_id;
			unsigned char old_pin_length;
			unsigned char old_pin_value[0];
		} __packed;

		struct qmi_uim_change_pin_req2 {
			unsigned char new_pin_length;
			unsigned char new_pin_value[0];
		} __packed;

	#define QMI_UIM_CHANGE_PIN_RESP_TYPE 0x10

// Location service (QMILOC #16)
////////////////////////////////////////////////////////////////////////////////
#define QMI_LOC_GET_SUPPORTED_MSGS 0x001E
	#define QMI_LOC_GET_SUPPORTED_MSGS_RESP_TYPE 0x01

	#define QMI_LOC_GET_SUPPORTED_MSGS_RESP_TYPE_LIST_OF_SUPPORTED_MESSAGES 0x10

#define QMI_LOC_REG_EVENTS 0x0021
	#define QMI_LOC_REG_EVENTS_REQ_TYPE_EVENTREGMASK 0x01
	struct qmi_loc_reg_events_req_type_eventregmask {
		unsigned long long eventregmask;
		#define QMI_LOC_EVENT_MASK_POSITION_REPORT                     0x00000001
		#define QMI_LOC_EVENT_MASK_GNSS_SV_INFO                        0x00000002
		#define QMI_LOC_EVENT_MASK_NMEA                                0x00000004
		#define QMI_LOC_EVENT_MASK_NI_NOTIFY_VERIFY_REQ                0x00000008
		#define QMI_LOC_EVENT_MASK_INJECT_TIME_REQ                     0x00000010
		#define QMI_LOC_EVENT_MASK_INJECT_PREDICTED_ORBITS_REQ         0x00000020
		#define QMI_LOC_EVENT_MASK_INJECT_POSITION_REQ                 0x00000040
		#define QMI_LOC_EVENT_MASK_ENGINE_STATE                        0x00000080
		#define QMI_LOC_EVENT_MASK_FIX_SESSION_STATE                   0x00000100
		#define QMI_LOC_EVENT_MASK_WIFI_REQ                            0x00000200
		#define QMI_LOC_EVENT_MASK_SENSOR_STREAMING_READY_STATUS       0x00000400
		#define QMI_LOC_EVENT_MASK_TIME_SYNC_REQ                       0x00000800
		#define QMI_LOC_EVENT_MASK_SET_SPI_STREAMING_REPORT            0x00001000
		#define QMI_LOC_EVENT_MASK_LOCATION_SERVER_CONNECTION_REQ      0x00002000
		#define QMI_LOC_EVENT_MASK_NI_GEOFENCE_NOTIFICATION            0x00004000
		#define QMI_LOC_EVENT_MASK_GEOFENCE_GEN_ALERT                  0x00008000
		#define QMI_LOC_EVENT_MASK_GEOFENCE_BREACH_NOTIFICATION        0x00010000
		#define QMI_LOC_EVENT_MASK_PEDOMETER_CONTROL                   0x00020000
		#define QMI_LOC_EVENT_MASK_MOTION_DATA_CONTROL                 0x00040000
		#define QMI_LOC_EVENT_MASK_BATCH_FULL_NOTIFICATION             0x00080000
		#define QMI_LOC_EVENT_MASK_LIVE_BATCHED_POSITION_REPORT        0x00100000
		#define QMI_LOC_EVENT_MASK_INJECT_WIFI_AP_DATA_REQ             0x00200000
		#define QMI_LOC_EVENT_MASK_GEOFENCE_BATCH_BREACH_NOTIFICATION  0x00400000
		#define QMI_LOC_EVENT_MASK_ VEHICLE_DATA_READY_STATUS          0x00800000
		#define QMI_LOC_EVENT_MASK_GNSS_MEASUREMENT_REPORT             0x01000000
		#define QMI_LOC_EVENT_MASK_GNSS_SV_POLYNOMIAL_REPORT           0x02000000
		#define QMI_LOC_EVENT_MASK_GEOFENCE_PROXIMITY_NOTIFICATION     0x04000000
		#define QMI_LOC_EVENT_MASK_GDT_ UPLOAD_BEGIN_REQ               0x08000000
		#define QMI_LOC_EVENT_MASK_GDT_UPLOAD_END_REQ                  0x10000000
		#define QMI_LOC_EVENT_MASK_GEOFENCE_BATCH_DWELL_NOTIFICATION   0x20000000
		#define QMI_LOC_EVENT_MASK_GET_TIME_ZONE_REQ                   0x40000000
		#define QMI_LOC_EVENT_MASK_BATCHING_STATUS                     0x80000000
	} __packed;

#define QMI_LOC_START 0x0022
	#define QMI_LOC_START_REQ_TYPE_SESSION_ID 0x01
	struct qmi_loc_start_req_type_session_id {
		unsigned char sessionId;
	} __packed;

	#define QMI_LOC_START_REQ_TYPE_RECURRENCE_TYPE 0x10
	struct qmi_loc_start_req_type_recurrence_type {
		unsigned int fixRecurrence;
		#define eQMI_LOC_RECURRENCE_PERIODIC 1
		#define eQMI_LOC_RECURRENCE_SINGLE   2
	} __packed;

	#define QMI_LOC_START_REQ_TYPE_HORIZONTAL_ACCURACY 0x11
	struct qmi_loc_start_req_type_horizontal_accuracy {
		unsigned int horizontalAccuracyLevel;
		#define eQMI_LOC_ACCURACY_LOW  (1)
		#define eQMI_LOC_ACCURACY_MED  (2)
		#define eQMI_LOC_ACCURACY_HIGH (3)
	} __packed;

	#define QMI_LOC_START_REQ_TYPE_INTERMEDIATE_REPORTS 0x12
	struct qmi_loc_start_req_type_intermediate_reports {
		unsigned int intermediateReportState;
		#define eQMI_LOC_INTERMEDIATE_REPORTS_ON  (1)
		#define eQMI_LOC_INTERMEDIATE_REPORTS_OFF (2)
	} __packed;

	#define QMI_LOC_START_REQ_TYPE_MINIMUM_INTERVAL_REPORTS 0x13
	struct qmi_loc_start_req_type_minimum_interval_reports {
		unsigned int minInterval;
	} __packed;

#define QMI_LOC_STOP 0x0023
	#define QMI_LOC_STOP_REQ_TYPE_SESSION_ID 0x01
	struct qmi_loc_stop_req_type_session_id {
		unsigned char sessionId;
	} __packed;

#define QMI_LOC_EVENT_POSITION_REPORT_IND 0x0024
	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SESSION_STATUS 0x01
	struct qmi_loc_event_position_report_ind_type_session_status {
		unsigned int sessionStatus;
		#define eQMI_LOC_SESS_STATUS_SUCCESS         (0)
		#define eQMI_LOC_SESS_STATUS_IN_PROGRESS     (1)
		#define eQMI_LOC_SESS_STATUS_GENERAL_FAILURE (2)
		#define eQMI_LOC_SESS_STATUS_TIMEOUT         (3)
		#define eQMI_LOC_SESS_STATUS_USER_END        (4)
		#define eQMI_LOC_SESS_STATUS_BAD_PARAMETER   (5)
		#define eQMI_LOC_SESS_STATUS_PHONE_OFFLINE   (6)
		#define eQMI_LOC_SESS_STATUS_ENGINE_LOCKED   (7)
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SESSIONID 0x02
	struct qmi_loc_event_position_report_ind_type_sessionid {
		unsigned char sessionId;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_LATITUDE 0x10
	struct qmi_loc_event_position_report_ind_type_latitude {
		double latitude;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_LONGITUDE 0x11
	struct qmi_loc_event_position_report_ind_type_longitude {
		double longitude;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_CIRCULAR_HORIZONTAL_POSITION_UNCERTAINTY 0x12
	struct qmi_loc_event_position_report_ind_type_circular_horizontal_position_uncertainty {
		float horUncCircular;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_ELLIPTICAL_UNCERTAINTY_SEMI_MINOR_AXIS 0x13
	struct qmi_loc_event_position_report_ind_type_horizontal_elliptical_uncertainty_semi_minor_axis {
		float horUncEllipseSemiMinor;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_ELLIPTICAL_UNCERTAINTYSEMI_MAJOR_AXIS 0x14
	struct qmi_loc_event_position_report_ind_type_horizontal_elliptical_uncertaintysemi_major_axis {
		float horUncEllipseSemiMajor;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_ELLIPTICAL_HORIZONTAL_UNCERTAINTY_AZIMUTH 0x15
	struct qmi_loc_event_position_report_ind_type_elliptical_horizontal_uncertainty_azimuth {
		float horUncEllipseOrientAzimuth;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_CONFIDENCE 0x16
	struct qmi_loc_event_position_report_ind_type_horizontal_confidence {
		unsigned char horConfidence;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_RELIABILITY 0x17
	struct qmi_loc_event_position_report_ind_type_horizontal_reliability {
		unsigned int horReliability;
		#define eQMI_LOC_RELIABILITY_NOT_SET  (0)
		#define eQMI_LOC_RELIABILITY_VERY_LOW (1)
		#define eQMI_LOC_RELIABILITY_LOW      (2)
		#define eQMI_LOC_RELIABILITY_MEDIUM   (3)
		#define eQMI_LOC_RELIABILITY_HIGH     (4)
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_SPEED 0x18
	struct qmi_loc_event_position_report_ind_type_horizontal_speed {
		float speedHorizontal;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SPEED_UNCERTAINTY 0x19
	struct qmi_loc_event_position_report_ind_type_speed_uncertainty {
		float speedUnc;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_ALTITUDE_WITH_RESPECT_TO_ELLIPSOID 0x1A
	struct qmi_loc_event_position_report_ind_type_altitude_with_respect_to_ellipsoid {
		float altitudeWrtEllipsoid;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_ALTITUDE_WITH_RESPECT_TO_SEA_LEVEL 0x1B
	struct qmi_loc_event_position_report_ind_type_altitude_with_respect_to_sea_level {
		float altitudeWrtMeanSeaLevel;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_VERTICAL_UNCERTAINTY 0x1C
	struct qmi_loc_event_position_report_ind_type_vertical_uncertainty {
		float vertUnc;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_VERTICAL_CONFIDENCE 0x1D
	struct qmi_loc_event_position_report_ind_type_vertical_confidence {
		unsigned char vertConfidence;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_VERTICAL_RELIABILITY 0x1E
	struct qmi_loc_event_position_report_ind_type_vertical_reliability {
		unsigned int vertReliability;
		#define eQMI_LOC_RELIABILITY_NOT_SET  (0)
		#define eQMI_LOC_RELIABILITY_VERY_LOW (1)
		#define eQMI_LOC_RELIABILITY_LOW      (2)
		#define eQMI_LOC_RELIABILITY_MEDIUM   (3)
		#define eQMI_LOC_RELIABILITY_HIGH     (4)
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_VERTICAL_SPEED 0x1F
	struct qmi_loc_event_position_report_ind_type_vertical_speed {
		float speedVertical;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HEADING 0x20
	struct qmi_loc_event_position_report_ind_type_heading {
		float heading;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HEADING_UNCERTAINTY 0x21
	struct qmi_loc_event_position_report_ind_type_heading_uncertainty {
		float headingUnc;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_MAGNETIC_DEVIATION 0x22
	struct qmi_loc_event_position_report_ind_type_magnetic_deviation {
		float magneticDeviation;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_TECHNOLOGY_USED 0x23
	struct qmi_loc_event_position_report_ind_type_technology_used {
		unsigned int technologyMask;
		#define QMI_LOC_POS_TECH_MASK_SATELLITE                (0x00000001)
		#define QMI_LOC_POS_TECH_MASK_CELLID                   (0x00000002)
		#define QMI_LOC_POS_TECH_MASK_WIFI                     (0x00000004)
		#define QMI_LOC_POS_TECH_MASK_SENSORS                  (0x00000008)
		#define QMI_LOC_POS_TECH_MASK_REFERENCE_LOCATION       (0x00000010)
		#define QMI_LOC_POS_TECH_MASK_INJECTED_COARSE_POSITION (0x00000020)
		#define QMI_LOC_POS_TECH_MASK_AFLT                     (0x00000040)
		#define QMI_LOC_POS_TECH_MASK_HYBRID                   (0x00000080)
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_DILUTION_OF_PRECISION 0x24
	struct qmi_loc_event_position_report_ind_type_dilution_of_precision {
		float PDOP;
		float HDOP;
		float VDOP;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_UTC_TIMESTAMP 0x25
	struct qmi_loc_event_position_report_ind_type_utc_timestamp {
		unsigned long long timestampUtc;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_LEAP_SECONDS 0x26
	struct qmi_loc_event_position_report_ind_type_leap_seconds {
		unsigned char leapSeconds;

	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_GPS_TIME 0x27
	struct qmi_loc_event_position_report_ind_type_gps_time {
		unsigned short gpsWeek;
		unsigned int gpsTimeOfWeekMs;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_TIME_UNCERTAINTY 0x28
	struct qmi_loc_event_position_report_ind_type_time_uncertainty {
		float timeUnc;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_TIME_SOURCE 0x29
	struct qmi_loc_event_position_report_ind_type_time_source {
		unsigned int timeSrc;
		#define eQMI_LOC_TIME_SRC_INVALID                  (0)
		#define eQMI_LOC_TIME_SRC_NETWORK_TIME_TRANSFER    (1)
		#define eQMI_LOC_TIME_SRC_NETWORK_TIME_TAGGING     (2)
		#define eQMI_LOC_TIME_SRC_EXTERNAL_INPUT           (3)
		#define eQMI_LOC_TIME_SRC_TOW_DECODE               (4)
		#define eQMI_LOC_TIME_SRC_TOW_CONFIRMED            (5)
		#define eQMI_LOC_TIME_SRC_TOW_AND_WEEK_CONFIRMED   (6)
		#define eQMI_LOC_TIME_SRC_NAV_SOLUTION             (7)
		#define eQMI_LOC_TIME_SRC_SOLVE_FOR_TIME           (8)
		#define eQMI_LOC_TIME_SRC_GLO_TOW_DECODE           (9)
		#define eQMI_LOC_TIME_SRC_TIME_TRANSFORM           (10)
		#define eQMI_LOC_TIME_SRC_WCDMA_SLEEP_TIME_TAGGING (11)
		#define eQMI_LOC_TIME_SRC_GSM_SLEEP_TIME_TAGGING   (12)
		#define eQMI_LOC_TIME_SRC_UNKNOWN                  (13)
		#define eQMI_LOC_TIME_SRC_SYSTEM_TIMETICK          (14)
		#define eQMI_LOC_TIME_SRC_QZSS_TOW_DECODE          (15)
		#define eQMI_LOC_TIME_SRC_BDS_TOW_DECODE           (16)
		#define eQMI_LOC_TIME_SRC_GAL_TOW_DECODE           (17)
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SENSOR_DATA_USAGE 0x2A
	struct qmi_loc_event_position_report_ind_type_sensor_data_usage {
		unsigned int usageMask;
		#define SENSOR_USED_ACCEL (0x00000001)
		#define SENSOR_USED_GYRO   (0x00000002)

		unsigned int  aidingIndicatorMask;
		#define AIDED_HEADING (0x00000001)
		#define AIDED_SPEED    (0x00000002)
		#define AIDED_POSITION (0x00000004)
		#define AIDED_VELOCITY (0x00000008)
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_FIX_COUNT_FOR_THIS_SESSION 0x2B
	struct qmi_loc_event_position_report_ind_type_fix_count_for_this_session {
		unsigned int fixId;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SVS_USED_TO_CALCULATE_THE_FIX 0x2C
	struct qmi_loc_event_position_report_ind_type_svs_used_to_calculate_the_fix {
		unsigned char gnssSvUsedList_len;
		unsigned short gnssSvUsedList;
	} __packed;

	#define QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_ALTITUDE_ASSUMED 0x2D
	struct qmi_loc_event_position_report_ind_type_altitude_assumed {
		unsigned char altitudeAssumed;
	} __packed;


#define QMI_LOC_EVENT_GNSS_SV_INFO_IND 0x0025
	#define QMI_LOC_EVENT_GNSS_SV_INFO_IND_TYPE_ALTITUDE_ASSUMED 0x01
	// boolean altitudeAssumed

#define QMI_LOC_EVENT_NMEA_IND 0x0026
	#define QMI_LOC_EVENT_NMEA_IND_TYPE_NMEA_STRING 0x01
	// Type: NULL-terminated string
	// Maximum string length (including NULL terminator): 201

#define QMI_LOC_EVENT_ENGINE_STATE_IND 0x002b
	#define QMI_LOC_EVENT_ENGINE_STATE_IND_TYPE_ENGINE_STATE 0x01
	struct qmi_loc_event_engine_state_ind_type_engine_state {
		unsigned int engineState;
	} __packed;

// Request message with Asynchronous Messaging Paradigm
#define QMI_LOC_SET_OPERATION_MODE 0x004A
	#define QMI_LOC_SET_OPERATION_MODE_REQ_TYPE_OPERATION_MODE 0x01
	struct qmi_loc_set_operation_mode_req_type_operation_mode {
		unsigned int operationMode;
	} __packed;
	enum loc_operation_mode {
		eQMI_LOC_OPER_MODE_DEFAULT=1,
		eQMI_LOC_OPER_MODE_MSB=2,
		eQMI_LOC_OPER_MODE_MSA=3,
		eQMI_LOC_OPER_MODE_STANDALONE=4,
		eQMI_LOC_OPER_MODE_CELL_ID=5,
		eQMI_LOC_OPER_MODE_WWAN=6
	};

	#define QMI_LOC_SET_OPERATION_MODE_RESP_TYPE 0x01
	#define QMI_LOC_SET_OPERATION_MODE_IND_TYPE_SET_OPERATION_MODE_STATUS 0x01
	struct qmi_loc_set_operation_mode_ind_type_set_operation_mode_status {
		unsigned int set_operation_mode_status;
	} __packed;

#endif
