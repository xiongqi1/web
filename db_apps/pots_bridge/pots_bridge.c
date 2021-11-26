/*!
* Copyright Notice:
* Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
*
* This file or portions thereof may not be copied or distributed in any form
* (including but not limited to printed or electronic forms and binary or object forms)
* without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
* Copyright laws and International Treaties protect the contents of this file.
* Unauthorized use is prohibited.
*
*
* THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
* CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
* SUCH DAMAGE.
*
*/

#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <unistd.h>
#include <alloca.h>
#include <sys/ioctl.h>

#ifndef USE_ALSA
#include <sys/select.h>
#endif

#include "slic/types.h"
#include "cdcs_utils.h"
#include "cdcs_syslog.h"
#include "../cnsmgr/voicecall_constants.h"
#include "./calls/calls.h"
#include "./daemon.h"
#include "./pots_rdb_operations.h"
#include "./slic_control/slic_control.h"
#ifndef USE_ALSA
/* slic cal data save/restore feature */
#include "slic/calibration.h"
#endif
#include "./telephony_profile.h"

#if defined(PLATFORM_PLATYPUS)
#include <nvram.h>
#endif
#if defined(PLATFORM_PLATYPUS) || defined(PLATFORM_PLATYPUS2)
#include "ntc_pcm_cmd.h"
#endif

#define SUCCESS 0

static char* loop_stat_names[] = { "on hook", "off hook" };

typedef enum
{
	event_on_hook
	, event_off_hook
	, event_recall
	, event_digit
	, event_dial_timer_expired
	, event_ss_response
	, event_call_state_changed
	, event_shutdown
	, event_roh_timer_expired
	, event_vmwi_timer_expired
	, event_stutter_tone_timer_expired
	, event_DTMF_timer_expired
/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
	, event_autodial_timer_expired
#endif
	, event_congestion_tone_timer_expired
	, event_none
} pots_bridge_event_t;

static char* event_names[] = {
	"on hook"
	, "off hook"
	, "recall"
	, "digit"
	, "dial timer expired"
	, "ss response"
	, "call state changed"
	, "shutdown"
	, "roh timer expired"
	, "vmwi timer expired"
	, "stutter tone timer expired"
	, "incall dtmf timer expired"
/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
	, "auto dial timer expired"
#endif
	, "congestion tone timer expired"
	, "none"
};

static void handle_pots_event_on_hook(int idx, pots_bridge_event_t event);
static void handle_pots_event_off_hook(int idx, pots_bridge_event_t event);
static void handle_pots_event_recall(int idx, pots_bridge_event_t event);
static void handle_pots_event_digit(int idx, pots_bridge_event_t event);
static void handle_pots_event_dial_timer_expired(int idx, pots_bridge_event_t event);
static void handle_pots_event_ss_response(int idx, pots_bridge_event_t event);
static void handle_pots_event_call_state_changed(int idx, pots_bridge_event_t event);
static void handle_pots_event_shutdown(int idx, pots_bridge_event_t event);
static void handle_pots_event_roh_timer_expired(int idx, pots_bridge_event_t event);
static void handle_pots_event_vmwi_timer_expired(int idx, pots_bridge_event_t event);
static void handle_pots_event_stutter_tone_timer_expired(int idx, pots_bridge_event_t event);
static void handle_pots_event_dtmf_timer_expired(int idx, pots_bridge_event_t event);
/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
static void handle_pots_event_autodial_timer_expired(int idx, pots_bridge_event_t event);
#endif
static void handle_pots_event_congestion_tone_timer_expired(int idx, pots_bridge_event_t event);
static void handle_pots_event_none(int idx, pots_bridge_event_t event);

typedef void (*p_handle_pots_event_type)(int, pots_bridge_event_t);
p_handle_pots_event_type p_handle_pots_event[event_none+1] =
{
		handle_pots_event_on_hook,
		handle_pots_event_off_hook,
		handle_pots_event_recall,
		handle_pots_event_digit,
		handle_pots_event_dial_timer_expired,
		handle_pots_event_ss_response,
		handle_pots_event_call_state_changed,
		handle_pots_event_shutdown,
		handle_pots_event_roh_timer_expired,
		handle_pots_event_vmwi_timer_expired,
		handle_pots_event_stutter_tone_timer_expired,
		handle_pots_event_dtmf_timer_expired,
		/* special emergency dialing feature for Rail Corps. */
		#ifdef AUTOMATED_EMERGENCY_DIALING
		handle_pots_event_autodial_timer_expired,
		#endif
		handle_pots_event_congestion_tone_timer_expired,
		handle_pots_event_none
};

static void handle_pots_event(int idx, pots_bridge_event_t event);
static void play_busy_tone(int idx);

// POTS bridge constants
#define MAX_RDB_EVENTS_PER_SECOND 10
#define DIGITS_SIZE 256
#define LOOP_STATE_POLL_DURATION_MSEC 20
#define RECALL_DURATION_MIN_MSEC 35 // Telstra Generic IAD requirements, 1.2.2.4.1
#define RECALL_DURATION_MAX_MSEC 750 // Canada TELUS
//#define RECALL_DURATION_MAX_MSEC 160 // Telstra Generic IAD requirements, 1.2.2.4.1
//#define RECALL_DURATION_MAX_MSEC 145 // Telstra Generic IAD requirements, 1.2.2.4.1
#define CLEAR_FORWARD_DURATION_MSEC (RECALL_DURATION_MAX_MSEC+100)
//#define CLEAR_FORWARD_DURATION_MSEC 250 // Telstra Generic IAD requirements, 1.2.2.1.3

struct timer_t
{
	struct timeb expiration;
};

typedef struct
{
	char digits[DIGITS_SIZE];
	unsigned int size;
	unsigned int begin;
	unsigned int dialed_size;
	struct timer_t timer;
} number_t;

struct ss_t
{
	BOOL request_pending;
	char command[DIGITS_SIZE];
	char status[16]; // TODO: arbitrary size, define it where appropriate
};

struct dial_plan_t
{
	regex_t regex;
	char regex_string[ REGEX_SIZE ];
	char international_prefix[ INTERNATIONAL_PREFIX_SIZE ];
	unsigned int timeout;
};

/* Bell Canada VMWI feature */
typedef enum
{
	EMPTY = 0,
	ACTIVE,
	INACTIVE
} vmwi_cmd_enum_type;

typedef enum
{
	INIT = 0,
	IDLE,
	NOTI_DELIVERING,
	NOTI_DELIVERED,
	OFFHOOK_PENDING,
	WAITING_TIMEOUT
} vmwi_status_enum_type;

static const char* vmwi_cmd_name[] = { "EMPTY", "ACTIVE", "INACTIVE" };
static const char* vmwi_status_name[] = { "INIT", "IDLE", "NOTI_DELIVERING", "NOTI_DELIVERED", "OFFHOOK_PENDING", "WAITING_TIMEOUT" };

struct vmwi_t
{
	vmwi_cmd_enum_type cmd;
	vmwi_status_enum_type status;
	struct timer_t timer;
};
/* end of Bell Canada VMWI feature */

typedef enum
{
	SIM_OK = 0,
	NO_SIM,
	SIM_LOCKED,
	SIM_PUK,
	MEP_LOCKED,
	LIMITED_SVC
} sim_status_enum_type;
static const char* sim_status_name[] = { "SIM OK", "NO SIM", "SIM LOCKED", "SIM PUK", "MEP LOCKED", "LIMITED SERVICE" };

/* roh timer control structure */
struct roh_t
{
	BOOL initial;			/* for Telstra ROH specification which needs two timer, 90s and 30s.
							* set initial to TRUE and start 90s timer
							* set initial to FALSE and start 30s timer after 90s timer expired */
	struct timer_t timer;
};

volatile int pots_bridge_running;
extern char pots_bridge_rdb_prefix[64];

static struct pots_bridge_t
{
	number_t number;
	struct dial_plan_t dial_plan;
	struct ss_t ss;
	struct
	{
		slic_on_off_hook_enum state;
		BOOL recall_active;
	} loop;
	struct
	{
		struct call_control_t control;
		char event[32];
		struct timer_t poll_timer;
	} call;
	BOOL wait_for_fsk_complete;
	struct roh_t roh;
	/* Bell Canada VMWI feature */
	struct vmwi_t vmwi;
	/* end of Bell Canada VMWI feature */
	/* out band DTMF sending */
	BOOL send_outband_dmtf;
	pots_led_control_type led_state;
	pots_bridge_event_t pending_event;
	BOOL waiting_for_recall_dialing;
	BOOL detect_3way_calling_prefix;
	int recall_key_count;
	/* Rogers/Telus stutter tone feature */
	struct timer_t stutter_tone_timer;
	number_t incall_dtmf_number;
	BOOL dtmf_key_blocked;
	BOOL waiting_next_dtmf_keys;
	struct timer_t congestion_tone_timer;
} pots_bridge[MAX_CHANNEL_NO];

const char digitmap[] = "D1234567890*#ABC";

/* support international telephony profile */
tel_profile_type tel_profile = DEFAULT_PROFILE;
tel_sp_type tel_sp_profile = SP_TELSTRA;
/* end of support international telephony profile */

/* out band DTMF sending */
#define MBN_DIGITS_LIMIT	10
#define MAX_MBN_DIGITS		11
#define MAX_DIAL_DIGITS		33
char sim_mbn[MAX_MBN_DIGITS]={0,};
char sim_mbn_orig[MAX_DIAL_DIGITS]={0,};
char sim_mbdn[MAX_MBN_DIGITS]={0,};
char sim_mbdn_orig[MAX_DIAL_DIGITS]={0,};
char sim_adn[MAX_MBN_DIGITS]={0,};
char sim_adn_orig[MAX_DIAL_DIGITS]={0,};
char sim_msisdn[MAX_MBN_DIGITS]={0,};
BOOL valid_mbn = FALSE;
BOOL valid_mbdn = FALSE;
BOOL valid_adn = FALSE;
BOOL valid_msisdn = FALSE;
/* end of out band DTMF sending */

/* Rogers Call Return feature */
char last_incoming_call_num[DIGITS_SIZE]={0,};

extern pots_call_type db_slic_call_type[MAX_CHANNEL_NO];
#ifdef HAS_VOIP_FEATURE
BOOL voip_sip_server_registered = FALSE;
#endif	/* HAS_VOIP_FEATURE */

extern char* call_type_name[MAX_CALL_TYPE_INDEX];
int call_type_cnt[MAX_CALL_TYPE_INDEX];

typedef struct
{
	unsigned int outgoing[MAX_CALL_TYPE_INDEX];
	unsigned int incoming[MAX_CALL_TYPE_INDEX];
	unsigned int total[MAX_CALL_TYPE_INDEX];
} call_cnt_type;
call_cnt_type pots_call_count;

/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
BOOL autodial_enabled = FALSE;
int  autodial_dialtone_duration = 1;
char autodial_dial_string[DIGITS_SIZE] = {0,};
struct timer_t autodial_timer;
#endif

int handle_rdb_event_dialplan_regex(const char *var_p);
int handle_rdb_event_dialplan_int_prefix(const char *var_p);
int handle_rdb_event_dialplan_timeout(const char *var_p);
int handle_rdb_event_sms_vm_status(const char *var_p);
int handle_rdb_event_umts_cmd_status(const char *var_p);
int handle_rdb_event_umts_calls_event(const char *var_p);
int handle_rdb_event_mcc_mnc(const char *var_p);
#ifdef HAS_VOIP_FEATURE
int handle_rdb_event_voip_reg_status(const char *var_p);
int handle_rdb_event_voip_cmd_status(const char *var_p);
int handle_rdb_event_voip_calls_event(const char *var_p);
#endif	/* HAS_VOIP_FEATURE */

rdb_handler_type	rdb_handler[] = {
	/* 	prefix						name			instance  trigger	call type		action */
	/* Common */
	{ RDB_DIALPLAN_PREFIX, 		RDB_DP_REGEX, 			0,		1,		NONE,			handle_rdb_event_dialplan_regex },
	{ RDB_DIALPLAN_PREFIX, 		RDB_DP_INT_PREFIX, 		0,		1,		NONE,			handle_rdb_event_dialplan_int_prefix },
	{ RDB_DIALPLAN_PREFIX, 		RDB_DP_TIMEOUT, 		0,		1,		NONE,			handle_rdb_event_dialplan_timeout },
	{ RDB_SMS_VM_PREFIX, 		RDB_VM_STATUS, 			0,		1,		NONE,			handle_rdb_event_sms_vm_status },
	/* 3G UMTS */
	{ RDB_UMTS_CMD_PREFIX, 		"", 					0,		0,		VOICE_ON_3G,	NULL },
	{ RDB_UMTS_CMD_PREFIX, 		RDB_CMD_STATUS, 		0,		1,		VOICE_ON_3G,	handle_rdb_event_umts_cmd_status },
	{ RDB_UMTS_CMD_PREFIX, 		RDB_CMD_RESULT, 		0,		0,		VOICE_ON_3G,	NULL },
	{ RDB_PHONE_CMD_PREFIX, 	"", 					0,		0,		VOICE_ON_3G,	NULL },
	{ RDB_PHONE_CMD_PREFIX, 	RDB_CMD_STATUS, 		0,		0,		VOICE_ON_3G,	NULL },
	{ RDB_UMTS_CALLS_PREFIX,	RDB_CALLS_LIST, 		0,		0,		VOICE_ON_3G,	NULL },
	{ RDB_UMTS_CALLS_PREFIX,	RDB_CALLS_EVENT, 		0,		1,		VOICE_ON_3G,	handle_rdb_event_umts_calls_event },
	{ RDB_UMTS_DTMF_PREFIX, 	"", 					0, 		0, 		VOICE_ON_3G, 	NULL },
	{ RDB_IMSI_PREFIX, 		    RDB_PLMN_MCC, 			0,		1,		VOICE_ON_3G,	handle_rdb_event_mcc_mnc },
	{ RDB_IMSI_PREFIX, 		    RDB_PLMN_MNC, 			0,		1,		VOICE_ON_3G,	handle_rdb_event_mcc_mnc },
#ifdef HAS_VOIP_FEATURE
	/* VOIP */
	{ RDB_VOIP_ST_PREFIX, 		RDB_SIP_REG_RESULT, 	0,		1,		VOICE_ON_IP, 	handle_rdb_event_voip_reg_status },
	{ RDB_VOIP_CMD_PREFIX, 		"", 					1,		0,		VOICE_ON_IP,	NULL },
	{ RDB_VOIP_CMD_PREFIX, 		RDB_CMD_STATUS, 		1,		1,		VOICE_ON_IP,	handle_rdb_event_voip_cmd_status },
	{ RDB_VOIP_CMD_PREFIX, 		RDB_CMD_RESULT, 		1,		0,		VOICE_ON_IP,	NULL },
	{ RDB_VOIP_CALLS_PREFIX,	RDB_CALLS_LIST, 		1,		0,		VOICE_ON_IP,	NULL },
	{ RDB_VOIP_CALLS_PREFIX,	RDB_CALLS_EVENT, 		1,		1,		VOICE_ON_IP,	handle_rdb_event_voip_calls_event },
	{ RDB_VOIP_DTMF_PREFIX, 	"", 					1, 		0, 		VOICE_ON_IP, 	NULL},
	/* FOIP */
#endif	/* HAS_VOIP_FEATURE */
	{ 0,}
};

//-----------------------------------------------------------------------

/* Version Information */
#define VER_MJ		1
#define VER_MN		0
#define VER_BLD	    2

#define APPLICATION_NAME "pots_bridge"

/* The user under which to run */
#define RUN_AS_USER "daemon"

/* Bell Canada VMWI feature */
static void display_vmwi_state( int idx, int new );
static void vmwi_led_control(int idx, pots_led_control_type mode);
/* end of Bell Canada VMWI feature */

/* out band DTMF sending */
static void read_mbdn_msisdn_from_rdb(void);
/* end of out band DTMF sending */

static void set_vmwi_waiting_state( int idx, int timeout );
static unsigned int call_count(int idx, cc_state_enum state);

extern BOOL pots_initialized;
extern BOOL is_modem_exist;

static void update_call_counts(int idx, BOOL outgoing_call);
BOOL is_external_crystal_exist = FALSE;

/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
int read_autodial_variables_from_db(void);
#endif

static BOOL allow_dtmf_key_send(int idx);

static int handle_pots_event_fsk_complete(int idx, const struct slic_event_t* event);

static const char* ct_str(int idx)
{
	return (call_type_name[slic_info[idx].call_type]);
}

void timer_set(struct timer_t* t, unsigned int timeout_sec)
{
	ftime(&t->expiration);
	t->expiration.time += timeout_sec;
}

void timer_reset(struct timer_t* t)
{
	t->expiration.time = 0;
	t->expiration.millitm = 0;
}

/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
static void autodial_timer_set(unsigned int timeout_sec)
{
	SYSLOG_DEBUG("autodial_timer_set");
	timer_set(&autodial_timer, timeout_sec);
}

static void autodial_timer_reset(void)
{
	SYSLOG_DEBUG("autodial_timer_REset");
	timer_reset(&autodial_timer);
}
#endif

static void roh_timer_set(struct roh_t* roh, BOOL initial)
{
	int timeout_sec;
	if ( tel_sp_profile == SP_TELSTRA) {
		if (initial) {
			timeout_sec = TELSTRA_ROH_TIMEOUT1;
		} else {
			timeout_sec = TELSTRA_ROH_TIMEOUT2;
		}
		roh->initial = initial;
	} else {
		timeout_sec = CANADIAN_ROH_TIMEOUT;
	}
	SYSLOG_DEBUG("roh_timer_set to %d seconds", timeout_sec);
	timer_set(&roh->timer, timeout_sec);
}

static void roh_timer_reset(struct roh_t* roh, BOOL init_all)
{
	SYSLOG_DEBUG("roh_timer_REset");
	if (init_all) {
		roh->initial = FALSE;
	}
	timer_reset(&roh->timer);
}

static void congestion_tone_timer_set(struct timer_t* t, unsigned int timeout_sec)
{
	SYSLOG_DEBUG("congestion tone timer set to %d seconds", timeout_sec);
	timer_set(t, timeout_sec);
}

static void congestion_tone_timer_reset(struct timer_t* t)
{
	SYSLOG_DEBUG("congestion tone REset");
	timer_reset(t);
}

BOOL timer_expired(struct timer_t* t)
{
	struct timeb now;
	if (t->expiration.time == 0)
	{
		return FALSE;
	}
	ftime(&now);
	return now.time > t->expiration.time || (now.time == t->expiration.time && now.millitm >= t->expiration.millitm);
}

static void clear_digits(number_t *number)
{
	number->size = 0;
	number->begin = 0;
	memset(number->digits, 0, sizeof(number->digits));
	timer_reset(&number->timer);
}

#define INCALL_SERVICE_TIMEOUT 2 // the first two digits should be dialled reasonably fast, otherwise the incall services feel not responsive enough
static int add_digit(int idx, char d)
{
	int ret = 0;
	if (pots_bridge[idx].number.size < sizeof(pots_bridge[idx].number.digits) - 1)
	{
		pots_bridge[idx].number.digits[ pots_bridge[idx].number.size++ ] = d;
		ret = 1;
		timer_set( &pots_bridge[idx].number.timer, pots_bridge[idx].loop.recall_active && pots_bridge[idx].number.size <= 2 ? INCALL_SERVICE_TIMEOUT : pots_bridge[idx].dial_plan.timeout);
	}
	else
	{
		SYSLOG_DEBUG("skip adding digits : %d >= %d?", pots_bridge[idx].number.size, sizeof(pots_bridge[idx].number.digits) - 1);
	}
	if (pots_bridge[idx].send_outband_dmtf == TRUE)
	{
		/* if incall dtmf number exceed limit (256), clear buffer and notify to simple_at_manager to reset its key index */
		if (pots_bridge[idx].incall_dtmf_number.size >= sizeof(pots_bridge[idx].incall_dtmf_number.digits) -1)
		{
			clear_digits(&pots_bridge[idx].incall_dtmf_number);
			rdb_set_single(rdb_variable((const char *) slic_info[idx].rdb_name_dtmf_cmd, "", ""), "");
			SYSLOG_DEBUG("Exceed outband dtmf buffer limit. RESET outband dtmf buffer");
		}

		if (pots_bridge[idx].incall_dtmf_number.size < sizeof(pots_bridge[idx].incall_dtmf_number.digits) - 1 &&
			!pots_bridge[idx].loop.recall_active && allow_dtmf_key_send(idx))
		{
			pots_bridge[idx].incall_dtmf_number.digits[ pots_bridge[idx].incall_dtmf_number.size++ ] = d;
			timer_set(&pots_bridge[idx].incall_dtmf_number.timer,  1);
			pots_bridge[idx].waiting_next_dtmf_keys = TRUE;
		}
		if (strlen(pots_bridge[idx].incall_dtmf_number.digits))
			SYSLOG_DEBUG("dtmf '%s'", pots_bridge[idx].incall_dtmf_number.digits);
	}
	SYSLOG_DEBUG( "'%s' %s '%s'", pots_bridge[idx].number.digits, (pots_bridge[idx].send_outband_dmtf ? ",outband dtmf" : ""),
			(pots_bridge[idx].send_outband_dmtf ? pots_bridge[idx].incall_dtmf_number.digits : ""));
	return ret;
}

