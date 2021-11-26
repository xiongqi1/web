/*!
 * C header file of LTE NAS protocol parser
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

/*
        EMM and ESM protocol structures in this file are based on 3GPP 24.301 v13.5.0 (2016-04) section 8

        * limitation and assumption

        For a better maintenance and readability,

                1. Apply best effort to use full names of 3GPP protocol type names for structures and members
                2. Refer 3GPP document and section number for a structure
                3. Use packed type.
                4. Consider little-endian processors only for the initial structure
                5. Use bit type regardless of bit length of structure members - to easily convert structures to big-endian in the future if required
*/

#ifndef __LTE_EMM_ESM_H__
#define __LTE_EMM_ESM_H__

#define __packed __attribute__((__packed__))

/* 3GPP 24.301 table 9.8.1 */

/* EPS mobility management messages */
#define EMM_EPS_MOBILITY_MANAGEMENT_MESSAGES 0x40
/* Attach request */
#define EMM_ATTACH_REQUEST 0x41
/* Attach accept */
#define EMM_ATTACH_ACCEPT 0x42
/* Attach complete */
#define EMM_ATTACH_COMPLETE 0x43
/* Attach reject */
#define EMM_ATTACH_REJECT 0x44
/* Detach request */
#define EMM_DETACH_REQUEST 0x45
/* Detach accept */
#define EMM_DETACH_ACCEPT 0x46
/* Tracking area update request */
#define EMM_TRACKING_AREA_UPDATE_REQUEST 0x48
/* Tracking area update accept */
#define EMM_TRACKING_AREA_UPDATE_ACCEPT 0x49
/* Tracking area update complete */
#define EMM_TRACKING_AREA_UPDATE_COMPLETE 0x4a
/* Tracking area update reject */
#define EMM_TRACKING_AREA_UPDATE_REJECT 0x4b
/* Extended service request */
#define EMM_EXTENDED_SERVICE_REQUEST 0x4c
/* Service reject */
#define EMM_SERVICE_REJECT 0x4e
/* GUTI reallocation command */
#define EMM_GUTI_REALLOCATION_COMMAND 0x50
/* GUTI reallocation complete */
#define EMM_GUTI_REALLOCATION_COMPLETE 0x51
/* Authentication request */
#define EMM_AUTHENTICATION_REQUEST 0x52
/* Authentication response */
#define EMM_AUTHENTICATION_RESPONSE 0x53
/* Authentication reject */
#define EMM_AUTHENTICATION_REJECT 0x54
/* Authentication failure */
#define EMM_AUTHENTICATION_FAILURE 0x5c
/* Identity request */
#define EMM_IDENTITY_REQUEST 0x55
/* Identity response */
#define EMM_IDENTITY_RESPONSE 0x56
/* Security mode command */
#define EMM_SECURITY_MODE_COMMAND 0x5d
/* Security mode complete */
#define EMM_SECURITY_MODE_COMPLETE 0x5e
/* Security mode reject */
#define EMM_SECURITY_MODE_REJECT 0x5f
/* EMM status */
#define EMM_STATUS 0x60
/* EMM information */
#define EMM_INFORMATION 0x61
/* Downlink NAS transport */
#define EMM_DOWNLINK_NAS_TRANSPORT 0x62
/* Uplink NAS transport */
#define EMM_UPLINK_NAS_TRANSPORT 0x63
/* CS Service notification */
#define EMM_CS_SERVICE_NOTIFICATION 0x64
/* Downlink generic NAS transport */
#define EMM_DOWNLINK_GENERIC_NAS_TRANSPORT 0x68
/* Uplink generic NAS transport */
#define EMM_UPLINK_GENERIC_NAS_TRANSPORT 0x69

/* 3GPP 24.301 table 9.8.2 */

/* EPS session management messages */
#define ESM_EPS_SESSION_MANAGEMENT_MESSAGES 0xc0
/* Activate default EPS bearer context request */
#define ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST 0xc1
/* Activate default EPS bearer context accept */
#define ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT 0xc2
/* Activate default EPS bearer context reject */
#define ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REJECT 0xc3
/* Activate dedicated EPS bearer context request */
#define ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST 0xc5
/* Activate dedicated EPS bearer context accept */
#define ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_ACCEPT 0xc6
/* Activate dedicated EPS bearer context reject */
#define ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REJECT 0xc7
/* Modify EPS bearer context request */
#define ESM_MODIFY_EPS_BEARER_CONTEXT_REQUEST 0xc9
/* Modify EPS bearer context accept */
#define ESM_MODIFY_EPS_BEARER_CONTEXT_ACCEPT 0xca
/* Modify EPS bearer context reject */
#define ESM_MODIFY_EPS_BEARER_CONTEXT_REJECT 0xcb
/* Deactivate EPS bearer context request */
#define ESM_DEACTIVATE_EPS_BEARER_CONTEXT_REQUEST 0xcd
/* Deactivate EPS bearer context accept */
#define ESM_DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT 0xce
/* PDN connectivity request */
#define ESM_PDN_CONNECTIVITY_REQUEST 0xd0
/* PDN connectivity reject */
#define ESM_PDN_CONNECTIVITY_REJECT 0xd1
/* PDN disconnect request */
#define ESM_PDN_DISCONNECT_REQUEST 0xd2
/* PDN disconnect reject */
#define ESM_PDN_DISCONNECT_REJECT 0xd3
/* Bearer resource allocation request */
#define ESM_BEARER_RESOURCE_ALLOCATION_REQUEST 0xd4
/* Bearer resource allocation reject */
#define ESM_BEARER_RESOURCE_ALLOCATION_REJECT 0xd5
/* Bearer resource modification request */
#define ESM_BEARER_RESOURCE_MODIFICATION_REQUEST 0xd6
/* Bearer resource modification reject */
#define ESM_BEARER_RESOURCE_MODIFICATION_REJECT 0xd7
/* ESM information request */
#define ESM_INFORMATION_REQUEST 0xd9
/* ESM information response */
#define ESM_INFORMATION_RESPONSE 0xda
/* Notification */
#define ESM_NOTIFICATION 0xdb
/* ESM status */
#define ESM_ESM_STATUS 0xe8
/* Remote UE report */
#define ESM_REMOTE_UE_REPORT 0xe9
/* Remote UE report response */
#define ESM_REMOTE_UE_REPORT_RESPONSE 0xea

