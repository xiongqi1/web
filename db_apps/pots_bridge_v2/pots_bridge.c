/*
 * pots_bridge is the main module for POTS bridge daemon.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE

#include "call_prefix.h"
#include "call_track.h"
#include "callback_timer.h"
#include "dialplan.h"
#include "fsm.h"
#include "indexed_rdb.h"
#include "netcomm_proslic.h"
#include "pbx_common.h"
#include "pbx_config.h"
#include "proslic.h"
#include "strarg.h"
#include "utils.h"
#include "softtonegen.h"
#include "ezrdb.h"
#include "call_history.h"
#include "block_calls.h"
#include <regex.h>
#include <dbenum.h>
#include <errno.h>
#include <rdb_ops.h>
#include <si3217x.h>
#include <si_voice.h>
#include <si_voice_datatypes.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <syslog.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include FXS_CONSTANTS_HDR

///////////////////////////////////////////////////////////////////////////////
// pre-compile defines
///////////////////////////////////////////////////////////////////////////////

/* config defines */
#define CONFIG_ENABLE_DTMF_LONG_PRESS_VOICEMAIL
//#define CONFIG_SHORT_DTMF_DETECT

/* mute TX DTMF by SLIC - workaround until Qualcomm modem provides DTMF mute */
#define CONFIG_MUTE_TX_DTMF
/* use OOB DTMF with QMI signaling */
#define CONFIG_TX_OOB_DTMF
//#define CONFIG_DEBUG_VBAT
#define CONFIG_USE_IP_CALLS

//#define CONFIG_FUTURE_FEATURE

/* debug defines */
//#define DEBUG_SIMULATE_CALL
//#define DEBUG_SIMULATE_MWI
//#define DEBUG_SIMULATE_CWI
//#define DEBUG_DUMP_REGS

/* fake call debug */
#ifdef DEBUG_SIMULATE_CALL
#define DEBUG_SIMULATE_CALL_RING_INIT_DELAY SEC_TO_MSEC(10)
#define DEBUG_SIMULATE_CALL_RING_DURATION SEC_TO_MSEC(15)
#endif

/* fake mwi debug */
#ifdef DEBUG_SIMULATE_MWI
#define DEBUG_SIMULATE_MWI_INTERVAL	SEC_TO_MSEC(15)
#endif

#ifdef DEBUG_SIMULATE_CWI
#define DEBUG_SIMULATE_CWI_INTERVAL	SEC_TO_MSEC(3)
#endif

/* ProSLIC macros */

#define proslic_delay(cptr,delay) \
    (cptr)->deviceId->ctrlInterface->Delay_fptr(cptr->deviceId->ctrlInterface->hTimer,delay)

#define proslic_sleep(cptr, sleep_msec) \
    (cptr->deviceId->ctrlInterface->Delay_fptr(cptr->deviceId->ctrlInterface->hTimer, (sleep_msec)))

///////////////////////////////////////////////////////////////////////////////
// delays and durations
///////////////////////////////////////////////////////////////////////////////

#define SEC_TO_MSEC(SEC) ((SEC)*1000)

/*
	///////////////////////////////////////////////////////////////////////////////
	//  caller ID hardware buffer sizes
	///////////////////////////////////////////////////////////////////////////////
*/

/* SLIC physical hardware buffer size */
#define FSK_CALLER_ID_HW_FIFO_SIZE 8
/* under-run threshold bytes that SLIC generates FSK interrupt */
#define FSK_CALLER_ID_UNDERRUN_THRESHOLD 4
/* FSK minimum write size that POTS bridge writes every FSK interrupt */
#define FSK_CALLER_ID_MIN_WRITE_SIZE (FSK_CALLER_ID_HW_FIFO_SIZE-FSK_CALLER_ID_UNDERRUN_THRESHOLD)

#if FSK_CALLER_ID_MIN_WRITE_SIZE < FSK_CALLER_ID_UNDERRUN_THRESHOLD
#error FSK_CALLER_ID_MIN_WRITE_SIZE is required to be bigger than FSK_CALLER_ID_UNDERRUN_THRESHOLD
#endif

/* SLIC FSK BPS 1200 BPS but we are using the worst measured FSK BPS which include any over-head (860 BPS) */
#define FSK_CALLER_ID_BPS 860
/* 10 bit ( 8 bit data + start and stop bit ) per byte */
#define FSK_BYTE_TO_MSEC(b) (((b)*1000*8+FSK_CALLER_ID_BPS-1)/FSK_CALLER_ID_BPS)
#define FSK_MSEC_TO_BYTE(msec) (((int)msec)*FSK_CALLER_ID_BPS/1000/8)

/*
	///////////////////////////////////////////////////////////////////////////////
	//  delay values
	///////////////////////////////////////////////////////////////////////////////

	* FSK states and delays

	--> ring 1 --> INIT_DELAY --> preamble --> PREAMBLE_DELAY --> caller ID --> FINI_DELAY --> ring 2

		note:
		FSK time parameters are originally copied from ProSLIC examples after cross-checking by Googling.
*/

/* delay after ring 1 */
#define FSK_CALLER_ID_INIT_DELAY 500
/* delay after preamble */
#define FSK_CALLER_ID_PREAMBLE_DELAY (133 + FSK_BYTE_TO_MSEC(FSK_CALLER_ID_HW_FIFO_SIZE))
/* delay after caller ID */
#define FSK_CALLER_ID_FINI_DELAY 2000
/* FINI_DELAY for in-call caller ID */
#define FSK_CALLER_ID_FINI_DELAY2 (120 + FSK_BYTE_TO_MSEC(FSK_CALLER_ID_HW_FIFO_SIZE))
/* FINI_DELAY for visual mwi */
#define FSK_CALLER_ID_FINI_DELAY3 FSK_BYTE_TO_MSEC(FSK_CALLER_ID_HW_FIFO_SIZE)
/* number of caller ID preamble bytes */
#define FSK_CALLER_ID_PREAMBLE_COUNT 30

/* RWPIPE IPC default timeout */
#define RWPIPE_DEFAULT_TIMEOUT 10
/* RWPIPE IPC outgoing call timeout */
#define RWPIPE_OUTGOING_CALL_TIMEOUT 30

#define RWPIPE_DEFAULT_RDB_CMD_TIMEOUT 30
/* timeout for response of outgoing call */
#define TIMEOUT_PLACE_CALL SEC_TO_MSEC(30)

/* RDB */
#define RDB_VOICE_COMMAND_CTRL "wwan.0.voice.command.ctrl"
#define RDB_VOICE_COMMAND_NOTI "wwan.0.voice.command.noti"
#define RDB_SYSTEM_MODE "wwan.0.system_network_status.system_mode"
#define RDB_MBDN "wwan.0.sim.data.mbdn"
#define RDB_DIALPLAN "pbx.dialplan.pots_bridge."
#define RDB_TEST_MODE "pbx.test_mode"
/*
	RDB_MODEM_TIMEOFFSET_IN_HOUR is [MDM system UTC time] based. So, the value is
	offset (distance) from [MDM system UTC time] to [Real world local time].
	(RDB_MODEM_TIMEOFFSET_IN_HOUR = [Real world local time] - [MDM system UTC time])
*/
#define RDB_MODEM_TIMEOFFSET_IN_HOUR "wwan.0.networktime.timezone"
/*
	RDB_IPQ_TIMEOFFSET_IN_SEC is [Real world local time] based. So, the value is
	offset (distance) from [Real world local time] to [MDM system UTC time].
	(RDB_IPQ_TIMEOFFSET_IN_SEC = [MDM system UTC time] - [Real world local time])

	!! negative offset compared to RDB_MODEM_TIMEOFFSET_IN_HOUR !!
*/
#define RDB_IPQ_TIMEOFFSET_IN_SEC "system.ipq.timeoffset"

#define RDB_VOICE_MGMT_COMMAND_CTRL "wwan.0.voice.mgmt.ctrl"

#define RDB_VOICEMAIL_ACTIVE "wwan.0.mwi.voicemail.active"
#define RDB_VOICEMAIL_COUNT "wwan.0.mwi.voicemail.count"

#define VOICE_CALL_CALL_STATUS_PREFIX "voice_call.call_status"
#define VOICE_CALL_OUTGOING_CALL_COUNT "voice_call.current_outgoing_call_count"
#define VOICE_CALL_INCOMING_CALL_COUNT "voice_call.current_incoming_call_count"
#define VOICE_CALL_TOTAL_CALL_COUNT "voice_call.current_call_count"

/* misc. delays and durations */

/* in-call digit expire time */
#define TIMEOUT_INCALL_SINGLE_DIGIT SEC_TO_MSEC(5)
/* incoming call delay to collect caller ID */
#define DELAY_COLLECT_CALLER_ID 1000
/* supplementary confirmation tone period */
#define DURATION_SUPPL_TONE 500
/* supplementary silence period between confirmation and reorder tone */
#define DURATION_SUPPL_SILENCE SEC_TO_MSEC(15)
/* hook-flash maximum duration */
#define DURATION_MINMUM_HOOKFLASH 800
/* select waiting period */
#define DURATION_SELECT 5
/* dialtone timeout - timeout for reorder */
#define TIMEOUT_DIALTONE SEC_TO_MSEC(15)

/* dialtone stutter duration */
#define DURATION_STUTTER SEC_TO_MSEC(5)

/* when RDB hangup does not genereate QMI notificaiton */
#define TIMEOUT_MSEC_FOR_DISCONNECT  SEC_TO_MSEC(30)

/* digit collect timeout - timeout for dialing */
#define TIMEOUT_INTERDIGIT_LONG SEC_TO_MSEC(8)
#define TIMEOUT_INTERDIGIT_SHORT SEC_TO_MSEC(3)
/* ROH timeout */
#define TIMEOUT_ROH SEC_TO_MSEC(15)
#define TIMEOUT_PREREORDER SEC_TO_MSEC(20)

/* call waiting id ACK timeout */
#define TIMEOUT_CWID_ACK 500

#ifdef CONFIG_ENABLE_DTMF_LONG_PRESS_VOICEMAIL
#define DURATION_DTMF_LONG_PRESS 2000
#endif

/* dialplan instant dial priority */
#define DIALPLAN_DIAL_PRIORITY_IMMEDIATELY 100
#define DIALPLAN_DIAL_PRIORITY_SHORT 300

/* random values that are written to REG and RAM SLIC */
#define SLIC_STAT_MONITOR_REGDATA1 0
#define SLIC_STAT_MONITOR_REGDATA2 0x88
#define SLIC_STAT_MONITOR_RAMDATA1 0
#define SLIC_STAT_MONITOR_RAMDATA2 0x12345678L

///////////////////////////////////////////////////////////////////////////////
// finite-state machine defines
///////////////////////////////////////////////////////////////////////////////

/* FXS events */
enum fxs_event_t {
    fxs_event_none = 0,

    /* slic events */
    fxs_event_hookflash,
    fxs_event_onhook,
    fxs_event_offhook,
    fxs_event_digit,
    fxs_event_digit_pressed,
    fxs_event_digit_timeout_long,
    fxs_event_digit_timeout_short,
    fxs_event_stutter_timeout,
    fxs_event_place_call,
    fxs_event_place_call_timeout,

    fxs_event_suppl,
    fxs_event_set_tty_mode,
    fxs_event_suppl_ack,
    fxs_event_suppl_stop_tone,
    fxs_event_suppl_done,

    /* modem events */
    fxs_event_ring,
    fxs_event_ring_with_cid,
    fxs_event_mwi,
    fxs_event_fgcall_disconnected,
    fxs_event_disconnected,
    fxs_event_disconnect_timeout,
    fxs_event_progressed,
    fxs_event_connected,
    fxs_event_fgcall_changed,
    fxs_event_ringback_remote,
    fxs_event_ringback_local,
    fxs_event_stop_ringing,
    fxs_event_waiting,
    fxs_event_waiting_with_cid,
    fxs_event_service_stat_chg,
    fxs_event_voicemail_stat_chg,
    fxs_event_rx_dtmf,

    /* interrupt events */
    fxs_event_interrupt_fskbuf,
    fxs_event_interrupt_ring,

    /* timer events */
    fxs_event_fsk_init_delay_expired,
    fxs_event_fsk_caller_id_preamble_delay_expired,
    fxs_event_fsk_fini_delay_expired,
    fxs_event_roh_timeout,
    fxs_event_prereorder_timeout,

    fxs_event_wcid_sas_expired,
    fxs_event_wcid_cas_expired,
    fxs_event_wcid_ack_expired,
    fxs_event_wcid_ack_received,

    fxs_event_dial_failure,

    fxs_event_force_connect,

    fxs_event_last,
};

/* FXS event names */
char* fxs_event_names[] = {
    [fxs_event_none] = "none",
    [fxs_event_ring] = "ring",
    [fxs_event_ring_with_cid] = "ring_with_cid",
    [fxs_event_onhook] = "onhook",
    [fxs_event_offhook] = "offhook",
    [fxs_event_digit] = "digit",
    [fxs_event_digit_pressed] = "digit_pressed",
    [fxs_event_digit_timeout_long] = "digit_timeout_long",
    [fxs_event_digit_timeout_short] = "digit_timeout_short",
    [fxs_event_stutter_timeout] = "stutter_timeout",
    [fxs_event_mwi] = "mwi",
    [fxs_event_fsk_init_delay_expired] = "fsk_init_delay_expired",
    [fxs_event_fsk_caller_id_preamble_delay_expired] = "fsk_preamble_delay_expired",
    [fxs_event_fsk_fini_delay_expired] = "fini_delay_expired",
    [fxs_event_interrupt_fskbuf] = "interrupt_fskbuf",
    [fxs_event_interrupt_ring] = "interrupt_ring",
    [fxs_event_fgcall_disconnected] = "fgcall_disconnected",
    [fxs_event_disconnected] = "disconnected",
    [fxs_event_disconnect_timeout] = "disconnect_timeout",
    [fxs_event_place_call] = "place_call",
    [fxs_event_progressed] = "progressed",
    [fxs_event_connected] = "connected",
    [fxs_event_hookflash] = "hookflash",
    [fxs_event_ringback_remote] = "ringback remote",
    [fxs_event_ringback_local] = "ringback local",
    [fxs_event_fgcall_changed] = "fgcall_changed",
    [fxs_event_stop_ringing] = "stop_ringing",
    [fxs_event_waiting] = "waiting",
    [fxs_event_waiting_with_cid] = "waiting_with_cid",
    [fxs_event_suppl] = "suppl",
    [fxs_event_set_tty_mode] = "set_tty_mode",
    [fxs_event_suppl_ack] = "suppl_ack",
    [fxs_event_suppl_done] = "suppl_done",
    [fxs_event_suppl_stop_tone] = "suppl_stop_tone",
    [fxs_event_roh_timeout] = "roh_timeout",
    [fxs_event_service_stat_chg] = "service_stat_chg",
    [fxs_event_voicemail_stat_chg] = "voicemail_stat_chg",
    [fxs_event_rx_dtmf] = "rx_dtmf",
    [fxs_event_place_call_timeout] = "place_call_timeout",
    [fxs_event_wcid_sas_expired] = "wcid_sas_expired",
    [fxs_event_wcid_cas_expired] = "wcid_cas_expired",
    [fxs_event_wcid_ack_expired] = "wcid_ack_expired",
    [fxs_event_wcid_ack_received] = "wid_ack_received",
    [fxs_event_dial_failure] = "dial_failure",
    [fxs_event_prereorder_timeout] = "reorder_timeout",

    [fxs_event_force_connect] = "force_connect"
};

/* FXS states */
enum fxs_state_t {
    fxs_state_none = 0,

    fxs_state_init,

    fxs_state_idle,

    /* MO call states */
    fxs_state_dialtone,
    fxs_state_digit_collect,
    fxs_state_place_call,
    fxs_state_ringback_remote,
    fxs_state_ringback_local,

    /* MT call states */
    fxs_state_ringing_1st,
    fxs_state_fsk_preamble_vmwi,
    fxs_state_fsk_preamble_caller_id,
    fxs_state_fsk_preamble_wcaller_id,
    fxs_state_fsk_cid,
    fxs_state_fsk_wcid,
    fxs_state_ringing_2nd,
    fxs_state_ringing,
    fxs_state_answer,

    fxs_state_wcid_init,
    fxs_state_wcid_wait_ack,

    /* mwi */
    fxs_state_init_mwi,
    fxs_state_fsk_vmwi,

    /* common states */
    fxs_state_connected,
    fxs_state_disconnect,
    fxs_state_prereorder,
    fxs_state_reorder,

    fxs_state_suppl,

    /* test mode states */
    fxs_state_force_connected,

    fxs_state_last,
};

/* FXS state names */
char* fxs_state_names[] = {
    [fxs_state_none] = "none",
    [fxs_state_init] = "init", /* not used */
    [fxs_state_idle] = "idle",
    [fxs_state_dialtone] = "dialtone",
    [fxs_state_digit_collect] = "collect",
    [fxs_state_place_call] = "place_call",
    [fxs_state_prereorder] = "prereorder",
    [fxs_state_reorder] = "reorder",
    [fxs_state_ringing_1st] = "ringing_1st",
    [fxs_state_fsk_preamble_vmwi] = "fsk_preamble_vmwi",
    [fxs_state_fsk_preamble_caller_id] = "fsk_preamble_caller_id",
    [fxs_state_fsk_preamble_wcaller_id] = "fsk_preamble_wcaller_id",
    [fxs_state_fsk_cid] = "fsk_cid",
    [fxs_state_fsk_wcid] = "fsk_wcid",
    [fxs_state_ringing_2nd] = "ringing_2nd",
    [fxs_state_ringing] = "ringing",
    [fxs_state_answer] = "answer",
    [fxs_state_ringback_remote] = "ringback remote",
    [fxs_state_ringback_local] = "ringback local",
    [fxs_state_connected] = "connected",
    [fxs_state_force_connected] = "force_connected",
    [fxs_state_disconnect] = "disconnect",
    [fxs_state_fsk_vmwi] = "mwi",
    [fxs_state_init_mwi] = "init_mwi",
    [fxs_state_suppl] = "suppl",
    [fxs_state_wcid_init] = "wcid_init",
    [fxs_state_wcid_wait_ack] = "wcid_wait_ack",
};

const char* call_dir_names[] = {
    [call_dir_incoming] = "incoming",
    [call_dir_outgoing] = "outgoing",
};

const char* call_type_names[] = {
    [call_type_invalid] = "invalid",
    [call_type_normal] = "normal",
    [call_type_conference] = "conference",
    [call_type_emergency] = "emergency",
};

const char* call_status_names[] = {
    [call_status_none] = "none",
    [call_status_progressed] = "progressed",
    [call_status_ringback] = "ringback",
    [call_status_ringing] = "ringing",
    [call_status_waiting] = "waiting",
    [call_status_connected] = "connected",
    [call_status_held] = "held",
    [call_status_disconnected] = "disconnected",
};

///////////////////////////////////////////////////////////////////////////////
// event argument structures
///////////////////////////////////////////////////////////////////////////////

struct states_collection_t {
    const int* states;
    int states_count;
};

enum event_arg_suppl_ack_res_t {
    event_arg_suppl_ack_res_ok,
    event_arg_suppl_ack_res_err,
};

struct event_arg_suppl_ack_t {
    enum event_arg_suppl_ack_res_t res;
};

struct event_arg_suppl_t {
    const char* opt;
};

enum {
    OIR_NONE,
    OIR_SUPPRESSION,
    OIR_INVOCATION,
};

struct event_arg_place_call_t {
    enum call_type_t call_type;
    const char* num;
    int oir_type; /* originating identification restriction type */
};

struct event_arg_digit_t {
    int digit;
    char ch;
    int long_press;
};

struct event_arg_digit_pressed_t {
    int digit;
    char ch;
    int pressed;
};

struct event_arg_caller_id_t {
    int month;
    int day_of_month;
    int hour;
    int minute;
    const char* cid_num;
    const char* cid_num_pi;
    const char* cid_name;
    const char* cid_name_pi;
};

struct event_arg_mwi_t {
    int state;
};

struct event_arg_rx_dtmf_t {
    char ch;
    int start_or_end; /* start = FALSE, end = TRUE */
};

///////////////////////////////////////////////////////////////////////////////
// channel information
///////////////////////////////////////////////////////////////////////////////

struct caller_id_t {
    char cid[MAX_LENGTH_OF_CID];
    int idx;
    int len;
};

enum {
    fsk_preamble_reason_none = 0,
    fsk_preamble_reason_caller_id,
    fsk_preamble_reason_vmwi
};

enum {
    fsk_cid_reason_none = 0,
    fsk_cid_reason_ringing,
    fsk_cid_reason_waiting
};

#define DUMP_REGISTER_COUNT 10

/* FXS channel state structure */
struct chan_state_t {
    /* channel index */
    int chan_idx;

    /* linked structures */
    struct fsm_t fsm;
    SiVoiceChanType_ptr cptr;
    struct fxs_port_t* port;

    struct call_track_t* foreground_call;

    struct softtonegen_t stg;

    /* hook debounce timer and structure */
    timeStamp hookTimer;
    int hookTimerValid;
    uInt8 hook_state;

    timeStamp timeout_dialtone;

    /* fake call debug */
    timeStamp timeout_fakecall;

    /* fake mwi debug */
    int fakemwi;
    timeStamp timeout_fakemwi;

    /* fake cwi debug */
    timeStamp timeout_fakecwi;

    /* schedule fsm event */
    timeStamp scheduled_fsm_timer;
    int scheduled_fsm_event;

    /* flag is set when FSK FIFO is written after initiating */
    int fsk_fifo_written;
    uint64_t fsk_fifo_timestamp;

    /* preamble index */
    int preamble_idx;

    /* caller id */
    struct caller_id_t caller_id_to_send;

    /* mwi state */
    int mwi_state_valid;
    int mwi_state;

    /* dial plan */
    char num[MAX_NUM_OF_PHONE_NUM + 1];
    int num_idx;

    /* fake mwi debug */
    char last_incall_num;
    timeStamp timeout_last_incall_num;

    struct irq_storage_t irq_storage;

    unsigned int digitCount; /* How many digits did we collect? */
    BOOLEAN cid_is_offhook; /* Is the device off hook ? */
    BOOLEAN cid_quit; /* Do we quit ? */
    BOOLEAN cid_fsk_interrupt; /* Did we detect a FSK interrupt */
    BOOLEAN ignore_hook_change; /* Do we ignore hook state change? */
    BOOLEAN hook_change; /* Did the hook state change during the execution? */
    uInt8 ringCount; /* How many rings did we do? */

    int chan_connect; /** Which channel is being attempted to be connected */
    int scratch_var[2]; /** Used by states for temp storage. 1 is reserved to store prior state */

    int perform_conference_when_hookflash;
    int during_tx_dtmf;
    int during_rx_dtmf;

    uInt32 dumpRegs[DUMP_REGISTER_COUNT];
};

/* structure that has registers to dump */
struct dump_register_collection_t {
    const char* name;
    int ram;
    int log_chg;
    int addr;
};

struct dump_register_collection_t dump_register_collection[DUMP_REGISTER_COUNT] = {
    {"regID",         FALSE, TRUE,  PROSLIC_REG_ID},
    {"regMstrStat",   FALSE, TRUE,  PROSLIC_REG_MSTRSTAT},
    {"regRamStat",    FALSE, TRUE,  PROSLIC_REG_RAMSTAT},
    {"regLineFeed",   FALSE, TRUE,  PROSLIC_REG_LINEFEED},
    {"regLCRRTP",     FALSE, TRUE,  PROSLIC_REG_LCRRTP},
    {"regEnhance",    FALSE, TRUE,  PROSLIC_REG_ENHANCE},
    {"regOCon",       FALSE, FALSE, PROSLIC_REG_OCON},
    {"ramDcDcStat",   TRUE,  TRUE,  PROSLIC_RAM_DCDC_STATUS},
    {"ramPdDcDC",     TRUE,  TRUE,  PROSLIC_RAM_PD_DCDC},
    {"ramCaDc",       TRUE,  TRUE,  PROSLIC_RAM_PD_CADC},
};

///////////////////////////////////////////////////////////////////////////////
// FSK protocol defines
///////////////////////////////////////////////////////////////////////////////

/* caller id in MDMF */
#define MDMF_DATA_TYPE_DATETIME 1
#define MDMF_DATA_TYPE_PHONE_NUMBER 2
#define MDMF_DATA_TYPE_REASON_NO_NUMBER 4
#define MDMF_DATA_TYPE_NAME 7
#define MDMF_DATA_TYPE_REASON_NO_NAME 8

struct mdmf_caller_id_data_datetime_t {
    char month[2];
    char day_of_month[2];
    char hour[2];
    char minute[2];
} __packed;

struct mdmf_caller_id_data_reason_t {
    char reason;
} __packed;

struct mdmf_caller_id_data_header_t {
    char data_type;
    char length;
} __packed;

struct mdmf_caller_id_header_t {
    char message_type;
    char length;
} __packed;

/* caller id in single data message format */
struct sdmf_caller_id_t {
    char message_type;
    char length;
    char month[2];
    char day_of_month[2];
    char hour[2];
    char minute[2];
    char number[0];
} __packed;

/* vmwi in single data message format */

#define FSK_CALLER_ID_MESSAGE_TYPE_SDMF_VMWI 6
#define FSK_CALLER_ID_MESSAGE_TYPE_SDMF_CALLER_ID 4
#define FSK_CALLER_ID_MESSAGE_TYPE_MDMF_CALLER_ID 128

#define SDMF_VMWI_STATE_ON 0x42
#define SDMF_VMWI_STATE_OFF 0x6f

struct sdmf_vmwi_t {
    char message_type;
    char length;
    char state[3];
} __packed;

/* single data message format trailer */

struct sdmf_trailer_t {
    char checksum;
} __packed;

struct mdmf_trailer_t {
    char checksum;
    char markout;
} __packed;

///////////////////////////////////////////////////////////////////////////////
// call current state
///////////////////////////////////////////////////////////////////////////////

struct curr_call_stat_t {
    int incoming_calls;
    int outgoing_calls;
};

///////////////////////////////////////////////////////////////////////////////
// dial plan
///////////////////////////////////////////////////////////////////////////////

struct dialplan_func_map_t {
    const char* func_name;
    dialplan_func_t func_ptr;
};

///////////////////////////////////////////////////////////////////////////////
// global instances
///////////////////////////////////////////////////////////////////////////////

static struct chan_state_t* _chan_state = NULL;

static int _bridge_running = 1;

static regex_t _phone_num_regex;

/* rdb global instances */
static struct rwpipe_t* _pp = NULL;
static struct dbenum_t* _dbenum = NULL;
static struct indexed_rdb_t* _ir = NULL;

#define __STR(vc) indexed_rdb_get_cmd_str(_ir,(vc))

/* curr call state */
static struct curr_call_stat_t _curr_call_stat_inst;
static struct curr_call_stat_t* _curr_call_stat = &_curr_call_stat_inst;

///////////////////////////////////////////////////////////////////////////////
// local function defines
///////////////////////////////////////////////////////////////////////////////

static void on_event(struct strarg_t* a);
static void dump_channel_register(int loglevel, struct chan_state_t* csptr, int force_to_dump);

///////////////////////////////////////////////////////////////////////////////
// tool functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief converts ProSLIC DTMF digit to ASCII code digit.
 *
 * @param digit is ProSLIC DTMF digit.
 *
 * @return ASCII code digit.
 */