static void send_dtmf_tones(int idx)
{
	/* out band DTMF sending */
#ifdef HAS_VOIP_FEATURE
	if (pots_bridge[idx].send_outband_dmtf || is_match_slic_call_type(idx, VOICE_ON_IP))
#else	/* HAS_VOIP_FEATURE */
	if (pots_bridge[idx].send_outband_dmtf)
#endif	/* HAS_VOIP_FEATURE */
	{
		SYSLOG_ERR("set '%s' to '%s'", rdb_variable( (const char *) slic_info[idx].rdb_name_dtmf_cmd, "", ""),
				pots_bridge[idx].incall_dtmf_number.digits);
		//rdb_set_single(rdb_variable((const char *) slic_info[idx].rdb_name_dtmf_cmd, "", ""), "");
		rdb_set_single(rdb_variable((const char *) slic_info[idx].rdb_name_dtmf_cmd, "", ""),
				pots_bridge[idx].incall_dtmf_number.digits);
		timer_set(&pots_bridge[idx].incall_dtmf_number.timer, 1);
		return;
	}
	/* end of out band DTMF sending */
	clear_digits(&pots_bridge[idx].incall_dtmf_number);
	pots_bridge[idx].waiting_next_dtmf_keys = FALSE;
}

static void clear_dtmf_key_cmd_n_digits(int idx)
{
	rdb_set_single(rdb_variable((const char *) slic_info[idx].rdb_name_dtmf_cmd, "", ""), "");
	clear_digits(&pots_bridge[idx].incall_dtmf_number);
}

static int is_dial_plan_number(int idx)
{
	int r = regexec(&pots_bridge[idx].dial_plan.regex, pots_bridge[idx].number.digits, (unsigned int)0, NULL, 0) == 0 ? 1 : 0;
	return r;
}

static int is_dialing(int idx)
{
//	/SYSLOG_DEBUG("number.begin = %d, number.size = %d", pots_bridge[idx].number.begin, pots_bridge[idx].number.size);
	return pots_bridge[idx].number.begin == 0 && pots_bridge[idx].number.size > 0;
}

