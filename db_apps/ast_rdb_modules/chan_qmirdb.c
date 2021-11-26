/*
 * QMIRDB Channel driver for Asterisk PBX
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#define _GNU_SOURCE
#define PJ_AUTOCONF

/* include standard c headers */
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <strings.h>
#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/times.h>

/* include gnu c header */
#include <alsa/asoundlib.h>
#include <alsa/use-case.h>
#include <pjsip.h>
#include <pjsip_ua.h>

/* include asterisk headers */
#include "asterisk.h"

#include "asterisk/res_pjsip.h"
#include "asterisk/res_pjsip_session.h"

#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/format_cache.h"
#include "asterisk/causes.h"
#include "asterisk/lock.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h"
#include "asterisk/res_mwi_external.h"
#include "asterisk/sorcery.h"
#include "asterisk/indications.h"
#include "asterisk/cli.h"
#include "asterisk/linkedlists.h"
#include "asterisk/bridge_channel.h"

/* include netcomm headers */
#include "rdb_ops.h"
#include "dbenum.h"
#include "rwpipe.h"
#include "indexed_rdb.h"
#include "strarg.h"

/* include channel QMI headers */
#include "qmirdbctrl.h"
#include "dbhash.h"


#define __countof(x) (sizeof(x)/sizeof((x)[0]))
#define __noop() do {} while(0)

/* device defines */
#define DEVICE_FRAME_SIZE 48

/* define asterisk mandatory variables */
#define DEVICE_FRAME_FORMAT ast_format_slin16

#define AST_MODULE "chan_qmi"
ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

/* redefine log defines - override incorrect asterisk defines */
#undef LOG_EMERG
#undef LOG_ALERT
#undef LOG_CRIT
#undef LOG_ERR
#undef LOG_WARNING
#undef LOG_NOTICE
#undef LOG_INFO
#undef LOG_DEBUG
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7


#if __BYTE_ORDER == __LITTLE_ENDIAN
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
#else
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_BE;
#endif

/* PCM hardware configuration */

/* 16Khz */
#define DEFAULT_SAMPLING_FREQUENCY 16000
/* receive frame size - 20ms */
#define DEFAULT_RECEIVE_FRAME_SIZE 20
/* playback & record period frame size, playback startup threadhold - 20 ms */
#define DEFAULT_PERIOD_FRAME_SIZE 20
/* playback buffer frame size - 200 ms */
#define DEFAULT_PLAYBACK_BUFFER_SIZE 20

#define TO_FRAME_SIZE(freq,ms) (((ms)*(freq)+1000-1)/1000)

#define RDB_MWI_VOICEMAIL_ACTIVE "mwi.voicemail.active"
#define RDB_MWI_VOICEMAIL_COUNT "mwi.voicemail.count"

/* increase debug level */
//#define VERBOSE_DEBUG_RECORD
//#define VERBOSE_DEBUG_PLAYBACK
//#define VERBOSE_DEBUG_MUTEX
//#define VERBOSE_DEBUG_SIP_INFO

/* use followings for debug purpose only */
//#define CONFIG_NOAUDIOCAPTURE
//#define CONFIG_USE_INCALL_PCM
//#define CONFIG_USE_INBOUND_DTMF
//#define CONFIG_IGNORE_DTMF
//#define CONFIG_USE_ASTERISK_AS_HOOKFLASH
//#define CONFIG_USE_IP_CALLS
#define CONFIG_MANUALLY_START_PCM_DEVICES
#define CONFIG_CONVERT_INT_PREFIX_INTO_PLUS
#define CONFIG_LINK_RX_AND_TX

/* use following by default */
#define CONFIG_USE_INBOUND_TONE
#define CONFIG_USE_EXTRA_CB
#define CONFIG_USE_INBAND_RINGBACK
#define CONFIG_USE_INCALL_TONE
//#define CONFIG_USE_STATISTICS

#define DTMF_DIGIT_END '#'

#define DTMF_DIGIT_CW_START 0
#define DTMF_DIGIT_CW_STOP 1

/* voice mail */
#define VOICEMAIL_DIALNO "VOICEMAIL"
#define VOICEMAIL_RDB    "wwan.0.sim.data.mbdn"


#define DTMF_DIGIT_HOOKFLASH '*'

#ifdef CONFIG_USE_ASTERISK_AS_HOOKFLASH
#warning ASTERISK(*) is used instead of hook-flash
#endif

#ifdef CONFIG_USE_INCALL_PCM
#warning IN-CALl PCM is in use instead of host PCM device
#endif

#ifdef CONFIG_USE_INBOUND_DTMF
#warning INBOUND DTMF is in use instead of outbound DTMF
#endif

#ifdef CONFIG_IGNORE_DTMF
#warning DTMF is being ignored.
#endif

#ifdef CONFIG_USE_STATISTICS
#define RDB_VOICE_CALL_STATISTICS_RESET "voice_call.statistics.reset"
#endif

/* alsa variables */


#ifdef CONFIG_NOAUDIOCAPTURE
/* nothing to do */
#else
static int mute = 0;
#endif

/* main alsa variables */
char* main_icard_dev_name = NULL;
char* main_ocard_dev_name = NULL;

static int main_readdev = 0;
static int main_writedev = 0;

static snd_pcm_t* main_icard = NULL;
static snd_pcm_t* main_ocard = NULL;

int main_sampling_frequency = DEFAULT_SAMPLING_FREQUENCY;
int main_receive_frame_size = TO_FRAME_SIZE(DEFAULT_SAMPLING_FREQUENCY, DEFAULT_RECEIVE_FRAME_SIZE);
int main_receive_frame_period = DEFAULT_RECEIVE_FRAME_SIZE;
int main_period_frame_size = TO_FRAME_SIZE(DEFAULT_SAMPLING_FREQUENCY, DEFAULT_PERIOD_FRAME_SIZE);
int main_playback_buffer_size = TO_FRAME_SIZE(DEFAULT_SAMPLING_FREQUENCY, DEFAULT_PLAYBACK_BUFFER_SIZE);

/* ua register flag */
int ua_registered = 0;


/* defines */
struct dev_t;
struct call_t;
enum incall_stat_t;

/* const information for channel */
static const char tdesc[] = "Netcomm QMI Device Channel Driver";
static const char config[] = "chan_qmirdb.conf";

/* local functions - channel technology */
static struct ast_channel *_ast_request(const char *type, struct ast_format_cap *cap, const struct ast_assigned_ids *assignedids, const struct ast_channel *requestor, const char *data, int *cause);
static int _ast_hangup(struct ast_channel *ast);
static int _ast_answer(struct ast_channel *ast);
static int _ast_call(struct ast_channel *ast, const char *dest, int timeout);
static int _ast_queue_frame(struct dev_t *dev, struct ast_frame* f);
static int _ast_queue_control(struct dev_t *dev, enum ast_control_frame_type control);
static struct ast_frame *_ast_read(struct ast_channel *ast);
static int _ast_write(struct ast_channel *ast, struct ast_frame *frame);
#ifdef CONFIG_USE_EXTRA_CB
static int _ast_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static int _ast_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen);
#endif
static int _ast_digit_begin(struct ast_channel *c, char digit);
static int _ast_digit_end(struct ast_channel *c, char digit, unsigned int duration);

/* info supplement */
static int _ast_info_incoming_request(struct ast_sip_session *session, struct pjsip_rx_data *rdata);

/* local incall functions */
#ifdef CONFIG_USE_INCALL_TONE
static int _ast_playtones(struct ast_channel *chan, const char *data);
static void _ast_stopplaytones(struct ast_channel *chan);
#endif
static int incall_start_mt_call(struct dev_t* dev, struct call_t* call);
static int incall_playtones_in_peer(struct dev_t* dev, const char* data);
static int incall_set_stat(struct dev_t* dev, enum incall_stat_t stat);
static const char* incall_get_stat_str(enum incall_stat_t stat);
static int do_hookflash(struct dev_t* dev);

/* local call functions */
static int cc_make_mo_call(struct dev_t* dev, const char* num, int timeout, int conference);
static struct call_t* cc_add_new_call(struct dev_t* dev, int cid, int incoming, int outgoing, int conference, const char* pi, const char* clip, const char* clip_name);
static void cc_remove_call(struct dev_t* dev, int cid);
static struct call_t* cc_get_call_by_cid(struct dev_t* dev, int cid);
static int cc_is_empty(struct dev_t* dev);
static int cc_get_call_count(struct dev_t* dev);
//static struct call_t* cc_get_last_call(struct dev_t* dev);
//static struct call_t* cc_get_first_incoming_call_to_answer(struct dev_t* dev);
static struct call_t* cc_get_first_call_on_hold(struct dev_t* dev);
#ifndef CONFIG_IGNORE_DTMF
static struct call_t* cc_get_active_dtmf_call(struct dev_t* dev);
#endif
static struct call_t* cc_get_last_answered_or_connected_call(struct dev_t* dev);
static int cc_is_any_call_on_hold(struct dev_t* dev);
static int cc_is_any_emergency_call(struct dev_t* dev);
static int cc_is_any_incomging_call(struct dev_t* dev);
static int cc_is_any_call_ringing(struct dev_t* dev);
static int cc_is_any_call_to_be_answered(struct dev_t* dev);
static struct call_t* cc_get_first_call(struct dev_t* dev);
static int cc_is_conference_existing(struct dev_t* dev);

/* device local function */
static int prepare_to_unload();
static int is_any_call_existing();

void soundcard_stop_and_fini(struct dev_t* dev);
int soundcard_init_and_start(struct dev_t* dev, int reopen);
int soundcard_restart(struct dev_t* dev);


static void *do_rdb_procedure(void *data);

/* rdb functions */
int rdb_answer(struct dev_t* dev, int cid);
int rdb_call(struct dev_t* dev, const char* dest_num, int timeout);
int rdb_ecall(struct dev_t* dev, const char* dest_num, int timeout);
int rdb_reject(struct dev_t* dev, int cid);
int rdb_hangup(struct dev_t* dev, int cid);
int rdb_manage_calls(struct dev_t* dev, const char* sups_type, int cid, const char* reject_cause);
int rdb_start_dtmf(struct dev_t* dev, int cid, char digit);
int rdb_stop_dtmf(struct dev_t* dev, int cid);
int rdb_manage_calls_conference(struct dev_t* dev);
int rdb_manage_calls_local_hold(struct dev_t* dev);
int rdb_manage_calls_local_unhold(struct dev_t* dev);
int rdb_manage_calls_activate(struct dev_t* dev, int cid);
int rdb_manage_calls_resume(struct dev_t* dev, int cid);
int rdb_manage_calls_accept_waiting_or_held(struct dev_t* dev);
int rdb_manage_calls_release(struct dev_t* dev, int cid, const char* reject_cause);
int rdb_manage_calls_end_all_calls(struct dev_t* dev);

static int _rdb_set_str(const char* rdb, const char* val);
#ifdef CONFIG_USE_STATISTICS
static int _rdb_set_int(const char* rdb, unsigned long long val);
#endif
static const char* _rdb_get_str(const char* rdb);
static unsigned long long _rdb_get_int(const char* rdb);


/* cli stuff */

