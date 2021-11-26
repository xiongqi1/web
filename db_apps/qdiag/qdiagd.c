/*!
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
        ### Reference ###

        [ref-1] Qualcomm document 80-VP457-1 YM "Serial Interface Control Document for Long Term Evolution (LTE)"
        [ref-2] 13340 AT&T requirement, Compliance matrix
        [ref-3] 13289 AT&T requirement, WLL data model expanded fields v1.0 - WLL TR-098 fields
        [ref-4] Qualcomm document 80-VP457-1 YYN "Serial Interface Control Document for Long Term Evolution (LTE)"

        In the source code, variables names are following AT&T requirement and 80-VP457-1 due to better future reference

        * structures that are not matching to ICD document
        EVENT_IMS_QIPCALL_SIP_SESSION_EVENT - 0x86e
        EVENT_LTE_RRC_NEW_CELL_IND - 1611
        LOG_VOICE_CALL_STATISTICS_C - 0x17f2
*/

/* use mutex for diag messages */

/* as no local value is used, mutex is not required for now */
// #define CONFIG_USE_MUTEX
// #define CONFIG_TRAFFIC_OVERLAP_TEST
// #define CONFIG_DEBUG_THROUGHPUT
// #define CONFIG_DEBUG_PDCP
// #define CONFIG_DEBUG_RLC

/* traffic overlap test - reduce total bits to 10 bits to make overlap happens quicker */
#ifdef CONFIG_TRAFFIC_OVERLAP_TEST
#warning !!!! DEBUG VERSION - this must not be enabled in production !!!!
#define OVERLAP_TEST_BIT_COUNT 10
#define OVERLAP_TEST_BIT_MASK ((1 << OVERLAP_TEST_BIT_COUNT) - 1)
#define OVERLAP_TEST_SITUATE(v, b) (((v) << ((b)-OVERLAP_TEST_BIT_COUNT)) & ((1LLU << (b)) - 1))
#define OVERLAP_TEST_RVALUE(v, b) (((v) >> ((b)-OVERLAP_TEST_BIT_COUNT)) & OVERLAP_TEST_BIT_MASK)
#endif

/* local headers */
#include "logcodes.h"

/* qualcomm headers */
#include <diag/diag_lsm.h>
#include <diag/diag_lsm_dci.h>
#include <diag/event_defs.h>

/* standard headers */
#include <arpa/inet.h>
#include <errno.h>
#include <list.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

/* local headers */
#include "asn1c_helper.h"
#include "avg.h"
#include "def.h"
#include "eps_bearer_rdb.h"
#include "gcell.h"
#include "hash.h"
#include "lte_nas_parser.h"
#include "pdn_mgr.h"
#include "rdb.h"

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

/* qualcomm formula for dbx16 and dbx10 with variable base */
#define __dbx16(x, b) ((double)(x) / 16 + (b))
#define __dbx10(x, b) ((double)(x) / 10 + (b))

struct app_info_t;

/* defines for log functions */
#define __attrib_unused__ __attribute__((unused))

#define declare_app_func(code) static void __on_##code(struct app_info_t *app_info, unsigned char *ptr, int len)
#define declare_app_func_define_vars_for_hdr() \
    log_hdr_type *hdr = (log_hdr_type *)ptr; \
    unsigned long long __attrib_unused__ *ts = (unsigned long long *)&hdr->ts_lo;

#define declare_app_func_define_var_log(struc) \
    struc __attrib_unused__ *log = (struc *)(ptr + sizeof(*hdr)); \
    VERBOSE("define log struct (name=%s,code=0x%04x,recv=%d,struc=%d)", (app_info == NULL) ? "unknown" : app_info->name, hdr->code, \
            hdr->len - sizeof(*hdr), sizeof(struc));

#define declare_app_func_define_var_event(struc) \
    struc *event = (struc *)(ptr + sizeof(*hdr)); \
    VERBOSE("define event packet struct (name=%s,id=%d,payload_len=%d,recv=%d,struc=%d)", (app_info == NULL) ? "unknown" : app_info->name, \
            id_type->id, id_type->payload_len, hdr->len - sizeof(*hdr), sizeof(struc));

/* log structure major and minor version */
#define struct_mm_version(major, minor) (((major) << 16) | ((minor)&0xffff))
#define struct_version(ver) (ver)

#define declare_app_func_define_vars_for_log_with_version_type(version_t) \
    declare_app_func_define_vars_for_hdr(); \
    int __attrib_unused__ log_ver = (hdr->len >= sizeof(hdr->len) + sizeof(version_t)) ? *(version_t *)(hdr + 1) : 0; \
    VERBOSE("log packet recieved (name=%s,code=0x%04x,len=%d,payload_len=%d,ver=%d(%x))", (app_info == NULL) ? "unknown" : app_info->name, \
            hdr->code, hdr->len, hdr->len - sizeof(*hdr), log_ver, log_ver); \
    VERBOSE("* payload dump"); \
    log_verbose_dump((const char *)(hdr + 1), hdr->len - sizeof(hdr));

#define declare_app_func_define_vars_for_log() declare_app_func_define_vars_for_log_with_version_type(char)

#define declare_app_func_define_vars_for_event() \
    declare_app_func_define_vars_for_hdr(); \
    event_id_type __attrib_unused__ *id_type = (event_id_type *)&(hdr->code); \
    VERBOSE("event packet recieved (name=%s,id=%d,payload_len=%d,len=%d,cont_len=%d)", (app_info == NULL) ? "unknown" : app_info->name, id_type->id, \
            id_type->payload_len, hdr->len, len - sizeof(*hdr));

/*
 * The log packet version numbering is not serialized so very tricky to
 * check.
 * 1) version array should include all supporting versions lowest version first.
 * 2) If version number is lower than first version array element then
 *    return with error because the first one is lowest supporting version.
 * 3) If find any matching version from version array then parsing the
 *    found version.
 * 4) If version number is higher than last version array element then
 *    go ahead with the version number though that version may not be
 *    compatible with lower version.
 */
#define declare_app_func_require_ver(...) \
    do { \
        int versions[] = { __VA_ARGS__ }; \
        int i, found = 0; \
        static int print_msg = 1; \
        int ver_len = sizeof(versions) / sizeof(versions[0]); \
        if (log_ver < versions[0]) { \
            ERR("Current version %d is too old", log_ver); \
            return; \
        } \
        for (i = 0; i < ver_len; i++) { \
            if ((log_ver) == (versions[i])) { \
                if (print_msg) { \
                    VERBOSE("Found version %d", log_ver); \
                    print_msg = 0; \
                } \
                found = 1; \
                break; \
            } \
        } \
        if (!found && log_ver <= versions[ver_len - 1]) { \
            ERR("Version %d is not supported", log_ver); \
            return; \
        } \
        if (!found) { \
            DEBUG("version not matched (name=%s,code=0x%04x,required=v%d(0x%x),used=v%d(0x%x))", app_info->name, hdr->code, log_ver, log_ver, \
                versions[ver_len - 1], versions[ver_len - 1]); \
        } \
    } while (0)

#define declare_app_func_ver_match(req) (log_ver == (req))

/* defines for qdiag */

/* time window size in msec to track harq */
#define HARQ_TIME_WINDOW_MSEC (10 * 1000)
/* RRC rdb update frequency */
#define RRC_RDB_UPDATE_FREQ (10 * 1000)
/* servinfo rdb update frequency */
#define SERVINFO_RDB_UPDATE_FREQ (10 * 1000)
/* RRC maximum history count */
#define RRC_RDB_MAX_HISTORY 10
/* voice call maximum history count */
#define VOICECALL_RDB_MAX_HISTORY 10
/* maximum string length for voice call items */
#define VOICECALL_ITEM_MAX_LENGTH 255
/* maximum global cell element number */
#define MAX_GC_RDB 10
/* maximum voice call session information */
#define MAX_VOICECALL_SESSION_INFO 16

/* max number of server cells that data can be captured from in a measuring period */
#define MAX_SERVER_CELLS_PER_MEASURING_PERIOD 5
#define LTE_MAX_NUMBER_OF_UPLINK_CARRIER_CELLS 2

/* Qualcomm SIP diag defines from QC IMS ICD document */
#define SIP_SESSION_INCOMING_MESSAGE_EV 1
#define SIP_SESSION_OUTGOING_MESSAGE_EV 2
#define SIP_SESSION_OUTGOING_MESSAGE_OK_EV 3
#define SIP_SESSION_INCOMING_MESSAGE_OK_EV 4
#define SIP_SESSION_OUTGOING_EV 5
#define SIP_SESSION_INCOMING_EV 6
#define SIP_SESSION_MEDIA_PARAM_EV 7
#define SIP_OUTGOING_SESSION_PROGRESS_EV 8
#define SIP_SESSION_RINGING_EV 9
#define SIP_SESSION_INCOMING_CANCEL_EV 10
#define SIP_SESSION_OUTGOING_CANCEL_EV 11
#define SIP_SESSION_INCOMING_ESTABLISHED_EV 12
#define SIP_SESSION_OUTGOING_ESTABLISHED_EV 13
#define SIP_SESSION_INCOMING_TERMINATED_EV 14
#define SIP_SESSION_OUTGOING_TERMINATED_OK_EV 15
#define SIP_SESSION_INCOMING_TERMINATED_OK_EV 16
#define SIP_INCOMING_REINVITE_EV 17
#define SIP_OUTGOING_REINVITE_EV 18
#define SIP_REINVITE_ESTABLISHED_EV 19
#define SIP_ACK_REINVITE_ESTABLISHED_EV 20
#define SIP_ACK_REINVITE_FAILURE_EV 21

#define SIP_SESSION_FAILURE_EV 22
#define SIP_SESSION_REQUEST_FAILURE_EV 23
#define SIP_REINVTE_FAILURE_EV 24
#define SIP_SESSION_OUTGOING_UPDATE_EV 25
#define SIP_SESSION_INCOMING_UPDATE_EV 26
#define SIP_SESSION_INCOMING_INFO_EV 27
#define SIP_SESSION_OUTGOING_INFO_EV 28
#define SIP_SESSION_INCOMING_INFO_OK_EV 29
#define SIP_INCOMING_PRACK_EV 30
#define SIP_SESSION_INCOMING_REFER_EV 31
#define SIP_SESSION_OUTGOING_REFER_EV 32
#define SIP_SESSION_OUTGOING_REFER_OK_EV 33
#define SIP_SESSION_REFER_FINAL_EV 34
#define SIP_SESSION_INCOMING_REFER_OK_EV 35
#define SIP_PRACK_SUCCESS_EV 36
#define SIP_SESSION_OUTGOING_INFO_OK_EV 37
#define SIP_SESSION_CALL_BEING_FORWARDED_EV 38

#define CALL_TYPE_NORMAL 0
#define CALL_TYPE_HOLD 1
#define CALL_TYPE_UNHOLD 2
#define CALL_TYPE_WAITING 3
#define CALL_TYPE_CONFERENCE 4
#define CALL_TYPE_FORWARD 5
#define CALL_TYPE_TRANSFER 6

/* maximum harq count - 3gpp */
#define MAX_HARQ_ACK_COUNT 16

/* maximum harq Re-Tx count */
#define MAX_HARQ_RETX_COUNT 10

/* Select DM packet groups to monitor */
#ifdef V_RF_MEASURE_REPORT_basic
#define INCLUDE_BPLMN_SEARCH_MSG
#define INCLUDE_LTE_RRC_STATE_MSG
#elif V_RF_MEASURE_REPORT_bellca
#define DM_PACKET_INTERVAL_MS 500 /* ms */
/* selected event groups required for Bell Canada */
#define INCLUDE_LTE_RRC_STATE_MSG
#define INCLUDE_LTE_CQI_MSG
#define SAVE_AVERAGED_DATA
#define INCLUDE_COMMON_MSG
#define INCLUDE_BPLMN_SEARCH_MSG
#elif defined V_RF_MEASURE_REPORT_nbn
#define ACCUMULATE_BLER_SINCE_BOOTUP
#define SAVE_RFQ_EARFCN_LIST
#define DM_PACKET_INTERVAL_MS 500 /* ms */
/* selected event groups required for NBN */
#define INCLUDE_COMMON_MSG
#define INCLUDE_BPLMN_SEARCH_MSG
#define INCLUDE_LTE_RRC_STATE_MSG
#define INCLUDE_LTE_CQI_MSG
#define INCLUDE_LTE_TX_MCS_BLER_MSG
#define INCLUDE_LTE_RX_MCS_BLER_MSG
#define INCLUDE_LTE_EMM_ESM_MSG
#else
/* DO not limit packet rate for Titan */
#if !defined V_RF_MEASURE_REPORT_dundee
#define DM_PACKET_INTERVAL_MS 500 /* ms */
#endif

/* include everything */
#define INCLUDE_COMMON_MSG
#define INCLUDE_BPLMN_SEARCH_MSG
#define INCLUDE_LTE_RRC_STATE_MSG
#define INCLUDE_LTE_CQI_MSG
#define INCLUDE_LTE_TX_MCS_BLER_MSG
#define INCLUDE_LTE_RX_MCS_BLER_MSG
#define INCLUDE_LTE_CM_MSG
#define INCLUDE_LTE_EMM_ESM_MSG
/* No voice supported in Myna/Magpie */
#if !defined V_RF_MEASURE_REPORT_myna
#define INCLUDE_IMS_VOIP_MSG
#endif
#define SAVE_AVERAGED_DATA
#endif

#if (!defined SAVE_AVERAGED_DATA) || (defined V_RF_MEASURE_REPORT_myna)
#define RX_MOD_DISTR_NUM_TYPES 4
#else
#define RX_MOD_DISTR_NUM_TYPES 3
#endif

#define SCELL_STATE_DECONFIGURED 0
#define SCELL_STATE_CONFIGURED_DEACTIVATED 1
#define SCELL_STATE_CONFIGURED_ACTIVATED 2

#define MAX_NR5G_REPORT_COUNT       200     // by observation as of July 2020, it is ~200 +/- 50 times per second
#define MAX_NR5G_RSRP_REPORT_COUNT  64      // by observation as of July 2020, it is ~64 times per second
#define MAX_NR5G_CQI_REPORT_COUNT   32      // by observation as of July 2020, it is ~32 times per second
#define MAX_NR5G_MAC_PDSCH_REPORT_COUNT 2   // by observation as of July 2020, it is ~2 times per second

/* QC diag thread thread from QC DCI */
extern pthread_t read_thread_hdl;

/* variables for qdiag */
static int _term = 0;
static int _inst = 0;

/*
        #### standard structures ####
*/

/* Qualcomm IMS variable string structure */
struct variable_string_t {
    uint8 length;
    uint8 string[0];
} PACK();

/* QC LTE NAS standard header */
struct event_lte_msg_t {
    uint8 len;
    uint32 message_id;
} PACK();

/* disabled as RTP parser is not currently required */
#if 0
/* standard RTP header */
struct rtp_header_t {
#if __BYTE_ORDER == __BIG_ENDIAN
	    //For big endian
    unsigned char version: 2;      // Version, currently 2
    unsigned char padding: 1;      // Padding bit
    unsigned char extension: 1;    // Extension bit
    unsigned char cc: 4;           // CSRC count

    unsigned char marker: 1;       // Marker bit
    unsigned char payload: 7;      // Payload type
#else
	    //For little endian
    unsigned char cc: 4;           // CSRC count
    unsigned char extension: 1;    // Extension bit
    unsigned char padding: 1;      // Padding bit
    unsigned char version: 2;      // Version, currently 2

    unsigned char payload: 7;      // Payload type
    unsigned char marker: 1;       // Marker bit
#endif
    u_int16_t sequence;        // sequence number
    u_int32_t timestamp;       // timestamp
    u_int32_t sources[1];      // contributing sources
};
#endif

/*
        #### local structures ####
*/

/* subframe structure */
struct subfn_t {
    uint ack_valid : 1;
    uint ack : 1;
    uint tx_valid : 1;
    time_t timestamp;
};

/* structure for relationship between log (event or log message) and log handler */
struct app_info_t {
    int code;
    const char *name;
    void (*func)(struct app_info_t *app_info, unsigned char *ptr, int len);
    struct app_info_t *next;

    /* Save the time in ms when the packet was processed before in order to
     * prevent excessive DM packet which significantly drops throughput. */
    time_t last_time_ms;

    /* Set 1 for the packet which should be monitored without rate limitation such as
     * lte_ml1_gm_tx_report and lte_ml1_dlm_pdsch_stat_ind that count ack and nack
     * packet */
    uint8 nolimit;
};

/* processed app structure that contains Qualcomm app array and hashed app info */
struct app_t {
    const char *name;

    uint16 *app_code;
    int *app_code2;
    int app_code_count;

    struct app_info_t **hashed_app_info;
};

/* Qualcomm diag structure - mandatory variables for Qualcomm diag */
struct diag_stat_t {

    /* initiate status */
    int flag_init;
    int flag_reg;
    int flag_sig_reg;
    int flag_proc_reg;
    int flag_logapp;
    int flag_eventapp;

    /* diag constant */
    int channel;
    int noti_signal;
    int data_signal;

    struct app_t log_app;
    struct app_t event_app;

    int client_id;
    struct diag_dci_health_stats *dci_health_stats;
};

/*
        ### Following structures are for RG interface ###
*/

/* [RG interface] session structure for [/VoiceService/LineService] */
struct linestats_info_t {

    int direction; /* 0: incoming, 1:outgoing */
    int ring;

    char __reset_ptr;

    /* line information */
    unsigned long long PacketsSent;
    unsigned long long PacketsReceived;
    unsigned long long PacketsLost;
    unsigned long long BytesSent;
    unsigned long long BytesReceived;
    unsigned long long Overruns;
    unsigned long long Underruns;

    unsigned long long IncomingCallsReceived;
    unsigned long long IncomingCallsAnswered;
    unsigned long long IncomingCallsConnected;
    unsigned long long IncomingCallsFailed;

    unsigned long long OutgoingCallsAttempted;
    unsigned long long OutgoingCallsAnswered;
    unsigned long long OutgoingCallsConnected;
    unsigned long long OutgoingCallsFailed;

    unsigned long long CallsDropped;

    unsigned long long TotalCallTime;
    unsigned long long ServerDownTime; /* not available yet */

    char CurrCallRemoteIP[VOICECALL_ITEM_MAX_LENGTH];
    char LastCallRemoteIP[VOICECALL_ITEM_MAX_LENGTH];
};

/* [RG interface] session structure for [/VoiceService/CallHistory] */
struct voicecall_info_t {
    struct list_head list;

    int index;

    char __reset_ptr;

    int running;

    /* trackable cm call id */
    unsigned int cm_call_id;
    int cm_call_id_valid;
    /* trackable sip call id */
    char sip_call_id[VOICECALL_ITEM_MAX_LENGTH];
    int sip_call_id_valid;
    /* trackable qc call id */
    unsigned int qc_call_id;
    int qc_call_id_valid;
    /* trackable ssrc id - tx */
    unsigned int ssrc_tx_id;
    int ssrc_tx_id_valid;
    /* trackable ssrc id - rx */
    unsigned int ssrc_rx_id;
    int ssrc_rx_id_valid;

    int session_end_summary;

    struct avg_t AvgRTPLatency;

    /* session information */
    time_t StartTime;
    int StopTimeValid;
    time_t StopTime;

    time_t StartTimeMonotonic;
    time_t StopTimeMonotonic;

    int update_timestamp;

    unsigned int MaxReceiveInterarrivalJitter;
    unsigned int AvgReceiveInterarrivalJitter;
    unsigned int InboundTotalRTPPackets;
    unsigned int InboundLostRTPPackets;
    unsigned int InboundDejitterDiscardedFrames;
    unsigned int InboundDejitterDiscardedRTPPackets;
    unsigned int InboundDecoderDiscardedRTPPackets;

    unsigned int DecoderDelay; /* unavailable yet by Qualcomm */

    char LastCallNumber[VOICECALL_ITEM_MAX_LENGTH];
    char OriginatingURI[VOICECALL_ITEM_MAX_LENGTH];
    char TerminatingURI[VOICECALL_ITEM_MAX_LENGTH];
    int PayloadType;
    char CodecGSMUMTS[VOICECALL_ITEM_MAX_LENGTH];

    unsigned int OutboundTotalRTPPackets;
    unsigned int InboundCumulativeAveragePacketSize;
    unsigned int OutboundCumulativeAveragePacketSize;
    unsigned int OutboundLastRTPTime;
    unsigned int InboundLastRTPTime;

    time_t OutboundLastRTPTod; /* last outbound RTP packet ToD */
    time_t InboundLastRTPTod;  /* last inboud RTP packet ToD */

    unsigned int EncoderDelay; /* unavailable yet by Qualcomm */
    int SIPResultCode;
    char CallDirection[VOICECALL_ITEM_MAX_LENGTH];
};

/* global instant stats structure */
#define INST_MAX_ANTENNA 2
#define MCS_INDEX_NUM_ELEMENTS 32
#define CQI_NUM_ELEMENTS 16
#define PMI_NUM_ELEMENTS 16

/* [RG interface] session structure for [/ServCell/History] */
struct servcell_info_t {
    time_t StartTime;
    time_t StartTimeMonotonic;

    /* retransmission status of uplink HARQ */
    int HarqUlRetx[MAX_HARQ_ACK_COUNT];       /* previous retx index */
    int HarqUlRetx_valid[MAX_HARQ_ACK_COUNT]; /* previous retx index valid flag */
    struct subfn_t HarqUlLast[0x4000];        /* 14 bit hash table (sfn 10 bits + sbu-fn 4bits ) for nak/ack */
    /* averaged signal receptions */
    struct avg_t SCellRSSNR;
    struct avg_t SCellRSSINR;
    struct avg_t SCellRSRP;
    struct avg_t SCellRSRQ;
    struct avg_t PathLoss;
    int max_ue_tx_power;
    /* CQI average */
    struct avg_t AvgWidebandCQI;
    /* distributions */
    struct distr_t PUSCHTrasmitPower;
    struct distr_t PMIDistribution;
    struct distr_t RIDistribution;
    struct distr_t ReceivedModulationDistribution;
    struct distr_t SendModulationDistribution;
    /* RRC timestamps and latencies */
    time_t RRCEstabAtemptTimestamp;
    time_t RRCReEstabAtemptTimestamp;
    struct avg_t RRCEstabLatency;
    struct avg_t RRCReEstabLatency;
    /* HARQ transmission count */
    int max_ul_harq_transmissions; /* max HARQ re-transmission count for UL */
    int max_dl_harq_transmissions; /* max HARQ re-transmission count for DL */

/* NBN wants to accumulate BLER info since system up so
 * define BLER variables above the reset pointer.
 */
#if defined ACCUMULATE_BLER_SINCE_BOOTUP
    /* downlink HARQ */
    unsigned long long MACiBLERReceivedNackCount;
    unsigned long long MACiBLERReceivedAckNackCount;
    unsigned long long MACrBLERReceivedNackCount;
    unsigned long long MACrBLERReceivedAckNackCount;
    /* uplink HARDQ */
    unsigned long long MACiBLERSentNackCount;
    unsigned long long MACiBLERSentAckNackCount;
    unsigned long long MACrBLERSentNackCount;
    unsigned long long MACrBLERSentAckNackCount;

    /* Re-Transmission count
     * Total 10 same as V2
     * 1st - initial Transmit then got ack
     * 2~10 - Re-Transmission then got ack
     */
    unsigned long long HarqUlRetxOk[MAX_HARQ_RETX_COUNT];
#endif

    char __reset_ptr;

    /* active TTI statistics */
    unsigned long long TotalActiveTTIsReceived;
    unsigned long long TotalActiveTTIsSent;
    /* PRBs statistics */
    unsigned long long TotalPRBsReceived;
    unsigned long long TotalPRBsSent;
    /* radio link failure count */
    unsigned long long RLFCount;
    /* handover count */
    unsigned long long HandoverCount;
#ifdef V_RF_MEASURE_REPORT_myna
    /* handover fail count */
    unsigned long long HandoverAttempt;
#endif
    /* ACK state of downlink HARQ */
    int HarqDlAck[MAX_HARQ_ACK_COUNT];
    int HarqDlAck_valid[MAX_HARQ_ACK_COUNT];
    /* NDI flags */
    int HarqDlNdi[MAX_HARQ_ACK_COUNT];
    int HarqDlNdi_valid[MAX_HARQ_ACK_COUNT];

#if !defined ACCUMULATE_BLER_SINCE_BOOTUP
    /* downlink HARQ */
    unsigned long long MACiBLERReceivedNackCount;
    unsigned long long MACiBLERReceivedAckNackCount;
    unsigned long long MACrBLERReceivedNackCount;
    unsigned long long MACrBLERReceivedAckNackCount;
    /* uplink HARDQ */
    unsigned long long MACiBLERSentNackCount;
    unsigned long long MACiBLERSentAckNackCount;
    unsigned long long MACrBLERSentNackCount;
    unsigned long long MACrBLERSentAckNackCount;

    /* Re-Transmission count
     * Total 10 same as V2
     * 1st - initial Transmit then got ack
     * 2~10 - Re-Transmission then got ack
     */
    unsigned long long HarqUlRetxOk[MAX_HARQ_RETX_COUNT];
#endif

    /* RRC statistics */
    unsigned long long NumberofRRCEstabAttempts;
    unsigned long long NumberofRRCEstabFailures;
    unsigned long long NumberofRRCReEstabAttempts;
    unsigned long long NumberofRRCReEstabFailures;
    /* failure count per cause */
#define MAX_FAILURE_CAUSES 11
    unsigned long long FailureCounts[MAX_FAILURE_CAUSES];

    /* Receive/Transmit MCS (Modulation and Coding Scheme) index flags
     * MCS index is included in max 25 records * 2 TBs for downlink and
     * in max 50 records for uplink but it could be multiplied as per CA.
     * Use bit mask to indicate which MCS index among 0~31 is used.
     */
    uint32 RxMcsIndex;
    uint RxMcsIndexValid;
    uint32 TxMcsIndex;
    uint TxMcsIndexValid;

    uint32 McsIndexCount[MCS_INDEX_NUM_ELEMENTS];
    uint32 CrcFailCount[MCS_INDEX_NUM_ELEMENTS];

#ifdef INCLUDE_LTE_CQI_MSG
    uint32 CqiStatsCount[CQI_NUM_ELEMENTS];
    uint32 PmiStatsCount[PMI_NUM_ELEMENTS];
#endif // INCLUDE_LTE_CQI_MSG

    /* RSRP per antenna */
    float Rsrp[INST_MAX_ANTENNA];
    uint RsrpValid[INST_MAX_ANTENNA];
};

#ifdef V_RF_MEASURE_REPORT_myna
struct cell_info_by_id_t {
    unsigned long CellID;
    int PCI;
    struct avg_t SCellRSRP;
    struct avg_t SCellRSRQ;
    struct avg_t SCellRSSNR;
    struct avg_t SCellRSSINR;
    unsigned long long HandoverAttempt;
    unsigned long long HandoverCount;
    unsigned long long MACiBLERReceivedNackCount;
    unsigned long long MACiBLERReceivedAckNackCount;
    unsigned long long MACrBLERReceivedNackCount;
    unsigned long long MACrBLERReceivedAckNackCount;
    unsigned long long MACiBLERSentNackCount;
    unsigned long long MACiBLERSentAckNackCount;
    unsigned long long MACrBLERSentNackCount;
    unsigned long long MACrBLERSentAckNackCount;
    /* active TTI statistics */
    unsigned long long TotalActiveTTIsReceived;
    unsigned long long TotalActiveTTIsSent;
    /* PRBs statistics */
    unsigned long long TotalPRBsReceived;
    unsigned long long TotalPRBsSent;
    struct distr_t PMIDistribution;
    struct distr_t RIDistribution;
    struct distr_t ReceivedModulationDistribution;
    struct distr_t SendModulationDistribution;
    struct distr_t PUSCHTrasmitPower;
    unsigned long long RLFCount;
    /* RRC statistics */
    unsigned long long NumberofRRCEstabAttempts;
    unsigned long long NumberofRRCEstabFailures;
    unsigned long long NumberofRRCReEstabAttempts;
    unsigned long long NumberofRRCReEstabFailures;
    struct avg_t RRCEstabLatency;
    struct avg_t RRCReEstabLatency;
};
#endif
/* [RG interface] session structure for [/RRC] */
struct rrc_info_t {
    int running;

    int index;
    int update_timestamp;

    time_t StartTime;
    time_t StartTimeMonotonic;
    int PCI;
    unsigned long long RlcUlDuration;
    unsigned long long RlcDlDuration;

    struct avg_t SCellRSRP;
    struct avg_t SCellRSRQ;
    struct avg_t SCellRSSNR;
    struct avg_t SCellRSSINR;

    /*
        int SCellAvgRSRP;
        int SCellAvgRSRQ;
        int SCellAvgRSSNR;
        int SCellWorstRSRP;
        int SCellWorstRSRQ;
        int SCellWorstRSSNR;
    */

    struct acc_t PDCPBytesReceived;
    struct acc_t PDCPBytesSent;

    /* RRC release cause from RLF event */
    int RRCReleaseCause;

    /*
        int PDCPBytesReceived;
        int PDCPBytesSent;
    */

    struct acc_t RLCDownThroughput;
    struct acc_t RLCUpThroughput;

    /*
        int RLCMaxDownThroughput;
        int RLCMaxUpThroughput;
        int RLCAvgDownThroughput;
        int RLCAvgUpThroughput;
    */

    struct avg_t PUSCHTxPower;
    struct avg_t PUCCHTxPower;
    struct avg_t PUSCHTxPowerByCell[LTE_MAX_NUMBER_OF_UPLINK_CARRIER_CELLS];
    struct avg_t PUCCHTxPowerByCell[LTE_MAX_NUMBER_OF_UPLINK_CARRIER_CELLS];
    union
    {
        uint32 active;
        struct {
            uint32 PCC : 1;
            uint32 SCC : 1;
        };
    } tx_power_carrier_idx_flags;

    /*
        int MaxPUSCHTxPower;
        int AvgPUSCHTxPower;
        int MaxPUCCHTxPower;
        int AvgPUCCHTxPower;
    */

#ifdef USE_QDIAG_FOR_CQI
    struct avg_t CQI;
#endif
    struct avg_t AvgWidebandCQIByCell[MAX_SERVER_CELLS_PER_MEASURING_PERIOD];

    int PDCPPacketLossRateReceived;
    int PDCPLatencySent;

    struct acc_t RLCDownRetrans;
    struct acc_t RLCUpRetrans;

    struct acc_t PDCPTotalReceivedPDU;
    struct acc_t PDCPTotalLossSDU;
};

/* [RG interface] session structure for [/RF] and etc */
struct misc_info_t {
    /* statistic for attach */
    unsigned long long AttachAttempts;
    unsigned long long AttachFailures;
    unsigned long long AttachAccepts;

    /* attach latency and time stamp */
    int qctm_attach_attempt_valid;
    time_t qctm_attach_attempt;
    struct avg_t AttachLatency;
};

#define MAX_MANUAL_CELL_MEAS_NUM 20
#define MANUAL_CELL_MEAS_MAX_RSRP_VALUE -40
#define MANUAL_CELL_MEAS_MIN_RSRQ_VALUE -30

/*
 * RF measurement information per each cell.
 */
struct manual_cell_meas_t {
    unsigned int earfcn;
    unsigned int pci;
    double rsrp;
    double rsrq;
};

/*
 * Collection of RF measurements for all measured cells.
 */
struct manual_cell_meas_info_t {
    int is_requested;
    unsigned int seq_no;
    unsigned int num_cells;
    struct manual_cell_meas_t cells[MAX_MANUAL_CELL_MEAS_NUM];
};

struct inst_stats_t {
    float inner_loop_gain; /* RX inner loop gain - invalid when the value is smaller than 0 */

    int pusch_tx_power_valid;
    int pusch_tx_power;
};

/* instance of global instant stats structure */
struct inst_stats_t int_stats = {
    .inner_loop_gain = -1,

    .pusch_tx_power_valid = 0,
    .pusch_tx_power = 0,
};

#define MAX_NUM_P_CELLS 10
#define MAX_NUM_CA_CELLS 20
#define CA_STATE_DISCONNECTED 0
#define CA_STATE_PCC 1
#define CA_STATE_1CC 2
#define CA_STATE_2CC 3
#define CA_STATE_3CC 4
#define CA_STATE_MAX_NUM CA_STATE_3CC
#define CA_INDEX_PCC_ONLY 0
#define CA_INDEX_SCC_ONLY 1
#define CA_INDEX_PCC_AND_SCC 2

struct caCell_t {
    unsigned int freq : 16;
    unsigned int pci : 10;
    unsigned int index : 2;
    unsigned int previousState : 2;
    unsigned int fill : 2;

    unsigned int configuredTime;
    unsigned int activatedTime;
    unsigned int configStartTime;
    unsigned int actStartTime;
};

struct pCell_t {
    unsigned int freq : 16;
    unsigned int pci : 10;
    unsigned int index : 2;
    unsigned int fill : 4;

    unsigned int duration[CA_STATE_MAX_NUM];
};

struct caUsage_t {
    struct caCell_t caCell[MAX_NUM_CA_CELLS];
    struct pCell_t pCell[MAX_NUM_P_CELLS];

    unsigned int pCellStartTime;
    unsigned char pCellState;
    unsigned char currentCaCellIndex;
    unsigned char currentPCellIndex;
    unsigned char numCcConfigured;
    unsigned char ccConfiguredIndex[CA_STATE_MAX_NUM];
    unsigned char usedNumCaCells;
    unsigned char usedNumPCells;
} caUsage;

// local function prototypes
void increase_pcc_ca_state(time_t event_time);
void decrease_pcc_ca_state(time_t event_time);
char *print_error_check(char *buf, char *endBuf, int returnValue);

#define define_app_info(code, name) \
    { \
        code, #name, __on_##name, NULL, 0, 0 \
    }
#define define_app_info_nolimit(code, name) \
    { \
        code, #name, __on_##name, NULL, 0, 1 \
    }

/* log and event handlers */

#ifdef INCLUDE_LTE_CM_MSG
declare_app_func(event_lte_cm_incoming_msg);
declare_app_func(event_lte_cm_outgoing_msg);
#endif /* INCLUDE_LTE_CM_MSG */

#ifdef INCLUDE_LTE_EMM_ESM_MSG
declare_app_func(event_lte_emm_incoming_msg);
declare_app_func(event_lte_emm_ota_incoming_msg);
declare_app_func(event_lte_emm_ota_outgoing_msg);
declare_app_func(event_lte_emm_outgoing_msg);
declare_app_func(event_lte_esm_incoming_msg);
declare_app_func(event_lte_esm_ota_incoming_msg);
declare_app_func(event_lte_esm_ota_outgoing_msg);
declare_app_func(event_lte_esm_outgoing_msg);
#endif /* INCLUDE_LTE_EMM_ESM_MSG */

#ifdef INCLUDE_COMMON_MSG
declare_app_func(event_lte_rrc_dl_msg);
declare_app_func(event_lte_rrc_new_cell_ind);
#ifdef V_RF_MEASURE_REPORT_myna
declare_app_func(event_lte_rrc_handover_failure_ind);
#endif
declare_app_func(event_lte_rrc_radio_link_failure);
declare_app_func(event_lte_rrc_radio_link_failure_stat);
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_RRC_STATE_MSG
declare_app_func(event_lte_rrc_state_change);
#endif /* ifdef INCLUDE_LTE_RRC_STATE_MSG */

#ifdef INCLUDE_COMMON_MSG
declare_app_func(event_lte_rrc_ul_msg);
declare_app_func(event_mac_reset_v2);
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_CQI_MSG
declare_app_func(lte_ll1_csf_pucch_periodic_report);
#endif /* INCLUDE_LTE_CQI_MSG */

#ifdef INCLUDE_COMMON_MSG
declare_app_func(lte_ll1_csf_pusch_aperiodic_report);
declare_app_func(lte_ll1_dl_common_cfg);
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ll1_srch_serving_cell_ttl_result_int);
declare_app_func(lte_ll1_pdsch_demapper_cfg);
declare_app_func(lte_mac_dl_tb);
declare_app_func(lte_mac_ul_tb);
#endif
#endif /* INCLUDE_COMMON_MSG */
#ifdef INCLUDE_BPLMN_SEARCH_MSG
declare_app_func(lte_ml1_bplmn_complete_ind_log);
declare_app_func(lte_ml1_bplmn_start_req_log);
declare_app_func(lte_ml1_bplmn_cell_confirm);
#endif /* INCLUDE_BPLMN_SEARCH_MSG */

#ifdef INCLUDE_LTE_RX_MCS_BLER_MSG
declare_app_func(lte_ml1_dlm_pdsch_stat_ind);
#endif /* INCLUDE_LTE_TX_MCS_BLER_MSG */

declare_app_func(lte_ml1_pusch_power_control);

#ifdef INCLUDE_COMMON_MSG
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ml1_gm_dci_info);
#endif
declare_app_func(lte_ml1_gm_ded_cfg_log);
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ml1_gm_pdcch_phich_info);
#endif
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_TX_MCS_BLER_MSG
declare_app_func(lte_ml1_gm_tx_report);
#endif /* INCLUDE_LTE_TX_MCS_BLER_MSG */

#ifdef INCLUDE_COMMON_MSG
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ml1_serv_cell_meas);
#endif
declare_app_func(lte_ml1_serv_meas_eval);
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ml1_srch_intra_freq_meas);
#endif
#endif /* INCLUDE_COMMON_MSG */
#ifdef INCLUDE_LTE_EMM_ESM_MSG
declare_app_func(lte_nas_emm_ota_in_msg);
declare_app_func(lte_nas_emm_ota_out_msg);
declare_app_func(lte_nas_esm_bearer_context_info);
declare_app_func(lte_nas_esm_bearer_context_state);
declare_app_func(lte_nas_esm_ota_in_msg_log);
declare_app_func(lte_nas_esm_ota_out_msg_log);
#endif /* INCLUDE_LTE_EMM_ESM_MSG */
#ifdef INCLUDE_COMMON_MSG
declare_app_func(lte_pdcpdl_statistics);
declare_app_func(lte_pdcpul_statistics);
declare_app_func(lte_rlcdl_statistics);
declare_app_func(lte_rlcul_statistics);
declare_app_func(lte_rrc_ota_msg);
declare_app_func(lte_serving_cell_info);
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_IMS_VOIP_MSG
/* ims log and event functions */
declare_app_func(event_ims_qipcall_sip_session_event);
declare_app_func(event_ims_sip_deregister_end);
declare_app_func(event_ims_sip_deregister_start);
declare_app_func(event_ims_sip_register_end);
declare_app_func(event_ims_sip_registration_start);
declare_app_func(event_ims_sip_request_recv);
declare_app_func(event_ims_sip_request_send);
declare_app_func(event_ims_sip_response_recv);
declare_app_func(event_ims_sip_response_send);
declare_app_func(event_ims_sip_session_cancel);
declare_app_func(event_ims_sip_session_established);
declare_app_func(event_ims_sip_session_failure);
declare_app_func(event_ims_sip_session_ringing);
declare_app_func(event_ims_sip_session_start);
declare_app_func(event_ims_sip_session_terminated);
declare_app_func(ims_message);
declare_app_func(ims_registration_summary);
declare_app_func(ims_rtcp_packet);
declare_app_func(ims_rtp_sn_and_payload_log_packet);
declare_app_func(voice_call_statistics);
declare_app_func(voip_session_end_summary);
declare_app_func(voip_session_setup_summary);
#endif /* INCLUDE_IMS_VOIP_MSG */

#ifdef INCLUDE_EXTRA_MSG
/* following 2 functions are replaced with lte_ml1_gm_tx_report */
declare_app_func(lte_common);
declare_app_func(lte_ll1_csf_pusch_aperiodic_report);
declare_app_func(lte_ll1_pucch_tx_report);
declare_app_func(lte_ll1_pusch_tx_report);
declare_app_func(lte_ml1_serving_cell_information_log_packet);
#endif /* INCLUDE_EXTRA_MSG */

#ifdef NR5G
declare_app_func(event_nr_undoc_updown);
declare_app_func(nr5g_searcher_measurement_log);
declare_app_func(nr5g_mac_pdsch_status_log);
declare_app_func(nr5g_mac_pdsch_stats_log);
declare_app_func(nr5g_mac_csf_report);
declare_app_func(nr5g_rrc_configuration_info);
#endif

/*

    ## Qualcom ICD documents ##

    * Initially. implemented accordingly to 80-VP457-1 YM, Dec. 2015
    * Modified and improved accordingly to 80-VP457-1 YYN, Jul. 2017
    * Modifed and improved accordingly to 80-VP457-6 Rev. AK March 29, 2020
    * NR5G log structures are re-constructed based on QxDM and QCAT output. NR5G ICD document (QCAP 80-PC6742-2) is not released to
      Casa systems yet and many of missing fields need revisiting when the document becomes available.

    !! note !!
    Event and log messages are used intentionally in the numeric constant form in order to make it easy to match them to the documents. Please avoid
   to use #define for event and log messages, and use the Qualcomm define in the form of comment if the messages are found in Qualcomm source code.

*/

/* relationship between logs and log handlers */
/* disable logs that affect throughput by commenting them out of this structure */
static struct app_info_t log_app_info[] = {

/*
 * Cracknell does not have IMS, VoIP.
 * These 7 log codes do not defined in Qualcomm document 80-VP457-1 xxx.
 * Version comparison is not performed yet so latest document should be
 * checked with old version document to find out the differences before
 * using these messages.
 */
#ifdef INCLUDE_IMS_VOIP_MSG
    define_app_info(0x1568, ims_rtp_sn_and_payload_log_packet),  // LOG_IMS_RTP_SN_AND_PAYLOAD_LOG_PACKET_C
    define_app_info(0x156a, ims_rtcp_packet),                    // LOG_IMS_RTCP_PACKET_C
    define_app_info_nolimit(0x156e, ims_message),                // LOG_IMS_MESSAGE_C
    define_app_info_nolimit(0x17f2, voice_call_statistics),      // LOG_VOICE_CALL_STATISTICS_C
    define_app_info_nolimit(0x1830, voip_session_setup_summary), // LOG_VOIP_SESSION_SETUP_SUMMARY_C
    define_app_info_nolimit(0x1831, voip_session_end_summary),   // LOG_VOIP_SESSION_END_SUMMARY_C
    define_app_info_nolimit(0x1832, ims_registration_summary),   // LOG_IMS_REGISTRATION_SUMMARY_C
#endif                                                           /* INCLUDE_IMS_VOIP_MSG */

#ifdef INCLUDE_COMMON_MSG
/* LOG_CODE 0xb063 & 0xb064 are updated in 80-VP457-1 YYN but only
 * num_samples field is used here so don't need to change. */

/* remove message for Pentecost to improve throughput. Do not remove for Titan */
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
    define_app_info(0xb063, lte_mac_dl_tb), // LOG_LTE_MAC_DL_TB_LOG_C
    define_app_info(0xb064, lte_mac_ul_tb), // LOG_LTE_MAC_UL_TB_LOG_C
#endif

    /* LOG_CODE 0xb087 is same in 80-VP457-1 YYN. */
    define_app_info(0xb087, lte_rlcdl_statistics), // LOG_LTE_RLCDL_STATISTICS_LOG_C

    /* LOG_CODE 0xb097 V3 is implemented for changes in 80-VP457-1 YYN. */
    define_app_info(0xb097, lte_rlcul_statistics), // LOG_LTE_RLCUL_STATISTICS_LOG_C

    /* LOG_CODE 0xb0a4 is updated to v2 in 80-VP457-1 YYN but current
     * modem version is v1 so don't need to change at the moment.
     */
    define_app_info(0xb0a4, lte_pdcpdl_statistics), // LOG_LTE_PDCPDL_STATISTICS_LOG_C

    /* LOG_CODE 0xb0b4 is updated to v24 in 80-VP457-1 YYN but current
     * modem version is v1 so don't need to change at the moment.
     */
    define_app_info(0xb0b4, lte_pdcpul_statistics), // LOG_LTE_PDCPUL_STATISTICS_LOG_C

    /* LOG_CODE 0xb0c0 is updated to v15 in 80-VP457-1 YYN and current
     * modem version is v13 in 9x50. The only changes are PDU_NUM definition
     * and gdiagd has only interest when PDU_NUM is 2 (BCCH_DL_SCH) so
     * don't need to change at the moment.
     */
    define_app_info_nolimit(0xb0c0, lte_rrc_ota_msg), // LOG_LTE_RRC_OTA_MSG_LOG_C

    /* Below LOG_CODEs are same in 80-VP457-1 YYN.
     * 0xb0c2, 0xb0e2, 0xb0e3, 0xb0e4, 0xb0e5, 0xb0ec & 0xb0ed
     */
    define_app_info(0xb0c2, lte_serving_cell_info), // LOG_LTE_RRC_SERV_CELL_INFO_LOG_C
#endif                                              /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_EMM_ESM_MSG
    define_app_info_nolimit(0xb0e2, lte_nas_esm_ota_in_msg_log),  // LOG_LTE_NAS_ESM_OTA_IN_MSG_LOG_C
    define_app_info_nolimit(0xb0e3, lte_nas_esm_ota_out_msg_log), // LOG_LTE_NAS_ESM_OTA_OUT_MSG_LOG_C
    define_app_info(0xb0e4, lte_nas_esm_bearer_context_state),    // LOG_LTE_NAS_ESM_BEARER_CONTEXT_STATE_LOG_C
    define_app_info(0xb0e5, lte_nas_esm_bearer_context_info),     // LOG_LTE_NAS_ESM_BEARER_CONTEXT_INFO_LOG_C
    define_app_info_nolimit(0xb0ec, lte_nas_emm_ota_in_msg),      // LOG_LTE_NAS_EMM_OTA_IN_MSG_LOG_C
    define_app_info_nolimit(0xb0ed, lte_nas_emm_ota_out_msg),     // LOG_LTE_NAS_EMM_OTA_OUT_MSG_LOG_C
#endif                                                            /* INCLUDE_LTE_EMM_ESM_MSG */

#ifdef INCLUDE_COMMON_MSG
/* remove message for Pentecost to improve throughput. Do not remove for Titan */
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
    define_app_info(0xb11d, lte_ll1_srch_serving_cell_ttl_result_int), // LOG_LTE_LL1_SRCH_SERVING_CELL_TTL_RESULT_INT_LOG_C
#endif

/* LOG_CODE 0xb126 is updated to v28 in 80-VP457-1 YYN and current
 * modem version is v121 (v121 -> v122 -> v123 -> v28) in 9x50 which
 * is same as currently implemented version so don't need to change at the moment.
 */
/* remove message for Pentecost to improve throughput. Do not remove for Titan */
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
    define_app_info(0xb126, lte_ll1_pdsch_demapper_cfg), // LOG_LTE_LL1_PDSCH_DEMAPPER_LOG_C
#endif

#endif /* INCLUDE_COMMON_MSG */

/* Below LOG_CODEs are same in 80-VP457-1 YYN.
 * 0xb14d, 0xb14e
 */
#ifdef INCLUDE_LTE_CQI_MSG
    define_app_info(0xb14d, lte_ll1_csf_pucch_periodic_report), // LOG_LTE_LL1_CSF_PUCCH_PERIODIC_REPORT_LOG_C
#endif                                                          /* INCLUDE_LTE_CQI_MSG */

#ifdef INCLUDE_COMMON_MSG
    define_app_info(0xb14e, lte_ll1_csf_pusch_aperiodic_report), // LOG_LTE_LL1_CSF_PUSCH_APERIODIC_REPORT_LOG_C

    /* LOG_CODE 0xb160 is upto date as in 80-VP457-1 YYN. */
    define_app_info(0xb160, lte_ll1_dl_common_cfg), // LOG_LTE_LL1_DL_CFG_LOG_C

    /* LOG_CODE 0xb165 was already v5 which is latest in 80-VP457-1 YYN but
     * had structural mismatch so updated the packet structure definition. */
    define_app_info(0xb165, lte_ml1_gm_ded_cfg_log), // LOG_LTE_ML1_GM_DED_CFG_LOG_C

/* remove message for Pentecost to improve throughput. Do not remove for Titan */
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
    /* LOG_CODE 0xb165 was v25 and in 80-VP457-1 YYN latest version is v34 but
     * modem has v33 so implemented v33 here. */
    define_app_info(0xb16b, lte_ml1_gm_pdcch_phich_info), // LOG_LTE_ML1_GM_PDCCH_PHICH_INFO_LOG_C

    /* LOG_CODE 0xb16c was v26 and in 80-VP457-1 YYN latest version is v9 (-> v34 -> v9)
     * but modem has v34 so implemented v34 here. */
    define_app_info(0xb16c, lte_ml1_gm_dci_info), // LOG_LTE_ML1_GM_DCI_INFO_LOG_C
#endif

#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_TX_MCS_BLER_MSG
    /* LOG_CODE 0xb16d was v26 and in 80-VP457-1 YYN latest version is v33 and
     * modem has v33 so implemented v33 here. */
    define_app_info_nolimit(0xb16d, lte_ml1_gm_tx_report), // LOG_LTE_ML1_GM_TX_REPORT_LOG_C

#endif /* INCLUDE_LTE_TX_MCS_BLER_MSG */

    define_app_info(0xb16e, lte_ml1_pusch_power_control), // LOG_LTE_ML1_PUSCH_POWER_CONTROL_LOG_C

#ifdef INCLUDE_LTE_RX_MCS_BLER_MSG
    /* LOG_CODE 0xb173 was v24 and in 80-VP457-1 YYN latest version is v36 but
     * modem has v34 so implemented v34 here. */
    define_app_info_nolimit(0xb173, lte_ml1_dlm_pdsch_stat_ind), // LOG_LTE_ML1_DLM_PDSCH_STAT_IND_LOG_C
#endif                                                           /* INCLUDE_LTE_RX_MCS_BLER_MSG */

#ifdef INCLUDE_COMMON_MSG
/* LOG_CODE 0xb179 is upto date (v4) as in 80-VP457-1 YYN. */
/* remove message for Pentecost to improve throughput. Do not remove for Titan */
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
    define_app_info(0xb179, lte_ml1_srch_intra_freq_meas), // LOG_LTE_ML1_SRCH_INTRA_FREQ_MEAS_LOG_C
#endif

    /* LOG_CODE 0xb17f is upto date (v5) as in 80-VP457-1 YYN but structure definition
     * does not match with the document so fixed it verified the result. */
    define_app_info(0xb17f, lte_ml1_serv_meas_eval), // LOG_LTE_ML1_SERV_MEAS_EVAL_LOG_C

/* LOG_CODE 0xb193 is upto date (v1) as in 80-VP457-1 YYN but sub packet version
 * does not match, v22 was implemented, 80-VP457-1 YYN defines to v35 but the modem
 * has v33 so implemented v33 here. */

/* remove message for Pentecost to improve throughput. Do not remove for Titan */
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
    define_app_info(0xb193, lte_ml1_serv_cell_meas), // LOG_LTE_ML1_SERV_CELL_MEAS_RESPONSE_LOG_C
#endif
#endif /* INCLUDE_COMMON_MSG */
#ifdef INCLUDE_BPLMN_SEARCH_MSG
    /*
        brief description of BPLMN sequence

        1. UE performs PLMN scan - by QMI, by AT command or internal reason
        2. start BPLMN scan - qdiag receives "lte_ml1_bplmn_start_req_log"
        3. collect cell IDs by parsing SIB1 messages from "lte_rrc_ota_msg"
        4. complete BPLMN scan - qdiag receives "lte_ml1_bplmn_complete_ind_log"
    */
    /* LOG_CODE 0xb1a0, 0xb1a2 and 0xb1a4 are upto date as defined in 80-VP457-1 YYN
     * so don't need to update. */
    define_app_info_nolimit(0xb1a0, lte_ml1_bplmn_start_req_log),    // LOG_LTE_ML1_BPLMN_START_REQ_LOG_C
    define_app_info_nolimit(0xb1a2, lte_ml1_bplmn_cell_confirm),     // LOG_LTE_ML1_BPLMN_CELL_CONFIRM_LOG_C
    define_app_info_nolimit(0xb1a4, lte_ml1_bplmn_complete_ind_log), // LOG_LTE_ML1_BPLMN_COMPLETE_IND_LOG_C
#endif                                                               /* INCLUDE_BPLMN_SEARCH_MSG */

#ifdef NR5G
    define_app_info_nolimit(0xb97f, nr5g_searcher_measurement_log),
    define_app_info_nolimit(0xb887, nr5g_mac_pdsch_status_log),
    define_app_info_nolimit(0xb888, nr5g_mac_pdsch_stats_log),
    define_app_info_nolimit(0xb8a7, nr5g_mac_csf_report),
    define_app_info_nolimit(0xb825, nr5g_rrc_configuration_info),
#endif

#ifdef INCLUDE_EXTRA_MSG
    /* implemented but not in use */
    /* LOG_CODE 0xb139 was v101 and in 80-VP457-1 YYN latest version is v124 but
     * modem has v121 so implemented v121 here. */
    define_app_info(0xb139, lte_ll1_pusch_tx_report), // LOG_LTE_LL1_UL_PUSCH_TX_REPORT_LOG_C
    /* LOG_CODE 0xb13c is upto date (v101) as in 80-VP457-1 YYN. */
    define_app_info(0xb13c, lte_ll1_pucch_tx_report), // LOG_LTE_LL1_UL_PUCCH_TX_REPORT_LOG_C
    /* LOG_CODE 0xb197 was v2 so implemented v3 as defined in 80-VP457-1 YYN */
    define_app_info(0xb197, lte_ml1_serving_cell_information_log_packet), // LOG_LTE_ML1_SERV_CELL_INFO_LOG_C
    define_app_info(0xffff, lte_common),
#endif /* INCLUDE_EXTRA_MSG */

/* not used yet */
/* TO DO : checked version update with 80-VP457-1 YYN */
#if 0
    /* missing messages */
    define_app_info(0xb160, lte_ml1_downlink_common_configuration),
    define_app_info(0xb164, lte_ml1_grant_manager_common_configuration), /* downlink bandwidth */
    define_app_info(0xb1b9, lte_ml1_coex_state_info_log), /* uplink and downlink bandwidth */
#endif
};

/* relationship between events and event handlers */
static struct app_info_t event_app_info[] = {
#ifdef INCLUDE_LTE_RRC_STATE_MSG
    define_app_info_nolimit(1606, event_lte_rrc_state_change), // EVENT_LTE_RRC_STATE_CHANGE
#endif                                                         /* INCLUDE_LTE_RRC_STATE_MSG */

#ifdef INCLUDE_COMMON_MSG
    define_app_info_nolimit(1608, event_lte_rrc_radio_link_failure), // EVENT_LTE_RRC_RADIO_LINK_FAILURE
    define_app_info_nolimit(1609, event_lte_rrc_dl_msg),             // EVENT_LTE_RRC_DL_MSG
    define_app_info_nolimit(1610, event_lte_rrc_ul_msg),             // EVENT_LTE_RRC_UL_MSG
    define_app_info_nolimit(1611, event_lte_rrc_new_cell_ind),       // EVENT_LTE_RRC_NEW_CELL_IND
#ifdef V_RF_MEASURE_REPORT_myna
    define_app_info_nolimit(1613, event_lte_rrc_handover_failure_ind),
#endif
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_CM_MSG
    define_app_info_nolimit(1627, event_lte_cm_incoming_msg), // EVENT_LTE_CM_INCOMING_MSG
    define_app_info_nolimit(1628, event_lte_cm_outgoing_msg), // EVENT_LTE_CM_OUTGOING_MSG
#endif                                                        /* INCLUDE_LTE_CM_MSG */

#ifdef INCLUDE_LTE_EMM_ESM_MSG
    define_app_info_nolimit(1629, event_lte_emm_incoming_msg),     // EVENT_LTE_EMM_INCOMING_MSG
    define_app_info_nolimit(1630, event_lte_emm_outgoing_msg),     // EVENT_LTE_EMM_OUTGOING_MSG
    define_app_info_nolimit(1635, event_lte_esm_incoming_msg),     // EVENT_LTE_ESM_INCOMING_MSG
    define_app_info_nolimit(1636, event_lte_esm_outgoing_msg),     // EVENT_LTE_ESM_OUTGOING_MSG
    define_app_info_nolimit(1966, event_lte_emm_ota_incoming_msg), // EVENT_LTE_EMM_OTA_INCOMING_MSG
    define_app_info_nolimit(1967, event_lte_emm_ota_outgoing_msg), // EVENT_LTE_EMM_OTA_OUTGOING_MSG
    define_app_info_nolimit(1968, event_lte_esm_ota_incoming_msg), // EVENT_LTE_ESM_OTA_INCOMING_MSG
    define_app_info_nolimit(1969, event_lte_esm_ota_outgoing_msg), // EVENT_LTE_ESM_OTA_OUTGOING_MSG
#endif                                                             /* INCLUDE_LTE_EMM_ESM_MSG */

#ifdef INCLUDE_COMMON_MSG
    define_app_info_nolimit(1995, event_lte_rrc_radio_link_failure_stat), // EVENT_LTE_RRC_RADIO_LINK_FAILURE_STAT
    define_app_info_nolimit(2692, event_mac_reset_v2),                    // EVENT_LTE_MAC_RESET_V2
#endif                                                                    /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_IMS_VOIP_MSG
    define_app_info_nolimit(EVENT_IMS_QIPCALL_SIP_SESSION_EVENT, event_ims_qipcall_sip_session_event), // 0x86e
    define_app_info_nolimit(EVENT_IMS_SIP_DEREGISTER_END, event_ims_sip_deregister_end),               // 0x874
    define_app_info_nolimit(EVENT_IMS_SIP_DEREGISTER_START, event_ims_sip_deregister_start),           // 0x873
    define_app_info_nolimit(EVENT_IMS_SIP_REGISTER_END, event_ims_sip_register_end),                   // 0x872
    define_app_info_nolimit(EVENT_IMS_SIP_REGISTRATION_START, event_ims_sip_registration_start),       // 0x86f
    define_app_info_nolimit(EVENT_IMS_SIP_REQUEST_RECV, event_ims_sip_request_recv),                   // 0x59b
    define_app_info_nolimit(EVENT_IMS_SIP_REQUEST_SEND, event_ims_sip_request_send),                   // 0x59d
    define_app_info_nolimit(EVENT_IMS_SIP_RESPONSE_RECV, event_ims_sip_response_recv),                 // 0x59a
    define_app_info_nolimit(EVENT_IMS_SIP_RESPONSE_SEND, event_ims_sip_response_send),                 // 0x59c
    define_app_info_nolimit(EVENT_IMS_SIP_SESSION_CANCEL, event_ims_sip_session_cancel),               // 0x598
    define_app_info_nolimit(EVENT_IMS_SIP_SESSION_ESTABLISHED, event_ims_sip_session_established),     // 0x596
    define_app_info_nolimit(EVENT_IMS_SIP_SESSION_FAILURE, event_ims_sip_session_failure),             // 0x599
    define_app_info_nolimit(EVENT_IMS_SIP_SESSION_RINGING, event_ims_sip_session_ringing),             // 0x595
    define_app_info_nolimit(EVENT_IMS_SIP_SESSION_START, event_ims_sip_session_start),                 // 0x594
    define_app_info_nolimit(EVENT_IMS_SIP_SESSION_TERMINATED, event_ims_sip_session_terminated),       // 0x597
#endif                                                                                                 /* INCLUDE_IMS_VOIP_MSG */

#ifdef NR5G
    define_app_info_nolimit(3191, event_nr_undoc_updown),
#endif

};

static struct diag_stat_t diag_stat; /* Qualcomm diag initiation status */

/*
        ### instance of RB interface session structures ###
*/
#ifdef INCLUDE_IMS_VOIP_MSG
static struct linestats_info_t linestats_info; /* VoLTE line statistics */
#endif
static struct rrc_info_t rrc_info;           /* RRC session */
static struct servcell_info_t servcell_info; /* Serving cell */
#ifdef V_RF_MEASURE_REPORT_myna
static struct servcell_info_t accu_servcell_info; /* accumulated Serving cell information */
int g_servcell_info_acc_count = 0;
#endif
struct avg_t total_pusch_tx_power; /* total pusch tx power, maintained by lte_ll1_pusch_tx_report */
struct misc_info_t misc_info;      /* misc statistics information */
struct manual_cell_meas_info_t manual_cell_meas_info = {
    .is_requested = 0,
    .seq_no = 0,
}; /* manual cell scan measurement information */

/*
        ### linked list of voice call information ###
*/
struct voicecall_info_t voicecall_info_pool[MAX_VOICECALL_SESSION_INFO]; /* VoLTE voice call session poll */
LIST_HEAD(voicecall_info_offline);                                       /* VoLTE voice call offline session */
LIST_HEAD(voicecall_info_online);                                        /* VoLTE voice call online session */

/*
        ### constant declares ###
*/

/* RRC state from Qualcomm document 80-VP457-1 YM */

const char *rrc_state_str[] = {
    [0x00] = "inactive",  [0x01] = "idle not camped", [0x02] = "idle camped",         [0x03] = "connecting",
    [0x04] = "connected", [0x05] = "suspended",       [0x06] = "irat to lte started", [0x07] = "closing",
};

/* voice call session ID */
enum session_id_type
{
    session_id_type_none,
    session_id_type_cm,
    session_id_type_qc,
    session_id_type_sip,
    session_id_type_ssrc_tx,
    session_id_type_ssrc_rx,
};

#if defined SAVE_RFQ_EARFCN_LIST
#define RFQ_EARFCN_RDB "wwan.0.rfq_earfcn"
#define MAX_EARFCN_ENTRIES 64
static unsigned int manual_cell_meas_earfcns[MAX_EARFCN_ENTRIES];
static unsigned int manual_cell_meas_earfcn_count = 0;
void init_internal_list_per_rfq_earfcn();
#endif /* SAVE_RFQ_EARFCN_LIST */

#ifdef V_RF_MEASURE_REPORT_myna
/* collect serving cell info per cell ID */
static struct cell_info_by_id_t cell_info_by_id[MAX_SERVER_CELLS_PER_MEASURING_PERIOD];
#endif

/*
        ### local functions ###
*/

/* utility functions */
void log_verbose_dump(const char *buf, int len);
void log_debug_dump(const char *buf, int len);
void reset_rdb_sets(const char *prefix);
const char *get_str_from_var(char *str, int str_len, const void *val, int len);

#ifdef INCLUDE_IMS_VOIP_MSG
/* voice call session tracking functions */
static struct voicecall_info_t *vci_get_session_by_cm_call_id(int cm_call_id);
static struct voicecall_info_t *vci_get_session(int (*cmp)(struct voicecall_info_t *, const void *), const void *ref);
static void vci_delete_session(struct voicecall_info_t *vci);
struct voicecall_info_t *vci_create_session();
struct voicecall_info_t *vci_get_last();
struct voicecall_info_t *vci_create_session_by_id(enum session_id_type id_type, const void *session_id);
static struct voicecall_info_t *vci_get_session_by_qc_call_id(unsigned int qc_call_id);
static struct voicecall_info_t *vci_get_session_by_ssrc_tx_id(unsigned int ssrc);
static struct voicecall_info_t *vci_get_session_by_ssrc_rx_id(unsigned int ssrc);
static struct voicecall_info_t *vci_get_session_by_sip_call_id(const char *sip_call_id);
static struct voicecall_info_t *vci_get_session_by_qc_call_id_mt(unsigned int qc_call_id);
static struct voicecall_info_t *vci_get_session_by_ssrc_tx_id_mt(unsigned int ssrc);
static struct voicecall_info_t *vci_get_session_by_ssrc_rx_id_mt(unsigned int ssrc);
static struct voicecall_info_t *vci_get_session_by_sip_call_id_mt(const char *sip_call_id);
static struct voicecall_info_t *vci_get_session_by_cm_call_id_mt(int cm_call_id);
#endif /* INCLUDE_IMS_VOIP_MSG */

/* information collector */
void rrc_info_start();
void rrc_info_stop();

#ifdef INCLUDE_IMS_VOIP_MSG
int voicecall_info_start(struct voicecall_info_t *vci, unsigned long long *ts);
void voicecall_info_stop(struct voicecall_info_t *vci, unsigned long long *ts);
void voicecall_info_update(struct voicecall_info_t *vci);
#endif /* INCLUDE_IMS_VOIP_MSG */

/* rdb update functions */
#ifdef INCLUDE_IMS_VOIP_MSG
void linestats_info_update_rdb();
#endif /* INCLUDE_IMS_VOIP_MSG */
void servcell_info_update(struct servcell_info_t *servcell, time_t tm, time_t tm_monotonic);
void servcell_info_reset(struct servcell_info_t *servcell, int init_size, time_t tm, time_t tm_monotonic);
void misc_info_update();

#ifdef TEST_MODE_MODULE
/*
 perform Unaligned PER decode verification

 Return:
  0 = success, -1 = failure
*/
int perform_per_decode_verification()
{
    asn_dec_rval_t rval;
    asn_codec_ctx_t *opt_codec_ctx;
    asn_TYPE_descriptor_t *pdu_type;

    BCCH_DL_SCH_Message_t *bcch_dl_sch_message;
    PLMN_IdentityList_t *plmn_identitylist;
    PLMN_IdentityInfo_t *plmn_identityinfo;
    CellIdentity_t *cellidentity;
    int i;

    int cell_id;

    /* OTA 0xb0c0 message */
    char ota_per[] = { 0x60, 0x40, 0x04, 0x03, 0x00, 0x01, 0x1a, 0x2d, 0x00, 0x18, 0x02, 0x80, 0x40, 0x42, 0x0c, 0x80 };
    int ota_per_len = sizeof(ota_per);

    /* decode per */
    opt_codec_ctx = 0;
    bcch_dl_sch_message = 0;
    pdu_type = &asn_DEF_BCCH_DL_SCH_Message;
    rval = uper_decode_complete(opt_codec_ctx, pdu_type, (void **)&bcch_dl_sch_message, ota_per, ota_per_len);

    /* bypass if there is an error */
    if (rval.code != RC_OK) {
        ERR("failed to decode UPER (error code = %d)", rval.code);
        goto err;
    }

    /*
        * structure of BCC-DL-SCH message

        bcch_dl_sch_message->message.present==BCCH_DL_SCH_MessageType_PR_c1
        bcch_dl_sch_message->message.choice.c1.present==BCCH_DL_SCH_MessageType__c1_PR_systemInformationBlockType1
        bcch_dl_sch_message->message.choice.c1.choice.systemInformationBlockType1
        bcch_dl_sch_message->message.choice.c1.choice.systemInformationBlockType1.cellAccessRelatedInfo
    */

    plmn_identitylist = &bcch_dl_sch_message->message.choice.c1.choice.systemInformationBlockType1.cellAccessRelatedInfo.plmn_IdentityList;
    cellidentity = &bcch_dl_sch_message->message.choice.c1.choice.systemInformationBlockType1.cellAccessRelatedInfo.cellIdentity;

    /* get cell id */
    cell_id = get_uint_from_bit_string(cellidentity);
    DEBUG("[unit-test-ota-parser] test cell id = %d", cell_id);

    MCC_t *mcc;
    MNC_t *mnc;

    char mcc_str[8];
    char mnc_str[8];

    /* for each of plmn indentity */
    for_each_asn1c_array(i, plmn_identitylist, plmn_identityinfo)
    {
        mcc = plmn_identityinfo->plmn_Identity.mcc;
        mnc = &plmn_identityinfo->plmn_Identity.mnc;

        /* decode MCC */
        if (!get_mccmnc_from_mccmnc_digits(mcc_str, sizeof(mcc_str), mcc, NULL)) {
            ERR("failed to decode MCC");
            goto err;
        }

        /* decode MNC */
        if (!get_mccmnc_from_mccmnc_digits(mnc_str, sizeof(mnc_str), NULL, mnc)) {
            ERR("failed to decode MNC");
            goto err;
        }

        DEBUG("[OTA-VERIFY] PLMN#%d, mcc=%s, mnc=%s", i, mcc_str, mnc_str);
    }

    /*
    asn_fprint(stdout,pdu_type,bcch_dl_sch_message);
    */

    ASN_STRUCT_FREE(*pdu_type, bcch_dl_sch_message);
    return 0;

err:
    ASN_STRUCT_FREE(*pdu_type, bcch_dl_sch_message);
    return -1;
}
#endif

/*
        ### following functions are peripheral functions for log handlers ###
*/

/*
 Get sub-frame.
  Search sub-frame structure in the hash table by using SFN and SUBFN numbers.

 Parameters:
  subfns : sub-frame array.
  sfn : SFN.
  subfn : Sub-frame.

 Return:
  Point to sub-frame.
*/
static inline struct subfn_t *get_subfn(struct subfn_t *subfns, int sfn, int subfn)
{
    int index = ((sfn & 0x3ff) << 4) | (subfn & 0x0f);

    return &subfns[index];
}

/*
 Get 4th next sub-frame.
  Search 4th sub-frame structure in the hash table by using SFN and SUBFN numbers.

 Parameters:
  subfns : sub-frame array.
  sfn : SFN.
  subfn : Sub-frame.

 Return:
  Point to 4th next sub-frame.
*/
static inline struct subfn_t *get_next_subfn(struct subfn_t *subfns, int sfn, int subfn)
{
    int new_subfn;
    int sfn_carrier;

    /* get new subfn */
    sfn_carrier = (subfn + 4 < 10) ? 0 : 1;
    new_subfn = (subfn + 4) % 10;

    return get_subfn(subfns, sfn + sfn_carrier, new_subfn);
}

/*
 Add new cell measurement

 Parameters:
  earfcn : EARFCN
  pci : PCI(Physical Cell ID)
  rsrp : RSRP
  rsrq : RSRQ
*/
static void add_manual_cell_measurement(unsigned int earfcn, unsigned int pci, double rsrp, double rsrq)
{
    int i;
    struct manual_cell_meas_t *cell = NULL;

    /* Check whether the manual scan has been started */
    if (!manual_cell_meas_info.is_requested) {
        return;
    }

    /* Check if the corresponding cell measurement already exists */
    for (i = 0; i < manual_cell_meas_info.num_cells; i++) {
        if ((earfcn == manual_cell_meas_info.cells[i].earfcn) && (pci == manual_cell_meas_info.cells[i].pci)) {
            cell = &manual_cell_meas_info.cells[i];
        }
    }

    if (!cell) {
        /* It's a new cell measurement, so ensure we have enough space to add it.
         * If the measurement queue is full and the lowest RSRP cell in the queue
         * is lower than the new cell measurement, replace it with new one.
         */
        if (manual_cell_meas_info.num_cells >= MAX_MANUAL_CELL_MEAS_NUM) {
            int min_rsrp_idx = 0;
            double min_rsrp = MANUAL_CELL_MEAS_MAX_RSRP_VALUE;
            for (i = 0; i < manual_cell_meas_info.num_cells; i++) {
                if (min_rsrp > manual_cell_meas_info.cells[i].rsrp) {
                    min_rsrp_idx = i;
                    min_rsrp = manual_cell_meas_info.cells[i].rsrp;
                }
            }
            if (min_rsrp > rsrp) {
                return;
            }
            cell = &manual_cell_meas_info.cells[min_rsrp_idx];
        } else {
            /* Append new cell measurement */
            cell = &manual_cell_meas_info.cells[manual_cell_meas_info.num_cells];
            manual_cell_meas_info.num_cells++;
        }
    }

    if (cell) {
        cell->earfcn = earfcn;
        cell->pci = pci;
        cell->rsrp = rsrp;
        cell->rsrq = rsrq;
    }
}

/*
        ### log handlers ###
*/

#ifdef V_RF_MEASURE_REPORT_myna
int get_cell_index_by_pci(int pci)
{
    int i = 0;
    int found = 0;
    /* find the pci or first empty array element */
    while ((i < MAX_SERVER_CELLS_PER_MEASURING_PERIOD) && (found == 0)) {
        if ((cell_info_by_id[i].PCI == pci) || (cell_info_by_id[i].PCI == -1)) {
            found = 1;
        } else {
            i++;
        }
    }
    /* if cell_id wasn't found and there are no empty slots */
    if (found == 0) {
        /* return error */
        i = -1;
    }
    return i;
}

void reset_cell_count_info(unsigned long long *counter, unsigned long long value)
{
    *counter = value;
}

void add_cell_avg_info(struct avg_t *source, struct avg_t *destination)
{
    /* do not add average if source is invalid */
    if (!source->stat_valid) {
        return;
    }
    if (source->c <= 0) {
        return;
    }
    /* update last value */
    destination->l = source->l;
    /* update min */
    if (source->min < destination->min) {
        destination->min = source->min;
    }
    /* update max */
    if (source->max > destination->max) {
        destination->max = source->max;
    }
    if ((destination->c + source->c) <= 0) {
        /* no data, mark it as invalid */
        destination->stat_valid = 0;
    } else {
        /* update the average */
        destination->avg = ((destination->avg * destination->c) + (source->avg * source->c)) / (destination->c + source->c);
        /* copy the validity */
        destination->stat_valid = source->stat_valid;
    }
    /* update count */
    destination->c += source->c;
}

int add_cell_distr_info(struct distr_t *source, struct distr_t *destination)
{
    int i;

    if (!source->c) {
        ERR("no range buffer allocated");
        return -1;
    }
    destination->range = source->range;
    for (i = 0; i < destination->range; i++) {
        destination->c[i] += source->c[i];
    }
    return 0;
}

void add_cell_count_info(unsigned long long *source, unsigned long long *destination)
{
    *destination += *source;
}

void servcell_info_by_id_init(void)
{
    int cell_idx;

    /* initiate servcell_info */
    memset(&cell_info_by_id, 0, sizeof(cell_info_by_id));

    for (cell_idx = 0; cell_idx < MAX_SERVER_CELLS_PER_MEASURING_PERIOD; cell_idx++) {
        cell_info_by_id[cell_idx].CellID = 0;
        cell_info_by_id[cell_idx].PCI = -1;
        avg_init(&cell_info_by_id[cell_idx].SCellRSRP, "SCellRSRP", 0);
        avg_init(&cell_info_by_id[cell_idx].SCellRSRQ, "SCellRSRQ", 0);
        avg_init(&cell_info_by_id[cell_idx].SCellRSSNR, "SCellRSSNR", 1);
        avg_init(&cell_info_by_id[cell_idx].SCellRSSINR, "SCellRSSINR", 1);
        reset_cell_count_info(&cell_info_by_id[cell_idx].HandoverAttempt, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].HandoverCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACiBLERReceivedNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACiBLERReceivedAckNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACrBLERReceivedNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACrBLERReceivedAckNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACiBLERSentNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACiBLERSentAckNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACrBLERSentNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACrBLERSentAckNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].TotalActiveTTIsReceived, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].TotalActiveTTIsSent, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].TotalPRBsReceived, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].TotalPRBsSent, 0);
        distr_init(&cell_info_by_id[cell_idx].PMIDistribution, "PMIDistribution", 16);
        distr_init(&cell_info_by_id[cell_idx].RIDistribution, "RIDistribution", 4);
        distr_init(&cell_info_by_id[cell_idx].ReceivedModulationDistribution, "ReceivedModulationDistribution", RX_MOD_DISTR_NUM_TYPES);
        distr_init(&cell_info_by_id[cell_idx].SendModulationDistribution, "SendModulationDistribution", 3);
        distr_init(&cell_info_by_id[cell_idx].PUSCHTrasmitPower, "PUSCHTrasmitPower", 2);
        reset_cell_count_info(&cell_info_by_id[cell_idx].RLFCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].NumberofRRCEstabAttempts, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].NumberofRRCEstabFailures, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].NumberofRRCReEstabAttempts, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].NumberofRRCReEstabFailures, 0);
        avg_init(&cell_info_by_id[cell_idx].RRCEstabLatency, "RRCEstabLatency", 0);
        avg_init(&cell_info_by_id[cell_idx].RRCReEstabLatency, "RRCReEstabLatency", 0);
    }
}
void servcell_info_by_id_reset(void)
{
    int cell_idx;

    for (cell_idx = 0; cell_idx < MAX_SERVER_CELLS_PER_MEASURING_PERIOD; cell_idx++) {
        /* collecting data from the new cell, just add to the data */
        cell_info_by_id[cell_idx].CellID = 0;
        cell_info_by_id[cell_idx].PCI = -1;
        avg_reset(&cell_info_by_id[cell_idx].SCellRSRP);
        avg_reset(&cell_info_by_id[cell_idx].SCellRSRQ);
        avg_reset(&cell_info_by_id[cell_idx].SCellRSSNR);
        avg_reset(&cell_info_by_id[cell_idx].SCellRSSINR);
        reset_cell_count_info(&cell_info_by_id[cell_idx].HandoverAttempt, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].HandoverCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACiBLERReceivedNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACiBLERReceivedAckNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACrBLERReceivedNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACrBLERReceivedAckNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACiBLERSentNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACiBLERSentAckNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACrBLERSentNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].MACrBLERSentAckNackCount, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].TotalActiveTTIsReceived, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].TotalActiveTTIsSent, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].TotalPRBsReceived, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].TotalPRBsSent, 0);
        distr_reset(&cell_info_by_id[cell_idx].PMIDistribution);
        distr_reset(&cell_info_by_id[cell_idx].RIDistribution);
        distr_reset(&cell_info_by_id[cell_idx].ReceivedModulationDistribution);
        distr_reset(&cell_info_by_id[cell_idx].SendModulationDistribution);
        distr_reset(&cell_info_by_id[cell_idx].PUSCHTrasmitPower);
        reset_cell_count_info(&cell_info_by_id[cell_idx].NumberofRRCEstabAttempts, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].NumberofRRCEstabFailures, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].NumberofRRCReEstabAttempts, 0);
        reset_cell_count_info(&cell_info_by_id[cell_idx].NumberofRRCReEstabFailures, 0);
        avg_reset(&cell_info_by_id[cell_idx].RRCEstabLatency);
        avg_reset(&cell_info_by_id[cell_idx].RRCReEstabLatency);
    }
}

/*
 find the cell ID with given PCI from gc list

 Parameters:
  pci

 Return:
  cell ID if found, or return 0
*/
int get_cell_id_by_pci(int pci)
{
    struct gcell_t *gc;
    gc = gcell_get_first_or_next(NULL);
    while (gc) {
        if (gc->phys_cell_id == pci) {
            return gc->cell_id;
        }
        gc = gcell_get_first_or_next(gc);
    }
    return 0;
}

/*
 servcell_info might been reset in cell_based_data_collection() if cell ID was
 changed during measuring period so servcell_info should be sumed up with
 accu_servcell_info for full data for this measuring period.

 Parameters:
  None

 Return:
  None
*/
void sumup_servcell_info(void)
{
    int i;
    add_cell_avg_info(&servcell_info.SCellRSSNR, &accu_servcell_info.SCellRSSNR);
    add_cell_avg_info(&servcell_info.SCellRSSINR, &accu_servcell_info.SCellRSSINR);
    add_cell_avg_info(&servcell_info.SCellRSRP, &accu_servcell_info.SCellRSRP);
    add_cell_avg_info(&servcell_info.SCellRSRQ, &accu_servcell_info.SCellRSRQ);
    add_cell_avg_info(&servcell_info.PathLoss, &accu_servcell_info.PathLoss);
    add_cell_avg_info(&servcell_info.AvgWidebandCQI, &accu_servcell_info.AvgWidebandCQI);
    add_cell_distr_info(&servcell_info.PUSCHTrasmitPower, &accu_servcell_info.PUSCHTrasmitPower);
    add_cell_distr_info(&servcell_info.PMIDistribution, &accu_servcell_info.PMIDistribution);
    add_cell_distr_info(&servcell_info.RIDistribution, &accu_servcell_info.RIDistribution);
    add_cell_distr_info(&servcell_info.ReceivedModulationDistribution, &accu_servcell_info.ReceivedModulationDistribution);
    add_cell_distr_info(&servcell_info.SendModulationDistribution, &accu_servcell_info.SendModulationDistribution);
    if (servcell_info.NumberofRRCEstabAttempts) {
        accu_servcell_info.RRCEstabAtemptTimestamp = servcell_info.RRCEstabAtemptTimestamp;
    }
    if (servcell_info.NumberofRRCReEstabAttempts) {
        accu_servcell_info.RRCReEstabAtemptTimestamp = servcell_info.RRCReEstabAtemptTimestamp;
    }
    add_cell_avg_info(&servcell_info.RRCEstabLatency, &accu_servcell_info.RRCEstabLatency);
    add_cell_avg_info(&servcell_info.RRCReEstabLatency, &accu_servcell_info.RRCReEstabLatency);
    accu_servcell_info.max_ul_harq_transmissions += servcell_info.max_ul_harq_transmissions;
    accu_servcell_info.max_dl_harq_transmissions += servcell_info.max_dl_harq_transmissions;
    add_cell_count_info(&servcell_info.MACiBLERReceivedNackCount, &accu_servcell_info.MACiBLERReceivedNackCount);
    add_cell_count_info(&servcell_info.MACiBLERReceivedAckNackCount, &accu_servcell_info.MACiBLERReceivedAckNackCount);
    add_cell_count_info(&servcell_info.MACrBLERReceivedNackCount, &accu_servcell_info.MACrBLERReceivedNackCount);
    add_cell_count_info(&servcell_info.MACrBLERReceivedAckNackCount, &accu_servcell_info.MACrBLERReceivedAckNackCount);
    add_cell_count_info(&servcell_info.MACiBLERSentNackCount, &accu_servcell_info.MACiBLERSentNackCount);
    add_cell_count_info(&servcell_info.MACiBLERSentAckNackCount, &accu_servcell_info.MACiBLERSentAckNackCount);
    add_cell_count_info(&servcell_info.MACrBLERSentNackCount, &accu_servcell_info.MACrBLERSentNackCount);
    add_cell_count_info(&servcell_info.MACrBLERSentAckNackCount, &accu_servcell_info.MACrBLERSentAckNackCount);
    for (i = 0; i < MAX_HARQ_RETX_COUNT; i++) {
        add_cell_count_info(&servcell_info.HarqUlRetxOk[i], &accu_servcell_info.HarqUlRetxOk[i]);
    }
    add_cell_count_info(&servcell_info.TotalActiveTTIsReceived, &accu_servcell_info.TotalActiveTTIsReceived);
    add_cell_count_info(&servcell_info.TotalActiveTTIsSent, &accu_servcell_info.TotalActiveTTIsSent);
    add_cell_count_info(&servcell_info.TotalPRBsReceived, &accu_servcell_info.TotalPRBsReceived);
    add_cell_count_info(&servcell_info.TotalPRBsSent, &accu_servcell_info.TotalPRBsSent);
    add_cell_count_info(&servcell_info.RLFCount, &accu_servcell_info.RLFCount);
    add_cell_count_info(&servcell_info.HandoverCount, &accu_servcell_info.HandoverCount);
    add_cell_count_info(&servcell_info.HandoverAttempt, &accu_servcell_info.HandoverAttempt);
    for (i = 0; i < MAX_HARQ_ACK_COUNT; i++) {
        if (servcell_info.HarqUlRetx_valid[i] == 1) {
            accu_servcell_info.HarqUlRetx[i] += servcell_info.HarqUlRetx[i];
        }
        accu_servcell_info.HarqUlRetx_valid[i] = servcell_info.HarqUlRetx_valid[i];
        if (servcell_info.HarqDlAck_valid[i] == 1) {
            accu_servcell_info.HarqDlAck[i] += servcell_info.HarqDlAck[i];
        }
        accu_servcell_info.HarqDlAck_valid[i] = servcell_info.HarqDlAck_valid[i];
        if (servcell_info.HarqDlNdi_valid[i] == 1) {
            accu_servcell_info.HarqDlNdi[i] += servcell_info.HarqDlNdi[i];
        }
        accu_servcell_info.HarqDlNdi_valid[i] = servcell_info.HarqDlNdi_valid[i];
    }
    add_cell_count_info(&servcell_info.NumberofRRCEstabAttempts, &accu_servcell_info.NumberofRRCEstabAttempts);
    add_cell_count_info(&servcell_info.NumberofRRCEstabFailures, &accu_servcell_info.NumberofRRCEstabFailures);
    add_cell_count_info(&servcell_info.NumberofRRCReEstabAttempts, &accu_servcell_info.NumberofRRCReEstabAttempts);
    add_cell_count_info(&servcell_info.NumberofRRCReEstabFailures, &accu_servcell_info.NumberofRRCReEstabFailures);
    for (i = 0; i < MAX_FAILURE_CAUSES; i++) {
        add_cell_count_info(&servcell_info.FailureCounts[i], &accu_servcell_info.FailureCounts[i]);
    }
    accu_servcell_info.RxMcsIndex = servcell_info.RxMcsIndex;
    accu_servcell_info.RxMcsIndexValid = servcell_info.RxMcsIndexValid;
    accu_servcell_info.TxMcsIndex = servcell_info.TxMcsIndex;
    accu_servcell_info.TxMcsIndexValid = servcell_info.TxMcsIndexValid;
    for (i = 0; i < MCS_INDEX_NUM_ELEMENTS; i++) {
        accu_servcell_info.McsIndexCount[i] += servcell_info.McsIndexCount[i];
        accu_servcell_info.CrcFailCount[i] += servcell_info.CrcFailCount[i];
    }
#ifdef INCLUDE_LTE_CQI_MSG
    for (i = 0; i < CQI_NUM_ELEMENTS; i++) {
        accu_servcell_info.CqiStatsCount[i] += servcell_info.CqiStatsCount[i];
    }
    for (i = 0; i < PMI_NUM_ELEMENTS; i++) {
        accu_servcell_info.PmiStatsCount[i] += servcell_info.PmiStatsCount[i];
    }
#endif
    for (i = 0; i < INST_MAX_ANTENNA; i++) {
        accu_servcell_info.Rsrp[i] += servcell_info.Rsrp[i];
        accu_servcell_info.RsrpValid[i] += servcell_info.RsrpValid[i];
    }
}

/*
 cell_based_data_collection will determine if the cell ID has changed.
 If the cell ID has changed, then previous data collected will be summed
 with previous data for that cell and newly collected data will start
 from scratch.

 Parameters:
  pci

 Return:
  none
*/
void cell_based_data_collection(int pci)
{
    int cell_idx;

    /* server cell ID changed, save off previously collected data */
    /* first, find the cell ID in the list of IDs */
    cell_idx = get_cell_index_by_pci(pci);
    /* if an ID slot is found, then append it to the array */
    if (cell_idx != -1) {
        /* we have a slot in the array to save this cell's info before */
        /* collecting data from the new cell, just add to the data */

        /* get cell id from given pci */
        cell_info_by_id[cell_idx].CellID = get_cell_id_by_pci(pci);
        cell_info_by_id[cell_idx].PCI = pci;
        add_cell_avg_info(&servcell_info.SCellRSRP, &cell_info_by_id[cell_idx].SCellRSRP);
        add_cell_avg_info(&servcell_info.SCellRSRQ, &cell_info_by_id[cell_idx].SCellRSRQ);
        add_cell_avg_info(&servcell_info.SCellRSSNR, &cell_info_by_id[cell_idx].SCellRSSNR);
        add_cell_avg_info(&servcell_info.SCellRSSINR, &cell_info_by_id[cell_idx].SCellRSSINR);
        add_cell_count_info(&servcell_info.HandoverAttempt, &cell_info_by_id[cell_idx].HandoverAttempt);
        add_cell_count_info(&servcell_info.HandoverCount, &cell_info_by_id[cell_idx].HandoverCount);
        add_cell_count_info(&servcell_info.MACiBLERReceivedNackCount, &cell_info_by_id[cell_idx].MACiBLERReceivedNackCount);
        add_cell_count_info(&servcell_info.MACiBLERReceivedAckNackCount, &cell_info_by_id[cell_idx].MACiBLERReceivedAckNackCount);
        add_cell_count_info(&servcell_info.MACrBLERReceivedNackCount, &cell_info_by_id[cell_idx].MACrBLERReceivedNackCount);
        add_cell_count_info(&servcell_info.MACrBLERReceivedAckNackCount, &cell_info_by_id[cell_idx].MACrBLERReceivedAckNackCount);
        add_cell_count_info(&servcell_info.MACiBLERSentNackCount, &cell_info_by_id[cell_idx].MACiBLERSentNackCount);
        add_cell_count_info(&servcell_info.MACiBLERSentAckNackCount, &cell_info_by_id[cell_idx].MACiBLERSentAckNackCount);
        add_cell_count_info(&servcell_info.MACrBLERSentNackCount, &cell_info_by_id[cell_idx].MACrBLERSentNackCount);
        add_cell_count_info(&servcell_info.MACrBLERSentAckNackCount, &cell_info_by_id[cell_idx].MACrBLERSentAckNackCount);
        add_cell_count_info(&servcell_info.TotalActiveTTIsReceived, &cell_info_by_id[cell_idx].TotalActiveTTIsReceived);
        add_cell_count_info(&servcell_info.TotalActiveTTIsSent, &cell_info_by_id[cell_idx].TotalActiveTTIsSent);
        add_cell_count_info(&servcell_info.TotalPRBsReceived, &cell_info_by_id[cell_idx].TotalPRBsReceived);
        add_cell_count_info(&servcell_info.TotalPRBsSent, &cell_info_by_id[cell_idx].TotalPRBsSent);
        add_cell_distr_info(&servcell_info.PMIDistribution, &cell_info_by_id[cell_idx].PMIDistribution);
        add_cell_distr_info(&servcell_info.RIDistribution, &cell_info_by_id[cell_idx].RIDistribution);
        add_cell_distr_info(&servcell_info.ReceivedModulationDistribution, &cell_info_by_id[cell_idx].ReceivedModulationDistribution);
        add_cell_distr_info(&servcell_info.SendModulationDistribution, &cell_info_by_id[cell_idx].SendModulationDistribution);
        add_cell_distr_info(&servcell_info.PUSCHTrasmitPower, &cell_info_by_id[cell_idx].PUSCHTrasmitPower);
        add_cell_count_info(&servcell_info.NumberofRRCEstabAttempts, &cell_info_by_id[cell_idx].NumberofRRCEstabAttempts);
        add_cell_count_info(&servcell_info.NumberofRRCEstabFailures, &cell_info_by_id[cell_idx].NumberofRRCEstabFailures);
        add_cell_count_info(&servcell_info.NumberofRRCReEstabAttempts, &cell_info_by_id[cell_idx].NumberofRRCReEstabAttempts);
        add_cell_count_info(&servcell_info.NumberofRRCReEstabFailures, &cell_info_by_id[cell_idx].NumberofRRCReEstabFailures);
        add_cell_avg_info(&servcell_info.RRCEstabLatency, &cell_info_by_id[cell_idx].RRCEstabLatency);
        add_cell_avg_info(&servcell_info.RRCReEstabLatency, &cell_info_by_id[cell_idx].RRCReEstabLatency);
        /* sum up current servcell info to accu_servcell_info which holds
         * accumulated serving cell information during measuring period */
        sumup_servcell_info();
        g_servcell_info_acc_count++;

        /* now that the existing data is saved, clear the data from servcell_info */
        servcell_info_reset(&servcell_info, sizeof(servcell_info), time(NULL), get_monotonic_ms());
    } else {
        ERR("Could not find a data collection slot for new cell ");
    }
}
#endif

#ifdef INCLUDE_COMMON_MSG
/*
 Get max UE tx power.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_ml1_serv_meas_eval)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(5);
    declare_app_func_define_var_log(lte_ml1_sm_log_idle_serv_meas_eval_s);

#if (0)
    VERBOSE("earfcn = %d", log->earfcn);
    VERBOSE("phy_cell_id = %d", log->phy_cell_id);
    VERBOSE("serv_layer_prio = %d", log->serv_layer_prio);
    VERBOSE("measured_rsrp = %d", log->measured_rsrp);
    VERBOSE("avg_rsrp = %d", log->avg_rsrp);
    VERBOSE("measured_rsrq = %d", log->measured_rsrq);
    VERBOSE("avg_rsrq = %d", log->avg_rsrq);
    VERBOSE("measured_rssi = %d", log->measured_rssi);
    VERBOSE("q_rxlevmin = %d", log->q_rxlevmin);
    VERBOSE("p_max = %d", log->p_max);
    VERBOSE("max_ue_tx_pwr = %d", log->max_ue_tx_pwr);
    VERBOSE("s_rxlev = %d", log->s_rxlev);
    VERBOSE("num_drx_S_fail = %d", log->num_drx_S_fail);
    VERBOSE("s_intra_search = %d", log->s_intra_search);
    VERBOSE("s_non_intr_search = %d", log->s_non_intr_search);
    VERBOSE("meas_rules_updated = %d", log->meas_rules_updated);
    VERBOSE("meas_rules = %d", log->meas_rules);
#ifdef FEATURE_LTE_REL9
    VERBOSE("q_qualmin = %d", log->q_qualmin);
    VERBOSE("s_qual = %d", log->s_qual);
    VERBOSE("s_intra_search_q = %d", log->s_intra_search_q);
    VERBOSE("s_non_intra_search_q = %d", log->s_non_intra_search_q);
#endif
#endif

    rdb_enter_csection();
    {
        VERBOSE("len = %d", len);

        /* In units of dBm, range -30 to 33, 0x40 indicates not present 0x0 = -30, 0x1 = -29 to 0x3F = 33, 0x40 = NP */
        servcell_info.max_ue_tx_power = log->max_ue_tx_pwr - 30;
        VERBOSE("max_ue_tx_power = %d", servcell_info.max_ue_tx_power);

        rdb_leave_csection();
    };
}
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_TX_MCS_BLER_MSG
/*
 Get TX power.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_ml1_gm_tx_report)
{
    struct lte_ml1_gm_tx_report_t {
        uint32 version : 8;
        uint32 reserved : 16;
        uint32 number_of_records : 8;

        lte_ml1_gm_log_tx_report_u tx_report[0];
    } PACK();

    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    enum
    {
        response_invalid = 0,
        response_ack,
        response_nak,
    };

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(26, 33);
    declare_app_func_define_var_log(struct lte_ml1_gm_tx_report_t);

    uint32 record_size;
    char *p;

    /* Only supporting v26 and v33 of structure. */
    /* ------------ v33 -----------------------------------------------------------------*/
    if (declare_app_func_ver_match(33)) {
#ifdef SAVE_AVERAGED_DATA
        rdb_enter_csection();
        {
#endif
            VERBOSE("len = %d", len);
            VERBOSE("lte_ml1_gm_tx_report_t = %d", sizeof(struct lte_ml1_gm_tx_report_t));
            /* pusch record size is 48 bytes and pucch record size is 44 bytes in v33 */
            record_size = sizeof(lte_ml1_gm_log_pusch_tx_report_record_v33_s);
            VERBOSE("lte_ml1_gm_log_pusch_tx_report_record_size = %d", record_size);
            VERBOSE("number_of_records = %d", log->number_of_records);

            int i;

            int tx_power;
#ifdef DEBUG_VERBOSE
            int tx_power_raw;
#endif
            int mod_type;

#ifdef SAVE_AVERAGED_DATA
            // ML1 GM appears to only applies to PCC
            rrc_info.tx_power_carrier_idx_flags.PCC = 1;
            struct avg_t *PUSCHTxPower = &rrc_info.PUSCHTxPower;
            struct avg_t *PUCCHTxPower = &rrc_info.PUCCHTxPower;
            struct avg_t *PUSCHTxPowerForPCC = &rrc_info.PUSCHTxPowerByCell[0];
            struct avg_t *PUCCHTxPowerForPCC = &rrc_info.PUCCHTxPowerByCell[0];
            int pusch_tx_power_new;
            static int pusch_tx_power_last = -200; /* init with unrealistic tx power value */
#endif
            lte_ml1_gm_log_pucch_tx_report_record_v33_s *pucch_tx_report;
            lte_ml1_gm_log_pusch_tx_report_record_v33_s *pusch_tx_report;

            int retx;
            int retx_prev;
            uint8_t mcs_index;
            uint8_t pusch_tx_report_flag = 0;

#ifdef DEBUG_VERBOSE
            int printed_sch = 0;
            int printed_cch = 0;
#endif

            time_t now = get_monotonic_ms();

            int max_retx;
            struct subfn_t *subfn;

            /* ignore max_retx if less than 2 */
            max_retx = (servcell_info.max_ul_harq_transmissions < 2) ? 0 : servcell_info.max_ul_harq_transmissions - 1;

            int ack;
            p = (char *)&log->tx_report[0];

            for (i = 0; i < log->number_of_records; i++) {

                pucch_tx_report = (lte_ml1_gm_log_pucch_tx_report_record_v33_s *)p;
                pusch_tx_report = (lte_ml1_gm_log_pusch_tx_report_record_v33_s *)p;

                if (pusch_tx_report->chan_type) {
                    pusch_tx_report_flag = 1;

                    tx_power = (int)pusch_tx_report->total_tx_power;

                    mcs_index = pusch_tx_report->mcs_index;
                    VERBOSE("Tx MCS index[%d] = %d", i, mcs_index);
                    if (mcs_index >= 0 && mcs_index < 31) {
                        servcell_info.TxMcsIndex |= (0x1 << mcs_index);
                        servcell_info.TxMcsIndexValid = 1;
                    }

                    /* get current retx */
                    retx = pusch_tx_report->retx_index;
                    VERBOSE("retx_index = %d", retx);

                    /* detect ACK or NAK for first HARQ ID */
                    if (servcell_info.HarqUlRetx_valid[pusch_tx_report->harq_id]) {

                        /* invalidate ack */
                        ack = response_invalid;

                        /* get previous retx in harq processing id */
                        retx_prev = servcell_info.HarqUlRetx[pusch_tx_report->harq_id];

                        /* detect first ACK - if [ retx 0 ==> 0 ] */
                        if (retx_prev == 0 && retx == 0) {
                            VERBOSE("[BLER] iHARQ, ACK to 1st HARQ - sfn=%d,sub-fn=%d,harq=%d,retx=%d", pusch_tx_report->sfn, pusch_tx_report->sub_fn,
                                    pusch_tx_report->harq_id, retx);
                            ack = response_ack;
                        }
                        /* detect first NAK - if [ retx 0 ==> 1 ] */
                        else if (retx_prev == 0 && retx == 1) {
                            VERBOSE("[BLER] iHARQ, NAK to 1st HARQ - sfn=%d,sub-fn=%d,harq=%d,retx=%d", pusch_tx_report->sfn, pusch_tx_report->sub_fn,
                                    pusch_tx_report->harq_id, retx);
                            ack = response_nak;
                        }
                        /* detect last ACK/NAK - if [ retx 4 ==> 0 ] */
                        else if (max_retx && retx_prev == max_retx && retx == 0) {
                            subfn = get_next_subfn(servcell_info.HarqUlLast, pusch_tx_report->sfn, pusch_tx_report->sub_fn);
                            if (subfn->ack_valid && !__is_expired(subfn->timestamp, now, HARQ_TIME_WINDOW_MSEC)) {
                                VERBOSE("[BLER] rHARQ, got PHICH - phich=%s,sfn=%d,sub-fn=%d,harq=%d,retx=%d", subfn->ack ? "ACK" : "NAK",
                                        pusch_tx_report->sfn, pusch_tx_report->sub_fn, pusch_tx_report->harq_id, retx);
                                /* previously, PHICH is recieved. we can count ACK/NAK now */
                                ack = subfn->ack ? response_ack : response_nak;

                                /* invalidate used subfn */
                                subfn->ack_valid = 0;
                                subfn->tx_valid = 0;
                            } else {
                                VERBOSE("[BLER] rHARQ, wait for PHICH - sfn=%d,sub-fn=%d,harq=%d,retx=%d", pusch_tx_report->sfn,
                                        pusch_tx_report->sub_fn, pusch_tx_report->harq_id, retx);

                                /* PHICH will be recieved. we cannot count ACK/NAK now */
                                ack = response_invalid;

                                /* wait until PHICH is recieved */
                                subfn->ack_valid = 0;
                                subfn->tx_valid = 1;
                                subfn->timestamp = now;
                            }
                        }

                        /* increase statistics */
                        if (ack != response_invalid) {
                            if (retx_prev == 0) {
                                servcell_info.MACiBLERSentAckNackCount++;
                                if (ack == response_nak) {
                                    servcell_info.MACiBLERSentNackCount++;
                                } else {
                                    /* increment Re-Tx Ok count */
                                    servcell_info.HarqUlRetxOk[retx]++;
                                }

                                VERBOSE("[BLER] MACiBLERSent = %llu/%llu,harq=%d,retx=%d", servcell_info.MACiBLERSentNackCount,
                                        servcell_info.MACiBLERSentAckNackCount, pusch_tx_report->harq_id, retx);
                            } else if (max_retx && retx_prev == max_retx) {
                                servcell_info.MACrBLERSentAckNackCount++;
                                if (ack == response_nak) {
                                    servcell_info.MACrBLERSentNackCount++;
                                } else {
                                    /* increment Re-Tx Ok count */
                                    servcell_info.HarqUlRetxOk[retx]++;
                                }

                                VERBOSE("[BLER] MACrBLERSent = %llu/%llu,harq=%d,retx=%d", servcell_info.MACrBLERSentNackCount,
                                        servcell_info.MACrBLERSentAckNackCount, pusch_tx_report->harq_id, retx);
                            }
                        }
                    }

                    /* store current retx index into servcell structure */
                    servcell_info.HarqUlRetx[pusch_tx_report->harq_id] = pusch_tx_report->retx_index;
                    servcell_info.HarqUlRetx_valid[pusch_tx_report->harq_id] = 1;

                    /* get pusch modulation */
                    mod_type = pusch_tx_report->mod_type;
                    /* 0 -> BPSK, 1 -> QPSK, 2 -> 16 QAM, 3 -> 64 QAM - BPSK is not used by AT&T requirement */
                    if ((0 < mod_type) && (mod_type < 4))
                        distr_feed(&servcell_info.SendModulationDistribution, mod_type - 1);
                    else
                        ERR("invalid modulation type : %d", mod_type);

                } else {
                    tx_power = (int)pucch_tx_report->total_tx_power;
                }

#ifdef DEBUG_VERBOSE
                tx_power_raw = tx_power;
#endif
                /* apply negative */
                if (tx_power & 0x80)
                    tx_power |= 0xffffff80;

                if (pusch_tx_report->chan_type) {
#ifdef SAVE_AVERAGED_DATA
                    /* feed 480 moving average tx power */
                    avg_feed(&total_pusch_tx_power, tx_power);
                    /* feed rrc tx power */
                    avg_feed(PUSCHTxPower, tx_power);
                    avg_feed(PUSCHTxPowerForPCC, tx_power);

                    /* feed max and normal tx power distribution to servcell info */
                    distr_feed(&servcell_info.PUSCHTrasmitPower, (tx_power >= servcell_info.max_ue_tx_power) ? 0 : 1);
                    VERBOSE("max_ue_tx_power = %d", servcell_info.max_ue_tx_power);
#endif

                    /* set pusch tx - set valid flag after setting as no sync method is used */
                    int_stats.pusch_tx_power = tx_power;
                    int_stats.pusch_tx_power_valid = 1;
#ifdef DEBUG_VERBOSE
                    if (!printed_sch)
                        DEBUG("pusch tx power #%d = %d, 0x%x", i, tx_power, tx_power_raw);
                    printed_sch = 1;
#else
                VERBOSE("pusch tx power #%d = %d, 0x%x", i, tx_power, tx_power_raw);
#endif
                    VERBOSE("afc_rx_freq_error #%d = %d Hz", i, pusch_tx_report->afc_rx_freq_error);
                } else {

#ifdef SAVE_AVERAGED_DATA
                    avg_feed(PUCCHTxPower, tx_power);
                    avg_feed(PUCCHTxPowerForPCC, tx_power);
#endif

#ifdef DEBUG_VERBOSE
                    if (!printed_cch)
                        DEBUG("pucch tx power #%d = %d, 0x%x", i, tx_power, tx_power_raw);
                    printed_cch = 1;
#else
                VERBOSE("pucch tx power #%d = %d, 0x%x", i, tx_power, tx_power_raw);
#endif
                    VERBOSE("afc_rx_freq_error #%d = %d Hz", i, pucch_tx_report->afc_rx_freq_error);
                }

                p += record_size;
            }

#ifdef SAVE_AVERAGED_DATA
            /* update rdb if needed */
            pusch_tx_power_new = __round(total_pusch_tx_power.avg);
            if (pusch_tx_power_last != pusch_tx_power_new) {
                _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_pusch_transmission_status.total_pusch_txpower", pusch_tx_power_new);
                pusch_tx_power_last = pusch_tx_power_new;
            }
#endif

            if (pusch_tx_report_flag) {
                VERBOSE("servcell_info.TxMcsIndex = 0x%08x", servcell_info.TxMcsIndex);
                VERBOSE("int_stats.pusch_tx_power = %d", int_stats.pusch_tx_power);
                for (i = 0; i < MAX_HARQ_RETX_COUNT; i++)
                    VERBOSE("servcell_info.HarqUlRetxOk[%d] = %lld", i, servcell_info.HarqUlRetxOk[i]);
            }

#ifdef SAVE_AVERAGED_DATA
            rdb_leave_csection();
        }
#endif
        /* ------------ v26 -----------------------------------------------------------------*/
    } else if (declare_app_func_ver_match(26)) {
        rdb_enter_csection();
        {
            VERBOSE("len = %d", len);
            VERBOSE("lte_ml1_gm_tx_report_t = %d", sizeof(struct lte_ml1_gm_tx_report_t));
            record_size = sizeof(lte_ml1_gm_log_pusch_tx_report_record_v26_s);
            VERBOSE("lte_ml1_gm_log_pusch_tx_report_record_size = %d", record_size);
            VERBOSE("number_of_records = %d", log->number_of_records);

            int i;

            int tx_power;
            int mod_type;

            struct avg_t *PUSCHTxPower = &rrc_info.PUSCHTxPower;
            struct avg_t *PUCCHTxPower = &rrc_info.PUCCHTxPower;
            lte_ml1_gm_log_pucch_tx_report_record_v26_s *pucch_tx_report;
            lte_ml1_gm_log_pusch_tx_report_record_v26_s *pusch_tx_report;

            int pusch_tx_power_new;
            static int pusch_tx_power_last = -200; /* init with unrealistic tx power value */
            int retx;
            int retx_prev;
            uint8_t mcs_index;
            uint8_t pusch_tx_report_flag = 0;

#ifdef DEBUG_VERBOSE
            int printed_sch = 0;
            int printed_cch = 0;
#endif

            time_t now = get_monotonic_ms();

            int max_retx = servcell_info.max_ul_harq_transmissions;
            struct subfn_t *subfn;

            /* ignore max_retx if less than 2 */
            max_retx = (servcell_info.max_ul_harq_transmissions < 2) ? 0 : servcell_info.max_ul_harq_transmissions - 1;

            int ack;

            p = (char *)&log->tx_report[0];

            for (i = 0; i < log->number_of_records; i++) {

                pucch_tx_report = (lte_ml1_gm_log_pucch_tx_report_record_v26_s *)p;
                pusch_tx_report = (lte_ml1_gm_log_pusch_tx_report_record_v26_s *)p;

                if (pusch_tx_report->chan_type) {
                    pusch_tx_report_flag = 1;

                    tx_power = (int)pusch_tx_report->total_tx_power;

                    mcs_index = pusch_tx_report->mcs_index;

                    VERBOSE("Tx MCS index[%d] = %d", i, mcs_index);
                    if (mcs_index >= 0 && mcs_index < 31) {
                        servcell_info.TxMcsIndex |= (0x1 << mcs_index);
                        servcell_info.TxMcsIndexValid = 1;
                    }

                    /* get current retx */
                    retx = pusch_tx_report->retx_index;

                    /* detect ACK or NAK for first HARQ ID */
                    if (servcell_info.HarqUlRetx_valid[pusch_tx_report->harq_id]) {

                        /* invalidate ack */
                        ack = response_invalid;

                        /* get previous retx in harq processing id */
                        retx_prev = servcell_info.HarqUlRetx[pusch_tx_report->harq_id];

                        /* detect first ACK - if [ retx 0 ==> 0 ] */
                        if (retx_prev == 0 && retx == 0) {
                            VERBOSE("[BLER] iHARQ, ACK to 1st HARQ - sfn=%d,sub-fn=%d,harq=%d,retx=%d", pusch_tx_report->sfn, pusch_tx_report->sub_fn,
                                    pusch_tx_report->harq_id, retx);
                            ack = response_ack;
                        }
                        /* detect first NAK - if [ retx 0 ==> 1 ] */
                        else if (retx_prev == 0 && retx == 1) {
                            VERBOSE("[BLER] iHARQ, NAK to 1st HARQ - sfn=%d,sub-fn=%d,harq=%d,retx=%d", pusch_tx_report->sfn, pusch_tx_report->sub_fn,
                                    pusch_tx_report->harq_id, retx);
                            ack = response_nak;
                        }
                        /* detect last ACK/NAK - if [ retx 4 ==> 0 ] */
                        else if (max_retx && retx_prev == max_retx && retx == 0) {
                            subfn = get_next_subfn(servcell_info.HarqUlLast, pusch_tx_report->sfn, pusch_tx_report->sub_fn);
                            if (subfn->ack_valid && !__is_expired(subfn->timestamp, now, HARQ_TIME_WINDOW_MSEC)) {
                                VERBOSE("[BLER] rHARQ, got PHICH - phich=%s,sfn=%d,sub-fn=%d,harq=%d,retx=%d", subfn->ack ? "ACK" : "NAK",
                                        pusch_tx_report->sfn, pusch_tx_report->sub_fn, pusch_tx_report->harq_id, retx);
                                /* previously, PHICH is recieved. we can count ACK/NAK now */
                                ack = subfn->ack ? response_ack : response_nak;

                                /* invalidate used subfn */
                                subfn->ack_valid = 0;
                                subfn->tx_valid = 0;
                            } else {
                                VERBOSE("[BLER] rHARQ, wait for PHICH - sfn=%d,sub-fn=%d,harq=%d,retx=%d", pusch_tx_report->sfn,
                                        pusch_tx_report->sub_fn, pusch_tx_report->harq_id, retx);

                                /* PHICH will be recieved. we cannot count ACK/NAK now */
                                ack = response_invalid;

                                /* wait until PHICH is recieved */
                                subfn->ack_valid = 0;
                                subfn->tx_valid = 1;
                                subfn->timestamp = now;
                            }
                        }

                        /* increase statistics */
                        if (ack != response_invalid) {
                            if (retx_prev == 0) {
                                servcell_info.MACiBLERSentAckNackCount++;
                                if (ack == response_nak)
                                    servcell_info.MACiBLERSentNackCount++;

                                VERBOSE("[BLER] MACiBLERSent = %llu/%llu,harq=%d,retx=%d", servcell_info.MACiBLERSentNackCount,
                                        servcell_info.MACiBLERSentAckNackCount, pusch_tx_report->harq_id, retx);
                            } else if (max_retx && retx_prev == max_retx) {
                                servcell_info.MACrBLERSentAckNackCount++;
                                if (ack == response_nak)
                                    servcell_info.MACrBLERSentNackCount++;

                                VERBOSE("[BLER] MACrBLERSent = %llu/%llu,harq=%d,retx=%d", servcell_info.MACrBLERSentNackCount,
                                        servcell_info.MACrBLERSentAckNackCount, pusch_tx_report->harq_id, retx);
                            }
                        }
                    }

                    /* store current retx index into servcell structure */
                    servcell_info.HarqUlRetx[pusch_tx_report->harq_id] = pusch_tx_report->retx_index;
                    servcell_info.HarqUlRetx_valid[pusch_tx_report->harq_id] = 1;

                    /* get pusch modulation */
                    mod_type = pusch_tx_report->mod_type;
                    /* 0 -> BPSK, 1 -> QPSK, 2 -> 16 QAM, 3 -> 64 QAM - BPSK is not used by AT&T requirement */
                    if ((0 < mod_type) && (mod_type < 4))
                        distr_feed(&servcell_info.SendModulationDistribution, mod_type - 1);

                } else {
                    tx_power = (int)pucch_tx_report->total_tx_power;
                }

                /* apply negative */
                if (tx_power & 0x80)
                    tx_power |= 0xffffff80;

                if (pusch_tx_report->chan_type) {
                    /* feed 480 moving average tx power */
                    avg_feed(&total_pusch_tx_power, tx_power);
                    /* feed rrc tx power */
                    avg_feed(PUSCHTxPower, tx_power);

                    /* feed max and normal tx power distribution to servcell info */
                    distr_feed(&servcell_info.PUSCHTrasmitPower, (tx_power >= servcell_info.max_ue_tx_power) ? 0 : 1);
                    VERBOSE("max_ue_tx_power = %d", servcell_info.max_ue_tx_power);

                    VERBOSE("pusch tx power #%d = %d", i, tx_power);
                    VERBOSE("afc_rx_freq_error #%d = %d Hz", i, pusch_tx_report->afc_rx_freq_error);

#ifdef DEBUG_VERBOSE
                    if (!printed_sch)
                        DEBUG("pusch tx power #%d = %d", i, tx_power);
                    printed_sch = 1;
#endif
                } else {

                    avg_feed(PUCCHTxPower, tx_power);

                    VERBOSE("pucch tx power #%d = %d", i, tx_power);
                    VERBOSE("afc_rx_freq_error #%d = %d Hz", i, pucch_tx_report->afc_rx_freq_error);

#ifdef DEBUG_VERBOSE
                    if (!printed_cch)
                        DEBUG("pucch tx power #%d = %d", i, tx_power);
                    printed_cch = 1;
#endif
                }

                p += record_size;
            }

            /* update rdb if needed */
            pusch_tx_power_new = __round(total_pusch_tx_power.avg);
            if (pusch_tx_power_last != pusch_tx_power_new) {
                _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_pusch_transmission_status.total_pusch_txpower", pusch_tx_power_new);
                pusch_tx_power_last = pusch_tx_power_new;
            }

            if (pusch_tx_report_flag) {
                VERBOSE("servcell_info.TxMcsIndex = 0x%08x", servcell_info.TxMcsIndex);
            }

            rdb_leave_csection();
        }
    }
}
#endif /* #ifdef INCLUDE_LTE_TX_MCS_BLER_MSG */

declare_app_func(lte_ml1_pusch_power_control)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(LTE_ML1_LOG_PUSCH_POWER_CONTROL_RECORD_VERSION);
    declare_app_func_define_var_log(lte_ml1_log_pusch_power_control_records_s);

    /* Update path loss. Equipment is stationary, so there's no need to average all the records. */
    avg_feed(&servcell_info.PathLoss, log->record[0].dl_path_loss);
}

#ifdef INCLUDE_EXTRA_MSG
/*
 Skeleton function for common LTE log handler.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_common)
{
    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(int);

    VERBOSE("version = %d", *log);
}

/*
 Get TX power.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_ll1_pusch_tx_report)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(101, 121, 124);
    declare_app_func_define_var_log(lte_LL1_log_ul_pusch_tx_report_ind_struct);

    int pusch_tx_power;
    unsigned int carrier_idx;

#ifdef SAVE_AVERAGED_DATA
    static int pusch_tx_power_last = -200; /* invalid tx power as the first value */
    int pusch_tx_power_new;
#endif

    int i;

    VERBOSE("number_of_records = %d", log->number_of_records);
    VERBOSE("cell id = %d", log->serving_cell_id);

#if !defined SAVE_AVERAGED_DATA
    /* Only supporting v101, v121, and v124 of structure. */
    /* ------------ v101 -----------------------------------------------------------------*/
    if (declare_app_func_ver_match(101)) {
        pusch_tx_power = log->records_v101[0].pusch_transmit_power_dbm;
        /* ------------ v121 -----------------------------------------------------------------*/
    } else if (declare_app_func_ver_match(121)) {
        pusch_tx_power = log->records_v121[0].pusch_transmit_power_dbm;
        /* ------------ v124 -----------------------------------------------------------------*/
    } else if (declare_app_func_ver_match(124)) {
        pusch_tx_power = log->records_v124[0].pusch_transmit_power_dbm;
    }

    /* apply negative */
    if (pusch_tx_power & 0x40)
        pusch_tx_power |= 0xffffffc80;
    VERBOSE("pusch tx power = %d", pusch_tx_power);
    /* set pusch tx - set valid flag after setting as no sync method is used */
    int_stats.pusch_tx_power = pusch_tx_power;
    int_stats.pusch_tx_power_valid = 1;
#else
    rdb_enter_csection();
    {
        struct avg_t *PUSCHTxPower = &rrc_info.PUSCHTxPower;
        struct avg_t *PUSCHTxPowerByCell;

        rrc_info.PCI = log->serving_cell_id;
        for (i = 0; i < log->number_of_records && i < LTE_LL1_LOG_UL_NUMBER_OF_PUSCH_RECORDS_FIXED; i++) {
            /* Only supporting v101, v121 and v124 of structure. */
            /* ------------ v101 -----------------------------------------------------------------*/
            if (declare_app_func_ver_match(101)) {
                pusch_tx_power = log->records_v101[i].pusch_transmit_power_dbm;
                carrier_idx = log->records_v101[i].ul_carrier_idx;
            }
            /* ------------ v121 -----------------------------------------------------------------*/
            else if (declare_app_func_ver_match(121)) {
                pusch_tx_power = log->records_v121[i].pusch_transmit_power_dbm;
                carrier_idx = log->records_v121[i].ul_carrier_idx;
            }
            /* ------------ v124 -----------------------------------------------------------------*/
            else if (declare_app_func_ver_match(124)) {
                pusch_tx_power = log->records_v124[i].pusch_transmit_power_dbm;
                carrier_idx = log->records_v124[i].ul_carrier_idx;
            }

            /* apply negative */
            if (pusch_tx_power & 0x40)
                pusch_tx_power |= 0xffffffc80;

            switch (carrier_idx) {
                case 0:
                    rrc_info.tx_power_carrier_idx_flags.PCC = 1;
                    PUSCHTxPowerByCell = &rrc_info.PUSCHTxPowerByCell[carrier_idx];
                    break;
                case 1:
                    rrc_info.tx_power_carrier_idx_flags.SCC = 1;
                    PUSCHTxPowerByCell = &rrc_info.PUSCHTxPowerByCell[carrier_idx];
                    break;
            }

            /* feed 480 moving average tx power */
            avg_feed(&total_pusch_tx_power, pusch_tx_power);
            /* feed rrc tx power */
            avg_feed(PUSCHTxPower, pusch_tx_power);
            avg_feed(PUSCHTxPowerByCell, pusch_tx_power);

            VERBOSE("pusch tx power #%d = %d, cnt=%llu,avg=%.2Lf", i, pusch_tx_power, PUSCHTxPower->c, PUSCHTxPower->avg);
            VERBOSE("record#%d: (per cell) pusch_tx_power=%d - carrier_idx=%u: cnt=%llu, avg=%.2Lf", i, carrier_idx, pusch_tx_power,
                    PUSCHTxPowerByCell->c, PUSCHTxPowerByCell->avg);
            VERBOSE("pusch tx power = %d", pusch_tx_power);
        }

        /* update rdb if needed */
        if (total_pusch_tx_power.stat_valid) {
            pusch_tx_power_new = __round(total_pusch_tx_power.avg);
            if (pusch_tx_power_last != pusch_tx_power_new)
                _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_pusch_transmission_status.total_pusch_txpower", pusch_tx_power_new);
            pusch_tx_power_last = pusch_tx_power_new;

            /* feed max and normal tx power distribution to servcell info */
            distr_feed(&servcell_info.PUSCHTrasmitPower, (pusch_tx_power_new >= 23) ? 0 : 1);
            VERBOSE("pusch tx power = %d", pusch_tx_power_new);
        }
        rdb_leave_csection();
    }
#endif
}

/*
 Get TX power.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_ll1_pucch_tx_report)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(101, 121);
    declare_app_func_define_var_log(lte_LL1_log_ul_pucch_tx_report_ind_struct);

    int pucch_tx_power;
    unsigned int carrier_idx;

#if !defined SAVE_AVERAGED_DATA

    if (declare_app_func_ver_match(101)) {
        pucch_tx_power = log->records_v101[0].pucch_transmit_power_dbm;
        carrier_idx = log->records_v101[0].ul_carrier_idx;
    }
    /* ------------ v121 -----------------------------------------------------------------*/
    else if (declare_app_func_ver_match(121)) {
        pucch_tx_power = log->records_v121[0].pucch_transmit_power_dbm;
        carrier_idx = log->records_v121[0].ul_carrier_idx;
    }

    /* apply negative */
    if (pucch_tx_power & 0x40)
        pucch_tx_power |= 0xffffffc80;
    VERBOSE("pucch tx power = %d", pucch_tx_power);
#else
    int i;

    struct avg_t *PUCCHTxPower = &rrc_info.PUCCHTxPower;
    struct avg_t *PUCCHTxPowerByCell;

    VERBOSE("number_of_records = %d", log->number_of_records);
    for (i = 0; i < log->number_of_records && i < LTE_LL1_LOG_UL_NUMBER_OF_PUCCH_RECORDS_FIXED; i++) {
        /* ------------ v101 -----------------------------------------------------------------*/
        if (declare_app_func_ver_match(101)) {
            pucch_tx_power = log->records_v101[i].pucch_transmit_power_dbm;
            carrier_idx = log->records_v101[i].ul_carrier_idx;
        }
        /* ------------ v121 -----------------------------------------------------------------*/
        else if (declare_app_func_ver_match(121)) {
            pucch_tx_power = log->records_v121[i].pucch_transmit_power_dbm;
            carrier_idx = log->records_v121[i].ul_carrier_idx;
        }

        /* apply negative */
        if (pucch_tx_power & 0x40)
            pucch_tx_power |= 0xffffffc80;

        PUCCHTxPowerByCell = &rrc_info.PUCCHTxPowerByCell[carrier_idx];
        avg_feed(PUCCHTxPowerByCell, pucch_tx_power);
        avg_feed(PUCCHTxPower, pucch_tx_power);

        VERBOSE("pucch tx power #%d = %d, cnt=%llu,avg=%.2Lf", i, pucch_tx_power, PUCCHTxPower->c, PUCCHTxPower->avg);
        VERBOSE("(per cell) pucch tx power #%d [%u] = %d, cnt=%llu, avg=%.2Lf", i, carrier_idx, pucch_tx_power, PUCCHTxPowerByCell->c,
                PUCCHTxPowerByCell->avg);
        DEBUG("ul_carrier_idx#%d pucch tx power = %d", i, pucch_tx_power);
    }
#endif
}

/*
 Get serving cell information.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_ml1_serving_cell_information_log_packet)
{
    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(lte_ml1_sm_log_serv_cell_info_pkt_s);

    VERBOSE("version = %d", log->version);
    DEBUG("dl bandwidth = %d", log->dl_bandwidth);
}

#endif /* INCLUDE_EXTRA_MSG */

#ifdef INCLUDE_LTE_EMM_ESM_MSG
/*
 Capture incoming NAS EMM OTA messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_nas_emm_ota_in_msg)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(lte_nas_emm_plain_in_ota_msg_type);

    DEBUG("got EMM OTA incoming diag message");

    unsigned long long ms = get_ms_from_ts(ts);
    int payload_len = hdr->len - sizeof(*hdr);
    lte_nas_parser_perform_nas_ota_message(&ms, nas_message_type_emm, nas_message_dir_in, log->in_ota_raw_data, payload_len);
}

/*
 Capture outgoing NAS EMM OTA messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_nas_emm_ota_out_msg)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(lte_nas_emm_plain_out_ota_msg_type);

    DEBUG("got EMM OTA outgoing diag message");

    unsigned long long ms = get_ms_from_ts(ts);
    int payload_len = hdr->len - sizeof(*hdr);
    lte_nas_parser_perform_nas_ota_message(&ms, nas_message_type_emm, nas_message_dir_out, log->in_ota_raw_data, payload_len);
}

/*
 Capture incoming NAS ESM OTA messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_nas_esm_ota_in_msg_log)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(lte_nas_esm_plain_in_ota_msg_T);

    DEBUG("got ESM OTA incoming diag message");

    unsigned long long ms = get_ms_from_ts(ts);
    int payload_len = hdr->len - sizeof(*hdr);
    lte_nas_parser_perform_nas_ota_message(&ms, nas_message_type_esm, nas_message_dir_in, log->in_ota_raw_data, payload_len);
}

/*
 Capture outgoing NAS ESM OTA messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_nas_esm_ota_out_msg_log)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(lte_nas_esm_plain_out_ota_msg_T);

    DEBUG("got ESM OTA outgoing diag message");

    unsigned long long ms = get_ms_from_ts(ts);
    int payload_len = hdr->len - sizeof(*hdr);
    lte_nas_parser_perform_nas_ota_message(&ms, nas_message_type_esm, nas_message_dir_out, log->in_ota_raw_data, payload_len);
}
#endif /* INCLUDE_LTE_EMM_ESM_MSG */

#ifdef INCLUDE_COMMON_MSG
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
/* 6.22 LTE ML1 Connected Mode Measurement Configuration in ICD 80-VP457-1 YYP */
declare_app_func(lte_ll1_srch_serving_cell_ttl_result_int)
{
    /* structure for mdm9x50 MPSS.AT.2.0.c3-00085-9655_GEN_PACK-1.113119.1 */
    typedef PACK(struct)
    {
        uint32 frame_number : 10;
        uint32 subframe_number : 4;
        uint32 ttl_mode : 2;
        uint32 total_timing_adjustment : 16;
        uint32 inst_timing_adjustment : 16;
        uint32 inner_loop_gain_data : 16; /* Inner Loop Gain = Inner Loop Gain Data" / 256.f */
        uint32 reserved : 32;
        uint32 com_ts_cell_1 : 16;
        uint32 com_ts_cell_2 : 16;
        uint32 com_ts_cell_3 : 16;
        uint32 com_ts_cell_4 : 16;
        uint32 reserved1 : 16;
        uint32 reserved2 : 16;
        uint32 reserved3 : 16;
        uint32 reserved4 : 16;
    }
    lte_servingcellttl_results_v45_reco;

    typedef PACK(struct)
    {
        uint32 serving_cell_id : 9;
        uint32 reserved1 : 4;
        uint32 number_of_records : 5;
        uint32 reserved2 : 2;
        uint32 system_frame_number : 10;
        uint16 reserved3[4];
        uint32 subframe_number : 4;
        uint32 reserved4 : 2;
        uint32 reserved5 : 16;
        lte_servingcellttl_results_v45_reco records[0];
    }
    lte_servingcellttlresults_v45;

    typedef PACK(struct)
    {
        uint32 version : 8; /* v45 */
        lte_servingcellttlresults_v45 versions;
    }
    lte_ll1_log_srch_serving_cell_ttl_results_ind_msg_struct;

    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(lte_ll1_log_srch_serving_cell_ttl_results_ind_msg_struct);

    int rec_no;
    lte_servingcellttl_results_v45_reco *rec;
    int i;

#ifdef DEBUG_VERBOSE
    int rec_off;
    int diag_packet_len;
#endif

    declare_app_func_require_ver(45);

    VERBOSE("offset records = %d", (size_t)&log->versions.records - (size_t)log);
    VERBOSE("records size = %d", sizeof(*log->versions.records));

    /*
     *
     * NBN data model requires RX gain.
     *
     *  The data model requires RX gain with no filter and with no physical channel information. Additionally, the sampling
     * frequency is much less frequent than sub-frame frequency. In result, RX gain will be sampled from any random sub-frame
     * that has an unknown physical channel allocated.
     *
     * This implementation follows NBN v2 and it is unclear how this RX gain helps statistics.
     *
     * TODO: revisit the requirement.
     *
     */

    /* use packet length to get total record number. number_of_records is not matching to document */
    rec_no = (hdr->len - sizeof(*hdr) - sizeof(*log)) / sizeof(*log->versions.records);
#ifdef DEBUG_VERBOSE
    diag_packet_len = hdr->len;
#endif
    rec = &log->versions.records[rec_no - 1];

/* get content length */
#ifdef DEBUG_VERBOSE
    rec_off = (size_t)rec - (size_t)hdr;
#endif
    VERBOSE("rec_no=%d,diag_plen=%d,rec_off=%d", rec_no, diag_packet_len, rec_off);

    for (i = 0; i < rec_no; i++) {
        rec = &log->versions.records[i];

        /* 6.22 ICD 80-VP457-1 YYP Inner Loop Gain = Inner Loop Gain Data" / 256.f */
        int_stats.inner_loop_gain = rec->inner_loop_gain_data / 256;
        VERBOSE("inner loop gain = #%d,%d,%.2f", i, rec->inner_loop_gain_data, int_stats.inner_loop_gain);
    }
}
#endif

void process_system_information_block(struct SystemInformation_r8_IEs__sib_TypeAndInfo__Member *sib)
{
    char var[RDB_MAX_NAME_LEN]; // used internally by rdb macros
    int var_len = sizeof(var);  // used internally by rdb macros

    switch (sib->present) {
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_NOTHING:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib3:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib4:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib6:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib7:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib8:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib9:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib10:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib11:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib12_v920:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib13_v920:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib14_v1130:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib15_v1130:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib16_v1130:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib17_v1250:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib18_v1250:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib19_v1250:
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib20_v1310:
            break;

        /**********************************************************************
         * SIB 2
         **********************************************************************/
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib2: {
            SystemInformationBlockType2_t *sib2 = &sib->choice.sib2;
            PDSCH_ConfigCommon_t *pdsch_ConfigCommon = &sib2->radioResourceConfigCommon.pdsch_ConfigCommon;
            long rspower = pdsch_ConfigCommon->referenceSignalPower;

            _rdb_prefix_call(_rdb_set_int, "system_network_status.RSTxPower", rspower);
            break;
        } // sib2

        /**********************************************************************
         * SIB 5
         **********************************************************************/
        case SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib5: {
            SystemInformationBlockType5_t *sib5 = &sib->choice.sib5;

            char *rdb_val = (char *)malloc(RDB_MAX_VAL_LEN);

            if (rdb_val) {
                struct InterFreqCarrierFreqInfo *freqInfo;
                int i;

                // Convert enum to bandwidth:
                const uint8 bw[] = {
                    [AllowedMeasBandwidth_mbw6] = 6,   [AllowedMeasBandwidth_mbw15] = 15, [AllowedMeasBandwidth_mbw25] = 25,
                    [AllowedMeasBandwidth_mbw50] = 50, [AllowedMeasBandwidth_mbw75] = 75, [AllowedMeasBandwidth_mbw100] = 100,
                };

                int len = RDB_MAX_VAL_LEN; // available string len
                char *s = rdb_val;         // working position in string

                rdb_val[0] = '\0'; // null terminate the allocated string

                /******************************************************************
                 * Step through the InterFreqCarrierFreqInfo list, extracting
                 * each Frequency and Bandwidth pair.
                 ******************************************************************/
                for_each_asn1c_array(i, &sib5->interFreqCarrierFreqList, freqInfo)
                {
                    int l;  // Number chars written
                    long b; // The bandwidth (as an enum)

                    if (0 == asn_INTEGER2long(&freqInfo->allowedMeasBandwidth, &b) // must convert BW enum from ASN.1 format!
                        && b >= 0                                                  // && make sure the result fits the bw[] table!
                        && b < ARRAY_LEN(bw)) {
                        l = snprintf(s, len, "%ld:%d,", freqInfo->dl_CarrierFreq, bw[b]);

                        if (l < 0) { // an error occurred
                            ERR("snprintf() error encountered");
                        } else if (l < len) { // (this is the normal path)
                            s += l;
                            len -= l;
                        } else { // no more room in the string, so quit the loop
                            break;
                        }
                    }
                } // for each

                /******************************************************************
                 * Write the frequency/bandwidth data into an RDB variable
                 * (wwan.0.cell_measurement.freq_to_bandwidth). This will be used
                 * in wmmd (qmi_nas.lua) to obtain the bandwidths for the neighbor
                 * cells (see lua function: getBandwidth()).
                 *
                 * Sample RDB value (freq:bw,...):
                 *
                 *    9820:50,2150:15,3350:75,  (the trailing comma is intentional)
                 *
                 ******************************************************************/
                _rdb_prefix_call(_rdb_set_str, "cell_measurement.freq_to_bandwidth", rdb_val);

                free(rdb_val);
            }

            break;
        } // sib5
    }     // switch
}

/*
 Capture RRC OTA messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_rrc_ota_msg)
{
    declare_app_func_define_vars_for_log();
    declare_app_func_require_ver(13, 15, 26);

    int i;
    void *msg;   /* RRC PDU */
    int msg_len; /* RRC PDU length */
    int pdu_num;

    int log_freq;
    int log_phy_cell_id;

    int bcchDlSchPduNum;

    switch (log_ver) {
        case 15:
        case 13: {
            declare_app_func_define_var_log(lte_rrc_log_ota_msg_s_v13);

            /* pdu type number */
            pdu_num = log->pdu_num;
            /* get RRC PDU legth */
            msg_len = log->encoded_msg_len;
            /* get RRC PDU body */
            msg = &log->encoded_msg_first_byte;
            /* in v13, BCCH_DL_SCH is 2 */
            bcchDlSchPduNum = 2;

            log_freq = log->freq;
            log_phy_cell_id = log->phy_cell_id;

            VERBOSE("[RRC-OTA] packet version = %d", log->log_packet_ver);
            VERBOSE("[RRC-OTA] rrc rel = %d", log->rrc_rel);
            VERBOSE("[RRC-OTA] rrc ver = %d.%d", (log->rrc_ver >> 4) & 0x0f, log->rrc_ver & 0x0f);
            VERBOSE("[RRC-OTA] rb id = %d", log->rb_id);
            VERBOSE("[RRC-OTA] phy_cell_id = %d", log->phy_cell_id);
            VERBOSE("[RRC-OTA] freq = %u", log->freq);
            VERBOSE("[RRC-OTA] pdu num = %d", pdu_num);
            VERBOSE("[RRC-OTA] len = %d", log->encoded_msg_len);
            break;
        }

        default: {
            declare_app_func_define_var_log(lte_rrc_log_ota_msg_s_v26);

            /* pdu type number */
            pdu_num = log->pdu_num;
            /* get RRC PDU legth */
            msg_len = log->encoded_msg_len;
            /* get RRC PDU body */
            msg = &log->encoded_msg_first_byte;
            /* in v26, BCCH_DL_SCH is 3 */
            bcchDlSchPduNum = 3;

            log_freq = log->freq;
            log_phy_cell_id = log->phy_cell_id;

            VERBOSE("[RRC-OTA] packet version = %d", log->log_packet_ver);
            VERBOSE("[RRC-OTA] rrc rel = %d", log->rrc_rel);
            VERBOSE("[RRC-OTA] rrc ver = %d.%d", (log->rrc_ver >> 4) & 0x0f, log->rrc_ver & 0x0f);
            VERBOSE("[RRC-OTA] nr rrc rel = %d", log->nr_rrc_rel);
            VERBOSE("[RRC-OTA] nr rrc ver = %d.%d", (log->nr_rrc_ver >> 4) & 0x0f, log->nr_rrc_ver & 0x0f);
            VERBOSE("[RRC-OTA] rb id = %d", log->rb_id);
            VERBOSE("[RRC-OTA] phy_cell_id = %d", log_phy_cell_id);
            VERBOSE("[RRC-OTA] freq = %u", log_freq);
            VERBOSE("[RRC-OTA] pdu num = %d", pdu_num);
            VERBOSE("[RRC-OTA] len = %d", log->encoded_msg_len);
            break;
        }
    }

    asn_dec_rval_t rval;
    asn_codec_ctx_t *opt_codec_ctx;
    asn_TYPE_descriptor_t *pdu_type;

    /* QC diag cannot handle RRC PDU bigger than 6156 */
    if (!msg_len) {
        ERR("too big RRC PDU received");
        goto err;
    }

    VERBOSE("[RRC-OTA] ota msg len = %d", msg_len);

    /* bypass if zero-length PDU */
    if (!msg_len) {
        ERR("zero length RRC PDU received");
        goto err;
    }

    /* process each of PDU type - BCCH_DL_SCH message */
    if (pdu_num == bcchDlSchPduNum) {

        BCCH_DL_SCH_Message_t *bcch_dl_sch_message;
        PLMN_IdentityList_t *plmn_identitylist;
        PLMN_IdentityInfo_t *plmn_identityinfo;
        CellIdentity_t *cellidentity;

        int cell_id;

        MCC_t *mcc;
        MNC_t *mnc;

        char mcc_str[8];
        char mnc_str[8];

        struct gcell_t *gc;
        struct gcell_t *egc;
        int gc_count;

        /* decode per */
        opt_codec_ctx = 0;
        bcch_dl_sch_message = 0;
        pdu_type = &asn_DEF_BCCH_DL_SCH_Message;
        rval = uper_decode_complete(opt_codec_ctx, pdu_type, (void **)&bcch_dl_sch_message, msg, msg_len);

        /* bypass if there is an error */
        if (rval.code != RC_OK) {
            DEBUG("failed to decode UPER (error code = %d,len=%d)", rval.code, msg_len);
            DEBUG("* dump UPER (len=%d)\n", msg_len);
            log_debug_dump(msg, msg_len);
            goto fin_bcch_dl_sch_message;
        }

        /* check if PDU has c1 */
        if (bcch_dl_sch_message->message.present != BCCH_DL_SCH_MessageType_PR_c1) {
            DEBUG("c1 not included in BCCH DL SCH_Message");
            goto fin_bcch_dl_sch_message;
        }

        /* check if PDU has SIB1 */
        switch (bcch_dl_sch_message->message.choice.c1.present) {

            case BCCH_DL_SCH_MessageType__c1_PR_systemInformationBlockType1:
                DEBUG("[SIB] SIB1 recieved");

                /* get plmn identity list and cell identity */
                plmn_identitylist =
                    &bcch_dl_sch_message->message.choice.c1.choice.systemInformationBlockType1.cellAccessRelatedInfo.plmn_IdentityList;
                cellidentity = &bcch_dl_sch_message->message.choice.c1.choice.systemInformationBlockType1.cellAccessRelatedInfo.cellIdentity;

                /* get cell id */
                cell_id = get_uint_from_bit_string(cellidentity);
                DEBUG("[SIB] cell id = %d", cell_id);

                /* for each of plmn indentity */
                for_each_asn1c_array(i, plmn_identitylist, plmn_identityinfo)
                {

                    mcc = plmn_identityinfo->plmn_Identity.mcc;
                    mnc = &plmn_identityinfo->plmn_Identity.mnc;

                    /* decode MCC */
                    if (!get_mccmnc_from_mccmnc_digits(mcc_str, sizeof(mcc_str), mcc, NULL)) {
                        ERR("failed to decode MCC");
                        goto err;
                    }

                    /* decode MNC */
                    if (!get_mccmnc_from_mccmnc_digits(mnc_str, sizeof(mnc_str), NULL, mnc)) {
                        ERR("failed to decode MNC");
                        goto err;
                    }

                    /* do not accumlate SIB1 message, only print */
                    /* create a new gc */
                    gc = gcell_new();
                    if (gc) {
                        /* update members */
                        snprintf(gc->mcc, sizeof(gc->mcc), mcc_str);
                        snprintf(gc->mnc, sizeof(gc->mnc), mnc_str);
                        gc->phys_cell_id = log_phy_cell_id;
#if 0
                    /* test for accumulation of base stations - disabled as this block is only for test code */
                    static int freq_offset = 0;
                    freq_offset++;
                    gc->freq = log->freq + freq_offset;
#else
                        gc->freq = log_freq;
#endif
                        gc->cell_id = cell_id;

                        /* remove existing global cell element */
                        egc = gcell_find(gc);
                        if (egc) {
                            DEBUG("[SIB] PLMN#%d, mcc=%s, mnc=%s, phys_cell=%d,freq=%d,cell_id=%d - remove", i, gc->mcc, gc->mnc, gc->phys_cell_id,
                                  gc->freq, gc->cell_id);
                            gcell_del(egc);
                        }

                        /* remove oldest global cell element */
                        gc_count = gcell_get_count();
                        if (gc_count >= MAX_GC_RDB) {
                            DEBUG("[SIB] remove old element (gc_count=%d)", gc_count);
                            egc = gcell_get_first_or_next(NULL);
                            if (egc)
                                gcell_del(egc);
                        }

                        /* add to global pool */
                        gcell_add(gc);

                        DEBUG("[SIB] PLMN#%d, mcc=%s, mnc=%s, phys_cell=%d,freq=%d,cell_id=%d - add", i, gc->mcc, gc->mnc, gc->phys_cell_id, gc->freq,
                              gc->cell_id);
                    } else {
                        ERR("failed to create a new gc");
                    }
                }
                break;

            case BCCH_DL_SCH_MessageType__c1_PR_systemInformation: {
                SystemInformation_t *systemInformation;
                struct SystemInformation__criticalExtensions *criticalExtensions;
                SystemInformation_r8_IEs_t *systemInformation_r8;
                struct SystemInformation_r8_IEs__sib_TypeAndInfo__Member *systemInformation_r8_member;
                int i;

                DEBUG("[SIB] SIBx recieved");

                /* get system information and criticalExtensions */
                systemInformation = &bcch_dl_sch_message->message.choice.c1.choice.systemInformation;
                criticalExtensions = &systemInformation->criticalExtensions;

                /* bypass if not r8 */
                if (criticalExtensions->present != SystemInformation__criticalExtensions_PR_systemInformation_r8) {
                    DEBUG("[SIB] SIB is not system information r8, skip");
                    break;
                }

                /* get system information r8 */
                systemInformation_r8 = &criticalExtensions->choice.systemInformation_r8;

                for_each_asn1c_array(i, &systemInformation_r8->sib_TypeAndInfo, systemInformation_r8_member)
                {

                    process_system_information_block(systemInformation_r8_member);
                }
                break;
            }

            default:
                DEBUG("skip BCCH DL SCH_Message (present=%d)", bcch_dl_sch_message->message.choice.c1.present);
                break;
        }

    fin_bcch_dl_sch_message:
        if (bcch_dl_sch_message)
            ASN_STRUCT_FREE(*pdu_type, bcch_dl_sch_message);
    }

    return;
err:
    return;
}

/*
 Get PLMN, PCI and etc.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_serving_cell_info)
{
    /* MCC + MNC */
#define ECGI_PLMN_LEN (3 + 3)
    /* ECI */
#define ECGI_CELL_ID_LEN 9
#define ECGI_STRING_LEN (ECGI_PLMN_LEN + ECGI_CELL_ID_LEN)
#define ECGI_FREQ_STRING_LEN 5 /* 5 char max */
    /* enough for "|", freq, ",", pci, ",", ecgi, null terminator */
#define ECGI_FREQ_PCI_ECGI_STRING_LEN (1 + ECGI_FREQ_STRING_LEN + 1 + ECGI_CELL_ID_LEN + 1 + ECGI_STRING_LEN + 1)
#define ECGI_LIST_SIZE 1024

    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(3);
    declare_app_func_define_var_log(lte_rrc_log_serv_cell_info_s);

    char mnc_format[RDB_MAX_VAL_LEN];
    char mnc[4];
    char mcc[4];
    int mnc_len;
    int ecgi_len;
    int r;

    /* allocate space for neighbor cells ecgi, plus comma, plus null terminator */
    char ecgi_string[ECGI_STRING_LEN + 1];
    char temp_frq_pci_ecgi[ECGI_FREQ_PCI_ECGI_STRING_LEN];

    DEBUG("dl_bw=%d, ul_bw=%d", log->dl_bw, log->ul_bw);

    r = snprintf(mcc, sizeof(mcc), "%03d", log->sel_plmn_mcc);
    if (r < 0)
        return; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_prefix_call(_rdb_set_str, "radio_stack.e_utra_measurement_report.mcc", mcc);

    r = snprintf(mnc_format, sizeof(mnc_format), "%%0%dd", log->num_mnc_digits);
    if (r < 0)
        return; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    r = snprintf(mnc, sizeof(mnc), mnc_format, log->sel_plmn_mnc);
    if (r < 0)
        return; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
    _rdb_prefix_call(_rdb_set_str, "radio_stack.e_utra_measurement_report.mnc", mnc);

    _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_measurement_report.cellid", log->cell_id);
    _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_measurement_report.tac", log->tracking_area_code);

    _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_measurement_report.serv_earfcn.dl_freq", log->dl_freq);
    _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_measurement_report.serv_earfcn.ul_freq", log->ul_freq);

    _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_measurement_report.servphyscellid", log->phy_cell_id);

    _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_measurement_report.freqbandind", log->freq_band_ind);

    _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_measurement_report.dl_bandwidth", log->dl_bw);
    _rdb_prefix_call(_rdb_set_int, "radio_stack.e_utra_measurement_report.ul_bandwidth", log->ul_bw);

    /* search PCI and frequency in SIB1 messages */
    struct gcell_t *gc;
    struct gcell_t *gc_ecgi = NULL;

    /* log SIB1 messages while searching SIB1 */
    gc = gcell_get_first_or_next(NULL);
    char *frq_pci_ecgi_list = (char *)malloc(ECGI_LIST_SIZE);
    if (frq_pci_ecgi_list) {
        frq_pci_ecgi_list[0] = '\0';
    }

    while (gc) {

        if (!gc_ecgi && gc->phys_cell_id == log->phy_cell_id && gc->freq == log->dl_freq) {
            gc_ecgi = gc;
            DEBUG("[ECGI] got SIB1 - %s,%s,%d,%d,%d", gc->mcc, gc->mnc, gc->freq, gc->phys_cell_id, gc->cell_id);
            /* please do not break here to log all SIB1 messages for better log messages */
        }
        /* if space available for neighbor cell list */
        if (frq_pci_ecgi_list) {
            /* build ECGI - valid MCC is 3 digit and valid MNC is either 2 or 3 digit */
            mnc_len = strlen(gc->mnc);
            ecgi_len = snprintf(ecgi_string, sizeof(ecgi_string), "%s%s%s%09d", gc->mcc, gc->mnc, (mnc_len < 3) ? "0" : "", gc->cell_id);

            /* verify the length of ECGI */
            if (ecgi_len != ECGI_STRING_LEN) {
                ERR("[ECGI] incorrect length of ECGI - %s (%s,%s,%d,%d,%d)", ecgi_string, gc->mcc, gc->mnc, gc->freq, gc->phys_cell_id, gc->cell_id);
            } else {
                /* if there is room for another cell ecgi ( + "|" + frq + "," + pci + "," + ecgi ) */
                if ((strlen(frq_pci_ecgi_list) + ECGI_FREQ_PCI_ECGI_STRING_LEN) < ECGI_LIST_SIZE) {
                    /* append this to the cell list */
                    /* freq,pci,ecgi| */
                    snprintf(temp_frq_pci_ecgi, sizeof(temp_frq_pci_ecgi), "%d,%d,%s", gc->freq, gc->phys_cell_id, ecgi_string);
                    if (strlen(frq_pci_ecgi_list) != 0) {
                        strcat(frq_pci_ecgi_list, "|");
                    }
                    strcat(frq_pci_ecgi_list, temp_frq_pci_ecgi);
                } else {
                    DEBUG("[ECGI] ecgi cell list is full");
                }
            }
        } else { /* end if malloc was successful */
            ERR("[ECGI] cannot allocate mem for neibhgor ecgi list");
        }

        gc = gcell_get_first_or_next(gc);
    }

    /* write ECGI to RDB if a corresponding SIB1 message is found */
    if (gc_ecgi) {

        char ecgi[ECGI_PLMN_LEN + ECGI_CELL_ID_LEN + 1 + 1]; /* ECGI + NULL + 1 more space to validate the result */
        mnc_len = strlen(gc_ecgi->mnc);

        /* build ECGI - valid MCC is 3 digit and valid MNC is either 2 or 3 digit */
        ecgi_len = snprintf(ecgi, sizeof(ecgi), "%s%s%s%09d", gc_ecgi->mcc, gc_ecgi->mnc, (mnc_len < 3) ? "0" : "", gc_ecgi->cell_id);

        /* verify the length of ECGI */
        if (ecgi_len != ECGI_PLMN_LEN + ECGI_CELL_ID_LEN) {
            ERR("[ECGI] incorrect length of ECGI - %s (%s,%s,%d,%d,%d)", ecgi, gc_ecgi->mcc, gc_ecgi->mnc, gc_ecgi->freq, gc_ecgi->phys_cell_id,
                gc_ecgi->cell_id);
        } else {
            DEBUG("[ECGI] got ECGI - %s", ecgi);
            _rdb_prefix_call(_rdb_set_str, "system_network_status.ECGI", ecgi);
        }
    }
    /* if malloc created space for neighbor cell list */
    if (frq_pci_ecgi_list) {
        /* write the list of cell info including ecgi to rdb */
        if (strlen(frq_pci_ecgi_list) > 0) {
            _rdb_prefix_call(_rdb_set_str, "manual_cell_meas.ecgi", frq_pci_ecgi_list);
        }
        /* done, so free the space */
        free(frq_pci_ecgi_list);
    }
}

/*
 Get downlink RLC statistics.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_rlcdl_statistics)
{
    struct lte_rlc_hdr_t {
        uint16 version : 8;
        uint16 number_subpackets : 8;
        uint16 reserved;
    } PACK();

    struct logical_channel_t {
        uint16 rb_cfg_idx : 8;
        uint16 mode : 8;
        uint32 num_rst;
        uint32 num_data_pdu;
        uint64 data_pdu_bytes;
        uint32 num_status_rxed;
        uint64 status_rxed_bytes;
        uint32 num_invalid_pdu;
        uint64 invalid_pdu_bytes;
        uint32 num_retx_pdu;
        uint64 retx_pdu_bytes;
        uint32 num_dup_pdu;
        uint64 num_dup_bytes;
        uint32 num_dropped_pdu;
        uint64 dropped_pdu__bytes;
        uint32 num_dropped_pdu_fc;
        uint64 dropped_pdu__bytes_fc;
        uint32 num_sdu;
        uint64 num_sdu_bytes;
        uint32 num_nonseq_sdu;
        uint32 num_ctrl_pdu;
        uint32 num_comp_nack;
        uint32 num_segm_nack;
        uint32 num_t_reord_exp;
        uint32 num_t_reord_start;
        uint32 num_missed_um_pdu;
        uint32 reserved1;
        uint32 num_data_pdu_rst;
        uint64 data_pdu_bytes_rst;
        uint32 num_status_rxed_rst;
        uint64 status_rxed_bytes_rst;
        uint32 num_invalid_pdu_rst;
        uint64 invalid_pdu_bytes_rst;
        uint32 num_retx_pdu_rst;
        uint64 retx_pdu_bytes_rst;
        uint32 num_dup_pdu_rst;
        uint64 num_dup_bytes_rst;
        uint32 num_dropped_pdu_rst;
        uint64 dropped_pdu__bytes_rst;
        uint32 num_dropped_pdu_fc_rst;
        uint64 dropped_pdu__bytes_fc_rst;
        uint32 num_sdu_rst;
        uint64 num_sdu_bytes_rst;
        uint32 num_nonseq_sdu_rst;
        uint32 num_ctrl_pdu_rst;
        uint32 num_comp_nack_rst;
        uint32 num_segm_nack_rst;
        uint32 num_t_reord_exp_rst;
        uint32 num_t_reord_start_rst;
        uint32 num_missed_um_pdu_rst;
        uint32 reserved2;
    } PACK();

    struct lte_rlc_dl_statistics_subpacket_t {
        uint16 sub_packet_id : 8;
        uint16 sub_packet_version : 8;
        uint16 sub_packet_size;

        uint16 num_rb : 8;
        uint32 rlc_pdcp_q_full_cnt;
        uint32 rlcdl_error_cnt;

        struct logical_channel_t logical_channels[0];
    } PACK();

    struct lte_rlc_dl_statistics_t {
        struct lte_rlc_hdr_t hdr;
        struct lte_rlc_dl_statistics_subpacket_t subpackets[0];
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(struct lte_rlc_dl_statistics_t);

    struct acc_t *RLCDownThroughput;
    struct acc_t *RLCDownRetrans;

    int i;
    int j;

#ifdef CONFIG_DEBUG_RLC
    DEBUG("got log (ptr=0x%p,len=%d,code=0x%04x,payload_len=%d)", ptr, len, hdr->code, hdr->len);
    DEBUG("struct lte_rlc_dl_statistics_t=%d", sizeof(struct lte_rlc_dl_statistics_t));
    DEBUG("struct lte_rlc_dl_statistics_subpacket_t=%d", sizeof(struct lte_rlc_dl_statistics_subpacket_t));
    DEBUG("struct logical_channel_t=%d", sizeof(struct logical_channel_t));
#endif

    VERBOSE("number_subpackets = %d", log->hdr.number_subpackets);
    VERBOSE("num_rb = %d", log->subpackets[0].num_rb);

    unsigned long long rlc_dl_throughput;
    unsigned long long total_dl_retrans;
    rlc_dl_throughput = 0;
    total_dl_retrans = 0;
    struct lte_rlc_dl_statistics_subpacket_t *subpacket;
    struct logical_channel_t *logical_channel;

    for (i = 0; i < log->hdr.number_subpackets; i++) {
        subpacket = &log->subpackets[i];

        for (j = 0; j < subpacket->num_rb; j++) {
            logical_channel = &subpacket->logical_channels[j];

#ifdef CONFIG_DEBUG_RLC
            DEBUG("subpacket#%d logical_channel#%d = %d", i, j, logical_channel->rb_cfg_idx);
            DEBUG("subpacket#%d logical_channel#%d num_rst = %d", i, j, logical_channel->num_rst);
#endif

            /* RLC DL throughput for a radio bearer can be calculated as: DATA_PDU_BYTES + STATUS_RXED_BYTES + INVALID_PDU_BYTES */
            rlc_dl_throughput += logical_channel->data_pdu_bytes + logical_channel->status_rxed_bytes + logical_channel->invalid_pdu_bytes;
            total_dl_retrans = logical_channel->retx_pdu_bytes + logical_channel->num_retx_pdu_rst;

            VERBOSE("subpacket#%d logical_channel#%d data_pdu_bytes = %llu", i, j, logical_channel->data_pdu_bytes);
            VERBOSE("subpacket#%d logical_channel#%d status_rxed_bytes = %llu", i, j, logical_channel->status_rxed_bytes);
            VERBOSE("subpacket#%d logical_channel#%d invalid_pdu_bytes = %llu", i, j, logical_channel->invalid_pdu_bytes);
        }
    }

    rdb_enter_csection();
    {
        /* update accumulation info */
        RLCDownThroughput = &rrc_info.RLCDownThroughput;
        acc_feed(RLCDownThroughput, rlc_dl_throughput, ts);
        /* update total dl retrans */
        RLCDownRetrans = &rrc_info.RLCDownRetrans;
        acc_feed(RLCDownRetrans, total_dl_retrans, ts);

#ifdef CONFIG_DEBUG_THROUGHPUT
        DEBUG("[rlc_down_throughput] ts = %llu", *ts);
        DEBUG("[rlc_down_throughput] avg = %lld", RLCDownThroughput->avg);
        DEBUG("[rlc_down_throughput] min = %lld", RLCDownThroughput->min);
        DEBUG("[rlc_down_throughput] max = %lld", RLCDownThroughput->max);
        DEBUG("[rlc_down_throughput] down retrans diff = %lld", acc_get_diff(RLCDownRetrans));
#endif

#ifdef CONFIG_DEBUG_RLC
        DEBUG("[rlc_down_throughput] diff = %lld", RLCDownThroughput->diff);
        DEBUG("[rlc_down_throughput] sacc = %lld", RLCDownThroughput->sacc);
        DEBUG("[rlc_down_throughput] acc = %lld", RLCDownThroughput->acc);
        DEBUG("[rlc_down_throughput] rlc_dl_throughput = %llu", rlc_dl_throughput);
#endif

        rdb_leave_csection();
    }
}

/*
 Get uplink RLC statistics.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_rlcul_statistics)
{

    struct lte_rlc_hdr_t {
        uint16 version : 8;
        uint16 number_subpackets : 8;
        uint16 reserved;
    } PACK();

    struct lte_rlc_ul_statistics_subpacket_v3_delay_t {
        uint32 max;
        uint32 min;
        uint32 avg;
        uint32 num_samples;
    } PACK();

    struct lte_rlc_ul_statistics_subpacket_v3_occup_t {
        uint32 max;
        uint32 min;
        uint32 dne_discard;
    } PACK();

    struct logical_channel_v3_t {
        uint16 rb_cfg_idx : 8;
        uint16 mode : 8;
        uint32 num_rst;
        uint32 apn_type;
        uint32 num_sdu_discard;
        struct lte_rlc_ul_statistics_subpacket_v3_delay_t q_delay;
        struct lte_rlc_ul_statistics_subpacket_v3_occup_t q_occupancy;
        uint32 num_new_data_pdu;
        uint32 num_new_data_pdu_bytes;
        uint32 num_sdu;
        uint32 num_sdu_bytes;
        uint32 num_ctrl_pdu_tx;
        uint32 num_ctrl_pdu_bytes_tx;
        uint32 num_retx_pdus;
        uint32 num_retx_pdu_bytes;
        uint32 num_ctrl_pdu_rx;
        uint32 num_comp_nack;
        uint32 num_segm_nack;
        uint32 num_invalid_ctrl_pdu_rx;
        uint32 num_poll;
        uint32 num_t_poll_retx_expiry;
        uint32 reserved1;
        uint32 num_new_data_pdu_rst;
        uint32 num_new_data_pdu_bytes_rst;
        uint32 num_sdu_rst;
        uint32 num_sdu_bytes_rst;
        uint32 num_ctrl_pdu_tx_rst;
        uint32 num_ctrl_pdu_bytes_tx_rst;
        uint32 num_retx_pdus_rst;
        uint32 num_retx_pdu_bytes_rst;
        uint32 num_ctrl_pdu_rx_rst;
        uint32 num_comp_nack_rst;
        uint32 num_segm_nack_rst;
        uint32 num_invalid_ctrl_pdu_rx_rst;
        uint32 num_poll_rst;
        uint32 num_t_poll_retx_expiry_rst;
        uint32 reserved2;
    } PACK();

    struct lte_rlc_ul_statistics_subpacket_v3_t {
        uint16 sub_packet_id : 8;
        uint16 unique_subpacket_identifier : 8;
        uint16 sub_packet_version : 16;

        uint16 num_rb : 8;
        uint32 rlcul_error_cnt;
        struct lte_rlc_ul_statistics_subpacket_v3_delay_t rtt;

        struct logical_channel_v3_t logical_channels[0];
    } PACK();

    struct lte_rlc_ul_statistics_v3_t {
        struct lte_rlc_hdr_t hdr;
        struct lte_rlc_ul_statistics_subpacket_v3_t subpackets[0];
    } PACK();

    struct logical_channel_v1_t {
        uint16 rb_cfg_idx : 8;
        uint16 mode : 8;
        uint32 num_rst;
        uint32 num_new_data_pdu;
        uint32 num_new_data_pdu_bytes;
        uint32 num_sdu;
        uint32 num_sdu_bytes;
        uint32 num_ctrl_pdu_tx;
        uint32 num_ctrl_pdu_bytes_tx;
        uint32 num_retx_pdus;
        uint32 num_retx_pdu_bytes;
        uint32 num_ctrl_pdu_rx;
        uint32 num_comp_nack;
        uint32 num_segm_nack;
        uint32 num_invalid_ctrl_pdu_rx;
        uint32 num_poll;
        uint32 num_t_poll_retx_expiry;
        uint32 reserved1;
        uint32 num_new_data_pdu_rst;
        uint32 num_new_data_pdu_bytes_rst;
        uint32 num_sdu_rst;
        uint32 num_sdu_bytes_rst;
        uint32 num_ctrl_pdu_tx_rst;
        uint32 num_ctrl_pdu_bytes_tx_rst;
        uint32 num_retx_pdus_rst;
        uint32 num_retx_pdu_bytes_rst;
        uint32 num_ctrl_pdu_rx_rst;
        uint32 num_comp_nack_rst;
        uint32 num_segm_nack_rst;
        uint32 num_invalid_ctrl_pdu_rx_rst;
        uint32 num_poll_rst;
        uint32 num_t_poll_retx_expiry_rst;
        uint32 reserved2;
    } PACK();

    struct lte_rlc_ul_statistics_subpacket_v1_t {
        uint16 sub_packet_id : 8;
        uint16 unique_subpacket_identifier : 8;
        uint16 sub_packet_version : 16;

        uint16 num_rb : 8;
        uint32 rlcul_error_cnt;

        struct logical_channel_v1_t logical_channels[0];
    } PACK();

    struct lte_rlc_ul_statistics_t {
        struct lte_rlc_hdr_t hdr;
        union
        {
            struct lte_rlc_ul_statistics_subpacket_v3_t subpackets_v3[0];
            struct lte_rlc_ul_statistics_subpacket_v1_t subpackets_v1[0];
        };
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1, 3);
    declare_app_func_define_var_log(struct lte_rlc_ul_statistics_t);

    struct acc_t *RLCUpThroughput;
    struct acc_t *RLCUpRetrans;

    int i;
    int j;

    unsigned long long rlc_ul_throughput;
    unsigned long long total_ul_retrans;
    rlc_ul_throughput = 0;
    total_ul_retrans = 0;

#ifdef CONFIG_DEBUG_RLC
    DEBUG("got log (ptr=0x%p,len=%d,code=0x%04x,payload_len=%d)", ptr, len, hdr->code, hdr->len);
    DEBUG("struct lte_rlc_ul_statistics_t=%d", sizeof(struct lte_rlc_ul_statistics_t));
#endif

    /* Only supporting v1 and v3 of structure. */
    /* ------------ v3 -----------------------------------------------------------------*/
    if (declare_app_func_ver_match(3)) {
#ifdef CONFIG_DEBUG_RLC
        DEBUG("struct lte_rlc_ul_statistics_subpacket_t=%d", sizeof(struct lte_rlc_ul_statistics_subpacket_v3_t));
        DEBUG("struct logical_channel_t=%d", sizeof(struct logical_channel_v3_t));
#endif

        VERBOSE("number_subpackets = %d", log->hdr.number_subpackets);
        VERBOSE("num_rb = %d", log->subpackets_v3[0].num_rb);

        struct lte_rlc_ul_statistics_subpacket_v3_t *subpacket;
        struct logical_channel_v3_t *logical_channel;

        for (i = 0; i < log->hdr.number_subpackets; i++) {
            subpacket = &log->subpackets_v3[i];

            for (j = 0; j < subpacket->num_rb; j++) {
                logical_channel = &subpacket->logical_channels[j];

#ifdef CONFIG_DEBUG_RLC
                DEBUG("subpacket#%d logical_channel#%d = %d", i, j, logical_channel->rb_cfg_idx);
                DEBUG("subpacket#%d logical_channel#%d num_rst = %d", i, j, logical_channel->num_rst);
#endif

                /* RLC UL throughput for a radio bearer can be calculated by the following: NUM_NEW_DATA_PDU_BYTES + NUM_CTRL_PDU_BYTES +
                 * NUM_RETX_PDU_BYTES. */
                rlc_ul_throughput +=
                    logical_channel->num_new_data_pdu_bytes + logical_channel->num_ctrl_pdu_bytes_tx + logical_channel->num_retx_pdu_bytes;
                total_ul_retrans += logical_channel->num_retx_pdu_bytes + logical_channel->num_retx_pdu_bytes_rst;

                VERBOSE("subpacket#%d logical_channel#%d num_new_data_pdu_bytes = %d", i, j, logical_channel->num_new_data_pdu_bytes);
                VERBOSE("subpacket#%d logical_channel#%d num_ctrl_pdu_bytes_tx = %d", i, j, logical_channel->num_ctrl_pdu_bytes_tx);
                VERBOSE("subpacket#%d logical_channel#%d num_retx_pdu_bytes = %d", i, j, logical_channel->num_retx_pdu_bytes);
            }
        }
        /* ------------ v1 -----------------------------------------------------------------*/
    } else if (declare_app_func_ver_match(1)) {
#ifdef CONFIG_DEBUG_RLC
        DEBUG("struct lte_rlc_ul_statistics_subpacket_t=%d", sizeof(struct lte_rlc_ul_statistics_subpacket_v1_t));
        DEBUG("struct logical_channel_t=%d", sizeof(struct logical_channel_v1_t));
#endif

        VERBOSE("number_subpackets = %d", log->hdr.number_subpackets);
        VERBOSE("num_rb = %d", log->subpackets_v1[0].num_rb);

        struct lte_rlc_ul_statistics_subpacket_v1_t *subpacket;
        struct logical_channel_v1_t *logical_channel;

        for (i = 0; i < log->hdr.number_subpackets; i++) {
            subpacket = &log->subpackets_v1[i];

            for (j = 0; j < subpacket->num_rb; j++) {
                logical_channel = &subpacket->logical_channels[j];

#ifdef CONFIG_DEBUG_RLC
                DEBUG("subpacket#%d logical_channel#%d = %d", i, j, logical_channel->rb_cfg_idx);
                DEBUG("subpacket#%d logical_channel#%d num_rst = %d", i, j, logical_channel->num_rst);
#endif

                /* RLC UL throughput for a radio bearer can be calculated by the following: NUM_NEW_DATA_PDU_BYTES + NUM_CTRL_PDU_BYTES +
                 * NUM_RETX_PDU_BYTES. */
                rlc_ul_throughput +=
                    logical_channel->num_new_data_pdu_bytes + logical_channel->num_ctrl_pdu_bytes_tx + logical_channel->num_retx_pdu_bytes;
                total_ul_retrans += logical_channel->num_retx_pdu_bytes + logical_channel->num_retx_pdu_bytes_rst;

                VERBOSE("subpacket#%d logical_channel#%d num_new_data_pdu_bytes = %d", i, j, logical_channel->num_new_data_pdu_bytes);
                VERBOSE("subpacket#%d logical_channel#%d num_ctrl_pdu_bytes_tx = %d", i, j, logical_channel->num_ctrl_pdu_bytes_tx);
                VERBOSE("subpacket#%d logical_channel#%d num_retx_pdu_bytes = %d", i, j, logical_channel->num_retx_pdu_bytes);
            }
        }
    }

#ifdef CONFIG_DEBUG_THROUGHPUT
    DEBUG("[rlc_up_throughput] rlc_ul_throughput = %llu", rlc_ul_throughput);
#endif

    rdb_enter_csection();
    {
        /* update accumulation info */
        RLCUpThroughput = &rrc_info.RLCUpThroughput;
        acc_feed(RLCUpThroughput, rlc_ul_throughput, ts);
        /* update total ul retrans */
        RLCUpRetrans = &rrc_info.RLCUpRetrans;
        acc_feed(RLCUpRetrans, total_ul_retrans, ts);

#ifdef CONFIG_DEBUG_THROUGHPUT
        DEBUG("[rlc_up_throughput] ts = %llu", *ts);
        DEBUG("[rlc_up_throughput] avg = %lld", RLCUpThroughput->avg);
        DEBUG("[rlc_up_throughput] min = %lld", RLCUpThroughput->min);
        DEBUG("[rlc_up_throughput] max = %lld", RLCUpThroughput->max);
        DEBUG("[rlc_up_throughput] up retrans diff = %lld", acc_get_diff(RLCUpRetrans));
#endif

#ifdef CONFIG_DEBUG_RLC
        DEBUG("[rlc_up_throughput] diff = %lld", RLCUpThroughput->diff);
#endif

        rdb_leave_csection();
    }
}
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_BPLMN_SEARCH_MSG
/*
 Capture beginning of LTE ML1 BPLMN.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_ml1_bplmn_start_req_log)
{
    struct lte_ml1_bplmn_start_req_log_t {
        int version;
    } PACK();

    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(struct lte_ml1_bplmn_start_req_log_t);

    DEBUG("packet version = %d", log->version);

    /* Start new manual cell RF measurements */
    manual_cell_meas_info.is_requested = 1;
    manual_cell_meas_info.num_cells = 0;
    manual_cell_meas_info.seq_no++;
}

/*
 Log the searched cell information

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_ml1_bplmn_cell_confirm)
{
    struct lte_ml1_bplmn_cell_confirm_v16_t {
        uint32 standard_version : 8;
        uint32 reserved0 : 16;
        uint32 frequency : 32;
        int rsrp : 16;
        uint16 __pad : 16;
        uint32 bw : 8;
        uint32 cell_id : 9;
        uint32 reserved1 : 15;
        uint32 srx_lev_calculated : 1;
        int srx_lev : 16;
        uint32 reserved2 : 15;
        uint32 rel9_info : 32;
    } PACK();

    struct lte_ml1_bplmn_cell_confirm_v32_t {
        uint32 standard_version : 8;
        uint32 reserved0 : 16;
        uint32 frequency : 32;
        uint32 rsrp : 12;
        uint32 rsrq : 10;
        uint16 __pad : 10;
        uint32 bw : 8;
        uint32 cell_id : 9;
        uint32 reserved1 : 15;
        uint32 srx_lev_calculated : 1;
        int srx_lev : 16;
        uint32 reserved2 : 15;
        uint32 rel9_info : 32;
    } PACK();

    struct lte_ml1_bplmn_cell_confirm_t {
        uint32 version : 8;
        union
        {
            struct lte_ml1_bplmn_cell_confirm_v16_t v16;
            struct lte_ml1_bplmn_cell_confirm_v32_t v32;
        };
    } PACK();

    double rsrp;
    double rsrq;
    unsigned int pci;
    unsigned int earfcn = 0;

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(16, 32);
    declare_app_func_define_var_log(struct lte_ml1_bplmn_cell_confirm_t);

    if (declare_app_func_ver_match(16)) {
        earfcn = log->v16.frequency;
        pci = log->v16.cell_id;
        rsrp = log->v16.rsrp;
        /*
         * Assign one dB lower than the minimum RSRQ in order to tell
         * applications that RSRQ is not available in the measurement.
         */
        rsrq = MANUAL_CELL_MEAS_MIN_RSRQ_VALUE - 1;
    } else if (declare_app_func_ver_match(32)) {
        earfcn = log->v32.frequency;
        pci = log->v32.cell_id;
        rsrp = __dbx16(log->v32.rsrp, -180);
        rsrq = __dbx16(log->v32.rsrq, -30);
    }

    if (earfcn) {
        add_manual_cell_measurement(earfcn, pci, rsrp, rsrq);
    }
}

/*
 Capture completion of LTE ML1 BPLMN.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(lte_ml1_bplmn_complete_ind_log)
{
    struct lte_ml1_bplmn_complete_ind_log_t {
        int version;
    } PACK();

    struct gcell_t *gc;

    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    char val[RDB_MAX_VAL_LEN];

    char rname[RDB_MAX_NAME_LEN];

    int i;

    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(struct lte_ml1_bplmn_complete_ind_log_t);

    DEBUG("packet version = %d", log->version);

    /* loop for each of global cell elements */
    i = 0;
    gc = gcell_get_first_or_next(NULL);
    while (gc) {
        snprintf(rname, sizeof(rname), "rrc_info.cell.%d", i++);

        /* MCC, MNC, frequency, physical cell ID, CGI (cell ID) */
        snprintf(val, sizeof(val), "%s,%s,%d,%d,%d", gc->mcc, gc->mnc, gc->freq, gc->phys_cell_id, gc->cell_id);
        _rdb_prefix_call(_rdb_set_str, rname, val);

        DEBUG("[SIB1] cell info #%d - %s", i, val);

        gc = gcell_get_first_or_next(gc);
    }

    /* write total number of cell info into RDB */
    _rdb_prefix_call(_rdb_set_int, "rrc_info.cell.qty", i);
    DEBUG("[SIB1] total cell info count = %d", i);

    /* On completion of manual cell measurements, update corresponding RDB variables */
    for (i = 0; i < manual_cell_meas_info.num_cells; i++) {
        snprintf(rname, sizeof(rname), "manual_cell_meas.%d", i);
        /* Check whether RSRQ is available */
        if (manual_cell_meas_info.cells[i].rsrq < MANUAL_CELL_MEAS_MIN_RSRQ_VALUE) {
            snprintf(val, sizeof(val), "E,%u,%u,%.f", manual_cell_meas_info.cells[i].earfcn, manual_cell_meas_info.cells[i].pci,
                     manual_cell_meas_info.cells[i].rsrp);
        } else {
            snprintf(val, sizeof(val), "E,%u,%u,%.2f,%.2f", manual_cell_meas_info.cells[i].earfcn, manual_cell_meas_info.cells[i].pci,
                     manual_cell_meas_info.cells[i].rsrp, manual_cell_meas_info.cells[i].rsrq);
        }
        _rdb_prefix_call(_rdb_set_str, rname, val);
    }
    _rdb_prefix_call(_rdb_set_int, "manual_cell_meas.qty", manual_cell_meas_info.num_cells);
    _rdb_prefix_call(_rdb_set_int, "manual_cell_meas.seq", manual_cell_meas_info.seq_no);

#if defined SAVE_RFQ_EARFCN_LIST
    /* Merging EARFCN list into wwan.0.rfq_earfcn */
    if (manual_cell_meas_info.num_cells) {
        if (manual_cell_meas_earfcn_count == MAX_EARFCN_ENTRIES) {
            ERR("Parsing wwan.0.rfq_earfcn: Buffer overflow or no more room for new data");
        } else {
            int i, j;
            unsigned int existing_earfcn_count = manual_cell_meas_earfcn_count;
            /* Add newly measured earfcn values into array if not already present in the array */
            for (i = 0; i < manual_cell_meas_info.num_cells && manual_cell_meas_earfcn_count < MAX_EARFCN_ENTRIES; i++) {
                for (j = 0; j < manual_cell_meas_earfcn_count; j++) {
                    if (manual_cell_meas_info.cells[i].earfcn == manual_cell_meas_earfcns[j]) {
                        break;
                    }
                }
                if (j == manual_cell_meas_earfcn_count) {
                    manual_cell_meas_earfcns[manual_cell_meas_earfcn_count++] = manual_cell_meas_info.cells[i].earfcn;
                }
            }
            if (manual_cell_meas_earfcn_count == MAX_EARFCN_ENTRIES) {
                ERR("Buffer overflow, new data of wwan.0.rfq_earfcn may be truncated");
            }
            /* write the list if new data is available (even if new earfcn list is truncated) */
            if (manual_cell_meas_earfcn_count > existing_earfcn_count) {
                /*
                 * EARFCN number is within range 0 to 65535 so each EARFCN takes up to 5 characters.
                 * Beside it needs 1 extra byte for comma or \0.
                 * 6 * 64 = 384 < 1024 so static buffer should be sufficient.
                 */
#define FIRST_EAFRCN_PRINT_LEN 6
#define EAFRCN_PRINT_LEN 7
                char buffer[RDB_MAX_VAL_LEN];
                int position = 0, printed_size, entry_size = FIRST_EAFRCN_PRINT_LEN, failed = 0;
                const char *entry_format = "%d";
                for (i = 0; i < manual_cell_meas_earfcn_count; i++) {
                    printed_size = snprintf(buffer + position, entry_size, entry_format, manual_cell_meas_earfcns[i]);
                    if (printed_size >= entry_size || printed_size < 0) {
                        ERR("Invalid EARFCN entry: %d", manual_cell_meas_earfcns[i]);
                        failed = 1;
                        break;
                    } else {
                        position += printed_size;
                    }
                    entry_size = EAFRCN_PRINT_LEN;
                    entry_format = ",%d";
                }
                if (!failed) {
                    _rdb_set_str(RFQ_EARFCN_RDB, buffer);
                } else {
                    ERR("Failed to build EARFCN list due to invalid EARFCN entries");
                }
            }
        }
    }
#endif /* SAVE_RFQ_EARFCN_LIST */

    manual_cell_meas_info.is_requested = 0;
}
#endif /* INCLUDE_BPLMN_SEARCH_MSG */

#ifdef INCLUDE_LTE_EMM_ESM_MSG
/*
        ### DCI event handler ###
*/

/*
 [EVENT] Get uplink RLC statistics.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_emm_ota_incoming_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint8);

    time_t ms = get_ms_from_ts(ts);

    uint8 msg = *event;
    const char *msg_str = lte_nas_parser_get_nas_msg_type_name_str(msg);

    DEBUG("event_id=%d, msg=%d, msg_str=[%s]", id_type->id, msg, msg_str);

    switch (msg) {
        case EMM_ATTACH_REJECT: {
            misc_info.qctm_attach_attempt_valid = 0;

            /* add accept statistics */
            misc_info.AttachFailures++;
            DEBUG("[ATTACH] increase attach failures (failures=%lld)", misc_info.AttachFailures);

            misc_info_update();
            break;
        }

        case EMM_ATTACH_ACCEPT: {
            /* calculate latency */
            time_t latency = ms - misc_info.qctm_attach_attempt;

            /* process if attach attempt time stamp is valid */
            if (misc_info.qctm_attach_attempt_valid) {
                avg_feed(&misc_info.AttachLatency, latency);
                DEBUG("[ATTACH] add attach latency (latency=%ld, avg=%.2Lf)", latency, misc_info.AttachLatency.avg);
                misc_info.qctm_attach_attempt_valid = 0;
            }

            /* add accept statistics */
            misc_info.AttachAccepts++;
            DEBUG("[ATTACH] increase attach accepts (accepts=%lld)", misc_info.AttachAccepts);

            misc_info_update();
            break;
        }
    }
}

/*
 [EVENT] Capture LTE EMM OTA outgoing messages

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_emm_ota_outgoing_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint8);

    time_t ms = get_ms_from_ts(ts);

    uint8 msg = *event;
    const char *msg_str = lte_nas_parser_get_nas_msg_type_name_str(msg);

    DEBUG("event_id=%d, msg=%d, msg_str=[%s]", id_type->id, msg, msg_str);

    switch (msg) {
        case EMM_ATTACH_REQUEST:
            /* update attempt time stamp */
            misc_info.qctm_attach_attempt = ms;
            misc_info.qctm_attach_attempt_valid = 1;

            /* add attempt statistics */
            misc_info.AttachAttempts++;
            DEBUG("[ATTACH] increase attach attempts (attempts=%lld)", misc_info.AttachAttempts);

            misc_info_update();
            break;
    }
}

/*
 [EVENT] Capture LTE ESM incoming messages

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_esm_incoming_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint8);

    char *msg = NULL;
    char *names[] = {
        [0x01] = "NAS_ESM_DATA_IND",
        [0x02] = "NAS_ESM_FAILURE_IND",
        [0x03] = "NAS_ESM_SIG_CONNECTION_RELEASED_IND",
        [0x04] = "NAS_ESM_ACTIVE_EPS_IND",
        [0x07] = "NAS_ESM_GET_PDN_CONNECTIVITY_REQ_IND",
    };

    int mid = *event & 0x0f;
    if (__in_array(mid, names))
        msg = names[mid];

    if (msg)
        DEBUG("event_id=%d,msg=0x%08x,msg_str=[%s]", id_type->id, *event, msg);
    else
        DEBUG("event_id=%d, msg=0x%08x", id_type->id, *event);
}
#endif /* INCLUDE_LTE_EMM_ESM_MSG */

#ifdef INCLUDE_COMMON_MSG
/*
 [EVENT] Get indication when MAC layer performs RESET, this event gets called

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_mac_reset_v2)
{
    struct event_mac_reset_v2_t {
        uint8 cause;
    } PACK();

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_mac_reset_v2_t);

    /* print mac release cause */
    DEBUG("mac release cause = %d", event->cause);
}
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_EMM_ESM_MSG
/*
 [EVENT] Capture LTE ESM outgoing messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_esm_outgoing_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_msg_t);

    char *msg = NULL;
    char *names[] = {
        [0x03] = "NAS_EMM_DETACH_CMD", [0x04] = "NAS_EMM_EPS_BEARER_STATUS_CMD", [0x01] = "NAS_EMM_SERVICE_REQ",
        [0x02] = "NAS_EMM_DATA_REQ",   [0x05] = "NAS_EMM_1XCSFB_ESR_CALL_REQ",   [0x06] = "NAS_EMM_1XCSFB_ESR_CALL_ABORT_REQ",
    };

    int mid = event->message_id & 0x0f;
    if (__in_array(mid, names))
        msg = names[mid];

    if (msg)
        DEBUG("event_id=%d,msg=0x%08x,msg_str=[%s]", id_type->id, event->message_id, msg);
    else
        DEBUG("event_id=%d, msg=0x%08x", id_type->id, event->message_id);
}

/*
 [EVENT] Capture LTE ESM incoming messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_esm_ota_incoming_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint8);

    uint8 msg = *event;

    const char *msg_str = lte_nas_parser_get_nas_msg_type_name_str(msg);
    DEBUG("event_id=%d, msg=%d, msg_str=[%s]", id_type->id, msg, msg_str);
}

/*
 [EVENT] Capture LTE ESM incoming messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_esm_ota_outgoing_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint8);

    uint8 msg = *event;

    const char *msg_str = lte_nas_parser_get_nas_msg_type_name_str(msg);
    DEBUG("event_id=%d, msg=%d, msg_str=[%s]", id_type->id, msg, msg_str);
}
#endif /* INCLUDE_LTE_EMM_ESM_MSG */

#ifdef INCLUDE_LTE_CM_MSG
/*
 [EVENT] Capture call manager incoming message.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_cm_incoming_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_msg_t);

    char *msg = NULL;
    char *names[] = {
        [0x01] = "NAS_ESM_BEARER_RESOURCE_ALLOC_REQ",  [0x03] = "NAS_ESM_PDN_CONNECTIVTY_REQ",
        [0x04] = "NAS_ESM_PDN_DISCONNECT_REQ",         [0x05] = "NAS_ESM_BEARER_RESOURCE_ALLOC_ABORT_REQ",
        [0x06] = "NAS_ESM_PDN_CONNECTIVITY_ABORT_REQ", [0x0e] = "NAS_ESM_BEARER_RESOURCE_MODIFICATION_REQ",
    };

    int mid = event->message_id & 0x0f;
    if (__in_array(mid, names))
        msg = names[mid];

    if (msg)
        DEBUG("event_id=%d,msg=0x%08x,msg_str=[%s]", id_type->id, event->message_id, msg);
    else
        DEBUG("event_id=%d, msg=0x%08x", id_type->id, event->message_id);
}

/*
 [EVENT] Capture call manager outgoing message.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_cm_outgoing_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_msg_t);

    char *msg = NULL;
    char *names[] = {
        [0x12] = "MM_CM_1XCSFB_ABORT_RSP",
        [0x13] = "MM_CM_1XCSFB_CALL_RSP",
        [0x02] = "MM_CM_ACT_DEDICATED_BEARER_CONTEXT_REQUEST_IND",
        [0x03] = "MM_CM_DEACT_BEARER_CONTEXT_REQUEST_IND",
        [0x04] = "MM_CM_BEARER_RESOURCE_ALLOC_REJECT_IND",
        [0x05] = "MM_CM_MODIFY_BEARER_CONTEXT_REQUEST_IND",
        [0x06] = "MM_CM_PDN_CONNECTIVITY_REJECT_IND",
        [0x07] = "MM_CM_PDN_DISCONNECT_REJECT_IND",
        [0x08] = "MM_CM_ACT_DRB_RELEASED_IND",
        [0x09] = "MM_CM_DRB_SETUP_IND",
        [0x0b] = "MM_CM_PDN_CONNECTIVITY_FAILURE_IND",
        [0x0c] = "MM_CM_BEARER_RESOURCE_ALLOC_FAILURE_IND",
        [0x0d] = "MM_CM_DRB_REESTABLISH_REJECT_IND",
        [0x0e] = "MM_CM_GET_PDN_CONNECTIVITY_REQUEST_IND",
        [0x0f] = "MM_CM_BEARER_CONTEXT_MODIFY_REJECT_IND",
    };

    int mid = event->message_id & 0x0f;
    if (__in_array(mid, names))
        msg = names[mid];

    if (msg)
        DEBUG("event_id=%d,msg=0x%08x,msg_str=[%s]", id_type->id, event->message_id, msg);
    else
        DEBUG("event_id=%d, msg=0x%08x", id_type->id, event->message_id);
}
#endif /* INCLUDE_LTE_CM_MSG */

#ifdef INCLUDE_LTE_EMM_ESM_MSG
/*
 [EVENT] Capture LTE EMM incoming messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_emm_incoming_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_msg_t);

    char *msg = NULL;
    char *names[] = {
        [0x03] = "NAS_EMM_DETACH_CMD",         [0x04] = "NAS_EMM_EPS_BEARER_STATUS_CMD", [0x07] = "NAS_EMM_EMC_SRV_STATUS_CMD",
        [0x08] = "NAS_EMM_PS_CALL_STATUS_CMD", [0x01] = "NAS_EMM_SERVICE_REQ",           [0x02] = "NAS_EMM_DATA_REQ",
    };

    int mid = event->message_id & 0x0f;
    if (__in_array(mid, names))
        msg = names[mid];

    if (msg)
        DEBUG("event_id=%d,msg=0x%08x,msg_str=[%s]", id_type->id, event->message_id, msg);
    else
        DEBUG("event_id=%d, msg=0x%08x", id_type->id, event->message_id);
}

/*
 [EVENT] Capture LTE EMM outgoing messages.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_lte_emm_outgoing_msg)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_msg_t);

    char *msg = NULL;
    char *names[] = {
        [0x0b] = "NAS_ESM_1XCSFB_ESR_CALL_RSP",   [0x0c] = "NAS_ESM_1XCSFB_ESR_CALL_ABORT_RSP",
        [0x02] = "NAS_ESM_FAILURE_IND",           [0x03] = "NAS_ESM_SIG_CONNECTION_RELEASED_IND",
        [0x04] = "NAS_ESM_ACTIVE_EPS_IND",        [0x05] = "NAS_ESM_DETACH_IND",
        [0x06] = "NAS_ESM_EPS_BEARER_STATUS_IND", [0x07] = "NAS_ESM_GET_PDN_CONNECTIVITY_REQ_IND",
        [0x08] = "NAS_ESM_GET_ISR_STATUS_IND",    [0x0a] = "NAS_ESM_ISR_STATUS_CHANGE_IND",
        [0x0d] = "NAS_ESM_UNBLOCK_ALL_APNS_IND",
    };

    int mid = event->message_id & 0x0f;
    if (__in_array(mid, names))
        msg = names[mid];

    if (msg)
        DEBUG("event_id=%d,msg=0x%08x,msg_str=[%s]", id_type->id, event->message_id, msg);
    else
        DEBUG("event_id=%d, msg=0x%08x", id_type->id, event->message_id);
}
#endif /* INCLUDE_LTE_EMM_ESM_MSG */

#ifdef INCLUDE_IMS_VOIP_MSG
/*
 [EVENT] Capture SIP session ringing.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_ims_sip_session_ringing)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);

    /* line statistics - connected calls */
    if (!linestats_info.direction) {
        linestats_info.IncomingCallsConnected++;
        DEBUG("[line-stats] IncomingCallsConnected++ (calls=%lld)", linestats_info.IncomingCallsConnected);
    } else {
        linestats_info.OutgoingCallsConnected++;
        DEBUG("[line-stats] OutgoingCallsConnected++ (calls=%lld)", linestats_info.OutgoingCallsConnected);
    }

    linestats_info.ring = 1;

    linestats_info_update_rdb();
}

/*
 [EVENT] Capture SIP session cancel.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_ims_sip_session_cancel)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

/*
 [EVENT] Capture SIP registration start.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_ims_sip_registration_start)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

/*
 [EVENT] Capture SIP registration end.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_ims_sip_register_end)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

/*
 [EVENT] Capture SIP deregistration start.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_ims_sip_deregister_start)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

/*
 [EVENT] Capture SIP deregistration end.

 Parameters:
  standard DCI log parameters.

 Return:
  None.
*/
declare_app_func(event_ims_sip_deregister_end)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

declare_app_func(event_ims_sip_response_recv)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

declare_app_func(event_ims_sip_request_recv)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 qc_call_id = *event;
    DEBUG("event_id=%d, qc_call_id=%u(0x%08x)", id_type->id, qc_call_id, qc_call_id);
}

declare_app_func(event_ims_sip_response_send)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

declare_app_func(event_ims_qipcall_sip_session_event)
{
    struct event_ims_qipcall_sip_session_event_t {
        uint8 len;
        uint32 cm_call_id;
        uint8 event_message_id;
        uint8 cause_id;
    } PACK();

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_ims_qipcall_sip_session_event_t);

    DEBUG("event_id=%d, len=%d, call_id=%d,msg_id=%d,cause_id=%d", id_type->id, event->len, event->cm_call_id, event->event_message_id,
          event->cause_id);

    /* if incoming or outgoing session */
    switch (event->event_message_id) {
        case SIP_SESSION_INCOMING_EV:
        case SIP_SESSION_OUTGOING_EV:
            /* get direction */
            linestats_info.direction = event->event_message_id == SIP_SESSION_OUTGOING_EV;

            /* line statistics - received or attempted calls */
            if (!linestats_info.direction) {
                linestats_info.IncomingCallsReceived++;
                DEBUG("[line-stats] IncomingCallsReceived++ (calls=%lld)", linestats_info.IncomingCallsReceived);
            } else {
                linestats_info.OutgoingCallsAttempted++;
                DEBUG("[line-stats] OutgoingCallsAttempted++ (calls=%lld)", linestats_info.OutgoingCallsAttempted);
            }

            /* reset ring flag */
            linestats_info.ring = 0;

            linestats_info_update_rdb();
            break;

        case SIP_SESSION_RINGING_EV:
            /* nothing to do - done by event_ims_sip_session_ringing */
            break;

        case SIP_SESSION_INCOMING_ESTABLISHED_EV:
        case SIP_SESSION_OUTGOING_ESTABLISHED_EV: {
            int direction;

            struct voicecall_info_t *vci;

            /* update line stats */
            direction = event->event_message_id == SIP_SESSION_OUTGOING_ESTABLISHED_EV;

            if (!direction) {
                linestats_info.IncomingCallsAnswered++;
                DEBUG("[line-stats] IncomingCallsAnswered++ (calls=%lld)", linestats_info.IncomingCallsAnswered);
            } else {
                linestats_info.OutgoingCallsAnswered++;
                DEBUG("[line-stats] OutgoingCallsAnswered++ (calls=%lld)", linestats_info.OutgoingCallsAnswered);
            }

            linestats_info_update_rdb();

            /* process voice call info */

            /* get vci session */
            vci = vci_create_session_by_id(session_id_type_cm, &event->cm_call_id);
            if (!vci) {
                DEBUG("[voicecall-session] session already existing (cm_call_id=%d)", event->cm_call_id);
            } else {
                DEBUG("[voicecall-session] session created (qc_call_id=%u)", event->cm_call_id);

                /* apply start condition to vci */
                voicecall_info_start(vci, ts);
            }

            break;
        }

        case SIP_SESSION_FAILURE_EV:
            if (linestats_info.ring) {
                DEBUG("[line-stats] call setup procedure passed");
                break;
            }

            /* line statistics - failed calls */
            if (!linestats_info.direction) {
                linestats_info.IncomingCallsFailed++;
                DEBUG("[line-stats] IncomingCallsFailed++ (calls=%lld)", linestats_info.IncomingCallsFailed);
            } else {
                linestats_info.OutgoingCallsFailed++;
                DEBUG("[line-stats] OutgoingCallsFailed++ (calls=%lld)", linestats_info.OutgoingCallsFailed);
            }

            linestats_info_update_rdb();
            break;

        case SIP_SESSION_INCOMING_TERMINATED_OK_EV:
        case SIP_SESSION_OUTGOING_TERMINATED_OK_EV: {
            /*
                ## 1st workaround for odd behaviour of Qualcomm IMS stack ##
                MO call that is terminated by network does not generate event_ims_sip_session_terminated (0x597)
            */
            struct voicecall_info_t *vci;

            /* search cm call id */
            vci = vci_get_session_by_cm_call_id_mt(event->cm_call_id);
            if (!vci) {
                DEBUG("no session with cm call id#%d found", event->cm_call_id);
                goto sip_session_outgoing_terminated_ok_ev_fini;
            }

            voicecall_info_stop(vci, ts);

            /* return vci session to poll */
            DEBUG("[voicecall-session] delete vci (index=%d)", vci->index);
            vci_delete_session(vci);

        sip_session_outgoing_terminated_ok_ev_fini:
            __noop();
            break;
        }
    }
}

declare_app_func(event_ims_sip_request_send)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

declare_app_func(event_ims_sip_session_failure)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 msg = *event;
    DEBUG("event_id=%d, msg=0x%08x", id_type->id, msg);
}

declare_app_func(event_ims_sip_session_established)
{
    struct voicecall_info_t *vci;

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 qc_call_id = *event;
    DEBUG("event_id=%d, qc_call_id=%u(0x%08x)", id_type->id, qc_call_id, qc_call_id);

    /* get vci session */
    vci = vci_create_session_by_id(session_id_type_qc, &qc_call_id);
    if (!vci) {
        DEBUG("[voicecall-session] session already existing (qc_call_id=%u)", qc_call_id);
    } else {
        DEBUG("[voicecall-session] session created (qc_call_id=%u)", qc_call_id);
        /* apply start condition to vci */
        voicecall_info_start(vci, ts);
    }
}

declare_app_func(event_ims_sip_session_terminated)
{
    struct voicecall_info_t *vci;

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    uint32 qc_call_id = *event;
    DEBUG("event_id=%d, qc_call_id=%u", id_type->id, qc_call_id);

    /* get qc call id */
    vci = vci_get_session_by_qc_call_id_mt(qc_call_id);
    if (!vci) {
        DEBUG("qc call id not found (qc_call_id=%u)", qc_call_id);
        goto err;
    }

    /* apply stop condition to vci */
    voicecall_info_stop(vci, ts);

    /* return vci session to poll */
    DEBUG("[voicecall-session] delete vci (index=%d)", vci->index);
    vci_delete_session(vci);

err:
    __noop();
}

declare_app_func(voip_session_setup_summary)
{
    struct voip_session_setup_summary_t {
        uint8 version;
        struct variable_string_t dialed;
        uint8 direction;
        struct variable_string_t callid;
        uint32 type;
        struct variable_string_t originating_uri;
        struct variable_string_t terminating_uri;
        uint16 result;
        uint32 call_set_up_delay;
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(struct voip_session_setup_summary_t);

    struct voicecall_info_t *vci;

    char *p = (char *)log;
    struct variable_string_t *str;
    const char *sip_call_id;

    struct variable_string_t *dialed;
    uint8 direction;
    struct variable_string_t *callid;

    char var_str[VOICECALL_ITEM_MAX_LENGTH];
    int var_str_len = sizeof(var_str);

    /* skip version */
    p += sizeof(log->version);

    /* get last call number */
    str = (struct variable_string_t *)p;
    dialed = str;
    p += sizeof(*str) + str->length;

    /* get direction - MO=0, MT=1 */
    direction = *p;
    p += sizeof(log->direction);

    /* get call id */
    str = (struct variable_string_t *)p;
    callid = str;
    p += sizeof(*str) + str->length;

    /* get session */
    sip_call_id = get_str_from_var(var_str, var_str_len, callid->string, callid->length);
    vci = vci_get_session_by_sip_call_id_mt(sip_call_id);
    if (!vci) {
        ERR("session not found (sip_call_id=%s)", sip_call_id);
        goto err;
    }

    DEBUG("[voicecall-session] index = %d, sip call id = %s", vci->index, vci->sip_call_id);

    /* store previous settings */
    __strncpy(vci->LastCallNumber, sizeof(vci->LastCallNumber), dialed->string, dialed->length);
    DEBUG("LastCallNumber=%s #%d", vci->LastCallNumber, dialed->length);
    strncpy(vci->CallDirection, direction ? "incoming" : "outgoing", sizeof(vci->CallDirection));
    DEBUG("CallDirection=%s", vci->CallDirection);

    /* skip type */
    p += sizeof(log->type);

    /* get originating uri */
    str = (struct variable_string_t *)p;
    __strncpy(vci->OriginatingURI, sizeof(vci->OriginatingURI), str->string, str->length);
    p += sizeof(*str) + str->length;
    DEBUG("OriginatingURI=%s #%d", vci->OriginatingURI, str->length);

    /* get terminating uri */
    str = (struct variable_string_t *)p;
    __strncpy(vci->TerminatingURI, sizeof(vci->TerminatingURI), str->string, str->length);
    p += sizeof(*str) + str->length;
    DEBUG("TerminatingURI=%s #%d", vci->TerminatingURI, str->length);

/* sip result code is only by ims message */
#if 0
    /* get SIP result code */
    vci->SIPResultCode = *(uint16*)p;
    p += sizeof(log->result);
    DEBUG("SIPResultCode=%d", vci->SIPResultCode);
#endif

err:
    __noop();
}

declare_app_func(voip_session_end_summary)
{
    struct voip_session_end_summary_t {
        uint8 version;
        struct variable_string_t dialed_string;
        uint8 direction;
        struct variable_string_t callid;
        uint32 type;
        struct variable_string_t originating_uri;
        struct variable_string_t terminating_uri;
        uint16 end_cause;
        uint32 call_set_up_delay;
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(struct voip_session_end_summary_t);

    char *p = (char *)log;
    struct variable_string_t *str;
    int end_cause;

    char var_str[VOICECALL_ITEM_MAX_LENGTH];
    int var_str_len = sizeof(var_str);
    const char *sip_call_id;

    struct voicecall_info_t *vci;

    /* skip version */
    p += sizeof(log->version);

    /* skip dialed string */
    str = (struct variable_string_t *)p;
    p += sizeof(*str) + str->length;

    /* skip direction */
    p += sizeof(log->direction);

    /* skip call id */
    str = (struct variable_string_t *)p;

    /* get sip call id */
    sip_call_id = get_str_from_var(var_str, var_str_len, str->string, str->length);
    if (!*sip_call_id) {
        ERR("no sip call id found in IMS SIP");
        goto err;
    }

    p += sizeof(*str) + str->length;

    /* skip type */
    p += sizeof(log->type);

    /* get originating uri */
    str = (struct variable_string_t *)p;
    p += sizeof(*str) + str->length;

    /* get terminating uri */
    str = (struct variable_string_t *)p;
    p += sizeof(*str) + str->length;

    end_cause = *(uint16 *)p;

    /*
        0: MO initiated
        1: MT initiated
        2: RTP inactivity
        3: RTCP inactivity
        4: SRVCC
        5: radio link failure
    */

    switch (end_cause) {
        case 0:
        case 1:
            DEBUG("[line-stats] call end (end_cause=%s)", !end_cause ? "MO initiated" : "MT initiated");
            break;

        default:
            linestats_info.CallsDropped++;
            DEBUG("[line-stats] call drop (end_cause=%d,calls=%lld)", end_cause, linestats_info.CallsDropped);
            linestats_info_update_rdb();
            break;
    }

    /* get session */
    vci = vci_get_session_by_sip_call_id_mt(sip_call_id);
    if (!vci) {
        DEBUG("[voicecall-session] session not found (sip_call_id=%s)", sip_call_id);
        goto err;
    }

    DEBUG("[voicecall-session] end summary detected, prepare to terminate (index=%d,sip_call_id=%s)", vci->index, sip_call_id);
    vci->session_end_summary = 1;

err:
    __noop();
}

declare_app_func(ims_registration_summary)
{
    struct ims_registration_summary_t {
        uint8 version;
        uint8 type;
        struct variable_string_t callid;
        struct variable_string_t request_uri;
        struct variable_string_t to_header;
        uint16 result;
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(struct ims_registration_summary_t);
}

/*
        [LOG] IMS SIP Message (0x156E)
        This log packet is generated every time a IMS message is sent or received.
*/
declare_app_func(ims_message)
{
    struct ims_message_t {
        uint8 version;
        uint8 direction;
        uint8 sdp_presence;
        uint8 sip_call_id_length;
        uint16 sip_message_length;
        uint16 sip_message_logged_bytes;
        uint16 message_id;
        uint16 response_code;
        uint32 cm_call_id;
        uint8 sip_call_id[0];
        uint8 sip_message[0];
    } PACK();

    static const char *sip_msg_names[] = {
        [0] = "UNKNOWN", [1] = "REGISTER", [2] = "INVITE", [3] = "PRACK",    [4] = "CANCEL", [5] = "ACK",      [6] = "BYE",      [7] = "SUBSCRIBE",
        [8] = "NOTIFY",  [9] = "UPDATE",   [10] = "REFER", [11] = "MESSAGE", [12] = "INFO",  [13] = "PUBLISH", [14] = "OPTIONS",
    };

    const char *sip_msg_name;
    const char *sip_msg_dir;

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(struct ims_message_t);

    struct voicecall_info_t *vci;
    const char *sip_call_id;

    char var_str[VOICECALL_ITEM_MAX_LENGTH];
    int var_str_len = sizeof(var_str);

    /* get sip_call_id */
    sip_call_id = get_str_from_var(var_str, var_str_len, log->sip_call_id, log->sip_call_id_length);
    if (!*sip_call_id) {
        ERR("no sip call id found in IMS SIP");
        goto err;
    }

    DEBUG("[voicecall-session] ims sip, message_id=0x%04x, response_code=%d", log->message_id, log->response_code);

    /* get SIP message name */
    if (log->message_id < __countof(sip_msg_names)) {
        sip_msg_name = sip_msg_names[log->message_id];
    } else {
        sip_msg_name = "UNKNOWN";
    }

    /* get SIP message direction */
    sip_msg_dir = log->direction ? "UE --> NW" : "NW --> UE";

    DEBUG("[IMS SIP] %s : msg='%s' (%u), rsp=%u", sip_msg_dir, sip_msg_name, log->message_id, log->response_code);

    /* if IMS_SIP_INVITE and OK */
    if ((log->message_id == 0x0002) && (log->response_code == 200)) {
        /* get vci session */
        vci = vci_create_session_by_id(session_id_type_sip, sip_call_id);
        if (!vci) {
            DEBUG("[voicecall-session] session already existing (sip_call_id=%s)", sip_call_id);
        } else {
            DEBUG("[voicecall-session] session created (index=%d,sip_call_id=%s)", vci->index, sip_call_id);
            /* apply start condition to vci */
            voicecall_info_start(vci, ts);
        }
    }

    /* search sip call id */
    vci = vci_get_session_by_sip_call_id_mt(sip_call_id);
    if (!vci) {
        DEBUG("[voicecall-session] no session found with sip call id (sip_call_id=%s)", sip_call_id);
        goto err;
    }

    /* get SIP result code */
    if (log->response_code) {
        vci->SIPResultCode = log->response_code;
        DEBUG("[voicecall-session] update SIP result (index=%d,response_code=%d,sip_call_id=%s)", vci->index, log->response_code, vci->sip_call_id);
    }

err:
    __noop();
}

declare_app_func(ims_rtcp_packet)
{
    struct sdes_t {
        uint32 ssrc;
        uint8 cname;
        struct variable_string_t cname_str;
    } PACK();

    struct report_block_t {
        uint32 ssrc;
        uint32 fraction_lost : 8;
        uint32 cumulative_number_of_packets_lost : 24;
        uint32 extended_highest_sequence_number_received;
        uint32 interarrival_jitter;
        uint32 last_sr;
        uint32 delay_since_last_sr;
        uint16 roundtrip_time;
        uint8 padding;
        uint8 sc;
        uint8 packet_type;
        uint8 length;

        struct sdes_t sdes[0];
    } PACK();

    struct ims_rtcp_packet_receiver_report_t {
        uint8 p;
        uint8 rc;
        uint8 packet_type;
        uint16 length;
        uint32 ssrc_of_sender;

        struct report_block_t report_block[0];
    } PACK();

    struct ims_rtcp_packet_sender_report_t {
        uint8 p;
        uint8 rc;
        uint8 packet_type;
        uint16 length;
        uint32 ssrc_of_sender;

        uint32 ntp_timestamp_high;
        uint32 ntp_timestamp_low;
        uint32 rtp_timestamp;
        uint32 sender_packet_count;
        uint32 sender_octet_count;

        struct report_block_t report_block[0];
    } PACK();

    union packet_t
    {
        struct ims_rtcp_packet_sender_report_t sender_report;
        struct ims_rtcp_packet_receiver_report_t receiver_report;
    } PACK();

    struct ims_rtcp_packet_t {
        uint8 version;
        uint8 direction : 3;
        uint8 rat_type : 5;
        uint8 report_type;
        uint8 codec_type;

        union packet_t packet;
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(4);
    declare_app_func_define_var_log(struct ims_rtcp_packet_t);

    int i;
    struct report_block_t *report_block;
    int rc;

#ifdef DEBUG_VERBOSE
    char *dir_name;
#endif

    struct voicecall_info_t *vci;
    unsigned int ssrc;

    VERBOSE("p=%d", log->packet.sender_report.p);

    /* 0: Network->UE, 1:UE->Network */
    VERBOSE("dir=%d", log->direction);
    /* 0: Sender report (SR), 1:Receiver report (RR) */
    VERBOSE("report type=%d", log->report_type);
    VERBOSE("rc=%d", log->packet.sender_report.rc);

    /* get report block */
    report_block = !log->report_type ? log->packet.sender_report.report_block : log->packet.receiver_report.report_block;
    rc = !log->report_type ? log->packet.sender_report.rc : log->packet.receiver_report.rc;

    /* if return SR or RR */
    if (!log->direction) {
        /* get ssrc */
        ssrc = !log->report_type ? log->packet.sender_report.ssrc_of_sender : log->packet.receiver_report.ssrc_of_sender;
        /* create session based on ssrc */
        VERBOSE("[voicecall-session] rtcp dir=rx,ssrc=%u", ssrc);
        vci_create_session_by_id(session_id_type_ssrc_rx, &ssrc);

        /* get session */
        vci = vci_get_session_by_ssrc_rx_id_mt(ssrc);
        if (!vci) {
            ERR("session not found (ssrc=%u)", ssrc);
            goto err;
        }

        VERBOSE("[voicecall-session] index = %d, sip call id = %s", vci->index, vci->sip_call_id);

        for (i = 0; i < rc; i++) {
            VERBOSE("roundtrip_time#%d=%d", i, report_block->roundtrip_time);
            avg_feed(&vci->AvgRTPLatency, report_block->roundtrip_time);
        }

        VERBOSE("AvgRTPLatency=%.2Lf", vci->AvgRTPLatency.avg);
    }

/* get direction */
#ifdef DEBUG_VERBOSE
    dir_name = !log->direction ? "DL" : "UL";
#endif
    for (i = 0; i < rc; i++) {
        VERBOSE("[RTCP-%s, i=%d, fraction_lost=%d,cumulative_packets_lost=%d", dir_name, i, report_block->fraction_lost,
                report_block->cumulative_number_of_packets_lost);
    }

err:
    __noop();
    return;
}

declare_app_func(ims_rtp_sn_and_payload_log_packet)
{
    struct audio_t {
        uint8 marker;
        uint8 rtp_payload_header_cmr;
        uint8 rtp_payload_header_f;
        uint8 rtp_payload_header_ft_index;
        uint8 rtp_payload_header_frame_quality_indicator;
        uint8 reserved;
    } PACK();

    union payload_t
    {
        /* do not use following structures as we are not able to verify them */

        /*
        struct video_codec_type_2_t {
                uint8 marker;
                uint8 rtp_payload_header_f;
                uint8 rtp_payload_header_nri;
                uint8 rtp_payload_header_payload_type;
                uint8 iframe_packet_indicator;
        } PACK();

        struct video_codec_type_4_t {
                uint8 marker;
                uint8 rtp_payload_header_f;
                uint8 rtp_payload_header_p;
                uint8 rtp_payload_header_src_quant;
                uint8 iframe_packet_i;
        } PACK();

        struct video_codec_type_8_t {
                uint8 rtp_payload_header_f;
                uint8 rtp_payload_header_type;
                uint8 rtp_payload_header_payload_type;
                uint8 layer_id;
                uint8 tid;
        } PACK();
        */

        struct audio_t audio;
    } PACK();

    struct ims_rtp_sn_and_payload_log_packet_t {
        uint8 version;
        uint8 direction : 3;
        uint8 rat_type : 5;
        uint16 rtp_sequence_number;
        uint32 ssrc;
        uint32 rtp_timestamp;
        uint8 codec_type;
        uint8 media_type;
        uint16 rtp_payload_size;
        uint16 rtp_logged_payload_size;

        union payload_t payload;

        uint8 latency_info_present;
        char reserved[10];
        uint8 rtp_raw_payload[0];
        uint8 rtp_redundant_indicator;
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(9);
    declare_app_func_define_var_log(struct ims_rtp_sn_and_payload_log_packet_t);

    const char *codec_type = NULL;
    const char *codec_type_names[] = {
        [0] = "AMR",         [1] = "AMR-WB", [2] = "H.264", [3] = "H.263 (1998 and 2000)", [4] = "H.263 (1996)", [5] = "G.711 u-law",
        [6] = "G.711 a-law", [7] = "EVS",    [8] = "H.265"
    };

#if 0
    struct rtp_header_t* rtp;
#endif

    int direction;
    unsigned int rtp_timestamp;
    /* ctime for rtp timestamp */
    time_t rtp_ctime_timestamp;

    struct voicecall_info_t *vci;
    unsigned int ssrc;

    /* bypass if not audio */
    if (log->media_type) {
        ERR("video media type detected (media_typ=%d)", log->media_type);
        goto fini;
    }

    /* get direction - 0:Network->UE, 1:UE->Network */
    direction = log->direction;

    /* create session based on ssrc */
    ssrc = log->ssrc;
    VERBOSE("[voicecall-session] rtp dir=%s,ssrc=%u", direction ? "rx" : "tx", ssrc);
    if (direction)
        vci_create_session_by_id(session_id_type_ssrc_tx, &ssrc);
    else
        vci_create_session_by_id(session_id_type_ssrc_rx, &ssrc);

    /* get session */
    ssrc = log->ssrc;
    if (direction)
        vci = vci_get_session_by_ssrc_tx_id_mt(ssrc);
    else
        vci = vci_get_session_by_ssrc_rx_id_mt(ssrc);

    if (!vci) {
        VERBOSE("session not found (ssrc=%u,dir=%d)", ssrc, direction);
        goto err;
    }

    VERBOSE("[voicecall-session] index = %d, sip call id = %s", vci->index, vci->sip_call_id);

    /* get codec type */
    if (!*vci->CodecGSMUMTS) {
        if (__in_array(log->codec_type, codec_type_names))
            codec_type = codec_type_names[log->codec_type];
        if (!codec_type)
            codec_type = "unknown";
        strncpy(vci->CodecGSMUMTS, codec_type, sizeof(vci->CodecGSMUMTS));
        DEBUG("codec_type=%s #%d", vci->CodecGSMUMTS, sizeof(log->codec_type));
    }

/* to get payload type, use voice call statistics 0x17f2 */
#if 0
    /* get payload type */
    rtp = (struct rtp_header_t*)log->rtp_raw_payload;
    vci->PayloadType = rtp->payload;
    DEBUG("PayloadType=%d", vci->PayloadType);
#endif

    /* get last rtp time timestamp */
    rtp_timestamp = log->rtp_timestamp;
    /* convert last rtp QxDM timestamp into C time */
    rtp_ctime_timestamp = get_ctime_from_ts(ts);
    if (direction) {
        vci->InboundLastRTPTime = rtp_timestamp;
        vci->InboundLastRTPTod = rtp_ctime_timestamp;
    } else {
        vci->OutboundLastRTPTime = rtp_timestamp;
        vci->OutboundLastRTPTod = rtp_ctime_timestamp;
    }

    VERBOSE("latency_info_present=%d", log->latency_info_present);
fini:
    __noop();
    return;

err:
    __noop();
    return;
}

static int reverse_ipaddr(char *dst, int dst_len, const char *src, int src_len)
{
    uint16 *d = (uint16 *)dst;
    const uint16 *s = (const uint16 *)src;
    int s_l = src_len / 2;
    int i;

    if (dst_len < src_len)
        goto err;

    for (i = 0; i < s_l; i++)
        d[i] = s[i];

    return 0;
err:
    return -1;
}

declare_app_func(voice_call_statistics)
{
    struct voice_call_statistics_t {
        uint8 version;
        uint16 sip_call_duration;
        uint8 codec_type;
        uint32 tx_ssrc;
        uint32 rx_ssrc;
        uint32 number_of_tx_rtp_packets;
        uint32 number_of_rx_rtp_packets;
        uint32 number_of_rx_lost_rtp_packets;
        uint32 average_inter_arrival_jitter;
        uint32 max_inter_arrival_jitter;
        uint32 average_instantaneous_jitter;
        uint32 max_instantaneous_jitter;
        uint32 number_of_frames_received;
        uint16 number_of_frames_not_enqueued;
        uint16 number_of_frames_underflow;
        uint16 average_underflow_rate;
        uint16 average_frame_delay;
        uint16 max_frame_delay;
        uint16 average_q_size;
        uint16 average_target_delay;
        uint8 tx_rtp_bitrate;
        uint8 rx_rtp_bitrate;
        uint32 tx_rtp_payload_bytecount;
        uint32 rx_rtp_payload_bytecount;
        uint32 max_delta;
        uint16 max_delta_lmax;
        uint16 max_delta_lmin;
        uint8 tx_payload_type;
        uint8 rx_payload_type;
        uint8 ip_version;
        char rx_src_ip_addr[0];
        uint32 rx_dst_port_addr;
        char tx_dst_ip_addr[0];
        uint32 tx_dst_port_addr;
        uint16 call_stat;
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(6);
    declare_app_func_define_var_log(struct voice_call_statistics_t);

    struct voicecall_info_t *vci;

    VERBOSE("tx packets=%d", log->number_of_tx_rtp_packets);
    VERBOSE("rx packets=%d", log->number_of_rx_rtp_packets);
    VERBOSE("rx lost packets=%d", log->number_of_rx_lost_rtp_packets);
    VERBOSE("offset of ip_version=%d", __offset(struct voice_call_statistics_t, ip_version));

    /* get session */
    vci = vci_get_session_by_ssrc_tx_id_mt(log->tx_ssrc);
    if (!vci)
        vci = vci_get_session_by_ssrc_rx_id_mt(log->rx_ssrc);

    if (!vci) {
        ERR("session not found (tx ssrc=%u, rx ssrc=%u)", log->tx_ssrc, log->rx_ssrc);
        goto err;
    }

    DEBUG("[voicecall-session] index = %d, sip call id = %s", vci->index, vci->sip_call_id);

    /* get payload type */
    vci->PayloadType = log->rx_payload_type;
    DEBUG("PayloadType=%d", vci->PayloadType);
    vci->PayloadType = log->rx_payload_type;

    /*
        MPSS.TH.2.0.c1-00234-M9645LAAAANAZM-1
    */
    if (!log->number_of_tx_rtp_packets && !log->number_of_rx_lost_rtp_packets) {
        DEBUG("[voicecall-session] blank voice statistics detected, ignore");
    } else {
        DEBUG("[voicecall-session] set voice statistics");

        vci->MaxReceiveInterarrivalJitter = log->max_inter_arrival_jitter * 1000;
        vci->AvgReceiveInterarrivalJitter = log->average_inter_arrival_jitter * 1000;

        /* inbound rtp packet statistics */
        vci->InboundLostRTPPackets = log->number_of_rx_lost_rtp_packets;
        /* number of discarded RTP packets is calculated based on average frame numbers per packet - not accurate but average */
        vci->InboundDejitterDiscardedFrames = log->number_of_frames_not_enqueued;
        vci->InboundDejitterDiscardedRTPPackets =
            !log->number_of_frames_received ? 0
                                            : (log->number_of_rx_rtp_packets * log->number_of_frames_not_enqueued / log->number_of_frames_received);
        vci->InboundDecoderDiscardedRTPPackets = 0; /* According to Qualcomm, MTP decoder does not discard a packet */

        /* inbound and outbound total packets */
        vci->InboundTotalRTPPackets = log->number_of_rx_rtp_packets;
        vci->OutboundTotalRTPPackets = log->number_of_tx_rtp_packets;

        /* inbound and outbound average packet sizes */
        vci->OutboundCumulativeAveragePacketSize =
            !log->number_of_tx_rtp_packets ? 0 : (log->tx_rtp_payload_bytecount / log->number_of_tx_rtp_packets);
        vci->InboundCumulativeAveragePacketSize =
            !log->number_of_rx_rtp_packets ? 0 : (log->rx_rtp_payload_bytecount / log->number_of_rx_rtp_packets);

#if 0

        /* do not use call duration - invalid duration is given for E911 */

        /* store sip duration */
        DEBUG("[voicecall-session] sip call duration (index = %d, duration = %d)", vci->index, log->sip_call_duration);
        vci->sip_call_duration = log->sip_call_duration;
#endif

        /* accumulate line statistics */
        linestats_info.BytesSent += log->tx_rtp_payload_bytecount;
        linestats_info.BytesReceived += log->rx_rtp_payload_bytecount;
        linestats_info.PacketsReceived += log->number_of_rx_rtp_packets;
        linestats_info.PacketsSent += log->number_of_tx_rtp_packets;
        linestats_info.PacketsLost += log->number_of_rx_lost_rtp_packets;
        linestats_info.Overruns += log->number_of_frames_not_enqueued;
        linestats_info.Underruns += log->number_of_frames_underflow;
    }

    char addr_len;
    int ip_version;
    char *p;
    char addr[INET6_ADDRSTRLEN];
    int port;
    char saddr[16];

    ip_version = log->ip_version;
    /* 0: IPv4, 1: IPv6 */
    addr_len = !ip_version ? 4 : 16;
    /* skip rx src ip addr */
    p = log->rx_src_ip_addr;

    /* get rx src ip addr */
    reverse_ipaddr(saddr, sizeof(saddr), p, addr_len);
    if (!ip_version) {
        inet_ntop(AF_INET, p, addr, INET_ADDRSTRLEN);
    } else {
        inet_ntop(AF_INET6, saddr, addr, INET6_ADDRSTRLEN);
    }

    p += addr_len;
    /* skip rx dst port addr */
    p += sizeof(log->rx_dst_port_addr);

    /* skip tx dst ip addr */
    p += addr_len;

    strncpy(linestats_info.CurrCallRemoteIP, addr, sizeof(linestats_info.CurrCallRemoteIP));
    strncpy(linestats_info.LastCallRemoteIP, addr, sizeof(linestats_info.LastCallRemoteIP));

    /* get tx dst port */
    port = *(uint32 *)p;

    DEBUG("addr=%s, port=%d", addr, port);

    /* if IMS_SIP_BYTE and OK */
    if (vci->session_end_summary) {
        /*
                ## 2nd workaround for odd behaviour of Qualcomm IMS stack ##
                terminated MT call occationally does not event_ims_sip_session_terminated (0x597)
        */

        DEBUG("[voicecall-session] terminate call by end summary (index=%d)", vci->index);
        voicecall_info_stop(vci, ts);

        /* return vci session to poll */
        DEBUG("[voicecall-session] delete vci (index=%d)", vci->index);
        vci_delete_session(vci);
    }

err:
    __noop();
    return;
}

declare_app_func(event_ims_sip_session_start)
{
    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint32);

    struct voicecall_info_t *vci;

    uint32 qc_call_id = *event;
    DEBUG("event_id=%d, msg=%u(0x%08x)", id_type->id, qc_call_id, qc_call_id);

    /* search vci */
    vci = vci_get_session_by_qc_call_id_mt(qc_call_id);
    if (!vci) {
        ERR("session not found (qc_call_id=%u)", qc_call_id);
        goto err;
    }

err:
    __noop();
}
#endif /* INCLUDE_IMS_VOIP_MSG */

#ifdef INCLUDE_COMMON_MSG
declare_app_func(event_lte_rrc_new_cell_ind)
{
    unsigned int i, newCellFlag = 1;
    time_t event_time = get_monotonic_ms();

    struct event_lte_rrc_new_cell_ind_t {
        char len;
        char cause;
        unsigned short frequency;
        unsigned short cell_id;
    } PACK();

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_rrc_new_cell_ind_t);

    rdb_enter_csection();
    {
#ifdef V_RF_MEASURE_REPORT_myna
        if (event->cell_id != rrc_info.PCI) {
            cell_based_data_collection(rrc_info.PCI);
        }
#endif
        DEBUG("cause = %d", event->cause);
        DEBUG("frequency = %d", event->frequency);
        DEBUG("cell_id = %d", event->cell_id);

        switch (event->cause) {
            case 0x00: /* cell selection */
                break;

            case 0x02: /* hand-over */
                servcell_info.HandoverCount++;
                INFO("HandoverCount = %llu", servcell_info.HandoverCount);
#ifdef V_RF_MEASURE_REPORT_myna
                servcell_info.HandoverAttempt++;
                INFO("HandoverAttempt = %llu", servcell_info.HandoverAttempt);
#endif
                break;

            case 0x03: /* redirection */
                break;
        }

        rrc_info.PCI = event->cell_id;

        /* obtain new PCC to be reported in ca_mode and cell_usage
           if this is a handover, calculate timing for previous pcell  */
        if (caUsage.usedNumPCells && (caUsage.pCellState != CA_STATE_DISCONNECTED)) {
            caUsage.pCell[caUsage.currentPCellIndex].duration[caUsage.pCellState - 1] += event_time - caUsage.pCellStartTime;
        }

        /* check if pcell already being logged */
        for (i = 0; i < caUsage.usedNumPCells; i++) {
            if ((event->frequency == caUsage.pCell[i].freq) && (event->cell_id == caUsage.pCell[i].pci)) {
                caUsage.currentPCellIndex = i;
                newCellFlag = 0;
                break;
            }
        }

        /* log a new pcell if needed  */
        if (newCellFlag && (caUsage.usedNumPCells < MAX_NUM_P_CELLS)) {
            caUsage.currentPCellIndex = caUsage.usedNumPCells;
            caUsage.usedNumPCells++;
            caUsage.pCell[caUsage.currentPCellIndex].freq = event->frequency;
            caUsage.pCell[caUsage.currentPCellIndex].pci = event->cell_id;
        }

        /* Set state for new RRC connection. Keep the same state in case of handover */
        if (caUsage.pCellState == CA_STATE_DISCONNECTED) {
            caUsage.pCellState = CA_STATE_PCC;
        }
        caUsage.pCellStartTime = event_time;

        rdb_leave_csection();
    }
}

#ifdef V_RF_MEASURE_REPORT_myna
declare_app_func(event_lte_rrc_handover_failure_ind)
{
    struct event_lte_rrc_handover_failure_ind_t {
        char len;
        unsigned short frequency;
        unsigned short cell_id;
        char cause;
    } PACK();

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_rrc_handover_failure_ind_t);

    rdb_enter_csection();
    {
        DEBUG("cause = %d", event->cause);
        DEBUG("frequency = %d", event->frequency);
        DEBUG("cell_id = %d", event->cell_id);

        switch (event->cause) {
            case 0x00: /* NONE - N/A for handover failure */
                break;

            case 0x01: /* INVALID_CFG - Validation failure on RRC Conn Reconfig message */
            case 0x02: /* CPHY - Physical layre failure on the target cell */
            case 0x03: /* RACH - T304 expiry due to RACH failure on target cell */
            case 0x04: /* RACH_MEAS - T304 expirey due to RACH failure on target cell */
                servcell_info.HandoverAttempt++;
                INFO("HandoverAttempt = %llu", servcell_info.HandoverAttempt);
                break;
        }

        rdb_leave_csection();
    }
}
#endif

declare_app_func(event_lte_rrc_dl_msg)
{
    struct event_lte_rrc_dl_msg_t {
        uint8 channel_type;
        uint8 message_type;
    } PACK();

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_rrc_dl_msg_t);

    rdb_enter_csection();
    {

        /*
                0x01 – DL_BCCH
                0x02 – DL_PCCH
                0x03 – DL_CCCH
                0x04 – DL_DCCH
        */

        switch (event->message_type) {

            case 0x49: /* RRCConnectionReestablishmentReject */
                servcell_info.NumberofRRCEstabFailures++;
                DEBUG("[RRC-DL] connection reestablishment reject, NumberofRRCEstabFailures=%lld", servcell_info.NumberofRRCEstabFailures);
                break;

            case 0x4A: /* RRCConnectionReject */
                servcell_info.NumberofRRCReEstabFailures++;
                DEBUG("[RRC-DL] connection reject, NumberofRRCEstabFailures=%lld", servcell_info.NumberofRRCReEstabFailures);
                break;

            case 0x4B: /* RRCConnectionSetup */
                DEBUG("[RRC-DL] connection setup");
                break;

            case 0x48: /* RRCConnectionReestablishment */
                DEBUG("[RRC-DL] connection reestablishment");
                break;

            case 0x85: /* RRCConnectionRelease */
                DEBUG("[RRC-DL] rrc cconnection release");
                break;

            case 0x00: /* MasterInformationBlock */
                DEBUG("[RRC-DL] master information block");
                break;

            case 0x01: /* SystemInformationBlockType1 */
                DEBUG("[RRC-DL] system information block type 1");
                break;

            case 0x02: /* SystemInformationBlockType2 */
                DEBUG("[RRC-DL] system information block type 2");
                break;

            case 0x03: /* SystemInformationBlockType3 */
                DEBUG("[RRC-DL] system information block type 3");
                break;

            case 0x04: /* SystemInformationBlockType4 */
                DEBUG("[RRC-DL] system information block type 4");
                break;

            case 0x05: /* SystemInformationBlockType5 */
                DEBUG("[RRC-DL] system information block type 5");
                break;

            case 0x06: /* SystemInformationBlockType6 */
                DEBUG("[RRC-DL] system information block type 6");
                break;

            case 0x07: /* SystemInformationBlockType7 */
                DEBUG("[RRC-DL] system information block type 7");
                break;

            case 0x08: /* SystemInformationBlockType8 */
                DEBUG("[RRC-DL] system information block type 8");
                break;

            case 0x09: /* SystemInformationBlockType9 */
                DEBUG("[RRC-DL] system information block type 9");
                break;

            case 0x0A: /* SystemInformationBlockType10 */
                DEBUG("[RRC-DL] system information block type 10");
                break;

            case 0x0B: /* SystemInformationBlockType11 */
                DEBUG("[RRC-DL] system information block type 11");
                break;

            case 0x40: /* Paging */
                DEBUG("[RRC-DL] paging");
                break;

            case 0x80: /* CSFBParametersRequestCDMA2000 */
                DEBUG("[RRC-DL] csfb parameters request cdma2000");
                break;

            case 0x81: /* DLInformationTransfer */
                DEBUG("[RRC-DL] dl information transfer");
                break;

            case 0x82: /* HandoverFromEUTRAPreparationRequest */
                DEBUG("[RRC-DL] handover from eutra preparation request");
                break;

            case 0x83: /* MobilityFromEUTRACommand */
                DEBUG("[RRC-DL] mobility frome utra command");
                break;

            case 0x84: /* RRCConnectionReconfiguration */
                DEBUG("[RRC-DL] rrc connection reconfiguration");
                break;

            case 0x86: /* SecurityModeCommand */
                DEBUG("[RRC-DL] security mode command");
                break;

            case 0x87: /* UECapabilityEnquiry */
                DEBUG("[RRC-DL] ue capability enquiry");
                break;

            case 0x88: /* CounterCheck */
                DEBUG("[RRC-DL] counter check");
                break;

            default:
                DEBUG("[RRC-DL] unknown message_type=0x%04x", event->message_type);
                break;
        }

        rdb_leave_csection();
    }
}

declare_app_func(event_lte_rrc_ul_msg)
{
    struct event_lte_rrc_ul_msg_t {
        uint8 channel_type;
        uint8 message_type;
    } PACK();

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_rrc_ul_msg_t);

    rdb_enter_csection();
    {
        time_t duration;
        time_t now = get_monotonic_ms();

        /*
                0x01 – DL_BCCH
                0x02 – DL_PCCH
                0x03 – DL_CCCH
                0x04 – DL_DCCH
        */

        switch (event->message_type) {

            case 0x01: /* RRCConnectionRequest */
                servcell_info.NumberofRRCEstabAttempts++;
                servcell_info.RRCEstabAtemptTimestamp = now;
                DEBUG("[RRC-DL] connection request, NumberofRRCEstabAttempts=%lld", servcell_info.NumberofRRCEstabAttempts);
                break;

            case 0x84: /* RRCConnectionSetupComplete */
                duration = now - servcell_info.RRCEstabAtemptTimestamp;
                DEBUG("[RRC-DL] connection setup complete, duration=%ld", duration);
                avg_feed(&servcell_info.RRCEstabLatency, duration);
                break;

            case 0x00: /* RRCConnectionReestablishmentRequest */
                servcell_info.NumberofRRCReEstabAttempts++;
                servcell_info.RRCReEstabAtemptTimestamp = now;
                DEBUG("[RRC-DL] connection reestablishment request, NumberofRRCReEstabAttempts=%lld", servcell_info.NumberofRRCReEstabAttempts);
                break;

            case 0x83: /* RRCConnectionReestablishmentComplete */
                duration = now - servcell_info.RRCReEstabAtemptTimestamp;
                DEBUG("[RRC-DL] connection reestablishment complete, duration=%ld", duration);
                avg_feed(&servcell_info.RRCReEstabLatency, duration);
                break;

            case 0x80: /* CSFBParametersRequestCDMA2000 */
                DEBUG("[RRC-DL] csfb parameters request cdma2000");
                break;

            case 0x81: /* MeasurementReport */
                DEBUG("[RRC-DL] measurement report");
                break;

            case 0x82: /* RRCConnectionReconfigurationComplete */
                DEBUG("[RRC-DL] rrc connection reconfiguration complete");
                break;

            case 0x85: /* SecurityModeComplete */
                DEBUG("[RRC-DL] security mode complete");
                break;

            case 0x86: /* SecurityModeFailure */
                DEBUG("[RRC-DL] security mode failure");
                break;

            case 0x87: /* UECapabilityInformation */
                DEBUG("[RRC-DL] ue capability information");
                break;

            case 0x88: /* ULHandoverPreparationTransfer */
                DEBUG("[RRC-DL] ul handover preparation transfer");
                break;

            case 0x89: /* ULInformationTransfer */
                DEBUG("[RRC-DL] ul information transfer");
                break;

            case 0x8A: /* CounterCheckResponse */
                DEBUG("[RRC-DL] counter checkre sponse");
                break;

            default:
                DEBUG("[RRC-UL] unknown message_type=0x%04x", event->message_type);
                break;
        }

        rdb_leave_csection();
    }
}

declare_app_func(event_lte_rrc_radio_link_failure_stat)
{
    struct event_lte_rrc_radio_link_failure_stat_t {
        uint8 len;
        uint16 rlf_count_since_rrc_connected;
        uint16 rlf_count_since_lte_active;
        uint16 rlf_cause;
    } PACK();

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(struct event_lte_rrc_radio_link_failure_stat_t);

    rdb_enter_csection();
    {
        DEBUG("rlf count since lte active = %d", event->rlf_count_since_lte_active);
        DEBUG("rlf cause = %d", event->rlf_cause);

        if (!__in_array(event->rlf_cause, servcell_info.FailureCounts)) {
            ERR("out of range (rlf_count=%d,range=%d)", event->rlf_cause, __countof(servcell_info.FailureCounts));
            goto err;
        }

        /* increase cause */
        servcell_info.FailureCounts[event->rlf_cause]++;
        DEBUG("rlf cause count [%d] = %lld", event->rlf_cause, servcell_info.FailureCounts[event->rlf_cause]);

        servcell_info.RLFCount++;
        DEBUG("total rlf = %lld", servcell_info.RLFCount);

        /* keep the last rlf cause - to use when RRC disconnects */
        rrc_info.RRCReleaseCause = event->rlf_cause;

#if 0
        /* do not enable the following block of code - this block is to emulate abnormal termination of QC thread only for test */
#warning !!!! emulation of QC thread abnormal termination by RLF is enabled !!!!
        ERR("!!!! emulate abnormal termination of QC thread !!!!");
        pthread_exit(NULL);
#endif

        rdb_leave_csection();
    }

err:
    rdb_leave_csection();
    return;
}

declare_app_func(event_lte_rrc_radio_link_failure)
{
#if 0
    /*
    	do not use this counter - counter is completely incorrect (mdm version : MPSS.TH.2.0.c1-00200-M9645LAAAANAZM-1)
    	count failure with "event_lte_rrc_radio_link_failure_stat"
    */

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(uint16);

    int counter = *event;

    servcell_info.RLFCount = counter;
    INFO("RLFCount = %d", servcell_info.RLFCount);
#endif
}
#endif /* INCLUDE_COMMON_MSG */

#ifdef NR5G
int assumptive_nr5g_up_cnt = 0;
declare_app_func(event_nr_undoc_updown)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(char);

    char nr_up = event[1];

    DEBUG("NR5G state = %d", nr_up);

    _rdb_prefix_call(_rdb_set_str, "radio_stack.nr5g.up", (nr_up) ? "UP" : "DOWN");
    DEBUG("[NR5G] got event 3191, set radio_stack.nr5g.up %s",  (nr_up) ? "UP" : "DOWN, clear assumptive_nr5g_up_cnt = 0");
    /* Reset the assumptive NR5G up counter upon receiving 3191 event */
    assumptive_nr5g_up_cnt = 0;
    return;
}

declare_app_func(nr5g_mac_pdsch_status_log)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);
    struct PDSCH_Status_Stat {
        unsigned int carrier : 16;
        unsigned int pad1 : 18;     // presumably techid and opcode?
        unsigned int slotagain : 6; // why? I don't know.
        unsigned int conid : 1;     // seems too small to be useful
        unsigned int bandwidth : 5;
        unsigned int bandtype : 1;
        unsigned int bandvar : 1;
        unsigned int pci : 10;
        unsigned int earfcn : 22; //* length suspect, n260 (FR2) gets to 2279165 = 22 bits
        unsigned int tbindex : 5; //* only seen 0
        unsigned int tbsize : 18;
        unsigned int scsmu : 3;
        unsigned int mcs : 6;       // max seen 21
        unsigned int numRB : 9;     // max seen 271
        unsigned int rv : 2;        // max seen 2
        unsigned int harq : 10;     //* length suspect, max seen 15
        unsigned int rnti_type : 1; //* only seen 0
        unsigned int k1 : 6;        //* length suspect, max seen 11
        unsigned int tci : 1;       //* only seen 0
        unsigned int layers : 2;    //* seems too small but bounded? may be power of 2, but only seen 0=1 and 1=2 so far
        unsigned int iter : 1;      //* only seen 0
        unsigned int crcgood : 1;
        unsigned int newTx : 1;
        unsigned int ndi : 1;
        unsigned int pad2 : 5; // presumably discard,bypass but only seen 0 and too many bits!
        unsigned int reTx : 3; // max seen 4
        unsigned int pad3 : 4; // only seen 0 - presumably HD/HARQ on/off load
        unsigned int recomb : 1;
        unsigned int mumimo : 2; // only seen 0
        unsigned int modulation : 2;
        unsigned int pad4 : 12; // presumably highclock,numrx:3(=3 for 4x4) and antmap
    } __attribute__((__packed__));

    struct PDSCH_Status_Rec {
        uint8_t slot;
        uint8_t numerology; // 1 = 30KHz; guessing 15*power of 2
        uint16_t frame;     // max seen 1023
        uint16_t numstat;   //*Only seen 1, so can't be sure what follows is really an array. QXDM thinks it is.
        struct PDSCH_Status_Stat s[1];
    } __attribute__((__packed__));

    struct B887_NR5G_MAC_PDSCH_Status {
        uint16_t minor;
        uint16_t major;
        char pad1[11];
        uint8_t numRec;
        struct PDSCH_Status_Rec d[];
    } __attribute__((__packed__));

    // char *modulations[] = {"QPSK", "?16QAM", "?64QAM", "256QAM"};

    declare_app_func_define_vars_for_log_with_version_type(uint32_t);

    declare_app_func_require_ver(struct_mm_version(2, 4), struct_mm_version(2, 5));
    switch (log_ver) {
        case struct_mm_version(2, 4): {
            declare_app_func_define_var_log(struct B887_NR5G_MAC_PDSCH_Status);

            struct PDSCH_Status_Stat *s = &log->d[0].s[0];

            DEBUG("NR5G nr5g_mac_pdsch_status_log ver %d.%d count %d, frame %d", log->major, log->minor, log->numRec, log->d[0].slot);
            if (log->major != 2 || log->minor != 4 || log->numRec < 1)
                return;
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.scs", 15 << (log->d[0].numerology));
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.pci", s->pci);
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.mcs", s->mcs);
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.layers", 1 << (s->layers));
            break;
        }

        /* struct_mm_version(2, 5) */
        default: {
            struct nr5g_mac_pdsch_status_v2_5 {
                nr5g_struct_version_t version;
                nr5g_log_fields_change_t log_fields_change;

                uint16_t log_fields_change_bmask;
                uint8_t subid;

                uint8_t num_records;
                struct {
                    nr5g_timestamp_t timestamp;

                    uint8_t num_pdsch_status;
                    uint8_t reserved3[3];

                    struct {
                        uint32_t unknown1 : 8; /* TODO: unknown */
                        uint32_t slot : 6;
                        uint32_t sfn : 10;
                        uint32_t unknown2 : 2; /* TODO: unknown */
                        uint32_t bandwidth : 5;
                        uint32_t band_type : 1;

                        uint32_t phyiscal_cell_id : 10;
                        uint32_t earfcn : 22;

                        uint32_t tb_index : 5;
                        uint32_t tb_size : 18;
                        uint32_t scs_mu : 3;
                        uint32_t mcs : 6;

                        uint32_t num_rbs : 9;
                        uint32_t rv : 2;
                        uint32_t harq_or_mbsfn_area_id : 10;
                        uint32_t rnti_type : 1;
                        uint32_t k1_or_pmch_id : 6;
                        uint32_t tci : 1;
                        uint32_t num_layers_raw : 2;
                        uint32_t num_layers : 1;

                        uint32_t unknown3; /* TODO: unknown */
                    } PACK() pdsch_status_info[0];
                } PACK() records[0];
            } PACK();

            declare_app_func_define_var_log(struct nr5g_mac_pdsch_status_v2_5);

            /* use the first record only - a simplified version that does not take CPU-time much */

            VERBOSE("num_records=%d", log->num_records);
            if (log->num_records > 0) {
                VERBOSE("num_pdsch_status=%d", log->records[0].num_pdsch_status);

                if (log->records[0].num_pdsch_status > 0) {
                    static uint8_t count = 0;
                    VERBOSE("numerology=%d", log->records[0].timestamp.numerology);

                    VERBOSE("sfn=%d", log->records[0].pdsch_status_info[0].sfn);
                    VERBOSE("slot=%d", log->records[0].pdsch_status_info[0].slot);

                    VERBOSE("bandwidth=%d", log->records[0].pdsch_status_info[0].bandwidth);
                    VERBOSE("band_type=%d", log->records[0].pdsch_status_info[0].band_type);
                    VERBOSE("phyiscal_cell_id=%d", log->records[0].pdsch_status_info[0].phyiscal_cell_id);
                    VERBOSE("earfcn=%d", log->records[0].pdsch_status_info[0].earfcn);
                    VERBOSE("mcs=%d", log->records[0].pdsch_status_info[0].mcs);
                    VERBOSE("num_rbs=%d", log->records[0].pdsch_status_info[0].num_rbs);
                    VERBOSE("layers=%d", 1 << log->records[0].pdsch_status_info[0].num_layers);

                    if (count++ >= MAX_NR5G_MAC_PDSCH_REPORT_COUNT) {
                        count = 0;
                        _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.scs", 15 << (log->records[0].timestamp.numerology));
                        _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.pci", log->records[0].pdsch_status_info[0].phyiscal_cell_id);
                        _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.mcs", log->records[0].pdsch_status_info[0].mcs);
                        _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.layers", 1 << log->records[0].pdsch_status_info[0].num_layers);
                    }
                }
            }

            break;
        };
    }
}

declare_app_func(nr5g_rrc_configuration_info)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    declare_app_func_define_vars_for_log_with_version_type(uint32_t);

    declare_app_func_require_ver(struct_mm_version(0, 6), struct_mm_version(0, 7), struct_mm_version(0, 8), struct_mm_version(2, 0));

    uint8_t dl_bandwidth = 0;
    uint8_t ul_bandwidth = 0;
    const char *dl_bw_str = NULL;
    const char *ul_bw_str = NULL;
    uint32_t dl_arfcn = 0;
    uint32_t ul_arfcn = 0;
    uint8_t band_type = 0;
    int i;

    /* From SDX55_modem/modem_proc/nr5g/api/public/nr5g_as.h
        NR5G support bandwidths
        Defined in 38.101-1/2 Table 5.3.5-1

        typedef enum
        {
            NR5G_BW_5MHz,
            NR5G_BW_10MHz,
            NR5G_BW_15MHz,
            NR5G_BW_20MHz,
            NR5G_BW_25MHz,
            NR5G_BW_30MHz,
            NR5G_BW_40MHz,
            NR5G_BW_50MHz,
            NR5G_BW_60MHz,
            NR5G_BW_70MHz,
            NR5G_BW_80MHz,
            NR5G_BW_90MHz,
            NR5G_BW_100MHz,
            NR5G_BW_200MHz,
            NR5G_BW_400MHz,
            NR5G_BW_NUM_MAX = NR5G_BW_400MHz,
            NR5G_BW_INVALID
        } nr5g_bandwidth_e;
    */
    const char *bandwidths[] = { "5MHz",  "10MHz", "15MHz", "20MHz", "25MHz",  "30MHz",  "40MHz",
                                 "50MHz", "60MHz", "70MHz", "80MHz", "90MHz", "100MHz", "200MHz", "400MHz" };
    const char *bandTypeTbl[] = { "unknown",  "SUB6", "MMW" };

    switch (log_ver) {

        case struct_mm_version(0, 6): {
            struct nr5g_rrc_configuration_info_v0_6 {
                nr5g_struct_version_t version;
                uint8_t state;
                uint8_t config_status;
                uint8_t connectivity_mode;
                struct {
                    uint8_t num_bands;
                    uint16_t band_list[12];
                } PACK() lte_serving_cell_info;
                struct {
                    uint8_t num_cont_cc;
                    struct {
                        uint16_t band_number;
                        uint8_t dl_bw_class;
                        uint8_t ul_bw_class;
                    } PACK() contiguous_cc[8];
                } PACK() contiguous_cc_info;
                struct {
                    uint8_t num_active_cc;
                    struct {
                        uint8_t cc_id;
                        uint16_t cell_id; // PCI
                        uint32_t dl_arfcn;
                        uint32_t ul_arfcn;
                        uint16_t band;
                        uint8_t band_type;
                        uint8_t dl_carrier_bandwidth;
                        uint8_t ul_carrier_bandwidth;
                        uint8_t dl_max_mimo;
                        uint8_t ul_max_mimo;
                    } PACK() param_list[8];
                } PACK() nr5g_serving_cell_info;
                struct {
                    uint8_t num_active_rb;
                    struct {
                        uint8_t rb_id;
                        uint8_t termination_point;
                        uint8_t dl_rb_type;
                        uint8_t dl_rb_path;
                        uint8_t dl_rohc_enabled;
                        uint8_t dl_cipher_algo;
                        uint8_t dl_integrity_algo;
                        uint8_t ul_rb_type;
                        uint8_t ul_rb_path;
                        uint8_t ul_rohc_enabled;
                        uint8_t ul_cipher_algo;
                        uint8_t ul_integrity_algo;
                        uint8_t ul_primary_path;
                        uint8_t ul_pdcp_dup_activated;
                        uint32_t ul_data_split_threshold;
                    } PACK() rb[36];
                } PACK() radio_bearer_info;
                uint8_t num_active_srb;
                uint8_t num_active_drb;
                uint32_t mn_mcg_drb_ids;
                uint32_t sn_mcg_drb_ids;
                uint32_t mn_scg_drb_ids;
                uint32_t sn_scg_drb_ids;
                uint32_t mn_split_drb_ids;
                uint32_t sn_split_drb_ids;
            } PACK();

            declare_app_func_define_var_log(struct nr5g_rrc_configuration_info_v0_6);

            VERBOSE("nr5g_serving_cell_info.num_active_cc = %d", log->nr5g_serving_cell_info.num_active_cc);

            if (log->nr5g_serving_cell_info.num_active_cc > 0) {
                dl_bandwidth = log->nr5g_serving_cell_info.param_list[0].dl_carrier_bandwidth;
                ul_bandwidth = log->nr5g_serving_cell_info.param_list[0].ul_carrier_bandwidth;
                dl_bw_str = dl_bandwidth < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[dl_bandwidth] : "";
                ul_bw_str = ul_bandwidth < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[ul_bandwidth] : "";
                dl_arfcn = log->nr5g_serving_cell_info.param_list[0].dl_arfcn;
                ul_arfcn = log->nr5g_serving_cell_info.param_list[0].ul_arfcn;
                band_type = log->nr5g_serving_cell_info.param_list[0].band_type;

                VERBOSE("nr5g_serving_cell_info dl_arfcn=%d, ul_arfcn=%d, dl_carrier_bandwidth=%d, ul_carrier_bandwidth=%d, band_type=%d", dl_arfcn, ul_arfcn,
                        dl_bandwidth, ul_bandwidth, band_type);
            }
            break;
        }

        case struct_mm_version(2, 0): {
            /*
                same as 0.7 and 0.8 except for an additional ssb_arfcn field in nr5g_serving_cell_info_t
            */

            struct nr5g_rrc_configuration_info_v2_0 {
                nr5g_struct_version_t version;
                uint8_t state;
                uint8_t config_status;
                uint8_t connectivity_mode;

                uint8_t num_active_srb;
                uint8_t num_active_drb;
                uint32_t mn_mcg_drb_ids;
                uint32_t sn_mcg_drb_ids;
                uint32_t mn_scg_drb_ids;
                uint32_t sn_scg_drb_ids;
                uint32_t mn_split_drb_ids;
                uint32_t sn_split_drb_ids;

                struct {
                    uint8_t num_bands;
                    uint16_t band_list[12];
                } PACK() lte_serving_cell_info;

                uint8_t num_cont_cc;
                uint8_t num_active_cc;
                uint8_t num_active_rb;

            } PACK();

            struct contiguous_cc_info_t {
                uint16_t band_number;
                uint8_t dl_bw_class;
                uint8_t ul_bw_class;
            } PACK();

            struct nr5g_serving_cell_info_t {
                uint8_t cc_id;
                uint16_t cell_id; // PCI
                uint32_t dl_arfcn;
                uint32_t ul_arfcn;
                uint32_t ssb_arfcn;
                uint16_t band;
                uint8_t band_type;
                uint8_t dl_carrier_bandwidth;
                uint8_t ul_carrier_bandwidth;
                uint8_t dl_max_mimo;
                uint8_t ul_max_mimo;
            } PACK();

            struct radio_bearer_info_t {
                uint8_t rb_id;
                uint8_t termination_point;
                uint8_t dl_rb_type;
                uint8_t dl_rb_path;
                uint8_t dl_rohc_enabled;
                uint8_t dl_cipher_algo;
                uint8_t dl_integrity_algo;
                uint8_t ul_rb_type;
                uint8_t ul_rb_path;
                uint8_t ul_rohc_enabled;
                uint8_t ul_cipher_algo;
                uint8_t ul_integrity_algo;
                uint8_t ul_primary_path;
                uint8_t ul_pdcp_dup_activated;
                uint32_t ul_data_split_threshold;
            } PACK();

            declare_app_func_define_var_log(struct nr5g_rrc_configuration_info_v2_0);

            struct contiguous_cc_info_t *contiguous_cc_info = (struct contiguous_cc_info_t *)(log + 1);
            struct nr5g_serving_cell_info_t *nr5g_serving_cell_info = (struct nr5g_serving_cell_info_t *)(contiguous_cc_info + log->num_cont_cc);

            VERBOSE("nr5g_serving_cell_info.num_active_cc = %d", log->num_active_cc);

            /* leave this for backward compatibility */
            if (log->num_active_cc > 0) {
                dl_bandwidth = nr5g_serving_cell_info->dl_carrier_bandwidth;
                ul_bandwidth = nr5g_serving_cell_info->ul_carrier_bandwidth;
                dl_bw_str = dl_bandwidth < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[dl_bandwidth] : "";
                ul_bw_str = ul_bandwidth < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[ul_bandwidth] : "";
                dl_arfcn = nr5g_serving_cell_info->dl_arfcn;
                ul_arfcn = nr5g_serving_cell_info->ul_arfcn;
                band_type = nr5g_serving_cell_info->band_type;

                VERBOSE("nr5g_serving_cell_info dl_arfcn=%d, ul_arfcn=%d, dl_carrier_bandwidth=%d, ul_carrier_bandwidth=%d, band_type=%d", dl_arfcn, ul_arfcn,
                        dl_bandwidth, ul_bandwidth, band_type);
            }
            /* save all serving cell information to RDB */
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.num_active_cc", log->num_active_cc);
            char nr5g_prefix[RDB_MAX_NAME_LEN];
            for (i = 0; i < log->num_active_cc; i++) {
                struct nr5g_serving_cell_info_t *nr5g_serv_cell_info = (struct nr5g_serving_cell_info_t *)(nr5g_serving_cell_info + i);
                snprintf(nr5g_prefix, sizeof(nr5g_prefix), "radio_stack.nr5g.scell.%d.", i);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "cc_id", nr5g_serv_cell_info->cc_id);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "cell_id", nr5g_serv_cell_info->cell_id);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "dl_arfcn", nr5g_serv_cell_info->dl_arfcn);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "ul_arfcn", nr5g_serv_cell_info->ul_arfcn);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "band", nr5g_serv_cell_info->band);
                _rdb_suffix_call(_rdb_set_str, nr5g_prefix, "band_type", bandTypeTbl[nr5g_serv_cell_info->band_type]);
                uint8_t dl_bw = 0, ul_bw = 0;
                const char *dl_bw_sptr = NULL, *ul_bw_sptr = NULL;
                dl_bw = nr5g_serv_cell_info->dl_carrier_bandwidth;
                ul_bw = nr5g_serv_cell_info->ul_carrier_bandwidth;
                dl_bw_sptr = dl_bw < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[dl_bw] : "";
                ul_bw_sptr = ul_bw < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[ul_bw] : "";
                _rdb_suffix_call(_rdb_set_str, nr5g_prefix, "dl_bw", dl_bw_sptr);
                _rdb_suffix_call(_rdb_set_str, nr5g_prefix, "ul_bw", ul_bw_sptr);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "dl_max_mimo", nr5g_serv_cell_info->dl_max_mimo);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "ul_max_mimo", nr5g_serv_cell_info->ul_max_mimo);
            }
            break;
        }

        case struct_mm_version(0, 7):
        /* struct_mm_version(0, 8) */
        /* Found no difference with 0.7 via revere engineering */
        default: {
            /*
                no major changes exist but some of the structure fields are rearrage from 0.6
            */

            struct nr5g_rrc_configuration_info_v0_7 {
                nr5g_struct_version_t version;
                uint8_t state;
                uint8_t config_status;
                uint8_t connectivity_mode;

                uint8_t num_active_srb;
                uint8_t num_active_drb;
                uint32_t mn_mcg_drb_ids;
                uint32_t sn_mcg_drb_ids;
                uint32_t mn_scg_drb_ids;
                uint32_t sn_scg_drb_ids;
                uint32_t mn_split_drb_ids;
                uint32_t sn_split_drb_ids;

                struct {
                    uint8_t num_bands;
                    uint16_t band_list[12];
                } PACK() lte_serving_cell_info;

                uint8_t num_cont_cc;
                uint8_t num_active_cc;
                uint8_t num_active_rb;

            } PACK();

            struct contiguous_cc_info_t {
                uint16_t band_number;
                uint8_t dl_bw_class;
                uint8_t ul_bw_class;
            } PACK();

            struct nr5g_serving_cell_info_t {
                uint8_t cc_id;
                uint16_t cell_id; // PCI
                uint32_t dl_arfcn;
                uint32_t ul_arfcn;
                uint16_t band;
                uint8_t band_type;
                uint8_t dl_carrier_bandwidth;
                uint8_t ul_carrier_bandwidth;
                uint8_t dl_max_mimo;
                uint8_t ul_max_mimo;
            } PACK();

            struct radio_bearer_info_t {
                uint8_t rb_id;
                uint8_t termination_point;
                uint8_t dl_rb_type;
                uint8_t dl_rb_path;
                uint8_t dl_rohc_enabled;
                uint8_t dl_cipher_algo;
                uint8_t dl_integrity_algo;
                uint8_t ul_rb_type;
                uint8_t ul_rb_path;
                uint8_t ul_rohc_enabled;
                uint8_t ul_cipher_algo;
                uint8_t ul_integrity_algo;
                uint8_t ul_primary_path;
                uint8_t ul_pdcp_dup_activated;
                uint32_t ul_data_split_threshold;
            } PACK();

            declare_app_func_define_var_log(struct nr5g_rrc_configuration_info_v0_7);

            struct contiguous_cc_info_t *contiguous_cc_info = (struct contiguous_cc_info_t *)(log + 1);
            struct nr5g_serving_cell_info_t *nr5g_serving_cell_info = (struct nr5g_serving_cell_info_t *)(contiguous_cc_info + log->num_cont_cc);

#ifdef DEBUG_VERBOSE
            struct radio_bearer_info_t *radio_bearer_info = (struct radio_bearer_info_t *)(nr5g_serving_cell_info + log->num_active_cc);

            size_t total_struc_len = (size_t)(radio_bearer_info + log->num_active_rb) - (size_t)log;
            VERBOSE("expected struc size = %d", total_struc_len);

            VERBOSE("num_cont_cc = %d", log->num_cont_cc);
            VERBOSE("num_active_cc = %d", log->num_active_cc);

#endif

            VERBOSE("nr5g_serving_cell_info.num_active_cc = %d", log->num_active_cc);

            /* leave this for backward compatibility */
            if (log->num_active_cc > 0) {
                dl_bandwidth = nr5g_serving_cell_info->dl_carrier_bandwidth;
                ul_bandwidth = nr5g_serving_cell_info->ul_carrier_bandwidth;
                dl_bw_str = dl_bandwidth < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[dl_bandwidth] : "";
                ul_bw_str = ul_bandwidth < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[ul_bandwidth] : "";
                dl_arfcn = nr5g_serving_cell_info->dl_arfcn;
                ul_arfcn = nr5g_serving_cell_info->ul_arfcn;
                band_type = nr5g_serving_cell_info->band_type;

                VERBOSE("nr5g_serving_cell_info dl_arfcn=%d, ul_arfcn=%d, dl_carrier_bandwidth=%d, ul_carrier_bandwidth=%d, band_type=%d", dl_arfcn, ul_arfcn,
                        dl_bandwidth, ul_bandwidth, band_type);
            }
            /* save all serving cell information to RDB */
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.num_active_cc", log->num_active_cc);
            char nr5g_prefix[RDB_MAX_NAME_LEN];
            for (i = 0; i < log->num_active_cc; i++) {
                struct nr5g_serving_cell_info_t *nr5g_serv_cell_info = (struct nr5g_serving_cell_info_t *)(nr5g_serving_cell_info + i);
                snprintf(nr5g_prefix, sizeof(nr5g_prefix), "radio_stack.nr5g.scell.%d.", i);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "cc_id", nr5g_serv_cell_info->cc_id);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "cell_id", nr5g_serv_cell_info->cell_id);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "dl_arfcn", nr5g_serv_cell_info->dl_arfcn);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "ul_arfcn", nr5g_serv_cell_info->ul_arfcn);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "band", nr5g_serv_cell_info->band);
                _rdb_suffix_call(_rdb_set_str, nr5g_prefix, "band_type", bandTypeTbl[nr5g_serv_cell_info->band_type]);
                uint8_t dl_bw = 0, ul_bw = 0;
                const char *dl_bw_sptr = NULL, *ul_bw_sptr = NULL;
                dl_bw = nr5g_serv_cell_info->dl_carrier_bandwidth;
                ul_bw = nr5g_serv_cell_info->ul_carrier_bandwidth;
                dl_bw_sptr = dl_bw < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[dl_bw] : "";
                ul_bw_sptr = ul_bw < sizeof(bandwidths) / sizeof(bandwidths[0]) ? bandwidths[ul_bw] : "";
                _rdb_suffix_call(_rdb_set_str, nr5g_prefix, "dl_bw", dl_bw_sptr);
                _rdb_suffix_call(_rdb_set_str, nr5g_prefix, "ul_bw", ul_bw_sptr);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "dl_max_mimo", nr5g_serv_cell_info->dl_max_mimo);
                _rdb_suffix_call(_rdb_set_int, nr5g_prefix, "ul_max_mimo", nr5g_serv_cell_info->ul_max_mimo);
            }
            break;
        }
    }

    // check invalid NR5G Serving Cell Info
    // band_type : 0 : unknown
    //             1 : SUB6
    //             2 : MMW
    // NR5G ARFCN in this message is not reliable which represents
    // center frequency while all other OTA/QMI messages shows
    // SSB frequency as NR-ARFCN so do not use as NR-ARFCN here.
    if (dl_arfcn != 0 && ul_arfcn != 0 && band_type > 0 && band_type <= 2) {
        if (dl_bw_str != NULL) {
            _rdb_prefix_call(_rdb_set_str, "radio_stack.nr5g.dl_bw", dl_bw_str);
            _rdb_prefix_call(_rdb_set_str, "radio_stack.nr5g.bw", dl_bw_str);
        }
        if (ul_bw_str != NULL) {
            _rdb_prefix_call(_rdb_set_str, "radio_stack.nr5g.ul_bw", ul_bw_str);
        }
    } else {
        _rdb_prefix_call(_rdb_set_str, "radio_stack.nr5g.dl_bw", "");
        _rdb_prefix_call(_rdb_set_str, "radio_stack.nr5g.bw", "");
        _rdb_prefix_call(_rdb_set_str, "radio_stack.nr5g.ul_bw", "");
    }

}

declare_app_func(nr5g_mac_csf_report)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    declare_app_func_define_vars_for_log_with_version_type(uint32_t);

    declare_app_func_require_ver(struct_mm_version(2, 1));

    switch (log_ver) {

        /* struct_mm_version(2, 1) */
        default: {
            struct nr5g_mac_csf_report_v2_1 {
                nr5g_struct_version_t version;              // 4 bytes
                nr5g_log_fields_change_t log_fields_change; // 8 bytes

                uint16_t log_fields_change_bmask;
                uint8_t sub_id;
                uint8_t num_records;

                struct {

                    nr5g_timestamp_t timestamp; // 4 bytes
                    uint32_t num_csf_reports;   // 4 bytes

                    struct {

                        uint32_t carrier_id : 4;
                        uint32_t resource_carrier_id : 4;

                        uint8_t report_id_type;
                        /*
                        uint32_t report_id : 6;  // TODO: report_id is either 5 or 6
                        uint8_t report_type : 2; // TODO: report_type is either 2 or 3
                        */

                        uint32_t report_quantity_bitmask : 8;

                        uint32_t late_csf : 1;
                        uint32_t fake_csf : 1;
                        uint32_t num_csi_p1_bits : 6;

                        uint32_t num_csi_p2___dropped;
                        /*
                        uint32_t num_csi_p2_wb_bits : 5 - 7;
                        uint32_t num_csi_p2_sb_odd_bits : 5 - 7;
                        uint32_t num_csi_p2_sb_even_bits 5 - 7;
                        uint32_t report_dropped : 1;
                        uint32_t p1_dropped : 1;

                        uint32_t p2_wb_dropped : 2 - ;
                        uint32_t p2_sb_odd_report_dropped : 1;
                        uint32_t p2_sb_even_report_dropped : 1;

                        uint32_t reserved : 9 - 12;
                        */
                        struct {
                            struct {
                                struct {
                                    uint8_t cri_ri : 5; // cri + ri takes 5 bits
                                    /*
                                    uint32_t cri : 1 - 5;
                                    uint32_t ri : 1 - 5;
                                    */
                                    uint32_t wb_cqi : 4;
                                    uint32_t pmi_wb_x1 : 18;
                                    uint32_t pmi_wb_x2 : 4;
                                    uint32_t reserved : 1;

                                    uint32_t li_num_sb_reserved;
                                    /*
                                    uint32_t li : 1 - 3;
                                    uint32_t num_sb : 5 - 7;
                                    uint32_t reserved : 28 - 31;
                                    */

                                    uint16_t sb_result[19];
                                    /*
                                    struct {
                                        uint32_t sb_id : 1 - 3;
                                        uint32_t sb_cqi : 2 - 7;
                                        uint32_t sb_pmi : 5 - 7;
                                        uint32_t reserved : 1 - 3;
                                    } PACK() sb_result[1];
                                    */
                                    uint16_t reserved_2;
                                } PACK() metrics;

                                uint32_t bit_width;
                                /*
                                struct {
                                    uint32_t cri;
                                    uint32_t ri; // 5 bits
                                    uint32_t zero_padding : 2;
                                    uint32_t wb_cqi : 4;

                                    uint32_t pmi_wb_x1 : 8 - 12;
                                    uint32_t pmi_wb_x2;
                                    uint32_t li : 2 - 7;
                                    uint32_t pmi_sb_x2 : 1 - 3;
                                } PACK() bit_width;
                                */

                            } PACK() csi;

                        } PACK() quantities;

                    } PACK() reports[1];

                } PACK() records[1];

            } PACK();
            static uint8_t count = 0;

            declare_app_func_define_var_log(struct nr5g_mac_csf_report_v2_1);

            if (log->num_records < 0) {
                INFO("No records");
                return;
            }
            if (log->records[0].num_csf_reports < 0) {
                INFO("No reports");
                return;
            }

            VERBOSE("wb_cqi=%d", log->records[0].reports[0].quantities.csi.metrics.wb_cqi);

            if (count++ >= MAX_NR5G_CQI_REPORT_COUNT) {
                count = 0;
                _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.cqi", log->records[0].reports[0].quantities.csi.metrics.wb_cqi);
            }
        }
    }
}

declare_app_func(nr5g_mac_pdsch_stats_log)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);
    char str[RDB_MAX_VAL_LEN];
    int str_len = sizeof(str);

    struct B887_NR5G_MAC_PDSCH_Stats {
        uint16_t minor;
        uint16_t major;
        char pad1[16]; // includes sub and total slot count
        uint32_t slots_elapsed;
        uint32_t total_tb;
        uint32_t good_tb;
        uint32_t bad_tb;
        uint32_t retx_tb;
        uint32_t ack_as_nack;
        uint32_t harq_fail;
        uint32_t pad2;
        uint64_t good_bytes;
        uint64_t bad_bytes;
        uint64_t total_bytes;
        uint64_t pad_bytes;
    } __attribute__((__packed__));

    declare_app_func_define_vars_for_log_with_version_type(uint32_t);

    declare_app_func_require_ver(struct_mm_version(2, 0), struct_mm_version(2, 2));

    switch (log_ver) {
        case struct_mm_version(2, 0): {
            declare_app_func_define_var_log(struct B887_NR5G_MAC_PDSCH_Stats);

            DEBUG("NR5G nr_mac_pdsch_STATISTIC ver %d.%d total_tb %d total_bytes %lld", log->major, log->minor, log->total_tb, log->total_bytes);
            if (log->major != 2 || log->minor != 0)
                return;
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.total_tb", log->total_tb);
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.bad_tb", log->bad_tb);
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.good_bytes", log->good_bytes);
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.bad_bytes", log->bad_bytes);

            break;
        }

        /* struct_mm_version(2, 2) */
        default: {
            struct nr5g_mac_pdsch_stats_v2_2 {
                nr5g_struct_version_t version;
                nr5g_log_fields_change_t log_fields_change;

                uint16_t log_fields_change_bmask;
                uint8_t reserved1;
                uint8_t num_records;

                uint32_t flush_gap_count : 4;
                uint32_t num_total_slots : 24;
                uint32_t num_ca : 4;

                uint8_t cumulative_bitmask;
                uint8_t reserved2[7];

                struct {
                    uint32_t carrier_id;
                    uint32_t num_slots_elapsed;
                    uint32_t num_pdsch_decode;
                    uint32_t num_crc_pass_tb;
                    uint32_t num_crc_fail_tb;
                    uint32_t num_retx;
                    uint32_t ack_as_nack;
                    uint32_t harq_failure;
                    uint64_t crc_pass_tb_bytes;
                    uint64_t crc_fail_tb_bytes;
                    uint64_t tb_bytes;
                    uint64_t padding_bytes;
                    uint64_t retx_bytes;
                } PACK() records[0];
            } PACK();
            static uint16_t count = 0;
            static uint64_t sum_total_tb = 0;
            static uint64_t sum_bad_tb = 0;
            static uint64_t sum_good_bytes = 0;
            static uint64_t sum_bad_bytes = 0;

            declare_app_func_define_var_log(struct nr5g_mac_pdsch_stats_v2_2);

            VERBOSE("num_total_slots=%d", log->num_total_slots);
            VERBOSE("num_ca=%d", log->num_ca);
            VERBOSE("num_records=%d", log->num_records);

            /* TODO: accumulate all of records */
            if (log->num_records > 0) {

                VERBOSE("carrier_id=%u", log->records[0].carrier_id);
                VERBOSE("num_slots_elapsed=%u", log->records[0].num_slots_elapsed);
                VERBOSE("num_pdsch_decode=%u", log->records[0].num_pdsch_decode);
                VERBOSE("num_crc_pass_tb=%u", log->records[0].num_crc_pass_tb);
                VERBOSE("num_crc_fail_tb=%u", log->records[0].num_crc_fail_tb);
                VERBOSE("crc_pass_tb_bytes=%llu", log->records[0].crc_pass_tb_bytes);
                VERBOSE("crc_fail_tb_bytes=%llu", log->records[0].crc_fail_tb_bytes);
                VERBOSE("tb_bytes=%llu", log->records[0].tb_bytes);

                // sum over MAX_NR5G_REPORT_COUNT reports
                sum_total_tb += log->records[0].num_pdsch_decode;
                sum_bad_tb += log->records[0].num_crc_fail_tb;
                sum_good_bytes += log->records[0].crc_pass_tb_bytes;
                sum_bad_bytes += log->records[0].crc_fail_tb_bytes;

                if (count++ >= MAX_NR5G_REPORT_COUNT) {
                    _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.total_tb", sum_total_tb);
                    _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.bad_tb", sum_bad_tb);
                    _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.good_bytes", sum_good_bytes);
                    _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.bad_bytes", sum_bad_bytes);
                    count = 0;
                    sum_total_tb = 0;
                    sum_bad_tb = 0;
                    sum_good_bytes = 0;
                    sum_bad_bytes = 0;
                }
            }

            break;
        }
    }

    /* assume NR5G is up when we receive statistics but the nr5g status is not
       UP (may failed to receive event 3191). */
    _rdb_get_str(str, str_len, "wwan.0.radio_stack.nr5g.up");
    if (assumptive_nr5g_up_cnt == 0 && strcmp(str, "UP")) {
        DEBUG("[NR5G] got nr5g_mac_pdsch_stats_log but status is not UP, set nr5g.up UP");
        _rdb_prefix_call(_rdb_set_str, "radio_stack.nr5g.up", "UP");
    }
    /* Reset the assumptive NR5G up counter to 1 in order to prevent
        changing NR5G status to down status by timeout. */
    assumptive_nr5g_up_cnt = 1;
}

declare_app_func(nr5g_searcher_measurement_log)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);
    static uint8_t count = 0;

    struct NRbeam {
        uint8 SSB;
        char pad[19]; // includes timing
        int32_t rawRSRP[2];
        int32_t filtRSRP;
        int32_t filtRSRQ;
        int32_t filtSNR;
        int32_t l3RSRP;
        int32_t l2RSRP;
        int32_t l3RSRQ;
        int32_t l2RSRQ;
        int32_t l3SNR;
        int32_t l2SNR;
    } __attribute__((__packed__));

    struct NRcell {
        uint16_t pci;
        uint16_t sfn;
        uint8_t numBeams;
        uint8_t indSBeam;
        uint16_t pad;
        struct NRbeam beams[1];
    } __attribute__((__packed__));

    struct SearcherMeas {
        uint16_t minor;
        uint16_t major;
        uint32_t numLayers;
        uint32_t channelSSB;
        uint16_t numCells;
        uint16_t indSCell;
        struct NRcell cells; // really an array!
    } __attribute__((__packed__));

    declare_app_func_define_vars_for_log_with_version_type(uint32_t);

    int32_t filtRSRP = UINT32_MAX;
    int32_t filtRSRQ = UINT32_MAX;
    int32_t filtSNR = UINT32_MAX;
    int32_t SSB = UINT32_MAX;

    declare_app_func_require_ver(struct_mm_version(2, 1), struct_mm_version(2, 4), struct_mm_version(2, 5), struct_mm_version(2, 6));
    switch (log_ver) {
        case struct_mm_version(2, 1): {
            declare_app_func_define_var_log(struct SearcherMeas);

            DEBUG("NR5G nr5g_searcher_measurement_log v%d.%d count %d/%d", log->major, log->minor, log->numCells, log->cells.numBeams);

            if (log->indSCell || log->cells.indSBeam)
                ERR("NR5G nr5g_searcher_measurement_log WRONG BEAM %d/%d (of %d/%d)", log->indSCell, log->cells.indSBeam, log->numCells,
                    log->cells.numBeams);

            struct NRbeam *s = &log->cells.beams[0];
            filtRSRP = s->filtRSRP / 128;
            filtRSRQ = s->filtRSRQ / 128;
            filtSNR = s->filtSNR / 128;
            SSB = s->SSB;
            break;
        }

        case struct_mm_version(2, 5): {
            /*
                mll_searcher_measurement_v2_5 is very identical to mll_searcher_measurement_v2_4

                serving_sub_array_id is added.
            */

            struct mll_searcher_measurement_v2_5 {
                nr5g_struct_version_t version;

                uint8_t num_layers;
                uint32_t reserved : 24;

                struct {
                    uint32_t raster_freq;
                    uint8_t num_of_cell;
                    uint8_t serving_cell_index;
                    uint16_t serving_cell_pci;
                    uint8_t serving_ssb;
                    uint32_t reserved1 : 24;

                    uint32_t serving_bm_rsrp_4rx2nd_pair[2];

                    uint16_t serving_rx_beam[2];

                    uint16_t serving_rfic_id;
                    uint16_t reserved2;
                    uint16_t serving_sub_array_id[2];

                    struct {
                        uint16_t pci;
                        uint16_t sfn;
                        uint8_t num_beams;
                        uint32_t reserved : 24;
                        int32_t cell_quality_rsrp;
                        int32_t cell_quality_rsrq;

                        struct {
                            uint16_t ssb_index;
                            uint16_t reserved;

                            struct {
                                uint16_t rx_beam_id[2];
                                uint32_t reserved1;

                                union
                                {
                                    struct {
                                        uint32_t cell_timing_1;
                                        uint32_t cell_timing_2;
                                    } PACK();
                                    uint64_t cell_timing;
                                } PACK();

                                uint32_t rsrps[2];
                            } PACK() rx_beam_info;

                            int32_t filtered_tx_beam_rsrp;
                            int32_t filtered_tx_beam_rsrq;
                            int32_t filtered_tx_beam_rsrp_l3;
                            int32_t l2_nr_filtered_tx_beam_rsrp_l3;
                            int32_t filtered_tx_beam_rsrq_l3;
                            int32_t l2_nr_filtered_tx_beam_rsrq_l3;
                        } PACK() detected_beams[1];
                    } PACK() cells[1];

                } PACK() component_carrier_list[1];

            } PACK();

            declare_app_func_define_var_log(struct mll_searcher_measurement_v2_5);

            VERBOSE("minor=%d", log->version.minor);
            VERBOSE("major=%d", log->version.major);
            VERBOSE("num_layers=%d", log->num_layers);

            /* make sure that we have the correct carrier */
            if (log->num_layers > 0) {

                int serving_cell_index = log->component_carrier_list[0].serving_cell_index;
                int num_of_cell = log->component_carrier_list[0].num_of_cell;

                VERBOSE("num_of_cell=%d", num_of_cell);
                VERBOSE("serving_cell_index=%d", serving_cell_index);
                VERBOSE("freq=%d", log->component_carrier_list[0].raster_freq);
                VERBOSE("serving_cell_pci=%d", log->component_carrier_list[0].serving_cell_pci);
                VERBOSE("serving_ssb=%d", log->component_carrier_list[0].serving_ssb);

                if (serving_cell_index < num_of_cell) {
                    VERBOSE("num_beams=%d", log->component_carrier_list[0].cells[serving_cell_index].num_beams);
                    VERBOSE("pci=%d", log->component_carrier_list[0].cells[serving_cell_index].pci);
                    VERBOSE("sfn=%d", log->component_carrier_list[0].cells[serving_cell_index].sfn);

                    VERBOSE("rsrp=%d", log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrp / 128);
                    VERBOSE("rsrq=%d", log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrq / 128);

                    filtRSRP = log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrp / 128;
                    filtRSRQ = log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrq / 128;

                    if (log->component_carrier_list[0].cells[serving_cell_index].num_beams > 0) {
                        /*
                            TODO:
                            check if the serving cell beam ID is always the first one. Otherwise, search beam ID instead of using the first beam
                           entry.
                        */
                        int serving_cell_beam = 0;
                        VERBOSE("ssb_index=%d", log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].ssb_index);
                        VERBOSE("filtered_tx_beam_rsrp=%d",
                                log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].filtered_tx_beam_rsrp /
                                    128);
                        VERBOSE("filtered_tx_beam_rsrq=%d",
                                log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].filtered_tx_beam_rsrq /
                                    128);

                        SSB = log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].ssb_index;
                    }
                }
            }

            break;
        }

        case struct_mm_version(2, 4): {
            struct mll_searcher_measurement_v2_4 {
                nr5g_struct_version_t version;

                uint8_t num_layers;
                uint32_t reserved : 24;

                struct {
                    uint32_t raster_freq;
                    uint8_t num_of_cell;
                    uint8_t serving_cell_index;
                    uint16_t serving_cell_pci;
                    uint8_t serving_ssb;
                    uint32_t reserved1 : 24;

                    uint32_t serving_bm_rsrp_4rx2nd_pair[2];

                    uint16_t serving_rx_beam[2];

                    uint16_t serving_antenna_panel;
                    uint16_t reserved2;

                    struct {
                        uint16_t pci;
                        uint16_t sfn;
                        uint8_t num_beams;
                        uint32_t reserved : 24;
                        int32_t cell_quality_rsrp;
                        int32_t cell_quality_rsrq;

                        struct {
                            uint16_t ssb_index;
                            uint16_t reserved;

                            struct {
                                uint16_t rx_beam_id[2];
                                uint32_t reserved1;

                                union
                                {
                                    struct {
                                        uint32_t cell_timing_1;
                                        uint32_t cell_timing_2;
                                    } PACK();
                                    uint64_t cell_timing;
                                } PACK();

                                uint32_t rsrps[2];
                            } PACK() rx_beam_info;

                            int32_t filtered_tx_beam_rsrp;
                            int32_t filtered_tx_beam_rsrq;
                            int32_t filtered_tx_beam_rsrp_l3;
                            int32_t l2_nr_filtered_tx_beam_rsrp_l3;
                            int32_t filtered_tx_beam_rsrq_l3;
                            int32_t l2_nr_filtered_tx_beam_rsrq_l3;
                        } PACK() detected_beams[0];
                    } PACK() cells[0];

                } PACK() component_carrier_list[0];

            } PACK();

            declare_app_func_define_var_log(struct mll_searcher_measurement_v2_4);

            VERBOSE("minor=%d", log->version.minor);
            VERBOSE("major=%d", log->version.major);
            VERBOSE("num_layers=%d", log->num_layers);

            /* make sure that we have the correct carrier */
            if (log->num_layers > 0) {

                int serving_cell_index = log->component_carrier_list[0].serving_cell_index;
                int num_of_cell = log->component_carrier_list[0].num_of_cell;

                VERBOSE("num_of_cell=%d", num_of_cell);
                VERBOSE("serving_cell_index=%d", serving_cell_index);
                VERBOSE("freq=%d", log->component_carrier_list[0].raster_freq);
                VERBOSE("serving_cell_pci=%d", log->component_carrier_list[0].serving_cell_pci);
                VERBOSE("serving_ssb=%d", log->component_carrier_list[0].serving_ssb);

                if (serving_cell_index < num_of_cell) {
                    VERBOSE("num_beams=%d", log->component_carrier_list[0].cells[serving_cell_index].num_beams);
                    VERBOSE("pci=%d", log->component_carrier_list[0].cells[serving_cell_index].pci);
                    VERBOSE("sfn=%d", log->component_carrier_list[0].cells[serving_cell_index].sfn);

                    VERBOSE("rsrp=%d", log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrp / 128);
                    VERBOSE("rsrq=%d", log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrq / 128);

                    filtRSRP = log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrp / 128;
                    filtRSRQ = log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrq / 128;

                    if (log->component_carrier_list[0].cells[serving_cell_index].num_beams > 0) {
                        /*
                            TODO:
                            check if the serving cell beam ID is always the first one. Otherwise, search beam ID instead of using the first beam
                           entry.
                        */
                        int serving_cell_beam = 0;
                        VERBOSE("ssb_index=%d", log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].ssb_index);
                        VERBOSE("filtered_tx_beam_rsrp=%d",
                                log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].filtered_tx_beam_rsrp /
                                    128);
                        VERBOSE("filtered_tx_beam_rsrq=%d",
                                log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].filtered_tx_beam_rsrq /
                                    128);

                        SSB = log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].ssb_index;
                    }
                }
            }

            break;
        }

        case struct_mm_version(2, 6): {
            /*
                mll_searcher_measurement_v2_6 is very identical to mll_searcher_measurement_v2_5

                * following fields are removed
                int32_t filtered_tx_beam_rsrp;
                int32_t filtered_tx_beam_rsrq;
            */

            struct mll_searcher_measurement_v2_6 {
                nr5g_struct_version_t version;

                uint8_t num_layers;
                uint32_t reserved : 24;

                struct {
                    uint32_t raster_freq;
                    uint8_t num_of_cell;
                    uint8_t serving_cell_index;
                    uint16_t serving_cell_pci;
                    uint8_t serving_ssb;
                    uint32_t reserved1 : 24;

                    uint32_t serving_bm_rsrp_4rx2nd_pair[2];

                    uint16_t serving_rx_beam[2];

                    uint16_t serving_rfic_id;
                    uint16_t reserved2;
                    uint16_t serving_sub_array_id[2];

                    struct {
                        uint16_t pci;
                        uint16_t sfn;
                        uint8_t num_beams;
                        uint32_t reserved : 24;
                        int32_t cell_quality_rsrp;
                        int32_t cell_quality_rsrq;

                        struct {
                            uint16_t ssb_index;
                            uint16_t reserved;

                            struct {
                                uint16_t rx_beam_id[2];
                                uint32_t reserved1;

                                union
                                {
                                    struct {
                                        uint32_t cell_timing_1;
                                        uint32_t cell_timing_2;
                                    } PACK();
                                    uint64_t cell_timing;
                                } PACK();

                                uint32_t rsrps[2];
                            } PACK() rx_beam_info;

                            int32_t filtered_tx_beam_rsrp_l3;
                            int32_t l2_nr_filtered_tx_beam_rsrp_l3;
                            int32_t filtered_tx_beam_rsrq_l3;
                            int32_t l2_nr_filtered_tx_beam_rsrq_l3;
                        } PACK() detected_beams[1];
                    } PACK() cells[1];

                } PACK() component_carrier_list[1];

            } PACK();

            declare_app_func_define_var_log(struct mll_searcher_measurement_v2_6);

            VERBOSE("minor=%d", log->version.minor);
            VERBOSE("major=%d", log->version.major);
            VERBOSE("num_layers=%d", log->num_layers);

            /* make sure that we have the correct carrier */
            if (log->num_layers > 0) {

                int serving_cell_index = log->component_carrier_list[0].serving_cell_index;
                int num_of_cell = log->component_carrier_list[0].num_of_cell;

                VERBOSE("num_of_cell=%d", num_of_cell);
                VERBOSE("serving_cell_index=%d", serving_cell_index);
                VERBOSE("freq=%d", log->component_carrier_list[0].raster_freq);
                VERBOSE("serving_cell_pci=%d", log->component_carrier_list[0].serving_cell_pci);
                VERBOSE("serving_ssb=%d", log->component_carrier_list[0].serving_ssb);

                if (serving_cell_index < num_of_cell) {
                    VERBOSE("num_beams=%d", log->component_carrier_list[0].cells[serving_cell_index].num_beams);
                    VERBOSE("pci=%d", log->component_carrier_list[0].cells[serving_cell_index].pci);
                    VERBOSE("sfn=%d", log->component_carrier_list[0].cells[serving_cell_index].sfn);

                    VERBOSE("rsrp=%d", log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrp / 128);
                    VERBOSE("rsrq=%d", log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrq / 128);

                    filtRSRP = log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrp / 128;
                    filtRSRQ = log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrq / 128;

                    if (log->component_carrier_list[0].cells[serving_cell_index].num_beams > 0) {
                        int serving_cell_beam = 0;
                        VERBOSE("ssb_index=%d", log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].ssb_index);
                        SSB = log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].ssb_index;
                    }
                }
            }
            break;
        }

        /* struct_mm_version(2, 7) */
        default: {
            struct mll_searcher_measurement_v2_7 {
                nr5g_struct_version_t version;

                uint8_t num_layers;
                uint32_t reserved : 24;
                uint32_t reserved0[2];

                struct {
                    uint32_t raster_freq;
                    uint8_t num_of_cell;
                    uint8_t serving_cell_index;
                    uint16_t serving_cell_pci;
                    uint8_t serving_ssb;
                    uint32_t reserved1 : 24;

                    uint32_t serving_bm_rsrp_4rx2nd_pair[2];

                    uint16_t serving_rx_beam[2];

                    uint16_t serving_rfic_id;
                    uint16_t reserved2;
                    uint16_t serving_sub_array_id[2];

                    struct {
                        uint16_t pci;
                        uint16_t sfn;
                        uint8_t num_beams;
                        uint32_t reserved : 24;
                        int32_t cell_quality_rsrp;
                        int32_t cell_quality_rsrq;

                        struct {
                            uint16_t ssb_index;
                            uint16_t reserved;

                            struct {
                                uint16_t rx_beam_id[2];
                                uint32_t reserved1;

                                union
                                {
                                    struct {
                                        uint32_t cell_timing_1;
                                        uint32_t cell_timing_2;
                                    } PACK();
                                    uint64_t cell_timing;
                                } PACK();

                                uint32_t rsrps[2];
                            } PACK() rx_beam_info;

                            int32_t filtered_tx_beam_rsrp_l3;
                            int32_t filtered_tx_beam_rsrq_l3;
                            int32_t l2_nr_filtered_tx_beam_rsrp_l3;
                            int32_t l2_nr_filtered_tx_beam_rsrq_l3;
                        } PACK() detected_beams[1];
                    } PACK() cells[1];

                } PACK() component_carrier_list[1];

            } PACK();

            declare_app_func_define_var_log(struct mll_searcher_measurement_v2_7);

            VERBOSE("minor=%d", log->version.minor);
            VERBOSE("major=%d", log->version.major);
            VERBOSE("num_layers=%d", log->num_layers);

            /* make sure that we have the correct carrier */
            if (log->num_layers > 0) {

                int serving_cell_index = log->component_carrier_list[0].serving_cell_index;
                int num_of_cell = log->component_carrier_list[0].num_of_cell;

                VERBOSE("num_of_cell=%d", num_of_cell);
                VERBOSE("serving_cell_index=%d", serving_cell_index);
                VERBOSE("freq=%d", log->component_carrier_list[0].raster_freq);
                VERBOSE("serving_cell_pci=%d", log->component_carrier_list[0].serving_cell_pci);
                VERBOSE("serving_ssb=%d", log->component_carrier_list[0].serving_ssb);

                if (serving_cell_index < num_of_cell) {
                    VERBOSE("num_beams=%d", log->component_carrier_list[0].cells[serving_cell_index].num_beams);
                    VERBOSE("pci=%d", log->component_carrier_list[0].cells[serving_cell_index].pci);
                    VERBOSE("sfn=%d", log->component_carrier_list[0].cells[serving_cell_index].sfn);

                    VERBOSE("rsrp=%d", log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrp / 128);
                    VERBOSE("rsrq=%d", log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrq / 128);

                    filtRSRP = log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrp / 128;
                    filtRSRQ = log->component_carrier_list[0].cells[serving_cell_index].cell_quality_rsrq / 128;

                    if (log->component_carrier_list[0].cells[serving_cell_index].num_beams > 0) {
                        int serving_cell_beam = 0;
                        VERBOSE("ssb_index=%d", log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].ssb_index);
                        SSB = log->component_carrier_list[0].cells[serving_cell_index].detected_beams[serving_cell_beam].ssb_index;
                    }
                }
            }
            break;
        }
    }

    if (count++ >= MAX_NR5G_RSRP_REPORT_COUNT) {
        count = 0;
        if (filtRSRP != UINT32_MAX) {
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.rsrp", filtRSRP);
        }

        if (filtRSRQ != UINT32_MAX) {
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.rsrq", filtRSRQ);
        }

        if (filtSNR != UINT32_MAX) {
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.snr", filtSNR);
        }

        if (SSB != UINT32_MAX) {
            _rdb_prefix_call(_rdb_set_int, "radio_stack.nr5g.ssb_index", SSB);
        }
    }
}
#endif

#define RRC_STATE_CLOSING 7
#ifdef INCLUDE_LTE_RRC_STATE_MSG
declare_app_func(event_lte_rrc_state_change)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    struct acc_t *PDCPBytesReceived = &rrc_info.PDCPBytesReceived;
    struct acc_t *PDCPBytesSent = &rrc_info.PDCPBytesSent;
    struct acc_t *RLCUpThroughput = &rrc_info.RLCUpThroughput;
    struct acc_t *RLCDownThroughput = &rrc_info.RLCDownThroughput;
    struct acc_t *RLCUpRetrans = &rrc_info.RLCUpRetrans;

    declare_app_func_define_vars_for_event();
    declare_app_func_define_var_event(char);

    static int prev_rrc_stat = 0x00; /* inactive */
    int prev_idle;
    int idle;

    int rrc_stat = *event;

    if (rrc_stat < 0 || rrc_stat >= __countof(rrc_state_str)) {
        ERR("invalid rrc state (rrc_stat=%d)", rrc_stat);
        goto err;
    }

    DEBUG("rrc state = '%s'", rrc_state_str[rrc_stat]);

    /* if RRC state is closing, provide PCC time for ca_mode  */
    if ((rrc_stat == RRC_STATE_CLOSING) && (caUsage.pCellState > CA_STATE_DISCONNECTED)) {
        decrease_pcc_ca_state(get_monotonic_ms());
    }

    /* get previous and current idle status */
    prev_idle = !(prev_rrc_stat == 0x03 || prev_rrc_stat == 0x04);
    idle = !(rrc_stat == 0x03 || rrc_stat == 0x04);

    /* store rrc state */
    prev_rrc_stat = rrc_stat;

    if (prev_idle && !idle) {
        DEBUG("[rrc] start session");
        rrc_info_start();

#ifdef CONFIG_DEBUG_PDCP
        DEBUG("[pdcp] initiate pdcp_tx_sent and pdcp_rx_received.");
#endif

        /* As PDCP statistics does not always start from 0, we have to feed 0 to PDCP received and sent when RRC becomes online */
        acc_feed(PDCPBytesReceived, 0, ts);
        acc_feed(PDCPBytesSent, 0, ts);

        /* feed initial period time for better statistics */
        acc_feed_time(RLCUpThroughput, ts);
        acc_feed_time(RLCDownThroughput, ts);
        acc_feed_time(RLCUpRetrans, ts);

    } else if (!prev_idle && idle) {

        /* feed final time period for better statistics */
        acc_feed(RLCUpThroughput, RLCUpThroughput->acc, ts);
        acc_feed(RLCDownThroughput, RLCDownThroughput->acc, ts);
        acc_feed(RLCUpRetrans, RLCUpRetrans->acc, ts);

        DEBUG("[rrc] stop session");
        rrc_info_stop();

#ifdef CONFIG_DEBUG_PDCP
        DEBUG("[pdcp] finalize pdcp_tx_sent and pdcp_rx_received.");
#endif
    }

    _rdb_prefix_call(_rdb_set_str, "radio_stack.rrc_stat.rrc_stat", rrc_state_str[rrc_stat]);

err:
    __noop();
    return;
}
#endif /* ifdef INCLUDE_LTE_RRC_STATE_MSG */

#ifdef INCLUDE_COMMON_MSG
const char *bearer_state_names[] = {
    [0] = "INACTIVE",
    [1] = "ACTIVE_PENDING",
    [2] = "ACTIVE",
    [3] = "MODIFY",
};

#ifdef INCLUDE_LTE_EMM_ESM_MSG
declare_app_func(lte_nas_esm_bearer_context_info)
{
    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(lte_nas_esm_bearer_context_info_T);

    const char **bearer_state_ptr = &bearer_state_names[log->bearer_state];
    const char *bearer_state_str = __is_in_boundary(bearer_state_ptr, bearer_state_names, sizeof(bearer_state_names)) ? *bearer_state_ptr : NULL;

    DEBUG("context info (lv=%d,ebi=%d,stat=%s #%d,ebt=%d,lbi_valid=%d,lbi=%d,rbid=%d,sdf_id=%d,cid=%d)", log->log_version, log->bearer_id,
          bearer_state_str, log->bearer_state, log->context_type, log->lbi_valid, log->lbi, log->sdf_id, log->rb_id, log->connection_id);
}

declare_app_func(lte_nas_esm_bearer_context_state)
{
    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(lte_nas_esm_bearer_context_state_T);
    VERBOSE("version = %d", log->log_version);

    unsigned long long ms = get_ms_from_ts(ts);
    int ebi = log->bearer_id;
    int state = log->bearer_state;

    const char **bearer_state_ptr = &bearer_state_names[state];
    const char *bearer_state_str = __is_in_boundary(bearer_state_ptr, bearer_state_names, sizeof(bearer_state_names)) ? *bearer_state_ptr : NULL;

    DEBUG("context stat (lv=%d,ebi=%d,stat=%s #%d,cid=%d)", log->log_version, ebi, bearer_state_str, state, log->connection_id);

#ifdef TEST_MODE_BROKEN_BEARER_INTEGRITY
    ERR("!!! warning !!! bearer integrity test mode");
    if (ebi == 7) {
        ERR("replace ebi #7 with ebi #6");
        ebi = 6;
    }
#endif

    pdn_mgr_check_ue_stat_integrity(&ms, ebi, state);
}
#endif

void log_debug_dump(const char *buf, int len)
{
    char line[1024];
    char *out;

    int i;

    i = 0;
    out = line;
    while (i++ < len) {
        out += sprintf(out, "%02x ", *buf++);
        if (!(i % 16)) {
            DEBUG("%s\n", line);
            out = line;
        }
    }

    if (out != line)
        DEBUG("%s\n", line);
}

#ifdef DEBUG_VERBOSE
void log_verbose_dump(const char *buf, int len)
{
    char line[1024];
    char *out;

    int i;

    i = 0;
    out = line;
    while (i++ < len) {
        out += sprintf(out, "%02x ", *buf++);
        if (!(i % 16)) {
            VERBOSE("%s\n", line);
            out = line;
        }
    }

    if (out != line)
        VERBOSE("%s\n", line);
}
#else
void log_verbose_dump(const char *buf, int len) {}
#endif

declare_app_func(lte_pdcpdl_statistics)
{
    struct lte_pdcpdl_statistics_rb_t {
        uint8 Rb_Cfg_Idx;
        uint8 mode;
        uint8 pdcp_hdr_len;
        uint32 num_rst;
        char unknown[160]; /* ref. 80-VP457-1 YYE 9.5 - extra data */
    } PACK();

    struct lte_pdcpdl_statistics_subpacket_t {
        uint8 subpacket_id;
        /* SubPacket - ( DL Statistics ) */
        uint8 version;
        uint16 subpacket_size;

        /* Version 2*/
        uint8 num_rbs;
        uint32 num_errors;
        uint32 num_offload_q_full_count;
        uint32 num_packet_dropped_offload_q_full;

        struct lte_pdcpdl_statistics_rb_t rbs[0];
    } PACK();

    struct lte_pdcpdl_statistics_t {
        uint8 version;
        uint8 number_of_subpackets;
        uint16 reserved;

        struct lte_pdcpdl_statistics_subpacket_t subpacket[0];

    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1);
    declare_app_func_define_var_log(struct lte_pdcpdl_statistics_t);

    struct acc_t *PDCPTotalReceivedPDU = &rrc_info.PDCPTotalReceivedPDU;
    ;
    struct acc_t *PDCPTotalLossSDU = &rrc_info.PDCPTotalLossSDU;
    ;
    struct acc_t *PDCPBytesReceived = &rrc_info.PDCPBytesReceived;

    char *p;
    int i;
    int j;

    unsigned long long pdcp_packetloss_received = 0;
    unsigned long long pdcp_packet_received = 0;
    unsigned long long pdcp_rx_received = 0;

#ifdef DEBUG_VERBOSE
    unsigned int num_data_pdu;
    unsigned int missing_sdus;
#endif
    unsigned int num_data_pdu_rst;
    unsigned int missing_sdus_rst;
    unsigned int num_data_pdu_rx_bytes_rst;

    VERBOSE("number_subpackets = %d", log->number_of_subpackets);

#ifdef CONFIG_DEBUG_PDCP
    DEBUG("len = %d", len);
    DEBUG("len log = %d", sizeof(*log));

    DEBUG("size of lte_pdcpdl_statistics_t = %d", sizeof(struct lte_pdcpdl_statistics_t));
    DEBUG("size of subpacket = %d", sizeof(*log->subpacket));
    DEBUG("size of rb = %d", sizeof(*log->subpacket->rbs));

    DEBUG("number_of_subpackets = %d", log->number_of_subpackets);
    DEBUG("num_rbs = %d", log->subpacket[0].num_rbs);
#endif

    for (i = 0; i < log->number_of_subpackets; i++) {
        for (j = 0; j < log->subpacket[i].num_rbs; j++) {
            p = (char *)&log->subpacket[i].rbs[j];

#ifdef CONFIG_DEBUG_PDCP
            DEBUG("pdcp_rx_received rb index = %d", log->subpacket[i].rbs[j].Rb_Cfg_Idx);
#endif

            /* constant values from ref-1 */

#ifdef DEBUG_VERBOSE
            num_data_pdu = *(unsigned int *)(p + 88 / 8);
            missing_sdus = *(unsigned int *)(p + 408 / 8);
#endif

            num_data_pdu_rst = *(unsigned int *)(p + 728 / 8);
            missing_sdus_rst = *(unsigned int *)(p + 1048 / 8);

            num_data_pdu_rx_bytes_rst = *(unsigned int *)(p + 760 / 8);

            VERBOSE("num_data_pdu #%d/%d = %d", i, j, num_data_pdu);
            VERBOSE("num_data_pdu_rst #%d/%d = %d", i, j, num_data_pdu_rst);
            VERBOSE("missing_sdus #%d/%d = %d", i, j, missing_sdus);
            VERBOSE("missing_sdus_rst #%d/%d = %d", i, j, missing_sdus_rst);

            VERBOSE("num_data_pdu_rx_bytes_rst #%d/%d = %d", i, j, num_data_pdu_rx_bytes_rst);

            pdcp_packetloss_received += missing_sdus_rst;
            pdcp_packet_received += num_data_pdu_rst;
            pdcp_rx_received += num_data_pdu_rx_bytes_rst;
        }
    }

    VERBOSE("total lost SDU = %llu", pdcp_packetloss_received);
    VERBOSE("total received PDU = %llu", pdcp_packet_received);
#ifdef CONFIG_DEBUG_PDCP
    DEBUG("[pdcp] pdcp_rx_received = %llu", pdcp_rx_received);
#endif

    /*
     * # accept statistics only when RRC is running
     *
     *  This RRC running verification is not truly required since the statistics get reset at the beginning of RRC and
     * any orphan statistics will be removed and not written to RDB. Although, to have the identical internal statistic
     * values all the time, the condition is kept.
     *
     */
    if (rrc_info.running) {
        acc_feed(PDCPTotalLossSDU, pdcp_packetloss_received, ts);
        acc_feed(PDCPTotalReceivedPDU, pdcp_packet_received, ts);
        acc_feed(PDCPBytesReceived, pdcp_rx_received, ts);
    }
}

#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_mac_dl_tb)
{
    struct lte_mac_dl_tb_t {
        uint32 subpacket_id : 8;
        uint32 subpacket_version : 8;
        uint32 subpacket_size : 16;
        char num_samples;

        char unknown[0];
    } PACK();

    declare_app_func_define_vars_for_log();

    /* declare_app_func_require_ver(); */
    declare_app_func_define_var_log(struct lte_mac_dl_tb_t);

    rdb_enter_csection();
    {
        servcell_info.TotalActiveTTIsReceived += log->num_samples;
        VERBOSE("TotalActiveTTIsReceived = %llu", servcell_info.TotalActiveTTIsReceived);

        rdb_leave_csection();
    }
}
#endif
#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_RX_MCS_BLER_MSG

#define LTE_ML1_DLM_LOG_PDSCH_STAT_MAX_CNT (25)

declare_app_func(lte_ml1_dlm_pdsch_stat_ind)
{

    struct lte_pdschstatindication_v36_record_t {
        /*! ********** PDSCH stat per record **************/

        /* OTA Tx time stamp */
        uint16 subframe_num : 4; ///< 0 ~9     //32bit word 1 start
        uint16 frame_num : 12;   ///< radio frame number 0-1023

        uint16 num_rbs : 8;    ///< number or rbs allocated
        uint16 num_layers : 8; ///< number of layers
                               // 32bit word 1 end

        /*! Num of transport blocks present or valid */
        uint16 num_transport_blocks_present : 8; // 32bit word 2 start
        uint16 serving_cell_index : 3;
        uint16 reserved0 : 5;

        uint8 num_rx_enabled : 8;             ///< number of antennas enabled
        uint8 reserved1 : 8;                  ///< Reserved for alignment - update if new fields are added
                                              // 32bit word 2 end
        uint8 rx_ant_phy_to_logic_mapping[4]; ///< mapping of physical antennas to logical antennas //32bit word 3 start
                                              // 32bit word 3 end

        /*! FIXED SIZE ALLOCATION based on "LTE_LL1_MAX_PDSCH_TRANMISSIONS_PER_TTI" */
        struct lte_pdschstatindication_v36_record_tb_t { // 32bit word 4 - 9 start
            uint8 harq_id : 4;                           ///< (up to 8 for FDD, up to 15 for TDD)
            uint8 rv : 2;                                ///< (0, 1, 2, 3)
            uint8 ndi : 1;                               ///< (0 or 1)
            uint8 crc_result : 1;

            uint8 rnti_type : 4;
            uint8 tb_index : 1;               ///< (0 or  1 (MIMO))
            uint8 discarded_retx_present : 1; ///< Are discarded reTx present
            uint8 discard_retx_details : 2;   ///< Detailed information for discarded reTx
                                              ///< 0 - No Details
                                              ///< 1 - Discard and ACK
                                              ///< 2 - Discard and NAK
                                              ///< 3 - No Initial Grant Discard and ACK

            uint16 did_recombining : 1;
            uint16 reserved2 : 15; ///< Reserved for alignment - update if new fields are added
                                   // inner struct 32bit word 1 end

            uint16 tb_size; /// Size in bytes

            uint16 mcs : 8;     ///< ( MCS index 0 - 31)
            uint16 num_rbs : 8; ///< number of rbs allocated to this tb
                                // inner struct 32bit word 2 end

            uint8 modulation_type : 8; ///< modulation type (QPSK, 64QAM, etc.)

            uint8 qed2_interim_status : 1; ///< QED2 Interim Status
            uint8 qed_num_iterations : 7;  ///< QED Iterations (0 == OFF)

            uint16 reserved3 : 16; ///< Reserved for alignment - update if new fields are added
                                   // inner struct 32bit word 3 end
        } transport_blocks[2];
        // 32bit word 4 - 9 end

        uint16 pmch_id : 8;    ///< PMCH id: only applicable to pmch decodes //32bit word 8 start
        uint16 area_id : 8;    ///< Area id used: only applicable to pmch decodes
        uint16 reserved4 : 16; ///< Reserved for alignment - update if new fields are added
                               // 32bit word 10 end
    };

    struct lte_pdschstatindication_v34_record_tb_t {
        uint8 harq_id : 4;
        uint8 rv : 2;
        uint8 ndi : 1;
        uint8 crc_result : 1;
        uint8 rnti_type : 4;
        uint8 tb_index : 1;
        uint8 discarded_retx_present : 2;
        uint8 did_recombining : 1;
        uint16 tb_size;
        uint8 mcs;
        uint8 num_rbs;
        uint8 modulation_type;
        uint8 qed2_interim_status : 1;
        uint8 qed2_iterations : 7;
    } PACK();

    struct lte_pdschstatindication_v34_record_t {
        uint16 subframe_num : 4;
        uint16 frame_num : 12;
        uint8 num_rbs : 8;
        uint8 num_layers : 8;
        uint16 num_transport_blocks_present : 8;
        uint8 serving_cell_index : 3;
        uint16 reserved : 5;
        struct lte_pdschstatindication_v34_record_tb_t transport_blocks[2];
        uint8 pmch_id;
        uint8 area_id;
    } PACK();

    struct lte_pdschstatindication_v24_record_tb_t {
        uint8 harq_id : 4;
        uint8 rv : 2;
        uint8 ndi : 1;
        uint8 crc_result : 1;
        uint8 rnti_type : 4;
        uint8 tb_index : 1;
        uint8 discarded_retx_present : 1;
        uint8 did_recombining : 1;
        uint8 reserved1 : 1;
        uint16 tb_size;
        uint8 mcs;
        uint8 num_rbs;
        uint8 modulation_type;
        uint8 reserved2;
    } PACK();

    struct lte_pdschstatindication_v24_record_t {
        uint16 subframe_num : 4;
        uint16 frame_num : 12;
        uint8 num_rbs : 8;
        uint8 num_layers : 8;
        uint16 num_transport_blocks_present : 8;
        uint8 serving_cell_index : 3;
        uint8 hsic_enabled : 1;
        uint16 reserved : 4;
        struct lte_pdschstatindication_v24_record_tb_t transport_blocks[2];
        uint8 pmch_id;
        uint8 area_id;
    } PACK();

    struct lte_ml1_dlm_pdsch_stat_ind_t {
        uint32 version : 8;
        uint32 num_records : 8;
        uint32 reserved : 16;
        union
        {
            struct lte_pdschstatindication_v36_record_t records_v34[LTE_ML1_DLM_LOG_PDSCH_STAT_MAX_CNT];
            struct lte_pdschstatindication_v24_record_t records_v24[LTE_ML1_DLM_LOG_PDSCH_STAT_MAX_CNT];
        };
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(24, 36);
    declare_app_func_define_var_log(struct lte_ml1_dlm_pdsch_stat_ind_t);

    int i;
    int j;
    int total_rbs;

    int ack_nack_decision;
    int mod_type;

    VERBOSE("log->num_records = %d", log->num_records);
    VERBOSE("len = %d", len);
    VERBOSE("sizeof lte_ml1_dlm_pdsch_stat_ind_t = %d", sizeof(struct lte_ml1_dlm_pdsch_stat_ind_t));

    int nt; /* new transmission flag */
    int max_retx;
    int tb_count;

    /* Only supporting v24 and v36 of structure. */
    /* ------------ v36 -----------------------------------------------------------------*/
    if (declare_app_func_ver_match(36)) {
        struct lte_pdschstatindication_v36_record_t *rec;
        struct lte_pdschstatindication_v36_record_tb_t *tb;
        VERBOSE("sizeof lte_pdschstatindication_record_t = %d", sizeof(struct lte_pdschstatindication_v34_record_t));

        rdb_enter_csection();
        {
            /* get max retransmission */
            max_retx = (servcell_info.max_dl_harq_transmissions < 2) ? 0 : servcell_info.max_dl_harq_transmissions - 1;

            total_rbs = 0;
            for (i = 0; i < log->num_records; i++) {
                rec = &log->records_v34[i];

                VERBOSE("rec->num_rbs #%d = %d", i, rec->num_rbs);
                total_rbs += rec->num_rbs;

                tb_count = rec->num_transport_blocks_present;

                for (j = 0; j < tb_count; j++) {
                    tb = &rec->transport_blocks[j];

                    /* bypass if HARQ ID is out of range */
                    if (!__in_array(tb->harq_id, servcell_info.HarqDlAck_valid)) {
                        ERR("HARQ ID out of range (harq id = %d)", tb->harq_id);
                        continue;
                    }

                    /* it is a new transmission if harq is new or ndi is not toggle - 3GPP TS 36.321 5.3.2.2 */
                    nt = !servcell_info.HarqDlNdi_valid[tb->harq_id] || (servcell_info.HarqDlNdi[tb->harq_id] != tb->ndi) || !tb->did_recombining;

                    /* update ndi tracking information */
                    servcell_info.HarqDlNdi_valid[tb->harq_id] = 1;
                    servcell_info.HarqDlNdi[tb->harq_id] = tb->ndi;

                    VERBOSE("TB size[%d][%d] = %d", i, j, tb->tb_size);

                    /* get ack flag by Qualcomm formula */
                    ack_nack_decision = tb->discarded_retx_present ? (tb->mcs <= 28 ? 1 : tb->tb_size != 5) : tb->crc_result;

                    VERBOSE("Rx MCS index[%d][%d] = %d", i, j, tb->mcs);
                    if (tb->mcs >= 0 && tb->mcs < MCS_INDEX_NUM_ELEMENTS) {
                        servcell_info.RxMcsIndex |= (0x1 << tb->mcs);
                        servcell_info.RxMcsIndexValid = 1;

                        // fill mcs / crc fail data distribution
                        servcell_info.McsIndexCount[tb->mcs]++;

                        if (!tb->crc_result) {
                            servcell_info.CrcFailCount[tb->mcs]++;
                        }
                    }

                    if (nt) {
                        /* increase BLER count of initial transport block */
                        servcell_info.MACiBLERReceivedAckNackCount++;
                        if (!ack_nack_decision)
                            servcell_info.MACiBLERReceivedNackCount++;

                        if (servcell_info.HarqDlAck_valid[tb->harq_id] == max_retx) {
                            servcell_info.MACrBLERReceivedAckNackCount++;
                            if (!servcell_info.HarqDlAck[tb->harq_id])
                                servcell_info.MACrBLERReceivedNackCount++;
                        }

                        /* reset valid flag */
                        servcell_info.HarqDlAck_valid[tb->harq_id] = 0;
                    } else {
                        /* update last harq ack decision */
                        servcell_info.HarqDlAck_valid[tb->harq_id]++;
                        servcell_info.HarqDlAck[tb->harq_id] = ack_nack_decision;
                    }

                    /* 2:QPSK, 4:16QAM, 6:64QAM, 8:256QAM */
                    mod_type = (tb->modulation_type >> 1) - 1;
                    if (0 <= mod_type && mod_type < RX_MOD_DISTR_NUM_TYPES)
                        distr_feed(&servcell_info.ReceivedModulationDistribution, mod_type);
                }
            }

            servcell_info.TotalPRBsReceived += total_rbs;
            VERBOSE("TotalPRBsReceived = %llu", servcell_info.TotalPRBsReceived);

            if (servcell_info.MACiBLERReceivedAckNackCount)
                VERBOSE("[BLER] MACiBLERReceived = %llu/%llu/%lld", servcell_info.MACiBLERReceivedNackCount,
                        servcell_info.MACiBLERReceivedAckNackCount,
                        servcell_info.MACiBLERReceivedNackCount * 100 / servcell_info.MACiBLERReceivedAckNackCount);
            if (servcell_info.MACrBLERReceivedAckNackCount)
                VERBOSE("[BLER] MACrBLERReceived = %llu/%llu/%lld", servcell_info.MACrBLERReceivedNackCount,
                        servcell_info.MACrBLERReceivedAckNackCount,
                        servcell_info.MACrBLERReceivedNackCount * 100 / servcell_info.MACrBLERReceivedAckNackCount);

            rdb_leave_csection();

            VERBOSE("servcell_info.RxMcsIndex = 0x%08x", servcell_info.RxMcsIndex);
        }
        /* ------------ v24 -----------------------------------------------------------------*/
    } else {
        struct lte_pdschstatindication_v24_record_t *rec;
        struct lte_pdschstatindication_v24_record_tb_t *tb;
        VERBOSE("sizeof lte_pdschstatindication_record_t = %d", sizeof(struct lte_pdschstatindication_v24_record_t));

        rdb_enter_csection();
        {
            /* get max retransmission */
            max_retx = (servcell_info.max_dl_harq_transmissions < 2) ? 0 : servcell_info.max_dl_harq_transmissions - 1;

            total_rbs = 0;
            for (i = 0; i < log->num_records; i++) {
                rec = &log->records_v24[i];

                VERBOSE("rec->num_rbs #%d = %d", i, rec->num_rbs);
                total_rbs += rec->num_rbs;

                tb_count = rec->num_transport_blocks_present;

                for (j = 0; j < tb_count; j++) {
                    tb = &rec->transport_blocks[j];

                    /* bypass if HARQ ID is out of range */
                    if (!__in_array(tb->harq_id, servcell_info.HarqDlAck_valid)) {
                        ERR("HARQ ID out of range (harq id = %d)", tb->harq_id);
                        continue;
                    }

                    /* it is a new transmission if harq is new or ndi is not toggle - 3GPP TS 36.321 5.3.2.2 */
                    nt = !servcell_info.HarqDlNdi_valid[tb->harq_id] || (servcell_info.HarqDlNdi[tb->harq_id] != tb->ndi) || !tb->did_recombining;

                    /* update ndi tracking information */
                    servcell_info.HarqDlNdi_valid[tb->harq_id] = 1;
                    servcell_info.HarqDlNdi[tb->harq_id] = tb->ndi;

                    /* get ack flag by Qualcomm formula */
                    ack_nack_decision = tb->discarded_retx_present ? (tb->mcs <= 28 ? 1 : tb->tb_size != 5) : tb->crc_result;

                    VERBOSE("Rx MCS index[%d][%d] = %d", i, j, tb->mcs);
                    if (tb->mcs >= 0 && tb->mcs < 31) {
                        servcell_info.RxMcsIndex |= (0x1 << tb->mcs);
                        servcell_info.RxMcsIndexValid = 1;
                    }

                    if (nt) {
                        /* increase BLER count of initial transport block */
                        servcell_info.MACiBLERReceivedAckNackCount++;
                        if (!ack_nack_decision)
                            servcell_info.MACiBLERReceivedNackCount++;

                        if (servcell_info.HarqDlAck_valid[tb->harq_id] == max_retx) {
                            servcell_info.MACrBLERReceivedAckNackCount++;
                            if (!servcell_info.HarqDlAck[tb->harq_id])
                                servcell_info.MACrBLERReceivedNackCount++;
                        }

                        /* reset valid flag */
                        servcell_info.HarqDlAck_valid[tb->harq_id] = 0;
                    } else {
                        /* update last harq ack decision */
                        servcell_info.HarqDlAck_valid[tb->harq_id]++;
                        servcell_info.HarqDlAck[tb->harq_id] = ack_nack_decision;
                    }

                    /* 2:QPSK, 4:16QAM, 6:64QAM, 8:256QAM */
                    mod_type = (tb->modulation_type >> 1) - 1;
                    if (0 <= mod_type && mod_type < RX_MOD_DISTR_NUM_TYPES)
                        distr_feed(&servcell_info.ReceivedModulationDistribution, mod_type);
                }
            }

            servcell_info.TotalPRBsReceived += total_rbs;
            VERBOSE("TotalPRBsReceived = %llu", servcell_info.TotalPRBsReceived);

            if (servcell_info.MACiBLERReceivedAckNackCount)
                VERBOSE("[BLER] MACiBLERReceived = %llu/%llu/%lld", servcell_info.MACiBLERReceivedNackCount,
                        servcell_info.MACiBLERReceivedAckNackCount,
                        servcell_info.MACiBLERReceivedNackCount * 100 / servcell_info.MACiBLERReceivedAckNackCount);
            if (servcell_info.MACrBLERReceivedAckNackCount)
                VERBOSE("[BLER] MACrBLERReceived = %llu/%llu/%lld", servcell_info.MACrBLERReceivedNackCount,
                        servcell_info.MACrBLERReceivedAckNackCount,
                        servcell_info.MACrBLERReceivedNackCount * 100 / servcell_info.MACrBLERReceivedAckNackCount);

            VERBOSE("servcell_info.RxMcsIndex = 0x%08x", servcell_info.RxMcsIndex);

            rdb_leave_csection();
        }
    }
}
#endif /* INCLUDE_LTE_RX_MCS_BLER_MSG */

#ifdef INCLUDE_COMMON_MSG
declare_app_func(lte_ml1_gm_ded_cfg_log)
{
    struct lte_ml1_gm_ded_cfg_log_t {
        uint8 version : 8;
        uint8 standards_version : 8;
        uint16 reserved1;

        uint32 sr_cfg_present : 1;
        uint32 sr_enable : 1;
        uint32 sr_pucch_resource_index : 11;
        uint32 sr_configuration_index : 8;
        uint32 sr_maximum_transmissions : 7;
        uint32 sr_prohibit_timer : 3;

        uint32 cqi_cfg_present : 1;
        uint32 cqi_enable : 1;
        uint32 cqi_reporting_a_periodic_present : 1;
        uint32 cqi_reporting_a_periodic : 3;
        uint32 nominal_pdsch_rs_epre_offset : 4;
        uint32 cqi_reporting_periodic_present : 1;
        uint32 cqi_reporting_periodic_enable : 1;
        uint32 cqi_format_indicator : 1;
        uint32 simul_acknak_and_cqi : 1;
        uint32 cqi_pucch_resource_index : 11;
        uint32 reserved2 : 8;
        uint32 ri_config_index : 10;
        uint32 ri_config_enabled : 1;
        uint32 k : 3;
        uint32 cqi_pmi_config_index : 10;
        uint32 cqi_mask : 1;

        uint32 srs_cfg_present : 1;
        uint32 srs_enable : 1;
        uint32 srs_bandwidth : 2;
        uint32 srs_hopping_bandwidth : 2;
        uint32 reserved2_1 : 1;
        uint32 frequency_domain_position : 5;
        uint32 duration : 1;
        uint32 tx_combination : 1;
        uint32 cyclic_shift : 3;
        uint32 srs_config_index : 10;

        uint32 phr_cfg_present : 1;
        uint32 phr_enable : 1;
        uint32 periodic_phr_timer_value : 10;
        uint32 prohibit_phr_timer_value : 10;
        uint32 dl_pathloss_change_threshold : 3;

        uint32 pusch_tpc_cfg_present : 1;
        uint32 pusch_tpc_enable : 1;
        uint32 pusch_tpc_dci_format : 1;
        uint32 pusch_tpc_index : 5;
        uint32 reserved3 : 11;
        uint32 pusch_tpc_rnti : 16;

        uint32 pucch_tpc_cfg_present : 1;
        uint32 pucch_tpc_enable : 1;
        uint32 pucch_tpc_dci_format : 1;
        uint32 reserved4 : 2;
        uint32 pucch_tpc_index : 5;
        uint32 reserved5 : 6;
        uint32 pucch_tpc_rnti : 16;

        uint32 ulsch_cfg_present : 1;
        uint32 max_harq_transmissions : 5;
        uint32 tti_bundling_enabled : 1;

        uint32 ul_power_control_cfg_present : 1;
        uint32 accumulation_enabled : 1;
        uint32 p0_ue_pusch : 4;
        uint32 reserved6 : 3;
        uint32 p0_ue_pucch : 4;
        uint32 delta_mcs_enabled : 1;
        uint32 p_srs_offset : 4;

        uint32 pusch_cfg_present : 1;
        uint32 delta_offset_ack_index : 4;
        uint32 delta_offset_cqi_index : 4;
        uint32 delta_offset_ri_index : 4;

        uint32 pucch_cfg_present : 1;
        uint32 pucch_cfg_enable : 1;
        uint32 ack_nak_feedback_mode : 1;
        uint32 ack_nak_repitition_enabled : 1;
        uint32 ack_nak_repitition_factor : 3;
        uint32 reserved7 : 3;
        uint32 n1_pucch_anrep : 11;

        uint32 antenna_cfg_present : 1;
        uint32 transmission_mode : 3;
        uint32 tx_antenna_selection_enabled : 1;
        uint32 _tx_antenna_selection_control : 1;
        uint32 reserved8 : 15;
        uint32 antenna_codebook_subset_restriction_lo : 32;
        uint32 antenna_codebook_subset_restriction_hi : 32;

        uint32 cdrx_config_present : 1;
        uint32 cdrx_enable : 1;
        uint32 on_duration_timer : 8;
        uint32 inactivity_timer : 12;
        uint32 dl_drx_retx_timer : 8;
        uint32 reserved9 : 2;
        uint32 long_cycle_len : 12;
        uint32 cycle_start_offset : 12;
        uint32 reserved10 : 8;
        uint32 short_drx_cycle_enable : 1;
        uint32 short_drx_cycle_len : 10;
        uint32 short_drx_timer : 5;
        uint32 reserved11 : 16;
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(5);
    declare_app_func_define_var_log(struct lte_ml1_gm_ded_cfg_log_t);

    VERBOSE("ulsch_cfg_present = %d", log->ulsch_cfg_present);
    VERBOSE("max_harq_transmissions = %d", log->max_harq_transmissions);

    rdb_enter_csection();
    {
        if (log->ulsch_cfg_present) {
            if (servcell_info.max_ul_harq_transmissions != log->max_harq_transmissions) {
                DEBUG("max_ul_harq_transmissions changed (max_retx=%d)", log->max_harq_transmissions);
                servcell_info.max_ul_harq_transmissions = log->max_harq_transmissions;
            } else {
                DEBUG("max_ul_harq_transmissions (max_retx=%d)", log->max_harq_transmissions);
            }
        } else {
            VERBOSE("max_ul_harq_transmissions not valid");
        }
        if (log->pusch_cfg_present) {
            VERBOSE("delta_offset_ack_index %d", log->delta_offset_ack_index);
            VERBOSE("delta_offset_cqi_index %d", log->delta_offset_cqi_index);
            VERBOSE("delta_offset_ri_index %d", log->delta_offset_ri_index);
        }
        rdb_leave_csection();
    }
}

#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ml1_gm_pdcch_phich_info)
{

    struct lte_pdcchphichindicationreport_v33_record_phich_t {
        uint32 cell_index : 3;
        uint32 phich_included : 1;
        uint32 phich1_included : 1;
        uint32 phich_value : 1;
        uint32 phich1_value : 1;
        uint32 reserved : 25;
    } PACK();

    struct lte_pdcchphichindicationreport_v33_pdcchinfo_t {
        uint8 serv_cell_index : 3;
        uint8 rnti_type : 4;
        uint16 pdcch_payload_size : 7;
        uint8 aggregation_level : 2;
        uint8 search_space : 1;
        uint8 sps_grant_type : 3;
        uint8 new_dl_tx : 1;
        uint8 num_dl_trblks : 2;
        uint32 reserved : 9;
        uint32 s0_index : 5;
        uint32 s1_index : 5;
        uint32 s2_index : 5;
        uint32 s3_index : 5;
        uint32 msleep : 1;
        uint32 usleep : 1;
        uint32 usleep_duration : 5;
        uint32 fake_pdccd_sf : 1;
        uint32 reserved2 : 4;
    } PACK();

    struct lte_pdcchphichindicationreport_v33_record_t {
        uint32 num_pdcch_results : 3;
        uint32 num_phich_results : 3;
        uint32 pdcch_timing_sfn : 10;
        uint32 pdcch_timing_subfn : 4;
        uint32 reserved1 : 12;
        uint64 reserved2;

        struct lte_pdcchphichindicationreport_v33_record_phich_t phich[5];
        struct lte_pdcchphichindicationreport_v33_pdcchinfo_t pdcch[8];
    } PACK();

    struct lte_pdcchphichindicationreport_v25_record_phich_t {
        uint32 cell_index : 3;
        uint32 phich_included : 1;
        uint32 phich1_included : 1;
        uint32 phich_value : 1;
        uint32 phich1_value : 1;
        uint32 reserved : 25;
    } PACK();

    struct lte_pdcchphichindicationreport_v25_pdcchinfo_t {
        uint8 serv_cell_index : 3;
        uint8 rnti_type : 4;
        uint16 pdcch_payload_size : 7;
        uint8 aggregation_level : 2;
        uint8 search_space : 1;
        uint8 sps_grant_type : 3;
        uint8 new_dl_tx : 1;
        uint8 num_dl_trblks : 2;
        uint32 reserved : 9;
        uint32 s0_index : 5;
        uint32 s1_index : 5;
        uint32 s2_index : 5;
        uint32 s3_index : 5;
        uint32 reserved2 : 12;
    } PACK();

    struct lte_pdcchphichindicationreport_v25_record_t {
        uint32 num_pdcch_results : 3;
        uint32 num_phich_results : 3;
        uint32 pdcch_timing_sfn : 10;
        uint32 pdcch_timing_subfn : 4;
        uint32 reserved : 12;

        struct lte_pdcchphichindicationreport_v25_record_phich_t phich[3];
        struct lte_pdcchphichindicationreport_v25_pdcchinfo_t pdcch[8];
    } PACK();

    struct lte_ml1_gm_pdcch_phich_info_t {
        uint8 version;

        uint32 duplex_mode : 1;
        uint32 ul_dl_config : 4;
        uint32 reserved : 11;
        uint8 number_of_records;

        union
        {
            struct lte_pdcchphichindicationreport_v25_record_t info_records_v25[0];
            struct lte_pdcchphichindicationreport_v33_record_t info_records_v33[0];
        };
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(25, 33);
    declare_app_func_define_var_log(struct lte_ml1_gm_pdcch_phich_info_t);

    int i;
    int j;

    struct subfn_t *subfn;

    int nack;
    int ack;

    time_t now = get_monotonic_ms();

    VERBOSE("number_of_records=%d", log->number_of_records);

    /* Only supporting v25 and v33 of structure. */
    /* ------------ v33 -----------------------------------------------------------------*/
    if (declare_app_func_ver_match(33)) {
        struct lte_pdcchphichindicationreport_v33_record_phich_t *phich;
#ifdef DEBUG_VERBOSE
        struct lte_pdcchphichindicationreport_v33_pdcchinfo_t *pdcch;
#endif
        struct lte_pdcchphichindicationreport_v33_record_t *rec;
        VERBOSE("size of record=%d", sizeof(struct lte_pdcchphichindicationreport_v33_record_t));

        rdb_enter_csection();
        {
            for (i = 0; i < log->number_of_records; i++) {
                rec = &log->info_records_v33[i];

                VERBOSE("pdcch_timing_sfn[%d]=%d", i, rec->pdcch_timing_sfn);
                VERBOSE("pdcch_timing_subfn[%d]=%d", i, rec->pdcch_timing_subfn);
                /* DEBUG("num_pdcch_results#%d=%d",i,rec->num_pdcch_results); */

                for (j = 0; j < rec->num_pdcch_results; j++) {
#ifdef DEBUG_VERBOSE
                    pdcch = &rec->pdcch[j];
#endif
                    VERBOSE("pdcch payload [%d][%d]=%d", i, j, pdcch->pdcch_payload_size);
                    VERBOSE("pdcch aggregation_level [%d][%d]=%d", i, j, pdcch->aggregation_level);
                }

                for (j = 0; j < rec->num_phich_results; j++) {
                    phich = &rec->phich[j];

                    /* get ack and nack */
                    nack = (phich->phich_included && !phich->phich_value) || (phich->phich1_included && !phich->phich1_value);
                    ack = (phich->phich_included && phich->phich_value) || (phich->phich1_included && phich->phich1_value);

                    if (ack || nack) {
                        VERBOSE("[BLER] PHICH %s received - sfn=%d,sub-fn=%d", ack ? "ACK" : "NAK", rec->pdcch_timing_sfn, rec->pdcch_timing_subfn);

                        subfn = get_subfn(servcell_info.HarqUlLast, rec->pdcch_timing_sfn, rec->pdcch_timing_subfn);
                        if (subfn->tx_valid && !__is_expired(subfn->timestamp, now, HARQ_TIME_WINDOW_MSEC)) {
                            VERBOSE("[BLER] rHARQ, got Tx - phich=%s,sfn=%d,sub-fn=%d", ack ? "ACK" : "NAK", rec->pdcch_timing_sfn,
                                    rec->pdcch_timing_subfn);

                            /* Tx already sent, we can count ACK/NAK now */
                            servcell_info.MACrBLERSentAckNackCount++;
                            if (nack)
                                servcell_info.MACrBLERSentNackCount++;

                            /* invalidate used subfn */
                            subfn->ack_valid = 0;
                            subfn->tx_valid = 0;

                        } else {
                            VERBOSE("[BLER] rHARQ, wait for Tx - phich=%s,sfn=%d,sub-fn=%d", ack ? "ACK" : "NAK", rec->pdcch_timing_sfn,
                                    rec->pdcch_timing_subfn);

                            /* store ack status */
                            subfn->ack_valid = 1;
                            subfn->tx_valid = 0;
                            subfn->ack = ack ? 1 : 0;
                            subfn->timestamp = now;
                        }
                    }
                }
            }

            rdb_leave_csection();
        }
        /* ------------ v25 -----------------------------------------------------------------*/
    } else {
        struct lte_pdcchphichindicationreport_v25_record_phich_t *phich;
        struct lte_pdcchphichindicationreport_v25_record_t *rec;
        VERBOSE("size of record=%d", sizeof(struct lte_pdcchphichindicationreport_v25_record_t));

        rdb_enter_csection();
        {
            for (i = 0; i < log->number_of_records; i++) {
                rec = &log->info_records_v25[i];

                /* DEBUG("num_pdcch_results#%d=%d",i,rec->num_pdcch_results); */

                for (j = 0; j < rec->num_phich_results; j++) {
                    phich = &rec->phich[j];

                    /* get ack and nack */
                    nack = (phich->phich_included && !phich->phich_value) || (phich->phich1_included && !phich->phich1_value);
                    ack = (phich->phich_included && phich->phich_value) || (phich->phich1_included && phich->phich1_value);

                    if (ack || nack) {
                        VERBOSE("[BLER] PHICH %s received - sfn=%d,sub-fn=%d", ack ? "ACK" : "NAK", rec->pdcch_timing_sfn, rec->pdcch_timing_subfn);

                        subfn = get_subfn(servcell_info.HarqUlLast, rec->pdcch_timing_sfn, rec->pdcch_timing_subfn);
                        if (subfn->tx_valid && !__is_expired(subfn->timestamp, now, HARQ_TIME_WINDOW_MSEC)) {
                            VERBOSE("[BLER] rHARQ, got Tx - phich=%s,sfn=%d,sub-fn=%d", ack ? "ACK" : "NAK", rec->pdcch_timing_sfn,
                                    rec->pdcch_timing_subfn);

                            /* Tx already sent, we can count ACK/NAK now */
                            servcell_info.MACrBLERSentAckNackCount++;
                            if (nack)
                                servcell_info.MACrBLERSentNackCount++;

                            /* invalidate used subfn */
                            subfn->ack_valid = 0;
                            subfn->tx_valid = 0;

                        } else {
                            VERBOSE("[BLER] rHARQ, wait for Tx - phich=%s,sfn=%d,sub-fn=%d", ack ? "ACK" : "NAK", rec->pdcch_timing_sfn,
                                    rec->pdcch_timing_subfn);

                            /* store ack status */
                            subfn->ack_valid = 1;
                            subfn->tx_valid = 0;
                            subfn->ack = ack ? 1 : 0;
                            subfn->timestamp = now;
                        }
                    }
                }
            }

            rdb_leave_csection();
        }
    }
}
#endif

#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ml1_gm_dci_info)
{
    struct lte_dciinformationreport_v34_record_ulgrant_t {
        uint8 grant_present : 1;
        uint32 cell_index : 3;
        uint32 resource_allocation_type : 1;
        uint32 riv_width : 4;
        uint32 riv_value : 14;
        uint8 hopping_flag : 1;
        uint32 mcs_index : 5;
        uint32 ndi : 1;
        uint32 tpc : 2;
        uint32 cycle_shift_dmrs : 3;
        uint32 k_of_dci_0 : 3;
        uint32 ul_index_dai : 3;
        uint32 cqi_request : 2;
        uint32 srs_request : 1;
        uint32 start_of_resource_block : 7;
        uint32 number_of_resource_blocks : 7;
        uint32 tbs_index : 5;
        uint32 reserved : 1;
        uint32 modulation_type : 2;
        uint32 start_of_resource_block_2 : 7;
        uint32 number_of_resource_blocks_2 : 7;
        uint32 rbg_size : 3;
        uint32 redundancy_version_index : 2;
        uint32 harq_id : 3;
        uint32 rnti_type : 4;
        uint32 aggregation_level : 4;
        uint32 search_space : 1;
        uint32 tx_antenna_selection : 3;
        uint32 reserved1 : 28;
    } PACK();

    struct lte_dciinformationreport_v34_record_dlgrant_t {
        uint32 dl_grant_present : 1;
        uint32 dl_grant_cc_id : 3;
        uint32 dl_grant_format_type : 4;
        uint32 num_ack_nak_bits : 2;
        uint32 dl_grant_tpc_command : 3;
        uint32 dai : 3;
        uint32 dl_grant_srs_req : 1;
        uint32 dl_grant_n_cee : 10;
        uint32 rnti_type : 4;
        uint32 reserved1 : 1;
        uint32 aggregation_level : 4;
        uint32 search_space : 1;
        uint32 reserved2 : 27;
    } PACK();

    struct lte_dciinformationreport_v34_record_tpcdci_t {
        uint32 tpc_dci_present : 1;
        uint32 tpc_dci_fomrat_type : 1;
        uint32 tpc_dci_rnti_type : 4;
        uint32 tpc_dci_tpc_command : 3;
        uint32 pdcch_order_present_data : 1;
        uint32 reserved : 22;
    } PACK();

    struct lte_dciinformationreport_v34_record_t {
        uint32 sfn : 10;
        uint32 sub_fn : 4;

        uint32 num_ul_grant : 3;
        uint32 reserved1 : 15;

        struct lte_dciinformationreport_v34_record_ulgrant_t ul_grant_info[4];

        uint32 num_dl_grants : 3;
        uint32 reserved2 : 29;

        struct lte_dciinformationreport_v34_record_dlgrant_t dl_grant_info[5];

        struct lte_dciinformationreport_v34_record_tpcdci_t tpc_dci_info;

    } PACK();

    struct lte_dciinformationreport_v26_record_ulgrant_t {
        uint8 grant_present : 1;
        uint32 cell_index : 3;
        uint32 resource_allocation_type : 1;
        uint32 riv_width : 4;
        uint32 riv_value : 14;
        uint8 hopping_flag : 1;
        uint32 mcs_index : 5;
        uint32 ndi : 1;
        uint32 tpc : 2;
        uint32 cycle_shift_dmrs : 3;
        uint32 k_of_dci_0 : 3;
        uint32 ul_index_dai : 2;
        uint32 cqi_request : 2;
        uint32 srs_request : 1;
        uint32 start_of_resource_block : 7;
        uint32 number_of_resource_blocks : 7;
        uint32 tbs_index : 5;
        uint32 modulation_type : 2;
        uint32 start_of_resource_block_2 : 7;
        uint32 number_of_resource_blocks_2 : 7;
        uint32 rbg_size : 3;
        uint32 redundancy_version_index : 2;
        uint32 harq_id : 3;
        uint32 pdcch_order_present : 1;
        uint32 rnti_type : 4;
        uint32 aggregation_level : 4;
        uint32 search_space : 1;
        uint32 tx_antenna_selection : 3;
        uint32 reserved : 29;
    } PACK();

    struct lte_dciinformationreport_v26_record_dlgrant_t {
        uint32 dl_grant_present : 1;
        uint32 dl_grant_cc_id : 3;
        uint32 dl_grant_format_type : 4;
        uint32 num_ack_nak_bits : 2;
        uint32 dl_grant_tpc_command : 3;
        uint32 dai : 2;
        uint32 dl_grant_srs_req : 1;
        uint32 dl_grant_n_cee : 10;
        uint32 rnti_type : 4;
        uint32 reserved1 : 2;
        uint32 aggregation_level : 4;
        uint32 search_space : 1;
        uint32 reserved2 : 27;
    } PACK();

    struct lte_dciinformationreport_v26_record_tpcdci_t {
        uint32 tpc_dci_present : 1;
        uint32 tpc_dci_fomrat_type : 1;
        uint32 tpc_dci_rnti_type : 4;
        uint32 tpc_dci_tpc_command : 3;
        uint32 reserved : 23;
    } PACK();

    struct lte_dciinformationreport_v26_record_t {
        uint32 sfn : 10;
        uint32 sub_fn : 4;

        uint32 num_ul_grant : 3;
        uint32 reserved1 : 15;

        struct lte_dciinformationreport_v26_record_ulgrant_t ul_grant_info[4];

        uint32 num_dl_grants : 3;
        uint32 reserved2 : 29;

        struct lte_dciinformationreport_v26_record_dlgrant_t dl_grant_info[3];

        struct lte_dciinformationreport_v26_record_tpcdci_t tpc_dci_info;

    } PACK();

    struct lte_ml1_gm_dci_info_t {
        uint32 version : 8;

        uint32 reserved : 15;
        uint32 duplex_mode : 1;
        uint32 number_of_records : 8;
        union
        {
            struct lte_dciinformationreport_v34_record_t records_v34[0];
            struct lte_dciinformationreport_v26_record_t records_v26[0];
        };
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(26, 34);
    declare_app_func_define_var_log(struct lte_ml1_gm_dci_info_t);

    int i;
    int j;
    int total_rbs;

    total_rbs = 0;

    VERBOSE("number_of_records = %d", log->number_of_records);
    VERBOSE("len = %d", len);
    VERBOSE("sizeof lte_ml1_gm_dci_info_t = %d", sizeof(struct lte_ml1_gm_dci_info_t));

    /* Only supporting v26 and v34 of structure. */
    /* ------------ v34 -----------------------------------------------------------------*/
    if (declare_app_func_ver_match(34)) {
        struct lte_dciinformationreport_v34_record_t *rec;
        struct lte_dciinformationreport_v34_record_ulgrant_t *ul_grant_info;

        VERBOSE("sizeof lte_dciinformationreport_record_t = %d", sizeof(struct lte_dciinformationreport_v34_record_t));
        VERBOSE("sizeof lte_dciinformationreport_record_ulgrant_t = %d", sizeof(struct lte_dciinformationreport_v34_record_ulgrant_t));
        VERBOSE("sizeof lte_dciinformationreport_record_dlgrant_t = %d", sizeof(struct lte_dciinformationreport_v34_record_dlgrant_t));
        VERBOSE("sizeof lte_dciinformationreport_record_tpcdci_t = %d", sizeof(struct lte_dciinformationreport_v34_record_tpcdci_t));

        for (i = 0; i < log->number_of_records; i++) {
            rec = &log->records_v34[i];
            VERBOSE("sfn #%d = %d", i, rec->sfn);
            VERBOSE("sub_fn #%d = %d", i, rec->sub_fn);
            VERBOSE("num_ul_grant #%d = %d", i, rec->num_ul_grant);
            VERBOSE("num_dl_grants #%d = %d", i, rec->num_dl_grants);

            for (j = 0; j < rec->num_ul_grant; j++) {
                ul_grant_info = &rec->ul_grant_info[j];
                VERBOSE("grant_present #%d/%d = %d", i, j, ul_grant_info->grant_present);
                VERBOSE("start_of_resource_block #%d/%d = %d", i, j, ul_grant_info->start_of_resource_block);
                VERBOSE("number_of_resource_blocks #%d/%d = %d", i, j, ul_grant_info->number_of_resource_blocks);
                VERBOSE("number_of_resource_blocks_2 #%d/%d = %d", i, j, ul_grant_info->number_of_resource_blocks_2);
                total_rbs += ul_grant_info->number_of_resource_blocks;
            }
        }
        /* ------------ v26 -----------------------------------------------------------------*/
    } else {
        struct lte_dciinformationreport_v26_record_t *rec;
        struct lte_dciinformationreport_v26_record_ulgrant_t *ul_grant_info;

        VERBOSE("sizeof lte_dciinformationreport_record_t = %d", sizeof(struct lte_dciinformationreport_v26_record_t));
        VERBOSE("sizeof lte_dciinformationreport_record_ulgrant_t = %d", sizeof(struct lte_dciinformationreport_v26_record_ulgrant_t));
        VERBOSE("sizeof lte_dciinformationreport_record_dlgrant_t = %d", sizeof(struct lte_dciinformationreport_v26_record_dlgrant_t));
        VERBOSE("sizeof lte_dciinformationreport_record_tpcdci_t = %d", sizeof(struct lte_dciinformationreport_v26_record_tpcdci_t));

        for (i = 0; i < log->number_of_records; i++) {
            rec = &log->records_v26[i];
            VERBOSE("sfn #%d = %d", i, rec->sfn);
            VERBOSE("sub_fn #%d = %d", i, rec->sub_fn);
            VERBOSE("num_ul_grant #%d = %d", i, rec->num_ul_grant);
            VERBOSE("num_dl_grants #%d = %d", i, rec->num_dl_grants);

            for (j = 0; j < rec->num_ul_grant; j++) {
                ul_grant_info = &rec->ul_grant_info[j];
                VERBOSE("grant_present #%d/%d = %d", i, j, ul_grant_info->grant_present);
                VERBOSE("start_of_resource_block #%d/%d = %d", i, j, ul_grant_info->start_of_resource_block);
                VERBOSE("number_of_resource_blocks #%d/%d = %d", i, j, ul_grant_info->number_of_resource_blocks);
                VERBOSE("number_of_resource_blocks_2 #%d/%d = %d", i, j, ul_grant_info->number_of_resource_blocks_2);
                total_rbs += ul_grant_info->number_of_resource_blocks;
            }
        }
    }

    rdb_enter_csection();
    {
        servcell_info.TotalPRBsSent += total_rbs;
        VERBOSE("TotalPRBsSent = %llu", servcell_info.TotalPRBsSent);

        rdb_leave_csection();
    }
}
#endif

#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_mac_ul_tb)
{
    struct lte_mac_ul_tb_t {
        uint32 subpacket_id : 8;
        uint32 subpacket_version : 8;
        uint32 subpacket_size : 16;
        char num_samples;

        char unknown[0];
    } PACK();

    declare_app_func_define_vars_for_log();

    /* declare_app_func_require_ver(); */
    declare_app_func_define_var_log(struct lte_mac_ul_tb_t);

    rdb_enter_csection();
    {
        servcell_info.TotalActiveTTIsSent += log->num_samples;
        VERBOSE("TotalActiveTTIsSent = %llu", servcell_info.TotalActiveTTIsSent);

        rdb_leave_csection();
    }
}
#endif

declare_app_func(lte_pdcpul_statistics)
{
    struct lte_pdcpul_statistics_rb_t {
        uint8 rb_cfg_idx;
        uint8 mode;
        uint16 pdcp_hdr_len;

        char unknown[249]; /* ref. 80-VP457-1 YYE 9.7 - extra data */
    } PACK();

    struct lte_pdcpul_statistics_subpacket_t {
        uint8 subpacket_id;

        uint8 version;
        uint16 subpacket_size;

        uint8 num_rbs;
        uint32 num_errors;

        struct lte_pdcpul_statistics_rb_t rb[0];
    } PACK();

    struct lte_pdcpul_statistics_t {
        uint8 version;
        uint8 number_of_subpackets;
        uint16 reserved;

        struct lte_pdcpul_statistics_subpacket_t subpacket[0];
    } PACK();

    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(struct lte_pdcpul_statistics_t);

    VERBOSE("log len = %d", len);
    VERBOSE("log struct lte_pdcpul_statistics_t = %d", sizeof(struct lte_pdcpul_statistics_t));
    VERBOSE("log struct subpacaket = %d", sizeof(*log->subpacket));

    VERBOSE("version = %d", log->version);
    VERBOSE("number_of_subpackets = %d", log->number_of_subpackets);
    VERBOSE("subpacket_id = %d", log->subpacket[0].subpacket_id);
    VERBOSE("subpacket version = %d", log->subpacket[0].version);
    VERBOSE("subpacket_size = %d", log->subpacket[0].subpacket_size);
    VERBOSE("num_rbs = %d", log->subpacket[0].num_rbs);

    VERBOSE("len log = %d", sizeof(*log));
    VERBOSE("len subpacket = %d", sizeof(*log->subpacket));
    VERBOSE("len rb = %d", sizeof(*log->subpacket[0].rb));

#ifdef DEBUG_VERBOSE
    int num_missing_sdu_to_ul;
    int num_missing_sdu_to_ul_rst;
#endif
    unsigned int num_data_pdu_tx_bytes_rst;
    unsigned long long pdcp_tx_sent = 0;
    char *p;

    struct acc_t *PDCPBytesSent = &rrc_info.PDCPBytesSent;

    int i, j;
    for (i = 0; i < log->number_of_subpackets; i++) {
        for (j = 0; j < log->subpacket[i].num_rbs; j++) {
            VERBOSE("rb_cfg_idx #%d = %d", j, log->subpacket[i].rb[j].rb_cfg_idx);
            VERBOSE("mode #%d = %d", j, log->subpacket[i].rb[j].mode);
            VERBOSE("pdcp_hdr_len #%d = %d", j, log->subpacket[i].rb[j].pdcp_hdr_len);

            p = (char *)&log->subpacket[i].rb[j];

#ifdef CONFIG_DEBUG_PDCP
            DEBUG("pdcp_tx_sent rb index = %d", log->subpacket[i].rb[j].rb_cfg_idx);
#endif

#ifdef DEBUG_VERBOSE
            num_missing_sdu_to_ul = *(unsigned int *)(p + 408 / 8);
            num_missing_sdu_to_ul_rst = *(unsigned int *)(p + 1048 / 8);
#endif

            VERBOSE("num_missing_sdu_to_ul=%d", num_missing_sdu_to_ul);
            VERBOSE("num_missing_sdu_to_ul_rst=%d", num_missing_sdu_to_ul_rst);

            VERBOSE("num_data_pdu_tx_bytes_rst offset #%d/%d=%d", i, j, p - (char *)log);
            VERBOSE("num_data_pdu_tx_bytes_rst offset #%d/%d=%d", i, j, 1144 / 8);

            num_data_pdu_tx_bytes_rst = *(unsigned int *)(p + 1144 / 8);
            VERBOSE("num_data_pdu_tx_bytes_rst #%d/%d=%d", i, j, num_data_pdu_tx_bytes_rst);

            pdcp_tx_sent += num_data_pdu_tx_bytes_rst;
        }
    }

#ifdef CONFIG_TRAFFIC_OVERLAP_TEST
    pdcp_tx_sent = OVERLAP_TEST_SITUATE(pdcp_tx_sent, 32);
    DEBUG("[pdcp_bytes_sent] bit=%d, val=0x%016llx, rval=0x%08x, sent=%llu", OVERLAP_TEST_BIT_COUNT, pdcp_tx_sent,
          (unsigned int)OVERLAP_TEST_RVALUE(pdcp_tx_sent, 32), pdcp_tx_sent);
#endif

#ifdef CONFIG_DEBUG_PDCP
    DEBUG("[pdcp] pdcp_tx_sent = %llu", pdcp_tx_sent);
#endif

    /*
     * # accept statistics only when RRC is running
     *
     *  This RRC running verification is not truly required since the statistics get reset at the beginning of RRC and
     * any orphan statistics will be removed and not written to RDB. Although, to have the identical internal statistic
     * values all the time, the condition is kept.
     *
     */
    if (rrc_info.running) {
        acc_feed(PDCPBytesSent, pdcp_tx_sent, ts);
    }

#ifdef CONFIG_DEBUG_THROUGHPUT
    DEBUG("[pdcp_bytes_sent] val = %llu, diff = %llu", pdcp_tx_sent, PDCPBytesSent->diff);
#endif
}

#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ml1_serv_cell_meas)
{
    struct subpkt_hdr_t {
        uint8 version;
        uint16 subpacket_size;
    } PACK();

    struct lte_ml1_serv_cell_meas_subpacket_t {
        struct subpkt_hdr_t hdr;
        lte_ml1_sm_log_serv_cell_meas_response_subpkt_s body;
    } PACK();

    struct lte_ml1_serv_cell_meas_t {
        uint32 version : 8;
        uint8 number_of_subpackets;
        uint16 reserved;
        uint8 subpacket_id;

        struct lte_ml1_serv_cell_meas_subpacket_t subpackets[0];

    } PACK();

    int snr;

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(1, 22, 33);
    declare_app_func_define_var_log(struct lte_ml1_serv_cell_meas_t);

    VERBOSE("ts = %llu", *ts);
    VERBOSE("subpacket_id = %d", log->subpacket_id);

    VERBOSE("hdr version = %d", log->subpackets[0].hdr.version);
    VERBOSE("subpacket_size = %d", log->subpackets[0].hdr.subpacket_size);

    VERBOSE("earfcn = %d", log->subpackets[0].body.earfcn);
    VERBOSE("num_cells = %d", log->subpackets[0].body.num_cells);
    VERBOSE("horxd_mode = %d", log->subpackets[0].body.horxd_mode);

    /* Only supporting v22 and v33 of structure. */
    /* ------------ v33 -----------------------------------------------------------------*/
    /* rsrp, rsrq, rssi, frl_snr value is invalid if 0 */
#define RSRP(val) (val * 0.0625 - 180)
#define RSRP2(val) (val * 0.0625 - 140)
#define RSRQ(val) (val * 0.0625 - 30)
#define RSSI(val) (val * 0.0625 - 110)
#define SNR(val) (val * 0.100000001490116 - 20)
    if (declare_app_func_ver_match(33)) {
        lte_ml1_sm_log_meas_result_per_cell_v33_35_s *meas_result;
        meas_result = &log->subpackets[0].body.meas_result_v33[0];
#if (0)
        VERBOSE("phy_cell_id = %d", meas_result->phy_cell_id);
        VERBOSE("serv_cell_index = %d", meas_result->serv_cell_index);
        VERBOSE("is_serv_cell = %d", meas_result->is_serv_cell);
        VERBOSE("current_sfn = %d", meas_result->current_sfn);
        VERBOSE("current_subfn = %d", meas_result->current_subfn);
        VERBOSE("is_subfm_restricted = %d", meas_result->is_subfm_restricted);
        VERBOSE("cell_timing_0 = %d", meas_result->cell_timing_0);
        VERBOSE("cell_timing_1 = %d", meas_result->cell_timing_1);
        VERBOSE("cell_timing_sfn_0 = %d", meas_result->cell_timing_sfn_0);
        VERBOSE("cell_timing_sfn_1 = %d", meas_result->cell_timing_sfn_1);
        VERBOSE("inst_rsrp_rx_0 = %d, %.2f", meas_result->inst_rsrp_rx_0, RSRP(meas_result->inst_rsrp_rx_0));
        VERBOSE("inst_rsrp_rx_1 = %d, %.2f", meas_result->inst_rsrp_rx_1, RSRP(meas_result->inst_rsrp_rx_1));
        VERBOSE("inst_rsrp_rx_2 = %d, %.2f", meas_result->inst_rsrp_rx_2, RSRP(meas_result->inst_rsrp_rx_2));
        VERBOSE("inst_rsrp_rx_3 = %d, %.2f", meas_result->inst_rsrp_rx_3, RSRP(meas_result->inst_rsrp_rx_3));
        VERBOSE("inst_rsrp = %d, %f", meas_result->inst_rsrp, RSRP2(meas_result->inst_rsrp));
        VERBOSE("filtered_rsrp = %d, %.2f", meas_result->filtered_rsrp, RSRP(meas_result->filtered_rsrp));
        VERBOSE("inst_rsrq_rx_0 = %d, %.2f", meas_result->inst_rsrq_rx_0, RSRQ(meas_result->inst_rsrq_rx_0));
        VERBOSE("inst_rsrq_rx_1 = %d, %.2f", meas_result->inst_rsrq_rx_1, RSRQ(meas_result->inst_rsrq_rx_1));
        VERBOSE("inst_rsrq_rx_2 = %d, %.2f", meas_result->inst_rsrq_rx_2, RSRQ(meas_result->inst_rsrq_rx_2));
        VERBOSE("inst_rsrq_rx_3 = %d, %.2f", meas_result->inst_rsrq_rx_3, RSRQ(meas_result->inst_rsrq_rx_3));
        VERBOSE("inst_rsrq = %d, %.2f", meas_result->inst_rsrq, RSRQ(meas_result->inst_rsrq));
        VERBOSE("filtered_rsrq = %d, %.2f", meas_result->filtered_rsrq, RSRQ(meas_result->filtered_rsrq));
        VERBOSE("inst_rssi_rx_0 = %d, %.2f", meas_result->inst_rssi_rx_0, RSSI(meas_result->inst_rssi_rx_0));
        VERBOSE("inst_rssi_rx_1 = %d, %.2f", meas_result->inst_rssi_rx_1, RSSI(meas_result->inst_rssi_rx_1));
        VERBOSE("inst_rssi_rx_2 = %d, %.2f", meas_result->inst_rssi_rx_2, RSSI(meas_result->inst_rssi_rx_2));
        VERBOSE("inst_rssi_rx_3 = %d, %.2f", meas_result->inst_rssi_rx_3, RSSI(meas_result->inst_rssi_rx_3));
        VERBOSE("inst_rssi = %d, %.2f", meas_result->inst_rssi, RSSI(meas_result->inst_rssi));
        VERBOSE("res_freq_error = %d", meas_result->res_freq_error);
        VERBOSE("ftl_snr_rx_0 = %d, %.2f", meas_result->ftl_snr_rx_0, SNR(meas_result->ftl_snr_rx_0));
        VERBOSE("ftl_snr_rx_1 = %d, %.2f", meas_result->ftl_snr_rx_1, SNR(meas_result->ftl_snr_rx_1));
        VERBOSE("ftl_snr_rx_2 = %d, %.2f", meas_result->ftl_snr_rx_2, SNR(meas_result->ftl_snr_rx_2));
        VERBOSE("ftl_snr_rx_3 = %d, %.2f", meas_result->ftl_snr_rx_3, SNR(meas_result->ftl_snr_rx_3));
        VERBOSE("projected_sir = %.2f", meas_result->projected_sir / 16);
        VERBOSE("post_ic_rsrq = %d, %.2f", meas_result->post_ic_rsrq, RSRQ(meas_result->post_ic_rsrq));
        VERBOSE("cinr_rx_0 = %d", meas_result->cinr_rx_0);
        VERBOSE("cinr_rx_1 = %d", meas_result->cinr_rx_1);
        VERBOSE("cinr_rx_2 = %d", meas_result->cinr_rx_2);
        VERBOSE("cinr_rx_3 = %d", meas_result->cinr_rx_3);
#endif

#if !defined SAVE_AVERAGED_DATA
        if (meas_result->inst_rsrp_rx_0) {
            servcell_info.Rsrp[0] = RSRP(meas_result->inst_rsrp_rx_0);
            servcell_info.RsrpValid[0] = 1;
        } else {
            servcell_info.RsrpValid[0] = 0;
        }
        if (meas_result->inst_rsrp_rx_1) {
            servcell_info.Rsrp[1] = RSRP(meas_result->inst_rsrp_rx_1);
            servcell_info.RsrpValid[1] = 1;
        } else {
            servcell_info.RsrpValid[1] = 0;
        }
#else
        rdb_enter_csection();
        {
            if (meas_result->is_serv_cell) {
                snr = meas_result->ftl_snr_rx_0;
                /* feed snr into rrc */
                avg_feed(&rrc_info.SCellRSSNR, snr);
                /* feed snr into servcell */
                avg_feed(&servcell_info.SCellRSSNR, snr);
            }
            rdb_leave_csection();
        }
#endif
        /* ------------ v22 -----------------------------------------------------------------*/
    } else {
        lte_ml1_sm_log_meas_result_per_cell_v22_s *meas_result;
        meas_result = &log->subpackets[0].body.meas_result_v22[0];
        VERBOSE("packet phy_cell_id = %d", meas_result->phy_cell_id);
        VERBOSE("ftl_snr_rx_0 = %.2f", __dbx10(meas_result->ftl_snr_rx_0, -20));
        VERBOSE("ftl_snr_rx_1 = %.2f", __dbx10(meas_result->ftl_snr_rx_1, -20));

#if !defined SAVE_AVERAGED_DATA
        if (meas_result->inst_rsrp_rx_0) {
            servcell_info.Rsrp[0] = RSRP(meas_result->inst_rsrp_rx_0);
            servcell_info.RsrpValid[0] = 1;
        } else {
            servcell_info.RsrpValid[0] = 0;
        }
        if (meas_result->inst_rsrp_rx_1) {
            servcell_info.Rsrp[1] = RSRP(meas_result->inst_rsrp_rx_1);
            servcell_info.RsrpValid[1] = 1;
        } else {
            servcell_info.RsrpValid[1] = 0;
        }
#else
        rdb_enter_csection();
        {
            if (meas_result->is_serv_cell) {
                snr = meas_result->ftl_snr_rx_0;
                /* feed snr into rrc */
                avg_feed(&rrc_info.SCellRSSNR, snr);
                /* feed snr into servcell */
                avg_feed(&servcell_info.SCellRSSNR, snr);
            }
            rdb_leave_csection();
        }
#endif
    }
}

#ifdef NR5G
#include "bands.h"
#endif

declare_app_func(lte_ml1_srch_intra_freq_meas)
{
    struct measured_neighbor_cell_t {
        uint16 physical_cell_id;
        uint16 filtered_rsrp;
        uint16 reserved1;
        uint16 filtered_rsrq;
        uint16 reserved2;
        uint16 reserved3;
    } PACK();

    struct detected_cell_t {
        uint16 physical_cell_id;
        uint16 reserved1;
        uint32 sss_corr_value;
        uint64 reference_time;
    } PACK();

    struct lte_ml1_srch_intra_freq_meas_t {
        uint32 version : 8;
        uint32 reserved1 : 24;
        uint32 serving_cell_index : 3;
        uint32 reserved2 : 29;
        uint32 earfcn;
        uint16 serving_physical_cell_id;
        uint16 subframe_number;
        uint16 serving_filtered_rsrp;
        uint16 reserved3;
        uint16 serving_filtered_rsrq;
        uint16 reserved4;
        uint16 number_of_measured_neighbor_cells : 8;
        uint16 number_of_detected_cells : 8;
        uint16 reserved5;

        struct measured_neighbor_cell_t measured_neighbor_cell[0];
        struct detected_cell_t detected_cells[0];
    } PACK();

    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(4);
    declare_app_func_define_var_log(struct lte_ml1_srch_intra_freq_meas_t);

    VERBOSE("struct lte_ml1_srch_intra_freq_meas_t=%d", sizeof(struct lte_ml1_srch_intra_freq_meas_t));
    VERBOSE("struct measured_neighbor_cell_t=%d", sizeof(struct measured_neighbor_cell_t));
    VERBOSE("struct detected_cell_t=%d", sizeof(struct detected_cell_t));

    VERBOSE("ts = %llu", *ts);
    VERBOSE("log_version = %d", log->version);
    VERBOSE("serving_cell_index = %d", log->serving_cell_index);
    VERBOSE("earfcn = %d", log->earfcn);
    VERBOSE("serving_physical_cell_id = %d", log->serving_physical_cell_id);
    VERBOSE("serving_filtered_rsrp = %d, %.2f", log->serving_filtered_rsrp, __dbx16(log->serving_filtered_rsrp, -180));
    VERBOSE("serving_filtered_rsrq = %d, %.2f", log->serving_filtered_rsrq, __dbx16(log->serving_filtered_rsrq, -30));
    VERBOSE("number_of_measured_neighbor_cells = %d", log->number_of_measured_neighbor_cells);
    VERBOSE("number_of_detected_cells = %d", log->number_of_detected_cells);

    rdb_enter_csection();
    {
#ifdef NR5G
/* Capturing the unfiltered rrc information  */
#define RRC_RDB(fun, index, name, value) \
    do { \
        char nbuf[512]; \
        sprintf(nbuf, "lte.rrc.%d.%s", (index), (name)); \
        fun(nbuf, (value)); \
    } while (0)

        RRC_RDB(_rdb_set_tenths_decimal, log->serving_cell_index, "scell_rsrq", __dbx16(log->serving_filtered_rsrq, -30));
        RRC_RDB(_rdb_set_tenths_decimal, log->serving_cell_index, "scell_rsrp", __dbx16(log->serving_filtered_rsrp, -180));
        const struct lte_band *lteb = bands_lookup_earfcn(log->earfcn);
        if (lteb) {
            RRC_RDB(_rdb_set_tenths_decimal, log->serving_cell_index, "cell_freq", lte_frequency(lteb, log->earfcn));
            RRC_RDB(_rdb_set_tenths_decimal, log->serving_cell_index, "cell_band_ul", lteb->full);
            RRC_RDB(_rdb_set_tenths_decimal, log->serving_cell_index, "cell_band_dl", lteb->fdll);
            RRC_RDB(_rdb_set_str, log->serving_cell_index, "scell_bandname", lteb->name);
            RRC_RDB(_rdb_set_uint, log->serving_cell_index, "scell_band", lteb->band);
        } else {
            RRC_RDB(_rdb_set_str, log->serving_cell_index, "cell_freq", "");
            RRC_RDB(_rdb_set_str, log->serving_cell_index, "cell_band_ul", "");
            RRC_RDB(_rdb_set_str, log->serving_cell_index, "cell_band_dl", "");
            RRC_RDB(_rdb_set_str, log->serving_cell_index, "scell_bandname", "");
            RRC_RDB(_rdb_set_str, log->serving_cell_index, "scell_band", "");
        }
        RRC_RDB(_rdb_set_uint, log->serving_cell_index, "scell_earfcn", log->earfcn);
        RRC_RDB(_rdb_set_uint, log->serving_cell_index, "scell_phys_id", log->serving_physical_cell_id);
#endif

        avg_feed(&rrc_info.SCellRSRP, log->serving_filtered_rsrp);
        avg_feed(&rrc_info.SCellRSRQ, log->serving_filtered_rsrq);

        log_verbose_dump((char *)ptr, len);

        rdb_leave_csection();
    }
}
#endif

#endif /* INCLUDE_COMMON_MSG */

#ifdef INCLUDE_LTE_CQI_MSG
declare_app_func(lte_ll1_csf_pucch_periodic_report)
{
    declare_app_func_define_vars_for_log();
    declare_app_func_require_ver(101, 162);

    int log_rank_index;
    int log_wideband_pmi;
    int log_cqi_cw0;
    int log_cqi_cw1;
    int log_pucch_reporting_mode;
    int log_carrier_index;
    int log_pucch_report_type;

    declare_app_func_require_ver(101, 162);
    switch (log_ver) {
        case 101: {
            declare_app_func_define_var_log(lte_LL1_log_csf_pucch_report_ind_struct_v101);
            log_pucch_report_type = log->pucch_report_type;
            log_rank_index = log->rank_index;
            log_wideband_pmi = log->wideband_pmi;
            log_cqi_cw0 = log->cqi_cw0;
            log_cqi_cw1 = log->cqi_cw1;
            log_pucch_reporting_mode = log->pucch_reporting_mode;
            log_carrier_index = log->carrier_index;
            break;
        }

        default: {
            declare_app_func_define_var_log(lte_LL1_log_csf_pucch_report_ind_struct_v162);
            log_pucch_report_type = log->pucch_report_type;
            log_rank_index = log->rank_index;
            log_wideband_pmi = log->wideband_pmi;
            log_cqi_cw0 = log->cqi_cw0;
            log_cqi_cw1 = log->cqi_cw1;
            log_pucch_reporting_mode = log->pucch_reporting_mode;
            log_carrier_index = log->carrier_index;
            break;
        }
    }

    VERBOSE("log_pucch_report_type = %d", log_pucch_report_type);
    VERBOSE("log_rank_index = %d", log_rank_index);
    VERBOSE("log_wideband_pmi = %d", log_wideband_pmi);
    VERBOSE("log_cqi_cw0 = %d", log_cqi_cw0);
    VERBOSE("log_cqi_cw1 = %d", log_cqi_cw1);
    VERBOSE("log_pucch_reporting_mode = %d", log_pucch_reporting_mode);
    VERBOSE("log_carrier_index = %d", log_carrier_index);

    int cqi_cw_count = 0;
    int cqi_cw_total = 0;

#ifdef USE_QDIAG_FOR_CQI
    int cqi;
#endif

    rdb_enter_csection();
    {
        /* update rank index distribution */
        switch (log_pucch_report_type) {
            case 3:
                distr_feed(&servcell_info.RIDistribution, log_rank_index);
                VERBOSE("rank index = %d", log_rank_index);
                break;
        }

        /* update pmi distribution */
        switch (log_pucch_report_type) {
            case 0:
            case 2:
            case 8:
            case 9:
                distr_feed(&servcell_info.PMIDistribution, log_wideband_pmi);
                VERBOSE("wideband pmi = %d", log_wideband_pmi);
                break;
        }

        /* collect wideband cqi only from CCH */
        switch (log_pucch_report_type) {
            case 2:
                /* wb cqi codeword 1 valid only if */
                /* report type is TYPE_2_WBCQI_PMI and */
                /* reporting mode is MODE_1_1 and cqi codeword 1 > 0 */
                if ((log_pucch_reporting_mode == 1) && (log_cqi_cw1 > 0)) {
                    /* add cqi codeword 1 into this sample */
                    cqi_cw_count++;
                    cqi_cw_total += log_cqi_cw1;
                }
            /* this intentionally falls through to case 4 because wb codeword */
            /* 0 is valid in both report type 2 (TYPE_2_WBCQI_PMI) and */
            /* report type 4 (TYPE_4_WBCQI) */
            case 4:
#ifdef USE_QDIAG_FOR_CQI
                /* TODO: this CQI is not supposed to be wideband */
                cqi = log_cqi_cw0;
                avg_feed(&rrc_info.CQI, cqi);
                VERBOSE("CQI = %d, cnt=%llu,avg=%.2Lf", cqi, rrc_info.CQI.c, rrc_info.CQI.avg);
#endif
                /* cqi codeword 0 valid only if it is > 0 */
                if (log_cqi_cw0 > 0) {
                    /* add cqi codeword 0 into this sample */
                    cqi_cw_count++;
                    cqi_cw_total += log_cqi_cw0;
                }

                /* if we have at least 1 valid cqi codeword */
                if (cqi_cw_count > 0) {
                    /* update average wideband cqi for PCC */
                    if ((log_carrier_index == 0)) {
                        avg_feed(&servcell_info.AvgWidebandCQI, cqi_cw_total / cqi_cw_count);
                    }

                    /* Only update average wideband cqi for valid cells */
                    if ((log_carrier_index >= 0) && (log_carrier_index < MAX_SERVER_CELLS_PER_MEASURING_PERIOD)) {
                        avg_feed(&rrc_info.AvgWidebandCQIByCell[log_carrier_index], cqi_cw_total / cqi_cw_count);
                    }
                }

#ifdef INCLUDE_LTE_CQI_MSG
                // fill cqi data distribution
                servcell_info.CqiStatsCount[log_cqi_cw0]++;

                // fill pmi data distribution
                if (log_pucch_report_type != 4) {
                    servcell_info.PmiStatsCount[log_wideband_pmi]++;
                }
#endif // INCLUDE_LTE_CQI_MSG
                break;

            case 1:
            case 0:
            case 8:
            case 9:
            default:
                break;
        }

        rdb_leave_csection();
    }
}
#endif /* INCLUDE_LTE_CQI_MSG */

#ifdef INCLUDE_COMMON_MSG
declare_app_func(lte_ll1_dl_common_cfg)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(2);
    declare_app_func_define_var_log(lte_ll1_dl_common_cfg_ind_struct);

    rdb_enter_csection();
    {
        int8_t ref_pwr = (int8_t)log->reference_signal_power;
        // ref_sig_power is used on the calculation for path loss
        _rdb_set_int("wwan.0.ref_sig_power", ref_pwr);
        _rdb_set_uint("wwan.0.p_b", log->p_b);

        rdb_leave_csection();
    }
}

#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
declare_app_func(lte_ll1_pdsch_demapper_cfg)
{
    declare_app_func_define_vars_for_log();
    declare_app_func_define_var_log(lte_ll1_pdsch_demapper_cfg_ind_struct);
    declare_app_func_require_ver(121);

#ifdef SAVE_AVERAGED_DATA
#ifdef USE_QDIAG_FOR_TRANSMISSION_SCHEME
    rdb_enter_csection();
    {
        _rdb_set_uint("wwan.0.transmission_mode", log->transmission_scheme);

        rdb_leave_csection();
    }
#endif
#endif
}
#endif

declare_app_func(lte_ll1_csf_pusch_aperiodic_report)
{
    declare_app_func_define_vars_for_log();

    declare_app_func_require_ver(101);
    declare_app_func_define_var_log(lte_LL1_log_csf_pusch_report_ind_struct);

    DEBUG("pusch_reporting_mode = %d", log->pusch_reporting_mode);
}
#endif /* INCLUDE_COMMON_MSG */

/*
        ### monotonic time functions ###
*/

const char *get_str_from_var(char *str, int str_len, const void *val, int len)
{

    *str = 0;
    __strncpy(str, str_len, val, len);

    return str;
}

/*
        ### misc info info functions ###
*/

/*
 Initiate misc statistic information.

 Parameters:
  None.

 Return:
  None.
*/
void misc_info_init()
{
    memset(&misc_info, 0, sizeof(misc_info));

    /* initiate average latency */
    avg_init(&misc_info.AttachLatency, "attach_avg_latency", 0);
    avg_reset(&misc_info.AttachLatency);
}

/*
 Write misc statistic information to RDB.

 Parameters:
  None.

 Return:
  None.
*/
void misc_info_update()
{
    /* buffer for RDB content */
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    /* write attach attempts */
    _rdb_prefix_call(_rdb_set_int, "pdpcontext.attach_attempts", misc_info.AttachAttempts);
    /* write attach failures */
    _rdb_prefix_call(_rdb_set_int, "pdpcontext.attach_failures", misc_info.AttachFailures);

    if (misc_info.AttachLatency.stat_valid)
        _rdb_prefix_call(_rdb_set_int, "pdpcontext.attach_avg_latency", __round(misc_info.AttachLatency.avg));
    else
        _rdb_prefix_call(_rdb_set_reset, "pdpcontext.attach_avg_latency");
}

/*
 Finalize misc statistic information.

 Parameters:
  None.

 Return:
  None.
*/
void misc_info_fini() {}

/*
        ### servcell_info info functions ###
*/

/*
 Create serving cell RDB Variables.

 Parameters:
  None.

 Return:
  None.
*/
void servcell_info_init_rdb()
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    /* failure cause names */
    const char *failure_cause_names[] = {
        [0x00] = "cfg_failure",        [0x01] = "ho_failure",       [0x02] = "rlf",           [0x03] = "rach_problem", [0x04] = "max_retrx",
        [0x05] = "ip_check_failure",   [0x06] = "sib_read_failure", [0x07] = "other_failure", [0x08] = "max",          [0x09] = "mib_change",
        [0x0a] = "rlf_rf_unavailable",
    };

    int i;
    char rname[RDB_MAX_NAME_LEN];

    /* write failure case name count into rdb */
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.number_of_rlf_failure_causes", MAX_FAILURE_CAUSES);

    /* write failure cause names into rdb */
    for (i = 0; i < MAX_FAILURE_CAUSES; i++) {
        snprintf(rname, sizeof(rname), "servcell_info.rlf_failures.%d.", i);
        _rdb_suffix_call(_rdb_set_str, rname, "failure_cause", failure_cause_names[i]);
        _rdb_suffix_call(_rdb_set_reset, rname, "count");
    }
}

/*
 Reset servcell info.
  Clear all of statistic-related members in servcell info.

 Parameters:
  servcell          servcell_info_t struct point to reset
  init_size         size of the area to reset
  tm                start time
  tm_monotonic      start time in monotonic

 Return:
  None.
*/
void servcell_info_reset(struct servcell_info_t *servcell, int init_size, time_t tm, time_t tm_monotonic)
{
    char *p;
    int off;

    /* reset members */
    off = __offset(struct servcell_info_t, __reset_ptr);
    p = (char *)servcell + off;
    memset(p, 0, init_size - off);
    /* reset signal receptions */
    avg_reset(&servcell->SCellRSSNR);
    avg_reset(&servcell->SCellRSSINR);
    avg_reset(&servcell->SCellRSRP);
    avg_reset(&servcell->SCellRSRQ);
    avg_reset(&servcell->PathLoss);

    avg_reset(&servcell->AvgWidebandCQI);
    distr_reset(&servcell->PUSCHTrasmitPower);
    distr_reset(&servcell->PMIDistribution);
    distr_reset(&servcell->RIDistribution);
    distr_reset(&servcell->ReceivedModulationDistribution);
    distr_reset(&servcell->SendModulationDistribution);
    avg_reset(&servcell->RRCEstabLatency);
    avg_reset(&servcell->RRCReEstabLatency);

    /* update start time */
    servcell->StartTime = tm;
    servcell->StartTimeMonotonic = tm_monotonic;
}

#if !defined SAVE_AVERAGED_DATA
/*
 update RDBs for V3 30 seconds measure & report requirement.

 Parameters:
  None.

 Return:
  None.
*/
void update_measure_params()
{
    int i;
    /* buffer for RDB content */
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);
    char val[RDB_MAX_VAL_LEN];
    char *valp;
    char s[RDB_MAX_NAME_LEN];
    long long rsrp;

    /* Rx MCS (Modulation and Coding Scheme) index
     * Measure & report instant value
     * Convert bit mask format to string format
     * ex) "0 1 10 31"
     */
    if (servcell_info.RxMcsIndexValid) {
        (void)memset(val, 0, RDB_MAX_VAL_LEN);
        valp = val;
        for (i = 0; i < MAX_MCS_INDEX; i++) {
            if (servcell_info.RxMcsIndex & (0x1 << i)) {
                valp += sprintf(valp, "%d ", i);
            }
        }
        _rdb_prefix_call(_rdb_set_str, "servcell_info.rx_mcs_index", val);
    } else {
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.rx_mcs_index");
    }
    servcell_info.RxMcsIndex = 0;

    /* Tx MCS (Modulation and Coding Scheme) index
     * Measure & report instant value
     * Convert bit mask format to string format
     * ex) "0 1 10 31"
     */
    if (servcell_info.TxMcsIndexValid) {
        (void)memset(val, 0, RDB_MAX_VAL_LEN);
        valp = val;
        for (i = 0; i < MAX_MCS_INDEX; i++) {
            if (servcell_info.TxMcsIndex & (0x1 << i)) {
                valp += sprintf(valp, "%d ", i);
            }
        }
        _rdb_prefix_call(_rdb_set_str, "servcell_info.tx_mcs_index", val);
    } else {
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.tx_mcs_index");
    }
    servcell_info.TxMcsIndex = 0;

    // update MCS index and CRC fail distribution
    for (i = 0; i < MCS_INDEX_NUM_ELEMENTS; i++) {
        sprintf(s, "servcell_info.mcs_index_crc_fail_count.%02d", i);
        sprintf(val, "%u,%u", servcell_info.McsIndexCount[i], servcell_info.CrcFailCount[i]);
        _rdb_prefix_call(_rdb_set_str, s, val);
    }

    // update path loss
    _rdb_prefix_call(_rdb_set_int, "servcell_info.path_loss", __round(servcell_info.PathLoss.avg));

#ifdef INCLUDE_LTE_CQI_MSG
    // update CQI distribution
    for (i = 0; i < CQI_NUM_ELEMENTS; i++) {
        sprintf(s, "servcell_info.cqi_cw0_count.%02d", i);
        sprintf(val, "%u", servcell_info.CqiStatsCount[i]);
        _rdb_prefix_call(_rdb_set_str, s, val);
    }

    // update PMI distribution
    for (i = 0; i < PMI_NUM_ELEMENTS; i++) {
        sprintf(s, "servcell_info.wideband_pmi_count.%02d", i);
        sprintf(val, "%u", servcell_info.PmiStatsCount[i]);
        _rdb_prefix_call(_rdb_set_str, s, val);
    }

#endif // INCLUDE_LTE_CQI_MSG
    /* ---------------------------------------------------------------------
     * Below HARQ related variables are reported as accumulated value since
     * system up
     */

    /* BLERi received */
    _rdb_prefix_call(_rdb_set_uint, "harq.dl_initial_bler",
                     servcell_info.MACiBLERReceivedAckNackCount
                         ? (servcell_info.MACiBLERReceivedNackCount * 100 / servcell_info.MACiBLERReceivedAckNackCount)
                         : 0);

    /* BLERr received */
    _rdb_prefix_call(_rdb_set_uint, "harq.dl_residual_bler",
                     servcell_info.MACrBLERReceivedAckNackCount
                         ? (servcell_info.MACrBLERReceivedNackCount * 100 / servcell_info.MACrBLERReceivedAckNackCount)
                         : 0);

    /* BLERi sent */
    _rdb_prefix_call(_rdb_set_uint, "harq.ul_initial_bler",
                     servcell_info.MACiBLERSentAckNackCount ? (servcell_info.MACiBLERSentNackCount * 100 / servcell_info.MACiBLERSentAckNackCount)
                                                            : 0);

    /* BLERr sent */
    _rdb_prefix_call(_rdb_set_uint, "harq.ul_residual_bler",
                     servcell_info.MACrBLERSentAckNackCount ? (servcell_info.MACrBLERSentNackCount * 100 / servcell_info.MACrBLERSentAckNackCount)
                                                            : 0);

    /* Downlink Harq Ack count */
    _rdb_prefix_call(_rdb_set_uint, "harq.dl_ack",
                     (servcell_info.MACiBLERReceivedAckNackCount + servcell_info.MACrBLERReceivedAckNackCount -
                      servcell_info.MACiBLERReceivedNackCount - servcell_info.MACrBLERReceivedNackCount));

    /* Downlink Harq Nack count */
    _rdb_prefix_call(_rdb_set_uint, "harq.dl_nack", servcell_info.MACiBLERReceivedNackCount + servcell_info.MACrBLERReceivedNackCount);

    /* Uplink Harq Ack count */
    _rdb_prefix_call(_rdb_set_uint, "harq.ul_ack",
                     (servcell_info.MACiBLERSentAckNackCount + servcell_info.MACrBLERSentAckNackCount - servcell_info.MACiBLERSentNackCount -
                      servcell_info.MACrBLERSentNackCount));

    /* Up Harq Nack count */
    _rdb_prefix_call(_rdb_set_uint, "harq.ul_nack", servcell_info.MACiBLERSentNackCount + servcell_info.MACrBLERSentNackCount);

    /* Uplink maximum Re-Tx count */
    _rdb_prefix_call(_rdb_set_uint, "harq.retx_max", servcell_info.max_ul_harq_transmissions ? servcell_info.max_ul_harq_transmissions : 0);
    /* Uplink Re-Tx status */
    for (i = 0; i < MAX_HARQ_RETX_COUNT; i++) {
        if (i == 0)
            sprintf(s, "harq.1st_tx_ok");
        else
            sprintf(s, "harq.retx%d", i);
        _rdb_prefix_call(_rdb_set_uint, s, servcell_info.HarqUlRetxOk[i]);
    }
    /* --------------------------------------------------------------------- */

#ifdef USE_QDIAG_FOR_PUSCH_TX_POWER
    /* Instant Tx power */
    if (int_stats.pusch_tx_power_valid)
        _rdb_prefix_call(_rdb_set_int, "signal.tx_power_PUSCH", int_stats.pusch_tx_power);
    else
        _rdb_prefix_call(_rdb_set_reset, "signal.tx_power_PUSCH");
#endif // USE_QDIAG_FOR_PUSCH_TX_POWER

        // get rsrp0 and rsrp1 from Qdiag for Titan. Pentecost removes the Qdiag message and gets value from QMI.
#if (defined V_RF_MEASURE_REPORT_dundee) || (defined V_RF_MEASURE_REPORT_myna)
    /* RSRP 0 */
    if (servcell_info.RsrpValid[0])
        _rdb_prefix_call(_rdb_set_tenths_decimal, "servcell_info.rsrp.0", servcell_info.Rsrp[0]);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.rsrp.0");

    /* RSRP 1 */
    if (servcell_info.RsrpValid[1])
        _rdb_prefix_call(_rdb_set_tenths_decimal, "servcell_info.rsrp.1", servcell_info.Rsrp[1]);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.rsrp.1");

#else // V_RF_MEASURE_REPORT_dundee || V_RF_MEASURE_REPORT_myna
    rsrp = _rdb_get_int_quiet("wwan.0.signal.0.rsrp");

    // Expect rsrp RDB is available when attached
    if (!rsrp && _rdb_get_int_quiet("wwan.0.system_network_status.attached")) {
        ERR("[RDB] failed to get RDB wwan.0.signal.0.rsrp");
    }

    /* RSRP 0 */
    _rdb_prefix_call(_rdb_set_tenths_decimal, "servcell_info.rsrp.0", rsrp);

    /* RSRP 1 */
    _rdb_prefix_call(_rdb_set_tenths_decimal, "servcell_info.rsrp.1", rsrp);

#endif // V_RF_MEASURE_REPORT_dundee || V_RF_MEASURE_REPORT_myna

    /* rx gain */
    if (int_stats.inner_loop_gain > 0)
        _rdb_set_tenths_decimal("wwan.0.signal.rx_gain.inner", int_stats.inner_loop_gain);
    else
        _rdb_set_reset("wwan.0.signal.rx_gain.inner");
}
#endif
void increase_pcc_ca_state(time_t event_time)
{
    if (caUsage.pCellState > CA_STATE_DISCONNECTED) {
        caUsage.pCell[caUsage.currentPCellIndex].duration[caUsage.pCellState - 1] += event_time - caUsage.pCellStartTime;
    }
    if (caUsage.pCellState < CA_STATE_MAX_NUM) {
        caUsage.pCellState++;
    }
    caUsage.pCellStartTime = event_time;
}

void decrease_pcc_ca_state(time_t event_time)
{
    if (caUsage.pCellState > CA_STATE_DISCONNECTED) {
        caUsage.pCell[caUsage.currentPCellIndex].duration[caUsage.pCellState - 1] += event_time - caUsage.pCellStartTime;
        caUsage.pCellState--;
    }
    caUsage.pCellStartTime = event_time;
}

/* log snprintf() errors */
char *print_error_check(char *buf, char *endBuf, int returnValue)
{
    if (returnValue < 0) {
        ERR("snprintf() error encountered");
    } else {
        buf += returnValue;
    }

    if (buf > endBuf) {
        buf = endBuf;
        ERR("snprintf() data truncated");
    }

    return (buf);
}

/**********************************************************************
Report cell_usage and ca_mode as following:

cell_usage:
String array contains CA duration. Multiple cells’ information is separated by
“|”. For each cell, the following fields are provided and separated by “,”.
EARFCN,PCI,Index,ConfiguredDuration,ActivatedDuration
Notes:
EARFCN: EARFCN of the Cell’s operating frequency
Index: Integer with potential value of 0, 1, 2, where 0 means the cell acted
purely as Pcell during the interval, 1 means the cell acted purely as Scell
during the interval, and 2 means the cell acted as Pcell for some period, and
Scell for some period during the interval.
ConfiguredDuration: Combined duration (in milliseconds) of periods when the
cell is configured as Scell during the interval. Field is empty if the cell is used
as PCell only for the interval.
ActivatedDuration: Combined duration (in milliseconds) of periods when the
cell is configured and activated as Scell during the interval. Field is empty if
the cell is used as PCell only for the interval.

ca_mode:
String array containing CA mode duration aggregated for each PCell for the
interval. If there are multiple PCells used in the period, the multiple PCells’
information is separated by “|”. For each PCell, the following fields are
provided and separated by “,”.
EARFCN,PCI,NoCADuration,2CADuration,3CADuration,4CADuration
Notes:
EARFCN: EARFCN of the Cell’s operating frequency
PCI: Cell physical cell ID
NoCADuration: Combined duration (in milliseconds) of periods when there is traffic but not operating in CA mode (PCell traffic only).
2CADuration: Combined duration (in milliseconds) of periods when device has traffic and operating in 2CA mode.
3CADuration: Combined duration (in milliseconds) of periods when device has traffic and operating in 3CA mode.
4CADuration: Combined duration (in milliseconds) of periods when device has traffic and operating in 4CA mode.

*****************************************************************************/

void ca_cell_usage_update_reset(time_t update_time)
{
    unsigned short int pFreq = 0, pPci = 0, caFreq[CA_STATE_MAX_NUM], caPci[CA_STATE_MAX_NUM];
    unsigned char i, j, caCellCurrentState[CA_STATE_MAX_NUM], pCellCurrentState, numCc;
    /* buffer for RDB content */
    char var[RDB_MAX_NAME_LEN] = "";
    int var_len = sizeof(var);
    char val[RDB_MAX_VAL_LEN] = "";
    char *val_ptr = val;
    char *val_end_ptr = val + sizeof(val);

    // save state before resetting
    pCellCurrentState = caUsage.pCellState;
    numCc = caUsage.numCcConfigured;

    if (pCellCurrentState != CA_STATE_DISCONNECTED) {
        pFreq = caUsage.pCell[caUsage.currentPCellIndex].freq;
        pPci = caUsage.pCell[caUsage.currentPCellIndex].pci;

        for (i = 0; i < numCc; i++) {
            caFreq[i] = caUsage.caCell[caUsage.ccConfiguredIndex[i]].freq;
            caPci[i] = caUsage.caCell[caUsage.ccConfiguredIndex[i]].pci;
            caCellCurrentState[i] = caUsage.caCell[caUsage.ccConfiguredIndex[i]].previousState;

            /* if scell is activated, calculate final activated and configured times for the interval.
               if scell is configured-deactivated calculate final configured time                  */
            switch (caCellCurrentState[i]) {
                case SCELL_STATE_CONFIGURED_ACTIVATED:
                    caUsage.caCell[caUsage.ccConfiguredIndex[i]].activatedTime +=
                        update_time - caUsage.caCell[caUsage.ccConfiguredIndex[i]].actStartTime;
                    decrease_pcc_ca_state(update_time);

                    // fall-through

                case SCELL_STATE_CONFIGURED_DEACTIVATED:
                    caUsage.caCell[caUsage.ccConfiguredIndex[i]].configuredTime +=
                        update_time - caUsage.caCell[caUsage.ccConfiguredIndex[i]].configStartTime;
            }
        }
        caUsage.pCell[caUsage.currentPCellIndex].duration[CA_STATE_PCC - 1] += update_time - caUsage.pCellStartTime;
    }

    // report CellUsage
    for (i = 0; i < caUsage.usedNumCaCells; i++) {
        for (j = 0; j < caUsage.usedNumPCells; j++) {
            if ((caUsage.caCell[i].freq == caUsage.pCell[j].freq) && (caUsage.caCell[i].pci == caUsage.pCell[j].pci)) {
                caUsage.caCell[i].index = CA_INDEX_PCC_AND_SCC;
                caUsage.pCell[j].index = CA_INDEX_PCC_AND_SCC;
            }
        }
    }

    for (i = 0, j = 0; i < caUsage.usedNumPCells; i++) {
        if (!caUsage.pCell[i].index) {
            if (j) {
                val_ptr = print_error_check(val_ptr, val_end_ptr, snprintf(val_ptr, val_end_ptr - val_ptr, "|"));
            }
            val_ptr = print_error_check(val_ptr, val_end_ptr,
                                        snprintf(val_ptr, val_end_ptr - val_ptr, "%u,%u,0,0,0", caUsage.pCell[i].freq, caUsage.pCell[i].pci));
            j = 1;
        }
    }

    for (i = 0; i < caUsage.usedNumCaCells; i++) {
        if (j) {
            val_ptr = print_error_check(val_ptr, val_end_ptr, snprintf(val_ptr, val_end_ptr - val_ptr, "|"));
        }
        val_ptr = print_error_check(val_ptr, val_end_ptr,
                                    snprintf(val_ptr, val_end_ptr - val_ptr, "%u,%u,%u,%u,%u", caUsage.caCell[i].freq, caUsage.caCell[i].pci,
                                             caUsage.caCell[i].index, caUsage.caCell[i].configuredTime, caUsage.caCell[i].activatedTime));
        j = 1;
    }
    _rdb_prefix_call(_rdb_set_str, "system_network_status.ca.cell_usage", val);

    // report CAMode
    val_ptr = val;
    *val_ptr = 0;

    for (i = 0; i < caUsage.usedNumPCells; i++) {
        if (i) {
            val_ptr = print_error_check(val_ptr, val_end_ptr, snprintf(val_ptr, val_end_ptr - val_ptr, "|"));
        }
        val_ptr = print_error_check(val_ptr, val_end_ptr,
                                    snprintf(val_ptr, val_end_ptr - val_ptr, "%u,%u,%u,%u,%u,%u", caUsage.pCell[i].freq, caUsage.pCell[i].pci,
                                             caUsage.pCell[i].duration[CA_STATE_PCC - 1], caUsage.pCell[i].duration[CA_STATE_1CC - 1],
                                             caUsage.pCell[i].duration[CA_STATE_2CC - 1], caUsage.pCell[i].duration[CA_STATE_3CC - 1]));
    }
    _rdb_prefix_call(_rdb_set_str, "system_network_status.ca.ca_mode", val);

    // reset values
    memset((char *)&caUsage, 0, sizeof(caUsage));

    // restore active cell(s) from previous interval.
    if (pCellCurrentState > 0) {
        caUsage.currentPCellIndex = 0;
        caUsage.usedNumPCells = 1;
        caUsage.pCell[0].freq = pFreq;
        caUsage.pCell[0].pci = pPci;
        increase_pcc_ca_state(update_time);

        caUsage.numCcConfigured = numCc;
        caUsage.usedNumCaCells = numCc;
        /* if scell is activated, restore activated and configured times.
           if scell is configured deactivated, restore configured time. */
        for (i = 0; i < numCc; i++) {
            switch (caCellCurrentState[i]) {
                case SCELL_STATE_CONFIGURED_ACTIVATED:
                    caUsage.caCell[i].actStartTime = update_time;
                    increase_pcc_ca_state(update_time);

                    // fall-through

                case SCELL_STATE_CONFIGURED_DEACTIVATED:
                    caUsage.caCell[i].configStartTime = update_time;
                    caUsage.caCell[i].freq = caFreq[i];
                    caUsage.caCell[i].pci = caPci[i];
                    caUsage.caCell[i].index = CA_INDEX_SCC_ONLY;
                    caUsage.caCell[i].previousState = caCellCurrentState[i];
                    caUsage.ccConfiguredIndex[i] = i;
            }
        }
    }
}

/*
 Write servcell info into RDB.
   After writing all of statistic information into [wwan.x.servcell_info.] RDBs, the function resets
  servcell info.

 Parameters:
  servcell          servcell_info_t struct point to use
  tm                start time
  tm_monotonic      start time in monotonic

 Return:
  None.
*/
void servcell_info_update(struct servcell_info_t *servcell, time_t tm, time_t tm_monotonic)
{
    int i;

    /* buffer for RDB variable name */
    char rname[RDB_MAX_NAME_LEN];
    /* buffer for RDB content */
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    time_t tm_start;

    if (servcell->AvgWidebandCQI.stat_valid) {
        _rdb_prefix_call(_rdb_set_int, "servcell_info.avg_wide_band_cqi", __round(servcell->AvgWidebandCQI.avg));
    } else {
        _rdb_prefix_call(_rdb_set_str, "servcell_info.avg_wide_band_cqi", "");
    }
    _rdb_prefix_call(_rdb_set_int, "servcell_info.number_of_rrc_estab_attempts", servcell->NumberofRRCEstabAttempts);
    _rdb_prefix_call(_rdb_set_int, "servcell_info.number_of_rrc_estab_failures", servcell->NumberofRRCEstabFailures);
    _rdb_prefix_call(_rdb_set_int, "servcell_info.number_of_rrc_reestab_attempts", servcell->NumberofRRCReEstabAttempts);
    _rdb_prefix_call(_rdb_set_int, "servcell_info.number_of_rrc_reestab_failures", servcell->NumberofRRCReEstabFailures);
    _rdb_prefix_call(_rdb_set_int, "servcell_info.rrc_estab_latency", __round(servcell->RRCEstabLatency.avg));
    _rdb_prefix_call(_rdb_set_int, "servcell_info.rrc_reestab_latency", __round(servcell->RRCReEstabLatency.avg));
    _rdb_prefix_call(_rdb_set_str, "servcell_info.pmi_distribution", distr_get(&servcell->PMIDistribution));
    _rdb_prefix_call(_rdb_set_str, "servcell_info.pusch_transmit_power_distribution", distr_get(&servcell->PUSCHTrasmitPower));
    _rdb_prefix_call(_rdb_set_str, "servcell_info.received_modulation_distribution", distr_get(&servcell->ReceivedModulationDistribution));
    _rdb_prefix_call(_rdb_set_str, "servcell_info.ri_distribution", distr_get(&servcell->RIDistribution));
    _rdb_prefix_call(_rdb_set_str, "servcell_info.send_modulation_distribution", distr_get(&servcell->SendModulationDistribution));
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.handover_count", servcell->HandoverCount);
    ca_cell_usage_update_reset(get_monotonic_ms());
    // update starting of 15 min collection interval (absolute time in secs from jan 1st, 1970)
    _rdb_prefix_call(_rdb_set_uint, "system_network_status.ca.start_time", time(NULL) - 15 * 60);
#ifdef V_RF_MEASURE_REPORT_myna
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.handover_attempt", servcell->HandoverAttempt);
#endif
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.rlf_count", servcell->RLFCount);
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.total_active_ttis_received", servcell->TotalActiveTTIsReceived);
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.total_active_ttis_sent", servcell->TotalActiveTTIsSent);
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.total_prbs_received", servcell->TotalPRBsReceived);
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.total_prbs_sent", servcell->TotalPRBsSent);

/* WNTDv3 requires to measure instant signal strength rather than averaging and most of
 * values are collected via WMMD except for rsrp per antenna. */
#ifdef SAVE_AVERAGED_DATA
    /* rssnr */
    if (servcell->SCellRSSNR.stat_valid)
        _rdb_prefix_call(_rdb_set_tenths_decimal, "servcell_info.rssnr", __dbx10(servcell->SCellRSSNR.avg, -20));
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.rssnr");

    /* rssinr */
    if (servcell->SCellRSSINR.stat_valid)
        _rdb_prefix_call(_rdb_set_tenths_decimal, "servcell_info.rssinr", servcell->SCellRSSINR.avg);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.rssinr");

    /* rsrp */
    if (servcell->SCellRSRP.stat_valid)
        _rdb_prefix_call(_rdb_set_tenths_decimal, "servcell_info.rsrp", servcell->SCellRSRP.avg);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.rsrp");

    /* rsrq */
    if (servcell->SCellRSRQ.stat_valid)
        _rdb_prefix_call(_rdb_set_tenths_decimal, "servcell_info.rsrq", servcell->SCellRSRQ.avg);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.rsrq");
#endif

    /* Qualcomm max tx power is 39 */
    if (servcell->max_ue_tx_power >= 40)
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.max_ue_tx_power");
    else
        _rdb_prefix_call(_rdb_set_int, "servcell_info.max_ue_tx_power", servcell->max_ue_tx_power);

/* WNTDv3 requires to update these information in 30 seconds interval so
 * moved RDB updates for them into 30 seconds polling function */
#ifdef SAVE_AVERAGED_DATA
    /* BLERi received */
    if (servcell->MACiBLERReceivedAckNackCount)
        _rdb_prefix_call(_rdb_set_uint, "servcell_info.mac_i_bler_received",
                         servcell->MACiBLERReceivedNackCount * 1000000 / servcell->MACiBLERReceivedAckNackCount);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.mac_i_bler_received");

    /* BLERr received */
    if (servcell->MACrBLERReceivedAckNackCount)
        _rdb_prefix_call(_rdb_set_uint, "servcell_info.mac_r_bler_received",
                         servcell->MACrBLERReceivedNackCount * 1000000 / servcell->MACrBLERReceivedAckNackCount);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.mac_r_bler_received");

    /* BLERi sent */
    if (servcell->MACiBLERSentAckNackCount)
        _rdb_prefix_call(_rdb_set_uint, "servcell_info.mac_i_bler_sent",
                         servcell->MACiBLERSentNackCount * 1000000 / servcell->MACiBLERSentAckNackCount);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.mac_i_bler_sent");

    /* BLERr sent */
    if (servcell->MACrBLERSentAckNackCount)
        _rdb_prefix_call(_rdb_set_uint, "servcell_info.mac_r_bler_sent",
                         servcell->MACrBLERSentNackCount * 1000000 / servcell->MACrBLERSentAckNackCount);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.mac_r_bler_sent");

    if (servcell->max_ul_harq_transmissions)
        _rdb_prefix_call(_rdb_set_int, "servcell_info.max_ul_harq_transmissions", servcell->max_ul_harq_transmissions);
    else
        _rdb_prefix_call(_rdb_set_reset, "servcell_info.max_ul_harq_transmissions");
#endif

    /* failure count */
    for (i = 0; i < MAX_FAILURE_CAUSES; i++) {
        snprintf(rname, sizeof(rname), "servcell_info.rlf_failures.%d.", i);
        _rdb_suffix_call(_rdb_set_int, rname, "count", servcell->FailureCounts[i]);
    }

#ifdef V_RF_MEASURE_REPORT_myna
    /* now update cell info based on cell ID */
    for (i = 0; i < MAX_SERVER_CELLS_PER_MEASURING_PERIOD; i++) {
        snprintf(rname, sizeof(rname), "servcell_info.pcell.%d.", i);
        /* if this slot is unused, then use it */
        if (cell_info_by_id[i].PCI != -1) {
            _rdb_suffix_call(_rdb_set_int, rname, "CellID", cell_info_by_id[i].CellID);
            _rdb_suffix_call(_rdb_set_int, rname, "pci", cell_info_by_id[i].PCI);
            if (cell_info_by_id[i].SCellRSRP.stat_valid)
                _rdb_suffix_call(_rdb_set_tenths_decimal, rname, "rsrp", cell_info_by_id[i].SCellRSRP.avg);
            else
                _rdb_suffix_call(_rdb_set_str, rname, "rsrp", "");
            if (cell_info_by_id[i].SCellRSRQ.stat_valid)
                _rdb_suffix_call(_rdb_set_tenths_decimal, rname, "rsrq", cell_info_by_id[i].SCellRSRQ.avg);
            else
                _rdb_suffix_call(_rdb_set_str, rname, "rsrq", "");
            if (cell_info_by_id[i].SCellRSSINR.stat_valid)
                _rdb_suffix_call(_rdb_set_tenths_decimal, rname, "rssinr", cell_info_by_id[i].SCellRSSINR.avg);
            else
                _rdb_suffix_call(_rdb_set_str, rname, "rssinr", "");
            _rdb_suffix_call(_rdb_set_int, rname, "handover_attempt", cell_info_by_id[i].HandoverAttempt);
            _rdb_suffix_call(_rdb_set_int, rname, "handover_count", cell_info_by_id[i].HandoverCount);
            if (cell_info_by_id[i].MACiBLERReceivedAckNackCount)
                _rdb_suffix_call(_rdb_set_uint, rname, "mac_i_bler_received",
                                 cell_info_by_id[i].MACiBLERReceivedNackCount * 1000000 / cell_info_by_id[i].MACiBLERReceivedAckNackCount);
            else
                _rdb_suffix_call(_rdb_set_str, rname, "mac_i_bler_received", "");
            if (cell_info_by_id[i].MACiBLERSentAckNackCount)
                _rdb_suffix_call(_rdb_set_uint, rname, "mac_i_bler_sent",
                                 cell_info_by_id[i].MACiBLERSentNackCount * 1000000 / cell_info_by_id[i].MACiBLERSentAckNackCount);
            else
                _rdb_suffix_call(_rdb_set_str, rname, "mac_i_bler_sent", "");
            if (cell_info_by_id[i].MACrBLERReceivedAckNackCount)
                _rdb_suffix_call(_rdb_set_uint, rname, "mac_r_bler_received",
                                 cell_info_by_id[i].MACrBLERReceivedNackCount * 1000000 / cell_info_by_id[i].MACrBLERReceivedAckNackCount);
            else
                _rdb_suffix_call(_rdb_set_str, rname, "mac_r_bler_received", "");
            if (cell_info_by_id[i].MACrBLERSentAckNackCount)
                _rdb_suffix_call(_rdb_set_uint, rname, "mac_r_bler_sent",
                                 cell_info_by_id[i].MACrBLERSentNackCount * 1000000 / cell_info_by_id[i].MACrBLERSentAckNackCount);
            else
                _rdb_suffix_call(_rdb_set_str, rname, "mac_r_bler_sent", "");
            _rdb_suffix_call(_rdb_set_uint, rname, "total_prbs_received", cell_info_by_id[i].TotalPRBsReceived);
            _rdb_suffix_call(_rdb_set_uint, rname, "total_active_ttis_received", cell_info_by_id[i].TotalActiveTTIsReceived);
            _rdb_suffix_call(_rdb_set_uint, rname, "total_prbs_sent", cell_info_by_id[i].TotalPRBsSent);
            _rdb_suffix_call(_rdb_set_uint, rname, "total_active_ttis_sent", cell_info_by_id[i].TotalActiveTTIsSent);
            _rdb_suffix_call(_rdb_set_str, rname, "pmi_distribution", distr_get(&cell_info_by_id[i].PMIDistribution));
            _rdb_suffix_call(_rdb_set_str, rname, "ri_distribution", distr_get(&cell_info_by_id[i].RIDistribution));
            _rdb_suffix_call(_rdb_set_str, rname, "received_modulation_distribution", distr_get(&cell_info_by_id[i].ReceivedModulationDistribution));
            _rdb_suffix_call(_rdb_set_str, rname, "send_modulation_distribution", distr_get(&cell_info_by_id[i].SendModulationDistribution));
            _rdb_suffix_call(_rdb_set_str, rname, "pusch_transmit_power_distribution", distr_get(&cell_info_by_id[i].PUSCHTrasmitPower));
            _rdb_suffix_call(_rdb_set_uint, rname, "rlf_count", cell_info_by_id[i].RLFCount);

            _rdb_suffix_call(_rdb_set_int, rname, "number_of_rrc_estab_attempts", cell_info_by_id[i].NumberofRRCEstabAttempts);
            _rdb_suffix_call(_rdb_set_int, rname, "number_of_rrc_estab_failures", cell_info_by_id[i].NumberofRRCEstabFailures);
            _rdb_suffix_call(_rdb_set_int, rname, "rrc_estab_latency", __round(cell_info_by_id[i].RRCEstabLatency.avg));
            _rdb_suffix_call(_rdb_set_int, rname, "number_of_rrc_reestab_attempts", cell_info_by_id[i].NumberofRRCReEstabAttempts);
            _rdb_suffix_call(_rdb_set_int, rname, "number_of_rrc_reestab_failures", cell_info_by_id[i].NumberofRRCReEstabFailures);
            _rdb_suffix_call(_rdb_set_int, rname, "rrc_reestab_latency", __round(cell_info_by_id[i].RRCReEstabLatency.avg));
        } else {
            /* this slot is empty, clear the data in it */
            _rdb_suffix_call(_rdb_set_str, rname, "CellID", "");
            _rdb_suffix_call(_rdb_set_str, rname, "pci", "");
            _rdb_suffix_call(_rdb_set_str, rname, "rsrp", "");
            _rdb_suffix_call(_rdb_set_str, rname, "rsrq", "");
            _rdb_suffix_call(_rdb_set_str, rname, "rssinr", "");
            _rdb_suffix_call(_rdb_set_str, rname, "handover_attempt", "");
            _rdb_suffix_call(_rdb_set_str, rname, "handover_count", "");
            _rdb_suffix_call(_rdb_set_str, rname, "mac_i_bler_received", "");
            _rdb_suffix_call(_rdb_set_str, rname, "mac_i_bler_sent", "");
            _rdb_suffix_call(_rdb_set_str, rname, "mac_r_bler_received", "");
            _rdb_suffix_call(_rdb_set_str, rname, "mac_r_bler_sent", "");
            _rdb_suffix_call(_rdb_set_str, rname, "total_prbs_received", "");
            _rdb_suffix_call(_rdb_set_str, rname, "total_active_ttis_received", "");
            _rdb_suffix_call(_rdb_set_str, rname, "total_prbs_sent", "");
            _rdb_suffix_call(_rdb_set_str, rname, "total_active_ttis_sent", "");
            _rdb_suffix_call(_rdb_set_str, rname, "pmi_distribution", "");
            _rdb_suffix_call(_rdb_set_str, rname, "ri_distribution", "");
            _rdb_suffix_call(_rdb_set_str, rname, "received_modulation_distribution", "");
            _rdb_suffix_call(_rdb_set_str, rname, "send_modulation_distribution", "");
            _rdb_suffix_call(_rdb_set_str, rname, "pusch_transmit_power_distribution", "");
            _rdb_suffix_call(_rdb_set_str, rname, "rlf_count", "");
            _rdb_suffix_call(_rdb_set_str, rname, "number_of_rrc_estab_attempts", "");
            _rdb_suffix_call(_rdb_set_str, rname, "number_of_rrc_estab_failures", "");
            _rdb_suffix_call(_rdb_set_str, rname, "rrc_estab_latency", "");
            _rdb_suffix_call(_rdb_set_str, rname, "number_of_rrc_reestab_attempts", "");
            _rdb_suffix_call(_rdb_set_str, rname, "number_of_rrc_reestab_failures", "");
            _rdb_suffix_call(_rdb_set_str, rname, "rrc_reestab_latency", "");
        }
    }
#endif

    /* DO NOT MOVE below lines
     * This is a trigger variable which triggers TurboNtc history collection so should be placed last of this function
     * otherwise history data is not reliable at all */
    /* start time */
    tm_start = tm - (tm_monotonic - servcell->StartTimeMonotonic) / 1000;
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.start_time_monotonic", servcell->StartTimeMonotonic);
    _rdb_prefix_call(_rdb_set_uint, "servcell_info.start_time", tm_start);
}

#ifdef INCLUDE_IMS_VOIP_MSG
/*
        ### line stats functions ###
*/

void linestats_info_read_rdb()
{
    char str[RDB_MAX_VAL_LEN];
    int str_len = sizeof(str);

    linestats_info.BytesReceived = _rdb_get_int("voicecall.statistics.bytes_received");
    linestats_info.BytesSent = _rdb_get_int("voicecall.statistics.bytes_sent");
    linestats_info.CallsDropped = _rdb_get_int("voicecall.statistics.calls_dropped");
    linestats_info.IncomingCallsAnswered = _rdb_get_int("voicecall.statistics.incoming_calls_answered");
    linestats_info.IncomingCallsConnected = _rdb_get_int("voicecall.statistics.incoming_calls_connected");
    linestats_info.IncomingCallsFailed = _rdb_get_int("voicecall.statistics.incoming_calls_failed");
    linestats_info.IncomingCallsReceived = _rdb_get_int("voicecall.statistics.incoming_calls_received");
    linestats_info.OutgoingCallsAnswered = _rdb_get_int("voicecall.statistics.outgoing_calls_answered");
    linestats_info.OutgoingCallsAttempted = _rdb_get_int("voicecall.statistics.outgoing_calls_attempted");
    linestats_info.OutgoingCallsConnected = _rdb_get_int("voicecall.statistics.outgoing_calls_connected");
    linestats_info.OutgoingCallsFailed = _rdb_get_int("voicecall.statistics.outgoing_calls_failed");
    linestats_info.Overruns = _rdb_get_int("voicecall.statistics.overruns");
    linestats_info.PacketsLost = _rdb_get_int("voicecall.statistics.packets_lost");
    linestats_info.PacketsReceived = _rdb_get_int("voicecall.statistics.packets_received");
    linestats_info.PacketsSent = _rdb_get_int("voicecall.statistics.packets_sent");
    linestats_info.TotalCallTime = _rdb_get_int("voicecall.statistics.total_call_time");
    linestats_info.Underruns = _rdb_get_int("voicecall.statistics.underruns");

    strncpy(linestats_info.CurrCallRemoteIP, _rdb_get_str(str, str_len, "voicecall.statistics.curr_call_remote_ip"),
            sizeof(linestats_info.CurrCallRemoteIP));
    strncpy(linestats_info.LastCallRemoteIP, _rdb_get_str(str, str_len, "voicecall.statistics.last_call_remote_ip"),
            sizeof(linestats_info.LastCallRemoteIP));
}

void linestats_info_update_rdb()
{
    _rdb_set_uint("voicecall.statistics.bytes_received", linestats_info.BytesReceived);
    _rdb_set_uint("voicecall.statistics.bytes_sent", linestats_info.BytesSent);
    _rdb_set_uint("voicecall.statistics.calls_dropped", linestats_info.CallsDropped);
    _rdb_set_uint("voicecall.statistics.incoming_calls_answered", linestats_info.IncomingCallsAnswered);
    _rdb_set_uint("voicecall.statistics.incoming_calls_connected", linestats_info.IncomingCallsConnected);
    _rdb_set_uint("voicecall.statistics.incoming_calls_failed", linestats_info.IncomingCallsFailed);
    _rdb_set_uint("voicecall.statistics.incoming_calls_received", linestats_info.IncomingCallsReceived);
    _rdb_set_uint("voicecall.statistics.outgoing_calls_answered", linestats_info.OutgoingCallsAnswered);
    _rdb_set_uint("voicecall.statistics.outgoing_calls_attempted", linestats_info.OutgoingCallsAttempted);
    _rdb_set_uint("voicecall.statistics.outgoing_calls_connected", linestats_info.OutgoingCallsConnected);
    _rdb_set_uint("voicecall.statistics.outgoing_calls_failed", linestats_info.OutgoingCallsFailed);
    _rdb_set_uint("voicecall.statistics.overruns", linestats_info.Overruns);
    _rdb_set_uint("voicecall.statistics.packets_lost", linestats_info.PacketsLost);
    _rdb_set_uint("voicecall.statistics.packets_received", linestats_info.PacketsReceived);
    _rdb_set_uint("voicecall.statistics.packets_sent", linestats_info.PacketsSent);
    _rdb_set_uint("voicecall.statistics.total_call_time", linestats_info.TotalCallTime);
    _rdb_set_uint("voicecall.statistics.underruns", linestats_info.Underruns);

    _rdb_set_str("voicecall.statistics.curr_call_remote_ip", linestats_info.CurrCallRemoteIP);
    _rdb_set_str("voicecall.statistics.last_call_remote_ip", linestats_info.LastCallRemoteIP);

    _rdb_set_uint("voicecall.statistics.trigger", 1);
}

void linestats_info_reset()
{
    int off;
    char *p;

    /* reset members */
    off = __offset(struct linestats_info_t, __reset_ptr);
    p = (char *)&linestats_info + off;
    memset(p, 0, sizeof(linestats_info) - off);

    linestats_info_update_rdb();
}

/*
        ### voice call history functions ###
*/

static void vci_fini() {}

static void vci_init()
{
    int i;
    struct voicecall_info_t *vci;

    /* add entries into off line pool */
    for (i = 0; i < MAX_VOICECALL_SESSION_INFO; i++) {
        vci = &voicecall_info_pool[i];

        memset(vci, 0, sizeof(*vci));

        vci->index = i;
        list_add_tail(&vci->list, &voicecall_info_offline);
    }
}

static void vci_delete_session(struct voicecall_info_t *vci)
{
    list_del(&vci->list);
    list_add_tail(&vci->list, &voicecall_info_offline);
}

static struct voicecall_info_t *vci_get_session(int (*cmp)(struct voicecall_info_t *, const void *), const void *ref)
{
    struct voicecall_info_t *vci = NULL;
    struct voicecall_info_t *e;
    struct list_head *p;

    /* bypass if no session available */
    if (list_empty(&voicecall_info_online)) {
        goto fini;
    }

    /* search cm call id */
    list_for_each(p, &voicecall_info_online)
    {
        e = container_of(p, struct voicecall_info_t, list);
        if (!cmp(e, ref)) {
            vci = e;
            break;
        }
    }

fini:
    return vci;
}

int vci_get_session_by_cm_call_id_cmp(struct voicecall_info_t *e, const void *ref)
{
    /* bypass if cm call id is not valid */
    if (!e->cm_call_id_valid)
        return -1;

    return e->cm_call_id - *(int *)ref;
}

int vci_get_session_by_sip_call_id_cmp(struct voicecall_info_t *e, const void *ref)
{
    /* byass if sip call id is not valid */
    if (!e->sip_call_id_valid)
        return -1;

    return strcmp(e->sip_call_id, (const char *)ref);
}

int vci_get_session_by_qc_call_id_cmp(struct voicecall_info_t *e, const void *ref)
{
    /* bypass if qc call id is not valid */
    if (!e->qc_call_id_valid)
        return -1;

    return e->qc_call_id - *(int *)ref;
}

int vci_get_session_by_ssrc_rx_id_cmp(struct voicecall_info_t *e, const void *ref)
{
    /* byass if sip call id is not valid */
    if (!e->ssrc_rx_id_valid)
        return -1;

    return e->ssrc_rx_id - *(int *)ref;
}

int vci_get_session_by_ssrc_tx_id_cmp(struct voicecall_info_t *e, const void *ref)
{
    /* byass if sip call id is not valid */
    if (!e->ssrc_tx_id_valid)
        return -1;

    return e->ssrc_tx_id - *(unsigned int *)ref;
}

static struct voicecall_info_t *vci_get_session_by_cm_call_id(int cm_call_id)
{
    return vci_get_session(vci_get_session_by_cm_call_id_cmp, &cm_call_id);
}

static struct voicecall_info_t *vci_get_session_by_sip_call_id(const char *sip_call_id)
{
    return vci_get_session(vci_get_session_by_sip_call_id_cmp, sip_call_id);
}

static struct voicecall_info_t *vci_get_session_by_ssrc_rx_id(unsigned int ssrc)
{
    return vci_get_session(vci_get_session_by_ssrc_rx_id_cmp, &ssrc);
}

static struct voicecall_info_t *vci_get_session_by_ssrc_tx_id(unsigned int ssrc)
{
    return vci_get_session(vci_get_session_by_ssrc_tx_id_cmp, &ssrc);
}

static struct voicecall_info_t *vci_get_session_by_qc_call_id(unsigned int qc_call_id)
{
    return vci_get_session(vci_get_session_by_qc_call_id_cmp, &qc_call_id);
}

static struct voicecall_info_t *vci_get_session_by_cm_call_id_mt(int cm_call_id)
{
    struct voicecall_info_t *vci;

    vci = vci_get_session(vci_get_session_by_cm_call_id_cmp, &cm_call_id);

    return vci;
}

static struct voicecall_info_t *vci_get_session_by_sip_call_id_mt(const char *sip_call_id)
{
    struct voicecall_info_t *vci;

    vci = vci_get_session(vci_get_session_by_sip_call_id_cmp, sip_call_id);

    return vci;
}

static struct voicecall_info_t *vci_get_session_by_ssrc_rx_id_mt(unsigned int ssrc)
{
    struct voicecall_info_t *vci;

    vci = vci_get_session(vci_get_session_by_ssrc_rx_id_cmp, &ssrc);
    if (!vci)
        vci = vci_get_session(vci_get_session_by_ssrc_tx_id_cmp, &ssrc);

    return vci;
}

static struct voicecall_info_t *vci_get_session_by_ssrc_tx_id_mt(unsigned int ssrc)
{
    struct voicecall_info_t *vci;

    vci = vci_get_session(vci_get_session_by_ssrc_tx_id_cmp, &ssrc);
    if (!vci)
        vci = vci_get_session(vci_get_session_by_ssrc_rx_id_cmp, &ssrc);

    return vci;
}

static struct voicecall_info_t *vci_get_session_by_qc_call_id_mt(unsigned int qc_call_id)
{
    struct voicecall_info_t *vci;

    vci = vci_get_session(vci_get_session_by_qc_call_id_cmp, &qc_call_id);

    return vci;
}

static void voicecall_info_reset_members(struct voicecall_info_t *vci)
{
    char *p;
    int off;

    DEBUG("reset voicecall slot (index=%d)", vci->index);

    /* reset members */
    off = __offset(struct voicecall_info_t, __reset_ptr);
    p = (char *)vci + off;
    memset(p, 0, sizeof(*vci) - off);

    avg_reset(&vci->AvgRTPLatency);
}

struct voicecall_info_t *vci_get_last()
{
    if (list_empty(&voicecall_info_online))
        goto err;

    return container_of(voicecall_info_online.next, struct voicecall_info_t, list);

err:
    return NULL;
}

struct voicecall_info_t *vci_create_session()
{
    struct voicecall_info_t *vci = NULL;

    /* bypass if no slot available */
    if (list_empty(&voicecall_info_offline)) {
        ERR("too many online voice sessions in use");
        goto fini;
    }

    /* get first available slot */
    vci = container_of(voicecall_info_offline.next, struct voicecall_info_t, list);
    list_del(&vci->list);

    voicecall_info_reset_members(vci);

    /* add slot to online slot */
    list_add(&vci->list, &voicecall_info_online);

fini:
    return vci;
}

struct voicecall_info_t *vci_create_session_by_id(enum session_id_type id_type, const void *session_id)
{
    struct voicecall_info_t *vci;
    int valid;
    int existing = 0;

    /* get vci */
    switch (id_type) {
        case session_id_type_cm:
            VERBOSE("[voicecall-session] get session by cm call id (cm_call_id=%d)", *(unsigned int *)session_id);
            vci = vci_get_session_by_cm_call_id(*(unsigned int *)session_id);
            break;

        case session_id_type_qc:
            VERBOSE("[voicecall-session] get session by qc call id (qc_call_id=%u)", *(unsigned int *)session_id);
            vci = vci_get_session_by_qc_call_id(*(unsigned int *)session_id);
            break;

        case session_id_type_sip:
            VERBOSE("[voicecall-session] get session by sip call id (sip_call_id=%s)", (const char *)session_id);
            vci = vci_get_session_by_sip_call_id((const char *)session_id);
            break;

        case session_id_type_ssrc_tx:
            VERBOSE("[voicecall-session] get session by ssrc id (ssrc_tx_id=%u)", *(unsigned int *)session_id);
            vci = vci_get_session_by_ssrc_tx_id(*(unsigned int *)session_id);
            break;

        case session_id_type_ssrc_rx:
            VERBOSE("[voicecall-session] get session by ssrc id (ssrc_rx_id=%u)", *(unsigned int *)session_id);
            vci = vci_get_session_by_ssrc_rx_id(*(unsigned int *)session_id);
            break;

        default:
            ERR("unknown session id type (id_type=%d)", id_type);
            goto err;
    }

    existing = vci != NULL;

    if (!existing) {

        /* create vci if vci does not exist */
        vci = vci_get_last();

        /* by defailt, vailid */
        valid = 1;

        /* get valid flag based on session id type */
        if (vci) {
            switch (id_type) {
                case session_id_type_cm:
                    valid = vci->cm_call_id_valid;
                    VERBOSE("[voicecall-session] last session found (index=%d,cm_call_id_valid=%d)", vci->index, valid);
                    break;

                case session_id_type_qc:
                    valid = vci->qc_call_id_valid;
                    VERBOSE("[voicecall-session] last session found (index=%d,qc_call_id_valid=%d)", vci->index, valid);
                    break;

                case session_id_type_sip:
                    valid = vci->sip_call_id_valid;
                    VERBOSE("[voicecall-session] last session found (index=%d,sip_call_id_valid=%d)", vci->index, valid);
                    break;

                case session_id_type_ssrc_tx:
                    valid = vci->ssrc_tx_id_valid;
                    VERBOSE("[voicecall-session] last session found (index=%d,ssrc_tx_id_valid=%d)", vci->index, valid);
                    break;

                case session_id_type_ssrc_rx:
                    valid = vci->ssrc_rx_id_valid;
                    VERBOSE("[voicecall-session] last session found (index=%d,ssrc_rx_id_valid=%d)", vci->index, valid);
                    break;

                default:
                    ERR("unknown session id type (id_type=%d)", id_type);
                    goto err;
            }
        }

        /* create if not existing */
        existing = !valid;
        if (!existing) {
            switch (id_type) {
                case session_id_type_cm:
                case session_id_type_qc:
                case session_id_type_sip:
                    DEBUG("[voicecall-session] create vci session (id_type=%d)", id_type);
                    vci = vci_create_session();
                    if (!vci) {
                        ERR("[voicecall-session] failed to create session (id_type=%d)", id_type);
                        goto err;
                    }
                    break;

                case session_id_type_ssrc_tx:
                case session_id_type_ssrc_rx:
                    DEBUG("[voicecall-session] id type not matching, skip creation (id_type=%d)", id_type);
                    goto err;

                default:
                    ERR("unknown session id type (id_type=%d)", id_type);
                    goto err;
            }
        }

        /* update call id based on session id type */
        switch (id_type) {
            case session_id_type_cm:
                vci->cm_call_id_valid = 1;
                vci->cm_call_id = *(unsigned int *)session_id;
                DEBUG("[voicecall-session] update cm_call_id (index=%d,id=%u)", vci->index, vci->cm_call_id);
                break;

            case session_id_type_qc:
                vci->qc_call_id_valid = 1;
                vci->qc_call_id = *(unsigned int *)session_id;
                DEBUG("[voicecall-session] update qc_call_id (index=%d,id=%u)", vci->index, vci->qc_call_id);
                break;

            case session_id_type_sip:
                strcpy(vci->sip_call_id, (const char *)session_id);
                vci->sip_call_id_valid = 1;
                DEBUG("[voicecall-session] update sip_call_id (index=%d,id=%s)", vci->index, vci->sip_call_id);
                break;

            case session_id_type_ssrc_tx:
                vci->ssrc_tx_id_valid = 1;
                vci->ssrc_tx_id = *(unsigned int *)session_id;
                DEBUG("[voicecall-session] update ssrc_tx_id (index=%d,id=%u)", vci->index, vci->ssrc_tx_id);
                break;

            case session_id_type_ssrc_rx:
                vci->ssrc_rx_id_valid = 1;
                vci->ssrc_rx_id = *(unsigned int *)session_id;
                DEBUG("[voicecall-session] update ssrc_rx_id (index=%d,id=%u)", vci->index, vci->ssrc_rx_id);
                break;

            default:
                ERR("unknown session id type (index=%d,id_type=%u)", vci->index, id_type);
                goto err;
        }
    }

    /* bypass if existing already */
    if (existing)
        goto err;

    return vci;

err:
    return NULL;
}

void voicecall_info_reset_rdb()
{
    reset_rdb_sets("voicecall_session.");
}

/*
 Start a voice call session in voice call history.

 Parameters:
  vci : pointer to a voice call information that represents a voice call session.
  ts  : QxDM timestamp when the voice call starts.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int voicecall_info_start(struct voicecall_info_t *vci, unsigned long long *ts)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    DEBUG("[voicecall_info] start voicecall info session (session_index=%d)", vci->index);

    /* update start time */
    vci->StartTime = get_ctime_from_ts(ts);
    vci->StartTimeMonotonic = get_ms_from_ts(ts);

    vci->running = 1;

    voicecall_info_update(vci);

    _rdb_prefix_call(_rdb_set_int, "voicecall_session.index", vci->index);

    return 0;
}

/*
 Stop a voice call session in voice call history.

 Parameters:
  vci : pointer to a voice call information that represents a voice call session.
  ts  : QxDM timestamp when the voice call stops.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
void voicecall_info_stop(struct voicecall_info_t *vci, unsigned long long *ts)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);
    time_t now = get_ms_from_ts(ts);
    time_t duration;

    /* bypass if not running */
    if (!vci->running) {
        VERBOSE("[voicecall_info] not running, bypass stop procedure");
        goto fini;
    }

    DEBUG("[voicecall_info] stop voicecall info session (session_index=%d)", vci->index);

    /* add total call time - TODO: 49.7 days limit */
    duration = (now - vci->StartTimeMonotonic + 999) / 1000;
    linestats_info.TotalCallTime += duration;
    DEBUG("[voicecall-session] sip call duration (index = %d, duration = %ld)", vci->index, duration);

    /* update line stats */
    linestats_info_update_rdb();

    /* update stop time */
    vci->StopTimeValid = 1;
    vci->StopTime = get_ctime_from_ts(ts);
    vci->StopTimeMonotonic = get_ms_from_ts(ts);

    vci->running = 0;

    voicecall_info_update(vci);

    _rdb_prefix_call(_rdb_set_int, "voicecall_session.index", vci->index);

    /* set end_of_call flag */
    _rdb_set_int("voicecall.statistics.end_of_call", 1);

fini:
    __noop();
    return;
}

void voicecall_info_update(struct voicecall_info_t *vci)
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    char voicecall_prefix[RDB_MAX_NAME_LEN];

    DEBUG("[voicecall] update voicecall info (session_index=%d)", vci->index);

    vci->update_timestamp = get_monotonic_ms();

    snprintf(voicecall_prefix, sizeof(voicecall_prefix), "voicecall_session.%d.", vci->index);

    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "start_time", vci->StartTime);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "start_time_monotonic", vci->StartTimeMonotonic);

    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "max_receive_interarrival_jitter", vci->MaxReceiveInterarrivalJitter);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "avg_receive_interarrival_jitter", vci->AvgReceiveInterarrivalJitter);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "inbound_total_rtp_packets", vci->InboundTotalRTPPackets);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "inbound_lost_rtp_packets", vci->InboundLostRTPPackets);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "inbound_dejitter_discarded_frames", vci->InboundDejitterDiscardedFrames);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "inbound_dejitter_discarded_rtp_packets", vci->InboundDejitterDiscardedRTPPackets);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "inbound_decoder_discarded_rtp_packets", vci->InboundDecoderDiscardedRTPPackets);

    _rdb_suffix_call(_rdb_set_str, voicecall_prefix, "last_call_number", vci->LastCallNumber);
    _rdb_suffix_call(_rdb_set_str, voicecall_prefix, "originating_uri", vci->OriginatingURI);
    _rdb_suffix_call(_rdb_set_str, voicecall_prefix, "terminating_uri", vci->TerminatingURI);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "payload_type", vci->PayloadType);
    _rdb_suffix_call(_rdb_set_str, voicecall_prefix, "codec_gsm_umts", vci->CodecGSMUMTS);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "outbound_total_rtp_packets", vci->OutboundTotalRTPPackets);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "inbound_cumulative_average_packet_size", vci->InboundCumulativeAveragePacketSize);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "outbound_cumulative_average_packet_size", vci->OutboundCumulativeAveragePacketSize);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "outbound_last_rtp_time", vci->OutboundLastRTPTime);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "inbound_last_rtp_time", vci->InboundLastRTPTime);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "outbound_last_rtp_tod", vci->OutboundLastRTPTod);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "inbound_last_rtp_tod", vci->InboundLastRTPTod);
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "sip_result_code", vci->SIPResultCode);
    _rdb_suffix_call(_rdb_set_str, voicecall_prefix, "call_direction", vci->CallDirection);

    /* AS Qualcomm does not have RTP latency, AvgRTPLatency contains RTT. To get average RTP latency, divide by 2 */
    if (vci->AvgRTPLatency.stat_valid)
        _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "avg_rtp_latency", __round(vci->AvgRTPLatency.avg / 2));
    else
        _rdb_suffix_call(_rdb_set_reset, voicecall_prefix, "avg_rtp_latency");

    if (vci->StopTimeValid) {
        _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "stop_time", vci->StopTime);
        _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "stop_time_monotonic", vci->StopTimeMonotonic);
    } else {
        _rdb_suffix_call(_rdb_set_reset, voicecall_prefix, "stop_time");
        _rdb_suffix_call(_rdb_set_reset, voicecall_prefix, "stop_time_monotonic");
    }

    _rdb_suffix_call(_rdb_set_str, voicecall_prefix, "state", vci->running ? "running" : "stopped");
    _rdb_suffix_call(_rdb_set_int, voicecall_prefix, "changed", 1);
}
#endif /* INCLUDE_IMS_VOIP_MSG */

/*
        ### rrc info functions ###
*/

void rrc_info_update(int enable_log)
{
    char rrc_prefix[RDB_MAX_NAME_LEN];
    char rrc_cell_prefix[RDB_MAX_NAME_LEN];
    char sys_net_stat_prefix[RDB_MAX_NAME_LEN];

    struct acc_t *RLCUpThroughput;
    struct acc_t *RLCUpRetrans;
    struct acc_t *RLCDownThroughput;
    struct acc_t *RLCDownRetrans;
    struct avg_t *PUSCHTxPower;
    struct avg_t *PUCCHTxPower;
    struct avg_t *PUSCHTxPowerByCell;
    struct avg_t *PUCCHTxPowerByCell;
    struct avg_t *SCellRSRP;
    struct avg_t *SCellRSRQ;
    struct avg_t *SCellRSSNR;
    struct avg_t *SCellRSSINR;
#ifdef USE_QDIAG_FOR_CQI
    struct avg_t *CQI;
#endif
    struct acc_t *PDCPTotalReceivedPDU;
    struct acc_t *PDCPBytesReceived;
    struct acc_t *PDCPBytesSent;
    struct acc_t *PDCPTotalLossSDU;

    time_t now;
    time_t tm;
    time_t start_time;
    time_t duration;

    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);
    int num_active_ul_carrier_cells = 0;
    int i;

    int r;

    int RRCReleaseCause;

    /* RRCReleaseCause */

    /*
        0x00 - CFG_FAILURE
        0x01 – HO_FAILURE
        0x02 – RLF
        0x03 – RACH_PROBLEM
        0x04 – MAX_RETRX
        0x05 – IP_CHECK_FAILURE
        0x06 – SIB_READ_FAILURE
        0x07 – OTHER_FAILURE
        0x08 –MAX
        0x09 – MIB_CHANGE
        0x0a – RLF_RF_UNAVAILABLE
    */

    const char *rrc_release_cause_names[] = {
        [0x00] = "drop", [0x01] = "drop",  [0x02] = "rlf",  [0x03] = "drop", [0x04] = "drop", [0x05] = "drop",
        [0x06] = "drop", [0x07] = "other", [0x08] = "drop", [0x09] = "drop", [0x0a] = "drop",
    };

    const char *rrc_release_cause_str;

    DEBUG("[rrc_info] update rrc info (session_index=%d)", rrc_info.index);

    /* get rrc prefix */
    snprintf(rrc_prefix, sizeof(rrc_prefix), "rrc_session.%d.", rrc_info.index);
    snprintf(sys_net_stat_prefix, sizeof(sys_net_stat_prefix), "system_network_status.%d.", rrc_info.index);

    /* get start time */
    now = get_monotonic_ms();
    tm = time(NULL);
    start_time = tm - (now - rrc_info.StartTimeMonotonic) / 1000;

    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "start_time", start_time);
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "cell_id", rrc_info.PCI);

    /* RLCUpThroughput */
    RLCUpThroughput = &rrc_info.RLCUpThroughput;
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_max_up_throughput", __round(RLCUpThroughput->max));
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_avg_up_throughput", __round(RLCUpThroughput->avg));
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_ul_bytes", RLCUpThroughput->diff);

    /* RLCDownThroughput */
    RLCDownThroughput = &rrc_info.RLCDownThroughput;
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_max_down_throughput", __round(RLCDownThroughput->max));
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_avg_down_throughput", __round(RLCDownThroughput->avg));
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_dl_bytes", RLCDownThroughput->diff);

#ifdef NR5G
    _rdb_set_uint("lte.rrc.session.rlc_ul_bytes", RLCUpThroughput->diff);
    _rdb_set_uint("lte.rrc.session.rlc_dl_bytes", RLCDownThroughput->diff);
#endif

    unsigned long long diff;
    unsigned long long diff2;

    /* get rlc retrans */
    RLCUpRetrans = &rrc_info.RLCUpRetrans;
    int rlcupretrans = 0;
    diff = acc_get_diff(RLCUpThroughput);
    if (diff)
        rlcupretrans = acc_get_diff(RLCUpRetrans) * 100 / diff;
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_retrans_rate_sent", rlcupretrans);

    RLCDownRetrans = &rrc_info.RLCDownRetrans;
    diff = acc_get_diff(RLCDownThroughput);
    int rlcdownretrans = 0;
    if (diff)
        rlcdownretrans = acc_get_diff(RLCDownRetrans) * 100 / diff;
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_retrans_rate_received", rlcdownretrans);

    /* get PDCP loss */
    PDCPTotalReceivedPDU = &rrc_info.PDCPTotalReceivedPDU;
    PDCPTotalLossSDU = &rrc_info.PDCPTotalLossSDU;
    PDCPBytesReceived = &rrc_info.PDCPBytesReceived;
    PDCPBytesSent = &rrc_info.PDCPBytesSent;

    diff = acc_get_diff(PDCPBytesReceived);
    _rdb_suffix_call(_rdb_set_uint, rrc_prefix, "pdcp_bytes_received", diff);
    DEBUG("pdcp_bytes_received diff = %llu acc = %llu, sacc = %llu", PDCPBytesReceived->diff, PDCPBytesReceived->acc, PDCPBytesReceived->sacc);

    diff = acc_get_diff(PDCPBytesSent);
    _rdb_suffix_call(_rdb_set_uint, rrc_prefix, "pdcp_bytes_sent", diff);
    DEBUG("pdcp_bytes_sent diff = %llu, acc = %llu, sacc = %llu", PDCPBytesSent->diff, PDCPBytesSent->acc, PDCPBytesSent->sacc);

    diff = acc_get_diff(PDCPTotalLossSDU);
    diff2 = acc_get_diff(PDCPTotalReceivedPDU);
    int pdcp_packetloss_rate_received = 0;
    if (diff2)
        pdcp_packetloss_rate_received = diff * 100 / diff2;
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "pdcp_packetloss_rate_received", pdcp_packetloss_rate_received);

    for (i = 0; i < LTE_MAX_NUMBER_OF_UPLINK_CARRIER_CELLS; i++) {
        if ((rrc_info.tx_power_carrier_idx_flags.active >> i) & 1) {
            num_active_ul_carrier_cells++;
            r = snprintf(rrc_cell_prefix, sizeof(rrc_cell_prefix), "%scell.%d.", rrc_prefix, i);
            if (r < 0)
                return; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */

            PUSCHTxPowerByCell = &rrc_info.PUSCHTxPowerByCell[i];
            if (PUSCHTxPowerByCell->stat_valid) {
                _rdb_suffix_call(_rdb_set_int, rrc_cell_prefix, "avg_pusch_tx_power", __round(PUSCHTxPowerByCell->avg));
                _rdb_suffix_call(_rdb_set_int, rrc_cell_prefix, "max_pusch_tx_power", __round(PUSCHTxPowerByCell->max));
            } else {
                _rdb_suffix_call(_rdb_set_reset, rrc_cell_prefix, "avg_pusch_tx_power");
                _rdb_suffix_call(_rdb_set_reset, rrc_cell_prefix, "max_pusch_tx_power");
            }

            PUCCHTxPowerByCell = &rrc_info.PUCCHTxPowerByCell[i];
            if (PUCCHTxPowerByCell->stat_valid) {
                _rdb_suffix_call(_rdb_set_int, rrc_cell_prefix, "avg_pucch_tx_power", __round(PUCCHTxPowerByCell->avg));
                _rdb_suffix_call(_rdb_set_int, rrc_cell_prefix, "max_pucch_tx_power", __round(PUCCHTxPowerByCell->max));
            } else {
                _rdb_suffix_call(_rdb_set_reset, rrc_cell_prefix, "avg_pucch_tx_power");
                _rdb_suffix_call(_rdb_set_reset, rrc_cell_prefix, "max_pucch_tx_power");
            }
        }
    }

    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "cell_count", num_active_ul_carrier_cells);

    for (i = 0; i < MAX_SERVER_CELLS_PER_MEASURING_PERIOD; i++) {
        r = snprintf(rrc_cell_prefix, sizeof(rrc_cell_prefix), "%scell.%d.", rrc_prefix, i);
        if (r < 0)
            return; /* GCC > 8.3 thinks the above may truncate. We need a check, even if it never fires. */
        if (rrc_info.AvgWidebandCQIByCell[i].stat_valid) {
            _rdb_suffix_call(_rdb_set_int, rrc_cell_prefix, "avg_cqi", __round(rrc_info.AvgWidebandCQIByCell[i].avg));
        } else {
            _rdb_suffix_call(_rdb_set_str, rrc_cell_prefix, "avg_cqi", "");
        }
    }

    PUSCHTxPower = &rrc_info.PUSCHTxPower;
    if (PUSCHTxPower->stat_valid) {
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "avg_pusch_tx_power", __round(PUSCHTxPower->avg));
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "max_pusch_tx_power", __round(PUSCHTxPower->max));
    } else {
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "avg_pusch_tx_power");
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "max_pusch_tx_power");
    }

    PUCCHTxPower = &rrc_info.PUCCHTxPower;
    if (PUCCHTxPower->stat_valid) {
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "avg_pucch_tx_power", __round(PUCCHTxPower->avg));
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "max_pucch_tx_power", __round(PUCCHTxPower->max));
    } else {
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "avg_pucch_tx_power");
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "max_pucch_tx_power");
    }

    _rdb_suffix_call(_rdb_set_str, rrc_prefix, "state", rrc_info.running ? "running" : "stopped");

    /* set durations */
    duration = now - rrc_info.StartTimeMonotonic;
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_ul_duration", duration);
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "rlc_dl_duration", duration);

    /* set rsrp */
    SCellRSRP = &rrc_info.SCellRSRP;
    if (SCellRSRP->stat_valid) {
        _rdb_suffix_call(_rdb_set_tenths_decimal, rrc_prefix, "scell_avg_rsrp", __dbx16(SCellRSRP->avg, -180));
        _rdb_suffix_call(_rdb_set_tenths_decimal, rrc_prefix, "scell_worst_rsrp", __dbx16(SCellRSRP->min, -180));
    } else {
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "scell_avg_rsrp");
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "scell_worst_rsrp");
    }

    /* set rsrq */
    SCellRSRQ = &rrc_info.SCellRSRQ;
    if (SCellRSRQ->stat_valid) {
        _rdb_suffix_call(_rdb_set_tenths_decimal, rrc_prefix, "scell_avg_rsrq", __dbx16(SCellRSRQ->avg, -30));
        _rdb_suffix_call(_rdb_set_tenths_decimal, rrc_prefix, "scell_worst_rsrq", __dbx16(SCellRSRQ->min, -30));
    } else {
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "scell_avg_rsrq");
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "scell_worst_rsrq");
    }

/* set cqi */
#ifdef USE_QDIAG_FOR_CQI
    CQI = &rrc_info.CQI;
    if (CQI->stat_valid)
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "avg_cqi", __round(CQI->avg));
    _rdb_suffix_call(_rdb_set_int, sys_net_stat_prefix, "avg_cqi", __round(CQI->avg));
    else _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "avg_cqi");
    _rdb_suffix_call(_rdb_set_reset, sys_net_stat_prefix, "avg_cqi");
#endif

    /* set rssnr */
    SCellRSSNR = &rrc_info.SCellRSSNR;
    if (SCellRSSNR->stat_valid) {
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "scell_avg_rssnr", __round(__dbx10(SCellRSSNR->avg, -20)));
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "scell_worst_rssnr", __round(__dbx10(SCellRSSNR->min, -20)));
    } else {
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "scell_avg_rssnr");
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "scell_worst_rssnr");
    }

    /* set rssinr */
    SCellRSSINR = &rrc_info.SCellRSSINR;
    if (SCellRSSINR->stat_valid) {
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "scell_avg_rssinr", __round(SCellRSSINR->avg));
        _rdb_suffix_call(_rdb_set_int, rrc_prefix, "scell_worst_rssinr", __round(SCellRSSINR->min));
    } else {
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "scell_avg_rssinr");
        _rdb_suffix_call(_rdb_set_reset, rrc_prefix, "scell_worst_rssinr");
    }

    /* set RRCReleaseCause */
    if (rrc_info.RRCReleaseCause < 0) {
        rrc_release_cause_str = "normal";
    } else {
        RRCReleaseCause = rrc_info.RRCReleaseCause; /* assume normal release if no mac reset happened */
        rrc_release_cause_str =
            (0 <= RRCReleaseCause && RRCReleaseCause <= __countof(rrc_release_cause_names)) ? rrc_release_cause_names[RRCReleaseCause] : "unknown";
    }
    _rdb_suffix_call(_rdb_set_str, rrc_prefix, "rrc_release_cause", rrc_release_cause_str);

    /* print debug log for better trouble-shotting */
    if (enable_log) {
        DEBUG("rrc_release_cause = '%s'", rrc_release_cause_str);
    }

    /* update change flag */
    rrc_info.update_timestamp = get_monotonic_ms();
    _rdb_suffix_call(_rdb_set_int, rrc_prefix, "changed", 1);
}

void rrc_info_start()
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);
    int i;

    DEBUG("[rrc_info] start rrc info session (session_index=%d)", rrc_info.index);

    /* update start time */
    rrc_info.StartTime = time(NULL);
    rrc_info.StartTimeMonotonic = get_monotonic_ms();

    rrc_info.RRCReleaseCause = -1;

    /* reset servcell info */
    memset(servcell_info.HarqDlAck_valid, 0, sizeof(servcell_info.HarqDlAck_valid));
    memset(servcell_info.HarqUlRetx_valid, 0, sizeof(servcell_info.HarqUlRetx_valid));
    memset(servcell_info.HarqUlLast, 0, sizeof(servcell_info.HarqUlLast));

    avg_reset(&rrc_info.SCellRSRP);
    avg_reset(&rrc_info.SCellRSRQ);
    avg_reset(&rrc_info.SCellRSSNR);
    avg_reset(&rrc_info.SCellRSSINR);

    acc_reset(&rrc_info.RLCUpThroughput);
    acc_reset(&rrc_info.RLCUpRetrans);
    acc_reset(&rrc_info.PDCPTotalLossSDU);
    acc_reset(&rrc_info.PDCPBytesReceived);
    acc_reset(&rrc_info.PDCPBytesSent);
    acc_reset(&rrc_info.PDCPTotalReceivedPDU);

    acc_reset(&rrc_info.RLCDownThroughput);
    acc_reset(&rrc_info.RLCDownRetrans);

    avg_reset(&rrc_info.PUSCHTxPower);
    avg_reset(&rrc_info.PUCCHTxPower);

    rrc_info.tx_power_carrier_idx_flags.active = 0;
    for (i = 0; i < LTE_MAX_NUMBER_OF_UPLINK_CARRIER_CELLS; i++) {
        avg_reset(&rrc_info.PUSCHTxPowerByCell[i]);
        avg_reset(&rrc_info.PUCCHTxPowerByCell[i]);
    }

#ifdef USE_QDIAG_FOR_CQI
    avg_reset(&rrc_info.CQI);
#endif

    for (i = 0; i < MAX_SERVER_CELLS_PER_MEASURING_PERIOD; i++) {
        avg_reset(&rrc_info.AvgWidebandCQIByCell[i]);
    }

    rrc_info.running = 1;

    rrc_info_update(0);
    _rdb_prefix_call(_rdb_set_int, "rrc_session.index", rrc_info.index);
}

void rrc_info_reset_rdb()
{
    reset_rdb_sets("rrc_session.");
}

int rrc_info_exec()
{
    /* bypass if not running */
    if (!rrc_info.running) {
        goto fini;
    }

    rrc_info_update(0);

    return 0;

fini:
    return -1;
}

void rrc_info_stop()
{
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    /* bypass if not running */
    if (!rrc_info.running) {
        VERBOSE("[rrc_info] not running, bypass stop procedure");
        goto fini;
    }

    DEBUG("[rrc_info] stop rrc info session (session_index=%d)", rrc_info.index);

    rrc_info.running = 0;

    rrc_info_update(1);
    _rdb_prefix_call(_rdb_set_int, "rrc_session.index", rrc_info.index);

    /* go next */
    rrc_info.index = (rrc_info.index + 1) % RRC_RDB_MAX_HISTORY;

fini:
    __noop();
    return;
}

void initiate_servcell_info(struct servcell_info_t *servcell, int init_size)
{
    int max_dl_retx;
    char var[RDB_MAX_NAME_LEN];
    int var_len = sizeof(var);

    /* initiate servcell info */
    memset(servcell, 0, sizeof(init_size));
    avg_init(&servcell->AvgWidebandCQI, "AvgWidebandCQI", 0);
    avg_init(&servcell->PathLoss, "PathLoss", 16);
    avg_init(&servcell->RRCEstabLatency, "RRCEstabLatency", 0);
    avg_init(&servcell->RRCReEstabLatency, "RRCReEstabLatency", 0);
    distr_init(&servcell->PUSCHTrasmitPower, "PUSCHTrasmitPower", 2);
    distr_init(&servcell->PMIDistribution, "PMIDistribution", 16);
    distr_init(&servcell->RIDistribution, "RIDistribution", 4);
    distr_init(&servcell->ReceivedModulationDistribution, "ReceivedModulationDistribution", RX_MOD_DISTR_NUM_TYPES);
    distr_init(&servcell->SendModulationDistribution, "SendModulationDistribution", 3);
    /* set ue tx power to impossible max */
    servcell->max_ue_tx_power = 40;
    avg_init(&servcell->SCellRSSNR, "SCellRSSNR", 1);
    avg_init(&servcell->SCellRSSINR, "SCellRSSINR", 1);
    avg_init(&servcell->SCellRSRP, "SCellRSRP", 0);
    avg_init(&servcell->SCellRSRQ, "SCellRSRQ", 0);

    /* default max retx - default max transmissions when network max harq transmission is not available */
    servcell->max_ul_harq_transmissions = 4;
    servcell->max_dl_harq_transmissions = 4;

    /* override max dl retx if the value is set in RDB */
    max_dl_retx = _rdb_prefix_call(_rdb_get_int, "servcell->max_dl_harq_transmissions");
    if (max_dl_retx > 0) {
        DEBUG("max retx count adjusted by RDB (max_dl_retx=%d)", max_dl_retx);
        servcell->max_dl_harq_transmissions = max_dl_retx;
    }
}

/*
        ### state maintain functions ###
*/

void init_stat()
{
    int i;

    /* ### global ### */

    /* initiate pusch tx power */
    avg_init(&total_pusch_tx_power, "total_pusch_tx_power", 480); /* last 480 moving average by WLL data model v1.0 */

    /* ### misc ### */

    /* init */
    misc_info_init();

    /* ### rrc ### */

    /* initiate rrc */
    memset(&rrc_info, 0, sizeof(rrc_info));
    avg_init(&rrc_info.SCellRSRP, "SCellRSRP", 0);
    avg_init(&rrc_info.SCellRSRQ, "SCellRSRQ", 0);
    avg_init(&rrc_info.SCellRSSNR, "SCellRSSNR", 0);
    avg_init(&rrc_info.SCellRSSINR, "SCellRSSINR", 0);
    acc_init(&rrc_info.RLCUpThroughput, "rlc_up_throughput");
    acc_init(&rrc_info.RLCUpRetrans, "rlc_up_retrans");
    acc_init(&rrc_info.RLCDownThroughput, "rlc_down_throughput");
    acc_init(&rrc_info.RLCDownRetrans, "rlc_down_retrans");
    acc_init(&rrc_info.PDCPTotalLossSDU, "pdcp_total_loss_sdu");
    acc_init(&rrc_info.PDCPBytesReceived, "pdcp_bytes_received");
    acc_init(&rrc_info.PDCPBytesSent, "pdcp_bytes_sent");
    acc_init(&rrc_info.PDCPTotalReceivedPDU, "pdcp_total_received_pdu");
    avg_init(&rrc_info.PUSCHTxPower, "pusch_tx_power", 0);
    avg_init(&rrc_info.PUCCHTxPower, "pucch_tx_power", 0);
    rrc_info.tx_power_carrier_idx_flags.active = 0;
    for (i = 0; i < LTE_MAX_NUMBER_OF_UPLINK_CARRIER_CELLS; i++) {
        avg_init(&rrc_info.PUSCHTxPowerByCell[i], "pusch_tx_power", 0);
        avg_init(&rrc_info.PUCCHTxPowerByCell[i], "pucch_tx_power", 0);
    }
#ifdef USE_QDIAG_FOR_CQI
    avg_init(&rrc_info.CQI, "cqi", 0);
#endif
    for (i = 0; i < MAX_SERVER_CELLS_PER_MEASURING_PERIOD; i++) {
        avg_init(&rrc_info.AvgWidebandCQIByCell[i], "AvgWidebandCQI", 0);
    }
    rrc_info.PCI = -1;
    /* reset rrc rdb */
    rrc_info_reset_rdb();

#ifdef INCLUDE_IMS_VOIP_MSG
    /* ### voicecall ### */
    {
        struct voicecall_info_t *e;
        struct list_head *p;

        /* initiate voicecall */
        list_for_each(p, &voicecall_info_online)
        {
            e = container_of(p, struct voicecall_info_t, list);
            avg_init(&e->AvgRTPLatency, "avg_rtp_latency", 0);
        }

        /* reset voicecall rdb */
        voicecall_info_reset_rdb();
    }
#endif /* INCLUDE_IMS_VOIP_MSG */

    /* ### servcell ### */

    /* setup constant rdb variables */
    servcell_info_init_rdb();

    /* initiate servcell_info */
    initiate_servcell_info(&servcell_info, sizeof(servcell_info));
#ifdef V_RF_MEASURE_REPORT_myna
    initiate_servcell_info(&accu_servcell_info, sizeof(accu_servcell_info));
#endif

    /* ### linestats ### */

#ifdef INCLUDE_IMS_VOIP_MSG
    /* initiate linestats */
    linestats_info_reset();
    linestats_info_read_rdb();
#endif /* INCLUDE_IMS_VOIP_MSG */
}

void fini_stat()
{
#ifdef INCLUDE_IMS_VOIP_MSG
    /* update linestats for the last time before terminating */
    linestats_info_update_rdb();
#endif /* INCLUDE_IMS_VOIP_MSG */

    /* finish pusch tx power */
    distr_fini(&servcell_info.PUSCHTrasmitPower);
    distr_fini(&servcell_info.PMIDistribution);
    distr_fini(&servcell_info.RIDistribution);
    distr_fini(&servcell_info.ReceivedModulationDistribution);
    distr_fini(&servcell_info.SendModulationDistribution);

    /* fini misc */
    misc_info_fini();
}

/*
        ### Qualcomm diag functions - including log and event primary handlers ###
*/

static void process_dci_app_stream(struct app_t *app, int code, unsigned char *ptr, int len)
{
    int index;
    struct app_info_t **info_ptr;

    index = hash_get_hashed_val(code);

    /* process up to the tail */
    info_ptr = &app->hashed_app_info[index];
    while (*info_ptr) {
        if ((*info_ptr)->code == code)
            break;
        info_ptr = &(*info_ptr)->next;
    }

    /* call handler */
    if (*info_ptr) {
#ifdef DM_PACKET_INTERVAL_MS
        uint8 process = 0;
        time_t ms = get_monotonic_ms();
        if ((*info_ptr)->nolimit) {
            process = 1;
            VERBOSE("nolimit pkt rcv, process (app_name=%s,code=%d,hexcode=0x%04x,len=%d)", app->name, code, code, len);
        } else if (ms - (*info_ptr)->last_time_ms > DM_PACKET_INTERVAL_MS) {
            process = 1;
            VERBOSE("curr time %ld, last time %ld, interval %ld > %d : process 0x%04x packet)", ms, (*info_ptr)->last_time_ms,
                    ms - (*info_ptr)->last_time_ms, DM_PACKET_INTERVAL_MS, code);
            (*info_ptr)->last_time_ms = ms;
        } else {
            VERBOSE("curr time %ld, last time %ld, interval %ld <= %d : skip 0x%04x packet)", ms, (*info_ptr)->last_time_ms,
                    ms - (*info_ptr)->last_time_ms, DM_PACKET_INTERVAL_MS, code);
        }
        if (process) {
            VERBOSE("call log handler (app_name=%s,code=%d,hexcode=0x%04x,len=%d,handler='%s')", app->name, code, code, len, (*info_ptr)->name);
            (*info_ptr)->func(*info_ptr, ptr, len);
        }
#else  /* DM_PACKET_INTERVAL_MS */
        VERBOSE("call log handler (app_name=%s,code=%d,hexcode=0x%04x,len=%d,handler='%s')", app->name, code, code, len, (*info_ptr)->name);
        (*info_ptr)->func(*info_ptr, ptr, len);
#endif /* DM_PACKET_INTERVAL_MS */
    } else {
        VERBOSE("got unknown code (app_name=%s,code=%d,hexcode=0x%04x,len=%d)", app->name, code, code, len);
    }
}

static void process_dci_log_stream(unsigned char *ptr, int len)
{
    struct app_info_t __attrib_unused__ *app_info = NULL;
    declare_app_func_define_vars_for_log();

    process_dci_app_stream(&diag_stat.log_app, hdr->code, ptr, len);
}

static void process_dci_event_stream(unsigned char *ptr, int len)
{
    struct app_info_t __attrib_unused__ *app_info = NULL;
    declare_app_func_define_vars_for_event();

    process_dci_app_stream(&diag_stat.event_app, id_type->id, ptr, len);
}

/* Signal handler that handles the change in DCI channel */
static void notify_handler(int signal, siginfo_t *info, void *unused)
{
    (void)unused;

    if (info) {
        int err;
        diag_dci_peripherals list = 0;

        VERBOSE("diag: In %s, signal %d received from kernel, data is: %x\n", __func__, signal, info->si_int);

        if (info->si_int & DIAG_STATUS_OPEN) {
            if (info->si_int & DIAG_CON_MPSS) {
                VERBOSE("diag: DIAG_STATUS_OPEN on DIAG_CON_MPSS\n");
            } else if (info->si_int & DIAG_CON_LPASS) {
                VERBOSE("diag: DIAG_STATUS_OPEN on DIAG_CON_LPASS\n");
            } else {
                VERBOSE("diag: DIAG_STATUS_OPEN on unknown peripheral\n");
            }
        } else if (info->si_int & DIAG_STATUS_CLOSED) {
            if (info->si_int & DIAG_CON_MPSS) {
                VERBOSE("diag: DIAG_STATUS_CLOSED on DIAG_CON_MPSS\n");
            } else if (info->si_int & DIAG_CON_LPASS) {
                VERBOSE("diag: DIAG_STATUS_CLOSED on DIAG_CON_LPASS\n");
            } else {
                VERBOSE("diag: DIAG_STATUS_CLOSED on unknown peripheral\n");
            }
        }
        err = diag_get_dci_support_list_proc(MSM, &list);
        if (err != DIAG_DCI_NO_ERROR) {
            VERBOSE("diag: could not get support list, err: %d\n", err);
        }
        /* This will print out all peripherals supporting DCI */
        if (list & DIAG_CON_MPSS)
            VERBOSE("diag: Modem supports DCI\n");
        if (list & DIAG_CON_LPASS)
            VERBOSE("diag: LPASS supports DCI\n");
        if (list & DIAG_CON_WCNSS)
            VERBOSE("diag: RIVA supports DCI\n");
        if (list & DIAG_CON_APSS)
            VERBOSE("diag: APSS supports DCI\n");
        if (!list)
            VERBOSE("diag: No current dci support\n");
    } else {
        VERBOSE("diag: In %s, signal %d received from kernel, but no info value, info: 0x%p\n", __func__, signal, info);
    }
}

/* Signal Handler that will be fired when we receive DCI data */
static void dci_data_handler(int signal)
{
    (void)signal;

    /* Do something here when you receive DCI data. */

    /* This is usually for holding wakelocks when the
      clients are running in Diag Non Real Time mode
      or when they know the Apps processor is in deep
      sleep but they still need to process DCI data.

      Please Note: Wakelocks must be released
      after processing the data in the respective
      response/log/event handler. Failure to do so
      will affect the power consumption of the Apps
      processor.*/

    VERBOSE("got signal");
}

static void app_fini(struct app_t *app)
{
    free(app->app_code);
    free(app->app_code2);
    free(app->hashed_app_info);

    app->app_code = NULL;
    app->app_code2 = NULL;
    app->hashed_app_info = NULL;
}

static int app_init(struct app_t *app, const char *name, struct app_info_t *app_info, int app_info_count)
{
    uint16 *code;
    int *code2;
    struct app_info_t *info;
    struct app_info_t **info_ptr;
    int index;
    int i;
    time_t last_ms;

    app->name = name;

    INFO("allocate apps (name=%s)", app->name);
    app->app_code_count = app_info_count;
    app->app_code = calloc(app->app_code_count, sizeof(*app->app_code));
    app->app_code2 = calloc(app->app_code_count, sizeof(*app->app_code2));
    if (!app->app_code || !app->app_code2) {
        ERR("failed to allocate memory for array apps (name=%s,count=%d)", app->name, app->app_code_count);
        goto err;
    }

    INFO("build apps (name=%s,count=%d)", app->name, app->app_code_count);
    info = app_info;
    code = app->app_code;
    code2 = app->app_code2;
    for (i = 0; i < app->app_code_count; i++) {
        *code++ = info->code;
        *code2++ = info->code;

        info++;
    }

    INFO("allocate hashed index (name=%s)", app->name);
    app->hashed_app_info = calloc(1 << hash_get_hash_bit(), sizeof(*app->hashed_app_info));
    if (!app->hashed_app_info) {
        ERR("failed to allocate hashed log info (name=%s)", app->name);
        goto err;
    }

    INFO("initiate app info (name=%s)", app->name);
#ifdef DM_PACKET_INTERVAL_MS
    last_ms = get_monotonic_ms() - DM_PACKET_INTERVAL_MS;
#else
    last_ms = get_monotonic_ms();
#endif
    info = app_info;
    for (i = 0; i < app->app_code_count; i++) {
        info->last_time_ms = last_ms;
        info++->next = NULL;
    }

    INFO("build hashed index");
    info = app_info;
    for (i = 0; i < app->app_code_count; i++) {
        index = hash_get_hashed_val(info->code);

        DEBUG("convert index, code(%d,0x%04x) ==> hash(0x%04x) #%d/%d", info->code, info->code, index, i + 1, app->app_code_count);

        /* process up to the tail */
        info_ptr = &(app->hashed_app_info[index]);
        while (*info_ptr)
            info_ptr = &(*info_ptr)->next;

        *info_ptr = info++;
    }

    return 0;

err:
    return -1;
}

static void _diag_fini()
{
    struct sigaction notify_action;
    struct sigaction dci_data_action;

    if (diag_stat.flag_eventapp) {
        INFO("disable event");
        diag_disable_all_events(diag_stat.client_id);
    }

    if (diag_stat.flag_logapp) {
        INFO("disable log");
        diag_disable_all_logs(diag_stat.client_id);
    }

    if (diag_stat.flag_sig_reg) {
        INFO("deregister data signal");
        diag_deregister_dci_signal_data(diag_stat.client_id);
    }

    if (diag_stat.flag_reg) {
        INFO("release dci client");
        diag_release_dci_client(&diag_stat.client_id);
    }

    if (diag_stat.flag_init) {
        INFO("deinit diag lsm");
        Diag_LSM_DeInit();
    }

    INFO("free resource");
    free(diag_stat.dci_health_stats);

    app_fini(&diag_stat.event_app);
    app_fini(&diag_stat.log_app);

    INFO("restore notify signal handler");
    sigemptyset(&notify_action.sa_mask);
    dci_data_action.sa_handler = SIG_DFL;
    sigaction(diag_stat.noti_signal, &notify_action, NULL);

    INFO("restore data signal handler");
    sigemptyset(&dci_data_action.sa_mask);
    dci_data_action.sa_handler = SIG_DFL;
    sigaction(diag_stat.data_signal, &dci_data_action, NULL);
}

static int _diag_init()
{
    int res;

    diag_dci_peripherals list = DIAG_CON_MPSS | DIAG_CON_APSS | DIAG_CON_LPASS;

    struct sigaction notify_action;
    struct sigaction dci_data_action;

    /* init diag */
    INFO("initiate diag struct");
    memset(&diag_stat, 0, sizeof(diag_stat));
    diag_stat.channel = MSM;
    diag_stat.noti_signal = SIGCONT;
    diag_stat.data_signal = SIGRTMIN + 15;

    app_init(&diag_stat.log_app, "log", log_app_info, __countof(log_app_info));
    app_init(&diag_stat.event_app, "event", event_app_info, __countof(event_app_info));

    INFO("initiate notify signal handler");
    if (sigemptyset(&notify_action.sa_mask) < 0) {
        ERR("sigemptyset(&notify_action.sa_mask) failed");
        goto err;
    }
    notify_action.sa_sigaction = notify_handler;
    notify_action.sa_flags = SA_SIGINFO;
    if (sigaction(diag_stat.noti_signal, &notify_action, NULL) < 0) {
        ERR("sigaction(diag_stat.noti_signal, &notify_action, NULL) failed");
        goto err;
    }

    INFO("initiate data signal handler");
    if (sigemptyset(&dci_data_action.sa_mask) < 0) {
        ERR("sigemptyset(&dci_data_action.sa_mask) failed");
        goto err;
    }
    dci_data_action.sa_handler = dci_data_handler;
    dci_data_action.sa_flags = 0;
    if (sigaction(diag_stat.data_signal, &dci_data_action, NULL) < 0) {
        ERR("sigaction(diag_stat.data_signal, &dci_data_action, NULL) failed");
    }

    INFO("allocate health stats");
    diag_stat.dci_health_stats = calloc(1, sizeof(*diag_stat.dci_health_stats));
    if (!diag_stat.dci_health_stats) {
        ERR("failed to allocate dci health stats");
        goto err;
    }

    INFO("initiate libdiag");
    if (!Diag_LSM_Init(NULL)) {
        ERR("failed to initiate libdiag");
        goto err;
    }
    diag_stat.flag_init = 1;

    INFO("register with DCI");
    res = diag_register_dci_client(&diag_stat.client_id, &list, diag_stat.channel, &diag_stat.noti_signal);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to register with DCI (res=%d,errno=%d,str='%s')", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_reg = 1;
    INFO("DCI registered (client_id=%d)", diag_stat.client_id);

    INFO("register signal");
    res = diag_register_dci_signal_data(diag_stat.client_id, diag_stat.data_signal);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to register signal (err=%d,errno=%d,str='%s')", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_sig_reg = 1;

    /* Getting supported Peripherals list*/
    int err;
    static int channel = MSM;

    // Depending on timing of qdiag launching time and modem power up, qdiag event is blocked
    // so qdiagd can't receive any event from the modem nor may miss SIB1 message so can't
    // construct ECGI information. To minimize the event loss/block issue, wait until
    // essential service (MPSS & APSS) available here.
    INFO("* DCI Status on Processors:");
    while (1) {
        err = diag_get_dci_support_list_proc(channel, &list);
        if (err != DIAG_DCI_NO_ERROR) {
            ERR("could not get support list, err: %d, errno: %d", err, errno);
            goto err;
        }
        INFO("MPSS:  %s", (list & DIAG_CON_MPSS) ? "UP" : "DOWN");
        INFO("LPASS: %s", (list & DIAG_CON_LPASS) ? "UP" : "DOWN");
        INFO("WCNSS: %s", (list & DIAG_CON_WCNSS) ? "UP" : "DOWN");
        INFO("APSS:  %s", (list & DIAG_CON_APSS) ? "UP" : "DOWN");
        if ((list & DIAG_CON_MPSS) && (list & DIAG_CON_APSS)) {
            break;
        }
        if (!(list & DIAG_CON_MPSS)) {
            WARN("MPSS is down, wait & retry");
        }
        if (!(list & DIAG_CON_APSS)) {
            WARN("APSS is down, wait & retry");
        }
        sleep(1);
    }
    INFO("register stream proc");
    res = diag_register_dci_stream_proc(diag_stat.client_id, process_dci_log_stream, process_dci_event_stream);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to register stream proc (err=%d,errno=%d,str='%s')", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_proc_reg = 1;

    INFO("enable log apps (count=%d)", diag_stat.log_app.app_code_count);
    res = diag_log_stream_config(diag_stat.client_id, ENABLE, diag_stat.log_app.app_code, diag_stat.log_app.app_code_count);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to enable log stream proc (err=%d,errno=%d,str='%s')", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_logapp = 1;

    INFO("enable event apps (count=%d)", diag_stat.event_app.app_code_count);
    res = diag_event_stream_config(diag_stat.client_id, ENABLE, diag_stat.event_app.app_code2, diag_stat.event_app.app_code_count);
    if (res != DIAG_DCI_NO_ERROR) {
        ERR("failed to enable event stream proc (err=%d,errno=%d,str='%s')", res, errno, strerror(errno));
        goto err;
    }
    diag_stat.flag_eventapp = 1;

    return 0;
err:
    _diag_fini();
    return -1;
}

static int _diag_check_health()
{
    int res;

    struct diag_dci_health_stats *dci_health_stats;

    dci_health_stats = diag_stat.dci_health_stats;

    res = diag_get_health_stats_proc(diag_stat.client_id, dci_health_stats, DIAG_APPS_PROC);

    if (res == DIAG_DCI_NO_ERROR) {
        INFO("Log Drop Count for Modem:\t%d\n", dci_health_stats->dropped_logs);
        INFO("Log Total Count for Modem:\t%d\n", dci_health_stats->received_logs);
        INFO("Event Drop Count for Modem:\t%d\n", dci_health_stats->dropped_events);
        INFO("Event Total Count for Modem:\t%d\n", dci_health_stats->received_events);
    } else {
        INFO("Error in collecting statistics, err: %d, errno: %d\n", res, errno);
    }

    return res;
}

static void sig_handler(int sig)
{
    /* VERBOSE only for debug - not safe functions for signals */

    switch (sig) {
        case SIGTERM:
            _term = sig;
            VERBOSE("SIGTERM caught, terminate");
            break;

        case SIGINT:
            _term = sig;
            VERBOSE("SIGINT caught, terminate");
            break;

        case SIGSEGV:
            _term = sig;
            VERBOSE("SIGSEGV caught, terminate");
            exit(-1);
            break;

        default:
            VERBOSE("sig(%d) caught", sig);
            break;
    }
}

static void usage(FILE *fd)
{
    fprintf(fd,

            "\n"
            "Usage: qdiag [options]\n"
            "\n"
            "	Options:\n"
            "		-h print this usage screen\n"
            "\n");
}

void mcs_index_crc_fail_reset()
{
    memset(servcell_info.McsIndexCount, 0, sizeof(servcell_info.McsIndexCount));
    memset(servcell_info.CrcFailCount, 0, sizeof(servcell_info.CrcFailCount));
}

void cqi_pmi_reset()
{
#ifdef INCLUDE_LTE_CQI_MSG
    memset(servcell_info.CqiStatsCount, 0, sizeof(servcell_info.CqiStatsCount));
    memset(servcell_info.PmiStatsCount, 0, sizeof(servcell_info.PmiStatsCount));
#endif // INCLUDE_LTE_CQI_MSG
}

void process_ca_ind_msg(unsigned long freq_pci_state, time_t event_time)
{
    unsigned int freq, pci, state, i;
    int newCellFlag = 1;

    freq = freq_pci_state >> 12;
    pci = (freq_pci_state >> 2) & 0x3ff;
    state = freq_pci_state & 3;

    for (i = 0; i < caUsage.usedNumCaCells; i++) {
        if ((freq == caUsage.caCell[i].freq) && (pci == caUsage.caCell[i].pci)) {
            caUsage.currentCaCellIndex = i;
            newCellFlag = 0;
            break;
        }
    }

    if (newCellFlag) {
        // state should be CONFIGURED_DEACTIVATED for new cell
        if ((caUsage.usedNumCaCells < MAX_NUM_CA_CELLS) && (state == SCELL_STATE_CONFIGURED_DEACTIVATED)) {
            caUsage.currentCaCellIndex = caUsage.usedNumCaCells;
            caUsage.usedNumCaCells++;
            caUsage.caCell[caUsage.currentCaCellIndex].freq = freq;
            caUsage.caCell[caUsage.currentCaCellIndex].pci = pci;
            caUsage.caCell[caUsage.currentCaCellIndex].index = CA_INDEX_SCC_ONLY;
            caUsage.caCell[caUsage.currentCaCellIndex].configStartTime = event_time;
            caUsage.ccConfiguredIndex[caUsage.numCcConfigured] = caUsage.currentCaCellIndex;

            if (caUsage.numCcConfigured < CA_STATE_MAX_NUM) {
                caUsage.numCcConfigured++;
            }
        }
    } else {
        switch (state) {
            case SCELL_STATE_DECONFIGURED:
                if (caUsage.caCell[caUsage.currentCaCellIndex].previousState == SCELL_STATE_CONFIGURED_ACTIVATED) {
                    caUsage.caCell[caUsage.currentCaCellIndex].activatedTime += event_time - caUsage.caCell[caUsage.currentCaCellIndex].actStartTime;
                    decrease_pcc_ca_state(event_time);
                }

                caUsage.caCell[caUsage.currentCaCellIndex].configuredTime += event_time - caUsage.caCell[caUsage.currentCaCellIndex].configStartTime;

                if (caUsage.numCcConfigured > CA_STATE_DISCONNECTED) {
                    caUsage.numCcConfigured--;
                }

                break;

            case SCELL_STATE_CONFIGURED_DEACTIVATED:
                if (caUsage.caCell[caUsage.currentCaCellIndex].previousState == SCELL_STATE_CONFIGURED_ACTIVATED) {
                    caUsage.caCell[caUsage.currentCaCellIndex].activatedTime += event_time - caUsage.caCell[caUsage.currentCaCellIndex].actStartTime;
                    decrease_pcc_ca_state(event_time);
                } else { // previousState == SCELL_STATE_DECONFIGURED
                    caUsage.caCell[caUsage.currentCaCellIndex].configStartTime = event_time;
                    caUsage.ccConfiguredIndex[caUsage.numCcConfigured] = caUsage.currentCaCellIndex;

                    if (caUsage.numCcConfigured < CA_STATE_MAX_NUM) {
                        caUsage.numCcConfigured++;
                    }
                }

                break;

            case SCELL_STATE_CONFIGURED_ACTIVATED:
                caUsage.caCell[caUsage.currentCaCellIndex].actStartTime = event_time;
                increase_pcc_ca_state(event_time);

                break;
        }
    }

    caUsage.caCell[caUsage.currentCaCellIndex].previousState = state;
}

/*
        ### rdb notification
*/

int process_rdb_event(char *rdb)
{
#ifdef INCLUDE_IMS_VOIP_MSG
    /* reset statistics */
    if (!strcmp(rdb, "voicecall.statistics.reset")) {
        if (_rdb_get_int(rdb)) {
            DEBUG("got voice call statistics reset command");
            linestats_info_reset();

            _rdb_set_int(rdb, 0);
        }
    }
#endif /* INCLUDE_IMS_VOIP_MSG */
#if defined SAVE_RFQ_EARFCN_LIST
    if (!strcmp(rdb, RFQ_EARFCN_RDB)) {
        init_internal_list_per_rfq_earfcn();
    }
#endif /* SAVE_RFQ_EARFCN_LIST */

    /* reset mcs index and crc fail count */
    if (!strcmp(rdb, "wwan.0.servcell_info.reset_mcs_data")) {
        if (_rdb_get_int(rdb)) {
            mcs_index_crc_fail_reset();
            _rdb_set_int(rdb, 0);
        }
    }

    /* reset cqi and pmi */
    if (!strcmp(rdb, "wwan.0.servcell_info.reset_cqi_pmi_data")) {
        if (_rdb_get_int(rdb)) {
            cqi_pmi_reset();
            _rdb_set_int(rdb, 0);
        }
    }

    /* ca indication message */
    if (!strcmp(rdb, "wwan.0.radio_stack.e_utra_measurement_report.ca_ind.freq_pci_state")) {
        process_ca_ind_msg(_rdb_get_int(rdb), get_monotonic_ms());
    }

    return 0;
}

/*
 Read RDB value as double.

 Parameters:
  name : RDB variable name.
  f : pointer to return the result double.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int sample_rdb(const char *name, double *f)
{
    char str[RDB_MAX_NAME_LEN];
    char val[RDB_MAX_VAL_LEN];
    int val_len = sizeof(val);
    int valid;

    snprintf(str, sizeof(str), "%s%s", _rdb_prefix, name);

    /* get rssinr from RDB */
    _rdb_get_str_quiet(val, val_len, str);
    valid = *val;

    /* check validation */
    if (!valid || (1 != sscanf(val, "%lf", f)))
        goto err;

    return 0;
err:
    return -1;
}

/*
 Update RS-SINR in RRC information.

 Parameters:
  None.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int sample_rssinr_for_rrc()
{
    double sinr;

    /* bypass if not running */
    if (!rrc_info.running) {
        goto err;
    }

    /* sinr */
    if (sample_rdb("signal.rssinr", &sinr) < 0) {
        DEBUG("[interval] no RSSINR available");
    } else {
        /* feed into rrc and servcell info */
        avg_feed(&rrc_info.SCellRSSINR, sinr);

        DEBUG("[interval] sample RSSINR for RRC (rssinr=%.2lf,avg=%.2Lf)", sinr, rrc_info.SCellRSSINR.avg);
    }

    return 0;

err:
    return -1;
}

/*
 Update RS-SINR, RSRP and RSRQ in Serving cell information structures.

 Parameters:
  None.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int sample_rssinr_rsrp_and_rsrq()
{
    double sinr;
    double rsrp;
    double rsrq;

    /* sinr */
    if (sample_rdb("signal.rssinr", &sinr) < 0) {
        DEBUG("[interval] no RSSINR available");
    } else {
        /* feed into servcell info */
        avg_feed(&servcell_info.SCellRSSINR, sinr);

        DEBUG("[interval] sample RSSINR (rssinr=%.2lf,avg=%.2Lf)", sinr, servcell_info.SCellRSSINR.avg);
    }

    /* rsrp */
    if (sample_rdb("signal.0.rsrp", &rsrp) < 0) {
        DEBUG("[interval] no RSRP available");
    } else {
        avg_feed(&servcell_info.SCellRSRP, rsrp);
        DEBUG("[interval] sample RSRP (rsrq=%.2lf,avg=%.2Lf)", rsrp, servcell_info.SCellRSRP.avg);
    }

    /* rsrq */
    if (sample_rdb("signal.rsrq", &rsrq) < 0) {
        DEBUG("[interval] no RSRQ available");
    } else {
        avg_feed(&servcell_info.SCellRSRQ, rsrq);
        DEBUG("[interval] sample RSRQ (rsrq=%.2lf,avg=%.2Lf)", rsrq, servcell_info.SCellRSRQ.avg);
    }

    return 0;
}

int check_if_qc_diag_thread_is_running()
{
    return pthread_kill(read_thread_hdl, 0) == 0;
}

int post_nas_ota_msg_to_pdn_mgr(const unsigned long long *ms, const char *logpx, int msg_type, const char *msg_type_str,
                                const struct pdn_entity_t *bearer_info)
{
    int nas_to_pdn_tbl[] = {
        [EMM_ATTACH_REQUEST] = PDN_EVENT_ATTACH_REQUEST,
        [EMM_ATTACH_REJECT] = PDN_EVENT_ATTACH_REJECT,
        [EMM_ATTACH_ACCEPT] = PDN_EVENT_ATTACH_ACCEPT,
        [EMM_ATTACH_COMPLETE] = PDN_EVENT_ATTACH_COMPLETE,

        [EMM_DETACH_REQUEST] = PDN_EVENT_DETACH_REQUEST,
        [EMM_DETACH_ACCEPT] = PDN_EVENT_DETACH_ACCEPT,

        [ESM_INFORMATION_RESPONSE] = PDN_EVENT_UPDATE,

        [ESM_PDN_CONNECTIVITY_REQUEST] = PDN_EVENT_PRECONNECT_REQUEST,
        [ESM_PDN_CONNECTIVITY_REJECT] = PDN_EVENT_PRECONNECT_REJECT,
        [ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST] = PDN_EVENT_CONNECT_REQUEST,
        [ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REJECT] = PDN_EVENT_CONNECT_REJECT,
        [ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT] = PDN_EVENT_CONNECT_ACCEPT,

        [ESM_PDN_DISCONNECT_REQUEST] = PDN_EVENT_PREDISCONNECT_REQUEST,
        [ESM_PDN_DISCONNECT_REJECT] = PDN_EVENT_PREDISCONNECT_REJECT,
        [ESM_DEACTIVATE_EPS_BEARER_CONTEXT_REQUEST] = PDN_EVENT_DISCONNECT_REQUEST,
        [ESM_DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT] = PDN_EVENT_DISCONNECT_ACCEPT,

        [ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST] = PDN_EVENT_CONNECT_REQUEST,
        [ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REJECT] = PDN_EVENT_CONNECT_REJECT,
        [ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_ACCEPT] = PDN_EVENT_CONNECT_ACCEPT,
    };

    const char *pdn_event_str;
    int *pdn_event = &nas_to_pdn_tbl[msg_type];
    int rc;

    if (!__is_in_boundary(pdn_event, nas_to_pdn_tbl, sizeof(nas_to_pdn_tbl)) || !*pdn_event) {
        DEBUG("[%s] no corresponding PDN event found (msg_type=%d)", logpx, msg_type);
        goto err;
    }

    pdn_event_str = pdn_mgr_get_event_name(*pdn_event);

    DEBUG("%s post PDN event '%s' ==> '%s' (ebi=%d,pti=%d,apn='%s')", logpx, msg_type_str, pdn_event_str, bearer_info->ebi, bearer_info->pti,
          bearer_info->apn);
    rc = pdn_mgr_post_event(ms, *pdn_event, bearer_info);

    return rc;
err:
    return -1;
}

int lte_nas_parser_on_nas_ota_message(const unsigned long long *ms, const char *logpx, int msg_type, const char *msg_type_str, int ebi, int pti,
                                      void *data, int len)
{
    struct pdn_entity_t bearer_info;

    /* reset and initiate members in bearer information */
    memset(&bearer_info, 0, sizeof(bearer_info));
    bearer_info.ebi = ebi;
    bearer_info.pti = pti;

#ifdef TEST_MODE_BEARER_REJECT
#warning !!! NETWORK SIMULATION MODE FOR TEST !!!
    struct esm_status esm_stat;
    int override_msg_type = 0;

    switch (msg_type) {
        /* to test reject cases, enable each of following cases at a time */
        case ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT:
            override_msg_type = ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REJECT;
            esm_stat.esm_cause = ESM_CAUSE_INSUFFICIENT_RESOURCES;
            break;

        case ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_ACCEPT:
            override_msg_type = ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REJECT;
            esm_stat.esm_cause = ESM_CAUSE_INSUFFICIENT_RESOURCES;
            break;

        case ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST:
            override_msg_type = ESM_PDN_CONNECTIVITY_REJECT;
            esm_stat.esm_cause = ESM_CAUSE_INSUFFICIENT_RESOURCES;
            break;

        case ESM_DEACTIVATE_EPS_BEARER_CONTEXT_REQUEST:
            override_msg_type = ESM_PDN_DISCONNECT_REJECT;
            esm_stat.esm_cause = ESM_CAUSE_LAST_PDN_DISCONNECTION_NOT_ALLOWED;
            break;
    }

    /* override nas message */
    if (override_msg_type) {
        ERR("%s [%s] !!! BEARER TEST !!! replace '%s' with '%s'", logpx, msg_type_str, lte_nas_parser_get_nas_msg_type_name_str(msg_type),
            lte_nas_parser_get_nas_msg_type_name_str(override_msg_type));

        msg_type = override_msg_type;
        esm_stat.esm_cause = ESM_CAUSE_LAST_PDN_DISCONNECTION_NOT_ALLOWED;

        data = &esm_stat;
        len = sizeof(esm_stat);
    }

#endif

    /* pre-process EMM and esm request to convert to PDN event */
    switch (msg_type) {
        case ESM_PDN_CONNECTIVITY_REJECT:                        /* BS >>> UE */
        case ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REJECT:     /* BS >>> UE */
        case ESM_PDN_DISCONNECT_REJECT:                          /* BS >>> UE */
        case ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REJECT: { /* BS <<< UE */
            int rc_esm_cause;
            int esm_cause;

            rc_esm_cause = lte_nas_parser_pdu_get_esm_cause(data, len, &esm_cause);

            if (rc_esm_cause < 0) {
                DEBUG("%s [%s] failed to get ESM cause", logpx, msg_type_str);
            } else {
                NOTICE("%s [%s] esm reject cause '%s' %d (ref. 3GPP 24.301 9.9.4.4)", logpx, msg_type_str,
                       lte_nas_parser_get_esm_cause_name_str(esm_cause), esm_cause);
                bearer_info.esm_cause = esm_cause;
                if (msg_type == ESM_PDN_CONNECTIVITY_REJECT && esm_cause == 55) {
                    /* cause 55, Multiple PDN connections for a given APN not allowed. see MAG-1308 */
                    static char *recovery_cmd = "qmisys lowpower; sleep 1;qmisys online";
                    int rc = system(recovery_cmd);
                    NOTICE("recovery action(%s) initiated, rc=%d", recovery_cmd, rc);
                }
            }
            break;
        }

        case EMM_SERVICE_REJECT: {
            int emm_cause;

            if (lte_nas_parser_pdu_get_cause_from_serv_rej(data, len, &emm_cause) < 0) {
                DEBUG("%s [%s] failed to get EMM cause", logpx, msg_type_str);
            } else {
                NOTICE("%s [%s] EMM reject cause '%s' #%d (ref. 3GPP 24.301 9.9.3.9)", logpx, msg_type_str,
                       lte_nas_parser_get_emm_cause_name_str(emm_cause), emm_cause);
            }
            break;
        }

        case EMM_ATTACH_REJECT: {
            int emm_cause;

            if (lte_nas_parser_pdu_get_cause_from_att_rej(data, len, &emm_cause) < 0) {
                DEBUG("%s [%s] failed to get EMM cause", logpx, msg_type_str);
            } else {
                NOTICE("%s [%s] EMM reject cause '%s' #%d (ref. 3GPP 24.301 9.9.3.9)", logpx, msg_type_str,
                       lte_nas_parser_get_emm_cause_name_str(emm_cause), emm_cause);
                bearer_info.emm_cause = emm_cause;
            }
            break;
        }

        case ESM_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST: { /* BS >>> UE */
            int rc_linked_ebi = -1;
            int linked_ebi;

            bearer_info.ebt = PDN_TYPE_DEDICATED;

            rc_linked_ebi = lte_nas_parser_pdu_get_linked_ebi_from_act_ded_req(data, len, &linked_ebi);

            if (rc_linked_ebi < 0) {
                ERR("%s [%s] no linked ebi found", logpx, msg_type_str);
                goto err;
            }

            NOTICE("%s [%s] linked ebi found (linked_ebi=%d)", logpx, msg_type_str, linked_ebi);
            bearer_info.default_bearer_ebi = linked_ebi;
            break;
        }
        case ESM_PDN_CONNECTIVITY_REQUEST:                      /* BS <<< UE */
        case ESM_INFORMATION_RESPONSE:                          /* BS <<< UE */
        case ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST: { /* BS >>> UE */
            int rc_apn = -1;

            char apn[MAX_APN_BUFFER_SIZE];

            bearer_info.ebt = PDN_TYPE_DEFAULT;

            if (msg_type == ESM_PDN_CONNECTIVITY_REQUEST)
                rc_apn = lte_nas_parser_pdu_get_apn_from_conn_req(data, len, apn, sizeof(apn));
            else if (msg_type == ESM_INFORMATION_RESPONSE)
                rc_apn = lte_nas_parser_pdu_get_apn_from_info_resp(data, len, apn, sizeof(apn));
            else if (msg_type == ESM_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST) {
                rc_apn = lte_nas_parser_pdu_get_apn_from_bearer_req(data, len, apn, sizeof(apn));
            }

            /* get APN if possible */
            if (rc_apn < 0) {
                DEBUG("%s [%s] no APN found", logpx, msg_type_str);
            } else {
                bearer_info.apn = apn;
                DEBUG("%s [%s] APN found (apn='%s')", logpx, msg_type_str, apn);
            }
            break;
        }

        case ESM_PDN_DISCONNECT_REQUEST: { /* BS <<< UE */
            int linked_ebi;
            int rc_ebi;

            rc_ebi = lte_nas_parser_pdu_get_ebi_from_disconn_req(data, len, &linked_ebi);
            if (rc_ebi < 0) {
                ERR("%s [%s] no linked EBI found", logpx, msg_type_str);
                goto err;
            }

            DEBUG("%s [%s] linked EBI found (ebi=%d)", logpx, msg_type_str, linked_ebi);
            bearer_info.ebi = linked_ebi;
            break;
        }
    }

    /* post-process EMM and esm request - convert and post to PDN manager */
    return post_nas_ota_msg_to_pdn_mgr(ms, logpx, msg_type, msg_type_str, &bearer_info);

err:
    return -1;
}

#if defined SAVE_RFQ_EARFCN_LIST
/*
 * Initialise internal structures per EARFCN list in RF qualification mode
 */
void init_internal_list_per_rfq_earfcn()
{
    char buffer[RDB_MAX_VAL_LEN];
    char *pstring, *token;
    char *endptr;
    long int val;

    manual_cell_meas_earfcn_count = 0;
    _rdb_get_str(buffer, RDB_MAX_VAL_LEN, RFQ_EARFCN_RDB);
    pstring = buffer;
    /* Extract existing earfcn rdb values into an array */
    while (manual_cell_meas_earfcn_count < MAX_EARFCN_ENTRIES && (token = strsep(&pstring, ",")) != NULL) {
        if (*token == 0)
            continue;
        val = strtol(token, &endptr, 10);
        if (*endptr == '\0' && val <= 65535 && val >= 0) {
            manual_cell_meas_earfcns[manual_cell_meas_earfcn_count++] = (unsigned int)val;
        } else {
            ERR("Invalid EARFCN entry: %s", token);
        }
    }
}
#endif /* SAVE_RFQ_EARFCN_LIST */

/*
        ### main ###
*/

int main(int argc, char *argv[])
{
    int opt;

    int rdb_hndl;
    int nfds;

    int rdb_interval; /* rdb write interval */
    int rdb_update_int; /* real rdb update interval */
    int first = 1;
    int servcell_sampling_interval;
    int rrc_rdb_update_interval;

    time_t ms; /* msec tick count */
    time_t tm;
    time_t ms_last_sinr;
    time_t ms_last_rrc_sinr;
    time_t ms_last_interval;
#if !defined SAVE_AVERAGED_DATA
#define NBN_RF_MEASURE_INTERVAL (10 * 1000) // 10 seconds
    time_t ms_last_measure;
#endif
#ifdef NR5G
    time_t ms_last_nr5g_status;
#endif

    struct dbenum_t *dbenum;

    while ((opt = getopt(argc, argv, "i:h?")) != EOF) {
        switch (opt) {
            case 'i':
                _inst = atoi(optarg);
                break;

            case 'h':
                usage(stdout);
                exit(0);

            case ':':
                fprintf(stderr, "missing argument - %c\n", opt);
                usage(stderr);
                exit(-1);

            case '?':
                fprintf(stderr, "unknown option - %c\n", opt);
                usage(stderr);
                exit(-1);

            default:
                usage(stderr);
                exit(-1);
                break;
        }
    }

#ifdef TEST_MODE_MODULE
    INFO("check UPER decode engine");
    perform_per_decode_verification();
    unit_test_gcell();
#endif

    INFO("get rdb prefix");
    snprintf(_rdb_prefix, sizeof(_rdb_prefix), "wwan.%d.", _inst);

    INFO("initiate signal handlers");
    signal(SIGCHLD, sig_handler);
    signal(SIGHUP, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGPIPE, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGSEGV, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR1, sig_handler);

    INFO("initiate RDB");
    rdb_init();

    /*
        ### read qdiagd config rdb ###
    */

    /* read rdb update interval */
    rdb_interval = _rdb_get_int("qdiagd.config.rdb_update_interval");
    if (!rdb_interval)
        rdb_interval = 15 * 60 * 1000; /* default interval - every 15 minutes */
    rdb_update_int = rdb_interval;

    /* read servcell sampling interval */
    servcell_sampling_interval = _rdb_get_int("qdiagd.config.servcell_sampling_interval");
    if (!servcell_sampling_interval)
        servcell_sampling_interval = 30 * 1000; /* default sampling interval - every 30 second */

    /* read rrc update interval */
    rrc_rdb_update_interval = _rdb_get_int("qdiagd.config.rrc_rdb_update_interval");
    if (!rrc_rdb_update_interval)
        rrc_rdb_update_interval = 10 * 1000; /* default rrc update interval - default 10 second*/

    /* log qdiagd config rdbs */
    INFO("rdb update interval is %d [qdiagd.config.rdb_update_interval]", rdb_interval);
    INFO("servcell sampling interval is %d [qdiagd.config.servcell_sampling_interval]", servcell_sampling_interval);
    INFO("RRC RDB update interval is %d [qdiagd.config.rrc_rdb_update_interval]", rrc_rdb_update_interval);

    INFO("initiate PDN manager");
    pdn_mgr_init();
    pdn_mgr_set_event(eps_bearer_rdb_on_pdn_mgr_event);

    INFO("initiate LTE NAS parser");
    lte_nas_parser_init();
    lte_nas_parser_set_callback(lte_nas_parser_on_nas_ota_message);

    INFO("initiate esp bearer RDB");
    eps_bearer_rdb_init();

    /* get rdb fd */
    nfds = rdb_hndl = rdb_fd(_s);

    /* create dbenum */
    DEBUG("create dbenum");
    dbenum = dbenum_create(_s, TRIGGERED);

    __rdb_subscribe("voicecall.statistics.reset");
    __rdb_subscribe("wwan.0.servcell_info.reset_mcs_data");
    __rdb_subscribe("wwan.0.servcell_info.reset_cqi_pmi_data");
    __rdb_subscribe("wwan.0.radio_stack.e_utra_measurement_report.ca_ind.freq_pci_state");

#ifdef INCLUDE_IMS_VOIP_MSG
    INFO("initiate vci");
    vci_init();
#endif /* INCLUDE_IMS_VOIP_MSG */

    INFO("initiate stat");
    init_stat();

    INFO("initiate diag");
    if (_diag_init() < 0) {
        ERR("failed to initiate diag");
        goto fini;
    }

    /* initiate last update timestamps - initial values */
    ms = get_monotonic_ms();
    tm = time(NULL);
    ms_last_sinr = ms;
    ms_last_rrc_sinr = ms;
    ms_last_interval = ms;
#if !defined SAVE_AVERAGED_DATA
    ms_last_measure = ms;
#endif
#ifdef NR5G
    ms_last_nr5g_status = ms;
#endif

    /* reset servcell and eps bearer RDB */
    servcell_info_reset(&servcell_info, sizeof(servcell_info), tm, ms);
    eps_bearer_rdb_reset_all_sess(tm, ms);

#if defined SAVE_RFQ_EARFCN_LIST
    init_internal_list_per_rfq_earfcn();
    __rdb_subscribe(RFQ_EARFCN_RDB);
#endif /* SAVE_RFQ_EARFCN_LIST */

#ifdef V_RF_MEASURE_REPORT_myna
    servcell_info_reset(&accu_servcell_info, sizeof(accu_servcell_info), tm, ms);
    servcell_info_by_id_init();
#endif

    struct timeval tv;
    int stat;
    fd_set readfds;
    int total_triggers;

    INFO("start select loop");
    while (!_term) {
        tv.tv_sec = 0;
        tv.tv_usec = 500 * 1000; /* every 500 ms - not to miss any RS-SINR RDB update */

        /* initiate fds */
        FD_ZERO(&readfds);
        FD_SET(rdb_hndl, &readfds);

        stat = select(nfds + 1, &readfds, NULL, NULL, &tv);
        if (stat < 0) {

            /* continue if interrupted by signal */

            if (errno == EINTR) {
                DEBUG("select punk!");
                continue;
            }

            ERR("select failed (error=%d,str=%s)", errno, strerror(errno));
            break;

        } else if (stat == 0) {
            /* timeout - nothing to do */
        } else {
            /* process if rdb is triggered */
            if (FD_ISSET(rdb_hndl, &readfds)) {
                struct dbenumitem_t *item;

                DEBUG("rdb triggered");

                total_triggers = dbenum_enumDb(dbenum);
                if (total_triggers <= 0) {
                    DEBUG("triggered but no flaged rdb variable found - %s", strerror(errno));
                } else {
                    DEBUG("walk through triggered rdb");

                    item = dbenum_findFirst(dbenum);
                    while (item) {
                        DEBUG("triggered (rdb=%s)", item->szName);

                        process_rdb_event(item->szName);
                        item = dbenum_findNext(dbenum);
                    }
                }
            }
        }

        /* get current */
        tm = time(NULL);
        ms = get_monotonic_ms();

        /*
                ### process interval procedures ###
        */

#define IF_IT_IS_EXPIRED(l, i) if ((ms - (l)) >= (i))
#define UPDATE_EXPIRY(i) i = ms

        rdb_enter_csection();
        {
            /* read sinr from RDB and feed into rrc */
            IF_IT_IS_EXPIRED(ms_last_rrc_sinr, servcell_sampling_interval)
            {
                if (!(sample_rssinr_for_rrc() < 0)) {
                    DEBUG("[interval] * sinr updated for rrc");
                    UPDATE_EXPIRY(ms_last_rrc_sinr);
                }
            }

            /* read sinr from RDB and feed into serving cell information */
            IF_IT_IS_EXPIRED(ms_last_sinr, servcell_sampling_interval)
            {
                DEBUG("[interval] * sample signal strength values");
                sample_rssinr_rsrp_and_rsrq();
                UPDATE_EXPIRY(ms_last_sinr);
            }

            /* update rrc information in RDB */
            IF_IT_IS_EXPIRED(rrc_info.update_timestamp, rrc_rdb_update_interval)
            {
                if (!(rrc_info_exec() < 0)) {
                    DEBUG("[interval] * RRC information updated");
                    UPDATE_EXPIRY(rrc_info.update_timestamp);
                }
            }

            /* update serving cell and bearer RDB */
/* A workaround to update serving cell information quickly after
 * booting by setting short interval for 1st serving cell information
 * update.
 */
#ifdef V_RF_MEASURE_REPORT_myna
            if (!accu_servcell_info.AvgWidebandCQI.stat_valid &&
#else
            if (!servcell_info.AvgWidebandCQI.stat_valid &&
#endif
                first) {
                rdb_update_int = rdb_interval/10;
            }
            IF_IT_IS_EXPIRED(ms_last_interval, rdb_update_int)
            {
                first = 0;
                rdb_update_int = rdb_interval;
                DEBUG("[interval] * update Serving cell information RDB");
#ifdef V_RF_MEASURE_REPORT_myna
                /* end of measuring period, update with final set of data
                 * if cell_based_data_collection() is never called during measuring period then
                 * need to call here at lease once so servcell info could refect to
                 * cell_info_by_id
                 */
                int network_pci = _rdb_get_int("wwan.0.system_network_status.PCID");
                if (rrc_info.PCI != -1) {
                    cell_based_data_collection(rrc_info.PCI);
                } else if (network_pci) {
                    cell_based_data_collection(network_pci);
                }
                if (g_servcell_info_acc_count == 0) {
                    sumup_servcell_info();
                }
                servcell_info_update(&accu_servcell_info, tm, ms);
                servcell_info_reset(&accu_servcell_info, sizeof(accu_servcell_info), tm, ms);
                g_servcell_info_acc_count = 0;
                servcell_info_by_id_reset();
#else
                servcell_info_update(&servcell_info, tm, ms);
#endif
                /* reset servcell info */
                servcell_info_reset(&servcell_info, sizeof(servcell_info), tm, ms);

                DEBUG("[interval] * update PDP context RDB");
                eps_bearer_rdb_perform_periodic_flush(tm, ms);
                eps_bearer_rdb_reset_all_sess(tm, ms);
                UPDATE_EXPIRY(ms_last_interval);

                DEBUG("[interval] * set trigger RDB");
                _rdb_set_int("qdiagd.trigger.interval", 1);
            }

#if !defined SAVE_AVERAGED_DATA
            /* Update RDBs every 10 seconds for 30 seconds measure & report requirement.
             */
            IF_IT_IS_EXPIRED(ms_last_measure, NBN_RF_MEASURE_INTERVAL)
            {
                DEBUG("[interval] * NBN 30s measurement & report parameters updated");
                update_measure_params();
                UPDATE_EXPIRY(ms_last_measure);
            }
#endif

#ifdef NR5G
#define NR5G_STATUS_CHECK_INTERVAL      (10 * 1000)    // 10 seconds
#define NR5G_MAX_CNT_FOR_STATUS_RESET   3
            IF_IT_IS_EXPIRED(ms_last_nr5g_status, NR5G_STATUS_CHECK_INTERVAL)
            {
                /* When missing event 3191 but receiving nr5g_mac_pdsch_stats_log then
                   NR5G status is set to UP manually. If there is no nr5g_mac_pdsch_stats_log
                   message nor event 3191 for given period (10s * 3 = 30s by default) then
                   reset the NR5G status to DOWN here to synchronise RDB variable with
                   network status. */
                if (assumptive_nr5g_up_cnt > 0) {
                    assumptive_nr5g_up_cnt++;
                    DEBUG("[interval] * NR5G %ds status check period, increase counter to %d", NR5G_STATUS_CHECK_INTERVAL, assumptive_nr5g_up_cnt);
                    UPDATE_EXPIRY(ms_last_nr5g_status);
                    if (assumptive_nr5g_up_cnt > NR5G_MAX_CNT_FOR_STATUS_RESET) {
                        DEBUG("[NR5G] NR5G_STATUS_CHECK_INTERVAL, reached max count, reset ssumptive_nr5g_up_cnt 0, set radio_stack.nr5g.up to DOWN");
                        assumptive_nr5g_up_cnt = 0;
                        _rdb_set_str("wwan.0.radio_stack.nr5g.up", "DOWN");
                    }
                }
            }
#endif

            rdb_leave_csection();
        }

        /*
                watchdog for QC diag thread

                This workaround to recover unknown terminations of QC diag thread.
         */

        if (!check_if_qc_diag_thread_is_running()) {
            ERR("!!!! QC read diag thread disappeared, immediately exit !!!!");
            break;
        }
    }

    DEBUG("terminating by %d", _term);

    INFO("check health");
    _diag_check_health();

fini:
    INFO("finalize diag");
    _diag_fini();

#ifdef INCLUDE_IMS_VOIP_MSG
    INFO("finalize vci");
    vci_fini();
#endif /* INCLUDE_IMS_VOIP_MSG */

    INFO("finalize stat");
    fini_stat();

    INFO("delete dbenum");
    dbenum_destroy(dbenum);

    INFO("finalize esp bearer RDB");
    eps_bearer_rdb_fini();

    INFO("finalize LTE NAS parser");
    lte_nas_parser_fini();

    INFO("finalize PDN manager");
    pdn_mgr_fini();

    INFO("finalize rdb");

    rdb_fini();

    return 0;
}