static int roaming_call_blocked(int idx, const char *var_name)
{
	char* value = alloca(RDB_VALUE_SIZE_SMALL);
	const char *default_roaming = "0";
#if defined(PLATFORM_PLATYPUS)
	char* db_str;
	int result;
#endif

	if (rdb_get_single(rdb_variable(RDB_ROAMING_STATUS, "", ""), value, RDB_VALUE_SIZE_SMALL) != 0 ||
#ifdef HAS_VOIP_FEATURE
		slic_info[idx].call_type != VOICE_ON_3G ||
#endif	/* HAS_VOIP_FEATURE */
		*value == 0 || strcmp(value, "active") != 0)
	{
		return 0;
	}

#if defined(PLATFORM_PLATYPUS)
	nvram_init(RT2860_NVRAM);
	db_str = nvram_get(RT2860_NVRAM, (char *) var_name);
	SYSLOG_DEBUG("read NV item : %s : %s", var_name, db_str);
	if (!db_str || !strlen(db_str))
	{
		SYSLOG_DEBUG("Voice roaming variable is not defined. Set to default %s", default_roaming);
		nvram_strfree(db_str);
		result = nvram_bufset(RT2860_NVRAM, (char *) var_name, (char *) default_roaming);
		if (result < 0)
		{
			SYSLOG_ERR("write NV item failure: %s : %s", var_name, default_roaming);
			return (strcmp(default_roaming, "0") == 0);
		}
		result = nvram_commit(RT2860_NVRAM);
		if (result < 0)
			SYSLOG_ERR("commit NV items failure");
		nvram_close(RT2860_NVRAM);
		return (strcmp(default_roaming, "0") == 0);
	}
	if (strcmp(db_str, "0") == 0)
		result = 1;
	else
		result = 0;
	nvram_strfree(db_str);
	nvram_close(RT2860_NVRAM);
	return result;
#else
	if( rdb_get_single( var_name, value, RDB_VALUE_SIZE_SMALL ) != 0)
	{
		SYSLOG_ERR( "failed to read '%s'", var_name );
		if (rdb_update_single(var_name, default_roaming, CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to create '%s' (%s)", var_name, strerror(errno));
		}
		return (strcmp(default_roaming, "0") == 0);
	}
	return (strcmp(value, "0") == 0);
#endif
}


static void mark_voice_roaming_call_blocked(BOOL blocked, const char *var_name)
{
	char* value = alloca(RDB_VALUE_SIZE_SMALL);
	if( rdb_get_single( var_name, value, RDB_VALUE_SIZE_SMALL ) != 0)
	{
		//SYSLOG_ERR( "failed to read '%s'", var_name );
		if (rdb_update_single(var_name, "0", CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to create '%s' (%s)", var_name, strerror(errno));
		}
		rdb_set_single( var_name, (blocked? "1":"0") );
	}
	else
	{
		// for no apparent reason, 2x VOICE_ROAMING_BLOCKED_DB_NAME rdb variables
		// (roaming.voice.incoming.blocked and roaming.voice.outgoing.blocked)
		// are persistent. Being updated too often is not a good idea because rdb_manager
		// (configuration) will write all persistent RDB vars to a file when any of them
		// is updated. So, write only if it is changing.
		// Same change in connection_mgr.c
		if (atoi(value) != (int)blocked)
		{
			rdb_set_single( var_name, (blocked? "1":"0") );
		}
	}
}


/* 3G dedicated function */
static int init_phone(void)
{
	int retry = 0;
	SYSLOG_INFO("configuring phone module...");
	//slic_play( slic_tone_busy );
	while (pots_bridge_running && retry++ < 10 &&
			send_rdb_command_blocking(RDB_PHONE_CMD_PREFIX, RDB_SCMD_PROFILE_INIT, 0) != 0)
	{
		sleep(1);
	}
	//slic_play( slic_tone_none );
	if (retry < 10)
	{
		SYSLOG_INFO("phone module configured");
		return 0;
	}
	else
	{
		SYSLOG_INFO("phone module could not configured");
		return 1;
	}
}

/* 3G dedicated function */
static int restore_phone(void)
{
	return send_rdb_command_blocking(RDB_PHONE_CMD_PREFIX, RDB_SCMD_PROFILE_SET, 0);
}

static void set_recall_state(int idx, BOOL enabled)
{
	pots_bridge[idx].loop.recall_active = enabled;
	slic_enable_pcm_dynamic(slic_info[idx].slic_fd, !enabled, idx);
}

typedef enum { incall_service_0 = '0', incall_service_1, incall_service_2, incall_service_3, incall_service_4, incall_service_5 } incall_service_enum;

static int chld(int idx, incall_service_enum service, unsigned int id)
{
	char command[32];

#ifdef HAS_VOIP_FEATURE
	if (slic_info[idx].call_type != VOICE_ON_3G)
	{
		SYSLOG_ERR("***************************************************************");
		SYSLOG_ERR("CHLD for %s call type should be implemented", ct_str(idx));
		SYSLOG_ERR("***************************************************************");
		return -1;
	}
#endif	/* HAS_VOIP_FEATURE */

	if (id > 0)
	{
		sprintf(command, "AT+CHLD=%c%u", service, id);
	}
	else
	{
		sprintf(command, "AT+CHLD=%c", service);
	}
	clear_digits(&pots_bridge[idx].number);
	set_recall_state(idx, FALSE);
	SYSLOG_DEBUG("------------------------------------------------------------------------------");
	SYSLOG_DEBUG("send '%s'", command);
	SYSLOG_DEBUG("------------------------------------------------------------------------------");
	if (send_rdb_command_blocking((const char *)slic_info[idx].rdb_name_at_cmd, command, get_cmd_retry_count(idx)) != 0)
	{
		return -1;
	}
	//handle_pots_event(idx, event_call_state_changed);
	/* store event_call_state_changed to pending event buffer */
	pots_bridge[idx].pending_event = event_call_state_changed;
	return 0;
}

const char* ss_to_name(int idx)   // TODO: quick and dirty, move to a proper place
{
	const char* begin;
	const char* end;
	static const int size = 128;
	char* value = alloca(size);
	for (begin = pots_bridge[idx].ss.command; *begin == '#' || *begin == '*'; ++begin);
	for (end = begin; *end != '#'; ++end);
	if (begin == end)
	{
		return NULL;
	}
	if (rdb_get_single(rdb_variable(slic_info[idx].rdb_name_cmd_result, "", ""), value, size) == 0 && *value != 0)
	{
		SYSLOG_DEBUG("'%s'='%s'", rdb_variable(slic_info[idx].rdb_name_cmd_result, "", ""), value);
		return value;
	}
	return NULL;
}

/* modification to support per call basis clip/clir.
* during dialing : is_dialing = TRUE, should waiting more digits after [*#][30|31]#..
* after dialing  : is_dialing = FALSE, should check real ss_request
*/
static int is_ss_request(int idx, BOOL is_dialing)
{
	int r;

	/* Canadian spec. Per call basis clir en/dis : #31#DN, *31#DN */
	if (tel_profile == CANADIAN_PROFILE && is_dialing)
	{
		r = pots_bridge[idx].number.size > 2 &&
			(pots_bridge[idx].number.digits[0] == '*' || pots_bridge[idx].number.digits[0] == '#') &&
			pots_bridge[idx].number.digits[ pots_bridge[idx].number.size - 1 ] == '#' &&
			!(pots_bridge[idx].number.digits[1] == '3' && pots_bridge[idx].number.digits[2] == '1');
		if (tel_profile == CANADIAN_PROFILE && tel_sp_profile == SP_ROGERS &&
			strcmp(pots_bridge[idx].number.digits, "*10#") == 0)
		{
			r = 0;
		}

	}
	/* Telstra spec. Per call basis clir en/dis : #31#DN, *31#DN
	*               Per call basis clip en/dis : #30#DN, *30#DN */
	else if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA && is_dialing)
	{
		r = pots_bridge[idx].number.size > 2 &&
			(pots_bridge[idx].number.digits[0] == '*' || pots_bridge[idx].number.digits[0] == '#') &&
			pots_bridge[idx].number.digits[ pots_bridge[idx].number.size - 1 ] == '#' &&
			!(pots_bridge[idx].number.digits[1] == '3' && pots_bridge[idx].number.digits[2] == '1') &&
			!(pots_bridge[idx].number.digits[1] == '3' && pots_bridge[idx].number.digits[2] == '0');
	}
	else if (!is_dialing) {
		r = pots_bridge[idx].number.size == 4 &&
			(pots_bridge[idx].number.digits[0] == '*' || pots_bridge[idx].number.digits[0] == '#') &&
			pots_bridge[idx].number.digits[ pots_bridge[idx].number.size - 1 ] == '#' &&
			pots_bridge[idx].number.digits[1] == '3' &&
			(pots_bridge[idx].number.digits[2] == '1' || pots_bridge[idx].number.digits[2] == '0');
	}
	else
	{
		r = pots_bridge[idx].number.size > 2 &&
			(pots_bridge[idx].number.digits[0] == '*' || pots_bridge[idx].number.digits[0] == '#') &&
			pots_bridge[idx].number.digits[ pots_bridge[idx].number.size - 1 ] == '#';
	}

	if (r)
	{
		timer_reset(&pots_bridge[idx].number.timer);
	}
	return r;
}

static BOOL ss_request_pending(int idx)
{
	return pots_bridge[idx].ss.request_pending;
}

static int clear_ss_request(int idx)
{
	pots_bridge[idx].ss.request_pending = FALSE;
	clear_digits(&pots_bridge[idx].number);
	return 0;
}

static int send_ss_request(int idx)
{
	SYSLOG_DEBUG("idx [%d] '%s'", idx, pots_bridge[idx].number.digits);
	strcpy(pots_bridge[idx].ss.command, pots_bridge[idx].number.digits);
	pots_bridge[idx].ss.request_pending = TRUE;

#ifdef HAS_VOIP_FEATURE
	if (slic_info[idx].call_type != VOICE_ON_3G)
	{
		SYSLOG_ERR("***************************************************************");
		SYSLOG_ERR("SEND_SS_REQUEST for %s call type should be implemented", ct_str(idx));
		SYSLOG_ERR("***************************************************************");
		return -1;
	}
#endif	/* HAS_VOIP_FEATURE */

	if (send_rdb_command(rdb_variable((const char *)slic_info[idx].rdb_name_at_cmd, "", ""),
		pots_bridge[idx].ss.command, get_cmd_retry_count(idx)) == 0)
	{
		return 0;
	}
	slic_play_fixed_pattern(slic_info[idx].slic_fd, slic_tone_pattern_failure, 1);
	clear_ss_request(idx);
	return -1;
}

static int ss_status_ok(int idx)
{
	return strcmp(pots_bridge[idx].ss.status, "1") == 0;
}

typedef enum { at_cmd_class_voice = 1, at_cmd_class_data = 2 } at_cmd_class_enum;

static int ss_enabled(const char* v, at_cmd_class_enum classx)
{
	const char* p;
	if (memcmp(v, "disabled", STRLEN("disabled")) == 0 || memcmp(v, "enabled", STRLEN("enabled")) != 0)
	{
		return 0;
	}
	if (strlen(v) == STRLEN("enabled"))
	{
		return 1;
	}
	for (p = v + STRLEN("enabled") + 1; *p; ++p)
	{
		unsigned char c = atoi(p);
		if (c & classx)
		{
			return 1;
		}
		for (; *p != 0 && *p != ','; ++p);
	}
	return 0;
}

static int handle_ss_response(int idx)
{
	slic_tone_pattern_t pattern = slic_tone_pattern_failure;

#ifdef HAS_VOIP_FEATURE
	if (slic_info[idx].call_type != VOICE_ON_3G)
	{
		SYSLOG_ERR("***************************************************************");
		SYSLOG_ERR("HANDLE_SS_RESPONSE for %s call type should be implemented", ct_str(idx));
		SYSLOG_ERR("***************************************************************");
		return -1;
	}
#endif	/* HAS_VOIP_FEATURE */

	if (!ss_request_pending(idx))
	{
		return 0;
	}
	if (ss_status_ok(idx))
	{
		if (memcmp(pots_bridge[idx].ss.command, "*#", 2) == 0)
		{
			static const int size = 128; // TODO: quick and dirty, define size properly
			char* n = alloca(size);
			char* value = alloca(size);
			(void) memcpy(n, ss_to_name(idx), size);
			if (n && rdb_get_single(rdb_variable("umts.services", "", (const char*)n), value, size) == 0 && *value != 0)
			{
				SYSLOG_DEBUG("'%s'='%s'", rdb_variable("umts.services", "", n), value);
				pattern = ss_enabled(value, at_cmd_class_voice) ? slic_tone_pattern_enabled : slic_tone_pattern_disabled;
			}
		}
		else
		{
			pattern =   pots_bridge[idx].ss.command[0] == '*' ? slic_tone_pattern_enabled
						: pots_bridge[idx].ss.command[0] == '#' ? slic_tone_pattern_disabled
						: slic_tone_pattern_failure;
		}
	}
	slic_play_fixed_pattern(slic_info[idx].slic_fd, pattern, 1);
	clear_ss_request(idx);
	return pattern == slic_tone_pattern_failure ? -1 : 0;
}

static int update_calls(int idx)
{
	enum { call_state_poll_timeout = 1 };
	int ret = call_control_update(idx, &pots_bridge[idx].call.control, FALSE);
	if (ret != 0)
	{
		SYSLOG_ERR("failed; the following call list may be stale");
	}
	calls_print(pots_bridge[idx].call.control.calls);

	if (ret != 0 || pots_bridge[idx].call.control.count_all > 0)
	{
		timer_set(&pots_bridge[idx].call.poll_timer, call_state_poll_timeout);
	}
	else
	{
		timer_reset(&pots_bridge[idx].call.poll_timer);
	}
	return ret;
}

static unsigned int call_count(int idx, cc_state_enum state)
{
	return pots_bridge[idx].call.control.count[ state ];
}

static BOOL calls_disconnected(int idx)
{
	int i;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (call_disconnected(&pots_bridge[idx].call.control.calls[i], i))
		{
			if (pots_bridge[idx].call.control.calls[i].last.state == call_waiting)
			{
				//SYSLOG_ERR("-------------------------------");
				//#SYSLOG_ERR(" STOP CALL WAITING TONE");
				//SYSLOG_ERR("-------------------------------");
				slic_play(slic_info[idx].slic_fd, slic_tone_none);
			}
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL calls_empty(int idx)
{
	int i;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (!call_empty(&pots_bridge[idx].call.control.calls[i]))
		{
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL allow_dtmf_key_send(int idx)
{
	return(call_count(idx, call_active) > 0 ||
		(call_count(idx, call_active) == 0 && pots_bridge[idx].call.control.count_all > 0));
}

static int pick_up(int idx, const struct call_t* incoming_call)
{
	if (!incoming_call)
	{
		return -1;
	}
	return send_rdb_command_blocking((const char *)slic_info[idx].rdb_name_at_cmd, RDB_SCMD_ATA, get_cmd_retry_count(idx));
}

/* Canadian spec. international prefix 01 or 011 --> '+' */
void convert_international_prefix(int idx)
{
	char tmp_digits[DIGITS_SIZE];

	(void) memset(tmp_digits, 0x00, DIGITS_SIZE);
	(void) memcpy(tmp_digits, pots_bridge[idx].number.digits, DIGITS_SIZE);
	if (pots_bridge[idx].number.digits[0] == '0' &&
		pots_bridge[idx].number.digits[1] == '1' &&
		pots_bridge[idx].number.digits[2] == '1')
	{
		(void) memset(pots_bridge[idx].number.digits, 0x00, DIGITS_SIZE);
		pots_bridge[idx].number.digits[0] = '+';
		(void) memcpy(&pots_bridge[idx].number.digits[1], &tmp_digits[3], pots_bridge[idx].number.size - 3);
		pots_bridge[idx].number.size -= 2;
	}
	else if (pots_bridge[idx].number.digits[0] == '0' &&
			pots_bridge[idx].number.digits[1] == '1')
	{
		(void) memset(pots_bridge[idx].number.digits, 0x00, DIGITS_SIZE);
		pots_bridge[idx].number.digits[0] = '+';
		(void) memcpy(&pots_bridge[idx].number.digits[1], &tmp_digits[2], pots_bridge[idx].number.size - 2);
		pots_bridge[idx].number.size -= 1;
	}
}

/* out band DTMF sending */
#define ALWAYS_OUT_OF_BAND_DTMF_MODE
static void check_outband_dtmf_dial(int idx)
{
	char tmp_digits[DIGITS_SIZE];

	/* read mbdn & msisdn until both value are valid */
	read_mbdn_msisdn_from_rdb();

	(void) memset(tmp_digits, 0x00, DIGITS_SIZE);
	(void) memcpy(tmp_digits, pots_bridge[idx].number.digits, DIGITS_SIZE);
	if (pots_bridge[idx].number.size > 2 &&
		pots_bridge[idx].number.digits[0] == '#' &&
		pots_bridge[idx].number.digits[1] == '#')
	{
		SYSLOG_INFO("detect special dial string with leading ## for OUTBAND DTMF TX");
		(void) memset(pots_bridge[idx].number.digits, 0x00, DIGITS_SIZE);
		(void) memcpy(&pots_bridge[idx].number.digits[0], &tmp_digits[2], pots_bridge[idx].number.size - 2);
		pots_bridge[idx].number.size -= 2;
		pots_bridge[idx].send_outband_dmtf = TRUE;
		return;
	}
	else if (pots_bridge[idx].number.size > 2 &&
		pots_bridge[idx].number.digits[0] == '*' &&
		pots_bridge[idx].number.digits[1] == '*')
	{
		SYSLOG_INFO("detect special dial string with leading ** for INBAND DTMF TX");
		(void) memset(pots_bridge[idx].number.digits, 0x00, DIGITS_SIZE);
		(void) memcpy(&pots_bridge[idx].number.digits[0], &tmp_digits[2], pots_bridge[idx].number.size - 2);
		pots_bridge[idx].number.size -= 2;
		pots_bridge[idx].send_outband_dmtf = FALSE;
		return;
	}

#ifndef ALWAYS_OUT_OF_BAND_DTMF_MODE
#ifdef USE_PREDEF_VMBOX_NO
	if (valid_mbdn && strstr((const char *)&pots_bridge[idx].number.digits[0], (const char *)&sim_mbdn[0]))
#else
	if (tel_profile == CANADIAN_PROFILE && valid_mbdn && strstr((const char *)&pots_bridge[idx].number.digits[0], (const char *)&sim_mbdn[0]))
#endif
	{
		SYSLOG_INFO("found voice mail box number in the dial string for OUTBAND DTMF TX");
		pots_bridge[idx].send_outband_dmtf = TRUE;
	}
#ifdef USE_PREDEF_VMBOX_NO
	else if (valid_mbn && strstr((const char *)&pots_bridge[idx].number.digits[0], (const char *)&sim_mbn[0]))
#else
	else if (tel_profile == CANADIAN_PROFILE && valid_mbn && strstr((const char *)&pots_bridge[idx].number.digits[0], (const char *)&sim_mbn[0]))
#endif
	{
		SYSLOG_INFO("found voice mail box number in the dial string for OUTBAND DTMF TX");
		pots_bridge[idx].send_outband_dmtf = TRUE;
	}
	else if (valid_msisdn && strstr((const char *)&pots_bridge[idx].number.digits[0], (const char *)&sim_msisdn[0]))
	{
		SYSLOG_INFO("found self mob. number in the dial string for OUTBAND DTMF TX");
		pots_bridge[idx].send_outband_dmtf = TRUE;
	}
#endif

	/* voice mail box access number setting : *98 */
#ifdef USE_PREDEF_VMBOX_NO
	if (pots_bridge[idx].number.size == 3 && strcmp(pots_bridge[idx].number.digits, "*98") == 0)
#else
	if ((tel_profile == CANADIAN_PROFILE || (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_NZ_TELECOM))&&
		pots_bridge[idx].number.size == 3 &&
		strcmp(pots_bridge[idx].number.digits, "*98") == 0)
#endif
	{
		SYSLOG_INFO("Found Voice Mail Box dial string '*98' for OUTBAND DTMF TX");
		pots_bridge[idx].send_outband_dmtf = TRUE;
#ifndef USE_PREDEF_VMBOX_NO
		if (tel_profile == CANADIAN_PROFILE) {
#endif
			if (valid_mbdn) {
				strcpy(pots_bridge[idx].number.digits, sim_mbdn_orig);
				pots_bridge[idx].number.size = strlen(pots_bridge[idx].number.digits);
			} else if (valid_mbn) {
				strcpy(pots_bridge[idx].number.digits, sim_mbn_orig);
				pots_bridge[idx].number.size = strlen(pots_bridge[idx].number.digits);
			}
#ifndef USE_PREDEF_VMBOX_NO
		}
		else
#endif
		if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_NZ_TELECOM) {
			if (valid_adn) {
				strcpy(pots_bridge[idx].number.digits, sim_adn_orig);
			} else {
				SYSLOG_INFO("using default voicemail center number");
				strcpy(pots_bridge[idx].number.digits, NZ_TELECOM_VMC);
			}
			pots_bridge[idx].number.size = strlen(pots_bridge[idx].number.digits);
		}
		SYSLOG_INFO("Convert dial string '*98' to '%s'", pots_bridge[idx].number.digits);
	}

	//SYSLOG_INFO("strlen(sim_mbdn) %d, strlen(sim_msisdn) %d", strlen(sim_mbdn), strlen(sim_msisdn));

#ifdef ALWAYS_OUT_OF_BAND_DTMF_MODE
	/* overide outband dtmf setting */
	pots_bridge[idx].send_outband_dmtf = TRUE;
#endif
}
/* end of out band DTMF sending */

/* Rogers : 3-way calling initiated by "*71"+number */
static BOOL check_Canadian_3way_calling(int idx)
{
	char temp_buf[DIGITS_SIZE];
	/* return TRUE for TELUS always */
	if (tel_sp_profile == SP_TELUS) {
		return TRUE;
	}
	SYSLOG_DEBUG("checking Roger's 3-way dialing starting with *71");
	if(strncmp(pots_bridge[idx].number.digits, "*71", 3) == 0)
	{
		SYSLOG_DEBUG("found Roger's 3-way dialing starting, extract number");
		(void) strcpy(temp_buf, &pots_bridge[idx].number.digits[3]);
		(void) memset(pots_bridge[idx].number.digits, 0x00, DIGITS_SIZE);
		(void) strcpy(pots_bridge[idx].number.digits, temp_buf);
		pots_bridge[idx].number.size = strlen(pots_bridge[idx].number.digits);
		return TRUE;
	}
	SYSLOG_DEBUG("found normal dial string, do nothing");
	return FALSE;;
}

/* Rogers : Call Return feature */
static BOOL check_call_return_number(int idx)
{
	SYSLOG_DEBUG("checking Roger's Call Return dial with *10#");
	if(strcmp(pots_bridge[idx].number.digits, "*10#") == 0)
	{
		SYSLOG_DEBUG("found Roger's Call Return dialing string");
		if (!strlen(last_incoming_call_num))
		{
			SYSLOG_DEBUG("last incoming call number is empty!");
			return FALSE;
		}
		SYSLOG_DEBUG("dial last incoming call number : %s", last_incoming_call_num);
		(void) strcpy(pots_bridge[idx].number.digits, last_incoming_call_num);
		pots_bridge[idx].number.size = strlen(last_incoming_call_num);
		return TRUE;
	}
	SYSLOG_DEBUG("found normal dial string, do nothing");
	return TRUE;;
}

static void set_dtmf_key_block_state(int idx, BOOL block)
{
	if ((block && (call_count(idx, call_active) + call_count(idx, call_held)) <= 0) ||
		(!block && (call_count(idx, call_active) + call_count(idx, call_held)) > 0))
	{
		pots_bridge[idx].dtmf_key_blocked = block;
		SYSLOG_DEBUG("**** set pots_bridge[%d].dtmf_key_blocked = %d", idx, block);
	}
}

#if defined(PLATFORM_PLATYPUS) || defined(PLATFORM_PLATYPUS2)
#define PCM_CPU_BUFFER_DELAY	30		/* 30 ms */
#define MAX_PCM_DEV_NAME_LEN	10
int pcm_fd;
static void set_pcm_cpu_buffer( idx)
{
	char pcm_dev[MAX_PCM_DEV_NAME_LEN];
	//sprintf(pcm_dev, "/dev/PCM%d", slic_info[idx].cid);
	/* normal audio bufferring path is fixed to PCM0 */
	sprintf(pcm_dev, "/dev/PCM0");
	int test_options;
	pcm_fd = open(pcm_dev, O_RDWR);
	ioctl(pcm_fd, PCM_SET_SLAVE_MODE, 1);
	test_options = (slic_info[idx].cid << 8) | (PCM_CPU_BUFFER_DELAY / 10);
	ioctl(pcm_fd, PCM_SET_CPU_BUFFER, test_options);
	//close(fd);
}
#endif

/* standard per call basis operation is activated by leading '*' and
 * deactivated by leading '#' but Telstra requests reverse way as below;
 * 		- *31#number : show CND
 * 		- #31#number : hide CND
 * Modify user's input here not to disturb common per call basis codes
 * implemented in simple_at_manager and used for other profile.
 */
static void process_telstra_per_call_basis_ss_request(int idx)
{
	if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA &&
		pots_bridge[idx].number.size > 4) {
		if (strncmp(pots_bridge[idx].number.digits, "*31#", 4) == 0) {
			pots_bridge[idx].number.digits[0] = '#';
			SYSLOG_DEBUG("new dial str '%s'", pots_bridge[idx].number.digits);
		} else if (strncmp(pots_bridge[idx].number.digits, "#31#", 4) == 0) {
			pots_bridge[idx].number.digits[0] = '*';
			SYSLOG_DEBUG("new dial str '%s'", pots_bridge[idx].number.digits);
		}
	}
}

static int is_emergency_number(int idx)
{
	return (!strncmp(pots_bridge[idx].number.digits, "000", 3)|
			!strncmp(pots_bridge[idx].number.digits, "112", 3)|
			!strncmp(pots_bridge[idx].number.digits, "111", 3)|
			!strncmp(pots_bridge[idx].number.digits, "555", 3)|
			!strncmp(pots_bridge[idx].number.digits, "911", 3)|
			!strncmp(pots_bridge[idx].number.digits, "311", 3)|
			!strncmp(pots_bridge[idx].number.digits, "999", 3)|
			!strncmp(pots_bridge[idx].number.digits, "994", 3)|
			!strncmp(pots_bridge[idx].number.digits, "991", 3));
}

void play_sim_warning_tone(int idx, sim_status_enum_type sim_status);
static void dial(int idx)
{
	int result;
	char* atd = alloca(DIGITS_SIZE + 5);

	/* if outgoing call is blocked by roaming feature, do not dial unless emergency number */
	if (roaming_call_blocked(idx, OUTGOING_VOICE_ROAMING_DB_NAME) &&
		!is_emergency_number(idx))
	{
		SYSLOG_ERR("Outgoing call is blocked in roaming area, clear digits");
		clear_digits(&pots_bridge[idx].number);
		clear_dtmf_key_cmd_n_digits(idx);
		if (tel_profile == DEFAULT_PROFILE) {
			play_sim_warning_tone(idx, LIMITED_SVC);
		} else {
			play_busy_tone(idx);
		}
		return;
	}

	/* Rogers Call Return feature */
	if (tel_profile == CANADIAN_PROFILE && tel_sp_profile == SP_ROGERS)
	{
		if (!check_call_return_number(idx))
		{
			play_busy_tone(idx);
			return;
		}
	}

	/* out band DTMF sending */
	check_outband_dtmf_dial(idx);

#if defined(PLATFORM_PLATYPUS) || defined(PLATFORM_PLATYPUS2)
#ifdef ALWAYS_OUT_OF_BAND_DTMF_MODE
	pots_bridge[idx].send_outband_dmtf = TRUE;
#else
	else if (tel_profile == DEFAULT_PROFILE)
	{
		pots_bridge[idx].send_outband_dmtf = TRUE;
	}
	else
	{
		pots_bridge[idx].send_outband_dmtf = FALSE;
	}
#endif

	/* if there is a need to send whole DTMF without cutting, use 'SEND_ALL' type */
	if (pots_bridge[idx].send_outband_dmtf)
	{
		(void) slic_set_dtmf_tx_mode( slic_info[idx].slic_fd, OUTBAND_CUT );
		set_pcm_cpu_buffer(idx);
		slic_change_cpld_mode(CPLD_MODE_BUFFER);
	}
	else
	{
		(void) slic_set_dtmf_tx_mode( slic_info[idx].slic_fd, INBAND_CUT );
		if (is_external_crystal_exist)
			slic_change_cpld_mode(CPLD_MODE_VOICE);
	}
#else
	// TO DO : can not use outband dtmf until new PCM driver is implemented for Bovine platform.
	pots_bridge[idx].send_outband_dmtf = FALSE;
	(void) slic_set_dtmf_tx_mode( slic_info[idx].slic_fd, INBAND_CUT );
	slic_change_cpld_mode(CPLD_MODE_VOICE);
#endif

	/* Canadian spec. international prefix 01 or 011 --> '+' */
	if (tel_profile == CANADIAN_PROFILE)
	{
		convert_international_prefix(idx);
	}

	SYSLOG_DEBUG("dialing '%s'", pots_bridge[idx].number.digits);
	process_telstra_per_call_basis_ss_request(idx);
	sprintf(atd, "ATD%s;", pots_bridge[idx].number.digits);
	set_recall_state(idx, FALSE);
	clear_digits(&pots_bridge[idx].number);
	clear_dtmf_key_cmd_n_digits(idx);

	if ((result = send_rdb_command_blocking((const char *)slic_info[idx].rdb_name_at_cmd, atd, get_cmd_retry_count(idx))) == 0)
	{
		/* insert below 2 lines to treat as a real call from dialing stage */
		//handle_pots_event(idx, event_call_state_changed);
		//SYSLOG_DEBUG("idx [%d] event_call_state_changed", idx);
		/* store event_call_state_changed to pending event buffer */
		pots_bridge[idx].pending_event = event_call_state_changed;

#ifdef HAS_VOIP_FEATURE
		/* change PCM clk to processor for VOIP call */
		if (slic_info[idx].call_type == VOICE_ON_IP)
		{
			(void) slic_change_pcm_clk_source(slic_info[0].slic_fd, 0);
		}
#endif	/* HAS_VOIP_FEATURE */

		/* Call status is not changed from 'alerting' to 'active' when call to IVR server in Canada.
		* So changed to not use DTMF key block feature
		*/
		//set_dtmf_key_block_state(idx, TRUE);

		return;
	}
	SYSLOG_ERR("dialing '%s' failed", pots_bridge[idx].number.digits);
	if (tel_profile == DEFAULT_PROFILE && roaming_call_blocked(idx, OUTGOING_VOICE_ROAMING_DB_NAME)) {
		play_sim_warning_tone(idx, LIMITED_SVC);
	} else {
		play_busy_tone(idx);
	}
}

//#define USE_CLIP_INFO_FOR_CALLER_ID_FSK
#define RDB_UMTS_CLIP_NUMBER	"umts.services.clip.number"
static void ring(int idx, const struct call_t* call)
{
	fsk_packet packet;
	char dn[11];
#ifdef USE_CLIP_INFO_FOR_CALLER_ID_FSK
	char clip_num[DIGITS_SIZE];
	BOOL clip_enabled = FALSE;
#endif

	(void) memset((char *)&dn[0], 0x00, 11);
#ifdef USE_CLIP_INFO_FOR_CALLER_ID_FSK
	(void) memset((char *)&clip_num[0], 0x00, DIGITS_SIZE);
	if( rdb_get_single( rdb_variable( RDB_UMTS_CLIP_NUMBER, "", "" ), clip_num, DIGITS_SIZE ) != 0)
	{
		SYSLOG_ERR( "failed to read CLIP" );
	}
#endif

	// test code for caller id fsk
	//(void) memset((char *)&call->number[0], 0x00, CALLS_NUMBER_SIZE);
	//(void) memcpy(&call->number[0], "1234567890", 10);
	//(void) memcpy((char *)&call->number[0], "1234567890123456789", 19);
	//(void) memcpy(&call->number[0], "12345", 5);

#ifdef USE_CLIP_INFO_FOR_CALLER_ID_FSK
	if (strlen(clip_num) > 0) {
		clip_enabled = TRUE;
	}
	if (clip_enabled)
#endif
	{
		/* Canadian spec. max DN field length is 10 */
		if (tel_profile == CANADIAN_PROFILE)
		{
#ifdef USE_CLIP_INFO_FOR_CALLER_ID_FSK
			fsk_packet_clip(&packet, clip_num);
#else
			fsk_packet_clip(&packet, call->number);
#endif
			#if (0)
			if (strlen(call->number) > 10)
			{
				(void) memcpy((char *)&dn[0], (char *)&call->number[0], 10);
				fsk_packet_clip(&packet, (char *)&dn[0]);
			}
			else
			{
				fsk_packet_clip(&packet, call->number);
			}
			#endif
		}
		else
		{
#ifdef USE_CLIP_INFO_FOR_CALLER_ID_FSK
			fsk_packet_clip(&packet, clip_num);
#else
			fsk_packet_clip(&packet, call->number);
#endif
		}
	}
	/* append slic_led_control */
	vmwi_led_control(idx, led_flash_on);
#ifdef USE_CLIP_INFO_FOR_CALLER_ID_FSK
	if (clip_enabled)
#endif
	{
#ifdef USE_CLIP_INFO_FOR_CALLER_ID_FSK
		SYSLOG_DEBUG("send FSK on ch [%d]: %s", idx, clip_num);
#else
		SYSLOG_DEBUG("send FSK on ch [%d]: %s", idx, call->number);
#endif
		slic_fsk_send(slic_info[idx].slic_fd, &packet);
		// send fsk first then wait for fsk complete event, then change to distinctive mode
		pots_bridge[idx].wait_for_fsk_complete = TRUE;
		//slic_play_distinctive_ring( slic_info[idx].slic_fd, slic_distinctive_ring_0 );
	}

	/* Rogers Call Return feature */
	if (strlen(call->number))
	{
		strcpy(last_incoming_call_num, call->number);
	}

	/* Call status is not changed from 'alerting' to 'active' when call to IVR server in Canada.
	* So changed to not use DTMF key block feature
	*/
	//set_dtmf_key_block_state(idx, TRUE);

#ifdef USE_CLIP_INFO_FOR_CALLER_ID_FSK
	if (!clip_enabled) {
		SYSLOG_DEBUG("no CLIP, skip FSK sending on ch [%d]", idx);
		pots_bridge[idx].wait_for_fsk_complete = FALSE;
		/* support international telephony profile */
		if (tel_profile ==  CANADIAN_PROFILE || tel_profile ==  NORTH_AMERICA_PROFILE)
		{
			slic_play_distinctive_ring(slic_info[idx].slic_fd, slic_distinctive_ring_canada);
		}
		else
		{
			slic_play_distinctive_ring(slic_info[idx].slic_fd, slic_distinctive_ring_0);
		}
		/* end of support international telephony profile */
	}
#endif
}

static int hang_up_all(int idx)
{
	int i;
	/* do not send AT+CHUP if other port is active for same service */
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (i == idx) continue;
		if (slic_info[i].call_type == slic_info[idx].call_type && slic_info[i].call_active)
		{
			SYSLOG_DEBUG("HANG-UP ignored, idx %d is active for %s type", i, call_type_name[slic_info[i].call_type]);
			return 0;
		}
	}

#ifdef HAS_VOIP_FEATURE
	/* change PCM clk to SLIC after call disconnect for VOIP call */
	if (slic_info[idx].call_type == VOICE_ON_IP)
	{
		(void) slic_change_pcm_clk_source(slic_info[0].slic_fd, 1);
	}
#endif	/* HAS_VOIP_FEATURE */

	return send_rdb_command_blocking((const char *)slic_info[idx].rdb_name_at_cmd, RDB_SCMD_ATCHUP, get_cmd_retry_count(idx));
}

static BOOL tone_busy_required(int idx)
{
	int i;
	BOOL disconnected = FALSE;
	const struct call_t* calls = pots_bridge[idx].call.control.calls;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (calls[i].current.empty)
		{
			if (!calls[i].last.empty && calls[i].last.state != call_waiting)
			{
				disconnected = TRUE;
			}
		}
		else
		{
			if (calls[i].current.state != call_held && calls[i].current.state != call_waiting)
			{
				return FALSE;
			}
		}
	}
	return disconnected;
}

static BOOL tone_none_required(int idx)
{
	int i;
	const struct call_t* calls = pots_bridge[idx].call.control.calls;
	for (i = 0; i < CALLS_SIZE; ++i)
	{
		if (calls[i].current.empty)
		{
			continue;
		}
		switch (calls[i].current.state)
		{
			case call_active:
			case call_dialing:
			case call_alerting:
				return calls[i].last.empty || calls[i].last.state != calls[i].current.state;
			case call_held:
			case call_incoming:
			case call_waiting:
			default:
				continue;
		}
	}
	return FALSE;
}

static void play_tone(idx)
{
	if (tone_busy_required(idx))
	{
		play_busy_tone(idx);
		return;
	}
	if (tone_none_required(idx))
	{
		slic_play(slic_info[idx].slic_fd, slic_tone_none);
		return;
	}
	//if( tone_dial_required() ) { slic_play( slic_tone_dial ); return; }
}


/* Bell Canada VMWI feature */
static void vmwi_led_control(int idx, pots_led_control_type mode)
{
	if (pots_bridge[idx].led_state != mode)
		SYSLOG_DEBUG("VMWI : vmwi_led_control [%d] = old %d, new %d", idx, pots_bridge[idx].led_state, mode);
	switch (mode)
	{
		/* led on/flash on has higher priority than vmwi led flashing */
		case led_on:
			slic_handle_pots_led(slic_info[idx].cid, led_flash_off);
		case led_flash_on:
			break;
		case led_vmwi_on:
			if (pots_bridge[idx].led_state == led_on ||
				pots_bridge[idx].led_state == led_flash_on)
			{
				SYSLOG_DEBUG("VMWI : Can't not turn on VMWI LED indication in off-hook or ringing state");
				return;
			}
			break;
		case led_off:
			slic_handle_pots_led(slic_info[idx].cid, led_flash_off);
			if (pots_bridge[idx].vmwi.cmd == ACTIVE)
			{
				SYSLOG_DEBUG("VMWI : Turn on delayed VMWI LED indication");
				mode = led_vmwi_on;
			}
			break;
		default:
			break;
	}
	slic_handle_pots_led(slic_info[idx].cid, mode);
	pots_bridge[idx].led_state = mode;
}


/* Rogers/Telus stutter tone feature */
static void stutter_tone_timer_set( struct timer_t* t, unsigned int timeout_sec )
{
	SYSLOG_DEBUG( "stutter_tone_timer_set");
	timer_set( t, timeout_sec );
}

static void stutter_tone_timer_reset( struct timer_t* t )
{
	SYSLOG_DEBUG( "stutter_tone_timer_REset");
	timer_reset( t );
}
/* end of Rogers/Telus stutter tone feature */

static void play_dial_tone(int idx)
{
	if (pots_bridge[idx].vmwi.cmd == ACTIVE)
	{
#ifdef HAS_STUTTER_DIAL_TONE
		if (1)
#else
		if (tel_profile == CANADIAN_PROFILE)
#endif
		{
			/* Rogers/Telus stutter tone feature */
			SYSLOG_DEBUG( "playing STUTTER dial tone");
			slic_play(slic_info[idx].slic_fd, slic_tone_stutter);
			/* Telstra requirement : continuous stutter tone */
			if (tel_sp_profile != SP_TELSTRA) {
				stutter_tone_timer_set(&pots_bridge[idx].stutter_tone_timer, CANADIAN_STUTTER_TONE_TIMEOUT);
			}
			/* end of Rogers/Telus stutter tone feature */
		}
		else
		{
			SYSLOG_DEBUG( "playing AMWI tone");
			slic_play(slic_info[idx].slic_fd, slic_tone_amwi);
		}
	}
	else
	{
		slic_play(slic_info[idx].slic_fd, slic_tone_dial);
	}
}

static void play_busy_tone(int idx)
{
	slic_play(slic_info[idx].slic_fd, slic_tone_busy);
	if (tel_sp_profile == SP_TELSTRA)
	{
		if(calls_disconnected(idx))
		{
			congestion_tone_timer_set(&pots_bridge[idx].congestion_tone_timer, TELSTRA_CPE_TIMEOUT);
		}
		else
		{
			congestion_tone_timer_set(&pots_bridge[idx].congestion_tone_timer, TELSTRA_ROH_TIMEOUT2);
		}
		roh_timer_reset(&pots_bridge[idx].roh, TRUE);
	}
}

void play_sim_warning_tone(int idx, sim_status_enum_type sim_status)
{
	slic_tone_enum sim_warning_tones[6] = {0, slic_tone_no_sim, slic_tone_sim_locked, slic_tone_sim_puk,
										   slic_tone_mep_locked, slic_tone_limited_svc};
	if (sim_status == SIM_OK && sim_status != MEP_LOCKED && sim_status != LIMITED_SVC) {
		return;
	}
	SYSLOG_DEBUG( "playing SIM warning tone : %s", sim_status_name[sim_status]);
	slic_play(slic_info[idx].slic_fd, sim_warning_tones[sim_status]);
}

static void display_vmwi_state( int idx, int new )
{
	static vmwi_cmd_enum_type old_cmd[MAX_CHANNEL_NO];
	static vmwi_status_enum_type old_status[MAX_CHANNEL_NO];
	if (!new)
	{
		old_cmd[idx] = pots_bridge[idx].vmwi.cmd;
		old_status[idx] = pots_bridge[idx].vmwi.status;
	}
	else if (old_cmd[idx] != pots_bridge[idx].vmwi.cmd || old_status[idx] != pots_bridge[idx].vmwi.status)
		SYSLOG_DEBUG( "vmwi[%d]:cmd %s -> %s, status %s -> %s", idx,
				vmwi_cmd_name[old_cmd[idx]], vmwi_cmd_name[pots_bridge[idx].vmwi.cmd],
				vmwi_status_name[old_status[idx]], vmwi_status_name[pots_bridge[idx].vmwi.status] );
}

static void vmwi_timer_set( struct timer_t* t, unsigned int timeout_sec )
{
	SYSLOG_DEBUG( "vmwi_timer_set");
	timer_set( t, timeout_sec );
}

static void vmwi_timer_reset( struct timer_t* t )
{
	SYSLOG_DEBUG( "vmwi_timer_REset");
	timer_reset( t );
}

static int vmwi_noti_delivery_available( int idx )
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		if (i == idx) continue;
		if (pots_bridge[i].vmwi.status == NOTI_DELIVERING)
			return 0;
	}
	return 1;
}