static char convert_digit_to_ascii(int digit)
{
    char ascii[] = "D"
                   "1234567890"
                   "*#ABC";

    if (digit < 0 || digit >= __countof(ascii))
        goto err;

    return ascii[digit];

err:
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// RDB constant variables
///////////////////////////////////////////////////////////////////////////////

enum {
    subscribed_rdb_voice_command_noti = 0,
    subscribed_rdb_voice_command_ctrl,
    subscribed_rdb_system_mode,
    subscribed_rdb_voicemail_active,
    subscribed_rdb_block_call_trigger,
    subscribed_rdb_voice_mgmt_command_ctrl,
};

struct dbhash_element_t _subscribed_rdb_elements[] = {
    {RDB_VOICE_COMMAND_NOTI, subscribed_rdb_voice_command_noti},
    {RDB_VOICE_COMMAND_CTRL, subscribed_rdb_voice_command_ctrl},
    {RDB_SYSTEM_MODE, subscribed_rdb_system_mode},
    {RDB_VOICEMAIL_ACTIVE, subscribed_rdb_voicemail_active},
    {RDB_BLOCK_CALL_TRIGGER, subscribed_rdb_block_call_trigger},
    {RDB_VOICE_MGMT_COMMAND_CTRL, subscribed_rdb_voice_mgmt_command_ctrl},
};

const char* _subscribed_rdb_var[] = {
    RDB_VOICE_COMMAND_CTRL,
    RDB_VOICE_COMMAND_NOTI,
    RDB_SYSTEM_MODE,
    RDB_VOICEMAIL_ACTIVE,
    RDB_BLOCK_CALL_TRIGGER,
    RDB_VOICE_MGMT_COMMAND_CTRL,
};

const char* _rdb_var_to_reset[] = {
    RDB_VOICE_COMMAND_CTRL,
    RDB_VOICE_COMMAND_NOTI,
    RDB_BLOCK_CALL_TRIGGER,
    RDB_VOICE_MGMT_COMMAND_CTRL,
};

struct indexed_rdb_t* _ir_subscribed = NULL;

///////////////////////////////////////////////////////////////////////////////
// voice RDB command functions
///////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_TX_OOB_DTMF
static int rdb_stop_dtmf(int cid_num)
{
    return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d", __STR(vc_stop_dtmf), cid_num);
}

static int rdb_start_dtmf(int cid_num, char digit)
{
    return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d %c", __STR(vc_start_dtmf), cid_num, digit);
}

#endif

#ifdef CONFIG_FUTURE_FEATURE
static int rdb_manage_calls_local_hold(void)
{
    return rdb_manage_calls("HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD", 0, NULL);
}

static int rdb_manage_calls_local_unhold(void)
{
    /* acitivy of network unhold is using the identical command */
    return rdb_manage_calls("HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD", 0, NULL);

    #if 0
    /* do not use release - this release activity is disconnecting the call */
    return rdb_manage_calls("RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING", 0, NULL);
    #endif
}

static int rdb_manage_calls_activate(int cid_num)
{
    return rdb_manage_calls("HOLD_ALL_EXCEPT_SPECIFIED_CALL", cid_num, NULL);
}

static int rdb_manage_calls_resume(int cid_num)
{
    return rdb_manage_calls("CALL_RESUME", cid_num, NULL);
}

static int rdb_manage_calls_end_all_calls(void)
{
    return rdb_manage_calls("END_ALL_CALLS", 0, NULL);
}

static int rdb_manage_calls_release(int cid_num, const char* reject_cause)
{
    return rdb_manage_calls("RELEASE_SPECIFIED_CALL", cid_num, reject_cause);
}

#endif

/**
 * @brief reject an incoming voice call.
 *
 * @param cid call id to answer.
 * @param reject_cause reject reason.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_reject(int cid_num, const char* reject_cause)
{
    if (reject_cause) {
        return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d %s", __STR(vc_reject), cid_num, reject_cause);
    } else {
        return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d", __STR(vc_reject), cid_num);
    }
}

/**
 * @brief answers voice call.
 *
 * @param cid call id to answer.
 * @param reject_cause reject cause. the function rejects the call if reject_cause is provided.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_answer(int cid, const char* reject_cause)
{
    if (reject_cause) {
        return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d %s", __STR(vc_answer), cid, reject_cause);
    } else {
        return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d", __STR(vc_answer), cid);
    }
}

/**
 * @brief manages voice call.
 *
 * @param sups_type is supplementary type as string.
 * @param cid is call id.
 * @param reject_cause is reject cause as string.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_manage_calls(const char* sups_type, int cid, const char* reject_cause)
{
    const char* vc_manage_calls_str;
    char msg[RDB_MAX_VAL_LEN];

    syslog(LOG_DEBUG, "[INCALL] send manage calls (cmd='%s',cid=%d)", sups_type, cid);

    #ifdef CONFIG_USE_IP_CALLS
    vc_manage_calls_str = __STR(vc_manage_ip_calls);
    #else
    vc_manage_calls_str = __STR(vc_manage_calls);
    #endif

    snprintf(msg, sizeof(msg), "%s %s %d", vc_manage_calls_str, sups_type, cid);

    if (reject_cause)
        return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %s", msg, reject_cause);
    else
        return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s", msg);
}

/**
 * @brief send manage call voice call command - "release active accept held or waiting"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_manage_calls_release_active_acept_held_or_waiting(void)
{
    return rdb_manage_calls("RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING", 0, NULL);
}

/**
 * @brief send manage call voice call command - "make conference call"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_manage_calls_make_conference(void)
{
    return rdb_manage_calls("MAKE_CONFERENCE_CALL", 0, NULL);
}

/**
 * @brief send manage call voice call command - "hold active accept waiting or held"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_manage_calls_hold_active_accept_waiting_or_held(void)
{
    return rdb_manage_calls("HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD", 0, NULL);
}

/**
 * @brief send voice call command - "hang up"
 *
 * @param cid call id to hang up.
 * @param reason reject reason.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_hangup(int cid, const char* reason)
{
    int stat;

    if (reason) {
        stat = rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d %s", __STR(vc_hangup), cid, reason);
    } else {
        stat = rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d", __STR(vc_hangup), cid);
    }

    return stat;
}

/**
 * @brief send voice call command - "setup_answer"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_setup_answer(int cid, const char* reason)
{
    int stat;

    if (reason) {
        stat = rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d %s", __STR(vc_setup_answer), cid, reason);
    } else {
        stat = rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d", __STR(vc_setup_answer), cid);
    }

    return stat;
}

/**
 * @brief send voice call command - "call"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_call(const char* dest_num, int timeout)
{
    syslog(LOG_INFO, "[RDB] make a call (%s)", dest_num);
    return rwpipe_post_write_printf(_pp, NULL, timeout, 0, "%s %s %d", __STR(vc_call), dest_num, timeout);
}

/**
 * @brief send voice call command - "init_voice_call"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_init_voice_call()
{
    syslog(LOG_INFO, "[RDB] init voice call");
    return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s", __STR(vc_init_voice_call));
}

/**
 * @brief send voice call command - "fini_voice_call"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_fini_voice_call()
{
    syslog(LOG_INFO, "[RDB] fini voice call");
    return rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s", __STR(vc_fini_voice_call));
}

/**
 * @brief send voice call command - "pcall"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_pcall(const char* dest_num, int timeout)
{
    syslog(LOG_INFO, "[RDB] make a pcall (%s)", dest_num);
    return rwpipe_post_write_printf(_pp, NULL, timeout, 0, "%s %s %d", __STR(vc_pcall), dest_num, timeout);
}

/**
 * @brief send voice call command - "ocall"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_ocall(const char* dest_num, int timeout)
{
    syslog(LOG_INFO, "[RDB] make a ocall (%s)", dest_num);
    return rwpipe_post_write_printf(_pp, NULL, timeout, 0, "%s %s %d", __STR(vc_ocall), dest_num, timeout);
}

/**
 * @brief send voice call command - "emergency call"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_ecall(const char* dest_num, int timeout)
{
    syslog(LOG_INFO, "[RDB] make a call (%s)", dest_num);
    return rwpipe_post_write_printf(_pp, NULL, timeout, 0, "%s %s %d", __STR(vc_ecall), dest_num, timeout);
}

/**
 * @brief send voice call command - "supplementary forward"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_suppl_forward(const char* opt)
{
    int stat;

    syslog(LOG_INFO, "submit SUPPL FORWARD (opt='%s')", opt);

    ///////////////////////////////////////////////////////////////////////////////
    // post command
    ///////////////////////////////////////////////////////////////////////////////

    stat = rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %s", __STR(vc_suppl_forward), opt);

    return stat;
}

/**
 * @brief send voice call command - "set tty mode"
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int rdb_set_tty_mode(const char* opt)
{
    int stat;

    syslog(LOG_INFO, "submit SET_TTY_MODE (opt='%s')", opt);

    ///////////////////////////////////////////////////////////////////////////////
    // post command
    ///////////////////////////////////////////////////////////////////////////////

    stat = rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %s", __STR(vc_set_tty_mode), opt);

    return stat;
}

///////////////////////////////////////////////////////////////////////////////
// rdb call log
///////////////////////////////////////////////////////////////////////////////

static void rdb_delete_voice_call_status(struct rdb_session* rdb_session)
{
    int total_rdbs;
    struct dbenum_t* dbenum;
    struct dbenumitem_t* item;

    /* reset voice call status rdb */
    dbenum = dbenum_create(rdb_session, 0);
    if (dbenum) {

        syslog(LOG_DEBUG, "search voice call status RDB to reset (prefix=%s)", VOICE_CALL_CALL_STATUS_PREFIX);

        total_rdbs = dbenum_enumDbByNames(dbenum, VOICE_CALL_CALL_STATUS_PREFIX);

        syslog(LOG_DEBUG, "init. voice status RDBs (total_rdbs=%d)", total_rdbs);

        item = dbenum_findFirst(dbenum);
        while (item) {
            if (!strncmp(item->szName, VOICE_CALL_CALL_STATUS_PREFIX, strlen(VOICE_CALL_CALL_STATUS_PREFIX))) {

                if (rdb_set_string(rdb_session, item->szName, "") < 0) {
                    syslog(LOG_ERR, "rdb_set_string(%s) failed in start_devices() - %s", item->szName, strerror(errno));
                } else {
                    syslog(LOG_DEBUG, "reset RDB [%s]", item->szName);
                }
            }

            item = dbenum_findNext(dbenum);
        }

        dbenum_destroy(dbenum);
    }
}

static int rdb_set_voice_call_status(int cid_num, const char* status, const char* param)
{
    char val[RDB_MAX_VAL_LEN];
    char rdb[RDB_MAX_VAL_LEN];
    int stat;

    static struct rdb_session* rdb_session;

    /* get voice call rdb */
    snprintf(rdb, sizeof(rdb), VOICE_CALL_CALL_STATUS_PREFIX ".%d", cid_num);

    if (!param || !*param)
        snprintf(val, sizeof(val), "%s", status);
    else
        snprintf(val, sizeof(val), "%s %s", status, param);

    syslog(LOG_INFO, "[RDB] voice call status changed (cid=%d,status='%s')", cid_num, val);
    rdb_session = ezrdb_get_session();
    if ((stat = rdb_set_string(rdb_session, rdb, val)) < 0) {
        if (stat == -ENOENT) {
            stat = rdb_create_string(rdb_session, rdb, val, 0, 0);
        }
    }

    if (stat < 0) {
        syslog(LOG_ERR, "rdb_set_string() failed in on_rdb_ctrl() - %s", strerror(errno));
        goto err;
    }

    return 0;

err:
    return -1;
}

///////////////////////////////////////////////////////////////////////////////
// call control (CC) functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief update call count RDB variables.
 *
 */
static void cc_update_call_counts(void)
{

    ezrdb_set_int(VOICE_CALL_INCOMING_CALL_COUNT, _curr_call_stat->incoming_calls);
    ezrdb_set_int(VOICE_CALL_OUTGOING_CALL_COUNT, _curr_call_stat->outgoing_calls);
    ezrdb_set_int(VOICE_CALL_TOTAL_CALL_COUNT, _curr_call_stat->incoming_calls + _curr_call_stat->outgoing_calls);

    syslog(LOG_DEBUG, "[CC] update call counts (incoming=%d,outgoing=%d)", _curr_call_stat->incoming_calls,
           _curr_call_stat->outgoing_calls);
}

/**
 * @brief call control function to change MT (mobile terminated) call state to 'connected'.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int cc_connect_mt_call(struct call_track_t* call)
{
    syslog(LOG_DEBUG, "[CC] make MT call status connected");

    if ((call->call_status != call_status_connected) && (call->call_status !=  call_status_held)) {
        syslog(LOG_DEBUG, "[CC] got a new MT call");
        call->timestamp_connected_msec = get_monotonic_msec();

        /* increase incoming call count */
        _curr_call_stat->incoming_calls++;
        call->incoming_call_count_coeff++;
        cc_update_call_counts();
    }

    call->call_status = call_status_connected;

    return 0;
}

/**
 * @brief call control function to change MO (mobile originated) call state to 'connected'.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int cc_connect_mo_call(struct call_track_t* call)
{
    syslog(LOG_DEBUG, "[CC] make MO call status connected");

    if ((call->call_status != call_status_connected) && (call->call_status !=  call_status_held)) {
        syslog(LOG_DEBUG, "[CC] got a new MO call");
        call->timestamp_connected_msec = get_monotonic_msec();

        /* increase outgonig call count */
        _curr_call_stat->outgoing_calls++;
        call->outgoing_call_count_coeff++;
        cc_update_call_counts();

    }

    call->call_status = call_status_connected;

    return 0;

}

/**
 * @brief call control function to change MO (mobile originated) call state to 'ringback'.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int cc_make_mo_ringback(struct call_track_t* call)
{
    syslog(LOG_DEBUG, "[CC] make MO call status ringback");

    call->call_status = call_status_ringback;

    return 0;
}

/**
 * @brief checks to see if there is any state machine available to answer an incoming call.
 *
 * @return TRUE if there is any state machine available to answer an incoming call. Otherwise, FALSE.
 */
static int cc_is_any_sm_available_to_answer(void)
{
    struct chan_state_t* csptr;
    int i;
    struct fsm_t* fsm;

    for (i = 0; i < _fxs_info->num_of_channels; i++) {
        csptr = &_chan_state[i];

        fsm = &csptr->fsm;

        /* if connected or onhook **/
        if ((fsm->cur_state_idx == fxs_state_connected) || (fsm->cur_state_idx == fxs_state_disconnect)
            || (fsm->cur_state_idx == fxs_state_idle) || (fsm->cur_state_idx == fxs_state_fsk_preamble_vmwi)
            || (fsm->cur_state_idx == fxs_state_fsk_vmwi)) {
            goto fini;
        }
    }

    return FALSE;

fini:
    return TRUE;

}

/**
 * @brief checks to see if there is any channel in onhook state.
 *
 * @return TRUE if there is any channel in onhook state. Otherwise, FALSE.
 */
static int cc_is_any_onhook_channel(void)
{
    struct chan_state_t* csptr;
    int i;
    struct fsm_t* fsm;

    for (i = 0; i < _fxs_info->num_of_channels; i++) {
        csptr = &_chan_state[i];
        fsm = &csptr->fsm;

        if (csptr->hook_state == PROSLIC_ONHOOK) {
            goto fini;
        }

        if ((fsm->cur_state_idx == fxs_state_idle) || (fsm->cur_state_idx == fxs_state_fsk_preamble_vmwi)
            || (fsm->cur_state_idx == fxs_state_fsk_vmwi)) {
            goto fini;
        }
    }

    return FALSE;

fini:
    return TRUE;

}

/**
 * @brief call control function to change MT (mobile terminated) call state to 'ringing'.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int cc_make_mt_ringing(struct call_track_t* call)
{
    syslog(LOG_DEBUG, "[CC] make MT call status ringing");

    call->call_status = call_status_ringing;

    return 0;
}

/**
 * @brief call control function to change MT (mobile terminated) call state to 'setup'.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int cc_make_mt_setup(struct call_track_t* call)
{
    syslog(LOG_DEBUG, "[CC] make MT call status setup");

    call->call_status = call_status_setup;

    return 0;
}

/**
 * @brief call control function to change MT (mobile terminated) call state to 'waiting'.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int cc_make_mt_waiting(struct call_track_t* call)
{
    syslog(LOG_DEBUG, "[CC] make MT call status waiting");

    call->call_status = call_status_waiting;

    return 0;
}

/**
 * @brief call control function to change MO (mobile originated) or MT (mobile terminated) call state to 'disconnected'.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int cc_make_call_disconnected(struct call_track_t* call)
{
    uint64_t duration;

    syslog(LOG_DEBUG, "[CC] make call status disconnected");

    call->timestamp_disconnected_msec = get_monotonic_msec();

    duration = (call->timestamp_disconnected_msec - call->timestamp_connected_msec) / 1000;
    call->duration_valid = call->call_status >= call_status_connected;
    call->duration = (duration <= INT_MAX) ? duration : INT_MAX;

    call->call_status = call_status_disconnected;

    /* decrease call counts */
    _curr_call_stat->incoming_calls -= call->incoming_call_count_coeff;
    _curr_call_stat->outgoing_calls -= call->outgoing_call_count_coeff;
    cc_update_call_counts();

    return 0;
}

/**
 * @brief call control function to change MO (mobile originated) call state to 'progressed'.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int cc_make_mo_progressed(struct call_track_t* call)
{
    syslog(LOG_DEBUG, "[CC] make MO call status progressed");

    call->call_status = call_status_progressed;

    return 0;
}

/**
 * @brief gets call track object by call id.
 *
 * @param cid is call id.
 *
 * @return
 */
static struct call_track_t* cc_get_call_by_cid(int cid)
{
    return call_track_find(cid);
}

/**
 * @brief adds call information to call.
 *
 * @param call is call track object.
 * @param pi is privacy information.
 * @param clip is caller id.
 */
static void cc_add_info_to_call(struct call_track_t* call, const char* cid_num, const char* cid_num_pi,
                                const char* cid_name, const char* cid_name_pi)
{
    if (cid_num_pi) {
        syslog(LOG_DEBUG, "[CNAME] update number PI in call (cid=%d,cid_num_pi='%s')", call->call_track_id, cid_num_pi);

        free(call->cid_num_pi);
        call->cid_num_pi = strdup(cid_num_pi);
    }

    if (cid_num) {
        syslog(LOG_DEBUG, "[CNAME] update number in call (cid=%d,cid_num='%s')", call->call_track_id, cid_num);

        cid_num = call_prefix_get_incoming_cid_for_rj11(cid_num);

        free(call->cid_num);
        call->cid_num = strdup(cid_num);
    }

    if (cid_name) {
        syslog(LOG_DEBUG, "[CNAME] update name in call (cid=%d,cid_name='%s')", call->call_track_id, cid_name);

        free(call->cid_name);
        call->cid_name = strdup(cid_name);
    }

    if (cid_name_pi) {
        syslog(LOG_DEBUG, "[CNAME] update name in call PI (cid=%d,cid_name='%s')", call->call_track_id, cid_name_pi);

        free(call->cid_name_pi);
        call->cid_name_pi = strdup(cid_name_pi);
    }
}

/**
 * @brief checks to see if there is any call.
 *
 * @return TRUE if there is no existing call. Otherwise, FALSE.
 */
static int cc_is_empty(void)
{
    return call_track_is_empty();
}

/**
 * @brief gets the first waiting call.
 *
 * @return waiting call. Otherwise, NULL.
 */
static struct call_track_t* cc_get_waiting_call(void)
{
    struct call_track_t* call;
    struct call_track_t* waiting_call = NULL;

    call_track_walk_for_each_begin(call) {

        if (call->call_status == call_status_waiting) {
            waiting_call = call;
            break;
        }

        call_track_walk_for_each_end();
    }

    return waiting_call;
}

/**
 * @brief search for a call by call type
 *
 * @param call_type is a call type to search for.
 * @return a call. Otherwise, NULL.
 */
static struct call_track_t* cc_get_call_by_call_type(enum call_type_t call_type)
{
    struct call_track_t* call;
    struct call_track_t* call_to_return = NULL;

    call_track_walk_for_each_begin(call) {

        if (call->call_type == call_type) {
            call_to_return = call;
            break;
        }

        call_track_walk_for_each_end();
    }

    return call_to_return;
}

static int cc_is_waiting_call_ringing(void)
{
    int waiting_flag;

    struct call_track_t* call;

    waiting_flag = 0;

    call_track_walk_for_each_begin(call) {

        waiting_flag = waiting_flag || ((call->call_status == call_status_waiting) && call->ringing);

        call_track_walk_for_each_end();

        if (waiting_flag) {
            break;
        }
    }

    return waiting_flag;
}

/**
 * @brief checks to see if there is any held call.
 *
 * @return TRUE when there is any held call. Otherwise, FALSE.
 */
static int cc_is_any_held_call(void)
{
    int hold_flag;

    struct call_track_t* call;

    hold_flag = 0;

    call_track_walk_for_each_begin(call) {

        hold_flag = hold_flag || call->call_status == call_status_held;

        call_track_walk_for_each_end();

        if (hold_flag) {
            break;
        }
    }

    return hold_flag;
}

/**
 * @brief get the total number of connected or held call count
 *
 * @return total number of connected or held call count
 */
static int cc_get_connected_or_held_call_count(void)
{
    int call_count;

    struct call_track_t* call;

    call_count = 0;

    call_track_walk_for_each_begin(call) {

        if ((call->call_status == call_status_connected) || (call->call_status == call_status_held)) {
            call_count++;
        }

        call_track_walk_for_each_end();
    }

    return call_count;
}

#ifdef CONFIG_FUTURE_FEATURE
/**
 * @brief checks to see if there is any incoming held call.
 *
 * @return TRUE when there is any held call. Otherwise, FALSE.
 */
static int cc_is_any_incoming_held_call(void)
{
    int hold_flag;

    struct call_track_t* call;

    hold_flag = 0;

    call_track_walk_for_each_begin(call) {

        hold_flag = hold_flag || ((call->call_dir == call_dir_incoming) && (call->call_status == call_status_held));

        call_track_walk_for_each_end();

        if (hold_flag) {
            break;
        }
    }

    return hold_flag;
}
#endif

/**
 * @brief check to see if there is any active call.
 *
 * @return TRUE when there is any active call. Otherwise, FALSE.
 */
static int cc_is_any_call_active(void)
{
    int active_flag;

    struct call_track_t* call;

    active_flag = 0;

    call_track_walk_for_each_begin(call) {

        active_flag = active_flag || call->call_status == call_status_connected;

        call_track_walk_for_each_end();

        if (active_flag) {
            break;
        }
    }

    return active_flag;
}

/**
 * @brief check to see if there is any ringing or waiting call.
 *
 * @param call is to exclude.
 * @return TRUE when there is any ringing or waiting call. Otherwise, FALSE.
 */
static int cc_is_any_call_ringing_or_waiting(struct call_track_t* call)
{
    int ringing_flag;

    struct call_track_t* call_i;

    ringing_flag = 0;

    call_track_walk_for_each_begin(call_i) {

        if (call_i != call) {
            ringing_flag = ringing_flag || call_i->call_status == call_status_ringing || call_i->call_status == call_status_waiting;
        }

        call_track_walk_for_each_end();

        if (ringing_flag) {
            break;
        }
    }

    return ringing_flag;
}

/**
 * @brief checks MasterStat register and terminates POTS bridge if any error is found.
 *
 */
static void monitor_slic_hw_stat()
{
    struct chan_state_t* csptr;
    SiVoiceChanType_ptr cptr;

    int i;
    uInt8 regData;
    uInt8 regData2;

    ramData ramDat;
    ramData ramDat2;

    int slic_hw_error_det;

    /* verify each of chanenls */
    for (i = 0; i < _fxs_info->num_of_channels; i++) {
        /* get channel state ptr */
        csptr = &_chan_state[i];
        /* get channel ptr */
        cptr = pbx_get_cptr(i);

        slic_hw_error_det = FALSE;

        /* check clock errors */
        regData = SiVoice_ReadReg(cptr, PROSLIC_REG_MSTRSTAT);
        if (regData != PROSLIC_REG_MSTRSTAT_RUNNING) {
            syslog(LOG_CRIT, "!!!!!! SLIC clock error detected (channel=%d) !!!!!!", i);

            slic_hw_error_det = TRUE;
        }

        /* check to see if register read and write access is okay by writing random values */
        SiVoice_WriteReg(cptr, PROSLIC_REG_USERSTAT, SLIC_STAT_MONITOR_REGDATA1);
        regData =  SiVoice_ReadReg(cptr, PROSLIC_REG_USERSTAT);
        SiVoice_WriteReg(cptr, PROSLIC_REG_USERSTAT, SLIC_STAT_MONITOR_REGDATA2);
        regData2 =  SiVoice_ReadReg(cptr, PROSLIC_REG_USERSTAT);
        if ((regData != SLIC_STAT_MONITOR_REGDATA1) || (regData2 != SLIC_STAT_MONITOR_REGDATA2)) {
            syslog(LOG_CRIT, "!!!!!! SPI error detected in accessing SLIC register (channel=%d) !!!!!!", i);

            slic_hw_error_det = TRUE;
        }

        /* check to see if RAM read and write access is okay by writing random values */
        netcomm_proslic_ram_set(cptr, PROSLIC_RAM_VERIFY_IO, SLIC_STAT_MONITOR_RAMDATA1);
        ramDat = netcomm_proslic_ram_get(cptr, PROSLIC_RAM_VERIFY_IO);
        netcomm_proslic_ram_set(cptr, PROSLIC_RAM_VERIFY_IO, SLIC_STAT_MONITOR_RAMDATA2);
        ramDat2 = netcomm_proslic_ram_get(cptr, PROSLIC_RAM_VERIFY_IO);
        if ((ramDat != SLIC_STAT_MONITOR_RAMDATA1) || (ramDat2 != SLIC_STAT_MONITOR_RAMDATA2)) {
            syslog(LOG_CRIT, "!!!!!! SPI error detected in accessing SLIC RAM (channel=%d) !!!!!!", i);

            slic_hw_error_det = TRUE;
        }

        /* check line feed */
        regData =  SiVoice_ReadReg(cptr, PROSLIC_REG_LINEFEED);
        if ((regData & PROSLIC_REG_LINEFEED_STAT_MASK) == 0) {
            syslog(LOG_CRIT, "!!!!!! OPEN line-feed detected (channel=%d) !!!!!!", i);

            slic_hw_error_det = TRUE;
        }

        if (slic_hw_error_det) {
            syslog(LOG_CRIT, "!!!!!! restart POTS bridge (channel=%d) !!!!!!", i);
            dump_channel_register(LOG_CRIT, csptr, TRUE);

            _bridge_running = FALSE;
        }
    }
}

/**
 * @brief enable Qualcomm DTMF mute.
 *
 */
static void enable_dtmf_mute()
{
    syslog(LOG_NOTICE, "enable dtmf mute");
    system("enable_dtmf_mute.sh");
}

/**
 * @brief initiates or finalises voice call.
 *
 */
static void init_or_fini_voice_call()
{
    static int voice_call_status = FALSE;

    int voice_call_status_new = !cc_is_empty();
    int chg = (!voice_call_status && voice_call_status_new) || (voice_call_status && !voice_call_status_new);

    voice_call_status = voice_call_status_new;

    if (chg) {
        syslog(LOG_DEBUG, "voice call status is changed (voice_call_status=%d)", voice_call_status);

        if (voice_call_status) {
            rdb_init_voice_call();
        } else {
            rdb_fini_voice_call();
        }
    }

    monitor_slic_hw_stat();
}

/**
 * @brief gets time offset (distance) from real world local time to MDM system UTC time.
 *
 * [MDM system UTC time] - [return value] = real world local time.
 *
 * @return offset (distance) from real world local time to MDM system UTC time.
 */
static int get_time_offset_from_real_localtime_to_mdm_system_utctime(void)
{
    int time_offset;
    const char* time_offset_str;

    time_offset_str = ezrdb_get_str(RDB_IPQ_TIMEOFFSET_IN_SEC);

    if (*time_offset_str) {
        /* Real world local time offset = RDB_IPQ_TIMEOFFSET_IN_SEC */
        time_offset = atol(time_offset_str);
        syslog(LOG_DEBUG, "[cid timestamp] time offset from IPQ is available (offset=%d)", time_offset);
    } else {
        time_offset_str = ezrdb_get_str(RDB_MODEM_TIMEOFFSET_IN_HOUR);

        if (*time_offset_str) {
            /* Real world local time offset = negative(-) RDB_MODEM_TIMEOFFSET_IN_HOUR */
            time_offset = atol(time_offset_str) * 60 * 60 * -1;
            syslog(LOG_DEBUG, "[cid timestamp] time offset from IPQ not available, use time offset from Modem (offset=%d)",
                   time_offset);
        } else {
            time_offset = 0;
            syslog(LOG_DEBUG, "[cid timestamp] no time offset available, use 0 time offset (offset=%d)", time_offset);
        }
    }

    return time_offset;
}

/**
 * @brief creates and adds a new call.
 *
 * @param cid is call ID.
 * @param call_dir is call direction.
 * @param call_type is call type.
 * @param pi is privacy information.
 * @param clip is caller ID.
 *
 * @return call track object when it succeeds. Otherwise, NULL.
 */
