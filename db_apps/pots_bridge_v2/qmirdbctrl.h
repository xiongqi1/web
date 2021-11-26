#ifndef __QMIRDBCTRL_H__
#define __QMIRDBCTRL_H__

/*
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

#include "dbhash.h"

enum voice_command_t {
    vc_answer = 0,
    vc_call,
    vc_pcall,
    vc_ocall,
    vc_ecall,
    vc_hangup,
    vc_setup_answer,
    vc_reject,
    vc_call_notify,
    vc_dtmf_notify,
    vc_suppl_forward,
    vc_set_tty_mode,

    vc_start_dtmf,
    vc_stop_dtmf,

    vc_manage_calls,
    vc_manage_ip_calls,

    vc_init_voice_call,
    vc_fini_voice_call,

    vc_last,

};

extern struct dbhash_element_t rcchp_cmds[];
extern int rcchp_cmds_count;

#define QMIRDBCTRL_CTRL "ctrl"
#define QMIRDBCTRL_NOTI "noti"
#define QMIRDBCTRL_TIMEOUT	10

#define RDB_MAX_VAL_LEN	1024

#endif