static char *handle_cli_qmirdb_test(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *handle_cli_qmirdb_hookflash(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *handle_cli_qmirdb_hangup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *handle_cli_qmirdb_hangupall(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *handle_cli_qmirdb_show(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);

/* statistic functions */
static int cc_answer_mt_call(struct dev_t* dev);

/* registrar */
static pj_bool_t on_rx_request(struct pjsip_rx_data *rdata);
static pj_status_t on_tx_request(pjsip_tx_data *tdata);

static struct ast_cli_entry qmirdb_cli[] = {
	AST_CLI_DEFINE(handle_cli_qmirdb_hookflash, "perform hook flash"),
	AST_CLI_DEFINE(handle_cli_qmirdb_hangup, "hangup a call"),
	AST_CLI_DEFINE(handle_cli_qmirdb_hangupall, "hangup all calls"),
	AST_CLI_DEFINE(handle_cli_qmirdb_show, "show information"),
	AST_CLI_DEFINE(handle_cli_qmirdb_test, "test for debugging"),
};

enum peer_ringback_t {
	peer_ringback_none = 0,
	peer_ringback_local,
	peer_ringback_remote
};

enum incall_stat_t {
	incall_stat_none = 0,
	incall_stat_dial, /* dial with dial-tone */
	incall_stat_dialing, /* dial without dial-tone - after one digit is pressed */
	incall_stat_ring, /* ring - just start to call */
	incall_stat_ringing, /* ringing - after get alert */
	incall_stat_disconnected,
	incall_stat_waiting,
	incall_stat_busy,
	incall_stat_last,
};

/* qmi channel technology */
static struct ast_channel_tech qmirdb_tech = {
	.type = "QMIRDB",
	.description = tdesc,
	.requester = _ast_request,

	.send_digit_begin = _ast_digit_begin,
	.send_digit_end = _ast_digit_end,

	.hangup = _ast_hangup,
	.answer = _ast_answer,

	.read = _ast_read,
	.call = _ast_call,

	.write = _ast_write,

#ifdef CONFIG_USE_EXTRA_CB
	.indicate = _ast_indicate,
	.fixup = _ast_fixup,
#endif

};

/* qmi info supplement */
static struct ast_sip_session_supplement info_supplement = {
	.method = "INFO",
	.incoming_request = _ast_info_incoming_request,
};

static pjsip_module registrar_module = {
	.name = { "NewRegistrar", sizeof("NewRegistrar") },
	.id = -1,
	.priority = PJSIP_MOD_PRIORITY_DIALOG_USAGE,
	.on_rx_request = on_rx_request,
	.on_tx_request = on_tx_request,
};

/* maximum waiting time in second to unload QMI Asterisk module */
#define MAX_WAITING_TIME_FOR_UNLOADING 10

/* carrier international number + E.164 = 6+15 */
#define MAX_DIAL_DIGIT	32

struct call_t {
	int cid;
	char* clip;

	int clip_name_valid;
	char clip_name[AST_MAX_CONTEXT];

	char* pi;

	/* MO call status */
	int outgoing;
	int ringing;
	int connected;
	int progressed;

	/* MO call type */
	int conference;
	int emergency;

	/* MT call status */
	int incoming;
	int answered;

	int invite; /* invite flag - 1: INVITE already sent to RG, 0: not sent yet */
	int invite_timestamp_valid;	/* validate flag for initial invite */
	unsigned long invite_timestamp;	/* initial invite time stamp */

	int callwaiting; /* call waiting flag - 1: incall waiting already signaled, 0: not signal yet */

	/* MO/MT call status */
	int held;

	time_t StartTime;
	time_t StopTime;

	unsigned long uptime_StartTime;
	unsigned long uptime_StopTime;


	AST_LIST_ENTRY(call_t) entry;
};

/* qmi private info */
struct dev_t {
	char id[AST_MAX_CONTEXT]; /* the 'name' from chan_qmirdb.conf */

	struct ast_channel *owner; /* Channel we belong to, possibly NULL */

	char* rdb_prefix;
	int timeout;

	ast_mutex_t lock;				/*!< dev lock */

	char context[AST_MAX_CONTEXT];			/* the context for incoming calls */

	enum incall_stat_t incall_stat;
	enum peer_ringback_t peer_ringback;

	int hangupcause;

	struct ast_frame fr;

	struct rwpipe_t* pp;

	char* rdb_noti;
	char* rdb_ctrl;

	char* rdb_mwi;
	char* rdb_mwi_count;

	int mwi_valid;
	int mwi_active;
	int mwi_count;

	/* alsa dev names */
	char* icard_dev_name;
	char* ocard_dev_name;

	/* pcm device handle */
	snd_pcm_t *alsa_icard;
	snd_pcm_t *alsa_ocard;
	int readdev;
	int writedev;

	unsigned int sampling_frequency;
	unsigned int receive_frame_size;
	unsigned int receive_frame_period;
	unsigned int period_frame_size;
	unsigned int playback_buffer_size;

	char* pcm_write_buf;
	int pcm_write_buf_size;

	char* pcm_read_buf;
	int pcm_read_buf_size;
	int pcm_read_buf_pos;

	struct ast_frame read_frame;

	int dtmf_timestamp;
	int dtmf_digit_index;
	char dtmf_digits[MAX_DIAL_DIGIT + 1];

	int incall_tone_playing;
	AST_LIST_HEAD(calls, call_t) calls;

	AST_LIST_ENTRY(dev_t) entry;
};

#ifdef CONFIG_USE_STATISTICS
/* call statistics */
struct {

	unsigned long long IncomingCallsReceived;
	unsigned long long IncomingCallsAnswered;
	unsigned long long IncomingCallsConnected;

	unsigned long long OutgoingCallsAttempted;
	unsigned long long OutgoingCallsAnswered;
	unsigned long long OutgoingCallsConnected;

	unsigned long long TotalCallTime;

} statistics;
#endif

static AST_RWLIST_HEAD_STATIC(devices, dev_t);

/* local variables */
struct rdb_session* _s = NULL; /* rdb session handle */
static pthread_t rdb_thread = AST_PTHREADT_NULL;	/* RDB thread */
int rdb_thread_running; /* rdb thread running flag */

/* suppl command defines */

/* suppl command name */
static const char _ast_app_command_suppl_forward[] = "SupplForward";

/* suppl command argument */
AST_DEFINE_APP_ARGS_TYPE(suppl_forward_command_args_t,
                         AST_APP_ARG(dev);
                         AST_APP_ARG(service_type);
                         AST_APP_ARG(service_command);
                         AST_APP_ARG(param1);
                        );

/* suppl service type hash table */
struct dbhash_t* _hash_suppl_forward_service_type = NULL;
static const struct dbhash_element_t _hash_suppl_forward_elements_service_type[] = {
	{ "UNCONDITIONAL", 1},
	{ "NOANSWER", 2},
	{ "BUSY", 3},
	{ "CALLWAITING", 4},
	{ "UNREACHABLE", 5},
};

/* suppl service command hash table */
struct dbhash_t* _hash_suppl_forward_service_command = NULL;
static const struct dbhash_element_t _hash_suppl_forward_elements_service_command[] = {
	{ "REGISTER", 1},
	{ "ACTIVATE", 2},
	{ "DEACTIVATE", 3},
	{ "STATUS", 4},
	{ "ERASE", 5},
};

struct indexed_rdb_t* _ir = NULL;


/* local macros */

#define __STR(vc) indexed_rdb_get_cmd_str(_ir,(vc))


/*

	* RDB MSG call flow

	do_rdb_procedure() --+---> process_rdb_event() ----------------------> on_rdb_noti(dev) or on_rdb_ctrl(dev)
	                     +
	                     +---> process_rdb_event_ctrl() -----------------> on_rdb_ctrl(dev)
	                     +
	                     +---> process_msg_event() ----> on_msg_noti(dev)

	* description

	do_rdb_procedure() : mainly background thread to pump RDB events

		process_rdb_event() : runs when a RDB event arrives - NOTI or CTRL
		process_rdb_event_ctrl() : runs when a MSG-to-write arrives in Write queue

			on_rdb_noti(): read a MSG from NOTI RDB to Read queue
			on_rdb_ctrl(): write a MSG from Write queue to CTRL RDB

		process_msg_event() : runs when a MSG arrives in Read queue

			on_msg_noti(): process MSGs
*/


/*
	peripheral functions
*/

#ifdef VERBOSE_DEBUG_MUTEX

#undef ast_mutex_lock
#define ast_mutex_lock(a) \
	(_log(LOG_DEBUG, "%s:%d ast_mutex_lock(%s,%p)",__FUNCTION__,__LINE__,#a,a),__ast_pthread_mutex_lock(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a))

#undef ast_mutex_unlock
#define ast_mutex_unlock(a) \
	(_log(LOG_DEBUG, "%s:%d ast_mutex_unlock(%s,%p)",__FUNCTION__,__LINE__, #a,a),__ast_pthread_mutex_unlock(__FILE__, __LINE__, __PRETTY_FUNCTION__, #a, a))

#endif

static void _log(int priority, const char* format, ...)
{
	va_list ap;

	char line[1024];

	va_start(ap, format);

	vsnprintf(line, sizeof(line), format, ap);
	syslog(priority, "[%s] %s", AST_MODULE, line);

	va_end(ap);
}

static unsigned long long get_monotonic_time(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (unsigned long long)ts.tv_sec * 1000 + (unsigned long long)ts.tv_nsec / 1000000;
}

static unsigned long get_time(void)
{
	struct tms buf;
	clock_t cur;

	cur = times(&buf);

	return cur * 1000 / sysconf(_SC_CLK_TCK);
}

static struct dev_t* _find_dev_by_id(const char* id)
{
	struct dev_t *dev = NULL;

	AST_RWLIST_RDLOCK(&devices);
	{
		AST_RWLIST_TRAVERSE(&devices, dev, entry) {
			if(!strcmp(dev->id, id)) {
				break;
			}
		}

		AST_RWLIST_UNLOCK(&devices);
	}

	return dev;
}

/*
	alsa interface
*/

static snd_pcm_t *alsa_card_init(struct dev_t* dev, char *dev_name, snd_pcm_stream_t stream, int* rwdev)
{
	int err;
	int direction;
	snd_pcm_t *handle = NULL;
	snd_pcm_hw_params_t *hwparams = NULL;
	snd_pcm_sw_params_t *swparams = NULL;
	struct pollfd pfd;
	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buffer_size = 0;
	unsigned int rate;
	snd_pcm_uframes_t start_threshold, stop_threshold;

	err = snd_pcm_open(&handle, dev_name, stream, SND_PCM_NONBLOCK);
	if(err < 0) {
		_log(LOG_ERR, "snd_pcm_open failed: '%s' - %s\n", dev_name, snd_strerror(err));
		return NULL;
	} else {
		_log(LOG_DEBUG, "Opening device %s in %s mode\n", dev_name, (stream == SND_PCM_STREAM_CAPTURE) ? "read" : "write");
	}

	snd_pcm_nonblock(handle, 1);

	hwparams = ast_alloca(snd_pcm_hw_params_sizeof());
	memset(hwparams, 0, snd_pcm_hw_params_sizeof());
	snd_pcm_hw_params_any(handle, hwparams);

	err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if(err < 0)
		_log(LOG_ERR, "set_access failed: %s\n", snd_strerror(err));

	err = snd_pcm_hw_params_set_format(handle, hwparams, format);
	if(err < 0)
		_log(LOG_ERR, "set_format failed: %s\n", snd_strerror(err));

	err = snd_pcm_hw_params_set_channels(handle, hwparams, 1);
	if(err < 0)
		_log(LOG_ERR, "set_channels failed: %s\n", snd_strerror(err));

	rate = dev->sampling_frequency;
	period_size = dev->period_frame_size;
	buffer_size = dev->playback_buffer_size;

	_log(LOG_DEBUG, "period size to config : %d\n", period_size);
	_log(LOG_DEBUG, "buffer size to config : %d\n", buffer_size);

	direction = 0;
	err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, &direction);
	if(rate != dev->sampling_frequency)
		_log(LOG_WARNING, "Rate not correct, requested %d, got %u\n", dev->sampling_frequency, rate);

	direction = 0;
	err = snd_pcm_hw_params_set_period_size_near(handle, hwparams, &period_size, &direction);
	if(err < 0)
		_log(LOG_ERR, "period_size(%lu frames) is bad: %s\n", period_size, snd_strerror(err));

	err = snd_pcm_hw_params_set_buffer_size_near(handle, hwparams, &buffer_size);
	if(err < 0)
		_log(LOG_WARNING, "Problem setting buffer size of %lu: %s\n", buffer_size, snd_strerror(err));

	err = snd_pcm_hw_params(handle, hwparams);
	if(err < 0)
		_log(LOG_ERR, "Couldn't set the new hw params: %s\n", snd_strerror(err));

	swparams = ast_alloca(snd_pcm_sw_params_sizeof());
	memset(swparams, 0, snd_pcm_sw_params_sizeof());
	snd_pcm_sw_params_current(handle, swparams);

	if(stream == SND_PCM_STREAM_PLAYBACK)
		start_threshold = period_size;
	else
		start_threshold = period_size;

	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
	if(err < 0)
		_log(LOG_ERR, "start threshold: %s\n", snd_strerror(err));

	if(stream == SND_PCM_STREAM_PLAYBACK)
		stop_threshold = buffer_size;
	else
		stop_threshold = buffer_size;

	err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);
	if(err < 0)
		_log(LOG_ERR, "stop threshold: %s\n", snd_strerror(err));

	err = snd_pcm_sw_params(handle, swparams);
	if(err < 0)
		_log(LOG_ERR, "sw_params: %s\n", snd_strerror(err));

	/* hardware and software configuration setup */
	{
		_log(LOG_INFO, "* hw config summary (%s - %s)", (stream == SND_PCM_STREAM_CAPTURE) ? "record" : "playback", dev_name);

		unsigned int config_ch;
		err = snd_pcm_hw_params_get_channels(hwparams, &config_ch);
		if(err == 0)
			_log(LOG_INFO, "channels : %d\n", err == 0 ? config_ch : -1);
		else
			_log(LOG_INFO, "channels : unknown\n");

		unsigned int config_rate;
		int config_dir;

		err = snd_pcm_hw_params_get_rate(hwparams, &config_rate, &config_dir);
		if(err == 0)
			_log(LOG_INFO, "rate : %d (dir=%d)\n", config_rate, config_dir);
		else
			_log(LOG_INFO, "rate : unknown\n");

		snd_pcm_uframes_t config_period_size;

		err = snd_pcm_hw_params_get_period_size(hwparams, &config_period_size, &config_dir);
		if(err == 0)
			_log(LOG_INFO, "period size : %d (dir=%d)\n", config_period_size, config_dir);
		else
			_log(LOG_INFO, "period size : unknown\n");

		snd_pcm_uframes_t config_buf_size;
		err = snd_pcm_hw_params_get_buffer_size(hwparams, &config_buf_size);
		if(err == 0)
			_log(LOG_INFO, "buffer size : %d\n", config_buf_size);
		else
			_log(LOG_INFO, "buffer size : unknown\n");

		snd_pcm_uframes_t config_start_threshold;
		err = snd_pcm_sw_params_get_start_threshold(swparams, &config_start_threshold);
		if(err == 0)
			_log(LOG_INFO, "start threshold : %d\n", config_start_threshold);
		else
			_log(LOG_INFO, "start threshold : unknown\n");

		snd_pcm_uframes_t config_stop_threshold;
		err = snd_pcm_sw_params_get_stop_threshold(swparams, &config_stop_threshold);
		if(err == 0)
			_log(LOG_INFO, "stop threshold : %d\n", config_stop_threshold);
		else
			_log(LOG_INFO, "stop threshold : unknown\n");

	}


	err = snd_pcm_poll_descriptors_count(handle);
	if(err <= 0)
		_log(LOG_ERR, "Unable to get a poll descriptors count, error is %s\n", snd_strerror(err));
	if(err != 1) {
		_log(LOG_DEBUG, "Can't handle more than one device\n");
	}

	snd_pcm_poll_descriptors(handle, &pfd, err);
	_log(LOG_DEBUG, "Acquired fd %d from the poll descriptor\n", pfd.fd);

	*rwdev = pfd.fd;

	return handle;
}

static void enable_voice_mixer()
{
#ifdef CONFIG_NOAUDIOCAPTURE
	_log(LOG_INFO, "[SND] disable pcm audio, skip voice mixer");
#else

	char pcm_amix_command[1024];

	_log(LOG_INFO, "[SND] enable voice mixer");

	snprintf(pcm_amix_command, sizeof(pcm_amix_command),
	         /* enable voice mixer */
	         "amix 'PRI_MI2S_RX_Voice Mixer VoiceMMode1' 1;"
	         "amix 'VoiceMMode1_Tx Mixer PRI_MI2S_TX_MMode1' 1;"
	        );

	system(pcm_amix_command);
#endif
}

static void enable_host_pcm_interface()
{
#ifdef CONFIG_NOAUDIOCAPTURE
	_log(LOG_INFO, "[SND] disable pcm audio, skip int host pcm interface.");
#else

	char pcm_amix_command[1024];

	_log(LOG_INFO, "[SND] enable host pcm interface");

	snprintf(pcm_amix_command, sizeof(pcm_amix_command),

#ifdef CONFIG_USE_INCALL_PCM
	         "amix 'Incall_Music Audio Mixer MultiMedia2' 1;"
	         "amix 'MultiMedia1 Mixer VOC_REC_DL' 1;"
	         "amix 'MultiMedia1 Mixer VOC_REC_UL' 1;"

#else
	         /* disable topology */
	         "amix 'Voice Topology Disable' 1 297816064 2> /dev/null > /dev/null;"
	         /* pcm rx output - record */
	         "amix 'HPCM_VMMode1 tappoint direction samplerate' 1 0 %d 2> /dev/null > /dev/null;"
	         /* pcm tx input - playback */
	         "amix 'HPCM_VMMode1 tappoint direction samplerate' 2 1 %d 2> /dev/null > /dev/null;"

	         , main_sampling_frequency, main_sampling_frequency
#endif
	        );

	system(pcm_amix_command);
#endif
}


static void soundcard_start_recording(struct dev_t* dev)
{
#ifdef CONFIG_NOAUDIOCAPTURE
	_log(LOG_INFO, "[SND] disable audio capture, not starting");
#else

	_log(LOG_INFO, "[SND] start recording");

	snd_pcm_prepare(dev->alsa_ocard);
	snd_pcm_prepare(dev->alsa_icard);

#ifdef CONFIG_MANUALLY_START_PCM_DEVICES
	snd_pcm_start(dev->alsa_ocard);
	snd_pcm_start(dev->alsa_icard);
#endif
#endif
}

static void soundcard_stop_recording(struct dev_t* dev)
{
#ifdef CONFIG_NOAUDIOCAPTURE
	_log(LOG_INFO, "[SND] disable audio capture - not stopping");
#else

	if(dev->alsa_icard) {
		_log(LOG_INFO, "[SND] stop recording");
		snd_pcm_drop(dev->alsa_icard);
	}

	if(dev->alsa_ocard) {
		_log(LOG_INFO, "[SND] stop play-back");
		snd_pcm_drop(dev->alsa_ocard);
	}

#endif
}

static void soundcard_fini(struct dev_t* dev)
{
#ifdef CONFIG_NOAUDIOCAPTURE
	_log(LOG_INFO, "[SND] disable pcm audio, skip sound card fini.");
#else

	_log(LOG_INFO, "finialise capture PCM device (dev=%s)\n", dev->icard_dev_name);

	if(dev->writedev)
		close(dev->writedev);
	if(dev->readdev)
		close(dev->readdev);

	if(dev->alsa_icard)
		snd_pcm_close(dev->alsa_icard);
	if(dev->alsa_ocard)
		snd_pcm_close(dev->alsa_ocard);

	if(dev->pcm_read_buf)
		ast_free(dev->pcm_read_buf);

	if(dev->pcm_write_buf)
		ast_free(dev->pcm_write_buf);

	dev->alsa_icard = NULL;
	dev->alsa_ocard = NULL;

	dev->readdev = 0;
	dev->writedev = 0;

	dev->pcm_write_buf = NULL;
	dev->pcm_read_buf = NULL;

	enable_host_pcm_interface();
#endif
}

static int soundcard_is_open(struct dev_t* dev)
{
	return dev->alsa_icard && dev->alsa_ocard;
}

static int soundcard_init(struct dev_t* dev)
{
#ifdef CONFIG_NOAUDIOCAPTURE
	_log(LOG_INFO, "[SND] disable pcm audio, skip sound card init.");
	return 0;
#else

	_log(LOG_INFO, "[SND] initate playback PCM device (dev=%s)\n", dev->ocard_dev_name);

	/* allocate pcm write buffer */
	dev->pcm_write_buf_size = dev->playback_buffer_size * 2;
	dev->pcm_write_buf = ast_malloc(dev->pcm_write_buf_size);
	if(!dev->pcm_write_buf) {
		_log(LOG_ERR, "[SND] failed to allocate plabyack buffer (dev=%s,len=%d)\n", dev->ocard_dev_name, dev->pcm_write_buf_size);
		goto err;
	}

	dev->alsa_ocard = alsa_card_init(dev, dev->ocard_dev_name, SND_PCM_STREAM_PLAYBACK, &dev->writedev);
	if(!dev->alsa_ocard) {
		_log(LOG_ERR, "[SND] Problem opening ALSA playback device (dev=%s)\n", dev->ocard_dev_name);
		goto err;
	}

	fcntl(dev->writedev, F_SETFD, FD_CLOEXEC);

#if 0
	_log(LOG_ERR, "[SND] wait for playback pcm (dev=%s)", dev->ocard_dev_name);
	snd_pcm_wait(dev->alsa_ocard, 5000);
#endif

	_log(LOG_INFO, "[SND] initate capture PCM device (dev=%s)\n", dev->icard_dev_name);

	/* allocate pcm read buffer */
	dev->pcm_read_buf_size = dev->playback_buffer_size * 2 + AST_FRIENDLY_OFFSET;
	dev->pcm_read_buf_pos = 0;
	dev->pcm_read_buf = ast_malloc(dev->pcm_read_buf_size);
	if(!dev->pcm_read_buf) {
		_log(LOG_ERR, "[SND] failed to allocate capture buffer (dev=%s,len=%d)\n", dev->icard_dev_name, dev->pcm_read_buf_size);
		goto err;
	}

	dev->alsa_icard = alsa_card_init(dev, dev->icard_dev_name, SND_PCM_STREAM_CAPTURE, &dev->readdev);

	if(!dev->alsa_icard) {
		_log(LOG_ERR, "[SND] problem opening alsa capture device (dev=%s)\n", dev->icard_dev_name);
		goto err;
	}

	fcntl(dev->readdev, F_SETFD, FD_CLOEXEC);

#ifdef CONFIG_LINK_RX_AND_TX
	/* link */
	snd_pcm_link(dev->alsa_icard, dev->alsa_ocard);
#endif

#if 0
	_log(LOG_ERR, "[SND] wait for record pcm (dev=%s)", dev->icard_dev_name);
	snd_pcm_wait(dev->alsa_icard, 5000);
#endif

	return dev->readdev;

err:
	soundcard_fini(dev);

	return -1;
#endif
}

/*
	Asterisk interface functions
*/

#define _deadlock_avoidance(lock) \
	do { \
		int __res; \
		if (!(__res = ast_mutex_unlock(lock))) { \
			usleep(1); \
			ast_mutex_lock(lock); \
		} else { \
			_log(LOG_WARNING, "Failed to unlock mutex '%s' (%s).  I will NOT try to relock. {{{ THIS IS A BUG. }}}\n", #lock, strerror(__res)); \
		} \
	} while (0)

static int _ast_queue_control(struct dev_t *dev, enum ast_control_frame_type control)
{
	struct ast_channel *owner;

	for(;;) {
		owner = dev->owner;
		if(owner) {
			if(ast_channel_trylock(owner)) {
				_deadlock_avoidance(&dev->lock);
			} else {
				ast_queue_control(owner, control);
				ast_channel_unlock(owner);
				break;
			}
		} else
			break;
	}
	return 0;
}


/*
	Asterisk event functions
*/

static void disconnect_pcm(struct dev_t* dev)
{
	if(dev->alsa_icard) {

		if(dev->owner) {
			_log(LOG_DEBUG, "[SND] disconnect PCM capture device from channel (dev=%s,readdev=%d)\n", dev->icard_dev_name, dev->readdev);
			ast_channel_set_fd(dev->owner, 0, -1);
		} else {
			_log(LOG_ERR, "[SND] no channel assigned to device (dev=%s)\n", dev->icard_dev_name);
		}
	}
}

static void connect_pcm(struct dev_t* dev)
{
	/* connect readdev into owner */
	if(dev->owner) {
		_log(LOG_DEBUG, "[SND] connect PCM capture device to channel (dev=%s,readdev=%d)", dev->icard_dev_name, dev->readdev);
		ast_channel_set_fd(dev->owner, 0, dev->readdev);
	} else {
		_log(LOG_ERR, "[SND] no channel assigned for device (dev=%s)", dev->icard_dev_name);
	}

}

#if 0
static void wait_initial_pcm(struct dev_t* dev)
{
	short* buf;
	int len;
	int r;

	buf = (short*)(dev->pcm_read_buf + AST_FRIENDLY_OFFSET);
	len = dev->receive_frame_size;

	_log(LOG_DEBUG, "[SND] wait for audio packets (dev=%s)\n", dev->id);
	do {
		r = snd_pcm_readi(dev->alsa_icard, buf, len);
	} while(r != -EAGAIN);

	_log(LOG_DEBUG, "[SND] audio recording packet ready (dev=%s,r=%d)\n", dev->id, r);
}
#endif

#ifdef CONFIG_USE_STATISTICS
/*
	runtime update functions

	TODO: use mutext to avoid race-condition in multiple QMI RDB channel devices
*/

static void rupdate_run_script(struct call_t* call, int up)
{
	if(up) {
		_rdb_set_str("voice_call.statistics.StopTime", "");
		_rdb_set_int("voice_call.statistics.StartTime", call->StartTime);
	} else {
		_rdb_set_int("voice_call.statistics.StopTime", call->StopTime);

		_rdb_set_int("voice_call.statistics.trigger", 1);
		_rdb_set_int("voice_call.statistics.end_of_call", 1);
	}
}

static void rupdate_statistics()
{
	_log(LOG_DEBUG, "[statistics] update statistics");

	_log(LOG_DEBUG, "[statistics] IncomingCallsReceived : %d", statistics.IncomingCallsReceived);
	_log(LOG_DEBUG, "[statistics] IncomingCallsAnswered : %d", statistics.IncomingCallsAnswered);
	_log(LOG_DEBUG, "[statistics] IncomingCallsConnected : %d", statistics.IncomingCallsConnected);

	_log(LOG_DEBUG, "[statistics] OutgoingCallsAttempted : %d", statistics.OutgoingCallsAttempted);
	_log(LOG_DEBUG, "[statistics] OutgoingCallsAnswered : %d", statistics.OutgoingCallsAnswered);
	_log(LOG_DEBUG, "[statistics] OutgoingCallsConnected : %d", statistics.OutgoingCallsConnected);

	_log(LOG_DEBUG, "[statistics] TotalCallTime : %d", statistics.TotalCallTime);

	_rdb_set_int("voice_call.statistics.IncomingCallsReceived", statistics.IncomingCallsReceived);
	_rdb_set_int("voice_call.statistics.IncomingCallsAnswered", statistics.IncomingCallsAnswered);
	_rdb_set_int("voice_call.statistics.IncomingCallsConnected", statistics.IncomingCallsConnected);

	_rdb_set_int("voice_call.statistics.OutgoingCallsAttempted", statistics.OutgoingCallsAttempted);
	_rdb_set_int("voice_call.statistics.OutgoingCallsAnswered", statistics.OutgoingCallsAnswered);
	_rdb_set_int("voice_call.statistics.OutgoingCallsConnected", statistics.OutgoingCallsConnected);

	_rdb_set_int("voice_call.statistics.TotalCallTime", statistics.TotalCallTime);

	_rdb_set_int("voice_call.statistics.changed", 1);
}

static void rupdate_TotalCallTime(struct call_t* call)
{
	statistics.TotalCallTime += (call->uptime_StopTime - call->uptime_StartTime + 999) / 1000;
	rupdate_statistics();
}

static void rupdate_StartTime(struct call_t* call)
{
	time_t dummy;

	call->StartTime = time(&dummy);
	call->StopTime = 0;
	call->uptime_StartTime = get_time();
	call->uptime_StopTime = 0;

	_log(LOG_DEBUG, "[statistics] update StartTime : %d", call->StartTime);
}

static void rupdate_StopTime(struct call_t* call)
{
	time_t dummy;
	call->StopTime = time(&dummy);;
	call->uptime_StopTime = get_time();

	_log(LOG_DEBUG, "[statistics] update StopTime : %d", call->StopTime);
}

static int init_rdbs()
{
	memset(&statistics, 0, sizeof(statistics));

	statistics.IncomingCallsReceived = _rdb_get_int("voice_call.statistics.IncomingCallsReceived");
	statistics.IncomingCallsAnswered = _rdb_get_int("voice_call.statistics.IncomingCallsAnswered");
	statistics.IncomingCallsConnected = _rdb_get_int("voice_call.statistics.IncomingCallsConnected");

	statistics.OutgoingCallsAttempted = _rdb_get_int("voice_call.statistics.OutgoingCallsAttempted");
	statistics.OutgoingCallsAnswered = _rdb_get_int("voice_call.statistics.OutgoingCallsAnswered");
	statistics.OutgoingCallsConnected = _rdb_get_int("voice_call.statistics.OutgoingCallsConnected");

	statistics.TotalCallTime = _rdb_get_int("voice_call.statistics.TotalCallTime");

	rupdate_statistics();

	return 0;
}

#endif


static int rdb_release_all_incoming_calls(struct dev_t* dev, const char* reject_cause)
{
	struct call_t* call;

	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		if(call->cid && call->incoming && !call->answered) {
			rdb_manage_calls_release(dev, call->cid, reject_cause);
		}
	}

	return 0;
}

static int rdb_hangupall(struct dev_t* dev)
{
	struct call_t* call;

	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		if(call->cid && (call->outgoing || call->answered)) {
			_log(LOG_DEBUG, "hanging up call (cid=%d)", call->cid);
			rdb_hangup(dev, call->cid);
		}
	}

	return 0;
}

#if 0
static int rdb_hangup_calls_on_hold(struct dev_t* dev)
{
	struct call_t* call;

	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		if(call->cid && call->held && (call->connected || call->answered)) {
			_log(LOG_DEBUG, "hanging up call on hold (cid=%d)", call->cid);
			rdb_hangup(dev, call->cid);
		}
	}

	return 0;
}
#endif

static int _ast_hangup(struct ast_channel *ast)
{
	struct dev_t *dev;


	_log(LOG_INFO, "[AST] got hangup callback");

	if(!ast_channel_tech_pvt(ast)) {
		_log(LOG_NOTICE, "asked to hangup channel not connected");
		return 0;
	}

	dev = ast_channel_tech_pvt(ast);

	_log(LOG_DEBUG, "[%s] hanging up device", dev->id);

	/* hangup all calls */
	ast_mutex_lock(&dev->lock);
	{
		rdb_hangupall(dev);
		//rdb_manage_calls(dev,"end_all_calls",0,NULL);

		soundcard_stop_and_fini(dev);

		dev->owner = NULL;
		ast_channel_tech_pvt_set(ast, NULL);

		ast_mutex_unlock(&dev->lock);
	}

	_log(LOG_INFO, "[AST] set AST_STATE_DOWN");
	ast_setstate(ast, AST_STATE_DOWN);

	ast_mutex_lock(&dev->lock);
	{
		struct call_t* call;

		/* remove incomplete calls */
		call = NULL;
		AST_LIST_TRAVERSE_SAFE_BEGIN(&dev->calls, call, entry) {
			if(!call->cid || (call->outgoing && !call->progressed)) {
				_log(LOG_INFO, "[AST] remove incomplete call (cid=%d)", call->cid);
				AST_LIST_REMOVE(&dev->calls, call, entry);
			}
		}
		AST_LIST_TRAVERSE_SAFE_END;


		/* ring incoming */
		call = NULL;
		AST_LIST_TRAVERSE(&dev->calls, call, entry) {
			if(call->incoming && !call->answered)
				break;
		}

		if(call) {
			_log(LOG_INFO, "[AST] waiting call found, ring (cid=%d)", call->cid);
			incall_start_mt_call(dev, call);
		}

		ast_mutex_unlock(&dev->lock);
	}


	return 0;
}

#ifdef CONFIG_USE_EXTRA_CB
static int _ast_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	struct dev_t* dev = ast_channel_tech_pvt(newchan);

	dev->owner = newchan;

	return 0;
}

static int _ast_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen)
{
	struct dev_t* dev = ast_channel_tech_pvt(ast);
	int res = 0;

	_log(LOG_INFO, "[AST] got indicate callback (condition=%d)", condition);

	switch(condition) {
		case AST_CONTROL_SRCCHANGE:
			break;

		case AST_CONTROL_RINGING:
			_log(LOG_INFO, "[AST] got AST_CONTROL_RINGING");
			res = -1;
			break;

		case AST_CONTROL_BUSY:
			_log(LOG_INFO, "[AST] got AST_CONTROL_BUSY");

			rdb_release_all_incoming_calls(dev, "USER_BUSY");
			break;

		case AST_CONTROL_PVT_CAUSE_CODE: {
			_log(LOG_INFO, "[AST] got AST_CONTROL_PVT_CAUSE_CODE");

#ifdef VERBOSE_DEBUG_SIP_INFO
			int i;
			const char* p = (char*)data;

			for(i = 0; i < datalen; i++)
				_log(LOG_DEBUG, "data[%d] = 0x%02x '%c'", i, p[i], p[i] ? p[i] & 0x7f : ' ');

#endif

			res = -1;
			break;
		}

		case AST_CONTROL_CONNECTED_LINE:
		case AST_CONTROL_CONGESTION:
		case AST_CONTROL_INCOMPLETE:
		case -1:
			/* Ask for inband indications */
			res = -1;
			break;

		case AST_CONTROL_PROGRESS:
		case AST_CONTROL_PROCEEDING:
		case AST_CONTROL_VIDUPDATE:
		case AST_CONTROL_SRCUPDATE:
			break;
		case AST_CONTROL_HOLD:
			/* TODO */
			break;
		case AST_CONTROL_UNHOLD:
			/* TODO */
			break;

		case AST_CONTROL_FLASH:
			_log(LOG_INFO, "[AST] got AST_CONTROL_FLASH");
			res = -1;
			break;

		default:
			_log(LOG_WARNING, "unknown indicate condition %d\n", condition);
			res = -1;
			break;

	}

	return res;
}
#endif

#define DEFAULT_INCALL_INTERDIGIT_DELAY 8000
#define DEFAULT_INCALL_INITDIGIT_DELAY 8000
#define DEFAULT_INCALL_CALL_TIMEOUT 30

#define INVITE_DELAY_TIMER 1000

/* dtmf functions */
static int incall_interdigit_delay = DEFAULT_INCALL_INTERDIGIT_DELAY;
static int incall_initdigit_delay = DEFAULT_INCALL_INITDIGIT_DELAY;
static int incall_call_timeout = DEFAULT_INCALL_CALL_TIMEOUT;

struct info_cw_data {
	struct ast_sip_session *session;
	int cw_indicator;
	char* pi;
	char* clip;
	char* clip_name;
};

static void info_cw_data_destroy(void *obj)
{
	struct info_cw_data *cw_data = obj;

	ast_free(cw_data->pi);
	ast_free(cw_data->clip);
	ast_free(cw_data->clip_name);

	ao2_ref(cw_data->session, -1);
}

static struct info_cw_data *info_cw_data_alloc(struct ast_sip_session *session, int cw_indicator, const char* pi, const char* clip, const char* clip_name)
{
	struct info_cw_data *cw_data = ao2_alloc(sizeof(*cw_data), info_cw_data_destroy);

	if(!cw_data) {
		return NULL;
	}

	ao2_ref(session, +1);

	cw_data->session = session;
	cw_data->cw_indicator = cw_indicator;
	cw_data->pi = pi ? ast_strdup(pi) : NULL;
	cw_data->clip = clip ? ast_strdup(clip) : NULL;
	cw_data->clip_name = clip_name ? ast_strdup(clip_name) : NULL;

	return cw_data;
}

static int transmit_info_cw(void *data)
{
	RAII_VAR(struct info_cw_data *, cw_data, data, ao2_cleanup);
	RAII_VAR(struct ast_str *, body_text, NULL, ast_free_ptr);

	struct ast_sip_session *session = cw_data->session;

	struct pjsip_tx_data *tdata;
	const char* event_name;
	const char* clip = cw_data->clip;

	/* get event name */
	event_name = (cw_data->cw_indicator == DTMF_DIGIT_CW_START) ? "start-CWT" : "stop-CWT";

	/* build INFO */
	_log(LOG_DEBUG, "build SIP INFO [%s]", event_name);

	if(ast_sip_create_request("INFO", session->inv_session->dlg, session->endpoint, NULL, NULL, &tdata)) {
		_log(LOG_ERROR, "Could not create text video update INFO request\n");
		goto err;
	}

	ast_sip_add_header(tdata, "Event", event_name);

	/* get clip based on pi */
	if(cw_data->cw_indicator == DTMF_DIGIT_CW_START) {

		if(cw_data->pi && !strcmp(cw_data->pi, "RESTRICTED"))
			clip = "anonymous@anonymous.invalid";
		else if(cw_data->pi && !strcmp(cw_data->pi, "UNAVAILABLE"))
			clip = "unavailable@unknown.invalid";
		else if(ast_strlen_zero(cw_data->clip) || (ast_strlen_zero(cw_data->pi) && ast_strlen_zero(cw_data->clip)))
			clip = "notavailable@unknown.invalid";

		/* add clip */
		if(!ast_strlen_zero(clip)) {
			char sip_clip[1024];

			const char* addr = _rdb_get_str("vlan.voice.address");

			if(cw_data->clip_name)
				snprintf(sip_clip, sizeof(sip_clip), "\"%s\" <sip:%s@%s>", cw_data->clip_name ? cw_data->clip_name : clip , clip, addr);
			else
				snprintf(sip_clip, sizeof(sip_clip), "<sip:%s@%s>", clip, addr);


			/*
				From: "default.virtual" <sip:default.virtual@192.168.13.1>;tag=43d2d8c0-5f87-4e4c-a53d-41167993e3d0
			*/

			ast_sip_add_header(tdata, "P-Asserted-Identity", sip_clip);
		}
	}

	if(!(body_text = ast_str_create(32))) {
		_log(LOG_ERROR, "Could not allocate buffer for INFO DTMF.\n");
		goto err;
	}
	ast_str_set(&body_text, 0, "Signal=%c\r\nDuration=%u\r\n", 'a', 10);

	_log(LOG_DEBUG, "send SIP INFO (clip=%s)", clip);
	ast_sip_session_send_request(session, tdata);

	return 0;
err:
	return -1;
}


static int incall_send_info_to_peer(struct dev_t* dev, int cw_indicator, const char* pi, const char* clip, const char* clip_name)
{
	/*
		This hack is to support INFO in SIP call flow for AT&T PACE, fundamentally requiring peer patch in Asterisk channel structure.
	*/


	struct ast_channel* peer_chan;
	const struct ast_channel_tech* peer_tech;
	struct ast_sip_channel_pvt *peer_pvt;

	struct info_cw_data *cw_data;

	int rc = -1;

	/* get peer */
	_log(LOG_DEBUG, "get peer channel");
	peer_chan = ast_channel_peer(dev->owner);
	if(!peer_chan) {
		_log(LOG_DEBUG, "no peer assigned, ignore");
		goto err;
	}

	/* get peer tech */
	_log(LOG_DEBUG, "get peer tech");
	peer_tech = ast_channel_tech(peer_chan);
	if(strcmp(peer_tech->type, "PJSIP")) {
		_log(LOG_ERR, "peer is not PJSIP, ignore (type=%s)", peer_tech->type);
		goto err;
	}

	ast_channel_lock(peer_chan);
	{
		/* get private */
		peer_pvt = ast_channel_tech_pvt(peer_chan);

		/* allocate cw */
		cw_data = info_cw_data_alloc(peer_pvt->session, cw_indicator, pi, clip, clip_name);
		if(!cw_data) {
			_log(LOG_WARNING, "failed to allocate cw data");
			goto fini_lock_sec;
		}

		/* push task */
		if(ast_sip_push_task(peer_pvt->session->serializer, transmit_info_cw, cw_data)) {
			_log(LOG_WARNING, "failed to send CW via INFO.\n");
			ao2_cleanup(cw_data);
			goto fini_lock_sec;
		}

		rc = 0;

fini_lock_sec:
		ast_channel_unlock(peer_chan);
	}

	_log(LOG_DEBUG, "done SIP INFO");

	return rc;
err:
	return -1;
}

static int incall_set_peer_ringback(struct dev_t* dev, enum peer_ringback_t peer_ringback)
{
	const char* peer_ringback_names[] = {
		[peer_ringback_none] = NULL,
		[peer_ringback_local] = "ring",
		[peer_ringback_remote] = NULL,
	};

	const char* ringback_names[] = {
		[peer_ringback_none] = "NONE",
		[peer_ringback_local] = "LOCAL",
		[peer_ringback_remote] = "REMOTE",
	};

	/* bypass if ringback is not changed */
	if(dev->peer_ringback == peer_ringback) {
		goto fini;
	}

	/* update peer ringback */
	_log(LOG_DEBUG, "[RINGBACK] change ringback [%s] ==> [%s]", ringback_names[dev->peer_ringback], ringback_names[peer_ringback]);
	dev->peer_ringback = peer_ringback;

	incall_playtones_in_peer(dev, peer_ringback_names[dev->peer_ringback]);

fini:
	return 0;
}

static int incall_playtones_in_peer(struct dev_t* dev, const char* data)
{
	struct ast_channel* chan = dev->owner;
	struct ast_channel* peer_chan;

	if(!chan) {
		_log(LOG_DEBUG, "no channel assigned to control tone in peer");
		goto err;
	}

	/* get peer */
	peer_chan = ast_channel_peer(dev->owner);
	if(!peer_chan) {
		_log(LOG_DEBUG, "no peer channel assigned to control tone in peer");
		goto err;
	}

#ifdef CONFIG_USE_INCALL_TONE
	ast_mutex_unlock(&dev->lock);
	{
		ast_channel_lock(peer_chan);
		{
			/* stop any previous tone */
			if(!data) {
				_log(LOG_DEBUG, "stop tone in peer");
				_ast_stopplaytones(peer_chan);
			} else {
				_log(LOG_DEBUG, "play tone in peer (tone=%s)", data);
				_ast_playtones(peer_chan, data);
			}

			ast_channel_unlock(peer_chan);
		}

		ast_mutex_lock(&dev->lock);
	}

#endif

	return 0;

err:
	return -1;
}

static int incall_set_stat(struct dev_t* dev, enum incall_stat_t stat)
{
#ifdef CONFIG_USE_INCALL_TONE
	const char* linetone_names[] = {
		[incall_stat_none] = NULL,
		[incall_stat_dial] = "dial",
		[incall_stat_dialing] = NULL,
		[incall_stat_ring] = NULL,
		[incall_stat_ringing] = NULL,
		[incall_stat_busy] = "busy",
		[incall_stat_disconnected] = "busy",
		[incall_stat_waiting] = NULL,
	};

	const char* linetone_name;
#endif
	struct ast_channel* chan = dev->owner;
	struct ast_channel* peer_chan;

	/* bypass if not changed */
	if(dev->incall_stat == stat) {
		_log(LOG_DEBUG, "[INCALL] incall status not changed (dev=%s,stat=%d)", dev->id, stat);
		goto fini;
	}

	/* perform sanity check */
	if(stat < incall_stat_none || incall_stat_last <= stat) {
		_log(LOG_ERR, "[INCALL] incorrect linetone selected (dev=%s,stat=%d)", dev->id, stat);
		goto err;
	}

	_log(LOG_DEBUG, "[INCALL] switch [%s] ==> [%s] (dev=%s)", incall_get_stat_str(dev->incall_stat), incall_get_stat_str(stat), dev->id);

	/* update incall status */
	dev->incall_stat = stat;

	if(!chan) {
		_log(LOG_DEBUG, "no channel assigned to control tone");
		goto fini;
	}

	/* get peer */
	peer_chan = ast_channel_peer(dev->owner);
	if(!peer_chan) {
		_log(LOG_DEBUG, "no peer channel assigned to control tone");
		goto fini;
	}


#ifdef CONFIG_USE_INCALL_TONE
	linetone_name = linetone_names[stat];

	/*
		## do not release dev->lock ##

		It is to avoid competing with PCM thread write or read thread after entering Asterisk critical section of channel driver.
		These lock/unlock mutex lines are commented out - not completely deleted for future reference
	*/

	//ast_mutex_unlock(&dev->lock);
	{
		ast_channel_lock(peer_chan);
		{
			/* stop any previous tone */
			if(dev->incall_tone_playing) {
				_ast_stopplaytones(peer_chan);
				dev->incall_tone_playing = 0;
			}

			/* play or stop linetone */
			if(linetone_name) {
				_log(LOG_DEBUG, "[INCALL] start tone (dev=%s,stat=%d,tone=%s)", dev->id, stat, linetone_name);
				_ast_playtones(peer_chan, linetone_name);

				dev->incall_tone_playing = 1;
			} else {
				_log(LOG_DEBUG, "[INCALL] stop tone (dev=%s,stat=%d)", dev->id, stat);
			}

			ast_channel_unlock(peer_chan);
		}

		//ast_mutex_lock(&dev->lock);
	}
#endif


fini:
	return 0;

err:
	return -1;
}

static const char* incall_get_stat_str(enum incall_stat_t stat)
{
	const char* stat_names[] = {
		[incall_stat_none] = "NONE",
		[incall_stat_dial] = "DIAL",
		[incall_stat_dialing] = "DIALING",
		[incall_stat_ring] = "RING",
		[incall_stat_ringing] = "RINGING",
		[incall_stat_busy] = "BUSY",
		[incall_stat_disconnected] = "DISCONNECTED",
		[incall_stat_waiting] = "WAITING",
	};

	/* perform sanity check */
	if(stat < incall_stat_none || incall_stat_last <= stat) {
		return "UNKNOWN";
	}

	return stat_names[stat];
}

static int incall_is_stat(struct dev_t *dev, enum incall_stat_t stat)
{
	return dev->incall_stat == stat;
}

static int incall_get_stat(struct dev_t *dev)
{
	return dev->incall_stat;
}

static void dtmf_reset(struct dev_t *dev)
{
	/* updat dtmf timestamp */
	dev->dtmf_timestamp = get_time();

	/* reset index */
	dev->dtmf_digit_index = 0;
	*dev->dtmf_digits = 0;
}

static const char* dtmf_get(struct dev_t *dev)
{
	return dev->dtmf_digits;
}

#ifndef CONFIG_IGNORE_DTMF
static int dtmf_add(struct dev_t *dev, char digit)
{
	/* updat dtmf timestamp */
	dev->dtmf_timestamp = get_time();

	/* check validation */
	if(dev->dtmf_digit_index < 0 || MAX_DIAL_DIGIT <= dev->dtmf_digit_index) {
		_log(LOG_WARNING, "[INCALL] too many digits detected (digit=%d)", dev->dtmf_digit_index);
		goto err;
	}

	/* collect digit */
	dev->dtmf_digits[dev->dtmf_digit_index++] = digit;
	dev->dtmf_digits[dev->dtmf_digit_index] = 0;

	/* TODO: match dial pattern */

	return 0;

err:
	return -1;
}
#endif

static const char* dtmf_apply_dialplan(const char* destno)
{
	char* international_prefixes;
	char* emergency_calls;
	char* voice_mail;
	char* saveptr;

	static char destno_buf[256];
	const char* processed_destno;

	char* emergency_call;
	char* international_prefix;
	int international_prefix_len;

	/* get rdb dialplan */
	international_prefixes = ast_strdupa(_rdb_get_str("pbx.dialplan.misc.international"));
	emergency_calls = ast_strdupa(_rdb_get_str("pbx.dialplan.misc.emergency"));
	voice_mail = ast_strdupa(_rdb_get_str("pbx.dialplan.call.voicemail"));

	/* if voicemail */
	if(!strcmp(destno, voice_mail)) {
		_log(LOG_DEBUG, "[INCALL] dialplan - voicemail detected");
		processed_destno = VOICEMAIL_DIALNO;
		goto fini;
	}

	/* if emergency call */
	emergency_call = strtok_r(emergency_calls, "/", &saveptr);
	while(emergency_call) {
		if(!strcmp(destno, emergency_call)) {
			_log(LOG_DEBUG, "[INCALL] dialplan - emergency detected");

			snprintf(destno_buf, sizeof(destno_buf), "E%s", destno);
			processed_destno = destno_buf;
			goto fini;
		}
		emergency_call = strtok_r(NULL, "/", &saveptr);
	}

	/* if interntaional prefix */
	international_prefix = strtok_r(international_prefixes, "/", &saveptr);
	while(international_prefix) {
		international_prefix_len = strlen(international_prefix);
		if(!strncmp(destno, international_prefix, international_prefix_len)) {
			_log(LOG_DEBUG, "[INCALL] dialplan - international prefix detected");

			snprintf(destno_buf, sizeof(destno_buf), "+%s", destno + international_prefix_len);
			processed_destno = destno_buf;
			goto fini;
		}
		international_prefix = strtok_r(NULL, "/", &saveptr);
	}

	/* bypass destno without processing */
	processed_destno = destno;

fini:
	return processed_destno;
}

static int dtmf_dial(struct dev_t *dev)
{
	int stat = -1;

	const char* destno = dtmf_get(dev);
	const char* processed_destno;

	/* bypass if no digit is pressed */
	if(ast_strlen_zero(destno)) {

		_log(LOG_DEBUG, "[INCALL] dial with no number pressed");

		/* switch to ring */
		incall_set_stat(dev, incall_stat_busy);
		goto fini;
	}

	/* switch to ring */
	incall_set_stat(dev, incall_stat_ring);

	/* get processed destno */
	processed_destno = dtmf_apply_dialplan(destno);

	/* dial */
	_log(LOG_INFO, "[INCALL] call '%s' ==> '%s'", destno, processed_destno);
	stat = cc_make_mo_call(dev, processed_destno, incall_call_timeout, 0);

fini:
	return stat;
}

static int dtmf_process(struct dev_t *dev)
{
	unsigned long interdigit;
	int stat = 0;

	ast_mutex_lock(&dev->lock);

	/* bypass if no dial or dialing mode */
	if(!incall_is_stat(dev, incall_stat_dial) && !incall_is_stat(dev, incall_stat_dialing))
		goto fini;

	/* check interdigit expiration */
	interdigit = get_time() - dev->dtmf_timestamp;

	if(!dev->dtmf_digit_index) {
		if(interdigit < incall_initdigit_delay) {
			goto fini;
		} else {
			_log(LOG_DEBUG, "[INCALL] initdigit delay expired (interdigit=%u)", interdigit);
		}
	} else {

		if(interdigit < incall_interdigit_delay) {
			goto fini;
		} else {
			_log(LOG_DEBUG, "[INCALL] initdigit delay expired (interdigit=%d,digits='%s')", interdigit, dtmf_get(dev));
		}
	}

	dtmf_dial(dev);

fini:
	ast_mutex_unlock(&dev->lock);
	return stat;

}

/* dtmf asterisk functions */

static int _ast_digit_begin(struct ast_channel *ast, char digit)
{
#ifdef CONFIG_IGNORE_DTMF
	_log(LOG_INFO, "[AST] ignore outgoing DTMF begin");
	return 0;
#else

	struct dev_t *dev = ast_channel_tech_pvt(ast);
	struct call_t* call;
	int res;


	if(incall_is_stat(dev, incall_stat_none)) {
#ifdef CONFIG_USE_INBOUND_DTMF
		_log(LOG_INFO, "[AST] got digit begin callback (digit='%c') [INBOUND]", digit);

		res = -1;
#else

		_log(LOG_INFO, "[AST] got digit begin callback (digit='%c') [OUTBOUND]", digit);
		call = cc_get_active_dtmf_call(dev);
		if(!call) {
			_log(LOG_ERR, "[AST] no active call found - digit begin");
			goto err;
		}

		rdb_start_dtmf(dev, call->cid, digit);
		res = 0;
#endif

	} else {
		_log(LOG_DEBUG, "[AST] no call mode, ignore INBOUND or OUTBOUND start DTMF (digit='%c')", digit);
		res = 0;
	}

	ast_mutex_lock(&dev->lock);
	{
		if(incall_is_stat(dev, incall_stat_dial)) {
			/* clear linetone */
			incall_set_stat(dev, incall_stat_dialing);
		}

		ast_mutex_unlock(&dev->lock);
	}

	return res;

err:
	return -1;
#endif // CONFIG_IGNORE_DTMF
}

static int _ast_digit_end(struct ast_channel *ast, char digit, unsigned int duration)
{
#ifdef CONFIG_IGNORE_DTMF
	_log(LOG_INFO, "[AST] ignore outgoing DTMF end");
	return 0;
#else
	struct dev_t *dev = ast_channel_tech_pvt(ast);
	struct call_t* call;
	int res;

	if(incall_is_stat(dev, incall_stat_none)) {
#ifdef CONFIG_USE_INBOUND_DTMF
		_log(LOG_INFO, "[AST] got digit end callback (digit='%c',duration=%d) [INBOUND]", digit, duration);

		res = -1;
#else

		_log(LOG_INFO, "[AST] got digit end callback (digit='%c',duration=%d) [OUTBOUND]", digit, duration);
		call = cc_get_active_dtmf_call(dev);
		if(!call) {
			_log(LOG_ERR, "[AST] no active call found - digit end");
			goto err;
		}

		rdb_stop_dtmf(dev, call->cid);
		res = 0;
#endif
	} else {
		_log(LOG_DEBUG, "[AST] no call mode, ignore INBOUND or OUTBOUND end DTMF (digit='%c'", digit);
		res = 0;
	}


	ast_mutex_lock(&dev->lock);
	{

		switch(digit) {
#ifdef CONFIG_USE_ASTERISK_AS_HOOKFLASH
			case DTMF_DIGIT_HOOKFLASH:
				_log(LOG_DEBUG, "[INCALL] hookflash dtmf detected");
				do_hookflash(dev);
				break;
#endif

			case DTMF_DIGIT_END:
				if(incall_is_stat(dev, incall_stat_dial) || incall_is_stat(dev, incall_stat_dialing)) {
					_log(LOG_DEBUG, "[INCALL] digit end dtmf detected");
					dtmf_dial(dev);
				}
				break;

			default:
				if(incall_is_stat(dev, incall_stat_dial) || incall_is_stat(dev, incall_stat_dialing)) {
					incall_set_stat(dev, incall_stat_dialing);
					dtmf_add(dev, digit);
				}
		}

		ast_mutex_unlock(&dev->lock);
	}

	return res;

err:
	return -1;
#endif // CONFIG_IGNORE_DTMF
}

static const char* get_pcm_stat_str(int stat)
{
	const char* pcm_str_tbl[] = {
		[SND_PCM_STATE_OPEN] = "open",
		[SND_PCM_STATE_SETUP] = "setup",
		[SND_PCM_STATE_PREPARED] = "prepared",
		[SND_PCM_STATE_RUNNING] = "running",
		[SND_PCM_STATE_XRUN] = "xrun",
		[SND_PCM_STATE_DRAINING] = "draining",
		[SND_PCM_STATE_PAUSED] = "paused",
		[SND_PCM_STATE_SUSPENDED] = "suspended",
		[SND_PCM_STATE_DISCONNECTED] = "disconnected",
	};

	static char unknown_stat_str[64];

	/* return name immediately if stat is in the range */
	if(!(stat < 0) && stat < __countof(pcm_str_tbl)) {
		return pcm_str_tbl[stat];
	}

	snprintf(unknown_stat_str, sizeof(unknown_stat_str), "unknown#%d", stat);
	return unknown_stat_str;
}


static int _ast_write(struct ast_channel *ast, struct ast_frame *f)
{
#ifdef CONFIG_NOAUDIOCAPTURE
	return 0;
#else
	struct dev_t *dev = ast_channel_tech_pvt(ast);

	int len;
	int r = 0;

	int state;

#ifdef VERBOSE_DEBUG_PLAYBACK
	static unsigned long long last_write = 0;
	unsigned long long new_write = get_monotonic_time();

	_log(LOG_DEBUG, "_ast_write called (datalen=%d,tm=%llu %llu ms)", f->datalen, get_monotonic_time(), new_write - last_write);
	last_write = new_write;
#endif

	ast_mutex_lock(&dev->lock);

	/* bypass if no auido */
	if(!dev->alsa_ocard) {
#ifdef VERBOSE_DEBUG_PLAYBACK
		_log(LOG_DEBUG, "PCM playback device not initiated");
#endif
		goto fini;
	}

	/* check overflow */
	if(f->datalen > dev->pcm_write_buf_size) {
		_log(LOG_WARNING, "write buffer overflow (left=%d,datalen=%d)\n", dev->pcm_write_buf_size, f->datalen);
		r = -1;
		goto fini;
	}

	/* add frame to write buffer */
	memcpy(dev->pcm_write_buf, f->data.ptr, f->datalen);
	len = f->datalen;


	/* check xrun */
	state = snd_pcm_state(dev->alsa_ocard);
	if((state != SND_PCM_STATE_PREPARED) && (state != SND_PCM_STATE_RUNNING)) {
		_log(LOG_DEBUG, "XRUN in PCM playback (state=%s)", get_pcm_stat_str(state));
		snd_pcm_prepare(dev->alsa_ocard);
	}

	unsigned long long stime;
	unsigned long long ptime;

	/* get start time */
	stime = get_monotonic_time();

	/* write to pcm */
	r = -1;

	while(1) {
		/* write into pcm */
		r = snd_pcm_writei(dev->alsa_ocard, dev->pcm_write_buf, len / 2);

		/* bypass if time expired */
		ptime = get_monotonic_time();
		if(!(ptime - stime < dev->receive_frame_period))
			break;

		/* bypass if not EAGAIN */
		if(r != -EAGAIN)
			break;

		ast_mutex_unlock(&dev->lock);
		usleep(1);
		ast_mutex_lock(&dev->lock);

		/* bypass if no call established */
		if(cc_is_empty(dev) || !dev->alsa_ocard) {
			_log(LOG_DEBUG, "no voice playback device available, skip snd_pcm_writei()\n");
			goto fini;
		}
	}

	if(r == -EPIPE) {
		_log(LOG_DEBUG, "got -EPIPE in playback PCM device, skip (tm=%llu)", get_monotonic_time());
		snd_pcm_prepare(dev->alsa_ocard);
		r = 0;
		goto fini;
	} else if(r == -ESTRPIPE) {
		_log(LOG_ERR, "got -ESTRPIPE in playback PCM device");
		goto fini;
	} else if(r == -EAGAIN) {
		r = 0;
		_log(LOG_ERR, "got -EAGAIN in playback PCM device, ignore");
		goto fini;
	} else if(r < 0) {
		_log(LOG_ERR, "got %d in playback PCM device - %s", r, snd_strerror(r));
		goto fini;
	}

#ifdef VERBOSE_DEBUG_PLAYBACK
	_log(LOG_DEBUG, "got to send (tm=%llu)", get_monotonic_time());
#endif
fini:
	ast_mutex_unlock(&dev->lock);
	return (r < 0) ? r : 0;
#endif
}

static struct ast_frame *_ast_read(struct ast_channel *ast)
{
	struct ast_frame* f;

	struct dev_t *dev = ast_channel_tech_pvt(ast);

#ifdef CONFIG_NOAUDIOCAPTURE
	/* nothing to do */
#else

	int r = 0;

	unsigned int left;

#endif

	short *buf;
	int recv_frame;

	int state;

#ifdef VERBOSE_DEBUG_RECORD
	static unsigned long long last_read = 0;
	unsigned long long new_read = get_monotonic_time();

	_log(LOG_DEBUG, "_ast_read called (pos=%d,ch_stat=%d,fd=%d,p=0x%p,tm=%llu %llu ms)", dev->pcm_read_buf_pos, ast_channel_state(ast), dev->readdev, dev->alsa_icard, get_monotonic_time(), new_read - last_read);
	last_read = new_read;
#endif

	ast_mutex_lock(&dev->lock);

	/* get read frame */
	f = &(dev->read_frame);

	/* build NULL frame */
	f->frametype = AST_FRAME_NULL;
	f->subclass.integer = 0;
	f->samples = 0;
	f->datalen = 0;
	f->data.ptr = NULL;
	f->offset = 0;
	f->src = "chan_qmirdb";
	f->mallocd = 0;
	f->delivery.tv_sec = 0;
	f->delivery.tv_usec = 0;

	/* return silent frame if no audio capture */
#ifdef CONFIG_NOAUDIOCAPTURE
	recv_frame = dev->receive_frame_size;
	buf = (short*)(dev->pcm_read_buf + AST_FRIENDLY_OFFSET);

	memset(dev->pcm_read_buf, 0, recv_frame * 2 + AST_FRIENDLY_OFFSET);
#else

	/* bypass if no auido */
	if(!dev->alsa_icard) {
#ifdef VERBOSE_DEBUG_RECORD
		_log(LOG_DEBUG, "PCM record device not initiated");
#endif
		goto fini;
	}

	/* prepare if required */
	state = snd_pcm_state(dev->alsa_icard);
	if((state != SND_PCM_STATE_PREPARED) && (state != SND_PCM_STATE_RUNNING)) {
		_log(LOG_DEBUG, "XRUN in PCM record (state=%s)", get_pcm_stat_str(state));
		snd_pcm_prepare(dev->alsa_icard);
	}

	/* get read buffer */
	buf = (short*)(dev->pcm_read_buf + AST_FRIENDLY_OFFSET);


	int retried = 0;

	unsigned long long stime;
	unsigned long long ptime;

retry_to_read:
	/* get start time */
	stime = get_monotonic_time();

	/* reset read buffer position */
	dev->pcm_read_buf_pos = 0;

	while(1) {
		/* update positions */
		left = dev->receive_frame_size - dev->pcm_read_buf_pos;

		/* capture */
		r = snd_pcm_readi(dev->alsa_icard, buf + dev->pcm_read_buf_pos, left);

		/* bypass if time expired */
		ptime = get_monotonic_time();
		if(!(ptime - stime < dev->receive_frame_period))
			break;

		/* bypass if not EAGAIN */
		if(r != -EAGAIN)
			break;

		_log(LOG_DEBUG, "got -EAGAIN in record PCM device, retry");

		ast_mutex_unlock(&dev->lock);
		usleep(1);
		ast_mutex_lock(&dev->lock);

		/* bypass if no call established */
		if(cc_is_empty(dev) || !dev->alsa_icard) {
			_log(LOG_DEBUG, "no voice record device available, skip snd_pcm_readi()\n");
			goto fini;
		}

	}

	/* retry once more if -EPIPE */
	if((r == -EPIPE) && !retried) {
		retried = 1;

		_log(LOG_DEBUG, "got -EPIPE in record PCM device, retry");
		snd_pcm_prepare(dev->alsa_icard);
		goto retry_to_read;
	}

	if(r == -EPIPE) {
		_log(LOG_DEBUG, "got -EPIPE in record PCM device, skip (tm=%llu)", get_monotonic_time());
		snd_pcm_prepare(dev->alsa_icard);
		goto fini;
	} else if(r == -ESTRPIPE) {
		_log(LOG_ERR, "got -ESTRPIPE in record PCM device, skip");
		goto fini;
	} else if(r == -EAGAIN) {
		_log(LOG_ERR, "got -EAGAIN in record PCM device, ignore");
		goto fini;
	} else if(r < 0) {
		_log(LOG_ERR, "got %d in record PCM device - %s", r, snd_strerror(r));
		goto fini;
	}

	dev->pcm_read_buf_pos += r;

	/* get recv frame size */
	recv_frame = dev->pcm_read_buf_pos;

	/* return null frame if down or mute */
	if(mute) {
#ifdef VERBOSE_DEBUG_RECORD
		_log(LOG_DEBUG, "mute, insert silence\n");
#endif

		goto fini;
	}
#endif

	/* build a real frame */
	f->frametype = AST_FRAME_VOICE;
	f->subclass.format = DEVICE_FRAME_FORMAT;
	f->samples = recv_frame;
	f->datalen = recv_frame * 2;
	f->data.ptr = buf;
	f->offset = AST_FRIENDLY_OFFSET;
	f->src = "chan_qmirdb";
	f->mallocd = 0;

	/* start remote ring back if any packet is received */
	//incall_set_peer_ringback(dev,peer_ringback_remote);

#ifdef VERBOSE_DEBUG_RECORD
	_log(LOG_DEBUG, "got %d received (tm=%llu)", recv_frame, get_monotonic_time());
#endif

#ifdef CONFIG_NOAUDIOCAPTURE
	/* nothing to do */
#else
fini:
#endif
	ast_mutex_unlock(&dev->lock);

	return f;
}

static int _ast_answer(struct ast_channel *ast)
{
	struct dev_t *dev;
	struct call_t *call;

	_log(LOG_INFO, "[AST] got answer callback");

	/* get device */
	dev = ast_channel_tech_pvt(ast);

	ast_mutex_lock(&dev->lock);
	{
		call = AST_LIST_FIRST(&dev->calls);

		if(call && call->incoming) {
			rdb_answer(dev, call->cid);
			/* apply to call statistics */
			cc_answer_mt_call(dev);
		}
		ast_mutex_unlock(&dev->lock);
	}


	return 0;
}

static int _ast_call(struct ast_channel *ast, const char *dest, int timeout)
{
	struct dev_t *dev;
	char *dest_dev;
	char *dest_num = NULL;
	int stat = -1;

	_log(LOG_INFO, "[AST] got call callback (dest=%s,to=%d)", dest, timeout);

	/* get dev */
	dev = ast_channel_tech_pvt(ast);

	/* get dest. num. */
	dest_dev = ast_strdupa(dest);
	dest_num = strchr(dest_dev, '/');
	if(!dest_num) {
		_log(LOG_WARNING, "cant determine destination number.");
		goto err;
	}
	*dest_num++ = 0x00;

	/* bypass if channel is not available */
	if((ast_channel_state(ast) != AST_STATE_DOWN) && (ast_channel_state(ast) != AST_STATE_RESERVED)) {
		_log(LOG_WARNING, "_ast_call() called on %s, neither down nor reserved", ast_channel_name(ast));
		goto err;
	}

	_log(LOG_DEBUG, "calling %s on %s", dest, ast_channel_name(ast));

	ast_mutex_lock(&dev->lock);
	{
		stat = cc_make_mo_call(dev, dest_num, timeout, 0);
		ast_mutex_unlock(&dev->lock);
	}

	return stat;

err:
	return -1;
}

static struct ast_channel *_ast_create_chn(int state, struct dev_t *dev, char *cid_num, const char *cid_name, const struct ast_assigned_ids *assignedids, const struct ast_channel *requestor)
{
	struct ast_channel *chn;
	const char* dev_id;


	char* new_cid_num = cid_num;

#if 0
	/* [TODO] do alignment, smoother and dsp */

	dev->alignment_count = 0;
	dev->alignment_detection_triggered = 0;

	if(dev->adapter->alignment_detection)
		dev->do_alignment_detection = 1;
	else
		dev->do_alignment_detection = 0;


	ast_smoother_reset(dev->smoother, DEVICE_FRAME_SIZE);
	ast_dsp_digitreset(dev->dsp);
#endif

	_log(LOG_DEBUG, "[caller-id] dev->id = %s", dev->id);
	_log(LOG_DEBUG, "[caller-id] cid_num = %s", cid_num);
	_log(LOG_DEBUG, "[caller-id] dev->context = %s", dev->context);

#ifdef DIALPLAN_usa_att
	/* AT&T special CID requirement */
	if(cid_num && cid_num[0] == '+') {
		char* prefix = "";
		int cid_size;
		char* cid;
		char* cid_naked = cid_num;

		switch(cid_num[1]) {
			case '1':
				cid_naked = &cid_num[2];
				break;

			default:
				cid_naked = &cid_num[1];
				prefix = "011";
				break;
		}

		/* allocate cid buffer */
		cid_size = strlen(prefix) + strlen(cid_naked) + 1;
		cid = ast_alloca(cid_size);

		/* generate new cid */
		snprintf(cid, cid_size, "%s%s", prefix, cid_naked);
		new_cid_num = cid;

		_log(LOG_DEBUG, "[caller-id] plus(+) detected, convert CID ('%s' ==> '%s')", cid_num, new_cid_num);
	}

#endif


	/* use cid as dev_id to override uri display name */
	dev_id = cid_name ? cid_name : "";

	/* allocate channel */
	chn = ast_channel_alloc(1, state, new_cid_num, dev_id, 0, 0, dev->context,
	                        assignedids, requestor, 0,
	                        "QMIRDB/%s-%04lx", dev->id, ast_random() & 0xffff);
	if(!chn) {
		_log(LOG_ERR, "channel allocation failed (dev=%s,cid_num=%s)", dev->id, new_cid_num ? new_cid_num : "");
		goto err;
	}

	/* initiate channel */
	ast_channel_tech_set(chn, &qmirdb_tech);

	ast_channel_set_readformat(chn, DEVICE_FRAME_FORMAT);
	ast_channel_set_writeformat(chn, DEVICE_FRAME_FORMAT);
	ast_channel_nativeformats_set(chn, qmirdb_tech.capabilities);
#if 0
	ast_channel_set_rawreadformat(chn, DEVICE_FRAME_FORMAT);
	ast_channel_set_rawwriteformat(chn, DEVICE_FRAME_FORMAT);
#endif

	ast_channel_tech_pvt_set(chn, dev);

	/* */
	if(state == AST_STATE_RING) {
		_log(LOG_INFO, "[AST] post a RING");
		ast_channel_rings_set(chn, 1);
	}

	ast_channel_language_set(chn, "en");
	dev->owner = chn;

	ast_channel_unlock(chn);

	return chn;

err:
	return NULL;
}

static struct ast_channel *_ast_request(const char *type, struct ast_format_cap *cap, const struct ast_assigned_ids *assignedids, const struct ast_channel *requestor, const char *data, int *cause)
{
	struct ast_channel *chn = NULL;
	struct dev_t *dev;
	char *dest_dev = NULL;
	char *dest_num = NULL;

	_log(LOG_INFO, "[AST] got request callback");

	_log(LOG_DEBUG, "requestor tech type : %s", ast_channel_tech(requestor)->type);
	// PJSIP

	/* check data */
	if(!data) {
		_log(LOG_WARNING, "channel requested with no data");
		*cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
		goto err;
	}

	/* check format */
	if(ast_format_cap_iscompatible_format(cap, DEVICE_FRAME_FORMAT) == AST_FORMAT_CMP_NOT_EQUAL) {
		struct ast_str *codec_buf = ast_str_alloca(64);
		_log(LOG_WARNING, "asked to get a channel of unsupported format '%s'", ast_format_cap_get_names(cap, &codec_buf));
		*cause = AST_CAUSE_FACILITY_NOT_IMPLEMENTED;
		goto err;
	}

	/* get dest. num. */
	dest_dev = ast_strdupa(data);
	dest_num = strchr(dest_dev, '/');
	if(dest_num)
		*dest_num++ = 0x00;

	/* find requested device */
	dev = _find_dev_by_id(dest_dev);

	/* bypass if not found, not connected or already owned */
	if(!dev || dev->owner) {
		_log(LOG_WARNING, "request to call on device %s which is already in use.", dest_dev);
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		goto err;
	}

	/* create qmi channel */
	ast_mutex_lock(&dev->lock);
	{
		chn = _ast_create_chn(AST_STATE_DOWN, dev, NULL, NULL, assignedids, requestor);

		ast_mutex_unlock(&dev->lock);
	}

	if(!chn) {
		_log(LOG_WARNING, "Unable to allocate channel structure.");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		return NULL;
	}

	return chn;

err:
	return NULL;
}

static void close_main_pcm_devices()
{
	if(main_readdev > 0)
		close(main_readdev);
	if(main_writedev > 0)
		close(main_writedev);

	if(main_icard)
		snd_pcm_close(main_icard);
	if(main_ocard)
		snd_pcm_close(main_ocard);

	main_ocard = NULL;
	main_icard = NULL;

	main_readdev = 0;
	main_writedev = 0;
}

static int open_main_pcm_devices()
{
	struct dev_t main_dev;

	/* init main dev */
	main_dev.sampling_frequency = DEFAULT_SAMPLING_FREQUENCY;
	main_dev.period_frame_size = TO_FRAME_SIZE(main_dev.sampling_frequency, DEFAULT_PERIOD_FRAME_SIZE);
	main_dev.playback_buffer_size = TO_FRAME_SIZE(main_dev.sampling_frequency, DEFAULT_PLAYBACK_BUFFER_SIZE);

	_log(LOG_INFO, "main_dev.period_frame_size=%d", main_dev.period_frame_size);
	_log(LOG_INFO, "main_dev.playback_buffer_size=%d", main_dev.playback_buffer_size);

	if(main_icard_dev_name) {
		_log(LOG_INFO, "initiate main pcm icards (icard=%s)", main_icard_dev_name);

		main_icard = alsa_card_init(&main_dev, main_icard_dev_name, SND_PCM_STREAM_CAPTURE, &main_readdev);
		if(!main_icard) {
			_log(LOG_ERR, "failed to initate main icard (icard=%s)", main_icard_dev_name);
			goto err;
		}

		fcntl(main_readdev, F_SETFD, FD_CLOEXEC);
	}

	if(main_ocard_dev_name) {
		_log(LOG_INFO, "initiate main pcm ocards (ocard=%s)", main_ocard_dev_name);

		main_ocard = alsa_card_init(&main_dev, main_ocard_dev_name, SND_PCM_STREAM_PLAYBACK, &main_writedev);
		if(!main_ocard) {
			_log(LOG_ERR, "failed to initate main ocard (ocard=%s)", main_ocard_dev_name);
			goto err;
		}

		fcntl(main_writedev, F_SETFD, FD_CLOEXEC);
	}

	return 0;

err:
	return -1;
}

/*
 Check to see if there is a MT or MO call in all of devices.

 Parameters:
  N/A.

 Return:
  True is returned if there is any MT or MO call. Otherwise, zero is returned.
*/
static int is_any_call_existing()
{
	struct dev_t* dev;
	int call_existing = 0;

	/* Destroy the device list */
	AST_RWLIST_WRLOCK(&devices);
	{
		AST_RWLIST_TRAVERSE(&devices, dev, entry) {
			ast_mutex_lock(&dev->lock);
			{
				call_existing |= !cc_is_empty(dev);
				ast_mutex_unlock(&dev->lock);
			}
		}
		AST_RWLIST_UNLOCK(&devices);
	}

	return call_existing;
}

/*
 Prepare to unload Asterisk channel driver. Mainly the function disconnects all calls.

 Parameters:
  N/A.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int prepare_to_unload()
{
	struct dev_t* dev;

	/* hang up all calls in each device */
	AST_RWLIST_RDLOCK(&devices);
	{
		AST_RWLIST_TRAVERSE(&devices, dev, entry) {
			ast_mutex_lock(&dev->lock);
			{
				rdb_hangupall(dev);
				ast_mutex_unlock(&dev->lock);
			}
		}

		AST_RWLIST_UNLOCK(&devices);
	}

	return 0;
}