/* 3GPP 24.301 table 9.3.1 */
#define SECURITY_HEADER_TYPE_PLAIN 0x00
#define SECURITY_HEADER_TYPE_SERVICE_REQUEST 0x0c

/* 3GPP 24.007 table 11.2 */
#define PROTOCOL_DISCRIMINATOR_ESM 0x02
#define PROTOCOL_DISCRIMINATOR_EMM 0x07

/*
        ### 3GPP 24.301 9.9.3.9 ###
*/

/* IMSI unknown in HSS */
#define EMM_CAUSE_IMSI_UNKNOWN_IN_HSS 0x0002
/* Illegal UE */
#define EMM_CAUSE_ILLEGAL_UE 0x0003
/* IMEI not accepted */
#define EMM_CAUSE_IMEI_NOT_ACCEPTED 0x0005
/* Illegal ME */
#define EMM_CAUSE_ILLEGAL_ME 0x0006
/* EPS services not allowed */
#define EMM_CAUSE_EPS_SERVICES_NOT_ALLOWED 0x0007
/* EPS services and non-EPS services not allowed */
#define EMM_CAUSE_EPS_SERVICES_AND_NON_EPS_SERVICES_NOT_ALLOWED 0x0008
/* UE identity cannot be derived by the network */
#define EMM_CAUSE_UE_IDENTITY_CANNOT_BE_DERIVED_BY_THE_NETWORK 0x0009
/* Implicitly detached */
#define EMM_CAUSE_IMPLICITLY_DETACHED 0x000a
/* PLMN not allowed */
#define EMM_CAUSE_PLMN_NOT_ALLOWED 0x000b
/* Tracking Area not allowed */
#define EMM_CAUSE_TRACKING_AREA_NOT_ALLOWED 0x000c
/* Roaming not allowed in this tracking area */
#define EMM_CAUSE_ROAMING_NOT_ALLOWED_IN_THIS_TRACKING_AREA 0x000d
/* EPS services not allowed in this PLMN */
#define EMM_CAUSE_EPS_SERVICES_NOT_ALLOWED_IN_THIS_PLMN 0x000e
/* No Suitable Cells In tracking area */
#define EMM_CAUSE_NO_SUITABLE_CELLS_IN_TRACKING_AREA 0x000f
/* MSC temporarily not reachable */
#define EMM_CAUSE_MSC_TEMPORARILY_NOT_REACHABLE 0x0010
/* Network failure */
#define EMM_CAUSE_NETWORK_FAILURE 0x0011
/* CS domain not available */
#define EMM_CAUSE_CS_DOMAIN_NOT_AVAILABLE 0x0012
/* ESM failure */
#define EMM_CAUSE_ESM_FAILURE 0x0013
/* MAC failure */
#define EMM_CAUSE_MAC_FAILURE 0x0014
/* Synch failure */
#define EMM_CAUSE_SYNCH_FAILURE 0x0015
/* Congestion */
#define EMM_CAUSE_CONGESTION 0x0016
/* UE security capabilities mismatch */
#define EMM_CAUSE_UE_SECURITY_CAPABILITIES_MISMATCH 0x0017
/* Security mode rejected, unspecified */
#define EMM_CAUSE_SECURITY_MODE_REJECTED_UNSPECIFIED 0x0018
/* Not authorized for this CSG */
#define EMM_CAUSE_NOT_AUTHORIZED_FOR_THIS_CSG 0x0019
/* Non-EPS authentication unacceptable */
#define EMM_CAUSE_NON_EPS_AUTHENTICATION_UNACCEPTABLE 0x001a
/* Requested service option not authorized in this PLMN */
#define EMM_CAUSE_REQUESTED_SERVICE_OPTION_NOT_AUTHORIZED_IN_THIS_PLMN 0x0023
/* CS service temporarily not available */
#define EMM_CAUSE_CS_SERVICE_TEMPORARILY_NOT_AVAILABLE 0x0027
/* No EPS bearer context activated */
#define EMM_CAUSE_NO_EPS_BEARER_CONTEXT_ACTIVATED 0x0028
/* Severe network failure */
#define EMM_CAUSE_SEVERE_NETWORK_FAILURE 0x002a
/* Semantically incorrect message */
#define EMM_CAUSE_SEMANTICALLY_INCORRECT_MESSAGE 0x005f
/* Invalid mandatory information */
#define EMM_CAUSE_INVALID_MANDATORY_INFORMATION 0x0060
/* Message type non-existent or not implemented */
#define EMM_CAUSE_MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED 0x0061
/* Message type not compatible with the protocol state */
#define EMM_CAUSE_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_THE_PROTOCOL_STATE 0x0062
/* Information element non-existent or not implemented */
#define EMM_CAUSE_INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED 0x0063
/* Conditional IE error */
#define EMM_CAUSE_CONDITIONAL_IE_ERROR 0x0064
/* Message not compatible with the protocol state */
#define EMM_CAUSE_MESSAGE_NOT_COMPATIBLE_WITH_THE_PROTOCOL_STATE 0x0065
/* Protocol error, unspecified */
#define EMM_CAUSE_PROTOCOL_ERROR_UNSPECIFIED 0x006f

/*
        ### 3GPP 24.301 9.9.4.4.1 ###
*/

