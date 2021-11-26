/*!
 * C source file of LTE NAS protocol parser
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

#include "lte_nas_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "def.h"

/*
        ### parser help define macro ###
*/

/* get next TV pointer after TV */
#define TV_GET_NEXT_PTR(lv) (typeof(lv))((char *)((lv) + 1))
/* get next TLV pointer after TLV */
#define TLV_GET_NEXT_PTR(lv) (typeof(lv))(((char *)((lv) + 1)) + (lv)->l)
/* get length of TVL */
#define TLV_GET_LEN(lv) ((lv)->l_lo | (lv)->l_hi << 8)
/* convert VL of raw string into NULL-terminated string */
#define TO_STR(str, sz, v, l) \
    { \
        int m = __min((sz)-1, (l)); \
        memcpy(str, v, m); \
        str[m] = 0; \
    } \
    while (0)

#define GOTO_ERROR_IF_LENGTH_IS_SMALL(len, min) \
    { \
        if ((len) < (min)) { \
            ERR("too small PDU length (len=%d,min_len=%d)", len, min); \
            goto err; \
        } \
    } \
    while (0)

/* LTE NAS parser main class */
struct lte_nas_parser {
    lte_nas_parser_callback cb;
};

/* singleton object of LTE NAS parser */
struct lte_nas_parser _np_singleton;
struct lte_nas_parser *_np = &_np_singleton;

/* EMM and ESM message type strings */
const char *nas_msg_type_names[] = {
    [EMM_EPS_MOBILITY_MANAGEMENT_MESSAGES] = "EPS mobility management messages",
    [EMM_ATTACH_REQUEST] = "Attach request",
    [EMM_ATTACH_ACCEPT] = "Attach accept",
    [EMM_ATTACH_COMPLETE] = "Attach complete",
    [EMM_ATTACH_REJECT] = "Attach reject",
    [EMM_DETACH_REQUEST] = "Detach request",
    [EMM_DETACH_ACCEPT] = "Detach accept",
    [EMM_TRACKING_AREA_UPDATE_REQUEST] = "Tracking area update request",
    [EMM_TRACKING_AREA_UPDATE_ACCEPT] = "Tracking area update accept",
    [EMM_TRACKING_AREA_UPDATE_COMPLETE] = "Tracking area update complete",
    [EMM_TRACKING_AREA_UPDATE_REJECT] = "Tracking area update reject",
    [EMM_EXTENDED_SERVICE_REQUEST] = "Extended service request",
    [EMM_SERVICE_REJECT] = "Service reject",
    [EMM_GUTI_REALLOCATION_COMMAND] = "GUTI reallocation command",
    [EMM_GUTI_REALLOCATION_COMPLETE] = "GUTI reallocation complete",
    [EMM_AUTHENTICATION_REQUEST] = "Authentication request",
    [EMM_AUTHENTICATION_RESPONSE] = "Authentication response",
    [EMM_AUTHENTICATION_REJECT] = "Authentication reject",
    [EMM_AUTHENTICATION_FAILURE] = "Authentication failure",
    [EMM_IDENTITY_REQUEST] = "Identity request",
    [EMM_IDENTITY_RESPONSE] = "Identity response",
    [EMM_SECURITY_MODE_COMMAND] = "Security mode command",
    [EMM_SECURITY_MODE_COMPLETE] = "Security mode complete",
    [EMM_SECURITY_MODE_REJECT] = "Security mode reject",
    [EMM_STATUS] = "EMM status",
    [EMM_INFORMATION] = "EMM information",
    [EMM_DOWNLINK_NAS_TRANSPORT] = "Downlink NAS transport",
    [EMM_UPLINK_NAS_TRANSPORT] = "Uplink NAS transport",
    [EMM_CS_SERVICE_NOTIFICATION] = "CS Service notification",
    [EMM_DOWNLINK_GENERIC_NAS_TRANSPORT] = "Downlink generic NAS transport",
    [EMM_UPLINK_GENERIC_NAS_TRANSPORT] = "Uplink generic NAS transport",
    [ESM_EPS_SESSION_MANAGEMENT_MESSAGES] = "EPS session management messages",
    [ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST] = "Activate default EPS bearer context request",
    [ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT] = "Activate default EPS bearer context accept",
    [ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REJECT] = "Activate default EPS bearer context reject",
    [ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST] = "Activate dedicated EPS bearer context request",
    [ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_ACCEPT] = "Activate dedicated EPS bearer context accept",
    [ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REJECT] = "Activate dedicated EPS bearer context reject",
    [ESM_MODIFY_EPS_BEARER_CONTEXT_REQUEST] = "Modify EPS bearer context request",
    [ESM_MODIFY_EPS_BEARER_CONTEXT_ACCEPT] = "Modify EPS bearer context accept",
    [ESM_MODIFY_EPS_BEARER_CONTEXT_REJECT] = "Modify EPS bearer context reject",
    [ESM_DEACTIVATE_EPS_BEARER_CONTEXT_REQUEST] = "Deactivate EPS bearer context request",
    [ESM_DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT] = "Deactivate EPS bearer context accept",
    [ESM_PDN_CONNECTIVITY_REQUEST] = "PDN connectivity request",
    [ESM_PDN_CONNECTIVITY_REJECT] = "PDN connectivity reject",
    [ESM_PDN_DISCONNECT_REQUEST] = "PDN disconnect request",
    [ESM_PDN_DISCONNECT_REJECT] = "PDN disconnect reject",
    [ESM_BEARER_RESOURCE_ALLOCATION_REQUEST] = "Bearer resource allocation request",
    [ESM_BEARER_RESOURCE_ALLOCATION_REJECT] = "Bearer resource allocation reject",
    [ESM_BEARER_RESOURCE_MODIFICATION_REQUEST] = "Bearer resource modification request",
    [ESM_BEARER_RESOURCE_MODIFICATION_REJECT] = "Bearer resource modification reject",
    [ESM_INFORMATION_REQUEST] = "ESM information request",
    [ESM_INFORMATION_RESPONSE] = "ESM information response",
    [ESM_NOTIFICATION] = "Notification",
    [ESM_ESM_STATUS] = "ESM status",
    [ESM_REMOTE_UE_REPORT] = "Remote UE report",
    [ESM_REMOTE_UE_REPORT_RESPONSE] = "Remote UE report response",
};

