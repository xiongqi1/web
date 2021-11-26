#ifndef __QMIRDBCTRL_H__
#define __QMIRDBCTRL_H__

#include "dbhash.h"

enum voice_command_t {
	vc_answer = 0,
	vc_call,
	vc_ecall,
	vc_hangup,
	vc_reject,
	vc_call_notify,
	vc_dtmf_notify,
	vc_suppl_forward,

	vc_start_dtmf,
	vc_stop_dtmf,

	vc_manage_calls,
	vc_manage_ip_calls,

	vc_last,

};

extern struct dbhash_element_t rcchp_cmds[];
extern int rcchp_cmds_count;

#define QMIRDBCTRL_CTRL "ctrl"
#define QMIRDBCTRL_NOTI "noti"
#define QMIRDBCTRL_TIMEOUT	10

#define RDB_MAX_VAL_LEN	1024

#endif