/* Operator Determined Barring */
#define ESM_CAUSE_OPERATOR_DETERMINED_BARRING 0x0008
/* Insufficient resources */
#define ESM_CAUSE_INSUFFICIENT_RESOURCES 0x001a
/* Missing or unknown APN */
#define ESM_CAUSE_MISSING_OR_UNKNOWN_APN 0x001b
/* Unknown PDN type */
#define ESM_CAUSE_UNKNOWN_PDN_TYPE 0x001c
/* User authentication failed */
#define ESM_CAUSE_USER_AUTHENTICATION_FAILED 0x001d
/* Request rejected by Serving GW or PDN GW */
#define ESM_CAUSE_REQUEST_REJECTED_BY_SERVING_GW_OR_PDN_GW 0x001e
/* Request rejected, unspecified */
#define ESM_CAUSE_REQUEST_REJECTED__UNSPECIFIED 0x001f
/* Service option not supported */
#define ESM_CAUSE_SERVICE_OPTION_NOT_SUPPORTED 0x0020
/* Requested service option not subscribed */
#define ESM_CAUSE_REQUESTED_SERVICE_OPTION_NOT_SUBSCRIBED 0x0021
/* Service option temporarily out of order */
#define ESM_CAUSE_SERVICE_OPTION_TEMPORARILY_OUT_OF_ORDER 0x0022
/* PTI already in use */
#define ESM_CAUSE_PTI_ALREADY_IN_USE 0x0023
/* Regular deactivation */
#define ESM_CAUSE_REGULAR_DEACTIVATION 0x0024
/* EPS QoS not accepted */
#define ESM_CAUSE_EPS_QOS_NOT_ACCEPTED 0x0025
/* Network failure */
#define ESM_CAUSE_NETWORK_FAILURE 0x0026
/* Reactivation requested */
#define ESM_CAUSE_REACTIVATION_REQUESTED 0x0027
/* Semantic error in the TFT operation */
#define ESM_CAUSE_SEMANTIC_ERROR_IN_THE_TFT_OPERATION 0x0029
/* Syntactical error in the TFT operation */
#define ESM_CAUSE_SYNTACTICAL_ERROR_IN_THE_TFT_OPERATION 0x002a
/* Invalid EPS bearer identity */
#define ESM_CAUSE_INVALID_EPS_BEARER_IDENTITY 0x002b
/* Semantic errors in packet filter(s) */
#define ESM_CAUSE_SEMANTIC_ERRORS_IN_PACKET_FILTERS 0x002c
/* Syntactical errors in packet filter(s) */
#define ESM_CAUSE_SYNTACTICAL_ERRORS_IN_PACKET_FILTERS 0x002d
/* Unused (see NOTE 2) */
#define ESM_CAUSE_UNUSED_SEE_NOTE_2 0x002e
/* PTI mismatch */
#define ESM_CAUSE_PTI_MISMATCH 0x002f
/* Last PDN disconnection not allowed */
#define ESM_CAUSE_LAST_PDN_DISCONNECTION_NOT_ALLOWED 0x0031
/* PDN type IPv4 only allowed */
#define ESM_CAUSE_PDN_TYPE_IPV4_ONLY_ALLOWED 0x0032
/* PDN type IPv6 only allowed */
#define ESM_CAUSE_PDN_TYPE_IPV6_ONLY_ALLOWED 0x0033
/* Single address bearers only allowed */
#define ESM_CAUSE_SINGLE_ADDRESS_BEARERS_ONLY_ALLOWED 0x0034
/* ESM information not received */
#define ESM_CAUSE_ESM_INFORMATION_NOT_RECEIVED 0x0035
/* PDN connection does not exist */
#define ESM_CAUSE_PDN_CONNECTION_DOES_NOT_EXIST 0x0036
/* Multiple PDN connections for a given APN not allowed */
#define ESM_CAUSE_MULTIPLE_PDN_CONNECTIONS_FOR_A_GIVEN_APN_NOT_ALLOWED 0x0037
/* Collision with network initiated request */
#define ESM_CAUSE_COLLISION_WITH_NETWORK_INITIATED_REQUEST 0x0038
/* Unsupported QCI value */
#define ESM_CAUSE_UNSUPPORTED_QCI_VALUE 0x003b
/* Bearer handling not supported */
#define ESM_CAUSE_BEARER_HANDLING_NOT_SUPPORTED 0x003c
/* Maximum number of EPS bearers reached */
#define ESM_CAUSE_MAXIMUM_NUMBER_OF_EPS_BEARERS_REACHED 0x0041
/* Requested APN not supported in current RAT and PLMN combination */
#define ESM_CAUSE_REQUESTED_APN_NOT_SUPPORTED_IN_CURRENT_RAT_AND_PLMN_COMBINATION 0x0042
/* Invalid PTI value */
#define ESM_CAUSE_INVALID_PTI_VALUE 0x0051
/* Semantically incorrect message */
#define ESM_CAUSE_SEMANTICALLY_INCORRECT_MESSAGE 0x005f
/* Invalid mandatory information */
#define ESM_CAUSE_INVALID_MANDATORY_INFORMATION 0x0060
/* Message type non-existent or not implemented */
#define ESM_CAUSE_MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED 0x0061
/* Message type not compatible with the protocol state */
#define ESM_CAUSE_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_THE_PROTOCOL_STATE 0x0062
/* Information element non-existent or not implemented */
#define ESM_CAUSE_INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED 0x0063
/* Conditional IE error */
#define ESM_CAUSE_CONDITIONAL_IE_ERROR 0x0064
/* Message not compatible with the protocol state */
#define ESM_CAUSE_MESSAGE_NOT_COMPATIBLE_WITH_THE_PROTOCOL_STATE 0x0065
/* Protocol error, unspecified */
#define ESM_CAUSE_PROTOCOL_ERROR__UNSPECIFIED 0x006f
/* APN restriction value incompatible with active EPS bearer context */
#define ESM_CAUSE_APN_RESTRICTION_VALUE_INCOMPATIBLE_WITH_ACTIVE_EPS_BEARER_CONTEXT 0x0070
/* Multiple accesses to a PDN connection not allowed */
#define ESM_CAUSE_MULTIPLE_ACCESSES_TO_A_PDN_CONNECTION_NOT_ALLOWED 0x0071
/* This ESM cause is not 3GPP defined - Netcomm defined ESM cause */
#define ESM_CAUSE_NETCOMM_DISCONNECTION_WITHOUT_NAS_SIGNALING 0x0001

/* 3GPP 24.007 figure 11.4 */
struct T {
    unsigned int t : 8;
} __packed;

/* 3GPP 24.007 figure 11.6 */
struct TV {
    unsigned int tv : 8;
    char v[0];
} __packed;

/* 3GPP 24.007 figure 11.7 */
struct LV {
    unsigned int l : 8;
    char v[0];
} __packed;

/* 3GPP 24.007 figure 11.8 */
struct TLV {
    unsigned int t : 8;
    unsigned int l : 8;
    char v[0];
} __packed;

/* 3GPP 24.007 figure 11.9 */
struct LVE {
    unsigned int l_hi : 8;
    unsigned int l_lo : 8;
    char v[0];
} __packed;

/* 3GPP 24.007 figure 11.10 */
struct TLVE {
    unsigned int t : 8;
    unsigned int l_hi : 8;
    unsigned int l_lo : 8;
    char v[0];
} __packed;