/*
 Wait until all of MT or MO calls are disconnected.

 Parameters:
  N/A.

 Return:
  If QMI Asterisk channel module is ready to unload, zero is returned. Otherwise, -1.
*/
static int wait_for_devices()
{
	int i;
	int ready_to_unload = 0;

	/* wait until no device is online */
	i = 0;
	while(i++ < MAX_WAITING_TIME_FOR_UNLOADING) {
		_log(LOG_DEBUG, "waiting until device disconnects calls #%d", i);

		/* check to see if it is ready to unload */
		ready_to_unload = !is_any_call_existing();
		if(ready_to_unload)
			break;

		sleep(1);
	}

	/* return error if not ready to unload */
	if(!ready_to_unload)
		return -1;

	return 0;
}

/*
 Unload devices.

 Parameters:
  N/A.

 Return:
  N/A.
*/
static void unload_devices()
{
	struct dev_t* dev;
	struct ast_channel* ast;

	/* Destroy the device list */
	AST_RWLIST_WRLOCK(&devices);
	{
		while((dev = AST_RWLIST_REMOVE_HEAD(&devices, entry))) {
			ast_mutex_lock(&dev->lock);
			{
				_log(LOG_DEBUG, "[unload] stop soundcard");
				soundcard_stop_and_fini(dev);

				_log(LOG_DEBUG, "[unload] disconnect association between device and channel");
				/* get asterisk channel of device */
				ast = dev->owner;
				/* disconnect association between device and channel */
				dev->owner = NULL;
				if(ast)
					ast_channel_tech_pvt_set(ast, NULL);

				_log(LOG_DEBUG, "[unload] free device elements");
				ast_free(dev->rdb_prefix);
				ast_free(dev->icard_dev_name);
				ast_free(dev->ocard_dev_name);
				AST_LIST_HEAD_DESTROY(&dev->calls);

				ast_mutex_unlock(&dev->lock);
			}

			ast_mutex_destroy(&dev->lock);
			_log(LOG_DEBUG, "[unload] free device");
			ast_free(dev);
		}
		AST_RWLIST_UNLOCK(&devices);
	}

	close_main_pcm_devices();

	ast_free(main_ocard_dev_name);
	ast_free(main_icard_dev_name);

	main_ocard_dev_name = NULL;
	main_icard_dev_name = NULL;
}

