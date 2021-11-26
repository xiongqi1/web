#include "qmirdbctrl.h"

struct dbhash_element_t rcchp_cmds[] = {
	{"ANSWER", vc_answer},
	{"CALL", vc_call},
	{"ECALL", vc_ecall},
	{"HANGUP", vc_hangup},
	{"REJECT", vc_reject},
	{"CALL_NOTIFY", vc_call_notify},
	{"DTMF_NOTIFY", vc_dtmf_notify},
	{"SUPPL_FORWARD", vc_suppl_forward},

	{"START_DTMF", vc_start_dtmf},
	{"STOP_DTMF", vc_stop_dtmf},

	{"MANAGE_CALLS", vc_manage_calls},
	{"MANAGE_IP_CALLS", vc_manage_ip_calls},

};

int rcchp_cmds_count = sizeof(rcchp_cmds) / sizeof(rcchp_cmds[0]);