static void set_vmwi_noti_delivering_state( int idx )
{
	fsk_packet packet;

	if (!vmwi_noti_delivery_available( idx ))
	{
		SYSLOG_DEBUG( "wait other port finishing VMWI transmitting..." );
		set_vmwi_waiting_state( idx, 1 );
		return;
	}

	SYSLOG_DEBUG( "send VMWI without delay" );
	pots_bridge[idx].vmwi.status = NOTI_DELIVERING;
	if (pots_bridge[idx].vmwi.cmd == ACTIVE)
	{
		vmwi_packet_clip( &packet, 1);
	}
	else if (pots_bridge[idx].vmwi.cmd == INACTIVE)
	{
		vmwi_packet_clip( &packet, 0);
	}
	else
	{
		SYSLOG_DEBUG( "Can not send VMWI" );
		return;
	}
	slic_send_vmwi_fsk( slic_info[idx].slic_fd, &packet );
}

static void set_vmwi_waiting_state( int idx, int timeout )
{
	SYSLOG_DEBUG( "set %ds time to send VMWI later", timeout );
	pots_bridge[idx].vmwi.status = WAITING_TIMEOUT;
	vmwi_timer_set( &pots_bridge[idx].vmwi.timer, timeout );
}

static void set_vmwi_offhook_pending_state( int idx )
{
	vmwi_timer_reset( &pots_bridge[idx].vmwi.timer );
	pots_bridge[idx].vmwi.status = OFFHOOK_PENDING;
}

static void vmwi_state_machine( int idx, pots_bridge_event_t event )
{
	SYSLOG_DEBUG( "VMWI : idx [%d] on event '%s'", idx, event_names[event] );
	display_vmwi_state( idx, 0 );
	if ( event == event_on_hook ||
	( event == event_call_state_changed && pots_bridge[idx].loop.state == slic_on_hook ) )
	{
		/* If off-hook to on-hook transition, set 3 seconds time to send VMWI later
		* or send VMWI immediately */
		if ( pots_bridge[idx].vmwi.cmd != EMPTY )
		{
			if ( pots_bridge[idx].vmwi.status == IDLE )
				set_vmwi_noti_delivering_state( idx );
			else if ( pots_bridge[idx].vmwi.status == OFFHOOK_PENDING )
				set_vmwi_waiting_state( idx, CANADIAN_VMWI_TIMEOUT );
			//else
			//	SYSLOG_DEBUG( "nothing to do in this status : '%s'", vmwi_status_name[pots_bridge[idx].vmwi.status] );
		}
	}
	else if ( pots_bridge[idx].loop.state == slic_off_hook )
	{
		/* If got vmwi event in off-hook state, wait until on-hook state.
		* If off-hook during vmwi timer, cancel timer & wait until on-hook state. */
		if ( pots_bridge[idx].vmwi.cmd != EMPTY )
		{
			if ( pots_bridge[idx].vmwi.status == IDLE )
			{
				SYSLOG_DEBUG( "Got VMWI but wait until on-hook" );
				set_vmwi_offhook_pending_state( idx );
			}
			else if ( pots_bridge[idx].vmwi.status == WAITING_TIMEOUT )
			{
				SYSLOG_DEBUG( "Cancel VMWI timer and wait until on-hook" );
				set_vmwi_offhook_pending_state( idx );
			}
			//else
			//	SYSLOG_DEBUG( "nothing to do in this status : '%s'", vmwi_status_name[pots_bridge[idx].vmwi.status] );
		}
	}
	else if ( event == event_vmwi_timer_expired && pots_bridge[idx].loop.state == slic_on_hook )
	{
		set_vmwi_noti_delivering_state( idx );
	}
	//else
	//	SYSLOG_DEBUG( "nothing to do for this event %d", event );
	display_vmwi_state( idx, 1 );
}
/* end of Bell Canada VMWI feature */

static void call_end_process(int idx, pots_bridge_event_t event, BOOL pcm_close)
{
	/* common variable initialization */
	slic_play(slic_info[idx].slic_fd, slic_tone_none);
	set_slic_call_active_state(idx, FALSE);
	pots_bridge[idx].waiting_for_recall_dialing = FALSE;
	pots_bridge[idx].detect_3way_calling_prefix = FALSE;
	pots_bridge[idx].recall_key_count = 0;
	pots_bridge[idx].dtmf_key_blocked = FALSE;
	clear_digits(&pots_bridge[idx].number);
	clear_dtmf_key_cmd_n_digits(idx);
	pots_bridge[idx].waiting_next_dtmf_keys = FALSE;
#if defined(PLATFORM_PLATYPUS) || defined(PLATFORM_PLATYPUS2)
	if (pcm_close)
	{
		if (pots_bridge[idx].send_outband_dmtf)
		{
			close(pcm_fd);
		}
		if (is_external_crystal_exist)
			slic_change_cpld_mode(CPLD_MODE_LOOP);
		else
			slic_change_cpld_mode(CPLD_MODE_VOICE);
	}
#endif
	slic_set_dtmf_tx_mode( slic_info[idx].slic_fd, SEND_ALL );

/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
	autodial_timer_reset();
#endif

	rdb_set_single( rdb_variable( RDB_UMTS_CLIP_NUMBER, "", "" ), "" );

	/* separate  variable initialization */
	switch (event)
	{
		case event_shutdown:
		case event_on_hook:
			/* append slic_led_control */
			vmwi_led_control(idx, led_off);
			roh_timer_reset(&pots_bridge[idx].roh, TRUE);
			/* Bell Canada VMWI feature */
			vmwi_state_machine(idx, event);
			/* Rogers/Telus stutter tone feature */
			stutter_tone_timer_reset(&pots_bridge[idx].stutter_tone_timer);
			congestion_tone_timer_reset(&pots_bridge[idx].congestion_tone_timer);
			clear_ss_request(idx);
			hang_up_all(idx);
			if (is_match_slic_call_type(idx, VOICE_ON_3G))
			{
				restore_phone();
			}
			break;

		case event_call_state_changed:
			/* append slic_led_control */
			vmwi_led_control(idx, pots_bridge[idx].loop.state == slic_off_hook ? led_on : led_off);
			if ( pots_bridge[idx].loop.state == slic_off_hook )
				roh_timer_set(&pots_bridge[idx].roh, TRUE);
			break;

		default:
			break;
	}
}

#define BLOCK_CALL_CONTROL_UPDATE_DURING_DTMF_KEY_PROCESSING
#ifdef BLOCK_CALL_CONTROL_UPDATE_DURING_DTMF_KEY_PROCESSING
BOOL need_to_block_call_control_update(void)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (pots_bridge[i].waiting_next_dtmf_keys)
			return TRUE;
	}
	return FALSE;
}
#endif

static void handle_pots_event_on_hook(int idx, pots_bridge_event_t event)
{
	int i;
	/* skip call_end_process() if other port is active for same service */
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (i == idx) continue;
		if (slic_info[i].call_type == slic_info[idx].call_type && slic_info[i].call_active)
		{
			SYSLOG_DEBUG("skip call_end_process(%d), idx %d is active for %s type", i, i, call_type_name[slic_info[i].call_type]);
			return;
		}
	}
	call_end_process(idx, event, TRUE);
}

/* check No Network Service or Invalid Service
 * note : cnsmgr and simple_at_manager uses different rdb variables and values */
static BOOL is_no_coverage_area(void)
{
	char s[RDB_VALUE_SIZE_SMALL];

	if( rdb_get_single( RDB_IF_NAME, s, RDB_VALUE_SIZE_SMALL ) != 0 ) {
		SYSLOG_ERR("failed to read port manager interface name");
		return FALSE;
	}
	if( rdb_get_single( rdb_variable( RDB_SYSTEM_PREFIX, "", RDB_SYSTEM_MODE ), s, RDB_VALUE_SIZE_SMALL ) != 0 ||
		strcmp(s, "no network service") == 0 ||
		strcmp(s, "No Network Service") == 0 ||
		strcmp(s, "Not Available") == 0 ||
		strcmp(s, "None") == 0 ||
		strcmp(s, "Invalid service") == 0)
	{
		SYSLOG_ERR("system mode is '%s', generate engage tone", s);
		return TRUE;
	}
	return FALSE;
}

/* check emergency call or limited service to generate soft dial tone
 * note : cnsmgr and simple_at_manager uses different rdb variables and values */
static BOOL is_limited_service(void)
{
	char s[RDB_VALUE_SIZE_SMALL];

	if( rdb_get_single( RDB_IF_NAME, s, RDB_VALUE_SIZE_SMALL ) != 0 ) {
		SYSLOG_ERR("failed to read port manager interface name");
		return FALSE;
	}
	if (strcmp(s, "cns") == 0) {
		if( rdb_get_single( rdb_variable( RDB_SYSTEM_PREFIX, "", RDB_SERVICE_STATUS ), s, RDB_VALUE_SIZE_SMALL ) == 0 &&
			( strstr(s, "Emergency") || strstr(s, "limited") ) ) {
			SYSLOG_ERR("'%s' status, generate soft dial tone", s);
			return TRUE;
		}
	} else {
		if( rdb_get_single( rdb_variable( RDB_SYSTEM_PREFIX, "", RDB_NETWORK_UNENC ), s, RDB_VALUE_SIZE_SMALL ) == 0 &&
			strstr(s, "Limited Service") ) {
			SYSLOG_ERR("'%s' status, generate soft dial tone", s);
			return TRUE;
		}
		if( rdb_get_single( rdb_variable( RDB_SYSTEM_PREFIX, "", RDB_NETWORK ), s, RDB_VALUE_SIZE_SMALL ) == 0 &&
			( strstr(s, "Limited Service") || strstr(s, "%4c%69%6d%69%74%65%64%20%53%65%72%76%69%63%65") ) ) {	/* Limited Service */
			SYSLOG_ERR("'%s' status, generate soft dial tone", s);
			return TRUE;
		}
	}
	return FALSE;
}

static sim_status_enum_type check_sim_status(void)
{
	char s[RDB_VALUE_SIZE_SMALL];

	/* check mep lock status prior to sim status */
	if (rdb_get_single( rdb_variable( RDB_SIM_STATUS_PREFIX, "", RDB_SIM_STATUS ), s, RDB_VALUE_SIZE_SMALL ) != 0 ||
		strcmp(s, "SIM OK") == 0) {
		return SIM_OK;
	}
	if (strcmp(s, "SIM removed") == 0 || strcmp(s, "SIM not inserted") == 0) {
		return NO_SIM;
	} else if (strncmp(s, "SIM locked", strlen("SIM locked")) == 0 || strncmp(s, "SIM PIN", strlen("SIM PIN")) == 0) {
		return SIM_LOCKED;
	} else if (strncmp(s, "SIM PUK", strlen("SIM PUK")) == 0) {
		return SIM_PUK;
	} else if (strncmp(s, "PH-NET PIN", strlen("PH-NET PIN")) == 0 ||
	           strncmp(s, "SIM PH-NET", strlen("SIM PH-NET")) == 0 ||
	           strncmp(s, "MEP", strlen("MEP")) == 0) {
		return MEP_LOCKED;
	}
	return SIM_OK;
}

static void handle_pots_event_off_hook(int idx, pots_bridge_event_t event)
{
	int i, next_active_idx;
	pots_call_type this_call_type = slic_info[idx].call_type;
	sim_status_enum_type sim_status;

	/* out band DTMF sending */
	pots_bridge[idx].send_outband_dmtf = FALSE;

	if (pick_up(idx, calls_first_of(pots_bridge[idx].call.control.calls, call_incoming)) == SUCCESS)
	{
		if (roaming_call_blocked(idx, INCOMING_VOICE_ROAMING_DB_NAME))
		{
			SYSLOG_ERR("Can not pick up a incoming call in roaming area.");
			call_end_process(idx, event_on_hook, TRUE);
			play_busy_tone(idx);
			slic_enable_pcm_dynamic(slic_info[idx].slic_fd, FALSE, idx);
			mark_voice_roaming_call_blocked(TRUE, INCOMING_VOICE_ROAMING_BLOCKED_DB_NAME);
			return;
		}
		mark_voice_roaming_call_blocked(FALSE, INCOMING_VOICE_ROAMING_BLOCKED_DB_NAME);

		SYSLOG_DEBUG("idx [%d] pick_up SUCCESS", idx);
		slic_play(slic_info[idx].slic_fd, slic_tone_none);
		set_slic_call_active_state(idx, TRUE);
		slic_enable_pcm_dynamic(slic_info[idx].slic_fd, TRUE, idx);
		pots_bridge[idx].wait_for_fsk_complete = FALSE;

#ifdef HAS_VOIP_FEATURE
		/* change PCM clk to processor for VOIP call */
		if (slic_info[idx].call_type == VOICE_ON_IP)
		{
			(void) slic_change_pcm_clk_source(slic_info[0].slic_fd, 0);
		}
#endif	/* HAS_VOIP_FEATURE */

		clear_dtmf_key_cmd_n_digits(idx);

		/* update incoming call count */
		update_call_counts(idx, FALSE);

		/* set to Outband DTMF mode for all incoming call for AU/NZ */
		if (tel_profile == DEFAULT_PROFILE  || tel_profile == CANADIAN_PROFILE) {
			pots_bridge[idx].send_outband_dmtf = TRUE;
			(void) slic_set_dtmf_tx_mode( slic_info[idx].slic_fd, OUTBAND_CUT );
#if defined(PLATFORM_PLATYPUS) || defined(PLATFORM_PLATYPUS2)
			set_pcm_cpu_buffer(idx);
			slic_change_cpld_mode(CPLD_MODE_BUFFER);
#else
			slic_change_cpld_mode(CPLD_MODE_VOICE);
#endif
		} else {
			if (is_external_crystal_exist)
				slic_change_cpld_mode(CPLD_MODE_VOICE);
		}

		// TO DO : originally multiple VOIP calls are allowed but restrict for one call temporally
		for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
		{
			if (i == idx || slic_info[i].call_type != this_call_type)
			{
				continue;
			}
			SYSLOG_DEBUG("disable other %s call port exclusively [%d]", ct_str(idx), i);
			clear_ss_request(i);
			timer_reset(&pots_bridge[i].number.timer);
			//timer_reset(&pots_bridge[i].call.poll_timer);
			//hang_up_all(i);
			if (pots_bridge[i].loop.state == slic_off_hook) {
				play_busy_tone(idx);
			} else {
				slic_play(slic_info[i].slic_fd, slic_tone_none);
			}

			vmwi_led_control(i, pots_bridge[i].loop.state == slic_off_hook ? led_on : led_off);
			set_slic_call_active_state(i, FALSE);
			slic_enable_pcm_dynamic(slic_info[i].slic_fd, FALSE, idx);
		}
	}
	else
	{

/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
		if (read_autodial_variables_from_db() == -1)
		{
			SYSLOG_ERR("autodial variable reading fail, give up autodial!!");
			play_busy_tone(idx);
			return;
		}
#endif

		if (roaming_call_blocked(idx, OUTGOING_VOICE_ROAMING_DB_NAME))
		{
			SYSLOG_ERR("Can not make outgoing call in roaming area.");
			if (tel_profile == DEFAULT_PROFILE) {
				play_sim_warning_tone(idx, LIMITED_SVC);
			} else {
				play_busy_tone(idx);
			}
			slic_enable_pcm_dynamic(slic_info[idx].slic_fd, FALSE, idx);
			mark_voice_roaming_call_blocked(TRUE, OUTGOING_VOICE_ROAMING_BLOCKED_DB_NAME);
			return;
		}
		mark_voice_roaming_call_blocked(FALSE, OUTGOING_VOICE_ROAMING_BLOCKED_DB_NAME);

		// TO DO : currently only one 3G/VOIP call can be connected.
		// Silent incoming call if any other call is active already.
		//next_active_idx = get_slic_next_call_active_index(this_call_type, idx);
		next_active_idx = get_slic_other_call_active_index(idx);
		if (next_active_idx >= 0)
		{
			SYSLOG_ERR("Line busy! Other %s call is already active.", ct_str(next_active_idx));
			play_busy_tone(idx);
			slic_enable_pcm_dynamic(slic_info[idx].slic_fd, FALSE, idx);
			return;
		}
#ifdef HAS_VOIP_FEATURE
		if (this_call_type != VOICE_ON_3G && !voip_sip_server_registered)
		{
			SYSLOG_ERR("VOIP client is not registered to SIP server!");
			play_busy_tone(idx);
			slic_enable_pcm_dynamic(slic_info[idx].slic_fd, FALSE, idx);
			return;
		}
#endif	/* HAS_VOIP_FEATURE */

		/* set ROH timer before play dial tone or busy tone to be reset
		 * below when busy tone starts for Telstra */
		roh_timer_set(&pots_bridge[idx].roh, TRUE);

		/* play engage tone in no coverage area for default profile */
		sim_status = check_sim_status();
		if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA && sim_status != SIM_OK) {
			play_sim_warning_tone(idx, sim_status);
		} else if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA && is_limited_service()) {
			play_sim_warning_tone(idx, LIMITED_SVC);
		} else if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA && is_no_coverage_area()) {
			play_busy_tone(idx);
		} else {
			/* Bell Canada VMWI feature */
			play_dial_tone(idx);
		}

		//slic_play( slic_info[idx].slic_fd, slic_tone_dial );
		/* end of Bell Canada VMWI feature */
		vmwi_state_machine(idx, event);
		if (call_control_init(idx, &pots_bridge[idx].call.control) != 0)
			SYSLOG_ERR("failed to initialize call control!");

/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
		if (autodial_enabled)
			autodial_timer_set(autodial_dialtone_duration);
#endif

	}
}

static void handle_pots_event_digit(int idx, pots_bridge_event_t event)
{
	if (pots_bridge[idx].loop.state == slic_on_hook)
	{
		clear_digits(&pots_bridge[idx].number);
		clear_dtmf_key_cmd_n_digits(idx);
		return;
	}
	if (allow_dtmf_key_send(idx) && !pots_bridge[idx].loop.recall_active)
	{
		SYSLOG_DEBUG("clear_digits for allow_dtmf_key_send(%d):%d && !pots_bridge[%d].loop.recall_active:%d",
				idx, allow_dtmf_key_send(idx), idx, !pots_bridge[idx].loop.recall_active);
		send_dtmf_tones(idx);
		clear_digits(&pots_bridge[idx].number);
		return;
	}
	else
	{
		SYSLOG_DEBUG("call count %d, is_dialing %d, recall active %d", call_count(idx, call_active), is_dialing(idx), pots_bridge[idx].loop.recall_active);
	}

	if (ss_request_pending(idx))
	{
		clear_digits(&pots_bridge[idx].number);
		return;
	}
	/* dial speed enhancement : Muting slic tone is needed after first digit dial only. */
	if (pots_bridge[idx].number.size <= 1)
	{
		// TO DO : currently only one 3G/VOIP call can be connected.
		// Silent incoming call if any other call is active already.
		//if (!check_call_resource_available(idx, this_call_type))
		if (get_slic_other_call_active_index(idx) >= 0)
		{
			play_busy_tone(idx);
			set_slic_call_active_state(idx, FALSE);
			clear_digits(&pots_bridge[idx].number);
			slic_enable_pcm_dynamic(slic_info[idx].slic_fd, FALSE, idx);
			return;
		}
		slic_play(slic_info[idx].slic_fd, slic_tone_none);
		set_slic_call_active_state(idx, TRUE);
		roh_timer_reset(&pots_bridge[idx].roh, TRUE);
		/* Rogers/Telus stutter tone feature */
		stutter_tone_timer_reset(&pots_bridge[idx].stutter_tone_timer);
	}

	if (is_dial_plan_number(idx))
	{
		/* dialing through handle_pots_event_dial_timer_expired() for recall process */
		//dial(idx);
		handle_pots_event_dial_timer_expired(idx, event_dial_timer_expired);
	}
	if (!is_ss_request(idx, TRUE))
	{
		return;
	}
	if (send_ss_request(idx) != 0)
	{
		slic_play(slic_info[idx].slic_fd, slic_tone_dial);
	}
}