/* EMM failure case name strings */
const char *emm_cause_names[] = {
    [EMM_CAUSE_IMSI_UNKNOWN_IN_HSS] = "IMSI unknown in HSS",
    [EMM_CAUSE_ILLEGAL_UE] = "Illegal UE",
    [EMM_CAUSE_IMEI_NOT_ACCEPTED] = "IMEI not accepted",
    [EMM_CAUSE_ILLEGAL_ME] = "Illegal ME",
    [EMM_CAUSE_EPS_SERVICES_NOT_ALLOWED] = "EPS services not allowed",
    [EMM_CAUSE_EPS_SERVICES_AND_NON_EPS_SERVICES_NOT_ALLOWED] = "EPS services and non-EPS services not allowed",
    [EMM_CAUSE_UE_IDENTITY_CANNOT_BE_DERIVED_BY_THE_NETWORK] = "UE identity cannot be derived by the network",
    [EMM_CAUSE_IMPLICITLY_DETACHED] = "Implicitly detached",
    [EMM_CAUSE_PLMN_NOT_ALLOWED] = "PLMN not allowed",
    [EMM_CAUSE_TRACKING_AREA_NOT_ALLOWED] = "Tracking Area not allowed",
    [EMM_CAUSE_ROAMING_NOT_ALLOWED_IN_THIS_TRACKING_AREA] = "Roaming not allowed in this tracking area",
    [EMM_CAUSE_EPS_SERVICES_NOT_ALLOWED_IN_THIS_PLMN] = "EPS services not allowed in this PLMN",
    [EMM_CAUSE_NO_SUITABLE_CELLS_IN_TRACKING_AREA] = "No Suitable Cells In tracking area",
    [EMM_CAUSE_MSC_TEMPORARILY_NOT_REACHABLE] = "MSC temporarily not reachable",
    [EMM_CAUSE_NETWORK_FAILURE] = "Network failure",
    [EMM_CAUSE_CS_DOMAIN_NOT_AVAILABLE] = "CS domain not available",
    [EMM_CAUSE_ESM_FAILURE] = "ESM failure",
    [EMM_CAUSE_MAC_FAILURE] = "MAC failure",
    [EMM_CAUSE_SYNCH_FAILURE] = "Synch failure",
    [EMM_CAUSE_CONGESTION] = "Congestion",
    [EMM_CAUSE_UE_SECURITY_CAPABILITIES_MISMATCH] = "UE security capabilities mismatch",
    [EMM_CAUSE_SECURITY_MODE_REJECTED_UNSPECIFIED] = "Security mode rejected, unspecified",
    [EMM_CAUSE_NOT_AUTHORIZED_FOR_THIS_CSG] = "Not authorized for this CSG",
    [EMM_CAUSE_NON_EPS_AUTHENTICATION_UNACCEPTABLE] = "Non-EPS authentication unacceptable",
    [EMM_CAUSE_REQUESTED_SERVICE_OPTION_NOT_AUTHORIZED_IN_THIS_PLMN] = "Requested service option not authorized in this PLMN",
    [EMM_CAUSE_CS_SERVICE_TEMPORARILY_NOT_AVAILABLE] = "CS service temporarily not available",
    [EMM_CAUSE_NO_EPS_BEARER_CONTEXT_ACTIVATED] = "No EPS bearer context activated",
    [EMM_CAUSE_SEVERE_NETWORK_FAILURE] = "Severe network failure",
    [EMM_CAUSE_SEMANTICALLY_INCORRECT_MESSAGE] = "Semantically incorrect message",
    [EMM_CAUSE_INVALID_MANDATORY_INFORMATION] = "Invalid mandatory information",
    [EMM_CAUSE_MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED] = "Message type non-existent or not implemented",
    [EMM_CAUSE_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_THE_PROTOCOL_STATE] = "Message type not compatible with the protocol state",
    [EMM_CAUSE_INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED] = "Information element non-existent or not implemented",
    [EMM_CAUSE_CONDITIONAL_IE_ERROR] = "Conditional IE error",
    [EMM_CAUSE_MESSAGE_NOT_COMPATIBLE_WITH_THE_PROTOCOL_STATE] = "Message not compatible with the protocol state",
    [EMM_CAUSE_PROTOCOL_ERROR_UNSPECIFIED] = "Protocol error unspecified",
};