static struct call_track_t* cc_add_new_call(int cid, enum call_dir_t call_dir, enum call_type_t call_type,
        const char* cid_num, const char* cid_num_pi, const char* cid_name, const char* cid_name_pi)
{
    struct call_track_t* call = NULL;

    /* allocate call */
    call = call_track_add(cid);
    if (!call) {
        syslog(LOG_ERR, "unable to create call");
        goto err;
    }

    call->call_dir = call_dir;
    call->call_type = call_type;

    if (cid_num_pi) {
        call->cid_num_pi = strdup(cid_num_pi);
    }

    if (cid_num) {
        call->cid_num = strdup(cid_num);
    }

    if (cid_name) {
        call->cid_name = strdup(cid_name);
    }

    if (cid_name_pi) {
        call->cid_num_pi = strdup(cid_num_pi);
    }

    call->timestamp_created = time(NULL);

    /* system_timeoffset is offset (distance) from real world local time to MDM system UTC time */
    call->system_timeoffset = get_time_offset_from_real_localtime_to_mdm_system_utctime();

    /* init. voice call if required */
    init_or_fini_voice_call();

    /* init. qualcomm dtmf mute */
    enable_dtmf_mute();

    return call;

err:
    return NULL;
}

/**
 * @brief sets foreground call.
 *
 * @param fsm is an FSM object.
 * @param call is a call track object.
 */
static void cc_set_foreground_call(struct fsm_t* fsm, struct call_track_t* call)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    syslog(LOG_DEBUG, "change foreground call (prev=%d,new=%d)",
           csptr->foreground_call ? csptr->foreground_call->call_track_id : -1, call ? call->call_track_id : -1);

    csptr->foreground_call = call;
}

/**
 * @brief disassociates call with FSM.
 *
 * @param fsm is an FSM object.
 * @param call is a call track object.
 */
static void cc_disassociate_call_with_fsm(struct fsm_t* fsm, struct call_track_t* call)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    syslog(LOG_DEBUG, "disassociate call with channel (ch=%d,cid=%d)", csptr->chan_idx, call->call_track_id);

    csptr->foreground_call = NULL;
    call->ref = NULL;
}

/**
 * @brief associates call with FSM.
 *
 * @param fsm is an FSM object.
 * @param call is a call track object.
 */
static void cc_associate_call_with_fsm(struct fsm_t* fsm, struct call_track_t* call)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    syslog(LOG_DEBUG, "associate call with channel (ch=%d,cid=%d)", csptr->chan_idx, call->call_track_id);

    /* establish link between fsm and mt call */
    csptr->foreground_call = call;
    call->ref = &csptr->fsm;
}

/**
 * @brief answers MT (mobile terminated) call.
 *
 * @param fsm is an FSM object.
 *
 * @return 0 when succeeds. Otherwise, -1.
 */
static int cc_make_answer_mt_call(struct fsm_t* fsm)
{
    struct call_track_t* call = NULL;
    int cid;

    int stat = -1;

    syslog(LOG_DEBUG, "search ringing call");

    call_track_walk_for_each_begin(call) {

        /* answer a ringing call that no one claims ownership */
        if (!call->ref && (call->call_status == call_status_ringing)) {

            cid = call->call_track_id;

            syslog(LOG_DEBUG, "ringing call found, answer call (cid=%d)", cid);

            rdb_answer(call->call_track_id, NULL);

            /* establish link between fsm and mt call */
            cc_associate_call_with_fsm(fsm, call);

            stat = 0;
            break;
        }

        call_track_walk_for_each_end();
    }

    return stat;
}

/**
 * @brief call control function to make an MT (mobile termianted) call.
 *
 * @param cid is call ID.
 * @param pi is privacy information.
 * @param clip is caller ID.
 *
 * @return
 */
static struct call_track_t* cc_make_mt_call(int cid, const char* cid_num, const char* cid_num_pi, const char* cid_name,
        const char* cid_name_pi)
{
    struct call_track_t* call = cc_add_new_call(cid, call_dir_incoming, call_type_normal, cid_num, cid_num_pi, cid_name,
                                cid_name_pi);

    return call;
}

/**
 * @brief checks to see if a call is associated with FSM.
 *
 * @param fsm is an FSM object.
 * @param call is a call track object.
 *
 * @return
 */
static int cc_is_call_associate_with_fsm(struct fsm_t* fsm, struct call_track_t* call)
{
    struct chan_state_t* csptr;
    int associated;

    csptr = (struct chan_state_t*) fsm->ref;
    associated = csptr->foreground_call == call;

    if (associated) {
        syslog(LOG_DEBUG, "call associated with channel (ch=%d,cid=%d)", csptr->chan_idx, call->call_track_id);
    } else {
        syslog(LOG_DEBUG, "call not associated with channel (ch=%d,cid=%d)", csptr->chan_idx, call->call_track_id);
    }

    return associated;
}

/**
 * @brief removes call from call track list.
 *
 * @param call is a call to remove.
 */
static void cc_remove_call(struct call_track_t* call)
{
    struct fsm_t* fsm;

    call_history_write(call);

    fsm = (struct fsm_t*) call->ref;
    if (fsm) {
        if (cc_is_call_associate_with_fsm(fsm, call)) {
            cc_disassociate_call_with_fsm(fsm, call);
        }
    }

    free(call->cid_num);
    free(call->cid_num_pi);
    free(call->cid_name);
    free(call->cid_name_pi);

    /* remove and destroy */
    call_track_del(call);

    /* fini. voice call if required */
    init_or_fini_voice_call();

    return;
}

/**
 * @brief hangs up a call.
 *
 * @param call is a call to hang up.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int cc_hangup_call(struct call_track_t* call)
{
    int stat;
    int cid;

    cid = call->call_track_id;

    if (!cid) {
        syslog(LOG_DEBUG, "outgoing call not placed properly yet. remove call without disconnecting");
        cc_remove_call(call);

        stat = -1;
    } else if (call->call_status < call_status_progressed) {
        syslog(LOG_DEBUG, "outgoing call not progressed yet. disconnect and remove call ");
        cc_remove_call(call);
        rdb_hangup(cid, NULL);

        stat = -1;
    } else {
        syslog(LOG_DEBUG, "hangup call (cid=%d)", cid);
        rdb_hangup(cid, NULL);

        stat = 0;
    }

    return stat;
}

/**
 * @brief call control function to make MO (mobile originated) call.
 *
 * @param fsm is an FSM object.
 * @param call_type is call type.
 * @param num is a phone number.
 * @param timeout is call timeout.
 *
 * @return 0 when it succeeds. Otherwise, 0.
 */
static int cc_make_mo_call(struct fsm_t* fsm, enum call_type_t call_type, const char* cid_num, const char* cid_num_pi,
                           const char* cid_name, const char* cid_name_pi, int timeout, int action_only)
{
    struct call_track_t* call;
    int stat;

    syslog(LOG_DEBUG, "make MO call");

    /* post qmi call command */
    switch (call_type) {
        case call_type_conference:
            syslog(LOG_DEBUG, "send rdb conference call");
            stat = rdb_manage_calls_make_conference();
            break;

        case call_type_emergency:
            syslog(LOG_DEBUG, "send rdb emergency call");
            stat = rdb_ecall(cid_num, timeout);
            break;

        case call_type_normal:
            syslog(LOG_DEBUG, "send rdb call (num=%s)", cid_num);

            if (cid_num_pi && !strcmp(cid_num_pi, "RESTRICTED")) {
                stat = rdb_pcall(cid_num, timeout);
            } else if (cid_num_pi && !strcmp(cid_num_pi, "AVAILABLE")) {
                stat = rdb_ocall(cid_num, timeout);
            } else {
                stat = rdb_call(cid_num, timeout);
            }
            break;

        default:
            syslog(LOG_ERR, "unknown call type");
            stat = -1;
    }

    if (stat < 0) {
        syslog(LOG_ERR, "failed to place a RDB call");
        goto err;
    }

    if (!action_only) {
        /* initate call status members */
        syslog(LOG_DEBUG, "add call");

        call = cc_add_new_call(0, call_dir_outgoing, call_type, cid_num, cid_num_pi, cid_name, cid_name_pi);
        if (!call) {
            syslog(LOG_ERR, "failed to add mo call");
            goto err;
        }

        /* establish link between fsm and foreground call */
        cc_associate_call_with_fsm(fsm, call);
    }

    return 0;
err:
    return -1;
}

///////////////////////////////////////////////////////////////////////////////
// ProSLIC SDK - state machine
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief stops SLIC PCM.
 *
 * @param cptr
 *
 * @return 0 when it succeeds. Otherwise, SLIC error code.
 */
static int slic_stop_pcm(SiVoiceChanType_ptr cptr)
{
    syslog(LOG_DEBUG, "stop PCM");

    return ProSLIC_PCMStop(cptr);
}

/**
 * @brief starts SLIC PCM.
 *
 * @param cptr
 *
 * @return 0 when it succeeds. Otherwise, SLIC error code.
 */
static int slic_start_pcm(SiVoiceChanType_ptr cptr)
{
    syslog(LOG_DEBUG, "start PCM");

    return ProSLIC_PCMStart(cptr);
}

///////////////////////////////////////////////////////////////////////////////
// schedule event functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief call back function for scheduled event.
 *
 * @param ent is a call back entry.
 * @param elapsed_time is elapsed time.
 * @param ref is reference.
 */
static void fxs_schedule_event_post_on_callback_timer(struct callback_timer_entry_t* ent, uInt32 elapsed_time,
        void *ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    int event = csptr->scheduled_fsm_event;
    const char* event_name = fsm_get_event_name(fsm, event);

    syslog(LOG_DEBUG, "[schedule event] schedule expired, post event [%s,%d]", event_name, event);

    /* post event */
    fsm_post_event(fsm, event, NULL, 0);
}

/**
 * @brief cancels scheduled event.
 *
 * @param fsm is an FSM object.
 */
static void fxs_schedule_event_cancel(struct fsm_t* fsm)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    int event = csptr->scheduled_fsm_event;
    const char* event_name = fsm_get_event_name(fsm, event);

    syslog(LOG_DEBUG, "[schedule event] schedule cancelled event [%s,%d]", event_name, event);

    callback_timer_cancel(&csptr->scheduled_fsm_timer);

}

/**
 * @brief schedules event.
 *
 * @param fsm is an FSM object.
 * @param event is event to post.
 * @param timeout_ms is schedule timer.
 */
static void fxs_schedule_event_post(struct fsm_t* fsm, int event, uInt32 timeout_ms)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    const char* event_name = fsm_get_event_name(fsm, event);

    syslog(LOG_DEBUG, "[schedule event] schedule to post event [event %s]", event_name);

    /* schedule event */
    csptr->scheduled_fsm_event = event;
    callback_timer_set(cptr, "schedule event", &csptr->scheduled_fsm_timer, timeout_ms,
                       fxs_schedule_event_post_on_callback_timer, fsm);
}

///////////////////////////////////////////////////////////////////////////////
// broadcast event functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief broadcasts event to all FSMs.
 *
 * @param event is event to broadcast.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_broadcast_event(int event, void* event_arg, int event_arg_len)
{

    int i;
    struct chan_state_t* csptr;
    struct fsm_t* fsm;

    const char* event_name;

    for (i = 0; i < _fxs_info->num_of_channels; i++) {
        csptr = &_chan_state[i];
        fsm = &csptr->fsm;

        event_name = fsm_get_event_name(fsm, event);
        syslog(LOG_DEBUG, "[broadcast] post event [event '%s']", event_name);

        /* post event */
        fsm_post_event(fsm, event, event_arg, event_arg_len);
    }

}

///////////////////////////////////////////////////////////////////////////////
// Qualcomm fixup
///////////////////////////////////////////////////////////////////////////////

/*
 *
 * Since Qualcomm stack provides caller ID information any random time, we collect
 * caller ID information for a second.
 *
 */

/**
 * @brief updates pending MT call.
 *
 * @param call is a call track object.
 *
 * @return always 0.
 */
static int qc_fixup_update_pending_mt_call(struct call_track_t* call)
{
    unsigned long now;

    /* get flag */
    now = get_monotonic_msec();

    /* trigger when the call gets expired */
    call->ring_timestamp_valid = 1;
    call->ring_timestamp = now;
    call->ringing = 0;

    return 0;
}

/**
 * @brief builds call ID event argument from call information.
 *
 * @param call call structure that call ID event argument is from.
 * @param event_arg_caller_id pointer to call ID event argument.
 */
void build_event_arg_caller_id(struct call_track_t* call, struct event_arg_caller_id_t* event_arg_caller_id)
{
    struct tm tm_mdm_system_utctime;
    struct tm tm_real_localtime;
    time_t ts_real_localtime;
    int time_offset;

    /* call information */
    event_arg_caller_id->cid_num_pi = call->cid_num_pi;
    event_arg_caller_id->cid_num = call->cid_num;
    event_arg_caller_id->cid_name_pi = call->cid_name_pi;
    event_arg_caller_id->cid_name = call->cid_name;

    /*
     * convert time
     *
     * !! note !!
     *
     * call->timestamp_created is MDM's system time.
     * time_offset is offset from real world local time to MDM system UTC time.
     *
     * So, Real world local time = call->timestamp_created - call->system_timeoffset
     *
     *
    */

    time_offset = call->system_timeoffset;
    ts_real_localtime = call->timestamp_created - time_offset;

    gmtime_r(&call->timestamp_created, &tm_mdm_system_utctime);
    gmtime_r(&ts_real_localtime, &tm_real_localtime);

    syslog(LOG_DEBUG,
           "[cid timestamp] convert time off=%d, mdm utc(mon=%d,day=%d,hour=%d,min=%d) ==> real local(mon=%d,day=%d,hour=%d,min=%d)",
           time_offset, tm_mdm_system_utctime.tm_mon + 1, tm_mdm_system_utctime.tm_mday, tm_mdm_system_utctime.tm_hour,
           tm_mdm_system_utctime.tm_min, tm_real_localtime.tm_mon + 1, tm_real_localtime.tm_mday, tm_real_localtime.tm_hour,
           tm_real_localtime.tm_min);

    event_arg_caller_id->month = tm_real_localtime.tm_mon + 1;
    event_arg_caller_id->day_of_month = tm_real_localtime.tm_mday;
    event_arg_caller_id->hour = tm_real_localtime.tm_hour;
    event_arg_caller_id->minute = tm_real_localtime.tm_min;
}

/**
 * @brief performs pending MT call.
 *
 * @param cid is a call track object.
 *
 * @return always 0.
 */
static int qc_fixup_perform_pending_mt_call(int cid)
{
    int force;
    int expired;

    unsigned long now;
    struct call_track_t* call;

    /* get flag */
    now = get_monotonic_msec();

    call_track_walk_for_each_begin(call) {

        if (!call->ringing && (call->call_status == call_status_ringing || call->call_status == call_status_waiting)) {

            if (call->being_disconnected) {
                /* nothing to do */
            } else if (call->cid_num && block_calls_is_blocked(call->cid_num)) {

                syslog(LOG_INFO, "blocked call (cid=%d,num=%s)", call->call_track_id, call->cid_num);
                call->call_blocked = 1;
                rdb_reject(call->call_track_id, "BLACKLISTED_CALL_ID");

                call->being_disconnected = TRUE;

            } else if (call->call_status == call_status_ringing && !cc_is_any_onhook_channel()) {

                syslog(LOG_INFO, "no state machine available to answer a ringing call, hang up immediately (cid=%d)",
                       call->call_track_id);
                rdb_reject(call->call_track_id, "USER_BUSY");

                call->being_disconnected = TRUE;

            } else if ((call->call_status == call_status_waiting) && (!cc_is_any_sm_available_to_answer())) {
                syslog(LOG_INFO, "no state machine available to answer a waiting call, hang up immediately (cid=%d)",
                       call->call_track_id);
                rdb_reject(call->call_track_id, "USER_BUSY");

                call->being_disconnected = TRUE;
            } else {
                struct event_arg_caller_id_t event_arg_caller_id;

                /* get expired flag */
                expired = call->ring_timestamp_valid && !(now - call->ring_timestamp < DELAY_COLLECT_CALLER_ID);
                force = cid && (cid == call->call_track_id);

                if (expired || force) {

                    if (force)
                        syslog(LOG_DEBUG, "force to start pending ring (cid=%d)", call->call_track_id);
                    else if (expired) {
                        syslog(LOG_DEBUG, "ring timer expired, start pending ring (cid=%d)", call->call_track_id);

                        /* build event argument for caller id */
                        build_event_arg_caller_id(call, &event_arg_caller_id);

                        /* send 'ring' or 'ring_with_cid' */
                        if (!*event_arg_caller_id.cid_num) {
                            syslog(LOG_DEBUG, "no clip found, broadcast 'ring' to fsm (cid=%d)", call->call_track_id);

                            fxs_broadcast_event(call->call_status == call_status_ringing ? fxs_event_ring : fxs_event_waiting,
                                                NULL, 0);
                        } else {

                            syslog(LOG_DEBUG, "clip found, broadcast 'ring_with_cid' to fsm (clip=%s,cid=%d)", call->cid_num, call->call_track_id);

                            fxs_broadcast_event(call->call_status == call_status_ringing ? fxs_event_ring_with_cid : fxs_event_waiting_with_cid,
                                                &event_arg_caller_id, sizeof(event_arg_caller_id));
                        }

                    }

                    /* set ringing flag */
                    call->ringing = 1;
                }
            }
        }

        call_track_walk_for_each_end();
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// fake call for debug
///////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_SIMULATE_CALL

static void callback_timer_to_stop_fakecall(struct callback_timer_entry_t* ent, uInt32 elapsed_time, void *ref)
{
    struct fsm_t* fsm = (struct fsm_t*)ref;

    syslog(LOG_DEBUG, "[fake caller] callback_timer_to_stop_fakecall() triggered");
    fsm_post_event(fsm, fxs_event_fgcall_disconnected, NULL, 0);
}

static void callback_timer_to_start_fakecall(struct callback_timer_entry_t* ent, uInt32 elapsed_time, void *ref)
{
    struct fsm_t* fsm = (struct fsm_t*)ref;
    struct chan_state_t* csptr = (struct chan_state_t*)fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    struct event_arg_caller_id_t event_arg_caller_id;

    syslog(LOG_DEBUG, "[fake caller] callback_timer_to_start_fakecall() triggered");

    /* initiate caller id event argument */
    event_arg_caller_id.month = 12;
    event_arg_caller_id.day_of_month = 31;
    event_arg_caller_id.hour = 13;
    event_arg_caller_id.minute = 59;
    event_arg_caller_id.cid_num = "1234567";

    fsm_post_event(fsm, fxs_event_ring_with_cid, &event_arg_caller_id, sizeof(event_arg_caller_id));

    callback_timer_set(cptr, "fake call", &csptr->timeout_fakecall, DEBUG_SIMULATE_CALL_RING_DURATION,
                       callback_timer_to_stop_fakecall, fsm);
}

#endif

///////////////////////////////////////////////////////////////////////////////
// fake mwi for debug
///////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_SIMULATE_MWI

static void callback_timer_to_toggle_fakemwi(struct callback_timer_entry_t* ent, uInt32 elapsed_time, void *ref)
{
    struct fsm_t* fsm = (struct fsm_t*)ref;
    struct chan_state_t* csptr = (struct chan_state_t*)fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    struct event_arg_mwi_t event_arg_mwi;

    syslog(LOG_DEBUG, "[fake mwi] callback_timer_to_toggle_fakemwi() triggered (fakemwi=%d)", csptr->fakemwi);

    /* initiate caller id event argument */
    event_arg_mwi.state = csptr->fakemwi;
    fsm_post_event(fsm, fxs_event_mwi , &event_arg_mwi, sizeof(event_arg_mwi));

    if (!csptr->fakemwi)
        callback_timer_set(cptr, "fake mwi", &csptr->timeout_fakemwi, DEBUG_SIMULATE_MWI_INTERVAL,
                           callback_timer_to_toggle_fakemwi,
                           fsm);

    csptr->fakemwi = !csptr->fakemwi;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// fake cwi for debug
///////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_SIMULATE_CWI

static void callback_timer_to_do_fakecwi(struct callback_timer_entry_t* ent, uInt32 elapsed_time, void *ref)
{
    struct fsm_t* fsm = (struct fsm_t*)ref;

    struct event_arg_caller_id_t event_arg_caller_id;

    syslog(LOG_DEBUG, "[fake caller] callback_timer_to_start_fakecall() triggered");

    /* initiate caller id event argument */
    event_arg_caller_id.month = 12;
    event_arg_caller_id.day_of_month = 31;
    event_arg_caller_id.hour = 13;
    event_arg_caller_id.minute = 59;
    event_arg_caller_id.cid_num = "1234567";
    event_arg_caller_id.cid_name = "abcdef";

    fsm_perform_switch_state_by_event(fsm, fxs_event_waiting_with_cid, &event_arg_caller_id, sizeof(event_arg_caller_id),
                                      fxs_state_wcid_init);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// dialplan registered functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief dial plan function - "private_call'.
 *
 * @param func_name is a dial plan function name.
 * @param opt is option string.
 * @param ref is reference.
 *
 * @return always 0.
 */
static int diaplan_on_private_call(const char* func_name, const char* opt, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct event_arg_place_call_t event_arg_place_call;

    syslog(LOG_DEBUG, "private_call: post fxs_event_place_call (opt='%s')", opt);

    event_arg_place_call.call_type = call_type_normal;
    event_arg_place_call.num = opt;
    event_arg_place_call.oir_type = OIR_INVOCATION;

    fsm_post_event(fsm, fxs_event_place_call, (void*) &event_arg_place_call, sizeof(event_arg_place_call));

    return 0;
}

/**
 * @brief dial plan function - "public_call'.
 *
 * @param func_name is a dial plan function name.
 * @param opt is option string.
 * @param ref is reference.
 *
 * @return always 0.
 */
static int diaplan_on_public_call(const char* func_name, const char* opt, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct event_arg_place_call_t event_arg_place_call;

    syslog(LOG_DEBUG, "public_call: post fxs_event_place_call (opt='%s')", opt);

    event_arg_place_call.call_type = call_type_normal;
    event_arg_place_call.num = opt;
    event_arg_place_call.oir_type = OIR_SUPPRESSION;

    fsm_post_event(fsm, fxs_event_place_call, (void*) &event_arg_place_call, sizeof(event_arg_place_call));

    return 0;
}

/**
 * @brief dial plan function - "call'.
 *
 * @param func_name is a dial plan function name.
 * @param opt is option string.
 * @param ref is reference.
 *
 * @return always 0.
 */
static int diaplan_on_call(const char* func_name, const char* opt, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct event_arg_place_call_t event_arg_place_call;

    syslog(LOG_DEBUG, "call: post fxs_event_place_call (opt='%s')", opt);

    event_arg_place_call.call_type = call_type_normal;
    event_arg_place_call.num = opt;
    event_arg_place_call.oir_type = OIR_NONE;

    fsm_post_event(fsm, fxs_event_place_call, (void*) &event_arg_place_call, sizeof(event_arg_place_call));

    return 0;
}

/**
 * @brief dial plan function - "voicemail'.
 *
 * @param func_name is a dial plan function name.
 * @param opt is option string.
 * @param ref is reference.
 *
 * @return
 */
static int dialplan_on_voicemail(const char* func_name, const char* opt, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct event_arg_place_call_t event_arg_place_call;
    const char* mbdn;

    /* get MBDN */
    mbdn = strdupa(ezrdb_get_str(RDB_MBDN));
    if (!*mbdn) {
        syslog(LOG_ERR, "voice mail number not found (rdb='%s')", RDB_MBDN);
        goto err;
    }

    syslog(LOG_DEBUG, "post fxs_event_place_call (mbdn='%s')", mbdn);

    event_arg_place_call.call_type = call_type_normal;
    event_arg_place_call.num = mbdn;
    event_arg_place_call.oir_type = OIR_NONE;

    fsm_post_event(fsm, fxs_event_place_call, (void*) &event_arg_place_call, sizeof(event_arg_place_call));

    return 0;

err:
    return -1;
}

/**
 * @brief dial plan function - "emergency call'.
 *
 * @param func_name is a dial plan function name.
 * @param opt is option string.
 * @param ref is reference.
 *
 * @return
 */
static int diaplan_on_emergency_call(const char* func_name, const char* opt, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct event_arg_place_call_t event_arg_place_call;

    syslog(LOG_DEBUG, "post fxs_event_place_call (emergency='%s')", opt);

    event_arg_place_call.call_type = call_type_emergency;
    event_arg_place_call.num = opt;
    event_arg_place_call.oir_type = OIR_NONE;

    fsm_post_event(fsm, fxs_event_place_call, (void*) &event_arg_place_call, sizeof(event_arg_place_call));

    return 0;
}

/**
 * @brief dial plan function - "suppl_forward'.
 *
 * @param func_name is a dial plan function name.
 * @param opt is option string.
 * @param ref is reference.
 *
 * @return
 */
static int dialplan_on_suppl_forward(const char* func_name, const char* opt, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct event_arg_suppl_t event_arg_suppl;

    syslog(LOG_DEBUG, "call forwarding (opt='%s')", opt);

    event_arg_suppl.opt = opt;

    fsm_post_event(fsm, fxs_event_suppl, &event_arg_suppl, sizeof(event_arg_suppl));

    return 0;
}

/**
 * @brief dial plan function - "suppl_forward_reg'.
 *
 * @param func_name is a dial plan function name.
 * @param opt is option string.
 * @param ref is reference.
 *
 * @return
 */
static int dialplan_on_suppl_forward_reg(const char* func_name, const char* opt, void* ref)
{
    char num[MAX_NUM_OF_PHONE_NUM + 1];
    char new_opt[DIALPLAN_MAX_PLAN_LEN];
    const char *new_num;
    int stat;
    int numlen;

    syslog(LOG_DEBUG, "call forwarding register (opt='%s')", opt);

    /* retrieve phone number from command, e.g. "NOANSWER REGISTER 01113334444" */
    regmatch_t matches[1];
    stat = regexec(&_phone_num_regex, opt, 1, matches, 0);
    if (stat != REG_NOERROR) {
        syslog(LOG_ERR, "failed in matching phone number regular express for opt (opt=%s,err=%s)", opt, strerror(errno));
        return -1;
    }

    numlen = matches[0].rm_eo - matches[0].rm_so;
    if (numlen > MAX_NUM_OF_PHONE_NUM) {
        syslog(LOG_ERR, "number size exceeding maximum size (numlen=%d,max=%d)", numlen, MAX_NUM_OF_PHONE_NUM);
        return -1;
    }
    strncpy(num, opt + matches[0].rm_so, numlen);
    num[numlen] = '\0';

    /* update international call prefix of phone number */
    new_num = call_prefix_get_outgoing_num_for_network(num);

    /* update command with the new number */
    snprintf(new_opt, DIALPLAN_MAX_PLAN_LEN, "%.*s%s", matches[0].rm_so, opt, new_num);

    return dialplan_on_suppl_forward(func_name, new_opt, ref);
}

/**
 * @brief dial plan function - "set_tty_mode'.
 *
 * @param func_name is a dial plan function name.
 * @param opt is option string.
 * @param ref is reference.
 *
 * @return
 */
static int dialplan_on_set_tty_mode(const char* func_name, const char* opt, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct event_arg_suppl_t event_arg_suppl;

    syslog(LOG_DEBUG, "call tty mode (opt='%s')", opt);

    event_arg_suppl.opt = opt;

    fsm_post_event(fsm, fxs_event_set_tty_mode, &event_arg_suppl, sizeof(event_arg_suppl));

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// state machine call-back functions
///////////////////////////////////////////////////////////////////////////////

/*

	state machine call-back function types

	# type 1 - replay the identical event into the new state

		   eventA           eventA
			/                /
		   /                /
		stateA  ------>  stateB

	# type 2 - reproduce a new event for the new state

		   eventA           eventB
			/                /
		   /                /
		stateA  ------>  stateB

	# type 3 - no additional event after transit.

		   eventA
			/
		   /
		stateA  ------>  stateB

*/

///////////////////////////////////////////////////////////////////////////////
// MO (mobile originated) call states
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief FSM state enter action - 'suppl'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_suppl_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
                                       int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    /* stop tone */
    slic_stop_pcm(cptr);
    softtonegen_stop(&csptr->stg);
}

/**
 * @brief FSM state 'suppl'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_suppl(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_idle,
        [fxs_event_suppl_done] = fxs_state_prereorder,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {

            case fxs_event_hookflash: {

                if (cc_is_any_call_active()) {
                    syslog(LOG_DEBUG, "active call found, switch to 'connected'");
                    state = fxs_state_connected;
                }
                break;
            }

            case fxs_event_suppl_ack: {
                struct event_arg_suppl_ack_t* event_arg_noti = event_arg;
                int tone_type;

                if (event_arg_noti->res == event_arg_suppl_ack_res_ok) {
                    tone_type = softtonegen_type_confirmation;
                } else {
                    tone_type = softtonegen_type_error;
                }

                softtonegen_play_soft_tone(&csptr->stg, tone_type, FALSE, NULL, NULL);
                syslog(LOG_DEBUG, "received SUPPL FORWARD RESULT (res=%d)", event_arg_noti->res);

                fsm_post_event(fsm, fxs_event_suppl_stop_tone, NULL, 0);
                break;
            }

            case fxs_event_suppl_stop_tone: {
                fxs_schedule_event_post(fsm, fxs_event_suppl_done, DURATION_SUPPL_SILENCE);
                break;
            }

        }

        if (state > 0) {
            fxs_schedule_event_cancel(fsm);
            softtonegen_stop(&csptr->stg);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state entry action - 'idle'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_idle_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    /* enable tone detection */
    netcomm_proslic_enable_dtmf_detection(cptr, TRUE);
    /* 2 cycle for DTMF pass detection */
    netcomm_proslic_reg_set_with_mask(cptr, PROSLIC_REG_TONEN, 2 << 4, PROSLIC_REG_TONEN_DTMF_PASS);

    /* stop tone */
    slic_stop_pcm(cptr);
    softtonegen_stop(&csptr->stg);

    netcomm_proslic_set_linefeed(cptr, LF_FWD_ACTIVE);

    /* cancel timers */
    callback_timer_cancel(&csptr->timeout_dialtone);

    /* engage fake call */
    #ifdef DEBUG_SIMULATE_CALL
    syslog(LOG_DEBUG, "[fake caller] set timer to start fake call");
    callback_timer_set(cptr, "fake call", &csptr->timeout_fakecall, DEBUG_SIMULATE_CALL_RING_INIT_DELAY,
                       callback_timer_to_start_fakecall, fsm);
    #endif

    #ifdef DEBUG_SIMULATE_MWI
    syslog(LOG_DEBUG, "[fake caller] set timer to start fake mwi");
    callback_timer_set(cptr, "fake mwi", &csptr->timeout_fakemwi, DEBUG_SIMULATE_MWI_INTERVAL,
                       callback_timer_to_toggle_fakemwi,
                       fsm);
    #endif

    fsm_post_event(fsm, fxs_event_voicemail_stat_chg, NULL, 0);
}

/**
 * @brief FSM state - 'idle'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_idle(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_offhook] = fxs_state_dialtone,
        [fxs_event_ring_with_cid] = fxs_state_ringing_1st,
        [fxs_event_ring] = fxs_state_ringing,
        [fxs_event_mwi] = fxs_state_init_mwi,
        [fxs_event_force_connect] = fxs_state_force_connected,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {
            case fxs_event_voicemail_stat_chg: {
                int vm_active;
                int vm_count;
                int mwi_state;

                struct event_arg_mwi_t event_arg_mwi;

                /* get mwi state */
                vm_active = ezrdb_get_int(RDB_VOICEMAIL_ACTIVE);
                vm_count = ezrdb_get_int(RDB_VOICEMAIL_COUNT);
                mwi_state = vm_active && (vm_count > 0);

                if (!csptr->mwi_state_valid || (csptr->mwi_state && !mwi_state) || (!csptr->mwi_state && mwi_state)) {
                    syslog(LOG_DEBUG, "MWI state changed (old=%d,new=%d)", csptr->mwi_state, mwi_state);

                    /* initiate caller id event argument */
                    event_arg_mwi.state = mwi_state;
                    fsm_post_event(fsm, fxs_event_mwi, &event_arg_mwi, sizeof(event_arg_mwi));
                }

                break;
            }

            case fxs_event_force_connect: {
                syslog(LOG_ERR, "!!!! test mode !!!! : enter force_connected");
                break;
            }
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }

}