static void handle_pots_event_dial_timer_expired(int idx, pots_bridge_event_t event)
{
	unsigned int temp_call_id1 = 0, temp_call_id2 = 0;
	unsigned int active_call_cnt = 0, held_call_cnt = 0, waiting_call_cnt = 0, total_call_cnt = 0;

	if (pots_bridge[idx].loop.state == slic_on_hook ||
		!(is_dialing(idx) || pots_bridge[idx].waiting_for_recall_dialing))
	{
		pots_bridge[idx].waiting_for_recall_dialing = FALSE;
		return;
	}
	pots_bridge[idx].waiting_for_recall_dialing = FALSE;
	if (!pots_bridge[idx].loop.recall_active)
	{
		/* for per call basis clip/clir */
		if (is_ss_request(idx, FALSE))
		{
			if (send_ss_request(idx) != 0)
			{
				slic_play(slic_info[idx].slic_fd, slic_tone_dial);
			}
			return;
		}

		dial(idx);
		return;
	}
	/* Canadian spec. need one digit dial */
	if (tel_profile == CANADIAN_PROFILE)
	{
		if ((pots_bridge[idx].number.size == 1 && pots_bridge[idx].number.digits[0] == '0') ||
				(pots_bridge[idx].number.size == 2 && pots_bridge[idx].number.digits[0] == '0' && pots_bridge[idx].number.digits[1] == '0'))
		{
			dial(idx);
			return;
		}
	}
	SYSLOG_DEBUG("------------------------------------------------------------------------------");
	SYSLOG_DEBUG("3w_call_prefix=%d, recall_key_count = %d",
			pots_bridge[idx].detect_3way_calling_prefix, pots_bridge[idx].recall_key_count);
	switch (pots_bridge[idx].number.size)
	{
		/* Canadian profile and Australian Telstra profile requires call hold by hook flash */
		case 0:
			if (tel_profile == CANADIAN_PROFILE || (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA))
			{
				slic_play(slic_info[idx].slic_fd, slic_tone_none);
				set_slic_call_active_state(idx, TRUE);
				roh_timer_reset(&pots_bridge[idx].roh, TRUE);
				/* Rogers/Telus stutter tone feature */
				stutter_tone_timer_reset(&pots_bridge[idx].stutter_tone_timer);
				if (pots_bridge[idx].detect_3way_calling_prefix)
				{
					/* Adds a held call to the conversation */
					if (pots_bridge[idx].recall_key_count == 1)
					{
						chld(idx, incall_service_3, 0);
						pots_bridge[idx].detect_3way_calling_prefix = FALSE;
						pots_bridge[idx].recall_key_count = 0;
					}
					else if (pots_bridge[idx].recall_key_count >= 2)
					{
						pots_bridge[idx].detect_3way_calling_prefix = FALSE;
						pots_bridge[idx].recall_key_count = 0;

						/* Drop second multi-party call with 1 recall key */
						temp_call_id1 = get_last_multi_party_call_id(pots_bridge[idx].call.control.calls);
						if (temp_call_id1)
						{
							chld(idx, incall_service_1, temp_call_id1);
							break;
						}

						/* return to first call with 2 recall keys */
						temp_call_id1 = get_first_call_id_of(pots_bridge[idx].call.control.calls, call_alerting);
						temp_call_id2 = get_first_call_id_of(pots_bridge[idx].call.control.calls, call_held);
						SYSLOG_DEBUG("alerting call_id=%d", temp_call_id1);
						if (temp_call_id1)
							chld(idx, incall_service_1, temp_call_id1);
						SYSLOG_DEBUG("held call_id=%d", temp_call_id2);
						if (temp_call_id2)
							chld(idx, incall_service_2, 0);
					}
				}
				/* Places all active calls on hold and accepts the other (held or waiting) call */
				else
				{
					chld(idx, incall_service_2, 0);
					if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA) {
						pots_bridge[idx].loop.recall_active = TRUE;
					}
				}
			}
			return;
		case 1:
			if (tel_profile != CANADIAN_PROFILE) {
				/* Telstra want to switch callees with single '2' key. To support this, changed as below;
				* 1. If there is more than 2 active calls but no held call and no waiting call,
				*    switch to first active call
				* 2. If there is more than 1 holding call, than switch to the held call
				*/
				if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA) {
					if (pots_bridge[idx].number.digits[0] == '2') {
						active_call_cnt = calls_count(pots_bridge[idx].call.control.calls, call_active);
						held_call_cnt = calls_count(pots_bridge[idx].call.control.calls, call_held);
						waiting_call_cnt = calls_count(pots_bridge[idx].call.control.calls, call_waiting);
						total_call_cnt = pots_bridge[idx].call.control.count_all;
						SYSLOG_DEBUG("act_call_cnt = %d, held_call_cnt = %d, waiting_call_cnt = %d, total_call_cnt = %d",
							active_call_cnt, held_call_cnt, waiting_call_cnt, total_call_cnt);
						if (total_call_cnt >= 2 && held_call_cnt == 0 && waiting_call_cnt == 0) {
							temp_call_id1 = get_first_call_id_of(pots_bridge[idx].call.control.calls, call_active);
							SYSLOG_DEBUG("no held call, switch to first active call_id=%d", temp_call_id1);
							chld(idx, (incall_service_enum)(pots_bridge[idx].number.digits[0]), temp_call_id1);
							return;
						}
					}
				}
				chld(idx, (incall_service_enum)(pots_bridge[idx].number.digits[0]), 0);
			}
			return;
		case 2:
			if (tel_profile != CANADIAN_PROFILE) {
				/* Some MC8790V version start call index from 2 rather than 1.
				Adjust call index here before for +CHLD command as below;
						start call idx 2 and press recall/flash + 21 --> send +CHLD=22
				*/
				unsigned int fixed_call_idx = get_first_call_id_of(pots_bridge[idx].call.control.calls, call_dummy);
				chld(idx, (incall_service_enum)(pots_bridge[idx].number.digits[0]),
					(pots_bridge[idx].number.digits[1] - '0') + fixed_call_idx - 1);
			}
			return;
		default:
			/* Rogers : 3-way calling initiated by "*71"+number
			* 			call holding by recall/hook-flash */
			if (tel_profile == CANADIAN_PROFILE)
			{
				if (check_Canadian_3way_calling(idx) &&
					(call_count(idx, call_active) > 0 || call_count(idx, call_held) > 0) &&
					pots_bridge[idx].loop.recall_active &&
					pots_bridge[idx].recall_key_count == 1)
				{
					pots_bridge[idx].detect_3way_calling_prefix = TRUE;
					pots_bridge[idx].recall_key_count = 0;
				}
				else
				{
					pots_bridge[idx].detect_3way_calling_prefix = FALSE;
				}
			}
			dial(idx);
			return;
	}
	SYSLOG_DEBUG("3w_call_prefix=%d, recall_key_count = %d",
			pots_bridge[idx].detect_3way_calling_prefix, pots_bridge[idx].recall_key_count);
	SYSLOG_DEBUG("------------------------------------------------------------------------------");
}

static void handle_pots_event_ss_response(int idx, pots_bridge_event_t event)
{
	handle_ss_response(idx);
	if (pots_bridge[idx].loop.state == slic_off_hook && call_count(idx, call_active) == 0)
	{
		slic_play(slic_info[idx].slic_fd, slic_tone_dial);
		roh_timer_set(&pots_bridge[idx].roh, TRUE);
	}
}

static void handle_pots_event_recall(int idx, pots_bridge_event_t event)
{
	const struct call_t* active_call;
	active_call = calls_first_of(pots_bridge[idx].call.control.calls, call_active);
	SYSLOG_DEBUG("------------------------------------------------------------------------------");
	clear_digits(&pots_bridge[idx].number);

	/* Canadian call waiting and 3-way calling
	* generate dial tone only when 3-way calling commence
	*/
	if (tel_profile == CANADIAN_PROFILE &&
		active_call && active_call->direction == mobile_originated &&
		pots_bridge[idx].call.control.count_all == 1)
	{
		/* Canadian Recall Dial Tone
		* 350 + 440 Hz, 3 x ( 0.1s on / 0.1s off ) + continuous dial tone
		*/
		SYSLOG_DEBUG("generate recall dial tone");
		slic_play(slic_info[idx].slic_fd, slic_tone_recall);
		usleep(1000 * 600);
		slic_play(slic_info[idx].slic_fd, slic_tone_none);
		slic_play(slic_info[idx].slic_fd, slic_tone_dial);
	}
	/* Telstra recall dial tone uses normal dial tone */
	else if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA &&
		active_call && active_call->direction == mobile_originated &&
		pots_bridge[idx].call.control.count_all == 1)
	{
		SYSLOG_DEBUG("generate recall dial tone");
		slic_play(slic_info[idx].slic_fd, slic_tone_none);
		slic_play(slic_info[idx].slic_fd, slic_tone_dial);
	}
	else
	{
		slic_play(slic_info[idx].slic_fd, slic_tone_none);
		SYSLOG_DEBUG("surpress recall dial tone");
	}
	if(!pots_bridge[idx].call.control.count_all) {
		roh_timer_set(&pots_bridge[idx].roh, TRUE);
	}
	/* Canadian profile and Australian Telstra profile requires call hold by hook flash */
	if (tel_profile == CANADIAN_PROFILE || (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA))
	{
		if (active_call) {
			SYSLOG_DEBUG("act_call_dir %d, call cnt %d", active_call->direction, pots_bridge[idx].call.control.count_all);
		} else {
			SYSLOG_DEBUG("act_call_is null, call cnt %d", pots_bridge[idx].call.control.count_all);
		}
		SYSLOG_DEBUG("call_cnt act %d, held %d, recall_active %d",
				call_count(idx, call_active), call_count(idx, call_held), pots_bridge[idx].loop.recall_active);
		if ( (call_count(idx, call_active) > 0 || call_count(idx, call_held) > 0) &&
			pots_bridge[idx].loop.recall_active)
		{
			timer_set(&pots_bridge[idx].number.timer, INCALL_SERVICE_TIMEOUT);
			pots_bridge[idx].waiting_for_recall_dialing = TRUE;
			SYSLOG_DEBUG("waiting_for_recall_dialing = TRUE");
			pots_bridge[idx].recall_key_count++;
		}
	}
	if (pots_bridge[idx].loop.recall_active)
	{
		clear_digits(&pots_bridge[idx].incall_dtmf_number);
	}
	SYSLOG_DEBUG("3w_call_prefix=%d, recall_key_count = %d",
			pots_bridge[idx].detect_3way_calling_prefix, pots_bridge[idx].recall_key_count);
	SYSLOG_DEBUG("------------------------------------------------------------------------------");
}

static void handle_pots_event_call_state_changed(int idx, pots_bridge_event_t event)
{
	int i;
	int delayed_restore_3G_phone = 0;
	pots_call_type this_call_type = slic_info[idx].call_type;
	const struct call_t* incoming_call;
	const struct call_t* temp_incoming_call;
	const struct call_t* active_call;
	const struct call_t* waiting_call;
	BOOL force_disconnect = FALSE;
	BOOL call_disconnected = FALSE;

	update_calls(idx);

	incoming_call = calls_first_of(pots_bridge[idx].call.control.calls, call_incoming);
	active_call = calls_first_of(pots_bridge[idx].call.control.calls, call_active);
	waiting_call = calls_first_of(pots_bridge[idx].call.control.calls, call_waiting);

	SYSLOG_DEBUG("call.event[0] %s, [1] %s", pots_bridge[0].call.event, pots_bridge[1].call.event);

	call_disconnected = calls_disconnected(idx);
	//SYSLOG_DEBUG(" slic_info[%d].call_active = %d", idx, slic_info[idx].call_active);
	//SYSLOG_DEBUG(" calls_disconnected[%d] = %d, calls_empty[%d] = %d, strlen(pots_bridge[%d].call.event) = %d",
	//	idx, call_disconnected, idx, calls_empty(idx), idx, strlen(pots_bridge[idx].call.event));

	/* update outgoing call count */
	if (outgoing_call_connected(active_call))
		update_call_counts(idx, TRUE);

	/*
	* If dial to wrong number, sometimes receive "NO CARRIER" from simple_at_manager before receiving
	* first CLCC response. In this case every current/last call stack is empty so it passes
	* calls_disconnected() test and there is no way to initialize pots_bridge call state.
	* Therefore check as below with 4 condition to initialize pots_bridge call state.
	*/
	if (!call_disconnected &&
		memcmp(&pots_bridge[idx].call.event, "disconnect", sizeof("disconnect")) == 0 &&
		slic_info[idx].call_active &&
		calls_empty(idx))
	{
		SYSLOG_DEBUG(" disconnect mismatch[%d], disconnect by force", idx);
		//SYSLOG_DEBUG(" slic_info[%d].call_active = %d", idx, slic_info[idx].call_active);
		//SYSLOG_DEBUG(" calls_empty[%d] = %d", idx, calls_empty(idx));
		force_disconnect = TRUE;
	}

	if (call_disconnected || force_disconnect)
	{
		SYSLOG_DEBUG("----------------------------------------------------------------");
		SYSLOG_DEBUG("call disconnected[%d] = %d, by force = %d", idx, call_disconnected, force_disconnect);
		SYSLOG_DEBUG("call event %s, call count %d", &pots_bridge[idx].call.event[0], pots_bridge[idx].call.control.count_all);
		SYSLOG_DEBUG("----------------------------------------------------------------");

		if(!pots_bridge[idx].call.control.count_all)
		{
			call_end_process(idx, event, TRUE);
			if (this_call_type == VOICE_ON_3G)
			{
				delayed_restore_3G_phone = 1;
			}
			SYSLOG_DEBUG("call [%d] is disconnected", idx);
			timer_reset(&pots_bridge[idx].call.poll_timer);
		}

		pots_bridge[idx].wait_for_fsk_complete = FALSE;

		/* for sync other inactive ports' call state */
		for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
		{
			if (i == idx || slic_info[i].call_type != this_call_type)
				continue;
			SYSLOG_DEBUG("sync other call port [%d]", i);
			update_calls(i);
			/* 2009.12.16, khan, append some logic to stop ringing for all port
			* when the caller disconnect call immediately after connect. Without this logic,
			* sometimes one of slic port can not stop ringing.
			*/
			temp_incoming_call = calls_first_of(pots_bridge[i].call.control.calls, call_incoming);
			if (pots_bridge[i].loop.state == slic_on_hook && !temp_incoming_call)
			{
				call_end_process(i, event, FALSE);
				pots_bridge[i].wait_for_fsk_complete = FALSE;
			}
			/* end of 2009.12.15 */
		}

		if (delayed_restore_3G_phone && this_call_type == VOICE_ON_3G)
		{
			restore_phone();
			delayed_restore_3G_phone = 0;
		}
	}

	switch (pots_bridge[idx].loop.state)
	{
		case slic_on_hook:
			if (!incoming_call)
			{
				slic_play(slic_info[idx].slic_fd, slic_tone_none);
				if (slic_info[idx].call_active == TRUE)
				{
					hang_up_all(idx);
				}
				set_slic_call_active_state(idx, FALSE);
				roh_timer_reset(&pots_bridge[idx].roh, TRUE);
				/* Bell Canada VMWI feature */
				vmwi_state_machine(idx, event);
				/* Rogers/Telus stutter tone feature */
				stutter_tone_timer_reset(&pots_bridge[idx].stutter_tone_timer);
				timer_reset(&pots_bridge[idx].call.poll_timer);
				break;
			}
			// TO DO : currently only one 3G/VOIP call can be connected.
			// Silent incoming call if any other call is active already.
			if (incoming_call->last.empty || incoming_call->last.state != call_incoming)
			{
				// TO DO : currently only one 3G/VOIP call can be connected.
				// Silent incoming call if any other call is active already.
				if (get_slic_other_call_active_index(idx) >= 0)
				{
					SYSLOG_ERR("other call is active already, ignore incoming call");
					break;
				}
				ring(idx, incoming_call);
			}
			break;

		case slic_off_hook:
		{
			/* Call status is not changed from 'alerting' to 'active' when call to IVR server in Canada.
			* So changed to not use DTMF key block feature
			*/
			//if (pots_bridge[idx].dtmf_key_blocked)
			//	set_dtmf_key_block_state(idx, FALSE);

			//SYSLOG_DEBUG("dtmf_key_blocked[%d] = %d", idx, pots_bridge[idx].dtmf_key_blocked);

			vmwi_state_machine(idx, event);

			if (tel_profile == CANADIAN_PROFILE && tel_sp_profile == SP_TELUS && pots_bridge[idx].call.control.count_all < 2) {
				pots_bridge[idx].detect_3way_calling_prefix = FALSE;
				//pots_bridge[idx].waiting_for_recall_dialing = FALSE;
			}

			SYSLOG_DEBUG("is_dialing %d || wait_recall_dial %d || det_3w-call %d",
					is_dialing(idx), pots_bridge[idx].waiting_for_recall_dialing,
					pots_bridge[idx].detect_3way_calling_prefix);
			SYSLOG_DEBUG("call_cnt act %d, held %d, recall_act %d, recall_key_cnt %d",
					call_count(idx, call_active), call_count(idx, call_held),
					pots_bridge[idx].loop.recall_active, pots_bridge[idx].recall_key_count);
			if (is_dialing(idx))
			{
				break;
			}
			if (pots_bridge[idx].waiting_for_recall_dialing ||
				pots_bridge[idx].detect_3way_calling_prefix ||
				call_count(idx, call_active) > 0 ||
				call_count(idx, call_held) > 0)
			{
				; //SYSLOG_ERR("skip clear_digits()");
			}
			else
			{
				SYSLOG_ERR("call clear_digits()");
				clear_digits(&pots_bridge[idx].number);
			}
			if (force_disconnect ||
				(call_count(idx, call_held) > 0 && pots_bridge[idx].call.control.count_all == call_count(idx, call_held)))
			{
				play_busy_tone(idx);
			}
			else
			{
				if (waiting_call && waiting_call->last.empty && waiting_call->current.state == call_waiting)
				{
					//SYSLOG_ERR("-------------------------------");
					//SYSLOG_ERR(" START CALL WAITING TONE");
					//SYSLOG_ERR("-------------------------------");
					slic_play(slic_info[idx].slic_fd, slic_tone_call_waiting);
				}
				else
				{
					play_tone(idx);
				}
			}
			break;
		}
	}
}

static void handle_pots_event_shutdown(int idx, pots_bridge_event_t event)
{
	handle_pots_event_on_hook(idx, event);
}

#define RDB_FXS_TEST_NAME          "pots.fxs_testing"
static void handle_pots_event_roh_timer_expired(int idx, pots_bridge_event_t event)
{
	char s[RDB_VALUE_SIZE_SMALL];
	if (pots_bridge[idx].loop.state == slic_off_hook)
	{
		if (rdb_get_single(RDB_FXS_TEST_NAME, s, RDB_VALUE_SIZE_SMALL) == 0 && strcmp(s, "1") == 0)
		{
			SYSLOG_ERR("skip ROH tone in fxs test mode");
			return;
		}
		if (tel_sp_profile == SP_TELSTRA) {
			if (pots_bridge[idx].roh.initial) {
				SYSLOG_DEBUG("Telstra ROH start, ch [%d]", idx);
				roh_timer_set(&pots_bridge[idx].roh, FALSE);
				play_busy_tone(idx);
			} else {
				SYSLOG_DEBUG("Telstra ROH stop, ch [%d]", idx);
				roh_timer_reset(&pots_bridge[idx].roh, TRUE);
				slic_play(slic_info[idx].slic_fd, slic_tone_none);
			}
		} else {
			SYSLOG_DEBUG("ROH start, ch [%d]", idx);
			slic_play(slic_info[idx].slic_fd, slic_tone_roh);
		}
	}
}

static void handle_pots_event_vmwi_timer_expired(int idx, pots_bridge_event_t event)
{
	vmwi_state_machine( idx, event );
}

static void handle_pots_event_stutter_tone_timer_expired(int idx, pots_bridge_event_t event)
{
	if (pots_bridge[idx].loop.state == slic_off_hook)
	{
		SYSLOG_DEBUG("stutter tone expired, start normal dial tone, ch [%d]", idx);
		slic_play(slic_info[idx].slic_fd, slic_tone_dial);
	}
}

static void handle_pots_event_dtmf_timer_expired(int idx, pots_bridge_event_t event)
{
	pots_bridge[idx].waiting_next_dtmf_keys = FALSE;
}

/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
static void handle_pots_event_autodial_timer_expired(int idx, pots_bridge_event_t event)
{
	SYSLOG_DEBUG("auto dial timer expired, idx %d, prepare auto dial", idx);
	pots_bridge[idx].number.size = strlen(autodial_dial_string);
	strcpy(pots_bridge[idx].number.digits, autodial_dial_string);
	slic_play(slic_info[idx].slic_fd, slic_tone_none);
	slic_play_dtmf_tones(slic_info[idx].slic_fd, (const char*)&autodial_dial_string, TRUE);
	dial(idx);
}
#endif

static void handle_pots_event_congestion_tone_timer_expired(int idx, pots_bridge_event_t event)
{
	SYSLOG_DEBUG("congestion tone timer expired, idx %d, keep silence", idx);
	slic_play(slic_info[idx].slic_fd, slic_tone_none);
}

static void handle_pots_event_none(int idx, pots_bridge_event_t event)
{
}

static void handle_pots_event(int idx, pots_bridge_event_t event)
{
	SYSLOG_DEBUG("idx [%d][%s] on event '%s'", idx, ct_str(idx), event_names[event]);
	pots_bridge[idx].pending_event = event_none;

	(*p_handle_pots_event[event])(idx, event);

	if (pots_bridge[idx].pending_event != event_none)
	{
		SYSLOG_DEBUG("idx [%d][%s] pending event: '%s'", idx, ct_str(idx), event_names[pots_bridge[idx].pending_event]);
		handle_pots_event(idx, pots_bridge[idx].pending_event);
	}
}