static void stop_devices()
{
	struct dev_t* dev;

	AST_RWLIST_RDLOCK(&devices);
	{
		AST_RWLIST_TRAVERSE(&devices, dev, entry) {
			ast_mutex_lock(&dev->lock);
			{
				rwpipe_destroy(dev->pp);
				free(dev->rdb_ctrl);
				free(dev->rdb_noti);
				free(dev->rdb_mwi);
				free(dev->rdb_mwi_count);

				ast_mutex_unlock(&dev->lock);
			}
		}
		AST_RWLIST_UNLOCK(&devices);
	}

	_log(LOG_DEBUG, "close Netcomm RDB");
	rdb_close(&_s);
}

static int __rdb_subscribe(const char* rdb)
{
	rdb_create_string(_s, rdb, "", 0, 0);

	if(rdb_subscribe(_s, rdb) < 0)  {
		_log(LOG_ERR, "cannot subscribe variable (rdb=%s) - %s", rdb, strerror(errno));
		goto err;
	}

	return 0;

err:
	return -1;
}

static int update_mwi_mailbox(const char* mailbox_id, int num_new, int num_old)
{
	/*
		wwan.0.mwi.voicemail.active     -
		wwan.0.mwi.voicemail.count      -
	*/
	struct ast_mwi_mailbox_object *mailbox = NULL;

	mailbox = ast_mwi_mailbox_alloc(mailbox_id);
	if(mailbox) {
		ast_mwi_mailbox_set_msgs_new(mailbox, num_new);
		ast_mwi_mailbox_set_msgs_old(mailbox, num_old);
		if(ast_mwi_mailbox_update(mailbox)) {
			_log(LOG_ERR, "could not update mailbox %s", ast_sorcery_object_get_id(mailbox));
			goto err;
		}

		_log(LOG_DEBUG, "updated mailbox %s", ast_sorcery_object_get_id(mailbox));
		ast_mwi_mailbox_unref(mailbox);
	}

	return 0;

err:
	if(mailbox)
		ast_mwi_mailbox_unref(mailbox);

	return -1;
}

static int start_devices()
{
	struct dev_t* dev;

	char rdb[RDB_MAX_VAL_LEN];

	struct dbenum_t* dbenum;
	struct dbenumitem_t* item;
	int total_rdbs;

	int stat = 0;

	AST_RWLIST_RDLOCK(&devices);

	/* open rdb */
	_log(LOG_DEBUG, "open Netcomm RDB");
	if(rdb_open(NULL, &_s) < 0) {
		_log(LOG_ERR, "cannot open Netcomm RDB");
		goto err;
	}

	/* init rdbs */
#ifdef CONFIG_USE_STATISTICS
	init_rdbs();
#endif

	/* reset voice call status rdb */
	dbenum = dbenum_create(_s, 0);
	if(dbenum) {
#define VOICE_CALL_STATUS_PREFIX "voice_call"
#define VOICE_CALL_STATISTICS_PREFIX "voice_call.statistics"

		_log(LOG_DEBUG, "search voice call status RDB to reset (prefix=%s)", VOICE_CALL_STATUS_PREFIX);

		total_rdbs = dbenum_enumDbByNames(dbenum, VOICE_CALL_STATUS_PREFIX);

		_log(LOG_DEBUG, "init. voice status RDBs (total_rdbs=%d)", total_rdbs);

		item = dbenum_findFirst(dbenum);
		while(item) {
			if(!strncmp(item->szName, VOICE_CALL_STATISTICS_PREFIX, strlen(VOICE_CALL_STATISTICS_PREFIX))) {
				/* nothing to do */
			} else if(!strncmp(item->szName, VOICE_CALL_STATUS_PREFIX, strlen(VOICE_CALL_STATUS_PREFIX))) {


				if(rdb_set_string(_s, item->szName, "") < 0) {
					_log(LOG_ERR, "rdb_set_string(%s) failed in start_devices() - %s", item->szName, strerror(errno));
				} else {
					_log(LOG_DEBUG, "reset RDB [%s]", item->szName);
				}
			}

			item = dbenum_findNext(dbenum);
		}

		dbenum_destroy(dbenum);
	}


	/* init. RDB channel */

	AST_RWLIST_TRAVERSE(&devices, dev, entry) {
		ast_mutex_lock(&dev->lock);

		dev->pp = rwpipe_create(dev->timeout, NULL);
		if(!dev->pp) {
			_log(LOG_ERR, "cannot create rwpipe (dev=%s)", dev->id);
			goto err_in_loop;
		}

		/* duplicate ctrl */
		snprintf(rdb, sizeof(rdb), "%s.%s", dev->rdb_prefix, "voice.command." QMIRDBCTRL_CTRL);
		dev->rdb_ctrl = strdup(rdb);
		if(!dev->rdb_ctrl) {
			_log(LOG_ERR, "failed strup(dev=%s,rdb=%s)", dev->id, rdb);
			goto err_in_loop;
		}

		/* clear any previous command before subscribing */
		_rdb_set_str(dev->rdb_ctrl, "");

		/* subscribe ctrl */
		_log(LOG_DEBUG, "subscribe rdb(%s)", dev->rdb_ctrl);
		if(__rdb_subscribe(dev->rdb_ctrl) < 0)  {
			_log(LOG_ERR, "cannot subscribe variable (rdb=%s) - %s", rdb, strerror(errno));
			goto err_in_loop;
		}

		/* duplicate noti */
		snprintf(rdb, sizeof(rdb), "%s.%s", dev->rdb_prefix, "voice.command." QMIRDBCTRL_NOTI);
		dev->rdb_noti = strdup(rdb);
		if(!dev->rdb_noti) {
			_log(LOG_ERR, "failed strup(dev=%s,rdb=%s)", dev->id, rdb);
			goto err_in_loop;
		}

		/* subscribe noti */
		_log(LOG_DEBUG, "subscribe rdb(%s)", dev->rdb_noti);
		if(__rdb_subscribe(dev->rdb_noti) < 0)  {
			_log(LOG_ERR, "cannot subscribe variable (rdb=%s) - %s", rdb, strerror(errno));
			goto err_in_loop;
		}

		/* build mwi count */
		snprintf(rdb, sizeof(rdb), "%s.%s", dev->rdb_prefix, RDB_MWI_VOICEMAIL_COUNT);
		dev->rdb_mwi_count = strdup(rdb);
		if(!dev->rdb_mwi_count) {
			_log(LOG_ERR, "failed strup(dev=%s,rdb=%s)", dev->id, rdb);
			goto err_in_loop;
		}

		/* build mwi */
		snprintf(rdb, sizeof(rdb), "%s.%s", dev->rdb_prefix, RDB_MWI_VOICEMAIL_ACTIVE);
		dev->rdb_mwi = strdup(rdb);
		if(!dev->rdb_mwi) {
			_log(LOG_ERR, "failed strup(dev=%s,rdb=%s)", dev->id, rdb);
			goto err_in_loop;
		}

		/* subscribe noti */
		_log(LOG_DEBUG, "subscribe rdb(%s)", dev->rdb_mwi);
		if(__rdb_subscribe(dev->rdb_mwi) < 0)  {
			_log(LOG_ERR, "cannot subscribe variable (rdb=%s) - %s", rdb, strerror(errno));
			goto err_in_loop;
		}

#ifdef CONFIG_USE_STATISTICS
		/* subscribe noti */
		_log(LOG_DEBUG, "subscribe rdb(%s)", RDB_VOICE_CALL_STATISTICS_RESET);
		if(__rdb_subscribe(RDB_VOICE_CALL_STATISTICS_RESET) < 0)  {
			_log(LOG_ERR, "cannot subscribe variable (rdb=%s) - %s", RDB_VOICE_CALL_STATISTICS_RESET, strerror(errno));
			goto err_in_loop;
		}
#endif

		ast_mutex_unlock(&dev->lock);
		continue;

err_in_loop:
		stat = -1;
		ast_mutex_unlock(&dev->lock);
		break;
	}

	AST_RWLIST_UNLOCK(&devices);
	return stat;

err:
	AST_RWLIST_UNLOCK(&devices);
	return -1;
}