/* ESM failure case name strings */
const char *esm_cause_names[] = {
    [ESM_CAUSE_OPERATOR_DETERMINED_BARRING] = "Operator Determined Barring",
    [ESM_CAUSE_INSUFFICIENT_RESOURCES] = "Insufficient resources",
    [ESM_CAUSE_MISSING_OR_UNKNOWN_APN] = "Missing or unknown APN",
    [ESM_CAUSE_UNKNOWN_PDN_TYPE] = "Unknown PDN type",
    [ESM_CAUSE_USER_AUTHENTICATION_FAILED] = "User authentication failed",
    [ESM_CAUSE_REQUEST_REJECTED_BY_SERVING_GW_OR_PDN_GW] = "Request rejected by Serving GW or PDN GW",
    [ESM_CAUSE_REQUEST_REJECTED__UNSPECIFIED] = "Request rejected, unspecified",
    [ESM_CAUSE_SERVICE_OPTION_NOT_SUPPORTED] = "Service option not supported",
    [ESM_CAUSE_REQUESTED_SERVICE_OPTION_NOT_SUBSCRIBED] = "Requested service option not subscribed",
    [ESM_CAUSE_SERVICE_OPTION_TEMPORARILY_OUT_OF_ORDER] = "Service option temporarily out of order",
    [ESM_CAUSE_PTI_ALREADY_IN_USE] = "PTI already in use",
    [ESM_CAUSE_REGULAR_DEACTIVATION] = "Regular deactivation",
    [ESM_CAUSE_EPS_QOS_NOT_ACCEPTED] = "EPS QoS not accepted",
    [ESM_CAUSE_NETWORK_FAILURE] = "Network failure",
    [ESM_CAUSE_REACTIVATION_REQUESTED] = "Reactivation requested",
    [ESM_CAUSE_SEMANTIC_ERROR_IN_THE_TFT_OPERATION] = "Semantic error in the TFT operation",
    [ESM_CAUSE_SYNTACTICAL_ERROR_IN_THE_TFT_OPERATION] = "Syntactical error in the TFT operation",
    [ESM_CAUSE_INVALID_EPS_BEARER_IDENTITY] = "Invalid EPS bearer identity",
    [ESM_CAUSE_SEMANTIC_ERRORS_IN_PACKET_FILTERS] = "Semantic errors in packet filter(s)",
    [ESM_CAUSE_SYNTACTICAL_ERRORS_IN_PACKET_FILTERS] = "Syntactical errors in packet filter(s)",
    [ESM_CAUSE_UNUSED_SEE_NOTE_2] = "Unused (see NOTE 2)",
    [ESM_CAUSE_PTI_MISMATCH] = "PTI mismatch",
    [ESM_CAUSE_LAST_PDN_DISCONNECTION_NOT_ALLOWED] = "Last PDN disconnection not allowed",
    [ESM_CAUSE_PDN_TYPE_IPV4_ONLY_ALLOWED] = "PDN type IPv4 only allowed",
    [ESM_CAUSE_PDN_TYPE_IPV6_ONLY_ALLOWED] = "PDN type IPv6 only allowed",
    [ESM_CAUSE_SINGLE_ADDRESS_BEARERS_ONLY_ALLOWED] = "Single address bearers only allowed",
    [ESM_CAUSE_ESM_INFORMATION_NOT_RECEIVED] = "ESM information not received",
    [ESM_CAUSE_PDN_CONNECTION_DOES_NOT_EXIST] = "PDN connection does not exist",
    [ESM_CAUSE_MULTIPLE_PDN_CONNECTIONS_FOR_A_GIVEN_APN_NOT_ALLOWED] = "Multiple PDN connections for a given APN not allowed",
    [ESM_CAUSE_COLLISION_WITH_NETWORK_INITIATED_REQUEST] = "Collision with network initiated request",
    [ESM_CAUSE_UNSUPPORTED_QCI_VALUE] = "Unsupported QCI value",
    [ESM_CAUSE_BEARER_HANDLING_NOT_SUPPORTED] = "Bearer handling not supported",
    [ESM_CAUSE_MAXIMUM_NUMBER_OF_EPS_BEARERS_REACHED] = "Maximum number of EPS bearers reached",
    [ESM_CAUSE_REQUESTED_APN_NOT_SUPPORTED_IN_CURRENT_RAT_AND_PLMN_COMBINATION] = "Requested APN not supported in current RAT and PLMN combination",
    [ESM_CAUSE_INVALID_PTI_VALUE] = "Invalid PTI value",
    [ESM_CAUSE_SEMANTICALLY_INCORRECT_MESSAGE] = "Semantically incorrect message",
    [ESM_CAUSE_INVALID_MANDATORY_INFORMATION] = "Invalid mandatory information",
    [ESM_CAUSE_MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED] = "Message type non-existent or not implemented",
    [ESM_CAUSE_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_THE_PROTOCOL_STATE] = "Message type not compatible with the protocol state",
    [ESM_CAUSE_INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED] = "Information element non-existent or not implemented",
    [ESM_CAUSE_CONDITIONAL_IE_ERROR] = "Conditional IE error",
    [ESM_CAUSE_MESSAGE_NOT_COMPATIBLE_WITH_THE_PROTOCOL_STATE] = "Message not compatible with the protocol state",
    [ESM_CAUSE_PROTOCOL_ERROR__UNSPECIFIED] = "Protocol error, unspecified",
    [ESM_CAUSE_APN_RESTRICTION_VALUE_INCOMPATIBLE_WITH_ACTIVE_EPS_BEARER_CONTEXT] =
        "APN restriction value incompatible with active EPS bearer context",
    [ESM_CAUSE_MULTIPLE_ACCESSES_TO_A_PDN_CONNECTION_NOT_ALLOWED] = "Multiple accesses to a PDN connection not allowed",
    [ESM_CAUSE_NETCOMM_DISCONNECTION_WITHOUT_NAS_SIGNALING] = "abnormal bearer termination without NAS signaling by UE timer",
};