#define TIME_DIFF_MSEC( t1, t2 ) ( ( t2.time - t1.time ) * 1000 + ( t2.millitm - t1.millitm ) )

static int handle_pots_event_loop_state(int idx, const struct slic_event_t* slic_event)
{
	slic_on_off_hook_enum new_loop_state = slic_event->data.loop_state;
	struct timeb original, now;
	pots_bridge_event_t event = event_on_hook;
	ftime(&original);
	now.time = original.time;
	now.millitm = original.millitm;
	SYSLOG_DEBUG("idx [%d] event: '%s'", idx, loop_stat_names[new_loop_state]);
	if (pots_bridge[idx].loop.state == new_loop_state)
	{
		return 0;
	}
	switch (new_loop_state)
	{
		case slic_on_hook:
			while (TIME_DIFF_MSEC(original, now) <= CLEAR_FORWARD_DURATION_MSEC && new_loop_state == slic_on_hook)
			{
				usleep(LOOP_STATE_POLL_DURATION_MSEC * 1000);
				if (slic_get_loop_state(slic_info[idx].slic_fd, &new_loop_state) != 0)
				{
					SYSLOG_ERR("failed to get loop state; assume still 'on hook'");
				}
				ftime(&now);
			}
			event = new_loop_state == slic_off_hook && TIME_DIFF_MSEC(original, now) >= RECALL_DURATION_MIN_MSEC && TIME_DIFF_MSEC(original, now) <= RECALL_DURATION_MAX_MSEC
			? event = event_recall : event_on_hook;
			/* for On-Hook, PCM off */
			set_recall_state(idx, TRUE);
			if (event == event_on_hook) pots_bridge[idx].loop.recall_active = FALSE;
			break;
		case slic_off_hook:
			event = event_off_hook;
			break;
	}
	SYSLOG_DEBUG("idx %d, ch[%d] '%s' -> '%s'%s", idx, slic_info[idx].cid, loop_stat_names[pots_bridge[idx].loop.state], loop_stat_names[new_loop_state], event == event_recall ? "; recall detected" : "");
	/* append slic_led_control */
	if (event != event_recall)
	{
		vmwi_led_control(idx, new_loop_state ? led_on : led_off);
	}
	pots_bridge[idx].loop.state = new_loop_state;
	handle_pots_event(idx, event);
	return 0;
}

static int init_loop_state(int idx)
{
	slic_on_off_hook_enum loop_state;
	if (slic_register_event_handler(slic_event_loop_state, handle_pots_event_loop_state) != 0)
	{
		return -1;
	}
	if (slic_get_loop_state(slic_info[idx].slic_fd, &loop_state) != 0)
	{
		return -1;
	}
	pots_bridge[idx].loop.state = loop_state;
	if (pots_bridge[idx].loop.state == slic_off_hook)
	{
		handle_pots_event(idx, event_off_hook);
	};
	/* set default recall state FALSE & PCM off */
	//set_recall_state(idx, TRUE);	// default PCM off
	pots_bridge[idx].loop.recall_active = FALSE;
	slic_enable_pcm_dynamic(slic_info[idx].slic_fd, FALSE, idx);
	return 0;
}

struct timeb start[MAX_CHANNEL_NO];
static int handle_pots_event_dtmf(int idx, const struct slic_event_t* event)
{
	SYSLOG_ERR("DTMF: %i (%c)", event->data.dtmf_digit, digitmap[ event->data.dtmf_digit ]);
	if (pots_bridge[idx].dtmf_key_blocked)
	{
		//SYSLOG_DEBUG("**** pots_bridge[%d].dtmf_key_blocked", idx);
		return 0;
	}
	ftime(&start[idx]);
	if (add_digit(idx, digitmap[ event->data.dtmf_digit ]))
	{
		handle_pots_event(idx, event_digit);
	}
	return 0;
}

static int handle_pots_event_dtmf_complete(int idx, const struct slic_event_t* event)
{
	struct timeb now;
	unsigned int elapsed_t = 0;
	if (pots_bridge[idx].dtmf_key_blocked)
	{
		//SYSLOG_DEBUG("**** pots_bridge[%d].dtmf_key_blocked", idx);
		return 0;
	}
	ftime(&now);
	elapsed_t = TIME_DIFF_MSEC(start[idx], now) + 20;
	SYSLOG_ERR("DTMF detected for %d ms", elapsed_t);
	return 0;
}

static int handle_pots_event_fsk_complete(int idx, const struct slic_event_t* event)
{
	slic_on_off_hook_enum temp_loop_state, loop_state[MAX_CHANNEL_NO];
	int i;
	const struct call_t* incoming_call;
	pots_call_type this_call_type = slic_info[idx].call_type;
	update_calls(idx);
	incoming_call = calls_first_of(pots_bridge[idx].call.control.calls, call_incoming);
	SYSLOG_DEBUG("Got FSK Complete event in idx %d", idx);
	// send fsk first then wait for fsk complete event, then change to distinctive mode
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		loop_state[i] = slic_on_hook;
		/* check all off-hook port with same call type */
		if (slic_info[i].call_type == this_call_type &&
				slic_get_loop_state(slic_info[i].slic_fd, &temp_loop_state) == 0)
		{
			/* if a slic port is off-hook before incoming call and there is incoming call,
			* need to ring to other port and enable to connect call.
			*/
			if (temp_loop_state == slic_off_hook && i == get_slic_first_call_active_index(this_call_type))
				loop_state[i] = temp_loop_state;
		}
	}
	/* changed to start ringing all ports simutaneously after FSK transmission is over in all ports */
	if (idx  == MAX_CHANNEL_NO - 1 )
	{
		SYSLOG_DEBUG( "start ringing in all ports at the same time");
		for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
		{
			if (pots_bridge[i].wait_for_fsk_complete && loop_state[i] == slic_on_hook && incoming_call)
			{
				/* support international telephony profile */
				if (tel_profile ==  CANADIAN_PROFILE || tel_profile ==  NORTH_AMERICA_PROFILE)
				{
					slic_play_distinctive_ring(slic_info[i].slic_fd, slic_distinctive_ring_canada);
				}
				else
				{
					slic_play_distinctive_ring(slic_info[i].slic_fd, slic_distinctive_ring_0);
				}
				/* end of support international telephony profile */
			}
		}
	}
	/* call event machine to process post ring event */
	handle_pots_event(idx, event_call_state_changed);
	return 0;
}

static int handle_pots_event_vmwi_complete( int idx, const struct slic_event_t* event)
{
	SYSLOG_DEBUG( "Got VMWI Complete event in idx %d, result %d", idx, event->data.vmwi_result );
	/* if succeeded, set to vmwi delivered state,
	* else set to idle state to retry.
	*/
	display_vmwi_state( idx, 0 );
	if ( event->data.vmwi_result == 0)
	{
		pots_bridge[idx].vmwi.status = NOTI_DELIVERED;
		vmwi_led_control(idx, (pots_bridge[idx].vmwi.cmd == ACTIVE)? led_vmwi_on:led_off);
	}
	else
	{
		pots_bridge[idx].vmwi.status = INIT;
	}
	display_vmwi_state( idx, 1 );
	handle_pots_event( idx, pots_bridge[idx].loop.state );
	return 0;
}

static void handle_pots_bridge_timers(int idx)
{
	if (timer_expired(&pots_bridge[idx].number.timer))
	{
		timer_reset(&pots_bridge[idx].number.timer);
		handle_pots_event(idx, event_dial_timer_expired);
	}
	if (timer_expired(&pots_bridge[idx].incall_dtmf_number.timer))
	{
		timer_reset(&pots_bridge[idx].incall_dtmf_number.timer);
		handle_pots_event(idx, event_DTMF_timer_expired);
	}
	if (timer_expired(&pots_bridge[idx].call.poll_timer))
	{
		timer_reset(&pots_bridge[idx].call.poll_timer);
		handle_pots_event(idx, event_call_state_changed);
	}
	if (timer_expired(&pots_bridge[idx].roh.timer))
	{
		roh_timer_reset(&pots_bridge[idx].roh, FALSE);
		handle_pots_event(idx, event_roh_timer_expired);

	}
	if( timer_expired( &pots_bridge[idx].vmwi.timer ) )
	{
		vmwi_timer_reset( &pots_bridge[idx].vmwi.timer );
		handle_pots_event( idx, event_vmwi_timer_expired );
	}
	if( timer_expired( &pots_bridge[idx].stutter_tone_timer ) )
	{
		vmwi_timer_reset( &pots_bridge[idx].stutter_tone_timer );
		handle_pots_event( idx, event_stutter_tone_timer_expired );
	}
/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
	if( timer_expired( &autodial_timer ) )
	{
		autodial_timer_reset();
		handle_pots_event( idx, event_autodial_timer_expired );
	}
#endif
	if( timer_expired( &pots_bridge[idx].congestion_tone_timer ) )
	{
		timer_reset( &pots_bridge[idx].congestion_tone_timer );
		handle_pots_event( idx, event_congestion_tone_timer_expired );
	}
}

int dial_plan_regex_from_rdb(const char *var_p, int idx)
{
	char s[REGEX_SIZE];
	if (rdb_get_single(var_p, s, REGEX_SIZE) != 0 || s[0] == 0)
	{
		//SYSLOG_ERR("failed to read '%s' (%s)", var_p, strerror(errno));
		return -1;
	}
	if (strcmp(pots_bridge[idx].dial_plan.regex_string, s) == 0)
	{
		return 0;
	}
	regfree(&pots_bridge[idx].dial_plan.regex);
	if (regcomp(&pots_bridge[idx].dial_plan.regex, s, REG_EXTENDED | REG_NOSUB) != 0)
	{
		SYSLOG_ERR("invalid regex '%s'; restore '%s'", s, pots_bridge[idx].dial_plan.regex_string);
		regcomp(&pots_bridge[idx].dial_plan.regex, pots_bridge[idx].dial_plan.regex_string, REG_EXTENDED | REG_NOSUB);
		rdb_set_single(var_p, pots_bridge[idx].dial_plan.regex_string);
		return -1;
	}
	strcpy(pots_bridge[idx].dial_plan.regex_string, s);
	return 0;
}

int dial_plan_timeout_from_rdb(const char *var_p, int idx)
{
	char s[5];
	int timeout;
	if (rdb_get_single(var_p, s, 5) != 0 || s[0] == 0)
	{
		//SYSLOG_ERR("failed to read '%s' (%s)", var_p, strerror(errno));
		return -1;
	}
	timeout = atoi(s);
	pots_bridge[idx].dial_plan.timeout = timeout;
	return 0;
}

int dial_plan_international_prefix_from_rdb(const char *var_p, int idx)
{
	char s[INTERNATIONAL_PREFIX_SIZE];
	if (rdb_get_single(var_p, s, INTERNATIONAL_PREFIX_SIZE) != 0 || s[0] == 0)
	{
		//SYSLOG_ERR("failed to read '%s' (%s)", var_p, strerror(errno));
		return -1;
	}
	strcpy(pots_bridge[idx].dial_plan.international_prefix, s);
	return 0;
}

static int dial_plan_init(int idx)
{
	char s[RDB_VALUE_SIZE_LARGE];

	/* initialize dial plan : regex */
	if (rdb_get_single(rdb_variable(RDB_DIALPLAN_PREFIX, RDB_DP_REGEX, "override"), s, RDB_VALUE_SIZE_LARGE) == 0)
	{
		SYSLOG_ERR("found override dialplan '%s' : %s", rdb_variable(RDB_DIALPLAN_PREFIX, RDB_DP_REGEX, ".override"), s);
		strcpy(pots_bridge[idx].dial_plan.regex_string, s);
	} else {
		if (tel_profile == CANADIAN_PROFILE)
		{
			strcpy(pots_bridge[idx].dial_plan.regex_string,
					(tel_sp_profile == SP_ROGERS)? ROGERS_REGEX:CANADAIAN_REGEX);
		}
		else if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_TELSTRA)
		{
			strcpy(pots_bridge[idx].dial_plan.regex_string, TELSTRA_REGEX);
		}
		else if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_CELCOM)
		{
			strcpy(pots_bridge[idx].dial_plan.regex_string, CELCOM_REGEX);
		}
		else
		{
			strcpy(pots_bridge[idx].dial_plan.regex_string, DEFAULT_REGEX);
		}
	}
	if (regcomp(&pots_bridge[idx].dial_plan.regex, pots_bridge[idx].dial_plan.regex_string, REG_EXTENDED | REG_NOSUB) != 0)
	{
		SYSLOG_ERR("failed to compile default regex '%s'", pots_bridge[idx].dial_plan.regex_string);
		return -1;
	}
	rdb_set_single(rdb_variable(RDB_DIALPLAN_PREFIX, "", RDB_DP_REGEX), pots_bridge[idx].dial_plan.regex_string);

	/* initialize dial plan : international prefix */
	if (rdb_get_single(rdb_variable(RDB_DIALPLAN_PREFIX, RDB_DP_INT_PREFIX, "override"), s, RDB_VALUE_SIZE_LARGE) == 0)
	{
		SYSLOG_ERR("found override dialplan '%s' : %s", rdb_variable(RDB_DIALPLAN_PREFIX, RDB_DP_INT_PREFIX, ".override"), s);
		strcpy(pots_bridge[idx].dial_plan.international_prefix, s);
	} else {
		if (tel_profile == CANADIAN_PROFILE)
			strcpy(pots_bridge[idx].dial_plan.international_prefix, CANADIAN_INTERNATIONAL_PREFIX);
		else if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_CELCOM)
			strcpy(pots_bridge[idx].dial_plan.international_prefix, MALAYSIAN_INTERNATIONAL_PREFIX);
		else
			strcpy(pots_bridge[idx].dial_plan.international_prefix, DEFAULT_INTERNATIONAL_PREFIX);
	}
	rdb_set_single(rdb_variable(RDB_DIALPLAN_PREFIX, "", RDB_DP_INT_PREFIX), pots_bridge[idx].dial_plan.international_prefix);

	/* initialize dial plan : dial timeout */
	pots_bridge[idx].dial_plan.timeout = DEFAULT_DIAL_TIMEOUT_SEC;
	sprintf(s, "%u", pots_bridge[idx].dial_plan.timeout);
	rdb_set_single(rdb_variable(RDB_DIALPLAN_PREFIX, "", RDB_DP_TIMEOUT), s);
	return 0;
}

static void dial_plan_release(int idx)
{
	regfree(&pots_bridge[idx].dial_plan.regex);
}

#ifdef USE_ALSA
char* itoa(unsigned int val, int base)
{
	static char buf[32] = {0};
	int i = 30;

	if (val == 0)
	{
		buf[0] = '0';
		return &buf[0];
	}
	for (; val && i ; --i, val /= base)
		buf[i] = "0123456789abcdef"[val % base];
	return &buf[i+1];
}
#endif

/* Bell Canada VMWI feature */
static void init_vmwi(int idx)
{
	pots_bridge[idx].vmwi.cmd = EMPTY;
	pots_bridge[idx].vmwi.status = INIT;
}

static void vmwi_status_from_rdb(const char *var_p, int idx)
{
	char s[10];
	static vmwi_cmd_enum_type new_cmd = EMPTY;

	if( rdb_get_single( var_p, s, 10 ) != 0)
	{
		/* do not change vmwi status */
		SYSLOG_ERR( "failed to get rdb vmwi status" );
		return;
	}
	else if ( !strcmp( s, "active" ) )
	{
		new_cmd = ACTIVE;
	}
	else if ( !strcmp( s, "inactive" ) )
	{
		new_cmd = INACTIVE;
	}
	else
	{
		new_cmd = EMPTY;
	}
	SYSLOG_DEBUG("VMWI : old cmd %d, new cmd %d", pots_bridge[idx].vmwi.cmd, new_cmd);
	/* change to send VMWI whenever pots receives rdb notification from simple at manager
	* even though old and new state is same.
	*/
	pots_bridge[idx].vmwi.cmd = new_cmd;
	if (new_cmd == EMPTY)
	{
		pots_bridge[idx].vmwi.status = INIT;
	}
	else
	{
		pots_bridge[idx].vmwi.status = IDLE;
	}
}
/* end of Bell Canada VMWI feature */

/* out band DTMF sending */
static void read_mbdn_msisdn_from_rdb(void)
{
	char s[MAX_DIAL_DIGITS];
	int s_len;

#ifdef USE_PREDEF_VMBOX_NO
	if (1) {
#else
	if (tel_profile == CANADIAN_PROFILE) {
#endif
		if (!valid_mbn && !valid_mbdn) {
			(void) memset(s, 0x00, MAX_DIAL_DIGITS);
			if( rdb_get_single( rdb_variable( RDB_SIM_DATA_PREFIX, "", RDB_SIM_MBN ), s, MAX_DIAL_DIGITS ) != 0) {
				SYSLOG_ERR( "failed to read sim mbn" );
				return;
			}
			s_len = strlen(s);
			strcpy(sim_mbn_orig, s);
			if (s_len >=MBN_DIGITS_LIMIT) {
				(void) strncpy((char *)&sim_mbn[0], (const char *)&s[s_len-MBN_DIGITS_LIMIT], MBN_DIGITS_LIMIT);
			} else {
				(void) strncpy((char *)&sim_mbn[0], (const char *)&s, s_len);
			}
			if (s_len > 0)
				valid_mbn = TRUE;
		}
		SYSLOG_INFO( "SIM MBN = %s, valid = %d", sim_mbn, valid_mbn );

		if (!valid_mbdn) {
			(void) memset(s, 0x00, MAX_DIAL_DIGITS);
			if( rdb_get_single( rdb_variable( RDB_SIM_DATA_PREFIX, "", RDB_SIM_MBDN ), s, MAX_DIAL_DIGITS ) != 0) {
				SYSLOG_ERR( "failed to read sim mbdn" );
				return;
			}
			s_len = strlen(s);
			strcpy(sim_mbdn_orig, s);
			if (s_len >=MBN_DIGITS_LIMIT) {
				(void) strncpy((char *)&sim_mbdn[0], (const char *)&s[s_len-MBN_DIGITS_LIMIT], MBN_DIGITS_LIMIT);
			} else {
				(void) strncpy((char *)&sim_mbdn[0], (const char *)&s, s_len);
			}
			if (s_len > 0)
				valid_mbdn = TRUE;
		}
		SYSLOG_INFO( "SIM MBDN = %s, valid = %d", sim_mbdn, valid_mbdn );
	}

	if (tel_profile == DEFAULT_PROFILE && tel_sp_profile == SP_NZ_TELECOM) {
		if (!valid_adn)	{
			(void) memset(s, 0x00, MAX_DIAL_DIGITS);
			if( rdb_get_single( rdb_variable( RDB_SIM_DATA_PREFIX, "", RDB_SIM_ADN ), s, MAX_DIAL_DIGITS ) != 0) {
				SYSLOG_ERR( "failed to read sim adn" );
				return;
			}
			s_len = strlen(s);
			strcpy(sim_adn_orig, s);
			if (s_len >=MBN_DIGITS_LIMIT) {
				(void) strncpy((char *)&sim_adn[0], (const char *)&s[s_len-MBN_DIGITS_LIMIT], MBN_DIGITS_LIMIT);
			} else {
				(void) strncpy((char *)&sim_adn[0], (const char *)&s, s_len);
			}
			if (s_len > 0)
				valid_adn = TRUE;
		}
		SYSLOG_INFO( "SIM ADN = %s, valid = %d", sim_adn, valid_adn );
	}

	if (!valid_msisdn)
	{
		(void) memset(s, 0x00, MAX_DIAL_DIGITS);
		if( rdb_get_single( rdb_variable( RDB_SIM_DATA_PREFIX, "", RDB_SIM_MSISDN ), s, MAX_DIAL_DIGITS ) != 0)
		{
			SYSLOG_ERR( "failed to read sim msisdn" );
			return;
		}
		s_len = strlen(s);
		if (s_len >=MBN_DIGITS_LIMIT)
		{
			(void) strncpy((char *)&sim_msisdn[0], (const char *)&s[s_len-MBN_DIGITS_LIMIT], MBN_DIGITS_LIMIT);
		}
		else
		{
			(void) strncpy((char *)&sim_msisdn[0], (const char *)&s, s_len);
		}
		if (s_len > 0)
			valid_msisdn = TRUE;
	}
	SYSLOG_INFO( "SIM MSISDN = %s, valid = %d", sim_msisdn, valid_msisdn );
}
/* end of out band DTMF sending */

/* clear old dtmf key command */
static void clear_dtmf_key_variables(void)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		rdb_set_single(rdb_variable((const char *) slic_info[i].rdb_name_dtmf_cmd, "", ""), "");
	}
}

int handle_rdb_event_dialplan_regex(const char *var_p)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++) {
		dial_plan_regex_from_rdb(var_p, i);
	}
	return 0;
}

int handle_rdb_event_dialplan_int_prefix(const char *var_p)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		dial_plan_international_prefix_from_rdb(var_p, i);
	}
	return 0;
}

int handle_rdb_event_dialplan_timeout(const char *var_p)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		dial_plan_timeout_from_rdb(var_p, i);
	}
	return 0;
}

/* Bell Canada VMWI feature */
int handle_rdb_event_sms_vm_status(const char *var_p)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		vmwi_status_from_rdb(var_p, i);
	}
	return 0;
}
/* end of Bell Canada VMWI feature */