static int load_devices(struct ast_config *cfg)
{
	struct dev_t* dev;
	const char* rdb_prefix;
	const char* icard_dev_name;
	const char* ocard_dev_name;
	const char* context;
	const char* timeout;
	const char *cat;

	/* enable voice mixer */
	enable_voice_mixer();

	/* TODO: QC initial PCM open fails due to unknown reason */
	open_main_pcm_devices();
	close_main_pcm_devices();

	/* open main pcm */
	if(open_main_pcm_devices() < 0) {
		_log(LOG_ERR, "failed top initiate main pcm devices #2");
		goto err;
	}

	/* enable host pcm interface */
	enable_host_pcm_interface();

	/* now load devices */
	for(cat = ast_category_browse(cfg, NULL); cat; cat = ast_category_browse(cfg, cat)) {
		if(!strcasecmp(cat, "general")) {
			continue;
		}

		/* create and initialize our dev structure */
		if(!(dev = ast_calloc(1, sizeof(*dev)))) {
			_log(LOG_ERR, "skipping device %s. error allocating memory for dev.", cat);
			goto err;
		}
		ast_mutex_init(&dev->lock);

		AST_LIST_HEAD_INIT(&dev->calls);

		/* read section name as id */
		ast_copy_string(dev->id, cat, sizeof(dev->id));

		/* read rdb prefix */
		rdb_prefix = ast_variable_retrieve(cfg, cat, "rdb_prefix");
		if(ast_strlen_zero(rdb_prefix)) {
			_log(LOG_ERR, "skipping device %s. no rdb_prefix specified.", cat);
			continue;
		}

		/* store rdb prefix */
		if((dev->rdb_prefix = ast_strdup(rdb_prefix)) == NULL) {
			_log(LOG_ERR, "skipping device %s. error allocating memory for rdb.", cat);
			continue;
		}

		/* read timeout */
		timeout = ast_variable_retrieve(cfg, cat, "timeout");
		if(!ast_strlen_zero(timeout))
			dev->timeout = atoi(timeout);
		if(!dev->timeout)
			dev->timeout = QMIRDBCTRL_TIMEOUT;

		/* read context */
		context = ast_variable_retrieve(cfg, cat, "context");
		if(ast_strlen_zero(context))
			ast_copy_string(dev->context, "default", sizeof(dev->context));
		else
			ast_copy_string(dev->context, context, sizeof(dev->context));


		/* read icard_dev_name */
		icard_dev_name = ast_variable_retrieve(cfg, cat, "icard_dev_name");
		if(ast_strlen_zero(icard_dev_name)) {
			_log(LOG_ERR, "skipping device %s. no icard_dev_name specified.", cat);
			continue;
		}

		/* store icard_dev_name */
#ifdef CONFIG_USE_INCALL_PCM
		if((dev->icard_dev_name = ast_strdup("hw:0,0")) == NULL) {
#else
		if((dev->icard_dev_name = ast_strdup(icard_dev_name)) == NULL) {
#endif
			_log(LOG_ERR, "skipping device %s. error allocating memory for rdb.", cat);
			continue;
		}

		/* read ocard_dev_name */
		ocard_dev_name = ast_variable_retrieve(cfg, cat, "ocard_dev_name");
		if(ast_strlen_zero(ocard_dev_name)) {
			_log(LOG_ERR, "skipping device %s. no ocard_dev_name specified.", cat);
			continue;
		}

		/* store ocard_dev_name */
#ifdef CONFIG_USE_INCALL_PCM
		if((dev->ocard_dev_name = ast_strdup("hw:0,13")) == NULL) {
#else
		if((dev->ocard_dev_name = ast_strdup(ocard_dev_name)) == NULL) {
#endif
			_log(LOG_ERR, "skipping device %s. error allocating memory for rdb.", cat);
			continue;
		}

		/* read main settings */
		dev->sampling_frequency = main_sampling_frequency;
		dev->receive_frame_size = main_receive_frame_size;
		dev->receive_frame_period = main_receive_frame_period;
		dev->period_frame_size = main_period_frame_size;
		dev->playback_buffer_size = main_playback_buffer_size;

		_log(LOG_INFO, "* pcm device configuration summary (device %s)", cat);
		_log(LOG_INFO, "sampling_frequency=%d", dev->sampling_frequency);
		_log(LOG_INFO, "receive_frame_size=%d", dev->receive_frame_size);
		_log(LOG_INFO, "period_frame_size=%d", dev->period_frame_size);
		_log(LOG_INFO, "playback_buffer_size=%d", dev->playback_buffer_size);

		/* add into list */
		AST_RWLIST_WRLOCK(&devices);
		{
			AST_RWLIST_INSERT_HEAD(&devices, dev, entry);
			AST_RWLIST_UNLOCK(&devices);
		}
	}

	return 0;

err:
	return -1;
}

static int load_config(void)
{
	struct ast_config *cfg;
	struct ast_flags config_flags = { 0 };
	struct ast_variable *v;

	/* open config */
	if(!(cfg = ast_config_load(config, config_flags)))
		goto err;

	/* parse [general] section */
	for(v = ast_variable_browse(cfg, "general"); v; v = v->next) {

		/* read icard_dev_name */
		if(!strcasecmp(v->name, "icard_dev_name")) {
			if((main_icard_dev_name = ast_strdup(v->value)) == NULL) {
				_log(LOG_ERR, "failed to allocate main icard dev name");
				goto err;
			}
		}

		/* read ocard_dev_name */
		if(!strcasecmp(v->name, "ocard_dev_name")) {
			if((main_ocard_dev_name = ast_strdup(v->value)) == NULL) {
				_log(LOG_ERR, "failed to allocate main ocard dev name");
				goto err;
			}
		}

		/* read receive_frame_size */
		if(!strcasecmp(v->name, "receive_frame_size")) {
			if(!ast_strlen_zero(v->value)) {
				main_receive_frame_size = TO_FRAME_SIZE(main_sampling_frequency, atoi(v->value));
				main_receive_frame_period = atoi(v->value);
			}
			if(!main_receive_frame_size) {
				main_receive_frame_size = TO_FRAME_SIZE(main_sampling_frequency, DEFAULT_RECEIVE_FRAME_SIZE);
				main_receive_frame_period = DEFAULT_RECEIVE_FRAME_SIZE;
			}
		}

		/* read period_frame_size */
		if(!strcasecmp(v->name, "period_frame_size")) {
			if(!ast_strlen_zero(v->value))
				main_period_frame_size = TO_FRAME_SIZE(main_sampling_frequency, atoi(v->value));
			if(!main_period_frame_size)
				main_period_frame_size = TO_FRAME_SIZE(main_sampling_frequency, DEFAULT_PERIOD_FRAME_SIZE);
		}

		/* read playback_buffer_size */
		if(!strcasecmp(v->name, "playback_buffer_size")) {
			if(!ast_strlen_zero(v->value))
				main_playback_buffer_size = TO_FRAME_SIZE(main_sampling_frequency, atoi(v->value));
			if(!main_playback_buffer_size)
				main_playback_buffer_size = TO_FRAME_SIZE(main_sampling_frequency, DEFAULT_PLAYBACK_BUFFER_SIZE);
		}

		/* interdigit */
		if(!strcasecmp(v->name, "incall_interdigit_delay")) {
			if(!ast_strlen_zero(v->value)) {
				incall_interdigit_delay = atoi(v->value);
				incall_initdigit_delay = atoi(v->value);
			}
			if(!incall_interdigit_delay) {
				incall_interdigit_delay = DEFAULT_INCALL_INTERDIGIT_DELAY;
				incall_initdigit_delay = DEFAULT_INCALL_INTERDIGIT_DELAY;
			}
		}

		/* initdigit */
		if(!strcasecmp(v->name, "incall_initdigit_delay")) {
			if(!ast_strlen_zero(v->value))
				incall_initdigit_delay = atoi(v->value);
			if(!incall_initdigit_delay)
				incall_initdigit_delay = DEFAULT_INCALL_INITDIGIT_DELAY;
		}

		/* timeout */
		if(!strcasecmp(v->name, "incall_call_timeout")) {
			if(!ast_strlen_zero(v->value))
				incall_call_timeout = atoi(v->value);
			if(!incall_call_timeout)
				incall_call_timeout = DEFAULT_INCALL_CALL_TIMEOUT;
		}

	}

	/* fixup playback buffer size */
	if(main_playback_buffer_size < main_period_frame_size || main_playback_buffer_size < main_receive_frame_size) {
		main_playback_buffer_size = MAX(main_period_frame_size, main_receive_frame_size);
	}

	/* load devices */
	_log(LOG_DEBUG, "load devices");
	if(load_devices(cfg) < 0) {
		_log(LOG_ERR, "failed to load devices");
		goto err;
	}

	/* destroy cfg */
	ast_config_destroy(cfg);

	_log(LOG_INFO, "* main pcm settings");
	_log(LOG_INFO, "main_icard_dev_name=%s", main_icard_dev_name);
	_log(LOG_INFO, "main_ocard_dev_name=%s", main_ocard_dev_name);
	_log(LOG_INFO, "main_receive_frame_size=%d", main_receive_frame_size);
	_log(LOG_INFO, "main_period_frame_size=%d", main_period_frame_size);
	_log(LOG_INFO, "main_playback_buffer_size=%d", main_playback_buffer_size);

	_log(LOG_INFO, "* incall settings");
	_log(LOG_INFO, "incall_initdigit_delay=%d", incall_initdigit_delay);
	_log(LOG_INFO, "incall_interdigit_delay=%d", incall_interdigit_delay);
	_log(LOG_INFO, "incall_call_timeout=%d", incall_call_timeout);


	return 0;

err:
	return -1;
}

static int _ast_hangup_with_dev(struct dev_t *dev)
{
	ast_hangup(dev->owner);
	return 0;
}

static int _ast_queue_hangup(struct dev_t *dev)
{
	struct ast_channel *owner;

	for(;;) {
		owner = dev->owner;
		if(owner) {
			if(ast_channel_trylock(owner)) {
				_deadlock_avoidance(&dev->lock);
			} else {
				if(dev->hangupcause != 0) {
					ast_channel_hangupcause_set(owner, dev->hangupcause);
				}
				ast_queue_hangup(owner);
				ast_channel_unlock(owner);
				break;
			}
		} else
			break;
	}
	return 0;
}

static int _ast_queue_frame(struct dev_t *dev, struct ast_frame* f)
{
	struct ast_channel *owner;

	for(;;) {
		owner = dev->owner;
		if(owner) {
			if(ast_channel_trylock(owner)) {
				_deadlock_avoidance(&dev->lock);
			} else {
				_log(LOG_DEBUG, "[queue frame] send");
				ast_queue_frame(owner, f);
				_log(LOG_DEBUG, "[queue frame] fini");
				ast_channel_unlock(owner);
				break;
			}
		} else
			break;
	}
	return 0;
}

/*
	cli functions
*/

static char *handle_cli_qmirdb_hangupall(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct dev_t *dev = NULL;

	switch(cmd) {
		case CLI_INIT:
			e->command = "qmirdb hangupall";
			e->usage =
			    "Usage: qmirdb call hangup <device ID>\n"
			    "       Send hangup to the device\n";
			return NULL;

		case CLI_GENERATE:
			return NULL;
	}

	if(a->argc != 3)
		return CLI_SHOWUSAGE;

	/* get device */
	dev = _find_dev_by_id(a->argv[2]);
	if(!dev) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_FAILURE;
	}

	ast_mutex_lock(&dev->lock);
	{
		rdb_manage_calls_end_all_calls(dev);
		//rdb_hangupall(dev);
		//rdb_manage_calls(dev,"end_all_calls",0,NULL);
		ast_mutex_unlock(&dev->lock);
	}

	return CLI_SUCCESS;
}

static char *handle_cli_qmirdb_show(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct dev_t *dev = NULL;
	struct call_t* call = NULL;

	switch(cmd) {
		case CLI_INIT:
			e->command = "qmirdb show";
			e->usage =
			    "Usage: qmirdb show\n"
			    "       show all information\n";
			return NULL;

		case CLI_GENERATE:
			return NULL;
	}

	AST_RWLIST_RDLOCK(&devices);
	{
		AST_RWLIST_TRAVERSE(&devices, dev, entry) {
			ast_mutex_lock(&dev->lock);
			{
				ast_cli(a->fd, "[%s]\n", dev->id);
				ast_cli(a->fd, "incall stat : %s\n", incall_get_stat_str(dev->incall_stat));


				AST_LIST_TRAVERSE(&dev->calls, call, entry) {
					ast_cli(a->fd, "\n");
					ast_cli(a->fd, "	[cid#%d]\n", call->cid);
					ast_cli(a->fd, "	clip : %s\n", call->clip);

					ast_cli(a->fd, "	outgoing : %d\n", call->outgoing);
					ast_cli(a->fd, "	progressed : %d\n", call->progressed);
					ast_cli(a->fd, "	ringing : %d\n", call->ringing);
					ast_cli(a->fd, "	connected : %d\n", call->connected);

					ast_cli(a->fd, "	incoming : %d\n", call->incoming);
					ast_cli(a->fd, "	answered : %d\n", call->answered);
					ast_cli(a->fd, "	invite : %d\n", call->invite);
					ast_cli(a->fd, "	callwaiting : %d\n", call->callwaiting);

					ast_cli(a->fd, "	held : %d\n", call->held);

					ast_cli(a->fd, "	conference : %d\n", call->conference);
					ast_cli(a->fd, "	emergency : %d\n", call->emergency);

				}


				ast_mutex_unlock(&dev->lock);
			}
		}

		AST_RWLIST_UNLOCK(&devices);
	}


	return CLI_SUCCESS;
}

static char *handle_cli_qmirdb_test(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct dev_t *dev = NULL;

	switch(cmd) {
		case CLI_INIT:
			e->command = "qmirdb test";
			e->usage =
			    "Usage: qmirdb test <device ID>\n"
			    "       test the device\n";
			return NULL;

		case CLI_GENERATE:
			return NULL;
	}

	if(a->argc != 3)
		return CLI_SHOWUSAGE;

	/* get device */
	dev = _find_dev_by_id(a->argv[2]);
	if(!dev) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_FAILURE;
	}


	ast_cli(a->fd, "start\n");

	ast_channel_lock(dev->owner);
	{
		int ostate = snd_pcm_state(dev->alsa_ocard);
		int istate = snd_pcm_state(dev->alsa_ocard);

		ast_cli(a->fd, "ocard=%d,icard=%d\n", ostate, istate);

		ast_channel_unlock(dev->owner);
	}

	ast_cli(a->fd, "done\n");

	return CLI_SUCCESS;
}

static char *handle_cli_qmirdb_hangup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct dev_t *dev = NULL;

	switch(cmd) {
		case CLI_INIT:
			e->command = "qmirdb hangup";
			e->usage =
			    "Usage: qmirdb hangup <device ID> <call ID>\n"
			    "       Send hangup to the device\n";
			return NULL;

		case CLI_GENERATE:
			return NULL;
	}

	if(a->argc != 4)
		return CLI_SHOWUSAGE;

	/* get device */
	dev = _find_dev_by_id(a->argv[2]);
	if(!dev) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_FAILURE;
	}

	int cid = atoi(a->argv[3]);

	ast_mutex_lock(&dev->lock);
	{
		struct call_t* call = cc_get_call_by_cid(dev, cid);
		if(!call) {
			ast_cli(a->fd, "cid %d not connected", cid);
			goto err;
		}

		if(!call->cid) {
			ast_cli(a->fd, "cid %d is being connected yet\n", cid);
			goto err;
		}

		rdb_hangup(dev, cid);

		ast_mutex_unlock(&dev->lock);
	}

	return CLI_SUCCESS;

err:
	ast_mutex_unlock(&dev->lock);
	return CLI_FAILURE;
}

static int do_hookflash(struct dev_t* dev)
{
	int call_count;
	int multiparties;
	int held;
	int stat;
	int emergency;

	struct call_t* call;

	_log(LOG_DEBUG, "[INCALL] do hook flash");

	_log(LOG_DEBUG, "[INCALL] channel=%x", dev->owner);

	/* get call information */
	call_count = cc_get_call_count(dev);
	held = cc_is_any_call_on_hold(dev);
	emergency = cc_is_any_emergency_call(dev);
	multiparties = call_count > 1;

	switch(incall_get_stat(dev)) {
		case incall_stat_none:

			if(emergency) {
				_log(LOG_DEBUG, "[INCALL] emergency call online, ignore hook flash");
				goto fini;
			}

			/* dial mode */
			if(!multiparties) {
				_log(LOG_DEBUG, "[INCALL] start dial mode");

				dtmf_reset(dev);

				rdb_manage_calls_local_hold(dev);
				incall_set_stat(dev, incall_stat_dial);
			}
			/* conference modoe */
			else if(held) {

				if(cc_is_conference_existing(dev)) {
					_log(LOG_DEBUG, "[INCALL] already conference call on-going, ignore flash hook");
				} else if(cc_is_any_incomging_call(dev)) {
					call = cc_get_first_call_on_hold(dev);
					if(!call) {
						_log(LOG_ERR, "[INCALL] held call not found");
					} else {
						_log(LOG_DEBUG, "[INCALL] accept waiting or held call");
						rdb_manage_calls_accept_waiting_or_held(dev);
						//rdb_manage_calls_activate(dev,call->cid);
					}
				} else {
					_log(LOG_DEBUG, "[INCALL] start conference");
					stat = cc_make_mo_call(dev, "n-way_voice", incall_call_timeout, 1);
					if(stat < 0) {
						_log(LOG_ERR, "[INCALL] failed to make mo call for conference");
					}
				}
			} else {
				_log(LOG_DEBUG, "[INCALL] active last connected call from stat 'none'");
				call = cc_get_last_answered_or_connected_call(dev);
				if(!call) {
					_log(LOG_ERR, "[INCALL] connected call not found");
				} else {
					rdb_manage_calls_activate(dev, call->cid);
					incall_set_stat(dev, incall_stat_none);
				}
			}
			break;

		case incall_stat_busy:
			_log(LOG_DEBUG, "[INCALL] unhold from 'busy', back to normal state");
			rdb_manage_calls_local_unhold(dev);
			incall_set_stat(dev, incall_stat_none);
			break;

		case incall_stat_disconnected:
			_log(LOG_DEBUG, "[INCALL] active last call connected from stat 'disconnected'");
			call = cc_get_last_answered_or_connected_call(dev);
			if(!call) {
				_log(LOG_ERR, "[INCALL] connected call not found");
			} else {
				_log(LOG_INFO, "[INCALL] resume call (cid=%d)", call->cid);
				rdb_manage_calls_accept_waiting_or_held(dev);
			}
			break;

		case incall_stat_waiting:
			_log(LOG_DEBUG, "[INCALL] answer first wating call");
			rdb_manage_calls_accept_waiting_or_held(dev);

#if 0
			call = cc_get_first_incoming_call_to_answer(dev);
			rdb_manage_calls_activate(dev, call->cid);
#endif
			/* update call statistics */
			cc_answer_mt_call(dev);
			break;

		case incall_stat_dial:
		case incall_stat_dialing:
			_log(LOG_DEBUG, "[INCALL] ignore dialing, back to normal state");
			rdb_manage_calls_local_unhold(dev);
			incall_set_stat(dev, incall_stat_none);
			break;

		case incall_stat_ringing:
		case incall_stat_last:
		default:
			incall_set_stat(dev, incall_stat_none);
			break;
	}

fini:
	return 0;
}

static char *handle_cli_qmirdb_hookflash(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct dev_t *dev = NULL;

	switch(cmd) {
		case CLI_INIT:
			e->command = "qmirdb hookflash";
			e->usage =
			    "Usage: qmirdb call hookflash <device ID>\n"
			    "       Send flash hook to the device\n";
			return NULL;

		case CLI_GENERATE:
			return NULL;
	}

	/* check parameter validation */
	if(a->argc != 3)
		return CLI_SHOWUSAGE;

	/* get dev */
	dev = _find_dev_by_id(a->argv[2]);
	if(!dev) {
		ast_cli(a->fd, "Device %s not found.\n", a->argv[2]);
		return CLI_FAILURE;
	}

	ast_mutex_lock(&dev->lock);
	{
		/* get first call */
		struct call_t* call = cc_get_first_call(dev);
		if(!call) {
			ast_cli(a->fd, "no call available");
			goto err;
		}

		if(!call->cid) {
			ast_cli(a->fd, "Device is being connected yet\n");
			goto err;
		}

		do_hookflash(dev);

		ast_mutex_unlock(&dev->lock);
	}

	return CLI_SUCCESS;

err:
	ast_mutex_unlock(&dev->lock);
	return CLI_FAILURE;
}

/*
	play tones
*/

#ifdef CONFIG_USE_INCALL_TONE
static int _ast_playtones(struct ast_channel *chan, const char *data)
{
	const char *str = data;

#ifdef CONFIG_USE_INBOUND_TONE
	struct ast_tone_zone_sound *ts;
	int res;

	if(ast_strlen_zero(str)) {
		_log(LOG_NOTICE, "Nothing to play\n");
		return -1;
	}

	/* get tone */
	ts = ast_get_indication_tone(ast_channel_zone(chan), str);
	if(ts) {
		_log(LOG_DEBUG, "play tones ts (str=%s)", str);
		res = ast_playtones_start(chan, 0, ts->data, 0);
		ts = ast_tone_zone_sound_unref(ts);
	} else {
		_log(LOG_DEBUG, "play tones (str=%s)", str);

		res = ast_playtones_start(chan, 0, str, 0);
	}

	if(res) {
		_log(LOG_NOTICE, "Unable to start playtones\n");
	}

	return res;

#else
	_log(LOG_DEBUG, "[TONE] expect tone starts from network (data=%s)", str);
	return 0;
#endif
}

static void _ast_stopplaytones(struct ast_channel *chan)
{
#ifdef CONFIG_USE_INBOUND_TONE
	if(ast_channel_generator(chan))
		ast_playtones_stop(chan);
#else
	_log(LOG_DEBUG, "[TONE] expect tone stopped from network");
#endif
}
#endif


/*
	RDB interface functions
*/