/*
        ### numeric-type-to-string-type conversion functions ###
*/

/* get NAS message name */
const char *lte_nas_parser_get_nas_msg_type_name_str(int nas_msg_type)
{
    const char **nas_msg_name = &nas_msg_type_names[nas_msg_type];

    return __is_in_boundary(nas_msg_name, nas_msg_type_names, sizeof(nas_msg_type_names)) ? *nas_msg_name : NULL;
}

/* get EMM cause name */
const char *lte_nas_parser_get_emm_cause_name_str(int emm_cause)
{
    const char **emm_cause_str;

    emm_cause_str = &emm_cause_names[emm_cause];
    if (!__is_in_boundary(emm_cause_str, emm_cause_names, sizeof(emm_cause_names)))
        goto err;

    return *emm_cause_str;
err:
    return NULL;
}

/* get ESM cause name */
const char *lte_nas_parser_get_esm_cause_name_str(int esm_cause)
{
    const char **esm_cause_str;

    esm_cause_str = &esm_cause_names[esm_cause];
    if (!__is_in_boundary(esm_cause_str, esm_cause_names, sizeof(esm_cause_names)))
        goto err;

    return *esm_cause_str;
err:
    return NULL;
}

#ifdef DEBUG_PACKET_DUMP
/*
 Dump hexadecimal codes into System log.

 Parameters:
  dump_name: string information for the log.
  data : data to log.
  len : length of data.
*/
static void lte_nas_parser_log_dump_hex(const char *dump_name, const void *data, int len)
{
    char line[1024];
    const char *ptr = (const char *)data;
    char *buf;

    int i;

    DEBUG("* %s (data=0x%p,len=%d)", dump_name, data, len);

    buf = line;
    for (i = 0; i < len; i++) {
        if (i && !(i % 16)) {
            DEBUG("%04x: %s", i & ~0x0f, line);
            buf = line;
        }

        buf += sprintf(buf, "%02x ", ptr[i]);
    }

    if (i)
        DEBUG("%04x: %s", i & ~0x0f, line);
}
#else
static void lte_nas_parser_log_dump_hex(const char *dump_name, const void *data, int len) {}
#endif