/* 3GPP command header */
struct emm_protocol_header {
    unsigned int protocol_discriminator : 4;
    unsigned int security_header_type : 4;
} __packed;

struct emm_message_type_header {
    unsigned int protocol_discriminator : 4;
    unsigned int security_header_type : 4;
    unsigned int message_type : 8;
} __packed;

struct esm_message_type_header {
    unsigned int protocol_discriminator : 4;         /* Protocol discriminator 9.2 M V 1/2 */
    unsigned int eps_bearer_identity : 4;            /* EPS bearer identity 9.3.2 M V 1/2 */
    unsigned int procedure_transaction_identity : 8; /* Procedure transaction identity 9.4 M V 1 */
    unsigned int message_type : 8;                   /* Message type 9.8 M V 1 */
} __packed;

/* 3GPP 24.301 8.2.1 */
struct emm_attach_accept {
    struct emm_message_type_header header;

    unsigned int eps_attach_result : 4;             /* EPS attach result 9.9.3.10 M V 1/2 */
    unsigned int spare_half_octet : 4;              /* Spare half octet 9.9.2.9 M V 1/2 */
    unsigned int t3412_value_gprs_timer : 8;        /* T3412 value GPRS timer 9.9.3.16 M V 1 */
    struct LV tai_list_tracking_area_identity_list; /* TAI list Tracking area identity list 9.9.3.33 M LV 7-97 */
    struct LVE esm_message_container;               /* ESM message container 9.9.3.15 M LVE 5-n */
    struct TLV guti_eps_mobile_identity;            /* GUTI EPS mobile identity (50) 9.9.3.12 O TLV 13 */
    struct TV location_area_identification;         /* Location area identification (13) 9.9.2.2 O TV 6 */
    struct TLV ms_identity_mobile_identity;         /* MS identity Mobile identity (23) 9.9.2.3 O TLV 7-10 */
    struct TV emm_cause;                            /* EMM cause (53) 9.9.3.9 O TV 2 */
    struct TV t3402_value_gprs_timer;               /* T3402 value GPRS timer (17) 9.9.3.16 O TV 2 */
    struct TV t3423_value_gprs_timer;               /* T3423 value GPRS timer (59) 9.9.3.16 O TV 2 */
    struct TLV equivalent_plmns_plmn_list;          /* Equivalent PLMNs PLMN list (4A) 9.9.2.8 O TLV 5-47 */
    struct TLV emergency_number_list;               /* Emergency number list (34) 9.9.3.37 O TLV 5-50 */
    struct TLV eps_network_feature_support;         /* EPS network feature support (64) 9.9.3.12A O TLV 3 */
    struct TV additional_update_result;             /* Additional update result (F-) 9.9.3.0A O TV 1 */
    struct TLV t3412_extended_value_gprs_timer_3;   /* T3412 extended value GPRS timer 3 (5E) 9.9.3.16B O TLV 3 */
    struct TLV t3324_value_gprs_timer_2;            /* T3324 value GPRS timer 2 (6A) 9.9.3.16A O TLV 3 */
    struct TLV extended_drx_parameters;             /* Extended DRX parameters (6E) 9.9.3.46 O TLV 3 */
} __packed;

/* 3GPP 24.301 8.2.2 */
struct emm_attach_complete {
    struct emm_message_type_header header;
    struct LVE esm_message_container; /* ESM message container 9.9.3.15 M LVE 5-n */
} __packed;

/* 3GPP 24.301 8.2.3 */
struct emm_attach_reject {
    struct emm_message_type_header header;
    unsigned int emm_cause : 8;          /* EMM cause 9.9.3.9 M V 1 */
    struct TLVE esm_message_container;   /* ESM message container (78) 9.9.3.15 O TLVE 6-n */
    struct TLV t3346_value_gprs_timer_2; /* T3346 value GPRS timer 2 (5F) 9.9.3.16A O TLV 3 */
    struct TLV t3402_value_gprs_timer_2; /* T3402 value GPRS timer 2 (16) 9.9.3.16A O TLV 3 */
    struct TV extended_emm_cause;        /* Extended EMM cause (A-) 9.9.3.26A O TV 1 */
} __packed;

/* 3GPP 24.301 8.2.4 */
struct emm_attach_request {
    struct emm_message_type_header header;
    unsigned int eps_attach_type : 4;                             /* EPS attach type 9.9.3.11 M V 1/2 */
    unsigned int nas_key_set_identifier : 4;                      /* NAS key set identifier 9.9.3.21 M V 1/2 */
    struct LV eps_mobile_identity;                                /* EPS mobile identity 9.9.3.12 M LV 5-12 */
    struct LV ue_network_capability;                              /* UE network capability 9.9.3.34 M LV 3-14 */
    struct LVE esm_message_container;                             /* ESM message container 9.9.3.15 M LVE 5-n */
    struct TV old_p_tmsi_signature_p_tmsi_signature;              /* Old P-TMSI signature P-TMSI signature (19) 9.9.3.26 O TV 4 */
    struct TLV additional_guti_eps_mobile_identity;               /* Additional GUTI EPS mobile identity (50) 9.9.3.12 O TLV 13 */
    struct TV last_visited_registered_tai_tracking_area_identity; /* Last visited registered TAI Tracking area identity (52) 9.9.3.32 O TV 6 */
    struct TV drx_parameter;                                      /* DRX parameter (5C) 9.9.3.8 O TV 3 */
    struct TLV ms_network_capability;                             /* MS network capability (31) 9.9.3.20 O TLV 4-10 */
    struct TV old_location_area_identification_location_area_identification; /* Old location area identification Location area identification
                                                                                (13) 9.9.2.2 O TV 6 */
    struct TV tmsi_status;                                                   /* TMSI status (9-) 9.9.3.31 O TV 1 */
    struct TLV mobile_station_classmark_2;                                   /* Mobile station classmark 2 (11) 9.9.2.4 O TLV 5 */
    struct TLV mobile_station_classmark_3;                                   /* Mobile station classmark 3 (20) 9.9.2.5 O TLV 2-34 */
    struct TLV supported_codecs_supported_codec_list;                        /* Supported Codecs Supported Codec List (40) 9.9.2.10 O TLV 5-n */
    struct TV additional_update_type;                                        /* Additional update type (F-) 9.9.3.0B O TV 1 */
    struct TLV voice_domain_preference_and_ues_usage_setting; /* Voice domain preference and UE's usage setting (5D) 9.9.3.44 O TLV 3 */
    struct TV device_properties;                              /* Device properties (D-) 9.9.2.0A O TV 1 */
    struct TV old_guti_type_guti_type;                        /* Old GUTI type GUTI type (E-) 9.9.3.45 O TV 1 */
    struct TV ms_network_feature_support;                     /* MS network feature support (C-) 9.9.3.20A O TV 1 */
    struct TLV tmsi_based_nri_container_network_resource_identifier_container; /* TMSI based NRI container Network resource identifier container
                                                                                  (10) 9.9.3.24A O TLV 4 */
    struct TLV t3324_value_gprs_timer_2;                                       /* T3324 value GPRS timer 2 (6A) 9.9.3.16A O TLV 3 */
    struct TLV t3412_extended_value_gprs_timer_3;                              /* T3412 extended value GPRS timer 3 (5E) 9.9.3.16B O TLV 3 */
    struct TLV extended_drx_parameters;                                        /* Extended DRX parameters (6E) 9.9.3.46 O TLV 3 */

} __packed;

