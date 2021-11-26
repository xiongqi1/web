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

#ifndef HEADER_GUARD_RDB_OPERATIONS_20090217_
#define HEADER_GUARD_RDB_OPERATIONS_20090217_

#include "rdb_ops.h"
#include "./slic_control/slic_control.h"

#define RDB_VALUE_SIZE_SMALL 128
#define RDB_VALUE_SIZE_LARGE 256

/* define RDB variables */
#define	RDB_POTS_INSTANCE		"pots.instance"				/* pots_bridge instance number */
#define	RDB_VOIP_CH_NO			"pots.voip_ch_no"			/* VOIP channel count defined in pots_bridge */
#define	RDB_MODEM_NAME			"wwan.0.model"
#define	RDB_BOARD_NAME			"system.board"
#define	RDB_IF_NAME				"wwan.0.if"					/* at, atcns, cns,.. */

/* prefix names */
#define RDB_VOIP_ST_PREFIX		"voip.status"
#define RDB_VOIP_CMD_PREFIX		"voip.command"
#define RDB_VOIP_CALLS_PREFIX	"voip.calls"
#define RDB_VOIP_DTMF_PREFIX	"voip.dtmfkeys"
#define RDB_UMTS_CMD_PREFIX		"umts.services.command"
#define RDB_UMTS_CALLS_PREFIX	"umts.services.calls"
#define RDB_UMTS_DTMF_PREFIX	"umts.services.dtmfkeys"
#define RDB_PHONE_CMD_PREFIX	"phone.setup.command"
#define RDB_SMS_VM_PREFIX		"sms.received_message"
#define RDB_DIALPLAN_PREFIX		"voicecall.dial_plan"
#define RDB_SIM_DATA_PREFIX		"sim.data"
#define RDB_SIM_STATUS_PREFIX	"sim.status"
#define RDB_SYSTEM_PREFIX		"system_network_status"
#define RDB_IMSI_PREFIX		    "imsi"

/* status variable names */
#define	RDB_SIP_REG_RESULT		"registered"				/* 1 or 0 */

/* roaming status */
#define RDB_ROAMING_STATUS		"system_network_status.roaming"

/* command variable names */
#define	RDB_CMD_CMD				"cmd"
#define	RDB_CMD_STATUS			"status"
#define	RDB_CMD_RESP			"response"
#define	RDB_CMD_LAST			"last"
#define	RDB_CMD_RESULT			"result_name"
#define	RDB_CALLS_EVENT			"event"
#define	RDB_CALLS_LIST			"list"
#define	RDB_VM_STATUS			"vmstatus"
#define	RDB_DP_TIMEOUT			"timeout"
#define	RDB_DP_INT_PREFIX		"international_prefix"
#define	RDB_DP_REGEX			"regex"
#define RDB_SIM_MBN             "mbn"
#define	RDB_SIM_MBDN			"mbdn"
#define	RDB_SIM_ADN			    "adn"
#define	RDB_SIM_MSISDN			"msisdn"
#define	RDB_SIM_STATUS			"status"

/* command list */
#define	RDB_SCMD_CALLS_LIST		"calls list"
#define	RDB_SCMD_PROFILE_INIT	"profile init"
#define	RDB_SCMD_PROFILE_SET	"profile set"
#define	RDB_SCMD_ATA			"ATA"
#define	RDB_SCMD_ATCHUP			"AT+CHUP"

/* roaming variables */
#define OUTGOING_VOICE_ROAMING_DB_NAME			"roaming.voice.outgoing.en"
#define INCOMING_VOICE_ROAMING_DB_NAME			"roaming.voice.incoming.en"
#define OUTGOING_VOICE_ROAMING_BLOCKED_DB_NAME	"roaming.voice.outgoing.blocked"
#define INCOMING_VOICE_ROAMING_BLOCKED_DB_NAME	"roaming.voice.incoming.blocked"

/* call statistics variables */
#define RDB_STATISTIC_USAGE		"statistics.usage"
#define OUTGOING_CALL_COUNT		"outgoing_call_count"
#define INCOMING_CALL_COUNT		"incoming_call_count"
#define TATAL_CALL_COUNT		"total_call_count"

/* CLIP */
#define RDB_UMTS_CLIP_NUMBER	"umts.services.clip.number"

/* system mode */
#define RDB_SYSTEM_MODE	        "system_mode"

/* servie status */
#define RDB_SERVICE_STATUS	    "service_status"

/* network status */
#define RDB_NETWORK	    		"network"
#define RDB_NETWORK_UNENC	   	"network.unencoded"

/* PLMN */
#define RDB_PLMN_MCC	        "plmn_mcc"
#define RDB_PLMN_MNC	        "plmn_mnc"

/* profile set */
#define RDB_PROFILE_LIST		"pots.profilelist"
#define RDB_SP_LIST		        "pots.splist"

typedef int (*p_rdb_event_function_type)(const char *);
typedef struct {
	const char* szPrefix;
	const char* szName;
	const int instance;					/* -1 : no instance */
	const int subscribe;
	pots_call_type call_type;		/* none : common */
	p_rdb_event_function_type p_rdb_event_handler;
} rdb_handler_type;

int rdb_poll_events (int timeout, const char *name, const char *value);
int send_rdb_command( const char* name, const char* value, int retry_cnt );
//int get_rdb_command_status( const char* status );
int send_rdb_command_blocking( const char* path, const char* value, int retry_cnt );
int rdb_getvnames (char *name, char *value, int *length, int flags);
const char* rdb_name(const char* prefix, const char* path, const char* instance, const char* name);
const char* rdb_variable(const char* path, const char* instance, const char* name);
int rdb_ready_to_send_command(const char* name);

#endif // HEADER_GUARD_RDB_OPERATIONS_20090217_

/*
* vim:ts=4:sw=4:
*/