/*
        ### Generic (level 1) parser functions ###
*/

/* check to see if NAS message is service request */
static inline int lte_nas_parser_is_emm_serv_req(const struct emm_protocol_header *emm_proto_hdr)
{
    return emm_proto_hdr->security_header_type == SECURITY_HEADER_TYPE_SERVICE_REQUEST;
}

/*
 Parse NAS message and provide variable information such as head type, message type and etc.

 Parameters:
  buf : LTE NAS OTA message.
  len : length of LTE NAS OTA message.
  hdr_type : pointer to receive NAS header type after parsing.
  msg_type : pointer to receive NAS message type after parsing.
  msg_type_str : pointer to receive NAS message type as a string type after parsing.

 Return:
  On success, bearer is returned. Otherwise, NULL.
*/
int lte_nas_parser_get_esm_header_type(const char *buf, int len, int *hdr_type, int *msg_type, const char **msg_type_str)
{
    struct esm_message_type_header *hdr = (struct esm_message_type_header *)buf;

    /* set default result */
    *hdr_type = LTE_NAS_PARSER_ESM_HEADER_TYPE_UNKNOWN;
    *msg_type = 0;
    *msg_type_str = NULL;

    /* check minimum header length */
    if (len < sizeof(*hdr)) {
        ERR("too short EMM PDU size (len=%d)", len);
        goto err;
    }

    /* check PD */
    if (hdr->protocol_discriminator != PROTOCOL_DISCRIMINATOR_ESM) {
        ERR("incorrect ESM PD (pd=0x%02x)", hdr->protocol_discriminator);
        goto err;
    }

    /* set common emm message types */
    *hdr_type = LTE_NAS_PARSER_ESM_HEADER_TYPE_COMMON;
    *msg_type = hdr->message_type;
    *msg_type_str = lte_nas_parser_get_nas_msg_type_name_str(hdr->message_type);

    return 0;
err:
    return -1;
}

/*
        ### Specific (level 2) parser functions ###

        All of following parser functions must be used with the correct message header type.
        To avoid redundant argument sanitization. These functions are *not* with any argument verification.
*/

/* get EBI - only for disconnection request */
int lte_nas_parser_pdu_get_ebi_from_disconn_req(void *buf, int len, int *ebi)
{
    struct esm_pdn_disconnect_request *pdu = (struct esm_pdn_disconnect_request *)buf;

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, sizeof(*pdu));
    lte_nas_parser_log_dump_hex("disconn_req", buf, len);

    *ebi = pdu->linked_eps_bearer_identity;

    return 0;

err:
    return -1;
}

/* get linked EBI - only for activate dedicate request */
int lte_nas_parser_pdu_get_linked_ebi_from_act_ded_req(void *buf, int len, int *linked_ebi)
{
    struct esm_activate_dedicated_eps_bearer_context_request *pdu = (struct esm_activate_dedicated_eps_bearer_context_request *)buf;

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, __get_member_offset(pdu, transaction_identifier));
    lte_nas_parser_log_dump_hex("act_ded_req", buf, len);

    *linked_ebi = pdu->linked_eps_bearer_identity;

    return 0;