int handle_rdb_event_umts_cmd_status(const char *var_p)
{
	int i, active_idx;
	active_idx = get_slic_first_call_active_index(VOICE_ON_3G);
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (slic_info[i].call_type == VOICE_ON_3G &&
		(active_idx < 0 || slic_info[i].call_active == TRUE))
		{
			rdb_get_single(var_p, pots_bridge[i].ss.status, sizeof(pots_bridge[i].ss.status));
		}
	}
	return 0;
}

int handle_rdb_event_umts_calls_event(const char *var_p)
{
	int i, active_idx;
	active_idx = get_slic_first_call_active_index(VOICE_ON_3G);
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (slic_info[i].call_type == VOICE_ON_3G &&
		(active_idx < 0 || slic_info[i].call_active == TRUE))
		{
			rdb_get_single(var_p, pots_bridge[i].call.event, sizeof(pots_bridge[i].call.event));
		}
	}
	return 0;
}

/* check MCC and MNC and initialize SP profile */
int handle_rdb_event_mcc_mnc(const char *var_p)
{
	int i;
	const char *devices[MAX_CHANNEL_NO] = {0, };
	char instance[RDB_VALUE_SIZE_SMALL] = "0";
	static tel_profile_type last_tel_profile = DEFAULT_PROFILE;
	static tel_sp_type last_tel_sp_profile = MAX_SP_PROFILE;

	initialize_sp();
	if (last_tel_profile == tel_profile && last_tel_sp_profile == tel_sp_profile) {
		return 0;
	}

	/* add more process below if it depends on profile */
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		(void)dial_plan_init(i);
		devices[i] = (const char *)&slic_info[i].dev_name;
	}
	(void) rdb_get_single(RDB_POTS_INSTANCE, instance, RDB_VALUE_SIZE_SMALL);
	display_pots_setting(devices, (const char *)&instance, TRUE);
	last_tel_profile = tel_profile;
	last_tel_sp_profile = tel_sp_profile;
	return 0;
}

#ifdef HAS_VOIP_FEATURE
int handle_rdb_event_voip_reg_status(const char *var_p)
{
	char s[RDB_VALUE_SIZE_SMALL];

	if( rdb_get_single( var_p, s, RDB_VALUE_SIZE_SMALL ) != 0)
	{
		SYSLOG_ERR( "failed to get %s", var_p );
		return -1;
	}
	SYSLOG_DEBUG("VOIP : registered status %s", s);
	voip_sip_server_registered = (BOOL)atoi(s);
	return 0;
}

int handle_rdb_event_voip_cmd_status(const char *var_p)
{
	int i, active_idx;
	active_idx = get_slic_first_call_active_index(VOICE_ON_IP);
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (slic_info[i].call_type == VOICE_ON_IP &&
		(active_idx < 0 || slic_info[i].call_active == TRUE))
		{
			rdb_get_single(var_p, pots_bridge[i].ss.status, sizeof(pots_bridge[i].ss.status));
		}
	}
	return 0;
}

int handle_rdb_event_voip_calls_event(const char *var_p)
{
	int i, active_idx;
	active_idx = get_slic_first_call_active_index(VOICE_ON_IP);
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (slic_info[i].call_type == VOICE_ON_IP &&
		(active_idx < 0 || slic_info[i].call_active == TRUE))
		{
			rdb_get_single(var_p, pots_bridge[i].call.event, sizeof(pots_bridge[i].call.event));
		}
	}
	return 0;
}
#endif	/* HAS_VOIP_FEATURE */

/* read call service type (3G Voice, VOIP, FOIP) setting from NV or RDB */
#define SLIC_CALL_TYPE_DB_NAME	"slic_call_type"
static void read_call_type_from_db(void)
{
	char default_call_type[256]="\0";
	int i, idx = 0;
	char* db_str;
	char s[256];
	const char* pToken;
	char* pSavePtr;
#ifdef HAS_VOIP_FEATURE
	pots_call_type def_first_call_type = VOICE_ON_IP;
#else
	pots_call_type def_first_call_type = VOICE_ON_3G;
#endif

#if defined(PLATFORM_PLATYPUS)
	int result;
	nvram_init(RT2860_NVRAM);
	db_str = nvram_get(RT2860_NVRAM, SLIC_CALL_TYPE_DB_NAME);
#endif

	for (i = 0; i < MAX_CALL_TYPE_INDEX; i++)
	{
		call_type_cnt[i] = 0;
	}
	call_type_cnt[NONE] = 1;	/* used for common variable, so set to 1 */

	/* first channel default : VOIP */
	strcat((char *)&default_call_type, call_type_name[def_first_call_type]);
	strcat((char *)&default_call_type, ";");
	db_slic_call_type[0] = def_first_call_type;
	call_type_cnt[def_first_call_type]++;
	for (i = 1; i < MAX_CHANNEL_NO; i++)
	{
		strcat((char *)&default_call_type, call_type_name[VOICE_ON_3G]);
		strcat((char *)&default_call_type, ";");
		db_slic_call_type[i] = VOICE_ON_3G;
		call_type_cnt[VOICE_ON_3G]++;
	}


#if defined(PLATFORM_PLATYPUS)
	SYSLOG_DEBUG("read NV item : %s : %s", SLIC_CALL_TYPE_DB_NAME, db_str);
	/* if NV item not exist, write default call type to NV */
	if (!db_str || !strlen(db_str))
	{
		SYSLOG_DEBUG("No slic_call_type stored. Set to default '%s'", default_call_type);
		nvram_strfree(db_str);
		result = nvram_bufset(RT2860_NVRAM, SLIC_CALL_TYPE_DB_NAME, default_call_type);
		if (result < 0)
		{
			SYSLOG_ERR("write NV item failure: %s : %s", SLIC_CALL_TYPE_DB_NAME, default_call_type);
			goto nv_close_return;
		}
		result = nvram_commit(RT2860_NVRAM);
		if (result < 0)
			SYSLOG_ERR("commit NV items failure");
		goto nv_close_return;
	}
#else
	if( rdb_get_single( SLIC_CALL_TYPE_DB_NAME, s, 256 ) != 0)
	{
		SYSLOG_ERR( "failed to read '%s'", SLIC_CALL_TYPE_DB_NAME );
		if (rdb_update_single(SLIC_CALL_TYPE_DB_NAME, default_call_type, CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to create '%s' (%s)", SLIC_CALL_TYPE_DB_NAME, strerror(errno));
		}
		return;
	}
	db_str = s;
#endif

	for (i = 0; i < MAX_CALL_TYPE_INDEX; i++)
	{
		call_type_cnt[i] = 0;
	}
	call_type_cnt[NONE] = 1;	/* used for common variable, so set to 1 */

	/* parse call type and save */
	pToken = strtok_r(db_str, ";", &pSavePtr);
	while (pToken)
	{
		for (i = 0; i < MAX_CALL_TYPE_INDEX; i++)
		{
			if (strcmp(pToken, call_type_name[i]) == 0)
			{
				db_slic_call_type[idx] = (pots_call_type)i;
				call_type_cnt[i]++;
				break;
			}
		}
		idx++;
		pToken = strtok_r(NULL, ";", &pSavePtr);
	}
#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(db_str);
#endif

	/* store VOIP channel number to refer in pjsip */
	sprintf(s, "%d", call_type_cnt[VOICE_ON_IP]);
	if (rdb_update_single(RDB_VOIP_CH_NO, s, CREATE, ALL_PERM, 0, 0) != 0)
	{
		SYSLOG_ERR("failed to create 'pots.voip_ch_no' (%s)", strerror(errno));
	}

#if defined(PLATFORM_PLATYPUS)
nv_close_return:
	nvram_close(RT2860_NVRAM);
#endif
}

static unsigned int read_call_count_from_db(int call_type, char *cnt_name)
{
	char db_str[RDB_VALUE_SIZE_SMALL];
	char s[RDB_VALUE_SIZE_SMALL];
	(void)memset(db_str, 0x00, RDB_VALUE_SIZE_SMALL);
	(void)memset(s, 0x00, RDB_VALUE_SIZE_SMALL);
	sprintf(db_str, "%s.%s.%s", RDB_STATISTIC_USAGE, call_type_name[call_type], cnt_name);
	if( rdb_get_single( db_str, s, RDB_VALUE_SIZE_SMALL ) != 0)	{
		SYSLOG_ERR( "create new variable '%s'", db_str );
		if (rdb_update_single(db_str, "0", CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to create '%s' (%s)", db_str, strerror(errno));
		}
		return 0;
	}
	return atoi(s);
}

static void read_call_counts_from_db(void)
{
	int i;
	for (i = 1; i < MAX_CALL_TYPE_INDEX; i++)
	{
		pots_call_count.outgoing[i] = read_call_count_from_db(i, OUTGOING_CALL_COUNT);
		pots_call_count.incoming[i] = read_call_count_from_db(i, INCOMING_CALL_COUNT);
		pots_call_count.total[i] = read_call_count_from_db(i, TATAL_CALL_COUNT);
		SYSLOG_DEBUG("read db call cnt: out[%s] %d, in[%s] %d, total[%s] %d",
				call_type_name[i], pots_call_count.outgoing[i],
				call_type_name[i], pots_call_count.incoming[i],
				call_type_name[i], pots_call_count.total[i]);
	}
}

static void update_call_counts(int idx, BOOL outgoing_call)
{
	pots_call_type ct = slic_info[idx].call_type;
	char db_str[RDB_VALUE_SIZE_SMALL];
	char s[RDB_VALUE_SIZE_SMALL];

	/* update local call count variable from db before increase its value */
	read_call_counts_from_db();

	(void)memset(db_str, 0x00, RDB_VALUE_SIZE_SMALL);
	(void)memset(s, 0x00, RDB_VALUE_SIZE_SMALL);
	if (outgoing_call) {
		pots_call_count.outgoing[ct]++;
		sprintf(db_str, "%s.%s.%s", RDB_STATISTIC_USAGE, call_type_name[ct], OUTGOING_CALL_COUNT);
		sprintf(s, "%d", pots_call_count.outgoing[ct]);
		rdb_set_single(db_str, s);
	} else {
		pots_call_count.incoming[ct]++;
		sprintf(db_str, "%s.%s.%s", RDB_STATISTIC_USAGE, call_type_name[ct], INCOMING_CALL_COUNT);
		sprintf(s, "%d", pots_call_count.incoming[ct]);
		rdb_set_single(db_str, s);
	}
	(void)memset(db_str, 0x00, RDB_VALUE_SIZE_SMALL);
	(void)memset(s, 0x00, RDB_VALUE_SIZE_SMALL);
	pots_call_count.total[ct]++;
	sprintf(db_str, "%s.%s.%s", RDB_STATISTIC_USAGE, call_type_name[ct], TATAL_CALL_COUNT);
	sprintf(s, "%d", pots_call_count.total[ct]);
	rdb_set_single(db_str, s);
	SYSLOG_DEBUG("call cnt updated: out[%s] %d, in[%s] %d, total[%s] %d",
			call_type_name[ct], pots_call_count.outgoing[ct],
			call_type_name[ct], pots_call_count.incoming[ct],
			call_type_name[ct], pots_call_count.total[ct]);
}

/* special emergency dialing feature for Rail Corps. */
#ifdef AUTOMATED_EMERGENCY_DIALING
#define RDB_AUTODIAL_ENABLE		"autodial.enable"
#define RDB_AUTODIAL_DT_DURA	"autodial.dialtone_duration"
#define RDB_AUTODIAL_DIAL_STR	"autodial.dial_string"
#define AUTODIAL_MAX_DIALTONE_DURATON	10
int read_autodial_variables_from_db(void)
{
#if defined(PLATFORM_PLATYPUS)
	int result;
	char* db_str;
#else
	char s[DIGITS_SIZE];
#endif
	int ret_val = 0;

	autodial_enabled = FALSE;
#if defined(PLATFORM_PLATYPUS)
	nvram_init(RT2860_NVRAM);
	db_str = nvram_get(RT2860_NVRAM, RDB_AUTODIAL_ENABLE);
	if (!db_str || !strlen(db_str))
	{
		SYSLOG_ERR( "failed to read '%s'", RDB_AUTODIAL_ENABLE );
		nvram_strfree(db_str);
		result = nvram_bufset(RT2860_NVRAM, RDB_AUTODIAL_ENABLE, "0");
		if (result < 0)
		{
			SYSLOG_ERR("write NV item failure: %s : %s", RDB_AUTODIAL_ENABLE, "0");
			goto nv_close_return;
		}
		/* create other variables as well */
		result = nvram_bufset(RT2860_NVRAM, RDB_AUTODIAL_DT_DURA, "1");
		result = nvram_bufset(RT2860_NVRAM, RDB_AUTODIAL_DIAL_STR, "1");
		result = nvram_commit(RT2860_NVRAM);
		if (result < 0)
			SYSLOG_ERR("commit NV items failure");
		goto nv_close_return;
	}
	else if (strcmp(db_str, "1"))
	{
		goto nv_close_return;
	}
	autodial_enabled = TRUE;

	db_str = nvram_get(RT2860_NVRAM, RDB_AUTODIAL_DT_DURA);
	if (!db_str || !strlen(db_str))
	{
		SYSLOG_ERR( "failed to read '%s'", RDB_AUTODIAL_DT_DURA );
		nvram_strfree(db_str);
		result = nvram_bufset(RT2860_NVRAM, RDB_AUTODIAL_DT_DURA, "1");
		if (result < 0)
		{
			SYSLOG_ERR("write NV item failure: %s : %s", RDB_AUTODIAL_DT_DURA, "1");
			goto nv_close_err_return;
		}
		result = nvram_commit(RT2860_NVRAM);
		if (result < 0)
			SYSLOG_ERR("commit NV items failure");
		goto nv_close_err_return;
	}
	else
	{
		autodial_dialtone_duration = atoi(db_str);
		if (autodial_dialtone_duration > AUTODIAL_MAX_DIALTONE_DURATON) {
			SYSLOG_ERR("auto dial duration is too large, set to max %d seconds", AUTODIAL_MAX_DIALTONE_DURATON);
			autodial_dialtone_duration = AUTODIAL_MAX_DIALTONE_DURATON;
		}
	}

	db_str = nvram_get(RT2860_NVRAM, RDB_AUTODIAL_DIAL_STR);
	if (!db_str || !strlen(db_str))
	{
		SYSLOG_ERR( "failed to read '%s'", RDB_AUTODIAL_DIAL_STR );
		nvram_strfree(db_str);
		result = nvram_bufset(RT2860_NVRAM, RDB_AUTODIAL_DIAL_STR, "1");
		if (result < 0)
		{
			SYSLOG_ERR("write NV item failure: %s : %s", RDB_AUTODIAL_DIAL_STR, "");
			goto nv_close_err_return;
		}
		result = nvram_commit(RT2860_NVRAM);
		if (result < 0)
			SYSLOG_ERR("commit NV items failure");
		goto nv_close_err_return;
	}
	else if (strlen(db_str) < DIGITS_SIZE)
	{
		strcpy(autodial_dial_string, db_str);
		return 0;
	}
	goto nv_close_err_return;
#else
	if (rdb_get_single(RDB_AUTODIAL_ENABLE, s, DIGITS_SIZE) != 0)
	{
		SYSLOG_ERR( "failed to read '%s'", RDB_AUTODIAL_ENABLE );
		if(rdb_update_single(RDB_AUTODIAL_ENABLE, "0", CREATE | PERSIST, ALL_PERM, 0, 0)<0)
			SYSLOG_ERR("failed to create '%s'", RDB_AUTODIAL_ENABLE);
		else
			SYSLOG_DEBUG("rdb var '%s' created", RDB_AUTODIAL_ENABLE);
		/* create other variables as well */
		(void) rdb_update_single(RDB_AUTODIAL_DT_DURA, "1", CREATE | PERSIST, ALL_PERM, 0, 0);
		SYSLOG_DEBUG("rdb var '%s' created", RDB_AUTODIAL_DT_DURA);
		(void) rdb_update_single(RDB_AUTODIAL_DIAL_STR, "", CREATE | PERSIST, ALL_PERM, 0, 0);
		SYSLOG_DEBUG("rdb var '%s' created", RDB_AUTODIAL_DIAL_STR);
		goto result_return;
	}
	else if (strcmp(s, "1"))
	{
		goto result_return;
	}
	autodial_enabled = TRUE;

	if (rdb_get_single(RDB_AUTODIAL_DT_DURA, s, DIGITS_SIZE) != 0)
	{
		SYSLOG_ERR( "failed to read '%s'", RDB_AUTODIAL_DT_DURA );
		if(rdb_update_single(RDB_AUTODIAL_DT_DURA, "1", CREATE | PERSIST, ALL_PERM, 0, 0)<0)
			SYSLOG_ERR("failed to create '%s'", RDB_AUTODIAL_DT_DURA);
		else
			SYSLOG_DEBUG("rdb var '%s' created", RDB_AUTODIAL_DT_DURA);
		goto rdb_error_return;
	}
	else if (!strlen(s))
	{
		goto rdb_error_return;
	}
	else
	{
		autodial_dialtone_duration = atoi(s);
		if (autodial_dialtone_duration > AUTODIAL_MAX_DIALTONE_DURATON) {
			SYSLOG_ERR("auto dial duration is too large, set to max %d seconds", AUTODIAL_MAX_DIALTONE_DURATON);
			autodial_dialtone_duration = AUTODIAL_MAX_DIALTONE_DURATON;
		}
	}

	if (rdb_get_single(RDB_AUTODIAL_DIAL_STR, s, DIGITS_SIZE) != 0)
	{
		SYSLOG_ERR( "failed to read '%s'", RDB_AUTODIAL_DIAL_STR );
		if(rdb_update_single(RDB_AUTODIAL_DIAL_STR, "", CREATE | PERSIST, ALL_PERM, 0, 0)<0)
			SYSLOG_ERR("failed to create '%s'", RDB_AUTODIAL_DIAL_STR);
		else
			SYSLOG_DEBUG("rdb var '%s' created", RDB_AUTODIAL_DIAL_STR);
		goto rdb_error_return;
	}
	else if (strlen(s) > 0 && strlen(s) < DIGITS_SIZE)
	{
		strcpy(autodial_dial_string, s);
		goto result_return;
	}
#endif

#if defined(PLATFORM_PLATYPUS)
nv_close_err_return:
#else
rdb_error_return:
#endif
	ret_val = -1;
#if defined(PLATFORM_PLATYPUS)
nv_close_return:
	nvram_close(RT2860_NVRAM);
#else
result_return:
#endif

	if (ret_val == 0 && autodial_enabled)
		SYSLOG_DEBUG("autodial is enabled. autodial will commence after dialtone for %d seconds", autodial_dialtone_duration);
	else
		SYSLOG_DEBUG("autodial is disabled or variables are not valid. return to normal operation.");
	return ret_val;
}
#endif

typedef enum { flags_subscribed = SUBSCRIBED, flags_triggered = TRIGGERED } flags_enum;
enum { triggered_max_size = 2048 }; // TODO: quick and dirty
int triggered_size;
char triggered_names[ triggered_max_size ];
static int get_triggered(flags_enum flags)
{
	switch (flags)
	{
		case flags_subscribed: // workaround, as SUBSCRIBED flag is not implemented in RDB yet.
			return 0;
		case flags_triggered: // quite wasteful, improve it later
		{
			triggered_size=sizeof(triggered_names);
			if (rdb_get_names("", triggered_names, &triggered_size, flags) < 0)
			{
				SYSLOG_ERR("failed to get triggered variables '%s'", triggered_names);
				return -1;
			}
			triggered_names[triggered_size] = '\0';
			//SYSLOG_DEBUG( "--> %d bytes: triggered_names='%s'", triggered_size, triggered_names );
			return 0;
		}
	}
	return 0; // never here
}

static int triggered(const char* name, flags_enum flags)
{
	switch (flags)
	{
		case flags_subscribed: // workaround, as SUBSCRIBED flag is not implemented in RDB yet.
			return 1;
		case flags_triggered: // quite wasteful, improve it later
		{
			char* it;
			char* end;
			if (triggered_names[0] == 0)
			{
				return 0;
			}
			end = triggered_names + triggered_size + 1;
			for (it = triggered_names; it < end; ++it)
			{
				if (*it == '&')
				{
					*it = '\0';
				}
			}
			for (it = triggered_names; it < end; it += (strlen(it)) + 1)
			{
				//SYSLOG_DEBUG( "name '%s', triged_names='%s'", name, it );
				if (strcmp(name, it) == 0)
				{ /*SYSLOG_DEBUG( "'%s' triggered", name );*/
					return 1;
				}
			}
			return 0;
		}
	}
	return 0; // never here
}

static void handle_triggered_variables(flags_enum flags)
{
	const rdb_handler_type* pRdbHandler=rdb_handler;
	int i;
	const char* var_p;
	char s[RDB_VALUE_SIZE_LARGE];

	//SYSLOG_DEBUG("%s", (flags==flags_subscribed)? "subscribed":"triggered");
	while(pRdbHandler->szPrefix) {
		if (pRdbHandler->subscribe == 0)
		{
			pRdbHandler++;
			continue;
		}

		for (i = 0;i<call_type_cnt[pRdbHandler->call_type]; i++)
		{
			/* for single instance variable, check once */
			if (pRdbHandler->instance == 0 && i != 0)
			{
				continue;
			}
			sprintf(s, "%d", i);
			if (pRdbHandler->instance)
			{
				var_p = rdb_variable(pRdbHandler->szPrefix, s, pRdbHandler->szName);
			}
			else
			{
				var_p = rdb_variable(pRdbHandler->szPrefix, "", pRdbHandler->szName);
			}
			if (triggered(var_p, flags))
			{
				if (pRdbHandler->p_rdb_event_handler(var_p))
				{
					pRdbHandler->p_rdb_event_handler(var_p);
				}
			}
		}
		pRdbHandler++;
	}
}

static int get_triggered_variables(flags_enum flags)
{
	int result = 0, i;
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		pots_bridge[i].call.event[0] = pots_bridge[i].ss.status[0] = 0;
	}
	rdb_database_lock(0);
	if ((result = get_triggered(flags)) == 0)
	{
		handle_triggered_variables(flags);
	}
	rdb_database_unlock();
	return result;
}

static int handle_rdb_events(flags_enum flags)
{
	int i;
	if (get_triggered_variables(flags) != 0)
	{
		return -1;
	}
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		/* do not process if call type is not assigned yet */
		if (slic_info[i].call_type == NONE)
		{
			continue;
		}
		//SYSLOG_ERR("idx [%d][%s] calls event: '%s'", i, ct_str(i), pots_bridge[i].call.event);
		if (pots_bridge[i].call.event[0] != '\0' && strlen(pots_bridge[i].call.event) > 0)
		{
			handle_pots_event(i, event_call_state_changed);
			pots_bridge[i].call.event[0] = '\0';
		}
		if (ss_request_pending(i) && pots_bridge[i].ss.status[0] != 0)
		{
			handle_pots_event(i, event_ss_response);
		}
		if (pots_bridge[i].vmwi.cmd != EMPTY && pots_bridge[i].vmwi.status == IDLE)
		{
			handle_pots_event(i, event_call_state_changed);
		}
	}
	return 0;
}