static const char* _rdb_get_str(const char* rdb)
{
	static char str[RDB_MAX_VAL_LEN];
	int str_len = sizeof(str);
	int stat;

	if((stat = rdb_get(_s, rdb, str, &str_len)) < 0) {
		_log(LOG_ERR, "[RDB] failed to get RDB (rdb='%s',stat=%d) - %s#%d\n", rdb, stat, strerror(errno));
		*str = 0;
	}

	return str;
}

static unsigned long long _rdb_get_int(const char* rdb)
{
	return atoll(_rdb_get_str(rdb));
}

static int _rdb_set_str(const char* rdb, const char* val)
{
	int stat;

	/* set rdb */
	if((stat = rdb_set_string(_s, rdb , val)) < 0) {
		if(stat == -ENOENT) {
			stat = rdb_create_string(_s, rdb, val, 0, 0);
		}
	}

	/* log error message if it failed */
	if(stat < 0)
		_log(LOG_ERR, "[RDB] failed to set RDB (rdb='%s',val='%s',stat=%d) - %s#%d\n", rdb, val, stat, strerror(errno));

	return stat;
}

#ifdef CONFIG_USE_STATISTICS
static int _rdb_set_int(const char* rdb, unsigned long long val)
{
	char str[RDB_MAX_VAL_LEN];

	snprintf(str, sizeof(str), "%llu", val);
	return _rdb_set_str(rdb, str);
}
#endif


int rdb_stop_dtmf(struct dev_t* dev, int cid)
{
	return rwpipe_post_write_printf(dev->pp, NULL, 0, 0, "%s %d", __STR(vc_stop_dtmf), cid);
}

int rdb_start_dtmf(struct dev_t* dev, int cid, char digit)
{
	return rwpipe_post_write_printf(dev->pp, NULL, 0, 0, "%s %d %c", __STR(vc_start_dtmf), cid, digit);
}

int rdb_answer(struct dev_t* dev, int cid)
{
	return rwpipe_post_write_printf(dev->pp, NULL, 0, 0, "%s %d", __STR(vc_answer), cid);
}

int rdb_manage_calls(struct dev_t* dev, const char* sups_type, int cid, const char* reject_cause)
{
	const char* vc_manage_calls_str;
	char msg[RDB_MAX_VAL_LEN];

	_log(LOG_DEBUG, "[INCALL] send manage calls (cmd='%s',cid=%d)", sups_type, cid);

#ifdef CONFIG_USE_IP_CALLS
	vc_manage_calls_str = __STR(vc_manage_ip_calls);
#else
	vc_manage_calls_str = __STR(vc_manage_calls);
#endif

	snprintf(msg, sizeof(msg), "%s %s %d", vc_manage_calls_str, sups_type, cid);

	if(reject_cause)
		return rwpipe_post_write_printf(dev->pp, NULL, 0, 0, "%s %s", msg, reject_cause);
	else
		return rwpipe_post_write_printf(dev->pp, NULL, 0, 0, "%s", msg);
}

int rdb_manage_calls_local_hold(struct dev_t* dev)
{
	return rdb_manage_calls(dev, "HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD", 0, NULL);
}

int rdb_manage_calls_local_unhold(struct dev_t* dev)
{
	/* acitivy of network unhold is using the identical command */
	return rdb_manage_calls(dev, "HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD", 0, NULL);

#if 0
	/* do not use release - this release activity is disconnecting the call */
	return rdb_manage_calls(dev, "RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING", 0, NULL);
#endif
}

int rdb_manage_calls_conference(struct dev_t* dev)
{
	return rdb_manage_calls(dev, "MAKE_CONFERENCE_CALL", 0, NULL);
}

int rdb_manage_calls_accept_waiting_or_held(struct dev_t* dev)
{
	return rdb_manage_calls(dev, "HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD", 0, NULL);
}

int rdb_manage_calls_activate(struct dev_t* dev, int cid)
{
	return rdb_manage_calls(dev, "HOLD_ALL_EXCEPT_SPECIFIED_CALL", cid, NULL);
}

int rdb_manage_calls_resume(struct dev_t* dev, int cid)
{
	return rdb_manage_calls(dev, "CALL_RESUME", cid, NULL);
}

int rdb_manage_calls_end_all_calls(struct dev_t* dev)
{
	return rdb_manage_calls(dev, "END_ALL_CALLS", 0, NULL);
}

int rdb_manage_calls_release(struct dev_t* dev, int cid, const char* reject_cause)
{
	return rdb_manage_calls(dev, "RELEASE_SPECIFIED_CALL", cid, reject_cause);
}


int rdb_reject(struct dev_t* dev, int cid)
{
	return rwpipe_post_write_printf(dev->pp, NULL, 0, 0, "%s %d %s", __STR(vc_reject), cid, "USER_REJECT");
}

int rdb_hangup(struct dev_t* dev, int cid)
{
	return rwpipe_post_write_printf(dev->pp, NULL, 0, 0, "%s %d", __STR(vc_hangup), cid);
}

int rdb_call(struct dev_t* dev, const char* dest_num, int timeout)
{
	_log(LOG_INFO, "[RDB] make a call (%s)", dest_num);
	return rwpipe_post_write_printf(dev->pp, NULL, timeout, 0, "%s %s %d", __STR(vc_call), dest_num, timeout);
}

int rdb_ecall(struct dev_t* dev, const char* dest_num, int timeout)
{
	_log(LOG_INFO, "[RDB] make a call (%s)", dest_num);
	return rwpipe_post_write_printf(dev->pp, NULL, timeout, 0, "%s %s %d", __STR(vc_ecall), dest_num, timeout);
}

int rdb_set_voice_call_status(int cid, const char* status, const char* param)
{
	char val[RDB_MAX_VAL_LEN];
	char rdb[RDB_MAX_VAL_LEN];
	int stat;

	/* get voice call rdb */
	snprintf(rdb, sizeof(rdb), "voice_call.%d.status", cid);

	if(!param || ast_strlen_zero(param))
		snprintf(val, sizeof(val), "%s", status);
	else
		snprintf(val, sizeof(val), "%s %s", status, param);

	_log(LOG_INFO, "[RDB] voice call status changed (cid=%d,status='%s')", cid, val);
	if((stat = rdb_set_string(_s, rdb, val)) < 0) {
		if(stat == -ENOENT) {
			stat = rdb_create_string(_s, rdb, val, 0, 0);
		}
	}

	if(stat < 0) {
		_log(LOG_ERR, "rdb_set_string() failed in on_rdb_ctrl() - %s", strerror(errno));
		goto err;
	}


	return 0;

err:
	return -1;
}


/* call management functions */

/*
	* call type definition

	active call		: answered incoming calls and connected outgoing calls, those calls that are not on hold


	running call		: answered incoming calls and any outgoing calls that is not even connected

	connected call		: connected outgoing calls regardless of held flag
	call to be answered	: incoming calls that are not answered
	ringing call		: outgoing calls that are not connected, cid is available for those outgoing calls

	* caution

	cid	: incoming calls are born with cid but outgoing calls are born without cid and cid becomes available after call progress
	held	: held flag becomes avaialble after connecting or answering, any call on hold must be connected or answered

*/

static struct call_t* cc_get_call_by_cid(struct dev_t* dev, int cid)
{
	struct call_t* call = NULL;

	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		if(call->cid == cid) {
			break;
		}
	}

	return call;
}

static int cc_get_call_count(struct dev_t* dev)
{
	int count = 0;
	struct call_t* call;

	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		count++;
	}

	return count;
}

static int cc_is_any_call_active(struct dev_t* dev)
{
	int active_flag;

	struct call_t* call;

	active_flag = 0;
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		active_flag = active_flag || (!call->held && (call->answered || call->connected));
	}

	return active_flag;
}

static struct call_t* cc_get_first_call_on_hold(struct dev_t* dev)
{
	struct call_t* call = NULL;

	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		if(call->held)
			break;
	}

	return call;
}

static struct call_t* cc_get_last_answered_or_connected_call(struct dev_t* dev)
{
	struct call_t* call = NULL;
	struct call_t* last_connected_call = NULL;

	AST_LIST_TRAVERSE(&dev->calls, call, entry) {

		if(call->connected || call->answered)
			last_connected_call = call;
	}

	return last_connected_call;
}

static int cc_is_any_call_to_be_answered(struct dev_t* dev)
{
	int to_be_answer;

	struct call_t* call;

	to_be_answer = 0;
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		to_be_answer = to_be_answer || (call->incoming && !call->answered);
	}

	return to_be_answer;
}

static int cc_is_conference_existing(struct dev_t* dev)
{
	int conference;

	struct call_t* call;

	conference = 0;
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		conference = conference || call->conference;
	}

	return conference;

}

static struct call_t* cc_get_first_call(struct dev_t* dev)
{
	return AST_LIST_FIRST(&dev->calls);
}
#ifndef CONFIG_IGNORE_DTMF
static struct call_t* cc_get_active_dtmf_call(struct dev_t* dev)
{
	struct call_t* call = NULL;

	/* find currently ringing outgoing call */
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		if(!call->held && (!call->connected && call->outgoing && call->ringing))
			break;
	}

	/* bypass if found */
	if(call)
		goto fini;

	/* search any answered call */
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		if(!call->held && (call->answered || call->connected))
			break;
	}

fini:
	return call;
}
#endif


static int cc_is_any_call_ringing(struct dev_t* dev)
{
	int ringing;

	struct call_t* call;

	ringing = 0;
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		ringing = ringing || (call->outgoing && call->ringing && !call->connected);
	}

	return ringing;
}

static int cc_is_any_call_on_hold(struct dev_t* dev)
{
	int held;

	struct call_t* call;

	held = 0;
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		held = held || (call->held && (call->connected || call->answered));
	}

	return held;
}

static int cc_is_any_emergency_call(struct dev_t* dev)
{
	int emergency;

	struct call_t* call;

	emergency = 0;
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		emergency = emergency || call->emergency;
	}

	return emergency;
}

static int cc_is_any_incomging_call(struct dev_t* dev)
{
	int incoming;

	struct call_t* call = NULL;

	incoming = 0;
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		incoming = incoming || call->incoming;
	}

	return incoming;
}


static int cc_is_empty(struct dev_t* dev)
{
	return AST_LIST_EMPTY(&dev->calls);
}

static int cc_is_any_call_running(struct dev_t* dev)
{
	int active;

	struct call_t* call = NULL;

	active = 0;
	AST_LIST_TRAVERSE(&dev->calls, call, entry) {
		active = active || (call->cid && (call->incoming || call->outgoing));
	}

	return active;
}


static void cc_remove_call(struct dev_t* dev, int cid)
{
	struct call_t* call = cc_get_call_by_cid(dev, cid);

	if(!call) {
		_log(LOG_ERR, "cid not found (cid=%d)", cid);
		goto err;
	}

	/* remove and destroy */
	AST_LIST_REMOVE(&dev->calls, call, entry);

	if(call->answered || call->connected) {
#ifdef CONFIG_USE_STATISTICS
		rupdate_StopTime(call);
		rupdate_run_script(call, 0);
		rupdate_TotalCallTime(call);
#endif
	}

	ast_free(call->clip);
	ast_free(call->pi);
	ast_free(call);

err:
	return;
}

static void cc_add_info_to_call(struct dev_t* dev, struct call_t* call, const char* pi, const char* clip, const char* clip_name)
{
	if(pi) {
		_log(LOG_DEBUG, "[CNAME] add pi info into call (cid=%d,clip=%s)", call->cid, pi);

		call->pi = pi ? ast_strdup(pi) : NULL;
	}

	if(clip) {
		_log(LOG_DEBUG, "[CNAME] add clip info into call (cid=%d,clip=%s)", call->cid, clip);

		call->clip = clip ? ast_strdup(clip) : NULL;
	}

	if(clip_name) {
		_log(LOG_DEBUG, "[CNAME] add clip name info into call (cid=%d,clip_name=%s)", call->cid, clip_name);

		call->clip_name_valid = 1;
		snprintf(call->clip_name, AST_MAX_CONTEXT, "%s", clip_name);
	}
}

static struct call_t* cc_add_new_call(struct dev_t* dev, int cid, int incoming, int outgoing, int conference, const char* pi, const char* clip, const char* clip_name)
{
	struct call_t* call = NULL;

	/* allocae call */
	call = (struct call_t*)ast_calloc(1, sizeof(struct call_t));
	if(!call) {
		_log(LOG_ERR, "unable to create mo call");
		goto err;
	}

	call->cid = cid;
	call->incoming = incoming;
	call->outgoing = outgoing;
	call->clip = clip ? ast_strdup(clip) : NULL;
	call->conference = conference;
	call->emergency = clip && (*clip == 'E');
	call->clip_name_valid = clip_name != NULL;
	if(call->clip_name_valid)
		snprintf(call->clip_name, AST_MAX_CONTEXT, "%s", clip_name);
	call->pi = pi ? ast_strdup(pi) : NULL;

	AST_LIST_INSERT_TAIL(&dev->calls, call, entry);
	return call;
err:
	return NULL;
}

/*
 Call statistics for answering of a MT (Mobile Terminated) call.

 Parameters:
  dev : device to answer a MT call.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int cc_answer_mt_call(struct dev_t* dev)
{
#ifdef CONFIG_USE_STATISTICS
	statistics.IncomingCallsAnswered++;
	rupdate_statistics();
#endif

	return 0;
}

static int cc_connect_mt_call(struct dev_t* dev, struct call_t* call)
{
	call->answered = 1;

#ifdef CONFIG_USE_STATISTICS
	statistics.IncomingCallsConnected++;
	rupdate_statistics();

	rupdate_StartTime(call);
	rupdate_run_script(call, 1);
#endif

	return 0;
}

struct call_t* cc_make_mt_call(struct dev_t* dev, int cid, const char* pi, const char* clip, const char* clip_name)
{
	struct call_t* call = cc_add_new_call(dev, cid, 1, 0, 0, pi, clip, clip_name);

#ifdef CONFIG_USE_STATISTICS
	statistics.IncomingCallsReceived++;
	rupdate_statistics();
#endif

	return call;
}

static int cc_connect_mo_call(struct dev_t* dev, struct call_t* call)
{
	call->connected = 1;

#ifdef CONFIG_USE_STATISTICS
	statistics.OutgoingCallsAnswered++;
	statistics.OutgoingCallsConnected++;
	rupdate_statistics();

	rupdate_StartTime(call);
	rupdate_run_script(call, 1);
#endif

	return 0;
}

static int cc_make_mo_progressed(struct dev_t* dev, struct call_t* call)
{
	call->progressed = 1;

	return 0;
}

static int cc_make_mo_ringing(struct dev_t* dev, struct call_t* call)
{
	call->ringing = 1;

	return 0;
}

static int cc_make_mo_call(struct dev_t* dev, const char* num, int timeout, int conference)
{
	struct call_t* call;
	int stat;

	/* post qmi call command */
	if(conference) {
		_log(LOG_DEBUG, "send rdb conference call");

		rdb_manage_calls_conference(dev);
	} else {
		if(*num == 'E') {
			_log(LOG_DEBUG, "send rdb emergency call");
			stat = rdb_ecall(dev, num + 1, timeout);
		} else {
			if(!strcasecmp(num, VOICEMAIL_DIALNO)) {
				const char* voicemail = _rdb_get_str(VOICEMAIL_RDB);
				if(!ast_strlen_zero(voicemail)) {
					_log(LOG_DEBUG, "send voice mail call (voicemail=%s)", voicemail);
					stat = rdb_call(dev, voicemail, timeout);
				} else {
					_log(LOG_ERR, "no voice mail dial number available (rdb=%s)", VOICEMAIL_RDB);
					stat = -1;
				}

			} else {
				_log(LOG_DEBUG, "send rdb call");
				stat = rdb_call(dev, num, timeout);
			}
		}

		if(stat < 0) {
			_log(LOG_ERR, "unable to send qmi command");
			goto err;
		}
	}

	/* initate call status members */
	_log(LOG_DEBUG, "add call");
	call = cc_add_new_call(dev, 0, 0, 1, conference, NULL, num, NULL);
	if(!call) {
		_log(LOG_ERR, "failed to add mo call");
		goto err;
	}

#ifdef CONFIG_USE_STATISTICS
	statistics.OutgoingCallsAttempted++;
	rupdate_statistics();
#endif

	return 0;
err:
	return -1;
}

static int incall_start_mt_call(struct dev_t* dev, struct call_t* call)
{
	struct ast_channel* chan;
	const char* clip_name;

	/* create channel if not exisitng */
	if(!dev->owner) {
		clip_name = call->clip_name_valid ? call->clip_name : NULL;
		/* create channel */
		if(!(chan = _ast_create_chn(AST_STATE_RING, dev, call->clip, clip_name, NULL, NULL))) {
			_log(LOG_ERR, "[%s] unable to allocate channel for incoming call\n", dev->id);
			rdb_hangup(dev, call->cid);
			goto err;
		}

		/* from this point on, we need to send a chup in the event of a
		* hangup */

		_log(LOG_INFO, "[AST] start pbx");
		if(ast_pbx_start(chan)) {
			_log(LOG_ERR, "[%s] unable to start pbx on incoming call\n", dev->id);
			_ast_hangup_with_dev(dev);
			goto err;
		}

		/* set invite */
		call->invite = 1;
	}

	/* update call status */
	rdb_set_voice_call_status(call->cid, "ring", call->clip);

	return 0;
err:
	return -1;
}

void soundcard_stop_and_fini(struct dev_t* dev)
{
	if(!soundcard_is_open(dev))
		return;

	disconnect_pcm(dev);
	soundcard_stop_recording(dev);

	_log(LOG_INFO, "[SND] fini host PCM interface\n");
	soundcard_fini(dev);
}

/*
 Restart sound card play-back and record devices and wake up PBX by posting NULL frame.

 Parameters:
  dev : device to restart sound card.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
int soundcard_restart(struct dev_t* dev)
{
	if(!soundcard_is_open(dev))
		goto err;

	struct ast_channel *owner;

	_log(LOG_DEBUG, "[snd-init] start recording initiate procedure");
	for(;;) {
		owner = dev->owner;
		if(owner) {
			if(ast_channel_trylock(owner)) {
				_deadlock_avoidance(&dev->lock);
			} else {
				_log(LOG_INFO, "[snd-init] stop sound card recording");
				soundcard_stop_recording(dev);

				_log(LOG_DEBUG, "[snd-init] start sound card recording");
				soundcard_start_recording(dev);

				_log(LOG_DEBUG, "[snd-init] send null frame to PBX");
				ast_queue_frame(owner, &ast_null_frame);
				_log(LOG_DEBUG, "[snd-init] unlock critical section");
				ast_channel_unlock(owner);

				break;
			}
		} else {
			_log(LOG_DEBUG, "[snd-init] no owner is associated, skip sound card starting procedure");
			goto err;
		}
	}

	return 0;
err:
	return -1;
}

int soundcard_init_and_start(struct dev_t* dev, int reopen)
{
	int open = soundcard_is_open(dev);

	if(reopen && open) {
		soundcard_restart(dev);
		goto fini;
	}

	if(open) {
		_log(LOG_INFO, "[SND] soundcard already running");
		goto fini;
	}

	/* initiate sound card */
	_log(LOG_INFO, "[SND] init host PCM interface");
	if(soundcard_init(dev) < 0) {
		_log(LOG_ERR, "no sound card detected (dev=%s)", dev->id);
		goto err;
	}

	connect_pcm(dev);

	soundcard_start_recording(dev);

fini:
	return 0;

err:
	return -1;
}

/*
	RDB event functions
*/

static int update_pending_mt_call(struct dev_t* dev, struct call_t* call)
{
	unsigned long now;

	/* get flag */
	now = get_monotonic_time();

	/* trigger when the call gets expired */
	if(call->incoming && !call->answered && !call->invite) {
		_log(LOG_DEBUG, "[CNAME] update invite timestamp (cid=%d,timestamp=%lu)", call->cid, now);
		call->invite_timestamp_valid = 1;
		call->invite_timestamp = now;
	}

	return 0;
}


static int incall_start_waiting_mt_call(struct dev_t* dev, struct call_t* call)
{
	int cid = call->cid;

	_log(LOG_INFO, "[INCALL] send INFO CW_START (cid=%d)", cid);
	incall_send_info_to_peer(dev, DTMF_DIGIT_CW_START, call->pi, call->clip, call->clip_name_valid ? call->clip_name : NULL);

	_log(LOG_INFO, "[INCALL] switch to callwaiting (cid=%d)", cid);
	incall_set_stat(dev, incall_stat_waiting);

	/* set invite flag */
	call->invite = 1;

	return 0;
}


static int perform_pending_mt_call(struct dev_t* dev, int cid)
{
	int force;
	int expired;

	unsigned long now;
	struct call_t* call;

	AST_LIST_TRAVERSE(&dev->calls, call, entry) {

		/* get flag */
		now = get_monotonic_time();

		/* get expired flag */
		expired = !(now - call->invite_timestamp < INVITE_DELAY_TIMER) && call->invite_timestamp_valid;
		force = cid == call->cid;

		/* trigger when the call gets expired */
		if(call->incoming && !call->answered && !call->invite && (expired || force)) {
			if(force)
				_log(LOG_DEBUG, "[CNAME] force to start mt call (cid=%d,callwiating=%d)", call->cid, call->callwaiting);
			else
				_log(LOG_DEBUG, "[CNAME] pending timer expired, start mt call (cid=%d,callwaiting=%d,now=%lu)", call->cid, call->callwaiting, now);

			if(call->callwaiting) {
				_log(LOG_DEBUG, "[CNAME] perform pending waiting mt call (cid=%d,callwiating=%d)", call->cid, call->callwaiting);
				incall_start_waiting_mt_call(dev, call);
			} else {
				_log(LOG_DEBUG, "[CNAME] perform pending mt call (cid=%d,callwiating=%d)", call->cid, call->callwaiting);
				incall_start_mt_call(dev, call);
			}
		}
	}

	return 0;
}