err:
    return -1;
}

/* get APN - only for ESM messages */
int lte_nas_parser_pdu_get_apn_from_tlv(void *buf, int len, char *apn_buf, int apn_buf_len, const struct TLV *tlv)
{
    struct ie_apn_t *ie_apn = NULL;
    const struct TV *tv;

    while (__is_in_boundary(tlv, buf, len)) {

        tv = (const struct TV *)tlv;

        /* assume TV */
        switch (tv->tv & 0x80) {
            case 0xd0:
            case 0xc0:
                tlv = (const struct TLV *)TV_GET_NEXT_PTR(tv);
                break;

            default:
                /* collect APN if IEI is APN */
                if (tlv->t == 0x28) {
                    ie_apn = (struct ie_apn_t *)tlv->v;
                    TO_STR(apn_buf, apn_buf_len, ie_apn->apn, ie_apn->len);
                }
                tlv = TLV_GET_NEXT_PTR(tlv);
                break;
        }
    }

    if (!ie_apn)
        goto err;

    return 0;
err:
    return -1;
}

/* get ESM cause - only for ESM messages */
int lte_nas_parser_pdu_get_esm_cause(void *buf, int len, int *cause)
{
    const struct esm_status *pdu = (struct esm_status *)buf;

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, sizeof(*pdu));

    *cause = pdu->esm_cause;

    return 0;
err:
    return -1;
}

/* get ESM cause - only for service reject */
int lte_nas_parser_pdu_get_cause_from_serv_rej(void *buf, int len, int *cause)
{
    const struct emm_service_reject *pdu = (struct emm_service_reject *)buf;

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, __get_member_offset(pdu, t3442_value));
    lte_nas_parser_log_dump_hex("serv_rej", buf, len);

    *cause = pdu->emm_cause;

    return 0;

err:
    return -1;
}

/* get ESM cause - only for attach reject */
int lte_nas_parser_pdu_get_cause_from_att_rej(void *buf, int len, int *cause)
{
    const struct emm_attach_reject *pdu = (struct emm_attach_reject *)buf;

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, sizeof(pdu->header));
    lte_nas_parser_log_dump_hex("att_rej", buf, len);

    *cause = pdu->emm_cause;

    return 0;

err:
    return -1;
}

/* get APN - only for connection request */
int lte_nas_parser_pdu_get_apn_from_conn_req(void *buf, int len, char *apn_buf, int apn_buf_len)
{
    const struct esm_pdn_connectivity_request *pdu = (struct esm_pdn_connectivity_request *)buf;

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, __get_member_offset(pdu, esm_information_transfer_flag));
    lte_nas_parser_log_dump_hex("conn_req", buf, len);

    return lte_nas_parser_pdu_get_apn_from_tlv(buf, len, apn_buf, apn_buf_len, (const struct TLV *)&pdu->esm_information_transfer_flag);

err:
    return -1;
}

/* get APN - only for information response */
int lte_nas_parser_pdu_get_apn_from_info_resp(void *buf, int len, char *apn_buf, int apn_buf_len)
{
    const struct esm_information_response *pdu = (struct esm_information_response *)buf;

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, __get_member_offset(pdu, access_point_name));
    lte_nas_parser_log_dump_hex("info_resp", buf, len);

    return lte_nas_parser_pdu_get_apn_from_tlv(buf, len, apn_buf, apn_buf_len, &pdu->access_point_name);

err:
    return -1;
}

/* get APN - only for ESM activate default EPS bearer */
int lte_nas_parser_pdu_get_apn_from_bearer_req(void *buf, int len, char *apn_buf, int apn_buf_len)
{
    const struct esm_activate_default_eps_bearer_context_request *pdu = (const struct esm_activate_default_eps_bearer_context_request *)buf;
    const struct LV *lv;

    struct ie_apn_t *ie_apn;

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, sizeof(pdu->header) + sizeof(pdu->eps_qos_eps_quality_of_service) + sizeof(pdu->access_point_name));
    lte_nas_parser_log_dump_hex("bearer_req", buf, len);

    /* APN */
    lv = TLV_GET_NEXT_PTR(&pdu->eps_qos_eps_quality_of_service);
    ie_apn = (struct ie_apn_t *)lv->v;
    TO_STR(apn_buf, apn_buf_len, ie_apn->apn, ie_apn->len);

    return 0;
err:
    return -1;
}