/**
 * @brief FSM state enter action - 'dialtone'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_dialtone_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    slic_stop_pcm(cptr);

    if (csptr->mwi_state) {
        syslog(LOG_DEBUG, "voice mail found, apply stutter dialtone");
        softtonegen_play_hard_tone(&csptr->stg, TONEGEN_FCC_DIAL_STUTTER);
    } else {
        syslog(LOG_DEBUG, "no voice mail found, apply dialtone");
        softtonegen_play_hard_tone(&csptr->stg, TONEGEN_FCC_DIAL);
    }

    syslog(LOG_DEBUG, "set dial stutter timeout callback %d sec", DURATION_STUTTER);
    fxs_schedule_event_post(fsm, fxs_event_stutter_timeout, DURATION_STUTTER);

    /* post service stat */
    fsm_post_event(fsm, fxs_event_service_stat_chg, NULL, 0);

    #ifdef DEBUG_SIMULATE_CWI
    syslog(LOG_DEBUG, "[fake cwi] set timer to start fake cwi");
    callback_timer_set(cptr, "fake cwi", &csptr->timeout_fakecwi, DEBUG_SIMULATE_CWI_INTERVAL, callback_timer_to_do_fakecwi,
                       fsm);
    #endif

}

/**
 * @brief switch to a waiting or held call if possible.
 *
 * @param fsm is a FSM object.
 * @param 0 when it succeeds. Otherwise, -1.
 */
static int do_hold_active_accept_waiting_or_held(struct fsm_t* fsm)
{
    struct call_track_t* call_waiting;

    call_waiting = cc_get_waiting_call();

    if (call_waiting) {
        cc_associate_call_with_fsm(fsm, call_waiting);
        syslog(LOG_DEBUG, "waiting call detected, switch to the call");
    } else if (cc_is_any_held_call()) {
        syslog(LOG_DEBUG, "held call detected, switch to the call");
    } else {
        syslog(LOG_DEBUG, "no call waiting");
        goto fini;
    }

    syslog(LOG_DEBUG, "do hold active accept waiting or held");
    rdb_manage_calls_hold_active_accept_waiting_or_held();

fini:
    return 0;
}

/**
 * @brief FSM state - 'dialtone'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_dialtone(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_digit] = fxs_state_digit_collect,
        [fxs_event_digit_timeout_long] = fxs_state_reorder,
        [fxs_event_fgcall_changed] = fxs_state_connected,
        [fxs_event_force_connect] = fxs_state_force_connected,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {
            case fxs_event_hookflash: {
                do_hold_active_accept_waiting_or_held(fsm);
                break;
            }

            case fxs_event_stutter_timeout: {
                softtonegen_play_hard_tone(&csptr->stg, TONEGEN_FCC_DIAL);

                syslog(LOG_DEBUG, "set dialtone timeout callback %d sec", TIMEOUT_DIALTONE);
                fxs_schedule_event_post(fsm, fxs_event_digit_timeout_long, TIMEOUT_DIALTONE);
                break;
            }

            case fxs_event_service_stat_chg: {
                const char* system_mode;

                system_mode = ezrdb_get_str(RDB_SYSTEM_MODE);
                if (!*system_mode || !strcmp(system_mode, "no service") || !strcmp(system_mode, "power save")) {
                    syslog(LOG_DEBUG, "service not ready, switch to reorder (system_mode='%s')", system_mode);
                    state = fxs_state_reorder;
                }
                break;
            }
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }

}

/**
 * @brief FSM state enter action - 'digit_collect'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_digit_collect_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    slic_stop_pcm(cptr);
    softtonegen_stop(&csptr->stg);

    /* reset dial index */
    csptr->num_idx = 0;

    syslog(LOG_DEBUG, "replay digit event");
    fsm_post_event(fsm, fxs_event_digit, event_arg, event_arg_len);
}

/**
 * @brief FSM state - 'digit_collect'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_digit_collect(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    const static int states[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_place_call] = fxs_state_place_call,
        [fxs_event_suppl] = fxs_state_suppl,
        [fxs_event_set_tty_mode] = fxs_state_suppl,
        [fxs_event_dial_failure] = fxs_state_reorder,
    };

    int state;
    int dialplan_call_rc;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {

        switch (event) {

            case fxs_event_set_tty_mode: {
                struct event_arg_suppl_t* event_arg_suppl = (struct event_arg_suppl_t*) event_arg;
                rdb_set_tty_mode(event_arg_suppl->opt);
                break;
            }

            case fxs_event_suppl: {
                struct event_arg_suppl_t* event_arg_suppl = (struct event_arg_suppl_t*) event_arg;
                rdb_suppl_forward(event_arg_suppl->opt);
                break;
            }

            case fxs_event_hookflash: {

                if (cc_is_any_call_active()) {
                    syslog(LOG_DEBUG, "active call found, switch to 'connected'");
                    state = fxs_state_connected;
                }
                break;
            }

            case fxs_event_digit: {
                struct event_arg_digit_t* event_arg_digit = (struct event_arg_digit_t*) event_arg;

                syslog(LOG_DEBUG, "got fxs_event_digit (digit=%d,char='%c',hold=%d,idx=%d)", event_arg_digit->digit,
                       event_arg_digit->ch, event_arg_digit->long_press, csptr->num_idx);

                /* schedule short inter-digit timer */
                fxs_schedule_event_post(fsm, fxs_event_digit_timeout_short, TIMEOUT_INTERDIGIT_SHORT);

                /* store if digit is valid */
                if (event_arg_digit->ch) {

                    #ifdef CONFIG_ENABLE_DTMF_LONG_PRESS_VOICEMAIL
                    int rc;

                    if (!csptr->num_idx && (event_arg_digit->ch == '1') && event_arg_digit->long_press) {
                        syslog(LOG_DEBUG, "long press digit detected (ch='%c')", event_arg_digit->ch);
                        rc = dialplan_call_func_by_name("voicemail", NULL, fsm);
                        if (rc < 0) {
                            syslog(LOG_ERR, "failed to call voicemail");
                            fsm_post_event(fsm, fxs_event_dial_failure, NULL, 0);
                        }
                        break;
                    }
                    #endif

                    if (csptr->num_idx < MAX_NUM_OF_PHONE_NUM) {

                        csptr->num[csptr->num_idx++] = event_arg_digit->ch;
                        csptr->num[csptr->num_idx] = 0;

                        syslog(LOG_DEBUG, "collect (num='%s', idx=%d)", csptr->num, csptr->num_idx);

                        dialplan_match_and_call_plan(DIALPLAN_DIAL_PRIORITY_IMMEDIATELY, csptr->num, fsm, &dialplan_call_rc);
                        if (dialplan_call_rc < 0) {
                            syslog(LOG_ERR, "failed to call dialplan function");
                            fsm_post_event(fsm, fxs_event_dial_failure, NULL, 0);
                        }
                    } else {
                        syslog(LOG_DEBUG, "total number of digit exceeded");
                    }

                }

                break;
            }

            case fxs_event_digit_timeout_short: {
                syslog(LOG_DEBUG, "got short fxs_event_digit_timeout");

                /* schedule long inter-digit timer */
                fxs_schedule_event_post(fsm, fxs_event_digit_timeout_long, TIMEOUT_INTERDIGIT_LONG - TIMEOUT_INTERDIGIT_SHORT);

                if (dialplan_match_and_call_plan(DIALPLAN_DIAL_PRIORITY_SHORT, csptr->num, fsm, &dialplan_call_rc) < 0) {
                    syslog(LOG_DEBUG, "no short timer dialplan matched, apply long timer (num='%s')", csptr->num);
                }

                if (dialplan_call_rc < 0) {
                    syslog(LOG_ERR, "failed to call dialplan function in short-timeout");
                    fsm_post_event(fsm, fxs_event_dial_failure, NULL, 0);
                }

                break;
            }

            case fxs_event_digit_timeout_long: {
                struct event_arg_place_call_t event_arg_place_call;

                syslog(LOG_DEBUG, "got long fxs_event_digit_timeout");

                if (dialplan_match_and_call_plan(0, csptr->num, fsm, &dialplan_call_rc) < 0) {

                    syslog(LOG_DEBUG, "no dialplan matched, place a call (num='%s')", csptr->num);

                    event_arg_place_call.call_type = call_type_normal;
                    event_arg_place_call.num = csptr->num;
                    event_arg_place_call.oir_type = OIR_NONE;

                    fsm_post_event(fsm, fxs_event_place_call, &event_arg_place_call, sizeof(event_arg_place_call));
                }

                if (dialplan_call_rc < 0) {
                    syslog(LOG_ERR, "failed to call dialplan function in long-timeout");
                    fsm_post_event(fsm, fxs_event_dial_failure, NULL, 0);
                }

                goto fini;
            }

            case fxs_event_place_call: {
                struct event_arg_place_call_t* event_arg_place_call = (struct event_arg_place_call_t*) event_arg;
                syslog(LOG_DEBUG, "place call (num='%s')", event_arg_place_call->num);

            }

        }

        if (state > 0) {
            fxs_schedule_event_cancel(fsm);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }

fini:
    return;
}

/**
 * @brief FSM state enter action - 'place_call'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_place_call_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;
    const char* num;
    char* cid_num_pi;

    slic_stop_pcm(cptr);
    softtonegen_stop(&csptr->stg);

    syslog(LOG_DEBUG, "make mo call");

    struct event_arg_place_call_t* event_arg_dial = (struct event_arg_place_call_t*) event_arg;

    if (!*event_arg_dial->num) {
        syslog(LOG_DEBUG, "no dial number is provided");
        fsm_post_event(fsm, fxs_event_fgcall_disconnected, NULL, 0);
    } else {
        num = call_prefix_get_outgoing_num_for_network(event_arg_dial->num);

        switch (event_arg_dial->oir_type) {
            case OIR_INVOCATION:
                cid_num_pi = "RESTRICTED";
                break;

            case OIR_SUPPRESSION:
                cid_num_pi = "AVAILABLE";
                break;

            default:
                cid_num_pi = NULL;
                break;

        }
        cc_make_mo_call(fsm, event_arg_dial->call_type, num, cid_num_pi, NULL, NULL, RWPIPE_OUTGOING_CALL_TIMEOUT, FALSE);
        fxs_schedule_event_post(fsm, fxs_event_place_call_timeout, TIMEOUT_PLACE_CALL);
    }
}

/**
 * @brief FSM state - 'place_call'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_place_call(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_ringback_remote] = fxs_state_ringback_remote,
        [fxs_event_ringback_local] = fxs_state_ringback_local,
        [fxs_event_fgcall_disconnected] = fxs_state_reorder,
        [fxs_event_connected] = fxs_state_connected,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {

            case fxs_event_place_call_timeout: {

                syslog(LOG_DEBUG, "place call response timeout");

                if (!csptr->foreground_call) {
                    syslog(LOG_ERR, "failed to get foreground call");
                    break;
                }

                cc_hangup_call(csptr->foreground_call);

                syslog(LOG_DEBUG, "place_call_timeout, immediately disconnect foreground call");
                fsm_post_event(fsm, fxs_event_fgcall_disconnected, NULL, 0);
                break;
            }
        }

        if (state > 0) {
            fxs_schedule_event_cancel(fsm);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'ringback remote'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringback_remote_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    slic_start_pcm(cptr);
    softtonegen_stop(&csptr->stg);
}

/**
 * @brief FSM sub function only for Tx DTMF processing 'connected', 'ringback remote' and 'ringback local'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_subfunc_process_tx_dtmf(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    #ifdef CONFIG_MUTE_TX_DTMF
    SiVoiceChanType_ptr cptr = csptr->cptr;
    #endif

    struct call_track_t* foreground_call;
    struct event_arg_digit_pressed_t* event_arg_digit_pressed = (struct event_arg_digit_pressed_t*)event_arg;

    syslog(LOG_DEBUG, "[TX-DTMF] DTMF %s (ch='%c')", event_arg_digit_pressed->pressed ? "pressed" : "released",
           event_arg_digit_pressed->ch);

    if (csptr->during_rx_dtmf) {
        syslog(LOG_DEBUG, "[TX-DTMF] RX-DTMF already started, ignore TX-DTMF");
        goto fini_tx_dtmf;
    }

    #ifdef CONFIG_MUTE_TX_DTMF
    syslog(LOG_DEBUG, "[TX-DTMF] %s TX DTMF (ch='%c')", event_arg_digit_pressed->pressed ? "mute" : "unmute",
           event_arg_digit_pressed->ch);
    netcomm_proslic_mute_tx(cptr, event_arg_digit_pressed->pressed);
    #else
    syslog(LOG_DEBUG, "[TX-DTMF] SLIC DTMF mute feature is disabled. ignore '%s' (ch='%c')",
           event_arg_digit_pressed->pressed ? "mute" : "unmute",
           event_arg_digit_pressed->ch);
    #endif

    foreground_call = csptr->foreground_call;
    if (!foreground_call) {
        syslog(LOG_ERR, "[TX-DTMF] foreground call not specified");
    } else {
        if (event_arg_digit_pressed->pressed) {
            #ifdef CONFIG_TX_OOB_DTMF
            syslog(LOG_DEBUG, "[TX-DTMF] start QMI DTMF (ch='%c')", event_arg_digit_pressed->ch);
            rdb_start_dtmf(foreground_call->call_track_id, event_arg_digit_pressed->ch);
            #else
            syslog(LOG_DEBUG, "[TX-DTMF] OOB TX DTMF feature is disabled. ignore 'start QMI DTMF' (ch='%c')",
                   event_arg_digit_pressed->ch);
            #endif

            /* set uplink dtmf flag */
            csptr->during_tx_dtmf = TRUE;
        } else {
            #ifdef CONFIG_TX_OOB_DTMF
            syslog(LOG_DEBUG, "[TX-DTMF] stop QMI DTMF");
            rdb_stop_dtmf(foreground_call->call_track_id);
            #else
            syslog(LOG_DEBUG, "[TX-DTMF] OOB TX DTMF feature is disabled. ignore 'stop QMI DTMF'");
            #endif

            /* clear uplink dtmf flag */
            csptr->during_tx_dtmf = FALSE;
        }
    }

fini_tx_dtmf:
    return;
}

/**
 * @brief FSM state - 'ringback remote'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringback_remote(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_ringback_remote] = fxs_state_ringback_remote,
        [fxs_event_ringback_local] = fxs_state_ringback_local,
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_connected] = fxs_state_connected,
        [fxs_event_fgcall_disconnected] = fxs_state_reorder,
    };

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {
            case fxs_event_digit_pressed: {
                fxs_state_subfunc_process_tx_dtmf(fsm, state_idx, event, event_arg, event_arg_len);
                break;
            }
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'ringback local'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringback_local_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    slic_stop_pcm(cptr);
    softtonegen_play_hard_tone(&csptr->stg, TONEGEN_FCC_RINGBACK);
}

/**
 * @brief FSM state - 'ringback local'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringback_local(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_ringback_remote] = fxs_state_ringback_remote,
        [fxs_event_ringback_local] = fxs_state_ringback_local,
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_connected] = fxs_state_connected,
        [fxs_event_fgcall_disconnected] = fxs_state_reorder,
    };

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {
            case fxs_event_digit_pressed: {
                fxs_state_subfunc_process_tx_dtmf(fsm, state_idx, event, event_arg, event_arg_len);
                break;
            }
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'prereorder'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_prereorder_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    slic_stop_pcm(cptr);

    fxs_schedule_event_post(fsm, fxs_event_prereorder_timeout, TIMEOUT_PREREORDER);
}

/**
 * @brief FSM state - 'reorder'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_prereorder(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_fgcall_changed] = fxs_state_connected,
        [fxs_event_prereorder_timeout] = fxs_state_reorder,
    };

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {

        switch (event) {
            case fxs_event_hookflash: {
                do_hold_active_accept_waiting_or_held(fsm);
                break;
            }
        }

        if (state > 0) {
            fxs_schedule_event_cancel(fsm);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'reorder'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_reorder_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    slic_stop_pcm(cptr);
    softtonegen_play_hard_tone(&csptr->stg, TONEGEN_FCC_REORDER);

    fxs_schedule_event_post(fsm, fxs_event_roh_timeout, TIMEOUT_ROH);
}

/**
 * @brief FSM state - 'reorder'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_reorder(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_fgcall_changed] = fxs_state_connected,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {
            case fxs_event_hookflash: {
                do_hold_active_accept_waiting_or_held(fsm);
                break;
            }

            case fxs_event_roh_timeout: {
                syslog(LOG_DEBUG, "ROH timeout");
                softtonegen_play_hard_tone(&csptr->stg, TONEGEN_FCC_ROH);
                break;
            }

        }

        if (state > 0) {
            fxs_schedule_event_cancel(fsm);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'connected'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_connected_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;
    struct softtonegen_t* stg = &csptr->stg;

    if (state_idx == fxs_state_force_connected) {
        syslog(LOG_ERR, "!!!! test mode !!!! : start force_connected");
    }

    /* reset tx and rx dtmf */
    csptr->during_tx_dtmf = FALSE;
    csptr->during_rx_dtmf = FALSE;

    /* enable tone detection */
    netcomm_proslic_enable_dtmf_detection(cptr, TRUE);

    slic_start_pcm(cptr);
    softtonegen_stop(stg);

    #ifdef CONFIG_SHORT_DTMF_DETECT
    netcomm_proslic_reg_set_with_mask(cptr, PROSLIC_REG_TONEN, 1 << 4, PROSLIC_REG_TONEN_DTMF_PASS);
    #endif

    /* play callwaiting if call waiting is rinnging */
    if (cc_is_waiting_call_ringing()) {
        softtonegen_play_soft_tone(stg, softtonegen_type_callwaiting, TRUE, NULL, NULL);
    }

    /* unmute TX */
    netcomm_proslic_mute_tx(cptr, FALSE);
    /* unmute RX */
    netcomm_proslic_mute_rx(cptr, FALSE);
}

/**
 * @brief clears incall digit.
 *
 * @param ent is a call back timer entry.
 * @param elapsed_time is elapsed time.
 * @param ref is reference.
 */
static void callback_timer_to_clear_last_incall_num(struct callback_timer_entry_t* ent, uInt32 elapsed_time, void *ref)
{
    struct fsm_t* fsm = (struct fsm_t*) ref;
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    /* clear last incall number */
    csptr->last_incall_num = 0;

    syslog(LOG_DEBUG, "last incall number expired");

}