static int on_msg_noti(struct dev_t* dev, int idx, struct strarg_t* a)
{
	const char* cmd = a->argv[0];

	struct call_t* call;
	int stat;

	_log(LOG_DEBUG, "cmd (%d/%s) recv", idx, cmd);

	switch(idx) {
		case vc_dtmf_notify: {
			int cid;
			const char* action;
			const char* digits;

			int dtmf_begin;

			struct ast_frame f;

			cid = atoi(a->argv[1]);
			action = a->argv[2];
			digits = a->argv[3];

			/* check action */
			if(!action || !*action) {
				_log(LOG_ERR, "[NOTI] action field missing in DTMF notification)");
				goto dtmf_fini;
			}

			/* check digit */
			if(!digits || !*digits) {
				_log(LOG_ERR, "[NOTI] digit field missing in DTMF notification)");
				goto dtmf_fini;
			}

			/* get frame type */
			dtmf_begin = !strcmp(action, "IP_INCOMING_DTMF_START");

			_log(LOG_DEBUG, "[NOTI] dtmf notification recv (call_id='%d',action='%s',digits='%s'", cid, action, digits);

			/* set asterisk frame */
			memset(&f, 0, sizeof(f));
			f.frametype = dtmf_begin ? AST_FRAME_DTMF_BEGIN : AST_FRAME_DTMF_END;
			f.subclass.integer = digits[0];

			_ast_queue_frame(dev, &f);
dtmf_fini:
			__noop();
			break;
		}

		case vc_call_notify: {
			int cid;

			const char* call_type;
			const char* call_dir;
			const char* call_desc;
			const char* alert_type;

			char* pi;
			const char* clip;
#if 0
			const char* clip_name_pi;
#endif
			const char* clip_name_str;

			int callwaiting = 0;

			int emergency;

			_log(LOG_DEBUG, "[NOTI] got noti");

			if(a->argc < 5) {
				_log(LOG_ERR, "[NOTI] incorrect argc (argc=%d)", a->argc);
				break;
			}

			/* get arguments */
			cid = atoi(a->argv[1]);
			call_dir = a->argv[2];
			call_type = a->argv[3];
			call_desc = a->argv[4];
			alert_type = a->argv[5];

			_log(LOG_DEBUG, "[NOTI] call_dir='%s',cid=%d,call_type='%s',call_desc='%s'", call_dir, cid, call_type, call_desc);

			/* check call type */
			emergency = !strcmp(call_type, "EMERGENCY") || !strcmp(call_type, "EMERGENCY_IP");
			if(strcmp(call_type, "VOICE") && strcmp(call_type, "VOICE_IP") && !emergency) {
				_log(LOG_ERR, "[NOTI] ignore - not voice call (call_type=%s)", call_type);
				break;
			}

			pi = a->argv[6] ? a->argv[6] : "";
			clip = a->argv[7] ? a->argv[7] : "";
#if 0
			clip_name_pi = a->argv[8] ? a->argv[8] : NULL;
#endif
			clip_name_str = a->argv[9] ? a->argv[9] : NULL;

			/* if incoming call */
			if(!strcmp(call_dir, "MT")) {
				if(!strcmp(call_desc, "INCOMING") || (callwaiting = !strcmp(call_desc, "WAITING"))) {
					_log(LOG_INFO, "[NOTI] got %d %s '%s' '%s' '%s'", cid, call_desc, pi, clip, clip_name_str);

					/* reject if no RG is registered */
					if(!ua_registered) {
						_log(LOG_INFO, "[NOTI] SETUP - RG not found, reject call (cid=%d)", cid);
						rdb_reject(dev, cid);
						goto fini_mt;
					}

					/* get call by cid */
					call = cc_get_call_by_cid(dev, cid);

					/* create incoming call */
					if(!call) {
						/* create a new call */
						call = cc_make_mt_call(dev, cid, pi, clip, clip_name_str);
						if(!call) {
							_log(LOG_ERR, "failed to allocate MT call");
							goto fini_mt;
						}
					}

					/* bypass if the call is not incoming call */
					if(!call->incoming) {
						_log(LOG_ERR, "not incoming call, bypass");
						goto fini_mt;
					}

					/* add clip or clip name to call */
					cc_add_info_to_call(dev, call, pi, clip, clip_name_str);

					/* set callwaiting flag */
					if(callwaiting) {
						call->callwaiting = 1;
					}

					/* pend invite if not a callwaiting call */
					_log(LOG_INFO, "[CNAME] pend invite (cid=%d)", cid);

					/* update mt call */
					update_pending_mt_call(dev, call);


				} else {
					/* get call by cid */
					call = cc_get_call_by_cid(dev, cid);
					if(!call) {
						_log(LOG_ERR, "[INCALL] cid not found in MT (cid=%d)", cid);
						goto fini_mt;
					}

					if(!dev->owner) {
						_log(LOG_DEBUG, "no channel assigned in MT, ignore");
						goto fini_mt;
					}

					if(!strcmp(call_desc, "CONVERSATION")) {
						_log(LOG_INFO, "[NOTI] got %d %s", cid, call_desc);

						_log(LOG_INFO, "[CNAME] perform any pending MT call by CONVERSATION (cid=%d)", cid);
						perform_pending_mt_call(dev, cid);

						/* get stat */
						stat = ast_channel_state(dev->owner);

						if(!call->incoming) {
							_log(LOG_ERR, "no incoming call existing (stat=%d), ignore", stat);
							goto fini_mt;
						}

						/* clear held flag */
						_log(LOG_DEBUG, "[NOTI] clear held flag (cid=%d) in MT", cid);
						call->held = 0;

						/* bypass if not after ring */
						if(stat != AST_STATE_UP) {
							_log(LOG_ERR, "incorrect sequence of conversation event (stat=%d), ignore", stat);
							goto fini_mt;
						}

#if 0
						/* reset peer ringback */
						_log(LOG_INFO, "[RINGBACK] reset ringback in MT conversation");
						incall_set_peer_ringback(dev, peer_ringback_none);
#endif

						/* update answered flag */
						cc_connect_mt_call(dev, call);

						if(soundcard_init_and_start(dev, 1) < 0) {
							_log(LOG_ERR, "no sound card detected in MT (dev=%s)\n", dev->id);
							goto fini_mt;
						}

						/* update call status */
						rdb_set_voice_call_status(cid, "conversation", clip);
					}
				}

fini_mt:
				__noop();


				/* if outgoing call */
			} else {
				int origination;

				if(!dev->owner) {
					_log(LOG_DEBUG, "no device owner assigned in MO call, ignore");
					goto fini_mo;
				}

				/* get stat */
				stat = ast_channel_state(dev->owner);

				origination = !strcmp(call_desc, "ORIGINATION");

				if(!strcmp(call_desc, "CC_IN_PROGRESS") || origination) {
					_log(LOG_INFO, "[NOTI] got %d %s", cid, call_desc);

					/* n-way_voice does not have CC_IN_PROGRESS */

					/* get call */
					call = cc_get_call_by_cid(dev, cid);
					if(!call) {
						/* get call */
						call = cc_get_call_by_cid(dev, 0);
						if(!call) {
							_log(LOG_ERR, "[INCALL] no waiting MO call found in progress (cid=%d)", cid);
							goto fini_mo;
						}

						/* update call */
						call->cid = cid;
						_log(LOG_DEBUG, "[INCALL] obtain cid (cid=%d)", cid);
					}

					if(origination) {
						/* bypass if already progress */
						if(call->progressed) {
							_log(LOG_ERR, "[INCALL] already progressed (cid=%d)", cid);
							goto fini_mo;
						}

						cc_make_mo_progressed(dev, call);

						/* do progress */
						if(stat == AST_STATE_DOWN) {
							_log(LOG_INFO, "[AST] post AST_CONTROL_PROGRESS");
							_ast_queue_control(dev, AST_CONTROL_PROGRESS);
						}
					}
				} else {
					/* get call */
					call = cc_get_call_by_cid(dev, cid);

					if(!call) {
						_log(LOG_ERR, "[INCALL] no waiting MO call found (cid=%d)", cid);
						goto fini_mo;
					}

					if(!call->outgoing) {
						_log(LOG_INFO, "not outgoing call, bypass (cid=%d)", cid);
						goto fini_mo;
					}

					if(0) {
					} else if(!strcmp(call_desc, "ALERTING")) {

						_log(LOG_INFO, "[NOTI] got %d %s", cid, call_desc);

						if(!call->ringing) {
							_log(LOG_INFO, "got a new outgoing call (cid=%d)", cid);

#ifdef CONFIG_USE_INBAND_RINGBACK
							/* do not send 180 for in-band ringback */
#else
							if(stat == AST_STATE_DOWN) {
								_log(LOG_INFO, "[AST] post AST_CONTROL_RINGING");
								_ast_queue_control(dev, AST_CONTROL_RINGING);

#ifdef CONFIG_USE_EXTRA_CB
								_log(LOG_INFO, "[AST] send indiciate AST_CONTROL_RINGING");
								ast_indicate(dev->owner, AST_CONTROL_RINGING);
#endif
							}
#endif

							/* update call */
							cc_make_mo_ringing(dev, call);

							call->emergency = emergency;

							/* update rdb call status */
							rdb_set_voice_call_status(cid, "dial", clip);

							/* reset peer ringback */
							_log(LOG_INFO, "[RINGBACK] reset ringback in MO");
							incall_set_peer_ringback(dev, peer_ringback_none);
						}

						if(incall_is_stat(dev, incall_stat_ring)) {
							_log(LOG_INFO, "[INCALL] switch to ringing (cid=%d)", cid);
							incall_set_stat(dev, incall_stat_ringing);
						}

						_log(LOG_INFO, "[NOTI] network ringback alert detected (cur_ring_back=%d,alert_type=%s)", dev->peer_ringback, alert_type);

						/* switch to local ringback only when necessary */
						if((dev->peer_ringback == peer_ringback_none && !strcmp(alert_type, "NONE")) || !strcmp(alert_type, "REMOTE")) {
							_log(LOG_INFO, "[NOTI] switch to local ringback (alert_type=%s)", alert_type);

							/* stop PCM devices as no early media is available */
							soundcard_stop_and_fini(dev);

							/* start local ringback */
							incall_set_peer_ringback(dev, peer_ringback_local);

						/* switch to remote ringback only when necessary */
						} else if(!strcmp(alert_type, "LOCAL")) {
							_log(LOG_INFO, "[NOTI] switch to remote ringback (alert_type=%s)", alert_type);

							/* stop PCM devices */
							soundcard_stop_and_fini(dev);

							incall_set_peer_ringback(dev, peer_ringback_remote);

							if(soundcard_init_and_start(dev, 0) < 0) {
								_log(LOG_ERR, "no sound card detected in MO ALERT (dev=%s)", dev->id);
								goto fini_mo;
							}
						}


					} else if(!strcmp(call_desc, "CONVERSATION")) {
						_log(LOG_INFO, "[NOTI] got %d %s", cid, call_desc);

						/*
							1. AT&T network starts CONVERSATION of n-way call without ALERT.
							2. AT&T FWLL network in Colby USA starts CONVERSATION without ALERT - allow CONVERSATION after being progressed (ORIGINATION).
						*/
						if(!call->progressed && !call->conference) {
							_log(LOG_INFO, "no 'progressed' indication, bypass (cid=%d)", cid);
							goto fini_mo;
						}

						/* clear held flag */
						_log(LOG_DEBUG, "[NOTI] clear held flag (cid=%d) in MO", cid);
						call->held = 0;

						if(call->connected) {
							_log(LOG_INFO, "call already connected, skip (cid=%d)", cid);
						} else {

							/* reset peer ringback */
							_log(LOG_INFO, "[RINGBACK] reset ringback in MO conversation");
							incall_set_peer_ringback(dev, peer_ringback_none);

							cc_connect_mo_call(dev, call);
						}

						/* stop PCM devices */
						soundcard_stop_and_fini(dev);

						if(soundcard_init_and_start(dev, 0) < 0) {
							_log(LOG_ERR, "no sound card detected in MO CONVERSATION (dev=%s)", dev->id);
							goto fini_mo;
						}

						if(stat == AST_STATE_DOWN) {
							_log(LOG_INFO, "[AST] post AST_CONTROL_ANSWER");
							_ast_queue_control(dev, AST_CONTROL_ANSWER);
						}

						/* update call status */
						rdb_set_voice_call_status(cid, "conversation", clip);

					}

				}

fini_mo:
				__noop();
			}

			if(!strcmp(call_desc, "HOLD")) {

				call = cc_get_call_by_cid(dev, cid);
				if(!call) {
					_log(LOG_DEBUG, "[NOTI] cid not found (cid=%d), ignore notification", cid);
				} else {
					/* update call status */
					_log(LOG_DEBUG, "[NOTI] set held flag (cid=%d)", cid);
					call->held = 1;

					rdb_set_voice_call_status(cid, "hold", clip);
				}
			}

			/* if call end */
			else if(!strcmp(call_desc, "DISCONNECTING")) {

			} else if(!strcmp(call_desc, "END")) {

				_log(LOG_INFO, "[NOTI] got %d %s %s", cid, call_dir, call_desc);

				_log(LOG_INFO, "[CNAME] perform any pending MT call by END (cid=%d)", cid);
				perform_pending_mt_call(dev, cid);

				/* get call */
				call = cc_get_call_by_cid(dev, cid);
				if(!call) {
					_log(LOG_ERR, "[INCALL] no waiting MO/MT call found in end (cid=%d)", cid);
					goto fini_end;
				}

				_log(LOG_INFO, "[INCALL] remove MO/MT call from calls (cid=%d)", cid);
				cc_remove_call(dev, cid);


				if(dev->owner) {

					if(!cc_is_any_call_running(dev)) {
						_log(LOG_DEBUG, "[INCALL] no active incoming or outgoing call left, hangup channel");
						_ast_queue_hangup(dev);
					}
				} else {
					_log(LOG_DEBUG, "no device owner assigned in MO/MT call end, ignore");
				}

				/* update call status */
				rdb_set_voice_call_status(cid, "end", clip);

				/* disconnect mode */
				if(!cc_is_empty(dev) && !cc_is_any_call_active(dev)) {
					/* stop PCM devices */
					_log(LOG_DEBUG, "[INCALL] no active call, stop pcm (cid=%d)", cid);
					soundcard_stop_and_fini(dev);

					_log(LOG_DEBUG, "[INCALL] no active call, disconnected (cid=%d)", cid);
					incall_set_stat(dev, incall_stat_disconnected);

#if 0
					if(cc_is_any_call_on_hold(dev)) {
						/*
							## TODO ##

							it is currently unknown how to re-activate background call on hold.
							this background call on hold is causing issues since no stream is available.

							at this stage, we hangup all held calls when we answer a new call.
						*/

						_log(LOG_INFO, "[INCALL] release calls on hold (cid=%d)", cid);
						rdb_hangup_calls_on_hold(dev);
					}
#endif
				}

				/* clear any existing mode */
				if(cc_is_empty(dev)) {
					_log(LOG_DEBUG, "[INCALL] no call left, back to none", cid);
					incall_set_stat(dev, incall_stat_none);
				}
			}

			/* swtich state machine */
			switch(incall_get_stat(dev)) {

				case incall_stat_disconnected:
					if(cc_is_any_call_active(dev)) {
						_log(LOG_INFO, "[INCALL] turn off disconnected tone (cid=%d)", cid);
						incall_set_stat(dev, incall_stat_none);
					}
					break;

				case incall_stat_ringing:
					if(!cc_is_any_call_ringing(dev)) {
						_log(LOG_INFO, "[INCALL] turn off incall ringing (cid=%d)", cid);
						incall_set_stat(dev, incall_stat_none);
					}
					break;


				case incall_stat_waiting:

					if(!cc_is_any_call_to_be_answered(dev)) {
						_log(LOG_INFO, "[INCALL] send INFO CW_STOP (cid=%d)", cid);
						incall_send_info_to_peer(dev, DTMF_DIGIT_CW_STOP, NULL, NULL, NULL);

						_log(LOG_DEBUG, "[INCALL] turn off incall callwaiting tone", cid);
						incall_set_stat(dev, incall_stat_none);
					}

					break;
			}

			_log(LOG_DEBUG, "[call-status] * call status of '%s'\n", dev->id);
			_log(LOG_DEBUG, "[call-status] incall stat : %s\n", incall_get_stat_str(dev->incall_stat));

			if(cc_is_empty(dev)) {
				_log(LOG_DEBUG, "[call-status] no call available");
			} else {
				AST_LIST_TRAVERSE(&dev->calls, call, entry) {
					_log(LOG_DEBUG, "[call-status] cid#%d", call->cid);
					_log(LOG_DEBUG, "[call-status] clip : %s", call->clip);

					_log(LOG_DEBUG, "[call-status] outgoing : %d", call->outgoing);
					_log(LOG_DEBUG, "[call-status] progressed : %d", call->progressed);
					_log(LOG_DEBUG, "[call-status] ringing : %d", call->ringing);
					_log(LOG_DEBUG, "[call-status] connected : %d", call->connected);

					_log(LOG_DEBUG, "[call-status] incoming : %d", call->incoming);
					_log(LOG_DEBUG, "[call-status] answered : %d", call->answered);
					_log(LOG_DEBUG, "[call-status] invite   : %d", call->invite);
					_log(LOG_DEBUG, "[call-status] callwaiting : %d", call->callwaiting);

					_log(LOG_DEBUG, "[call-status] held : %d", call->held);

					_log(LOG_DEBUG, "[call-status] conference : %d", call->conference);
					_log(LOG_DEBUG, "[call-status] emergency : %d", call->emergency);
				}
			}



fini_end:
			__noop();
			break;
		}

		/* ignore known replies for voice command */
		case vc_answer:
		case vc_call:
		case vc_ecall:
		case vc_hangup:
		case vc_start_dtmf:
		case vc_stop_dtmf:
		case vc_manage_calls:
		case vc_manage_ip_calls:
		case vc_suppl_forward: {
			break;
		}

		default: {
			_log(LOG_DEBUG, "unknown notification (idx=%d,cmd=%s)", idx, cmd);
			break;
		}

	}

	return 0;
}

int on_rdb_noti(struct dev_t* dev, char* rdb, char *val)
{
	int stat = -1;

	/* bypass if blank */
	if(!*val)
		goto err;

	/* feed val to pp */
	stat = rwpipe_feed_read(dev->pp, val);
	if(stat < 0) {
		_log(LOG_ERR, "on_rdb_noti() failed in on_rdb_ctrl() - %s", strerror(errno));
		goto err;
	}

	/* reset rdb */
	if(rdb_set_string(_s, rdb, "") < 0) {
		_log(LOG_ERR, "rdb_set_string() failed in on_rdb_ctrl() - %s", strerror(errno));
		goto err;
	}

	return stat;

err:
	return -1;
}

#ifdef CONFIG_USE_STATISTICS
int on_rdb_statistics_reset(struct dev_t* dev, char* rdb, char *val)
{
	_log(LOG_INFO, "reset call statistics");

	if(!atoi(val))
		goto fini;

	_rdb_set_int(rdb, 0);

	memset(&statistics, 0, sizeof(statistics));
	rupdate_statistics();

fini:
	return 0;
}
#endif

int on_rdb_mwi(struct dev_t* dev, char* rdb, char *val2)
{
	char val[RDB_MAX_VAL_LEN];
	int val_len;

	int mwi_count;
	int mwi_active;

	int num_new;
	int num_old;

	/* get mwi count */
	val_len = sizeof(val);
	*val = 0;
	if(rdb_get(_s, dev->rdb_mwi_count, val, &val_len) < 0) {
		_log(LOG_ERR, "rdb_get(%s) failed - %s", dev->rdb_mwi_count, strerror(errno));
	}
	mwi_count = atoi(val);

	/* get mwi active */
	val_len = sizeof(val);
	*val = 0;
	if(rdb_get(_s, dev->rdb_mwi, val, &val_len) < 0) {
		_log(LOG_ERR, "rdb_get(%s) failed - %s", dev->rdb_mwi_count, strerror(errno));
	}
	mwi_active = atoi(val);

	/* bypass if nothing is changed */
	if(dev->mwi_valid && (mwi_active == dev->mwi_active) && (mwi_count == dev->mwi_count)) {
		_log(LOG_DEBUG, "no change in mwi (count=%d,active=%d)", mwi_count, mwi_active);
		goto fini;
	}

	/* update dev mwi information */
	dev->mwi_active = mwi_active;
	dev->mwi_count = mwi_count;
	dev->mwi_valid = 1;

	/* update mwi */
	num_new = mwi_active ? mwi_count : 0;
	num_old = !mwi_active ? mwi_count : 0;

	_log(LOG_INFO, "update new mwi (active=%d,count=%d,num_new=%d,num_old=%d)", mwi_active, mwi_count, num_new, num_old);
	update_mwi_mailbox(dev->id, num_new, num_old);

fini:
	return 0;
}

int on_rdb_ctrl(struct dev_t* dev, char* rdb, char *val)
{
	char msg_to_write[RDB_MAX_VAL_LEN];

	rwpipe_clear_wq_signal(dev->pp);

	/* bypass if value exists */
	if(*val)
		goto err;

	/* bypass if no data is taken */
	if(rwpipe_pop_wr_msg(dev->pp, msg_to_write, sizeof(msg_to_write)) < 0) {
		_log(LOG_DEBUG, "no message found");
		goto err;
	}

	if(rdb_set_string(_s, rdb, msg_to_write) < 0) {
		_log(LOG_ERR, "rdb_set_string() failed in on_rdb_ctrl() - %s", strerror(errno));
		goto err;
	}

err:
	return -1;
}

int process_msg_event()
{
	char msg[RDB_MAX_VAL_LEN];
	struct strarg_t* a;

	int cmd_idx;
	struct dev_t* dev;

	AST_RWLIST_RDLOCK(&devices);
	{
		AST_RWLIST_TRAVERSE(&devices, dev, entry) {
			ast_mutex_lock(&dev->lock);

			if(!(rwpipe_pop_rd_msg(dev->pp, NULL, msg, sizeof(msg), 0) < 0)) {

				a = strarg_create(msg);
				if(!a || !a->argv[0]) {
					_log(LOG_ERR, "strarg_create() failed in process_msg_event()");
					goto fini;
				}

				cmd_idx = indexed_rdb_get_cmd_idx(_ir, a->argv[0]);

				if(cmd_idx < 0) {
					_log(LOG_ERR, "unknown command recieved (msg='%s')", msg);
				} else {
					_log(LOG_DEBUG, "cmd (%s#%d) received (msg=%s)", a->argv[0], cmd_idx, msg);
					on_msg_noti(dev, cmd_idx, a);
				}

fini:
				strarg_destroy(a);
			}

			ast_mutex_unlock(&dev->lock);
		}

		AST_RWLIST_UNLOCK(&devices);
	}

	return 0;
}

int process_rdb_event_ctrl(struct dev_t* dev, char* rdb)
{
	char val[RDB_MAX_VAL_LEN];
	int val_len;

	/* read rdb val */
	val_len = sizeof(val);
	if(rdb_get(_s, rdb, val, &val_len) < 0) {
		_log(LOG_ERR, "cannot read a triggered rdb (rdb=%s) - %s", rdb, strerror(errno));
		goto err;
	}

	on_rdb_ctrl(dev, rdb, val);

	return 0;

err:
	return -1;
}

int process_rdb_event(char* rdb)
{
	char val[RDB_MAX_VAL_LEN];
	int val_len;

	struct dev_t* dev;

	/* read rdb val */
	val_len = sizeof(val);
	if(rdb_get(_s, rdb, val, &val_len) < 0) {
		_log(LOG_ERR, "cannot read a triggered rdb (rdb=%s) - %s", rdb, strerror(errno));
		goto err;
	}

	AST_RWLIST_RDLOCK(&devices);
	{
		AST_RWLIST_TRAVERSE(&devices, dev, entry) {
			ast_mutex_lock(&dev->lock);
			{
				if(!strcmp(rdb, dev->rdb_noti)) {
					on_rdb_noti(dev, rdb, val);
				} else if(!strcmp(rdb, dev->rdb_ctrl)) {
					on_rdb_ctrl(dev, rdb, val);
				} else if(!strcmp(rdb, dev->rdb_mwi)) {
					on_rdb_mwi(dev, rdb, val);
				}
#ifdef CONFIG_USE_STATISTICS
				else if(!strcmp(rdb, RDB_VOICE_CALL_STATISTICS_RESET)) {
					on_rdb_statistics_reset(dev, rdb, val);
				}
#endif

				ast_mutex_unlock(&dev->lock);
			}
		}

		AST_RWLIST_UNLOCK(&devices);
	}

	return 0;

err:
	return -1;
}

AST_THREADSTORAGE(pj_thread_storage);

static void pj_thread_register_check(void)
{
	pj_thread_desc *desc;
	pj_thread_t *thread;

	if(pj_thread_is_registered() == PJ_TRUE) {
		return;
	}

	desc = ast_threadstorage_get(&pj_thread_storage, sizeof(pj_thread_desc));
	if(!desc) {
		_log(LOG_ERROR, "Could not get thread desc from thread-local storage. Expect awful things to occur\n");
		return;
	}
	pj_bzero(*desc, sizeof(*desc));

	if(pj_thread_register("Asterisk Thread", *desc, &thread) != PJ_SUCCESS) {
		_log(LOG_ERROR, "Coudln't register thread with PJLIB.\n");
	}
	return;
}