/* get emm header type - for all of EMM messages */
int lte_nas_parser_get_emm_header_type(void *buf, int len, int *hdr_type, int *msg_type, const char **msg_type_str)
{
    struct emm_protocol_header *phdr = (struct emm_protocol_header *)buf;
    struct emm_message_type_header *mhdr = (struct emm_message_type_header *)buf;
    const char **emm_msg_type_str = &nas_msg_type_names[mhdr->message_type];

    GOTO_ERROR_IF_LENGTH_IS_SMALL(len, sizeof(*phdr));

    /* set default result */
    *hdr_type = LTE_NAS_PARSER_EMM_HEADER_TYPE_UNKNOWN;
    *msg_type = 0;

    /* check PD */
    if (phdr->protocol_discriminator != PROTOCOL_DISCRIMINATOR_EMM) {
        ERR("incorrect EMM PD (pd=0x%02x)", phdr->protocol_discriminator);
        goto err;
    }

    /* set parser header type if service request is received */
    if (lte_nas_parser_is_emm_serv_req(phdr)) {
        *hdr_type = LTE_NAS_PARSER_EMM_HEADER_TYPE_SERVICE_REQUEST;
        goto fini;
    }

    /* set common emm message types */
    *hdr_type = LTE_NAS_PARSER_EMM_HEADER_TYPE_COMMON;
    *msg_type = mhdr->message_type;
    *msg_type_str = __is_in_boundary(emm_msg_type_str, nas_msg_type_names, sizeof(nas_msg_type_names)) ? *emm_msg_type_str : NULL;

fini:
    return 0;

err:
    return -1;
}

/* get embedded ESM body for all of EMM messages except service request */
int lte_nas_parser_get_embedded_esm_from_emm(void *data, int len, char **esm_data, int *esm_len)
{
    struct emm_message_type_header *hdr = (struct emm_message_type_header *)data;
    int msg_type = hdr->message_type;

    struct LVE *esm_message_container = NULL;

    switch (msg_type) {
        case EMM_ATTACH_REQUEST: {
            struct emm_attach_request *msg = (struct emm_attach_request *)hdr;
            struct LV *ue_network_capability = TLV_GET_NEXT_PTR(&msg->eps_mobile_identity);

            esm_message_container = (struct LVE *)TLV_GET_NEXT_PTR(ue_network_capability);
            break;
        }

        case EMM_ATTACH_ACCEPT: {
            struct emm_attach_accept *msg = (struct emm_attach_accept *)hdr;
            esm_message_container = (struct LVE *)TLV_GET_NEXT_PTR(&msg->tai_list_tracking_area_identity_list);
            break;
        }

        case EMM_ATTACH_COMPLETE: {
            struct emm_attach_complete *msg = (struct emm_attach_complete *)hdr;
            esm_message_container = &msg->esm_message_container;
            break;
        }

        default:
            DEBUG("no esm embedded in emm (msg_type=0x%04x", msg_type);
            goto err;
    }

    if (!esm_message_container)
        goto err;

    *esm_data = esm_message_container->v;
    *esm_len = TLV_GET_LEN(esm_message_container);

    return 0;

err:
    return -1;
}

/*
        * Primary default bearer

                1. BS <<< UE : EMM attach request (esm PDN connectivity request - PTI)
                   BS >>> UE : ESM information request
                   BS <<< UE : ESM information response - PTI, APN
                2. BS >>> UE : EMM attach accept (esm activate default EPS bearer request - PTI, EBI, APN)
                3. BS >>> UE : esm activate default EPS bearer - PTI, EBI
                4. BS <<< UE : EMM attach complete (esm activate default EPS bearer accept - EBI)

        * Other default bearer

                1. BS <<< UE : esm PDN connectivity request - PTI, APN
                2. BS >>> UE : esm activate default EPS bearer request - PTI, APN, EBI
                3. BS <<< UE : activate default EPS bearer accept - EBI

*/

