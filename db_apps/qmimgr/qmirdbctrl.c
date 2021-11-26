#include "qmirdbctrl.h"

struct dbhash_element_t rcchp_cmds[] = {
	{"ANSWER", vc_answer},
	{"CALL", vc_call},
	{"HANGUP", vc_hangup},
	{"CALL_NOTIFY", vc_call_notify},
	{"SUPPL_FORWARD", vc_suppl_forward},
};

int rcchp_cmds_count = sizeof(rcchp_cmds) / sizeof(rcchp_cmds[0]);