/**
 * @brief FSM state - 'connected'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_connected(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states_for_connected[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_fgcall_disconnected] = fxs_state_prereorder,
        [fxs_event_waiting_with_cid] = fxs_state_wcid_init,
    };

    const static int states_for_force_connected[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_fgcall_disconnected] = fxs_state_disconnect,
        [fxs_event_waiting_with_cid] = fxs_state_wcid_init,
    };

    struct states_collection_t states_collection[] = {
        [fxs_state_connected] = {states_for_connected, __countof(states_for_connected)},
        [fxs_state_force_connected] = {states_for_force_connected, __countof(states_for_force_connected)},
    };

    const int* states = states_collection[state_idx].states;
    const int states_count = states_collection[state_idx].states_count;

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;
    struct softtonegen_t* stg = &csptr->stg;

    int state;

    struct call_track_t* call_waiting;
    struct call_track_t* call_conference;
    struct call_track_t* call_emergency;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, states_count);
    {
        /*

         AT&T Call Waiting

         You will hear two tones if someone calls while you are already on a call. When this happens, you have several options:

         • To hang up on the first call and connect the incoming call, press the 1 key and then the Flash (or Phone/Talk) key.
         • To continue the first call and reject the incoming call, press the 0 key and then the Flash (or Phone/Talk) key.
         • To place the first call on hold and connect the incoming call, press the 2 key and then the Flash (or Phone/Talk) key.

         */

        switch (event) {
            case fxs_event_fgcall_disconnected: {
                if (state_idx == fxs_state_force_connected) {
                    syslog(LOG_ERR, "!!!! test mode !!!! : exit force_connected");
                }
                break;
            }

            case fxs_event_onhook: {
                if (state_idx == fxs_state_force_connected) {
                    syslog(LOG_ERR, "!!!! test mode !!!! : hang up force_connected");
                }
                break;
            }

            case fxs_event_offhook: {
                if (state_idx == fxs_state_force_connected) {
                    syslog(LOG_ERR, "!!!! test mode !!!! : ignore off-hook");
                }
                break;
            }

            case fxs_event_rx_dtmf: {

                struct event_arg_rx_dtmf_t* event_arg_rx_dtmf = (struct event_arg_rx_dtmf_t*)event_arg;

                syslog(LOG_DEBUG, "[RX-DTMF] DTMF %s ch='%c'", !event_arg_rx_dtmf->start_or_end ? "start" : "end",
                       event_arg_rx_dtmf->ch);

                if (csptr->during_tx_dtmf) {
                    syslog(LOG_DEBUG, "[RX-DTMF] TX-DTMF already started, ignore RX-DTMF");
                    goto fini_rx_dtmf;
                }

                if (!event_arg_rx_dtmf->start_or_end) {

                    /* mute tx */
                    syslog(LOG_DEBUG, "[RX-DTMF] mute SLIC TX before RX-DTMF (ch='%c')", event_arg_rx_dtmf->ch);
                    netcomm_proslic_mute_tx(cptr, TRUE);

                    syslog(LOG_DEBUG, "[RX-DTMF] start SLIC DTMF tone (ch='%c')", event_arg_rx_dtmf->ch);

                    /* disable tone detection */
                    netcomm_proslic_enable_dtmf_detection(cptr, FALSE);
                    /* play dtmf */
                    softtonegen_play_dtmf_tone(stg, event_arg_rx_dtmf->ch);

                    csptr->during_rx_dtmf = TRUE;
                } else {
                    syslog(LOG_DEBUG, "[RX-DTMF] stop SLIC DTMF tone (ch='%c')", event_arg_rx_dtmf->ch);

                    /* stop dtmf */
                    softtonegen_stop(stg);
                    /* enable tone detection */
                    netcomm_proslic_enable_dtmf_detection(cptr, TRUE);

                    /* unmute tx */
                    syslog(LOG_DEBUG, "[RX-DTMF] unmute SLIC TX after RX-DTMF (ch='%c')", event_arg_rx_dtmf->ch);
                    netcomm_proslic_mute_tx(cptr, FALSE);

                    csptr->during_rx_dtmf = FALSE;
                }

fini_rx_dtmf:
                break;
            }

            case fxs_event_digit_pressed: {
                fxs_state_subfunc_process_tx_dtmf(fsm, state_idx, event, event_arg, event_arg_len);
                break;
            }

            case fxs_event_waiting: {

                syslog(LOG_DEBUG, "start call waiting ring");
                softtonegen_play_soft_tone(stg, softtonegen_type_callwaiting, TRUE, NULL, NULL);
                break;
            }

            case fxs_event_waiting_with_cid: {
                syslog(LOG_DEBUG, "start call waiting ring with cid");
                break;
            }

            case fxs_event_stop_ringing: {
                syslog(LOG_DEBUG, "stop call waiting ring");
                softtonegen_stop(stg);
                break;
            }

            case fxs_event_digit: {
                struct event_arg_digit_t* event_arg_digit = (struct event_arg_digit_t*) event_arg;

                /* store last incall num */
                csptr->last_incall_num = event_arg_digit->ch;
                /* schedule timer to expire */
                callback_timer_set(cptr, "last_incall_num", &csptr->timeout_last_incall_num,
                                   TIMEOUT_INCALL_SINGLE_DIGIT, callback_timer_to_clear_last_incall_num, fsm);

                syslog(LOG_DEBUG, "got last incall number (ch='%c')", csptr->last_incall_num);
                break;
            }

            case fxs_event_hookflash: {
                int last_incall_num = csptr->last_incall_num;
                syslog(LOG_DEBUG, "hookflash in conversation (ch='%c')",
                       csptr->last_incall_num ? csptr->last_incall_num : ' ');

                /* get waiting call */
                call_waiting = cc_get_waiting_call();
                /* get conference call */
                call_conference = cc_get_call_by_call_type(call_type_conference);
                /* get emergency call */
                call_emergency = cc_get_call_by_call_type(call_type_emergency);

                if (call_emergency) {
                    /* No Supplementary Services in Emergency Calls */
                    csptr->last_incall_num = 0;
                    syslog(LOG_DEBUG, "[hookflash] emergency call found, ignore any supplementary");
                } else if (csptr->last_incall_num == 0) {

                    syslog(LOG_DEBUG, "[hookflash] pressed with no prior digit");

                    if (call_waiting) {
                        syslog(LOG_DEBUG, "[hookflash] waiting call found. press '2' - hold_active_accept_waiting_or_held");
                        last_incall_num = '2';

                    } else if (!cc_is_any_call_active()) {
                        syslog(LOG_DEBUG, "[hookflash] no active call found, press '2' - hold_active_accept_waiting_or_held");
                        last_incall_num = '2';

                    } else if (cc_is_any_held_call()) {

                        if (csptr->perform_conference_when_hookflash) {
                            syslog(LOG_DEBUG, "[hookflash] held call found with last outgoing call . press '3' - make conference");
                            last_incall_num = '3';
                        } else {
                            syslog(LOG_DEBUG,
                                   "[hookflash] held call found with last incoming call. press '2' - hold_active_accept_waiting_or_held");
                            last_incall_num = '2';
                        }
                    } else {
                        syslog(LOG_DEBUG, "[hookflash] no hookflash condition matched, start dialtone");

                        /* hold the active call */
                        rdb_manage_calls_hold_active_accept_waiting_or_held();

                        /* remove foreground call */
                        cc_set_foreground_call(fsm, NULL);

                        state = fxs_state_dialtone;
                    }
                }

                switch (last_incall_num) {
                    /* hang up active call and accept waiting call */
                    case '1': {
                        if (call_waiting) {
                            cc_associate_call_with_fsm(fsm, call_waiting);
                        }

                        if (cc_is_any_held_call() || call_waiting) {
                            syslog(LOG_DEBUG, "'1' pressed, hang up active call and accept waiting call");
                            rdb_manage_calls_release_active_acept_held_or_waiting();
                        } else {
                            syslog(LOG_DEBUG, "no held or waiting call found, ignore '1'");
                        }

                        break;
                    }

                    /* reject waiting call */
                    case '0': {
                        int cid;

                        if (call_waiting) {
                            cid = call_waiting->call_track_id;
                            syslog(LOG_DEBUG, "'0' pressed, reject call (cid=%d)", cid);
                            rdb_hangup(cid, NULL);
                        }
                        break;
                    }

                    /* hold active call and accept waiting call */
                    case '2': {
                        syslog(LOG_DEBUG, "'2' pressed, hold active call and accept waiting call");
                        do_hold_active_accept_waiting_or_held(fsm);
                        break;
                    }

                    case '3': {
                        if (cc_get_connected_or_held_call_count() > 1) {
                            syslog(LOG_DEBUG, "'3' pressed, start conference");
                            cc_make_mo_call(fsm, call_type_conference, "CONFERENCE CALL", NULL, NULL, NULL, RWPIPE_OUTGOING_CALL_TIMEOUT,
                                            call_conference != NULL);
                        } else {
                            syslog(LOG_DEBUG, "more than 1 conference participants required, ignore '3'");
                        }
                        break;
                    }

                }

                /* clear last num */
                csptr->last_incall_num = 0;
                break;
            }

        }

        if (state > 0) {
            #ifdef CONFIG_SHORT_DTMF_DETECT
            netcomm_proslic_reg_set_with_mask(cptr, PROSLIC_REG_TONEN, 2 << 4, PROSLIC_REG_TONEN_DTMF_PASS);
            #endif

            /* unmute TX */
            netcomm_proslic_mute_tx(cptr, FALSE);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

///////////////////////////////////////////////////////////////////////////////
// caller id management functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief calculates caller ID checksum.
 *
 * @param str is caller ID string.
 * @param length is length of caller ID string.
 *
 * @return
 */
static uInt8 caller_id_checkSum(uInt8 * str, int length)
{
    int i = 0;
    uInt8 sum = 0;

    while (i++ < length) {
        sum += *str++;
    }

    return -sum;
}

/**
 * @brief set visual MWI into caller id buffer.
 *
 * @param csptr is a channel state object.
 * @param on is visual MWI state. (0=off, 1=on)
 * @param cid is visual MWI buffer.
 * @param cid_len is visual MWI buffer length.
 *
 * @return total number of VMWI bytes when it succeeds. Otherwise, -1.
 */

static int caller_id_set_vmwi(struct chan_state_t* csptr, int on)
{
    struct caller_id_t* caller_id = &csptr->caller_id_to_send;
    char* cid;
    int cid_len;

    struct sdmf_vmwi_t* vmwi;
    struct sdmf_trailer_t* trailer;

    int vmwi_len;

    /* get caller id members */
    cid = caller_id->cid;
    cid_len = sizeof(caller_id->cid);

    vmwi = (struct sdmf_vmwi_t*) cid;
    trailer = (struct sdmf_trailer_t*)(vmwi + 1);

    /* bypass if not enough space */
    if (cid_len < sizeof(*vmwi) + sizeof(*trailer)) {
        goto err;
    }

    char state = on ? SDMF_VMWI_STATE_ON : SDMF_VMWI_STATE_OFF;

    /* build vmwi fsk packet */
    vmwi->message_type = FSK_CALLER_ID_MESSAGE_TYPE_SDMF_VMWI;
    vmwi->length = sizeof(*vmwi) - (sizeof(vmwi->message_type) + sizeof(vmwi->length));
    vmwi->state[0] = state;
    vmwi->state[1] = state;
    vmwi->state[2] = state;
    trailer->checksum = caller_id_checkSum((uInt8*) vmwi, sizeof(*vmwi));

    vmwi_len = sizeof(*vmwi) + sizeof(*trailer);

    /* set caller id members */
    caller_id->idx = 0;
    caller_id->len = vmwi_len;

    /* dump log */
    syslog(LOG_DEBUG, "[FSK data] info vmwi (len=%d,chksum=0x%02x)", vmwi_len, trailer->checksum);
    log_dump_hex("[FSK data] raw vmwi", cid, caller_id->len);

    return vmwi_len;

err:
    return -1;
}

/**
 * @brief gets preamble bytes.
 *
 * @param csptr is a channel state object.
 * @param cid is buffer for preamble bytes.
 * @param cid_len is buffer length.
 * @param preamble to get preamble, this preamble flag needs to be set. otherwise, the function will get cid.
 * @param timestamp  msec time-stamp is written to this pointer.
 *
 * @return total number of preamble bytes.
 */
static int caller_id_get_preamble_or_cid(struct chan_state_t* csptr, char* cid, int cid_len, uint64_t* timestamp,
        int preamble)
{
    int len;
    int rest;
    uint64_t now = get_monotonic_msec();
    uint64_t elapsed = now - csptr->fsk_fifo_timestamp;
    struct caller_id_t* caller_id = &csptr->caller_id_to_send;

    /* we prepare the whole buffer since we may need to send padding */
    if (preamble) {
        memset(cid, 0x55, cid_len);
        rest = FSK_CALLER_ID_PREAMBLE_COUNT - csptr->preamble_idx;

        syslog(LOG_DEBUG, "[caller id] get preamble (preamble_idx=%d,rest=%d)", csptr->preamble_idx, rest);
    } else {
        memset(cid, 0, cid_len);
        rest = caller_id->len - caller_id->idx;

        syslog(LOG_DEBUG, "[caller id] get (len=%d,idx=%d,rest=%d)", caller_id->len, caller_id->idx, rest);
    }

    len = min(rest, cid_len);

    /* assume that the rest of traffic is still in FIFO */
    if (csptr->fsk_fifo_written) {
        len = min(len, max(FSK_CALLER_ID_MIN_WRITE_SIZE, FSK_MSEC_TO_BYTE(elapsed)));
        syslog(LOG_DEBUG, "[FSK data] calculated timing (elapsed = %llu msec, sent=%d B)", elapsed, FSK_MSEC_TO_BYTE(elapsed));
    }
    csptr->fsk_fifo_written = TRUE;

    if (len) {
        if (preamble) {
            csptr->preamble_idx += len;
        } else {
            memcpy(cid, caller_id->cid + caller_id->idx, len);
            caller_id->idx += len;
        }
        /* add padding if len is too small */
        len = max(len, FSK_CALLER_ID_UNDERRUN_THRESHOLD);
    }

    *timestamp = now;

    return len;
}

/**
 * @brief gets preamble bytes.
 *
 * @param csptr is a channel state object.
 * @param cid is buffer for preamble bytes.
 * @param cid_len is buffer length.
 *
 * @return total number of preamble bytes.
 */
static int caller_id_get_preamble(struct chan_state_t* csptr, char* cid, int cid_len, uint64_t* timestamp)
{
    return caller_id_get_preamble_or_cid(csptr, cid, cid_len, timestamp, TRUE);
}

/**
 * @brief gets caller ID.
 *
 * @param csptr is a channel state object.
 * @param cid is buffer.
 * @param cid_len is buffer length.
 *
 * @return total number of caller ID.
 */
static int caller_id_get(struct chan_state_t* csptr, char* cid, int cid_len, uint64_t* timestamp)
{
    return caller_id_get_preamble_or_cid(csptr, cid, cid_len, timestamp, FALSE);
}

#if 0
/**
 * @brief sets caller ID for single data message format
 *
 * @param csptr is a channel state object.
 * @param month is month of time-stamp.
 * @param day_of_month is day of month of time-stamp.
 * @param hour is hour of time-stamp.
 * @param minute is minute of time-stamp.
 * @param number is a phone number.
 */
static void caller_id_set(struct chan_state_t* csptr, int month, int day_of_month, int hour, int minute,
                          const char* number)
{
    struct caller_id_t* caller_id = &csptr->caller_id_to_send;
    struct sdmf_caller_id_t* sdmf_header = (struct sdmf_caller_id_t*) caller_id->cid_num;
    struct sdmf_trailer_t* sdmf_trailer;
    int cid_timestamp_len = sizeof(sdmf_header->month) + sizeof(sdmf_header->day_of_month) + sizeof(sdmf_header->hour)
                            + sizeof(sdmf_header->minute);

    int number_len;
    int cid_number_len;

    /* get number length in cid */
    number_len = strlen(number);
    cid_number_len = sizeof(caller_id->cid_num) - sizeof(*sdmf_header) - sizeof(*sdmf_trailer);
    if (number_len < cid_number_len)
        cid_number_len = number_len;

    /* get trailer */
    sdmf_trailer = (struct sdmf_trailer_t*)(caller_id->cid_num + sizeof(*sdmf_header) + cid_number_len);

    syslog(LOG_DEBUG, "[caller id] set (num=%s,len=%d)", number, cid_number_len);

    /* initiate caller id */
    sdmf_header->message_type = FSK_CALLER_ID_MESSAGE_TYPE_SDMF_CALLER_ID;
    sdmf_header->length = sizeof(*sdmf_header) + cid_number_len
                          - (sizeof(sdmf_header->message_type) + sizeof(sdmf_header->length));
    /* set month, day of month, hour and minute */
    snprintf(sdmf_header->month, cid_timestamp_len + 1, "%02d%02d%02d%02d", month, day_of_month, hour, minute);
    /* copy number */
    memcpy(sdmf_header->number, number, cid_number_len);
    /* get checksum */
    sdmf_trailer->checksum = caller_id_checkSum((uInt8*) sdmf_header, sizeof(*sdmf_header) + cid_number_len);
    sdmf_trailer->markout = 0;

    /* set caller id members */
    caller_id->idx = 0;
    caller_id->len = sizeof(*sdmf_header) + cid_number_len + sizeof(*sdmf_trailer);
}
#endif

/**
 * @brief sets caller ID.
 *
 * @param csptr is a channel state object.
 * @param month is month of time-stamp.
 * @param day_of_month is day of month of time-stamp.
 * @param hour is hour of time-stamp.
 * @param minute is minute of time-stamp.
 * @param number is a phone number.
 */
static void caller_id_set(struct chan_state_t* csptr, int month, int day_of_month, int hour, int minute,
                          const char* cid_num, const char* cid_num_pi, const char* cid_name, const char* cid_name_pi)
{
    struct mdmf_caller_id_header_t* hdr;
    struct mdmf_caller_id_data_header_t* data_hdr;
    struct mdmf_caller_id_data_header_t* data_hdr_datetime;
    char* data_body;
    struct mdmf_caller_id_data_reason_t* data_body_reason;
    struct mdmf_caller_id_data_datetime_t* data_body_datetime;

    struct mdmf_trailer_t* tailer;

    int mdmf_len;
    int number_len;
    int name_len;

    struct caller_id_t* caller_id = &csptr->caller_id_to_send;
    char* cid = caller_id->cid;
    int idx;
    int cid_chksum_len = sizeof(caller_id->cid) - sizeof(*tailer);

    int prot;
    int unavail;

    syslog(LOG_DEBUG,
           "[cid timestamp] FSK settings (month=%d,day=%d,hour=%d,min=%d,cid_num=%s,cid_num_pi=%s,cid_name=%s,cid_name_pi=%s",
           month, day_of_month, hour, minute, cid_num, cid_num_pi, cid_name, cid_name_pi);

    /* long caller ID message for testing */
    #if 0
    static int inc_hour = 0;
    month = 3;
    day_of_month = 24;
    hour = inc_hour;
    minute = 2;

    inc_hour = (inc_hour + 1) % 24;

    cid_name = "JOHN DOE THE LONG NAME";
    cid_name_pi = "ALLOWED";
    cid_num = "80055512121123";
    cid_num_pi = "ALLOWED";
    #endif

    /* short caller ID message for testing */
    #if 0
    month = 6;
    day_of_month = 6;
    hour = 16;
    minute = 16;

    cid_name = NULL;
    cid_name_pi = "ALLOWED";
    cid_num = "00101012345612";
    cid_num_pi = "ALLOWED";
    #endif

    /* use unavailable when no PI is provided */
    if (!cid_num_pi) {
        cid_num_pi = "UNAVAILABLE";
    }

    /* use unavailable when no PI is provided */
    if (!cid_name_pi) {
        cid_name_pi = "UNAVAILABLE";
    }

    number_len = !cid_num ? 0 : strlen(cid_num);
    name_len = !cid_name ? 0 : strlen(cid_name);

    memset(caller_id, 0, sizeof(*caller_id));

    ///////////////////////////////////////////////////////////////////////////////
    // MDMF header
    ///////////////////////////////////////////////////////////////////////////////
    idx = 0;
    hdr = (struct mdmf_caller_id_header_t*)cid;
    idx += sizeof(*hdr);

    if (idx >= cid_chksum_len) {
        syslog(LOG_ERR, "no space for MDMF header");
        goto fini;
    }

    ///////////////////////////////////////////////////////////////////////////////
    // datetime
    ///////////////////////////////////////////////////////////////////////////////
    data_hdr_datetime = (struct mdmf_caller_id_data_header_t*)&cid[idx];
    idx += sizeof(*data_hdr_datetime);
    data_body_datetime = (struct mdmf_caller_id_data_datetime_t*)&cid[idx];
    idx += sizeof(*data_body_datetime);

    if (idx >= cid_chksum_len) {
        syslog(LOG_ERR, "no space for MDMF timestamp");
        goto fini;
    }

    data_hdr_datetime->data_type = MDMF_DATA_TYPE_DATETIME;
    data_hdr_datetime->length = sizeof(*data_body_datetime);
    snprintf((char*)data_body_datetime, sizeof(*data_body_datetime) + 1, "%02d%02d%02d%02d", month, day_of_month, hour,
             minute);

    ///////////////////////////////////////////////////////////////////////////////
    // name
    ///////////////////////////////////////////////////////////////////////////////

    prot = !strcmp(cid_name_pi, "RESTRICTED");
    unavail = !strcmp(cid_name_pi, "UNAVAILABLE");

    if (prot || unavail) {

        if (prot) {
            syslog(LOG_DEBUG, "cid name restricted");
        } else {
            syslog(LOG_DEBUG, "cid name unavailable");
        }

        data_hdr = (struct mdmf_caller_id_data_header_t*)&cid[idx];
        idx += sizeof(*data_hdr);
        data_body_reason = (struct mdmf_caller_id_data_reason_t*)&cid[idx];
        idx += sizeof(*data_body_reason);

        if (idx >= cid_chksum_len) {
            syslog(LOG_ERR, "no space for MDMF name reason");
            goto fini;
        }

        data_hdr->data_type = MDMF_DATA_TYPE_REASON_NO_NAME;
        data_hdr->length = sizeof(*data_body_reason);

        data_body_reason->reason = prot ? 'P' : 'O';
    } else {

        syslog(LOG_DEBUG, "cid name found (cid_name=%s)", cid_name);

        data_hdr = (struct mdmf_caller_id_data_header_t*)&cid[idx];
        idx += sizeof(*data_hdr);
        data_body = &cid[idx];
        idx += name_len;

        if (idx >= cid_chksum_len) {
            syslog(LOG_ERR, "no space for MDMF name");
            goto fini;
        }

        data_hdr->data_type = MDMF_DATA_TYPE_NAME;
        data_hdr->length = name_len;
        snprintf((char*)data_body, name_len + 1, "%s", cid_name);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // number
    ///////////////////////////////////////////////////////////////////////////////
    prot = !strcmp(cid_num_pi, "RESTRICTED");
    unavail = !strcmp(cid_num_pi, "UNAVAILABLE");

    if (prot || unavail) {

        if (prot) {
            syslog(LOG_DEBUG, "cid number restricted");
        } else {
            syslog(LOG_DEBUG, "cid number unavailable");
        }

        data_hdr = (struct mdmf_caller_id_data_header_t*)&cid[idx];
        idx += sizeof(*data_hdr);
        data_body_reason = (struct mdmf_caller_id_data_reason_t*)&cid[idx];
        idx += sizeof(*data_body_reason);

        if (idx >= cid_chksum_len) {
            syslog(LOG_ERR, "no space for MDMF number reason");
            goto fini;
        }

        data_hdr->data_type = MDMF_DATA_TYPE_REASON_NO_NUMBER;
        data_hdr->length = sizeof(*data_body_reason);

        data_body_reason->reason = prot ? 'P' : 'O';
    } else {
        syslog(LOG_DEBUG, "cid number found (num=%s,len=%d)", cid_num, number_len);

        data_hdr = (struct mdmf_caller_id_data_header_t*)&cid[idx];
        idx += sizeof(*data_hdr);
        data_body = &cid[idx];
        idx += number_len;

        if (idx >= cid_chksum_len) {
            syslog(LOG_ERR, "no space for MDMF number");
            goto fini;
        }

        data_hdr->data_type = MDMF_DATA_TYPE_PHONE_NUMBER;
        data_hdr->length = number_len;
        snprintf((char*)data_body, number_len + 1, "%s", cid_num);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // finish MDMF header
    ///////////////////////////////////////////////////////////////////////////////

    mdmf_len = idx - sizeof(*hdr);

    /* set header */
    hdr->message_type = FSK_CALLER_ID_MESSAGE_TYPE_MDMF_CALLER_ID;
    hdr->length = mdmf_len;

    ///////////////////////////////////////////////////////////////////////////////
    // tail
    ///////////////////////////////////////////////////////////////////////////////

    tailer = (struct mdmf_trailer_t*)&cid[idx];
    idx += sizeof(*tailer);

    ///////////////////////////////////////////////////////////////////////////////
    // checksum
    ///////////////////////////////////////////////////////////////////////////////

    /* set tailer */
    tailer->checksum = caller_id_checkSum((uInt8*) hdr, mdmf_len + sizeof(*hdr));
    tailer->markout = 0;

    ///////////////////////////////////////////////////////////////////////////////
    // caller information
    ///////////////////////////////////////////////////////////////////////////////

    /* set caller id members */
    caller_id->idx = 0;
    caller_id->len = sizeof(*hdr) + mdmf_len + sizeof(*tailer);

    /* dump log */
    syslog(LOG_DEBUG, "[FSK data] info cid (len=%d,chksum=0x%02x)", hdr->length, tailer->checksum);
    log_dump_hex("[FSK data] raw cid", cid, caller_id->len);

fini:
    return;
}

///////////////////////////////////////////////////////////////////////////////
// MT (mobile terminated) call states
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief FSM state enter action - 'ringing_1st'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringing_1st_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;
    struct event_arg_caller_id_t* event_arg_caller_id = (struct event_arg_caller_id_t*) event_arg;

    /* store caller id */
    syslog(LOG_DEBUG, "[incoming call] store caller id");
    caller_id_set(csptr, event_arg_caller_id->month, event_arg_caller_id->day_of_month, event_arg_caller_id->hour,
                  event_arg_caller_id->minute, event_arg_caller_id->cid_num, event_arg_caller_id->cid_num_pi,
                  event_arg_caller_id->cid_name, event_arg_caller_id->cid_name_pi);

    /* enable ring interrupt */
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_IRQEN1, PROSLIC_REG_IRQEN1_RING_TA_IE, 0);

    /* setup instance ring */
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_RINGCON, PROSLIC_REG_RINGCON_TA_EN,
                                      PROSLIC_REG_RINGCON_TI_EN);

    /* start ringing */
    netcomm_proslic_set_linefeed(cptr, LF_RINGING);
}

/**
 * @brief FSM state - 'ringing_1st'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringing_1st(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
                                       int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_offhook] = fxs_state_answer,
        [fxs_event_stop_ringing] = fxs_state_idle,
        [fxs_event_interrupt_ring] = fxs_state_fsk_preamble_caller_id,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        if (state > 0) {
            /* disable ring interrupt */
            netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_IRQEN1, 0, PROSLIC_REG_IRQEN1_RING_TA_IE);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief enable CID
 *
 * @param cptr pointer to Silab voice channel type
 */
static void enable_cid(struct chan_state_t* csptr)
{
    SiVoiceChanType_ptr cptr = csptr->cptr;

    syslog(LOG_DEBUG, "enable_cid() called");

    /* setup FSK */
    ProSLIC_FSKSetup(cptr, 0);

    /* enable cid oscillator */
    netcomm_proslic_enable_cid(cptr, FSK_CALLER_ID_UNDERRUN_THRESHOLD - 1);
    /* enable FSKBUF interrupt */
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_IRQEN1, PROSLIC_REG_IRQEN1_FSKBUF_AVAIL, 0);
    /* enable cid line feed */
    netcomm_proslic_set_linefeed(cptr, LF_FWD_OHT);

    /* FSK fifo is just initiated */
    csptr->fsk_fifo_written = FALSE;
}

/**
 * @brief disable CID
 *
 * @param cptr pointer to Silab voice channel type
 */
static void disable_cid(struct chan_state_t* csptr)
{
    SiVoiceChanType_ptr cptr = csptr->cptr;

    syslog(LOG_DEBUG, "disable_cid() called");

    netcomm_proslic_disable_cid(cptr);
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_IRQEN1, 0, PROSLIC_REG_IRQEN1_FSKBUF_AVAIL);
    netcomm_proslic_set_linefeed(cptr, LF_FWD_ACTIVE);
}

/**
 * @brief FSM state enter action - 'preamble'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_fsk_preamble_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;

    /* initiate preamble state */
    csptr->preamble_idx = 0;

    enable_cid(csptr);

    syslog(LOG_DEBUG, "[FSK data] start preamble (len=%d)", FSK_CALLER_ID_PREAMBLE_COUNT);

    syslog(LOG_DEBUG, "[incoming call] schedule to post fxs_event_fsk_init_delay_expired");

    if (state_idx == fxs_state_fsk_preamble_wcaller_id) {
        fsm_post_event(fsm, fxs_event_fsk_init_delay_expired, NULL, 0);
    } else {
        fxs_schedule_event_post(fsm, fxs_event_fsk_init_delay_expired, FSK_CALLER_ID_INIT_DELAY);
    }
}

/**
 * @brief FSM state - 'preamble'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_fsk_preamble(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
                                        int event_arg_len)
{
    const int states_vmwi[] = {
        [fxs_event_offhook] = fxs_state_dialtone,
        [fxs_event_fsk_caller_id_preamble_delay_expired] = fxs_state_fsk_vmwi,
        [fxs_event_force_connect] = fxs_state_force_connected,
        [fxs_event_ring_with_cid] = fxs_state_ringing_1st,
        [fxs_event_ring] = fxs_state_ringing,
    };

    const int states_caller_id[] = {
        [fxs_event_offhook] = fxs_state_answer,
        [fxs_event_fsk_caller_id_preamble_delay_expired] = fxs_state_fsk_cid,
        [fxs_event_stop_ringing] = fxs_state_idle,
    };

    const int states_wcaller_id[] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_fsk_caller_id_preamble_delay_expired] = fxs_state_fsk_wcid,
        [fxs_event_stop_ringing] = fxs_state_idle,
    };

    struct states_collection_t states_collection[] = {
        [fxs_state_fsk_preamble_vmwi] = {states_vmwi, __countof(states_vmwi)},
        [fxs_state_fsk_preamble_caller_id] = {states_caller_id, __countof(states_caller_id)},
        [fxs_state_fsk_preamble_wcaller_id] = {states_wcaller_id, __countof(states_wcaller_id)}
    };

    const int* states = states_collection[state_idx].states;
    const int states_count = states_collection[state_idx].states_count;

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;
    int rc;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, states_count);
    {
        switch (event) {
            case fxs_event_fsk_init_delay_expired:
            case fxs_event_interrupt_fskbuf: {
                char cid[FSK_CALLER_ID_HW_FIFO_SIZE];
                int len;

                /* get caller id */
                len = caller_id_get_preamble(csptr, cid, sizeof(cid), &csptr->fsk_fifo_timestamp);
                if (len) {
                    syslog(LOG_DEBUG, "[incoming call] send CID preamble (len=%d)", len);
                    rc = netcomm_proslic_sendcid(cptr, (uInt8*) cid, len,
                                                 (len == FSK_CALLER_ID_HW_FIFO_SIZE) ? &csptr->fsk_fifo_timestamp : NULL);
                    if (rc) {
                        syslog(LOG_ERR, "netcomm_proslic_sendcid() failed in sending preamble (rc=%d)", rc);
                    }
                } else {
                    syslog(LOG_DEBUG,
                           "[incoming call] no CID preamble left, schedule to post fxs_event_fsk_caller_id_preamble_delay_expired (%d msec)",
                           FSK_CALLER_ID_PREAMBLE_DELAY);

                    syslog(LOG_DEBUG, "[FSK data] start FSK body");

                    fxs_schedule_event_post(fsm, fxs_event_fsk_caller_id_preamble_delay_expired,
                                            FSK_CALLER_ID_PREAMBLE_DELAY);
                }
                break;
            }
        }

        if (state > 0) {
            switch (event) {
                case fxs_event_fsk_caller_id_preamble_delay_expired:
                    break;

                default: {
                    disable_cid(csptr);
                    break;
                }
            }
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'init_mwi'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_init_mwi_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*)fsm->ref;

    struct event_arg_mwi_t* mwi = (struct event_arg_mwi_t*)event_arg;

    /* set mwi */
    syslog(LOG_DEBUG, "store mwi event (mwi_state=%d)", mwi->state);
    csptr->mwi_state = mwi->state;
    csptr->mwi_state_valid = TRUE;

    /* prepare VMWI FSK message */
    caller_id_set_vmwi(csptr, csptr->mwi_state);

    fsm_post_event(fsm, fxs_event_fsk_init_delay_expired , NULL, 0);
}

/**
 * @brief FSM state - 'init_mwi'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_init_mwi(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_fsk_init_delay_expired] = fxs_state_fsk_preamble_vmwi,
    };

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'fsk_vmwi'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_fsk_vmwi_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    /* replay event */
    syslog(LOG_DEBUG, "[incoming call] replay fxs_event_fsk_caller_id_preamble_delay_expired");
    fsm_post_event(fsm, fxs_event_fsk_caller_id_preamble_delay_expired, NULL, 0);
}

/**
 * @brief FSM state - 'fsk_vmwi'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_fsk_vmwi(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_offhook] = fxs_state_dialtone,
        [fxs_event_ring_with_cid] = fxs_state_ringing_1st,
        [fxs_event_ring] = fxs_state_ringing,
        [fxs_event_fsk_fini_delay_expired] = fxs_state_idle,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;
    int rc;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {

            case fxs_event_fsk_caller_id_preamble_delay_expired:
            case fxs_event_interrupt_fskbuf: {
                char vmwi[FSK_CALLER_ID_HW_FIFO_SIZE];
                int len;

                len = caller_id_get(csptr, vmwi, sizeof(vmwi), &csptr->fsk_fifo_timestamp);
                if (len) {
                    syslog(LOG_DEBUG, "send VMWI (mwi_state=%d,len=%d)", csptr->mwi_state, len);
                    rc = netcomm_proslic_sendcid(cptr, (uInt8*) vmwi, len,
                                                 (len == FSK_CALLER_ID_HW_FIFO_SIZE) ? &csptr->fsk_fifo_timestamp : NULL);
                    if (rc) {
                        syslog(LOG_ERR, "netcomm_proslic_sendcid() failed in sending VMWI (rc=%d)", rc);
                    }
                } else {
                    syslog(LOG_DEBUG, "immediately post fsk_fini_delay_expired");
                    fxs_schedule_event_post(fsm, fxs_event_fsk_fini_delay_expired, FSK_CALLER_ID_FINI_DELAY3);
                }
                break;
            }
        }

        if (state > 0) {
            /* bypass if no state is found */
            disable_cid(csptr);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }

    return;
}