static void *do_rdb_procedure(void *data)
{
	int stat;
	struct timeval tv;

	int rdb_hndl;
	int sig_hndl;

	int nfds;

	struct dbenum_t* dbenum;
	int total_triggers;

	ast_fdset readfds;

	struct dev_t* dev;

	pthread_t self;
	struct sched_param param;
	int policy;
	int max_priority;

	/*
		[TODO]

		!!! warning !!!
		there is a potential race-condtion. we have to be ready by calling start_devices() before calling ast_channel_register()
		move all preparation and finalization procedure of this thread to main thread !!
	*/

	/* print thread start information */
	_log(LOG_DEBUG, "[thread] start thread (pid=%d)", getpid());


	/* get thread param */
	self = pthread_self();
	pthread_getschedparam(self,&policy,&param);
	_log(LOG_DEBUG, "[thread] policy = %d, sched_priority=%d",policy,param.sched_priority);
	/* get max priority */
	max_priority = sched_get_priority_max(policy);
	/* increase priority */
	_log(LOG_DEBUG, "[thread] increase priority to max (max_priority=%d)", max_priority);
	param.sched_priority = max_priority;
	pthread_setschedparam(self,policy,&param);

	_log(LOG_DEBUG, "[thread] register pjsip");
	pj_thread_register_check();

	/* start devices */
	_log(LOG_DEBUG, "[thread] start devices");
	if(start_devices() < 0) {
		_log(LOG_ERR, "[thread] failed to start devices");
		goto err;
	}

	/* get rdb fd */
	nfds = rdb_hndl = rdb_fd(_s);


	_log(LOG_DEBUG, "[thread] rdb ready (fd=%d)", rdb_hndl);

	/* create dbenum */
	_log(LOG_DEBUG, "[thread] create dbenum");
	dbenum = dbenum_create(_s, TRIGGERED);

	/* collect and process existing noti */
	AST_RWLIST_RDLOCK(&devices);
	{
		AST_RWLIST_TRAVERSE(&devices, dev, entry) {
			process_rdb_event(dev->rdb_noti);
			process_rdb_event(dev->rdb_mwi);
		}

		AST_RWLIST_UNLOCK(&devices);
	}


	while(rdb_thread_running) {
		/* initiate tv */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/* initiate fds */
		FD_ZERO(&readfds);
		FD_SET(rdb_hndl, &readfds);

		/* collect signal handle */
		AST_RWLIST_RDLOCK(&devices);
		{
			AST_RWLIST_TRAVERSE(&devices, dev, entry) {
				sig_hndl = rwpipe_get_wq_fd(dev->pp);
				FD_SET(sig_hndl, &readfds);

				if(sig_hndl > nfds)
					nfds = sig_hndl;

				/* process dtmf */
				dtmf_process(dev);

				/* perform peding MT call */
				perform_pending_mt_call(dev, -1);
			}

			AST_RWLIST_UNLOCK(&devices);
		}


		stat = ast_select(nfds + 1, &readfds, NULL, NULL, &tv);
		if(stat < 0) {

			/* continue if interrupted by signal */
			if(errno == EINTR) {
				_log(LOG_DEBUG, "[thread] select punk!");
				continue;
			}

			_log(LOG_DEBUG, "[thread] select failure - %s", strerror(errno));
			break;

		} else if(stat == 0) {
			/* nothing to do */
			/* _log(LOG_DEBUG, "[thread] select timeout"); */
		} else {
			/* process if rdb is triggered */
			if(FD_ISSET(rdb_hndl, &readfds)) {
				struct dbenumitem_t* item;

				_log(LOG_DEBUG, "[thread] rdb triggered");

				total_triggers = dbenum_enumDb(dbenum);
				if(total_triggers <= 0) {
					_log(LOG_DEBUG, "[thread] triggered but no flaged rdb variable found - %s", strerror(errno));
				} else {
					_log(LOG_DEBUG, "[thread] walk through triggered rdb");

					item = dbenum_findFirst(dbenum);
					while(item) {
						_log(LOG_DEBUG, "[thread] triggered (rdb=%s)", item->szName);

						process_rdb_event(item->szName);
						item = dbenum_findNext(dbenum);
					}
				}

				process_msg_event();
			}

			/* collect signal handle */
			AST_RWLIST_RDLOCK(&devices);
			{
				AST_RWLIST_TRAVERSE(&devices, dev, entry) {

					sig_hndl = rwpipe_get_wq_fd(dev->pp);
					if(FD_ISSET(sig_hndl, &readfds)) {
						_log(LOG_DEBUG, "[thread] signal received (id=%s,sig_hndl=%d)", dev->id, sig_hndl);
						process_rdb_event_ctrl(dev, dev->rdb_ctrl);
					}
				}

				AST_RWLIST_UNLOCK(&devices);
			}

		}
	}

	/* destroy dbenum */
	_log(LOG_DEBUG, "[thread] destroy dbenum");
	dbenum_destroy(dbenum);

	/* stop devices */
	_log(LOG_DEBUG, "[thread] stop devices");
	stop_devices();

	_log(LOG_DEBUG, "[thread] exit");
	pthread_exit(NULL);

	return NULL;

err:
	_log(LOG_DEBUG, "[thread] exit - error");
	pthread_exit(NULL);

	return NULL;
}


char* __str_upper(char* str)
{
	char *p = str;

	while(*p) {
		*p = toupper(*p);
		p++;
	}

	return str;
}

#if 0
int reply_filter_suppl_forward_by_prefix(struct rcch_t* ch, void* ref, int idx, const char* cmd, const char* arg, struct rcch_rdb_filter_t* f)
{
	char* prefix;
	int prefix_len;

	prefix = (char*)f->ref;
	prefix_len = strlen(prefix);

	return (idx == vc_suppl_forward) && strncmp(arg, prefix, prefix_len);
}
#endif

static int _ast_app_suppl_forward_exec(struct ast_channel *chan, const char *data)
{
	char* raw_arg;
	struct suppl_forward_command_args_t arg;
	struct dev_t* dev;

	char* suppl_arg;
	/*
		struct rcch_rdb_filter_t f;
	*/
	struct strarg_t* a = NULL;

	int stat = -1;

	_log(LOG_INFO, "[AST] got supple exec (data=%s)", data);


	/* parse arg */
	_log(LOG_DEBUG, "process arguments");
	raw_arg = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(arg, raw_arg);

	/* check mandatory arguments */
	_log(LOG_DEBUG, "check argument count (argc=%d)", arg.argc);
	if(arg.argc < 3 || ast_strlen_zero(arg.dev)) {
		_log(LOG_ERR, "%s requires Device,ServiceName and ServiceCommand", _ast_app_command_suppl_forward);
		goto err;
	}

	/* find device */
	_log(LOG_DEBUG, "search device (dev=%s)", arg.dev);
	if(!(dev = _find_dev_by_id(arg.dev))) {
		_log(LOG_ERR, "target device not found (dev=%s)", arg.dev);
		goto err;
	}

	/*
		SupplForward(baseband,Unconditioinal,Register,0413237592),
	*/
	char* service_type = __str_upper(strdupa(arg.service_type));
	char* service_command = __str_upper(strdupa(arg.service_command));

	/* do validation check for service type */
	if(dbhash_lookup(_hash_suppl_forward_service_type, service_type) < 0) {
		_log(LOG_ERR, "supplementary service type not found (service_type=%s)", service_type);
		goto err;
	}

	/* do validation check for service command */
	if(dbhash_lookup(_hash_suppl_forward_service_command, service_command) < 0) {
		_log(LOG_ERR, "supplementary service command not found (service_command=%s)", service_command);
		goto err;
	}

	_log(LOG_INFO, "[RDB] request suppl forward (service_type=%s,service_command=%s)", service_type, service_command);

	suppl_arg = alloca(RDB_MAX_VAL_LEN);

	const char* destno = arg.param1;
	const char* processed_destno = destno;

#ifdef CONFIG_CONVERT_INT_PREFIX_INTO_PLUS
	char destno_buf[256];

	if(!ast_strlen_zero(destno)) {
		char* international_prefix;
		int international_prefix_len;
		char* saveptr;
		char* international_prefixes;

		international_prefixes = ast_strdupa(_rdb_get_str("pbx.dialplan.misc.international"));

		/* if interntaional prefix */
		international_prefix = strtok_r(international_prefixes, "/", &saveptr);
		while(international_prefix) {
			international_prefix_len = strlen(international_prefix);
			if(!strncmp(destno, international_prefix, international_prefix_len)) {

				snprintf(destno_buf, sizeof(destno_buf), "+%s", destno + international_prefix_len);
				processed_destno = destno_buf;

				_log(LOG_DEBUG, "[SUPPL] dialplan - international prefix detected, %s ==> %s", destno, processed_destno);

				goto fini_i_prefix;
			}
			international_prefix = strtok_r(NULL, "/", &saveptr);
		}

		/* bypass destno without processing */
		processed_destno = destno;

fini_i_prefix:
		__noop();
	}
#endif

	if(ast_strlen_zero(processed_destno)) {
		snprintf(suppl_arg, RDB_MAX_VAL_LEN, "%s %s %s", __STR(vc_suppl_forward), service_type, service_command);
	} else {

		snprintf(suppl_arg, RDB_MAX_VAL_LEN, "%s %s %s %s", __STR(vc_suppl_forward), service_type, service_command, processed_destno);
	}


	char reply[RDB_MAX_VAL_LEN];
	stat = rwpipe_post_and_get(dev->pp, reply, sizeof(reply), 0, suppl_arg);
	if(stat < 0) {
		_log(LOG_ERR, "failed to write control command (service_type=%s,service_command=%s)", service_type, service_command);
		goto fini;
	}

	a = strarg_create(reply);
	{
		const char* suppl_stat;
		const char* suppl_number;

		suppl_stat = a->argv[3];
		suppl_number = a->argv[4];

		if(!suppl_number)
			suppl_number = "";

		_log(LOG_INFO, "[AST] suppl exec result [SUPPL_STAT] ==> %s", suppl_stat);
		_log(LOG_INFO, "[AST] suppl exec result [SUPPL_NUMBER] ==> '%s'", suppl_number);

		pbx_builtin_setvar_helper(chan, "SUPPL_STAT", suppl_stat);
		pbx_builtin_setvar_helper(chan, "SUPPL_NUMBER", suppl_number);

		strarg_destroy(a);
	}

	stat = 0;

fini:
	return stat;
err:
	return -1;
}

/* info supplement */

static int is_media_type(pjsip_rx_data *rdata, char *subtype)
{
	return rdata->msg_info.ctype
	       && !pj_strcmp2(&rdata->msg_info.ctype->media.type, "application")
	       && !pj_strcmp2(&rdata->msg_info.ctype->media.subtype, subtype);
}

static void send_response(struct ast_sip_session *session, struct pjsip_rx_data *rdata, int code)
{
	pjsip_tx_data *tdata;
	pjsip_dialog *dlg = session->inv_session->dlg;

	if(pjsip_dlg_create_response(dlg, rdata, code, NULL, &tdata) == PJ_SUCCESS) {
		struct pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);
		pjsip_dlg_send_response(dlg, tsx, tdata);
	}
}

static int _ast_info_incoming_request(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	pjsip_msg_body *body = rdata->msg_info.msg->body;
	char buf[body ? body->len : 0];

	struct ast_channel* chan;
	struct ast_channel* peer_chan;
	const struct ast_channel_tech* peer_tech;
	struct dev_t* dev;

	_log(LOG_DEBUG, "[AST] got sip info request");

	/*
		* INFO example

		INFO sip:7007471000@example.com SIP/2.0
		Via: SIP/2.0/UDP alice.uk.example.com:5060
		From: <sip:7007471234@alice.uk.example.com>;tag=d3f423d
		To: <sip:7007471000@example.com>;tag=8942
		Call-ID: 312352@myphone
		CSeq: 5 INFO
		Content-Length: 24
		Content-Type: application/dtmf-relay

		Signal=5
		Duration=160
	*/

	/* bypass if no content type found */
	if(!rdata->msg_info.ctype) {
		_log(LOG_DEBUG, "no content type available, ignore");
		goto fini;
	}

#if VERBOSE_DEBUG_SIP_INFO

#define CTYPE_STR_BUF	64
	char media_type[CTYPE_STR_BUF];
	char media_subtype[CTYPE_STR_BUF];
	pj_str_t media_type_pjstr = {media_type, sizeof(media_type)};
	pj_str_t media_subtype_pjstr = {media_subtype, sizeof(media_subtype)};

	pj_strncpy_with_null(&media_type_pjstr, &rdata->msg_info.ctype->media.type, CTYPE_STR_BUF);
	pj_strncpy_with_null(&media_subtype_pjstr, &rdata->msg_info.ctype->media.subtype, CTYPE_STR_BUF);

	_log(LOG_DEBUG, "[AST] got sip info '%s/%s'", media_type, media_subtype);
#endif

	/* bypass if not hookflash */
	if(!is_media_type(rdata, "hook-flash")) {
		return 0;
	}

	_log(LOG_INFO, "[AST] got hookflash");

	/* print if body exists */
	if(body && body->len) {
		body->print_body(body, buf, body->len);
	}

	/* get channel */
	chan = session->channel;
	if(!chan) {
		_log(LOG_ERR, "no channel assigned to session");
		goto fini;
	}

	/* get peer channel */
	peer_chan = ast_channel_peer(chan);
	if(!peer_chan) {
		_log(LOG_ERR, "no peer channel assigned to session");
		goto fini;
	}

	/* get peer tech */
	peer_tech = ast_channel_tech(peer_chan);
	if(!peer_tech) {
		_log(LOG_ERR, "no peer tech assigned to session");
		goto fini_peer_chan;
	}

	/* run if QMIRDB channel */
	if(!strcmp(peer_tech->type, "QMIRDB")) {
		dev = ast_channel_tech_pvt(peer_chan);

		if(!dev) {
			_log(LOG_ERR, "no dev assigned to peer channel");
			goto fini_peer_chan;
		}

		ast_mutex_lock(&dev->lock);
		{

			_log(LOG_DEBUG, "[hookflash] post hookflash to peer");
			do_hookflash(dev);
			_log(LOG_DEBUG, "[hookflash] post hookflash to peer #done");

			ast_mutex_unlock(&dev->lock);
		}

	}

fini_peer_chan:

	_log(LOG_DEBUG, "[hookflash] send response");

	/* send response */
	send_response(session, rdata, 200);

	_log(LOG_DEBUG, "[hookflash] send response #done");


fini:
	return 0;

}

static pj_status_t on_tx_request(pjsip_tx_data *tdata)
{
	pjsip_fromto_hdr *from;
	size_t pj_hdr_string_len;
	char pj_hdr_string[1024];

	if(pjsip_method_cmp(&tdata->msg->line.req.method, &pjsip_invite_method))
		return PJ_SUCCESS;

	from = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_FROM, NULL);
	pj_hdr_string_len = pjsip_hdr_print_on(from, pj_hdr_string, sizeof(pj_hdr_string));
	pj_hdr_string[pj_hdr_string_len] = 0;
	_log(LOG_DEBUG, "[caller-id] from = %s", pj_hdr_string);

	return PJ_SUCCESS;
}

static pj_bool_t on_rx_request(struct pjsip_rx_data *rdata)
{
	const char* ims_serv;
	int ims_full_serv;
	int nw_reg;
	int ign;

	RAII_VAR(struct ast_sip_endpoint *, endpoint, ast_pjsip_rdata_get_endpoint(rdata), ao2_cleanup);

	if(!endpoint)
		return PJ_FALSE;

	if(pjsip_method_cmp(&rdata->msg_info.msg->line.req.method, &pjsip_register_method))
		return PJ_FALSE;

	ign = _rdb_get_int("pbx.ignore_sim_activation");

	/* get network register */
	nw_reg = _rdb_get_int("wwan.0.system_network_status.registered");
	/* get ims service status */
	ims_serv = _rdb_get_str("wwan.0.ims.service.voip_service_status");
	ims_full_serv = !strcmp(ims_serv, "full service");


	_log(LOG_DEBUG, "[register] on_rx_request called() nw_reg=%d,ims_full_serv=%d,ign=%d", nw_reg, ims_full_serv, ign);

	if(!ign) {
		if(!nw_reg) {
			/* send 503 service unavailable */
			_log(LOG_DEBUG, "[register] service unavailable, send 503");
			pjsip_endpt_respond_stateless(ast_sip_get_pjsip_endpoint(), rdata, 503, NULL, NULL, NULL);

			goto fini;
		} else if(!ims_full_serv) {
			/* send 480 service unavailable */
			_log(LOG_DEBUG, "[register] service unavailable, send 480");
			pjsip_endpt_respond_stateless(ast_sip_get_pjsip_endpoint(), rdata, 480, NULL, NULL, NULL);

			goto fini;
		}
	}

	_log(LOG_DEBUG, "[register] RG registered");

	ua_registered = 1;

	return PJ_FALSE;

fini:
	return PJ_TRUE;
}

static void signal_handler_sigurg(int num)
{
	_log(LOG_DEBUG, "SIGURG punk!");
}

static int _ast_load_module(void)
{
	_log(LOG_EMERG,  "log level");
	_log(LOG_EMERG,  "========================================");
	_log(LOG_EMERG,  "_log(%d) - LOG_EMERG", LOG_EMERG);
	_log(LOG_ALERT,  "_log(%d) - LOG_ALERT", LOG_ALERT);
	_log(LOG_CRIT,   "_log(%d) - LOG_CRIT", LOG_CRIT);
	_log(LOG_ERR,    "_log(%d) - LOG_ERR", LOG_ERR);
	_log(LOG_WARNING, "_log(%d) - LOG_WARNING", LOG_WARNING);
	_log(LOG_NOTICE, "_log(%d) - LOG_NOTICE", LOG_NOTICE);
	_log(LOG_INFO,   "_log(%d) - LOG_INFO", LOG_INFO);
	_log(LOG_DEBUG,  "_log(%d) - LOG_DEBUG", LOG_DEBUG);
	_log(LOG_INFO,  "========================================");

	_log(LOG_EMERG,  "start qmirdb channel driver (%s %s)", __DATE__, __TIME__);

	CHECK_PJSIP_SESSION_MODULE_LOADED();

#if 0
	{
		snd_use_case_mgr_t* ucm_mgr;
		int rc;

		rc = snd_use_case_mgr_open(&ucm_mgr, "snd_soc_msm_Tomtom_I2S");
		_log(LOG_DEBUG,  "card rc=%d", rc);

		char devName[1024];
		rc = snd_use_case_get(ucm_mgr, "CapturePCM/SND_USE_CASE_VERB_VOICECALL", (const char **)devName);
		_log(LOG_DEBUG,  "case rc=%d", rc);

		snd_use_case_mgr_close(ucm_mgr);
	}
#endif

	signal(SIGURG, signal_handler_sigurg);

	/* add format */
	if(!(qmirdb_tech.capabilities = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT))) {
		_log(LOG_ERR, "unable to add format");
		goto dec;
	}
	ast_format_cap_append(qmirdb_tech.capabilities, DEVICE_FRAME_FORMAT, 0);

	/* create hash for supplementary */
	_hash_suppl_forward_service_type = dbhash_create(_hash_suppl_forward_elements_service_type, sizeof(_hash_suppl_forward_elements_service_type) / sizeof(_hash_suppl_forward_elements_service_type[0]));
	if(!_hash_suppl_forward_service_type) {
		_log(LOG_ERR, "unable to create hash table for service type");
		goto err;
	}

	_hash_suppl_forward_service_command = dbhash_create(_hash_suppl_forward_elements_service_command, sizeof(_hash_suppl_forward_elements_service_command) / sizeof(_hash_suppl_forward_elements_service_command[0]));
	if(!_hash_suppl_forward_service_command) {
		_log(LOG_ERR, "unable to create hash table for service command");
		goto err;
	}

	/* create index rdb */
	_ir = indexed_rdb_create(rcchp_cmds, rcchp_cmds_count);
	if(!_ir) {
		_log(LOG_ERR, "cannot create indexed rdb");
		goto err;
	}

	/* load config */
	if(load_config()) {
		_log(LOG_ERR, "unable to read configuration file %s. aborting.", config);
		goto dec;
	}

	/* register our channel type */
	_log(LOG_DEBUG, "register qmi channel driver");
	if(ast_channel_register(&qmirdb_tech)) {
		_log(LOG_ERR, "Unable to register channel class %s", "Mobile");
		goto err;
	}

	/* spin rdb thread */
	_log(LOG_DEBUG, "start rdb thread");
	rdb_thread_running = 1;
	if(ast_pthread_create_background(&rdb_thread, NULL, do_rdb_procedure, NULL) < 0) {
		_log(LOG_ERR, "unable to create discovery thread.");
		goto err;
	}

	/* register cli functions */
	ast_cli_register_multiple(qmirdb_cli, sizeof(qmirdb_cli) / sizeof(qmirdb_cli[0]));

	ast_register_application(
	    _ast_app_command_suppl_forward,
	    _ast_app_suppl_forward_exec,
	    "SupplForward(Device,ServiceName,ServiceCommand,ServiceArgument)",
	    "SupplForward(Device,ServiceName,ServiceCommand,ServiceArgument):n"
	    "  Device - Id of mobile device from chan_qmirdb.conf\n"
	    "\n"
	    "  ServiceName     - supplementary service name\n"
	    "                    UnconditionalForward\n"
	    "\n"
	    "  ServiceCommand  - supplementary service command\n"
	    "                    Register\n"
	    "                    Activate\n"
	    "                    Deactivate\n"
	    "                    Status\n"
	    "\n"
	    "  ServiceArgument - argument for supplementary\n"
	    "\n"
	);

	_log(LOG_DEBUG, "register info supplement");
	ast_sip_session_register_supplement(&info_supplement);

	/* register registrar */
	_log(LOG_DEBUG, "[register] register registrar");
	if(ast_sip_register_service(&registrar_module)) {
		_log(LOG_ERR, "[register] Unable to register registrar");
		goto err;
	}

	return AST_MODULE_LOAD_SUCCESS;

err:
	return AST_MODULE_LOAD_FAILURE;

dec:
	return AST_MODULE_LOAD_DECLINE;
}


/*
 Asterisk callback to unload QMI Asterisk module.

 Parameters:
  N/A.

 Return:
  On success, zero is returned. Otherwise, -1.
*/
static int _ast_unload_module(void)
{
	/* prepare to unload */
	_log(LOG_DEBUG, "[unload] prepare to unload");
	prepare_to_unload();

	/* wait for devices */
	_log(LOG_DEBUG, "[unload] wait for devices");
	if(wait_for_devices() < 0) {
		_log(LOG_ERR, "[unload] channel module is not ready to unload");
		goto err;
	}

	_log(LOG_DEBUG, "[unload] unregister registrar");
	ast_sip_unregister_service(&registrar_module);

	/* Kill the discovery thread */
	_log(LOG_DEBUG, "[unload] stop rdb thread");
	rdb_thread_running = 0;

	if(rdb_thread != AST_PTHREADT_NULL) {
		pthread_kill(rdb_thread, SIGURG);
		pthread_join(rdb_thread, NULL);
	}

	_log(LOG_DEBUG, "[unload] unregister info supplement");
	ast_sip_session_unregister_supplement(&info_supplement);

	/* First, take us out of the channel loop */
	_log(LOG_DEBUG, "[unload] unregister qmi channel driver");
	ast_channel_unregister(&qmirdb_tech);

	/* unregister cli */
	ast_cli_unregister_multiple(qmirdb_cli, sizeof(qmirdb_cli) / sizeof(qmirdb_cli[0]));

	ast_unregister_application(_ast_app_command_suppl_forward);

	/* unload devices */
	_log(LOG_DEBUG, "[unload] unload devices");
	unload_devices();

	/* destroy hash for supplementary functions */
	_log(LOG_DEBUG, "[unload] finish hashes");
	dbhash_destroy(_hash_suppl_forward_service_type);
	dbhash_destroy(_hash_suppl_forward_service_command);

	indexed_rdb_destroy(_ir);

	_log(LOG_DEBUG, "[unload] done.");

	return 0;

err:
	return -1;
}

AST_MODULE_INFO(
    ASTERISK_GPL_KEY,
    AST_MODFLAG_LOAD_ORDER,
    tdesc,
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = _ast_load_module,
    .unload = _ast_unload_module,
    .load_pri = AST_MODPRI_CHANNEL_DRIVER,
);