/*
 Briefly parse LTE NAS OTA messages and call parser callback function for further and detail parsing.

 Parameters:
  ms : QxMD time-stamp.
  msg_class : message class - EMM message or ESM message.
  msg_dir : message direction - incoming or outgoing.
  data : LTE NAS OTA message.
  len : length of LTE NAS OTA message.

 Return:
  On success, bearer is returned. Otherwise, NULL.
*/
int lte_nas_parser_perform_nas_ota_message(unsigned long long *ms, int msg_class, int msg_dir, void *data, int len)
{
    int hdr_type;
    int msg_type;
    const char *msg_type_str;
    int rc;

    const static char *type_names[] = {
        [nas_message_type_emm] = "EMM",
        [nas_message_type_esm] = "esm",
    };

    const static char *dir_names[] = {
        [nas_message_dir_in] = "BS >>> MS",
        [nas_message_dir_out] = "BS <<< MS",
    };

    char logpx[256];

    /* log message class and direction */
    snprintf(logpx, sizeof(logpx), "[NAS-OTA-%s / %s]", type_names[msg_class], dir_names[msg_dir]);

    switch (msg_class) {
        case nas_message_type_emm: {
            /* parse EMM header */
            rc = lte_nas_parser_get_emm_header_type(data, len, &hdr_type, &msg_type, &msg_type_str);
            if (rc < 0) {
                DEBUG("%s failed to parse EMM header", logpx);
                goto err;
            }

            /* service request type */
            if (hdr_type == LTE_NAS_PARSER_EMM_HEADER_TYPE_SERVICE_REQUEST) {
                struct emm_service_request *sreq = (struct emm_service_request *)data;
                struct emm_protocol_header *hdr = &sreq->header;

                DEBUG("%s PDU received (pd=0x%02x,seh=0x%02x,ksi=%d,seq=%d,mac=0x%02x,*service request*)", logpx, hdr->protocol_discriminator,
                      hdr->security_header_type, sreq->ksi, sreq->sequence_number, sreq->message_authentication_code);
                lte_nas_parser_log_dump_hex("svc_req", data, len);
            } else {
                struct emm_message_type_header *hdr = (struct emm_message_type_header *)data;
                char *esm_data;
                int esm_len;

                DEBUG("%s PDU received (pd=0x%02x,seh=0x%02x,msg_type=0x%04x,msg='%s')", logpx, hdr->protocol_discriminator,
                      hdr->security_header_type, msg_type, msg_type_str);

                rc = _np->cb(ms, logpx, msg_type, msg_type_str, 0, 0, data, len);
                if (rc < 0) {
                    DEBUG("skip emm message (msg_type=0x%04x)", msg_type);
                    goto err;
                }

                /* get embedded esm message */
                if (lte_nas_parser_get_embedded_esm_from_emm(data, len, &esm_data, &esm_len) < 0) {
                    DEBUG("%s no embedded esm found", logpx);
                } else {
                    /* recursively parse any embedded esm message */
                    DEBUG("%s embedded esm message found in '%s', start to parse recursively", logpx, msg_type_str);
                    if (lte_nas_parser_perform_nas_ota_message(ms, nas_message_type_esm, msg_dir, esm_data, esm_len) < 0) {
                        ERR("%s failed in recursively parsing", logpx);
                        goto err;
                    }
                }
            }

            break;
        }

        case nas_message_type_esm: {
            struct esm_message_type_header *hdr = (struct esm_message_type_header *)data;
            int rc;

            /* parse ESM header */
            rc = lte_nas_parser_get_esm_header_type(data, len, &hdr_type, &msg_type, &msg_type_str);
            if (rc < 0) {
                DEBUG("%s failed to parse ESM header", logpx);
                goto err;
            }

            DEBUG("%s PDU received (pd=0x%02x,ebi=%d,tid=%d,msg_type=0x%02x,msg='%s')", logpx, hdr->protocol_discriminator, hdr->eps_bearer_identity,
                  hdr->procedure_transaction_identity, msg_type, msg_type_str);
            rc = _np->cb(ms, logpx, msg_type, msg_type_str, hdr->eps_bearer_identity, hdr->procedure_transaction_identity, data, len);
            if (rc < 0) {
                DEBUG("skip esm message (msg_type=0x%04x)", msg_type);
                goto err;
            }

            break;
        }
    }

    return 0;

err:
    return -1;
}

/*
 Register callback for further parsing.

 Parameters:
  cb : callback for level 2 parser.
*/
void lte_nas_parser_set_callback(lte_nas_parser_callback cb)
{
    _np->cb = cb;
}

/* initiate parser */
int lte_nas_parser_init()
{
    memset(_np, 0, sizeof(*_np));

    return 0;
}

/* finalize parser */
void lte_nas_parser_fini() {}

#ifdef CONFIG_UNIT_TEST
int main(int argc, char *argv[])
{
    return 0;
}
#endif