static void softtonegen_play_cb_sas(struct softtonegen_t* stg, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*)ref;

    syslog(LOG_DEBUG, "SAS done");
    fsm_post_event(fsm, fxs_event_wcid_sas_expired, NULL, 0);
}

static void softtonegen_play_cb_cas(struct softtonegen_t* stg, void* ref)
{
    struct fsm_t* fsm = (struct fsm_t*)ref;

    syslog(LOG_DEBUG, "CAS done");
    fsm_post_event(fsm, fxs_event_wcid_cas_expired, NULL, 0);
}

/**
 * @brief FSM state enter action - 'waiting_cid'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_wcid_init_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;
    struct softtonegen_t* stg = &csptr->stg;

    struct event_arg_caller_id_t* event_arg_caller_id = (struct event_arg_caller_id_t*) event_arg;

    slic_stop_pcm(cptr);
    softtonegen_stop(stg);

    /* store caller id */
    syslog(LOG_DEBUG, "store caller ID for CWID");
    caller_id_set(csptr, event_arg_caller_id->month, event_arg_caller_id->day_of_month, event_arg_caller_id->hour,
                  event_arg_caller_id->minute, event_arg_caller_id->cid_num, event_arg_caller_id->cid_num_pi,
                  event_arg_caller_id->cid_name, event_arg_caller_id->cid_name_pi);

    /* play SAS */
    syslog(LOG_DEBUG, "play SAS");
    softtonegen_play_soft_tone(stg, softtonegen_type_sas, FALSE, softtonegen_play_cb_sas, fsm);
}

/**
 * @brief FSM state - 'waiting_cid'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_wcid_init(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
                                     int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_offhook] = fxs_state_disconnect,
        [fxs_event_wcid_cas_expired] = fxs_state_wcid_wait_ack,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    struct softtonegen_t* stg = &csptr->stg;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {
            case fxs_event_wcid_sas_expired: {
                /* play CAS */
                syslog(LOG_DEBUG, "play CAS");
                softtonegen_play_soft_tone(stg, softtonegen_type_cas, FALSE, softtonegen_play_cb_cas, fsm);
                break;
            }
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'waiting_cid'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */

static void fxs_state_func_wcid_wait_ack_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    struct softtonegen_t* stg = &csptr->stg;

    softtonegen_stop(stg);

    fxs_schedule_event_post(fsm, fxs_event_wcid_ack_expired, TIMEOUT_CWID_ACK);
}

/**
 * @brief FSM state - 'waiting_cid'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_wcid_wait_ack(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_wcid_ack_expired] = fxs_state_connected,
        [fxs_event_wcid_ack_received] = fxs_state_fsk_preamble_wcaller_id,
    };

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {
            case fxs_event_digit: {
                struct event_arg_digit_t* event_arg_digit = (struct event_arg_digit_t*) event_arg;
                char ch = event_arg_digit->ch;

                if (ch == 'A' || ch == 'D') {
                    syslog(LOG_DEBUG, "ack detected");
                    fsm_post_event(fsm, fxs_event_wcid_ack_received, NULL, 0);
                }
                break;
            }
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'fsk_cid'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_fsk_cid_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    /* replay event */
    syslog(LOG_DEBUG, "[incoming call] replay fxs_event_fsk_caller_id_preamble_delay_expired #2");
    fsm_post_event(fsm, fxs_event_fsk_caller_id_preamble_delay_expired, NULL, 0);
}

/**
 * @brief FSM state - 'fsk_cid'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_fsk_cid(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const int states_fsk_cid[] = {
        [fxs_event_offhook] = fxs_state_answer,
        [fxs_event_fsk_fini_delay_expired] = fxs_state_ringing_2nd,
        [fxs_event_stop_ringing] = fxs_state_idle,
    };

    const int states_fsk_wcid[] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_fsk_fini_delay_expired] = fxs_state_connected,
        [fxs_event_stop_ringing] = fxs_state_connected,
    };

    struct states_collection_t states_collection[] = {
        [fxs_state_fsk_cid] = {states_fsk_cid, __countof(states_fsk_cid)},
        [fxs_state_fsk_wcid] = {states_fsk_wcid, __countof(states_fsk_wcid)},
    };

    const int* states = states_collection[state_idx].states;
    const int states_count = states_collection[state_idx].states_count;

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;
    int rc;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, states_count);
    {
        switch (event) {
            case fxs_event_fsk_caller_id_preamble_delay_expired:
            case fxs_event_interrupt_fskbuf: {

                char cid[FSK_CALLER_ID_HW_FIFO_SIZE];
                int len;

                /* get caller id */
                len = caller_id_get(csptr, cid, sizeof(cid), &csptr->fsk_fifo_timestamp);
                if (len) {
                    syslog(LOG_DEBUG, "[incoming call] send CID (len=%d)", len);
                    rc = netcomm_proslic_sendcid(cptr, (uInt8*) cid, len,
                                                 (len == FSK_CALLER_ID_HW_FIFO_SIZE) ? &csptr->fsk_fifo_timestamp : NULL);
                    if (rc) {
                        syslog(LOG_ERR, "netcomm_proslic_sendcid() failed in sending CID (rc=%d)", rc);
                    }
                } else {

                    if (state_idx == fxs_state_fsk_wcid) {
                        syslog(LOG_DEBUG, "immediately post fsk_fini_delay_expired");
                        fxs_schedule_event_post(fsm, fxs_event_fsk_fini_delay_expired, FSK_CALLER_ID_FINI_DELAY2);
                    } else {
                        syslog(LOG_DEBUG,
                               "[incoming call] no CID left, schedule to post fxs_event_fsk_caller_id_fini_delay_expired (%d msec)",
                               FSK_CALLER_ID_INIT_DELAY);
                        fxs_schedule_event_post(fsm, fxs_event_fsk_fini_delay_expired, FSK_CALLER_ID_FINI_DELAY);
                    }
                }
                break;
            }

        }

        /* bypass if no state is found */
        if (state > 0) {
            disable_cid(csptr);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'ringing_2nd'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringing_2nd_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    /* setup continuous ring */
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_RINGCON,
                                      PROSLIC_REG_RINGCON_TA_EN | PROSLIC_REG_RINGCON_TI_EN, 0);

    /* start ringing */
    netcomm_proslic_set_linefeed(cptr, LF_RINGING);
}

/**
 * @brief FSM state - 'ringing_2nd'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringing_2nd(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
                                       int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_offhook] = fxs_state_answer,
        [fxs_event_stop_ringing] = fxs_state_idle,
    };

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        if (state > 0) {
            netcomm_proslic_set_linefeed(cptr, LF_FWD_ACTIVE);
        }

        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'ringing'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringing_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    slic_stop_pcm(cptr);
    softtonegen_stop(&csptr->stg);

    /* setup continuous ring */
    netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_RINGCON,
                                      PROSLIC_REG_RINGCON_TA_EN | PROSLIC_REG_RINGCON_TI_EN, 0);

    /* start ringing */
    netcomm_proslic_set_linefeed(cptr, LF_RINGING);

}

/**
 * @brief FSM state - 'ringing'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_ringing(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_offhook] = fxs_state_answer,
        [fxs_event_stop_ringing] = fxs_state_idle,
    };

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

/**
 * @brief FSM state enter action - 'answer'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_answer_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
                                        int event_arg_len)
{
    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    int stat;

    slic_stop_pcm(cptr);
    softtonegen_stop(&csptr->stg);

    netcomm_proslic_set_linefeed(cptr, LF_FWD_ACTIVE);

    stat = cc_make_answer_mt_call(fsm);
    if (stat < 0) {
        syslog(LOG_DEBUG, "no incoming call available to answer, immediately post 'fgcall_disconnected'");
        fsm_post_event(fsm, fxs_event_fgcall_disconnected, NULL, 0);
    }
}

/**
 * @brief FSM state - 'answer'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_answer(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_onhook] = fxs_state_disconnect,
        [fxs_event_connected] = fxs_state_connected,
        [fxs_event_fgcall_disconnected] = fxs_state_reorder,
    };

    int state;

    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

///////////////////////////////////////////////////////////////////////////////
// disconnect states
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief FSM state enter action - 'disconnect'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_disconnect_enter(struct fsm_t* fsm, int state_idx, int event, void* event_arg,
        int event_arg_len)
{
    struct call_track_t* call;
    int hangup = 0;

    struct chan_state_t* csptr = (struct chan_state_t*) fsm->ref;
    SiVoiceChanType_ptr cptr = csptr->cptr;

    /*
     * configure SLIC to play reorder tone.
     *
     * ## note ##
     *
     *  This reorder tone setup is a preparation for the situation when the state machine stays in the 'disconnect' state unusually long
     * due to some unknown reasons such as missing call events or temporary hardware issues. In this circumstance, instead of silence, reorder tone
     * will be heard. In a normal operational condition, this reorder tone is not heard since the 'disconnect' state is very short.
     *
     *
    */

    slic_stop_pcm(cptr);
    softtonegen_play_hard_tone(&csptr->stg, TONEGEN_FCC_REORDER);

    /*
     hang up all calls that are placed or answered. do not hang up incoming calls that are not answered.
     */

    call_track_walk_for_each_begin(call) {

        switch (call->call_status) {
            case call_status_ringing:
            case call_status_waiting:
            case call_status_disconnected:
                break;

            default:
                if (cc_hangup_call(call) < 0) {
                    syslog(LOG_DEBUG, "internal hangup performed");
                } else {
                    syslog(LOG_DEBUG, "RDB hangup performed");
                    hangup = 1;
                }
                break;
        }

        call_track_walk_for_each_end();
    }

    if (!hangup) {
        syslog(LOG_DEBUG, "no call to hangup, immediately post disconnected");
        fsm_post_event(fsm, fxs_event_disconnected, NULL, 0);
    } else {
        syslog(LOG_DEBUG, "[disconnect-timeout] schedule for timeout (timeout=%d msec)", TIMEOUT_MSEC_FOR_DISCONNECT);
        fxs_schedule_event_post(fsm, fxs_event_disconnect_timeout, TIMEOUT_MSEC_FOR_DISCONNECT);
    }
}

/**
 * @brief FSM state - 'disconnect'
 *
 * @param fsm is a FSM object.
 * @param state_idx is state index.
 * @param event is event.
 * @param event_arg is event argument.
 * @param event_arg_len is event argument length.
 */
static void fxs_state_func_disconnect(struct fsm_t* fsm, int state_idx, int event, void* event_arg, int event_arg_len)
{
    const static int states[fxs_event_last] = {
        [fxs_event_disconnected] = fxs_state_idle,
    };

    int state;

    /* if the received event is matching to one of events in the 'states' array, get state machine's corresponding state. Otherwise, state is 0  */
    state = fsm_prep_switch_state_by_event(fsm, event, event_arg, event_arg_len, states, __countof(states));
    {
        switch (event) {
            case fxs_event_disconnect_timeout: {
                struct call_track_t* call;

                /*

                    This block of code generates "VOICE CALL END" events to finish all of existing calls.

                    This is to avoid the situation where the state machine gets stuck in "disconnect" state when Modem (or WMMD) is not responding
                    to HANGUP command. Unfortunately, since we do not have a way to disconnect calls, we finalize POTS bridge internal states only.
                    As a result, we do not know whether the calls remain online or not.

                    This is a safety protection to protect POTS bridge from error conditions that could be caused by Modem (or WMMD).

                    ## note ##
                    fxs_event_disconnect_timeout is not used as a transit condition. The "fxs_event_disconnect_timeout" event generates
                    "fxs_event_disconnected" event and feeds "fxs_event_disconnected" to POTS bridge itself as if POTS bridge received call
                    end notifications from WMMD.
                */

                syslog(LOG_NOTICE, "[disconnect-timeout] disconnect timeout expired, generate own call-end (timeout=%d msec)",
                       TIMEOUT_MSEC_FOR_DISCONNECT);

                /* generate disconnect events for all calls except calls that we answered or we didn't disconnect */
                call_track_walk_for_each_begin(call) {

                    switch (call->call_status) {
                        case call_status_ringing:
                        case call_status_waiting:
                        case call_status_disconnected:
                            break;

                        default: {
                            struct strarg_t* a;
                            char line[STRARG_LINEBUFF_SIZE];

                            /*
                                feed "CALL_NOTIFY 1 MO VOICE END NONE TIMEOUT" to POTS bridge itself.
                            */

                            snprintf(line, sizeof(line), "CALL_NOTIFY %d %s VOICE END NONE TIMEOUT", call->call_track_id,
                                     (call->call_dir == call_dir_incoming) ? "MT" : "MO");

                            a = strarg_create(line);
                            if (!a || !a->argv[0]) {
                                syslog(LOG_ERR, "[disconnect-timeout] strarg_create() failed in fxs_event_disconnect_timeout");
                            } else {

                                syslog(LOG_NOTICE, "[disconnect-timeout] push CALL END event to call #%d", call->call_track_id);
                                on_event(a);

                                strarg_destroy(a);
                            }
                        }
                    }

                    call_track_walk_for_each_end();
                }

                break;
            }
        }

        /* cancel the scheduled "fxs_event_disconnect_timeout" message when we exit 'disconnect' state. */
        if (state > 0) {
            fxs_schedule_event_cancel(fsm);
        }

        /* if the received event is matching to one of events in the 'states' array, transit to the new state  */
        fsm_perform_switch_state_by_event(fsm, event, event_arg, event_arg_len, state);
    }
}

///////////////////////////////////////////////////////////////////////////////
// ProSLIC SDK - IRQ
///////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_FUTURE_FEATURE
static void pbx_save_slic_irqens(struct chan_state_t* csptr)
{
    SiVoiceChanType_ptr cptr = csptr->cptr;
    struct irq_storage_t* irq_storage = &csptr->irq_storage;

    int i;

    for (i = PROSLIC_REG_IRQEN1; i < PROSLIC_REG_IRQEN4; i++) {
        irq_storage->irq_save[i - PROSLIC_REG_IRQEN1] = SiVoice_ReadReg(cptr, i);
    }
}