/* 3GPP 24.301 8.2.5 */
struct authentication_failure {
    struct emm_message_type_header header;
    unsigned int emm_cause : 8;                  /* EMM cause 9.9.3.9 M V 1 */
    struct TLV authentication_failure_parameter; /* Authentication failure parameter (30) 9.9.3.1 O TLV 16 */
} __packed;

/* 3GPP 24.301 8.2.6 */
struct authentication_reject {
    struct emm_message_type_header header;
} __packed;

/* 3GPP 24.301 8.2.7 */
struct emm_authentication_request {
    struct emm_message_type_header header;
    unsigned int spare_half_octet : 4;       /* Spare half octet 9.9.2.9 M V 1/2 */
    char authentication_parameter_rand[16];  /* Authentication parameter RAND 9.9.3.3 M V 16 */
    struct LV authentication_parameter_autn; /* Authentication parameter AUTN 9.9.3.2 M LV 17 */
} __packed;

/* 3GPP 24.301 8.2.8 */
struct emm_authentication_reponse {
    struct emm_message_type_header header;
    struct LV authentication_response_parameter; /* Authentication response parameter 9.9.3.4 M LV 5-17 */
} __packed;

/* 3GPP 24.301 8.2.9 */
struct cs_service_notification {
    struct emm_message_type_header header;
    unsigned int paging_identity : 8; /* Paging identity 9.9.3.25A M V 1 */
    struct TLV cli;                   /* CLI (60) 9.9.3.38 O TLV 3-14 */
    struct TV ss_code;                /* SS Code (61) 9.9.3.39 O TV 2 */
    struct TV lcs_indicator;          /* LCS indicator (62) 9.9.3.40 O TV 2 */
    struct TLV lcs_client_identity;   /* LCS client identity (63) 9.9.3.41 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.2.10 */
struct emm_detach_accept {
    struct emm_message_type_header header;
} __packed;

/* 3GPP 24.301 8.2.11 */
struct emm_detach_request {
    struct emm_message_type_header header;
    unsigned int detach_type : 4;            /* Detach type 9.9.3.7 M V 1/2 */
    unsigned int nas_key_set_identifier : 4; /* NAS key set identifier 9.9.3.21 M V 1/2 */
    struct LV eps_mobile_identity;           /* EPS mobile identity 9.9.3.12 M LV 5-12 */
} __packed;

/* 3GPP 24.301 8.2.13 */
struct emm_information {
    struct emm_message_type_header header;
    struct TLV full_name_for_network_network_name;                   /* Full name for network Network name (43) 9.9.3.24 O TLV 3-n */
    struct TLV short_name_for_network_network_name;                  /* Short name for network Network name (45) 9.9.3.24 O TLV 3-n */
    struct TV local_time_zone_time_zone;                             /* Local time zone Time zone (46) 9.9.3.29 O TV 2 */
    struct TV universal_time_and_local_time_zone_time_zone_and_time; /* Universal time and local time zone Time zone and time (47) 9.9.3.30 O TV 8 */
    struct TLV network_daylight_saving_time_daylight_saving_time;    /* Network daylight saving time Daylight saving time (49) 9.9.3.6 O TLV 3 */
} __packed;

/* 3GPP 24.301 8.2.14 */
struct emm_status {
    struct emm_message_type_header header;
    unsigned int emm_cause : 8; /* EMM cause 9.9.3.9 M V 1 */
} __packed;

/* 3GPP 24.301 8.2.15 */
struct emm_extended_service_request {
    struct emm_message_type_header header;
    unsigned int service_type : 4;           /* Service type 9.9.3.27 M V 1/2 */
    unsigned int nas_key_set_identifier : 4; /* NAS key set identifier 9.9.3.21 M V 1/2 */
    struct LV m_tmsi_mobile_identity;        /* M-TMSI Mobile identity 9.9.2.3 M LV 6 */
    struct TLV eps_bearer_context_status;    /* EPS bearer context status (57) 9.9.2.1 O TLV 4 */
    struct TV device_properties;             /* Device properties (D-) 9.9.2.0A O TV 1 */
} __packed;

/* 3GPP 24.301 8.2.16 */
struct emm_guti_reallocation_command {
    struct emm_message_type_header header;
    struct LV guti_eps_mobile_identity;              /* GUTI EPS mobile identity 9.9.3.12 M LV 12 */
    struct TLV tai_list_tracking_area_identity_list; /* TAI list Tracking area identity list (54) 9.9.3.33 O TLV 8-98 */
} __packed;

/* 3GPP 24.301 8.2.17 */
struct guti_reallocation_complete {
    struct emm_message_type_header header;
} __packed;

/* 3GPP 24.301 8.2.18 */
struct emm_identity_request {
    struct emm_message_type_header header;
    unsigned int identity_type : 4;    /* Identity type 9.9.3.17 M V 1/2 */
    unsigned int spare_half_octet : 4; /* Spare half octet 9.9.2.9 M V 1/2 */
} __packed;

/* 3GPP 24.301 8.2.19 */
struct emm_identity_response {
    struct emm_message_type_header header;
    struct LV mobile_identity; /* Mobile identity 9.9.2.3 M LV 4-10 */
} __packed;

/* 3GPP 24.301 8.2.20 */
struct emm_security_mode_command {
    struct emm_message_type_header header;
    unsigned int
        selected_nas_security_algorithms_nas_security_algorithms : 8; /* Selected NAS security algorithms NAS security algorithms 9.9.3.23 M V 1 */
    unsigned int nas_key_set_identifier : 4;                          /* NAS key set identifier 9.9.3.21 M V 1/2 */
    unsigned int spare_half_octet : 4;                                /* Spare half octet 9.9.2.9 M V 1/2 */
    struct LV
        replayed_ue_security_capabilities_ue_security_capability; /* Replayed UE security capabilities UE security capability 9.9.3.36 M LV 3-6 */
    struct TV imeisv_request;                                     /* IMEISV request (C-) 9.9.3.18 O TV 1 */
    struct TV replayed_nonceue_nonce;                             /* Replayed nonceUE Nonce (55) 9.9.3.25 O TV 5 */
    struct TV noncemme_nonce;                                     /* NonceMME Nonce (56) 9.9.3.25 O TV 5 */
} __packed;

/* 3GPP 24.301 8.2.21 */
struct emm_security_mode_complete {
    struct emm_message_type_header header;
    struct TLV imeisv_mobile_identity; /* IMEISV Mobile identity 9.9.2.3 O TLV 11 */
} __packed;

/* 3GPP 24.301 8.2.22 */
struct emm_security_mode_reject {
    struct emm_message_type_header header;
    unsigned int emm_cause : 8; /* EMM cause 9.9.3.9 M V 1 */
} __packed;

/* 3GPP 24.301 8.2.23 */
struct emm_security_protected_nas_message {
    struct emm_message_type_header header;
    unsigned int sequence_number : 8; /* Sequence number 9.6 M V 1 */
    char nas_message[0];              /* NAS message 9.7 M V 1-n */
} __packed;

/* 3GPP 24.301 8.2.24 */
struct emm_service_reject {
    struct emm_message_type_header header;
    unsigned int emm_cause : 8; /* EMM cause 9.9.3.9 M V 1 */
    struct TV t3442_value;      /* 5B GPRS timer 9.9.3.16 C TV 2 */
    struct TLV t3346_value;     /* 5F GPRS timer 2 9.9.3.16A O TLV 3 */
} __packed;

/* 3GPP 24.301 8.2.25 */
struct emm_service_request {
    struct emm_protocol_header header;
    unsigned int ksi : 4;
    unsigned int sequence_number : 8;
    unsigned int message_authentication_code : 16;
} __packed;

/* 3GPP 24.301 8.3.1 */
struct esm_activate_dedicated_eps_bearer_context_accept {
    struct esm_message_type_header header;
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV nbifom_container;               /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.2 */
struct esm_activate_dedicated_eps_bearer_context_reject {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8;                /* ESM cause 9.9.4.4 M V 1 */
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV nbifom_container;               /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.3 */
struct esm_activate_dedicated_eps_bearer_context_request {
    struct esm_message_type_header header;
    unsigned int linked_eps_bearer_identity : 4;  /* Linked EPS bearer identity 9.9.4.6 M V 1/2 */
    unsigned int spare_half_octet : 4;            /* Spare half octet 9.9.2.9 M V 1/2 */
    struct LV eps_qos_eps_quality_of_service;     /* EPS QoS EPS quality of service 9.9.4.3 M LV 2-14 */
    struct LV tft_traffic_flow_template;          /* TFT Traffic flow template 9.9.4.16 M LV 2-256 */
    struct TLV transaction_identifier;            /* Transaction identifier (5D) 9.9.4.17 O TLV 3-4 */
    struct TLV negotiated_qos_quality_of_service; /* Negotiated QoS Quality of service (30) 9.9.4.12 O TLV 14-22 */
    struct TV
        negotiated_llc_sapi_llc_service_access_point_identifier;  /* Negotiated LLC SAPI LLC service access point identifier (32) 9.9.4.7 O TV 2 */
    struct TV radio_priority;                                     /* Radio priority (8-) 9.9.4.13 O TV 1 */
    struct TLV packet_flow_identifier;                            /* Packet flow Identifier (34) 9.9.4.8 O TLV 3 */
    struct TLV protocol_configuration_options;                    /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TV wlan_offload_indication_wlan_offload_acceptability; /* WLAN offload indication WLAN offload acceptability (C-) 9.9.4.18 O TV 1 */
    struct TLV nbifom_container;                                  /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.4 */
struct esm_activate_default_eps_bearer_context_accept {
    struct esm_message_type_header header;
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
} __packed;

/* 3GPP 24.301 8.3.5 */
struct esm_activate_default_eps_bearer_context_reject {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8;                /* ESM cause 9.9.4.4 M V 1 */
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
} __packed;

/* 3GPP 24.301 8.3.6 */
struct esm_activate_default_eps_bearer_context_request {
    struct esm_message_type_header header;
    struct LV eps_qos_eps_quality_of_service;     /* EPS QoS EPS quality of service 9.9.4.3 M LV 2-14 */
    struct LV access_point_name;                  /* Access point name 9.9.4.1 M LV 2-101 */
    struct LV pdn_address;                        /* PDN address 9.9.4.9 M LV 6-14 */
    struct TLV transaction_identifier;            /* Transaction identifier (5D) 9.9.4.17 O TLV 3-4 */
    struct TLV negotiated_qos_quality_of_service; /* Negotiated QoS Quality of service (30) 9.9.4.12 O TLV 14-22 */
    struct TV
        negotiated_llc_sapi_llc_service_access_point_identifier;  /* Negotiated LLC SAPI LLC service access point identifier (32) 9.9.4.7 O TV 2 */
    struct TV radio_priority;                                     /* Radio priority (8-) 9.9.4.13 O TV 1 */
    struct TLV packet_flow_identifier;                            /* Packet flow Identifier (34) 9.9.4.8 O TLV 3 */
    struct TLV apn_ambr_apn_aggregate_maximum_bit_rate;           /* APN-AMBR APN aggregate maximum bit rate (5E) 9.9.4.2 O TLV 4-8 */
    struct TV esm_cause;                                          /* ESM cause (58) 9.9.4.4 O TV 2 */
    struct TLV protocol_configuration_options;                    /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TV connectivity_type;                                  /* Connectivity type (B-) 9.9.4.2A O TV 1 */
    struct TV wlan_offload_indication_wlan_offload_acceptability; /* WLAN offload indication WLAN offload acceptability (C-) 9.9.4.18 O TV 1 */
    struct TLV nbifom_container;                                  /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
    struct TLV header_compression_configuration;                  /* Header compression configuration 9.9.4.22 O TLV 3-n */
    struct T control_plane_only_indication;                       /* Control plane only indication (91) 9.9.4.23 O T 1 */
} __packed;

/* 3GPP 24.301 8.3.7 */
struct esm_bearer_resource_allocation_reject {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8;                           /* ESM cause 9.9.4.4 M V 1 */
    struct TLV protocol_configuration_options;            /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV back_off_timer_value_gprs_timer_3;         /* Back-off timer value GPRS timer 3 (37) 9.9.3.16B O TLV 3 */
    struct TLV re_attempt_indicator_re_attempt_indicator; /* Re-attempt indicator Re-attempt indicator (6B) 9.9.4.13A O TLV 3 */
    struct TLV nbifom_container;                          /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.8 */
struct esm_bearer_resource_allocation_request {
    struct esm_message_type_header header;
    unsigned int linked_eps_bearer_identity : 4; /* Linked EPS bearer identity 9.9.4.6 M V 1/2 */
    unsigned int spare_half_octet : 4;           /* Spare half octet 9.9.2.9 M V 1/2 */
    struct LV
        traffic_flow_aggregate_traffic_flow_aggregate_description; /* Traffic flow aggregate Traffic flow aggregate description 9.9.4.15 M LV 2-256 */
    struct LV required_traffic_flow_qos_eps_quality_of_service;    /* Required traffic flow QoS EPS quality of service 9.9.4.3 M LV 2-14 */
    struct TLV protocol_configuration_options;                     /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TV device_properties;                                   /* Device properties (C-) 9.9.2.0A O TV 1 */
    struct TLV nbifom_container;                                   /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.9 */
struct esm_bearer_resource_modification_reject {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8;                           /* ESM cause 9.9.4.4 M V 1 */
    struct TLV protocol_configuration_options;            /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV back_off_timer_value_gprs_timer_3;         /* Back-off timer value GPRS timer 3 (37) 9.9.3.16B O TLV 3 */
    struct TLV re_attempt_indicator_re_attempt_indicator; /* Re-attempt indicator Re-attempt indicator (6B) 9.9.4.13A O TLV 3 */
    struct TLV nbifom_container;                          /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.10 */
struct esm_bearer_resource_modification_request {
    struct esm_message_type_header header;
    unsigned int linked_eps_bearer_identity : 4; /* Linked EPS bearer identity 9.9.4.6 M V 1/2 */
    unsigned int spare_half_octet : 4;           /* Spare half octet 9.9.2.9 M V 1/2 */
    struct LV
        traffic_flow_aggregate_traffic_flow_aggregate_description; /* Traffic flow aggregate Traffic flow aggregate description 9.9.4.15 M LV 2-256 */
    struct TLV required_traffic_flow_qos_eps_quality_of_service;   /* Required traffic flow QoS EPS quality of service (5B) 9.9.4.3 O TLV 3-15 */
    struct TV esm_cause;                                           /* ESM cause (58) 9.9.4.4 O TV 2 */
    struct TLV protocol_configuration_options;                     /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TV device_properties;                                   /* Device properties (C-) 9.9.2.0A O TV 1 */
    struct TLV nbifom_container;                                   /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.11 */
struct esm_deactivate_eps_bearer_context_accept {
    struct esm_message_type_header header;
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV nbifom_container;               /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.12 */
struct esm_deactivate_eps_bearer_context_request {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8;                                   /* ESM cause 9.9.4.4 M V 1 */
    struct TLV protocol_configuration_options;                    /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV t3396_value_gprs_timer_3;                          /* T3396 value GPRS timer 3 (37) 9.9.3.16B O TLV 3 */
    struct TV wlan_offload_indication_wlan_offload_acceptability; /* WLAN offload indication WLAN offload acceptability (C-) 9.9.4.18 O TV 1 */
    struct TLV nbifom_container;                                  /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.13 */
struct esm_information_request {
    struct esm_message_type_header header;
} __packed;

/* 3GPP 24.301 8.3.14 */
struct esm_information_response {
    struct esm_message_type_header header;
    struct TLV access_point_name;              /* Access point name (28) 9.9.4.1 O TLV 3-102 */
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
} __packed;

/* 3GPP 24.301 8.3.15 */
struct esm_status {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8; /* ESM cause 9.9.4.4 M V 1 */
} __packed;

/* 3GPP 24.301 8.3.16 */
struct esm_modify_eps_bearer_context_accept {
    struct esm_message_type_header header;
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV nbifom_container;               /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.17 */
struct esm_modify_eps_bearer_context_reject {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8;                /* ESM cause 9.9.4.4 M V 1 */
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV nbifom_container;               /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.18 */
struct esm_modify_eps_bearer_context_request {
    struct esm_message_type_header header;
    struct TLV new_eps_qos_eps_quality_of_service; /* New EPS QoS EPS quality of service (5B) 9.9.4.3 O TLV 3-15 */
    struct TLV tft_traffic_flow_template;          /* TFT Traffic flow template (36) 9.9.4.16 O TLV 3-257 */
    struct TLV new_qos_quality_of_service;         /* New QoS Quality of service (30) 9.9.4.12 O TLV 14-22 */
    struct TV
        negotiated_llc_sapi_llc_service_access_point_identifier;  /* Negotiated LLC SAPI LLC service access point identifier (32) 9.9.4.7 O TV 2 */
    struct TV radio_priority;                                     /* Radio priority (8-) 9.9.4.13 O TV 1 */
    struct TLV packet_flow_identifier;                            /* Packet flow Identifier (34) 9.9.4.8 O TLV 3 */
    struct TLV apn_ambr_apn_aggregate_maximum_bit_rate;           /* APN-AMBR APN aggregate maximum bit rate (5E) 9.9.4.2 O TLV 4-8 */
    struct TLV protocol_configuration_options;                    /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TV wlan_offload_indication_wlan_offload_acceptability; /* WLAN offload indication WLAN offload acceptability (C-) 9.9.4.18 O TV 1 */
    struct TLV nbifom_container;                                  /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
} __packed;

/* 3GPP 24.301 8.3.18A */
struct esm_notification {
    struct esm_message_type_header header;
    struct LV notification_indicator; /* Notification indicator 9.9.4.7A M LV 2 */
} __packed;

/* 3GPP 24.301 8.3.19 */
struct esm_pdn_connectivity_reject {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8;                           /* ESM cause 9.9.4.4 M V 1 */
    struct TLV protocol_configuration_options;            /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TLV back_off_timer_value_gprs_timer_3;         /* Back-off timer value GPRS timer 3 (37) 9.9.3.16B O TLV 3 */
    struct TLV re_attempt_indicator_re_attempt_indicator; /* Re-attempt indicator Re-attempt indicator (6B) 9.9.4.13A O TLV 3 */
} __packed;

/* 3GPP 24.301 8.3.20 */
struct esm_pdn_connectivity_request {
    struct esm_message_type_header header;
    unsigned int request_type : 4;                                                   /* Request type 9.9.4.14 M V 1/2 */
    unsigned int pdn_type : 4;                                                       /* PDN type 9.9.4.10 M V 1/2 */
    struct TV esm_information_transfer_flag;                                         /* ESM information transfer flag (D-) 9.9.4.5 O TV 1 */
    struct TLV access_point_name;                                                    /* Access point name (28) 9.9.4.1 O TLV 3-102 */
    struct TLV protocol_configuration_options;                                       /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
    struct TV device_properties;                                                     /* Device properties (C-) 9.9.2.0A O TV 1 */
    struct TLV nbifom_container;                                                     /* NBIFOM container (33) 9.9.4.19 O TLV 3-257 */
    struct TLV yz_header_compression_configuration_header_compression_configuration; /* YZ Header compression configuration Header compression
                                                                                        configuration 9.9.4.22 O TLV 3-TBD */
} __packed;

/* 3GPP 24.301 8.3.21 */
struct esm_pdn_disconnect_reject {
    struct esm_message_type_header header;
    unsigned int esm_cause : 8;                /* ESM cause 9.9.4.4 M V 1 */
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
} __packed;

/* 3GPP 24.301 8.3.22 */
struct esm_pdn_disconnect_request {
    struct esm_message_type_header header;
    int linked_eps_bearer_identity : 4;        /* Linked EPS bearer identity 9.9.4.6 M V 1/2 */
    int spare_half_octet : 4;                  /* Spare half octet 9.9.2.9 M V 1/2 */
    struct TLV protocol_configuration_options; /* Protocol configuration options (27) 9.9.4.11 O TLV 3-253 */
} __packed;

/*
        ### IE structures ###
*/

/* 3GPP 24.008 10.5.6.1 */
struct ie_apn_t {
    unsigned int len : 8;
    char apn[0];
} __packed;

/*
        ### Netcomm specific defines and structures ###
*/

#define LTE_NAS_PARSER_ESM_HEADER_TYPE_UNKNOWN 0
#define LTE_NAS_PARSER_ESM_HEADER_TYPE_COMMON 1

#define LTE_NAS_PARSER_EMM_HEADER_TYPE_UNKNOWN 0
#define LTE_NAS_PARSER_EMM_HEADER_TYPE_COMMON 1
#define LTE_NAS_PARSER_EMM_HEADER_TYPE_SERVICE_REQUEST 2

/*
        ### local defines based on 3GPP ###
*/

/*
        According to 3GPP TS 24.008 10.5.6.1, maximum length of APN is 102 as below

        "The Access point name is a type 4 information element with a minimum length of 3 octets and a maximum length of 102 octets."

        We use 256 to cover any future increasement as Qualcomm is already using bigger size - 150+1(NULL)
*/

/* buffer size for APN */
#define MAX_APN_BUFFER_SIZE 256

/* total number of 3GPP EMM or ESM failure cause */
#define MAX_EMM_FAILURE_CAUSE 256

/* NAS message type */
enum
{
    nas_message_type_emm = 0,
    nas_message_type_esm,
    nas_message_type_max,
};

/* NAS message direction */
enum
{
    nas_message_dir_in = 0,
    nas_message_dir_out,
};

/* LTE NAS parser callback */
typedef int (*lte_nas_parser_callback)(const unsigned long long *ms, const char *logpx, int msg_type, const char *msg_type_str, int ebi, int pti,
                                       void *data, int len);

/* lte_nas_parser.c */
const char *lte_nas_parser_get_nas_msg_type_name_str(int nas_msg_type);
const char *lte_nas_parser_get_emm_cause_name_str(int emm_cause);
const char *lte_nas_parser_get_esm_cause_name_str(int esm_cause);
int lte_nas_parser_get_esm_header_type(const char *buf, int len, int *hdr_type, int *msg_type, const char **msg_type_str);
int lte_nas_parser_pdu_get_ebi_from_disconn_req(void *buf, int len, int *ebi);
int lte_nas_parser_pdu_get_linked_ebi_from_act_ded_req(void *buf, int len, int *linked_ebi);
int lte_nas_parser_pdu_get_apn_from_tlv(void *buf, int len, char *apn_buf, int apn_buf_len, const struct TLV *tlv);
int lte_nas_parser_pdu_get_esm_cause(void *buf, int len, int *cause);
int lte_nas_parser_pdu_get_cause_from_att_rej(void *buf, int len, int *cause);
int lte_nas_parser_pdu_get_cause_from_serv_rej(void *buf, int len, int *cause);
int lte_nas_parser_pdu_get_apn_from_conn_req(void *buf, int len, char *apn_buf, int apn_buf_len);
int lte_nas_parser_pdu_get_apn_from_info_resp(void *buf, int len, char *apn_buf, int apn_buf_len);
int lte_nas_parser_pdu_get_apn_from_bearer_req(void *buf, int len, char *apn_buf, int apn_buf_len);
int lte_nas_parser_get_emm_header_type(void *buf, int len, int *hdr_type, int *msg_type, const char **msg_type_str);
int lte_nas_parser_get_embedded_esm_from_emm(void *data, int len, char **esm_data, int *esm_len);
int lte_nas_parser_perform_nas_ota_message(unsigned long long *ms, int msg_class, int msg_dir, void *data, int len);
void lte_nas_parser_set_callback(lte_nas_parser_callback cb);
int lte_nas_parser_init(void);
void lte_nas_parser_fini(void);
#endif