#ifdef USE_ALSA
static void poll_rdb(void)
{
	unsigned int i;
	for (i = 0; i < MAX_RDB_EVENTS_PER_SECOND; ++i)
	{
		if (rdb_poll_events(0, NULL, NULL) <= 0)
		{
			return;
		}
		handle_rdb_events(flags_triggered);
	}
}
#endif

static void toggle_slic_debug()
{
	static int slic_debug_mode = 0;

	if (slic_debug_mode)
	{
		slic_debug_mode=0;
		SYSLOG_ERR("slic debug msg disabled");
	}
	else
	{
		slic_debug_mode=1;
		SYSLOG_ERR("slic debug msg enabled");
	}
	(void) slic_toggle_debug_mode(slic_info[0].slic_fd, slic_debug_mode);
}

static void toggle_slic_pcm_clock()
{
	static int slic_pcm_clk_src = 0;

	if (slic_pcm_clk_src)
	{
		slic_pcm_clk_src=0;
	}
	else
	{
		slic_pcm_clk_src=1;
	}
	slic_change_pcm_clk_source(slic_info[0].slic_fd, slic_pcm_clk_src);
}

static void sig_handler(int signum)
{
	/* The rdb_library and driver have a bug that means that subscribing to
	variables always enables notification via SIGHUP. So we don't whinge
	on SIGHUP. */
	if (signum != SIGHUP)
		SYSLOG_INFO("caught signal %d", signum);

	switch (signum)
	{
		case SIGHUP:
			//rdb_import_config (config_file_name, TRUE);
			break;

		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			pots_bridge_running = 0;
			break;

		case SIGUSR1:
			toggle_slic_debug();
			break;
		case SIGUSR2:
			toggle_slic_pcm_clock();
			break;
	}
}

static void ensure_singleton(void)
{
	const char* lockfile = "/var/lock/subsys/"APPLICATION_NAME;
	if (open(lockfile, O_RDWR | O_CREAT | O_EXCL, 0640) >= 0)
	{
		SYSLOG_INFO("got lock on %s", lockfile);
		return;
	}
	SYSLOG_ERR("another instance of %s already running (because creating lock file %s failed: %s)", APPLICATION_NAME, lockfile, strerror(errno));
	exit(EXIT_FAILURE);
}

static void release_singleton(void)
{
	unlink("/var/lock/subsys/"APPLICATION_NAME);
}

static BOOL check_modem(void)
{
	char model[1024] = {0, };
	if (rdb_get_single(RDB_MODEM_NAME, model, sizeof(model)) < 0)
		model[0] = 0;
	SYSLOG_DEBUG("model_name=%s", model);

	if (!strcmp(model, "") || model[0] == 0)
		return FALSE;
		/* Huawei EM820U does not output PCM clock */
		if (strcmp(model, "EM820U") == 0)
				return FALSE;
	return TRUE;
}

const char* ext_xtal_board_list[] = {
		/* Platypus board */
		"3g38wv", "3g38wv2", "3g36wv", "3g39wv", "3g46",
		/* Platypus2 board */
		"3g22wv", "4g100w",
		/* Bovine board */
		"3g30ap", "3g30apo", "elaine",
		""
};
static BOOL check_board_has_external_pcm_crystal(void)
{
	char board[1024] = { 0, };
	int i;
	if (rdb_get_single(RDB_BOARD_NAME, board, sizeof(board)) < 0)
		board[0] = 0;
	SYSLOG_DEBUG("board_name=%s", board);

	for (i = 0; strlen(ext_xtal_board_list[i]); i++) {
		if (!strcmp(board, ext_xtal_board_list[i]))
			return TRUE;
	}
	return FALSE;
}

static void create_rdb_variables(void)
{
	const rdb_handler_type* pRdbHandler = rdb_handler;
	int i;
	const char* var_p;
	char s[RDB_VALUE_SIZE_SMALL];

	while (pRdbHandler->szPrefix) {

		for (i = 0;i<call_type_cnt[pRdbHandler->call_type]; i++)
		{
			/* for single instance variable, create once */
			if (pRdbHandler->instance == 0 && i != 0)
			{
				continue;
			}
			if (pRdbHandler->instance)
			{
				sprintf(s, "%d", i);
				var_p = rdb_variable(pRdbHandler->szPrefix, s, pRdbHandler->szName);
			}
			else
			{
				var_p = rdb_variable(pRdbHandler->szPrefix, "", pRdbHandler->szName);
			}
			if (rdb_get_single(var_p, s, RDB_VALUE_SIZE_SMALL) != 0)
			{
				if(rdb_create_variable(var_p, "", CREATE, ALL_PERM, 0, 0)<0)
				{
					SYSLOG_ERR("failed to create '%s'", var_p);

				}
				SYSLOG_DEBUG("rdb var '%s' created", var_p);
			}
			else
			{
				SYSLOG_DEBUG("rdb var '%s' already exists", var_p);
			}
		}
		pRdbHandler++;
	}
}

static void subscribe_rdb_variables(void)
{
	const rdb_handler_type* pRdbHandler=rdb_handler;
	int i;
	const char* var_p;
	char s[RDB_VALUE_SIZE_SMALL];

	while(pRdbHandler->szPrefix)
	{
		if(pRdbHandler->subscribe)
		{
			for (i = 0;i<call_type_cnt[pRdbHandler->call_type]; i++)
			{
				/* for single instance variable, create once */
				if (pRdbHandler->instance == 0 && i != 0)
				{
					continue;
				}
				if (pRdbHandler->instance)
				{
					sprintf(s, "%d", i);
					var_p = rdb_variable(pRdbHandler->szPrefix, s, pRdbHandler->szName);
				}
				else
				{
					var_p = rdb_variable(pRdbHandler->szPrefix, "", pRdbHandler->szName);
				}

				SYSLOG_DEBUG("subscribing rdb var %s",var_p);
				if (rdb_subscribe_variable(var_p) != 0)
				{
					SYSLOG_ERR("failed to subscribe to '%s'!", var_p);
				}
			}
		}
		pRdbHandler++;
	}
	(void) handle_rdb_events(flags_subscribed);
}

static BOOL initialize_rdb_variables( const char* instance )
{
	sprintf(pots_bridge_rdb_prefix, "wwan.%s", instance);

	if (rdb_open_db() <= 0)
	{
		SYSLOG_ERR("failed to open database!");
		return FALSE;
	}

	/* save pots_bridge instance number to share rdb variable with voip client */
	if (rdb_update_single(RDB_POTS_INSTANCE, instance, CREATE, ALL_PERM, 0, 0) != 0)
	{
		SYSLOG_ERR("failed to create 'pots.instance' (%s)", strerror(errno));
		return FALSE;
	}

	/* read call service type (3G Voice, VOIP, FOIP) setting from NV or RDB */
	read_call_type_from_db();

	/* read outgoing/incoming/total call counts from  NV or RDB */
	read_call_counts_from_db();

	create_rdb_variables();
	subscribe_rdb_variables();

	/* clear old call event */
	rdb_set_single(rdb_variable(RDB_UMTS_CALLS_PREFIX, "", RDB_CALLS_EVENT), "");

	/* clear old command status */
	rdb_set_single(rdb_variable(RDB_UMTS_CMD_PREFIX, "", RDB_CMD_STATUS), "");

	/* out band DTMF sending */
	read_mbdn_msisdn_from_rdb();

	/* special emergency dialing feature for Rail Corps. */
	#ifdef AUTOMATED_EMERGENCY_DIALING
	read_autodial_variables_from_db();
	#endif

	/* initialize service provider profile */
	initialize_sp();

	/* initialize profile & ISP list variables */
	initialize_profile_variables();

	return TRUE;
}

/*
* Calibration is done with SLIC clock and clock source is always modem.
*/
#define USE_PHONE_MODULE_CLK_FOR_CAL
extern int check_slic_event_during_rdb_command_processing;
static void init_pots_bridge(const char* device[], const char* instance)
{
	int i;
	struct slic_t *slic_p;
/* slic cal data save/restore feature */
#if defined(PROSLIC_SI322X) || defined(PROSLIC_SI3217X)
	int cal_retry_cnt = 0;
#endif
	char temp[10];
	BOOL force_cal = FALSE;
	BOOL is_cal_data_valid;

	ensure_singleton();

	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);

	if (!initialize_rdb_variables(instance))
		goto cleanup;

	SYSLOG_INFO("initializing SLIC...");
	initialize_slic_variables();
	for (i = 0; i < MAX_CHANNEL_NO && device[i] != 0x00 && strcmp(device[i], "") != 0; i++)
	{
		slic_p = (struct slic_t *) & slic_info[i];
		slic_p->slic_fd = slic_open(device[i]);
		if (slic_p->slic_fd < 0)
		{
			SYSLOG_ERR("failed to initialize SLIC!");
			goto cleanup;
		}
		if (slic_get_channel_id(slic_p->slic_fd, &slic_p->cid) < 0)
		{
			fprintf(stderr, "failed to get cid\n");
			goto cleanup;
		}
		strncpy((char *) &slic_p->dev_name[0], (const char *) device[i], strlen(device[i]));
		assign_slic_channel_to_call_fuction(i, slic_p);
		assign_slic_channel_to_rdb_variables(i, slic_p);
		if (slic_register_event_handler(slic_event_dtmf, handle_pots_event_dtmf) != 0)
		{
			SYSLOG_ERR("failed to register DTMF event handler!");
			goto cleanup;
		}
		if (slic_register_event_handler(slic_event_dtmf_complete, handle_pots_event_dtmf_complete) != 0)
		{
			SYSLOG_ERR("failed to register DTMF COMPLETE event handler!");
			goto cleanup;
		}
		if (slic_register_event_handler(slic_event_fsk_complete, handle_pots_event_fsk_complete) != 0)
		{
			SYSLOG_ERR("failed to register FSK COMPLETE event handler!");
			goto cleanup;
		}
		/* support international telephony profile
		* Setting telephony profile should be done before calling slic_initialize_device_hw() below
		* because before mode_set, tone spec. should be defined. */
		if (slic_set_tel_profile(slic_p->slic_fd, tel_profile) < 0)
		{
			fprintf(stderr, "failed to set telephony profile\n");
		}
		/* end of support international telephony profile */
		// send fsk first then wait for fsk complete event, then change to distinctive mode
		pots_bridge[i].wait_for_fsk_complete = FALSE;
		if( slic_register_event_handler( slic_event_vmwi_complete, handle_pots_event_vmwi_complete ) != 0 )
		{
			SYSLOG_ERR( "failed to register VMWI COMPLETE event handler!" );
			goto cleanup;
		}
		/* out band DTMF sending */
		pots_bridge[i].send_outband_dmtf = FALSE;

		pots_bridge[i].waiting_for_recall_dialing = FALSE;
		pots_bridge[i].detect_3way_calling_prefix = FALSE;
		pots_bridge[i].recall_key_count = 0;
		pots_bridge[i].dtmf_key_blocked = FALSE;

	}
	display_slic_info_db();

	pots_bridge_running = 1;

#ifdef USE_PHONE_MODULE_CLK_FOR_CAL
	/* -----------------------------------------------------------------------------------
	* check phone module and change default PCM clock source
	* - external crystal exists : change to 'loop' for all mode except voice call
	* - 3G module clock using : change to 'loop' for calibration and factory test only
	* 	 + w/o  3G modem : change to 'loop' for calibration and factory test
	*   + with 3G modem : change to 'voice' mode for normal operation
	* -----------------------------------------------------------------------------------
	*/
	is_modem_exist = check_modem();
	is_external_crystal_exist = check_board_has_external_pcm_crystal();

	SYSLOG_INFO("is_modem_exist = %d, is_external_crystal_exist %d", is_modem_exist, is_external_crystal_exist);
	if (is_external_crystal_exist || !is_modem_exist) {
		slic_change_cpld_mode(CPLD_MODE_LOOP);
		SYSLOG_DEBUG("set default CPLD mode to loop");
		if (!is_external_crystal_exist && !is_modem_exist)
			force_cal = TRUE;
	} else {
		slic_change_cpld_mode(CPLD_MODE_VOICE);
		SYSLOG_DEBUG("set default CPLD mode to voice");
	}
	if (is_modem_exist && init_phone() != 0)
	{
		SYSLOG_ERR("failed to configure phone module!");
	}
#endif

/* slic cal data save/restore feature */
#if defined(PROSLIC_SI322X) || defined(PROSLIC_SI3217X)
	SYSLOG_INFO("Check SLIC Cal data...");
	init_slic_cal_data_variable();
	//print_cal_data( &slic_cal_data );
cal_retry:
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (slic_initialize_device_hw(slic_info[i].slic_fd, &slic_cal_data, is_slic_calibrated(force_cal)) < 0)
		{
			goto cleanup;
		}
		else
		{
			break;	/* need one time */
		}
	}

	is_cal_data_valid = validate_slic_cal_data();
	if (!is_cal_data_valid && is_modem_exist)
	{
		cal_retry_cnt++;
		SYSLOG_INFO("SLIC Cal retry count = %d", cal_retry_cnt);
		if (cal_retry_cnt > 2)
		{
			SYSLOG_INFO("SLIC Cal retry count exceed 5, give up calibration");
			goto cleanup;
		}
		goto cal_retry;
	}

	SYSLOG_INFO("Finish SLIC Cal. completely: calibrated = %d", slic_cal_data.calibrated);

	if (slic_cal_data.calibrated == 0)
	{
		/* if cal data is not valid, do not mark slic_calibrated to 1 for next calibration. */
		force_cal = !is_cal_data_valid;
		if (!write_slic_cal_data_to_NV(force_cal))
		{
			goto cleanup;
		}
	}
	//print_cal_data( &slic_cal_data );
#endif
/* end of slic cal data save/restore feature */

	SYSLOG_INFO("SLIC initialized");

	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (call_control_init(i, &pots_bridge[i].call.control) != 0)
			SYSLOG_ERR("failed to initialize call control!");

		clear_ss_request(i);
		//clear_digits( &pots_bridge[i].number );
		if (dial_plan_init(i) != 0)
		{
			SYSLOG_ERR("failed to initialize dial plan!");
			goto cleanup;
		}
		if (init_loop_state(i) != 0)
		{
			SYSLOG_ERR("failed to initialize loop state!");
			goto cleanup;
		}
		/* Bell Canada VMWI feature */
		init_vmwi(i);
		/* end of Bell Canada VMWI feature */
		pots_bridge[i].led_state = led_off;
	}

	/* clear old dtmf key command */
	clear_dtmf_key_variables();

	SYSLOG_DEBUG("initialized!\n");

	/* This variable is used to prevent slic calibration failure caused by PPP up.
	* When pots_bridge sets this, booting script starts 3G connection. */
	if (rdb_get_single("pots.status", temp, 10) != 0)
	{
		if (rdb_update_single("pots.status", "pots_ready", CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to update %s to pots_ready", rdb_variable("status", "", ""));
			goto cleanup;
		}
	}
	else
	{
		rdb_set_single("pots.status", "pots_ready");

	}

	/* until this point, we should conventional rdb command blocking method to prevent kernel panic.
	* not enough time to find out the reason of kernel panic at the moment. */
	check_slic_event_during_rdb_command_processing = 1;

	pots_initialized = TRUE;

	return;
cleanup:
	release_singleton();
	rdb_set_single("pots.status", "pots_closed");
	exit(1);
}

static void shutdown_pots_bridge()
{
	int i;
	SYSLOG_DEBUG("pots_bridge: shutting down...\n");
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		handle_pots_event(i, event_shutdown);
		slic_close(slic_info[i].slic_fd);
		dial_plan_release(i);
	}
	if (is_external_crystal_exist)
		slic_change_cpld_mode(CPLD_MODE_LOOP);
	else
		slic_change_cpld_mode(CPLD_MODE_VOICE);
	rdb_close_db();
	release_singleton();
	rdb_set_single("pots.status", "pots_closed");
}

/* support international telephony profile */
const char shortopts[] = "dvVi:t:h?";
/* end of support international telephony profile */

static void usage(char **argv)
{
	fprintf(stderr, "\nUsage: %s [options] [device]\n", argv[0]);
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-d don't detach from controlling terminal (don't daemonise)\n");
	fprintf(stderr, "\t-i instance number\n");
	/* support international telephony profile */
	fprintf(stderr, "\t-t telephony profile\n");
	fprintf(stderr, "\t   0: default(Australia, New Zealand), 1: Canada, 2: North America\n");
	/* end of support international telephony profile */
	fprintf(stderr, "\t-v increase verbosity\n");
	fprintf(stderr, "\t-V display version information\n");
	fprintf(stderr, "\n");
}

int main(int argc, char **argv)
{
	const char *device[MAX_CHANNEL_NO];
	int i;
	int	ret = 0;
	int	verbosity = 0;
	int	be_daemon = 1;
	const char* instance = "0";

	// parse command line options
	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 'd':
				be_daemon = 0;
				break;
			case 'v':
				verbosity++ ;
				break;
			case 'V':
				fprintf(stderr, "%s Version %d.%d.%d\n", argv[0], VER_MJ, VER_MN, VER_BLD);
				break;
			case 'i':
				instance = optarg;
				break;
				/* support international telephony profile */
			case 't':
				tel_profile = (tel_profile_type)atoi(optarg);
				if (tel_profile >= MAX_PROFILE)
				{
					fprintf(stderr, "tel profile (%d) is out of range, use default\n", tel_profile );
					tel_profile = DEFAULT_PROFILE;
				}
				break;
				/* end of support international telephony profile */
			case 'h':
			case '?':
				usage(argv);
				return 2;
		}
	}
#ifdef MULTI_CHANNEL_SLIC
	for (i = 0; i < MAX_CHANNEL_NO; i++) device[i] = optind == argc ? "" : argv[optind++];
#else
	device[0] = optind == argc ? "/dev/slic0" : argv[optind];
#endif

	// Initialize the logging interface
	openlog(APPLICATION_NAME, LOG_PID | (be_daemon ? 0 : LOG_PERROR), LOG_LOCAL5);
	setlogmask(LOG_UPTO(verbosity + LOG_ERR));
	SYSLOG_INFO("starting");

	if (be_daemon)
	{
		daemonize("/var/lock/subsys/" APPLICATION_NAME, RUN_AS_USER);
	}

	init_pots_bridge(device, instance);
	display_pots_setting(device, instance, FALSE);
	while (pots_bridge_running)
	{
#ifdef USE_ALSA
		poll_rdb(); // non-blocking polling
		slic_handle_events(0);   // polling with timeout
#else
		fd_set fdr;
		int selected;
		int rdb_fd = rdb_get_fd();
		int nfds, max_slic_fd = 0;
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
		FD_ZERO(&fdr);
		for (i = 0; i < MAX_CHANNEL_NO; i++)
		{
			if (slic_info[i].slic_fd >= max_slic_fd) max_slic_fd = slic_info[i].slic_fd;
			if (slic_info[i].slic_fd >= 0) FD_SET(slic_info[i].slic_fd, &fdr);
		}
		FD_SET(rdb_fd, &fdr);
		nfds = 1 + (max_slic_fd > rdb_fd ? max_slic_fd : rdb_fd);
		selected = select(nfds, &fdr, NULL, NULL, &timeout);
		if (!pots_bridge_running)
		{
			break;
		}
		if (selected < 0)
		{
			SYSLOG_ERR("select() failed with error %d (%s); exit", selected, strerror(errno));
			//break;
			continue;
		}
		if (selected > 0)
		{
			for (i = 0; i < MAX_CHANNEL_NO; i++)
			{
				if (slic_info[i].slic_fd >= 0 && FD_ISSET(slic_info[i].slic_fd, &fdr))
				{
					SYSLOG_DEBUG("idx [%d] got SLIC event of slic_fd %d", i, slic_info[i].slic_fd);
					slic_handle_events(i);
				}
			}
			if (FD_ISSET(rdb_fd, &fdr))
			{
				handle_rdb_events(flags_triggered);
			}
		}
#endif
		for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++) handle_pots_bridge_timers(i);
	}
	shutdown_pots_bridge();

	SYSLOG_INFO("exiting");
	closelog();
	exit(0);
}

/*
* vim:ts=4:sw=4:
*/

