/*
 * call_progress.h
 *
 *  Created on: Feb 17, 2009
 *      Author: vsevolod
 */

#ifndef CALL_PROGRESS_H_20090217_
#define CALL_PROGRESS_H_20090217_

// rdb variable names
#define RDB_VOICECALL_STATUS_VARIABLE "wwan.0.voicecall.status"
#define RDB_VOICECALL_CMD_COMMAND_VARIABLE  "wwan.0.voicecall.cmd.command"
#define RDB_VOICECALL_CMD_PARAM_VARIABLE  "wwan.0.voicecall.cmd.param"
#define RDB_VOICECALL_PROGRESS_VARIABLE RDB_VOICECALL_STATUS_VARIABLE".progress"

// rdb variable values
#define RDB_VOICECALL_PROGRESS_FAILED "Call failed"
#define RDB_VOICECALL_PROGRESS_CONNECTING  "Call connecting"
#define RDB_VOICECALL_PROGRESS_QUEUED  "Call queued"
#define RDB_VOICECALL_PROGRESS_WAITING  "Call waiting"
#define RDB_VOICECALL_PROGRESS_INCOMING  "Call incoming"
#define RDB_VOICECALL_PROGRESS_RINGING  "Call ringing"
#define RDB_VOICECALL_PROGRESS_CONNECTED  "Call connected"
#define RDB_VOICECALL_PROGRESS_DISCONNECTING  "Call disconnecting"
#define RDB_VOICECALL_PROGRESS_DISCONNECTED  "Call disconnected"
#define RDB_VOICECALL_PROGRESS_FORWARD_CALL_INCOMING  "Forward call incoming"

#define RDB_VOICECALL_PROGRESS_VALUE_MAX_LEN 64
#define RDB_VOICECALL_CALLID_VALUE_MAX_LEN 4

#define RDB_VOICECALL_CMD_HANGUP  "hangup"
#define RDB_VOICECALL_CMD_PICKUP  "pickup"
#define RDB_VOICECALL_CMD_DIAL  "dial"

#endif /* CALL_PROGRESS_H_20090217_ */