static void pbx_restore_slic_irqens(struct chan_state_t* csptr)
{
    SiVoiceChanType_ptr cptr = csptr->cptr;
    struct irq_storage_t* irq_storage = &csptr->irq_storage;

    int i;

    for (i = PROSLIC_REG_IRQEN1; i < PROSLIC_REG_IRQEN4; i++) {
        SiVoice_WriteReg(cptr, i, irq_storage->irq_save[i - PROSLIC_REG_IRQEN1]);
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////
// ProSLIC SDK - initiate
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief sets up all channels.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int pbx_channels_setup(void)
{
    int i;
    SiVoiceChanType_ptr cptr;
    uInt8 reg_value;

    struct chan_state_t* csptr;
    struct fxs_port_t* port;

    /* Now setup CID specific info */
    _chan_state = SIVOICE_CALLOC(_fxs_info->num_of_channels, sizeof(struct chan_state_t));

    if (_chan_state == NULL) {
        syslog(LOG_ERR, "failed to allocate for cid_channel");
        goto err;
    }

    for (i = 0; i < _fxs_info->num_of_channels; i++) {
        /* get channel state ptr */
        csptr = &_chan_state[i];
        /* get channel ptr */
        cptr = pbx_get_cptr(i);
        port = pbx_get_port(i);

        syslog(LOG_DEBUG, "initiate channel %d", i);

        /* initiate channel state members */
        csptr->chan_idx = i;
        csptr->digitCount = 0;
        csptr->hook_change = 0;
        csptr->ringCount = 0;
        csptr->cptr = cptr;
        csptr->port = port;

        csptr->hook_state = PROSLIC_ONHOOK;

        /* enable IRQs */

        reg_value = SiVoice_ReadReg(cptr, PROSLIC_REG_IRQEN1);
        reg_value |= 0x50; /* FSK | Ring timer */
        SiVoice_WriteReg(cptr, PROSLIC_REG_IRQEN1, reg_value);

        reg_value = SiVoice_ReadReg(cptr, PROSLIC_REG_IRQEN1 + 1);
        reg_value |= 0x3; /* LCR | RTP */
        SiVoice_WriteReg(cptr, PROSLIC_REG_IRQEN1 + 1, reg_value);

        /* disable CID */
        netcomm_proslic_disable_cid(cptr);
        netcomm_proslic_reg_set_and_reset(cptr, PROSLIC_REG_IRQEN1, 0, PROSLIC_REG_IRQEN1_FSKBUF_AVAIL);

        /* initiate soft tone generator */
        softtonegen_init(&csptr->stg, cptr);

        #ifdef PBX_DEMO_ENABLE_MWI
        ProSLIC_MWIEnable(cptr);
        ProSLIC_SetMWIState(cptr, SIL_MWI_FLASH_ON);
        #endif

        #ifdef DEBUG_DUMP_REGS
        ProSLIC_PrintDebugReg(cptr);
        ProSLIC_PrintDebugRAM(cptr);
        #endif

    }

    return 0;

err:
    return -1;
}

/**
 * @brief tears down all channels.
 */
static void pbx_channels_teardown(void)
{
    struct chan_state_t* csptr;
    SiVoiceChanType_ptr cptr;
    int i;

    if (!_chan_state) {
        goto fini;
    }

    for (i = 0; i < _fxs_info->num_of_channels; i++) {
        csptr = &_chan_state[i];
        cptr = csptr->cptr;

        /* finalise soft tone generator */
        softtonegen_fini(&csptr->stg);

        SiVoice_Reset(cptr);
        SIVOICE_FREE(csptr);
    }

fini:
    return;
}

/**
 * @brief dump SLIC channel registers into syslog when any register is changed
 *
 * @param csptr is a channel state object.
 * @param force_to_dump force to dump.
 */
static void dump_channel_register(int loglevel, struct chan_state_t* csptr, int force_to_dump)
{
    SiVoiceChanType_ptr cptr = csptr->cptr;

    const struct dump_register_collection_t* dump_register_ptr;
    uInt32 val;

    proslicMonitorType monitor;
    int i;
    const char* fmt;

    int reg_chg = FALSE;

    for (i = 0; i < DUMP_REGISTER_COUNT; i++) {
        dump_register_ptr = &dump_register_collection[i];

        if (dump_register_ptr->ram) {
            val = netcomm_proslic_ram_get(cptr, dump_register_ptr->addr);
        } else {
            val = SiVoice_ReadReg(cptr, dump_register_ptr->addr);
        }

        if (csptr->dumpRegs[i] != val) {

            if (dump_register_ptr->log_chg) {
                if (dump_register_ptr->ram) {
                    fmt = "[regdump] %s #%d changed [0x%08x] ==> [0x%08x]";
                } else {
                    fmt = "[regdump] %s #%d changed [0x%02x] ==> [0x%02x]";
                }
                syslog(loglevel, fmt, dump_register_ptr->name, dump_register_ptr->addr, csptr->dumpRegs[i], val);
            }
            csptr->dumpRegs[i] = val;
            reg_chg = reg_chg || dump_register_ptr->log_chg;
        }
    }

    if (force_to_dump || reg_chg) {

        syslog(loglevel, "[regdump] * dump registers");
        for (i = 0; i < DUMP_REGISTER_COUNT; i++) {
            dump_register_ptr = &dump_register_collection[i];
            if (dump_register_ptr->ram) {
                fmt = "[regdump] dump(%s) %s #%d = 0x%08x";
            } else {
                fmt = "[regdump] dump(%s) %s #%d = 0x%02x";
            }
            syslog(loglevel, fmt, dump_register_ptr->log_chg ? "e" : "s", dump_register_ptr->name,
                   dump_register_ptr->addr,
                   csptr->dumpRegs[i]);
        }

        ProSLIC_LineMonitor(cptr, &monitor);

        syslog(loglevel,
               "[regdump] TR=%.3fv/%.3fmA TIP=%.3fv/%.3fmA RING=%.3fv/%.3fmA LONG=%.3fv/%.3fmA VBAT=%.3fv VDC=%.3fv P_HVIC=%dmW",
               (double)monitor.vtr / 1000,
               (double)monitor.itr / 1000,
               (double)monitor.vtip / 1000,
               (double)monitor.itip / 1000,
               (double)monitor.vring / 1000,
               (double)monitor.iring / 1000,
               (double)monitor.vlong / 1000,
               (double)monitor.ilong / 1000,
               (double)monitor.vbat / 1000,
               (double)monitor.vdc / 1000,
               (int)(monitor.p_hvic)
              );
    }

}

/**
 * @brief processes SLIC channel interrupt.
 *
 * @param csptr is a channel state object.
 * @param chan is channel number.
 */
static void poll_channel_interrupts(struct chan_state_t* csptr, unsigned int chan)
{
    SiVoiceChanType_ptr cptr = csptr->cptr;
    struct fsm_t* fsm = &csptr->fsm;

    ProslicInt arrayIrqs[MAX_PROSLIC_IRQS];
    proslicIntType irqs = { arrayIrqs, 0 };
    int i;
    int irq;

    int debounce = 0;

    system_getTime_fptr getTime_fptr = cptr->deviceId->ctrlInterface->getTime_fptr;
    system_timeElapsed_fptr timeElapsed_fptr = cptr->deviceId->ctrlInterface->timeElapsed_fptr;
    void* hTimer = cptr->deviceId->ctrlInterface->hTimer;

    uInt8 hook_state;

    int pressed;

    int slic_hw_num_of_interrupts;
    int log_dump_register = TRUE;

    ///////////////////////////////////////////////////////////////////////////////
    // DTMF hold feature
    ///////////////////////////////////////////////////////////////////////////////

    /* read dtmf */
    #ifdef CONFIG_ENABLE_DTMF_LONG_PRESS_VOICEMAIL
    {
        static uint64_t timestamp_pressed;
        static int timestamp_pressed_valid;
        static uint64_t timestamp_now;

        int mask = PROSLIC_REG_TONDTMF_VALID_TONE | PROSLIC_REG_TONDTMF_VALID;

        int digit_chg;
        static int digit_prev = -1;

        int pressed_raising;
        int pressed_cont;
        int pressed_new;
        int pressed_cur;
        static int pressed_old = 0;
        int presssed_falling;
        int digit_new;

        static int pressed_prev = FALSE;
        static int digit_sent = FALSE;

        int send_digit;
        int long_press = FALSE;

        uInt8 digit_raw;

        /* get digit */
        netcomm_proslic_dtmf_read_digit(cptr, &digit_raw);
        digit_new = digit_raw & ~mask;

        /* get change flag */
        digit_chg = (digit_prev != digit_new);

        /* get pressed flag */
        pressed_cur = (digit_raw & mask) == mask;

        /* remove debounce */
        pressed_new = (!pressed_old && pressed_cur) || pressed_old;
        pressed_old = pressed_cur;

        pressed_raising =  !pressed_prev && pressed_new;
        pressed_cont = pressed_prev && pressed_new;
        presssed_falling = pressed_prev && !pressed_new;

        timestamp_now = get_monotonic_msec();

        pressed = -1;

        /* get long press flag */
        long_press = timestamp_pressed_valid && ((timestamp_now - timestamp_pressed) >= DURATION_DTMF_LONG_PRESS);

        /* if raising edge or if digit changed */
        if (pressed_raising || (pressed_cont && digit_chg)) {
            syslog(LOG_DEBUG, "digit: pressed (digit=0x%02x)", digit_new);

            timestamp_pressed_valid = TRUE;
            timestamp_pressed = timestamp_now;

            digit_sent = FALSE;

            pressed = 1;
        }
        /* if falling edge */
        else if (presssed_falling) {
            syslog(LOG_DEBUG, "digit: released (digit=0x%02x)", digit_new);

            timestamp_pressed_valid = FALSE;

            pressed = 0;
        }

        /* send pressed or released */
        if (pressed >= 0) {

            struct event_arg_digit_pressed_t event_arg_digit_pressed;
            uInt8 digit = pressed ? digit_new : digit_prev;

            /* build event info */
            event_arg_digit_pressed.digit = digit;
            event_arg_digit_pressed.ch = convert_digit_to_ascii(digit);
            event_arg_digit_pressed.pressed = pressed;

            /* post dtmf event */
            syslog(LOG_DEBUG, "[DTMF] post fxs_event_digit_pressed '%s' (digit=0x%02x)",
                   pressed ? "pressed" : "released",
                   digit);
            fsm_post_event(fsm, fxs_event_digit_pressed, &event_arg_digit_pressed, sizeof(event_arg_digit_pressed));
        }

        send_digit = -1;

        /* if new digit is pressed_or_released */
        if (pressed_cont && digit_chg) {
            syslog(LOG_DEBUG, "digit: digit changed (digit=0x%02x)", digit_new);
            send_digit = digit_prev;
        }
        /* if digit is released without sending */
        else if (presssed_falling && !digit_sent) {
            syslog(LOG_DEBUG, "digit: digit released (digit=0x%02x)", digit_new);
            send_digit = digit_new;
        }

        /* send event */
        if (send_digit >= 0) {
            struct event_arg_digit_t event_arg_digit;

            /* build event info */
            event_arg_digit.digit = send_digit;
            event_arg_digit.ch = convert_digit_to_ascii(send_digit);
            event_arg_digit.long_press = long_press != 0;

            syslog(LOG_DEBUG, "digit: send digit (digit=0x%02x,ch='%c',long_press=%d)", send_digit, event_arg_digit.ch,
                   long_press != 0);

            /* post dtmf event */
            fsm_post_event(fsm, fxs_event_digit, &event_arg_digit, sizeof(event_arg_digit));

            digit_sent = TRUE;
        }

        digit_prev = digit_new;
        pressed_prev = pressed_new;
    }
    #endif

    ///////////////////////////////////////////////////////////////////////////////
    // process interrupts
    ///////////////////////////////////////////////////////////////////////////////

    /* get interrupts */
    slic_hw_num_of_interrupts = ProSLIC_GetInterrupts(cptr, &irqs);
    if (slic_hw_num_of_interrupts) {

        /* iterate through the interrupts */
        for (i = 0; i < irqs.number; i++) {

            /* get triggered IRQ */
            irq = irqs.irqs[i];

            switch (irq) {
                case IRQ_DTMF: { /* Detected a DTMF */
                    #ifndef CONFIG_ENABLE_DTMF_LONG_PRESS_VOICEMAIL
                    uInt8 digit;

                    struct event_arg_digit_t event_arg_digit;

                    /* read dtmf */
                    netcomm_proslic_dtmf_read_digit(cptr, &digit_raw);
                    digit = digit_raw & 0x0f;

                    if (!(digit_raw & PROSLIC_REG_TONDTMF_VALID_TONE)) {
                        syslog(LOG_DEBUG, "interrupt: valid tone flag not set (ch %d digit %d)", chan, digit);
                    } else if (!(digit_raw & PROSLIC_REG_TONDTMF_VALID)) {
                        syslog(LOG_DEBUG, "interrupt: valid flag not set (ch %d digit %d)", chan, digit);
                    } else {
                        syslog(LOG_DEBUG, "interrupt: DTMF ch %d digit %d", chan, digit);

                        /* build event info */
                        event_arg_digit.digit = digit;
                        event_arg_digit.ch = convert_digit_to_ascii(digit);
                        event_arg_digit.long_press = 0;

                        /* post dtmf event */
                        fsm_post_event(fsm, fxs_event_digit, &event_arg_digit, sizeof(event_arg_digit));
                    }
                    #endif

                    break;
                }
                case IRQ_LOOP_STATUS: /* Hook change during caller ID, abort and return back to normal state */
                case IRQ_RING_TRIP:
                    syslog(LOG_DEBUG, "interrupt: HOOK changed ch %d", chan);
                    debounce = 1;
                    break;

                case IRQ_RING_T1: /* We completed a ring cycle */
                    syslog(LOG_DEBUG, "interrupt: RING T1");
                    fsm_post_event(fsm, fxs_event_interrupt_ring, NULL, 0);

                    break;

                case IRQ_RING_T2:
                    syslog(LOG_DEBUG, "interrupt: RING T2");
                    break;

                case IRQ_FSKBUF_AVAIL:
                    syslog(LOG_DEBUG, "interrupt: FSKBUF");

                    /* FSK */
                    log_dump_register = FALSE;

                    fsm_post_event(fsm, fxs_event_interrupt_fskbuf, NULL, 0);
                    break;

                default:
                    break;
            }

            /* feed irq to soft tone generator */
            softtonegen_process_interrupt(&csptr->stg, irq);

        }
    }

    /* feed execution to softtone generator */
    softtonegen_feed_execution(&csptr->stg);

    ///////////////////////////////////////////////////////////////////////////////
    // process hookflash debounce
    ///////////////////////////////////////////////////////////////////////////////

    //syslog(LOG_DEBUG,"state=%d,debounce=%d,timeout=%d",hook_state,debounce,hook_detection->lookingForTimeout);

    /* check to see if hookflash timer gets expired */
    if (csptr->hookTimerValid) {

        int delta_time;

        timeElapsed_fptr(hTimer, &csptr->hookTimer, &delta_time);

        if (delta_time > DURATION_MINMUM_HOOKFLASH) {
            csptr->hookTimerValid = 0;

            csptr->hook_state = PROSLIC_ONHOOK;

            /* do delayed onhook */
            syslog(LOG_DEBUG, "debounce: ch %d onhook (delayed)", chan);
            fsm_post_event(fsm, fxs_event_onhook, NULL, 0);
        }
    }

    if (debounce) {
        ProSLIC_ReadHookStatus(cptr, &hook_state);

        if (csptr->hook_state == PROSLIC_ONHOOK && hook_state == PROSLIC_OFFHOOK) {

            csptr->hook_state = hook_state;

            syslog(LOG_DEBUG, "debounce: ch %d offhook", chan);
            fsm_post_event(fsm, fxs_event_offhook, NULL, 0);

        } else if (csptr->hook_state == PROSLIC_OFFHOOK && hook_state == PROSLIC_ONHOOK) {

            /* set hook timer */
            csptr->hookTimerValid = 1;
            getTime_fptr(hTimer, &csptr->hookTimer);

        } else if (csptr->hook_state == PROSLIC_OFFHOOK && hook_state == PROSLIC_OFFHOOK) {

            csptr->hook_state = hook_state;

            /* do hookflash if the time is not expired */
            if (csptr->hookTimerValid) {
                syslog(LOG_DEBUG, "debounce: ch %d hookflash", chan);
                fsm_post_event(fsm, fxs_event_hookflash, NULL, 0);
            }

            csptr->hookTimerValid = 0;
        }

    }

    ///////////////////////////////////////////////////////////////////////////////
    // check hardware registers
    ///////////////////////////////////////////////////////////////////////////////

    dump_channel_register(LOG_DEBUG, csptr, slic_hw_num_of_interrupts && log_dump_register);
}

///////////////////////////////////////////////////////////////////////////////
// initiate and finish RDB
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief initiates RDB.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int init_rdb(void)
{
    static struct rdb_session* rdb_session;

    /* open rdb */
    if (ezrdb_open() < 0) {
        goto err;
    }

    rdb_session = ezrdb_get_session();

    /* delete call status rdb */
    rdb_delete_voice_call_status(rdb_session);

    /* reset call stat */
    memset(_curr_call_stat, 0, sizeof(*_curr_call_stat));
    cc_update_call_counts();

    /* init. call history */
    call_history_init();

    block_calls_init();

    /* create index rdb */
    _ir = indexed_rdb_create(rcchp_cmds, rcchp_cmds_count);
    _ir_subscribed = indexed_rdb_create(_subscribed_rdb_elements, __countof(_subscribed_rdb_elements));

    /* create rdb rwpipe */
    _pp = rwpipe_create(RWPIPE_DEFAULT_TIMEOUT, NULL);
    if (!_pp) {
        syslog(LOG_ERR, "cannot create rwpipe");
        goto err;
    }

    /* create dbenum */
    syslog(LOG_DEBUG, "[thread] create dbenum");
    _dbenum = dbenum_create(rdb_session, TRIGGERED);

    /* reset and subscribe */
    ezrdb_set_array(_rdb_var_to_reset, __countof(_subscribed_rdb_var), "");
    ezrdb_subscribe_array(_subscribed_rdb_var, __countof(_subscribed_rdb_var));

    return 0;

err:
    return -1;
}

/**
 * @brief finalizes RDB.
 */
static void fini_rdb(void)
{
    dbenum_destroy(_dbenum);

    rwpipe_destroy(_pp);

    call_history_fini();

    block_calls_fini();

    ezrdb_close();

    indexed_rdb_destroy(_ir_subscribed);
    indexed_rdb_destroy(_ir);
}

/**
 * @brief signal handlerj
 *
 * @param sig is signal number.
 */
static void sighandler(int sig)
{
    syslog(LOG_DEBUG, "caught signal %d", sig);
    _bridge_running = 0;
}

#define fxs_for_each_foreground_call_begin(arg_csptr,arg_fsm,arg_foreground_call) \
    for(i=0;i<_fxs_info->num_of_channels;i++) { \
        arg_csptr = &_chan_state[i]; \
        arg_fsm = &(arg_csptr->fsm); \
        \
        (arg_foreground_call) = (arg_csptr)->foreground_call; \
        if(!(arg_foreground_call)) { \
            continue; \
        }

#define fxs_for_each_foreground_call_end() \
    }

/**
 * @brief RDB voice call handler.
 *
 * @param a is RDB argument.
 */
static void on_event(struct strarg_t* a)
{
    int cmd_idx;
    char* cmd_str;

    /* get command index and string */
    cmd_str = a->argv[0];
    cmd_idx = indexed_rdb_get_cmd_idx(_ir, cmd_str);

    switch (cmd_idx) {
        case vc_dtmf_notify: {
            int cid;
            const char* action;
            const char* digit;

            int dtmf_end;

            static struct call_track_t* call;
            struct fsm_t* fsm;

            struct event_arg_rx_dtmf_t event_arg_rx_dtmf;

            cid = atoi(a->argv[1]);
            action = a->argv[2];
            digit = a->argv[3];

            /* check action */
            if (!action || !*action) {
                syslog(LOG_ERR, "[NOTI] action field missing in DTMF notification)");
                goto dtmf_fini;
            }

            /* check digit */
            if (!digit || !*digit) {
                syslog(LOG_ERR, "[NOTI] digit field missing in DTMF notification)");
                goto dtmf_fini;
            }

            syslog(LOG_DEBUG, "[NOTI] dtmf notification recv (call_id='%d',action='%s',digits='%s'", cid, action,
                   digit);

            /*

              max power : 3 dBm
              default power : -18 dBm

            */

            /* get call */
            call = cc_get_call_by_cid(cid);
            if (!call) {
                syslog(LOG_INFO, "no CID found for DTMF (cid=%d)", cid);
                goto dtmf_fini;
            }

            /* get state machine */
            fsm = (struct fsm_t*) call->ref;
            if (!fsm) {
                syslog(LOG_ERR, "DTMF received with no FSM associated");
                goto dtmf_fini;
            }

            /* get frame type */
            dtmf_end = !!strcmp(action, "IP_INCOMING_DTMF_START");

            /* build event argument */
            event_arg_rx_dtmf.ch = *digit;
            event_arg_rx_dtmf.start_or_end = dtmf_end;

            /* send 'connected' to foreground call */
            syslog(LOG_DEBUG, "send 'rx dtmf' to foreground call (ch='%c',start_or_end=%d)", *digit, dtmf_end);
            fsm_post_event(fsm, fxs_event_rx_dtmf, &event_arg_rx_dtmf, sizeof(event_arg_rx_dtmf));

dtmf_fini:
            break;
        }

        case vc_call_notify: {
            const char* call_dir;
            int cid;
            const char* call_type;
            const char* call_desc;
            const char* alert_type;
            const char* call_end_reason;

            const char* cid_pi;
            const char* cid_num;
            const char* cid_name_pi;
            const char* cid_name;

            int emergency;

            static struct call_track_t* call;

            struct chan_state_t* csptr;
            struct fsm_t* fsm;
            struct call_track_t* foreground_call;
            const char* state_name;

            int i;

            if (a->argc < 5) {
                syslog(LOG_ERR, "[NOTI] incorrect argc (argc=%d)", a->argc);
                break;
            }

            /* get arguments */
            cid = atoi(a->argv[1]);
            call_dir = a->argv[2];
            call_type = a->argv[3];
            call_desc = a->argv[4];
            alert_type = a->argv[5];
            call_end_reason = a->argv[6];

            /* check call type */
            emergency = !strcmp(call_type, "EMERGENCY") || !strcmp(call_type, "EMERGENCY_IP");
            if (strcmp(call_type, "VOICE") && strcmp(call_type, "VOICE_IP") && !emergency) {
                syslog(LOG_ERR, "[NOTI] ignore - not voice call (call_type=%s)", call_type);
                break;
            }

            cid_pi = a->argv[7] ? a->argv[7] : "";
            cid_num = a->argv[8] ? a->argv[8] : "";
            cid_name_pi = a->argv[9] ? a->argv[9] : NULL;
            cid_name = a->argv[10] ? a->argv[10] : NULL;

            syslog(LOG_DEBUG, "[NOTI] call_dir='%s',cid=%d,call_type='%s',call_desc='%s', call_end_reason='%s'", call_dir, cid,
                   call_type,
                   call_desc, call_end_reason);

            if (!strcmp(call_dir, "MT")) {
                ///////////////////////////////////////////////////////////////////////////////
                // process notification - incoming call notification
                ///////////////////////////////////////////////////////////////////////////////
                int callwaiting;

                ///////////////////////////////////////////////////////////////////////////////
                // process notification - SETUP
                ///////////////////////////////////////////////////////////////////////////////

                if (!strcmp(call_desc, "SETUP")) {
                    syslog(LOG_INFO, "[NOTI] got %d %s '%s' '%s' '%s'", cid, call_desc, cid_pi, cid_num, cid_name);

                    /* get call by cid */
                    call = cc_get_call_by_cid(cid);

                    /* create call in setup */
                    if (!call) {
                        syslog(LOG_INFO, "new setup call detected in INCOMING/WAITING (cid=%d)", cid);

                        /* create a new call */
                        call = cc_make_mt_call(cid, cid_num, cid_pi, cid_name, cid_name_pi);
                        if (!call) {
                            syslog(LOG_ERR, "failed to allocate MT call");
                            goto fini_mt;
                        }

                    } else {
                        syslog(LOG_INFO, "setup call found in INCOMING/WAITING (cid=%d)", cid);
                    }

                    /* add clip or clip name to call */
                    cc_add_info_to_call(call, cid_num, cid_pi, cid_name, cid_name_pi);

                    /* set collating flag */
                    cc_make_mt_setup(call);

                    if (call->being_disconnected) {
                        /* nothing to do */
                    } else if (call->cid_num && block_calls_is_blocked(call->cid_num)) {

                        syslog(LOG_INFO, "blocked call in setup (cid=%d,num=%s)", call->call_track_id, call->cid_num);
                        call->call_blocked = 1;
                        rdb_setup_answer(call->call_track_id, "BLACKLISTED_CALL_ID");

                        call->being_disconnected = TRUE;

                    } else if (!cc_is_any_sm_available_to_answer()) {

                        syslog(LOG_INFO, "no state machine available to answer a setup call, immediately hang up (cid=%d)",
                               call->call_track_id);
                        rdb_setup_answer(call->call_track_id, "USER_BUSY");

                        call->being_disconnected = TRUE;

                    } else {
                        syslog(LOG_INFO, "answer call in setup (cid=%d)", call->call_track_id);
                        rdb_setup_answer(call->call_track_id, NULL);
                    }

                } else if ((callwaiting = !strcmp(call_desc, "WAITING")) || !strcmp(call_desc, "INCOMING")) {
                    ///////////////////////////////////////////////////////////////////////////////
                    // process notification - INCOMING or WAITING
                    ///////////////////////////////////////////////////////////////////////////////
                    syslog(LOG_INFO, "[NOTI] got %d %s '%s' '%s' '%s'", cid, call_desc, cid_pi, cid_num, cid_name);

                    /* get call by cid */
                    call = cc_get_call_by_cid(cid);

                    /* create incoming call */
                    if (!call) {
                        syslog(LOG_INFO, "new incoming call detected in INCOMING/WAITING (cid=%d)", cid);

                        /* create a new call */
                        call = cc_make_mt_call(cid, cid_num, cid_pi, cid_name, cid_name_pi);
                        if (!call) {
                            syslog(LOG_ERR, "failed to allocate MT call");
                            goto fini_mt;

                        }

                    } else {
                        syslog(LOG_INFO, "incoming call found in INCOMING/WAITING (cid=%d)", cid);
                    }

                    /* add clip or clip name to call */
                    cc_add_info_to_call(call, cid_num, cid_pi, cid_name, cid_name_pi);

                    /* clear disconnected flag */
                    if (call->call_status != call_status_waiting && call->call_status != call_status_ringing) {
                        syslog(LOG_INFO, "previous disconnection attempt failed, clear disconnecting flag (cid=%d,num=%s)", call->call_track_id,
                               call->cid_num);
                        call->being_disconnected = FALSE;
                    }

                    if (callwaiting) {
                        cc_make_mt_waiting(call);

                        /* pend incoming call*/
                        qc_fixup_update_pending_mt_call(call);

                    } else {
                        cc_make_mt_ringing(call);

                        /* pend incoming call*/
                        qc_fixup_update_pending_mt_call(call);
                    }

                    qc_fixup_perform_pending_mt_call(0);

                } else {
                    /* get call by cid */
                    call = cc_get_call_by_cid(cid);
                    if (!call) {
                        syslog(LOG_ERR, "[INCALL] cid not found in MT (cid=%d)", cid);
                        goto fini_mt;
                    }

                    ///////////////////////////////////////////////////////////////////////////////
                    // process notification - CONVERSATION
                    ///////////////////////////////////////////////////////////////////////////////
                    if (!strcmp(call_desc, "CONVERSATION")) {
                        int ringing_others;
                        int ringing_cur;

                        syslog(LOG_INFO, "[NOTI] got %d %s", cid, call_desc);

                        syslog(LOG_INFO, "perform pending MT call ring by CONVERSATION (cid=%d)", cid);
                        qc_fixup_perform_pending_mt_call(cid);

                        if (call->call_dir != call_dir_incoming) {
                            syslog(LOG_ERR, "no incoming call existing (cid=%d), ignore", cid);
                            goto fini_mt;
                        }

                        if (call->call_status < call_status_connected) {
                            /* do not perform conference when hookflash due to outgoing call */
                            fsm = (struct fsm_t*) call->ref;

                            if (!fsm) {
                                /* get POTS bridge test mode flag */
                                int pots_bridge_test_mode;
                                pots_bridge_test_mode = ezrdb_get_int(RDB_TEST_MODE);

                                if (pots_bridge_test_mode) {
                                    syslog(LOG_ERR, "!!!! test mode !!!! : process an external MT call (cid=%d)", cid);

                                    /* set flag */
                                    call->orphan_call = TRUE;

                                    /* get the first SLIC channel */
                                    csptr = &_chan_state[0];
                                    if (!csptr) {
                                        syslog(LOG_ERR, "no voice channel exists");
                                        goto fini_mo;
                                    }

                                    /* force to associate the first SLIC channel */
                                    fsm = &csptr->fsm;
                                    cc_associate_call_with_fsm(fsm, call);
                                }
                            }

                            if (!fsm) {
                                syslog(LOG_ERR, "incoming CONVERSATION event received with no FSM associated #2");
                            } else {
                                syslog(LOG_ERR, "[hookflash] set perform_conference_when_hookflash to FALSE");
                                csptr = (struct chan_state_t*) fsm->ref;
                                csptr->perform_conference_when_hookflash = FALSE;
                            }
                        }

                        /* see if it is the last ringing call */
                        ringing_others = cc_is_any_call_ringing_or_waiting(call);
                        ringing_cur = (call->call_status == call_status_ringing) || (call->call_status == call_status_waiting);

                        /* broadcast stop ringing */
                        if (!ringing_others && ringing_cur) {
                            syslog(LOG_DEBUG, "call in conversation, broadcast 'stop_ringing' to fsm");
                            fxs_broadcast_event(fxs_event_stop_ringing, NULL, 0);
                        }

                        /* update answered flag */
                        cc_connect_mt_call(call);

                        /* send 'connected' to foreground call */
                        fsm = (struct fsm_t*) call->ref;
                        if (fsm) {
                            if (call->orphan_call) {
                                syslog(LOG_ERR, "!!!! test mode !!!! : send 'fxs_event_force_connect' (cid=%d)", cid);
                                /* force to connect */
                                fsm_post_event(fsm, fxs_event_force_connect, NULL, 0);
                            } else {
                                syslog(LOG_DEBUG, "send 'connected' to foreground call");
                                fsm_post_event(fsm, fxs_event_connected, NULL, 0);
                            }
                        } else {
                            syslog(LOG_ERR, "incoming CONVERSATION event received with no FSM associated");
                        }

                        /* update call status */
                        rdb_set_voice_call_status(call->call_track_id, "conversation", call->cid_num);
                    }
                }

fini_mt:
                __noop();

            } else {
                ///////////////////////////////////////////////////////////////////////////////
                // process notification - outgoing call notification
                ///////////////////////////////////////////////////////////////////////////////

                int origination;
                int ringback_local;
                int ringback_remote;
                int ringback_none;

                /* get origination */
                origination = !strcmp(call_desc, "ORIGINATION");

                ///////////////////////////////////////////////////////////////////////////////
                // process notification - CC_IN_PROGRESS
                ///////////////////////////////////////////////////////////////////////////////

                if (!strcmp(call_desc, "CC_IN_PROGRESS") || origination) {
                    syslog(LOG_INFO, "[NOTI] got %d %s", cid, call_desc);

                    /* n-way_voice does not have CC_IN_PROGRESS */
                    syslog(LOG_DEBUG, "process PROGRESS/ORIGINATION (cid=%d)", cid);

                    /* get call */
                    call = cc_get_call_by_cid(cid);
                    if (!call) {
                        /* get the newly placed outgoing call */
                        call = cc_get_call_by_cid(0);
                        if (!call) {
                            /* test mode to allow dialing by AT command */
                            int pots_bridge_test_mode;

                            syslog(LOG_ERR, "newly placed outgoing call not found (cid=%d)", cid);

                            pots_bridge_test_mode = ezrdb_get_int(RDB_TEST_MODE);

                            if (pots_bridge_test_mode) {
                                static struct chan_state_t* csptr;
                                struct fsm_t* fsm;

                                /*

                                 create a new call from an MO orphan call.

                                */

                                syslog(LOG_ERR, "!!!! test mode !!!! : process an external MO call (cid=%d)", cid);

                                call = cc_add_new_call(cid, call_dir_outgoing, call_type_normal, cid_num, cid_pi, cid_name, cid_name_pi);
                                if (!call) {
                                    syslog(LOG_ERR, "failed to add mo call");
                                    goto fini_mo;
                                }

                                /* set flag */
                                call->orphan_call = TRUE;

                                /* get the first SLIC channel */
                                csptr = &_chan_state[0];
                                if (!csptr) {
                                    syslog(LOG_ERR, "no voice channel exists");
                                    goto fini_mo;
                                }

                                /* force to associate the first SLIC channel */
                                fsm = &csptr->fsm;
                                cc_associate_call_with_fsm(fsm, call);

                                /* force to connect */
                                fsm_post_event(fsm, fxs_event_force_connect, NULL, 0);
                            } else {
                                /*

                                 The call must have been removed before connecting. we need to hang up
                                 this orphan outgoing call. Otherwise, the call will remain online.
                                 When this orphan call gets disconnected and "END" event will send
                                 "DISCONNECTED" to FSM.

                                */

                                syslog(LOG_ERR, "orphan call detected, immediately hang up (cid=%d)", cid);
                                rdb_hangup(cid, NULL);
                            }

                            goto fini_mo;
                        }

                        /* update call */
                        call->call_track_id = cid;
                        call_track_update(call);

                        syslog(LOG_DEBUG, "newly placed outgoing call found (cid=%d)", cid);
                    } else {
                        syslog(LOG_DEBUG, "outgoing call found in PROGRESS/ORIGINATION (cid=%d)", cid);
                    }

                    ///////////////////////////////////////////////////////////////////////////////
                    // process notification - ORIGINATION
                    ///////////////////////////////////////////////////////////////////////////////
                    if (origination) {
                        syslog(LOG_DEBUG, "process ORIGINATION (cid=%d)", cid);

                        /* bypass if already progress */
                        if (call->call_status >= call_status_progressed) {
                            syslog(LOG_ERR, "call already progressed (cid=%d)", cid);
                            goto fini_mo;
                        }

                        cc_make_mo_progressed(call);
                    }

                } else {
                    /* get call */
                    call = cc_get_call_by_cid(cid);

                    if (!call) {
                        syslog(LOG_ERR, "outgoing call not found (cid=%d)", cid);
                        goto fini_mo;
                    }

                    if (call->call_dir != call_dir_outgoing) {
                        syslog(LOG_INFO, "incorrect call direction found (cid=%d)", cid);
                        goto fini_mo;
                    }

                    syslog(LOG_DEBUG, "outgoing call found (cid=%d)", cid);

                    ///////////////////////////////////////////////////////////////////////////////
                    // process notification - ALERTING
                    ///////////////////////////////////////////////////////////////////////////////

                    if (!strcmp(call_desc, "ALERTING")) {

                        syslog(LOG_INFO, "[NOTI] got %d %s", cid, call_desc);

                        if (call->call_status < call_status_ringback) {
                            syslog(LOG_INFO, "new outgoing call detected (cid=%d)", cid);

                            if (emergency)
                                call->call_type = call_type_emergency;

                            /* update rdb call status */
                            rdb_set_voice_call_status(call->call_track_id, "dial", call->cid_num);
                        }

                        syslog(LOG_INFO, "[NOTI] ringback alert detected (alert_type=%s)", alert_type);

                        /* get ringback types */
                        ringback_local = !strcmp(alert_type, "REMOTE");
                        ringback_remote = !strcmp(alert_type, "LOCAL");;
                        ringback_none = !strcmp(alert_type, "NONE");;

                        cc_make_mo_ringback(call);

                        /* send 'ringback' or 'earlymedia' to foreground call */
                        fxs_for_each_foreground_call_begin(csptr, fsm, foreground_call) {

                            if (ringback_local) {
                                syslog(LOG_DEBUG, "send 'ringback local' to foreground call");
                                fsm_post_event(fsm, fxs_event_ringback_local, NULL, 0);
                            } else if (ringback_remote) {
                                syslog(LOG_DEBUG, "send 'ringback remote' to foreground call");
                                fsm_post_event(fsm, fxs_event_ringback_remote, NULL, 0);
                            } else if (ringback_none) {
                                syslog(LOG_DEBUG, "ringback none, send 'ringback remote' to foreground call");
                                fsm_post_event(fsm, fxs_event_ringback_remote, NULL, 0);
                            }

                            fxs_for_each_foreground_call_end();
                        }

                    } else if (!strcmp(call_desc, "CONVERSATION")) {
                        syslog(LOG_INFO, "[NOTI] got %d %s", cid, call_desc);

                        /*

                         • n-way calls

                            1. AT&T network starts CONVERSATION of n-way call without ALERT.
                            2. AT&T FWLL network in Colby USA starts CONVERSATION without ALERT - allow CONVERSATION after being progressed (ORIGINATION).

                         • MO calls

                            According to IS#39904 log, AT&T network occasionally omits 180 RINGING. As a result, CONVERSATION occurs with no ALERT
                            QMI indication.

                            1. accept CONVERSATION indiciation without ALERT - allow CONVERSATION after being progressed (ORIGINATION).

                         */

                        if (call->call_status < call_status_ringback) {
                            syslog(LOG_NOTICE, "accept call without 'ringback' indication (cid=%d)", cid);
                        }

                        if (call->call_status < call_status_connected) {
                            /* perform conference when hookflash due to outgoing call */
                            fsm = (struct fsm_t*) call->ref;
                            if (fsm) {
                                syslog(LOG_DEBUG, "[hookflash] set perform_conference_when_hookflash to TRUE");
                                csptr = (struct chan_state_t*) fsm->ref;
                                csptr->perform_conference_when_hookflash = TRUE;
                            } else {
                                syslog(LOG_ERR, "outgoing CONVERSATON event received with no FSM associated #2");
                            }
                        }

                        switch (call->call_status) {
                            case call_status_held:
                                syslog(LOG_INFO, "unhold call (cid=%d)", cid);
                                cc_connect_mo_call(call);
                                break;

                            case call_status_connected:
                                syslog(LOG_INFO, "call already connected, skip (cid=%d)", cid);
                                break;

                            default:
                                syslog(LOG_INFO, "[RINGBACK] reset ringback in MO conversation (cid=%d,call_status=%d)", cid,
                                       call->call_status);

                                cc_connect_mo_call(call);

                                /* TODO: disengage ringback */
                                break;
                        }

                        /* send 'connected' to foreground call */
                        fsm = (struct fsm_t*) call->ref;
                        if (fsm) {
                            syslog(LOG_DEBUG, "send 'connected' to foreground call");
                            fsm_post_event(fsm, fxs_event_connected, NULL, 0);
                        } else {
                            syslog(LOG_ERR, "outgoing CONVERSATON event received with no FSM associated");
                        }

                        /* update call status */
                        rdb_set_voice_call_status(call->call_track_id, "conversation", call->cid_num);

                    }

                }

fini_mo:
                __noop();

            }

            ///////////////////////////////////////////////////////////////////////////////
            // reprogram foreground call
            ///////////////////////////////////////////////////////////////////////////////

            /* detect new foreground call */
            if (!strcmp(call_desc, "CONVERSATION")) {

                call = cc_get_call_by_cid(cid);
                if (!call) {
                    syslog(LOG_DEBUG, "call in CONVERSATION not found");
                    goto fini_fg_det;
                }

                fsm = (struct fsm_t*) call->ref;
                if (!fsm) {
                    syslog(LOG_DEBUG, "call not associated with FSM");
                    goto fini_fg_det;
                }

                csptr = (struct chan_state_t*) fsm->ref;

                if (!csptr->foreground_call || (csptr->foreground_call->call_status >= call_status_connected)) {
                    syslog(LOG_DEBUG, "no foreground call associated, send 'foreground_call_changed'");

                    cc_set_foreground_call(fsm, call);
                    fsm_post_event(fsm, fxs_event_fgcall_changed, NULL, 0);
                }

fini_fg_det:
                __noop();
            }

            if (!strcmp(call_desc, "HOLD")) {

                ///////////////////////////////////////////////////////////////////////////////
                // process notification - HOLD
                ///////////////////////////////////////////////////////////////////////////////

                call = cc_get_call_by_cid(cid);
                if (!call) {
                    syslog(LOG_DEBUG, "[NOTI] cid not found (cid=%d), ignore notification", cid);
                } else {
                    /* update call status */
                    syslog(LOG_DEBUG, "[NOTI] hold call (cid=%d)", cid);
                    call->call_status = call_status_held;

                    rdb_set_voice_call_status(call->call_track_id, "hold", call->cid_num);
                }

            } else if (!strcmp(call_desc, "DISCONNECTING")) {

                ///////////////////////////////////////////////////////////////////////////////
                // process notification - DISCONNECTING
                ///////////////////////////////////////////////////////////////////////////////

                /* nothing to do */

            } else if (!strcmp(call_desc, "END")) {

                int disconnected;
                int ringing_others;
                int ringing_cur;

                ///////////////////////////////////////////////////////////////////////////////
                // process notification - END
                ///////////////////////////////////////////////////////////////////////////////

                syslog(LOG_INFO, "[NOTI] got %d %s %s", cid, call_dir, call_desc);

                syslog(LOG_INFO, "perform pending MT call ring by DISCONNECTING (cid=%d)", cid);
                qc_fixup_perform_pending_mt_call(cid);

                /* get call */
                call = cc_get_call_by_cid(cid);
                if (!call) {
                    syslog(LOG_ERR, "[INCALL] no waiting MO/MT call found in end (cid=%d)", cid);
                    goto fini_end;
                }

                /* see if it is the last ringing call */
                ringing_others = cc_is_any_call_ringing_or_waiting(call);
                ringing_cur = (call->call_status == call_status_ringing) || (call->call_status == call_status_waiting);

                /* broadcast stop ringing */
                if (!ringing_others && ringing_cur) {
                    syslog(LOG_DEBUG, "call end, broadcast 'stop_ringing' to fsm");
                    fxs_broadcast_event(fxs_event_stop_ringing, NULL, 0);
                }

                cc_make_call_disconnected(call);

                /* send 'fgcall_disconnected' to foreground call */
                fsm = (struct fsm_t*) call->ref;
                if (fsm) {
                    if (cc_is_call_associate_with_fsm(fsm, call)) {
                        syslog(LOG_DEBUG, "send 'fgcall_disconnected' to foreground call");
                        fsm_post_event(fsm, fxs_event_fgcall_disconnected, NULL, 0);
                    }
                }

                /* set if the call is rejected */
                call->mt_call_declined = call->call_dir == call_dir_incoming && !strcmp(call_end_reason, "CALL_REJECTED");

                /* update call status */
                rdb_set_voice_call_status(call->call_track_id, "end", call->cid_num);

                /* remove cid */
                cc_remove_call(call);

                #ifdef CONFIG_FUTURE_FEATURE
                /* if no active call exists */
                if (!cc_is_empty() && !cc_is_any_call_active()) {

                    /* TODO: stop PCM devices */

                    #if 0 /* originally commented block */
                    if (cc_is_any_call_on_hold(dev)) {
                        /*
                         ## TODO ##

                         it is currently unknown how to re-activate background call on hold.
                         this background call on hold is causing issues since no stream is available.

                         at this stage, we hangup all held calls when we answer a new call.
                         */

                        syslog(LOG_INFO, "[INCALL] release calls on hold (cid=%d)", cid_num);
                        rdb_hangup_calls_on_hold(dev);
                    }
                    #endif
                }

                /* if no call exists */
                if (cc_is_empty()) {
                    syslog(LOG_DEBUG, "[INCALL] no call left, back to none (cid=%d)", cid_num);

                    /* TODO: do something */
                }
                #endif

fini_end:
                /* broadcast 'disconnected' to fsm */
                disconnected = 1;
                call_track_walk_for_each_begin(call) {
                    switch (call->call_status) {
                        case call_status_ringing:
                        case call_status_waiting:
                        case call_status_disconnected:
                            break;

                        default:
                            disconnected = 0;
                            break;
                    }
                }

                if (disconnected) {
                    syslog(LOG_DEBUG, "broadcast 'disconnected' to fms");
                    fxs_broadcast_event(fxs_event_disconnected, NULL, 0);
                }

            }

            #if 0
            /* swtich state machine */
            switch (incall_get_stat(dev)) {

                case incall_stat_disconnected:
                    if (cc_is_any_call_active(dev)) {
                        syslog(LOG_INFO, "[INCALL] turn off disconnected tone (cid=%d)", cid_num);
                        incall_set_stat(dev, incall_stat_none);
                    }
                    break;

                case incall_stat_ringing:
                    if (!cc_is_any_call_ringing(dev)) {
                        syslog(LOG_INFO, "[INCALL] turn off incall ringing (cid=%d)", cid_num);
                        incall_set_stat(dev, incall_stat_none);
                    }
                    break;

                case incall_stat_waiting:

                    if (!cc_is_any_call_to_be_answered(dev)) {
                        syslog(LOG_INFO, "[INCALL] send INFO CW_STOP (cid=%d)", cid_num);
                        incall_send_info_to_peer(dev, DTMF_DIGIT_CW_STOP, NULL, NULL, NULL);

                        syslog(LOG_DEBUG, "[INCALL] turn off incall callwaiting tone", cid_num);
                        incall_set_stat(dev, incall_stat_none);
                    }

                    break;
            }
            #endif

            ///////////////////////////////////////////////////////////////////////////////
            // log call status
            ///////////////////////////////////////////////////////////////////////////////

            /* log state-machine */
            syslog(LOG_DEBUG, "[ch-status] * channel info");

            for (i = 0; i < _fxs_info->num_of_channels; i++) {
                csptr = &_chan_state[i];
                fsm = &csptr->fsm;
                foreground_call = csptr->foreground_call;

                state_name = fsm_get_current_state_name(fsm);

                syslog(LOG_DEBUG, "[ch-status] channel %d (fsm=%s)", csptr->chan_idx, state_name);
                syslog(LOG_DEBUG, "[ch-status] foreground call (cid=%d)",
                       foreground_call ? foreground_call->call_track_id : -1);
            }

            if (cc_is_empty()) {
                syslog(LOG_DEBUG, "[call-status] no call available");
            } else {
                struct call_track_t* call;

                syslog(LOG_DEBUG, "[call-status] * call status");

                call_track_walk_for_each_begin(call) {

                    syslog(LOG_DEBUG, "[call-status] cid#%d", call->call_track_id);

                    syslog(LOG_DEBUG, "[call-status] clip : '%s'", call->cid_num);
                    syslog(LOG_DEBUG, "[call-status] pi : '%s'", call->cid_num_pi);
                    syslog(LOG_DEBUG, "[call-status] name : '%s'", call->cid_name);

                    syslog(LOG_DEBUG, "[call-status] call dir : %s(%d)", call_dir_names[call->call_dir], call->call_dir);
                    syslog(LOG_DEBUG, "[call-status] call type : %s(%d)", call_type_names[call->call_type], call->call_type);
                    syslog(LOG_DEBUG, "[call-status] call status : %s(%d)", call_status_names[call->call_status], call->call_status);

                    call_track_walk_for_each_end();
                }
            }

            break;
        }

        case vc_set_tty_mode:
        case vc_suppl_forward: {
            const char* cmd_result;
            struct event_arg_suppl_ack_t event_arg_suppl_ack;

            if (cmd_idx == vc_set_tty_mode) {
                if (a->argc < 2) {
                    syslog(LOG_ERR, "[NOTI] incorrect argc for set_tty_mode (argc=%d)", a->argc);
                    break;
                }

                cmd_result = a->argv[1];

            } else {

                if (a->argc < 4) {
                    syslog(LOG_ERR, "[NOTI] incorrect argc for suppl forward (argc=%d)", a->argc);
                    break;
                }

                cmd_result = a->argv[3];
            }

            if (!strcmp(cmd_result, "OK")) {
                event_arg_suppl_ack.res = event_arg_suppl_ack_res_ok;
            } else {
                event_arg_suppl_ack.res = event_arg_suppl_ack_res_err;
            }

            fxs_broadcast_event(fxs_event_suppl_ack, &event_arg_suppl_ack, sizeof(event_arg_suppl_ack));
            break;
        }

        /* ignore known replies for voice command */
        case vc_answer:
        case vc_call:
        case vc_pcall:
        case vc_ocall:
        case vc_ecall:
        case vc_hangup:
        case vc_start_dtmf:
        case vc_stop_dtmf:
        case vc_manage_calls:
        case vc_manage_ip_calls:
        case vc_init_voice_call:
        case vc_fini_voice_call:  {
            break;
        }

        default: {
            syslog(LOG_DEBUG, "unknown notification (idx=%d,cmd='%s')", cmd_idx, cmd_str);
            break;
        }
    }
}

/**
 * @brief RDB notification handler.
 *
 * @param rdb is RDB variable.
 * @param val is RDB value.
 */
static void on_rdb(const char* rdb, const char* val)
{
    int stat;

    int indexed_rdb;

    indexed_rdb = indexed_rdb_get_cmd_idx(_ir_subscribed, rdb);

    switch (indexed_rdb) {

        case subscribed_rdb_voice_mgmt_command_ctrl: {
            struct strarg_t* a = NULL;
            int i;

            if (!*val) {
                goto fini_mgmt;
            }

            a = strarg_create(val);
            if (!a || !a->argv[0]) {
                syslog(LOG_ERR, "strarg_create() failed in process_msg_event()");
                goto fini_mgmt;
            }

            /* if delete_log */
            if (!strcmp(a->argv[0], "DELETE_ALL_CALL_HISTORY")) {
                /* save blank hash table to delete all call history */
                syslog(LOG_DEBUG, "[CALL-HISTORY] delete all history");
                call_history_mgmt_save();
            } else if (!strcmp(a->argv[0], "DELETE_CALL_HISTORY")) {

                syslog(LOG_DEBUG, "[CALL-HISTORY] got DELETE_LOG command (arg=%s)", val);

                syslog(LOG_DEBUG, "[CALL-HISTORY] load history");
                call_history_mgmt_load();

                for (i = 1; i < a->argc; i++) {
                    syslog(LOG_DEBUG, "[CALL_HISTORY] delete %s", a->argv[i]);
                    call_history_mgmt_del(a->argv[i]);
                }

                syslog(LOG_DEBUG, "[CALL-HISTORY] save history");
                call_history_mgmt_save();

                syslog(LOG_DEBUG, "[CALL-HISTORY] unload history");
                call_history_mgmt_unload();
            }

            /* reset rdb */
            if (ezrdb_set_str(rdb, "") < 0) {
                syslog(LOG_ERR, "failed to reset voice notification - %s", strerror(errno));
                goto fini_mgmt;
            }

fini_mgmt:
            strarg_destroy(a);
            break;
        }

        case subscribed_rdb_voice_command_noti: {

            if (!*val) {
                goto fini;
            }

            /* feed val to pp */
            stat = rwpipe_feed_read(_pp, val);
            if (stat < 0) {
                syslog(LOG_ERR, "on_rdb_noti() failed to feed - %s", strerror(errno));
                goto fini;
            }

            /* reset rdb */
            if (ezrdb_set_str(rdb, "") < 0) {
                syslog(LOG_ERR, "failed to reset voice notification - %s", strerror(errno));
                goto fini;
            }
            break;
        }

        case subscribed_rdb_voice_command_ctrl: {

            /* signal to write-queue */
            rwpipe_set_wq_signal(_pp);
            break;
        }

        case subscribed_rdb_system_mode: {
            static char* val_prev = NULL;

            if (!val_prev || strcmp(val_prev, val)) {

                free(val_prev);
                val_prev = strdup(val);

                syslog(LOG_DEBUG, "network registration RDB changed, broadcast event");
                fxs_broadcast_event(fxs_event_service_stat_chg, NULL, 0);
            }

            break;
        }

        case subscribed_rdb_voicemail_active: {
            fxs_broadcast_event(fxs_event_voicemail_stat_chg, NULL, 0);
            break;
        }

        case subscribed_rdb_block_call_trigger: {
            block_calls_rebuild();
            break;
        }
    }

fini:
    return;
}

///////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief main.
 *
 * @param argc is argument count.
 * @param argv[] is an array of arguments.
 *
 * @return
 */
int main(int argc, char* argv[])
{
    struct fsm_state_t fxs_states[fxs_state_last];

    static struct rdb_session* rdb_session;

    int rc = -1;
    int i;

    struct chan_state_t* csptr;
    SiVoiceChanType_ptr cptr;
    struct fsm_t* fsm;

    struct dbenum_t* dbenum;

    struct dbenumitem_t* item;
    char plan[DIALPLAN_MAX_PLAN_LEN];

    int voice_command_ctrl_timestamp_valid = 0;
    int voice_command_ctrl_timestamp;

    int voice_command_ctrl_now;
    int voice_command_ctrl_elapsed;

    ///////////////////////////////////////////////////////////////////////////////
    // initiate local variables
    ///////////////////////////////////////////////////////////////////////////////

    memset(_fxs_info, 0, sizeof(*_fxs_info));

    /* compile regex for matching phone numbers */
    regcomp(&_phone_num_regex, "[0-9]+", REG_EXTENDED);

    ///////////////////////////////////////////////////////////////////////////////
    // initiate peripherals
    ///////////////////////////////////////////////////////////////////////////////

    init_rdb();

    /* initiate callback timer */
    callback_timer_init();
    /* initiate call track */
    call_track_init();

    ///////////////////////////////////////////////////////////////////////////////
    // initiate dialplan
    ///////////////////////////////////////////////////////////////////////////////

    /* initiate dialplan */
    dialplan_init();

    struct dialplan_func_map_t _dialplan_func_map[] = {
        { "call", diaplan_on_call },
        { "ecall", diaplan_on_emergency_call },
        { "suppl_forward", dialplan_on_suppl_forward },
        { "suppl_forward_reg", dialplan_on_suppl_forward_reg },
        { "voicemail", dialplan_on_voicemail },
        { "set_tty_mode", dialplan_on_set_tty_mode },
        { "pcall", diaplan_on_private_call },
        { "ocall", diaplan_on_public_call },
        { NULL, },
    };

    /* register functions */
    i = 0;
    while (_dialplan_func_map[i].func_name) {
        dialplan_add_func(_dialplan_func_map[i].func_name, _dialplan_func_map[i].func_ptr);
        i++;
    }

    /* register dialplan */
    rdb_session = ezrdb_get_session();
    dbenum = dbenum_create(rdb_session, 0);

    dbenum_enumDbByNames(dbenum, RDB_DIALPLAN);

    item = dbenum_findFirst(dbenum);
    while (item) {

        if (!strncmp(item->szName, RDB_DIALPLAN, strlen_static(RDB_DIALPLAN))) {

            if (ezrdb_get(item->szName, plan, sizeof(plan)) < 0) {
                syslog(LOG_ERR, "failed to read RDB (rdb='%s',error='%s')", item->szName, strerror(errno));
            } else {
                if (dialplan_parse_and_add_plan(plan) < 0) {
                    syslog(LOG_ERR, "failed to parse dial plan (rdb='%s',plan='%s')", item->szName, plan);
                }
            }
        }

        item = dbenum_findNext(dbenum);
    }

    dialplan_sort_plan();

    dbenum_destroy(dbenum);

    if (netcomm_proslic_init() < 0) {
        syslog(LOG_ERR, "failed to initiate proslic");
        goto fini;
    }

    ///////////////////////////////////////////////////////////////////////////////
    // initiate channels
    ///////////////////////////////////////////////////////////////////////////////

    if (pbx_channels_setup() < 0) {
        syslog(LOG_DEBUG, "failed to setup CID");
        goto fini;
    }

    ///////////////////////////////////////////////////////////////////////////////
    // initiate finite-state machine per channel
    ///////////////////////////////////////////////////////////////////////////////

    /* initiate states for state machine */
    memset(fxs_states, 0, sizeof(fxs_states));
    fxs_states[fxs_state_idle].actions[fsm_action_func_enter] = fxs_state_func_idle_enter;
    fxs_states[fxs_state_idle].actions[fsm_action_func_process] = fxs_state_func_idle;

    fxs_states[fxs_state_dialtone].actions[fsm_action_func_enter] = fxs_state_func_dialtone_enter;
    fxs_states[fxs_state_dialtone].actions[fsm_action_func_process] = fxs_state_func_dialtone;

    fxs_states[fxs_state_digit_collect].actions[fsm_action_func_enter] = fxs_state_func_digit_collect_enter;
    fxs_states[fxs_state_digit_collect].actions[fsm_action_func_process] = fxs_state_func_digit_collect;

    fxs_states[fxs_state_place_call].actions[fsm_action_func_enter] = fxs_state_func_place_call_enter;
    fxs_states[fxs_state_place_call].actions[fsm_action_func_process] = fxs_state_func_place_call;

    fxs_states[fxs_state_ringback_remote].actions[fsm_action_func_enter] = fxs_state_func_ringback_remote_enter;
    fxs_states[fxs_state_ringback_remote].actions[fsm_action_func_process] = fxs_state_func_ringback_remote;

    fxs_states[fxs_state_ringback_local].actions[fsm_action_func_enter] = fxs_state_func_ringback_local_enter;
    fxs_states[fxs_state_ringback_local].actions[fsm_action_func_process] = fxs_state_func_ringback_local;

    fxs_states[fxs_state_connected].actions[fsm_action_func_enter] = fxs_state_func_connected_enter;
    fxs_states[fxs_state_connected].actions[fsm_action_func_process] = fxs_state_func_connected;

    fxs_states[fxs_state_force_connected].actions[fsm_action_func_enter] = fxs_state_func_connected_enter;
    fxs_states[fxs_state_force_connected].actions[fsm_action_func_process] = fxs_state_func_connected;

    fxs_states[fxs_state_prereorder].actions[fsm_action_func_enter] = fxs_state_func_prereorder_enter;
    fxs_states[fxs_state_prereorder].actions[fsm_action_func_process] = fxs_state_func_prereorder;

    fxs_states[fxs_state_reorder].actions[fsm_action_func_enter] = fxs_state_func_reorder_enter;
    fxs_states[fxs_state_reorder].actions[fsm_action_func_process] = fxs_state_func_reorder;

    fxs_states[fxs_state_ringing_1st].actions[fsm_action_func_enter] = fxs_state_func_ringing_1st_enter;
    fxs_states[fxs_state_ringing_1st].actions[fsm_action_func_process] = fxs_state_func_ringing_1st;

    fxs_states[fxs_state_fsk_preamble_vmwi].actions[fsm_action_func_enter] = fxs_state_func_fsk_preamble_enter;
    fxs_states[fxs_state_fsk_preamble_vmwi].actions[fsm_action_func_process] = fxs_state_func_fsk_preamble;

    fxs_states[fxs_state_fsk_preamble_caller_id].actions[fsm_action_func_enter] = fxs_state_func_fsk_preamble_enter;
    fxs_states[fxs_state_fsk_preamble_caller_id].actions[fsm_action_func_process] = fxs_state_func_fsk_preamble;

    fxs_states[fxs_state_fsk_preamble_wcaller_id].actions[fsm_action_func_enter] = fxs_state_func_fsk_preamble_enter;
    fxs_states[fxs_state_fsk_preamble_wcaller_id].actions[fsm_action_func_process] = fxs_state_func_fsk_preamble;

    fxs_states[fxs_state_fsk_cid].actions[fsm_action_func_enter] = fxs_state_func_fsk_cid_enter;
    fxs_states[fxs_state_fsk_cid].actions[fsm_action_func_process] = fxs_state_func_fsk_cid;

    fxs_states[fxs_state_fsk_wcid].actions[fsm_action_func_enter] = fxs_state_func_fsk_cid_enter;
    fxs_states[fxs_state_fsk_wcid].actions[fsm_action_func_process] = fxs_state_func_fsk_cid;

    fxs_states[fxs_state_ringing_2nd].actions[fsm_action_func_enter] = fxs_state_func_ringing_2nd_enter;
    fxs_states[fxs_state_ringing_2nd].actions[fsm_action_func_process] = fxs_state_func_ringing_2nd;

    fxs_states[fxs_state_ringing].actions[fsm_action_func_enter] = fxs_state_func_ringing_enter;
    fxs_states[fxs_state_ringing].actions[fsm_action_func_process] = fxs_state_func_ringing;

    fxs_states[fxs_state_answer].actions[fsm_action_func_enter] = fxs_state_func_answer_enter;
    fxs_states[fxs_state_answer].actions[fsm_action_func_process] = fxs_state_func_answer;

    fxs_states[fxs_state_disconnect].actions[fsm_action_func_enter] = fxs_state_func_disconnect_enter;
    fxs_states[fxs_state_disconnect].actions[fsm_action_func_process] = fxs_state_func_disconnect;

    fxs_states[fxs_state_init_mwi].actions[fsm_action_func_enter] = fxs_state_func_init_mwi_enter;
    fxs_states[fxs_state_init_mwi].actions[fsm_action_func_process] = fxs_state_func_init_mwi;

    fxs_states[fxs_state_fsk_vmwi].actions[fsm_action_func_enter] = fxs_state_func_fsk_vmwi_enter;
    fxs_states[fxs_state_fsk_vmwi].actions[fsm_action_func_process] = fxs_state_func_fsk_vmwi;

    fxs_states[fxs_state_wcid_init].actions[fsm_action_func_enter] = fxs_state_func_wcid_init_enter;
    fxs_states[fxs_state_wcid_init].actions[fsm_action_func_process] = fxs_state_func_wcid_init;

    fxs_states[fxs_state_wcid_wait_ack].actions[fsm_action_func_enter] = fxs_state_func_wcid_wait_ack_enter;
    fxs_states[fxs_state_wcid_wait_ack].actions[fsm_action_func_process] = fxs_state_func_wcid_wait_ack;

    fxs_states[fxs_state_suppl].actions[fsm_action_func_enter] = fxs_state_func_suppl_enter;
    fxs_states[fxs_state_suppl].actions[fsm_action_func_process] = fxs_state_func_suppl;

    /* initiate channel state machines */
    for (i = 0; i < _fxs_info->num_of_channels; i++) {

        csptr = &_chan_state[i];

        /* get state machine */
        fsm = &csptr->fsm;

        /* initiate state machine */
        fsm_init(fsm, fxs_states, __countof(fxs_states), csptr);
        fsm_register_event_names(fsm, fxs_event_names, __countof(fxs_event_names));
        fsm_register_state_names(fsm, fxs_state_names, __countof(fxs_state_names));

        /* start idle state */
        fsm_switch_state(fsm, fxs_state_idle, fxs_event_none, NULL, 0);

    };

    ///////////////////////////////////////////////////////////////////////////////
    // main loop
    ///////////////////////////////////////////////////////////////////////////////

    struct timeval tv;
    fd_set rfd;

    /* initiate signals */
    signal(SIGHUP, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    int stat;
    int fd_q;
    int fd_rdb;
    int nfds;

    fd_rdb = rdb_fd(rdb_session);
    fd_q = rwpipe_get_wq_fd(_pp);

    nfds = max(fd_rdb, fd_q) + 1;

    while (_bridge_running) {

        ///////////////////////////////////////////////////////////////////////////////
        // channel task
        ///////////////////////////////////////////////////////////////////////////////

        /* process callback timer */
        callback_timer_process();

        /* process pending mt call */
        qc_fixup_perform_pending_mt_call(0);

        ///////////////////////////////////////////////////////////////////////////////
        // select
        ///////////////////////////////////////////////////////////////////////////////

        /* initiate select params */
        FD_ZERO(&rfd);
        FD_SET(fd_rdb, &rfd);
        FD_SET(fd_q, &rfd);
        tv.tv_sec = 0;
        tv.tv_usec = DURATION_SELECT * 1000;

        /* select */
        stat = select(nfds, &rfd, NULL, NULL, &tv);

        /* poll slic channel interrupts */
        for (i = 0; i < _fxs_info->num_of_channels; i++) {

            csptr = &_chan_state[i];
            cptr = csptr->cptr;

            if (cptr->channelType == PROSLIC) {
                poll_channel_interrupts(csptr, i);

                #ifdef CONFIG_DEBUG_VBAT
                {
                    int32 vbat;
                    int addr = 3; // SI3217X_COM_RAM_MADC_VBAT
                    vbat = netcomm_proslic_ram_get(cptr, addr);
                    if (vbat & 0x10000000L) {
                        vbat |= 0xF0000000L;
                    }

                    printf("channel %d (vbat %d.%d v)\n", i, (int)((vbat / SCALE_V_MADC) / 1000),
                           abs(((vbat / SCALE_V_MADC) - (vbat / SCALE_V_MADC) / 1000 * 1000)));

                }
                #endif

                /* monitor SLIC clock */
                monitor_slic_hw_stat();
            }
        }

        voice_command_ctrl_now = __get_realtime_sec();

        /* reset voice command ctrl if time out occurs */
        if (voice_command_ctrl_timestamp_valid) {
            voice_command_ctrl_elapsed = voice_command_ctrl_now - voice_command_ctrl_timestamp;

            if (voice_command_ctrl_elapsed > RWPIPE_DEFAULT_RDB_CMD_TIMEOUT) {
                syslog(LOG_ERR, "voice command ctrl time out, reset ctrl RDB (timeout=%d sec)", RWPIPE_DEFAULT_RDB_CMD_TIMEOUT);

                if (ezrdb_set_str(RDB_VOICE_COMMAND_CTRL, "") < 0) {
                    syslog(LOG_ERR, "failed to send voice control - %s #2", strerror(errno));
                }

                voice_command_ctrl_timestamp_valid = FALSE;
            }
        }

        if (stat < 0) {
            if (errno == EINTR) {
                syslog(LOG_NOTICE, "interrupted by signal");
                continue;
            } else {
                syslog(LOG_NOTICE, "failed to select - %s", strerror(errno));
                break;
            }

        } else if (stat == 0) {
            /* nothing to do */
        } else {
            ///////////////////////////////////////////////////////////////////////////////
            // process RDB write-queue - flush write-queue to RDB
            ///////////////////////////////////////////////////////////////////////////////
            if (FD_ISSET(fd_q, &rfd)) {

                char val[RDB_MAX_VAL_LEN];

                char msg_to_write[RDB_MAX_VAL_LEN];

                /* read ctrl */
                rwpipe_clear_wq_signal(_pp);

                if (ezrdb_get(RDB_VOICE_COMMAND_CTRL, val, sizeof(val)) < 0) {
                    goto fini_qwrite;
                }

                /* bypass if value exists */
                if (*val)
                    goto fini_qwrite;

                /* reset timestamp valid flag */
                voice_command_ctrl_timestamp_valid = FALSE;

                /* bypass if no data is taken */
                if (rwpipe_pop_wr_msg(_pp, msg_to_write, sizeof(msg_to_write)) < 0) {
                    syslog(LOG_DEBUG, "no voice command message to write");
                    goto fini_qwrite;
                }

                if (ezrdb_set_str(RDB_VOICE_COMMAND_CTRL, msg_to_write) < 0) {
                    syslog(LOG_ERR, "failed to send voice control - %s", strerror(errno));
                    goto fini_qwrite;
                }

                /* store timestamp */
                voice_command_ctrl_timestamp_valid = TRUE;
                voice_command_ctrl_timestamp = voice_command_ctrl_now;

fini_qwrite:
                __noop();
            }

            ///////////////////////////////////////////////////////////////////////////////
            // process RDB trigger - process voice event
            ///////////////////////////////////////////////////////////////////////////////
            if (FD_ISSET(fd_rdb, &rfd)) {
                struct dbenumitem_t* item;
                int total_triggers;

                char val[RDB_MAX_VAL_LEN];

                char msg[RDB_MAX_VAL_LEN];

                syslog(LOG_DEBUG, "[rdb] rdb signal");

                total_triggers = dbenum_enumDb(_dbenum);
                if (total_triggers <= 0) {
                    syslog(LOG_DEBUG, "[rdb] triggered but no flagged rdb variable found - %s", strerror(errno));
                } else {
                    syslog(LOG_DEBUG, "[rdb] walk through triggered rdb");

                    item = dbenum_findFirst(_dbenum);
                    while (item) {
                        /* read rdb val */
                        if (ezrdb_get(item->szName, val, sizeof(val)) < 0) {
                            syslog(LOG_ERR, "[rdb] cannot read a triggered rdb (rdb=%s) - %s", item->szName,
                                   strerror(errno));
                            goto fini_rdb;
                        }

                        syslog(LOG_DEBUG, "[rdb] rdb triggered (rdb='%s',val='%s')", item->szName, val);

                        /* call rdb event */
                        on_rdb(item->szName, val);

                        item = dbenum_findNext(_dbenum);
                    }
                }

fini_rdb:

                ///////////////////////////////////////////////////////////////////////////////
                // process RDB read-queue
                ///////////////////////////////////////////////////////////////////////////////

                if (!(rwpipe_pop_rd_msg(_pp, NULL, msg, sizeof(msg), 0) < 0)) {
                    struct strarg_t* a;

                    a = strarg_create(msg);
                    if (!a || !a->argv[0]) {
                        syslog(LOG_ERR, "strarg_create() failed in process_msg_event()");
                        goto fini;
                    }

                    syslog(LOG_DEBUG, "cmd (%s) received (msg=%s)", a->argv[0], msg);
                    on_event(a);

                    strarg_destroy(a);
                }
            }
        }

        #if 0
        cptr = _chan_state[0].cptr;
        proslic_sleep(cptr, DURATION_SELECT);
        #endif

    }

fini:
    syslog(LOG_NOTICE, "POTS bridge termination procedure starts");

    ///////////////////////////////////////////////////////////////////////////////
    // finalize finite-state machine per channel
    ///////////////////////////////////////////////////////////////////////////////

    for (i = 0; i < _fxs_info->num_of_channels; i++) {
        csptr = &_chan_state[i];
        fsm = &csptr->fsm;

        /* finalize state machine */
        fsm_fini(fsm);

    }

    ///////////////////////////////////////////////////////////////////////////////
    // finalize channels
    ///////////////////////////////////////////////////////////////////////////////

    pbx_channels_teardown();

    ///////////////////////////////////////////////////////////////////////////////
    // finalize ProSDK device (port)
    ///////////////////////////////////////////////////////////////////////////////

    for (i = 0; i < PBX_PORT_COUNT; i++) {
        if (_fxs_info->ports)
            pbx_shutdown(&(_fxs_info->ports[i]));
    }

    netcomm_proslic_fini();

    ///////////////////////////////////////////////////////////////////////////////
    // finalize dialplan
    ///////////////////////////////////////////////////////////////////////////////

    /* finalize dialplan */
    dialplan_fini();

    ///////////////////////////////////////////////////////////////////////////////
    // finalize peripherals
    ///////////////////////////////////////////////////////////////////////////////

    /* finalize call track */
    call_track_fini();

    /* finalize callback timer */
    callback_timer_fini();

    fini_rdb();

    syslog(LOG_NOTICE, "POTS bridge terminated");

    return rc;
}

